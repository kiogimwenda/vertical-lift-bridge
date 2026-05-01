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

static uint32_t s_off_time_ms = 0;
static uint8_t  s_pattern     = 0;     // 0 idle, 1 fault, 2 estop

void buzzer_init(void) {
    ledcSetup(LEDC_CH_BUZZER, 2700, LEDC_RES_BUZ);
    ledcAttachPin(PIN_BUZZER, LEDC_CH_BUZZER);
    ledcWrite(LEDC_CH_BUZZER, 0);
    Serial.println("[buz] init OK");
}

void buzzer_off(void) { ledcWrite(LEDC_CH_BUZZER, 0); s_pattern = 0; }

void buzzer_tone(uint16_t freq_hz, uint16_t ms) {
    ledcWriteTone(LEDC_CH_BUZZER, freq_hz);
    ledcWrite(LEDC_CH_BUZZER, BUZ_DUTY_HALF);
    s_off_time_ms = millis() + ms;
}

void buzzer_chirp(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        ledcWriteTone(LEDC_CH_BUZZER, 2700);
        ledcWrite(LEDC_CH_BUZZER, BUZ_DUTY_HALF);
        delay(80);
        ledcWrite(LEDC_CH_BUZZER, 0);
        delay(80);
    }
}

void buzzer_pattern_fault(void) { s_pattern = 1; }
void buzzer_pattern_estop(void) { s_pattern = 2; }

void buzzer_tick(void) {
    static uint32_t s_last = 0;
    uint32_t now = millis();
    if (s_pattern == 0) {
        if (s_off_time_ms != 0 && now >= s_off_time_ms) {
            ledcWrite(LEDC_CH_BUZZER, 0);
            s_off_time_ms = 0;
        }
        return;
    }
    if (now - s_last < 250) return;
    s_last = now;
    static bool on = false;
    on = !on;
    if (s_pattern == 1) {
        ledcWriteTone(LEDC_CH_BUZZER, on ? 2200 : 0);
        if (on) ledcWrite(LEDC_CH_BUZZER, BUZ_DUTY_HALF);
    } else if (s_pattern == 2) {
        ledcWriteTone(LEDC_CH_BUZZER, on ? 3500 : 1800);
        ledcWrite(LEDC_CH_BUZZER, BUZ_DUTY_HALF);
    }
}
