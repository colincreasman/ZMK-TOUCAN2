#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define CONFIG_ZMK_SPLIT 1
#define CONFIG_ZMK_SPLIT_ROLE_CENTRAL TEST_CENTRAL
#define CONFIG_ZMK_BLE 1
#define CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING TEST_CENTRAL
#define IS_ENABLED(x) (x)
#define ARG_UNUSED(x) (void)(x)
#define K_NO_WAIT 0
#define K_SECONDS(n) ((n) * 1000)
#define LOG_MODULE_DECLARE(...)
#define LOG_ERR(...) errors++
#define ZMK_EVENT_DECLARE(...)
#define ZMK_EVENT_IMPL(...)
#define ZMK_LISTENER(...)
#define ZMK_SUBSCRIPTION(...)
#define SYS_INIT(...)
#define DT_INST_FOREACH_STATUS_OKAY(...)
#define DEVICE_DT_NAME(...) "os_status"
#define ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL 255
#define ZMK_BEHAVIOR_OPAQUE 0
#define ZMK_EV_EVENT_BUBBLE 0
#define BEHAVIOR_LOCALITY_GLOBAL 2
#define K_WORK_DELAYABLE_DEFINE(name, fn) struct k_work_delayable name = {.handler = fn}

typedef struct { bool connected; } zmk_event_t;
struct k_work { int unused; };
struct k_work_delayable { void (*handler)(struct k_work *); int delay; };
struct zmk_behavior_binding { const char *behavior_dev; uint32_t param1, param2; };
struct zmk_behavior_binding_event { uint32_t position; int64_t timestamp; uint8_t source; };
struct behavior_driver_api {
    int locality;
    int (*binding_pressed)(struct zmk_behavior_binding *, struct zmk_behavior_binding_event);
    int (*binding_released)(struct zmk_behavior_binding *, struct zmk_behavior_binding_event);
};
struct zmk_split_peripheral_status_changed { bool connected; };
static const struct zmk_split_peripheral_status_changed *
as_zmk_split_peripheral_status_changed(const zmk_event_t *event) {
    static struct zmk_split_peripheral_status_changed result;
    result.connected = event->connected;
    return &result;
}
static unsigned errors, status_events;
static int64_t k_uptime_get(void) { return 42; }
static int k_work_reschedule(struct k_work_delayable *work, int delay) {
    work->delay = delay;
    return 0;
}

#include "../src/toucan_os_mode.h"
#include "../src/toucan_os_status.h"
static uint8_t selected_slot;
static enum toucan_os_mode selected_mode;
uint8_t toucan_os_mode_host_slot(void) { return selected_slot; }
enum toucan_os_mode toucan_os_mode_get(void) { return selected_mode; }
static struct toucan_os_status last_status;
static struct zmk_behavior_binding transmitted;
static int raise_toucan_os_status_changed(struct toucan_os_status_changed event) {
    last_status = event.status;
    status_events++;
    return 0;
}
static int zmk_behavior_invoke_binding(const struct zmk_behavior_binding *,
                                      struct zmk_behavior_binding_event, bool);
#include "../src/behavior_os_status.c"

static int zmk_behavior_invoke_binding(const struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event, bool pressed) {
    assert(pressed);
    assert(strcmp(binding->behavior_dev, "os_status") == 0);
    transmitted = *binding;
    return status_pressed(&transmitted, event);
}

int main(void) {
#if TEST_CENTRAL
    assert(toucan_os_status_get().valid);
    status_sync_init();
    assert(status_sync_work.delay == 1000);
    toucan_os_status_refresh();
    assert(status_sync_work.delay == 0);
    status_sync_work.handler(NULL);
    assert(transmitted.param1 == 0 && transmitted.param2 == TOUCAN_OS_MAC);
    assert(status_events == 1 && last_status.valid);
    selected_slot = 3; // Bluetooth profile 2.
    selected_mode = TOUCAN_OS_WIN;
    status_sync_listener(NULL); // Endpoint/profile or reconnect battery event.
    status_sync_work.handler(NULL);
    assert(transmitted.param1 == 3 && transmitted.param2 == TOUCAN_OS_WIN);
    assert(status_events == 2 && last_status.slot == 3);
    status_sync_work.handler(NULL);
    assert(status_events == 2);
    puts("PASS: central display snapshot, slot/OS refresh, split payload, duplicate suppression");
#else
    assert(!toucan_os_status_get().valid);
    struct zmk_behavior_binding binding = {.param1 = 3, .param2 = TOUCAN_OS_WIN};
    status_pressed(&binding, (struct zmk_behavior_binding_event){0});
    assert(toucan_os_status_get().valid);
    assert(last_status.slot == 3 && last_status.mode == TOUCAN_OS_WIN && status_events == 1);
    status_pressed(&binding, (struct zmk_behavior_binding_event){0});
    assert(status_events == 1);
    zmk_event_t event = {.connected = false};
    status_disconnect_listener(&event);
    assert(!toucan_os_status_get().valid && status_events == 2);
    event.connected = true;
    status_disconnect_listener(&event);
    assert(!toucan_os_status_get().valid); // Never show stale state as current after reconnect.
    status_pressed(&binding, (struct zmk_behavior_binding_event){0});
    assert(toucan_os_status_get().valid && status_events == 3);
    binding.param1 = 16;
    assert(status_pressed(&binding, (struct zmk_behavior_binding_event){0}) == -EINVAL);
    assert(errors == 1 && toucan_os_status_get().slot == 3);
    puts("PASS: peripheral status mirror, duplicate suppression, disconnect invalidation, reconnect");
#endif
    return 0;
}
