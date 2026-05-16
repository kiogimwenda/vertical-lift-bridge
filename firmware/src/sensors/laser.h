// sensors/laser.h
// Laser Break-Beam array — beams A and B for direction inference.
// Owner: M3 Cindy
#pragma once
#include "../system_types.h"

void sensors_laser_init(void);
void sensors_laser_tick(void);
LaserStatus_t sensors_laser_get(void);
