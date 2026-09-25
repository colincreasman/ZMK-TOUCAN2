/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_swipe_arbiter

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>

#include <drivers/input_processor.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

enum swipe_dir {
    SWIPE_DIR_NORTH = 0,
    SWIPE_DIR_EAST,
    SWIPE_DIR_SOUTH,
    SWIPE_DIR_WEST,
    SWIPE_DIR_COUNT,
};

struct arbiter_config {
    uint8_t lead;
    uint8_t max_samples;
    uint32_t reset_ms;
};

struct arbiter_data {
    uint8_t counts[SWIPE_DIR_COUNT];
    int64_t last_ms;
    uint16_t chosen_code;
    bool decided;
    bool emitted;
    bool await_release;
};

static int swipe_dir_of(uint16_t code) {
    switch (code) {
    case INPUT_BTN_NORTH:
        return SWIPE_DIR_NORTH;
    case INPUT_BTN_EAST:
        return SWIPE_DIR_EAST;
    case INPUT_BTN_SOUTH:
        return SWIPE_DIR_SOUTH;
    case INPUT_BTN_WEST:
        return SWIPE_DIR_WEST;
    default:
        return -1;
    }
}

static void arbiter_reset(struct arbiter_data *data) {
    for (size_t i = 0; i < SWIPE_DIR_COUNT; i++) {
        data->counts[i] = 0;
    }
    data->chosen_code = 0;
    data->decided = false;
    data->emitted = false;
    data->await_release = false;
}

static int arbiter_handle_event(const struct device *dev, struct input_event *event,
                                uint32_t param1, uint32_t param2,
                                struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    const struct arbiter_config *cfg = dev->config;
    struct arbiter_data *data = dev->data;

    if (event->type != INPUT_EV_KEY) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    // A gesture ends when the fingers leave the pad, so the next swipe starts
    // from a clean slate. This node must run before any behaviors processor
    // bound to INPUT_BTN_TOUCH, which would otherwise consume the event first.
    if (event->code == INPUT_BTN_TOUCH) {
        if (event->value == 0) {
            arbiter_reset(data);
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int dir = swipe_dir_of(event->code);
    if (dir < 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int64_t now = k_uptime_get();
    if (cfg->reset_ms > 0 && data->last_ms > 0 && (now - data->last_ms) > (int64_t)cfg->reset_ms) {
        arbiter_reset(data);
    }
    data->last_ms = now;

    // Only the release that pairs with the one press we let through may pass.
    if (event->value == 0) {
        if (data->await_release && event->code == data->chosen_code) {
            data->await_release = false;
            return ZMK_INPUT_PROC_CONTINUE;
        }
        return ZMK_INPUT_PROC_STOP;
    }

    if (data->emitted) {
        return ZMK_INPUT_PROC_STOP;
    }

    data->counts[dir]++;

    if (!data->decided) {
        const uint32_t vertical = data->counts[SWIPE_DIR_NORTH] + data->counts[SWIPE_DIR_SOUTH];
        const uint32_t horizontal = data->counts[SWIPE_DIR_EAST] + data->counts[SWIPE_DIR_WEST];
        const uint32_t total = vertical + horizontal;
        const uint32_t diff =
            (vertical > horizontal) ? (vertical - horizontal) : (horizontal - vertical);

        const bool settled = (diff >= cfg->lead) ||
                             (total >= cfg->max_samples && vertical != horizontal);
        if (!settled) {
            return ZMK_INPUT_PROC_STOP;
        }

        if (vertical > horizontal) {
            data->chosen_code = (data->counts[SWIPE_DIR_NORTH] >= data->counts[SWIPE_DIR_SOUTH])
                                    ? INPUT_BTN_NORTH
                                    : INPUT_BTN_SOUTH;
        } else {
            data->chosen_code = (data->counts[SWIPE_DIR_EAST] >= data->counts[SWIPE_DIR_WEST])
                                    ? INPUT_BTN_EAST
                                    : INPUT_BTN_WEST;
        }

        data->decided = true;
        LOG_DBG("swipe committed to code %d after %u samples", data->chosen_code, total);
    }

    // Wait for a sample that actually matches the committed direction.
    if (event->code != data->chosen_code) {
        return ZMK_INPUT_PROC_STOP;
    }

    data->emitted = true;
    data->await_release = true;

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api arbiter_driver_api = {
    .handle_event = arbiter_handle_event,
};

#define ARBITER_INST(n)                                                                            \
    static struct arbiter_data arbiter_data_##n = {};                                              \
    static const struct arbiter_config arbiter_config_##n = {                                      \
        .lead = DT_INST_PROP(n, lead),                                                             \
        .max_samples = DT_INST_PROP(n, max_samples),                                               \
        .reset_ms = DT_INST_PROP(n, reset_ms),                                                     \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &arbiter_data_##n, &arbiter_config_##n, POST_KERNEL,       \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &arbiter_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ARBITER_INST)

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
