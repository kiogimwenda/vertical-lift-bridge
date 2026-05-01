// hmi/input.h
// Owner: M4 Abigael
// Operator inputs: front-panel buttons via single ADC (resistor ladder)
// + e-stop (handled in safety/) + touch (handled by LVGL's indev driver).
#pragma once

void input_init(void);
void input_tick(void);    // 50 Hz
