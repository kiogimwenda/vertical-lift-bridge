// motor/motor_driver.h
// Owner: M2 Eugene (Mechanism) — implementation may be tuned to physical motor
#pragma once
#include "../system_types.h"

void motor_driver_init(void);
void motor_driver_apply(const MotorCommand_t& cmd);
void motor_driver_tick(void);                 // Called from task_motor
int16_t motor_driver_position_mm(void);
int16_t motor_driver_current_ma(void);
