# Vertical Lift Bridge — Perfboard Assembly Guide

This guide gives the **exact pin-to-pin connection for every component** in the
current build, soldered **one module at a time**, each followed by a quick test.
Follow it top to bottom. Every wire is listed — nothing is left to interpretation.

> **Golden rules**
> 1. The ESP32 logic is **3.3 V**. Never feed a 5 V signal into any ESP32 GPIO.
> 2. Build and **measure each power rail before** connecting any chip.
> 3. Wire one module, test it, then move to the next. Do not wire everything first.
> 4. Use **header pins** for anything that leaves the board (motor, servos, lasers,
>    LDRs, screen, E-stop, panel) so parts can be unplugged.

---

## 0. What is and isn't on this board

### Components used (this build)
| Qty | Component | Logic level |
| :-- | :--- | :--- |
| 1 | ESP32-WROOM-32 38-pin DevKit | 3.3 V |
| 1 | L298N dual H-bridge motor module | 12 V power / 3.3 V logic in |
| 1 | JGA25-370 12 V DC gearmotor | 12 V |
| 1 | 5 V relay module (1-channel, with onboard driver) | 5 V coil / 3.3 V IN |
| 1 | 74HC595 shift register (DIP-16) | 3.3 V |
| 6 | LEDs: Road R/Y/G + Marine R/Y/G | 3.3 V via 220 Ω |
| 6 | 220 Ω resistors (one per LED) | — |
| 2 | SG90 micro servo (barriers) | 5 V power / 3.3 V signal |
| 4 | Laser emitter module (break-beam source) | 5 V |
| 4 | LDR receiver module with digital out (DO) | **3.3 V** |
| 1 | ILI9341 2.8" TFT + XPT2046 touch | 3.3 V |
| 1 | Mushroom E-stop, **NC** contact | 3.3 V |
| 1 | 10 kΩ resistor (E-stop pull-up) | — |
| 5 | Tactile push-buttons (operator panel) | 3.3 V |
| 5 | 1 kΩ resistors (panel ladder string) | — |
| 1 | 10 kΩ resistor (panel ladder pull-up) | — |
| 1 | 12 V / ≥3 A barrel jack | 12 V |
| 1 | LM2596 buck module (12 V → 5 V) | — |
| 1 | AMS1117-3.3 regulator (5 V → 3.3 V) | — |
| — | 100 nF caps (one per IC), 1× 470–1000 µF on 5 V | — |

### Deliberately NOT installed (do not wire these)
| Item | Reason | Pin left… |
| :--- | :--- | :--- |
| Position potentiometer | Positioning is **timer-based** in firmware | GPIO 35 unconnected |
| Limit switches | End-stops are **timer-based** in firmware | GPIO 39 unconnected |
| Piezo buzzer | Disabled to keep UART TX0 free (`PIN_BUZZER = -1`) | GPIO 1 unconnected |
| ESP32-CAM / vision | Vision module omitted from this build | GPIO 17 unconnected |

---

## 1. Power architecture

```
        12 V Barrel Jack ──┬──────────────► LM2596 IN+        (buck converter)
                           │                LM2596 OUT+ ─────► 5 V RAIL
                           │                                   │
                           │                          AMS1117-3.3 IN ─► 3.3 V RAIL
                           │
                           └──► Relay COM ──(NO)──► L298N +12 V (motor supply, gated by relay)

   GND of jack, LM2596, AMS1117, ESP32, every module ───► one COMMON GND RAIL
```

- **ESP32 is powered from the 5 V rail** (into its `VIN`/`5V` pin). Its onboard
  regulator makes the chip's own 3.3 V.
- **The 3.3 V RAIL (AMS1117) powers all 3.3 V peripherals** (TFT, 74HC595, LDR
  receivers, and every pull-up).
- **Leave the ESP32 `3V3` pin unconnected** (do not tie it to the AMS1117 rail —
  avoid two 3.3 V sources fighting).
- **Rail summary:** 12 V (motor only, via relay), 5 V (ESP32 VIN, servos, relay
  coil, laser emitters), 3.3 V (all logic + sensors + pull-ups), common GND.

### Phase A — Build & verify the rails FIRST
1. Solder the barrel jack. Mark its **+** (center) and **–** terminals.
2. Jack **+12 V → LM2596 IN+**; jack **– → COMMON GND RAIL**; **LM2596 IN– → GND**.
3. **Before connecting anything to the output**, power the jack and turn the
   LM2596 trimpot until **OUT+ reads exactly 5.0 V** vs GND. Then power off.
4. LM2596 **OUT+ → 5 V RAIL**; **OUT– → GND RAIL**.
5. **5 V RAIL → AMS1117 IN**, **AMS1117 GND → GND RAIL**, **AMS1117 OUT → 3.3 V RAIL**.
6. Add a **470–1000 µF** cap across 5 V↔GND, and a **100 nF** across 3.3 V↔GND.
7. **TEST (multimeter):** 5 V rail = 5.0 V, 3.3 V rail = 3.3 V, GND continuous
   everywhere. **Do not proceed until correct**, or you will destroy the ESP32.

---

## 2. ESP32 master pin map (single source of truth)

Solder two 19-pin female header strips and plug the ESP32 in (do not solder the
module directly). Every GPIO this firmware uses:

| GPIO | Net | Module |
| :--- | :--- | :--- |
| VIN (5V) | ← 5 V RAIL | Power in |
| GND (×) | ← GND RAIL | Power |
| 3V3 | **leave unconnected** | — |
| 25 | L298N IN1 | Motor (up) |
| 26 | L298N IN2 | Motor (down) |
| 32 | Relay IN | Motor power cutoff |
| 14 | TFT SCK / T_CLK | Display + touch clock |
| 13 | TFT MOSI / T_DIN | Display + touch data-out |
| 12 | TFT MISO / T_DO | Display + touch data-in |
| 15 | TFT CS | Display chip-select |
| 2 | TFT DC | Display data/command |
| 33 | TOUCH T_CS | Touch chip-select |
| 23 | 74HC595 SER (pin 14) | Shift-register data |
| 4 | 74HC595 SRCLK (pin 11) | Shift-register clock |
| 27 | 74HC595 RCLK (pin 12) | Shift-register latch |
| 16 | Both servo signals | Barriers (shared) |
| 18 | LDR1 DO | Laser upstream A |
| 19 | LDR2 DO | Laser upstream B |
| 21 | LDR3 DO | Laser downstream A |
| 22 | LDR4 DO | Laser downstream B |
| 36 | E-stop + 10 kΩ pull-up | Safety (input-only) |
| 34 | Panel ladder node | Operator panel (input-only) |
| 1 | **unconnected** (buzzer omitted) | — |
| 17 | **unconnected** (vision omitted) | — |
| 35 | **unconnected** (pot omitted) | — |
| 39 | **unconnected** (limits omitted) | — |

### Phase B — ESP32 power
| From | To |
| :--- | :--- |
| ESP32 `VIN` / `5V` | 5 V RAIL |
| ESP32 `GND` (all GND pins) | GND RAIL |

**TEST:** Plug in 12 V (USB may stay connected for the serial monitor). The
ESP32 power LED lights; serial @115200 prints the boot banner.

---

## 3. Module-by-module wiring

Wire each table fully, then run its test before moving on.

### Module 1 — Safety relay (motor power cutoff)
The relay sits between the 12 V jack and the L298N so firmware (GPIO 32) and the
hardware E-stop can cut motor power.

| From | To | Purpose |
| :--- | :--- | :--- |
| Relay `VCC` | 5 V RAIL | Coil/driver power |
| Relay `GND` | GND RAIL | Common ground |
| Relay `IN` | ESP32 GPIO 32 | Firmware control |
| Relay `COM` | Barrel jack +12 V | Switched-in supply |
| Relay `NO` | L298N `+12V` (VS) | Switched-out to motor |

**TEST:** On boot the firmware energises the relay once no fault/E-stop — you
hear it click and `NO` reads ~12 V. Press E-stop → it clicks off, `NO` → 0 V.

### Module 2 — L298N motor driver + gearmotor
Keep the **ENA and ENB jumper caps installed** (ties enables high on-board) and
the **onboard 5 V-regulator jumper installed**. Speed/direction ride on IN1/IN2.

| From | To | Purpose |
| :--- | :--- | :--- |
| L298N `+12V` (VS) | Relay `NO` (see Module 1) | Motor supply (gated) |
| L298N `GND` | GND RAIL | Common ground |
| L298N `IN1` | ESP32 GPIO 25 | Up PWM |
| L298N `IN2` | ESP32 GPIO 26 | Down PWM |
| L298N `OUT1` | Gearmotor terminal A | Motor + |
| L298N `OUT2` | Gearmotor terminal B | Motor – |
| L298N `ENA` jumper | installed (on-board) | Channel A enabled |

> Do **not** connect the L298N `+5V` terminal to your 5 V rail — the module
> makes its own logic 5 V. Only `+12V`, `GND`, `IN1`, `IN2`, `OUT1`, `OUT2` are used.

**TEST:** From the HMI press RAISE; the motor runs one way, then LOWER reverses
it. If it runs the wrong way, swap `OUT1`/`OUT2`.

### Module 3 — Servo barriers (2 × SG90, mirrored on one signal)
Both servos share GPIO 16 (they move together). SG90 wires: **orange = signal,
red = +5 V, brown = GND.**

| From | To | Purpose |
| :--- | :--- | :--- |
| Servo 1 orange | ESP32 GPIO 16 | Shared PWM signal |
| Servo 2 orange | ESP32 GPIO 16 | Shared PWM signal |
| Servo 1 & 2 red | 5 V RAIL | Servo power |
| Servo 1 & 2 brown | GND RAIL | Common ground |

> Add a **470 µF** cap across 5 V↔GND near the servos to absorb their current spikes.

**TEST:** On boot the barriers sweep to the up position. RAISE closes them (down),
returning to idle opens them (up).

### Module 4 — 74HC595 shift register + traffic lights
DIP-16 pinout (notch up, pin 1 top-left):

```
   Q1 1 ┌──┐16 VCC
   Q2 2 │  │15 Q0
   Q3 3 │  │14 SER  (DATA)
   Q4 4 │  │13 OE   (active-low)
   Q5 5 │  │12 RCLK (LATCH)
   Q6 6 │  │11 SRCLK(CLOCK)
   Q7 7 │  │10 SRCLR(active-low)
  GND 8 └──┘ 9 Q7'
```

| From | To | Purpose |
| :--- | :--- | :--- |
| 595 pin 16 `VCC` | 3.3 V RAIL | Logic power |
| 595 pin 8 `GND` | GND RAIL | Common ground |
| 595 pin 10 `SRCLR` | 3.3 V RAIL | Disable reset (active-low) |
| 595 pin 13 `OE` | GND RAIL | Enable outputs (active-low) |
| 595 pin 14 `SER` | ESP32 GPIO 23 | Data |
| 595 pin 11 `SRCLK` | ESP32 GPIO 4 | Clock |
| 595 pin 12 `RCLK` | ESP32 GPIO 27 | Latch |
| 595 pin 16↔8 | 100 nF cap across | Decoupling |

Traffic LEDs — **anode → 595 output**, **cathode → 220 Ω → GND RAIL** (one
resistor per LED):

| 595 output | Pin | LED |
| :--- | :--- | :--- |
| Q0 | 15 | **Road RED** |
| Q1 | 1 | **Road YELLOW** |
| Q2 | 2 | **Road GREEN** |
| Q3 | 3 | **Marine RED** |
| Q4 | 4 | **Marine YELLOW** |
| Q5 | 5 | **Marine GREEN** |

(Q6 pin 6, Q7 pin 7, Q7' pin 9 — leave unconnected.)

**TEST:** Idle = Road GREEN + Marine RED. RAISE → Road RED. Fully up (HOLD) →
Marine GREEN. Each LED should be steady (no flicker).

### Module 5 — Laser break-beam (4 channels)
Each channel = one **laser emitter** aimed across the gap at one **LDR receiver**
module. Emitters take 5 V (they never touch a GPIO). **Receivers must be powered
from 3.3 V** so their digital output (`DO`) is GPIO-safe.

| From | To | Purpose |
| :--- | :--- | :--- |
| All 4 laser emitter `VCC` | 5 V RAIL | Emitter power |
| All 4 laser emitter `GND` | GND RAIL | Common ground |
| All 4 LDR receiver `VCC` | **3.3 V RAIL** | Receiver power (GPIO-safe DO) |
| All 4 LDR receiver `GND` | GND RAIL | Common ground |
| LDR1 `DO` (Upstream A) | ESP32 GPIO 18 | Direction sensing |
| LDR2 `DO` (Upstream B) | ESP32 GPIO 19 | Direction sensing |
| LDR3 `DO` (Downstream A) | ESP32 GPIO 21 | Direction sensing |
| LDR4 `DO` (Downstream B) | ESP32 GPIO 22 | Direction sensing |

> Aim each laser at its LDR and adjust the LDR module trimpot until `DO` goes
> HIGH when the beam is clear and LOW when blocked. Beam clear = not blocked.

**TEST:** Wave a hand through Upstream A then B → HMI shows a vessel approaching.

### Module 6 — E-stop (NC mushroom button)
GPIO 36 is **input-only and has NO internal pull-up**, so an **external 10 kΩ
pull-up to 3.3 V is mandatory.** With the NC contact: closed (idle) pulls the pin
LOW; pressing opens it and the pull-up drives it HIGH = "pressed".

| From | To | Purpose |
| :--- | :--- | :--- |
| E-stop terminal 1 | ESP32 GPIO 36 | Sense |
| E-stop terminal 2 | GND RAIL | Pulls LOW when closed |
| 10 kΩ resistor | GPIO 36 → 3.3 V RAIL | **Mandatory** pull-up |

**TEST:** Idle = system runs. Press E-stop → state goes ESTOP, relay clicks off,
motor dead. Release → returns to IDLE.

### Module 7 — Operator panel (5-button resistor ladder on GPIO 34)
GPIO 34 is input-only with its own external pull-up. Build a **string of five
1 kΩ resistors** from GND, and connect each button from the **ADC node (N)** to a
different tap. Tap resistance-to-ground sets each button's ADC band.

```
   3.3 V ──[10 kΩ pull-up]──┬── N ── ESP32 GPIO 34
                            │
   N ─[BTN RAISE]──────────► T5 (5 kΩ to GND)
   N ─[BTN LOWER]──────────► T4 (4 kΩ to GND)
   N ─[BTN HOLD]───────────► T3 (3 kΩ to GND)
   N ─[BTN CLEAR_FAULT]────► T2 (2 kΩ to GND)
   N ─[BTN NEXT_SCREEN]────► T1 (1 kΩ to GND)

   String:  GND ─[1k]─ T1 ─[1k]─ T2 ─[1k]─ T3 ─[1k]─ T4 ─[1k]─ T5
```

| From | To | Purpose |
| :--- | :--- | :--- |
| 10 kΩ resistor | GPIO 34 (node N) → 3.3 V RAIL | Pull-up / reference |
| Node N | ESP32 GPIO 34 | ADC input |
| Resistor R5 (1 kΩ) | GND → T1 | Ladder bottom |
| Resistor R4 (1 kΩ) | T1 → T2 | Ladder |
| Resistor R3 (1 kΩ) | T2 → T3 | Ladder |
| Resistor R2 (1 kΩ) | T3 → T4 | Ladder |
| Resistor R1 (1 kΩ) | T4 → T5 | Ladder top |
| Button RAISE | Node N → T5 | ADC ≈ 1365 |
| Button LOWER | Node N → T4 | ADC ≈ 1167 |
| Button HOLD | Node N → T3 | ADC ≈ 943 |
| Button CLEAR_FAULT | Node N → T2 | ADC ≈ 683 |
| Button NEXT_SCREEN | Node N → T1 | ADC ≈ 372 |

**TEST:** Each press triggers exactly its action (RAISE/LOWER/HOLD/clear-fault/
next-tab). If a button triggers the wrong action, you swapped two taps.

### Module 8 — TFT display + touch (ILI9341 + XPT2046, shared SPI)
14-pin combo board. TFT and touch **share** SCK/MOSI/MISO; only the chip-selects
differ. `T_IRQ` is unused (firmware polls).

| Module pin | To | Purpose |
| :--- | :--- | :--- |
| `VCC` | 3.3 V RAIL | Power |
| `GND` | GND RAIL | Ground |
| `CS` | ESP32 GPIO 15 | Display CS |
| `RESET` | ESP32 `EN` | Reset (tied to EN) |
| `DC` / `RS` | ESP32 GPIO 2 | Data/command |
| `SDI` / `MOSI` | ESP32 GPIO 13 | SPI data out |
| `SCK` | ESP32 GPIO 14 | SPI clock |
| `LED` / `BL` | 3.3 V RAIL | Backlight (always on) |
| `SDO` / `MISO` | ESP32 GPIO 12 | SPI data in |
| `T_CLK` | ESP32 GPIO 14 | Touch clock (shared SCK) |
| `T_CS` | ESP32 GPIO 33 | Touch CS |
| `T_DIN` | ESP32 GPIO 13 | Touch data (shared MOSI) |
| `T_DO` | ESP32 GPIO 12 | Touch data (shared MISO) |
| `T_IRQ` | leave unconnected | Polled, not used |

**TEST:** UI appears with HOME/OPS/CW/SET tabs. Touch RAISE on the OPS tab and
the bridge sequence starts.

---

## 4. Final checklist & power-up

1. **Visual:** no solder bridges; every IC VCC/GND correct; LDR receivers on
   **3.3 V** (never 5 V); E-stop 10 kΩ pull-up present; ESP32 `3V3` pin unconnected.
2. **Continuity (power off):** GND continuous; no short between 12 V, 5 V, 3.3 V,
   or any of them to GND.
3. **Rails (12 V in, ESP32 out):** confirm 5.0 V and 3.3 V again under load.
4. **Boot:** serial @115200 prints the banner and each `[module] init OK` line.
5. **Dry run (deck parked at bottom):** RAISE → barriers close, counterweight
   tanks fill on the CW tab as the deck bar rises, Marine turns GREEN at the top;
   LOWER drains them back to empty. E-stop kills everything instantly.

> **Calibration:** measure the real full-travel times and set `DECK_RAISE_TIME_MS`
> / `DECK_LOWER_TIME_MS` in `firmware/src/system_types.h` so the HMI deck height
> reads true. Always power off with the **deck parked at the bottom** (no limit
> switch homes it).

---

## 5. Soldering best practices
1. **Never cross bare wires.** Use insulated jumpers where paths cross.
2. **Header pins for everything external** (motor, servos, lasers, LDRs, screen,
   E-stop, panel) so a failed part swaps out without desoldering.
3. **3.3 V vs 5 V discipline.** Any sensor output going to a GPIO must be a 3.3 V
   signal. The LDR receivers and 74HC595 are powered from 3.3 V for this reason.
4. **One 100 nF cap per IC**, right at its VCC/GND pins, plus the bulk 5 V cap.
5. **Test after every module** — never wire the whole board then power it blind.
