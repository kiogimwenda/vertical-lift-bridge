// ============================================================================
// traffic/traffic_lights.cpp — Drives 74HC595 chain.
// Bit map (Q0..Q2 of the 595):
//   bit0 ROAD_R, bit1 ROAD_Y, bit2 ROAD_G
// Marine traffic lights are NOT implemented in this build — only the road
// stack (Q0..Q2) is driven; Q3..Q7 are held low.
// Owner: M4 Abigael
// ============================================================================
#include "traffic_lights.h"
#include "../pin_config.h"
#include "../system_types.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static TrafficLightState_t s_road   = TL_OFF;
static uint8_t              s_blink_phase = 0;
static uint8_t              s_last_bits = 0xFF; // force print on boot

static void shift_out(uint8_t byte) {
    // Force pins to GPIO mode
    pinMode(PIN_595_DATA, OUTPUT);
    pinMode(PIN_595_CLOCK, OUTPUT);
    pinMode(PIN_595_LATCH, OUTPUT);

    digitalWrite(PIN_595_LATCH, LOW);
    
    // ULTRA-SLOW BIT BANG (1ms per transition)
    // Immune to breadboard capacitance and cross-talk
    for (int i = 7; i >= 0; i--) {
        digitalWrite(PIN_595_CLOCK, LOW);
        delay(1);
        
        if (byte & (1 << i)) {
            digitalWrite(PIN_595_DATA, HIGH);
        } else {
            digitalWrite(PIN_595_DATA, LOW);
        }
        delay(1);
        
        digitalWrite(PIN_595_CLOCK, HIGH);
        delay(1);
    }

    digitalWrite(PIN_595_CLOCK, LOW);
    delay(1);
    digitalWrite(PIN_595_LATCH, HIGH);
    delay(1);
}

static uint8_t state_to_bits(TrafficLightState_t s, bool blink_on) {
    uint8_t r = 0, y = 0, g = 0;
    switch (s) {
    case TL_GREEN:        g = 1; break;
    case TL_AMBER:        y = 1; break;
    case TL_RED:          r = 1; break;
    case TL_AMBER_BLINK:  y = blink_on ? 1 : 0; break;
    case TL_RED_BLINK:    r = blink_on ? 1 : 0; break;
    default: break;
    }
    // Q0=Red, Q1=Yellow, Q2=Green
    return (r << 0) | (y << 1) | (g << 2);
}

void traffic_lights_init(void) {
    if (PIN_595_OE >= 0) {
        pinMode(PIN_595_OE, OUTPUT);
        digitalWrite(PIN_595_OE, LOW);
    }
    pinMode(PIN_595_LATCH, OUTPUT);
    digitalWrite(PIN_595_LATCH, HIGH);    // idle high — released
    pinMode(PIN_595_DATA,  OUTPUT);
    pinMode(PIN_595_CLOCK, OUTPUT);
    digitalWrite(PIN_595_DATA, LOW);
    digitalWrite(PIN_595_CLOCK, LOW);
    shift_out(0);                          // clear all LEDs at boot
    Serial.println("[lights] init OK");
}

void traffic_lights_set_road(TrafficLightState_t s)   { s_road = s; }

void traffic_lights_tick(void) {
    s_blink_phase ^= 1;
    bool on = (s_blink_phase != 0);
    // Road stack on Q0..Q2 of the 74HC595 byte (marine stack not implemented).
    uint8_t bits = state_to_bits(s_road, on);

    // Re-assert the 74HC595 periodically even when the commanded value has not
    // changed. We otherwise shift only on change, so a single corrupted latch —
    // e.g. a 3.3 V rail dip when the safety relay energises, or breadboard
    // noise on the bit-bang lines — would leave the LEDs stuck wrong until the
    // next state change. Refreshing every ~0.5 s makes the output self-healing.
    static uint8_t s_refresh_div = 0;
    bool refresh_due = (++s_refresh_div >= 5);   // tick is ~10 Hz -> ~2 Hz refresh
    if (refresh_due) s_refresh_div = 0;

    if (bits != s_last_bits || refresh_due) {
        if (bits != s_last_bits) {
            Serial.printf("[lights] Shift register output updated to: 0x%02X\n", bits);
        }
        shift_out(bits);
        s_last_bits = bits;

        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            g_status.lights_road   = bits & 0x07;
            xSemaphoreGive(g_status_mutex);
        }
    }
}