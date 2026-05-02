// ============================================================================
// safety/fault_register.cpp — Central fault evaluation.
// Owner: M5 Ian
//
// Aggregates fault flags from g_status and emits EVT_FAULT_RAISED to the FSM
// the first time any flag becomes set (rising edge), and EVT_FAULT_CLEARED
// on the falling edge.
//
// NOTE on rail monitoring: 12 V / 5 V rail ADC reads were removed in v2.1
// because the only available ADC pins (GPIO 34/35) are already owned by the
// BTS7960 IS pin and the deck-position potentiometer, which present low
// source impedance at all times. A high-impedance voltage divider on the same
// pin would be swamped by the motor sense circuitry — software-only
// multiplexing cannot fix the analog conflict. Brownout protection now
// relies on:
//   • ESP32 internal brownout detector
//   • BTS7960 built-in undervoltage lockout
//   • LM2596 thermal/current limit
// rail_*_volts fields remain in SharedStatus_t but are set to -1.0f (sentinel
// meaning "not measured"). See docs/known_limitations.md.
// ============================================================================
#include "fault_register.h"
#include "../system_types.h"
#include "../pin_config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static uint32_t s_last_flags = 0;

void fault_register_init(void) {
    s_last_flags = 0;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        g_status.rail_12v_volts = -1.0f;   // sentinel: not measured
        g_status.rail_5v_volts  = -1.0f;
        g_status.rail_3v3_volts = -1.0f;
        xSemaphoreGive(g_status_mutex);
    }
    Serial.println("[fault] init OK (rail monitoring disabled — see known_limitations.md)");
}

void fault_register_evaluate(void) {
    uint32_t flags_now = 0;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        flags_now = g_status.fault_flags;
        xSemaphoreGive(g_status_mutex);
    }

    // Edge detection: rising any flag -> emit FAULT_RAISED.
    if (flags_now != 0 && s_last_flags == 0) {
        SystemEvent_t e = EVT_FAULT_RAISED;
        xQueueSend(g_event_queue, &e, 0);
        Serial.printf("[fault] raised: 0x%08lX (%s)\n",
                      (unsigned long)flags_now,
                      fault_register_first_name(flags_now));
    } else if (flags_now == 0 && s_last_flags != 0) {
        SystemEvent_t e = EVT_FAULT_CLEARED;
        xQueueSend(g_event_queue, &e, 0);
        Serial.println("[fault] cleared");
    }
    s_last_flags = flags_now;
}

uint32_t fault_register_snapshot(void) {
    uint32_t v = 0;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        v = g_status.fault_flags;
        xSemaphoreGive(g_status_mutex);
    }
    return v;
}

void fault_register_clear_all(void) {
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        g_status.fault_flags = 0;
        xSemaphoreGive(g_status_mutex);
    }
}

const char* fault_register_first_name(uint32_t flags) {
    if (flags & FAULT_OVERCURRENT)      return "overcurrent";
    if (flags & FAULT_STALL)            return "stall";
    if (flags & FAULT_LIMIT_BOTH)       return "both-limits";
    if (flags & FAULT_POS_OUT_OF_RANGE) return "pos-range";
    if (flags & FAULT_WATCHDOG)         return "watchdog";
    if (flags & FAULT_VISION_LINK_LOST) return "vision-lost";
    if (flags & FAULT_ULTRASONIC_FAIL)  return "us-fail";
    if (flags & FAULT_TFT_INIT_FAIL)    return "tft-init";
    if (flags & FAULT_TOUCH_INIT_FAIL)  return "touch-init";
    if (flags & FAULT_BARRIER_TIMEOUT)  return "barrier-to";
    if (flags & FAULT_RELAY_FEEDBACK)   return "relay-fb";
    if (flags & FAULT_UNDERVOLT_12V)    return "uv-12v(deprecated)";
    if (flags & FAULT_OVERTEMP)         return "overtemp";
    if (flags & FAULT_CONFIG_CRC)       return "cfg-crc";
    if (flags & FAULT_QUEUE_OVERFLOW)   return "q-ovf";
    if (flags & FAULT_TASK_HANG)        return "task-hang";
    return "none";
}
