// ============================================================================
// fsm/fsm_engine.h — Vertical Lift Bridge FSM
// Owner: George (M1)
//
// Simple table-driven FSM. Each state has an entry action, an exit action,
// and a transition table evaluated when an event arrives.
// ============================================================================
#pragma once
#include "../system_types.h"

void fsm_engine_init(void);
void fsm_engine_handle(SystemEvent_t evt);
SystemState_t fsm_engine_current(void);
uint32_t fsm_engine_state_age_ms(void);
