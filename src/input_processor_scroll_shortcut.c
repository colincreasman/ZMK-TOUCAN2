/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_scroll_shortcut

#include <stdint.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>

#include <drivers/behavior.h>
#include <drivers/input_processor.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct scroll_shortcut_config {
    uint16_t event_code;
    int32_t threshold;
    uint32_t reset_ms;
    struct zmk_behavior_binding negative;
    struct zmk_behavior_binding positive;
};

struct scroll_shortcut_data {
    int32_t travel;
    int64_t last_ms;
    bool fired;
};

static void scroll_shortcut_reset(struct scroll_shortcut_data *data) {
    data->travel = 0;
    data->fired = false;
}

static int scroll_shortcut_handle_event(const struct device *dev, struct input_event *event,
                                        uint32_t param1, uint32_t param2,
                                        struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    const struct scroll_shortcut_config *cfg = dev->config;
    struct scroll_shortcut_data *data = dev->data;

    // One actuation per gesture: lifting the fingers arms the next one. This
    // node must run before any behaviors processor bound to INPUT_BTN_TOUCH,
    // which would otherwise consume the event before it arrives here.
    if (event->type == INPUT_EV_KEY && event->code == INPUT_BTN_TOUCH) {
        if (event->value == 0) {
            scroll_shortcut_reset(data);
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != INPUT_EV_REL || event->code != cfg->event_code) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int64_t now = k_uptime_get();
    if (cfg->reset_ms > 0 && data->last_ms > 0 && (now - data->last_ms) > (int64_t)cfg->reset_ms) {
        scroll_shortcut_reset(data);
    }
    data->last_ms = now;

    if (event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    // Already navigated during this gesture: swallow the rest of the stroke so
    // it cannot scroll the page sideways on the way out.
    if (data->fired) {
        event->value = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    data->travel += event->value;

    if (abs(data->travel) < cfg->threshold) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const struct zmk_behavior_binding *binding =
        (data->travel < 0) ? &cfg->negative : &cfg->positive;

    struct zmk_behavior_binding_event behavior_event = {
        .position = INT32_MAX,
        .timestamp = now,
    };

    LOG_DBG("scroll shortcut fired after %d counts", data->travel);

    zmk_behavior_invoke_binding(binding, behavior_event, true);
    zmk_behavior_invoke_binding(binding, behavior_event, false);

    data->fired = true;
    data->travel = 0;
    event->value = 0;

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api scroll_shortcut_driver_api = {
    .handle_event = scroll_shortcut_handle_event,
};

#define SCROLL_SHORTCUT_BINDING(n, idx)                                                            \
    {                                                                                              \
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, idx)),                  \
        .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, idx, param1), (0),          \
                              (DT_INST_PHA_BY_IDX(n, bindings, idx, param1))),                     \
        .param2 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, idx, param2), (0),          \
                              (DT_INST_PHA_BY_IDX(n, bindings, idx, param2))),                     \
    }

#define SCROLL_SHORTCUT_INST(n)                                                                    \
    static struct scroll_shortcut_data scroll_shortcut_data_##n = {};                              \
    static const struct scroll_shortcut_config scroll_shortcut_config_##n = {                      \
        .event_code = DT_INST_PROP(n, event_code),                                                 \
        .threshold = DT_INST_PROP(n, threshold),                                                   \
        .reset_ms = DT_INST_PROP(n, reset_ms),                                                     \
        .negative = SCROLL_SHORTCUT_BINDING(n, 0),                                                 \
        .positive = SCROLL_SHORTCUT_BINDING(n, 1),                                                 \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &scroll_shortcut_data_##n, &scroll_shortcut_config_##n,   \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                        \
                          &scroll_shortcut_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SCROLL_SHORTCUT_INST)

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
