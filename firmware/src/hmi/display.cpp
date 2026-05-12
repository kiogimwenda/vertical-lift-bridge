// ============================================================================
// hmi/display.cpp — LVGL + TFT_eSPI bring-up and Complete Layout.
// Owner: M4 Abigael
// ============================================================================

#include "display.h"
#include "input.h"
#include "../pin_config.h"
#include "../system_types.h"
#include "../safety/fault_register.h"
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#define TFT_W 240
#define TFT_H 320
#define LV_BUF_LINES 20

static TFT_eSPI    s_tft = TFT_eSPI(TFT_W, TFT_H);
static lv_display_t* s_disp = nullptr;
static lv_indev_t*  s_indev = nullptr;
static uint8_t      s_buf1[TFT_W * LV_BUF_LINES * 2];
static uint8_t      s_buf2[TFT_W * LV_BUF_LINES * 2];

static SemaphoreHandle_t s_lvgl_mutex = nullptr;

#define LEDC_CH_BL  2
#define LEDC_RES_BL 13

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    s_tft.startWrite();
    s_tft.setAddrWindow(area->x1, area->y1, w, h);
    s_tft.pushPixelsDMA((uint16_t*)px, w * h);
    s_tft.endWrite();
    lv_display_flush_ready(disp);
}

static void lvgl_touch_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    uint16_t tx, ty;
    bool touched = s_tft.getTouch(&tx, &ty, 250); // Lower threshold for better sensitivity
    if (touched) {
        data->state = LV_INDEV_STATE_PRESSED;
        // Coordinates are already mapped to screen space by TFT_eSPI calibration
        data->point.x =320 - tx;
        data->point.y = 240 -ty;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static uint32_t lvgl_tick_cb(void) { return millis(); }

static SharedStatus_t s_local;
static void g_status_snapshot(void) {
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_local = g_status;
        xSemaphoreGive(g_status_mutex);
    }
}

// --- UI Components ---
static lv_obj_t *tv;
static lv_obj_t *pg_home, *pg_ops, *pg_vision, *pg_cw, *pg_settings;
static lv_obj_t *header, *lbl_state, *lbl_pos, *bar_pos;
static lv_obj_t *lbl_mot_stat; // Separate label for motor status to handle color properly
static lv_obj_t *led_road_r, *led_road_y, *led_road_g;
static lv_obj_t *meter_motor;

static lv_obj_t *radar_bg;
static lv_obj_t *target_box;
static lv_obj_t *bar_confidence;
static lv_obj_t *lbl_target;

static lv_obj_t *bar_cw_l, *bar_cw_r;
static lv_obj_t *lbl_cw_l_stat, *lbl_cw_r_stat;

static lv_style_t style_card;
static lv_style_t style_btn_modern;

static void init_styles() {
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, lv_color_white());
    lv_style_set_radius(&style_card, 8);
    lv_style_set_border_width(&style_card, 2);
    lv_style_set_border_color(&style_card, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_style_set_shadow_width(&style_card, 10);
    lv_style_set_shadow_color(&style_card, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_shadow_opa(&style_card, LV_OPA_20);

    lv_style_init(&style_btn_modern);
    lv_style_set_radius(&style_btn_modern, 4);
    lv_style_set_bg_color(&style_btn_modern, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_btn_modern, lv_color_white());
    lv_style_set_shadow_width(&style_btn_modern, 4);
    lv_style_set_shadow_color(&style_btn_modern, lv_palette_main(LV_PALETTE_GREY));
}

static void btn_event_cb(lv_event_t * e) {
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    const char * txt = lv_label_get_text(lv_obj_get_child(btn, 0));
    
    SystemEvent_t evt = EVT_NONE;
    if(strcmp(txt, "RAISE") == 0) evt = EVT_OPERATOR_RAISE;
    if(strcmp(txt, "LOWER") == 0) evt = EVT_OPERATOR_LOWER;
    if(strcmp(txt, "HOLD") == 0)  evt = EVT_OPERATOR_HOLD;
    
    if (evt != EVT_NONE) xQueueSend(g_event_queue, &evt, 0);
}

static void popup_timer_cb(lv_timer_t * t) {
    lv_obj_t * mbox = (lv_obj_t *)lv_timer_get_user_data(t);
    lv_msgbox_close(mbox);
}

static void header_click_cb(lv_event_t * e) {
    char buf[64];
    sprintf(buf, "Instantaneous Motor Draw:\n%d mA", s_local.motor_current_ma);
    
    lv_obj_t * mbox = lv_obj_create(lv_screen_active());
    lv_obj_set_size(mbox, 220, 100);
    lv_obj_center(mbox);
    lv_obj_set_style_bg_color(mbox, lv_color_white(), 0);
    lv_obj_set_style_border_color(mbox, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(mbox, 2, 0);
    lv_obj_set_style_radius(mbox, 8, 0);

    lv_obj_t * lbl = lv_label_create(mbox);
    lv_label_set_text(lbl, buf);
    lv_obj_center(lbl);

    lv_timer_t * timer = lv_timer_create(popup_timer_cb, 4000, mbox);
    lv_timer_set_repeat_count(timer, 1);
}

static void build_home(lv_obj_t *parent) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 140, 150);
    lv_obj_align(card, LV_ALIGN_LEFT_MID, 5, -10);
    lv_obj_add_style(card, &style_card, 0);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, "DECK HEIGHT");
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);

    bar_pos = lv_bar_create(card);
    lv_obj_set_size(bar_pos, 25, 90);
    lv_obj_align(bar_pos, LV_ALIGN_CENTER, 0, 5);
    lv_bar_set_range(bar_pos, 0, DECK_HEIGHT_MAX_MM);
    lv_obj_set_style_bg_color(bar_pos, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);

    lbl_pos = lv_label_create(card);
    lv_obj_align(lbl_pos, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *card_lights = lv_obj_create(parent);
    lv_obj_set_size(card_lights, 140, 150);
    lv_obj_align(card_lights, LV_ALIGN_RIGHT_MID, -5, -10);
    lv_obj_add_style(card_lights, &style_card, 0);

    lv_obj_t *lbl2 = lv_label_create(card_lights);
    lv_label_set_text(lbl2, "ROAD TRAFFIC");
    lv_obj_align(lbl2, LV_ALIGN_TOP_MID, 0, 0);

    led_road_g = lv_led_create(card_lights);
    lv_obj_set_size(led_road_g, 24, 24);
    lv_obj_align(led_road_g, LV_ALIGN_CENTER, 0, -30);
    lv_led_set_color(led_road_g, lv_palette_main(LV_PALETTE_GREEN));

    led_road_y = lv_led_create(card_lights);
    lv_obj_set_size(led_road_y, 24, 24);
    lv_obj_align(led_road_y, LV_ALIGN_CENTER, 0, 0);
    lv_led_set_color(led_road_y, lv_palette_main(LV_PALETTE_AMBER));

    led_road_r = lv_led_create(card_lights);
    lv_obj_set_size(led_road_r, 24, 24);
    lv_obj_align(led_road_r, LV_ALIGN_CENTER, 0, 30);
    lv_led_set_color(led_road_r, lv_palette_main(LV_PALETTE_RED));
}

static void build_ops(lv_obj_t *parent) {
    lv_obj_t *btn_raise = lv_btn_create(parent);
    lv_obj_set_size(btn_raise, 100, 40);
    lv_obj_align(btn_raise, LV_ALIGN_TOP_LEFT, 10, 0);
    lv_obj_add_style(btn_raise, &style_btn_modern, 0);
    lv_obj_t *l1 = lv_label_create(btn_raise);
    lv_label_set_text(l1, "RAISE");
    lv_obj_center(l1);
    lv_obj_add_event_cb(btn_raise, btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lower = lv_btn_create(parent);
    lv_obj_set_size(btn_lower, 100, 40);
    lv_obj_align(btn_lower, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_obj_add_style(btn_lower, &style_btn_modern, 0);
    lv_obj_t *l2 = lv_label_create(btn_lower);
    lv_label_set_text(l2, "LOWER");
    lv_obj_center(l2);
    lv_obj_add_event_cb(btn_lower, btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_hold = lv_btn_create(parent);
    lv_obj_set_size(btn_hold, 100, 40);
    lv_obj_align(btn_hold, LV_ALIGN_TOP_LEFT, 10, 100);
    lv_obj_set_style_bg_color(btn_hold, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_t *l3 = lv_label_create(btn_hold);
    lv_label_set_text(l3, "HOLD");
    lv_obj_center(l3);
    lv_obj_add_event_cb(btn_hold, btn_event_cb, LV_EVENT_CLICKED, NULL);

    meter_motor = lv_arc_create(parent);
    lv_obj_set_size(meter_motor, 150, 150);
    lv_obj_align(meter_motor, LV_ALIGN_RIGHT_MID, -10, -10);
    lv_arc_set_rotation(meter_motor, 135);
    lv_arc_set_bg_angles(meter_motor, 0, 270);
    lv_arc_set_range(meter_motor, 0, 2500);
    lv_obj_remove_style(meter_motor, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(meter_motor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(meter_motor, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
}

static void build_vision(lv_obj_t *parent) {
    radar_bg = lv_obj_create(parent);
    lv_obj_set_size(radar_bg, 200, 120);
    lv_obj_align(radar_bg, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(radar_bg, lv_palette_darken(LV_PALETTE_BLUE_GREY, 4), 0);
    
    target_box = lv_obj_create(radar_bg);
    lv_obj_set_size(target_box, 40, 40);
    lv_obj_align(target_box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(target_box, 0, 0);
    lv_obj_set_style_border_width(target_box, 2, 0);
    lv_obj_set_style_border_color(target_box, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_add_flag(target_box, LV_OBJ_FLAG_HIDDEN);

    lbl_target = lv_label_create(parent);
    lv_label_set_text(lbl_target, "SCANNING...");
    lv_obj_align(lbl_target, LV_ALIGN_TOP_MID, 0, 125);
    lv_obj_set_style_text_color(lbl_target, lv_palette_main(LV_PALETTE_GREY), 0);

    bar_confidence = lv_bar_create(parent);
    lv_obj_set_size(bar_confidence, 200, 15);
    lv_obj_align(bar_confidence, LV_ALIGN_TOP_MID, 0, 145);
    lv_bar_set_range(bar_confidence, 0, 100);
}

static void build_cw(lv_obj_t *parent) {
    lv_obj_t *card_l = lv_obj_create(parent);
    lv_obj_set_size(card_l, 130, 150);
    lv_obj_align(card_l, LV_ALIGN_LEFT_MID, 10, -10);
    lv_obj_add_style(card_l, &style_card, 0);

    lv_obj_t *l1 = lv_label_create(card_l);
    lv_label_set_text(l1, "LEFT TANK");
    lv_obj_align(l1, LV_ALIGN_TOP_MID, 0, 0);

    bar_cw_l = lv_bar_create(card_l);
    lv_obj_set_size(bar_cw_l, 30, 80);
    lv_obj_align(bar_cw_l, LV_ALIGN_CENTER, 0, 0);
    lv_bar_set_range(bar_cw_l, 0, 150);

    lbl_cw_l_stat = lv_label_create(card_l);
    lv_label_set_text(lbl_cw_l_stat, "IDLE");
    lv_obj_align(lbl_cw_l_stat, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *card_r = lv_obj_create(parent);
    lv_obj_set_size(card_r, 130, 150);
    lv_obj_align(card_r, LV_ALIGN_RIGHT_MID, -10, -10);
    lv_obj_add_style(card_r, &style_card, 0);

    lv_obj_t *l2 = lv_label_create(card_r);
    lv_label_set_text(l2, "RIGHT TANK");
    lv_obj_align(l2, LV_ALIGN_TOP_MID, 0, 0);

    bar_cw_r = lv_bar_create(card_r);
    lv_obj_set_size(bar_cw_r, 30, 80);
    lv_obj_align(bar_cw_r, LV_ALIGN_CENTER, 0, 0);
    lv_bar_set_range(bar_cw_r, 0, 150);

    lbl_cw_r_stat = lv_label_create(card_r);
    lv_label_set_text(lbl_cw_r_stat, "IDLE");
    lv_obj_align(lbl_cw_r_stat, LV_ALIGN_BOTTOM_MID, 0, 0);
}

static void build_settings(lv_obj_t *parent) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 280, 150);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(card, &style_card, 0);

    lv_obj_t *lbl_title = lv_label_create(card);
    lv_label_set_text(lbl_title, "SYSTEM SETTINGS");
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, -5);

    lv_obj_t *lbl_auto = lv_label_create(card);
    lv_label_set_text(lbl_auto, "Auto Mode:");
    lv_obj_align(lbl_auto, LV_ALIGN_LEFT_MID, 10, -35);

    lv_obj_t *sw_auto = lv_switch_create(card);
    lv_obj_align(sw_auto, LV_ALIGN_RIGHT_MID, -10, -35);
    g_status_snapshot();
    if(s_local.auto_mode_active) lv_obj_add_state(sw_auto, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_auto, [](lv_event_t *e){
        lv_obj_t * sw = (lv_obj_t *)lv_event_get_target(e);
        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_status.auto_mode_active = lv_obj_has_state(sw, LV_STATE_CHECKED);
            xSemaphoreGive(g_status_mutex);
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *lbl_theme = lv_label_create(card);
    lv_label_set_text(lbl_theme, "Dark Mode:");
    lv_obj_align(lbl_theme, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *sw_theme = lv_switch_create(card);
    lv_obj_align(sw_theme, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(sw_theme, [](lv_event_t *e){
        lv_obj_t * sw = (lv_obj_t *)lv_event_get_target(e);
        bool is_dark = lv_obj_has_state(sw, LV_STATE_CHECKED);
        lv_theme_t * theme = lv_theme_default_init(s_disp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), is_dark, LV_FONT_DEFAULT);
        lv_display_set_theme(s_disp, theme);
        
        if(is_dark) {
            lv_style_set_bg_color(&style_card, lv_palette_darken(LV_PALETTE_GREY, 4));
            lv_style_set_border_color(&style_card, lv_palette_darken(LV_PALETTE_GREY, 3));
            lv_style_set_text_color(&style_btn_modern, lv_color_white());
        } else {
            lv_style_set_bg_color(&style_card, lv_color_white());
            lv_style_set_border_color(&style_card, lv_palette_lighten(LV_PALETTE_GREY, 2));
            lv_style_set_text_color(&style_btn_modern, lv_color_white());
        }
        lv_obj_report_style_change(&style_card);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *lbl_br = lv_label_create(card);
    lv_label_set_text(lbl_br, "HMI Brightness:");
    lv_obj_align(lbl_br, LV_ALIGN_LEFT_MID, 10, 35);

    lv_obj_t *slider_br = lv_slider_create(card);
    lv_obj_set_size(slider_br, 100, 10);
    lv_obj_align(slider_br, LV_ALIGN_RIGHT_MID, -10, 35);
    lv_slider_set_range(slider_br, 10, 100);
    lv_slider_set_value(slider_br, s_local.hmi_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_br, [](lv_event_t *e) {
        lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
        uint8_t val = lv_slider_get_value(slider);
        ledcWrite(LEDC_CH_BL, (1 << LEDC_RES_BL) * val / 100);
        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_status.hmi_brightness = val;
            xSemaphoreGive(g_status_mutex);
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);
}

static void ui_init() {
    init_styles();

    lv_theme_t * theme = lv_theme_default_init(s_disp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_display_set_theme(s_disp, theme);

    header = lv_obj_create(lv_screen_active());
    lv_obj_set_size(header, 320, 35);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(header, header_click_cb, LV_EVENT_CLICKED, NULL);

    lbl_state = lv_label_create(header);
    lv_label_set_text(lbl_state, "SYS: INIT");
    lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_state, LV_ALIGN_LEFT_MID, 10, 0);

    lbl_mot_stat = lv_label_create(header);
    lv_label_set_text(lbl_mot_stat, "MOT: IDLE");
    lv_obj_set_style_text_font(lbl_mot_stat, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_mot_stat, LV_ALIGN_RIGHT_MID, -10, 0);

    tv = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tv, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(tv, 40);
    lv_obj_align(tv, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_size(tv, 320, 205);
    lv_obj_set_style_bg_color(tv, lv_palette_lighten(LV_PALETTE_GREY, 4), 0);

    pg_home = lv_tabview_add_tab(tv, "HOME");
    pg_ops = lv_tabview_add_tab(tv, "OPS");
    pg_vision = lv_tabview_add_tab(tv, "VISION");
    pg_cw = lv_tabview_add_tab(tv, "CW");
    pg_settings = lv_tabview_add_tab(tv, "SET");

    build_home(pg_home);
    build_ops(pg_ops);
    build_vision(pg_vision);
    build_cw(pg_cw);
    build_settings(pg_settings);
}

static void refresh_active(void) {
    g_status_snapshot();

    const char* state_names[] = {"IDLE", "CLEARING", "RAISING", "HOLD", "LOWERING", "OPENING", "FAULT", "ESTOP", "INIT"};
    
    // Status Bar Color logic (simplified for LVGL 9)
    lv_color_t sys_color = lv_palette_main(LV_PALETTE_GREEN);
    if(s_local.state == STATE_FAULT || s_local.state == STATE_ESTOP) sys_color = lv_palette_main(LV_PALETTE_RED);
    else if(s_local.state == STATE_RAISED_HOLD) sys_color = lv_palette_main(LV_PALETTE_ORANGE);
    else if(s_local.state != STATE_IDLE) sys_color = lv_palette_main(LV_PALETTE_CYAN);

    char buf_sys[64];
    sprintf(buf_sys, "SYS: %s %s", state_names[s_local.state], s_local.auto_mode_active ? "[AUTO]" : "[MAN]");
    lv_label_set_text(lbl_state, buf_sys);
    lv_obj_set_style_text_color(lbl_state, sys_color, 0);

    const char* mot_stat = "IDLE";
    lv_color_t mot_color = lv_color_white();
    if (s_local.motor_current_ma == 0) {
        mot_stat = "IDLE";
        mot_color = lv_color_white();
    } else if (s_local.motor_current_ma < 1500) {
        mot_stat = "OK";
        mot_color = lv_palette_main(LV_PALETTE_GREEN);
    } else if (s_local.motor_current_ma <= 1980) {
        mot_stat = "WARN";
        mot_color = lv_palette_main(LV_PALETTE_YELLOW);
    } else {
        mot_stat = "CRIT";
        mot_color = (s_local.uptime_ms % 500 < 250) ? lv_palette_main(LV_PALETTE_RED) : lv_color_white();
    }

    char buf_mot[32];
    sprintf(buf_mot, "MOT: %s", mot_stat);
    lv_label_set_text(lbl_mot_stat, buf_mot);
    lv_obj_set_style_text_color(lbl_mot_stat, mot_color, 0);

    char buf_p[16];
    sprintf(buf_p, "%d mm", s_local.deck_position_mm);
    lv_label_set_text(lbl_pos, buf_p);
    lv_bar_set_value(bar_pos, s_local.deck_position_mm, LV_ANIM_ON);

    lv_arc_set_value(meter_motor, s_local.motor_current_ma);

    lv_led_off(led_road_g); lv_led_off(led_road_y); lv_led_off(led_road_r);
    if(s_local.state == STATE_IDLE) lv_led_on(led_road_g);
    else if(s_local.state == STATE_ROAD_CLEARING) lv_led_on(led_road_y);
    else lv_led_on(led_road_r);

    if(s_local.vision.vehicle_present) {
        lv_obj_remove_flag(target_box, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_target, "TARGET ACQUIRED");
        lv_obj_set_style_text_color(lbl_target, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_align(target_box, LV_ALIGN_CENTER, (rand()%10)-5, (rand()%10)-5);
    } else {
        lv_obj_add_flag(target_box, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_target, "SCANNING...");
        lv_obj_set_style_text_color(lbl_target, lv_palette_main(LV_PALETTE_GREY), 0);
    }
    lv_bar_set_value(bar_confidence, s_local.vision.confidence, LV_ANIM_ON);

    lv_bar_set_value(bar_cw_l, (int)s_local.counterweight.left.water_level_ml, LV_ANIM_ON);
    lv_bar_set_value(bar_cw_r, (int)s_local.counterweight.right.water_level_ml, LV_ANIM_ON);

    char cw_stat_l[32];
    if(s_local.counterweight.left.pump_active) sprintf(cw_stat_l, "PUMPING\n%d ML", (int)s_local.counterweight.left.water_level_ml);
    else if(s_local.counterweight.left.drain_open) sprintf(cw_stat_l, "DRAINING\n%d ML", (int)s_local.counterweight.left.water_level_ml);
    else sprintf(cw_stat_l, "IDLE\n%d ML", (int)s_local.counterweight.left.water_level_ml);
    lv_label_set_text(lbl_cw_l_stat, cw_stat_l);

    char cw_stat_r[32];
    if(s_local.counterweight.right.pump_active) sprintf(cw_stat_r, "PUMPING\n%d ML", (int)s_local.counterweight.right.water_level_ml);
    else if(s_local.counterweight.right.drain_open) sprintf(cw_stat_r, "DRAINING\n%d ML", (int)s_local.counterweight.right.water_level_ml);
    else sprintf(cw_stat_r, "IDLE\n%d ML", (int)s_local.counterweight.right.water_level_ml);
    lv_label_set_text(lbl_cw_r_stat, cw_stat_r);
}

// Hooks required by the rest of the FSM
void display_notify_state_change(SystemState_t new_state) {
    if (new_state == STATE_FAULT || new_state == STATE_ESTOP) {
        // Just force switch to tab if needed
    }
}
void display_notify_event(SystemEvent_t event) { (void)event; }
void display_request_screen(HmiScreen_t s) { (void)s; }

bool hmi_cmd_post(HmiCmd_t cmd) {
    uint8_t c = (uint8_t)cmd;
    return xQueueSend(g_hmi_cmd_queue, &c, 0) == pdTRUE;
}

// =====================================================================
// 9. The HMI task (Core 1)
// =====================================================================
void task_hmi(void* arg) {
    Serial.println("[hmi] task start (Core 1)");

    if (PIN_TFT_BL != -1) {
        ledcSetup(LEDC_CH_BL, 5000, LEDC_RES_BL);
        ledcAttachPin(PIN_TFT_BL, LEDC_CH_BL);
        ledcWrite(LEDC_CH_BL, (1 << LEDC_RES_BL) * 80 / 100);
    } 
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        g_status.hmi_brightness = 80;
        xSemaphoreGive(g_status_mutex);
    }

    s_tft.init();
    s_tft.setRotation(3); // Landscape for 320x240
    s_tft.fillScreen(TFT_BLACK);
    s_tft.initDMA();

    uint16_t cal[5] = { 318, 3505, 273, 3539, 6 };
    s_tft.setTouch(cal);

    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);

    s_disp = lv_display_create(320, 240);
    lv_display_set_buffers(s_disp, s_buf1, s_buf2, sizeof(s_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);

    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(s_indev, s_disp); // Mandatory in LVGL 9
    lv_indev_set_read_cb(s_indev, lvgl_touch_cb);

    s_lvgl_mutex = xSemaphoreCreateMutex();

    ui_init();

    TickType_t last = xTaskGetTickCount();
    uint32_t last_refresh = 0;

    for (;;) {
        uint8_t cmd;
        while (xQueueReceive(g_hmi_cmd_queue, &cmd, 0) == pdTRUE) {
            // Unused via physical buttons now, handled directly by LVGL callbacks
        }

        if (millis() - last_refresh > 200) {
            last_refresh = millis();
            refresh_active();
        }

        lv_timer_handler();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
    }
}
