// hmi/input.h
// Owner: M4 Abigael
//
// Operator inputs:
//   • Touchscreen buttons via the LVGL XPT2046 indev driver (display.cpp owns this)
//   • 5-button resistor-ladder front panel on PIN_BTN_LADDER (GPIO 34, ADC1_CH6)
//   • E-stop mushroom button (handled in safety/interlocks.cpp)
//
// GPIO 34 is dedicated to the resistor ladder (the L298N motor driver needs no
// current-sense ADC), so the panel runs alongside the touchscreen as a
// defence-in-depth input path.
#pragma once

void input_init(void);    // Configure ADC pin, initial scan
void input_tick(void);    // Call from task_safety at ~20 Hz
