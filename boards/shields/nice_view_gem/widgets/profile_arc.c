#include <zephyr/kernel.h>
#include "profile_arc.h"

#define BT_DOT_X        126
#define BT_DOT_START_Y  105
#define BT_DOT_SIZE     8
#define BT_DOT_SPACING  10
#define BT_N_PROFILES   5

static void draw_filled_rect(lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t color) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.border_width = 0;
    dsc.radius = 0;
    lv_canvas_draw_rect(canvas, x, y, w, h, &dsc);
}

static void draw_outlined_rect(lv_obj_t *canvas, int x, int y, int w, int h,
                                lv_color_t bg, lv_color_t border, int border_w) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color   = bg;
    dsc.border_color = border;
    dsc.border_width = border_w;
    dsc.radius     = 2;
    lv_canvas_draw_rect(canvas, x, y, w, h, &dsc);
}

void draw_profile_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_color_t bg    = LVGL_BACKGROUND; 
    lv_color_t fg    = LVGL_FOREGROUND;

    for (int i = 0; i < BT_N_PROFILES; i++) {
        int dy = BT_DOT_START_Y + i * BT_DOT_SPACING;

        if (i == state->active_profile_index) {
            draw_filled_rect(canvas, BT_DOT_X, dy, BT_DOT_SIZE, BT_DOT_SIZE, fg);
        } else {
            draw_outlined_rect(canvas, BT_DOT_X, dy, BT_DOT_SIZE, BT_DOT_SIZE,
                               bg, fg, 1);
        }
    }
}
