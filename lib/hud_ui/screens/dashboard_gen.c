/**
 * @file dashboard_gen.c
 * @brief Circular HUD — layout aligned to expected mockup: top RPM arc + RPM block,
 *        center speed, flanking voltage/temp columns, bottom link strip. No flex.
 *        Margins tuned for 240×240 round (inscribed circle clips square corners).
 */

/*********************
 *      INCLUDES
 *********************/

#include <stdio.h>
#include "dashboard_gen.h"
#include "hud_ui.h"

/*********************
 *      DEFINES
 *********************/

#define RPM_ARC_MAX 6000
/* Horizontal inset from square edge so labels stay inside round bezel */
#define DASH_SIDE_INSET   32
/* Nearer to physical bottom = clear row below km/h (round display) */
#define DASH_BOTTOM_INSET 8
/* Smaller side text — do NOT use LV_LABEL_LONG_MODE_CLIP with this (clips glyphs away) */
#define DASH_SIDE_SMALL 200
/* RPM / km/h labels — a bit smaller than full 24pt */
#define DASH_RPM_KMH_SMALL 188
/* Bottom “OBD OK” row */
#define DASH_LINK_SMALL 176
/* Gap from “RPM” label baseline block to speed digits (was too tight vs CENTER align) */
#define DASH_RPM_TO_SPEED_GAP 12

/**********************
 *      TYPEDEFS
 **********************/

/***********************
 *  STATIC VARIABLES
 **********************/

static lv_obj_t * s_volt_main_lbl;
static lv_obj_t * s_volt_sub_lbl;
static lv_obj_t * s_bat_body;
static lv_obj_t * s_link_txt;

/***********************
 *  STATIC PROTOTYPES
 **********************/

static void dash_rpm_arc_style_cb(lv_observer_t * observer, lv_subject_t * subject);
static void dash_coolant_main_cb(lv_observer_t * observer, lv_subject_t * subject);
static void dash_battery_text_cb(lv_observer_t * observer, lv_subject_t * subject);
static void dash_link_status_cb(lv_observer_t * observer, lv_subject_t * subject);
static lv_obj_t * dash_wifi_bars_create(lv_obj_t * parent);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * dashboard_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    s_volt_main_lbl = NULL;
    s_volt_sub_lbl = NULL;
    s_bat_body = NULL;
    s_link_txt = NULL;

    static lv_style_t style_arc_bg;
    static lv_style_t style_arc_ind;
    static lv_style_t style_knob;
    static bool style_inited = false;

    if (!style_inited) {
        lv_style_init(&style_arc_bg);
        lv_style_set_arc_width(&style_arc_bg, 6);
        lv_style_set_arc_rounded(&style_arc_bg, true);
        /* Visible track on black round panel (was too dark to see) */
        lv_style_set_arc_color(&style_arc_bg, lv_color_hex(0x4A5F78));

        lv_style_init(&style_arc_ind);
        lv_style_set_arc_width(&style_arc_ind, 12);
        lv_style_set_arc_rounded(&style_arc_ind, true);

        lv_style_init(&style_knob);
        lv_style_set_bg_opa(&style_knob, LV_OPA_TRANSP);
        lv_style_set_border_opa(&style_knob, LV_OPA_TRANSP);
        lv_style_set_outline_opa(&style_knob, LV_OPA_TRANSP);
        lv_style_set_shadow_opa(&style_knob, LV_OPA_TRANSP);

        style_inited = true;
    }

    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_name_static(scr, "dashboard_#");
    lv_obj_add_style(scr, &style_dark, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

#if defined(LV_USE_LINE) && LV_USE_LINE
    {
        /* Fainter dividers — less busy on 240×240 */
        static const lv_point_precise_t dec0[] = {
            {28, 86}, {76, 80}, {120, 78}, {164, 80}, {212, 86}};
        static const lv_point_precise_t dec1[] = {
            {30, 162}, {80, 168}, {120, 170}, {160, 168}, {210, 162}};
        for (int k = 0; k < 2; k++) {
            lv_obj_t * ln = lv_line_create(scr);
            lv_line_set_points(ln, k == 0 ? dec0 : dec1, 5);
            lv_obj_set_style_line_width(ln, 1, 0);
            lv_obj_set_style_line_color(ln, lv_color_hex(0x00A8CC), 0);
            lv_obj_set_style_line_opa(ln, LV_OPA_20, 0);
            lv_obj_remove_flag(ln, LV_OBJ_FLAG_CLICKABLE);
        }
    }
#endif

    /* --- RPM arc --- */
    lv_obj_t * rpm_arc = lv_arc_create(scr);
    lv_obj_set_align(rpm_arc, LV_ALIGN_TOP_MID);
    lv_obj_set_size(rpm_arc, 240, 240);
    lv_arc_set_range(rpm_arc, 0, RPM_ARC_MAX);
    lv_arc_set_bg_angles(rpm_arc, 208, 332);
    lv_arc_set_mode(rpm_arc, LV_ARC_MODE_NORMAL);
    lv_arc_bind_value(rpm_arc, &engine_rpm);
    lv_obj_remove_flag(rpm_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(rpm_arc, &style_arc_bg, LV_PART_MAIN);
    lv_obj_add_style(rpm_arc, &style_arc_ind, LV_PART_INDICATOR);
    lv_obj_add_style(rpm_arc, &style_knob, LV_PART_KNOB);

    lv_subject_add_observer_obj(&engine_rpm, dash_rpm_arc_style_cb, rpm_arc, NULL);

    lv_obj_t * rpm_val = lv_label_create(scr);
    lv_label_bind_text(rpm_val, &engine_rpm, "%d");
    lv_obj_set_style_text_font(rpm_val, roboto_bold_40, 0);
    lv_obj_set_style_text_color(rpm_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(rpm_val, LV_ALIGN_TOP_MID, 0, 42);

    lv_obj_t * rpm_tag = lv_label_create(scr);
    lv_label_set_text_static(rpm_tag, "RPM");
    lv_obj_set_style_text_font(rpm_tag, roboto_regular_24, 0);
    lv_obj_set_style_text_color(rpm_tag, lv_color_hex(0x7A8FA3), 0);
    lv_obj_add_flag(rpm_tag, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_transform_scale_x(rpm_tag, DASH_RPM_KMH_SMALL, 0);
    lv_obj_set_style_transform_scale_y(rpm_tag, DASH_RPM_KMH_SMALL, 0);
    lv_obj_set_style_transform_pivot_x(rpm_tag, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(rpm_tag, lv_pct(50), 0);
    lv_obj_align_to(rpm_tag, rpm_val, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    /* Speed: no transform — scaled label + CLIP often mutilates glyphs on SW render */
    lv_obj_t * speed_val = lv_label_create(scr);
    lv_label_bind_text(speed_val, &speed, "%d");
    lv_obj_set_width(speed_val, LV_SIZE_CONTENT);
    lv_obj_set_height(speed_val, LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(speed_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(speed_val, roboto_bold_40, 0);
    lv_obj_set_style_text_color(speed_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_flag(speed_val, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_align_to(speed_val, rpm_tag, LV_ALIGN_OUT_BOTTOM_MID, 0, DASH_RPM_TO_SPEED_GAP);

    lv_obj_t * speed_unit = lv_label_create(scr);
    lv_label_set_text_static(speed_unit, "km/h");
    lv_obj_set_style_text_font(speed_unit, roboto_regular_24, 0);
    lv_obj_set_style_text_color(speed_unit, lv_color_hex(0xB8C4D0), 0);
    lv_obj_add_flag(speed_unit, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_transform_scale_x(speed_unit, DASH_RPM_KMH_SMALL, 0);
    lv_obj_set_style_transform_scale_y(speed_unit, DASH_RPM_KMH_SMALL, 0);
    lv_obj_set_style_transform_pivot_x(speed_unit, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(speed_unit, lv_pct(50), 0);
    lv_obj_align_to(speed_unit, speed_val, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    /* --- Left: battery icon + voltage --- */
    lv_obj_t * bat_body = lv_obj_create(scr);
    lv_obj_set_size(bat_body, 18, 11);
    lv_obj_set_style_bg_opa(bat_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(bat_body, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(bat_body, 2, 0);
    lv_obj_set_style_radius(bat_body, 2, 0);
    lv_obj_set_style_pad_all(bat_body, 0, 0);
    lv_obj_remove_flag(bat_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(bat_body, LV_ALIGN_LEFT_MID, DASH_SIDE_INSET, -26);
    s_bat_body = bat_body;

    lv_obj_t * bat_tip = lv_obj_create(scr);
    lv_obj_set_size(bat_tip, 3, 5);
    lv_obj_set_style_bg_color(bat_tip, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(bat_tip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bat_tip, 0, 0);
    lv_obj_set_style_radius(bat_tip, 1, 0);
    lv_obj_remove_flag(bat_tip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(bat_tip, bat_body, LV_ALIGN_OUT_RIGHT_MID, -1, 0);

    lv_obj_t * volt_main = lv_label_create(scr);
    lv_label_set_text_static(volt_main, "-- V");
    lv_obj_set_style_text_font(volt_main, roboto_regular_24, 0);
    lv_obj_set_style_text_color(volt_main, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(volt_main, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(volt_main, LV_SIZE_CONTENT);
    lv_obj_set_height(volt_main, LV_SIZE_CONTENT);
    lv_obj_add_flag(volt_main, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_transform_scale_x(volt_main, DASH_SIDE_SMALL, 0);
    lv_obj_set_style_transform_scale_y(volt_main, DASH_SIDE_SMALL, 0);
    lv_obj_set_style_transform_pivot_x(volt_main, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(volt_main, lv_pct(50), 0);
    lv_obj_align_to(volt_main, bat_body, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    s_volt_main_lbl = volt_main;
    lv_subject_add_observer_obj(&battery_tenths, dash_battery_text_cb, volt_main, NULL);

    lv_obj_t * volt_sub = lv_label_create(scr);
    lv_label_set_text_static(volt_sub, "Volt");
    lv_obj_set_style_text_font(volt_sub, roboto_regular_24, 0);
    lv_obj_set_style_text_color(volt_sub, lv_color_hex(0x6B7C8F), 0);
    lv_obj_set_style_text_align(volt_sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(volt_sub, LV_SIZE_CONTENT);
    lv_obj_set_height(volt_sub, LV_SIZE_CONTENT);
    lv_obj_add_flag(volt_sub, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_transform_scale_x(volt_sub, DASH_SIDE_SMALL, 0);
    lv_obj_set_style_transform_scale_y(volt_sub, DASH_SIDE_SMALL, 0);
    lv_obj_set_style_transform_pivot_x(volt_sub, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(volt_sub, lv_pct(50), 0);
    lv_obj_align_to(volt_sub, volt_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    s_volt_sub_lbl = volt_sub;

    /* --- Right: thermometer + temp --- */
    lv_obj_t * th_bulb = lv_obj_create(scr);
    lv_obj_set_size(th_bulb, 10, 10);
    lv_obj_set_style_radius(th_bulb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(th_bulb, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(th_bulb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(th_bulb, 0, 0);
    lv_obj_remove_flag(th_bulb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(th_bulb, LV_ALIGN_RIGHT_MID, -DASH_SIDE_INSET, -24);

    lv_obj_t * th_stem = lv_obj_create(scr);
    lv_obj_set_size(th_stem, 2, 14);
    lv_obj_set_style_bg_color(th_stem, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(th_stem, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(th_stem, 0, 0);
    lv_obj_remove_flag(th_stem, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(th_stem, th_bulb, LV_ALIGN_OUT_TOP_MID, 0, 0);

    lv_obj_t * temp_main = lv_label_create(scr);
    lv_label_set_text_static(temp_main, "--°C");
    lv_obj_set_style_text_font(temp_main, roboto_regular_24, 0);
    lv_obj_set_style_text_align(temp_main, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(temp_main, LV_SIZE_CONTENT);
    lv_obj_set_height(temp_main, LV_SIZE_CONTENT);
    lv_obj_add_flag(temp_main, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_transform_scale_x(temp_main, DASH_SIDE_SMALL, 0);
    lv_obj_set_style_transform_scale_y(temp_main, DASH_SIDE_SMALL, 0);
    lv_obj_set_style_transform_pivot_x(temp_main, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(temp_main, lv_pct(50), 0);
    lv_subject_add_observer_obj(&coolant_temp, dash_coolant_main_cb, temp_main, NULL);
    lv_obj_align_to(temp_main, th_bulb, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    lv_obj_t * temp_sub = lv_label_create(scr);
    lv_label_set_text_static(temp_sub, "Temp");
    lv_obj_set_style_text_font(temp_sub, roboto_regular_24, 0);
    lv_obj_set_style_text_color(temp_sub, lv_color_hex(0x6B7C8F), 0);
    lv_obj_set_style_text_align(temp_sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(temp_sub, LV_SIZE_CONTENT);
    lv_obj_set_height(temp_sub, LV_SIZE_CONTENT);
    lv_obj_add_flag(temp_sub, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_transform_scale_x(temp_sub, DASH_SIDE_SMALL, 0);
    lv_obj_set_style_transform_scale_y(temp_sub, DASH_SIDE_SMALL, 0);
    lv_obj_set_style_transform_pivot_x(temp_sub, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(temp_sub, lv_pct(50), 0);
    lv_obj_align_to(temp_sub, temp_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    /* --- Bottom: centered status [wifi][dot][text] --- */
    lv_obj_t * status_row = lv_obj_create(scr);
    lv_obj_remove_style_all(status_row);
    lv_obj_set_size(status_row, 220, 22);
    lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(status_row, 0, 0);
    lv_obj_set_style_pad_column(status_row, 6, 0);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(status_row, LV_ALIGN_BOTTOM_MID, 0, -DASH_BOTTOM_INSET);

    dash_wifi_bars_create(status_row);

    lv_obj_t * link_dot = lv_obj_create(status_row);
    lv_obj_set_size(link_dot, 7, 7);
    lv_obj_set_style_radius(link_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(link_dot, lv_color_hex(0xCC3333), 0);
    lv_obj_set_style_bg_opa(link_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(link_dot, 0, 0);
    lv_obj_remove_flag(link_dot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * link_txt = lv_label_create(status_row);
    lv_label_set_text_static(link_txt, "No OBD");
    lv_obj_set_style_text_font(link_txt, roboto_regular_24, 0);
    lv_obj_set_style_text_color(link_txt, lv_color_hex(0x8899AA), 0);
    lv_obj_set_style_text_opa(link_txt, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_scale_x(link_txt, DASH_LINK_SMALL, 0);
    lv_obj_set_style_transform_scale_y(link_txt, DASH_LINK_SMALL, 0);
    lv_obj_set_style_transform_pivot_x(link_txt, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(link_txt, lv_pct(50), 0);
    s_link_txt = link_txt;

    lv_subject_add_observer_obj(&con_error, dash_link_status_cb, link_dot, NULL);

    /* CAN warning — small, top corner (does not cover speed) */
    lv_obj_t * warn_img = lv_image_create(scr);
    lv_image_set_src(warn_img, warning);
    lv_image_set_scale(warn_img, 160);
    lv_image_set_antialias(warn_img, true);
    lv_obj_align(warn_img, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_bind_flag_if_eq(warn_img, &can_error, LV_OBJ_FLAG_HIDDEN, 0);

    lv_subject_notify(&engine_rpm);
    lv_subject_notify(&coolant_temp);
    lv_subject_notify(&battery_tenths);
    lv_subject_notify(&con_error);

    lv_obj_update_layout(scr);

    LV_TRACE_OBJ_CREATE("finished");

    return scr;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* Cyan -> yellow -> red along RPM */
static void rpm_rgb_from_t(int t, uint8_t * r, uint8_t * g, uint8_t * b)
{
    if(t < 128) {
        int u = t * 2;
        *r = (uint8_t)((0x00 * (255 - u) + 0xFF * u) / 255);
        *g = (uint8_t)((0xE0 * (255 - u) + 0xCC * u) / 255);
        *b = (uint8_t)((0xFF * (255 - u) + 0x00 * u) / 255);
    }
    else {
        int u = (t - 128) * 2;
        if(u > 255) u = 255;
        *r = (uint8_t)((0xFF * (255 - u) + 0xFF * u) / 255);
        *g = (uint8_t)((0xCC * (255 - u) + 0x30 * u) / 255);
        *b = (uint8_t)((0x00 * (255 - u) + 0x40 * u) / 255);
    }
}

static void dash_rpm_arc_style_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    lv_obj_t * arc = lv_observer_get_target_obj(observer);
    if(arc == NULL) return;

    int rpm = lv_subject_get_int(subject);
    if(rpm < 0) rpm = 0;
    if(rpm > RPM_ARC_MAX) rpm = RPM_ARC_MAX;

    if(rpm == 0) {
        lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    }
    else {
        lv_obj_set_style_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
        int t = (rpm * 256) / RPM_ARC_MAX;
        if(t > 255) t = 255;
        uint8_t r, g, b;
        rpm_rgb_from_t(t, &r, &g, &b);
        lv_obj_set_style_arc_color(arc, lv_color_make(r, g, b), LV_PART_INDICATOR);
    }
}

static void dash_coolant_main_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    lv_obj_t * lbl = lv_observer_get_target_obj(observer);
    if(lbl == NULL) return;

    int temp = lv_subject_get_int(subject);
    if(temp < -100) {
        lv_label_set_text_static(lbl, "--°C");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    }
    else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d°C", temp);
        lv_label_set_text(lbl, buf);
        lv_color_t c = (temp > 95) ? lv_color_hex(0xFF5560) : lv_color_hex(0xFFFFFF);
        lv_obj_set_style_text_color(lbl, c, 0);
    }
}

static void dash_battery_text_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    lv_obj_t * lbl = lv_observer_get_target_obj(observer);
    if(lbl == NULL) return;

    int tenths = lv_subject_get_int(subject);
    if(tenths < 0) {
        lv_label_set_text_static(lbl, "-- V");
    }
    else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d.%dV", tenths / 10, tenths % 10);
        lv_label_set_text(lbl, buf);
    }

    if(s_bat_body != NULL) {
        lv_obj_align_to(lbl, s_bat_body, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    }
    if(s_volt_sub_lbl != NULL) {
        lv_obj_align_to(s_volt_sub_lbl, lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    }
}

static void dash_link_status_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    lv_obj_t * dot = lv_observer_get_target_obj(observer);
    if(dot == NULL || s_link_txt == NULL) return;

    /* con_error: 1 = disconnected / fault, 0 = link OK */
    int dis = lv_subject_get_int(subject);
    if(dis) {
        lv_obj_set_style_bg_color(dot, lv_color_hex(0xDD3344), 0);
        lv_label_set_text_static(s_link_txt, "No OBD");
        lv_obj_set_style_text_color(s_link_txt, lv_color_hex(0xCC99AA), 0);
        lv_obj_set_style_text_opa(s_link_txt, LV_OPA_COVER, 0);
    }
    else {
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x22DD66), 0);
        lv_label_set_text_static(s_link_txt, "OBD OK");
        lv_obj_set_style_text_color(s_link_txt, lv_color_hex(0x99AABB), 0);
        lv_obj_set_style_text_opa(s_link_txt, LV_OPA_COVER, 0);
    }
}

static lv_obj_t * dash_wifi_bars_create(lv_obj_t * parent)
{
    lv_obj_t * h = lv_obj_create(parent);
    lv_obj_remove_style_all(h);
    lv_obj_set_size(h, 18, 11);
    lv_obj_set_style_bg_opa(h, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(h, 0, 0);
    lv_obj_remove_flag(h, LV_OBJ_FLAG_SCROLLABLE);

    static const lv_coord_t bw = 2;
    static const lv_coord_t hs[] = {4, 7, 10};
    for(int i = 0; i < 3; i++) {
        lv_obj_t * b = lv_obj_create(h);
        lv_obj_set_size(b, bw, hs[i]);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x99AABB), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_radius(b, 1, 0);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(b, LV_ALIGN_BOTTOM_LEFT, (lv_coord_t)(i * 5), 0);
    }
    return h;
}
