/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_accel

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

#define ACCEL_SCALE 1000

struct accel_config {
    uint8_t type;
    uint16_t min_factor;
    uint16_t max_factor;
    uint32_t speed_threshold;
    uint32_t speed_max;
    uint8_t exponent;
    uint32_t activation_distance;
    uint32_t activation_reset_ms;
    size_t codes_len;
    uint16_t codes[];
};

struct accel_data {
    int64_t last_event_ms;
    uint32_t travel;
    bool active;
};

static uint32_t accel_factor(const struct accel_config *cfg, uint32_t cps) {
    const uint32_t f_min = cfg->min_factor;
    const uint32_t f_max = (cfg->max_factor > f_min) ? cfg->max_factor : f_min;
    const uint32_t v_min = cfg->speed_threshold;
    const uint32_t v_max = (cfg->speed_max > v_min) ? cfg->speed_max : (v_min + 1);

    if (cps <= v_min) {
        return f_min;
    }

    if (cps >= v_max) {
        return f_max;
    }

    const uint64_t progress = ((uint64_t)(cps - v_min) * ACCEL_SCALE) / (v_max - v_min);
    uint64_t shaped = progress;

    for (uint8_t i = 1; i < cfg->exponent; i++) {
        shaped = (shaped * progress) / ACCEL_SCALE;
    }

    return f_min + (uint32_t)(((uint64_t)(f_max - f_min) * shaped) / ACCEL_SCALE);
}

static void accel_rearm(struct accel_data *data) {
    data->travel = 0;
    data->active = false;
}

static int accel_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                              uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    const struct accel_config *cfg = dev->config;
    struct accel_data *data = dev->data;

    // Re-arm the activation gate the moment the finger leaves the pad, so every
    // new touch has to clear activation-distance again. This node must run before
    // any zmk,input-processor-behaviors node bound to INPUT_BTN_TOUCH, because
    // those return ZMK_INPUT_PROC_STOP and the event never reaches later entries.
    if (event->type == INPUT_EV_KEY && event->code == INPUT_BTN_TOUCH) {
        if (event->value == 0) {
            accel_rearm(data);
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    bool tracked = false;
    for (size_t i = 0; i < cfg->codes_len; i++) {
        if (cfg->codes[i] == event->code) {
            tracked = true;
            break;
        }
    }

    if (!tracked) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int64_t now = k_uptime_get();
    int64_t delta_ms = (data->last_event_ms > 0) ? (now - data->last_event_ms) : 0;

    if (cfg->activation_reset_ms > 0 && delta_ms > (int64_t)cfg->activation_reset_ms) {
        accel_rearm(data);
    }

    data->last_event_ms = now;

    if (event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const uint32_t magnitude = (uint32_t)abs(event->value);

    // Swallow the opening counts of every stroke. X and Y share one accumulator so
    // a diagonal nudge clears the gate at the same rate as a straight one.
    if (!data->active) {
        data->travel += magnitude;
        if (data->travel < cfg->activation_distance) {
            event->value = 0;
            return ZMK_INPUT_PROC_CONTINUE;
        }
        data->active = true;
    }

    if (delta_ms <= 0) {
        delta_ms = 1;
    } else if (delta_ms > 100) {
        delta_ms = 100;
    }

    const uint32_t cps = (uint32_t)(((uint64_t)magnitude * 1000U) / (uint64_t)delta_ms);
    const uint32_t factor = accel_factor(cfg, cps);
    const int32_t raw = event->value;

    int64_t scaled = (int64_t)raw * (int64_t)factor;

    if (state && state->remainder) {
        scaled += *state->remainder;
    }

    int64_t out = scaled / ACCEL_SCALE;

    if (state && state->remainder) {
        *state->remainder = (int16_t)(scaled - (out * ACCEL_SCALE));
    }

    event->value = (int32_t)CLAMP(out, INT16_MIN, INT16_MAX);

    LOG_DBG("accel code %d raw %d at %u cps by %u/%d to %d", event->code, raw, cps, factor,
            ACCEL_SCALE, event->value);

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api accel_driver_api = {
    .handle_event = accel_handle_event,
};

#define ACCEL_INST(n)                                                                              \
    static struct accel_data accel_data_##n = {};                                                  \
    static const struct accel_config accel_config_##n = {                                          \
        .type = DT_INST_PROP(n, type),                                                             \
        .min_factor = DT_INST_PROP(n, min_factor),                                                 \
        .max_factor = DT_INST_PROP(n, max_factor),                                                 \
        .speed_threshold = DT_INST_PROP(n, speed_threshold),                                       \
        .speed_max = DT_INST_PROP(n, speed_max),                                                   \
        .exponent = DT_INST_PROP(n, acceleration_exponent),                                        \
        .activation_distance = DT_INST_PROP(n, activation_distance),                               \
        .activation_reset_ms = DT_INST_PROP(n, activation_reset_ms),                               \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                                   \
        .codes = DT_INST_PROP(n, codes),                                                           \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &accel_data_##n, &accel_config_##n, POST_KERNEL,           \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &accel_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ACCEL_INST)

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
