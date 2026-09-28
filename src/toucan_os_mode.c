/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "toucan_os_mode.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// TOUCAN_WIN_MODE in toucan.dtsi now only seeds the power-on default; a stored
// setting, if any, overrides it at boot.
#if IS_ENABLED(CONFIG_TOUCAN_OS_MODE_DEFAULT_WIN)
static enum toucan_os_mode current_mode = TOUCAN_OS_WIN;
#else
static enum toucan_os_mode current_mode = TOUCAN_OS_MAC;
#endif

enum toucan_os_mode toucan_os_mode_get(void) { return current_mode; }

#if IS_ENABLED(CONFIG_SETTINGS)

static void toucan_os_mode_save_work(struct k_work *work) {
    ARG_UNUSED(work);
    settings_save_one("toucan/os_mode", &current_mode, sizeof(current_mode));
}

static struct k_work_delayable os_mode_save_work;

static void toucan_os_mode_save(void) {
    // Debounced the same way ZMK persists its own settings, so repeatedly
    // flipping the mode cannot hammer the flash.
    k_work_reschedule(&os_mode_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
}

static int toucan_os_mode_handle_set(const char *name, size_t len, settings_read_cb read_cb,
                                     void *cb_arg) {
    if (!settings_name_steq(name, "os_mode", NULL)) {
        return 0;
    }

    if (len != sizeof(current_mode)) {
        LOG_ERR("Invalid os_mode size (got %d, expected %d)", len, sizeof(current_mode));
        return -EINVAL;
    }

    int err = read_cb(cb_arg, &current_mode, sizeof(current_mode));
    if (err <= 0) {
        LOG_ERR("Failed to read os_mode from settings (err %d)", err);
        return err;
    }

    LOG_INF("Restored OS mode %d", current_mode);

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(toucan_os_mode, "toucan", NULL, toucan_os_mode_handle_set, NULL,
                               NULL);

static int toucan_os_mode_init(void) {
    k_work_init_delayable(&os_mode_save_work, toucan_os_mode_save_work);
    return 0;
}

SYS_INIT(toucan_os_mode_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#else

static void toucan_os_mode_save(void) {}

#endif /* IS_ENABLED(CONFIG_SETTINGS) */

void toucan_os_mode_set(enum toucan_os_mode mode) {
    if (mode != TOUCAN_OS_MAC && mode != TOUCAN_OS_WIN) {
        return;
    }

    if (mode == current_mode) {
        return;
    }

    current_mode = mode;
    LOG_INF("OS mode set to %s", mode == TOUCAN_OS_WIN ? "windows" : "mac");
    toucan_os_mode_save();
}

void toucan_os_mode_toggle(void) {
    toucan_os_mode_set(current_mode == TOUCAN_OS_MAC ? TOUCAN_OS_WIN : TOUCAN_OS_MAC);
}
