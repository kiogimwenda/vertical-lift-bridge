// fsm/fsm_guards.h
#pragma once
#include <stdbool.h>

bool fsm_guard_init_complete(void);
bool fsm_guard_road_clear(void);
bool fsm_guard_safe_to_lower(void);
bool fsm_guard_road_opened(void);
bool fsm_guard_estop_clearable(void);
