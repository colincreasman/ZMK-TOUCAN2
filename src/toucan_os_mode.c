/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "toucan_os_mode.h"
#include "toucan_os_status.h"

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/endpoints.h>
#define TOUCAN_OS_MODE_PER_HOST 1
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
static bool settings_loaded = !IS_ENABLED(CONFIG_SETTINGS);

bool toucan_os_mode_settings_loaded(void) { return settings_loaded; }

// One mode per host: slot 0 is USB and slot 1 + n is Bluetooth profile n, so
// pairing a Mac on one profile and a Windows PC on another switches gestures
// automatically with the profile. The stored layout is fixed-size so it stays
// valid if the profile count changes.
#define TOUCAN_OS_MODE_SLOTS 16

#if IS_ENABLED(CONFIG_TOUCAN_OS_MODE_DEFAULT_WIN)
#define TOUCAN_OS_MODE_DEFAULT TOUCAN_OS_WIN
#else
#define TOUCAN_OS_MODE_DEFAULT TOUCAN_OS_MAC
#endif

static uint8_t modes[TOUCAN_OS_MODE_SLOTS] = {
    [0 ...(TOUCAN_OS_MODE_SLOTS - 1)] = TOUCAN_OS_MODE_DEFAULT,
};

uint8_t toucan_os_mode_host_slot(void) {
#ifdef TOUCAN_OS_MODE_PER_HOST
    struct zmk_endpoint_instance endpoint = zmk_endpoints_selected();

    if (endpoint.transport == ZMK_TRANSPORT_BLE && endpoint.ble.profile_index >= 0 &&
        endpoint.ble.profile_index < TOUCAN_OS_MODE_SLOTS - 1) {
        return 1 + endpoint.ble.profile_index;
    }
#endif
    return 0;
}

enum toucan_os_mode toucan_os_mode_get(void) {
    return modes[toucan_os_mode_host_slot()] == TOUCAN_OS_WIN ? TOUCAN_OS_WIN : TOUCAN_OS_MAC;
}

#if IS_ENABLED(CONFIG_SETTINGS)

static bool per_host_loaded;

static void toucan_os_mode_save_work(struct k_work *work) {
    ARG_UNUSED(work);
    settings_save_one("toucan/os_modes", modes, sizeof(modes));
}

static struct k_work_delayable os_mode_save_work;

static void toucan_os_mode_save(void) {
    // Debounced the same way ZMK persists its own settings, so repeatedly
    // flipping the mode cannot hammer the flash.
    k_work_reschedule(&os_mode_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
}

static int toucan_os_mode_handle_set(const char *name, size_t len, settings_read_cb read_cb,
                                     void *cb_arg) {
    if (settings_name_steq(name, "os_modes", NULL)) {
        if (len != sizeof(modes)) {
            LOG_ERR("Invalid os_modes size (got %d, expected %d)", len, sizeof(modes));
            return -EINVAL;
        }

        int err = read_cb(cb_arg, modes, sizeof(modes));
        if (err <= 0) {
            LOG_ERR("Failed to read os_modes from settings (err %d)", err);
            return err;
        }

        per_host_loaded = true;
        return 0;
    }

    // The single mode stored by earlier firmware seeds every host until the
    // per-host table has been saved once.
    if (settings_name_steq(name, "os_mode", NULL) && !per_host_loaded) {
        enum toucan_os_mode legacy;

        if (len != sizeof(legacy) || read_cb(cb_arg, &legacy, sizeof(legacy)) <= 0) {
            return 0;
        }

        for (int i = 0; i < TOUCAN_OS_MODE_SLOTS; i++) {
            modes[i] = legacy == TOUCAN_OS_WIN ? TOUCAN_OS_WIN : TOUCAN_OS_MAC;
        }
    }

    return 0;
}

static int toucan_os_mode_settings_commit(void) {
    settings_loaded = true;
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_OS_STATUS)
    toucan_os_status_refresh();
#endif
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(toucan_os_mode, "toucan", NULL, toucan_os_mode_handle_set,
                               toucan_os_mode_settings_commit,
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

    const int slot = toucan_os_mode_host_slot();

    if (modes[slot] == mode) {
        return;
    }

    modes[slot] = mode;
    LOG_INF("OS mode for host slot %d set to %s", slot, mode == TOUCAN_OS_WIN ? "windows" : "mac");
    toucan_os_mode_save();
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_OS_STATUS)
    toucan_os_status_refresh();
#endif
}

void toucan_os_mode_toggle(void) {
    toucan_os_mode_set(toucan_os_mode_get() == TOUCAN_OS_MAC ? TOUCAN_OS_WIN : TOUCAN_OS_MAC);
}
