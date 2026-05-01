// ============================================================================
// traffic/traffic_lights.cpp — Drives 74HC595 chain.
// Bit map (Q0..Q5 of first 595):
//   bit0 ROAD_R, bit1 ROAD_Y, bit2 ROAD_G,
//   bit3 MARINE_R, bit4 MARINE_Y, bit5 MARINE_G
// Owner: M4 Abigael
// ============================================================================
#include "traffic_lights.h"
#include "../pin_config.h"
#include "../system_types.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static TrafficLightState_t s_road   = TL_OFF;
static TrafficLightState_t s_marine = TL_OFF;
static uint8_t              s_blink_phase = 0;

static void shift_out(uint8_t byte) {
    digitalWrite(PIN_595_LATCH, LOW);
    shiftOut(PIN_595_DATA, PIN_595_CLOCK, MSBFIRST, byte);
    digitalWrite(PIN_595_LATCH, HIGH);
}

static uint8_t state_to_bits(TrafficLightState_t s, uint8_t base_shift, bool blink_on) {
    uint8_t r = 0, y = 0, g = 0;
    switch (s) {
    case TL_GREEN:        g = 1; break;
    case TL_AMBER:        y = 1; break;
    case TL_RED:          r = 1; break;
    case TL_AMBER_BLINK:  y = blink_on ? 1 : 0; break;
    case TL_RED_BLINK:    r = blink_on ? 1 : 0; break;
    default: break;
    }
    return (r << base_shift) | (y << (base_shift + 1)) | (g << (base_shift + 2));
}

void traffic_lights_init(void) {
    pinMode(PIN_595_DATA,  OUTPUT);
    pinMode(PIN_595_CLOCK, OUTPUT);
    pinMode(PIN_595_LATCH, OUTPUT);
    pinMode(PIN_595_OE,    OUTPUT);
    digitalWrite(PIN_595_OE, LOW);  // Output enable
    shift_out(0);
    Serial.println("[lights] init OK");
}

void traffic_lights_set_road(TrafficLightState_t s)   { s_road = s; }
void traffic_lights_set_marine(TrafficLightState_t s) { s_marine = s; }

void traffic_lights_tick(void) {
    s_blink_phase ^= 1;
    bool on = (s_blink_phase != 0);
    uint8_t bits = state_to_bits(s_road, 0, on) | state_to_bits(s_marine, 3, on);
    shift_out(bits);

    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        g_status.lights_road   = bits & 0x07;
        g_status.lights_marine = (bits >> 3) & 0x07;
        xSemaphoreGive(g_status_mutex);
    }
}
