// ============================================================================
// counterweight/counterweight.h — Simulated dynamic counterweight system.
// Owner: M2 Eugene (simulation logic)
//
// Software-only simulation of a pump-and-drain water counterweight system.
// No physical hardware — the static lead counterweights remain on the rig.
// The tank levels are SLAVED to the deck height (direct/proportional), so the
// simulated counterweight stays synchronised with the bridge span as it moves.
// The simulation models water levels, pump/drain state, and exposes status to
// the HMI for dashboard display.
// ============================================================================
#pragma once
#include "../system_types.h"

void counterweight_init(void);
void counterweight_tick(void);     // Call at ~20 Hz from task; slaves tanks to deck height
void counterweight_prepare(void);  // Arm the EVT_CW_READY gate before a raise (call on ROAD_CLEARING)
CounterweightStatus_t counterweight_get(void);
