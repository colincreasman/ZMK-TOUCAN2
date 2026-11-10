#include <stdio.h>
#include "os_status.h"
#include "toucan_os_mode.h"
#include "../assets/custom_fonts.h"

void draw_os_status(lv_obj_t *canvas, const struct status_state *state) {
    char text[20];
    if (!state->os_status.valid) {
        snprintf(text, sizeof(text), "OS --");
    } else {
        const char *mode = state->os_status.mode == TOUCAN_OS_WIN ? "WIN" : "MAC";
        if (state->os_status.slot == 0) {
            snprintf(text, sizeof(text), "USB %s", mode);
        } else {
            snprintf(text, sizeof(text), "BT%u %s", state->os_status.slot - 1, mode);
        }
    }
    lv_draw_label_dsc_t label;
    init_label_dsc(&label, LVGL_FOREGROUND, &quinquefive_12, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 0, 62, SCREEN_WIDTH, &label, text);
}
