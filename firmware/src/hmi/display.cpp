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

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    s_tft.pushImage(area->x1, area->y1, w, h, (uint16_t*)px);
    lv_display_flush_ready(disp);
}

static void lvgl_touch_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    uint16_t tx, ty;
    bool touched = s_tft.getTouch(&tx, &ty, 250); // Lower threshold for better sensitivity
    if (touched) {
        data->state = LV_INDEV_STATE_PRESSED;
        // Coordinates are already mapped to screen space by TFT_eSPI calibration
        data->point.x = tx;
        data->point.y = ty;
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
static lv_obj_t *pg_home, *pg_ops, *pg_cw, *pg_settings;
static lv_obj_t *header, *lbl_state, *lbl_pos, *bar_pos;
static lv_obj_t *lbl_mot_stat; // Separate label for motor status to handle color properly
static lv_obj_t *led_road_r, *led_road_y, *led_road_g;
static lv_obj_t *led_us, *led_ds;
static lv_obj_t *lbl_marine_header;
static lv_obj_t *meter_motor;

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
    g_status_snapshot();
    char buf[64];
    // L293D module has no current-sense output, so motor_current_ma is always
    // 0. Report deck height and commanded PWM duty instead — both are real.
    sprintf(buf, "Deck Height: %d mm\nMotor PWM: %d%%",
            s_local.deck_position_mm,
            (s_local.motor_pwm_duty * 100) / MOTOR_PWM_MAX);
    
    lv_obj_t * mbox = lv_obj_create(lv_screen_active());
    lv_obj_set_size(mbox, 220, 100);
    lv_obj_center(mbox);
    lv_obj_set_style_bg_color(mbox, lv_color_white(), 0);
    lv_obj_set_style_border_color(mbox, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(mbox, 2, 0);
    lv_obj_set_style_radius(mbox, 8, 0);

    lv_obj_t * lbl = lv_label_create(mbox);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    lv_obj_center(lbl);

    lv_timer_t * timer = lv_timer_create(popup_timer_cb, 4000, mbox);
    lv_timer_set_repeat_count(timer, 1);
}

static void build_home(lv_obj_t *parent) {
    // --- Left Column: Deck Height ---
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 100, 150);
    lv_obj_align(card, LV_ALIGN_LEFT_MID, 5, -10);
    lv_obj_add_style(card, &style_card, 0);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, "DECK");
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);

    bar_pos = lv_bar_create(card);
    lv_obj_set_size(bar_pos, 20, 90);
    lv_obj_align(bar_pos, LV_ALIGN_CENTER, 0, 5);
    lv_bar_set_range(bar_pos, 0, DECK_HEIGHT_MAX_MM);
    lv_obj_set_style_bg_color(bar_pos, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);

    lbl_pos = lv_label_create(card);
    lv_obj_align(lbl_pos, LV_ALIGN_BOTTOM_MID, 0, 0);

    // --- Middle Column: Marine Sensors ---
    lv_obj_t *card_mid = lv_obj_create(parent);
    lv_obj_set_size(card_mid, 90, 150);
    lv_obj_align(card_mid, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_style(card_mid, &style_card, 0);

    // NOTE: this label is the one refresh_active() flips to "VESSEL ALERT!".
    // It MUST be stored in the file-scope lbl_marine_header or refresh_active()
    // dereferences a null pointer the first time a vessel is detected.
    lbl_marine_header = lv_label_create(card_mid);
    lv_label_set_text(lbl_marine_header, "MARINE");
    lv_obj_align(lbl_marine_header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *lbl_us = lv_label_create(card_mid);
    lv_label_set_text(lbl_us, "US");
    lv_obj_align(lbl_us, LV_ALIGN_CENTER, 0, -35);

    led_us = lv_led_create(card_mid);
    lv_obj_set_size(led_us, 16, 16);
    lv_obj_align(led_us, LV_ALIGN_CENTER, 0, -15);
    lv_led_set_color(led_us, lv_palette_main(LV_PALETTE_GREEN));
    lv_led_off(led_us);

    lv_obj_t *lbl_ds = lv_label_create(card_mid);
    lv_label_set_text(lbl_ds, "DS");
    lv_obj_align(lbl_ds, LV_ALIGN_CENTER, 0, 15);

    led_ds = lv_led_create(card_mid);
    lv_obj_set_size(led_ds, 16, 16);
    lv_obj_align(led_ds, LV_ALIGN_CENTER, 0, 35);
    lv_led_set_color(led_ds, lv_palette_main(LV_PALETTE_GREEN));
    lv_led_off(led_ds);

    // --- Right Column: Traffic Lights ---
    lv_obj_t *card_lights = lv_obj_create(parent);
    lv_obj_set_size(card_lights, 100, 150);
    lv_obj_align(card_lights, LV_ALIGN_RIGHT_MID, -5, -10);
    lv_obj_add_style(card_lights, &style_card, 0);

    lv_obj_t *lbl2 = lv_label_create(card_lights);
    lv_label_set_text(lbl2, "TRAFFIC");
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
    lv_arc_set_range(meter_motor, 0, 100);   // shows commanded PWM duty (%)
    lv_obj_set_style_bg_color(meter_motor, lv_palette_main(LV_PALETTE_RED), LV_PART_KNOB);
    lv_obj_set_style_pad_all(meter_motor, 5, LV_PART_KNOB);
    lv_obj_clear_flag(meter_motor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(meter_motor, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
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
    lv_obj_set_size(card, 280, 90);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(card, &style_card, 0);

    lv_obj_t *lbl_title = lv_label_create(card);
    lv_label_set_text(lbl_title, "SYSTEM SETTINGS");
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, -5);

    lv_obj_t *lbl_theme = lv_label_create(card);
    lv_label_set_text(lbl_theme, "Dark Mode:");
    lv_obj_align(lbl_theme, LV_ALIGN_LEFT_MID, 10, 10);

    lv_obj_t *sw_theme = lv_switch_create(card);
    lv_obj_align(sw_theme, LV_ALIGN_RIGHT_MID, -10, 10);
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
    pg_cw = lv_tabview_add_tab(tv, "CW");
    pg_settings = lv_tabview_add_tab(tv, "SET");

    build_home(pg_home);
    build_ops(pg_ops);
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

    static SystemState_t last_sys_state = STATE_COUNT;
    if (s_local.state != last_sys_state) {
        char buf_sys[64];
        sprintf(buf_sys, "SYS: %s", state_names[s_local.state]);
        lv_label_set_text(lbl_state, buf_sys);
        lv_obj_set_style_text_color(lbl_state, sys_color, 0);
        last_sys_state = s_local.state;
    }

    // Motor status is derived from FSM state, not current. The L293D module
    // has no current-sense output, so motor_current_ma is always 0 — keying
    // the status off it would leave MOT permanently stuck on "IDLE". State +
    // commanded duty are the truthful signals we have.
    const char* mot_stat;
    lv_color_t  mot_color;
    switch (s_local.state) {
    case STATE_RAISING:       mot_stat = "UP";   mot_color = lv_palette_main(LV_PALETTE_CYAN);   break;
    case STATE_LOWERING:      mot_stat = "DOWN"; mot_color = lv_palette_main(LV_PALETTE_CYAN);   break;
    case STATE_RAISED_HOLD:   mot_stat = "HOLD"; mot_color = lv_palette_main(LV_PALETTE_ORANGE); break;
    case STATE_ROAD_CLEARING: mot_stat = "WAIT"; mot_color = lv_palette_main(LV_PALETTE_YELLOW); break;
    case STATE_FAULT:
    case STATE_ESTOP:         mot_stat = "STOP"; mot_color = lv_palette_main(LV_PALETTE_RED);    break;
    default:                  mot_stat = "IDLE"; mot_color = lv_color_white();                   break;
    }

    // mot_stat always points at one of the string literals above, so a
    // pointer compare is a valid (and cheap) change check.
    static const char* last_mot_stat = nullptr;
    if (mot_stat != last_mot_stat) {
        char buf_mot[32];
        sprintf(buf_mot, "MOT: %s", mot_stat);
        lv_label_set_text(lbl_mot_stat, buf_mot);
        lv_obj_set_style_text_color(lbl_mot_stat, mot_color, 0);
        last_mot_stat = mot_stat;
    }

    static int16_t last_pos = -1;
    if (s_local.deck_position_mm != last_pos) {
        char buf_p[16];
        sprintf(buf_p, "%d mm", s_local.deck_position_mm);
        lv_label_set_text(lbl_pos, buf_p);
        lv_bar_set_value(bar_pos, s_local.deck_position_mm, LV_ANIM_ON);
        last_pos = s_local.deck_position_mm;
    }

    // Arc gauge tracks commanded PWM duty (0..100 %).
    int16_t pwm_pct = (int16_t)((s_local.motor_pwm_duty * 100) / MOTOR_PWM_MAX);
    static int16_t last_pwm_pct = -1;
    if (pwm_pct != last_pwm_pct) {
        lv_arc_set_value(meter_motor, pwm_pct);
        last_pwm_pct = pwm_pct;
    }

    static SystemState_t last_road_state = STATE_COUNT;
    if (s_local.state != last_road_state) {
        lv_led_off(led_road_g); lv_led_off(led_road_y); lv_led_off(led_road_r);
        if(s_local.state == STATE_IDLE) lv_led_on(led_road_g);
        else if(s_local.state == STATE_ROAD_CLEARING) lv_led_on(led_road_y);
        else lv_led_on(led_road_r);
        last_road_state = s_local.state;
    }

    static VehicleDirection_t last_us_dir = DIR_NONE;
    if (s_local.laser.upstream_direction != last_us_dir) {
        if (s_local.laser.upstream_direction == DIR_APPROACHING) lv_led_on(led_us);
        else lv_led_off(led_us);
        last_us_dir = s_local.laser.upstream_direction;
    }

    static VehicleDirection_t last_ds_dir = DIR_NONE;
    if (s_local.laser.downstream_direction != last_ds_dir) {
        if (s_local.laser.downstream_direction == DIR_APPROACHING) lv_led_on(led_ds);
        else lv_led_off(led_ds);
        last_ds_dir = s_local.laser.downstream_direction;
    }

    static bool last_vessel_alert = false;
    static bool first_run_alert = true;
    if (s_local.laser.vessel_approaching != last_vessel_alert || first_run_alert) {
        if (s_local.laser.vessel_approaching) {
            lv_label_set_text(lbl_marine_header, "VESSEL\nALERT!");
            lv_obj_set_style_text_color(lbl_marine_header, lv_palette_main(LV_PALETTE_RED), 0);
        } else {
            lv_label_set_text(lbl_marine_header, "MARINE");
            // Cards use a white background (style_card) under the default light
            // theme, so the normal header must use dark text to stay visible —
            // white-on-white made "MARINE" invisible whenever no vessel was near.
            lv_obj_set_style_text_color(lbl_marine_header, lv_color_black(), 0);
        }
        last_vessel_alert = s_local.laser.vessel_approaching;
        first_run_alert = false;
    }

    static float last_cw_l = -1;
    static float last_cw_r = -1;
    if (s_local.counterweight.left.water_level_ml != last_cw_l) {
        lv_bar_set_value(bar_cw_l, (int)s_local.counterweight.left.water_level_ml, LV_ANIM_ON);
        char cw_stat_l[32];
        if(s_local.counterweight.left.pump_active) sprintf(cw_stat_l, "PUMPING\n%d ML", (int)s_local.counterweight.left.water_level_ml);
        else if(s_local.counterweight.left.drain_open) sprintf(cw_stat_l, "DRAINING\n%d ML", (int)s_local.counterweight.left.water_level_ml);
        else sprintf(cw_stat_l, "IDLE\n%d ML", (int)s_local.counterweight.left.water_level_ml);
        lv_label_set_text(lbl_cw_l_stat, cw_stat_l);
        last_cw_l = s_local.counterweight.left.water_level_ml;
    }
    
    if (s_local.counterweight.right.water_level_ml != last_cw_r) {
        lv_bar_set_value(bar_cw_r, (int)s_local.counterweight.right.water_level_ml, LV_ANIM_ON);
        char cw_stat_r[32];
        if(s_local.counterweight.right.pump_active) sprintf(cw_stat_r, "PUMPING\n%d ML", (int)s_local.counterweight.right.water_level_ml);
        else if(s_local.counterweight.right.drain_open) sprintf(cw_stat_r, "DRAINING\n%d ML", (int)s_local.counterweight.right.water_level_ml);
        else sprintf(cw_stat_r, "IDLE\n%d ML", (int)s_local.counterweight.right.water_level_ml);
        lv_label_set_text(lbl_cw_r_stat, cw_stat_r);
        last_cw_r = s_local.counterweight.right.water_level_ml;
    }
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

// ---------------------------------------------------------------------------
// Operator-panel command handler. Runs inside task_hmi (Core 1) so it may
// touch LVGL objects directly. The touchscreen buttons post FSM events
// straight from btn_event_cb; the 5-button resistor-ladder panel (input.cpp)
// routes its presses through g_hmi_cmd_queue into here.
// ---------------------------------------------------------------------------
static void handle_hmi_cmd(HmiCmd_t cmd) {
    SystemEvent_t evt = EVT_NONE;
    switch (cmd) {
    case HMI_CMD_RAISE:       evt = EVT_OPERATOR_RAISE; break;
    case HMI_CMD_LOWER:       evt = EVT_OPERATOR_LOWER; break;
    case HMI_CMD_HOLD:        evt = EVT_OPERATOR_HOLD;  break;
    case HMI_CMD_CLEAR_FAULT: fault_register_clear_all(); break;  // FSM gets EVT_FAULT_CLEARED on the falling edge
    case HMI_CMD_NEXT_SCREEN: {
        uint32_t cur = lv_tabview_get_tab_active(tv);
        lv_tabview_set_active(tv, (cur + 1) % 4, LV_ANIM_ON);
        break;
    }
    case HMI_CMD_PREV_SCREEN: {
        uint32_t cur = lv_tabview_get_tab_active(tv);
        lv_tabview_set_active(tv, (cur + 3) % 4, LV_ANIM_ON);   // +3 == -1 mod 4
        break;
    }
    default: break;
    }
    if (evt != EVT_NONE) xQueueSend(g_event_queue, &evt, 0);
}

// =====================================================================
// 9. The HMI task (Core 1)
// =====================================================================
void task_hmi(void* arg) {
    Serial.println("[hmi] task start (Core 1)");

    // Backlight (LED pin) is hard-wired to 3.3V on the display board, so screen
    // brightness is fixed and not software-controllable — no LEDC setup needed.

    s_tft.init();
    s_tft.invertDisplay(true); // Fix physical screen inversion
    s_tft.setRotation(3); // Landscape for 320x240
    s_tft.fillScreen(TFT_BLACK);

    uint16_t cal[5] = { 318, 3505, 273, 3539, 1 }; // 1 = rotate=1, inv_x=0, inv_y=0 (un-mirror X)
    s_tft.setTouch(cal);

    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);

    s_disp = lv_display_create(320, 240);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_buffers(s_disp, s_buf1, s_buf2, sizeof(s_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);

    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(s_indev, s_disp); // Mandatory in LVGL 9
    lv_indev_set_read_cb(s_indev, lvgl_touch_cb);

    s_lvgl_mutex = xSemaphoreCreateMutex();

    ui_init();

    uint32_t last_refresh = 0;

    for (;;) {
        uint8_t cmd;
        while (xQueueReceive(g_hmi_cmd_queue, &cmd, 0) == pdTRUE) {
            handle_hmi_cmd((HmiCmd_t)cmd);
        }

        if (millis() - last_refresh > 200) {
            last_refresh = millis();
            refresh_active();
        }

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(50)); // Safely yield
    }
}
