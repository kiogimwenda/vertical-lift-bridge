# Known Limitations — VLB v2.1

This document records design constraints that ship with the v2 hardware and the
v3 fix path for each. None of these prevent the project from meeting its
EEE 2412 demo requirements; they are honest engineering trade-offs forced by
the ESP32-WROOM-32 GPIO budget.

---

## L1 — ADC pin budget exhaustion

### Symptom
Three logical ADC consumers want GPIO 34 (ADC1_CH6) and two want GPIO 35
(ADC1_CH7). All sources are wired to the same physical pin with no analog
multiplexer, so the voltage actually present on the pin is the
impedance-weighted average of every connected source.

### Affected functionality (v1 design intent → v2.1 actual)

| Feature | v1 intent | v2.1 reality | Mitigation |
|---|---|---|---|
| 12 V rail undervoltage detection | Read divider tap on GPIO 34 | **Removed** | ESP32 brownout + BTS7960 UVLO + LM2596 thermal limit |
| 5 V rail undervoltage detection | Read divider tap on GPIO 35 | **Removed** | Same as above |
| Front-panel 5-button resistor ladder | Read 5 voltage bands on GPIO 34 | **Removed** | Touchscreen is the sole HMI input device |
| BTS7960 motor current sense | Read IS pin on GPIO 34 | **Active** (sole owner) | — |
| Deck position potentiometer | Read wiper on GPIO 35 | **Active** (sole owner) | — |

### Why software multiplexing doesn't fix it
The pin_config.h v2.0 comments described a "multiplexed by FSM state" scheme
where each consumer would read only when the others were idle. This works for
ADC *contention* (only one task drives the SAR converter at a time, which
Arduino's `analogRead()` mutex already guarantees), but it does **not** fix
the *electrical* problem: the BTS7960 IS pin and the deck-position pot are
always electrically connected and always presenting low source impedance.
A high-impedance voltage divider tied to the same pin is swamped.

Worked example with BTS7960 IS (~1 kΩ) + 12 V divider (33k/10k, ~7.7 kΩ Thevenin):
```
V_pin ≈ (V_div × R_BTS + V_BTS × R_div) / (R_BTS + R_div)
      ≈ (2.79 × 1k + 0 × 7.7k) / (1k + 7.7k)
      ≈ 0.32 V   →  reported as ~1.4 V (wrong by 10×)
```

### v3 fix path
Add a **74HC4051** 8-channel analog mux on the next PCB revision. Cost ≈ KES 80,
1 IC + 0.1 µF decoupling + 3 GPIOs for select lines. This gives 8 ADC channels
through one ESP32 pin and resolves every collision in this table.

---

## L2 — Single GPIO for top + bottom limit switches

### Symptom
The PCB diode-ORs all eight KW12-3 limit switches into a single
`PIN_LIMIT_ANYHIT` (GPIO 39). Firmware can detect *that* a limit was hit but
not *which* limit electrically.

### v2.1 mitigation
`motor_driver.cpp` discriminates top vs bottom by inspecting
`g_status.deck_position_mm`: if the position is above the deck midpoint when
ANYHIT goes active, treat as TOP_LIMIT_HIT; below midpoint → BOTTOM_LIMIT_HIT.
This works in practice because the bridge cannot be at both ends simultaneously
and the position estimate is accurate to ±3 mm via the deck pot.

### v3 fix path
Either route TOP and BOTTOM through two GPIOs (GPIO 36 + GPIO 39 are both
input-only ADC pins; trade against TOUCH_IRQ which currently owns 36), or
use the same 74HC4051 from L1 to read individual switches sequentially.

---

## L3 — GPIO 39 has no internal pull-up

### Symptom
ESP32 GPIOs 34–39 are input-only and have **no internal pull-up or pull-down**.
`pinMode(PIN_LIMIT_ANYHIT, INPUT_PULLUP)` in motor_driver.cpp silently does
nothing on GPIO 39.

### v2.1 mitigation
External 10 kΩ pull-up resistor to 3.3 V on the PCB at the limit-switch input
node, before the diode-OR network. Verify with a multimeter before first
power-up.

### v3 fix path
Same as v2.1 — the external pull-up is the right answer; just make sure it's
explicitly populated on the BOM (currently R23 in `ConnectorsSafety.kicad_sch`).

---

## L4 — UART0 / US4 GPIO sharing

### Symptom
PIN_US4_TRIG (GPIO 1) and PIN_US4_ECHO (GPIO 3) are also UART0 TX/RX. While
the USB-UART bridge is connected (programming or serial monitor), US4 readings
are corrupted.

### v2.1 mitigation
- Programming: hold BOOT, flash, release. After boot, US4 is functional.
- Serial monitor: dev-only. For demo, disconnect USB-C and rely on touchscreen
  diagnostics, or accept that US4 will report `0xFFFF` (no echo) when USB is
  attached. Direction inference still works from US3 alone in that mode.

### v3 fix path
Move US4 to the GPIO pair freed when L1 is fixed (the 74HC4051 frees up
GPIO 22 / GPIO 23 currently shared with servos and US3).

---

## L5 — GPIO conflicts accepted via time-division

These are **documented in pin_config.h** as intentional design compromises:

| GPIO | Owners | Resolution |
|---|---|---|
| 5 | PIN_SERVO_LEFT + PIN_US1_TRIG | Servos move only when motor idle; ultrasonics off during barrier sweep |
| 21 | PIN_595_OE_N + PIN_US2_ECHO | Sensor task disables ultrasonic ISRs while shifting LED bytes (~100 µs) |
| 22 | PIN_SERVO_RIGHT + PIN_US3_TRIG | Same as GPIO 5 |
| 0 | PIN_BUZZER + PIN_ESTOP_IRQ + BOOT strapping | E-stop has separate hardware path; buzzer is PNP-driven HIGH-idle |
| 1 | PIN_BUZZER (LEDC) + PIN_US4_TRIG + UART0 TX | See L4 |

These are not "limitations" in the sense that they fail — they work — but they
are fragile and should be cleaned up in the v3 PCB respin.

---

## Summary for the lecturer demo

The bridge meets all functional requirements with the v2.1 firmware:
9-state FSM, 4 ultrasonic sensors with direction inference, ESP32-CAM vision,
BTS7960 motor control with closed-loop position, simulated dynamic
counterweights on the HMI, 16-flag fault register with edge-triggered FSM
events, and full safety chain (E-stop relay, watchdog, barrier interlocks).

What the v2.1 firmware does **not** do — and the demo script should not
claim it does:
- Active 12 V / 5 V rail voltage measurement on the dashboard
- Front-panel hardware buttons (touchscreen only)
- Per-switch limit identification (uses position-based inference instead)

The fix for all of these is a single 74HC4051 IC on the v3 board.
