// ============================================================================
// fsm/fsm_guards.cpp — boolean preconditions for FSM transitions.
// Owner: George (M1)
// Each guard is side-effect-free.
// ============================================================================
#include "fsm_guards.h"
#include "../system_types.h"
#include "../pin_config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// All peripherals up + no faults.
bool fsm_guard_init_complete(void) {
    bool ok = false;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        ok = !ANY_FAULT(g_status.fault_flags) && !g_status.estop_active;
        xSemaphoreGive(g_status_mutex);
    }
    return ok;
}

// Vehicle confirmed by EITHER vision OR ultrasonic, no faults, no e-stop.
bool fsm_guard_can_clear_road(void) {
    bool ok = false;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        bool sensor_ok = g_status.vision.vehicle_present
                      || g_status.ultrasonic.vessel_approaching;
        ok = sensor_ok && !ANY_FAULT(g_status.fault_flags) && !g_status.estop_active;
        xSemaphoreGive(g_status_mutex);
    }
    return ok;
}

// Both barriers down + no vehicle in zone.
bool fsm_guard_road_clear(void) {
    bool ok = false;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        ok = g_status.barrier_left_target_reached
          && g_status.barrier_right_target_reached
          && !g_status.ultrasonic.upstream_blocked
          && !g_status.ultrasonic.downstream_blocked;
        xSemaphoreGive(g_status_mutex);
    }
    return ok;
}

// Both barriers up.
bool fsm_guard_road_opened(void) {
    bool ok = false;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        ok = g_status.barrier_left_target_reached
          && g_status.barrier_right_target_reached;
        xSemaphoreGive(g_status_mutex);
    }
    return ok;
}

// E-stop physical button released AND fault flags clear.
bool fsm_guard_estop_clearable(void) {
    bool ok = false;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        ok = !g_status.estop_active && !ANY_FAULT(g_status.fault_flags);
        xSemaphoreGive(g_status_mutex);
    }
    return ok;
}
