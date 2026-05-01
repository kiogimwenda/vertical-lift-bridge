// traffic/buzzer.h
#pragma once
#include <stdint.h>

void buzzer_init(void);
void buzzer_off(void);
void buzzer_chirp(uint8_t count);
void buzzer_tone(uint16_t freq_hz, uint16_t ms);
void buzzer_pattern_fault(void);
void buzzer_pattern_estop(void);
void buzzer_tick(void);
