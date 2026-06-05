# Known Limitations

Honest, documented trade-offs in the current build. Each entry lists the
limitation, why it exists, how the firmware mitigates it, and any procedure the
operator must follow.

---

## L1 — No motor current sensing
- **Cause:** The L298N module exposes no MCU-facing current-sense output (its
  SENSA/SENSB pins are shunted to ground on the module).
- **Effect:** `FAULT_OVERCURRENT` is never raised; `motor_current_ma` is always 0.
- **Mitigation:** The L298N's on-chip thermal shutdown protects the bridge, and
  the timer runaway-guard (`FAULT_STALL`, see L3) bounds how long the motor may
  be driven.

## L2 — No deck-position potentiometer, no limit switches (timer-based positioning)
- **Cause:** The position pot (GPIO 35) and limit switches (GPIO 39) are omitted
  from this build. Both pins are left as reserved spares (not reassigned).
- **Effect:** Deck height is *estimated*, not measured. There is no physical
  end-stop detection.
- **Mitigation:** `motor/motor_driver.cpp` integrates height from motor run-time
  using `DECK_RAISE_TIME_MS` / `DECK_LOWER_TIME_MS`. The estimate is pinned hard
  to `0` / `DECK_HEIGHT_MAX_MM` and a virtual `EVT_TOP/BOTTOM_LIMIT_HIT` is
  emitted whenever a traverse completes, so error re-synchronises twice per
  cycle and cannot accumulate.
- **Operator procedure:** **Park the deck at the bottom (0 mm) before power-off.**
  The firmware assumes the deck is fully down at boot. Powering on while raised
  will make the height read low until the next full lower re-syncs it.
- **Calibration:** Bench-measure the full-travel times at the operating PWM
  duties and update `DECK_RAISE_TIME_MS` / `DECK_LOWER_TIME_MS` in
  `system_types.h`. This is what makes the HMI deck-height readout accurate.

## L3 — Runaway guard replaces true stall detection
- **Cause:** With no position feedback a genuine mechanical stall is invisible.
- **Mitigation:** If a move runs longer than a full traverse +
  `MOTION_TIMEOUT_MARGIN_MS`, `FAULT_STALL` is raised and the FSM trips to FAULT
  (relay cut). This bounds worst-case motor drive into a hard stop.

## L4 — No rail-voltage monitoring
- **Cause:** No GPIO/ADC is allocated to a high-impedance voltage divider.
- **Effect:** `rail_*_volts` fields hold the `-1.0f` "not measured" sentinel.
- **Mitigation:** ESP32 internal brownout detector + LM2596/AMS1117 internal
  current/thermal limits provide hardware-level protection.

## L5 — Open-loop barrier servos
- **Cause:** Two SG90 servos share one PWM channel (GPIO 16) and have no
  position feedback.
- **Mitigation:** A deterministic software sweep (~40°/s) drives the angle; a
  barrier watchdog raises `FAULT_BARRIER_TIMEOUT` if the sweep does not reach
  its target within `BARRIER_TIMEOUT_MS` (catches a stalled safety task rather
  than a stuck servo, since the sweep itself is deterministic).

## L6 — Buzzer disabled
- **Cause:** `PIN_BUZZER` is `-1` to preserve UART TX0 (GPIO 1) for the serial
  monitor. Audible alerts are therefore not emitted.

## L7 — Vision module omitted
- **Cause:** The ESP32-CAM vision path is omitted from this build. UART2 / GPIO
  17 is left as a spare. `FAULT_VISION_LINK_LOST` is reserved but never set.
- **Effect:** Vessel detection relies solely on the quad laser break-beam.

## L8 — Manual lowering only
- **Cause:** Lowering from `RAISED_HOLD` is operator-initiated by design, guarded
  by `fsm_guard_safe_to_lower()` (no auto-lower). `EVT_HOLD_TIMEOUT` /
  `HOLD_TIMEOUT_MS` are reserved but unused — the bridge will not auto-lower onto
  an approaching vessel.
