# Known Limitations — VLB v2.2

This document records design constraints that ship with the v2.2 hardware
and the v3 fix path for each. None prevent the project from meeting its
EEE 2412 demo requirements; they are honest engineering trade-offs forced
by the ESP32-WROOM-32E GPIO budget and the components specified in the
final BOM.

History note: v2.1 removed rail monitoring and the operator-panel
resistor ladder because both shared GPIO 34 with the BTS7960 IS pin
(impedance collision). v2.2 migrates the motor driver to the L293L
module per the final BOM — the L293L has no current-sense output, so
GPIO 34 is freed and the resistor ladder is restored. Rail monitoring
remains disabled because GPIO 35 is still occupied by the deck-position
pot.

---

## L1 — No rail-voltage sensing

### Symptom
The 12 V and 5 V rails are not measured by firmware. `g_status.rail_*_volts`
fields stay at the `-1.0f` "not measured" sentinel and `FAULT_UNDERVOLT_12V`
is reserved-but-never-set.

### Why
Every ADC pin available to the ESP32-WROOM-32E is occupied by another
peripheral that presents a low source impedance:

| GPIO | Owner | Source impedance | Why we can't share |
|---|---|---|---|
| 32 | (relay drive output) | (output) | not an input |
| 33 | TOUCH_CS (XPT2046) | (output) | not an input |
| 34 | BTN_LADDER (5-button R-ladder) | ~1 kΩ | dominates over a 7.7 kΩ Thevenin divider |
| 35 | DECK_POSITION pot wiper | ~5 kΩ | same — wiper potential always pulled to slider position |
| 36 | TOUCH_IRQ | (input-only, pen-down level) | not free |
| 39 | LIMIT_ANYHIT (diode-OR) | (input-only, switch state) | not free |

A high-impedance voltage divider on any of these pins would be swamped
by the existing source. Software-only "FSM-state multiplexing" cannot
fix it — the impedance collision is present whether we read or not.

### Mitigation in v2.2
Brownout protection is layered in hardware only:
- **ESP32 internal brownout detector** — trips at ~2.43 V on the 3.3 V rail and resets.
- **L293L thermal-shutdown** — built-in over-temperature cutout on the H-bridge silicon.
- **LM1084 module current/thermal limit** — protects the 5 V rail from sustained overload.

### v3 fix path
Add a **74HC4051** 8-channel analog mux on the next PCB revision. Cost ≈ KES 80,
1 IC + 0.1 µF decoupling + 3 GPIOs for select lines. This gives 8 ADC
channels through one ESP32 pin and resolves every collision in this
table — restores rail sense AND any future analog peripheral.

---

## L2 — Single GPIO for top + bottom limit switches

### Symptom
The PCB diode-ORs all four KW11-3Z limit switches into a single
`PIN_LIMIT_ANYHIT` (GPIO 39). Firmware can detect *that* a limit was hit
but not directly *which* limit electrically.

### v2.2 mitigation
`motor_driver.cpp::motor_driver_tick()` discriminates top vs bottom by
reading `pos_mm` (derived from the deck-position pot) at the moment ANYHIT
goes active: above `DECK_HEIGHT_MAX_MM / 2` → `EVT_TOP_LIMIT_HIT`,
otherwise → `EVT_BOTTOM_LIMIT_HIT`. The deck cannot physically be at both
ends simultaneously, and the pot is accurate to ±3 mm — comfortably
inside the half-deck-height (87 mm) margin of error for the
discriminator.

The earlier v2.1 firmware noted this discrimination as design intent but
did not actually implement it; v2.2 closes that gap.

### v3 fix path
Either route TOP and BOTTOM through two separate GPIOs (would consume
the GPIO 36 / 39 pair), or use the same 74HC4051 from L1 to read
individual switches sequentially.

---

## L3 — GPIO 39 has no internal pull-up

### Symptom
ESP32 GPIOs 34–39 are input-only and have **no internal pull-up or pull-down**.
Setting `pinMode(PIN_LIMIT_ANYHIT, INPUT)` requires an external pull-up.

### v2.2 mitigation
External 10 kΩ pull-up resistor to +3.3 V on the PCB at the limit-switch
input node, before the diode-OR network. Verified in
`ConnectorsSafety.kicad_sch` as `R_pu_39` and listed in the BOM under
"Resistor ladder kit" (item 18). Verify with a multimeter before first
power-up.

### v3 fix path
Same as v2.2 — the external pull-up is the right answer; just make sure
it's explicitly populated.

---

## L4 — UART0 / US4 GPIO sharing

### Symptom
`PIN_US4_TRIG` (GPIO 1) and `PIN_US4_ECHO` (GPIO 3) are also UART0 TX/RX.
While the USB-UART bridge (CH340G or the DevKit's onboard CP2102) is
connected for programming or serial monitor, US4 readings are corrupted.

### v2.2 mitigation
- Programming: hold BOOT, flash, release. After boot, US4 is functional.
- Serial monitor: dev-only. For the demo, disconnect USB-C and rely on
  the touchscreen + resistor-ladder panel for diagnostics, or accept
  that US4 will report `0xFFFF` (no echo) when USB is attached.
  Direction inference from the downstream pair still works on US3 alone
  in that mode.

### v3 fix path
Move US4 to the GPIO pair freed when L1 is fixed (the 74HC4051 frees
GPIO pins currently shared with servos and US3).

---

## L5 — GPIO conflicts accepted via time-division

These are **documented in `pin_config.h`** as intentional design compromises:

| GPIO | Owners | Resolution |
|---|---|---|
| 5 | PIN_SERVO_LEFT + PIN_US1_TRIG | Servos move only when motor idle; ultrasonics off during barrier sweep |
| 18 | PIN_595_CLOCK + PIN_US1_ECHO | DATA/CLOCK toggle to INPUT_PULLUP between LED shifts (see traffic_lights.cpp) — US1 echo readable most of the time |
| 19 | PIN_595_LATCH + PIN_US2_TRIG | Both drivers want OUTPUT — LATCH stays OUTPUT; trigger pulses overlap rarely (sub-µs LATCH vs 10 µs TRIG) |
| 21 | PIN_595_OE_N + PIN_US2_ECHO | OE must hold OUTPUT LOW; cannot share — US2 ECHO permanently dead in v2.2 (see L8) |
| 22 | PIN_SERVO_RIGHT + PIN_US3_TRIG | Servo PWM owns the pin once attached; US3 trigger pulses are absorbed (see L8) |
| 23 | PIN_595_DATA + PIN_US3_ECHO | Same toggle scheme as GPIO 18 — US3 echo readable most of the time |
| 0  | PIN_ESTOP_IRQ + BOOT strapping | E-stop ISR is OK because BOOT uses a separate strap circuit; mushroom button cuts hardware relay independently |
| 1  | PIN_BUZZER (LEDC) + PIN_US4_TRIG + UART0 TX | See L4 — buzzer + US4 TRIG + serial conflict, time-division managed manually |

These are not "limitations" in the sense that they fail — they work — but
they are fragile and should be cleaned up in the v3 PCB respin.

---

## L6 — No motor current sensing (new in v2.2)

### Symptom
The L293L module specified in the final BOM has no current-sense
output pin. `g_status.motor_current_ma` is therefore always 0 in v2.2,
and `FAULT_OVERCURRENT` is reserved-but-never-set.

### Why
Migrating from the original BTS7960 (which exposes an IS pin scaled at
~8.5 mV/A) to the L293L right-sized the H-bridge to the actual ~600 mA
load and freed GPIO 34 for the operator-panel resistor ladder (cf. L1).
The L293L's current-sense pins (SENS1/SENS2) are not broken out on the
common module the BOM specifies.

### v2.2 mitigation
Motor abuse is caught by two independent paths:
- **Firmware:** `FAULT_STALL` (no position change while energised for
  `MOTOR_STALL_TIMEOUT_MS` = 2 s).
- **Hardware:** the L293L's internal thermal-shutdown trips at silicon
  ~150 °C and self-recovers when temperature drops.

### v3 fix path
Either swap the motor driver back to a sense-equipped part (DRV8871,
BTS7960, or an L298N module with the 0.1 Ω SENS resistors broken out
to a header) or add a low-side shunt + INA169 amplifier on the v3 PCB
fed through the 74HC4051 from L1.

---

## L7 — Barrier-stuck cannot be detected in v2.2

### Symptom
`FAULT_BARRIER_TIMEOUT` is reserved-but-never-set. The firmware reports
the barriers reach their target after a fixed 800 ms time estimate,
regardless of whether the servo physically moved.

### Why
SG90 hobby servos provide no position-feedback signal. The firmware's
`interlocks_evaluate()` therefore uses an open-loop time estimator
(`barrier_age > BARRIER_REACHED_MS = 800 ms` ⇒ "done"). A jammed
barrier still draws PWM pulses but does not move; the firmware cannot
tell the difference.

### v2.2 mitigation
Operator visual inspection during the demo. The HMI MAIN screen
should display the requested barrier angle and the operator can
confirm the physical barrier matches.

### v3 fix path
Add a KW11-3Z microswitch under each barrier arm wired through the
diode-OR network on `PIN_LIMIT_ANYHIT` (or a dedicated GPIO once the
74HC4051 frees pins per L1). Update `interlocks_evaluate()` to set
`barrier_done` from the switch reading, then the existing
`BARRIER_TIMEOUT_MS` watchdog becomes effective.

---

## L8 — Two HC-SR04 sensors degraded by 74HC595 + servo pin-sharing

### Symptom
Of the four HC-SR04 modules in v2.2:
- **US1 (upstream Beam A)** — ECHO pad on GPIO 18 is shared with the
  74HC595 CLOCK; my time-multiplex fix in `traffic_lights.cpp::shift_out()`
  releases this pad to `INPUT_PULLUP` between LED updates, so the
  echo line is readable in principle. **However**, US1's TRIG pad
  (GPIO 5) is also the LEDC PWM output for `PIN_SERVO_LEFT`. Once
  `interlocks_init` calls `s_servo_l.attach()`, the GPIO matrix routes
  the LEDC signal to the pad permanently and `sensors_ultrasonic_tick()`'s
  `digitalWrite(PIN_US1_TRIG, HIGH/LOW)` calls do not reach the SR04.
  No trigger out → no echo back → distance reads `0xFFFF`.
- **US2 (upstream Beam B)** — ECHO permanently dead. GPIO 21 hosts the
  74HC595 OE pin which must stay a permanent `OUTPUT` (driven LOW =
  LED chain enabled). Any shared `INPUT_PULLUP` toggling here would
  extinguish the LEDs.
- **US3 (downstream Beam A)** — same TRIG issue as US1 (GPIO 22 owned
  by `PIN_SERVO_RIGHT` after `s_servo_r.attach()`). ECHO pad on GPIO 23
  is releasable but receives no echoes because no trigger is sent.
- **US4 (downstream Beam B)** — fully functional whenever the USB
  cable is unplugged (TRIG/ECHO on GPIO 1 / GPIO 3 share UART0 — see L4).

**Net effect in v2.2: only US4 delivers ranging data**, and only when
USB is unplugged. The HC-SR04 array is effectively a single-beam
proximity sensor on the downstream side. Direction inference is
unavailable.

### v2.2 mitigation
- Vehicle detection still works through **vision** (the ESP32-CAM on
  the dedicated UART2 channel — completely independent of the L8
  failure path).
- Vessel detection still works through **US4** when USB is unplugged.
  Even though direction inference is unavailable, mere presence is
  enough to trigger the FSM's `EVT_VEHICLE_DETECTED` (the FSM ORs
  vision and ultrasonic).
- For the demo: rely on vision as the primary path. Use US4 as a
  binary "vessel-in-zone" backup. Document direction inference as a
  v3 feature.

### v3 fix path
Move the 74HC595 control lines off the ultrasonic pins entirely. The
cleanest approach is to pair the 74HC595 with the v3-PCB 74HC4051
analog mux suggested in L1 — both can sit on a small dedicated I/O
sub-bus driven from GPIO pins freed by the mux.

---

## Summary for the lecturer demo

The bridge meets all functional requirements with the v2.2 firmware:
9-state FSM, ESP32-CAM vision (primary vehicle detection), ultrasonic
vessel detection (4 sensors fitted; per L8 only US4 actually delivers
ranging data in v2.2 — sufficient for binary presence but not direction),
L293L motor control with closed-loop position from the deck
pot, simulated dynamic counterweights on the HMI, 16-flag fault
register with edge-triggered FSM events, full safety chain (E-stop
relay through ULN2803, watchdog, barrier interlocks), and
defence-in-depth operator input (touchscreen + 5-button resistor
ladder).

What the v2.2 firmware does **not** do — and the demo script should not
claim it does:
- Active 12 V / 5 V rail voltage measurement on the dashboard (L1)
- Direct top-vs-bottom limit identification at the GPIO level (L2 — but
  position-based discrimination is implemented; it just needs the deck
  pot to be calibrated)
- Motor current measurement / `FAULT_OVERCURRENT` (L6)
- Servo barrier stall detection / `FAULT_BARRIER_TIMEOUT` (L7)
- Within-pair direction inference for vessels (L8 — US2 ECHO dead, US1
  delivers presence only)

The fix for L1, L2, L6, L8 (and free a path for L4) is a single 74HC4051
IC on the v3 board. L7 needs barrier-arm microswitches.
