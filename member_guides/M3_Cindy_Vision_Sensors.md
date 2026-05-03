# Member 3 — Cindy — Vision & Sensors

> **Your role.** You make the bridge **see**. The ESP32-CAM watches the road for vehicles. Four HC-SR04 ultrasonic sensors watch the waterway in two pairs (upstream + downstream) and tell the FSM not just *that* a vessel is there but *which way it's moving*. You also own the JSON link between the camera and the main ESP32.
>
> **Your single deliverable.** When a hand waves in front of the camera **OR** when something passes through both ultrasonic beams in upstream→downstream order, M1's FSM receives `EVT_VEHICLE_DETECTED` within 200 ms of the first detection sample.

---

## 0. What you are building (read this once)

There are **two independent detection paths** and the FSM combines them with logical OR:

```
Vision (ESP32-CAM watching road) ─┐
                                  ├─→  EVT_VEHICLE_DETECTED → FSM
Ultrasonic (4 sensors watching   ─┘
            water, direction
            inferred)
```

Either source can trigger a bridge cycle. Both contribute to the same `g_status` fields so the HMI can show which sensor saw what.

### Architecture
```
┌──────────────────────────┐                       ┌─────────────────────────────┐
│  ESP32-CAM (AI-Thinker)  │                       │   ESP32-WROOM (main board)  │
│  - OV2640 grayscale QQVGA│   UART2 @ 115200 8N1  │   - task_vision  reads JSON │
│  - frame-diff in ROI     │ ─────────────────────►│   - task_sensors triggers   │
│  - emit one JSON/frame   │   (CAM TX → main RX)  │     HC-SR04 round-robin     │
│  ROI: 120×60 at (20,30)  │                       │   - both write to g_status  │
└──────────────────────────┘                       │     ultrasonic & vision     │
                                                   └─────────────────────────────┘
        ┌────────────────┬────────────────┐
        │                │                │
   US1 (Beam A)      US2 (Beam B)    GPIO 5/18 + 19/21
   Upstream pair: detects approaching/leaving from upstream side, 3 cm beam spacing

   US3 (Beam A)      US4 (Beam B)    GPIO 22/23 + 1/3
   Downstream pair: detects approaching/leaving from downstream side, 3 cm beam spacing
```

The 3 cm spacing is small because beams arrive milliseconds apart at any realistic vessel speed; the order of arrival reveals direction, not the precise speed.

---

## Scope of your work — every file you touch

| File | What it does | Status |
|---|---|---|
| `firmware_cam/src/main.cpp` | ESP32-CAM companion firmware (frame diff + JSON output) | ✅ exists — you tune ROI / threshold |
| `firmware_cam/platformio.ini` | CAM build config | ✅ exists |
| `firmware/src/sensors/ultrasonic.{h,cpp}` | Quad HC-SR04 driver + direction inference | ✅ exists — you tune beam spacing constant + trigger distance |
| `firmware/src/vision/vision_link.{h,cpp}` | UART2 JSON parser on the main board | ✅ exists |
| `docs/sensor_calibration.md` | ❌ create — record your tuned ROI / thresholds / trigger distance |
| Wiring diagrams | `docs/wiring/` | ❌ create — photo each completed wiring stage |

**Not your work.** Anything outside `sensors/`, `vision/`, and `firmware_cam/`. You publish to `g_status.vision` and `g_status.ultrasonic`. The FSM (M1) consumes — you don't touch FSM logic.

---

## Step 1 — Set up your machine (Windows 11)

Follow **steps 1.1 to 1.7 of M1's guide** first (VS Code, Git, Python 3.11, PlatformIO, CH340 driver, repo clone). Then add the items below — you need a few extras because you flash two different boards.

### 1.8 Install Arduino IDE 2.x (for the ESP32-CAM)
PlatformIO can flash the ESP32-CAM, but the **Arduino IDE has a much simpler first-flash workflow** for the AI-Thinker board, and it's the recommended path for your first few iterations.

1. Edge → `arduino.cc/en/software`. Click "Windows ZIP file" or "Windows installer" — installer is easier. File `arduino-ide_2.3.x_Windows_64bit.exe`.
2. Run installer. Defaults. Install.
3. Open Arduino IDE.
4. File → Preferences. Find "Additional boards manager URLs" at the bottom. Paste:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
   Click OK.
5. Tools → Board → Boards Manager. In the search bar type `esp32`. Find **"esp32 by Espressif Systems"** (latest 3.x). Click Install. Wait ~5 min for the 200 MB toolchain.
6. Tools → Board → esp32 → scroll to **"AI Thinker ESP32-CAM"**. Click it.

### 1.9 Acquire an FTDI / USB-UART adapter
The AI-Thinker ESP32-CAM has **no USB port**. You program it through an external USB-UART adapter:

1. Buy an FT232RL or CP2102 USB-UART board from Pixel Electric (~KES 350). The CP2102 type is more reliable in our experience.
2. Standard wiring (CAM ↔ adapter):

   | Adapter | ESP32-CAM | Wire colour suggestion |
   |---|---|---|
   | 5 V | 5 V | red |
   | GND | GND | black |
   | TX | UOR (U0R / GPIO 3) | yellow |
   | RX | UOT (U0T / GPIO 1) | green |

3. **Programming jumper:** short **IO0 to GND** (with a wire or jumper) to enter download mode. Remove for normal run mode.

### 1.10 Install the CP2102 driver if needed
Like the CH340, the CP2102 needs a driver on a fresh Win11 install:
1. Edge → `silabs.com/developers/usb-to-uart-bridge-vcp-drivers`. Download the Windows driver ZIP.
2. Extract. Right-click the `.inf` file matching your Windows version → Install.
3. Plug the FT232/CP2102 in. Device Manager → Ports — should show "Silicon Labs CP210x USB to UART Bridge (COM<n>)".

---

## Step 2 — First flash of the ESP32-CAM (companion firmware)

Goal: get `{"boot":"ok"}` and a stream of detection JSON on the serial monitor.

### 2.1 Wire the CAM for programming
1. Adapter wired to CAM per Step 1.9 table.
2. **Short IO0 to GND** with a jumper wire. (This is the most-forgotten step. Without it, "Failed to connect" errors.)
3. Plug the adapter into your laptop USB port.

### 2.2 Open the CAM firmware in Arduino IDE
1. File → Open → navigate to `vertical-lift-bridge\firmware_cam\src\main.cpp` → Open. Arduino IDE may auto-create a `firmware_cam.ino` file alongside — that's fine.
2. Tools → Board → esp32 → **AI Thinker ESP32-CAM**.
3. Tools → Port → COM port of your FT232/CP2102.
4. Tools → Partition Scheme → **"Huge APP (3MB No OTA / 1MB SPIFFS)"**.
5. Tools → CPU Frequency → 240 MHz.
6. Tools → Flash Frequency → 80 MHz.

### 2.3 Upload
1. Press the **RST** button on the CAM (it's the only button — small surface-mount, on the bottom edge near the antenna). Hold for 1 second, release.
2. Within 5 seconds of releasing RST, click **Upload** in Arduino IDE (the right-arrow icon, top left).
3. Wait for "Writing at 0x...% (100%)... Hash of data verified. Hard resetting via RTS pin..."
4. **Remove the IO0–GND jumper.** Press RST again to reboot in run mode.

### 2.4 Open serial monitor
1. Tools → Serial Monitor.
2. **Set baud rate to 115200** (bottom right).
3. Expected output:
   ```
   {"boot":"ok"}
   {"t":1234,"v":0,"c":0,"x":20,"y":30,"w":0,"h":0}
   {"t":1334,"v":0,"c":0,"x":20,"y":30,"w":0,"h":0}
   {"t":1434,"v":0,"c":0,"x":20,"y":30,"w":0,"h":0}
   ```
4. Wave a hand in front of the camera (within 30 cm). You should see `"v":1` with `"c"` jumping to 15+:
   ```
   {"t":2034,"v":1,"c":42,"x":40,"y":60,"w":50,"h":30}
   ```

### 2.5 Field meanings of the JSON
| Field | Meaning |
|---|---|
| `t` | millis() since CAM boot |
| `v` | 0 or 1 — vehicle detected this frame? |
| `c` | 0..100 — % of ROI pixels with motion above threshold |
| `x`, `y` | top-left of motion bounding box (camera coordinates, full frame is 160×120) |
| `w`, `h` | width and height of bounding box |

If `c` is reading 0 even when you wave: the camera ribbon may not be seated. Power off, unlatch the connector, re-seat the ribbon, latch.

---

## Step 3 — Tune the ROI and motion threshold (`docs/sensor_calibration.md`)

The ROI (region of interest) is where vehicle detection happens. Default is the centre 120 × 60 patch of a 160 × 120 frame.

In `firmware_cam/src/main.cpp` lines 41–47:
```cpp
#define ROI_X    20      // top-left x of ROI within full 160-wide frame
#define ROI_Y    30      // top-left y within 120-high frame
#define ROI_W   120      // ROI width in pixels
#define ROI_H    60      // ROI height
#define MOTION_THRESHOLD   24   // per-pixel intensity delta (0–255)
#define DETECT_PCT_THRESH   6   // % of ROI pixels that must move
#define DETECT_HOLD_MS    400   // latch detection this long after last motion
```

### 3.1 Procedure
1. Mount the ESP32-CAM at its final position above the road approach. Suggested: **80 mm above road, looking down at 60° tilt**.
2. With nothing moving in frame, watch the monitor for 30 seconds — `"c"` should hover at **0–2**. If it's 5+ with no motion, your background has flicker (fluorescent lighting, or shadows from a moving curtain). Bump `MOTION_THRESHOLD` from 24 to 32, re-flash.
3. Walk a finger or toy car across the road. `"c"` should jump to 15–60. Note the **peak value** when the toy is mid-frame.
4. Set `DETECT_PCT_THRESH` to **about half the toy peak**. If the toy peaks at 30, set it to 15.
5. Adjust ROI if your camera angle is wrong:
   - Watch the bbox `"x"` and `"y"` values when motion is detected. If the bbox always lands at the bottom-right (e.g. `x=140, y=100`), your ROI is misaligned. Move ROI_X/Y to centre the road in frame.
   - To **inspect the ROI bounds visually**, temporarily make the CAM a wifi web server and view the frame in a browser. (Beyond the scope of v1; skip for the demo.)

### 3.2 Document final values
Create `docs/sensor_calibration.md`:
```markdown
# Sensor calibration log

## Vision (ESP32-CAM)
Date: 2026-05-15
Mount: 80 mm above road surface, 60° tilt
ROI:    X=20, Y=30, W=120, H=60   (covers road lane only, excludes deck)
Threshold: MOTION_THRESHOLD=32, DETECT_PCT_THRESH=12, DETECT_HOLD_MS=500
Notes: Background reads c=2-3 due to bench fluorescents; no false positives in 1-hour idle test.
       Toy car peaks c=44.

## Ultrasonic (HC-SR04 ×4)
... (filled in after Step 5)
```

---

## Step 4 — Wire the CAM to the main board

### 4.1 Critical: separate 5 V supply for the CAM
The ESP32-CAM **brownouts** if it shares the main ESP32's 5 V regulator during the camera capture burst (~240 mA spike). You **MUST** use a separate 5 V buck converter from the 12 V rail.

For breadboard prototyping (before the PCB exists):
1. Use a small **MP1584 or LM2596 buck module** (~KES 250 from Pixel) tied to the 12 V supply.
2. Adjust the trim pot to output **5.0 ± 0.1 V** (measure with multimeter before connecting CAM).
3. Connect CAM 5 V to the buck output.
4. Add a **470 µF electrolytic capacitor** across the CAM 5 V/GND pins, right at the module. This buffers the inrush at camera startup.

### 4.2 UART wiring
| ESP32-CAM | Main ESP32 | Wire colour |
|---|---|---|
| 5 V (VCC) | (CAM-dedicated buck output, NOT main 5 V) | red |
| GND | Common GND | black |
| UOT (U0T / GPIO 1, this is CAM TX) | ESP32 GPIO 16 (`PIN_VISION_RX`, alias `PIN_UART2_RX`) | yellow |
| UOR (U0R / GPIO 3, this is CAM RX) | (leave open — main → CAM is not used in v1) | — |

Use **22 AWG silicone wire**, length ≤ 30 cm. Longer runs need shielded cable.

### 4.3 Verify no shorts
Before powering on, multimeter continuity test (beep mode):
- 5 V to GND on CAM module: should NOT beep.
- 12 V to GND on supply: should NOT beep.
- ESP32 GND to CAM GND: should beep (common ground confirmed).

### 4.4 Power up + verify
1. Power 12 V supply (current limit 1 A).
2. Power ESP32 main board via USB.
3. CAM should boot — its onboard red LED comes on.
4. Open main board's serial monitor. After ~3 s the CAM JSON starts arriving over UART2.
5. Watch the main board log:
   ```
   [vision] UART2 init @ 115200
   ...
   ```
   No `[fault] raised: 0x... (vision-lost)` lines should appear after the first 3 seconds.

If you DO see vision-lost faults:
- Check TX/RX swap (CAM TX must go to main RX, NOT TX-to-TX).
- Check baud rate matches (both ends should be 115200).
- Check common ground.

---

## Step 5 — HC-SR04 ultrasonic install (4 sensors, 2 pairs)

### 5.1 What an HC-SR04 does
The HC-SR04 emits a 40 kHz ultrasonic chirp on its TRIG pin and reports the round-trip flight time as a pulse width on its ECHO pin. The firmware in `ultrasonic.cpp` line 33 (`measure_cm()`) divides by 58 to convert microseconds to centimetres (sound travels ~343 m/s).

### 5.2 Critical wiring detail: ECHO needs a level shifter
HC-SR04 modules are **5 V devices**. The ECHO pin swings 0–5 V. ESP32 GPIOs are **3.3 V tolerant only** — they will be slowly damaged (and read garbage in the meantime) if you feed them 5 V directly.

**You must add a 1 kΩ + 2 kΩ voltage divider on every ECHO line** to drop the level to ~3.3 V at the ESP32 pin.

For each of the 4 sensors:
```
HC-SR04 ECHO pin ──┬── 1 kΩ ────┬── (no connection further)
                   │             │
                   │             ▼
                   │           ESP32 GPIO (PIN_USx_ECHO)
                   │             │
                   │             │
                   └── 2 kΩ ────┴── GND
```

The TRIG pin can connect directly to ESP32 (3.3 V output is enough to trigger the SR04).

### 5.3 Pin assignments (per `pin_config.h`)
| Sensor | TRIG pin | ECHO pin | Role |
|---|---|---|---|
| US1 | GPIO 5 | GPIO 18 | Upstream Beam A |
| US2 | GPIO 19 | GPIO 21 | Upstream Beam B |
| US3 | GPIO 22 | GPIO 23 | Downstream Beam A |
| US4 | GPIO 1 (USB TX, see note) | GPIO 3 (USB RX, see note) | Downstream Beam B |

> **US4 conflict warning:** GPIO 1 and 3 are also UART0 (USB serial). When the USB cable is plugged in for monitoring, US4 will give garbage readings. For demo, either accept that US4 is dead during USB monitoring (US3 alone still works for downstream), or unplug the USB and rely on the touchscreen for diagnostics.

### 5.4 Beam geometry
Each pair has Beam A and Beam B spaced **3 cm apart** along the direction of vessel travel. The 3 cm value is in `system_types.h` as `ULTRASONIC_BEAM_SPACING_CM = 3`.

Why 3 cm? At 20 Hz tick rate, the firmware reads each sensor every 200 ms. A vessel moving at 50 mm/s (slow toy boat) takes 600 ms to traverse 3 cm — that's 3 sample windows, plenty of time to register the order Beam A → Beam B (or vice versa).

If you're testing with hand sweeps (much faster than a toy boat), bump the spacing to 5 cm and update the constant.

### 5.5 Mount the sensors
**Upstream pair** (US1 + US2):
- Position: 50 mm above the waterway level, 100 mm from the bridge edge.
- Both sensors point ACROSS the waterway, perpendicular to vessel travel direction.
- US1 (Beam A) is the FIRST sensor a vessel approaching from upstream encounters.
- US2 (Beam B) is 3 cm closer to the bridge.

**Downstream pair** (US3 + US4):
- Position: 50 mm above waterway, 100 mm on the other side of the bridge.
- US3 (Beam A) is FIRST sensor a vessel approaching from downstream encounters.
- US4 (Beam B) is 3 cm closer to the bridge.

> **Mental model:** "Beam A" always means "the outer one, hit first by an approaching vessel". "Beam B" always means "the inner one, closer to the bridge".

### 5.6 Direction inference algorithm (already in `ultrasonic.cpp`)
Read lines 26–72. Each pair maintains two history rings of 5 samples each (one for Beam A, one for Beam B). Each ring stores 1 if the beam was blocked at that sample, 0 if clear.

`infer_direction()`:
- Finds the index of the first 0→1 rising edge in each ring.
- If A's rising edge came BEFORE B's → DIR_APPROACHING.
- If B's rising edge came BEFORE A's → DIR_LEAVING.
- If both at the same index → DIR_BOTH (ambiguous).
- If only one beam saw a transition → that beam infers direction alone.

The combined `vessel_approaching` flag is true if either pair reports DIR_APPROACHING.

### 5.7 Bench test (one sensor at a time)
Before mounting all 4, validate one sensor:
1. Wire ONE HC-SR04 (US1, TRIG=GPIO 5, ECHO=GPIO 18 via divider) to the breadboard.
2. Add a temporary print to `sensors_ultrasonic_tick()`:
   ```cpp
   Serial.printf("US1=%u\n", s_us.distance_us1_cm);
   ```
3. Build, flash, monitor.
4. Hold a flat object (book, hand) at 30 cm in front of US1 — should read ~30. Move to 60 cm — reads ~60.
5. If reading is 0xFFFF (65535) consistently:
   - Check 5 V at the SR04 module pins (must be ≥ 4.8 V).
   - Check ECHO divider isn't shorting to GND.
   - Try a different SR04 module (DOA rate is ~5%).
6. Remove the temporary print.

### 5.8 Bench test all four
With all 4 wired:
1. Add temporary prints for all 4:
   ```cpp
   Serial.printf("US: %5u %5u %5u %5u\n",
                 s_us.distance_us1_cm, s_us.distance_us2_cm,
                 s_us.distance_us3_cm, s_us.distance_us4_cm);
   ```
2. Block each sensor in turn with your hand. Confirm the right column changes.

### 5.9 Direction-inference bench test
1. Pass your hand slowly through US1's beam, then 1 second later through US2's beam (mimicking a vessel approaching from upstream).
2. Watch for `g_status.ultrasonic.upstream_direction == DIR_APPROACHING` and `vessel_approaching = true`.
3. Reverse: hand through US2 first, then US1 — should infer DIR_LEAVING, no `EVT_VEHICLE_DETECTED` posted.
4. Repeat for US3/US4 (downstream pair).

If direction inference is unreliable:
- Hand is moving too fast — slow down. The 5-sample ring spans 1 second at 20 Hz round-robin.
- Reduce ring size from 5 to 3 in `#define DIR_HIST 3`.
- Increase beam spacing to 5 cm and update `ULTRASONIC_BEAM_SPACING_CM`.

### 5.10 Tune `ULTRASONIC_TRIGGER_CM`
Default in `system_types.h` is **80 cm**. A beam reading less than this is "blocked".
- Larger value (e.g. 150 cm) = sensitive to far objects, more false positives.
- Smaller value (e.g. 30 cm) = only triggers on close objects, may miss small/dark vessels.
- Tune to your bench rig: measure the typical waterway width, set trigger to slightly less than that.

---

## Step 6 — Verify the JSON link to the main board (full integration)

With CAM + main board both powered:

1. Main board monitor at 115200. You should see the normal boot log.
2. After CAM boots (~3 s after main): no `[fault] vision-lost` lines should appear.
3. Wave a hand in front of CAM. Main board log should show:
   ```
   [fsm] 0 -> 1     (IDLE -> ROAD_CLEARING — vehicle detected by vision)
   ```
4. If no transition fires:
   - Add a temporary print at the top of `vision_link.cpp` `parse_line()`:
     ```cpp
     Serial.printf("RX line: %s\n", line);
     ```
     This confirms bytes are arriving and parsing.
   - If lines are arriving but `vehicle_present` stays 0, the JSON is being parsed but `"v"` isn't reaching 1 — re-check ROI tuning.
   - If no lines are arriving, check UART wiring (TX/RX swap, common ground).

### 6.1 Common JSON-link debug fixes
| Symptom | Cause | Fix |
|---|---|---|
| Garbage characters (random unicode-looking junk) | Baud rate mismatch | Confirm CAM is 115200 (line 54 of `firmware_cam/src/main.cpp`) AND `vision_link.cpp` line 57 uses 115200 |
| No bytes at all | TX/RX swapped | CAM TX (U0T) → main RX (GPIO 16). Not TX→TX. |
| Bytes arriving but parse fails | ArduinoJson 6 vs 7 mismatch | Confirm `platformio.ini` has `bblanchon/ArduinoJson @ ^7.2.1` |
| Brownout reset on CAM (every few seconds) | Shared 5 V with main | Move CAM to dedicated buck (Step 4.1) |
| `[fault] vision-lost` flickering on/off | UART noise or weak link | Shorten wires; add 100 nF cap from RX to GND near ESP32 |

---

## Step 7 — Combined OR detection test

This is the integration milestone for end of week 5.

### 7.1 Setup
- All 4 ultrasonics wired and reading.
- ESP32-CAM wired and reading.
- Main board firmware running normally.
- Open serial monitor.

### 7.2 Test cases
| # | Action | Expected log |
|---|---|---|
| 1 | Wave hand in front of CAM only | `[fsm] 0 -> 1` (vision triggered) |
| 2 | Wait until cycle returns to IDLE. Sweep hand through US1 then US2 (upstream pair, A→B order) | `[fsm] 0 -> 1` (ultrasonic triggered) |
| 3 | Sweep hand through US2 then US1 (B→A order) | NO transition (DIR_LEAVING) |
| 4 | Cover the camera with a book. Then sweep US1→US2 normally. | `[fsm] 0 -> 1` (vision blocked = no false positive; ultrasonic still works) |
| 5 | Disconnect all 4 ultrasonics. Wave hand in front of CAM. | `[fsm] 0 -> 1` (vision still works alone). Also: `[fault] raised: us-fail` |
| 6 | Reconnect ultrasonics. Wait. Fault should clear. | `[fault] cleared` |

If all 6 pass: you're done with detection.

---

## Step 8 — Long-run stability test

A 1-hour idle test catches glitches that don't appear in 5-minute sessions.

1. Boot the main board. Confirm no faults at 30 seconds.
2. Leave it running for 1 hour, untouched.
3. After 1 hour:
   - Check serial log for any `[fault]` lines. Goal: zero.
   - Check the HMI for any latched faults.
4. If `[fault] vision-lost` appears intermittently:
   - Probably loose UART wiring. Crimp + heatshrink the connections.
5. If `[fault] us-fail` appears intermittently:
   - One of the 4 sensors is intermittently DOA. Identify by adding the per-sensor print from 5.8. Replace that sensor.

---

## Step 9 — Weekly milestones

| Week | Goal | Sign-off |
|---|---|---|
| 1 | Arduino IDE + ESP32 board package installed. CAM first-flash working. | Boot JSON visible |
| 2 | ROI tuned, motion detection reliable in lab lighting | Toy car triggers `v=1` 10/10 times |
| 3 | UART2 link to main board working — vision-lost fault clears | M1 confirms `EVT_VEHICLE_DETECTED` arrives in FSM |
| 4 | All 4 HC-SR04 wired with dividers. Each reads correct distances. | Per-sensor bench test passes |
| 5 | Direction inference works for both upstream + downstream pairs | 8/10 hand sweeps correct direction |
| 6 | Vision + ultrasonic agree (FSM uses OR of both) | Cover-camera test still triggers via ultrasonic |
| 7 | Long-run stability (1 hr idle, no faults) | Stability log committed |
| 8 | Demo polish — cable management, secure mounts | Lecturer demo |

---

## Step 10 — Failure recovery cookbook

| Symptom | Most likely cause | First fix to try |
|---|---|---|
| CAM upload fails "Failed to connect" | IO0 not grounded during reset | Re-check IO0-GND jumper. Press RST while jumper held, click Upload immediately after release |
| CAM boot loop with `Brownout detector triggered` | Shared 5 V with main | Move CAM to dedicated buck + 470 µF cap |
| Endless `"v":0` even with motion | ROI off-frame or threshold too high | Drop `DETECT_PCT_THRESH` from 6 to 3, re-test. If still 0, re-check ROI position. |
| Endless `"v":1` (false positive storm) | Background flicker / fluorescent light | Increase `MOTION_THRESHOLD` from 24 to 36 |
| `[fault] vision-lost` at 2 s intervals | UART line glitching | Shorten wire, add 100 nF cap from RX to GND |
| HC-SR04 reads 0xFFFF (no echo) consistently | Sensor faulty or 5 V rail dead | Swap sensor; check 5 V at module pins ≥ 4.8 V |
| HC-SR04 reads correct values but 0xFFFF every 5th sample | Cross-talk between pairs | Increase delay between sensors in `sensors_ultrasonic_tick()` from 0 to 5 ms |
| Direction always reports DIR_BOTH | Both beams blocked simultaneously | Increase beam spacing to 5 cm OR slow the test object |
| Direction always DIR_APPROACHING (even on retreat) | A and B physical positions swapped | Swap which sensor is wired to TRIG_A vs TRIG_B |
| US4 reads garbage when USB plugged | UART0 conflict | Expected. For demo, unplug USB or accept US3-only downstream |
| `EVT_VEHICLE_DETECTED` fires repeatedly during one approach | Edge detection broken | Check `s_was_approaching` static var in `sensors_ultrasonic_tick()` |

---

## Completion checklist

Hardware:
- [ ] ESP32-CAM mounted at fixed position above road, looking down 60° tilt.
- [ ] CAM 5 V supply is a dedicated buck from 12 V (NOT shared with main).
- [ ] 470 µF cap at CAM 5 V/GND.
- [ ] All 4 HC-SR04 mounted with 1 kΩ + 2 kΩ ECHO dividers.
- [ ] Beam pairs spaced 3 cm apart, oriented across vessel travel direction.
- [ ] All wires labelled (CAM-TX, US1-TRIG, etc.).

Firmware:
- [ ] CAM firmware flashed, ROI tuned for your mount position, thresholds documented.
- [ ] All 4 ultrasonics confirmed reading (per-sensor bench test).
- [ ] Direction inference 8/10 correct for both pairs.
- [ ] Vision JSON arriving at ≥ 8 Hz steady state on UART2.
- [ ] `[fault] vision-lost` and `[fault] us-fail` both clear after 30 s of healthy operation.
- [ ] Combined OR detection test (Step 7) passes all 6 cases.

Documentation:
- [ ] `docs/sensor_calibration.md` filled with measured ROI / thresholds / trigger distance.
- [ ] Wiring photos in `docs/wiring/` (CAM, each ultrasonic pair).
- [ ] 1-hour stability log committed (zero faults).
