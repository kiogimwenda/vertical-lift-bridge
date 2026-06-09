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

// task_safety runs the software watchdog checker, so the software layer cannot
// catch a hang in task_safety itself. These let task_safety subscribe to the
// hardware TWDT and feed it each loop, so a stall there triggers a hard reset.
void safety_watchdog_subscribe_task(void);   // Call once from the calling task
void safety_watchdog_feed_hw(void);          // Call every loop of that task

void safety_watchdog_check_all(void);   // Called from task_safety
