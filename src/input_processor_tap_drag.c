/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_tap_drag

#include <stdint.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>

#include <drivers/input_processor.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// Timer expiries are fed back through the input queue as this private key code,
// so every state change happens on the input thread, in order with the driver's
// own events. It is Linux's BTN_TOOL_DOUBLETAP: the Azoteq driver never emits it,
// and it is always consumed or rewritten here, so it never leaves this processor.
#define TAP_DRAG_TICK_CODE 0x14d

enum tap_drag_state {
    // Not involved: the driver's clicks and press-and-hold pass through as-is.
    TAP_DRAG_IDLE,
    // A tap's press went out and its release is held back for tap-window-ms in
    // case the finger comes straight back down.
    TAP_DRAG_TAPPED,
    // The finger came back down in time, so the button stays held and movement
    // drags.
    TAP_DRAG_DRAGGING,
    // The finger lifted mid-drag. The button stays held for release-delay-ms so
    // the finger can be repositioned on the small pad and the drag carried on.
    TAP_DRAG_RELEASING,
    // Another gesture (scroll, swipe, right click) took over. The button is
    // released on the next tick and touches no longer resume the drag.
    TAP_DRAG_ENDING,
};

struct tap_drag_config {
    uint32_t tap_window_ms;
    uint32_t release_delay_ms;
    uint32_t drag_distance;
};

struct tap_drag_data {
    const struct device *src;
    struct k_work_delayable tick_work;
    int64_t deadline;
    enum tap_drag_state state;
    // Pointer travel since the tap that started the current drag.
    uint32_t travel;
    bool armed;
    bool touching;
    // A press-and-hold press went out, so its release must too.
    bool hold_forwarded;
    // A press-and-hold press was absorbed by a drag that already holds the
    // button, so its release is absorbed too.
    bool hold_swallowed;
    // The driver reports a tap as a press immediately followed by a release.
    // These say what to do with that release.
    bool tap_release_swallow;
    bool tap_release_to_press;
};

static void tap_drag_tick_work(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct tap_drag_data *data = CONTAINER_OF(dwork, struct tap_drag_data, tick_work);

    if (data->src == NULL) {
        return;
    }

    // K_FOREVER: a dropped tick would leave the button held down.
    int ret = input_report(data->src, INPUT_EV_KEY, TAP_DRAG_TICK_CODE, 1, true, K_FOREVER);
    if (ret < 0) {
        LOG_ERR("tap-drag tick not queued: %d", ret);
    }
}

static void tap_drag_arm(struct tap_drag_data *data, uint32_t ms) {
    data->deadline = k_uptime_get() + ms;
    data->armed = true;
    k_work_reschedule(&data->tick_work, K_MSEC(ms));
}

static void tap_drag_disarm(struct tap_drag_data *data) {
    data->armed = false;
    k_work_cancel_delayable(&data->tick_work);
}

static void tap_drag_start_click(const struct tap_drag_config *cfg, struct tap_drag_data *data) {
    data->travel = 0;
    data->state = TAP_DRAG_TAPPED;
    tap_drag_arm(data, cfg->tap_window_ms);
}

// Another gesture started while the button is held for a tap or drag: let it
// go right away rather than scroll, swipe or right click with it held down.
static void tap_drag_abort(struct tap_drag_data *data) {
    if (data->state == TAP_DRAG_IDLE || data->state == TAP_DRAG_ENDING) {
        return;
    }
    data->state = TAP_DRAG_ENDING;
    tap_drag_arm(data, 0);
}

static int tap_drag_on_tick(struct tap_drag_data *data, struct input_event *event) {
    // A tick from a timer that has since been cancelled, or pushed back by a
    // later rearm that raced the work item, is stale.
    if (!data->armed) {
        return ZMK_INPUT_PROC_STOP;
    }

    const int64_t remaining = data->deadline - k_uptime_get();
    if (remaining > 0) {
        k_work_reschedule(&data->tick_work, K_MSEC(remaining));
        return ZMK_INPUT_PROC_STOP;
    }

    data->armed = false;

    switch (data->state) {
    case TAP_DRAG_TAPPED:
    case TAP_DRAG_RELEASING:
    case TAP_DRAG_ENDING:
        break;
    default:
        return ZMK_INPUT_PROC_STOP;
    }

    LOG_DBG("tap-drag releasing button (state %d)", data->state);
    data->state = TAP_DRAG_IDLE;

    // The tick itself becomes the release of the held button.
    event->code = INPUT_BTN_0;
    event->value = 0;
    event->sync = true;
    return ZMK_INPUT_PROC_CONTINUE;
}

static void tap_drag_on_touch(const struct tap_drag_config *cfg, struct tap_drag_data *data,
                              bool down) {
    data->touching = down;

    if (down) {
        if (data->state == TAP_DRAG_TAPPED || data->state == TAP_DRAG_RELEASING) {
            tap_drag_disarm(data);
            data->state = TAP_DRAG_DRAGGING;
            LOG_DBG("tap-drag dragging");
        }
    } else if (data->state == TAP_DRAG_DRAGGING) {
        data->state = TAP_DRAG_RELEASING;
        tap_drag_arm(data, cfg->release_delay_ms);
    }
}

static int tap_drag_on_press(const struct tap_drag_config *cfg, struct tap_drag_data *data,
                             struct input_event *event) {
    if (data->touching) {
        // The driver's own press-and-hold gesture, which only fires with a
        // finger down.
        if (data->state == TAP_DRAG_IDLE) {
            data->hold_forwarded = true;
            return ZMK_INPUT_PROC_CONTINUE;
        }

        data->hold_swallowed = true;
        if (data->state != TAP_DRAG_ENDING) {
            // A finger is down, so this is a drag even if its touch was missed.
            tap_drag_disarm(data);
            data->state = TAP_DRAG_DRAGGING;
        }
        return ZMK_INPUT_PROC_STOP;
    }

    // A tap, which the driver reports once the finger has lifted.
    if (data->state == TAP_DRAG_IDLE) {
        // Send the press now and hold the release back for the tap window.
        data->tap_release_swallow = true;
        tap_drag_start_click(cfg, data);
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const enum tap_drag_state prev = data->state;

    tap_drag_disarm(data);
    data->state = TAP_DRAG_IDLE;
    // This press becomes the release of the button that is already held.
    event->value = 0;

    const bool dragged = (prev == TAP_DRAG_RELEASING || prev == TAP_DRAG_DRAGGING) &&
                         data->travel >= cfg->drag_distance;
    if (dragged || prev == TAP_DRAG_ENDING) {
        // A tap straight after a real drag (or after another gesture took
        // over) just lets go, without an extra click at the drop point.
        data->tap_release_swallow = true;
    } else {
        // Tap, tap: end the held click here and start a fresh one with the
        // tap's release, so the host counts a double (or triple) click rather
        // than one long press.
        data->tap_release_to_press = true;
    }
    return ZMK_INPUT_PROC_CONTINUE;
}

static int tap_drag_on_release(const struct tap_drag_config *cfg, struct tap_drag_data *data,
                               struct input_event *event) {
    if (data->hold_forwarded) {
        data->hold_forwarded = false;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (data->hold_swallowed) {
        data->hold_swallowed = false;
        return ZMK_INPUT_PROC_STOP;
    }

    if (data->tap_release_to_press) {
        data->tap_release_to_press = false;
        tap_drag_start_click(cfg, data);
        event->value = 1;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (data->tap_release_swallow) {
        data->tap_release_swallow = false;
        return ZMK_INPUT_PROC_STOP;
    }

    // Unpaired, so not ours to hold back.
    return ZMK_INPUT_PROC_CONTINUE;
}

static int tap_drag_handle_event(const struct device *dev, struct input_event *event,
                                 uint32_t param1, uint32_t param2,
                                 struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    const struct tap_drag_config *cfg = dev->config;
    struct tap_drag_data *data = dev->data;

    if (data->src == NULL) {
        data->src = event->dev;
    }

    if (event->type == INPUT_EV_REL) {
        switch (event->code) {
        case INPUT_REL_X:
        case INPUT_REL_Y:
            if (data->state == TAP_DRAG_DRAGGING && data->travel < cfg->drag_distance) {
                data->travel += abs(event->value);
            }
            break;
        case INPUT_REL_WHEEL:
        case INPUT_REL_HWHEEL:
        case INPUT_REL_MISC:
            tap_drag_abort(data);
            break;
        default:
            break;
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != INPUT_EV_KEY) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    switch (event->code) {
    case TAP_DRAG_TICK_CODE:
        return tap_drag_on_tick(data, event);
    case INPUT_BTN_TOUCH:
        tap_drag_on_touch(cfg, data, event->value != 0);
        return ZMK_INPUT_PROC_CONTINUE;
    case INPUT_BTN_0:
        return event->value ? tap_drag_on_press(cfg, data, event)
                            : tap_drag_on_release(cfg, data, event);
    case INPUT_BTN_1:
    case INPUT_BTN_NORTH:
    case INPUT_BTN_EAST:
    case INPUT_BTN_SOUTH:
    case INPUT_BTN_WEST:
        if (event->value) {
            tap_drag_abort(data);
        }
        return ZMK_INPUT_PROC_CONTINUE;
    default:
        return ZMK_INPUT_PROC_CONTINUE;
    }
}

static int tap_drag_init(const struct device *dev) {
    struct tap_drag_data *data = dev->data;

    k_work_init_delayable(&data->tick_work, tap_drag_tick_work);
    return 0;
}

static const struct zmk_input_processor_driver_api tap_drag_driver_api = {
    .handle_event = tap_drag_handle_event,
};

#define TAP_DRAG_INST(n)                                                                           \
    static struct tap_drag_data tap_drag_data_##n = {};                                            \
    static const struct tap_drag_config tap_drag_config_##n = {                                    \
        .tap_window_ms = DT_INST_PROP(n, tap_window_ms),                                           \
        .release_delay_ms = DT_INST_PROP(n, release_delay_ms),                                     \
        .drag_distance = DT_INST_PROP(n, drag_distance),                                           \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, tap_drag_init, NULL, &tap_drag_data_##n, &tap_drag_config_##n,        \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tap_drag_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TAP_DRAG_INST)

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
