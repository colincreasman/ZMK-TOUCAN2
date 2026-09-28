/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_os_select

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include "toucan_os_mode.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_os_select_config {
    struct zmk_behavior_binding bindings[2];
};

struct behavior_os_select_data {
    // Which sub-binding the press resolved to. Remembered so a mode change
    // between press and release cannot leave a modifier stuck down.
    uint8_t held_index;
    bool held;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_os_select_config *config = dev->config;
    struct behavior_os_select_data *data = dev->data;

    data->held_index = (toucan_os_mode_get() == TOUCAN_OS_WIN) ? 1 : 0;
    data->held = true;

    return zmk_behavior_invoke_binding(&config->bindings[data->held_index], event, true);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_os_select_config *config = dev->config;
    struct behavior_os_select_data *data = dev->data;

    if (!data->held) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    data->held = false;

    return zmk_behavior_invoke_binding(&config->bindings[data->held_index], event, false);
}

static int behavior_os_select_init(const struct device *dev) {
    struct behavior_os_select_data *data = dev->data;

    data->held = false;
    data->held_index = 0;

    return 0;
}

static const struct behavior_driver_api behavior_os_select_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define OS_SELECT_INST(n)                                                                          \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, bindings) == 2,                                               \
                 "os-select needs exactly two bindings: mac then windows");                        \
    static struct behavior_os_select_data behavior_os_select_data_##n;                             \
    static const struct behavior_os_select_config behavior_os_select_config_##n = {                \
        .bindings =                                                                                \
            {                                                                                      \
                ZMK_KEYMAP_EXTRACT_BINDING(0, DT_DRV_INST(n)),                                     \
                ZMK_KEYMAP_EXTRACT_BINDING(1, DT_DRV_INST(n)),                                     \
            },                                                                                     \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_os_select_init, NULL, &behavior_os_select_data_##n,        \
                            &behavior_os_select_config_##n, POST_KERNEL,                           \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_os_select_driver_api);

DT_INST_FOREACH_STATUS_OKAY(OS_SELECT_INST)

#endif
