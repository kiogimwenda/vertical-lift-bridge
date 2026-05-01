// ============================================================================
// hmi/display.h — TFT + LVGL public API.
// Owner: M4 Abigael
//
// >>>>>>>>>>>>>  CREATIVE-LIBERTY TEMPLATE  <<<<<<<<<<<<<
// This header defines the *contract* with the rest of the firmware:
//   - the entry-point task            (task_hmi)
//   - the notification API            (display_notify_state_change, etc.)
//   - the screen IDs the rest of the
//     code may switch between        (HMI_SCREEN_*)
//
// Everything *visual* — layouts, fonts, colours, gauges, animations, icons —
// is intentionally left as TODO inside hmi/display.cpp. M4 has full creative
// liberty over the look and feel. This file only ensures the FSM, motor,
// and safety modules can talk to the display through a stable interface.
// ============================================================================
#pragma once
#include "../system_types.h"

// --- Screen identifiers (creative arrangement is M4's choice; FSM only
//     references logical IDs, not specific layouts). -----------------------
typedef enum : uint8_t {
    HMI_SCREEN_BOOT      = 0,
    HMI_SCREEN_MAIN      = 1,    // Main bridge status
    HMI_SCREEN_TELEMETRY = 2,    // Live numbers / charts
    HMI_SCREEN_FAULTS    = 3,    // Fault list + clear button
    HMI_SCREEN_SETTINGS  = 4,    // Brightness, manual mode, etc.
    HMI_SCREEN_COUNT
} HmiScreen_t;

// --- Lifecycle ------------------------------------------------------------
void task_hmi(void* arg);                                  // Pinned to Core 1
void display_notify_state_change(SystemState_t new_state); // Called from FSM
void display_notify_event(SystemEvent_t event);            // Optional hook
void display_request_screen(HmiScreen_t screen);           // External nav

// --- Operator inputs (input.cpp emits these) ------------------------------
typedef enum : uint8_t {
    HMI_CMD_NONE = 0,
    HMI_CMD_RAISE,
    HMI_CMD_LOWER,
    HMI_CMD_HOLD,
    HMI_CMD_CLEAR_FAULT,
    HMI_CMD_NEXT_SCREEN,
    HMI_CMD_PREV_SCREEN,
    HMI_CMD_BRIGHTNESS_UP,
    HMI_CMD_BRIGHTNESS_DOWN
} HmiCmd_t;

bool hmi_cmd_post(HmiCmd_t cmd);
