// ============================================================================
// sensors/ultrasonic.cpp — Dual HC-SR04 beams for vehicle range + direction.
// Owner: M3 Cindy
//
// Two HC-SR04 modules mounted ULTRASONIC_BEAM_SPACING_CM apart along the
// approach lane. Sequence of trigger reveals direction:
//   A blocked first, then B  -> APPROACHING
//   B blocked first, then A  -> LEAVING
// Echo measurement uses pulseIn() with timeout = 25 ms (~4 m max).
// At 20 Hz tick (50 ms) one trigger per sensor per cycle, alternating.
// ============================================================================
#include "ultrasonic.h"
#include "../pin_config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static UltrasonicStatus_t s_us = {};

// History for direction inference (keep last 5 samples).
#define DIR_HIST 5
static uint8_t s_hist_blocked_a[DIR_HIST] = {0};
static uint8_t s_hist_blocked_b[DIR_HIST] = {0};
static uint8_t s_hist_idx = 0;

static uint16_t measure_cm(uint8_t trig_pin, uint8_t echo_pin) {
    digitalWrite(trig_pin, LOW);
    delayMicroseconds(2);
    digitalWrite(trig_pin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig_pin, LOW);

    unsigned long us = pulseIn(echo_pin, HIGH, 25000UL);   // 25 ms timeout
    if (us == 0) return 0xFFFF;                            // No echo
    // Speed of sound 343 m/s -> 0.0343 cm/us, round-trip => /58.0
    return (uint16_t)(us / 58UL);
}

void sensors_ultrasonic_init(void) {
    pinMode(PIN_US_TRIG_A, OUTPUT);
    pinMode(PIN_US_ECHO_A, INPUT);
    pinMode(PIN_US_TRIG_B, OUTPUT);
    pinMode(PIN_US_ECHO_B, INPUT);
    digitalWrite(PIN_US_TRIG_A, LOW);
    digitalWrite(PIN_US_TRIG_B, LOW);
    Serial.println("[us] init OK");
}

void sensors_ultrasonic_tick(void) {
    // Alternate: even ticks measure A, odd ticks measure B.
    static uint32_t s_n = 0;
    s_n++;
    if ((s_n & 1) == 0) {
        s_us.distance_a_cm = measure_cm(PIN_US_TRIG_A, PIN_US_ECHO_A);
        s_us.beam_a_blocked = (s_us.distance_a_cm < ULTRASONIC_TRIGGER_CM);
    } else {
        s_us.distance_b_cm = measure_cm(PIN_US_TRIG_B, PIN_US_ECHO_B);
        s_us.beam_b_blocked = (s_us.distance_b_cm < ULTRASONIC_TRIGGER_CM);
    }

    // Push to history ring
    s_hist_blocked_a[s_hist_idx] = s_us.beam_a_blocked ? 1 : 0;
    s_hist_blocked_b[s_hist_idx] = s_us.beam_b_blocked ? 1 : 0;
    s_hist_idx = (s_hist_idx + 1) % DIR_HIST;

    // Direction inference: find which beam transitioned 0->1 first.
    int a_first = -1, b_first = -1;
    uint8_t a_prev = 0, b_prev = 0;
    for (int i = 0; i < DIR_HIST; i++) {
        int idx = (s_hist_idx + i) % DIR_HIST;
        if (s_hist_blocked_a[idx] && !a_prev && a_first < 0) a_first = i;
        if (s_hist_blocked_b[idx] && !b_prev && b_first < 0) b_first = i;
        a_prev = s_hist_blocked_a[idx];
        b_prev = s_hist_blocked_b[idx];
    }
    if (a_first >= 0 && b_first >= 0) {
        if      (a_first < b_first) s_us.direction = DIR_APPROACHING;
        else if (b_first < a_first) s_us.direction = DIR_LEAVING;
        else                        s_us.direction = DIR_BOTH;
    } else if (a_first >= 0 || b_first >= 0) {
        s_us.direction = (a_first >= 0) ? DIR_APPROACHING : DIR_LEAVING;
    } else {
        s_us.direction = DIR_NONE;
    }
    s_us.last_update_ms = millis();

    // Both timeouts -> sensor failure flag
    bool both_dead = (s_us.distance_a_cm == 0xFFFF && s_us.distance_b_cm == 0xFFFF);

    // Push to shared status + emit events
    static bool s_was_blocked = false;
    bool now_blocked = s_us.beam_a_blocked || s_us.beam_b_blocked;

    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.ultrasonic = s_us;
        if (both_dead) SET_FAULT(g_status.fault_flags, FAULT_ULTRASONIC_FAIL);
        else           CLR_FAULT(g_status.fault_flags, FAULT_ULTRASONIC_FAIL);
        xSemaphoreGive(g_status_mutex);
    }

    if (now_blocked && !s_was_blocked) {
        SystemEvent_t e = EVT_VEHICLE_DETECTED;
        xQueueSend(g_event_queue, &e, 0);
    } else if (!now_blocked && s_was_blocked) {
        SystemEvent_t e = EVT_VEHICLE_CLEARED;
        xQueueSend(g_event_queue, &e, 0);
    }
    s_was_blocked = now_blocked;
}

UltrasonicStatus_t sensors_ultrasonic_get(void) { return s_us; }
