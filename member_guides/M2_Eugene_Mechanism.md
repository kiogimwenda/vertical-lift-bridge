# Member 2 — Eugene — Mechanism, Print Pipeline & Motor Calibration

> Role: You own the **physical bridge** — every printed part, every cable, every counterweight, the motor, and its calibration. If something moves (or fails to), it's yours.

---

## Scope of your work

| Asset | Where | What you own |
|---|---|---|
| 13 OpenSCAD source files | `cad/scad/*.scad` | Parametric CAD for all printed parts |
| 13 STL files | `cad/stl/*.stl` | Slicer inputs |
| `cad/README_cad.md` | `cad/` | Print order + slicer profile |
| `firmware/src/motor/motor_driver.{h,cpp}` | firmware | BTS7960 PWM, encoder, current sensing |
| `firmware/src/counterweight/counterweight.{h,cpp}` | firmware | **Simulated** dynamic counterweight system (pump/drain water tanks) — software only, no physical hardware change |
| `firmware/src/pin_config.h` (motor section) | firmware | M1 owns file, but **you** own motor pins |
| Mechanical assembly photos | `docs/build_log/` | New folder you create — photo-document every step |

**Not your work:** PCB, traffic lights, vision. You consume `g_motor_cmd_queue` and update `g_status.deck_position_mm`. You also own the simulated counterweight module (`firmware/src/counterweight/`) which models a pump-and-drain water tank system in software — the physical counterweights remain static lead boxes.

---

## Step 1 — Set up your machine (Windows 11)

Follow steps 1.1–1.7 of M1's guide (VS Code, Git, Python 3.11, PlatformIO, CH340, repo clone). When done, come back here.

### 1.8 Install OpenSCAD (you need this even if you only edit .scad files)
1. Go to `https://openscad.org/downloads.html`.
2. Download the Windows 64-bit `.msi`. Run it. Defaults are fine.
3. Launch OpenSCAD once to confirm it opens. Quit.

### 1.9 Install your slicer
Pick one — both work, both are free. The slicer profile in `cad/README_cad.md` is written for **PrusaSlicer**.
1. Go to `https://www.prusa3d.com/page/prusaslicer_424/`.
2. Download Windows installer. Defaults.
3. On first launch the **Configuration Wizard** opens — pick "Other Vendor → Generic 0.4 mm nozzle", PLA filament profile.

### 1.10 Open the CAD files
1. In VS Code, open the workspace.
2. Navigate to `cad/scad/` in the file tree.
3. Right-click any `.scad` file → "Reveal in File Explorer".
4. Double-click the file — Windows should open it in OpenSCAD. If it opens in a text editor instead: right-click → Open With → Choose another app → OpenSCAD → tick "Always use this app".

---

## Step 2 — Read every CAD file before printing anything

You'll save days of failed prints by reading the source first. Open each `.scad` file and look at the `// === PARAMETERS ===` block at the top.

| File | What it makes | Watch for |
|---|---|---|
| `tower_left.scad` | Left vertical guide tower | `tower_height = 220` (tune ± 5 mm if your rail is short) |
| `tower_right.scad` | Right vertical guide tower | Mirror of left — symmetric |
| `bridge_deck.scad` | The lifting deck | `deck_length = 300`, `deck_width = 100` |
| `cable_drum.scad` | Motor spool | `drum_diameter = 30` — **this drives counts/mm calibration** |
| `pulley_top.scad` | Top pulley wheel | Bore Ø8 mm for 608ZZ bearing |
| `pulley_bracket.scad` | Pulley axle holder | M8 hole — match your axle bolt |
| `counterweight_box.scad` | Lead-shot container | Designed to hold 4× 30 g pieces = 120 g. Physical counterweight is static; the firmware simulates a dynamic water-tank system in parallel for demo purposes |
| `motor_mount.scad` | JGA25-370 cradle | Match your motor's M3 hole pattern (12 × 18 mm) |
| `barrier_arm.scad` | Servo barrier (×2) | Mounts directly on SG90 horn |
| `barrier_base.scad` | Servo base | M3 inserts |
| `tft_bezel.scad` | TFT display frame | 35° tilt — tuned for seated viewing |
| `pcb_standoffs.scad` | PCB mounting boss | Centre-to-centre 92 × 67 mm |
| `cable_keeper.scad` | Strain-relief clip | Pinches USB / power cable |

---

## Step 3 — Print queue (start week 1, finish by end of week 2)

Total ≈ **70 hours print time, 1.05 kg PLA**. Don't print everything at once — print in this order so you can start assembly as parts come off the bed.

### Wave 1 (≈18 hr — start of week 1)
1. `tower_left.stl`
2. `tower_right.stl`
3. `motor_mount.stl`
4. `pcb_standoffs.stl` × 1 (4 standoffs in one print)

### Wave 2 (≈22 hr — week 1 mid)
5. `bridge_deck.stl`
6. `pulley_bracket.stl` × 4 (one print, multiplied in slicer)
7. `pulley_top.stl` × 4 (one print)

### Wave 3 (≈18 hr — week 2 start)
8. `cable_drum.stl`
9. `counterweight_box.stl` × 2
10. `barrier_arm.stl` × 2
11. `barrier_base.stl` × 2

### Wave 4 (≈12 hr — week 2 mid)
12. `tft_bezel.stl`
13. `cable_keeper.stl` × 4

### Slicer settings (PrusaSlicer, PLA, 0.4 mm nozzle)
- Layer height: **0.20 mm** for structural parts (towers, motor mount)
- Layer height: **0.16 mm** for visible parts (TFT bezel)
- Infill: **40 %** gyroid for towers; **20 %** for everything else
- Walls: **3 perimeters** (towers); 2 elsewhere
- Top/bottom: **5 layers**
- Supports: **only on overhangs > 55°** (most parts: none)
- Brim: **5 mm** for towers (tall & narrow), none for the rest
- Filament temp: **210 °C** nozzle, **60 °C** bed
- Print speed: **60 mm/s** outer perimeter, **80 mm/s** infill

### Post-processing per part
- Test-fit M3 brass inserts in every hole **before** mass-printing — adjust hole diameter in OpenSCAD if too tight (`m3_insert_d = 4.2`).
- Set inserts with a soldering iron at 220 °C, 5 s push.
- Cable drum: ream the 6 mm bore with a hand drill if motor shaft is tight; do not force — splits the keyway slot.
- Tower bores for MGN12 carriage: file lightly until carriage slides without binding.

---

## Step 4 — Mechanical assembly sequence

> Photo-document every step. Save under `docs/build_log/yyyy-mm-dd_step_xx.jpg`.

1. **Base** (week 2 day 1): Cut MDF base 1200 × 600 × 12. Mark tower positions (centred, 250 mm apart). Drill M3 pilot holes.
2. **Towers**: Mount left + right towers vertically with L-brackets to base. Square them with a try-square — within 1° of vertical or counterweights bind.
3. **MGN12 rails**: Slide carriages onto rails *before* mounting rails to towers (carriages can't be added once ends are fixed). Bolt rails to inside face of each tower with M3 × 8 socket caps.
4. **Top pulleys**: Press 608ZZ bearings into pulley wheels. Bolt pulley brackets to top of each tower. Insert M8 × 80 axle through 4 pulleys (2 per tower) + brackets. Locknut.
5. **Motor + drum**: Bolt motor to motor_mount. Slide drum onto motor shaft, secure with M3 grub screw on shaft flat. Mount motor_mount to base on the **right tower side**, pulley axis aligned with drum.
6. **Cable**: Cut two 1 m lengths of 1 mm braided cable. Crimp loop terminals on one end of each. Anchor loop to deck (M3 eyelet). Run cable up over inner pulley → across top → down outer pulley → terminate at counterweight box.
7. **Drum cable**: Cut 0.6 m of cable. Crimp loop on each end. Loop one end through drum cross-hole, other end to centre of deck top. Wind 6 turns onto drum (winding direction = "deck rises when motor turns CW from front view").
8. **Counterweights**: Fill each box with 4 × 30 g lead. Hot-glue lid to seal. Hang on cable. Note: the firmware runs a simulated dynamic counterweight module (`counterweight/counterweight.cpp`) that models water tanks being filled/drained — this is a software-only simulation for demonstration on the TFT dashboard. The physical counterweights remain these static lead boxes.
9. **Limit switches**: Mount KW11-3Z microswitches at top + bottom of one tower. Wire NC contacts to limit pins (PIN_LIMIT_TOP / PIN_LIMIT_BOTTOM with INPUT_PULLUP — switch shorts to GND when activated).
10. **First manual test**: With motor power **OFF**, lift deck by hand. Should rise smoothly with counterweights falling. Drop deck slowly — counterweights rise. If binds: re-square towers, re-tension cables, debur rail.

---

## Step 5 — Motor calibration (CAL_COUNTS_PER_MM)

This is the single most important number in your firmware. The placeholder in `motor_driver.cpp` is `42` — you **must** re-measure on hardware.

### Calibration sketch
Create a temporary file `firmware/src/motor/cal_sketch.cpp` (delete after):

```cpp
// CALIBRATION SKETCH — replace the regular firmware entry with this
// during calibration. After measurement, restore main.cpp.
#include <Arduino.h>
#include "../pin_config.h"

volatile int32_t pulses = 0;
void IRAM_ATTR isr_a() { pulses++; }

void setup() {
    Serial.begin(115200);
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), isr_a, RISING);
    pinMode(PIN_MOT_RPWM, OUTPUT); pinMode(PIN_MOT_LPWM, OUTPUT);
    digitalWrite(PIN_MOT_LPWM, LOW);
    Serial.println("Calibration: motor will run UP for 5 s. Mark deck before/after.");
    delay(3000);
    pulses = 0;
    analogWrite(PIN_MOT_RPWM, 180);     // ~70 % duty 8-bit Arduino API
    delay(5000);
    analogWrite(PIN_MOT_RPWM, 0);
    Serial.printf("Pulses counted: %ld\n", pulses);
    Serial.println("Now physically measure deck travel in mm with a ruler.");
}
void loop() { delay(1000); }
```

### Procedure
1. Mark deck position with masking-tape arrow on tower.
2. Build & flash the calibration sketch.
3. Open Monitor. Wait 3 s, motor runs 5 s, prints pulse count.
4. **Stop deck immediately** if it hits top — kill USB power.
5. Measure travel with steel ruler from arrow to new deck position. Record in mm.
6. Compute: `CAL_COUNTS_PER_MM = pulses / measured_mm`. Round to nearest integer.
7. Run twice more, average the three results.

### Apply the calibration
Edit `firmware/src/motor/motor_driver.cpp`, change:
```cpp
#define CAL_COUNTS_PER_MM   42      // replace 42 with your measured value
```
Restore `main.cpp`. Rebuild. Flash.

### Verify
1. With normal firmware running, send `r` over serial to raise the bridge.
2. Watch monitor for `g_status.deck_position_mm`. It should reach exactly 175 (DECK_HEIGHT_MAX_MM) when the top limit switch trips.
3. Tolerance: ± 3 mm. If consistently off by more than that, re-calibrate.

---

## Step 6 — Tuning motor PWM duty

Defaults in `system_types.h`:
```c
#define MOTOR_PWM_RAISE_DEFAULT     5500    // ~67 %
#define MOTOR_PWM_LOWER_DEFAULT     4500    // gravity assist
```

Tune by observation:
- If deck rises too fast (< 4 s for 175 mm): drop `RAISE_DEFAULT` by 500.
- If deck rises sluggishly or stalls near top: bump it by 500.
- Lowering should take ≈ 5 s — if it runs away (free-fall), bump `LOWER_DEFAULT` (more braking torque from BTS7960).

---

## Step 7 — Weekly milestones

| Week | Goal | Sign-off |
|---|---|---|
| 1 | Wave 1 + 2 prints done. Towers + base assembled. | Photo of standing rig |
| 2 | Wave 3 + 4 prints done. Cable + counterweight system functional. | Manual lift smooth |
| 3 | Motor + drum installed. CAL_COUNTS_PER_MM measured. | Calibration log committed |
| 4 | Limit switches wired. Top/bottom events fire correctly. | M1 confirms FSM cycles |
| 5 | Full closed-loop: serial `r` raises to top, `l` lowers to bottom. | M1 sign-off |
| 6 | Stall + overcurrent fault tests pass | M5 sign-off |
| 7 | 10-cycle endurance run | No mechanical failures |
| 8 | Demo polish — cable wear check, lubrication | Lecturer demo |

---

## Step 8 — Failure recovery cookbook

| Symptom | Cause | Fix |
|---|---|---|
| Deck rises crooked | Towers not parallel | Loosen tower bolts, re-square with try-square |
| Cable squeals | Pulley bearing dry / dirty | Clean with IPA, drop of light machine oil |
| Motor stalls 2 cm from top | Top counterweight binding | Check counterweight box clearance — must not touch tower |
| Encoder count drifts | Loose pulse wire / EMI | Add 0.1 µF cap from PIN_ENC_A to GND, twist signal pair |
| `[fault] raised: stall` mid-travel | Cable jammed in pulley groove | Re-route cable so it stays inside groove on every pulley |
| Drum slips on shaft | Grub screw loose | Apply blue threadlocker (Loctite 243) to grub screw |
| Deck overshoots top limit | Motor not braking quickly | Confirm `MOTOR_BRAKE` action fires on `STATE_RAISED_HOLD` entry |

---

## Completion checklist

- [ ] All 13 STLs printed, post-processed, M3 inserts set.
- [ ] CAL_COUNTS_PER_MM measured (not the default 42).
- [ ] Both limit switches trip events fire to FSM.
- [ ] 5 consecutive raise+lower cycles complete without intervention.
- [ ] Motor current measured at top + bottom — < 1500 mA both directions.
- [ ] Build log photos committed to `docs/build_log/`.
- [ ] Spare cable + spare counterweight on hand for demo day.
