// ============================================================================
// fsm/fsm_engine.cpp — Table-driven FSM for the Vertical Lift Bridge.
// Owner: George (M1)
// ============================================================================
#include "fsm_engine.h"
#include "fsm_guards.h"
#include "fsm_actions.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ---------------------------------------------------------------------------
// Module-private state
// ---------------------------------------------------------------------------
static SystemState_t s_state         = STATE_INIT;
static SystemState_t s_prev_state    = STATE_INIT;
static uint32_t      s_entered_ms    = 0;
// Hoisted out of the STATE_ROAD_CLEARING case so it can be reset on every
// fresh entry. Without that reset, a CW_READY received during one cycle
// could carry over an ESTOP/FAULT round-trip and short-circuit the next
// cycle's counterweight wait.
static bool          s_cw_ok         = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void enter_state(SystemState_t new_state) {
    if (new_state == s_state) return;
    Serial.printf("[fsm] %u -> %u\n", (unsigned)s_state, (unsigned)new_state);

    // Exit action of old state
    fsm_action_on_exit(s_state);

    s_prev_state = s_state;
    s_state      = new_state;
    s_entered_ms = millis();

    // Per-state-entry resets (avoid stale carry-over across FAULT / ESTOP
    // round-trips). If a state grows new transient flags, clear them here.
    if (new_state == STATE_ROAD_CLEARING) {
        s_cw_ok = false;
    }

    // Mirror to shared status
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        g_status.prev_state       = s_prev_state;
        g_status.state            = s_state;
        g_status.state_entered_ms = s_entered_ms;
        xSemaphoreGive(g_status_mutex);
    }

    // Entry action of new state
    fsm_action_on_entry(s_state);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void fsm_engine_init(void) {
    s_state      = STATE_INIT;
    s_prev_state = STATE_INIT;
    s_entered_ms = millis();
    fsm_action_on_entry(s_state);
    // After init, drop into IDLE if guards permit.
    if (fsm_guard_init_complete()) {
        enter_state(STATE_IDLE);
    }
}

SystemState_t fsm_engine_current(void) { return s_state; }
uint32_t      fsm_engine_state_age_ms(void) { return millis() - s_entered_ms; }

// ---------------------------------------------------------------------------
// Transition logic — switch-of-switches; readable enough for 9 states.
// Order: E-stop / fault have priority over normal events.
// ---------------------------------------------------------------------------
void fsm_engine_handle(SystemEvent_t evt) {
    // Universal high-priority transitions
    if (evt == EVT_ESTOP_PRESSED) {
        enter_state(STATE_ESTOP);
        return;
    }
    if (evt == EVT_FAULT_RAISED && s_state != STATE_ESTOP) {
        enter_state(STATE_FAULT);
        return;
    }

    switch (s_state) {

    case STATE_INIT:
        if (evt == EVT_TICK_100MS && fsm_guard_init_complete()) {
            enter_state(STATE_IDLE);
        }
        break;

    case STATE_IDLE:
        if (evt == EVT_VEHICLE_DETECTED && fsm_guard_can_clear_road()) {
            enter_state(STATE_ROAD_CLEARING);
        } else if (evt == EVT_OPERATOR_RAISE) {
            enter_state(STATE_ROAD_CLEARING);
        }
        break;

    case STATE_ROAD_CLEARING:
        // Need both barriers closed AND counterweights balanced before raising.
        // s_cw_ok is the file-static reset by enter_state() above.
        if (evt == EVT_CW_READY) s_cw_ok = true;
        if (evt == EVT_BARRIER_CLOSED || evt == EVT_CW_READY || evt == EVT_TICK_100MS) {
            // Check if mechanical requirements are met AND the deck is clear of cars
            if (fsm_guard_road_clear() && s_cw_ok) {
                enter_state(STATE_RAISING);   // s_cw_ok cleared on next ROAD_CLEARING entry
            }
        }
        break;

    case STATE_RAISING:
        if (evt == EVT_TOP_LIMIT_HIT) {
            enter_state(STATE_RAISED_HOLD);
        } else if (evt == EVT_OPERATOR_HOLD) {
            // Mid-travel freeze: brake the motor and present the same
            // recovery options as if we'd reached the top early. The
            // existing RAISED_HOLD auto-timeout then either lowers the
            // deck after HOLD_TIMEOUT_MS or yields to operator LOWER.
            enter_state(STATE_RAISED_HOLD);
        }
        break;

    case STATE_RAISED_HOLD:
        // EVT_HOLD_TIMEOUT has no separate producer task — it is fired
        // here from the periodic EVT_TICK_100MS when the state age
        // exceeds HOLD_TIMEOUT_MS. This keeps the autonomous cycle
        // self-driving without requiring an operator press.
        if (evt == EVT_OPERATOR_LOWER) {
            enter_state(STATE_LOWERING);
        } else if (evt == EVT_OPERATOR_RAISE) {
            enter_state(STATE_RAISING);
        } else if (evt == EVT_TICK_100MS &&
                   fsm_engine_state_age_ms() >= HOLD_TIMEOUT_MS) {
            enter_state(STATE_LOWERING);
        }
        break;

    case STATE_LOWERING:
        if (evt == EVT_BOTTOM_LIMIT_HIT) {
            enter_state(STATE_ROAD_OPENING);
        } else if (evt == EVT_OPERATOR_HOLD) {
            // Same mid-travel freeze treatment as RAISING.
            enter_state(STATE_RAISED_HOLD);
        }
        break;

    case STATE_ROAD_OPENING:
        if (evt == EVT_BARRIER_OPEN || evt == EVT_TICK_100MS) {
            if (fsm_guard_road_opened()) {
                enter_state(STATE_IDLE);
            }
        }
        break;

    case STATE_FAULT:
        if (evt == EVT_FAULT_CLEARED) {
            enter_state(STATE_IDLE);
        }
        break;

    case STATE_ESTOP:
        if (evt == EVT_ESTOP_RELEASED && fsm_guard_estop_clearable()) {
            enter_state(STATE_IDLE);
        }
        break;

    default:
        break;
    }
}
