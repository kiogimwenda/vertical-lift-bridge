// traffic/traffic_lights.h
// Owner: M4 Abigael
// 74HC595 chain drives 6 LEDs (R/Y/G ×2 stacks: road & marine).
#pragma once
#include <stdint.h>

typedef enum : uint8_t {
    TL_OFF = 0,
    TL_GREEN,
    TL_AMBER,
    TL_RED,
    TL_AMBER_BLINK,
    TL_RED_BLINK
} TrafficLightState_t;

void traffic_lights_init(void);
void traffic_lights_tick(void);                       // Called at 10 Hz for blink
void traffic_lights_set_road  (TrafficLightState_t s);
void traffic_lights_set_marine(TrafficLightState_t s);
