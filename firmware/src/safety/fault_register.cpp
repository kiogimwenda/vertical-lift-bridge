// ============================================================================
// safety/fault_register.cpp — Central fault evaluation.
// Owner: M5 Ian
//
// Reads voltage rails, evaluates fault_flags from g_status, and emits
// EVT_FAULT_RAISED to the FSM the first time any flag becomes set.
// ============================================================================
#include "fault_register.h"
#include "../system_types.h"
#include "../pin_config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Voltage divider: 12V rail through 33k/10k -> ADC sees ~2.79V max.
// adc_count * (3.3/4095) * (1+33/10) = volts
#define V12_DIVIDER_RATIO   4.3f      // (33+10)/10
#define V5_DIVIDER_RATIO    2.0f      // (10+10)/10

static uint32_t s_last_flags = 0;

void fault_register_init(void) {
    pinMode(PIN_V12_SENSE, INPUT);
    pinMode(PIN_V5_SENSE,  INPUT);
    analogSetPinAttenuation(PIN_V12_SENSE, ADC_11db);
    analogSetPinAttenuation(PIN_V5_SENSE,  ADC_11db);
    s_last_flags = 0;
    Serial.println("[fault] init OK");
}

void fault_register_evaluate(void) {
    // Read rails
    uint32_t a12 = 0, a5 = 0;
    for (int i = 0; i < 4; i++) { a12 += analogRead(PIN_V12_SENSE); a5 += analogRead(PIN_V5_SENSE); }
    a12 >>= 2; a5 >>= 2;
    float v12 = (a12 * 3.3f / 4095.0f) * V12_DIVIDER_RATIO;
    float v5  = (a5  * 3.3f / 4095.0f) * V5_DIVIDER_RATIO;

    uint32_t flags_now = 0;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        g_status.rail_12v_volts = v12;
        g_status.rail_5v_volts  = v5;
        g_status.rail_3v3_volts = 3.30f;        // Regulator output, untrimmed

        if (v12 < 11.0f) SET_FAULT(g_status.fault_flags, FAULT_UNDERVOLT_12V);
        else             CLR_FAULT(g_status.fault_flags, FAULT_UNDERVOLT_12V);

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
    if (flags & FAULT_UNDERVOLT_12V)    return "uv-12v";
    if (flags & FAULT_OVERTEMP)         return "overtemp";
    if (flags & FAULT_CONFIG_CRC)       return "cfg-crc";
    if (flags & FAULT_QUEUE_OVERFLOW)   return "q-ovf";
    if (flags & FAULT_TASK_HANG)        return "task-hang";
    return "none";
}
