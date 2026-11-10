/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <dt-bindings/zmk/bt.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/physical_layouts.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Studio bindings survive flashing. Migrate only the complete old maintenance
// layout, leaving unrelated saved bindings, Bluetooth bonds and OS slots alone.
int toucan_maintenance_migrate(void) {
    // This work runs after the Toucan settings commit. Taking the settings
    // lock waits out the full boot load, including keymap local-ID resolution
    // and the selected physical layout, regardless of handler commit order.
    int loaded = settings_load_subtree("keymap");
    if (loaded < 0) {
        LOG_ERR("Cannot load saved maintenance bindings: %d", loaded);
        return loaded;
    }
    const uint32_t *map;
    int len = zmk_physical_layouts_get_selected_to_stock_position_map(&map);
    if (len < 0) {
        LOG_ERR("Cannot read maintenance key position map: %d", len);
        return len;
    }
    static const uint8_t positions[] = {2, 19, 20, 31}; // W, J, K, M.
    uint8_t indices[4];
    struct zmk_behavior_binding current[4];
    for (size_t p = 0; p < 4; p++) {
        int idx;
        for (idx = 0; idx < len && map[idx] != positions[p]; idx++) {}
        if (idx == len) {
            LOG_WRN("Maintenance migration skipped: position %u is not in the selected layout",
                    positions[p]);
            return 0;
        }
        indices[p] = idx;
        const struct zmk_behavior_binding *binding =
            zmk_keymap_get_layer_binding_at_idx(3, indices[p]);
        if (binding == NULL) {
            LOG_ERR("Cannot read maintenance binding at position %u", positions[p]);
            return -EINVAL;
        }
        current[p] = *binding;
    }
    const char *expected[] = {
        DEVICE_DT_NAME(DT_NODELABEL(bt)), DEVICE_DT_NAME(DT_NODELABEL(os_mode)),
        DEVICE_DT_NAME(DT_NODELABEL(os_mode)), DEVICE_DT_NAME(DT_NODELABEL(none))};
    const uint32_t params[] = {BT_CLR_CMD, 0, 1, 0};
    for (size_t p = 0; p < 4; p++) {
        if (current[p].behavior_dev == NULL || strcmp(current[p].behavior_dev, expected[p]) != 0 ||
            current[p].param1 != params[p] || current[p].param2 != 0) {
            return 0; // Already current, or a different user customization.
        }
    }
    // Reuse the loaded binding structs so behavior/local-ID fields stay valid.
    struct zmk_behavior_binding updated[] = {current[2], current[3], current[0], current[1]};
    for (size_t p = 0; p < 4; p++) {
        int err = zmk_keymap_set_layer_binding_at_idx(3, indices[p], updated[p]);
        if (err < 0) {
            LOG_ERR("Cannot migrate maintenance binding at position %u: %d", positions[p], err);
            // Restore in-memory bindings rather than leave a partial remap.
            for (size_t i = 0; i < p; i++) {
                int restore = zmk_keymap_set_layer_binding_at_idx(3, indices[i], current[i]);
                if (restore < 0) {
                    LOG_ERR("Cannot restore maintenance position %u: %d", positions[i], restore);
                }
            }
            return err;
        }
    }
    int err = zmk_keymap_save_changes();
    if (err < 0) {
        LOG_ERR("Maintenance remap applied but not saved: %d", err);
        return err;
    }
    LOG_INF("Migrated saved maintenance keys: W=Windows, M=Mac, K=Bluetooth clear");
    return 0;
}
