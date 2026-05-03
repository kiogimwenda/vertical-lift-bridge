# Member 2 — Eugene — Mechanism, Motor, Counterweight Simulation

> **Your role.** You own the physical bridge — every printed part, every screw, every cable, every counterweight, the motor, the motor driver, and the simulated dynamic counterweight system. If something moves (or fails to), it's yours.
>
> **Your single deliverable.** A mechanical bridge that, when M1's FSM commands `MOTOR_UP` at 67% duty, raises the deck 175 mm in 6–8 seconds, hits the top limit switch within ±3 mm of intended position, brakes cleanly, and reverses on `MOTOR_DOWN` for the same distance in 5–7 seconds. Plus a software simulation of pump/drain water tanks that posts `EVT_CW_READY` to M1's FSM when both tanks reach their target.

---

## 0. What you are building (read this once before touching anything)

The bridge has two vertical guide towers, one motor with a cable drum, four guide pulleys, two physical counterweights (lead-shot boxes), and a road deck that slides up and down on linear rails. The motor pulls a cable that runs over the top pulleys; as the deck rises, the counterweights on the other end of the cable fall. Counterweight mass is matched to deck mass so the motor needs only to overcome friction and acceleration.

**Two counterweight systems coexist:**
- **Physical:** static lead-shot boxes (~120 g each) — these provide the actual mechanical balance. Always present, never modified by firmware.
- **Simulated:** a software-only model of dynamic pump-and-drain water tanks. Pure HMI/demo theatre — exists only in `firmware/src/counterweight/counterweight.cpp` and is animated on the TFT. Posts `EVT_CW_READY` to the FSM when balanced; this gates the bridge raise. **No physical pumps, valves, or sensors exist.**

---

## Scope of your work — every file/asset you touch

| Asset | Where | Status |
|---|---|---|
| 13 OpenSCAD source files | `cad/scad/*.scad` | ✅ exist — you may parameterise further |
| 13 STL files | `cad/stl/*.stl` | ✅ exist — you slice + print |
| `cad/README_cad.md` | `cad/` | ✅ exists — you maintain print order + slicer profile |
| `firmware/src/motor/motor_driver.{h,cpp}` | firmware | ✅ exists — you tune `CAL_COUNTS_PER_MM` and PWM duty defaults |
| `firmware/src/counterweight/counterweight.{h,cpp}` | firmware | ✅ exists — you tune fill/drain rates |
| `firmware/src/pin_config.h` (motor section only) | firmware | M1 owns the file, you own the motor pins (GPIO 25, 26, 34, 35, 39) |
| Mechanical assembly photos | `docs/build_log/` | ❌ create — photo every step, JPEG, < 2 MB each |
| `docs/cal_log.md` | `docs/` | ❌ create — record CAL_COUNTS_PER_MM measurement runs |

**Not your work:** PCB (M5), traffic lights (M4), vision (M3). You consume the `g_motor_cmd_queue` (FreeRTOS queue M1 created), apply the command via the BTS7960, and write back to `g_status.deck_position_mm`, `g_status.motor_current_ma`, `g_status.top_limit_hit`, `g_status.bottom_limit_hit`.

---

## Step 1 — Set up your development machine (Windows 11)

Follow **steps 1.1 to 1.7 of M1's guide** first (VS Code, Git, Python 3.11, PlatformIO, CH340 driver, repo clone). Come back here when done.

### 1.8 Install OpenSCAD (you need this even if you only edit existing .scad files)
1. Open Edge. Navigate to `openscad.org/downloads.html`.
2. Click "Windows 64-bit MSI" (the second link). File `OpenSCAD-2021.01-x86-64.msi` lands in Downloads.
3. Double-click. Click Next, accept licence, take all defaults, Install. (Windows may warn "Unrecognised app" — click "More info" → "Run anyway".)
4. Launch OpenSCAD once to confirm it opens. You should see a blank editor on the left and a 3D view on the right with a yellow X-Y-Z axis indicator. Quit.

### 1.9 Install PrusaSlicer (your slicer)
The slicer profile in `cad/README_cad.md` is written for PrusaSlicer 2.6+.
1. Edge → `prusa3d.com/page/prusaslicer_424/`.
2. Click "Download Windows Installer" (top right). File `PrusaSlicer-2.7.x-win64.exe`.
3. Run. Defaults. Install.
4. On first launch the **Configuration Wizard** opens:
   - Click **"Other vendors"**.
   - Tick **"Generic"**.
   - Pick **"Prusa-style 0.4 mm nozzle generic FDM printer"**.
   - Filament profile: tick **PLA**.
   - Skip the network setup at the end.

### 1.10 Open the CAD files (verify your installation)
1. In VS Code, open the project folder (File → Open Folder → `vertical-lift-bridge`).
2. In the Explorer pane on the left, expand `cad/scad/`.
3. Right-click `tower_left.scad` → "Reveal in File Explorer". Windows opens File Explorer with the file selected.
4. **Double-click `tower_left.scad`.** If Windows opens it in an OpenSCAD editor with a yellow tower preview on the right, you're good. If it opens in Notepad or VS Code instead:
   - Right-click → Open With → Choose another app → scroll to **OpenSCAD** → tick **"Always use this app to open .scad files"** → OK.

### 1.11 Acquire physical components (you cannot start without these)
Order from the BOM (`bom/VLB_Group7_BOM.xlsx`) by **end of week 0**. Critical mechanical parts with long lead time:
- 1 kg PLA filament (any colour, but **black or grey looks more professional in the demo video**) — ~KES 1,200 from Pixel Electric, Luthuli Avenue.
- JGA25-370 12 V geared DC motor, 100 RPM variant — ~KES 950, Pixel Electric.
- 4 × 608ZZ skateboard bearings (Ø8 × 22 × 7 mm) — ~KES 100 from any bearing shop in Industrial Area.
- 2 × MGN12 linear rail with carriage, 250 mm length — ~KES 1,300 each, K-Technics or Aliexpress (allow 2 weeks if Aliexpress).
- M3 brass heat-set inserts, pack of 50 — ~KES 350.
- 1 mm braided steel cable, 5 m — ~KES 200.
- M3 × 8/12/16 socket cap screws (mixed pack of 100) — ~KES 400.
- M8 × 80 axle bolt with locknut — ~KES 50.
- 2 × KW12-3 microswitches (limit switches) — ~KES 50 each.
- 1 × BTS7960 motor driver module — ~KES 750 from Pixel Electric.
- 2 × SG90 9 g servos — ~KES 200 each.

If your BOM total looks different from the README's KES 24,825, there's been drift — flag M5 immediately.

---

## Step 2 — Read every CAD file before slicing anything

You can save days of failed prints by reading the source first. Open each `.scad` file in OpenSCAD and look at the `// === PARAMETERS ===` block at the top. Hit F5 to compile and F6 to render.

| File | What it makes | Watch for |
|---|---|---|
| `tower_left.scad` | Left vertical guide tower | `tower_height = 220` mm — confirm matches your MGN12 rail length |
| `tower_right.scad` | Right vertical guide tower | Mirror of left — confirm symmetric |
| `bridge_deck.scad` | The lifting deck | `deck_length = 300`, `deck_width = 100`. Holes for cable anchor |
| `cable_drum.scad` | Motor cable spool | `drum_diameter = 30` mm — **this drives the counts/mm calibration** |
| `pulley_top.scad` | Top pulley wheel | Bore Ø8 mm for 608ZZ bearing — exact fit, no slop |
| `pulley_bracket.scad` | Pulley axle holder | M8 hole — match your axle bolt diameter |
| `counterweight_box.scad` | Lead-shot container | Designed for 4 × 30 g lead pieces = 120 g per box |
| `motor_mount.scad` | JGA25-370 motor cradle | M3 hole pattern at 12 × 18 mm — verify against your motor |
| `barrier_arm.scad` | Servo barrier arm (×2) | Mounts directly on SG90 servo horn |
| `barrier_base.scad` | Servo base | M3 inserts |
| `tft_bezel.scad` | TFT display frame | 35° tilt — tuned for seated viewing |
| `pcb_standoffs.scad` | PCB mounting boss | Centre-to-centre 92 × 67 mm |
| `cable_keeper.scad` | Strain-relief clip | Pinches USB / power cable |

### 2.1 Why parameterised CAD matters
Every `.scad` file uses named variables at the top instead of magic numbers. If your motor shaft is 5 mm instead of the assumed 6 mm, you do not edit the geometry — you change `motor_shaft_d = 6` to `motor_shaft_d = 5` and re-render. This habit is what makes parametric CAD valuable; do not bypass it by editing inside the geometry blocks.

---

## Step 3 — Print queue — start week 1, finish by end of week 2

Total ≈ **70 hours print time, ~1.05 kg PLA** at the slicer settings below. Do not start all prints at once — print in waves so you can begin assembly while later parts are still printing.

### Wave 1 — start of week 1 (≈ 18 hours print time)
1. `tower_left.stl` (~6 hr)
2. `tower_right.stl` (~6 hr)
3. `motor_mount.stl` (~3 hr)
4. `pcb_standoffs.stl` ×1 print (4 standoffs in one print, ~3 hr)

### Wave 2 — middle of week 1 (≈ 22 hours)
5. `bridge_deck.stl` (~10 hr)
6. `pulley_bracket.stl` ×4 (one print, multiplied in slicer, ~6 hr)
7. `pulley_top.stl` ×4 (one print, ~6 hr)

### Wave 3 — start of week 2 (≈ 18 hours)
8. `cable_drum.stl` (~3 hr)
9. `counterweight_box.stl` ×2 (~6 hr)
10. `barrier_arm.stl` ×2 (~3 hr)
11. `barrier_base.stl` ×2 (~6 hr)

### Wave 4 — middle of week 2 (≈ 12 hours)
12. `tft_bezel.stl` (~6 hr)
13. `cable_keeper.stl` ×4 (~6 hr)

### 3.1 Slicer settings (PrusaSlicer 2.7+, Generic 0.4 mm nozzle, PLA)
Open PrusaSlicer. Drag in the STL. In the right panel:

| Setting | Value | Why |
|---|---|---|
| Layer height (towers, motor mount, deck) | **0.20 mm** | Strength + speed |
| Layer height (TFT bezel, barrier arms) | **0.16 mm** | Visible — better surface |
| Infill type (towers) | **Gyroid** | Best stiffness:weight |
| Infill density (towers) | **40%** | Towers see torque from cable tension |
| Infill density (everything else) | **20%** | Sufficient |
| Perimeters / Walls (towers) | **3** | Wall stiffness > infill |
| Perimeters (others) | **2** | Default |
| Top + bottom layers | **5** | Watertight surface |
| Supports | **Only on overhangs > 55°** | Most parts have none |
| Brim (towers only) | **5 mm** | Tall + narrow → adhesion |
| Brim (everything else) | **0** | Wastes filament |
| Filament temp | **210 °C** nozzle, **60 °C** bed | Default PLA |
| Print speed (outer perimeter) | **60 mm/s** | Surface quality |
| Print speed (infill) | **80 mm/s** | Time |
| First layer speed | **20 mm/s** | Bed adhesion |

Save these as a **PrusaSlicer profile** ("Print Settings" → "Save current"): name it `VLB-PLA-0.4`.

### 3.2 Slicing each file
1. Drag STL into PrusaSlicer plate.
2. Pick your `VLB-PLA-0.4` profile.
3. For multi-quantity (e.g. ×4 brackets): right-click on the part in 3D view → Settings → Multiply → 4.
4. Click "Slice now" (bottom right). Watch the time estimate — if it's wildly different from the table above, something is wrong with your settings.
5. Click "Export G-code" → save to USB stick → load on your printer.

### 3.3 Post-processing per print
- **Test-fit M3 brass inserts in every hole BEFORE bulk printing.** Adjust `m3_insert_d = 4.2` in OpenSCAD if too tight (inserts should slide in halfway by hand, then need heat to seat fully).
- **Set inserts with a soldering iron at 220 °C, 5 seconds push.** Do not push harder than necessary or you'll warp the surrounding plastic.
- **Cable drum:** ream the 6 mm bore with a hand drill if motor shaft is tight. Do NOT force — splits the keyway slot.
- **Tower bores for MGN12 carriage:** file lightly with a flat needle file until carriage slides without binding. Test by holding tower vertical and moving carriage by hand — it should slide under its own weight when tilted 45°.

### 3.4 Photo-document every print
Make `docs/build_log/` directory. After each print, take a photo of the finished part on a clean background. Filename format: `YYYY-MM-DD_part_name.jpg`. Commit weekly.

---

## Step 4 — Mechanical assembly sequence (week 2)

> **Photo-document every step.** If you cannot reproduce the assembly from the photos alone, you have not photographed enough.

### 4.1 Base
1. Cut MDF base **1200 × 600 × 12 mm** with a hand saw or jigsaw. Sand edges.
2. Mark tower centres: 250 mm apart, equidistant from base centreline, 100 mm from front edge.
3. Drill M3 pilot holes (2.5 mm bit) at the marked tower mounting positions.

### 4.2 Towers (vertical alignment is critical)
1. Slide MGN12 **carriage onto rail BEFORE mounting rail to tower**. The carriage cannot be added once both ends of the rail are bolted in — you must dismantle to retrofit.
2. Bolt MGN12 rail to inside face of tower with M3 × 8 socket caps through the threaded inserts. Use blue threadlocker (Loctite 243).
3. Mount tower vertically to base with **two L-brackets per tower** (one front, one back), M5 × 25 wood screws into MDF.
4. **Square the towers with a try-square against the base.** Within 1° of vertical or counterweights will bind. If the deck won't slide freely later, this is the first thing to check.

### 4.3 Top pulleys
1. Press 608ZZ bearings into pulley wheels by hand. They should be a tight fit — if loose, add a single drop of CA glue around the perimeter.
2. Bolt a `pulley_bracket` to the top of each tower (M3 × 12, into inserts).
3. Insert M8 × 80 axle bolt through 4 pulleys (2 per tower) and both brackets. Torque the locknut just enough that pulleys spin freely with no axial play.
4. Hand-spin a pulley — should rotate 5+ revolutions when flicked.

### 4.4 Motor + cable drum
1. Bolt the motor to `motor_mount` with M3 × 8.
2. Slide `cable_drum` onto the motor shaft. Secure with the M3 grub screw on the shaft flat (the flat is the ground-down side of the D-shaft).
3. Mount `motor_mount` to the base on the **right tower side**, with the drum axis aligned with where the cable will run vertically.

### 4.5 Cable routing
This determines whether the deck rises smoothly or jams.

1. Cut **two 1 m lengths** of 1 mm braided steel cable.
2. Crimp loop terminals on one end of each (use copper crimp tubes, squeeze with crimping pliers — eye should not slip out under hard pull).
3. **Anchor each cable to the deck** through M3 eyelet on the top edge of the deck. One cable per side.
4. **Route each cable up over the inner pulley → across the top of the tower → down over the outer pulley → terminate at the counterweight box** with another crimped loop.
5. **Drum cable:** cut a third length, 0.6 m. Loop one end through the drum cross-hole, wind 6 turns onto the drum, terminate the other end at the centre-top of the deck.
6. **Winding direction matters:** when the motor turns CW (viewed from the front), the deck must rise. If it lowers instead, reverse `MOTOR_UP` and `MOTOR_DOWN` in `motor_driver.cpp`'s switch statement, OR re-wind the cable from the other side of the drum.

### 4.6 Counterweights
1. Each `counterweight_box` holds 4 × 30 g lead pieces = 120 g target per box.
2. Pour lead shot in. Hot-glue the lid closed (lead shot rolls everywhere if the box opens).
3. Hang on the cable terminals. The boxes should hang freely with ~50 mm clearance from the tower face.
4. **Note:** the firmware ALSO simulates a dynamic counterweight system (the water tanks on the HMI). The static lead boxes are the real thing; the simulation is for HMI demo theatre. They don't physically interact.

### 4.7 Limit switches (KW12-3 microswitches)
**You only have ONE shared limit signal pin** — `PIN_LIMIT_ANYHIT` (GPIO 39). Both top and bottom switches feed into it via a diode-OR network on the PCB (M5's design).

**For breadboard prototyping** (before the PCB arrives):
1. Mount one KW12-3 at top of left tower (where the deck arrives at full height).
2. Mount the other at bottom of left tower (where the deck rests).
3. Wire NC contacts of **both** switches in parallel through 1N4148 diodes (cathodes together at GPIO 39).
4. Add a **10 kΩ pull-up resistor** from GPIO 39 to 3.3 V. **GPIO 39 is input-only and has no internal pull-up — without this external resistor, the input floats and registers spurious limit hits.**

The firmware reads `PIN_LIMIT_ANYHIT` and infers top-vs-bottom by the deck position from the potentiometer (above midpoint = top, below = bottom). See `motor_driver.cpp` lines 117–128.

### 4.8 Manual smoke test (motor power OFF)
1. With motor power disconnected, lift the deck by hand.
2. The deck should rise smoothly. Counterweights should fall as deck rises.
3. Lower deck slowly by hand. Counterweights rise.
4. If the deck binds at any point:
   - Re-square the towers (Step 4.2).
   - Re-tension the cables (loops should be tight, no slack).
   - Deburr the rails with a needle file.
   - Check counterweight boxes are not touching the tower face.

You **must** pass this test before applying motor power. A binding deck under motor torque shears teeth off the gearbox.

---

## Step 5 — Wire up the BTS7960 motor driver

### 5.1 BTS7960 module pinout
The BTS7960 dual H-bridge "red" module sold in Nairobi has 8 pins on the logic side:

| Pin | Connect to | Notes |
|---|---|---|
| RPWM | ESP32 GPIO 25 (`PIN_MOTOR_IN1`, `PIN_MOT_RPWM`) | Forward/up PWM input |
| LPWM | ESP32 GPIO 26 (`PIN_MOTOR_IN2`, `PIN_MOT_LPWM`) | Reverse/down PWM input |
| R_EN | 5 V (always enabled) | Right-half enable — tie HIGH |
| L_EN | 5 V (always enabled) | Left-half enable — tie HIGH |
| R_IS | ESP32 GPIO 34 (`PIN_MOTOR_VPROPI`, `PIN_MOT_VPROPI`) | Current sense out (active high, 8.5 mV/A typical) |
| L_IS | (leave open or tie to R_IS) | Mirror — not used |
| VCC | 5 V | Logic supply |
| GND | GND | Common ground with ESP32 |

And the power side (screw terminals):

| Terminal | Connect to |
|---|---|
| B+ | 12 V power input |
| B− | GND |
| M+ | Motor wire (red) |
| M− | Motor wire (black) |

### 5.2 Breadboard wiring (before the PCB exists)
1. Place the BTS7960 module on a piece of cardboard or a heatsink (it gets warm under load, ~50 °C).
2. Connect logic side to the ESP32 per the pinout above. Use **22 AWG hookup wire**, length < 30 cm.
3. Connect 12 V supply to B+ / B−. Use a **bench supply current-limited to 1 A initially**.
4. **GROUND BOTH SUPPLIES TOGETHER.** ESP32 GND ↔ BTS7960 GND ↔ bench supply GND. Without common ground, current sense reads garbage.
5. Connect motor wires to M+ and M−. Polarity decides direction; you'll fix it in firmware if reversed.

### 5.3 Smoke test the wiring (still no firmware changes)
1. Power the bench supply on at 12 V, current limit 1 A.
2. Power the ESP32 via USB.
3. Open serial monitor. The boot banner should print as before. The motor should NOT spin.
4. If the motor spins by itself the moment you power on: PWM pins are floating — confirm GPIO 25 / 26 are listed as OUTPUT in `motor_driver_init()`.

---

## Step 6 — Encoder / position feedback (potentiometer)

The current design uses a **10 kΩ linear potentiometer** as the deck position sensor (NOT a Hall encoder, despite the historical naming `PIN_ENC_A` aliased to `PIN_DECK_POSITION` = GPIO 35).

### 6.1 Wire the pot
1. Pot has 3 pins: 1 (one end of resistive track), 2 (wiper), 3 (other end).
2. Pin 1 → GND.
3. Pin 3 → 3.3 V.
4. Pin 2 (wiper) → ESP32 GPIO 35.
5. **Mechanically couple the pot shaft to the deck travel.** Two simple options:
   - String + spring: a string from the deck wraps around the pot shaft (turn the pot through its full ~270° as deck travels 175 mm).
   - Linear pot: a slide pot with the slider attached to the deck.

### 6.2 Read the pot in code
The firmware reads `analogRead(PIN_DECK_POSITION)` in `motor_driver_tick()`. The conversion to mm requires `CAL_COUNTS_PER_MM` — measured in Step 7.

---

## Step 7 — Motor calibration: `CAL_COUNTS_PER_MM`

This is the single most important number in your firmware. The placeholder in `motor_driver.cpp` line 35 is **42** — you must re-measure on hardware.

### 7.1 The calibration sketch (temporary file — delete after)
Create `firmware/src/motor/cal_sketch.cpp`:

```cpp
// CALIBRATION SKETCH — replace the regular firmware entry with this
// during calibration. After measurement, delete this file.
#include <Arduino.h>
#include "../pin_config.h"

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Setup PWM
    ledcSetup(0, 20000, 13);  // ch0, 20 kHz, 13-bit
    ledcSetup(1, 20000, 13);  // ch1
    ledcAttachPin(PIN_MOT_RPWM, 0);
    ledcAttachPin(PIN_MOT_LPWM, 1);
    ledcWrite(0, 0);
    ledcWrite(1, 0);

    // Setup ADC for pot
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_DECK_POSITION, ADC_11db);

    Serial.println("=== CAL ===");
    Serial.println("3 sec to start. Mark deck position now.");
    delay(3000);

    int pot_before = analogRead(PIN_DECK_POSITION);
    Serial.printf("pot_before = %d\n", pot_before);

    Serial.println("Motor UP for 3 seconds.");
    ledcWrite(0, 5500);  // 67% duty
    delay(3000);
    ledcWrite(0, 0);

    delay(1000);  // settle
    int pot_after = analogRead(PIN_DECK_POSITION);
    Serial.printf("pot_after  = %d\n", pot_after);
    Serial.printf("delta      = %d counts\n", pot_after - pot_before);
    Serial.println("Now measure deck travel in mm with a ruler. Compute counts/mm.");
}
void loop() { delay(1000); }
```

### 7.2 Procedure
1. Move the deck to ~50 mm above the bottom limit. Mark its current position with a piece of masking tape on the tower.
2. Build + flash the calibration sketch. (PlatformIO → Build → Upload.)
3. Open monitor. The motor will run UP for 3 seconds.
4. **Press the bench supply EMERGENCY OFF** if the deck approaches the top limit. Better to abort than to grind.
5. Note the printed `delta` (counts).
6. Measure deck travel in mm with a steel ruler from the tape mark to the new deck position.
7. Compute: `CAL_COUNTS_PER_MM = delta / measured_mm`. Round to nearest integer.
8. Repeat 3 more times (lower the deck back down by hand each time). Average all 4 measurements.
9. **Record every run in `docs/cal_log.md`:**
   ```markdown
   ## 2026-05-15 calibration
   Run 1: delta = 1320, mm = 95, c/mm = 13.9
   Run 2: delta = 1295, mm = 92, c/mm = 14.1
   Run 3: delta = 1310, mm = 93, c/mm = 14.1
   Run 4: delta = 1300, mm = 92, c/mm = 14.1
   Mean: 14.05  →  CAL_COUNTS_PER_MM = 14
   ```

### 7.3 Apply the calibration
Edit `firmware/src/motor/motor_driver.cpp` line 35:
```cpp
#define CAL_COUNTS_PER_MM      14      // measured 2026-05-15, see docs/cal_log.md
```
Delete `cal_sketch.cpp`. Rebuild, flash. Confirm normal firmware boots.

### 7.4 Verify the calibration
1. With normal firmware running, open the serial harness from M1's guide.
2. Type `r` ↵ to raise. Watch monitor output of `g_status.deck_position_mm`.
3. The reading should reach exactly **175 mm** (= `DECK_HEIGHT_MAX_MM`) when the top limit switch trips.
4. Tolerance: ±3 mm. If consistently off by more than that, re-calibrate.

---

## Step 8 — Tuning motor PWM duty

Defaults in `system_types.h`:
```c
#define MOTOR_PWM_RAISE_DEFAULT     5500    // ~67%
#define MOTOR_PWM_LOWER_DEFAULT     4500    // gravity assist, lighter duty
```

Tune by observation:
| Symptom | Adjustment |
|---|---|
| Deck rises too fast (< 4 s for 175 mm) | Drop `RAISE_DEFAULT` by 500 |
| Deck rises sluggishly or stalls near top | Bump `RAISE_DEFAULT` by 500 |
| Lowering takes > 8 s | Bump `LOWER_DEFAULT` by 500 |
| Lowering runs away (free-fall feel) | Bump `LOWER_DEFAULT` by 1000 (more BTS7960 brake torque) |

Target: **6–8 s up, 5–7 s down**. Update the values in `system_types.h`, commit with a message like `tune(motor): raise=5000 lower=5000 — measured 6.8s up, 6.1s down`.

---

## Step 9 — Counterweight simulation (the software part)

### 9.1 What the simulation models
Read `firmware/src/counterweight/counterweight.cpp`. It models two virtual water tanks (left, right). Each tank has:
- `water_level_ml` (float, 0..150)
- `target_ml` (float, set by `counterweight_set_target()`)
- `pump_active` / `drain_open` (booleans)

Each tick (50 ms, 20 Hz, called from `task_counterweight`):
- If pump active: `water_level += FILL_RATE * dt` until target reached.
- If drain open: `water_level -= DRAIN_RATE * dt` until target reached.
- Posts `EVT_CW_READY` to `g_event_queue` on rising edge of `balanced` (both tanks within ±2 ml of target).

### 9.2 Tunables in `system_types.h`
```c
#define CW_TANK_CAPACITY_ML        150.0f
#define CW_SIM_FILL_RATE_ML_PER_S  30.0f    // → 4 s to fill from 0 to 120 ml
#define CW_SIM_DRAIN_RATE_ML_PER_S 40.0f    // → 3 s to drain from 120 to 0
#define CW_SIM_TARGET_DEFAULT_ML   120.0f   // matches static lead weight in g
#define CW_SIM_TOLERANCE_ML        2.0f
```

The default fill rate of 30 ml/s means the bridge waits ~4 s in `STATE_ROAD_CLEARING` for the simulated counterweights to reach target before raising. **This delay is intentional — it gives the HMI time to animate the tank-fill, which the lecturer sees as "the bridge is preparing".**

### 9.3 Tuning the simulation
- Want a snappier demo? Increase `CW_SIM_FILL_RATE_ML_PER_S` to 60 (2 s to balance).
- Want more visible animation? Decrease to 15 (8 s to balance).
- Tank capacity 150 ml is arbitrary — can be anything; just ensure `CW_SIM_TARGET_DEFAULT_ML < CW_TANK_CAPACITY_ML`.

### 9.4 Verifying the simulation works
1. Boot the firmware with no peripherals (just ESP32).
2. Type `v` ↵ in monitor. FSM enters ROAD_CLEARING.
3. Watch monitor for `[fsm] 1 -> 2` after ~4 seconds. That's `EVT_CW_READY` arriving.
4. If it never arrives:
   - Check `task_counterweight` was created in main.cpp's `setup()`.
   - Add a temporary `Serial.printf` in `counterweight_tick()` to see if it's running.

---

## Step 10 — End-to-end mechanical bring-up (week 5)

This is the moment you've been building toward.

### 10.1 Pre-flight
- Bridge fully assembled, towers square, cables tensioned, manual lift smooth.
- Motor wired to BTS7960 wired to ESP32 per Step 5.
- Limit switches wired with diode-OR + pull-up per Step 4.7.
- Pot wired per Step 6.
- `CAL_COUNTS_PER_MM` measured per Step 7.
- PWM duty tuned per Step 8.
- Bench supply at 12 V, current limit 2 A.

### 10.2 First run
1. Power on ESP32 via USB. Confirm boot banner.
2. Power on bench supply.
3. Open monitor.
4. Type `r` ↵.
5. Expect:
   - `[fsm] 0 -> 1` immediately.
   - 4-second pause (counterweight simulation).
   - `[fsm] 1 -> 2`.
   - Motor spins UP. Deck rises.
   - When deck reaches the top tape mark, top limit switch trips, `EVT_TOP_LIMIT_HIT` fires, `[fsm] 2 -> 3`. Motor brakes.
   - Bridge holds at top.
6. Type `l` ↵. `[fsm] 3 -> 4`. Motor spins DOWN. Deck lowers.
7. Bottom limit trips, `[fsm] 4 -> 5`. Motor stops.
8. After 1 s timer: `[fsm] 5 -> 0`. Cycle complete.
9. Total time should be 18–25 s. Cycle the bridge 5 more times. Note any anomalies.

### 10.3 If something goes wrong
Use the failure cookbook in Step 12.

---

## Step 11 — Weekly milestones

| Week | Goal | Sign-off |
|---|---|---|
| 1 | Wave 1 + 2 prints done. Towers + base assembled. | Photo of standing rig in `docs/build_log/` |
| 2 | Wave 3 + 4 prints done. Cable + counterweight system functional. Manual lift smooth. | M1 confirms manual lift smooth |
| 3 | Motor + drum installed. BTS7960 wired. `CAL_COUNTS_PER_MM` measured (≠ 42). | `docs/cal_log.md` committed |
| 4 | Limit switches wired with diode-OR + pull-up. Top/bottom events fire correctly. | M1 confirms FSM cycles |
| 5 | Full closed-loop: serial `r` → raises to top, `l` → lowers to bottom. | M1 sign-off |
| 6 | Stall + overcurrent fault tests pass | M5 sign-off |
| 7 | 10-cycle endurance run (back-to-back, no manual intervention). | No mechanical failures, < 5 mm cumulative position drift |
| 8 | Demo polish — cable wear check, lubrication. | Photo of clean rig committed |

---

## Step 12 — Failure recovery cookbook

| Symptom | Most likely cause | First fix to try |
|---|---|---|
| Deck rises crooked | Towers not parallel | Loosen tower bolts, re-square with try-square, retighten |
| Cable squeals | Pulley bearing dry | Clean with isopropyl alcohol, single drop of light machine oil |
| Motor stalls 2 cm before top | Counterweight binding | Check counterweight box clearance — must not touch tower face |
| Counts/mm reads wildly different each run | Pot wiper noisy | Add 100 nF cap from GPIO 35 to GND |
| `[fault] raised: stall` mid-travel | Cable jammed in pulley groove | Re-route cable so it stays inside groove on every pulley |
| Drum slips on motor shaft | Grub screw loose / shaft flat missing | Apply blue threadlocker; add a second grub screw 90° around |
| Deck overshoots top limit | BTS7960 brake too weak / motor coasts | Bump `MOTOR_PWM_LOWER_DEFAULT` to increase brake torque, or add small physical bump-stop |
| Motor draws 1.5 A continuously while idle | PWM never reaches 0% | Check `motor_driver_apply()` switch — STOP/COAST cases must `ledcWrite(*, 0)` |
| Motor doesn't spin even with valid PWM | R_EN / L_EN not tied HIGH | Connect R_EN and L_EN to 5 V on the BTS7960 module |
| Simulated counterweight never reaches `EVT_CW_READY` | `task_counterweight` not created | Check main.cpp `setup()` has `xTaskCreatePinnedToCore(task_counterweight, ...)` |
| Limit switch fires constantly without anything touching | Floating GPIO 39 | Add 10 kΩ external pull-up from GPIO 39 to 3.3 V — required, no internal pull-up |

---

## Completion checklist

Mechanical:
- [ ] All 13 STLs printed, post-processed, M3 inserts set.
- [ ] Towers within ±1° of vertical (verified with try-square).
- [ ] Manual deck lift smooth top-to-bottom under counterweight.
- [ ] Cables show no wear after 50+ cycles.
- [ ] Spare cable + spare counterweight box on hand for demo day.

Firmware:
- [ ] `CAL_COUNTS_PER_MM` measured on hardware (not the placeholder 42). Logged in `docs/cal_log.md`.
- [ ] PWM duties tuned: raise = 6–8 s, lower = 5–7 s.
- [ ] Stall fault triggers within 2 s when motor blocked.
- [ ] Overcurrent fault triggers when bridge held against top limit.
- [ ] Both limit switches trip events fire to FSM.
- [ ] Counterweight simulation posts `EVT_CW_READY` within 5 s of state entry.
- [ ] 5 consecutive raise+lower cycles complete without intervention.
- [ ] Motor current measured at top + bottom — < 1500 mA both directions.

Documentation:
- [ ] Build log photos committed to `docs/build_log/` (≥ 15 photos).
- [ ] Calibration log in `docs/cal_log.md`.
- [ ] Mechanical drawing (or annotated photo) of cable routing committed.
