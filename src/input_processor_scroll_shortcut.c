/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_scroll_shortcut

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>

#include <drivers/behavior.h>
#include <drivers/input_processor.h>
#include <zmk/behavior.h>

#include "toucan_trig.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct scroll_shortcut_config {
    uint16_t event_code;
    uint16_t y_event_code;
    bool y_invert;
    int32_t angle;
    int32_t threshold;
    uint32_t reset_ms;
    struct zmk_behavior_binding negative;
    struct zmk_behavior_binding positive;
};

struct scroll_shortcut_data {
    int32_t sin_val;
    int32_t cos_val;
    int32_t travel_x;
    int32_t travel_y;
    int64_t last_ms;
    bool fired;
};

static void scroll_shortcut_reset(struct scroll_shortcut_data *data) {
    data->travel_x = 0;
    data->travel_y = 0;
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

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const bool is_x = (event->code == cfg->event_code);
    const bool is_y = (event->code == cfg->y_event_code);

    if (!is_x && !is_y) {
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
    // it cannot scroll the page on the way out.
    if (data->fired) {
        event->value = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    // The driver reports only the dominant axis per sample, so a swipe that is
    // diagonal in the sensor's frame arrives as an interleaved mix of the two.
    // Accumulating both across the gesture reconstructs the overall direction.
    if (is_x) {
        data->travel_x += event->value;
    } else {
        data->travel_y += cfg->y_invert ? -event->value : event->value;
    }

    // Rotate the accumulated vector into the hand's frame before judging it,
    // using the same convention as the pointer rotation.
    const int64_t hx = ((int64_t)data->travel_x * data->cos_val +
                        (int64_t)data->travel_y * data->sin_val) /
                       TOUCAN_TRIG_SCALE;
    const int64_t hy = ((int64_t)data->travel_y * data->cos_val -
                        (int64_t)data->travel_x * data->sin_val) /
                       TOUCAN_TRIG_SCALE;

    const int64_t hx_mag = (hx < 0) ? -hx : hx;
    const int64_t hy_mag = (hy < 0) ? -hy : hy;

    // Requiring horizontal dominance keeps ordinary vertical scrolling, which
    // also accumulates travel, from ever tripping the shortcut.
    if (hx_mag < cfg->threshold || hx_mag <= hy_mag) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const struct zmk_behavior_binding *binding = (hx < 0) ? &cfg->negative : &cfg->positive;

    struct zmk_behavior_binding_event behavior_event = {
        .position = INT32_MAX,
        .timestamp = now,
    };

    LOG_DBG("scroll shortcut fired: rotated %lld,%lld from raw %d,%d", hx, hy, data->travel_x,
            data->travel_y);

    zmk_behavior_invoke_binding(binding, behavior_event, true);
    zmk_behavior_invoke_binding(binding, behavior_event, false);

    data->fired = true;
    data->travel_x = 0;
    data->travel_y = 0;
    event->value = 0;

    return ZMK_INPUT_PROC_CONTINUE;
}

static int scroll_shortcut_init(const struct device *dev) {
    const struct scroll_shortcut_config *cfg = dev->config;
    struct scroll_shortcut_data *data = dev->data;

    data->sin_val = toucan_sin(cfg->angle);
    data->cos_val = toucan_cos(cfg->angle);

    return 0;
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
        .y_event_code = DT_INST_PROP(n, y_event_code),                                             \
        .y_invert = DT_INST_PROP(n, y_invert),                                                     \
        /* Negative devicetree ints arrive as raw cells; cast recovers the sign. */                \
        .angle = (int32_t)DT_INST_PROP(n, angle),                                                  \
        .threshold = DT_INST_PROP(n, threshold),                                                   \
        .reset_ms = DT_INST_PROP(n, reset_ms),                                                     \
        .negative = SCROLL_SHORTCUT_BINDING(n, 0),                                                 \
        .positive = SCROLL_SHORTCUT_BINDING(n, 1),                                                 \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, scroll_shortcut_init, NULL, &scroll_shortcut_data_##n,                \
                          &scroll_shortcut_config_##n, POST_KERNEL,                                \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &scroll_shortcut_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SCROLL_SHORTCUT_INST)

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
