// sensors/ultrasonic.h
// Dual HC-SR04 array — beams A and B for direction inference.
// Owner: M3 Cindy
#pragma once
#include "../system_types.h"

void sensors_ultrasonic_init(void);
void sensors_ultrasonic_tick(void);
UltrasonicStatus_t sensors_ultrasonic_get(void);
