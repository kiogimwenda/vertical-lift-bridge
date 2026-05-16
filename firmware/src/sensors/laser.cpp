// ============================================================================
// sensors/laser.cpp — Quad Laser Break-Beam for vessel direction detection.
// Owner: M3 Cindy
//
// Four Laser/LDR modules in two pairs:
//   Upstream pair  (LDR1 = Beam A, LDR2 = Beam B) — detects vessels from upstream
//   Downstream pair (LDR3 = Beam A, LDR4 = Beam B) — detects vessels from downstream
//
// Within each pair the sensors are spaced LASER_BEAM_SPACING_CM (3 cm)
// apart. Beam-arrival order reveals direction:
//   A blocked first, then B  -> APPROACHING (vessel heading toward bridge)
//   B blocked first, then A  -> LEAVING     (vessel heading away from bridge)
//
// Sensors are measured in round-robin or simultaneously.
// ============================================================================
#include "laser.h"
#include "../pin_config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static LaserStatus_t s_ls = {};

// History ring for direction inference (5 samples per pair).
#define DIR_HIST 5
static uint8_t s_hist_up_a[DIR_HIST]   = {0};
static uint8_t s_hist_up_b[DIR_HIST]   = {0};
static uint8_t s_hist_down_a[DIR_HIST] = {0};
static uint8_t s_hist_down_b[DIR_HIST] = {0};
static uint8_t s_hist_idx = 0;

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

void sensors_laser_init(void) {
    pinMode(PIN_LDR1_IN, INPUT_PULLDOWN);
    pinMode(PIN_LDR2_IN, INPUT_PULLDOWN);
    pinMode(PIN_LDR3_IN, INPUT_PULLDOWN);
    pinMode(PIN_LDR4_IN, INPUT_PULLDOWN);
    Serial.println("[laser] init OK (4 sensors)");
}

void sensors_laser_tick(void) {
    // Determine blocked state for each beam (Blocked = LOW)
    bool up_a  = (digitalRead(PIN_LDR1_IN) == LOW);
    bool up_b  = (digitalRead(PIN_LDR2_IN) == LOW);
    bool dn_a  = (digitalRead(PIN_LDR3_IN) == LOW);
    bool dn_b  = (digitalRead(PIN_LDR4_IN) == LOW);

    s_ls.upstream_blocked   = up_a || up_b;
    s_ls.downstream_blocked = dn_a || dn_b;

    // Push to history rings
    s_hist_up_a[s_hist_idx]   = up_a ? 1 : 0;
    s_hist_up_b[s_hist_idx]   = up_b ? 1 : 0;
    s_hist_down_a[s_hist_idx] = dn_a ? 1 : 0;
    s_hist_down_b[s_hist_idx] = dn_b ? 1 : 0;
    s_hist_idx = (s_hist_idx + 1) % DIR_HIST;

    // Direction inference per pair
    s_ls.upstream_direction   = infer_direction(s_hist_up_a,   s_hist_up_b,   s_hist_idx);
    s_ls.downstream_direction = infer_direction(s_hist_down_a, s_hist_down_b, s_hist_idx);

    // Combined: vessel approaching from either side triggers bridge opening
    s_ls.vessel_approaching = (s_ls.upstream_direction   == DIR_APPROACHING)
                           || (s_ls.downstream_direction == DIR_APPROACHING);

    s_ls.last_update_ms = millis();

    // Edge detection for FSM events
    static bool s_was_approaching = false;
    bool now_approaching = s_ls.vessel_approaching;

    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.laser = s_ls;
        // Laser does not easily timeout in the same way as ultrasonic, so clear the fault
        CLR_FAULT(g_status.fault_flags, FAULT_LASER_FAIL);
        xSemaphoreGive(g_status_mutex);
    }

    if (now_approaching && !s_was_approaching) {
        SystemEvent_t e = EVT_VEHICLE_DETECTED;
        xQueueSend(g_event_queue, &e, 0);
    } else if (!now_approaching && s_was_approaching) {
        // Only clear when no pair reports any blockage
        if (!s_ls.upstream_blocked && !s_ls.downstream_blocked) {
            SystemEvent_t e = EVT_VEHICLE_CLEARED;
            xQueueSend(g_event_queue, &e, 0);
        }
    }
    s_was_approaching = now_approaching;
}

LaserStatus_t sensors_laser_get(void) { return s_ls; }
