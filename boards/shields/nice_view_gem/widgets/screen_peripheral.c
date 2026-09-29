/*
 * Copyright (c) 2026 Colin Creasman
 * SPDX-License-Identifier: MIT
 *
 * Status screen for the left half when it runs as a split *peripheral*, i.e.
 * when a USB dongle is the central.
 *
 * screen.c draws layer, profile and output state, all of which only exist on
 * the central, so it is not built for peripherals. Without this file the
 * peripheral build had no zmk_widget_screen_init() at all and failed to link.
 * This provides the same API from state a peripheral does have: its own
 * battery, and whether its link to the dongle is up.
 *
 * Drawn on one full-screen canvas exactly like screen.c, reusing the same
 * battery widget, so the peripheral screen matches the central one's style.
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/usb.h>

#if defined(CONFIG_TOUCAN_STATUS_SCREEN) && CONFIG_TOUCAN_STATUS_SCREEN == 2
#include "battery_arc.h"
#else
#include "battery.h"
#endif

#include "../assets/custom_fonts.h"
#include "screen.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static void draw_link_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t title_dsc;
    init_label_dsc(&title_dsc, LVGL_FOREGROUND, &quinquefive_12, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 0, 82, SCREEN_WIDTH, &title_dsc, "DONGLE");

    lv_draw_label_dsc_t status_dsc;
    init_label_dsc(&status_dsc, LVGL_FOREGROUND, &quinquefive_12, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 0, 104, SCREEN_WIDTH, &status_dsc,
                        state->connected ? "LINKED" : "SEARCHING");
}

static void draw_screen(lv_obj_t *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    fill_background(canvas);

    draw_battery_status(canvas, state);
    draw_link_status(canvas, state);
}

/**
 * Battery status
 **/

struct peripheral_battery_state {
    uint8_t level;
    bool usb_present;
};

static void peripheral_battery_update_cb(struct peripheral_battery_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.battery = state.level;
        widget->state.charging = state.usb_present;
        draw_screen(widget->obj, &widget->state);
    }
}

static struct peripheral_battery_state peripheral_battery_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev =
        (eh != NULL) ? as_zmk_battery_state_changed(eh) : NULL;

    return (struct peripheral_battery_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#else
        .usb_present = false,
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_battery, struct peripheral_battery_state,
                            peripheral_battery_update_cb, peripheral_battery_get_state)

ZMK_SUBSCRIPTION(widget_peripheral_battery, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_peripheral_battery, zmk_usb_conn_state_changed);
#endif

/**
 * Link-to-central status
 **/

struct peripheral_link_state {
    bool connected;
};

static void peripheral_link_update_cb(struct peripheral_link_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.connected = state.connected;
        draw_screen(widget->obj, &widget->state);
    }
}

static struct peripheral_link_state peripheral_link_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    return (struct peripheral_link_state){.connected = zmk_split_bt_peripheral_is_connected()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_link, struct peripheral_link_state,
                            peripheral_link_update_cb, peripheral_link_get_state)

ZMK_SUBSCRIPTION(widget_peripheral_link, zmk_split_peripheral_status_changed);

/**
 * Initialization
 **/

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, SCREEN_WIDTH, SCREEN_HEIGHT);

    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, SCREEN_WIDTH, SCREEN_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);
    widget_peripheral_battery_init();
    widget_peripheral_link_init();

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
