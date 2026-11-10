#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zmk/event_manager.h>

struct toucan_os_status {
    uint8_t slot; // 0 = USB; 1 + n = Bluetooth profile n.
    uint8_t mode;
    bool valid;
};

struct toucan_os_status_changed {
    struct toucan_os_status status;
};

ZMK_EVENT_DECLARE(toucan_os_status_changed);

struct toucan_os_status toucan_os_status_get(void);
void toucan_os_status_refresh(void);
