#include <lvgl.h>
#include "system_types.h"
#include <stdio.h>

extern SharedStatus_t g_status;

// --- UI Components ---
static lv_obj_t *tv;
static lv_obj_t *pg_home, *pg_ops, *pg_vision, *pg_diag, *pg_settings;
static lv_obj_t *lbl_state, *lbl_pos, *bar_pos;
static lv_obj_t *led_road_r, *led_road_y, *led_road_g;
static lv_obj_t *led_marine_r, *led_marine_g;
static lv_obj_t *meter_motor;
static lv_meter_indicator_t *indic_motor;

// --- Styles ---
static lv_style_t style_card;
static lv_style_t style_btn_modern;

void init_styles() {
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_bg_opa(&style_card, 40);
    lv_style_set_radius(&style_card, 8);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, lv_palette_main(LV_PALETTE_BLUE_GREY));

    lv_style_init(&style_btn_modern);
    lv_style_set_radius(&style_btn_modern, 4);
    lv_style_set_bg_color(&style_btn_modern, lv_palette_darken(LV_PALETTE_BLUE, 2));
    lv_style_set_text_color(&style_btn_modern, lv_color_white());
}

// --- Callbacks ---
static void btn_event_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    const char * txt = lv_label_get_text(lv_obj_get_child(btn, 0));
    
    if(strcmp(txt, "RAISE") == 0) g_status.state = STATE_RAISING;
    if(strcmp(txt, "LOWER") == 0) g_status.state = STATE_LOWERING;
    if(strcmp(txt, "HOLD") == 0) g_status.state = STATE_IDLE; // Simplified for simulation
}

// --- Page Builders ---
void build_home(lv_obj_t *parent) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 140, 180);
    lv_obj_align(card, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_add_style(card, &style_card, 0);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, "BRIDGE HEIGHT");
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 0);

    bar_pos = lv_bar_create(card);
    lv_obj_set_size(bar_pos, 20, 120);
    lv_obj_align(bar_pos, LV_ALIGN_CENTER, 0, 10);
    lv_bar_set_range(bar_pos, 0, DECK_HEIGHT_MAX_MM);

    lbl_pos = lv_label_create(card);
    lv_obj_align(lbl_pos, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(lbl_pos, &lv_font_montserrat_18, 0);

    // Traffic Lights Card
    lv_obj_t *card_lights = lv_obj_create(parent);
    lv_obj_set_size(card_lights, 140, 180);
    lv_obj_align(card_lights, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_add_style(card_lights, &style_card, 0);

    lv_obj_t *lbl2 = lv_label_create(card_lights);
    lv_label_set_text(lbl2, "TRAFFIC");
    lv_obj_align(lbl2, LV_ALIGN_TOP_MID, 0, 0);

    led_road_g = lv_led_create(card_lights);
    lv_obj_set_size(led_road_g, 20, 20);
    lv_obj_align(led_road_g, LV_ALIGN_CENTER, 0, -30);
    lv_led_set_color(led_road_g, lv_palette_main(LV_PALETTE_GREEN));

    led_road_y = lv_led_create(card_lights);
    lv_obj_set_size(led_road_y, 20, 20);
    lv_obj_align(led_road_y, LV_ALIGN_CENTER, 0, 0);
    lv_led_set_color(led_road_y, lv_palette_main(LV_PALETTE_AMBER));

    led_road_r = lv_led_create(card_lights);
    lv_obj_set_size(led_road_r, 20, 20);
    lv_obj_align(led_road_r, LV_ALIGN_CENTER, 0, 30);
    lv_led_set_color(led_road_r, lv_palette_main(LV_PALETTE_RED));
}

void build_ops(lv_obj_t *parent) {
    lv_obj_t *btn_raise = lv_btn_create(parent);
    lv_obj_set_size(btn_raise, 100, 40);
    lv_obj_align(btn_raise, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_style(btn_raise, &style_btn_modern, 0);
    lv_obj_t *l1 = lv_label_create(btn_raise);
    lv_label_set_text(l1, "RAISE");
    lv_obj_center(l1);
    lv_obj_add_event_cb(btn_raise, btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lower = lv_btn_create(parent);
    lv_obj_set_size(btn_lower, 100, 40);
    lv_obj_align(btn_lower, LV_ALIGN_TOP_LEFT, 10, 60);
    lv_obj_add_style(btn_lower, &style_btn_modern, 0);
    lv_obj_t *l2 = lv_label_create(btn_lower);
    lv_label_set_text(l2, "LOWER");
    lv_obj_center(l2);
    lv_obj_add_event_cb(btn_lower, btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_hold = lv_btn_create(parent);
    lv_obj_set_size(btn_hold, 100, 40);
    lv_obj_align(btn_hold, LV_ALIGN_TOP_LEFT, 10, 110);
    lv_obj_set_style_bg_color(btn_hold, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_t *l3 = lv_label_create(btn_hold);
    lv_label_set_text(l3, "HOLD");
    lv_obj_center(l3);
    lv_obj_add_event_cb(btn_hold, btn_event_cb, LV_EVENT_CLICKED, NULL);

    meter_motor = lv_meter_create(parent);
    lv_obj_set_size(meter_motor, 150, 150);
    lv_obj_align(meter_motor, LV_ALIGN_RIGHT_MID, -10, 0);

    lv_meter_scale_t * scale = lv_meter_add_scale(meter_motor);
    lv_meter_set_scale_ticks(meter_motor, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_range(meter_motor, scale, 0, 2500, 270, 135);

    indic_motor = lv_meter_add_needle_line(meter_motor, scale, 4, lv_palette_main(LV_PALETTE_CYAN), -10);
}

void ui_init() {
    init_styles();

    // Top Bar
    lv_obj_t *header = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header, 320, 30);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_palette_darken(LV_PALETTE_GREY, 4), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);

    lbl_state = lv_label_create(header);
    lv_label_set_text(lbl_state, "SYSTEM IDLE");
    lv_obj_align(lbl_state, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(lbl_state, lv_palette_main(LV_PALETTE_GREEN), 0);

    // Main Tabview
    tv = lv_tabview_create(lv_scr_act(), LV_DIR_BOTTOM, 40);
    lv_obj_align(tv, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_size(tv, 320, 210);

    pg_home = lv_tabview_add_tab(tv, "HOME");
    pg_ops = lv_tabview_add_tab(tv, "OPS");
    pg_vision = lv_tabview_add_tab(tv, "VISION");
    pg_diag = lv_tabview_add_tab(tv, "DIAG");
    pg_settings = lv_tabview_add_tab(tv, "SET");

    build_home(pg_home);
    build_ops(pg_ops);
    
    // Timer to update UI from g_status
    lv_timer_create([](lv_timer_t * t) {
        // Update State
        char buf_s[32];
        const char* state_names[] = {"IDLE", "CLEARING", "RAISING", "HOLD", "LOWERING", "OPENING", "FAULT", "ESTOP", "INIT"};
        sprintf(buf_s, "SYSTEM %s", state_names[g_status.state]);
        lv_label_set_text(lbl_state, buf_s);
        
        if(g_status.state == STATE_FAULT || g_status.state == STATE_ESTOP)
            lv_obj_set_style_text_color(lbl_state, lv_palette_main(LV_PALETTE_RED), 0);
        else
            lv_obj_set_style_text_color(lbl_state, lv_palette_main(LV_PALETTE_CYAN), 0);

        // Update Position
        char buf_p[16];
        sprintf(buf_p, "%d mm", g_status.deck_position_mm);
        lv_label_set_text(lbl_pos, buf_p);
        lv_bar_set_value(bar_pos, g_status.deck_position_mm, LV_ANIM_ON);

        // Update Meter
        lv_meter_set_indicator_value(meter_motor, indic_motor, g_status.motor_current_ma);

        // Update LEDs
        lv_led_off(led_road_g); lv_led_off(led_road_y); lv_led_off(led_road_r);
        if(g_status.state == STATE_IDLE) lv_led_on(led_road_g);
        else if(g_status.state == STATE_ROAD_CLEARING) lv_led_on(led_road_y);
        else lv_led_on(led_road_r);

    }, 100, NULL);
}
