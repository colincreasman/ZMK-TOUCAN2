#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "chart.h"
#include "../assets/custom_fonts.h"

#define CHART_COLS      30
#define CHART_ROWS      6
#define CHART_SQ        2       // square size in px
#define CHART_GAP       2       // gap between squares in px
#define CHART_X         12      // left edge of chart
#define CHART_Y         78      // top edge of chart
#define WPM_MAX         100

static uint8_t wpm_buf[CHART_COLS] = {0};
static uint8_t wpm_head = 0;
static uint8_t last_wpm = 0;  


static uint8_t wpm_to_rows(uint8_t wpm)
{
    uint32_t rows = ((uint32_t)wpm * CHART_ROWS + WPM_MAX / 2) / WPM_MAX;
    if (rows > CHART_ROWS) rows = CHART_ROWS;
    return (uint8_t)rows;
}

static void draw_square(lv_obj_t *canvas, int col, int row, bool filled)
{
    int x = CHART_X + col * (CHART_SQ + CHART_GAP);
    int y = CHART_Y + (CHART_ROWS - 1 - row) * (CHART_SQ + CHART_GAP);

    lv_draw_rect_dsc_t dsc;

    if (filled) {
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color     = lv_color_make(255, 255, 255);
        dsc.bg_opa       = LV_OPA_COVER;
        dsc.border_width = 0;
        dsc.radius       = 1;
        lv_canvas_draw_rect(canvas, x, y, CHART_SQ, CHART_SQ, &dsc);
    }
}

void draw_chart_status(lv_obj_t *canvas, const struct status_state *state)
{
    if (state->wpm != last_wpm) {
        wpm_head = (wpm_head + 1) % CHART_COLS;
        wpm_buf[wpm_head] = state->wpm;
        last_wpm = state->wpm;
    }
    for (int col = 0; col < CHART_COLS; col++) {
        int buf_idx = (wpm_head + 1 + col) % CHART_COLS;
        uint8_t rows_filled = wpm_to_rows(wpm_buf[buf_idx]);

        for (int row = 0; row < CHART_ROWS; row++) {
            draw_square(canvas, col, row, row < rows_filled);
        }
    }
}
