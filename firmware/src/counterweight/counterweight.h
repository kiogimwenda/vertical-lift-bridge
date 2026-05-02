// ============================================================================
// counterweight/counterweight.h — Simulated dynamic counterweight system.
// Owner: M2 Eugene (simulation logic)
//
// Software-only simulation of a pump-and-drain water counterweight system.
// No physical hardware — the static lead counterweights remain on the rig.
// The simulation models water levels, pump/drain state, and exposes status
// to the HMI for dashboard display.
// ============================================================================
#pragma once
#include "../system_types.h"

void counterweight_init(void);
void counterweight_tick(void);     // Call at ~20 Hz from task
void counterweight_set_target(float left_ml, float right_ml);
CounterweightStatus_t counterweight_get(void);
