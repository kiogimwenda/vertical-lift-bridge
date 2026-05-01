// safety/fault_register.h
// Owner: M5 Ian
// Central evaluation of fault flags; raises EVT_FAULT_RAISED to FSM.
#pragma once
#include <stdint.h>

void fault_register_init(void);
void fault_register_evaluate(void);     // Called from task_safety
uint32_t fault_register_snapshot(void);
void fault_register_clear_all(void);    // Operator-initiated only
const char* fault_register_first_name(uint32_t flags);
