// ============================================================================
// system_types.h — Vertical Lift Bridge Control System (Group 7)
// Shared enums, structs, and constants used across all firmware modules.
// Owner: George (M1 — System & FSM) | Reviewers: All
// ============================================================================
#pragma once

#include <Arduino.h>
#include <stdint.h>

// ----------------------------------------------------------------------------
// FSM States — single source of truth for system state machine.
// Order matters: keep IDLE first (0) so zero-init lands here.
// ----------------------------------------------------------------------------
typedef enum : uint8_t {
    STATE_IDLE = 0,           // Bridge down, road open, waiting for vehicle
    STATE_ROAD_CLEARING,      // Vehicle detected — closing barriers, red lights
    STATE_RAISING,            // Motor up, bridge ascending to top limit
    STATE_RAISED_HOLD,        // Bridge at top, holding for boat passage
    STATE_LOWERING,           // Motor down, bridge descending to bottom limit
    STATE_ROAD_OPENING,       // Bridge down, opening barriers, green lights
    STATE_FAULT,              // Recoverable fault — operator can clear
    STATE_ESTOP,              // E-stop pressed — hard latched, requires reset
    STATE_INIT,                // Boot-time self-test
    STATE_COUNT                // Sentinel (do not use as state)
} SystemState_t;

// ----------------------------------------------------------------------------
// FSM Events — every transition trigger. Sent via xQueue from producers
// (sensors, vision, HMI, watchdog) to the FSM task.
// ----------------------------------------------------------------------------
typedef enum : uint8_t {
    EVT_NONE = 0,
    EVT_VEHICLE_DETECTED,     // Vision/ultrasonic confirms approach
    EVT_VEHICLE_CLEARED,      // Vehicle past barrier zone
    EVT_BARRIER_CLOSED,       // Both servo barriers down
    EVT_BARRIER_OPEN,         // Both servo barriers up
    EVT_TOP_LIMIT_HIT,        // Top limit switch closed (or position == top)
    EVT_BOTTOM_LIMIT_HIT,     // Bottom limit switch closed (or position == 0)
    EVT_HOLD_TIMEOUT,         // Raised-hold timer expired
    EVT_ESTOP_PRESSED,        // E-stop NC contact opened (active-low IRQ)
    EVT_ESTOP_RELEASED,       // E-stop reset (key-switch acknowledged)
    EVT_FAULT_RAISED,         // Any module flagged a fault
    EVT_FAULT_CLEARED,        // Operator pressed Clear-Fault
    EVT_OPERATOR_RAISE,       // Manual raise from HMI/button
    EVT_OPERATOR_LOWER,       // Manual lower from HMI/button
    EVT_OPERATOR_HOLD,        // Manual freeze in current position
    EVT_TICK_100MS,           // Periodic tick for guards
    EVT_COUNT
} SystemEvent_t;

// ----------------------------------------------------------------------------
// Fault flags — bitmask. Stored in SharedStatus.fault_flags.
// Each bit is independent; multiple faults can coexist.
// Owner of fault register: M5 Ian (safety/fault_register.cpp).
// ----------------------------------------------------------------------------
typedef enum : uint32_t {
    FAULT_NONE              = 0,
    FAULT_OVERCURRENT       = 1u << 0,   // VPROPI > threshold
    FAULT_STALL             = 1u << 1,   // Motor on but no position change
    FAULT_LIMIT_BOTH        = 1u << 2,   // Top AND bottom limit asserted (impossible)
    FAULT_POS_OUT_OF_RANGE  = 1u << 3,   // Hall/encoder reads invalid
    FAULT_WATCHDOG          = 1u << 4,   // Software watchdog timeout
    FAULT_VISION_LINK_LOST  = 1u << 5,   // No JSON from ESP32-CAM > 2s
    FAULT_ULTRASONIC_FAIL   = 1u << 6,   // Both ultrasonics timed out
    FAULT_TFT_INIT_FAIL     = 1u << 7,   // Display did not init
    FAULT_TOUCH_INIT_FAIL   = 1u << 8,   // XPT2046 did not init
    FAULT_BARRIER_TIMEOUT   = 1u << 9,   // Servo barrier did not reach target
    FAULT_RELAY_FEEDBACK    = 1u << 10,  // Relay coil energised but contact open
    FAULT_UNDERVOLT_12V     = 1u << 11,  // 12V rail < 11.0V (via divider)
    FAULT_OVERTEMP          = 1u << 12,  // Optional NTC > 60°C
    FAULT_CONFIG_CRC        = 1u << 13,  // NVS config CRC mismatch
    FAULT_QUEUE_OVERFLOW    = 1u << 14,  // Event queue full — events dropped
    FAULT_TASK_HANG         = 1u << 15,  // Task watchdog kicked
} FaultFlag_t;

// ----------------------------------------------------------------------------
// Motor command — sent from FSM to motor driver task.
// ----------------------------------------------------------------------------
typedef enum : uint8_t {
    MOTOR_STOP = 0,
    MOTOR_UP,
    MOTOR_DOWN,
    MOTOR_BRAKE,              // Active short-brake (both lows high)
    MOTOR_COAST               // Both lows low — freewheel
} MotorDirection_t;

typedef struct {
    MotorDirection_t direction;
    uint16_t pwm_duty;        // 0..8191 (13-bit LEDC)
    uint32_t request_id;      // Monotonic, for ack tracing
} MotorCommand_t;

// ----------------------------------------------------------------------------
// Vehicle detection result — from ESP32-CAM via UART2 JSON.
// Updated by vision_link task; consumed by FSM.
// ----------------------------------------------------------------------------
typedef struct {
    bool      vehicle_present;
    uint8_t   confidence;     // 0..100 (frame-diff score)
    uint32_t  last_seen_ms;   // millis() at last detection
    uint16_t  bbox_x, bbox_y; // Optional bounding box (top-left)
    uint16_t  bbox_w, bbox_h;
    uint8_t   link_ok;        // 1 if heartbeat fresh, 0 if lost
} VisionStatus_t;

// ----------------------------------------------------------------------------
// Ultrasonic dual-beam direction sensor result.
// Owner: M3 Cindy (sensors/ultrasonic.cpp).
// ----------------------------------------------------------------------------
typedef enum : uint8_t {
    DIR_NONE = 0,
    DIR_APPROACHING,          // Beam A then beam B triggered
    DIR_LEAVING,              // Beam B then beam A triggered
    DIR_BOTH                  // Stationary in zone or two vehicles
} VehicleDirection_t;

typedef struct {
    uint16_t          distance_a_cm;   // 0..400, 0xFFFF = no echo
    uint16_t          distance_b_cm;
    bool              beam_a_blocked;  // Within trigger threshold (default 80cm)
    bool              beam_b_blocked;
    VehicleDirection_t direction;
    uint32_t          last_update_ms;
} UltrasonicStatus_t;

// ----------------------------------------------------------------------------
// SharedStatus — single struct guarded by a mutex.
// All tasks read/write through xSemaphoreTake(g_status_mutex, ...).
// Producers: motor (position, current), sensors (ultrasonic), vision, FSM.
// Consumer: HMI (display + telemetry), safety (fault evaluation).
// ----------------------------------------------------------------------------
typedef struct {
    // FSM
    SystemState_t   state;
    SystemState_t   prev_state;
    uint32_t        state_entered_ms;

    // Motor / mechanism
    int16_t         deck_position_mm;       // 0 = down, +N = up (target N_max)
    int16_t         deck_target_mm;
    uint16_t        motor_pwm_duty;
    int16_t         motor_current_ma;       // Signed — negative = regen (rare)
    bool            top_limit_hit;
    bool            bottom_limit_hit;

    // Barriers
    uint8_t         barrier_left_angle;     // 0..180 (servo)
    uint8_t         barrier_right_angle;
    bool            barrier_left_target_reached;
    bool            barrier_right_target_reached;

    // Lights (74HC595 chain)
    uint8_t         lights_road;            // Bitfield: bit0=R, bit1=Y, bit2=G
    uint8_t         lights_marine;

    // Sensing
    UltrasonicStatus_t ultrasonic;
    VisionStatus_t  vision;

    // Safety
    uint32_t        fault_flags;
    bool            estop_active;
    uint32_t        watchdog_last_kick_ms;

    // System health
    uint32_t        uptime_ms;
    uint32_t        cycle_count;
    uint32_t        last_cycle_duration_ms;
    int8_t          rssi_dbm;
    uint8_t         cpu_load_core0;
    uint8_t         cpu_load_core1;
    float           rail_12v_volts;
    float           rail_5v_volts;
    float           rail_3v3_volts;

    // HMI
    uint8_t         hmi_active_screen;      // 0..4
    uint8_t         hmi_brightness;         // 0..100
} SharedStatus_t;

// ----------------------------------------------------------------------------
// Tunables — adjusted at integration time. Keep defaults conservative.
// ----------------------------------------------------------------------------
#define DECK_HEIGHT_MAX_MM          175      // Mechanical full-up position
#define DECK_HEIGHT_MIN_MM          0
#define DECK_HEIGHT_TOLERANCE_MM    3        // ±3 mm for "limit reached"
#define MOTOR_PWM_MAX               8191     // 13-bit LEDC
#define MOTOR_PWM_RAISE_DEFAULT     5500     // ~67% duty
#define MOTOR_PWM_LOWER_DEFAULT     4500     // Lighter duty descending (gravity assist)
#define MOTOR_OVERCURRENT_MA        2200     // BTS7960 trip threshold (well below 5.5A drop-out)
#define MOTOR_STALL_TIMEOUT_MS      2000     // No position change while energised
#define BARRIER_DOWN_ANGLE          0
#define BARRIER_UP_ANGLE            90
#define BARRIER_TIMEOUT_MS          1500
#define HOLD_TIMEOUT_MS             8000     // Raised-hold for boat passage
#define ULTRASONIC_TRIGGER_CM       80
#define ULTRASONIC_BEAM_SPACING_CM  30       // For direction inference
#define VISION_HEARTBEAT_TIMEOUT_MS 2000
#define WATCHDOG_KICK_PERIOD_MS     500
#define WATCHDOG_MAX_INTERVAL_MS    1500

// ----------------------------------------------------------------------------
// Helper macros — keep lightweight, prefer inline functions for non-trivial logic.
// ----------------------------------------------------------------------------
#define SET_FAULT(reg, flag)    do { (reg) |=  (uint32_t)(flag); } while (0)
#define CLR_FAULT(reg, flag)    do { (reg) &= ~(uint32_t)(flag); } while (0)
#define HAS_FAULT(reg, flag)    (((reg) & (uint32_t)(flag)) != 0)
#define ANY_FAULT(reg)          ((reg) != 0)

// String helpers — defined in system_types.cpp (debug builds only).
#if defined(VLB_DEBUG_STRINGS)
const char* state_to_str(SystemState_t s);
const char* event_to_str(SystemEvent_t e);
#endif

// ----------------------------------------------------------------------------
// Globals declared here, defined in main.cpp.
// ----------------------------------------------------------------------------
extern SharedStatus_t           g_status;
extern SemaphoreHandle_t        g_status_mutex;
extern QueueHandle_t            g_event_queue;          // SystemEvent_t
extern QueueHandle_t            g_motor_cmd_queue;      // MotorCommand_t
extern QueueHandle_t            g_hmi_cmd_queue;        // uint8_t (HMI events)
