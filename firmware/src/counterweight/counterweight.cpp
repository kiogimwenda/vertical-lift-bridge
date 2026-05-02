// ============================================================================
// counterweight/counterweight.cpp — Simulated dynamic counterweight system.
// Owner: M2 Eugene
//
// Pure software simulation. No physical pumps, valves, or level sensors.
// Models two water tanks (left and right) that can be filled or drained to
// a target level. Exposes status to SharedStatus for HMI display.
//
// Simulation runs at ~20 Hz (50 ms tick). Fill and drain rates are tunable
// in system_types.h. When both tanks reach their target (within tolerance),
// the module posts EVT_CW_READY to the FSM event queue.
// ============================================================================
#include "counterweight.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static CounterweightStatus_t s_cw = {};
static uint32_t s_last_tick_ms = 0;
static bool s_was_balanced = false;

// Simulate one tank for one tick.
static void sim_tank(CwTankSim_t& tank, float dt_s) {
    if (tank.pump_active) {
        tank.water_level_ml += CW_SIM_FILL_RATE_ML_PER_S * dt_s;
        if (tank.water_level_ml >= tank.target_ml) {
            tank.water_level_ml = tank.target_ml;
            tank.pump_active = false;
        }
    }
    if (tank.drain_open) {
        tank.water_level_ml -= CW_SIM_DRAIN_RATE_ML_PER_S * dt_s;
        if (tank.water_level_ml <= tank.target_ml) {
            tank.water_level_ml = tank.target_ml;
            tank.drain_open = false;
        }
    }

    // Clamp to valid range
    if (tank.water_level_ml < 0.0f) tank.water_level_ml = 0.0f;
    if (tank.water_level_ml > CW_TANK_CAPACITY_ML) tank.water_level_ml = CW_TANK_CAPACITY_ML;

    // Derived weight (1 ml water ≈ 1 g)
    tank.weight_g = tank.water_level_ml;
}

static bool tank_at_target(const CwTankSim_t& tank) {
    float diff = tank.water_level_ml - tank.target_ml;
    if (diff < 0) diff = -diff;
    return diff <= CW_SIM_TOLERANCE_ML;
}

void counterweight_init(void) {
    // Start with tanks at default fill level (simulating initial static weight)
    s_cw.left.water_level_ml  = CW_SIM_TARGET_DEFAULT_ML;
    s_cw.left.weight_g        = CW_SIM_TARGET_DEFAULT_ML;
    s_cw.left.target_ml       = CW_SIM_TARGET_DEFAULT_ML;
    s_cw.left.pump_active     = false;
    s_cw.left.drain_open      = false;

    s_cw.right.water_level_ml = CW_SIM_TARGET_DEFAULT_ML;
    s_cw.right.weight_g       = CW_SIM_TARGET_DEFAULT_ML;
    s_cw.right.target_ml      = CW_SIM_TARGET_DEFAULT_ML;
    s_cw.right.pump_active    = false;
    s_cw.right.drain_open     = false;

    s_cw.balanced = true;
    s_cw.last_update_ms = millis();
    s_last_tick_ms = millis();
    s_was_balanced = true;

    Serial.println("[cw] init OK (simulated dynamic counterweight)");
}

void counterweight_set_target(float left_ml, float right_ml) {
    // Clamp targets
    if (left_ml < 0.0f)  left_ml  = 0.0f;
    if (left_ml > CW_TANK_CAPACITY_ML) left_ml = CW_TANK_CAPACITY_ML;
    if (right_ml < 0.0f) right_ml = 0.0f;
    if (right_ml > CW_TANK_CAPACITY_ML) right_ml = CW_TANK_CAPACITY_ML;

    s_cw.left.target_ml  = left_ml;
    s_cw.right.target_ml = right_ml;

    // Determine whether to fill or drain each tank
    if (left_ml > s_cw.left.water_level_ml + CW_SIM_TOLERANCE_ML) {
        s_cw.left.pump_active = true;
        s_cw.left.drain_open  = false;
    } else if (left_ml < s_cw.left.water_level_ml - CW_SIM_TOLERANCE_ML) {
        s_cw.left.pump_active = false;
        s_cw.left.drain_open  = true;
    }

    if (right_ml > s_cw.right.water_level_ml + CW_SIM_TOLERANCE_ML) {
        s_cw.right.pump_active = true;
        s_cw.right.drain_open  = false;
    } else if (right_ml < s_cw.right.water_level_ml - CW_SIM_TOLERANCE_ML) {
        s_cw.right.pump_active = false;
        s_cw.right.drain_open  = true;
    }

    s_cw.balanced = false;
}

void counterweight_tick(void) {
    uint32_t now = millis();
    float dt_s = (now - s_last_tick_ms) / 1000.0f;
    s_last_tick_ms = now;

    // Clamp dt to avoid jumps on first tick or lag
    if (dt_s > 0.2f) dt_s = 0.2f;

    sim_tank(s_cw.left, dt_s);
    sim_tank(s_cw.right, dt_s);

    bool now_balanced = tank_at_target(s_cw.left) && tank_at_target(s_cw.right);
    s_cw.balanced = now_balanced;
    s_cw.last_update_ms = now;

    // Push to shared status
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.counterweight = s_cw;
        xSemaphoreGive(g_status_mutex);
    }

    // Post EVT_CW_READY on rising edge of balanced
    if (now_balanced && !s_was_balanced) {
        SystemEvent_t e = EVT_CW_READY;
        xQueueSend(g_event_queue, &e, 0);
    }
    s_was_balanced = now_balanced;
}

CounterweightStatus_t counterweight_get(void) { return s_cw; }
