// ============================================================================
// hmi/display.h — TFT + LVGL public API.
// Owner: M4 Abigael
//
// This header defines the *contract* with the rest of the firmware:
//   - the entry-point task            (task_hmi)
//   - the operator-command type        (HmiCmd_t) and post helper
//
// The HMI is a pure consumer of g_status (polled every refresh), so the FSM
// does not push state to it — there is no notification API. The 5-button
// resistor-ladder panel (input.cpp) injects operator commands via hmi_cmd_post().
// All layout/visuals live in hmi/display.cpp.
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
// The HMI task polls g_status every refresh (200 ms), so the FSM does not need
// to push state changes or events to the display — no notification hooks are
// exposed. task_hmi is the only public entry point.
void task_hmi(void* arg);                                  // Pinned to Core 1

// --- Operator inputs (input.cpp emits these) ------------------------------
typedef enum : uint8_t {
    HMI_CMD_NONE = 0,
    HMI_CMD_RAISE,
    HMI_CMD_LOWER,
    HMI_CMD_HOLD,
    HMI_CMD_CLEAR_FAULT,
    HMI_CMD_NEXT_SCREEN
} HmiCmd_t;

bool hmi_cmd_post(HmiCmd_t cmd);
