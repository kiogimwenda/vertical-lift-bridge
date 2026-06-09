// ============================================================================
// motor/motor_driver.cpp — L298N dual H-bridge control
// + TIMER-BASED deck-position estimate (no potentiometer, no limit switches).
// Owner: M2 Eugene (Mechanism)
//
// Pin mapping (see pin_config.h):
//   MOT_IN1 (forward/up)    — GPIO25  (LEDC ch0, 13-bit, 4 kHz)
//   MOT_IN2 (reverse/down)  — GPIO26  (LEDC ch1, 13-bit, 4 kHz)
//   ENA                     — tied HIGH on the L298N module
//                             (PWM rides on IN1/IN2 directly)
//
// Positioning model
// -----------------
// The position potentiometer and the limit switches are OMITTED in this
// build. Deck height is integrated from motor run-time: the firmware knows
// the deck travels the full DECK_HEIGHT_MAX_MM in DECK_RAISE_TIME_MS going up
// and DECK_LOWER_TIME_MS coming down (gravity-assisted, so faster). Each tick
// adds/subtracts the distance covered since the previous tick.
//
// Drift control: the estimate is pinned HARD to the mechanical end-stops
// (0 mm / DECK_HEIGHT_MAX_MM) the moment a traverse completes, and a virtual
// EVT_TOP_LIMIT_HIT / EVT_BOTTOM_LIMIT_HIT is emitted there. Because every
// full raise ends pinned at MAX and every full lower ends pinned at 0, the
// estimate re-synchronises twice per cycle and error cannot accumulate.
// ASSUMPTION: the deck is parked at the bottom (0 mm) at power-on.
//
// Runaway guard: with no feedback we cannot detect a true mechanical stall,
// but we DO bound how long the motor may be commanded to move. If a move runs
// longer than a full traverse + MOTION_TIMEOUT_MARGIN_MS without reaching its
// end-stop, FAULT_STALL is raised and the FSM trips to FAULT (relay cut).
//
// Current sensing: the L298N module exposes no MCU-facing current-sense
// output, so motor_current_ma stays 0 and FAULT_OVERCURRENT is never raised —
// the L298N's on-chip thermal shutdown is the hardware safety net.
// ============================================================================
#include "motor_driver.h"
#include "../pin_config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ---------------------------------------------------------------------------
// LEDC channel configuration
// ---------------------------------------------------------------------------
#define LEDC_CH_IN1     0          // motor IN1 (forward/up) PWM
#define LEDC_CH_IN2     1          // motor IN2 (reverse/down) PWM
#define LEDC_RES_BITS   LEDC_MOTOR_RES_BITS // 0..8191 duty (matches MOTOR_PWM_MAX)
#define LEDC_FREQ_HZ    LEDC_MOTOR_FREQ_HZ  // 4000 Hz, satisfying 80MHz APB limit

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static MotorDirection_t s_dir          = MOTOR_STOP;
static uint16_t         s_duty         = 0;

// Timer-based position estimate (millimetres, kept in float for sub-mm steps).
static float            s_pos_mm       = 0.0f;   // assume parked at bottom on boot
static uint32_t         s_last_tick_ms = 0;      // for dt integration
static uint32_t         s_move_start_ms= 0;      // when the current UP/DOWN move began
static bool             s_moving       = false;  // true while dir is UP or DOWN

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void motor_driver_init(void) {
    // PWM channels (ENA tied high on the L298N module; speed/direction via
    // PWM on IN1/IN2).
    ledcSetup(LEDC_CH_IN1, LEDC_FREQ_HZ, LEDC_RES_BITS);
    ledcSetup(LEDC_CH_IN2, LEDC_FREQ_HZ, LEDC_RES_BITS);
    ledcAttachPin(PIN_MOT_IN1, LEDC_CH_IN1);
    ledcAttachPin(PIN_MOT_IN2, LEDC_CH_IN2);
    ledcWrite(LEDC_CH_IN1, 0);
    ledcWrite(LEDC_CH_IN2, 0);

    // Deck assumed parked at the mechanical bottom at power-on (see header).
    s_pos_mm        = 0.0f;
    s_last_tick_ms  = millis();
    s_move_start_ms = s_last_tick_ms;
    s_moving        = false;

    Serial.println("[motor] init OK (L298N module, timer-based positioning, no pot/limit)");
}

void motor_driver_apply(const MotorCommand_t& cmd) {
    s_dir  = cmd.direction;
    s_duty = cmd.pwm_duty;
    if (s_duty > MOTOR_PWM_MAX) s_duty = MOTOR_PWM_MAX;

    switch (s_dir) {
    case MOTOR_UP:
        ledcWrite(LEDC_CH_IN2, 0);
        ledcWrite(LEDC_CH_IN1, s_duty);
        break;
    case MOTOR_DOWN:
        ledcWrite(LEDC_CH_IN1, 0);
        ledcWrite(LEDC_CH_IN2, s_duty);
        break;
    case MOTOR_BRAKE:
        // Dynamic short-brake on the L298N: drive both inputs HIGH so both
        // motor terminals sit at the same rail; back-EMF is shorted through
        // the high-side switches and the rotor is braked. s_duty stays at the
        // commanded value (0), so the HMI reports 0 % *propulsive* duty while
        // braking — truthful, since no drive power is being applied to move the
        // deck.
        ledcWrite(LEDC_CH_IN1, MOTOR_PWM_MAX);
        ledcWrite(LEDC_CH_IN2, MOTOR_PWM_MAX);
        break;
    case MOTOR_COAST:
    case MOTOR_STOP:
    default:
        ledcWrite(LEDC_CH_IN1, 0);
        ledcWrite(LEDC_CH_IN2, 0);
        break;
    }

    // (Re)start the runaway timer only on the rising edge into a moving state
    // so that re-issuing the same direction does not keep resetting it.
    bool now_moving = (s_dir == MOTOR_UP || s_dir == MOTOR_DOWN);
    if (now_moving && !s_moving) {
        s_move_start_ms = millis();
    }
    s_moving = now_moving;
}

void motor_driver_tick(void) {
    // --- Integrate position from elapsed time --------------------------------
    uint32_t now = millis();
    uint32_t dt  = now - s_last_tick_ms;
    s_last_tick_ms = now;
    if (dt > 200) dt = 200;          // clamp first-tick / scheduling lag

    bool top_hit = false;
    bool bot_hit = false;

    if (s_dir == MOTOR_UP) {
        // mm covered this tick at the calibrated raise rate
        s_pos_mm += ((float)DECK_HEIGHT_MAX_MM / (float)DECK_RAISE_TIME_MS) * (float)dt;
        if (s_pos_mm >= (float)DECK_HEIGHT_MAX_MM) {
            s_pos_mm = (float)DECK_HEIGHT_MAX_MM;   // pin to top end-stop
            top_hit  = true;
        }
    } else if (s_dir == MOTOR_DOWN) {
        s_pos_mm -= ((float)DECK_HEIGHT_MAX_MM / (float)DECK_LOWER_TIME_MS) * (float)dt;
        if (s_pos_mm <= 0.0f) {
            s_pos_mm = 0.0f;                         // pin to bottom end-stop
            bot_hit  = true;
        }
    }

    int16_t pos_mm = (int16_t)(s_pos_mm + 0.5f);

    // --- Runaway guard -------------------------------------------------------
    // No feedback means we can't see a true stall; instead we cap how long a
    // move may run. A full traverse should never take longer than its
    // calibrated time, so anything past that + margin is a jam / runaway.
    bool runaway = false;
    if (s_moving) {
        uint32_t budget = (s_dir == MOTOR_UP ? DECK_RAISE_TIME_MS : DECK_LOWER_TIME_MS)
                        + MOTION_TIMEOUT_MARGIN_MS;
        if ((now - s_move_start_ms) > budget) runaway = true;
    }

    // --- Publish to shared status -------------------------------------------
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.deck_position_mm = pos_mm;
        g_status.motor_current_ma = 0;            // L298N: no MCU-facing IS pin
        g_status.motor_pwm_duty   = s_duty;
        g_status.top_limit_hit    = (pos_mm >= DECK_HEIGHT_MAX_MM);
        g_status.bottom_limit_hit = (pos_mm <= 0);
        if (runaway) SET_FAULT(g_status.fault_flags, FAULT_STALL);
        xSemaphoreGive(g_status_mutex);
    }

    // --- Emit virtual end-stop events to the FSM (edge-triggered) ------------
    static bool s_top_prev = false;
    static bool s_bot_prev = false;
    if (top_hit && !s_top_prev) {
        SystemEvent_t e = EVT_TOP_LIMIT_HIT;
        xQueueSend(g_event_queue, &e, 0);
    }
    if (bot_hit && !s_bot_prev) {
        SystemEvent_t e = EVT_BOTTOM_LIMIT_HIT;
        xQueueSend(g_event_queue, &e, 0);
    }
    s_top_prev = top_hit;
    s_bot_prev = bot_hit;
}
