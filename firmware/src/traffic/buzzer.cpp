// ============================================================================
// traffic/buzzer.cpp — Active or passive piezo on LEDC ch5.
// Owner: M4 Abigael
//
// Pin: PIN_BUZZER (default GPIO1 / TX0). Buzzer is driven only AFTER boot
// (boot loader uses TX0). pin_config.h notes this constraint.
// ============================================================================
#include "buzzer.h"
#include "../pin_config.h"
#include <Arduino.h>

#define LEDC_CH_BUZZER  5
#define LEDC_RES_BUZ    10
#define BUZ_DUTY_HALF   ((1 << LEDC_RES_BUZ) / 2)

static uint8_t  s_pattern         = 0;   // 0 idle, 1 fault, 2 estop
// Non-blocking chirp state: a chirp is N beeps, each beep = an ON phase then an
// OFF phase, every CHIRP_PHASE_MS. Serviced incrementally by buzzer_tick() so
// the caller (an FSM entry action on task_fsm) never blocks on delay().
#define CHIRP_PHASE_MS  80
static uint8_t  s_chirp_phases    = 0;   // remaining on/off half-phases
static bool     s_chirp_on        = false;
static uint32_t s_chirp_next_ms   = 0;

void buzzer_init(void) {
    if (PIN_BUZZER != -1) {
        ledcSetup(LEDC_CH_BUZZER, 2700, LEDC_RES_BUZ);
        ledcAttachPin(PIN_BUZZER, LEDC_CH_BUZZER);
        ledcWrite(LEDC_CH_BUZZER, 0);
    }
    Serial.println("[buz] init OK");
}

void buzzer_off(void) {
    ledcWrite(LEDC_CH_BUZZER, 0);
    s_pattern      = 0;
    s_chirp_phases = 0;
    s_chirp_on     = false;
}

void buzzer_chirp(uint8_t count) {
    if (count == 0) return;
    // Two phases per beep (ON then OFF). buzzer_tick() drives them; the first
    // phase fires on the next tick.
    s_chirp_phases  = (uint8_t)(count * 2);
    s_chirp_on      = false;
    s_chirp_next_ms = 0;          // 0 = fire immediately on next tick
}

void buzzer_pattern_fault(void) { s_pattern = 1; }
void buzzer_pattern_estop(void) { s_pattern = 2; }

void buzzer_tick(void) {
    uint32_t now = millis();

    // Fault / E-stop alarm patterns take priority over chirps.
    if (s_pattern != 0) {
        static uint32_t s_last = 0;
        static bool on = false;
        if (now - s_last < 250) return;
        s_last = now;
        on = !on;
        if (s_pattern == 1) {
            ledcWriteTone(LEDC_CH_BUZZER, on ? 2200 : 0);
            if (on) ledcWrite(LEDC_CH_BUZZER, BUZ_DUTY_HALF);
        } else if (s_pattern == 2) {
            ledcWriteTone(LEDC_CH_BUZZER, on ? 3500 : 1800);
            ledcWrite(LEDC_CH_BUZZER, BUZ_DUTY_HALF);
        }
        return;
    }

    // Non-blocking chirp sequence.
    if (s_chirp_phases > 0 && now >= s_chirp_next_ms) {
        s_chirp_on = !s_chirp_on;
        if (s_chirp_on) {
            ledcWriteTone(LEDC_CH_BUZZER, 2700);
            ledcWrite(LEDC_CH_BUZZER, BUZ_DUTY_HALF);
        } else {
            ledcWrite(LEDC_CH_BUZZER, 0);
        }
        s_chirp_next_ms = now + CHIRP_PHASE_MS;
        s_chirp_phases--;
        if (s_chirp_phases == 0) ledcWrite(LEDC_CH_BUZZER, 0);   // ensure silent at end
    }
}
