# Vertical Lift Bridge — Control System (Group 7)

**EEE2412 / ECE 4.2 — Microprocessors II Course Project**

A scale-model **vertical lift bridge**: a road bridge whose deck rises vertically
between two towers to let vessels pass underneath, then lowers to restore road
traffic. The system is driven by an **ESP32-WROOM-32** running a real-time,
multi-task FreeRTOS firmware with a safety-first architecture, an LVGL
touchscreen HMI, and a 3D-printed mechanical assembly.

> **Project phase:** Breadboard/simulation is complete. The project is now in the
> **physical assembly phase** (perfboard soldering + 3D-printed enclosure). See
> [`docs/Perfboard_Assembly_Guide.md`](docs/Perfboard_Assembly_Guide.md).

---

## 1. What it does

1. Bridge sits **down**, road open, green light, barriers up (`IDLE`).
2. Operator presses **RAISE** (touchscreen or front-panel button).
3. Barriers close, amber→red lights, counterweight tanks balance (`ROAD_CLEARING`).
4. Motor drives the deck **up** to the top limit (`RAISING` → `RAISED_HOLD`).
5. Vessel passes; operator presses **LOWER**; deck descends to the bottom limit
   (`LOWERING`). Lowering is **blocked** while a vessel is detected approaching.
6. Barriers open, green light, back to `IDLE` (`ROAD_OPENING`).

A hardware **E-stop**, a motor-cutoff **relay**, a **watchdog**, and a 16-flag
**fault register** can interrupt the cycle at any point.

The system currently runs in **manual mode** — the operator initiates raise/lower.
The quad **laser break-beam** array provides vessel-direction sensing that feeds
the marine alert and the "safe to lower" interlock.

---

## 2. Repository layout

```
firmware/          Main ESP32 application (PlatformIO + Arduino framework)
  src/
    main.cpp             Boot, FreeRTOS task creation, the 7 tasks
    system_types.h       SINGLE SOURCE OF TRUTH: states, events, faults, SharedStatus
    pin_config.h         SINGLE SOURCE OF TRUTH: all GPIO assignments
    fsm/                 Table-driven 9-state machine (engine + guards + actions)
    motor/               L298N H-bridge + timer-based deck position + runaway guard
    sensors/             Quad laser break-beam direction inference
    counterweight/       Pure-software water-tank balance simulation
    traffic/             74HC595 road traffic lights + buzzer (buzzer pin disabled)
    hmi/                 LVGL touchscreen display + 5-button resistor-ladder input
    safety/              Interlocks (E-stop/relay/servos), fault register, watchdog
cad/               13 parametric OpenSCAD parts → STL/3MF + print guide
mechanical/        Assembly photos
bom/               Bill of materials + shopping list
docs/              Design docs + perfboard assembly/wiring guide + known limitations
member_guides/     Per-member build & theory guides
Simulations/       SDL2 desktop LVGL mock for presentations
```

---

## 3. Hardware & pin map

Single source of truth: [`firmware/src/pin_config.h`](firmware/src/pin_config.h) (v3.0 map).

| Subsystem | Signal | GPIO | Notes |
|---|---|---|---|
| **Motor (L298N dual H-bridge)** | IN1 (up) | 25 | LEDC ch0, 13-bit, 4 kHz; ENA tied high |
| | IN2 (down) | 26 | LEDC ch1 |
| | Safety relay | 32 | de-energised cuts motor power |
| | ~~Deck-position pot~~ | 35 | **omitted** — timer-based positioning; spare |
| | ~~Limit switches~~ | 39 | **omitted** — timer-based end-stops; spare |
| **TFT ILI9341 (SPI)** | SCK / MOSI / MISO | 14 / 13 / 12 | |
| | CS / DC | 15 / 2 | RST→EN, BL→3V3 in hardware |
| | Touch CS (XPT2046) | 33 | IRQ polled |
| **Laser break-beam ×4** | LDR1–4 | 18 / 19 / 21 / 22 | upstream A/B, downstream A/B |
| **Traffic lights (74HC595)** | DATA / CLK / LATCH | 23 / 4 / 27 | OE→GND, MR→3V3; Q0–Q2 road (marine not implemented) |
| **Servos (SG90 ×2)** | shared PWM | 16 | both barriers driven by one channel (mirror) |
| **Buzzer** | — | −1 (disabled) | freed to preserve UART TX0 |
| **Operator panel** | 5-button R-ladder | 34 | ADC1_CH6 |
| **E-stop** | NC mushroom button | 36 | input-only, ISR on CHANGE |
| **Spare** | — | 17 | unused (vision module omitted) |

**Power:** 12 V barrel jack → LM2596 buck → 5 V rail → AMS1117 → 3.3 V rail.
Build the power planes and verify rails with a multimeter **before** connecting
any logic — see the perfboard guide.

---

## 4. Firmware architecture

Seven FreeRTOS tasks share one mutex-guarded status struct (`g_status`) and three
queues (events, motor commands, HMI commands).

| Task | Core | Prio | Rate | Responsibility |
|---|---|---|---|---|
| `task_safety` | 0 | 5 | 20 Hz | interlocks, fault eval, watchdog check, panel scan; lights+buzzer @ 10 Hz |
| `task_fsm` | 0 | 4 | event + 100 ms tick | runs the state machine |
| `task_motor` | 0 | 4 | ~50 Hz | applies motor cmds, reads pot, stall detect, limit events |
| `task_sensors` | 0 | 3 | 20 Hz | laser sampling + direction inference |
| `task_counterweight` | 0 | 2 | 20 Hz | water-tank fill/drain simulation |
| `task_telemetry` | 0 | 1 | 1 Hz | uptime |
| `task_hmi` | 1 | 2 | ~20 Hz | LVGL tick/flush/refresh, operator-command handling |

Core 0 hosts safety + control; Core 1 is reserved for the LVGL HMI so display
work cannot stall the safety loop. A hardware Task Watchdog Timer (5 s) backstops
a software per-task watchdog (1.5 s hang threshold → `interlocks_force_safe()`).

### State machine

`IDLE → ROAD_CLEARING → RAISING → RAISED_HOLD → LOWERING → ROAD_OPENING → IDLE`,
plus `FAULT`, `ESTOP`, and a boot-time `INIT`. `EVT_ESTOP_PRESSED` and
`EVT_FAULT_RAISED` are evaluated before all normal transitions. See
[`fsm/fsm_engine.cpp`](firmware/src/fsm/fsm_engine.cpp).

---

## 5. Build & flash

Requires [PlatformIO](https://platformio.org/).

```bash
cd firmware
pio run -e vlb_main            # compile
pio run -e vlb_main -t upload  # flash (hold BOOT if needed)
pio device monitor -b 115200   # serial logs
```

Optional ESP32-CAM companion (separate board):

```bash
cd firmware_cam
pio run -t upload
```

Key build config (`firmware/platformio.ini`): `espressif32@6.7.0`, `esp32dev`,
240 MHz, `huge_app.csv` partition, C++17. Libraries: TFT_eSPI 2.5.43, LVGL 9.2.2,
ArduinoJson 7.2.1. Last verified build: **RAM 33.2 %, Flash 19.3 %**.

---

## 6. Operator controls

- **Touchscreen (HOME / OPS / CW / SET tabs):** RAISE / LOWER / HOLD buttons,
  deck-height bar, marine sensor LEDs, traffic-light stack, motor PWM gauge,
  counterweight tank bars, settings (auto-mode, dark-mode, brightness). Tap the
  blue header bar for a deck-height / motor-PWM popup.
- **5-button resistor-ladder panel (GPIO 34):** RAISE, LOWER, HOLD, CLEAR-FAULT,
  NEXT-SCREEN — a defence-in-depth path that works even if the touch panel cracks.
- **E-stop mushroom button:** hardware-latched; also cuts the motor relay.

---

## 7. Known limitations

The firmware ships with honest, documented trade-offs forced by the ESP32 GPIO
budget and the BOM: no motor current sense on the L298N, no rail-voltage
telemetry, **no position pot or limit switches** (deck height is timer-estimated
and re-synced to the end-stops each traverse), open-loop barriers, and a shared
servo pin. The vision module is omitted from this build. Each trade-off is
recorded in [`docs/known_limitations.md`](docs/known_limitations.md).

---

## 8. Team & module ownership

| Member | Area | Primary modules |
|---|---|---|
| **M1 — George** | System & FSM | `main.cpp`, `fsm/` |
| **M2 — Eugene** | Mechanism | `motor/`, `counterweight/` |
| **M3 — Cindy** | Sensing | `sensors/laser` |
| **M4 — Abigael** | Traffic & HMI | `traffic/`, `hmi/` |
| **M5 — Ian** | Power & Safety | `safety/`, PCB, power |

Per-member guides are in [`member_guides/`](member_guides/).
