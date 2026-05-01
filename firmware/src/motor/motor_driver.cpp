// ============================================================================
// motor/motor_driver.cpp — BTS7960 H-bridge control + current sensing
// + position estimate from Hall encoder pulses on JGA25-370 motor.
// Owner: M2 Eugene (Mechanism)
//
// Pin mapping (see pin_config.h):
//   MOT_RPWM (forward/up)   — GPIO25  (LEDC ch0, 13-bit, 20 kHz)
//   MOT_LPWM (reverse/down) — GPIO26  (LEDC ch1, 13-bit, 20 kHz)
//   MOT_R_EN, MOT_L_EN      — tied high to 5 V on board (always enabled)
//   MOT_VPROPI (current)    — GPIO34  (ADC1_CH6, input-only, NO pull-up)
//   ENC_A                   — GPIO27  (Hall A — interrupt RISING)
//   LIMIT_TOP, LIMIT_BOTTOM — GPIO16/17 (active-low with INPUT_PULLUP)
//
// Counts/mm calibration is captured in CAL_COUNTS_PER_MM. Re-run the
// calibration sketch (member guide M2) on each new gearmotor batch.
// ============================================================================
#include "motor_driver.h"
#include "../pin_config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ---------------------------------------------------------------------------
// LEDC channel configuration
// ---------------------------------------------------------------------------
#define LEDC_CH_RPWM    0
#define LEDC_CH_LPWM    1
#define LEDC_RES_BITS   13
#define LEDC_FREQ_HZ    20000

// ---------------------------------------------------------------------------
// Calibration — measured by raising deck 100mm and counting pulses.
// PLACEHOLDER: M2 Eugene must re-measure on hardware and update.
// ---------------------------------------------------------------------------
#define CAL_COUNTS_PER_MM      42      // From bench measurement
#define ADC_TO_MA_NUMERATOR    1466L   // (3300 mV / 4095) * (1000 / VPROPI_R)
#define ADC_TO_MA_DENOMINATOR  4095L

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static volatile int32_t s_enc_count   = 0;
static int32_t          s_enc_zero    = 0;
static MotorDirection_t s_dir         = MOTOR_STOP;
static uint16_t         s_duty        = 0;
static uint32_t         s_last_move_ms = 0;
static int32_t          s_last_enc_for_stall = 0;

// ---------------------------------------------------------------------------
// ISR — Hall A pulses. Direction inferred from current command.
// ---------------------------------------------------------------------------
static void IRAM_ATTR enc_a_isr(void) {
    if (s_dir == MOTOR_UP)        s_enc_count++;
    else if (s_dir == MOTOR_DOWN) s_enc_count--;
    // Brake/coast/stop: don't count (idle drift ignored)
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void motor_driver_init(void) {
    // Limits
    pinMode(PIN_LIMIT_TOP,    INPUT_PULLUP);
    pinMode(PIN_LIMIT_BOTTOM, INPUT_PULLUP);

    // Encoder
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), enc_a_isr, RISING);

    // PWM channels
    ledcSetup(LEDC_CH_RPWM, LEDC_FREQ_HZ, LEDC_RES_BITS);
    ledcSetup(LEDC_CH_LPWM, LEDC_FREQ_HZ, LEDC_RES_BITS);
    ledcAttachPin(PIN_MOT_RPWM, LEDC_CH_RPWM);
    ledcAttachPin(PIN_MOT_LPWM, LEDC_CH_LPWM);
    ledcWrite(LEDC_CH_RPWM, 0);
    ledcWrite(LEDC_CH_LPWM, 0);

    // ADC for current sense
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_MOT_VPROPI, ADC_11db);   // 0..3.3 V

    Serial.println("[motor] init OK");
}

void motor_driver_apply(const MotorCommand_t& cmd) {
    s_dir  = cmd.direction;
    s_duty = cmd.pwm_duty;
    if (s_duty > MOTOR_PWM_MAX) s_duty = MOTOR_PWM_MAX;

    switch (s_dir) {
    case MOTOR_UP:
        ledcWrite(LEDC_CH_LPWM, 0);
        ledcWrite(LEDC_CH_RPWM, s_duty);
        break;
    case MOTOR_DOWN:
        ledcWrite(LEDC_CH_RPWM, 0);
        ledcWrite(LEDC_CH_LPWM, s_duty);
        break;
    case MOTOR_BRAKE:
        // Active brake: drive both highs high (= shorting low side
        // through high-side body diodes). On BTS7960 this is achieved
        // by setting both PWM lines to 100% — motor sees Vmot+ on both
        // terminals, which dynamically brakes.
        ledcWrite(LEDC_CH_RPWM, MOTOR_PWM_MAX);
        ledcWrite(LEDC_CH_LPWM, MOTOR_PWM_MAX);
        break;
    case MOTOR_COAST:
    case MOTOR_STOP:
    default:
        ledcWrite(LEDC_CH_RPWM, 0);
        ledcWrite(LEDC_CH_LPWM, 0);
        break;
    }
    s_last_move_ms = millis();
}

void motor_driver_tick(void) {
    // Read current
    uint32_t adc_sum = 0;
    for (int i = 0; i < 4; i++) adc_sum += analogRead(PIN_MOT_VPROPI);
    int32_t adc_avg = adc_sum >> 2;
    int32_t mA = (adc_avg * ADC_TO_MA_NUMERATOR) / ADC_TO_MA_DENOMINATOR;

    // Read limits (active low)
    bool top_hit = (digitalRead(PIN_LIMIT_TOP)    == LOW);
    bool bot_hit = (digitalRead(PIN_LIMIT_BOTTOM) == LOW);

    // Position from encoder
    noInterrupts();
    int32_t enc_now = s_enc_count;
    interrupts();
    int16_t pos_mm = (int16_t)((enc_now - s_enc_zero) / CAL_COUNTS_PER_MM);

    // Auto-zero at bottom limit
    if (bot_hit) { s_enc_zero = enc_now; pos_mm = 0; }

    // Stall detection
    bool stalled = false;
    if (s_dir == MOTOR_UP || s_dir == MOTOR_DOWN) {
        if (abs(enc_now - s_last_enc_for_stall) < 5) {
            if ((millis() - s_last_move_ms) > MOTOR_STALL_TIMEOUT_MS) {
                stalled = true;
            }
        } else {
            s_last_enc_for_stall = enc_now;
            s_last_move_ms = millis();
        }
    } else {
        s_last_enc_for_stall = enc_now;
    }

    // Push to shared status
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.deck_position_mm = pos_mm;
        g_status.motor_current_ma = (int16_t)mA;
        g_status.motor_pwm_duty   = s_duty;
        g_status.top_limit_hit    = top_hit;
        g_status.bottom_limit_hit = bot_hit;
        if (mA > MOTOR_OVERCURRENT_MA)  SET_FAULT(g_status.fault_flags, FAULT_OVERCURRENT);
        if (stalled)                    SET_FAULT(g_status.fault_flags, FAULT_STALL);
        if (top_hit && bot_hit)         SET_FAULT(g_status.fault_flags, FAULT_LIMIT_BOTH);
        xSemaphoreGive(g_status_mutex);
    }

    // Emit limit-hit events to FSM
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

int16_t motor_driver_position_mm(void) {
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        int16_t v = g_status.deck_position_mm;
        xSemaphoreGive(g_status_mutex);
        return v;
    }
    return 0;
}

int16_t motor_driver_current_ma(void) {
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        int16_t v = g_status.motor_current_ma;
        xSemaphoreGive(g_status_mutex);
        return v;
    }
    return 0;
}
