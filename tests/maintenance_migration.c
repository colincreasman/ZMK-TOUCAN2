#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define LOG_MODULE_DECLARE(...)
#define LOG_ERR(...) errors++
#define LOG_WRN(...)
#define LOG_INF(...)
#define DT_NODELABEL(name) #name
#define DEVICE_DT_NAME(name) (name)
#define BT_CLR_CMD 0
struct zmk_behavior_binding {
    const char *behavior_dev;
    uint32_t param1, param2, local_id;
};
static uint32_t map[42];
static struct zmk_behavior_binding bindings[42];
static unsigned writes, saves, errors;
static int fail_at = -1;
static int zmk_physical_layouts_get_selected_to_stock_position_map(const uint32_t **out) {
    *out = map;
    return 42;
}
static const struct zmk_behavior_binding *zmk_keymap_get_layer_binding_at_idx(uint8_t layer,
                                                                           uint8_t index) {
    assert(layer == 3);
    return &bindings[index];
}
static int zmk_keymap_set_layer_binding_at_idx(uint8_t layer, uint8_t index,
                                              struct zmk_behavior_binding binding) {
    assert(layer == 3);
    if (index == fail_at) {
        fail_at = -1;
        return -EIO;
    }
    bindings[index] = binding;
    writes++;
    return 0;
}
static int zmk_keymap_save_changes(void) { saves++; return 0; }
static int settings_load_subtree(const char *name) {
    assert(strcmp(name, "keymap") == 0);
    return 0;
}
#include "../src/toucan_maintenance_migration.c"

static void old_layout(void) {
    writes = saves = errors = 0;
    for (int i = 0; i < 42; i++) {
        map[i] = i;
        bindings[i] = (struct zmk_behavior_binding){"custom", (uint32_t)i, 0, 5};
    }
    bindings[2] = (struct zmk_behavior_binding){"bt", BT_CLR_CMD, 0, 1};
    bindings[19] = (struct zmk_behavior_binding){"os_mode", 0, 0, 2};
    bindings[20] = (struct zmk_behavior_binding){"os_mode", 1, 0, 2};
    bindings[31] = (struct zmk_behavior_binding){"none", 0, 0, 3};
}

int main(void) {
    old_layout();
    assert(toucan_maintenance_migrate() == 0);
    assert(writes == 4 && saves == 1);
    assert(strcmp(bindings[2].behavior_dev, "os_mode") == 0 && bindings[2].param1 == 1);
    assert(strcmp(bindings[19].behavior_dev, "none") == 0);
    assert(strcmp(bindings[20].behavior_dev, "bt") == 0);
    assert(strcmp(bindings[31].behavior_dev, "os_mode") == 0 && bindings[31].param1 == 0);
    assert(bindings[16].param1 == 16 && strcmp(bindings[16].behavior_dev, "custom") == 0);
    assert(toucan_maintenance_migrate() == 0 && writes == 4 && saves == 1);
    old_layout();
    bindings[2].behavior_dev = "user_macro";
    assert(toucan_maintenance_migrate() == 0 && writes == 0 && saves == 0);
    old_layout();
    fail_at = 20;
    assert(toucan_maintenance_migrate() == -EIO);
    assert(saves == 0 && errors == 1);
    assert(strcmp(bindings[2].behavior_dev, "bt") == 0);
    assert(strcmp(bindings[19].behavior_dev, "os_mode") == 0);
    old_layout();
    // An alternate visual key order must still migrate physical W/J/K/M.
    struct zmk_behavior_binding temporary = bindings[2];
    bindings[2] = bindings[10]; bindings[10] = temporary;
    map[2] = 10; map[10] = 2;
    assert(toucan_maintenance_migrate() == 0);
    assert(strcmp(bindings[10].behavior_dev, "os_mode") == 0 && bindings[10].param1 == 1);
    assert(strcmp(bindings[2].behavior_dev, "custom") == 0);
    puts("PASS: saved maintenance migration, idempotence, custom keys preserved, rollback, layout map");
    return 0;
}
