// ============================================================================
// safety/watchdog.cpp — Software watchdog for each FreeRTOS task.
// Owner: M5 Ian
//
// Each task records its last-kick timestamp. task_safety polls all timestamps
// at 20 Hz; if any exceeds WATCHDOG_MAX_INTERVAL_MS, FAULT_WATCHDOG is set
// and the FSM is forced to STATE_ESTOP via interlocks_force_safe().
//
// Hardware backstop: the ESP32 Task Watchdog Timer (TWDT) is also enabled
// at 5 s — catches catastrophic hangs that even task_safety can't detect.
// ============================================================================
#include "watchdog.h"
#include "../system_types.h"
#include "../safety/interlocks.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static volatile uint32_t s_kick_main    = 0;
static volatile uint32_t s_kick_fsm     = 0;
static volatile uint32_t s_kick_motor   = 0;
static volatile uint32_t s_kick_sensors = 0;
static volatile uint32_t s_kick_vision  = 0;

void safety_watchdog_init(void) {
    // Hardware TWDT — 5 s, panic on timeout.
    esp_task_wdt_init(5, true);
    esp_task_wdt_add(NULL);                // setup() task
    uint32_t now = millis();
    s_kick_main = s_kick_fsm = s_kick_motor = s_kick_sensors = s_kick_vision = now;
    Serial.println("[wdt] init OK (5s hw, 1.5s sw)");
}

void safety_watchdog_kick_main   (void) { s_kick_main    = millis(); esp_task_wdt_reset(); }
void safety_watchdog_kick_fsm    (void) { s_kick_fsm     = millis(); }
void safety_watchdog_kick_motor  (void) { s_kick_motor   = millis(); }
void safety_watchdog_kick_sensors(void) { s_kick_sensors = millis(); }
void safety_watchdog_kick_vision (void) { s_kick_vision  = millis(); }

void safety_watchdog_check_all(void) {
    uint32_t now = millis();
    bool hung = false;
    if ((now - s_kick_fsm)     > WATCHDOG_MAX_INTERVAL_MS) hung = true;
    if ((now - s_kick_motor)   > WATCHDOG_MAX_INTERVAL_MS) hung = true;
    if ((now - s_kick_sensors) > WATCHDOG_MAX_INTERVAL_MS) hung = true;
    // Vision can be slower (UART blocking) — give it 2x
    if ((now - s_kick_vision)  > WATCHDOG_MAX_INTERVAL_MS * 2) hung = true;

    if (hung) {
        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            SET_FAULT(g_status.fault_flags, FAULT_WATCHDOG);
            g_status.watchdog_last_kick_ms = now;
            xSemaphoreGive(g_status_mutex);
        }
        interlocks_force_safe();
        Serial.printf("[wdt] HUNG: fsm=%lu mot=%lu sens=%lu vis=%lu\n",
                      now - s_kick_fsm, now - s_kick_motor,
                      now - s_kick_sensors, now - s_kick_vision);
    }
}
