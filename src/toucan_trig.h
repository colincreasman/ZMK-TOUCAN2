/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 *
 * Fixed-point trigonometry shared by the input processors that need to rotate
 * a vector out of the trackpad's frame and into the hand's. Kept integer-only:
 * these run in the input hot path, where floating point is undesirable.
 */

#pragma once

#include <stdint.h>

#define TOUCAN_TRIG_SCALE 10000

// sin(0..90 degrees) scaled by TOUCAN_TRIG_SCALE. Every other quadrant is
// derived from this one, which keeps the result exact to a whole degree.
static const int32_t toucan_sin_table[91] = {
    0,    175,  349,  523,  698,  872,  1045, 1219, 1392, 1564, 1736, 1908, 2079, 2250, 2419, 2588,
    2756, 2924, 3090, 3256, 3420, 3584, 3746, 3907, 4067, 4226, 4384, 4540, 4695, 4848, 5000, 5150,
    5299, 5446, 5592, 5736, 5878, 6018, 6157, 6293, 6428, 6561, 6691, 6820, 6947, 7071, 7193, 7314,
    7431, 7547, 7660, 7771, 7880, 7986, 8090, 8192, 8290, 8387, 8480, 8572, 8660, 8746, 8829, 8910,
    8988, 9063, 9135, 9205, 9272, 9336, 9397, 9455, 9511, 9563, 9613, 9659, 9703, 9744, 9781, 9816,
    9848, 9877, 9903, 9925, 9945, 9962, 9976, 9986, 9994, 9998, 10000,
};

static inline int32_t toucan_sin(int32_t degrees) {
    degrees %= 360;
    if (degrees < 0) {
        degrees += 360;
    }

    if (degrees <= 90) {
        return toucan_sin_table[degrees];
    }
    if (degrees <= 180) {
        return toucan_sin_table[180 - degrees];
    }
    if (degrees <= 270) {
        return -toucan_sin_table[degrees - 180];
    }
    return -toucan_sin_table[360 - degrees];
}

static inline int32_t toucan_cos(int32_t degrees) { return toucan_sin(degrees + 90); }
