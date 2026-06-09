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

// Both barriers fully closed (down). Vehicle clearance is enforced physically
// by the closed barriers, not by this guard.
bool fsm_guard_road_clear(void) {
    bool ok = false;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        ok = g_status.barrier_left_target_reached
          && g_status.barrier_right_target_reached;
        xSemaphoreGive(g_status_mutex);
    }
    return ok;
}

// Safety guard: cannot lower bridge if a vessel is currently approaching
bool fsm_guard_safe_to_lower(void) {
    bool ok = false;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        ok = !g_status.laser.vessel_approaching;
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

// Deck is parked at (or within tolerance of) the bottom end-stop. Used to gate
// fault/E-stop recovery: IDLE grants the road a GREEN light and opens the
// barriers, so we must NOT drop into IDLE while the deck is still raised.
bool fsm_guard_deck_down(void) {
    bool ok = false;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        ok = (g_status.deck_position_mm <= DECK_HEIGHT_MIN_MM + DECK_HEIGHT_TOLERANCE_MM);
        xSemaphoreGive(g_status_mutex);
    }
    return ok;
}
