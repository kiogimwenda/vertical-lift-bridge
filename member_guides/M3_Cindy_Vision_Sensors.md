# Member 3 — Cindy — Vision & Sensors

> Role: You make the bridge **see**. ESP32-CAM vehicle detection, dual ultrasonic direction sensors, the JSON link between them and the main board.

---

## Scope of your work

| File | What you own |
|---|---|
| `firmware_cam/src/main.cpp` | ESP32-CAM companion (frame-diff motion detection) |
| `firmware_cam/platformio.ini` | CAM build config |
| `firmware/src/sensors/ultrasonic.{h,cpp}` | Dual HC-SR04 driver + direction inference |
| `firmware/src/vision/vision_link.{h,cpp}` | UART2 JSON parser on the main board |
| `docs/sensor_calibration.md` | New file — record your tuned values |

You publish to `g_status.vision` and `g_status.ultrasonic`. The FSM consumes — you don't touch FSM logic.

---

## Step 1 — Set up your machine (Windows 11)

Follow steps 1.1–1.7 of M1's guide. Then add:

### 1.8 Install Arduino IDE 2.x (you'll use this for ESP32-CAM)
PlatformIO works for the CAM too, but the **Arduino IDE has a much easier first-flash workflow** for the AI-Thinker board because of its automatic boot-mode handling.

1. Go to `https://www.arduino.cc/en/software`. Download "Arduino IDE 2.3.x" Windows installer.
2. Run installer. Defaults.
3. Open Arduino IDE. File → Preferences. In "Additional boards manager URLs" paste:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
4. OK. Tools → Board → Boards Manager → search `esp32` → install **esp32 by Espressif Systems** (latest version, 3.x).
5. Wait for install (~5 min, downloads 200 MB toolchain).

### 1.9 Acquire an FTDI / USB-UART adapter
The AI-Thinker ESP32-CAM has **no USB port**. You program it through a CH340 / FT232 / CP2102 adapter:
1. Adapter wiring to ESP32-CAM:

   | Adapter | ESP32-CAM |
   |---|---|
   | 5 V | 5 V |
   | GND | GND |
   | TX  | UOR (U0R / GPIO3) |
   | RX  | UOT (U0T / GPIO1) |

2. **Programming jumper:** short **IO0 to GND** to enter download mode. Remove the jumper for normal run.

---

## Step 2 — First flash of the ESP32-CAM (companion firmware)

1. Open Arduino IDE.
2. File → Open → navigate to `firmware_cam/src/main.cpp`.
3. Tools → Board → esp32 → **AI Thinker ESP32-CAM**.
4. Tools → Port → COM port of your FTDI adapter.
5. Tools → Partition Scheme → "Huge APP (3MB No OTA / 1MB SPIFFS)".
6. Wire the IO0–GND jumper. Press the RST button on the CAM.
7. Click Upload (right-arrow icon). Wait for "Hard resetting…".
8. **Remove the IO0–GND jumper.** Press RST.
9. Tools → Serial Monitor at **115200 baud**. You should see:
   ```
   {"boot":"ok"}
   {"t":1234,"v":0,"c":0,"x":20,"y":30,"w":0,"h":0}
   {"t":1334,"v":0,"c":0,"x":20,"y":30,"w":0,"h":0}
   ```
10. Wave a hand in front of the camera — `"v":1` should appear with non-zero `c` (motion %).

---

## Step 3 — Tune the ROI and motion threshold

The ROI (region of interest) is where vehicle detection happens. Default is the centre 120 × 60 patch of a 160 × 120 frame.

In `firmware_cam/src/main.cpp`:

```cpp
#define ROI_X    20      // top-left x of ROI
#define ROI_Y    30      // top-left y
#define ROI_W   120      // width
#define ROI_H    60      // height
#define MOTION_THRESHOLD   24      // per-pixel intensity delta
#define DETECT_PCT_THRESH  6       // % of ROI pixels that must move
#define DETECT_HOLD_MS     400     // latch a detection this long
```

### Procedure
1. Mount the ESP32-CAM at its final position above the road approach (≈ 80 mm above road, looking down at 60° tilt).
2. Adjust `ROI_X / ROI_Y / ROI_W / ROI_H` so the ROI covers the road lane only — exclude the bridge deck and sky/ceiling.
3. With nothing moving, watch monitor — `"c"` should hover at 0–2.
4. Walk a finger / toy car across — `"c"` should jump to 15–60.
5. Set `DETECT_PCT_THRESH` ≈ half of the toy-car peak. So if hand reads 30, set threshold 15.
6. If background shadows are flagging false positives: increase `MOTION_THRESHOLD` from 24 → 32.
7. Document final values in `docs/sensor_calibration.md`.

---

## Step 4 — Wire the CAM into the main board

> **Critical:** the ESP32-CAM has **brownout-prone** 5 V supply behaviour. Do NOT share it with the main ESP32's 5 V regulator. Use a separate buck converter from the 12 V rail.

| ESP32-CAM | Main ESP32 / power rail |
|---|---|
| 5 V (VCC) | LM2596 buck output (5 V, dedicated to CAM) |
| GND       | Common GND |
| UOT (TX, GPIO1) | ESP32 GPIO16 (UART2 RX) |
| UOR (RX, GPIO3) | (leave open — v1 firmware doesn't talk back) |

Use 22 AWG silicone wire, length ≤ 30 cm. Add 470 µF electrolytic capacitor across CAM 5 V/GND right at the module pins.

---

## Step 5 — Verify the JSON link

With both boards powered:
1. Main board monitor at 115200. You should see your normal boot log.
2. After ESP32-CAM boots (~3 s after main), main board log should stop showing `[fault] raised: vision-lost`.
3. Wave hand in front of CAM — main board logs:
   ```
   [fsm] 0 -> 1     (IDLE -> ROAD_CLEARING — vehicle detected)
   ```
4. If no transition: open `vision_link.cpp`, add `Serial.println(line)` inside `parse_line()` to confirm bytes are arriving.

### Common debug fixes
- **Garbage characters on UART2:** baud rate mismatch. CAM is hard-coded 115200; check `vision_link.cpp` matches.
- **No bytes at all:** TX/RX swapped at the harness. CAM TX → main RX (it's a crossover).
- **Parse fails silently:** check ArduinoJson version is 7.x — older versions use a different API.

---

## Step 6 — Dual HC-SR04 ultrasonic install

Mount two HC-SR04 modules along the road approach, separated by `ULTRASONIC_BEAM_SPACING_CM = 30`. Beam A is **outer** (further from bridge); beam B is **inner** (closer).

| HC-SR04 pin | Beam A wiring | Beam B wiring |
|---|---|---|
| VCC | 5 V | 5 V |
| GND | GND | GND |
| TRIG | ESP32 GPIO5 (PIN_US_TRIG_A) | ESP32 GPIO18 (PIN_US_TRIG_B) |
| ECHO | ESP32 GPIO4 (PIN_US_ECHO_A) — **via 1 kΩ / 2 kΩ divider** | ESP32 GPIO19 (PIN_US_ECHO_B) — **via divider** |

> ⚠ HC-SR04 ECHO swings 0–5 V. ESP32 GPIO is **3.3 V tolerant only**. Always use a 1 kΩ + 2 kΩ divider on each ECHO to drop to ~3.3 V. Skipping this slowly damages the GPIO.

### Direction inference algorithm walkthrough
The algorithm in `sensors_ultrasonic_tick()` keeps a 5-sample ring of "blocked" booleans for each beam. On each call:
1. Alternate-trigger A or B (one per cycle, 20 Hz total = 10 Hz per beam).
2. Push beam-blocked bool into ring, increment ring index.
3. For each beam, find the first sample (in chronological order) where the value transitioned from 0 → 1.
4. Whichever beam transitioned first this window indicates the **leading edge** of a vehicle.
5. A first → APPROACHING (vehicle moving toward the bridge).
6. B first → LEAVING.

**Tuning:** if direction inference is unreliable (vehicle too fast):
- Reduce ring size from 5 → 3 (`#define DIR_HIST 3`).
- Move beams further apart (50 cm).

---

## Step 7 — Weekly milestones

| Week | Goal | Sign-off |
|---|---|---|
| 1 | Arduino IDE + ESP32 board package installed. CAM first-flash working. | Boot JSON visible |
| 2 | ROI tuned, motion detection reliable in lab lighting | Toy car triggers `v=1` 10/10 times |
| 3 | UART2 link to main board working — vision-lost fault clears | M1 confirms event delivered |
| 4 | HC-SR04 × 2 wired with dividers. Distances < 80 cm trigger detect. | Beam test passes |
| 5 | Direction inference (approaching vs leaving) works for hand sweep | 8/10 sweeps correct direction |
| 6 | Vision + ultrasonic agree (FSM uses OR of both) | Fault tolerance: covering CAM doesn't break detection |
| 7 | Long-run stability (1 hr) — no link-lost faults | Stability log committed |
| 8 | Demo polish — clean wires, secure mounts | Lecturer demo |

---

## Step 8 — Failure recovery cookbook

| Symptom | Cause | Fix |
|---|---|---|
| CAM upload fails "Failed to connect" | IO0 not grounded during reset | Re-check jumper; press RST while jumper held |
| `Brownout detector triggered` reboot loop | CAM PSU undersized | Use dedicated 5 V buck, 470 µF cap at module |
| Endless `"v":0` even with motion | ROI off-frame or threshold too high | Drop `DETECT_PCT_THRESH` to 3, re-test |
| Endless `"v":1` (false positive storm) | Background flicker / fluorescent light | Increase `MOTION_THRESHOLD` to 36 |
| `[fault] vision-lost` at 2 s intervals | UART line glitching | Check ground continuity, shorten wire |
| HC-SR04 reads 0xFFFF (no echo) | Sensor faulty or 5 V rail dead | Swap sensor; check 5 V at module pins (must be ≥ 4.8 V) |
| Direction always `BOTH` | Both beams triggering simultaneously | Increase beam spacing to 40 cm |

---

## Completion checklist

- [ ] CAM ROI documented in `docs/sensor_calibration.md` with screenshots.
- [ ] CAM detects toy car at 30 cm distance from lens, 100 lux lighting.
- [ ] Vision JSON arriving on UART2 at ≥ 8 Hz steady state.
- [ ] Both HC-SR04 modules read consistent distances within ± 1 cm.
- [ ] Direction inference correct in 8 of 10 hand sweeps.
- [ ] No vision-lost fault during 1-hour idle test.
- [ ] All cables labelled (CAM-TX, CAM-GND, US-A-TRIG, etc.).
