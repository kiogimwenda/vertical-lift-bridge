// ============================================================================
// sensors/ultrasonic.cpp — Quad HC-SR04 beams for vessel direction detection.
// Owner: M3 Cindy
//
// Four HC-SR04 modules in two pairs:
//   Upstream pair  (US1 = Beam A, US2 = Beam B) — detects vessels from upstream
//   Downstream pair (US3 = Beam A, US4 = Beam B) — detects vessels from downstream
//
// Within each pair the sensors are spaced ULTRASONIC_BEAM_SPACING_CM (3 cm)
// apart. Beam-arrival order reveals direction:
//   A blocked first, then B  -> APPROACHING (vessel heading toward bridge)
//   B blocked first, then A  -> LEAVING     (vessel heading away from bridge)
//
// Sensors are measured in round-robin: US1 -> US2 -> US3 -> US4 -> repeat.
// At 20 Hz tick each sensor is read every 200 ms.
// ============================================================================
#include "ultrasonic.h"
#include "../pin_config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static UltrasonicStatus_t s_us = {};

// History ring for direction inference (5 samples per pair).
#define DIR_HIST 5
static uint8_t s_hist_up_a[DIR_HIST]   = {0};
static uint8_t s_hist_up_b[DIR_HIST]   = {0};
static uint8_t s_hist_down_a[DIR_HIST] = {0};
static uint8_t s_hist_down_b[DIR_HIST] = {0};
static uint8_t s_hist_idx = 0;

static uint16_t measure_cm(uint8_t trig_pin, uint8_t echo_pin) {
    digitalWrite(trig_pin, LOW);
    delayMicroseconds(2);
    digitalWrite(trig_pin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig_pin, LOW);

    unsigned long us = pulseIn(echo_pin, HIGH, 25000UL);   // 25 ms timeout
    if (us == 0) return 0xFFFF;                            // No echo
    return (uint16_t)(us / 58UL);
}

// Scan a history ring and return the index of the first 0->1 transition,
// or -1 if no rising edge found.
static int find_first_rising(const uint8_t* hist, uint8_t start) {
    uint8_t prev = 0;
    for (int i = 0; i < DIR_HIST; i++) {
        int idx = (start + i) % DIR_HIST;
        if (hist[idx] && !prev) return i;
        prev = hist[idx];
    }
    return -1;
}

// Infer direction for one pair given its A and B history rings.
static VehicleDirection_t infer_direction(const uint8_t* hist_a,
                                          const uint8_t* hist_b,
                                          uint8_t start) {
    int a_first = find_first_rising(hist_a, start);
    int b_first = find_first_rising(hist_b, start);

    if (a_first >= 0 && b_first >= 0) {
        if      (a_first < b_first) return DIR_APPROACHING;
        else if (b_first < a_first) return DIR_LEAVING;
        else                        return DIR_BOTH;
    } else if (a_first >= 0 || b_first >= 0) {
        return (a_first >= 0) ? DIR_APPROACHING : DIR_LEAVING;
    }
    return DIR_NONE;
}

void sensors_ultrasonic_init(void) {
    pinMode(PIN_US1_TRIG, OUTPUT);
    pinMode(PIN_US1_ECHO, INPUT);
    pinMode(PIN_US2_TRIG, OUTPUT);
    pinMode(PIN_US2_ECHO, INPUT);
    pinMode(PIN_US3_TRIG, OUTPUT);
    pinMode(PIN_US3_ECHO, INPUT);
    pinMode(PIN_US4_TRIG, OUTPUT);
    pinMode(PIN_US4_ECHO, INPUT);
    digitalWrite(PIN_US1_TRIG, LOW);
    digitalWrite(PIN_US2_TRIG, LOW);
    digitalWrite(PIN_US3_TRIG, LOW);
    digitalWrite(PIN_US4_TRIG, LOW);
    Serial.println("[us] init OK (4 sensors, upstream + downstream)");
}

void sensors_ultrasonic_tick(void) {
    // Round-robin: cycle through US1 -> US2 -> US3 -> US4.
    static uint32_t s_n = 0;
    uint8_t slot = s_n % 4;
    s_n++;

    switch (slot) {
    case 0:
        s_us.distance_us1_cm = measure_cm(PIN_US1_TRIG, PIN_US1_ECHO);
        break;
    case 1:
        s_us.distance_us2_cm = measure_cm(PIN_US2_TRIG, PIN_US2_ECHO);
        break;
    case 2:
        s_us.distance_us3_cm = measure_cm(PIN_US3_TRIG, PIN_US3_ECHO);
        break;
    case 3:
        s_us.distance_us4_cm = measure_cm(PIN_US4_TRIG, PIN_US4_ECHO);
        break;
    }

    // Determine blocked state for each beam
    bool up_a  = (s_us.distance_us1_cm < ULTRASONIC_TRIGGER_CM);
    bool up_b  = (s_us.distance_us2_cm < ULTRASONIC_TRIGGER_CM);
    bool dn_a  = (s_us.distance_us3_cm < ULTRASONIC_TRIGGER_CM);
    bool dn_b  = (s_us.distance_us4_cm < ULTRASONIC_TRIGGER_CM);

    s_us.upstream_blocked   = up_a || up_b;
    s_us.downstream_blocked = dn_a || dn_b;

    // Push to history rings
    s_hist_up_a[s_hist_idx]   = up_a ? 1 : 0;
    s_hist_up_b[s_hist_idx]   = up_b ? 1 : 0;
    s_hist_down_a[s_hist_idx] = dn_a ? 1 : 0;
    s_hist_down_b[s_hist_idx] = dn_b ? 1 : 0;
    s_hist_idx = (s_hist_idx + 1) % DIR_HIST;

    // Direction inference per pair
    s_us.upstream_direction   = infer_direction(s_hist_up_a,   s_hist_up_b,   s_hist_idx);
    s_us.downstream_direction = infer_direction(s_hist_down_a, s_hist_down_b, s_hist_idx);

    // Combined: vessel approaching from either side triggers bridge opening
    s_us.vessel_approaching = (s_us.upstream_direction   == DIR_APPROACHING)
                           || (s_us.downstream_direction == DIR_APPROACHING);

    s_us.last_update_ms = millis();

    // Fault: all four sensors dead
    bool all_dead = (s_us.distance_us1_cm == 0xFFFF)
                 && (s_us.distance_us2_cm == 0xFFFF)
                 && (s_us.distance_us3_cm == 0xFFFF)
                 && (s_us.distance_us4_cm == 0xFFFF);

    // Edge detection for FSM events
    static bool s_was_approaching = false;
    bool now_approaching = s_us.vessel_approaching;

    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.ultrasonic = s_us;
        if (all_dead) SET_FAULT(g_status.fault_flags, FAULT_ULTRASONIC_FAIL);
        else          CLR_FAULT(g_status.fault_flags, FAULT_ULTRASONIC_FAIL);
        xSemaphoreGive(g_status_mutex);
    }

    if (now_approaching && !s_was_approaching) {
        SystemEvent_t e = EVT_VEHICLE_DETECTED;
        xQueueSend(g_event_queue, &e, 0);
    } else if (!now_approaching && s_was_approaching) {
        // Only clear when no pair reports any blockage
        if (!s_us.upstream_blocked && !s_us.downstream_blocked) {
            SystemEvent_t e = EVT_VEHICLE_CLEARED;
            xQueueSend(g_event_queue, &e, 0);
        }
    }
    s_was_approaching = now_approaching;
}

UltrasonicStatus_t sensors_ultrasonic_get(void) { return s_us; }
