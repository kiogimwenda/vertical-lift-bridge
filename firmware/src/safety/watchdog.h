// safety/watchdog.h
// Owner: M5 Ian
// Software watchdog: each major task must kick within WATCHDOG_MAX_INTERVAL_MS
// or fault is raised + relay opened.
#pragma once
#include <stdint.h>

void safety_watchdog_init(void);

void safety_watchdog_kick_main(void);
void safety_watchdog_kick_fsm(void);
void safety_watchdog_kick_motor(void);
void safety_watchdog_kick_sensors(void);
void safety_watchdog_kick_vision(void);

void safety_watchdog_check_all(void);   // Called from task_safety
