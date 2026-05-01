# Project Audit — Original vs Revised Mechanism

> Internal audit comparing the original repo's bridge architecture (4-tower, GT2 belt-coupled lead-screw) against the revised vertical-lift design adopted in the updated proposal.

## Original architecture (in repo at audit time)
- **Towers:** 4 (corners of a rectangular footprint)
- **Drive:** Single JGA25-370 motor → flexible coupling → T8 lead screw on Tower A-Left
- **Synchronisation:** GT2 6 mm timing belt forming a closed rectangular loop, connecting 20T pulleys at the top of each tower
- **Sensors:** Dual ultrasonic + IR retro-reflective for deck-clearance verification
- **Limit detection:** None (open-loop, software-only)

## Issues identified
| # | Issue | Severity |
|---|---|---|
| 1 | Belt synchronisation is **single-point-of-failure** — one belt jump desyncs all four corners | High |
| 2 | Lead-screw + 4-tower design → 8 alignment surfaces, every misalignment compounds | High |
| 3 | No counterweight → motor sized to lift full deck mass directly (4× higher current) | Medium |
| 4 | IR retro-reflective sensors unreliable in ambient lighting (lab fluorescent) | Medium |
| 5 | No hardware limit switches → relies on software estimate of position | High (safety) |
| 6 | No defined fault model — what triggers a stop? | High (safety) |
| 7 | TFT ribbon cable runs across motor 12 V trace area in PCB layout | Medium (EMI) |

## Revised architecture (adopted)
- **Towers:** 2 (left + right) with MGN12 linear rails
- **Drive:** Same JGA25-370, but driving a Ø30 mm cable drum
- **Cable + counterweight:** 2× 120 g counterweights via top pulleys, balancing ~240 g deck
- **Vision:** ESP32-CAM companion module on UART2 (replaces IR retro-reflective)
- **Limits:** KW11-3Z microswitches at top + bottom of one tower
- **Faults:** 16-flag bitmask register, edge-triggered events, software watchdog

## Why the change
1. **Mechanical reliability** — single-stage cable system has only the drum keyway as a sync point.
2. **Motor sizing** — counterweight reduces nominal lift current from ~1500 mA to ~600 mA.
3. **Sensor reliability** — vision over UART JSON is observable and debuggable; IR analog levels are not.
4. **Safety** — physical limit switches override any software estimate, fail-safe to STOP.
5. **Footprint reduction** — 1200 × 600 mm vs original ~1500 × 800 mm (smaller demo table).

## Migration impact
- All 4-tower CAD parts deprecated; 13 new OpenSCAD parts created.
- Motor calibration constant `CAL_COUNTS_PER_MM` now drum-circumference-based (≈ 42 counts/mm at Ø30 mm).
- PCB schematic gains J7 (4-pin JST-XH for ESP32-CAM); IR analog inputs deleted.
- BOM: removed 4× lead screw, 4× pulley, 2× belt; added 2× counterweight, ESP32-CAM, 2× MGN12 rail.

## Sign-off
This audit was discussed and approved by all 5 group members on the date of the revised proposal commit.
