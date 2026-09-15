#include <zephyr/kernel.h>
#include "output_arc.h"
#include "../assets/custom_fonts.h"

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static void draw_usb_connected(lv_obj_t *canvas) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_RIGHT);
    lv_canvas_draw_text(canvas, -18, 143, SCREEN_WIDTH-8, &label_dsc, "USB");
}
#endif

static void draw_ble_disconnected(lv_obj_t *canvas) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_RIGHT);
    lv_canvas_draw_text(canvas, -18, 143, SCREEN_WIDTH-8, &label_dsc, "NULL");
}

static void draw_ble_connected(lv_obj_t *canvas) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_RIGHT);
    lv_canvas_draw_text(canvas, -18, 143, SCREEN_WIDTH-8, &label_dsc, "BLE");
}

void draw_output_status(lv_obj_t *canvas, const struct status_state *state) {
    switch (state->selected_endpoint.transport) {
        case ZMK_TRANSPORT_USB:
            draw_usb_connected(canvas);
            break;
        case ZMK_TRANSPORT_BLE:
            draw_ble_connected(canvas);
            break;
        default:
            draw_ble_disconnected(canvas);
            break;
    }
}