// ============================================================================
// hmi/display.cpp — LVGL + TFT_eSPI bring-up TEMPLATE.
// Owner: M4 Abigael
//
// >>>>>>>>>>>>>  CREATIVE-LIBERTY ZONE  <<<<<<<<<<<<<
//
// What is finished here (DON'T REMOVE — the rest of the firmware depends on it):
//   ✓ LVGL 9.x init, TFT_eSPI driver, double-buffered DMA flush
//   ✓ XPT2046 touch driver wired up (read_cb)
//   ✓ FreeRTOS-safe lv_async_call pattern documented
//   ✓ Tick task, refresh task, brightness LEDC ch2
//   ✓ Stub screen_create_*() functions per HmiScreen_t
//   ✓ Screen-switch logic, notification hooks called from FSM
//   ✓ Suggested colour-token table (override freely)
//   ✓ Helper: pull a thread-safe snapshot of g_status into local copy
//   ✓ Render-tick poll: call g_status_snapshot() at 5 Hz from inside LVGL
//
// What is YOURS to design (ALL TODO blocks below — go wild):
//   ▸ Visual layout of every screen (Boot, Main, Telemetry, Faults, Settings)
//   ▸ Choice of widgets — bar / arc / meter / chart / canvas / animimg…
//   ▸ Colour palette — override / extend the suggested tokens
//   ▸ Custom font(s)  — drop .c files into firmware/assets/fonts/
//   ▸ Icons          — drop .c files into firmware/assets/icons/
//   ▸ Animations     — lv_anim_t, transitions, fades
//   ▸ Touch zones, gestures, swipe navigation
//   ▸ Logo / branding on Boot screen
//   ▸ Telemetry chart styling (sparkline vs gauge vs bar)
//
// USEFUL TOOLS PRE-WIRED:
//   • LVGL 9.2.2 widgets: label, btn, bar, arc, slider, meter, chart, image,
//                          canvas, animimg, line, table, list, msgbox, tabview
//   • Inter font (multiple sizes) in assets/fonts/inter_*.c — use
//       lv_obj_set_style_text_font(obj, &inter_24, 0);
//   • Animations:    lv_anim_init(&a); lv_anim_set_var(&a, obj); …
//   • Custom theme:  lv_theme_t* th = lv_theme_default_init(...);
//   • Online font converter: https://lvgl.io/tools/fontconverter
//   • Online image converter: https://lvgl.io/tools/imageconverter
//   • LVGL widget gallery for inspiration: https://docs.lvgl.io/master/widgets/
//
// THREAD-SAFETY RULE (non-negotiable):
//   ANY call into LVGL from outside this file (e.g. display_notify_state_change
//   from FSM on Core 0) MUST go through lv_async_call() — never touch widgets
//   directly from another task or it will corrupt LVGL's internal state.
//   The notification hooks in this file already do this — please preserve.
// ============================================================================

#include "display.h"
#include "input.h"
#include "../pin_config.h"
#include "../system_types.h"
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// =====================================================================
// 1. SUGGESTED COLOUR / SPACING TOKENS — feel free to override.
// =====================================================================
// (LVGL uses RGB888 inside lv_color_hex; these are 24-bit hex codes.)
namespace ui_tokens {
    // Backgrounds
    static constexpr uint32_t BG          = 0x0D1117;   // Page bg (near-black)
    static constexpr uint32_t SURFACE     = 0x161B22;   // Card bg
    static constexpr uint32_t DIVIDER     = 0x30363D;   // Hairline
    // Semantic
    static constexpr uint32_t CLEAR       = 0x3FB950;   // OK / GO
    static constexpr uint32_t WARN        = 0xD29922;   // Caution
    static constexpr uint32_t ALERT       = 0xF85149;   // Stop / fault
    static constexpr uint32_t INFO        = 0x58A6FF;   // Info / link
    static constexpr uint32_t MOTOR       = 0xBC8CFF;   // Mechanism accent
    // Text
    static constexpr uint32_t TXT_PRIMARY = 0xE6EDF3;
    static constexpr uint32_t TXT_MUTED   = 0x8B949E;

    // Spacing (px)
    static constexpr uint8_t  PAD_S = 4;
    static constexpr uint8_t  PAD_M = 8;
    static constexpr uint8_t  PAD_L = 12;
}

// =====================================================================
// 2. TFT + LVGL infrastructure — DO NOT BREAK
// =====================================================================
#define TFT_W 240
#define TFT_H 320
#define LV_BUF_LINES 20 //Changed for 40 to 20 to reduce memory usage. Adjust as needed (higher = smoother but more RAM).

static TFT_eSPI    s_tft = TFT_eSPI(TFT_W, TFT_H);
static lv_display_t* s_disp = nullptr;
static lv_indev_t*  s_indev = nullptr;
static uint8_t      s_buf1[TFT_W * LV_BUF_LINES * 2];
static uint8_t      s_buf2[TFT_W * LV_BUF_LINES * 2];

// Screens — allocated lazily by ensure_screen()
static lv_obj_t*    s_screens[HMI_SCREEN_COUNT] = {nullptr};
static HmiScreen_t  s_active = HMI_SCREEN_BOOT;
static SemaphoreHandle_t s_lvgl_mutex = nullptr;

// LEDC for backlight
#define LEDC_CH_BL  2
#define LEDC_RES_BL 13

// =====================================================================
// 3. LVGL flush + touch callbacks (don't touch unless TFT pins change)
// =====================================================================
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
    bool touched = s_tft.getTouch(&tx, &ty, 600);
    if (touched) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = tx;
        data->point.y = ty;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static uint32_t lvgl_tick_cb(void) { return millis(); }

// =====================================================================
// 4. Local snapshot of shared status — call from LVGL context only.
// =====================================================================
static SharedStatus_t s_local;

static void g_status_snapshot(void) {
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_local = g_status;
        xSemaphoreGive(g_status_mutex);
    }
}

// =====================================================================
// 5. Screen factories — STUBS for M4 to fill in.
//    Each returns an lv_obj_t* (an LVGL screen). The infrastructure
//    swaps to the right one when display_request_screen() is called.
// =====================================================================

// --- BOOT SCREEN -----------------------------------------------------
// TODO (M4): replace with branded splash. Suggestions: animated logo,
// progress bar tied to s_local.uptime_ms, tagline.
static lv_obj_t* screen_create_boot(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(ui_tokens::BG), 0);

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "VLB Group 7\nBooting...");
    lv_obj_set_style_text_color(lbl, lv_color_hex(ui_tokens::TXT_PRIMARY), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    // TODO: lv_anim_t for fade-in, progress bar, custom logo, etc.
    return scr;
}

// --- MAIN STATUS SCREEN ----------------------------------------------
// TODO (M4): the most-seen screen. Things you'll want to bind to s_local:
//   - s_local.state              : show as big banner (CLEAR / WAIT / GO UP / GO DOWN / FAULT)
//   - s_local.deck_position_mm   : visualize as vertical bar / illustration
//   - s_local.lights_road        : R/Y/G dots
//   - s_local.lights_marine      : R/Y/G dots
//   - s_local.vision.vehicle_present : icon + confidence bar
//   - s_local.cycle_count        : footer
//   - s_local.fault_flags        : warning chip if non-zero
// Bind via lv_obj_t* persisted in static vars; refresh inside refresh_active().
static lv_obj_t* screen_create_main(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(ui_tokens::BG), 0);

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "MAIN\n(M4: design me)");
    lv_obj_set_style_text_color(lbl, lv_color_hex(ui_tokens::TXT_MUTED), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    // TODO M4: build out the full layout you want.
    return scr;
}

// --- TELEMETRY SCREEN ------------------------------------------------
// TODO (M4): real-time numbers. Suggested bindings:
//   - s_local.motor_pwm_duty      : bar 0..8191 -> 0..100%
//   - s_local.motor_current_ma    : arc gauge 0..MOTOR_OVERCURRENT_MA
//   - s_local.deck_position_mm    : meter 0..DECK_HEIGHT_MAX_MM
//   - s_local.last_cycle_duration_ms : label "Last cycle: 18.2s"
//   - s_local.uptime_ms           : label HH:MM:SS
//   - s_local.rssi_dbm, cpu_load_*, rail_*_volts
//
// COUNTERWEIGHT SIMULATION BINDINGS (new):
//   - s_local.counterweight.left.water_level_ml  : vertical bar 0..CW_TANK_CAPACITY_ML
//   - s_local.counterweight.right.water_level_ml : vertical bar 0..CW_TANK_CAPACITY_ML
//   - s_local.counterweight.left.pump_active     : pump icon on/off
//   - s_local.counterweight.right.pump_active    : pump icon on/off
//   - s_local.counterweight.left.drain_open      : drain icon on/off
//   - s_local.counterweight.right.drain_open     : drain icon on/off
//   - s_local.counterweight.left.target_ml       : target line on bar
//   - s_local.counterweight.balanced             : "BALANCED" / "BALANCING..." label
//
// Use lv_chart for a sparkline of motor current or position over time.

// Static widget pointers for counterweight display (refreshed in refresh_active)
static lv_obj_t* s_cw_bar_left   = nullptr;
static lv_obj_t* s_cw_bar_right  = nullptr;
static lv_obj_t* s_cw_lbl_left   = nullptr;
static lv_obj_t* s_cw_lbl_right  = nullptr;
static lv_obj_t* s_cw_lbl_status = nullptr;
static lv_obj_t* s_cw_lbl_pump_l = nullptr;
static lv_obj_t* s_cw_lbl_pump_r = nullptr;

static lv_obj_t* screen_create_telemetry(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(ui_tokens::BG), 0);

    // Title
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "TELEMETRY");
    lv_obj_set_style_text_color(title, lv_color_hex(ui_tokens::TXT_PRIMARY), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    // --- Counterweight simulation panel ---
    lv_obj_t* cw_panel = lv_obj_create(scr);
    lv_obj_set_size(cw_panel, 230, 140);
    lv_obj_align(cw_panel, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_color(cw_panel, lv_color_hex(ui_tokens::SURFACE), 0);
    lv_obj_set_style_border_width(cw_panel, 1, 0);
    lv_obj_set_style_border_color(cw_panel, lv_color_hex(ui_tokens::DIVIDER), 0);
    lv_obj_set_style_radius(cw_panel, 4, 0);
    lv_obj_set_style_pad_all(cw_panel, ui_tokens::PAD_S, 0);

    lv_obj_t* cw_title = lv_label_create(cw_panel);
    lv_label_set_text(cw_title, "Counterweight Tanks");
    lv_obj_set_style_text_color(cw_title, lv_color_hex(ui_tokens::INFO), 0);
    lv_obj_align(cw_title, LV_ALIGN_TOP_MID, 0, 0);

    // Left tank bar
    lv_obj_t* lbl_l = lv_label_create(cw_panel);
    lv_label_set_text(lbl_l, "L");
    lv_obj_set_style_text_color(lbl_l, lv_color_hex(ui_tokens::TXT_MUTED), 0);
    lv_obj_align(lbl_l, LV_ALIGN_BOTTOM_LEFT, 20, -2);

    s_cw_bar_left = lv_bar_create(cw_panel);
    lv_obj_set_size(s_cw_bar_left, 20, 80);
    lv_bar_set_range(s_cw_bar_left, 0, (int32_t)CW_TANK_CAPACITY_ML);
    lv_bar_set_value(s_cw_bar_left, (int32_t)CW_SIM_TARGET_DEFAULT_ML, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_cw_bar_left, lv_color_hex(ui_tokens::INFO), LV_PART_INDICATOR);
    lv_obj_align(s_cw_bar_left, LV_ALIGN_LEFT_MID, 15, 5);

    s_cw_lbl_left = lv_label_create(cw_panel);
    lv_label_set_text(s_cw_lbl_left, "120ml");
    lv_obj_set_style_text_color(s_cw_lbl_left, lv_color_hex(ui_tokens::TXT_MUTED), 0);
    lv_obj_align(s_cw_lbl_left, LV_ALIGN_LEFT_MID, 40, 5);

    // Right tank bar
    lv_obj_t* lbl_r = lv_label_create(cw_panel);
    lv_label_set_text(lbl_r, "R");
    lv_obj_set_style_text_color(lbl_r, lv_color_hex(ui_tokens::TXT_MUTED), 0);
    lv_obj_align(lbl_r, LV_ALIGN_BOTTOM_RIGHT, -20, -2);

    s_cw_bar_right = lv_bar_create(cw_panel);
    lv_obj_set_size(s_cw_bar_right, 20, 80);
    lv_bar_set_range(s_cw_bar_right, 0, (int32_t)CW_TANK_CAPACITY_ML);
    lv_bar_set_value(s_cw_bar_right, (int32_t)CW_SIM_TARGET_DEFAULT_ML, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_cw_bar_right, lv_color_hex(ui_tokens::INFO), LV_PART_INDICATOR);
    lv_obj_align(s_cw_bar_right, LV_ALIGN_RIGHT_MID, -15, 5);

    s_cw_lbl_right = lv_label_create(cw_panel);
    lv_label_set_text(s_cw_lbl_right, "120ml");
    lv_obj_set_style_text_color(s_cw_lbl_right, lv_color_hex(ui_tokens::TXT_MUTED), 0);
    lv_obj_align(s_cw_lbl_right, LV_ALIGN_RIGHT_MID, -40, 5);

    // Pump/drain status labels
    s_cw_lbl_pump_l = lv_label_create(cw_panel);
    lv_label_set_text(s_cw_lbl_pump_l, "IDLE");
    lv_obj_set_style_text_color(s_cw_lbl_pump_l, lv_color_hex(ui_tokens::TXT_MUTED), 0);
    lv_obj_align(s_cw_lbl_pump_l, LV_ALIGN_LEFT_MID, 40, 20);

    s_cw_lbl_pump_r = lv_label_create(cw_panel);
    lv_label_set_text(s_cw_lbl_pump_r, "IDLE");
    lv_obj_set_style_text_color(s_cw_lbl_pump_r, lv_color_hex(ui_tokens::TXT_MUTED), 0);
    lv_obj_align(s_cw_lbl_pump_r, LV_ALIGN_RIGHT_MID, -40, 20);

    // Balanced status
    s_cw_lbl_status = lv_label_create(cw_panel);
    lv_label_set_text(s_cw_lbl_status, "BALANCED");
    lv_obj_set_style_text_color(s_cw_lbl_status, lv_color_hex(ui_tokens::CLEAR), 0);
    lv_obj_align(s_cw_lbl_status, LV_ALIGN_BOTTOM_MID, 0, -2);

    // TODO M4: Add motor current arc, deck position bar, sparkline below
    // the counterweight panel. Design the rest of this screen freely.

    return scr;
}

// --- FAULTS SCREEN ---------------------------------------------------
// TODO (M4): list every set bit in s_local.fault_flags with name + colour.
// Add a "Clear" button -> calls hmi_cmd_post(HMI_CMD_CLEAR_FAULT).
// Use fault_register_first_name() for human-readable strings.
static lv_obj_t* screen_create_faults(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(ui_tokens::BG), 0);

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "FAULTS\n(M4: design me)");
    lv_obj_set_style_text_color(lbl, lv_color_hex(ui_tokens::ALERT), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    return scr;
}

// --- SETTINGS SCREEN -------------------------------------------------
// TODO (M4): brightness slider (writes LEDC ch2 duty), manual mode toggle
// (emits HMI_CMD_RAISE / HMI_CMD_LOWER), buzzer mute, screen rotation.
static lv_obj_t* screen_create_settings(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(ui_tokens::BG), 0);

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "SETTINGS\n(M4: design me)");
    lv_obj_set_style_text_color(lbl, lv_color_hex(ui_tokens::TXT_MUTED), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    return scr;
}

// =====================================================================
// 6. Screen registry & switching
// =====================================================================
static lv_obj_t* ensure_screen(HmiScreen_t s) {
    if (s_screens[s]) return s_screens[s];
    switch (s) {
    case HMI_SCREEN_BOOT:      s_screens[s] = screen_create_boot();      break;
    case HMI_SCREEN_MAIN:      s_screens[s] = screen_create_main();      break;
    case HMI_SCREEN_TELEMETRY: s_screens[s] = screen_create_telemetry(); break;
    case HMI_SCREEN_FAULTS:    s_screens[s] = screen_create_faults();    break;
    case HMI_SCREEN_SETTINGS:  s_screens[s] = screen_create_settings();  break;
    default: break;
    }
    return s_screens[s];
}

static void switch_to(HmiScreen_t s) {
    lv_obj_t* scr = ensure_screen(s);
    if (scr) {
        lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
        s_active = s;
        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_status.hmi_active_screen = (uint8_t)s;
            xSemaphoreGive(g_status_mutex);
        }
    }
}

// Async wrapper used by external tasks (FSM, etc.)
static void async_switch_cb(void* p) {
    switch_to((HmiScreen_t)(uintptr_t)p);
}

// =====================================================================
// 7. Per-screen refresh — invoked at 5 Hz from inside LVGL task.
//    TODO (M4): update the widgets you create inside each screen factory.
// =====================================================================
static void refresh_active(void) {
    g_status_snapshot();
    switch (s_active) {
    case HMI_SCREEN_MAIN:
        // TODO M4: lv_label_set_text_fmt(banner_lbl, "%s",
        //                                state_to_str(s_local.state));
        break;
    case HMI_SCREEN_TELEMETRY:
        // Counterweight simulation display
        if (s_cw_bar_left) {
            lv_bar_set_value(s_cw_bar_left, (int32_t)s_local.counterweight.left.water_level_ml, LV_ANIM_ON);
            lv_label_set_text_fmt(s_cw_lbl_left, "%.0fml", s_local.counterweight.left.water_level_ml);

            if (s_local.counterweight.left.pump_active)
                lv_label_set_text(s_cw_lbl_pump_l, "PUMP");
            else if (s_local.counterweight.left.drain_open)
                lv_label_set_text(s_cw_lbl_pump_l, "DRAIN");
            else
                lv_label_set_text(s_cw_lbl_pump_l, "IDLE");

            lv_obj_set_style_text_color(s_cw_lbl_pump_l,
                lv_color_hex(s_local.counterweight.left.pump_active ? ui_tokens::CLEAR :
                             s_local.counterweight.left.drain_open  ? ui_tokens::WARN  :
                             ui_tokens::TXT_MUTED), 0);
        }
        if (s_cw_bar_right) {
            lv_bar_set_value(s_cw_bar_right, (int32_t)s_local.counterweight.right.water_level_ml, LV_ANIM_ON);
            lv_label_set_text_fmt(s_cw_lbl_right, "%.0fml", s_local.counterweight.right.water_level_ml);

            if (s_local.counterweight.right.pump_active)
                lv_label_set_text(s_cw_lbl_pump_r, "PUMP");
            else if (s_local.counterweight.right.drain_open)
                lv_label_set_text(s_cw_lbl_pump_r, "DRAIN");
            else
                lv_label_set_text(s_cw_lbl_pump_r, "IDLE");

            lv_obj_set_style_text_color(s_cw_lbl_pump_r,
                lv_color_hex(s_local.counterweight.right.pump_active ? ui_tokens::CLEAR :
                             s_local.counterweight.right.drain_open  ? ui_tokens::WARN  :
                             ui_tokens::TXT_MUTED), 0);
        }
        if (s_cw_lbl_status) {
            if (s_local.counterweight.balanced) {
                lv_label_set_text(s_cw_lbl_status, "BALANCED");
                lv_obj_set_style_text_color(s_cw_lbl_status, lv_color_hex(ui_tokens::CLEAR), 0);
            } else {
                lv_label_set_text(s_cw_lbl_status, "BALANCING...");
                lv_obj_set_style_text_color(s_cw_lbl_status, lv_color_hex(ui_tokens::WARN), 0);
            }
        }
        // TODO M4: lv_arc_set_value(current_arc, s_local.motor_current_ma);
        break;
    case HMI_SCREEN_FAULTS:
        // TODO M4: rebuild list if s_local.fault_flags changed.
        break;
    default: break;
    }
}

// =====================================================================
// 8. Public notification hooks — safe from any task.
// =====================================================================
void display_notify_state_change(SystemState_t new_state) {
    // Cheap: just update local snapshot next refresh.
    // If you want to switch screens automatically on FAULT/ESTOP, do it here:
    if (new_state == STATE_FAULT || new_state == STATE_ESTOP) {
        lv_async_call(async_switch_cb, (void*)(uintptr_t)HMI_SCREEN_FAULTS);
    }
}
void display_notify_event(SystemEvent_t event) { (void)event; /* TODO M4 */ }
void display_request_screen(HmiScreen_t s) {
    lv_async_call(async_switch_cb, (void*)(uintptr_t)s);
}

// HMI command queue post (called from input.cpp on touch / button press)
bool hmi_cmd_post(HmiCmd_t cmd) {
    uint8_t c = (uint8_t)cmd;
    return xQueueSend(g_hmi_cmd_queue, &c, 0) == pdTRUE;
}

// =====================================================================
// 9. The HMI task (Core 1)
// =====================================================================
void task_hmi(void* arg) {
    Serial.println("[hmi] task start (Core 1)");

    // --- Backlight ------------------------------------------------------
    ledcSetup(LEDC_CH_BL, 5000, LEDC_RES_BL);
    ledcAttachPin(PIN_TFT_BL, LEDC_CH_BL);
    ledcWrite(LEDC_CH_BL, (1 << LEDC_RES_BL) * 80 / 100);    // 80% default

    // --- TFT init -------------------------------------------------------
    s_tft.init();
    s_tft.setRotation(0);
    s_tft.fillScreen(TFT_BLACK);
    s_tft.initDMA();

    // --- Touch calibration (runs once, persisted to NVS in production) --
    // TODO M4: persist these via Preferences.h. For now, defaults from spec.
    uint16_t cal[5] = { 318, 3505, 273, 3539, 6 };
    s_tft.setTouch(cal);

    // --- LVGL init ------------------------------------------------------
    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);

    s_disp = lv_display_create(TFT_W, TFT_H);
    lv_display_set_buffers(s_disp, s_buf1, s_buf2, sizeof(s_buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);

    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, lvgl_touch_cb);

    s_lvgl_mutex = xSemaphoreCreateMutex();

    // --- Boot screen, then auto-advance to main after 1.5 s ------------
    switch_to(HMI_SCREEN_BOOT);

    TickType_t last = xTaskGetTickCount();
    uint32_t boot_until = millis() + 1500;
    uint32_t last_refresh = 0;

    for (;;) {
        // 1. Drain HMI command queue (touch callbacks / external triggers)
        uint8_t cmd;
        while (xQueueReceive(g_hmi_cmd_queue, &cmd, 0) == pdTRUE) {
            // TODO M4: dispatch HMI commands. Examples wired below.
            switch ((HmiCmd_t)cmd) {
            case HMI_CMD_NEXT_SCREEN: {
                int n = ((int)s_active + 1) % HMI_SCREEN_COUNT;
                if (n == HMI_SCREEN_BOOT) n = HMI_SCREEN_MAIN;
                switch_to((HmiScreen_t)n);
            } break;
            case HMI_CMD_PREV_SCREEN: {
                int n = ((int)s_active - 1 + HMI_SCREEN_COUNT) % HMI_SCREEN_COUNT;
                if (n == HMI_SCREEN_BOOT) n = HMI_SCREEN_SETTINGS;
                switch_to((HmiScreen_t)n);
            } break;
            case HMI_CMD_RAISE: {
                SystemEvent_t e = EVT_OPERATOR_RAISE;
                xQueueSend(g_event_queue, &e, 0);
            } break;
            case HMI_CMD_LOWER: {
                SystemEvent_t e = EVT_OPERATOR_LOWER;
                xQueueSend(g_event_queue, &e, 0);
            } break;
            case HMI_CMD_HOLD: {
                SystemEvent_t e = EVT_OPERATOR_HOLD;
                xQueueSend(g_event_queue, &e, 0);
            } break;
            case HMI_CMD_CLEAR_FAULT: {
                SystemEvent_t e = EVT_FAULT_CLEARED;
                xQueueSend(g_event_queue, &e, 0);
            } break;
            case HMI_CMD_BRIGHTNESS_UP: {
                static uint8_t pct = 80;
                if (pct < 100) pct += 10;
                ledcWrite(LEDC_CH_BL, (1 << LEDC_RES_BL) * pct / 100);
            } break;
            case HMI_CMD_BRIGHTNESS_DOWN: {
                static uint8_t pct = 80;
                if (pct > 10) pct -= 10;
                ledcWrite(LEDC_CH_BL, (1 << LEDC_RES_BL) * pct / 100);
            } break;
            default: break;
            }
        }

        // 2. Auto-leave boot screen
        if (s_active == HMI_SCREEN_BOOT && millis() > boot_until) {
            switch_to(HMI_SCREEN_MAIN);
        }

        // 3. Refresh screen at 5 Hz
        if (millis() - last_refresh > 200) {
            last_refresh = millis();
            refresh_active();
        }

        // 4. LVGL tick
        lv_timer_handler();

        vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
    }
}
