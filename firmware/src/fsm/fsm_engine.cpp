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

    case STATE_ROAD_CLEARING: {
        // Need both barriers closed AND counterweights balanced before raising
        static bool s_cw_ok = false;
        if (evt == EVT_CW_READY) s_cw_ok = true;
        if (evt == EVT_BARRIER_CLOSED || evt == EVT_CW_READY || evt == EVT_TICK_100MS) {
            if (fsm_guard_road_clear() && s_cw_ok) {
                s_cw_ok = false;  // Reset for next cycle
                enter_state(STATE_RAISING);
            }
        }
    } break;

    case STATE_RAISING:
        if (evt == EVT_TOP_LIMIT_HIT) {
            enter_state(STATE_RAISED_HOLD);
        }
        break;

    case STATE_RAISED_HOLD:
        if (evt == EVT_HOLD_TIMEOUT || evt == EVT_OPERATOR_LOWER) {
            enter_state(STATE_LOWERING);
        }
        break;

    case STATE_LOWERING:
        if (evt == EVT_BOTTOM_LIMIT_HIT) {
            enter_state(STATE_ROAD_OPENING);
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
