/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_hyper_guard

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <dt-bindings/zmk/modifiers.h>
#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define MODS_CTRL (MOD_LCTL | MOD_RCTL)
#define MODS_SHIFT (MOD_LSFT | MOD_RSFT)
#define MODS_ALT (MOD_LALT | MOD_RALT)
#define MODS_GUI (MOD_LGUI | MOD_RGUI)

// Positions whose press was swallowed, so their release is swallowed too.
static uint64_t blocked_positions;

static bool hyper_held(void) {
    const zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();

    return (mods & MODS_CTRL) && (mods & MODS_SHIFT) && (mods & MODS_ALT) && (mods & MODS_GUI);
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    // All four homerow mods resolving as holds plus this key is macOS's
    // sysdiagnose chord (Ctrl+Opt+Shift+Cmd+.), never an intended shortcut.
    if (hyper_held() && event.position < 64) {
        LOG_DBG("hyper-guard: dropped 0x%02X at position %d", binding->param1, event.position);
        blocked_positions |= BIT64(event.position);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    return raise_zmk_keycode_state_changed_from_encoded(binding->param1, true, event.timestamp);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    if (event.position < 64 && (blocked_positions & BIT64(event.position))) {
        blocked_positions &= ~BIT64(event.position);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    return raise_zmk_keycode_state_changed_from_encoded(binding->param1, false, event.timestamp);
}

static const struct behavior_driver_api behavior_hyper_guard_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define HYPER_GUARD_INST(n)                                                                        \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_hyper_guard_driver_api);

DT_INST_FOREACH_STATUS_OKAY(HYPER_GUARD_INST)

#endif
