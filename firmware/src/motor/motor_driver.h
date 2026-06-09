// motor/motor_driver.h
// Owner: M2 Eugene (Mechanism) — implementation may be tuned to physical motor
#pragma once
#include "../system_types.h"

void motor_driver_init(void);
void motor_driver_apply(const MotorCommand_t& cmd);
void motor_driver_tick(void);                 // Called from task_motor
// Position and current are published to g_status by motor_driver_tick(); all
// consumers (HMI, counterweight) read them from there, so no public accessors
// are exposed here.
