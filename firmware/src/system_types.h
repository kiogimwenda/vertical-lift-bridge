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
// (sensors, HMI, watchdog) to the FSM task.
// ----------------------------------------------------------------------------
typedef enum : uint8_t {
    EVT_NONE = 0,
    EVT_VEHICLE_DETECTED,     // RESERVED — manual-raise design: vessel state is surfaced via g_status.laser (HMI + safe_to_lower guard), not as an FSM transition
    EVT_VEHICLE_CLEARED,      // RESERVED — see EVT_VEHICLE_DETECTED
    EVT_BARRIER_CLOSED,       // Both servo barriers down
    EVT_BARRIER_OPEN,         // Both servo barriers up
    EVT_TOP_LIMIT_HIT,        // Deck reached top end-stop (timer-estimated, no switch)
    EVT_BOTTOM_LIMIT_HIT,     // Deck reached bottom end-stop (timer-estimated, no switch)
    EVT_HOLD_TIMEOUT,         // RESERVED — manual lowering only, no auto-lower timer
    EVT_ESTOP_PRESSED,        // E-stop NC contact opened (active-low IRQ)
    EVT_ESTOP_RELEASED,       // E-stop reset (key-switch acknowledged)
    EVT_FAULT_RAISED,         // Any module flagged a fault
    EVT_FAULT_CLEARED,        // Operator pressed Clear-Fault
    EVT_OPERATOR_RAISE,       // Manual raise from HMI/button
    EVT_OPERATOR_LOWER,       // Manual lower from HMI/button
    EVT_OPERATOR_HOLD,        // Manual freeze in current position
    EVT_CW_READY,             // Simulated counterweights reached target
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
    FAULT_OVERCURRENT       = 1u << 0,   // RESERVED — L298N module has no MCU-facing current-sense output. Bit never set; thermal protection is on-chip.
    FAULT_STALL             = 1u << 1,   // Motor run-time exceeded a full traverse + margin (runaway / jam guard — see DECK_*_TIME_MS)
    FAULT_LIMIT_BOTH        = 1u << 2,   // RESERVED — limit switches omitted in this build (timer-based end-stops). Never set.
    FAULT_POS_OUT_OF_RANGE  = 1u << 3,   // RESERVED — position potentiometer omitted in this build. Never set.
    FAULT_WATCHDOG          = 1u << 4,   // Software watchdog timeout
    FAULT_VISION_LINK_LOST  = 1u << 5,   // RESERVED — vision module omitted from this build. Never set.
    FAULT_LASER_FAIL        = 1u << 6,   // Laser fail flag
    FAULT_TFT_INIT_FAIL     = 1u << 7,   // Display did not init
    FAULT_TOUCH_INIT_FAIL   = 1u << 8,   // XPT2046 did not init
    FAULT_BARRIER_TIMEOUT   = 1u << 9,   // Barrier servo sweep did not reach target within BARRIER_TIMEOUT_MS (interlocks watchdog on the open-loop sweep).
    FAULT_RELAY_FEEDBACK    = 1u << 10,  // Relay coil energised but contact open
    FAULT_UNDERVOLT_12V     = 1u << 11,  // DEPRECATED v2.1 — rail monitoring removed (see known_limitations.md). Bit reserved; never set.
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
// Laser Break-Beam quad-sensor direction detection.
// Two pairs (upstream LDR1+LDR2, downstream LDR3+LDR4), each with beams A and B
// spaced 3 cm apart. Beam-arrival order within a pair reveals vessel direction.
// Owner: M3 Cindy (sensors/laser.cpp).
// ----------------------------------------------------------------------------
typedef enum : uint8_t {
    DIR_NONE = 0,
    DIR_APPROACHING,          // Vessel moving toward the bridge
    DIR_LEAVING,              // Vessel moving away from the bridge
    DIR_BOTH                  // Stationary in zone or two vessels
} VehicleDirection_t;

typedef struct {
    // Upstream pair (LDR1 = Beam A, LDR2 = Beam B)
    bool               upstream_blocked;  // Either LDR1 or LDR2 blocked
    VehicleDirection_t upstream_direction;

    // Downstream pair (LDR3 = Beam A, LDR4 = Beam B)
    bool               downstream_blocked;
    VehicleDirection_t downstream_direction;

    // Combined decision
    bool               vessel_approaching; // true if either side says APPROACHING
    uint32_t           last_update_ms;
} LaserStatus_t;

// ----------------------------------------------------------------------------
// Simulated dynamic counterweight system.
// Pure software simulation — no physical pumps, valves, or sensors.
// Models two water tanks that fill/drain to balance the bridge deck.
// Owner: M2 Eugene (counterweight/counterweight.cpp).
// ----------------------------------------------------------------------------
typedef struct {
    float    water_level_ml;     // 0.0 .. CW_TANK_CAPACITY_ML
    float    weight_g;           // water_level_ml * 1.0 (1 ml ≈ 1 g)
    bool     pump_active;        // Simulated pump running
    bool     drain_open;         // Simulated drain valve open
    float    target_ml;          // Desired fill level
} CwTankSim_t;

typedef struct {
    CwTankSim_t left;
    CwTankSim_t right;
    bool        balanced;        // Both tanks within tolerance of target
    uint32_t    last_update_ms;
} CounterweightStatus_t;

// ----------------------------------------------------------------------------
// SharedStatus — single struct guarded by a mutex.
// All tasks read/write through xSemaphoreTake(g_status_mutex, ...).
// Producers: motor (timer-estimated position), sensors (laser),
//            counterweight (sim), interlocks, FSM.
// Consumer: HMI (display + telemetry), safety (fault evaluation).
// ----------------------------------------------------------------------------
typedef struct {
    // FSM
    SystemState_t   state;
    SystemState_t   prev_state;
    uint32_t        state_entered_ms;

    // Motor / mechanism
    int16_t         deck_position_mm;       // 0 = down, +N = up. Timer-estimated (no pot/encoder).
    int16_t         deck_target_mm;
    uint16_t        motor_pwm_duty;
    int16_t         motor_current_ma;       // Always 0 — L298N module has no MCU-facing current sense
    bool            top_limit_hit;          // Virtual end-stop: deck at top (timer-estimated)
    bool            bottom_limit_hit;       // Virtual end-stop: deck at bottom (timer-estimated)

    // Barriers
    uint8_t         barrier_left_angle;     // 0..180 (servo)
    uint8_t         barrier_right_angle;
    bool            barrier_left_target_reached;
    bool            barrier_right_target_reached;

    // Lights (74HC595 chain)
    uint8_t         lights_road;            // Bitfield: bit0=R, bit1=Y, bit2=G

    // Sensing
    LaserStatus_t   laser;

    // Simulated counterweight
    CounterweightStatus_t counterweight;

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
    float           rail_12v_volts;         // v2.1: -1.0f sentinel — see known_limitations.md
    float           rail_5v_volts;          // v2.1: -1.0f sentinel
    float           rail_3v3_volts;         // v2.1: -1.0f sentinel

    // HMI
    uint8_t         hmi_active_screen;      // 0..4
} SharedStatus_t;

// ----------------------------------------------------------------------------
// Tunables — adjusted at integration time. Keep defaults conservative.
// ----------------------------------------------------------------------------
#define DECK_HEIGHT_MAX_MM          175      // Mechanical full-up position
#define DECK_HEIGHT_MIN_MM          0        // Mechanical full-down (home) position
#define DECK_HEIGHT_TOLERANCE_MM    3        // Reserved — ±3 mm "at target" band for future closed-loop work
#define MOTOR_PWM_MAX               8191     // 13-bit LEDC
#define MOTOR_PWM_RAISE_DEFAULT     5500     // ~67% duty (raise — against gravity)
#define MOTOR_PWM_LOWER_DEFAULT     4500     // Lighter duty descending (gravity assist)

// ---- Timer-based deck positioning ------------------------------------------
// Limit switches AND the position potentiometer are OMITTED in this build.
// Deck height is integrated from motor run-time: the deck covers the full
// DECK_HEIGHT_MAX_MM in DECK_RAISE_TIME_MS going up (against gravity) and in
// DECK_LOWER_TIME_MS coming down (gravity-assisted, hence faster). The estimate
// is pinned hard to 0 mm / DECK_HEIGHT_MAX_MM each time a full traverse
// completes, so error cannot accumulate from cycle to cycle.
//   CALIBRATION: bench-measure both times at the PWM duties above and update
//   these two values — this is what makes the HMI deck height read true.
//   ASSUMPTION: the deck is parked at the bottom (0 mm) at power-on.
#define DECK_RAISE_TIME_MS          6000     // 0 -> MAX at MOTOR_PWM_RAISE_DEFAULT  (MEASURE ON HARDWARE)
#define DECK_LOWER_TIME_MS          5000     // MAX -> 0 at MOTOR_PWM_LOWER_DEFAULT  (MEASURE ON HARDWARE)
#define MOTION_TIMEOUT_MARGIN_MS    2000     // Runaway guard: FAULT_STALL if a move runs this long past a full traverse

#define BARRIER_DOWN_ANGLE          0
#define BARRIER_UP_ANGLE            90
#define BARRIER_TIMEOUT_MS          3000     // Max time for a servo barrier sweep to reach target (full 90° sweep ≈ 2.25 s)
#define HOLD_TIMEOUT_MS             8000     // RESERVED — manual lowering only; no auto-lower is implemented
#define LASER_BEAM_SPACING_CM       3        // Gap between Beam A and B within each pair
// Simulated dynamic counterweight tunables
#define CW_TANK_CAPACITY_ML        150.0f   // Max tank capacity (ml)
#define CW_SIM_FILL_RATE_ML_PER_S  30.0f    // Simulated pump fill rate
#define CW_SIM_DRAIN_RATE_ML_PER_S 40.0f    // Simulated gravity drain rate
#define CW_SIM_TOLERANCE_ML        2.0f     // ±2 ml = "at target"

#define WATCHDOG_KICK_PERIOD_MS     500      // Reserved — current design has each task kick at its own tick rate, this constant is unused but kept in case a centralised kick scheduler is introduced later
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
