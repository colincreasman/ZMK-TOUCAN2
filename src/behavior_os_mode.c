/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_os_mode

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include "toucan_os_mode.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define OS_MODE_ACTION_MAC 0
#define OS_MODE_ACTION_WIN 1
#define OS_MODE_ACTION_TOGGLE 2

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    switch (binding->param1) {
    case OS_MODE_ACTION_MAC:
        toucan_os_mode_set(TOUCAN_OS_MAC);
        break;
    case OS_MODE_ACTION_WIN:
        toucan_os_mode_set(TOUCAN_OS_WIN);
        break;
    case OS_MODE_ACTION_TOGGLE:
        toucan_os_mode_toggle();
        break;
    default:
        LOG_WRN("Unknown os-mode action %d", binding->param1);
        return -EINVAL;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int behavior_os_mode_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

static const struct behavior_driver_api behavior_os_mode_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define OS_MODE_INST(n)                                                                            \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_os_mode_init, NULL, NULL, NULL, POST_KERNEL,               \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_os_mode_driver_api);

DT_INST_FOREACH_STATUS_OKAY(OS_MODE_INST)

#endif
