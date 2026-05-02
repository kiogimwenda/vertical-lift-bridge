// ============================================================================
// hmi/input.cpp — Front-panel input.
// Owner: M4 Abigael
//
// The original 5-button R/2R resistor-ladder scheme on PIN_BTN_LADDER was
// removed in v2.1. That ADC pin (GPIO 34) is owned by the BTS7960 IS pin
// and the rail-sense divider tap; both present low source impedance at all
// times, which would swamp any ladder voltage divider connected to the same
// pin. Software multiplexing by FSM state cannot resolve the impedance
// collision (see docs/known_limitations.md).
//
// All operator input now goes through the LVGL XPT2046 touch driver in
// display.cpp. These stubs remain so existing includes don't break.
// ============================================================================
#include "input.h"
#include <Arduino.h>

void input_init(void) {
    Serial.println("[input] disabled — touchscreen is the operator input device");
}

void input_tick(void) {
    // intentionally empty
}
