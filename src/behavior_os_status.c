/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_os_status

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include "toucan_os_mode.h"
#include "toucan_os_status.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
ZMK_EVENT_IMPL(toucan_os_status_changed);

static struct toucan_os_status cached_status;

static void update_status(struct toucan_os_status status) {
    if (cached_status.valid == status.valid && cached_status.slot == status.slot &&
        cached_status.mode == status.mode) {
        return;
    }
    cached_status = status;
    raise_toucan_os_status_changed((struct toucan_os_status_changed){.status = status});
}

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/position_state_changed.h>

struct toucan_os_status toucan_os_status_get(void) {
    return (struct toucan_os_status){
        .slot = toucan_os_mode_host_slot(), .mode = toucan_os_mode_get(), .valid = true};
}

static void status_sync_handler(struct k_work *work) {
    ARG_UNUSED(work);
    const struct toucan_os_status status = toucan_os_status_get();
    const struct zmk_behavior_binding binding = {
        .behavior_dev = DEVICE_DT_NAME(DT_NODELABEL(os_status)),
        .param1 = status.slot,
        .param2 = status.mode,
    };
    const struct zmk_behavior_binding_event event = {
        .position = UINT32_MAX,
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };
    // GLOBAL locality also mirrors the authoritative central state to the left
    // display when that half is a peripheral of the dongle.
    int err = zmk_behavior_invoke_binding(&binding, event, true);
    if (err < 0) {
        LOG_ERR("OS display status sync failed: %d", err);
    }
}

static K_WORK_DELAYABLE_DEFINE(status_sync_work, status_sync_handler);

void toucan_os_status_refresh(void) {
    k_work_reschedule(&status_sync_work, K_NO_WAIT);
}

static int status_sync_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    toucan_os_status_refresh();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(toucan_os_status_sync, status_sync_listener);
ZMK_SUBSCRIPTION(toucan_os_status_sync, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(toucan_os_status_sync, zmk_ble_active_profile_changed);
#endif
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
// Battery discovery/notifications also refresh the mirror after a half
// reconnects, even when no host/profile/OS selection changed.
ZMK_SUBSCRIPTION(toucan_os_status_sync, zmk_peripheral_battery_state_changed);
#endif

static int status_sync_init(void) {
    k_work_reschedule(&status_sync_work, K_SECONDS(1));
    return 0;
}
SYS_INIT(status_sync_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#else
#include <zmk/events/split_peripheral_status_changed.h>

struct toucan_os_status toucan_os_status_get(void) { return cached_status; }

void toucan_os_status_refresh(void) {}

static int status_disconnect_listener(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev =
        as_zmk_split_peripheral_status_changed(eh);
    if (ev != NULL && !ev->connected) {
        update_status((struct toucan_os_status){0});
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(toucan_os_status_disconnect, status_disconnect_listener);
ZMK_SUBSCRIPTION(toucan_os_status_disconnect, zmk_split_peripheral_status_changed);
#endif

static int status_pressed(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 >= 16 || binding->param2 > TOUCAN_OS_WIN) {
        LOG_ERR("Invalid OS display status: slot %u, mode %u", binding->param1, binding->param2);
        return -EINVAL;
    }
    update_status((struct toucan_os_status){
        .slot = binding->param1, .mode = binding->param2, .valid = true});
    return ZMK_BEHAVIOR_OPAQUE;
}

static int status_released(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api status_api = {
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
    .binding_pressed = status_pressed,
    .binding_released = status_released,
};

#define OS_STATUS_INST(n)                                                                          \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                           CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &status_api);
DT_INST_FOREACH_STATUS_OKAY(OS_STATUS_INST)
