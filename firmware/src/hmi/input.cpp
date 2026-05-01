// ============================================================================
// hmi/input.cpp — Front-panel buttons via resistor ladder on shared ADC.
// Owner: M4 Abigael
//
// Five buttons share PIN_BTN_LADDER through R/2R ladder; each press creates
// a distinct ADC band. Debounced 50 ms. Maps to HmiCmd_t.
//
// TODO (M4): If you change the ladder resistor values, update the bands.
// ============================================================================
#include "input.h"
#include "display.h"
#include "../pin_config.h"
#include <Arduino.h>

struct Band { int lo; int hi; HmiCmd_t cmd; };

// ADC counts (12-bit). Calibrated for 3.3V supply, 1k-1k-1k-1k-1k ladder.
static const Band kBands[] = {
    {   80,  500,  HMI_CMD_RAISE        },   // ~0.3 V
    {  600, 1100,  HMI_CMD_LOWER        },   // ~0.7 V
    { 1200, 1900,  HMI_CMD_HOLD         },   // ~1.3 V
    { 2000, 2700,  HMI_CMD_NEXT_SCREEN  },   // ~1.9 V
    { 2800, 3500,  HMI_CMD_CLEAR_FAULT  },   // ~2.5 V
    // 3500..4095 = no button pressed
};

static HmiCmd_t s_last_cmd  = HMI_CMD_NONE;
static uint32_t s_last_ms   = 0;
static const uint32_t DEBOUNCE_MS = 50;

void input_init(void) {
    pinMode(PIN_BTN_LADDER, INPUT);
    analogSetPinAttenuation(PIN_BTN_LADDER, ADC_11db);
    Serial.println("[input] init OK");
}

void input_tick(void) {
    int adc = analogRead(PIN_BTN_LADDER);
    HmiCmd_t cur = HMI_CMD_NONE;
    for (auto& b : kBands) {
        if (adc >= b.lo && adc <= b.hi) { cur = b.cmd; break; }
    }
    uint32_t now = millis();
    if (cur != HMI_CMD_NONE && cur != s_last_cmd && (now - s_last_ms) > DEBOUNCE_MS) {
        hmi_cmd_post(cur);
        s_last_cmd = cur;
        s_last_ms  = now;
    } else if (cur == HMI_CMD_NONE) {
        s_last_cmd = HMI_CMD_NONE;
    }
}
