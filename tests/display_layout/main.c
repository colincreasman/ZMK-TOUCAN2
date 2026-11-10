#include <assert.h>
#include <stdio.h>
#include <lvgl.h>

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
    (void)area;
    (void)pixels;
    lv_disp_flush_ready(driver);
}

int main(void) {
    lv_init();
    static lv_color_t pixels[144 * 168];
    static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, pixels, NULL, 144 * 168);
    lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = 144;
    driver.ver_res = 168;
    driver.flush_cb = flush;
    driver.draw_buf = &buffer;
    lv_disp_t *display = lv_disp_drv_register(&driver);
    lv_disp_set_theme(display, lv_theme_mono_init(display, true, LV_FONT_DEFAULT));
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_t *parent = lv_obj_create(screen);
    lv_obj_set_size(parent, 144, 168);
    lv_obj_align(parent, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, pixels, 144, 168, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(canvas, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_update_layout(screen);
    lv_area_t canvas_area, parent_area;
    lv_obj_get_coords(canvas, &canvas_area);
    lv_obj_get_coords(parent, &parent_area);
    // The previous label had a 10px line at y156. Default card padding/border
    // shifts the canvas down, putting the bottom of that text outside the parent.
    assert(canvas_area.y1 + 156 + 10 - 1 > parent_area.y2);
    assert(canvas_area.y1 + 62 >= parent_area.y1);
    assert(canvas_area.y1 + 62 + 14 - 1 <= parent_area.y2);
    printf("PASS: actual LVGL canvas y=%d, parent bottom=%d; old label clipped, new label fits\n",
           canvas_area.y1, parent_area.y2);
    return 0;
}
