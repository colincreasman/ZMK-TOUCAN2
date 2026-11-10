/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_win_switcher

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <dt-bindings/toucan/gestures.h>
#include <dt-bindings/zmk/keys.h>
#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>

#include "toucan_os_mode.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define MAX_STEPS 64

struct step {
    uint32_t keycode;
    bool pressed;
};

static void step_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(step_work, step_work_handler);
static K_MUTEX_DEFINE(switcher_lock);
static struct step steps[MAX_STEPS];
static uint8_t step_head;
static uint8_t step_count;
static int64_t next_step_at;
static bool contact_active;
static bool terminal_action;
static bool switcher_open;
static bool mac_mode;
static bool alt_queued;
static bool alt_held;
static bool shift_held;
static bool tab_held;
static bool gui_held;
static bool ctrl_held;
static bool w_held;
static bool grave_held;

static void send_key(uint32_t keycode, bool pressed) {
    switch (keycode) {
    case LALT:
        alt_held = pressed;
        break;
    case LSHIFT:
        shift_held = pressed;
        break;
    case TAB:
        tab_held = pressed;
        break;
    case LGUI:
        gui_held = pressed;
        break;
    case LCTRL:
        ctrl_held = pressed;
        break;
    case W:
        w_held = pressed;
        break;
    case GRAVE:
        grave_held = pressed;
        break;
    }
    raise_zmk_keycode_state_changed_from_encoded(keycode, pressed, k_uptime_get());
}

static void queue_step(uint32_t keycode, bool pressed) {
    steps[(step_head + step_count) % MAX_STEPS] = (struct step){keycode, pressed};
    step_count++;
}

static void queue_tap(uint32_t keycode) {
    queue_step(keycode, true);
    queue_step(keycode, false);
}

static void start_steps(void) {
    const int64_t delay = MAX(0, next_step_at - k_uptime_get());
    k_work_reschedule(&step_work, K_MSEC(delay));
}

// Callers reserve a whole action before queuing any of it. On overflow, fail
// visibly and release everything this behavior owns rather than strand a key.
static bool reserve_steps(uint8_t count) {
    if (step_count + count <= MAX_STEPS) {
        return true;
    }
    LOG_ERR("three-finger gesture queue full; cancelling contact");
    k_work_cancel_delayable(&step_work);
    step_count = 0;
    contact_active = false;
    alt_queued = false;
    if (tab_held) {
        send_key(TAB, false);
    }
    if (w_held) {
        send_key(W, false);
    }
    if (grave_held) {
        send_key(GRAVE, false);
    }
    if (shift_held) {
        send_key(LSHIFT, false);
    }
    if (gui_held) {
        send_key(LGUI, false);
    }
    if (ctrl_held) {
        send_key(LCTRL, false);
    }
    if (alt_held) {
        send_key(LALT, false);
    }
    return false;
}

static void step_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&switcher_lock, K_FOREVER);
    if (step_count > 0) {
        struct step s = steps[step_head];
        step_head = (step_head + 1) % MAX_STEPS;
        step_count--;
        send_key(s.keycode, s.pressed);
        next_step_at = k_uptime_get() + DT_INST_PROP(0, tap_ms);
        if (step_count > 0) {
            k_work_reschedule(&step_work, K_MSEC(DT_INST_PROP(0, tap_ms)));
        }
    }
    k_mutex_unlock(&switcher_lock);
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 > TOUCAN_GESTURE_CONTACT) {
        LOG_ERR("Invalid three-finger gesture %u", binding->param1);
        return -EINVAL;
    }

    k_mutex_lock(&switcher_lock, K_FOREVER);
    const bool idle = step_count == 0;
    if (binding->param1 == TOUCAN_GESTURE_CONTACT) {
        if (!contact_active) {
            if (!reserve_steps(1)) {
                k_mutex_unlock(&switcher_lock);
                return -ENOSPC;
            }
            contact_active = true;
            terminal_action = false;
            switcher_open = false;
            mac_mode = toucan_os_mode_get() == TOUCAN_OS_MAC;
            alt_queued = true;
            queue_step(LALT, true);
        }
    } else if (contact_active && !terminal_action) {
        // The longest action is the first Mac left: Tab, then Shift+Tab.
        if (!reserve_steps(6)) {
            k_mutex_unlock(&switcher_lock);
            return -ENOSPC;
        }
        switch (binding->param1) {
        case TOUCAN_GESTURE_RIGHT:
            queue_tap(TAB);
            switcher_open = true;
            break;
        case TOUCAN_GESTURE_LEFT:
            if (mac_mode && !switcher_open) {
                queue_tap(TAB);
            }
            queue_step(LSHIFT, true);
            queue_tap(TAB);
            queue_step(LSHIFT, false);
            switcher_open = true;
            break;
        case TOUCAN_GESTURE_UP:
            terminal_action = true;
            if (mac_mode) {
                queue_tap(GRAVE);
            } else {
                queue_step(LALT, false);
                alt_queued = false;
                queue_step(LGUI, true);
                queue_tap(TAB);
                queue_step(LGUI, false);
            }
            break;
        case TOUCAN_GESTURE_DOWN:
            terminal_action = true;
            queue_step(LALT, false);
            alt_queued = false;
            const uint32_t mod = mac_mode ? LGUI : LCTRL;
            queue_step(mod, true);
            queue_tap(W);
            queue_step(mod, false);
            break;
        }
    }
    if (idle && step_count > 0) {
        start_steps();
    }
    k_mutex_unlock(&switcher_lock);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 != TOUCAN_GESTURE_CONTACT) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    k_mutex_lock(&switcher_lock, K_FOREVER);
    contact_active = false;
    // Finish an already accepted swipe even if the fingers lift during its
    // short key sequence, then release Alt. No timer keeps it held after that.
    if (alt_queued && reserve_steps(1)) {
        const bool idle = step_count == 0;
        queue_step(LALT, false);
        alt_queued = false;
        if (idle) {
            start_steps();
        }
    }
    k_mutex_unlock(&switcher_lock);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_win_switcher_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define WIN_SWITCHER_INST(n)                                                                       \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                           CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                           &behavior_win_switcher_driver_api);

DT_INST_FOREACH_STATUS_OKAY(WIN_SWITCHER_INST)

#endif
