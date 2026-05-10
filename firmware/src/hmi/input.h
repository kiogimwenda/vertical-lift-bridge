// hmi/input.h
// Owner: M4 Abigael
//
// Operator inputs in v2.2:
//   • Touchscreen buttons via the LVGL XPT2046 indev driver (display.cpp owns this)
//   • 5-button resistor-ladder front panel on PIN_BTN_LADDER (GPIO 34, ADC1_CH6)
//   • E-stop mushroom button (handled in safety/interlocks.cpp)
//
// History: v2.1 stubbed input_init/input_tick to no-ops because PIN_BTN_LADDER
// shared GPIO 34 with the BTS7960 IS pin (impedance collision, see
// docs/known_limitations.md L1). The v2.2 motor-driver migration to the
// L293D module removed that conflict, so the resistor-ladder scheme is
// restored alongside the touchscreen as a defence-in-depth input path.
#pragma once

void input_init(void);    // Configure ADC pin, initial scan
void input_tick(void);    // Call from task_safety at ~20 Hz
