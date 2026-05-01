# EEE 2412 — Microprocessors II
# REVISED PROJECT PROPOSAL — Group 5 — JKUAT
## Automatic Vertical Lift Bridge Control System
### v2.0 — April 2026 (replaces v1.0 dated April 2026)

| Member | Adm. No. | Role |
|---|---|---|
| Ochiere George John | ENE212-0218/2021 | M1 — Firmware Lead & FSM |
| Mugaisi Eugene Muruli | ENE212-0083/2020 | M2 — Bridge Mechanism & Motor |
| Koech Cindy Chebet | ENE212-0069/2020 | M3 — Vision & Detection |
| Kirui Abigael Cherotich | ENE212-0070/2020 | M4 — Traffic, HMI Dashboard & Alarm |
| Kiogora Ian Mwenda | ENE212-0251/2021 | M5 — PCB, Power & Safety |

Lecturer: Dr. P. K. Kihato

---

## 1. EXECUTIVE SUMMARY

This proposal presents the **Automatic Vertical Lift Bridge Control System**, a 1.2 m × 0.6 m PLA-printed scale model bridge that demonstrates every learning outcome of EEE 2412 (Microprocessors II) at JKUAT. The bridge is automatic and unattended: a colour camera at the road approach watches for vehicles, and a pair of dual-beam ultrasonic sensors guards each end of the waterway and tells the controller when a vessel is approaching. The ESP32-WROOM-32 microcontroller runs FreeRTOS with seven priority-scheduled tasks across its two CPU cores, executes a 10-state finite state machine (FSM) with full safety interlocks, drives a single 12 V geared DC motor through a DRV8871 H-bridge (sense-resistor over-current protection), and presents the entire system state on a 2.8-inch ILI9341 TFT touchscreen running a bespoke LVGL dashboard.

Mechanically, the model is a **vertical lift bridge with dual counterweights**. A single JGA25-370 geared DC motor turns a Ø30 mm cable drum at the head of the left tower; a 1 mm braided steel cable runs from the drum, over twin guide pulleys at the top of each tower, and connects to two 120 g counterweights that fall as the deck rises. The counterweights balance the deck mass, so the motor must overcome only friction and acceleration — a single motor is sufficient at this 1:25 model scale; at full scale two motors with electronic synchronisation would be specified, and this is documented in the design log. The system replaces all infrared deck-clearance sensing from the v1.0 proposal with a **camera-based detection module** built on an ESP32-CAM (OV2640 + ESP32-S board) running an OpenCV-equivalent embedded vision pipeline that streams JPEG frames over UART to the main controller and reports vehicle presence, approach direction, and detection confidence on the LVGL dashboard. Every component on the bill of materials is sourced locally — Nerokas Enterprises, Pixel Electric, ASK Electronics, K-Technics, and the Luthuli/Sagana electronics market — with confirmed prices in Kenyan Shillings; no item depends on international shipping. The total project cost is **KES 22,184** (≈ KES 4,437 per member). All design files (KiCad PCB, OpenSCAD CAD, firmware source, LVGL screen mockups) are version-controlled at github.com/kiogimwenda/vertical-lift-bridge and form an integrated portfolio that maps directly onto the EEE 2412 course outline.

---

## 2. PROBLEM STATEMENT

Movable bridges sit on critical infrastructure corridors at every navigable river crossing. Across Kenya, the majority of such crossings still rely on manual operation, which introduces three recurring failure modes that this project addresses:

- **(i) Safety risk from human error.** Manually operated bridges have no automatic interlock to prove the deck is clear of vehicles before raising; a single inattentive operator can strand a car on a moving deck.
- **(ii) Synchronisation failure on multi-point lifts.** A vertical lift bridge's deck rides on two or more lift points. If those points move at different rates the deck tilts; even a 15 mm differential at model scale jams the deck against its guide rails.
- **(iii) Inability to determine traffic and vessel intent.** Single-sensor systems cannot distinguish an approaching vessel from a departing one, nor a stationary deck-clear condition from a vehicle merely paused on the deck. They also waste cycles by raising for departing traffic.

This project addresses all three through full automation: a mechanical synchronisation loop that physically cannot desynchronise (single motor → cable → pulleys → counterweights), a dual-beam directional ultrasonic detector at each waterway end, a frame-differencing computer-vision deck-clearance module that distinguishes a moving vehicle from a static object and reports a confidence score, and a watchdog/interlock chain that vetoes any deck movement until every safety predicate is true.

---

## 3. SYSTEM OVERVIEW (block diagram)

```
                        ┌─────────────────────────────────────┐
                        │    Main MCU — ESP32-WROOM-32        │
                        │    (FreeRTOS, 7 tasks, dual core)   │
                        │                                     │
   ULTRASONICS ─SPI/───►│  • FSM Engine                       │
   4 × HC-SR04          │  • Motor Control (LEDC PWM)         │◄──┐
                        │  • Sensor Poll                      │   │
   E-STOP ───IRQ───────►│  • Safety Watchdog                  │   │
   8× LIMITS ─IRQ──────►│  • Traffic Control (74HC595)        │   │
   POT (deck pos) ─ADC─►│  • HMI Input + LVGL Display         │   │
                        └──┬─────────────┬──────────────┬─────┘   │
                           │ SPI         │ UART2 (115k2)│ I2C(opt)│ DRV8871
                           ▼             ▼              ▼         ▼
                    ┌──────────┐  ┌────────────┐    ┌──────┐  ┌──────┐
                    │  ILI9341 │  │ ESP32-CAM  │    │  …   │  │MOTOR │
                    │  2.8" + XPT2046 touch    │    │      │  │+drum │
                    │  240×320 IPS             │    │      │  │+cable│
                    │  LVGL 9.2.2              │    │      │  └──┬───┘
                    └──────────┘   OV2640 cam  │              ▼
                                   OpenCV-ish  │           ┌──────────┐
                                   pipeline    │           │ DECK     │
                                   reports JSON│           │ +2× CW   │
                                   over UART   │           │ +2 towers│
                                   ┌──────────┐│           └──────────┘
                                   │   road   │
                                   │  approach│
                                   │  camera  │
                                   └──────────┘
```

The single PCB carries the ESP32-WROOM, motor driver, shift register, USB-UART, regulators, safety relay, and all JST-XH connectors. The ESP32-CAM is mounted on a 3D-printed gantry overhead of the road approach and connects via a 4-wire ribbon (5 V, GND, RX, TX) to the main PCB.

---

## 4. DESIGN-DECISION LOG (changes from v1.0)

| # | Decision | v1.0 (original) | v2.0 (revised) | Rationale |
|---|---|---|---|---|
| D1 | Vehicle-on-deck detection | 2× FC-51 IR sensors | **ESP32-CAM (OV2640) computer-vision module** | IR triggers on any reflective surface, including water spray, leaf litter, and the road barrier itself. A camera with frame-differencing detects motion, suppresses static-object false-positives, can report direction (approach vs. depart), and demonstrates an embedded vision pipeline (course outcome §2 Sensors and §8 Applications). |
| D2 | Lift mechanism | 4-point lead-screw + GT2 belt loop | **Dual-counterweight, single motor, cable + 4 pulleys, 2 towers** | The counterweight balances 100% of static deck mass, reducing required motor torque ~10×; eliminates 4 lead screws, 4 pulleys, ~1 m of GT2 belt and 8 trapezoidal nuts; halves print time and cost; mechanically guarantees synchronised lift because both counterweights and the deck are tied to one cable. |
| D3 | Number of towers | 4 (one per deck corner) | **2 (one per riverbank)** | Counterweight rides inside the tower; deck is rigid enough at 200×120 mm not to need 4-corner support, only edge support from linear guides. |
| D4 | Footprint | 900 × 600 mm | **1200 × 600 mm** | Adds 150 mm of road approach on each side for camera FOV and sensible vehicle queue length. |
| D5 | Bridge deck material | 5 mm acrylic | **PLA, FDM printed** | Brings deck into the same FDM workflow as the rest of the parts; keeps tolerances consistent with mating brackets. |
| D6 | HMI framework | TFT_eSPI direct draws | **LVGL 9.2.2 over TFT_eSPI driver** | LVGL gives anti-aliased widgets, animations, themes, touch input, screen manager, and is the industry-standard embedded GUI in 2026; required for a "bespoke dashboard". |
| D7 | HMI input | 4 pushbuttons only | **XPT2046 resistive touch + pushbuttons retained for E-stop / Mode** | Touch enables on-screen navigation across 5 screens; pushbuttons remain as the "always works even if touch panel cracks" emergency input — a defence-in-depth pattern. |
| D8 | Custom font | LVGL Montserrat default | **Custom LVGL font generated from Inter at 12/16/24/36 px** | Default fonts on a black panel look generic; Inter is open-licensed and has both a Display cut for the 36 px hero metric and a Body cut for 16 px labels. |
| D9 | Group number | "Group 7" (README typo) | **Group 5** (matches submitted proposal) | Correction. |

### 4.1 Counterweight sizing calculation

```
Bridge deck                    PLA volume 200×120×6 mm × 0.30 infill = 43.2 mL
Deck mass (PLA, ρ=1.24 g/mL)   = 53.6 g  (printed shell + 30% gyroid)
Add 5 mm guide-rail brackets   + 7 g
Effective moving mass m_d      ≈ 60 g per side  (deck mass / 2 sides)
                                 — but practical engineering ratio uses
                                 ~110–120% of half deck mass to also pre-load
                                 cable and guide friction
Counterweight target           120 g per side  (2 sides → 240 g total)

Counterweight construction     Hollow PLA shell 60×40×30 mm + 2× M8 hex nut (8 g each)
                               + lead fishing weights to make 120 g exactly,
                               trim-tuned at assembly.
```

### 4.2 Motor torque budget (with counterweights)

```
Cable drum diameter             D_drum  = 30 mm   ⇒ r = 0.015 m
Cable speed at v_lift = 8 mm/s  → drum ω = v/r = 0.533 rad/s = 5.10 RPM   ✓ (motor 60 RPM, ample headroom)
Static unbalance (deck > CW)    = 0  (perfectly tuned at build)
Friction estimate (cable+guide) = 0.25 N
Required torque T               = F·r = 0.25 × 0.015 = 3.75 mN·m  ≈ 0.038 kgf·cm
JGA25-370 stall torque          = 1.7 kgf·cm  → safety factor ≈ 45×    ✓
```

### 4.3 Print-split strategy (build volume 220×220×250 mm)

| Part | Raw size | Splits | Joint |
|---|---|---|---|
| Tower (each) | 90 × 60 × 350 mm | **2 sections** (175 mm each) | 4× M3 + dovetail tongue (10 mm) |
| Base plate | 1200 × 600 × 6 mm | **6 panels** 200×600 each, 2 long edges with M3 inserts | Tongue-and-groove + 2× M3 |
| Bridge deck | 200 × 120 × 6 mm | none (fits build volume) | n/a |
| Counterweight | 60 × 40 × 60 mm | none | n/a |
| Pulley wheel | Ø50 × 20 mm | none | n/a |
| Motor mount | 60 × 50 × 30 mm | none | n/a |
| Cable guide | 40 × 30 × 25 mm | none | n/a |
| Road barrier (servo arm) | 80 × 15 × 8 mm | none | n/a |
| PCB enclosure | 110 × 90 × 30 mm | **2 halves** (top + bottom) | 4× M3 self-tap |
| Camera mount | 80 × 60 × 100 mm | none | n/a |
| TFT display mount | 100 × 80 × 90 mm (35° tilt) | none | n/a |

### 4.4 TFT display selection (full evaluation)

| Criterion | ILI9341 (chosen) | ST7789 | ILI9488 | RA8875 |
|---|---|---|---|---|
| Resolution | **240 × 320** ✓ adequate for 5-screen HMI at 16-pt label size | 240×240 — too small for telemetry tables | 320×480 — ideal for richer dashboards but pricier | 800×480 — overkill; needs parallel bus |
| Colour depth | 16-bit RGB565 ✓ | 16-bit ✓ | 18/24-bit | 16-bit |
| Panel tech | TN on common 2.8" modules; **IPS available at +KES 600** | IPS standard | TN | TN |
| SPI clock that ESP32 sustains | **40 MHz half-duplex DMA** ✓ | 40 MHz | 40 MHz works but bandwidth-tight at 320×480 | n/a — parallel only |
| Touch controller | **XPT2046 resistive integrated** ✓ | usually none | XPT2046 on some | FT5x06 capacitive |
| LVGL driver | **lvgl/tft_espi v1.x in IDF registry** ✓ | yes | yes | yes (heavier) |
| Local availability | **Nerokas, Pixel — KES 1,200 (TN) / 1,800 (IPS)** ✓ | Nerokas KES 950 | Nerokas KES 2,500 | not stocked locally |
| Repo legacy | already locked (L4) | — | — | — |

**Selected: ILI9341 2.8" SPI module, IPS variant, with XPT2046 resistive touch.** All six criteria met. Repo legacy preserved.

---

## 5. SCOPE BOUNDARY

| In scope | Out of scope |
|---|---|
| Detection of one (1) vehicle approaching the bridge by camera | Multi-vehicle tracking, license plate recognition |
| Detection of vessel approach direction by ultrasonics | Vessel speed estimation, vessel size classification |
| 2-tower vertical lift, 180 mm deck travel | Bascule / swing / retractable bridge variants |
| AUTO and MANUAL operating modes | Remote / Wi-Fi operation (MQTT will be wired up but optional) |
| 5-screen LVGL dashboard with touch + encoder fallback (encoder is unused but supported in code) | Multilingual UI |
| Emergency stop with hardware relay cut-off | UPS / battery backup |
| Single-motor demo at model scale | Real-scale dual-motor electronic synchronisation (documented for completeness, not built) |

---

## 6. COURSE OUTCOME ALIGNMENT TABLE  (EEE 2412 outline mapping)

| Course-outline topic | Where it shows up in this project | Member |
|---|---|---|
| **1. Interfacing — Parallel (Intel 8255 PPI handshake modes)** | The 74HC595 acts as our I/O expander; we explicitly draw the analogy in the report and code-comment the SH/CP/STCP "handshake" against PPI mode-0 strobed output. We also implement an SR-style poll across the operator pushbutton matrix. | M4 |
| **1. Interfacing — DMA devices** | LVGL's TFT_eSPI flush callback uses ESP32 SPI DMA channels; we instrument and report effective DMA throughput. | M4 (HMI), M5 audits |
| **1. Interfacing — Serial Communication devices** | UART0 → CP2102N → USB-C for PC; UART2 → ESP32-CAM at 115 200 baud; SPI to TFT and XPT2046; I²C reserved for future expansion. | M5 (PCB), M3 (vision) |
| **1. Interfacing — Timers** | LEDC PWM for motor and backlight; high-resolution `esp_timer` for FreeRTOS tick into LVGL via `lv_tick_inc`; HC-SR04 echo timing using `gpio_set_intr` rising/falling edge ISRs. | M2, M3 |
| **2. Sensors and Actuators** | HC-SR04 ultrasonics, KW12-3 limit switches, OV2640 camera, potentiometer for deck position (ADC), servos (SG90), LEDs, buzzer, DC motor, relay. Full datasheet integration. | M3, M2, M4, M5 |
| **3. Memory Mapping (Linear / Fully Decoded)** | The ESP32 memory map (IRAM, DRAM, SPIRAM optional, flash) is documented; LVGL is configured for partial-buffer rendering in DRAM; vision frame buffer in PSRAM on the ESP32-CAM (when fitted). The 74HC595 chain illustrates linear decoding. | M5 (system memory map), M3 (PSRAM) |
| **4. Interrupting sequences — Operations / Devices / Polling / IDT** | E-stop and limit switches are level-triggered ISRs (Cortex-NVIC vector table is the IDT analogue); ultrasonics use edge ISRs; FSM event queue is polled at 10 ms; timer ISR for watchdog feeding. We tabulate ISR latency vs. polling latency in the report. | M5 (ISRs), M1 (poll) |
| **5. Program Structure — Modularization, PIC, Parameter passing, Subroutines** | The firmware is module-per-directory (`fsm/`, `motor/`, `sensors/`, …). PIC = position-independent C/C++ compiled by xtensa-esp32-elf-gcc. All inter-task communication uses queues with typed parameters (`MotorCommand_t`, `SystemEvent_t`). | M1 |
| **6. Software Engineering — Assemblers, link loaders, editors / Top-down, Bottom-up / Structured prog** | We use PlatformIO + GCC + ld; we trace the build log to show .text/.data/.bss; the FSM is *top-down* designed; the driver layer is *bottom-up*; structured-programming is enforced by clang-format and a pre-commit hook. Debugging via GDB-stub over JTAG is shown in M5's safety chapter. | M1, M5 |
| **7. Microprocessor Testing** | One unit-test sketch per module (`test_fault_register.cpp`, `test_motor_driver.cpp`, …) — Unity-style assertions, output via Serial Monitor at 115 200 baud, captured in the final report. | All members |
| **8. Software & Microprocessor Applications** | The whole project. Live demo to lecturer in Week 8. | All |

---

## 7. PROJECT TIMELINE  (8-week plan, updated)

| Wk | All members do | M1 | M2 | M3 | M4 | M5 |
|---|---|---|---|---|---|---|
| 1 | Component sourcing | `pin_config.h`, `system_types.h`, FSM skeleton | Tower SCAD + start prints | Order ESP32-CAM, breadboard test | LVGL "Hello" on bench TFT | KiCad → Gerbers → JLCPCB order |
| 2 | Mid-week sync meeting | FSM transition table | Counterweights, drum, deck print | Camera capture + frame diff prototype | Custom font + theme | PCB arrives, BOM finalised |
| 3 | All print parts complete | Module headers published | Mechanical assembly: towers, deck, cable | Vision-over-UART JSON protocol | LVGL screens 1, 2 mocked-up live | Solder PCB; bring-up |
| 4 | Module unit tests pass | Integrate motor task | Calibrate potentiometer; first lift | Detection accuracy test on 30 toy cars | Screens 3, 4, 5 implemented | `watchdog`, `fault_register`, `interlocks` |
| 5 | Hardware integration | Integrate sensors task | Belt tension, end-stop tuning | Day/night robustness test | Touch zones + animations | E-stop relay test |
| 6 | Full system integration | FSM walk-through with safety | Mechanical sign-off | Final vision tuning | Dashboard freeze | Power-rail audit |
| 7 | Reliability runs | 20 consecutive auto cycles | Re-tune anything that wears | Edge-case logging | UI polish | Generate final BOM with receipts |
| 8 | Demo + report + slides | Compile final report | Demo script run-through | Demo-day vision script | Demo-day UI script | Final assembly photos, tag release |

---

## 8. RISK REGISTER

| # | Risk | Mitigation |
|---|---|---|
| R1 | ESP32-CAM unavailable on the day | Two confirmed Nairobi suppliers (Nerokas + Pixel). If both stock-out, fallback to OV7670 module on a separate ESP32 dev board (also stocked at Nerokas, KES 1,500). |
| R2 | TFT module sold without IPS panel | Accept TN variant (KES 1,200) and tune backlight + viewing angle in mount; document as a minor cosmetic compromise. |
| R3 | Motor torque insufficient under counterweight imbalance | Counterweights are trim-tuned at assembly; safety margin is 45× so even gross mistuning works. |
| R4 | LVGL frame-rate drops below 30 FPS | Reduce active animations on telemetry screen; switch to tile renderer; we have the 36-pt font compiled both anti-aliased and 1-bit fast. |
| R5 | PCB arrives with manufacturing defect | Order at end of Week 1 → 18-day buffer; M5 has soldering jig fallback to repair vias. |
| R6 | Print failure on tower section | Modular split-print → reprint only the failed segment; .scad is parametric so dimensions can be tweaked in 30 s. |

---

## 9. EXPECTED OUTCOMES / DELIVERABLES

By Week 8 the group hands in: (a) the working scale model that performs 20 consecutive automatic cycles without error, (b) a 30-page technical report, (c) the bound source code on USB stick + GitHub, (d) the LVGL screen recordings, (e) the final bill of materials with receipts, (f) the live-demo script, and (g) a 12-minute demonstration to Dr. P. K. Kihato.

---
*End of revised proposal. Signed: M1, M2, M3, M4, M5. April 2026.*
