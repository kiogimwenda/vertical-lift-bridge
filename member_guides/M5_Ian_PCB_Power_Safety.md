# Member 5 — Ian — PCB, Power, ESP32-CAM Electronics & Safety Firmware

> **Your role.** You design the PCB, route the power tree, build the safety chain (E-stop, relay, watchdog), maintain the fault registry, and host the ESP32-CAM electronics on the same board so M3's vision link is plug-and-play. Without your work the system is unsafe; without your work the system has no electrical home.
>
> **Your single deliverable.** A 100 × 80 mm 2-layer PCB, populated and tested, that:
> - Accepts a 12 V DC barrel jack input.
> - Generates 5 V (3 A LM2596) and 3.3 V (800 mA AMS1117) rails.
> - Generates a separate clean 5 V/2 A rail for the ESP32-CAM via a second LM2596 (electrically isolated from the main 5 V to prevent brownout coupling).
> - Hosts the ESP32-WROOM-32E DevKit module on female headers.
> - Hosts the BTS7960 motor driver via screw terminals.
> - Hosts a 5 V SRD-05VDC relay in series with motor 12 V supply (E-stop chain).
> - Has connectors for: TFT (14-pin), ESP32-CAM (4-pin JST-XH), 4 × HC-SR04 (8-pin JST-XH), barrier servos, traffic LED panel, mushroom E-stop, USB-C programming.
> - Passes ERC + DRC clean, with antenna keepout enforced.
> - Powers up cleanly: 12 V draws < 100 mA at idle, ESP32 boots, all `[xxx] init OK` lines appear.
> - Cuts motor power within 50 ms of E-stop press (verified on bench).

---

## 0. What you are building (read this once)

Three separable subsystems on one board:

1. **Power tree** — 12 V in → protection (TVS, polyfuse, Schottky) → splits to motor + LM2596 (5 V main) + LM2596 (5 V CAM, isolated). 5 V main → AMS1117 → 3.3 V.
2. **Compute + I/O** — ESP32-WROOM-32E DevKit on female headers. All GPIO breakouts to JST-XH connectors.
3. **Safety chain** — E-stop mushroom switch in series with relay coil control; relay contact in series with motor B+ supply. 2N7000 MOSFET drives the relay coil from a GPIO. Watchdog firmware backs it up.

The ESP32-CAM is **off-board** (mounted on the camera gantry above the road), but **its power supply lives on this PCB** — that's the v2.1 change you must implement. The 4-pin JST-XH connector exposes GND, CAM 5 V (from the dedicated buck), CAM RX, and CAM TX (which becomes main board's UART2 RX).

---

## Scope of your work — every asset you touch

| Asset | Where | Status |
|---|---|---|
| KiCad project | `pcb/kicad-project/vertical_lift_pcb.kicad_pro` | ✅ exists — needs v2.1 updates |
| Schematic root sheet | `pcb/kicad-project/vertical_lift_pcb.kicad_sch` | ✅ exists |
| Sub-sheet: Power | `pcb/kicad-project/PowerSupply.kicad_sch` | ✅ exists — add CAM-dedicated buck |
| Sub-sheet: ESP32 | `pcb/kicad-project/ESP32Module.kicad_sch` | ✅ exists |
| Sub-sheet: USB | `pcb/kicad-project/USBUart.kicad_sch` | ✅ exists |
| Sub-sheet: Motor | `pcb/kicad-project/MotorDrivers.kicad_sch` | ✅ exists — already updated to BTS7960 |
| Sub-sheet: 595 LED chain | `pcb/kicad-project/ShiftRegLEDs.kicad_sch` | ✅ exists |
| Sub-sheet: Connectors + safety | `pcb/kicad-project/ConnectorsSafety.kicad_sch` | ✅ exists — add J7 ESP32-CAM connector |
| New sub-sheet: TFT + Camera | `pcb/kicad-project/TFT_Camera.kicad_sch` | ❌ create |
| PCB layout | `pcb/kicad-project/vertical_lift_pcb.kicad_pcb` | ✅ exists — needs re-layout |
| Gerber output | `pcb/gerbers/JLCPCB_order.zip` | ❌ generate |
| BOM | `bom/VLB_Group7_BOM.xlsx` | ✅ exists — you maintain |
| `firmware/src/safety/watchdog.{h,cpp}` | firmware | ✅ exists — you tune |
| `firmware/src/safety/fault_register.{h,cpp}` | firmware | ✅ exists — rail monitoring removed in v2.1 |
| `firmware/src/safety/interlocks.{h,cpp}` | firmware | ✅ exists — wire to your PCB |
| `docs/05_electronics_design.md` | docs | ✅ exists — keep up to date |

---

## Step 1 — Set up your machine (Windows 11)

Follow **steps 1.1 to 1.7 of M1's guide** first (VS Code, Git, Python 3.11, PlatformIO, CH340 driver, repo clone). Then add the items below.

### 1.8 Install KiCad 8.0 (the PCB design tool)
1. Edge → `kicad.org/download/windows/`. Click "Windows 64-bit installer". File `kicad-8.0.x-x86_64.exe` (~1.2 GB — patient on Kenyan broadband).
2. Run installer. Click "Yes" on UAC.
3. **Select all components** when offered (Schematic editor, PCB editor, 3D viewer, libraries, demos).
4. Default install location is fine: `C:\Program Files\KiCad\8.0\`.
5. After install, launch KiCad. The **Project Manager** opens. On first launch it will download the official symbol/footprint libraries (~5 min). Wait until "Synchronization complete".

### 1.9 Install the JLCPCB component-library plugin (helpful but optional)
JLCPCB has its own library with stock matching:
1. Edge → `github.com/Bouni/kicad-jlcpcb-tools`. Click "Code" → "Download ZIP".
2. Save the ZIP. Don't extract.
3. KiCad → top menu **Tools** → **Plugin and Content Manager** → tab **"Install from File…"** → select your ZIP → OK.
4. Restart KiCad. The tool now appears as a button in the PCB editor toolbar.

### 1.10 Install hardware tools (you will need these physically)
- **Soldering iron with temperature control**, 0.4 mm or 0.6 mm chisel tip. Hakko FX-888D, Pinecil, or TS80P. Set 350 °C for 60/40 leaded solder, 380 °C for lead-free.
- **Solder.** 0.6 mm leaded 60/40 with rosin core (smoother flow, easier for hand assembly). Lead-free is harder for beginners.
- **Liquid flux** — even cheap rosin flux dramatically improves drag-soldering quality.
- **Solder wick** (1.5 mm width) for cleanup of bridges.
- **Tweezers** — fine point + bent.
- **Multimeter with continuity beep** (UNI-T UT139C ~KES 2,500 from Pixel).
- **Bench power supply with current limit** (RIDEN RD6006 or similar; ~KES 4,500). **Without current limit, your first short will fry traces.**
- **Helping hands or PCB vise** to hold the board while soldering.
- **Magnifying loupe** (10×) or USB microscope to inspect SMD joints.
- **Hot-air rework station** (optional, ~KES 4,000) for SOT-223 components and rework.

### 1.11 Install OpenSCAD (you may need to view the enclosure model)
Same as M2's Step 1.8.

---

## Step 2 — KiCad primer (assume nothing — read once, reference later)

> Skip this if you have used KiCad before. Otherwise spend 30 minutes here — the rest of the guide assumes these basics.

### 2.1 KiCad project structure
A KiCad project is a folder with:
- One `.kicad_pro` file — the project definition.
- One `.kicad_sch` file per **sheet** (root sheet + each sub-sheet).
- One `.kicad_pcb` file — the layout.
- Symbol/footprint library tables.

Open a project by double-clicking the `.kicad_pro` file (KiCad Project Manager opens, then you double-click "Schematic Editor" or "PCB Editor" inside).

### 2.2 The 4 KiCad editors
| Tool | What it does |
|---|---|
| **Schematic Editor** (eeschema) | Draw the circuit diagram. Add symbols, wires, labels. Run ERC. |
| **PCB Editor** (pcbnew) | Place footprints, route copper, generate Gerbers. Run DRC. |
| **Symbol Editor** | Create custom symbols (rare for this project). |
| **Footprint Editor** | Create custom footprints (you might do this for the BTS7960 module). |

### 2.3 Hierarchical sheets
Big projects split into multiple schematic pages connected by **hierarchical sheet symbols** on the root sheet. You enter a sub-sheet by clicking its sheet box in eeschema. Each sub-sheet has **hierarchical labels** that map to the parent sheet's pins.

This project's hierarchy:
```
vertical_lift_pcb.kicad_sch        (root — just sheet symbols)
├── PowerSupply.kicad_sch          (12V → 5V → 3.3V tree, plus CAM-dedicated 5V)
├── ESP32Module.kicad_sch          (ESP32-WROOM headers, decoupling)
├── USBUart.kicad_sch              (CP2102N USB-C bridge for programming)
├── MotorDrivers.kicad_sch         (BTS7960 connector, current-sense to GPIO 34)
├── ShiftRegLEDs.kicad_sch         (74HC595 + 6 LEDs)
├── ConnectorsSafety.kicad_sch     (JST-XH connectors, relay, MOSFET, E-stop, fuse)
└── TFT_Camera.kicad_sch           (NEW — TFT 14-pin header, ESP32-CAM 4-pin connector,
                                    backlight Q1, all decoupling)
```

### 2.4 Hotkeys you will use 100 times
| Key | Action |
|---|---|
| `A` | Add symbol (in schematic) / Add footprint (in PCB) |
| `W` | Wire (in schematic) |
| `L` | Local label / Net label |
| `H` | Hierarchical label |
| `M` | Move (drag while holding) |
| `R` | Rotate selected |
| `G` | Drag (move keeping wires connected) |
| `Delete` | Delete selected |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+I` | Inspect ERC errors (schematic) / DRC errors (PCB) |
| `B` | Refill all zones (PCB — recompute ground pour) |
| `Ctrl+S` | Save |

---

## Step 3 — Open the existing project, take stock

1. Open KiCad. File → Open Project.
2. Navigate to `vertical-lift-bridge\pcb\kicad-project\` → select `vertical_lift_pcb.kicad_pro` → Open.
3. In the Project Manager pane on the left, double-click `vertical_lift_pcb.kicad_sch` to open the root schematic.
4. **Read every sheet.** Double-click each sheet box to descend; press the up-arrow icon to ascend. Make a list of:
   - Existing connectors (J1, J2, J3, …) and what each connects to.
   - Existing component reference designators (R1, C1, U1, …).
   - The ESP32-WROOM module's GPIO assignments.
   - Any "TODO" or "FIXME" notes the previous designer left.
5. Open the PCB layout (back in Project Manager → double-click `.kicad_pcb`). Note:
   - Board outline (should be 100 × 80 mm).
   - Mounting holes (4 × M3 at corners).
   - Antenna keepout zone (top edge).
   - Existing routes — get a feel for layer use.

### 3.1 What's missing in v2.1 — the things you will add
1. **CAM-dedicated 5 V buck** (second LM2596). Required because v2.1 firmware moves the ESP32-CAM to its own clean rail.
2. **J7 — ESP32-CAM 4-pin JST-XH connector.** GND, CAM 5 V, CAM RX (unused), CAM TX → ESP32 GPIO 16.
3. **TFT_Camera.kicad_sch** — a new sub-sheet that holds J6 (TFT), J7 (CAM), the backlight MOSFET (AO3400A) and decoupling caps for both.
4. **Antenna keepout reposition** — re-check the 7 × 30 mm zone matches the WROOM module footprint.

### 3.2 What you must NOT add (v2.1 removed)
- 12 V / 5 V rail-monitoring divider footprints (R23/R24/R25/R26 if they exist) — leave **unpopulated** in BOM. Their nets are dead in v2.1 and would only inject noise into VPROPI / DECK_POSITION. See `docs/known_limitations.md` (L1).
- Front-panel resistor-ladder buttons — touchscreen is sole input.

---

## Step 4 — Power supply schematic (the most safety-critical work)

Open `PowerSupply.kicad_sch`.

### 4.1 12 V input + protection chain
The 12 V barrel jack feeds through (in order):

```
J1 (DC barrel jack) → D1 (Schottky reverse-protection) → F1 (polyfuse) → TVS clamp → V12 net
```

Components:
| Refdes | Part | Why |
|---|---|---|
| J1 | DC barrel jack 5.5 × 2.1 mm, panel-mount or PCB-mount | Standard 12 V supply jack |
| D1 | **SS54** Schottky (40 V, 5 A, SMA package) | Reverse-polarity protection. Forward drop ~0.4 V loses < 1 W at 2 A. |
| F1 | **PTC polyfuse 1.5 A hold / 3 A trip** (1812 SMD) | Resettable; trips on motor stall over-current |
| D2 | **SMBJ16A** TVS diode (16 V working, SMB) | Clamps voltage spikes from inductive motor loads |

Symbols to use (in eeschema, press A):
- Barrel jack: `Connector:Barrel_Jack` symbol, footprint `Connector_BarrelJack:BarrelJack_Horizontal`.
- Schottky: `Diode:SS54`, footprint `Diode_SMD:D_SMA`.
- Polyfuse: `Device:Fuse`, footprint `Fuse:Fuse_1812_4532Metric`.
- TVS: `Diode_TVS:SMBJxxA` (pick the 16 V variant), footprint `Diode_SMD:D_SMB`.

### 4.2 Main 5 V rail (LM2596)
The LM2596 is a **buck switching regulator** that drops 12 V to 5 V at up to 3 A.

Connection diagram (from the LM2596 datasheet):
```
       +12V ──┬── Vin (pin 1)
              │
              ├── 470µF (Cin, electrolytic, 25V)
              │
              GND

  Output ──┬── L1 (33µH inductor) ──┬── Vout (pin 2)
           │                          │
           ▼                          ▼
       D3 (SS34)               220µF (Cout, electrolytic, 16V)
       cathode to Vout                │
       anode to GND                   │
                                      └── Feedback (pin 4)
                                          via 1k/3k divider OR
                                          use the fixed-5V LM2596S-5.0 variant
```

Use the **fixed-output LM2596S-5.0 variant** if available — no feedback resistors needed, 5 V is set internally. (Some shops only stock the adjustable LM2596S-ADJ; if so, set the divider for 5.0 V output: R1=1 kΩ, R2=3.0 kΩ, gives Vout = 1.23 × (1+R2/R1) = 4.92 V, close enough.)

| Refdes | Part | Footprint |
|---|---|---|
| U2 | **LM2596S-5.0** (TO-263-5 SMD) | `Package_TO_SOT_SMD:TO-263-5_TabPin3` |
| L2 | 33 µH 3 A power inductor | `Inductor_SMD:L_12x12mm_H8mm` (any 12×12 mm shielded) |
| D3 | SS34 Schottky (40 V, 3 A) | `Diode_SMD:D_SMA` |
| C2 | 470 µF / 25 V electrolytic | `Capacitor_THT:CP_Radial_D8.0mm_P3.50mm` |
| C3 | 220 µF / 16 V electrolytic | same |

**Layout tip:** keep the high-current loop (Vin → switch node → L → Cout → GND → back to Vin) **as small as possible**. Long traces in this loop radiate EMI.

### 4.3 ESP32-CAM dedicated 5 V buck (NEW in v2.1)
Add a **second** LM2596 identical to U2 above. Refdes `U2B`. Vin tied to the same 12 V rail (after F1/D2 protection). Vout becomes a separate net **`V5_CAM`** that goes to J7 pin 2.

**Why a second buck instead of just a separate fuse on the same 5 V rail?**
- The ESP32-CAM peaks at ~240 mA during camera frame capture. The main 5 V rail powers the TFT backlight (~30 mA), 4 × HC-SR04 (~60 mA each peak = 240 mA), the buzzer, the relay coil, and 5 V signal levels. Worst-case main 5 V is ~1.2 A.
- If CAM and main share 5 V, the camera burst causes a brief voltage dip (rail droops ~50 mV) that brownouts the CAM itself — it then resets, retriggering. Brownout loop.
- A dedicated buck for CAM eliminates the coupling. CAM gets its own clean 5 V at up to 2 A.

Same components as U2 (fixed-output LM2596S-5.0 + 33 µH inductor + Schottky + caps). Add `V5_CAM` as a hierarchical label exiting this sheet, mapped to a pin on the root sheet that delivers it to J7 in `TFT_Camera.kicad_sch`.

### 4.4 3.3 V rail (AMS1117-3.3)
Linear LDO. Drops 5 V to 3.3 V at up to 800 mA.

```
V5_main ──┬── Vin (AMS1117 pin 3)
           │
           ▼
       10µF X5R 1206 (input cap)
           │
           GND

       Vout (AMS1117 pin 2) ──┬── V3V3 net
                                │
                                ▼
                            10µF X5R 1206 (output cap, REQUIRED for stability)
                                │
                                GND

       GND (AMS1117 pin 1) ── GND
```

| Refdes | Part | Footprint |
|---|---|---|
| U3 | AMS1117-3.3 SOT-223 | `Package_TO_SOT_SMD:SOT-223-3_TabPin2` |
| C4 | 10 µF X5R 1206 input | `Capacitor_SMD:C_1206_3216Metric` |
| C5 | 10 µF X5R 1206 output | same |

**Critical:** the output 10 µF cap is **required** for stability. Without it the AMS1117 oscillates and the 3.3 V rail looks like a 1 MHz square wave on the scope. Don't skip this.

### 4.5 Decoupling (one cap per IC)
Every active IC needs a **0.1 µF X7R 0603 ceramic** within 5 mm of its VCC pin:
- ESP32 module — 0.1 µF + 10 µF on its 3.3 V pin.
- 74HC595 — 0.1 µF on its VCC.
- CP2102N — 0.1 µF on each of its 3.3 V pins.
- BTS7960 module — already has its own onboard caps; add an extra 100 µF electrolytic at the M+ output for inrush.

---

## Step 5 — Schematic edits per sub-sheet

### 5.1 `MotorDrivers.kicad_sch`
The BTS7960 is a **module** (not raw IC) sold with screw terminals. Use a connector symbol to represent it.

| Connector pin | Net |
|---|---|
| RPWM | GPIO 25 (`PIN_MOTOR_IN1`) |
| LPWM | GPIO 26 (`PIN_MOTOR_IN2`) |
| R_EN | V5_main (always HIGH) |
| L_EN | V5_main |
| R_IS | GPIO 34 (`PIN_MOTOR_VPROPI`) |
| L_IS | tied to R_IS or NC |
| VCC | V5_main |
| GND | GND |
| B+ | V12 (after relay contact, see 5.5) |
| B− | GND |
| M+, M− | screw terminals to motor |

### 5.2 `ESP32Module.kicad_sch`
ESP32-WROOM-32E DevKit slots into two **female 1×19 headers**, 2.54 mm pitch.
| Pin | Function |
|---|---|
| 1 (EN) | Reset (pull-up + tact button) |
| 2 (VP) | GPIO 36 — TOUCH_IRQ |
| 3 (VN) | GPIO 39 — LIMIT_ANYHIT (needs external 10 kΩ pull-up to 3.3 V on this pin) |
| 4 | GPIO 34 — VPROPI |
| 5 | GPIO 35 — DECK_POSITION |
| 6 | GPIO 32 — MOTOR_RELAY |
| 7 | GPIO 33 — TOUCH_CS |
| 8 | GPIO 25 — MOTOR_IN1 |
| 9 | GPIO 26 — MOTOR_IN2 |
| 10 | GPIO 27 — TFT_BL |
| 11 | GPIO 14 — TFT_SCK |
| 12 | GPIO 12 — TFT_MISO |
| 13 | GPIO 13 — TFT_MOSI |
| 14 | GND |
| 15 | GPIO 23 — 595_DATA |
| 16 | GPIO 22 — SERVO_RIGHT / US3_TRIG |
| 17 | GPIO 1 — BUZZER (UART0 TX) |
| 18 | GPIO 3 — US4_ECHO (UART0 RX) |
| 19 | GPIO 21 — 595_OE_N |
| (other side, 1×19 header) ... | ... |

Use `Module:ESP32-DEVKITC-32E` symbol. Confirm pin allocations match `firmware/src/pin_config.h`.

**Add the GPIO 39 external 10 kΩ pull-up here** — without it, the limit-switch input floats. (Internal pull-up not available on input-only pins 34–39.)

### 5.3 `USBUart.kicad_sch`
CP2102N USB-C bridge:
- USB-C connector → CP2102N (RX/TX/DTR/RTS pins) → ESP32 GPIO 1 (TX) and GPIO 3 (RX).
- DTR/RTS to ESP32 EN/IO0 via the standard auto-reset 2-transistor circuit (Q3 + Q4 = MMBT3904, R = 12 kΩ).
- 5 V from USB-C feeds the on-board ESP32 module power line (parallel with the LM2596 5 V output — the higher voltage wins via diode-OR).

Use `Interface_USB:CP2102N-A02-GQFN24` symbol, footprint `Package_DFN_QFN:QFN-24-1EP_4x4mm_P0.5mm_EP2.4x2.4mm`.

> CP2102N is a hand-solder-able QFN with 0.5 mm pitch — challenging but possible with flux + drag soldering. If you don't trust your soldering, swap to a CP2102 in QFN-28 (larger pitch) or CH340 in SOIC-16.

### 5.4 `ShiftRegLEDs.kicad_sch`
74HC595 + 6 LEDs (3 red, 1 yellow, 2 green — adjust per BOM).

| 595 pin | Net | Notes |
|---|---|---|
| 14 (SER) | GPIO 23 (`PIN_595_DATA`) | |
| 11 (SRCLK) | GPIO 18 (`PIN_595_CLOCK`) | |
| 12 (RCLK) | GPIO 19 (`PIN_595_LATCH`) | |
| 13 (OE̅) | GPIO 21 (`PIN_595_OE_N`) | active-LOW |
| 10 (SRCLR̅) | V5 | always HIGH |
| 16 (VCC) | V5 | + 0.1 µF decoupling |
| 8 (GND) | GND | |
| Q0..Q5 | each via 220 Ω SMD 0805 → LED anode | LED cathode to GND |

Use `74xx:74HC595` symbol, footprint `Package_DIP:DIP-16_W7.62mm_Socket` (use a socket — cheap to swap if you fry a chip).

LEDs: `LED:LED_THT` symbol, footprint `LED_THT:LED_D3.0mm`. 220 Ω resistor sets ~10 mA per LED at 5 V.

### 5.5 `ConnectorsSafety.kicad_sch`
This sheet hosts the safety chain. **Read it carefully — incorrect wiring here makes the bridge dangerous.**

#### 5.5.1 E-stop circuit
Mushroom NC button between `PIN_ESTOP_IRQ` (GPIO 0) and GND:
```
+3V3 ─── 10kΩ ─── PIN_ESTOP_IRQ ─── J9 ── (mushroom NC button, common pole) ── GND
                                  └── ESP32 GPIO 0 (BOOT/ESTOP)
```

When the button is **not pressed**, the NC contact is closed → GPIO 0 reads LOW (because the NC connects through to GND).

When the button **is pressed**, the contact opens → GPIO 0 floats up to 3.3 V via the 10 kΩ pull-up → reads HIGH.

The firmware in `interlocks.cpp` line 31 reads `digitalRead(PIN_ESTOP) == HIGH` as "pressed" — matches the wiring above.

> **Why mushroom NC, not NO?** Safety standards: a NC contact that **opens** on emergency means a wire-cut also triggers e-stop (failsafe). A NO contact that **closes** would silently fail open if the wire is cut.

| Refdes | Part | Footprint |
|---|---|---|
| J9 | 2-pin JST-XH 2.50 mm | `Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical` |
| R_estop | 10 kΩ 0805 | `Resistor_SMD:R_0805_2012Metric` |
| Mushroom button (off-board) | XB2-BS542 16 mm panel-mount, NC | KES 350 from Pixel |

#### 5.5.2 Relay + driver circuit
The relay in series with the motor's 12 V supply. When the relay coil is **energised**, motor power is enabled. When **de-energised** (E-stop pressed, fault, or boot), motor power is cut.

```
                                     +12V
                                       │
                                       ▼
                             ┌─── Relay coil (5V) ───┐
                             │                        │
                             │                        ▼
                             ▼                   1N4007 flyback
                           Q1 drain               anode to GND, cathode to coil V+
                       (2N7000 N-MOSFET,          (orientation matters — cathode marked on diode)
                          SOT-23)
                             │
                       ┌─────┴─────┐
                       │           │
                       │ G ── 220 Ω ── PIN_MOTOR_RELAY (GPIO 32)
                       │           │
                       │ S ────── GND
                       └───────────┘

  Relay COMMON pole ──── Motor B+ supply terminal
  Relay NO contact   ──── BTS7960 B+ input
```

| Refdes | Part | Footprint |
|---|---|---|
| K1 | SRD-05VDC-SL-C 5 V relay (10 A contacts) | `Relay_THT:Relay_SPDT_Omron_G5LE-1` (or generic SRD footprint) |
| Q1 | 2N7000 N-MOSFET | `Package_TO_SOT_THT:TO-92` |
| D4 | 1N4007 flyback diode | `Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal` |
| R_q1 | 220 Ω 0805 (gate series) | `Resistor_SMD:R_0805_2012Metric` |

**Verify the flyback orientation.** The diode cathode (band) goes to relay coil V+, anode to coil V−. Reverse this and the inductive kick from de-energising the coil destroys Q1 within seconds.

#### 5.5.3 Barrier servo connectors
Two 3-pin JST-XH connectors for the SG90 servos:
| Pin | Net |
|---|---|
| 1 | V5_main |
| 2 | GND |
| 3 (signal) | GPIO 5 (`PIN_SERVO_LEFT`) for left servo, GPIO 22 (`PIN_SERVO_RIGHT`) for right |

Add a **470 µF electrolytic across the V5/GND on the connector side** — SG90 inrush is brutal, ~1.5 A for 100 ms.

#### 5.5.4 Limit-switch chain
8 KW12-3 microswitches (4 per tower) wired with **diode-OR matrix** to a single GPIO 39 input.

```
Each switch NC contact:
  3.3 V ── (switch) ── 1N4148 anode | cathode ── GPIO 39

10 kΩ pull-down from GPIO 39 to GND  (input-only pin 39 has NO internal pull)

OR use 10 kΩ pull-UP to 3.3 V instead and reverse the diodes — the firmware
treats either polarity as long as the convention is consistent.
```

Use a **single 4-pin JST-XH per tower** (2 wires per switch × 2 switches = 4 wires).

### 5.6 `TFT_Camera.kicad_sch` (NEW — you create this sheet)
This is the v2.1 addition.

#### 5.6.1 Add the new sheet to the project
1. In eeschema, open the root sheet `vertical_lift_pcb.kicad_sch`.
2. Press **S** (or Place → Hierarchical Sheet). Click and drag a rectangle on an empty area to define the sheet box.
3. In the dialog: file name `TFT_Camera.kicad_sch`, sheet name `TFT_Camera`. OK.
4. Double-click the new sheet box. KiCad creates the empty sub-sheet and opens it.

#### 5.6.2 J6 — TFT 14-pin connector
Add `Connector_PinHeader_2.54mm:PinHeader_1x14_P2.54mm_Vertical`.

| TFT pin | Signal | ESP32 GPIO |
|---|---|---|
| 1 | VCC | V5_main (or V3V3 — check your TFT's silkscreen) |
| 2 | GND | GND |
| 3 | CS | GPIO 15 (`PIN_TFT_CS`) — strapping; needs 10 kΩ pull-up to 3.3 V on this PCB |
| 4 | RESET | GPIO 4 (`PIN_TFT_RST`) |
| 5 | DC | GPIO 2 (`PIN_TFT_DC`) — strapping; needs 10 kΩ pull-DOWN to GND on this PCB |
| 6 | MOSI | GPIO 13 (`PIN_TFT_MOSI`) |
| 7 | SCK | GPIO 14 (`PIN_TFT_SCK`) |
| 8 | LED | GPIO 27 via Q-BL backlight MOSFET (see 5.6.4) |
| 9 | MISO | GPIO 12 (`PIN_TFT_MISO`) — strapping; needs 10 kΩ pull-up to 3.3 V on this PCB |
| 10 | T_CLK | tied to GPIO 14 (shared SCK) |
| 11 | T_CS | GPIO 33 (`PIN_TOUCH_CS`) |
| 12 | T_DIN | tied to GPIO 13 (shared MOSI) |
| 13 | T_DO | tied to GPIO 12 (shared MISO) |
| 14 | T_IRQ | GPIO 36 (`PIN_TOUCH_IRQ`) |

Add **decoupling**: 0.1 µF X7R 0603 within 5 mm of the VCC pin, plus 10 µF X5R 1206 within 15 mm.

#### 5.6.3 J7 — ESP32-CAM 4-pin JST-XH connector (the v2.1 addition)
Add `Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical`.

| Pin | Signal | Notes |
|---|---|---|
| 1 | GND | Common ground with main board |
| 2 | V5_CAM | From the CAM-dedicated LM2596 buck (Step 4.3) |
| 3 | CAM RX (unused) | Reserved for future bidirectional protocol |
| 4 | CAM TX → ESP32 GPIO 16 (`PIN_VISION_RX`) | The vision link |

Add **decoupling** at J7:
- 100 nF X7R 0603 from V5_CAM to GND, within 5 mm of pin 2.
- 470 µF / 10 V electrolytic from V5_CAM to GND, within 15 mm. **Required** — the CAM startup inrush will brown out without this bulk capacitance at the connector.

Add a small **3 mm green LED** + 1 kΩ on V5_CAM as a "CAM POWER OK" indicator. This catches a missing buck output during bring-up without needing a multimeter.

#### 5.6.4 Backlight PWM driver (Q-BL)
The TFT has an internal current-limit resistor on its LED line. We switch the LED return to ground via an N-MOSFET driven by GPIO 27 PWM.

```
+5V  ── TFT module pin 8 (LED+) ── [TFT internal: 47Ω + LEDs] ── (LED- return)
                                                                  │
                                                                  ▼
                                                             Q-BL drain (AO3400A SOT-23)
                                                                  │
        GPIO 27 ─── 220 Ω ── G                                   │
                            │                                     │
                            └── 100 kΩ pull-down to GND          │
                                                                  S ── GND
```

| Refdes | Part | Footprint |
|---|---|---|
| Q-BL | AO3400A N-MOSFET (SOT-23-3, Vth ≈ 1.5 V) | `Package_TO_SOT_SMD:SOT-23` |
| R_qbl_g | 220 Ω 0805 gate series | |
| R_qbl_pd | 100 kΩ 0805 pull-down (keeps backlight off if MCU floats during boot) | |

#### 5.6.5 Hierarchical labels
Add hierarchical labels for every signal that crosses the sheet boundary:
- Inputs (from parent): GND, V5_main, V5_CAM, V3V3, GPIO 2, 4, 12, 13, 14, 15, 27, 33, 36, 16
- No outputs from this sheet.

Save the sheet. Press the up-arrow icon to return to root. The new sheet symbol now shows the correct labels on its border.

### 5.7 Run ERC (Electrical Rules Check)
1. In eeschema, top menu **Inspect** → **Electrical Rules Checker** (or `Ctrl+Shift+I`).
2. Click "Run ERC". Wait.
3. Fix every error. Common ones:
   - "Input pin not driven" — usually a label typo or missing connection. Find the pin, trace it.
   - "Two output pins driving the same net" — usually wrong symbol selected.
   - "Power input not driven" — your power flag (`PWR_FLAG`) is missing on a power net. Add `PWR_FLAG` symbols on every supply net (V12, V5, V5_CAM, V3V3, GND).
4. Warnings are okay if you understand them. Document any you intentionally ignore.

---

## Step 6 — PCB layout

Open `vertical_lift_pcb.kicad_pcb`.

### 6.1 Update from schematic
1. Top menu **Tools** → **Update PCB from Schematic** (`F8`).
2. In the dialog: tick **"Re-link footprints by reference"**, **"Delete tracks of removed nets"**, **"Re-fill all zones"**. Click **Update PCB**.
3. New footprints (J6, J7, the second LM2596, all the new caps and the AO3400A) appear unrouted in the lower-left corner of the board.

### 6.2 Layer stack
- **Layer 1 (F.Cu)** — top copper, signals and component-side
- **Layer 2 (B.Cu)** — bottom copper, GND pour + some return paths
- **F.Mask / B.Mask** — solder mask (matte black per project brief)
- **F.Silkscreen / B.Silkscreen** — white silkscreen
- **Edge.Cuts** — board outline (already drawn at 100 × 80 mm)

### 6.3 Trace width rules
File → **Board Setup** → **Design Rules** → **Net Classes**.

| Class | Trace width | Clearance | Use |
|---|---|---|---|
| Default (signal) | **0.25 mm** | 0.20 mm | All 3.3 V signals, GPIO |
| 3V3_Power | **0.40 mm** | 0.20 mm | 3.3 V rail |
| 5V_Power | **1.00 mm** | 0.25 mm | 5 V rail (up to 3 A) |
| 12V_Motor | **1.50 mm** | 0.30 mm | 12 V to motor (up to 2.5 A peak) |
| GND | **0.40 mm** (where not pour) | 0.20 mm | Ground returns not on pour |

Assign nets to classes: in Net Classes dialog, click "Assign to net" → select the matching net pattern (e.g. `V12*` → 12V_Motor).

### 6.4 Antenna keepout
The ESP32-WROOM-32E module antenna sits at the **top edge** of the module. Required keepout: **7 mm × 30 mm of NO COPPER** under the antenna.

1. Switch to the User.Drawings layer.
2. Tools → Add → **Keepout Area**. Click 4 corners to draw a 7 × 30 mm rectangle right under the antenna position on the WROOM module footprint.
3. In the keepout dialog: tick **"Tracks", "Vias", "Copper pour"**. OK.
4. Visual: should appear as a hatched rectangle on the layer.

### 6.5 Layout strategy
1. **Place big things first.** ESP32 module headers in the centre. BTS7960 connector on the right side (high-current edge). DC barrel jack at the top. USB-C at the top-front. Relay near the BTS7960 (short high-current path).
2. **Group by function.** Put both LM2596 bucks together (with their inductors and caps). Put the AMS1117 next to the WROOM module (3.3 V load is on the WROOM).
3. **Connectors on board edges.** TFT (J6) on the left edge so the cable runs cleanly to the panel. ESP32-CAM (J7) on the right edge near the antenna keepout — the CAM is mounted on the camera gantry on the right side of the rig. JST-XH for sensors on the bottom edge (for ribbon to off-board peripherals).
4. **The relay and BTS7960 generate heat.** Place them with 5 mm clearance from each other and from the ESP32 module.

### 6.6 Routing rules
- **45° corners only**, never 90° (acid traps + impedance discontinuity).
- **Shortest path for clock signals** (SPI SCK, 595 SRCLK).
- **Differential treatment for SPI MOSI/MISO/SCK** — keep them parallel, equal length, on the same layer.
- **Cross signals at 90° on different layers** — never run two signals parallel for > 5 mm on opposite layers (capacitive coupling).
- **Ground pour on both layers**, with multiple **stitch vias** (Ø 0.5 mm vias every 5 mm along high-current paths) connecting top and bottom GND.
- **Star-ground at the LM2596 GND pin** — all GND returns converge here, then exit to barrel jack GND via one fat trace.
- **Keep digital signals AT LEAST 5 mm away from the BTS7960 motor traces** — motor PWM at 20 kHz radiates EMI.

### 6.7 Run DRC (Design Rules Check)
1. **Inspect** → **Design Rules Checker** (or `F7`).
2. Click "Run DRC". Wait (large boards take ~30 s).
3. **Goal: 0 errors, 0 unconnected items.**
4. Common errors:
   - "Clearance violation" — two traces too close. Re-route one of them.
   - "Unconnected item" — a net is missing a connection. Trace it back to the schematic.
   - "Hole through hole" — a via overlapping a TH pad. Move the via.
5. Warnings can be ignored if you understand them (e.g. silkscreen overlapping a pad — cosmetic).

### 6.8 3D preview (optional but satisfying)
View → 3D Viewer (`Alt+3`). KiCad renders your board with components. Inspect for layout sanity — anything sticking off the edge, wrong orientation, missing components.

---

## Step 7 — Generate Gerbers + Drill files for JLCPCB

### 7.1 Plot Gerbers
1. PCB editor → **File** → **Plot**.
2. Output directory: `pcb/gerbers/`.
3. Plot format: **Gerber**.
4. **Layers** — tick exactly these:
   - F.Cu, B.Cu
   - F.Mask, B.Mask
   - F.Silkscreen, B.Silkscreen
   - Edge.Cuts
5. **General Options:**
   - ☑ Plot footprint references
   - ☑ Plot footprint values
   - ☐ Plot pad numbers
   - ☑ Use Protel filename extensions
   - Drill marks: **None**
6. Click **Plot**. Watch the log for "Plot complete".

### 7.2 Generate drill files
1. Same dialog → **Generate Drill Files…** button.
2. Drill file format: **Excellon**.
3. Map file format: **Gerber X2**.
4. Drill origin: **Absolute**.
5. Click **Generate Drill File**. Two new files appear: `.drl` (drill) and `.gbr` (drill map).

### 7.3 Verify Gerbers in a viewer
JLCPCB rejects bad files; verify locally first.
1. Open KiCad's built-in **GerbView** (Project Manager → second-to-last button).
2. File → Open Gerber Plot Files → select all `.gbr` files in `pcb/gerbers/`.
3. Visually compare to your PCB layout. Look for missing layers or mirrored silkscreen.

### 7.4 ZIP the folder
1. In Windows File Explorer, navigate to `pcb/gerbers/`.
2. Select all `.gbr`, `.drl` files.
3. Right-click → "Send to" → "Compressed (zipped) folder".
4. Rename to `JLCPCB_order.zip`. **Do NOT include parent folders in the zip** — JLCPCB needs files at the zip root.

---

## Step 8 — Order from JLCPCB

1. Edge → `jlcpcb.com`. Sign up (free).
2. Click **Order PCBs** (top nav).
3. Click **Add gerber file** → upload `JLCPCB_order.zip`. Wait for the 3D preview to render.
4. **Verify the preview** — board outline correct, holes in right place, silkscreen readable.
5. **Settings:**
   - Layers: **2**
   - Dimensions: read automatically (should match 100 × 80 mm)
   - PCB Qty: **5** (cheapest order, gives spares)
   - PCB Color: **matte black** (per project brief)
   - Surface finish: **HASL (with lead)** — cheapest, fine for prototypes
   - Outer copper weight: **1 oz**
   - Material: FR-4 TG130 (default)
   - Min hole size: **0.3 mm** (default)
   - Min track / spacing: **6/6 mil** (default — matches your DRC rules)
   - **Stencil:** No (hand-soldering this batch)
   - Castellated holes: No
   - Edge plating: No
6. Click "Save to Cart" → "Secure Checkout".
7. Shipping: **DHL Pickup Point Nairobi** — fastest. ~7 working days.
8. Pay (international card or M-Pesa via JLCPCB's Kenya partner). Total ≈ **KES 1,900** all-in for 5 boards.

> **Document the order.** Create `docs/procurement.md`:
> ```markdown
> ## JLCPCB Order — 2026-05-18
> Order #: JLC2026051812345
> Tracking: DHL XXXXX
> ETA: 2026-05-25
> ```

---

## Step 9 — Bill of Materials for the order

While the PCB is in transit (5–7 days), order all the parts you'll solder. The BOM lives in `bom/VLB_Group7_BOM.xlsx`. Cross-check against your schematic — every unique footprint must appear in BOM with quantity, package, and Kenya supplier.

Critical SMD parts to source from Pixel Electric or Aliexpress (allow 14 days for Aliexpress):
- LM2596S-5.0 (TO-263-5) ×2 — local KES 220 each.
- AMS1117-3.3 (SOT-223) ×1 — KES 30.
- AO3400A (SOT-23) ×1 — KES 20.
- 2N7000 (TO-92) ×1 — KES 15.
- SS54 / SS34 Schottky (SMA) ×3 — KES 25 each.
- SMBJ16A TVS (SMB) ×1 — KES 35.
- 1N4007 / 1N4148 ×10 — KES 5 each.
- 74HC595 (DIP-16) ×1 + DIP-16 socket — KES 80 + 30.
- CP2102N (QFN-24) — KES 200, may need to swap for CP2102 in QFN-28 if QFN-24 is hand-solder-prohibitive.
- 33 µH 3 A power inductor ×2 — KES 100 each.
- Electrolytic caps: 470 µF / 25 V (×3), 220 µF / 16 V (×2) — KES 20 each.
- SMD ceramic caps + resistors — buy a 0805 + 0603 assortment kit (~KES 800 once, lasts the whole project).

THT parts:
- DC barrel jack panel-mount — KES 50.
- USB-C breakout (or panel-mount USB-C) — KES 150.
- Polyfuse 1.5 A 1812 — KES 80.
- SRD-05VDC-SL-C 5 V relay — KES 100.
- JST-XH connectors + crimp housings (mixed sizes) — KES 400.
- 14-pin 2.54 mm female header for TFT — KES 30.
- 1×19 female headers for ESP32 module (×2) — KES 60.

Verify total against `bom/VLB_Group7_BOM.xlsx` — should sum to KES 24,825 across all five members' BOMs.

---

## Step 10 — Soldering sequence (when boards arrive)

> **Solder lowest-profile components first** so the board sits flat through the process.

### 10.1 Setup your bench
- Anti-static mat under the PCB.
- Soldering iron at 350 °C (60/40) or 380 °C (lead-free).
- Liquid flux applied to first pad area.
- Magnifier loupe under good lamp light.
- Multimeter set to continuity beep, ready.

### 10.2 Order
1. **0805 SMD resistors and caps** (lowest profile). Tin one pad, place R, hold with tweezers, melt the tinned pad. Solder the other pad. Inspect under loupe.
2. **0603 SMD caps** (decoupling). Same technique, tighter — extra flux helps.
3. **SOT-23** (AO3400A, 2N7000-SMD if used). Tin one pad, place, solder other pads.
4. **SOT-223** (AMS1117). Solder the tab first (it's the heat sink — needs good thermal contact). Then solder the other 2 pins.
5. **TO-263-5** (LM2596 ×2). Same — tab first. Use plenty of flux on the big tab — it sinks heat fast.
6. **DIP-16 socket** (74HC595). Through-hole. Solder corner pin first, check seating, solder remaining pins. Don't solder the IC yet — insert into socket later.
7. **TVS, polyfuse, Schottky diodes** (SMA / SMB / 1N4007). Verify diode orientation by silkscreen direction marker.
8. **Power inductors** (33 µH). Big surface-mount, drag-solder under-pads.
9. **Through-hole headers** for ESP32 module (1×19 female ×2). Solder corners first, check the spacing fits the DevKit, solder rest.
10. **Female header for TFT** (1×14). Same.
11. **Polarised connectors** — DC jack, USB-C, JST-XH. Verify orientation against silkscreen.
12. **Relay K1** — through-hole, solder all 5 pins.
13. **Mushroom button leads** — solder long flying leads (300 mm) terminating in JST-XH 2-pin male, plug into J9.
14. **ESP32-WROOM module** — push into headers, **do NOT solder permanently**. You may need to swap the module if it dies.
15. **74HC595** — push into socket.
16. **CP2102N (QFN)** — most challenging. Apply liberal flux, place chip, drag-solder one row at a time. Wick away bridges. Inspect with loupe.

### 10.3 Inter-step verification
After every group, with multimeter in continuity beep mode:
- Touch probe between V12 and GND → must NOT beep (no short).
- Touch probe between V5 and GND → must NOT beep.
- Touch probe between V5_CAM and GND → must NOT beep.
- Touch probe between V3V3 and GND → must NOT beep.
- Touch probe between adjacent IC pins → must NOT beep (no solder bridges).

If anything beeps, **stop**. Find the short with the loupe before applying power.

---

## Step 11 — First power-up procedure (the most important 10 minutes)

> Never plug 12 V into a freshly soldered board cold. Follow this sequence exactly.

### 11.1 Final continuity check (before any power)
Multimeter on continuity beep, all probes from any V+ to any GND:
- 12 V to GND — **must be open** (no beep).
- V5 to GND — **must be open**.
- V5_CAM to GND — **must be open**.
- V3V3 to GND — **must be open**.
- USB 5 V to GND — **must be open**.

If any beeps: short. Find it. Don't proceed.

### 11.2 Bench supply test, no ICs in sockets, no DevKit
1. Bench supply set to **12 V output**, **current limit 100 mA** (very low — this catches shorts safely).
2. Connect to barrel jack input.
3. Observe current draw on supply display.
4. **If current is at the 100 mA limit** → there's still a short. Power off, find it.
5. **If current is < 30 mA** → power tree is healthy at idle. Proceed.

### 11.3 Verify rails with multimeter (still no DevKit)
- 5 V rail node (output of LM2596 main) → should read **5.0 ± 0.1 V**.
- V5_CAM rail node (output of LM2596 CAM) → should read **5.0 ± 0.1 V**.
- 3.3 V rail node (output of AMS1117) → should read **3.30 ± 0.05 V**.

If any rail reads 0 V:
- LM2596 has 4 pins (Vin, GND, Vout, Feedback) plus tab. Verify each is connected to the right net under the loupe.
- AMS1117 — verify Vin pin (3) is connected to V5, GND pin (1) to GND, Vout pin (2) to V3V3.
- Check the LM2596 inductor — broken solder = open circuit = no Vout.

If a rail reads 4.5 V instead of 5 V:
- Adjustable LM2596 trim pot needs adjustment. Tweak with a small screwdriver until 5.0 V.

If a rail oscillates (multimeter jitters between 4 and 5 V):
- Missing output cap on AMS1117 (10 µF X5R required). Add one.

### 11.4 Insert ICs and DevKit
1. Power off bench supply. Wait 30 seconds for caps to discharge.
2. Insert 74HC595 into its socket. Pin 1 (notched corner) matches silkscreen marker.
3. Plug ESP32-WROOM-32E DevKit into its female headers. Verify pin 1 alignment.
4. Power on bench supply, still 100 mA limit.
5. Current should jump to ~80 mA (DevKit boots, USB enumerates).

### 11.5 Verify ESP32 enumerates
1. Plug USB-C into the board's USB-C port.
2. Windows: Device Manager → Ports — should show "USB-SERIAL CH340 (COM<n>)" or "Silicon Labs CP210x (COM<n>)".
3. If nothing shows, the CP2102N has a soldering issue. Reflow with hot air or a fresh iron tip. Re-test.

### 11.6 Flash firmware
1. PlatformIO → vlb_main → Build → Upload. (Hold BOOT button if needed.)
2. Open monitor at 115200.
3. **Expected boot log** (you've memorised this from M1's guide):
   ```
   === VLB Group 7 — Boot ===
   ...
   [wdt] init OK (5s hw, 1.5s sw)
   [fault] init OK (rail monitoring disabled — see known_limitations.md)
   [ilk] init OK (relay off until first OK eval)
   [motor] init OK
   ...
   [fsm] 8 -> 0
   ```

### 11.7 Raise current limit progressively
1. Once boot is clean, raise bench supply current limit to **1 A**.
2. Test motor command via serial (`r ↵` in M1's harness). Motor should attempt to spin.
3. If everything looks clean, raise to **3 A** for full motor under-load operation.

---

## Step 12 — Layered brownout protection (no firmware monitoring in v2.1)

The v2.1 firmware does NOT monitor the rail voltages — that hardware path was removed (see `docs/known_limitations.md` L1). Brownout is now detected by:

1. **ESP32 internal brownout detector** — built-in, trips at ~2.43 V on 3.3 V rail and resets the chip. Serial shows `Brownout detector triggered`.
2. **BTS7960 built-in undervoltage lockout** — trips at ~5.5 V on driver Vcc; the H-bridge stops switching, and FSM sees this as a stall fault (`FAULT_STALL`) within `MOTOR_STALL_TIMEOUT_MS` (2 s).
3. **LM2596 thermal/current limit** — protects the 5 V rail from sustained overload independently of firmware.

To bench-test the layered protection:
1. Drop the bench supply slowly from 12 V toward 0.
2. At ~10.5 V the BTS7960 should stop driving the motor; firmware logs `[fault] raised: stall`.
3. Continue dropping. At some point the ESP32 itself resets — observe the reboot banner. This is expected.

---

## Step 13 — Safety firmware deep dive (already coded — your responsibility to maintain)

### 13.1 `safety/watchdog.cpp`
- **Hardware TWDT:** ESP32 Task Watchdog, 5 s panic.
- **Software watchdog:** 5 task kicks (FSM, motor, sensors, vision, main). Each task records its last-kick timestamp.
- `task_safety` polls timestamps at 20 Hz; if any > 1.5 s old → sets `FAULT_WATCHDOG` and forces SAFE state via `interlocks_force_safe()`.
- **Tune** `WATCHDOG_MAX_INTERVAL_MS` only if a task legitimately needs more headroom (default 1500 ms is conservative).

### 13.2 `safety/fault_register.cpp`
- Aggregates the 16-bit fault flag bitmask from `g_status.fault_flags`.
- Edge-detects rising any flag → posts `EVT_FAULT_RAISED` to FSM.
- Edge-detects falling all flags → posts `EVT_FAULT_CLEARED`.
- `fault_register_first_name(flags)` returns human-readable name for serial logs.
- v2.1 change: rail reads removed; rail_*_volts fields stay at -1.0f sentinel.

### 13.3 `safety/interlocks.cpp`
- E-stop ISR on `PIN_ESTOP` (CHANGE trigger, INPUT_PULLUP). HIGH = pressed (mushroom NC opens).
- Relay control: energise (HIGH on `PIN_RELAY` = GPIO 32) only if `!estop && fault_flags == 0`.
- Barrier servo control via ESP32Servo lib on `PIN_SERVO_L` / `PIN_SERVO_R`.
- Open-loop barrier timing — assumes 1 s reach time. SG90 ≈ 0.6 s for 90°.

### 13.4 Things you tune on real hardware
- ADC current calibration — `ADC_TO_MA_NUMERATOR` / `ADC_TO_MA_DENOMINATOR` in `motor_driver.cpp`. Measure motor current with a multimeter under known load (e.g. 1 A draw), compare to firmware's reported `motor_current_ma`, scale.
- Relay flyback diode polarity — orientation matters; cathode goes to V+ side of coil.
- Barrier servo angles — `BARRIER_DOWN_ANGLE = 0`, `BARRIER_UP_ANGLE = 90` in `system_types.h`. Adjust if your physical barriers don't reach the right positions.

---

## Step 14 — End-to-end safety bench test matrix

Run all 5 tests before signing off. Record each in `docs/integration_log.md`.

| # | Test | Method | Pass criteria |
|---|---|---|---|
| 1 | E-stop hard cut | Bridge mid-cycle (raising). Press mushroom button. | Motor stops within 50 ms (verify with high-speed video or scope on motor terminals). FSM enters STATE_ESTOP. |
| 2 | E-stop recovery | Twist mushroom to release. Tap CLEAR on HMI. | FSM returns to STATE_IDLE. Bridge fully operable again. |
| 3 | Watchdog | Add `delay(5000)` temporarily in `task_motor`'s loop. Build + flash. | `[wdt] HUNG: motor=...` log appears. FSM forced to STATE_FAULT. Remove the delay and reflash. |
| 4 | Overcurrent | Hold the deck firmly mid-travel during a raise. | Motor current rises (visible on telemetry). At ~2200 mA, `FAULT_OVERCURRENT` raised. FSM enters STATE_FAULT. |
| 5 | Stall | Block the deck so the motor cannot move. Issue raise command. | After 2 s, `FAULT_STALL` raised. FSM enters STATE_FAULT. |

---

## Step 15 — Weekly milestones

| Week | Goal | Sign-off |
|---|---|---|
| 1 | KiCad project read end-to-end. ESP32-CAM-related sheet additions designed (paper sketch). | Schematic plan reviewed by M1 |
| 2 | Schematic edits done (J6, J7, U2B for CAM buck, AO3400A backlight, dividers REMOVED). ERC clean. | Peer-reviewed by M1 |
| 3 | PCB layout updated. Antenna keepout enforced. DRC clean. Gerbers generated. | Files in `pcb/gerbers/` |
| 4 | Order placed at JLCPCB. Component BOM cross-checked. | Order # in `docs/procurement.md` |
| 5 | Boards arrive. SMD assembly done. First power-up: 5 V / V5_CAM / 3.3 V rails verified. | Voltage measurements logged |
| 6 | DevKit + peripherals plug in. Boot banner clean. ESP32-CAM gets clean 5 V from V5_CAM rail. | Serial log committed; CAM boots without brownout loop |
| 7 | All 5 safety bench tests (Step 14) pass. | `docs/integration_log.md` updated |
| 8 | Demo polish: cable management, label connectors, photo of finished board (top + bottom). | Final photo committed |

---

## Step 16 — Failure recovery cookbook

| Symptom | Most likely cause | First fix to try |
|---|---|---|
| Bench supply hits current limit at 12 V power-on | Short on V12 or 5 V rail | Power off. Visually inspect under loupe. Use multimeter to find the short — touch probe to V+, slide along traces, listen for beep. |
| 5 V rail reads 4.6 V | Adjustable LM2596 trim pot mis-set | Tweak trim pot until 5.0 V output |
| 3.3 V rail oscillates | Missing 10 µF on AMS1117 output | Add 10 µF X5R 1206 cap at output pin |
| ESP32 doesn't enumerate over USB | DevKit module not seated, or CP2102N solder issue | Re-seat DevKit. Reflow CP2102N pins under loupe. |
| Relay clicks then immediately drops | Coil voltage sag during pull-in | Bump bulk cap on V5 rail to 1000 µF |
| E-stop press doesn't kill motor | Relay NC contact wired in wrong line | Verify NO contact in series with **motor V+** trace, NOT in series with the gate signal |
| Watchdog fault every few minutes | Vision task slow (UART blocking) | `WATCHDOG_MAX_INTERVAL_MS * 2` for vision (already done in code — verify) |
| Buzzer silent | LEDC channel collision | Confirm ch0/1 = motor, ch2 = backlight, ch3/4 = servos, ch5 = buzzer (all unique) |
| ESP32-CAM brownout loops with rest of system | CAM still on shared 5 V | Verify J7 pin 2 net is `V5_CAM` (separate buck), not `V5` |
| GPIO 39 reads HIGH constantly | Floating input (no internal pull-up on 34-39) | Verify external 10 kΩ pull-up to 3.3 V on PCB |
| Backlight on full brightness, no PWM control | Q-BL gate floating | Verify 100 kΩ pull-down on Q-BL gate is populated |

---

## Completion checklist

PCB design:
- [ ] Schematic ERC: 0 errors, 0 unjustified warnings.
- [ ] PCB DRC: 0 errors. Antenna keepout enforced.
- [ ] Gerbers in `pcb/gerbers/` zipped + JLCPCB order placed.
- [ ] BOM `bom/VLB_Group7_BOM.xlsx` reflects every part on the board (no missing footprints, no unused inventory).
- [ ] CAM-dedicated 5 V buck designed, schematic captured, in BOM.
- [ ] J7 ESP32-CAM 4-pin connector with 470 µF bulk cap and 100 nF decoupling.
- [ ] AO3400A backlight MOSFET + 100 kΩ pull-down + 220 Ω gate series.
- [ ] V12 / V5 / V5_CAM / V3V3 power flag symbols on the schematic.
- [ ] External 10 kΩ pull-up on GPIO 39 (LIMIT_ANYHIT).

Bring-up:
- [ ] Power-on test log: 12 V / V5 / V5_CAM / V3V3 rails within ±5 %.
- [ ] ESP32-CAM POWER OK indicator LED lights when V5_CAM is up.
- [ ] CAM boots without brownout loops on its dedicated rail.
- [ ] Boot banner reaches `[fsm] 8 -> 0` clean.

Safety:
- [ ] E-stop test: pressing mushroom kills motor in < 50 ms.
- [ ] Watchdog test: `delay(5000)` in motor task triggers `[wdt] HUNG: motor=...`.
- [ ] Overcurrent test: stall motor → `FAULT_OVERCURRENT` raised in < 100 ms.
- [ ] Stall test: blocked deck → `FAULT_STALL` after 2 s.
- [ ] Layered brownout test: rail drop → BTS7960 UVLO → ESP32 brownout reset (all expected to fire).

Documentation:
- [ ] `docs/05_electronics_design.md` updated with v2.1 changes (CAM-dedicated buck, J7, removed dividers).
- [ ] `docs/procurement.md` records JLCPCB order # + tracking.
- [ ] `docs/build_log/` has photos of: bare board, populated top, populated bottom, integrated into rig.
- [ ] Cable strain reliefs on every external connector (TFT, CAM, motor, servos, mushroom).
