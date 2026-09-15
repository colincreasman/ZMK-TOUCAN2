#include <zephyr/kernel.h>
#include "battery_arc.h"
#include "../assets/custom_fonts.h"

#define DOT_RADIUS      3
#define BATTERY_DOTS    10

static const lv_point_t bat_dots_left[BATTERY_DOTS] = {
    { 14, 53}, { 12, 43}, { 14, 32}, { 21, 24}, { 31, 20},
    { 41, 20}, { 51, 24}, { 58, 32}, { 60, 43}, { 58, 53},
};

static void draw_dot(lv_obj_t *canvas, int cx, int cy, int r, bool filled)
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

static void draw_battery_arc(lv_obj_t *canvas, const lv_point_t *dots, uint8_t level, bool blink_enabled)
{
    /* How many dots should be filled: level 10% -> 1 dot, 100% -> 10 dots */
    int filled = (level + 9) / 10;   /* round up */
    if (filled > BATTERY_DOTS) filled = BATTERY_DOTS;

    for (int i = 0; i < BATTERY_DOTS; i++) {
        draw_dot(canvas, dots[i].x, dots[i].y, DOT_RADIUS, i < filled);
    }
}

static void draw_battery_labels(lv_obj_t *canvas, uint8_t level)
{

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_12, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 9, 31, 56, &label_dsc, "L");

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", level);
    lv_draw_label_dsc_t label_dsc_txt;
    init_label_dsc(&label_dsc_txt, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 9, 45, 56, &label_dsc_txt, buf);
}

void draw_battery_status(lv_obj_t *canvas, const struct status_state *state) {
    draw_battery_arc(canvas, bat_dots_left, state->battery, true);
    draw_battery_labels(canvas, state->battery);
}