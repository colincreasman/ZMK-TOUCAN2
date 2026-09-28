/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 *
 * Runtime host-OS mode. The trackpad gestures send different shortcuts on
 * macOS and Windows, and this keyboard moves between both, so the choice has to
 * be switchable on the fly rather than baked in at build time.
 */

#pragma once

#include <stdint.h>

enum toucan_os_mode {
    TOUCAN_OS_MAC = 0,
    TOUCAN_OS_WIN = 1,
};

enum toucan_os_mode toucan_os_mode_get(void);

void toucan_os_mode_set(enum toucan_os_mode mode);

void toucan_os_mode_toggle(void);
