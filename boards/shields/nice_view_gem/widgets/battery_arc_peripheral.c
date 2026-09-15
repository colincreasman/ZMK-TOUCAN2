#include <zephyr/kernel.h>
#include "battery_arc_peripheral.h"
#include "../assets/custom_fonts.h"

#define DOT_RADIUS      3
#define BATTERY_DOTS    10

static const lv_point_t bat_dots_peripheral[BATTERY_DOTS] = {
    { 86, 53}, { 84, 43}, { 86, 32}, { 93, 24}, {103, 20},
    {113, 20}, {123, 24}, {130, 32}, {132, 43}, {130, 53},
};

static void draw_dot_peripheral(lv_obj_t *canvas, int cx, int cy, int r, bool filled)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);

    if (filled) {
        dsc.bg_color   = lv_color_white();
        dsc.bg_opa     = LV_OPA_COVER;
        dsc.border_width = 0;
    } else {
        dsc.bg_color     = lv_color_black();
        dsc.bg_opa       = LV_OPA_COVER;
        dsc.border_color = lv_color_white();
        dsc.border_width = 1;
        dsc.border_opa   = LV_OPA_COVER;
    }

    dsc.radius = r; 

    lv_canvas_draw_rect(canvas, cx - r, cy - r, r * 2, r * 2, &dsc);
}

static void draw_battery_arc_peripheral(lv_obj_t *canvas, const lv_point_t *dots, uint8_t level, bool blink_enabled)
{
    int filled = (level + 9) / 10;
    if (filled > BATTERY_DOTS) filled = BATTERY_DOTS;

    for (int i = 0; i < BATTERY_DOTS; i++) {
        draw_dot_peripheral(canvas, dots[i].x, dots[i].y, DOT_RADIUS, i < filled);
    }
}

static void draw_battery_labels_peripheral(lv_obj_t *canvas, uint8_t level)
{
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_12, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 81, 31, 56, &label_dsc, "R");

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", level);
    lv_draw_label_dsc_t label_dsc_txt;
    init_label_dsc(&label_dsc_txt, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 81, 45, 56, &label_dsc_txt, buf);
}

void draw_battery_peripheral_status(lv_obj_t *canvas, const struct status_state *state) {
    uint8_t level = state->battery_p;
    if (level > 0) {
        draw_battery_arc_peripheral(canvas, bat_dots_peripheral, level, true);
        draw_battery_labels_peripheral(canvas, level);
    }
}