# [5] ELECTRONICS DESIGN — Full Specification (v2.2)

> **Component sources.** Every part below is taken from the final BOM
> (`bom/VLB_Group7_BOM_Final.pdf`). When this document and the BOM
> disagree, the BOM wins.

## 5.1 System block diagram (text-structured, exact bus widths)

```
                            +12 V DC INPUT (5.5×2.1 mm jack)
                                       │
                                       ▼
                  ┌────────────────────────────────────┐
                  │   PROTECTION:                       │
                  │   • SS54 Schottky reverse-polarity  │
                  │   • SMBJ15CA TVS clamp              │
                  │   • F1: PTC 2 A polyfuse (RXEF200)  │
                  └────────────────────────────────────┘
                                       │
                          ┌────────────┴────────────┐
                          ▼                         ▼
                  LM1084-5 V buck module     L293D module Vmot
                       (5 V, 5 A)             (motor 12 V via relay NO)
                          │
                    ┌─────┴─────┐
                    │           │
                    ▼           ▼
                AMS1117-3.3   5 V to:
                (3.3 V, 0.8A)  • 2× SG90 servos
                      │        • TFT backlight (via ULN2803 ch)
                      │        • XPT2046 logic level
                      │        • 74HC595 logic
                      │        • HC-SR04 (×4) — local 5 V at module
                      ▼        • ESP32-CAM (separate buck — see 5.4)
                 ESP32-WROOM-32E
                    ┌──────────────────┬─────────────┬──────────────┐
                    │ SPI (HSPI)       │ UART2       │ GPIO         │
                    │ ─ ILI9341 TFT    │ ─ ESP32-CAM │ ─ HC-SR04 ×4 │
                    │ ─ XPT2046 touch  │ (115k2 8N1) │ ─ KW11-3Z ×4 │
                    │ ─ shared CS each │             │ ─ E-stop IRQ │
                    │                  │             │ ─ Pot (ADC)  │
                    │ SPI (VSPI - opt) │ UART0 → CH340G USB-UART     │
                    │ ─ external SD?   │             │ ─ BTN-LADDER │
                    │                  │             │   (ADC1_CH6) │
                    │ I2C (free)       │             │ LEDC PWM     │
                    │                  │             │ ─ L293D IN1  │
                    │                  │             │ ─ L293D IN2  │
                    │                  │             │ ─ TFT BL     │
                    │                  │             │              │
                    │                  │             │ Shift register│
                    │                  │             │ ─ 74HC595 SER│
                    │                  │             │ ─       SH/CP│
                    │                  │             │ ─       ST/CP│
                    │                  │             │ ─       /OE  │
                    │                  │             │              │
                    │                  │             │ Servo PWM ×2 │
                    │                  │             │ ─ SG90 left  │
                    │                  │             │ ─ SG90 right │
                    │                  │             │              │
                    │                  │             │ Buzzer       │
                    │                  │             │              │
                    │                  │             │ Relay drive  │
                    └──────────────────┴─────────────┴──────────────┘
```

## 5.2 PCB design brief (every parameter as an exact value)

| Parameter | Value | Justification |
|---|---|---|
| Board outline | 100.00 × 80.00 mm rectangular, 1.0 mm corner radius | Locked from repo (L5); fits standard JLCPCB economy slot, fits PCB enclosure (11_pcb_enclosure.scad) with 0.3 mm clearance |
| Layer count | **2-layer** (top + bottom) | Sufficient for low-density 3.3 V signalling + single 12 V power rail |
| Substrate | FR4 TG130 (default), 1.6 mm | Mechanical rigidity for connector forces |
| Copper weight | **1 oz/ft² (35 µm)** both layers | Standard, free at JLCPCB; trace 1 mm @ 1 oz handles 2.5 A (>2 A peak L293D) |
| Surface finish | HASL lead-free | Cheapest at JLCPCB (free); compatible with hand soldering |
| Solder mask | **Matte black** | Free, helps silkscreen contrast; tier-2 finish |
| Silkscreen | **White** | All component refdes + project ident "VLB-G7 v2.0" + GitHub QR code in bottom-right |
| Trace width — signal | **0.25 mm** (10 mil) | Comfortable hand-soldering and JLCPCB minimum |
| Trace width — 3.3 V | **0.40 mm** (16 mil) | <300 mA total |
| Trace width — 5 V | **1.00 mm** (40 mil) | Up to 5 A from LM1084 module (BOM line 4) |
| Trace width — 12 V (motor) | **1.50 mm** (60 mil) | Peak ~1.5 A under L293D stall (rail-limited by L293D thermal) |
| Trace clearance | **0.20 mm** (8 mil) | JLCPCB economy minimum |
| Via — signal | drill 0.30 / pad 0.60 mm | Standard |
| Via — power | drill 0.40 / pad 0.80 mm; thermal vias 5× under the L293D module footprint and 4× under each LM1084 buck-module header | Heat dissipation |
| Bottom layer | **Solid GND pour** | Star-ground at the LM1084 main-buck GND pin |
| Top layer | **GND pour everywhere not signal/power** | Reduces EMI; backs antenna keep-out |
| Antenna keep-out | **7 × 30 mm** at top edge near ESP32 | ESP32 datasheet §5.5 keep-out |
| Mounting holes | **4× M3 (Ø 3.2 mm) at 3 mm from each corner** | Matches PCB enclosure standoffs |
| Connectors at top edge | USB-C (programming) + 12 V DC jack | Single side for cable management |
| Connectors at bottom edge | 5× JST-XH 2-pin (SR04 trig/echo, IR-removed/spare, motor, servo, ESP-CAM UART) | Ribbon to off-board peripherals |
| Connectors at left edge | JST-XH 4-pin (TFT 8-wire fan-out via 2 connectors), JST-XH 2-pin (E-stop) | Operator panel side |
| Connectors at right edge | 1.27 mm 2×3 ICSP (JTAG) header | Optional debug |
| Test points | TP1 (3V3), TP2 (5V), TP3 (12V), TP4 (V5_CAM), TP5 (GND), TP6 (BTN_LADDER) | Hand probing — note: no VPROPI in v2.2 (L293D lacks IS) |
| Fiducial marks | None (hand-assembled) | n/a |

## 5.3 TFT display + touch wiring on the PCB

The 2.8" ILI9341 + XPT2046 module ships with a **14-pin 0.1" pin header**. We do **not** use an FPC/ZIF socket because the standard module is pin-header only — no FPC variant is sold locally.

**Footprint in KiCad:** `Connector_PinHeader_2.54mm:PinHeader_1x14_P2.54mm_Vertical` (KiCad standard library).
- Refdes `J6` on schematic.
- Footprint pad-1 marker on silkscreen + pin labels.

**Pin-by-pin wiring** (signal name as printed on the module, ESP32 pin allocated, PCB net):

| TFT pin | Signal | ESP32 GPIO | PCB net | Notes |
|---|---|---|---|---|
| 1 | VCC | (5 V) | NET_5V | Through 0.1 µF + 10 µF cap pair |
| 2 | GND | GND | GND | |
| 3 | CS  | GPIO15 | TFT_CS | Strapping pin OK if pulled low at boot via 10 kΩ |
| 4 | RESET | GPIO4 | TFT_RST | |
| 5 | DC | GPIO2 | TFT_DC | Strapping pin OK (must be low at boot — 10 kΩ pull-down) |
| 6 | MOSI | GPIO13 | HSPI_MOSI | Shared SPI with touch |
| 7 | SCK | GPIO14 | HSPI_SCK | Shared SPI with touch |
| 8 | LED (BL) | GPIO27 (PWM) | TFT_BL | Through Q1 backlight transistor (see 5.5) |
| 9 | MISO | GPIO12 | HSPI_MISO | Strapping; ensure pull-up 10 kΩ |
| 10 | T_CLK | (HSPI_SCK) | HSPI_SCK | Same net as SCK |
| 11 | T_CS | GPIO33 | TOUCH_CS | Independent CS so touch IC can be polled separately |
| 12 | T_DIN | (HSPI_MOSI) | HSPI_MOSI | |
| 13 | T_DO | (HSPI_MISO) | HSPI_MISO | |
| 14 | T_IRQ | GPIO36 (RTC, input-only) | TOUCH_IRQ | Falling-edge ISR signals pen-down |

**Decoupling:** 0.1 µF X7R 0603 within 5 mm of TFT VCC pin; 10 µF 1206 X5R within 15 mm.

**Why this works:** XPT2046 and ILI9341 share the SPI bus. The XPT2046 max clock is 2.5 MHz; the ILI9341 runs at 40 MHz. The driver must lower the bus speed when CS_TOUCH is asserted, or use a separate `spi_device_handle_t` with different clock (LVGL's standard pattern, supported by the `lvgl_esp32_drivers` component).

## 5.4 Camera (ESP32-CAM) wiring on the PCB

The ESP32-CAM is **off-board** (mounted on the camera gantry). The PCB exposes a **4-pin JST-XH header** (`J7`) carrying:

| J7 pin | Signal | ESP32-WROOM GPIO | Cable colour (suggest) |
|---|---|---|---|
| 1 | +5 V | NET_5V | red |
| 2 | GND | GND | black |
| 3 | UART2_TX (main → cam) | GPIO17 | yellow |
| 4 | UART2_RX (cam → main) | GPIO16 | green |

Cable: 4-conductor 24 AWG, 2 m max length to gantry. UART runs at **115 200 baud, 8N1, no flow control**. 100 nF cap to GND on each line near the connector for ESD.

## 5.5 Backlight + buzzer + relay-coil drive — ULN2803 channels

**Topology:** the BOM (lines 10 + 11) specifies **two ULN2803A** Darlington
arrays as the project's universal low-side switch fabric. Each chip is an
8-channel, 500 mA-per-channel sink array with built-in flyback diodes
(pin 10) — perfect for inductive loads (relay) and small DC loads
(backlight, buzzer) without needing per-channel discrete transistors.

**Channel allocation (U6 — `loads` ULN2803):**

| Pin | Input | Driven by ESP32 GPIO | Sink path |
|---|---|---|---|
| 1 (IN1) | TFT_BL | GPIO 27 (LEDC ch 2, 5 kHz PWM) | TFT pin 8 (LED–) → GND |
| 2 (IN2) | BUZZER | GPIO 1 (LEDC ch 5, 2.7 kHz tone) | Piezo BUZ– → GND |
| 3..8 | spare | — | (broken out to a 6-pin header for future loads) |
| 9 (COM) | unused | — | (only needed if driving an inductive load on this chip) |

**Channel allocation (U7 — `relay` ULN2803):**

| Pin | Input | Driven by | Sink path |
|---|---|---|---|
| 1 (IN1) | RELAY_COIL | GPIO 32 (digitalWrite HIGH = energise) | Coil pin 1 → GND |
| 9 (COM) | +5V | — | Built-in flyback diode anchors here |
| 2..8 | spare | — | broken out to header |

> Polarity preserved from the BTS7960-era firmware: writing logic HIGH on
> the GPIO turns the channel ON (sinks the load to GND). LEDC duty-cycle
> control therefore behaves identically. No firmware change is needed for
> backlight, buzzer, or relay control.

**Schematic snippet (backlight channel):**

```
            +5V
             │
   TFT pin 1 (VCC)
             │
   [TFT internal: 47 Ω + LEDs]
             │
   TFT pin 8 (LED–) ────────► U6.OUT1  (Darlington collector)
                              │
                       U6.IN1 ◄─── GPIO 27 (LEDC PWM)
                              │
                       U6.GND ── GND
```

**Driver integration:** LEDC channel 2, 5 kHz, 13-bit resolution, duty 0–8191.
At 100 % duty the ULN2803 channel saturates at Vce(sat) ≈ 0.9 V, so panel
current ≈ (5 − 0.9 − Vf_LED) / 47 Ω ≈ 25 mA — comfortably below the 47 Ω
on-board resistor's design point and well under the channel's 500 mA limit.

## 5.5a Operator-panel resistor ladder (5 buttons, 1 ADC pin)

The BOM (lines 17 + 18) provides 6 tactile buttons and a 5 × 1 kΩ resistor kit. v2.2 wires 5 of those buttons in a series-resistor ladder feeding a single ESP32 ADC pin, freeing the touchscreen to be the *primary* operator control while the panel is the *survival* path (cracked-screen, gloves-on, fast-stab).

**Topology (referenced by `firmware/src/hmi/input.cpp`):**

```
   +3V3
     │
   R_pu = 10 kΩ
     │
   ADC ◄──┬── BTN1 ── R1=1kΩ ── BTN2 ── R2=1kΩ ── BTN3 ── R3=1kΩ ── BTN4 ── R4=1kΩ ── BTN5 ── R5=1kΩ ── GND
          │
       PIN_BTN_LADDER (GPIO 34, ADC1_CH6)
```

**Voltage map (12-bit ADC, 11 dB attenuation, 3.3 V FSR):**

| Pressed | V at ADC | ADC counts (nominal) | HmiCmd_t |
|---|---|---|---|
| (none) | 3.30 V | 4095 | HMI_CMD_NONE |
| BTN1 | 1.10 V | 1365 ± 80 | HMI_CMD_RAISE |
| BTN2 | 0.94 V | 1167 ± 80 | HMI_CMD_LOWER |
| BTN3 | 0.76 V |  943 ± 80 | HMI_CMD_HOLD |
| BTN4 | 0.55 V |  683 ± 80 | HMI_CMD_CLEAR_FAULT |
| BTN5 | 0.30 V |  372 ± 80 | HMI_CMD_NEXT_SCREEN |

The ladder is feasible in v2.2 because the BTS7960 IS pin (the original
v2.0 conflict source) was eliminated by migrating to the L293D module.
GPIO 34 now sees only the ladder; the deck-position pot stays on
GPIO 35 with no shared pin. Decoupling: 100 nF X7R cap from the ADC
node to GND, placed within 5 mm of the ESP32 pad, suppresses ringing
when a button is released against the ladder's L-C parasitics.

**Sixth button (BTN6 from BOM line 17):** wired as a hardware reset
to the ESP32 module's EN pin (through a 10 kΩ series, with the chip's
own pull-up). Distinct from the ladder; lets the operator hard-reboot
without unplugging USB.

## 5.6 KiCad project structure (every sheet, every file)

> **Status (v2.2):** the previous KiCad 8 project was deleted on
> 2026-05-03 because it had drifted from the firmware pin map. The new
> project is built from scratch in **KiCad 10.x** following
> `member_guides/M5_Ian_PCB_Power_Safety.md`. The 7-sheet hierarchy is:

```
pcb/kicad-project/
├── vertical_lift_pcb.kicad_pro          ← project
├── vertical_lift_pcb.kicad_sch          ← root sheet (just hierarchical sheet symbols)
├── vertical_lift_pcb.kicad_pcb          ← layout
├── PowerSupply.kicad_sch                ← +12 V → +5 V LM1084 module → +3.3 V AMS1117 + V5_CAM dedicated buck
├── ESP32Module.kicad_sch                ← ESP32-WROOM-32E module on female headers, 10 kΩ pull-up on GPIO 39
├── USBProgramming.kicad_sch             ← CH340G + USB-C + auto-reset DTR/RTS pair
├── MotorDriver.kicad_sch                ← L293D module 8-pin logic header + screw terminal
├── ShiftRegLights.kicad_sch             ← 74HC595 (DIP-16 socket) + 6 traffic LEDs (0805 SMD)
├── ConnectorsSafety.kicad_sch           ← JST-XH headers, SRD-05VDC relay, ULN2803 ×2, E-stop ladder, button ladder, polyfuse
└── TFT_Camera.kicad_sch                 ← TFT 14-pin header, touch IRQ, ESP32-CAM 4-pin connector, ULN2803 backlight channel
```

> **Action items for M5 (post-v2.2):** the schematic is captured per the
> sheet list above; PCB layout is routed per `member_guides/M5_Ian` §7.
> Subsequent rev-spins should focus on freeing an ADC pin (74HC4051
> analog mux, see `docs/known_limitations.md` L1/L2) so rail sense and
> per-limit identification can return.

## 5.7 Power-budget table

| Subsystem | Rail | Idle (mA) | Peak (mA) | Source |
|---|---|---|---|---|
| ESP32-WROOM (Wi-Fi off, 240 MHz) | 3.3 V | 50 | 240 | Datasheet §5.2 |
| ESP32-WROOM (Wi-Fi TX peak) | 3.3 V | – | 800 (50 ms bursts) | Datasheet §5.2 |
| CH340G | 5 V | 8 | 30 | Datasheet (BOM line 23) |
| 74HC595 + 8 LEDs (avg 4 lit @ 10 mA) | 5 V | 0 | 45 | Component-level |
| ILI9341 panel (logic + LEDs at 100 % BL) | 5 V | 5 (sleep) | 90 | Datasheet |
| XPT2046 | 3.3 V | 0.5 | 1.2 | Datasheet |
| HC-SR04 × 4 | 5 V | 8 | 60 (during ping) | Datasheet (15 mA each) |
| SG90 × 2 | 5 V | 8 | 1500 (peak stall, 100 ms) | Datasheet |
| Buzzer | 5 V | 0 | 30 | Component-level |
| Relay coil SRD-05VDC | 5 V | 0 | 75 | Datasheet |
| ESP32-CAM (idle, no streaming) | 5 V | 80 | – | AI-Thinker datasheet |
| ESP32-CAM (camera streaming) | 5 V | – | 240 | AI-Thinker datasheet |
| JGA25-370 motor (12 V, free run) | 12 V | – | 130 | Datasheet |
| JGA25-370 motor (12 V, normal) | 12 V | – | 350 | Measured under counterweight |
| JGA25-370 motor (12 V, stall) | 12 V | – | 2200 | Datasheet (L293D thermal-limits at ~2 A continuous; brief stall transients absorbed by the SS54 freewheel pair) |

**Rail totals:**

| Rail | Idle | Typical operation | Worst-case |
|---|---|---|---|
| 3.3 V | 58.5 mA | 250 mA | 825 mA → **AMS1117 rated 800 mA — OK at typical, marginal at burst** |
| 5 V (main) | 101 mA | 565 mA | **1830 mA** (servo stall + cam streaming + relay coil + LEDs) → **LM1084 module rated 5 A — comfortable** |
| V5_CAM | 80 mA | 240 mA | 240 mA (camera streaming) → **dedicated buck, no contention** |
| 12 V | (input) | 350 mA + losses ≈ 500 mA | 2300 mA briefly → **12 V supply must be rated ≥ 3 A** |

**Recommended 12 V supply: 12 V / 3 A barrel-jack adapter (centre-positive).** Mean Well GS18A12-P1J equivalent; locally, Pixel sells a 12 V/3 A switch-mode at KES 600 (BOM line 5).

## 5.8 PCB build status (v2.2)

The KiCad project is built **from scratch** in KiCad 10.x following
`member_guides/M5_Ian_PCB_Power_Safety.md`. Build sequence in that
guide:

1. Step 3 — create blank `pcb/kicad-project/`, set up the 7 hierarchical
   sub-sheets per §5.6 above, define the 5 net classes, draw the
   100 × 80 mm board outline with 4× M3 mounting holes.
2. Step 4 — assign every component its KiCad library footprint per the
   master footprint table.
3. Step 5 — capture the schematic sheet by sheet, matching the BOM
   components specified in this document.
4. Step 6 — ERC clean (0 errors).
5. Step 7 — PCB layout: pull footprints, configure 2-layer FR-4 stack,
   enforce the 15 × 15 mm antenna keepout under the WROOM module,
   place per the coordinate table, route by net-class priority, GND
   pour both sides, DRC clean.
6. Step 8 — export Gerbers + drill files to `pcb/gerbers/`.
7. Step 9 — JLCPCB order, 5 boards, matte-black mask, ≈ KES 1,900 to
   Nairobi DHL Pickup Point.
