// hmi/input.h
// Owner: M4 Abigael
//
// Operator inputs in v2.1:
//   • Touchscreen buttons via the LVGL XPT2046 indev driver (display.cpp owns this)
//   • E-stop mushroom button (handled in safety/interlocks.cpp)
//
// The front-panel resistor-ladder ADC scheme was removed — it shared GPIO 34
// with the BTS7960 IS pin and could not be electrically multiplexed.
// input_init() and input_tick() are kept as no-op stubs for ABI/build stability;
// they are not called from any task in main.cpp.
#pragma once

void input_init(void);    // No-op stub — touchscreen is the input device
void input_tick(void);    // No-op stub
