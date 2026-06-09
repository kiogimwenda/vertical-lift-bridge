// traffic/buzzer.h
#pragma once
#include <stdint.h>

void buzzer_init(void);
void buzzer_off(void);
void buzzer_chirp(uint8_t count);   // Non-blocking: schedules `count` beeps serviced by buzzer_tick()
void buzzer_pattern_fault(void);
void buzzer_pattern_estop(void);
void buzzer_tick(void);             // Call at ~10 Hz; drives chirps + alarm patterns
