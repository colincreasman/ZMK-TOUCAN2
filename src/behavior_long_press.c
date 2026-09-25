/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_long_press

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_long_press_config {
    uint32_t duration_ms;
    struct zmk_behavior_binding behavior;
};

struct behavior_long_press_data {
    const struct device *dev;
    struct k_work_delayable work;
    struct zmk_behavior_binding_event event;
    bool active;
};

static void long_press_work_handler(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct behavior_long_press_data *data =
        CONTAINER_OF(delayable, struct behavior_long_press_data, work);
    const struct behavior_long_press_config *config = data->dev->config;

    if (!data->active) {
        return;
    }

    data->active = false;
    zmk_behavior_invoke_binding(&config->behavior, data->event, true);
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_long_press_config *config = dev->config;
    struct behavior_long_press_data *data = dev->data;

    data->event = event;
    data->active = true;

    int32_t duration_left = (event.timestamp + config->duration_ms) - k_uptime_get();
    k_work_reschedule(&data->work, K_MSEC(MAX(duration_left, 0)));

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_long_press_data *data = dev->data;

    data->active = false;
    k_work_cancel_delayable(&data->work);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int behavior_long_press_init(const struct device *dev) {
    struct behavior_long_press_data *data = dev->data;

    data->dev = dev;
    data->active = false;
    k_work_init_delayable(&data->work, long_press_work_handler);

    return 0;
}

static const struct behavior_driver_api behavior_long_press_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define LONG_PRESS_INST(n)                                                                         \
    static struct behavior_long_press_data behavior_long_press_data_##n;                           \
    static const struct behavior_long_press_config behavior_long_press_config_##n = {              \
        .duration_ms = DT_INST_PROP(n, duration_ms),                                                \
        .behavior = ZMK_KEYMAP_EXTRACT_BINDING(0, DT_DRV_INST(n)),                                 \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_long_press_init, NULL, &behavior_long_press_data_##n,       \
                            &behavior_long_press_config_##n, POST_KERNEL,                           \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_long_press_driver_api);

DT_INST_FOREACH_STATUS_OKAY(LONG_PRESS_INST)

#endif
