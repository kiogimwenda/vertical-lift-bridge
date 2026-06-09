// ============================================================================
// counterweight/counterweight.cpp — Simulated dynamic counterweight system.
// Owner: M2 Eugene
//
// Pure software simulation. No physical pumps, valves, or level sensors.
// Models two water tanks (left and right) whose levels are SLAVED to the deck
// height so the simulated counterweight stays synchronised with the bridge
// span at all times (direct / proportional mapping):
//
//     water_level = (deck_position_mm / DECK_HEIGHT_MAX_MM) * CW_TANK_CAPACITY_ML
//
//       deck   0 mm  ->  tanks   0 ml   (bridge down  -> counterweight empty)
//       deck  88 mm  ->  tanks  75 ml
//       deck 175 mm  ->  tanks 150 ml   (bridge up    -> counterweight full)
//
// The fill/drain animation (CW_SIM_*_RATE) makes the level chase the
// deck-derived target smoothly; with the default deck-travel calibration the
// pump/drain rates keep up, so the tanks track the deck within a tick and the
// HMI tank bars move in lockstep with the deck bar.
//
// Simulation runs at ~20 Hz (50 ms tick). When the deck is stationary and the
// tanks have settled at the level for that deck position, the module posts
// EVT_CW_READY — the pre-raise gate out of STATE_ROAD_CLEARING.
// ============================================================================
#include "counterweight.h"
#include "../system_types.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static CounterweightStatus_t s_cw = {};
static uint32_t s_last_tick_ms = 0;
static bool     s_was_balanced = false;
static int16_t  s_prev_deck_mm = 0;

// Slave a tank's target to the deck-derived level, and set pump/drain so the
// HMI status label reflects the direction the counterweight is moving.
static void set_tank_target(CwTankSim_t& tank, float target_ml) {
    if (target_ml < 0.0f)                 target_ml = 0.0f;
    if (target_ml > CW_TANK_CAPACITY_ML)  target_ml = CW_TANK_CAPACITY_ML;
    tank.target_ml = target_ml;
    if (target_ml > tank.water_level_ml + CW_SIM_TOLERANCE_ML) {
        tank.pump_active = true;  tank.drain_open = false;     // filling  (deck rising)
    } else if (target_ml < tank.water_level_ml - CW_SIM_TOLERANCE_ML) {
        tank.pump_active = false; tank.drain_open = true;      // draining (deck lowering)
    } else {
        tank.pump_active = false; tank.drain_open = false;     // settled
    }
}

// Simulate one tank for one tick — chases target_ml at the configured rate.
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
    if (tank.water_level_ml < 0.0f)                tank.water_level_ml = 0.0f;
    if (tank.water_level_ml > CW_TANK_CAPACITY_ML) tank.water_level_ml = CW_TANK_CAPACITY_ML;
    tank.weight_g = tank.water_level_ml;   // 1 ml water ≈ 1 g
}

static bool tank_at_target(const CwTankSim_t& tank) {
    float diff = tank.water_level_ml - tank.target_ml;
    if (diff < 0) diff = -diff;
    return diff <= CW_SIM_TOLERANCE_ML;
}

void counterweight_init(void) {
    // Deck is assumed parked at the bottom (0 mm) at boot, so the synchronised
    // counterweight starts empty and balanced.
    s_cw.left.water_level_ml = 0.0f;
    s_cw.left.weight_g       = 0.0f;
    s_cw.left.target_ml      = 0.0f;
    s_cw.left.pump_active    = false;
    s_cw.left.drain_open     = false;
    s_cw.right = s_cw.left;

    s_cw.balanced       = true;
    s_cw.last_update_ms = millis();
    s_last_tick_ms      = millis();
    s_was_balanced      = true;
    s_prev_deck_mm      = 0;

    Serial.println("[cw] init OK (deck-synchronised counterweight, starts empty)");
}

void counterweight_prepare(void) {
    // Arm the EVT_CW_READY rising-edge detector so the FSM receives exactly one
    // CW_READY once the counterweight has settled at the level for the current
    // deck position. Called on entry to STATE_ROAD_CLEARING (deck at 0 mm),
    // where the synchronised level is already 0 ml — the edge fires next tick.
    s_was_balanced = false;
}

void counterweight_tick(void) {
    uint32_t now = millis();
    float dt_s = (now - s_last_tick_ms) / 1000.0f;
    s_last_tick_ms = now;
    if (dt_s > 0.2f) dt_s = 0.2f;          // clamp first-tick / scheduling lag

    // --- Slave the tank targets to the (timer-estimated) deck height ---------
    int16_t deck_mm = 0;
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        deck_mm = g_status.deck_position_mm;
        xSemaphoreGive(g_status_mutex);
    }
    if (deck_mm < 0)                  deck_mm = 0;
    if (deck_mm > DECK_HEIGHT_MAX_MM) deck_mm = DECK_HEIGHT_MAX_MM;
    float target = ((float)deck_mm / (float)DECK_HEIGHT_MAX_MM) * CW_TANK_CAPACITY_ML;

    set_tank_target(s_cw.left,  target);
    set_tank_target(s_cw.right, target);

    sim_tank(s_cw.left,  dt_s);
    sim_tank(s_cw.right, dt_s);

    // "Balanced" means the deck is stationary AND both tanks have reached the
    // level for that position. Requiring a stationary deck keeps EVT_CW_READY
    // from firing spuriously while the counterweight is tracking a moving deck.
    bool deck_moving  = (deck_mm != s_prev_deck_mm);
    s_prev_deck_mm    = deck_mm;
    bool now_balanced = !deck_moving
                     && tank_at_target(s_cw.left)
                     && tank_at_target(s_cw.right);
    s_cw.balanced       = now_balanced;
    s_cw.last_update_ms = now;

    // Push to shared status
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.counterweight = s_cw;
        xSemaphoreGive(g_status_mutex);
    }

    // Post EVT_CW_READY on the rising edge of balanced (pre-raise gate).
    if (now_balanced && !s_was_balanced) {
        SystemEvent_t e = EVT_CW_READY;
        xQueueSend(g_event_queue, &e, 0);
    }
    s_was_balanced = now_balanced;
}
