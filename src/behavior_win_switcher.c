/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_win_switcher

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <dt-bindings/zmk/keys.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/position_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define SWITCHER_NEXT 0
#define SWITCHER_PREV 1

// Windows' Alt+Tab and macOS's AltTab app only stay open while Alt/Option is held, and
// Alt+Shift+Tab on its own does nothing useful. So Alt is held for a short
// session: each swipe taps Tab or Shift+Tab inside it, and Alt is let go once
// the swipes stop, which switches to the selected window.
#define STEP_GAP_MS 12
#define MAX_STEPS 16

struct step {
    uint32_t keycode;
    bool pressed;
};

static const struct device *switcher_dev;
static struct k_work_delayable step_work;
static struct k_spinlock lock;
static struct step steps[MAX_STEPS];
static uint8_t step_head;
static uint8_t step_count;
// Keys this behavior currently holds on the host.
static bool alt_held;
static bool shift_held;
static bool tab_held;
// Taps are queued or Alt is waiting out the session.
static bool session_active;
static int64_t session_deadline;
static uint32_t session_ms;

static void queue_step(uint32_t keycode, bool pressed) {
    if (step_count == MAX_STEPS) {
        LOG_WRN("win-switcher step queue full");
        return;
    }
    steps[(step_head + step_count) % MAX_STEPS] = (struct step){keycode, pressed};
    step_count++;
}

static void queue_tap(uint32_t keycode) {
    queue_step(keycode, true);
    queue_step(keycode, false);
}

static void track_held(uint32_t keycode, bool pressed) {
    if (keycode == LALT) {
        alt_held = pressed;
    } else if (keycode == LSHIFT) {
        shift_held = pressed;
    } else if (keycode == TAB) {
        tab_held = pressed;
    }
}

static void raise_key(uint32_t keycode, bool pressed) {
    raise_zmk_keycode_state_changed_from_encoded(keycode, pressed, k_uptime_get());
}

static void step_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    k_spinlock_key_t key = k_spin_lock(&lock);

    if (step_count > 0) {
        struct step s = steps[step_head];
        step_head = (step_head + 1) % MAX_STEPS;
        step_count--;
        track_held(s.keycode, s.pressed);
        k_spin_unlock(&lock, key);

        raise_key(s.keycode, s.pressed);

        // Decided after the key is sent, under the lock, so a swipe queued
        // meanwhile is not pushed back by the session timeout.
        key = k_spin_lock(&lock);
        session_deadline = k_uptime_get() + session_ms;
        k_work_reschedule(&step_work, K_MSEC(step_count > 0 ? STEP_GAP_MS : session_ms));
        k_spin_unlock(&lock, key);
        return;
    }

    const int64_t remaining = session_deadline - k_uptime_get();
    if (remaining > 0) {
        k_work_reschedule(&step_work, K_MSEC(remaining));
        k_spin_unlock(&lock, key);
        return;
    }

    const bool release_alt = alt_held;
    alt_held = false;
    session_active = false;
    k_spin_unlock(&lock, key);

    if (release_alt) {
        raise_key(LALT, false);
    }
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    k_spinlock_key_t key = k_spin_lock(&lock);

    // A swipe arriving while earlier taps are still queued joins the open
    // session rather than opening a second one.
    const bool session_open = session_active;
    const bool queue_idle = step_count == 0;

    if (!session_open) {
        queue_step(LALT, true);
        queue_tap(TAB);
    }

    if (binding->param1 == SWITCHER_PREV) {
        queue_step(LSHIFT, true);
        queue_tap(TAB);
        if (!session_open) {
            // Alt+Tab selects the next window, and one Shift+Tab would only
            // step back to the current one, so wrap to the far end instead.
            queue_tap(TAB);
        }
        queue_step(LSHIFT, false);
    } else if (session_open) {
        queue_tap(TAB);
    }

    session_active = true;
    if (queue_idle) {
        k_work_reschedule(&step_work, K_NO_WAIT);
    }
    k_spin_unlock(&lock, key);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

// Pressing any key ends the session first, so typing straight after a swipe
// is never sent with Alt held.
static int win_switcher_position_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);

    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    k_spinlock_key_t key = k_spin_lock(&lock);
    const bool active = session_active;
    k_spin_unlock(&lock, key);

    if (!active) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    // Wait out a step that is mid-flight, so nothing is pressed after the
    // cleanup below.
    struct k_work_sync sync;
    k_work_cancel_delayable_sync(&step_work, &sync);

    key = k_spin_lock(&lock);

    const bool release_tab = tab_held;
    const bool release_shift = shift_held;
    const bool release_alt = alt_held;
    step_count = 0;
    tab_held = false;
    shift_held = false;
    alt_held = false;
    session_active = false;
    k_spin_unlock(&lock, key);

    if (release_tab) {
        raise_key(TAB, false);
    }
    if (release_shift) {
        raise_key(LSHIFT, false);
    }
    if (release_alt) {
        raise_key(LALT, false);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(behavior_win_switcher, win_switcher_position_listener);
ZMK_SUBSCRIPTION(behavior_win_switcher, zmk_position_state_changed);

static int behavior_win_switcher_init(const struct device *dev) {
    if (switcher_dev == NULL) {
        switcher_dev = dev;
        session_ms = DT_INST_PROP(0, session_ms);
        k_work_init_delayable(&step_work, step_work_handler);
    }
    return 0;
}

static const struct behavior_driver_api behavior_win_switcher_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define WIN_SWITCHER_INST(n)                                                                       \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_win_switcher_init, NULL, NULL, NULL, POST_KERNEL,          \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_win_switcher_driver_api);

DT_INST_FOREACH_STATUS_OKAY(WIN_SWITCHER_INST)

#endif
