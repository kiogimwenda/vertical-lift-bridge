// ============================================================================
// motor/motor_driver.cpp — L293D module H-bridge control
// + position estimate from a 10 kΩ deck-position potentiometer.
// Owner: M2 Eugene (Mechanism)
//
// Pin mapping (see pin_config.h):
//   MOT_IN1 (forward/up)    — GPIO25  (LEDC ch0, 13-bit, 20 kHz)
//   MOT_IN2 (reverse/down)  — GPIO26  (LEDC ch1, 13-bit, 20 kHz)
//   MOT_EN                  — tied high to +5 V on the L293D module
//                             (PWM rides on IN1/IN2 directly)
//   DECK_POSITION           — GPIO35  (ADC1_CH7, potentiometer wiper)
//   LIMIT_ANYHIT            — GPIO39  (diode-OR of all limit switches)
//
// Channel paralleling: the L293D's two internal channels are wired in
// parallel on the PCB (IN1↔IN3, IN2↔IN4, EN1↔EN2; OUT1↔OUT3, OUT2↔OUT4)
// to lift the 600 mA single-channel rating to 1.2 A continuous / 2.4 A
// peak — necessary headroom for the JGA25-370's ~600 mA nominal draw.
// The firmware sees only IN1/IN2; the parallelisation is a hardware-only
// change and requires no driver code adjustments.
//
// Note on motor current sensing: the L293D has no current-sense output.
// FAULT_OVERCURRENT is therefore not raised by firmware — the L293D's
// internal thermal-shutdown handles abuse, and the stall detector below
// covers the dominant failure mode.
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
#define LEDC_CH_IN1     0          // motor IN1 (forward/up) PWM
#define LEDC_CH_IN2     1          // motor IN2 (reverse/down) PWM
#define LEDC_RES_BITS   13         // 0..8191 duty (matches MOTOR_PWM_MAX)
#define LEDC_FREQ_HZ    20000      // above audible

// ---------------------------------------------------------------------------
// Calibration — ADC counts per millimetre of deck travel.
// Measured by the calibration sketch in member_guides/M2 §7. Auto-zero
// happens whenever the bottom limit switch fires; pos_mm is then derived
// as (adc_now - s_adc_zero) / CAL_COUNTS_PER_MM.
// PLACEHOLDER: M2 Eugene must re-measure on hardware and update.
// ---------------------------------------------------------------------------
#define CAL_COUNTS_PER_MM      14      // From bench measurement (M2 §7)
#define CAL_ADC_ZERO_DEFAULT   400     // Absolute baseline ADC reading at physical 0mm

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static MotorDirection_t s_dir          = MOTOR_STOP;
static uint16_t         s_duty         = 0;
static uint32_t         s_last_move_ms = 0;
// ADC reading captured at the last bottom-limit hit. Until the first
// bottom-limit event, we use 0 (assumes deck starts at the bottom).
static int32_t          s_adc_zero     = 0;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void motor_driver_init(void) {
    // Single diode-OR'd limit input — GPIO 39 has no internal pull-up,
    // an external 10 kΩ pull-up to +3V3 is provided on the PCB (see
    // ConnectorsSafety sub-sheet, R_pu_39).
    pinMode(PIN_LIMIT_ANYHIT, INPUT);

    // PWM channels
    ledcSetup(LEDC_CH_IN1, LEDC_FREQ_HZ, LEDC_RES_BITS);
    ledcSetup(LEDC_CH_IN2, LEDC_FREQ_HZ, LEDC_RES_BITS);
    ledcAttachPin(PIN_MOT_IN1, LEDC_CH_IN1);
    ledcAttachPin(PIN_MOT_IN2, LEDC_CH_IN2);
    ledcWrite(LEDC_CH_IN1, 0);
    ledcWrite(LEDC_CH_IN2, 0);

    // ADC for deck-position pot (motor current sense not present on L293D)
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_DECK_POSITION, ADC_11db); // 0..3.3 V

    // The 10k pot is an absolute encoder. Seeding `s_adc_zero` dynamically
    // at boot causes a critical failure in limit-switch discrimination if the
    // bridge is booted while raised. We MUST rely on a hardcoded absolute
    // calibration zero, which is then fine-tuned on the first bottom-limit hit.
    s_adc_zero = CAL_ADC_ZERO_DEFAULT;

    Serial.printf("[motor] init OK (L293D module, no current-sense, adc_zero=%ld)\n",
                  (long)s_adc_zero);
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
        // Dynamic short-brake on the L293D: drive both half-bridges to the
        // same rail (here: both inputs HIGH = both motor terminals at +Vmot).
        // The motor sees zero terminal-to-terminal voltage and the back-EMF
        // is shorted through the high-side switches, braking the rotor.
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
    s_last_move_ms = millis();
}

void motor_driver_tick(void) {
    // Position from the deck-position pot wiper (no Hall encoder fitted).
    uint32_t adc_sum = 0;
    for (int i = 0; i < 4; i++) adc_sum += analogRead(PIN_DECK_POSITION);
    int32_t adc_avg = adc_sum >> 2;
    // Convert ADC delta from the bottom-limit reference to millimetres.
    // pos32_raw is unclamped — used by the FAULT_POS_OUT_OF_RANGE check
    // below. pos_mm is clamped for downstream consumers.
    int32_t pos32_raw = (adc_avg - s_adc_zero) / CAL_COUNTS_PER_MM;
    int32_t pos32     = pos32_raw;
    if (pos32 < 0)                       pos32 = 0;
    if (pos32 > DECK_HEIGHT_MAX_MM + 20) pos32 = DECK_HEIGHT_MAX_MM + 20;
    int16_t pos_mm = (int16_t)pos32;

    // Single diode-OR'd limit signal on GPIO 39 (see known_limitations L2).
    // Discriminate top vs bottom by deck position: above midpoint -> top,
    // below midpoint -> bottom. This was the v2.1 design intent that was
    // not yet wired in firmware; v2.2 closes that gap.
    bool any_hit = (digitalRead(PIN_LIMIT_ANYHIT) == LOW);
    bool top_hit = false, bot_hit = false;
    if (any_hit) {
        if (pos_mm >= (DECK_HEIGHT_MAX_MM / 2)) top_hit = true;
        else                                    bot_hit = true;
    }

    // Auto-zero on bottom-limit hit: capture the ADC reading at this
    // moment as the "deck = 0 mm" reference so subsequent ticks start
    // measuring from the true mechanical bottom. This is what makes
    // CAL_COUNTS_PER_MM a per-mm calibration rather than a full-range
    // calibration, and it self-corrects pot drift over the project's life.
    if (bot_hit) {
        s_adc_zero = adc_avg;
        pos_mm     = 0;
    }

    // Stall detection — derived from the position estimate, not encoder.
    bool stalled = false;
    static int16_t s_last_pos_for_stall = 0;
    if (s_dir == MOTOR_UP || s_dir == MOTOR_DOWN) {
        if (abs((int)pos_mm - (int)s_last_pos_for_stall) < 1) {
            if ((millis() - s_last_move_ms) > MOTOR_STALL_TIMEOUT_MS) {
                stalled = true;
            }
        } else {
            s_last_pos_for_stall = pos_mm;
            s_last_move_ms       = millis();
        }
    } else {
        s_last_pos_for_stall = pos_mm;
    }

    // Out-of-range check: if pos_mm reads negative or unreasonably above
    // DECK_HEIGHT_MAX_MM, the pot wiper is probably disconnected or the
    // mechanical coupling has broken. Raise FAULT_POS_OUT_OF_RANGE so the
    // FSM stops trusting the position estimate.
    bool pos_bad = (pos32_raw < -10) || (pos32_raw > DECK_HEIGHT_MAX_MM + 15);

    // Push to shared status. motor_current_ma stays at 0 (no IS pin on
    // L293D); FAULT_OVERCURRENT is therefore never raised by firmware in
    // v2.2 — the L293D's internal thermal shutdown is the safety net.
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.deck_position_mm = pos_mm;
        g_status.motor_current_ma = 0;
        g_status.motor_pwm_duty   = s_duty;
        g_status.top_limit_hit    = top_hit;
        g_status.bottom_limit_hit = bot_hit;
        if (stalled)  SET_FAULT(g_status.fault_flags, FAULT_STALL);
        if (pos_bad)  SET_FAULT(g_status.fault_flags, FAULT_POS_OUT_OF_RANGE);
        // FAULT_LIMIT_BOTH is impossible with the diode-OR scheme (only
        // one logical bit can be true at a time after the discriminator
        // above) — left in the enum for v3 PCB which wires both limits.
        xSemaphoreGive(g_status_mutex);
    }

    // Emit limit-hit events to FSM (edge-triggered)
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
