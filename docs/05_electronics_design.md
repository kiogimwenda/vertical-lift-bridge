# [5] ELECTRONICS DESIGN — Full Specification

## 5.1 System block diagram (text-structured, exact bus widths)

```
                            +12 V DC INPUT (5.5×2.1 mm jack)
                                       │
                                       ▼
                  ┌────────────────────────────────────┐
                  │   PROTECTION:                       │
                  │   • SS54 schottky reverse-polarity  │
                  │   • SMBJ16A TVS                     │
                  │   • F1: PTC 1.5 A poly-fuse         │
                  └────────────────────────────────────┘
                                       │
                                ┌──────┴──────┐
                                ▼             ▼
                          LM2596-5.0      DRV8871 12V VM
                          (5 V, 3 A)         (motor)
                                │
                          ┌─────┴─────┐
                          │           │
                          ▼           ▼
                      AMS1117-3.3   5 V to:
                      (3.3 V, 0.8A)  • 2× SG90
                            │        • TFT backlight
                            │        • XPT2046 logic level
                            │        • 74HC595 logic
                            │        • HC-SR04 (×4)
                            ▼        • ESP32-CAM (5 V VBUS)
                       ESP32-WROOM
                    ┌──────────────────┬─────────────┬──────────────┐
                    │ SPI (HSPI)       │ UART2       │ GPIO         │
                    │ ─ ILI9341 TFT    │ ─ ESP32-CAM │ ─ HC-SR04 ×4 │
                    │ ─ XPT2046 touch  │ (115k2 8N1) │ ─ KW12-3 ×8  │
                    │ ─ shared CS each │             │ ─ E-stop IRQ │
                    │                  │             │ ─ POT (ADC)  │
                    │ SPI (VSPI - opt) │ UART0 → CP2102N USB-C       │
                    │ ─ external SD?   │             │              │
                    │                  │             │              │
                    │ I2C (free)       │             │ LEDC PWM     │
                    │                  │             │ ─ DRV8871 IN1│
                    │                  │             │ ─ DRV8871 IN2│
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
| Copper weight | **1 oz/ft² (35 µm)** both layers | Standard, free at JLCPCB; trace 1 mm @ 1 oz handles 2.5 A (>2A peak DRV8871) |
| Surface finish | HASL lead-free | Cheapest at JLCPCB (free); compatible with hand soldering |
| Solder mask | **Matte black** | Free, helps silkscreen contrast; tier-2 finish |
| Silkscreen | **White** | All component refdes + project ident "VLB-G5 v2.0" + GitHub QR code in bottom-right |
| Trace width — signal | **0.25 mm** (10 mil) | Comfortable hand-soldering and JLCPCB minimum |
| Trace width — 3.3 V | **0.40 mm** (16 mil) | <300 mA total |
| Trace width — 5 V | **1.00 mm** (40 mil) | Up to 3 A from LM2596 |
| Trace width — 12 V (motor) | **1.50 mm** (60 mil) | Peak 2.5 A under stall |
| Trace clearance | **0.20 mm** (8 mil) | JLCPCB economy minimum |
| Via — signal | drill 0.30 / pad 0.60 mm | Standard |
| Via — power | drill 0.40 / pad 0.80 mm; thermal vias 5× under DRV8871 | Heat dissipation |
| Bottom layer | **Solid GND pour** | Star-ground at LM2596 GND pin |
| Top layer | **GND pour everywhere not signal/power** | Reduces EMI; backs antenna keep-out |
| Antenna keep-out | **7 × 30 mm** at top edge near ESP32 | ESP32 datasheet §5.5 keep-out |
| Mounting holes | **4× M3 (Ø 3.2 mm) at 3 mm from each corner** | Matches PCB enclosure standoffs |
| Connectors at top edge | USB-C (programming) + 12 V DC jack | Single side for cable management |
| Connectors at bottom edge | 5× JST-XH 2-pin (SR04 trig/echo, IR-removed/spare, motor, servo, ESP-CAM UART) | Ribbon to off-board peripherals |
| Connectors at left edge | JST-XH 4-pin (TFT 8-wire fan-out via 2 connectors), JST-XH 2-pin (E-stop) | Operator panel side |
| Connectors at right edge | 1.27 mm 2×3 ICSP (JTAG) header | Optional debug |
| Test points | TP1 (3V3), TP2 (5V), TP3 (12V), TP4 (GND), TP5 (VPROPI), TP6 (3.3V_AON) | Hand probing |
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

## 5.5 Backlight PWM control circuit

**Topology:** Logic-level N-channel MOSFET low-side switch on the TFT LED- return. The TFT module's onboard 47 Ω current-limit resistor sets ~30 mA at 5 V; a single transistor switches the return.

**Selected device:** **AO3400A** (SOT-23-3, Vth ≈ 1.5 V, RDS(on) ≈ 35 mΩ @ Vgs=4.5 V).
- Why not a BJT (e.g. 2N3904)? At 30 mA the BJT drops ~0.2 V and dissipates 6 mW — fine, but the MOSFET dissipates only 0.03 mW and switches faster, giving cleaner 5 kHz LEDC PWM with no audible ringing.
- Why not a dedicated LED driver (e.g. PT4115)? Overkill at 30 mA and not needed — the module already current-limits.

**Schematic snippet** (KiCad ASCII):

```
                +5V
                 │
                 │  TFT module pin 8 (LED+/BL)
                 │
                 ▼
      [TFT internal: 47Ω + LEDs]
                 │
                 ▼      Q1 = AO3400A
              ┌──────┐
              │  D   │
GPIO27─10k──▶│G  S─┼── GND
              └──────┘
              │
              R_pulldown 100 kΩ to GND on G to keep BL off if MCU floats during boot
```

**Driver integration:** LEDC channel 2, 5 kHz, 13-bit resolution, duty 0–8191. 100 % brightness ≈ 30 mA across the panel.

## 5.6 KiCad project structure (every sheet, every file)

```
pcb/kicad-project/
├── vertical_lift_pcb.kicad_pro          ← project
├── vertical_lift_pcb.kicad_sch          ← root sheet (just hierarchical sheet symbols)
├── vertical_lift_pcb.kicad_pcb          ← layout
├── PowerSupply.kicad_sch                ← +12 V → +5 V LM2596 → +3.3 V AMS1117
├── ESP32Module.kicad_sch                ← ESP32-WROOM-32 + strapping resistors + crystal (none, internal)
├── USBUart.kicad_sch                    ← CP2102N + USB-C + auto-reset DTR/RTS
├── MotorDrivers.kicad_sch               ← DRV8871 + VPROPI + flyback caps
├── ShiftRegLEDs.kicad_sch               ← 74HC595 + 8 LEDs (4× R, 2× Y, 2× G traffic) + Rs
├── ConnectorsSafety.kicad_sch           ← JST-XH headers, SRD-05VDC relay, MOSFET, E-stop, fuse
├── TFT_Camera.kicad_sch                 ← *NEW SHEET* — TFT pin header, touch IRQ, ESP32-CAM UART, backlight Q1
└── symbols/                             ← Custom symbols (none for v2 — all standard)
```

> **Action item for M5:** Add `TFT_Camera.kicad_sch` as a 7th hierarchical sub-sheet, hand-place the parts as detailed in 5.3–5.5 above, route, regenerate Gerbers, re-order. Existing PCB layout (already in repo) needs the IR sensor connector relabelled to the spare ESP32-CAM connector (see 5.4) and Q1+R network added.

## 5.7 Power-budget table

| Subsystem | Rail | Idle (mA) | Peak (mA) | Source |
|---|---|---|---|---|
| ESP32-WROOM (Wi-Fi off, 240 MHz) | 3.3 V | 50 | 240 | Datasheet §5.2 |
| ESP32-WROOM (Wi-Fi TX peak) | 3.3 V | – | 800 (50 ms bursts) | Datasheet §5.2 |
| CP2102N | 3.3 V | 8 | 25 | Datasheet |
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
| JGA25-370 motor (12 V, stall) | 12 V | – | 2200 | Datasheet (current limit by DRV8871 to ~1.3 A via Risense) |

**Rail totals:**

| Rail | Idle | Typical operation | Worst-case |
|---|---|---|---|
| 3.3 V | 58.5 mA | 250 mA | 825 mA → **AMS1117 rated 800 mA — OK at typical, marginal at burst** |
| 5 V | 101 mA | 565 mA | **1830 mA** (servo stall + cam streaming + relay + LEDs) → **LM2596 rated 3 A — OK** |
| 12 V | (input) | 350 mA + 100 mA losses ≈ 450 mA | 2300 mA briefly → **12 V supply must be rated ≥ 3 A** |

**Recommended 12 V supply: 12 V / 3 A barrel-jack adapter (centre-positive).** Mean Well GS18A12-P1J equivalent; locally, Pixel sells a 12 V/3 A switch-mode at KES 600.

## 5.8 PCB action items for M5 (delta from current repo state)

1. **Open** `pcb/kicad-project/vertical_lift_pcb.kicad_pro` in KiCad 8.
2. **Edit** `ConnectorsSafety.kicad_sch` — remove the two `IR_DECK_A`, `IR_DECK_B` JST-XH 3-pin headers; replace with one **JST-XH 4-pin header** named `J7_ESP32CAM` per 5.4.
3. **Add new sheet** `TFT_Camera.kicad_sch` per 5.3–5.5; tie to root sheet via hierarchical sheet symbol.
4. **Layout edit:** reposition the freed area on PCB to fit the AO3400A + 10 kΩ + 100 kΩ + 0.1 µF for backlight.
5. **DRC + ERC clean.**
6. **Re-export Gerbers** to `pcb/gerbers/JLCPCB_order.zip` (settings: 2 layers, 1.6 mm, HASL-LF, matte-black mask, white silk).
7. **Re-order JLCPCB** (5 PCBs, ≈ KES 950 with shipping to Nairobi via DHL Pickup Point).
