/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_rotate

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>

#include <drivers/input_processor.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define ROTATE_SCALE 10000

// sin(0..90 degrees) scaled by ROTATE_SCALE. Everything else is derived from
// this quadrant, which keeps the rotation exact to a whole degree without
// pulling in floating point.
static const int32_t rotate_sin_table[91] = {
    0,    175,  349,  523,  698,  872,  1045, 1219, 1392, 1564, 1736, 1908, 2079, 2250, 2419, 2588,
    2756, 2924, 3090, 3256, 3420, 3584, 3746, 3907, 4067, 4226, 4384, 4540, 4695, 4848, 5000, 5150,
    5299, 5446, 5592, 5736, 5878, 6018, 6157, 6293, 6428, 6561, 6691, 6820, 6947, 7071, 7193, 7314,
    7431, 7547, 7660, 7771, 7880, 7986, 8090, 8192, 8290, 8387, 8480, 8572, 8660, 8746, 8829, 8910,
    8988, 9063, 9135, 9205, 9272, 9336, 9397, 9455, 9511, 9563, 9613, 9659, 9703, 9744, 9781, 9816,
    9848, 9877, 9903, 9925, 9945, 9962, 9976, 9986, 9994, 9998, 10000,
};

static int32_t rotate_sin(int32_t degrees) {
    degrees %= 360;
    if (degrees < 0) {
        degrees += 360;
    }

    if (degrees <= 90) {
        return rotate_sin_table[degrees];
    }
    if (degrees <= 180) {
        return rotate_sin_table[180 - degrees];
    }
    if (degrees <= 270) {
        return -rotate_sin_table[degrees - 180];
    }
    return -rotate_sin_table[360 - degrees];
}

static int32_t rotate_cos(int32_t degrees) { return rotate_sin(degrees + 90); }

struct rotate_config {
    uint16_t x_code;
    uint16_t y_code;
    uint32_t reset_ms;
    int32_t angle;
};

struct rotate_data {
    // Resolved at init rather than in the config initializer: a function call is
    // not a constant expression, so these cannot be computed statically.
    int32_t sin_val;
    int32_t cos_val;
    int32_t pending_x_in;  // raw X awaiting its matching Y
    int32_t carry_x_out;   // rotated X awaiting the next X event
    int32_t rem_x;
    int32_t rem_y;
    int64_t last_ms;
    bool have_pending;
    bool have_carry;
};

static void rotate_reset(struct rotate_data *data) {
    data->pending_x_in = 0;
    data->carry_x_out = 0;
    data->rem_x = 0;
    data->rem_y = 0;
    data->have_pending = false;
    data->have_carry = false;
}

// Divide with the fractional part kept in *rem, so slow strokes accumulate
// instead of being truncated to nothing.
static int32_t rotate_apply(int32_t scaled, int32_t *rem) {
    scaled += *rem;
    const int32_t out = scaled / ROTATE_SCALE;
    *rem = scaled - (out * ROTATE_SCALE);
    return out;
}

static int rotate_handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                               uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    const struct rotate_config *cfg = dev->config;
    struct rotate_data *data = dev->data;

    // Each new touch starts from a clean buffer. This node must run before any
    // behaviors processor bound to INPUT_BTN_TOUCH, which would otherwise
    // consume the event with ZMK_INPUT_PROC_STOP before it arrives here.
    if (event->type == INPUT_EV_KEY && event->code == INPUT_BTN_TOUCH) {
        if (event->value == 0) {
            rotate_reset(data);
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const bool is_x = (event->code == cfg->x_code);
    const bool is_y = (event->code == cfg->y_code);

    if (!is_x && !is_y) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int64_t now = k_uptime_get();
    if (cfg->reset_ms > 0 && data->last_ms > 0 && (now - data->last_ms) > (int64_t)cfg->reset_ms) {
        rotate_reset(data);
    }
    data->last_ms = now;

    if (is_x) {
        // Hold this sample back until Y arrives, and meanwhile emit the X that
        // was computed for the previous sample.
        data->pending_x_in = event->value;
        data->have_pending = true;

        event->value = data->have_carry ? data->carry_x_out : 0;
        data->carry_x_out = 0;
        data->have_carry = false;

        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int32_t x_in = data->have_pending ? data->pending_x_in : 0;
    const int32_t y_in = event->value;

    data->pending_x_in = 0;
    data->have_pending = false;

    if (x_in == 0 && y_in == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    // Standard 2D rotation. Screen Y grows downward, which is why the signs are
    // mirrored relative to the usual maths-textbook form: a positive angle then
    // reads as counter-clockwise on screen.
    const int32_t x_out = (x_in * data->cos_val) + (y_in * data->sin_val);
    const int32_t y_out = (y_in * data->cos_val) - (x_in * data->sin_val);

    data->carry_x_out = rotate_apply(x_out, &data->rem_x);
    data->have_carry = true;

    event->value = rotate_apply(y_out, &data->rem_y);

    return ZMK_INPUT_PROC_CONTINUE;
}

static int rotate_init(const struct device *dev) {
    const struct rotate_config *cfg = dev->config;
    struct rotate_data *data = dev->data;

    data->sin_val = rotate_sin(cfg->angle);
    data->cos_val = rotate_cos(cfg->angle);

    LOG_DBG("rotate by %d degrees (sin %d, cos %d)", cfg->angle, data->sin_val, data->cos_val);

    return 0;
}

static const struct zmk_input_processor_driver_api rotate_driver_api = {
    .handle_event = rotate_handle_event,
};

#define ROTATE_INST(n)                                                                             \
    static struct rotate_data rotate_data_##n = {};                                                \
    static const struct rotate_config rotate_config_##n = {                                        \
        .x_code = DT_INST_PROP(n, x_code),                                                         \
        .y_code = DT_INST_PROP(n, y_code),                                                         \
        .reset_ms = DT_INST_PROP(n, reset_ms),                                                     \
        /* Devicetree stores ints as raw 32-bit cells, so a negative angle       */                \
        /* arrives as its two's-complement bit pattern. The cast recovers it.    */                \
        .angle = (int32_t)DT_INST_PROP(n, angle),                                                  \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, rotate_init, NULL, &rotate_data_##n, &rotate_config_##n, POST_KERNEL,  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &rotate_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ROTATE_INST)

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
