# Member 1 — George — System Architecture & Finite-State Machine

> **Your role.** You are the integrator. The mechanical (M2), sensing (M3), HMI (M4) and PCB/safety (M5) modules all hand off to **your** state machine. When module A talks to module B and something breaks, the buck stops at you. You will spend 60% of your time reading other people's code and the remaining 40% writing 200 lines that decide what the bridge actually does.
>
> **Your single deliverable.** A boot-to-cycle sequence where pressing `v` on the serial monitor causes: barriers close → counterweights balance → bridge raises → stops at top → holds → lowers → barriers reopen. End to end, no manual intervention, in under 25 seconds.

---

## 0. What you are building (read this once before touching anything)

The Vertical Lift Bridge is a **scale model bridge that lifts its road deck out of the way when a boat needs to pass underneath**. A road runs across it. Cars stop, barriers come down, the deck rises, the boat passes, the deck comes back down, barriers open, cars go again.

The brain is an **ESP32-WROOM-32E** running **FreeRTOS**. Eight tasks share one chip:

| Task | Owner | Pinned to | Job |
|---|---|---|---|
| `task_safety` | M5 | Core 0 | Watchdog + relay + barriers, 20 Hz |
| `task_fsm` | **You** | Core 0 | Read events from queue, decide next state |
| `task_motor` | M2 | Core 0 | PWM the BTS7960, read current sense |
| `task_sensors` | M3 | Core 0 | Trigger ultrasonics, infer direction |
| `task_vision` | M3 | Core 0 | Parse JSON from ESP32-CAM over UART2 |
| `task_counterweight` | M2 | Core 0 | Simulate the water-tank counterweights |
| `task_telemetry` | All | Core 0 | Update uptime, CPU load, 1 Hz |
| `task_hmi` | M4 | Core 1 | LVGL render + touchscreen, 60 Hz |

Your code is in `firmware/src/fsm/`. Three files: `fsm_engine.cpp`, `fsm_guards.cpp`, `fsm_actions.cpp`. Your code is also `firmware/src/main.cpp` because **you** create those tasks.

---

## Scope of your work — every file you touch

| File | Lines | Created? | What you do |
|------|---:|---|--------------|
| `firmware/platformio.ini` | 60 | ✅ exists | Maintain — accept M5/M2 lib_deps additions |
| `firmware/src/pin_config.h` | 230 | ✅ exists | Sign off on every change. Never edit without M5 review. |
| `firmware/src/system_types.h` | 270 | ✅ exists | Single source of truth for enums. Anyone adding a state/event/fault MUST PR to you. |
| `firmware/src/main.cpp` | 260 | ✅ exists | You own task creation, priorities, core affinity, queues |
| `firmware/src/fsm/fsm_engine.{h,cpp}` | 150 | ✅ exists | The 9-state transition table. Yours alone. |
| `firmware/src/fsm/fsm_guards.{h,cpp}` | 70 | ✅ exists | Boolean preconditions for each transition |
| `firmware/src/fsm/fsm_actions.{h,cpp}` | 120 | ✅ exists | Side-effects on entry/exit |
| `docs/demo_script.md` | 100 | ❌ create | Step-by-step demo script for the lecturer day |
| `docs/integration_log.md` | grow | ❌ create | One-line entry per integration session |

**Not your work** (do not modify):
- M2's `motor/`, `counterweight/` source
- M3's `sensors/`, `vision/` source, and the `firmware_cam/` subfolder
- M4's `hmi/`, `traffic/` source
- M5's `safety/` source, KiCad files, BOM
- Any printed mechanical part

You **call** their public APIs from `fsm_actions.cpp`. You do not change their innards.

---

## Step 1 — Set up your development machine (Windows 11, no prior knowledge assumed)

> Estimated time: 90 min. Do this on a fresh Windows install or a machine where you don't already have Python or a different VS Code Python extension.

### 1.1 Install VS Code
1. Open Microsoft Edge. Type `code.visualstudio.com`. Press Enter.
2. Click the big blue "Download for Windows" button. The file `VSCodeUserSetup-x64-1.<version>.exe` lands in `C:\Users\<you>\Downloads\`.
3. Double-click the `.exe`. Click "Yes" if Windows asks "Do you want to allow this app to make changes?".
4. Accept the licence agreement. Click Next on each step until the **"Select Additional Tasks"** screen.
5. **Tick all four boxes under "Other"**:
   - ☑ Add "Open with Code" action to Windows Explorer file context menu
   - ☑ Add "Open with Code" action to Windows Explorer directory context menu
   - ☑ Register Code as an editor for supported file types
   - ☑ Add to PATH (requires shell restart)
6. Next → Install. When done, tick "Launch Visual Studio Code" → Finish.

### 1.2 Install Git for Windows
1. In Edge, navigate to `git-scm.com/download/win`.
2. The 64-bit installer downloads automatically. Run it.
3. Take all defaults **except**:
   - **"Choosing the default editor used by Git"** → Use **Visual Studio Code** as Git's default editor.
   - **"Adjusting the name of the initial branch in new repositories"** → Override the default branch name → type `main`.
4. Verify: open Command Prompt (press `Win+R`, type `cmd`, Enter). Type:
   ```cmd
   git --version
   ```
   You should see `git version 2.4x.x.windows.1`.

### 1.3 Tell Git who you are
In Command Prompt:
```cmd
git config --global user.name "George Mwangi"
git config --global user.email "george@students.jkuat.ac.ke"
```
Use your real student email — your commits will carry it.

### 1.4 Install Python 3.11 (PlatformIO needs it; do NOT use 3.12)
1. Browse to `python.org/downloads/windows/`.
2. Find the latest **3.11.x** Windows installer (64-bit). NOT 3.12 — PlatformIO will not work cleanly on 3.12 yet.
3. Run the installer. **Critical:** before clicking "Install Now", tick **☑ Add python.exe to PATH** at the bottom of the dialog.
4. Verify in Command Prompt:
   ```cmd
   python --version
   pip --version
   ```
   Both must respond.

### 1.5 Install PlatformIO inside VS Code
1. Open VS Code.
2. On the left edge, click the Extensions icon (four squares with one detached). Or press `Ctrl+Shift+X`.
3. In the search box at the top of the panel, type `platformio ide`.
4. The first result should say "PlatformIO IDE — PlatformIO". Click **Install**. (~3 min download.)
5. When it finishes, the orange progress bar at the bottom of VS Code will turn into an alien-head icon 👽 on the left sidebar. VS Code may prompt you to reload — click "Reload Window".
6. Wait until the alien-head badge has no spinner next to it (PlatformIO finishes initialising on first run, ~2 min).

### 1.6 Install the CH340 USB driver (so your ESP32 board appears as a COM port)
The ESP32 DevKit boards sold in Nairobi use a CH340 USB-UART chip that Windows 11 does not recognise out of the box.
1. Plug your ESP32 board into a USB port using a known-good USB-C or micro-USB data cable. **Many cheap cables are charge-only — use one you know carries data.**
2. Open Device Manager: press `Win+X`, click "Device Manager".
3. Look under **Ports (COM & LPT)**. If you see "USB-SERIAL CH340 (COM<n>)" → the driver is already there. Note the COM number.
4. If you instead see "Unknown USB Device" or a yellow ⚠ next to "USB2.0-Serial":
   - Open Edge, navigate to `wch-ic.com/downloads/CH341SER_EXE.html`.
   - Click the download link, run `CH341SER.EXE`, click "INSTALL".
   - Wait until "Driver install success" pops up. Click OK.
   - Unplug + replug the ESP32. Re-check Device Manager.
5. Write the COM number on a sticky note. You will type it into `platformio.ini` later if you ever need to override.

### 1.7 Clone the repository
1. Pick a folder you will remember. The Documents folder is fine:
   ```cmd
   cd C:\Users\%USERNAME%\Documents
   git clone https://github.com/kiogimwenda/vertical-lift-bridge.git
   cd vertical-lift-bridge
   ```
2. In VS Code: File → Open Folder → navigate to `vertical-lift-bridge` → Select Folder. VS Code may ask "Do you trust the authors of the files in this folder?" — click "Yes, I trust the authors".
3. PlatformIO detects the `platformio.ini` and starts indexing. Watch the bottom status bar. You will see "Initializing tasks…" for ~2 min on first open. Let it finish before doing anything.

### 1.8 Sanity-check your install — build, but don't flash yet
1. On the left sidebar, click the alien-head 👽 (PIO Home).
2. Click "Project Tasks" in the left panel inside the PIO Home tab.
3. Expand `vlb_main` → `General` → click **Build**.
4. The first build downloads the Xtensa toolchain (~150 MB, takes ~5 min on Kenyan broadband). Subsequent builds finish in 30 s.
5. **Expected output** (in the terminal at the bottom):
   ```
   Linking .pio\build\vlb_main\firmware.elf
   Retrieving maximum program size
   Checking size .pio\build\vlb_main\firmware.elf
   RAM:   [=         ]   13.2% (used 43360 bytes from 327680 bytes)
   Flash: [==        ]  19.8% (used 622384 bytes from 3145728 bytes)
   ===== [SUCCESS] Took 28.4 seconds =====
   ```
6. If the build fails, **read the very first error in the log** (later errors are usually cascade noise). Most first-time failures are missing libraries, which PlatformIO downloads automatically on retry.

---

## Step 2 — Read everything before writing anything (3 hours, no skipping)

You cannot integrate code you have not read. Before week 1 ends, you must have read every line of every file listed below, in this order, with a notebook open. Write down every question that occurs to you — those questions become your acceptance tests.

### 2.1 The pin map — `firmware/src/pin_config.h`
Open it. You will see one #define per GPIO, then a long history of `#undef ... #define` blocks documenting how every pin allocation conflict was resolved. **Read every comment.** Pay special attention to:
- Which pins are input-only (GPIO 34–39 — no internal pull-up).
- Which pins double-duty between two peripherals (servos overlap with US TRIG; buzzer overlaps with USB TX after boot).
- The end of the file lists **backward-compat aliases** so the C++ files compile against the canonical names you see at the top.

The file ends with a `v2.1` changelog entry explaining the rail-sense and resistor-ladder removals (see `docs/known_limitations.md`).

### 2.2 The system enums — `firmware/src/system_types.h`
This is the contract between modules. Memorise the structure:
- `SystemState_t` — 9 states (IDLE, ROAD_CLEARING, RAISING, RAISED_HOLD, LOWERING, ROAD_OPENING, FAULT, ESTOP, INIT).
- `SystemEvent_t` — 17 events. Producers post to `g_event_queue`; you consume from inside `task_fsm`.
- `FaultFlag_t` — 16 bits (one is deprecated). Set by any module via `SET_FAULT(g_status.fault_flags, FAULT_X)`.
- `MotorCommand_t` — direction + duty + a request_id you bump every send.
- `SharedStatus_t` — the BIG struct guarded by `g_status_mutex`. Every field has one writer; many readers.

### 2.3 Boot + task wiring — `firmware/src/main.cpp`
Walk through `setup()` line by line:
1. Serial init at 115200.
2. Print boot banner.
3. Create the mutex and the 4 queues (event, motor_cmd, hmi_cmd, status_mutex).
4. Init shared state (`g_status.state = STATE_INIT`).
5. Init each peripheral — order matters: watchdog first (so a dying init still gets caught), then fault register, then interlocks, then motor, then sensors, vision, lights, buzzer, counterweight.
6. Create 7 tasks pinned to Core 0 + 1 task (HMI) on Core 1.
7. Push EVT_NONE to kick the FSM out of INIT.

Each task body is a `for(;;)` loop. Some are event-driven (`xQueueReceive`); some are time-driven (`vTaskDelayUntil` at 20 Hz / 1 Hz).

### 2.4 The state machine — `firmware/src/fsm/fsm_engine.cpp`
Read top to bottom. The transition function is one big switch:
```cpp
void fsm_engine_handle(SystemEvent_t evt) {
    if (evt == EVT_ESTOP_PRESSED) { enter_state(STATE_ESTOP); return; }
    if (evt == EVT_FAULT_RAISED && s_state != STATE_ESTOP) { enter_state(STATE_FAULT); return; }
    switch (s_state) {
        case STATE_IDLE: ...
        ...
    }
}
```
The two top-of-function checks make E-stop and Fault **universal** transitions: regardless of where you are, those events take you to the safe state. This is the single most important design pattern in the FSM.

### 2.5 Guards — `firmware/src/fsm/fsm_guards.cpp`
Five boolean functions. Each takes the mutex briefly, reads `g_status`, returns a bool. **Side-effect free.** If you ever add a guard that writes to `g_status`, you have introduced a bug.

### 2.6 Actions — `firmware/src/fsm/fsm_actions.cpp`
On entry to each state: light the appropriate traffic lights, send a motor command, raise/lower barriers, chirp the buzzer, notify the HMI. On exit from RAISING / LOWERING: brake the motor.

The reason actions are split from the engine is so you can read **what the bridge does in each state** in one screen, separate from **how the FSM decides to enter that state**.

---

## Step 3 — First successful flash (ESP32 only — no motor, no peripherals)

> Goal: get the boot banner + all `[xxx] init OK` lines on serial. No mechanical hardware required.

### 3.1 Wire up just the ESP32
- ESP32 DevKit module + USB cable, plugged into your laptop. Nothing else.

### 3.2 Upload
1. PlatformIO Project Tasks → vlb_main → General → **Upload**.
2. As soon as the terminal says `Connecting...______`, hold the **BOOT** button on the ESP32 for 1 second, then release.
3. Wait for "Hard resetting via RTS pin..." — flash is done.

> Some boards auto-reset and don't need the BOOT button. Try without first; if it times out, re-flash with the BOOT-hold trick.

### 3.3 Open serial monitor
1. PlatformIO Project Tasks → vlb_main → General → **Monitor**.
2. **Expected log** (memorise this — you will see it 100 times):
   ```
   === VLB Group 7 — Boot ===
   ESP32 SDK: v4.4.x | Sketch: <date>
   [wdt] init OK (5s hw, 1.5s sw)
   [fault] init OK (rail monitoring disabled — see known_limitations.md)
   [ilk] init OK (relay off until first OK eval)
   [motor] init OK
   [us] init OK (4 sensors, upstream + downstream)
   [vision] UART2 init @ 115200
   [lights] init OK
   [buz] init OK
   [cw] init OK (simulated dynamic counterweight)
   [boot] Peripherals OK
   [boot] Tasks created — entering scheduler
   [hmi] task start (Core 1)
   [fsm] 8 -> 0       <-- INIT (8) to IDLE (0)
   ```
3. The very last `[fsm] 8 -> 0` line is the proof that your FSM ran `fsm_guard_init_complete()`, got `true` (no faults, no e-stop), and dropped into `STATE_IDLE`.
4. Without HMI hardware connected, you may also see periodic `[fault] raised: 0x... (vision-lost)` lines because no ESP32-CAM is sending JSON. **This is expected on a bench rig** until M3 connects the camera. The bridge stays in STATE_FAULT in that case.

> If you only see "=== VLB Group 7 — Boot ===" and nothing else, your monitor baud is wrong. Check that PlatformIO monitor speed is **115200**.

### 3.4 Commit your "skeleton compiles" milestone
```cmd
git add -A
git commit -m "chore: skeleton compiles + boots to IDLE on bench"
git push
```

---

## Step 4 — Understand the FSM by walking through one full cycle

The bridge does this loop:

```
IDLE → ROAD_CLEARING → RAISING → RAISED_HOLD → LOWERING → ROAD_OPENING → IDLE
```

Each arrow is triggered by an **event** and conditioned by a **guard**. On entering each state, **actions** fire.

### 4.1 The event ladder for one normal cycle

| # | From state | Event | Guard | To state | Actions on entry |
|---|---|---|---|---|---|
| 1 | IDLE | `EVT_VEHICLE_DETECTED` (from M3 vision OR ultrasonic) | `fsm_guard_can_clear_road()` — returns true if vehicle present, no fault, no e-stop | ROAD_CLEARING | road=AMBER, marine=RED, buzzer chirp×2, barriers DOWN, counterweight target set |
| 2 | ROAD_CLEARING | `EVT_BARRIER_CLOSED` (from M5 interlocks after 1 s timer) **AND** `EVT_CW_READY` (from M2 counterweight when both tanks balanced) | `fsm_guard_road_clear()` — barriers down + no vessel in zone + counterweight ready | RAISING | road=RED, marine=RED, motor command UP at 67% duty |
| 3 | RAISING | `EVT_TOP_LIMIT_HIT` (from M2 motor task when limit switch closes) | (none) | RAISED_HOLD | motor BRAKE, road=RED, marine=GREEN |
| 4 | RAISED_HOLD | `EVT_HOLD_TIMEOUT` (from FSM internal timer after 8 s) **OR** `EVT_OPERATOR_LOWER` (from HMI button) | (none) | LOWERING | motor command DOWN at lower duty, marine=AMBER |
| 5 | LOWERING | `EVT_BOTTOM_LIMIT_HIT` (from M2 motor task) | (none) | ROAD_OPENING | motor STOP, road=AMBER, buzzer chirp×1, barriers UP |
| 6 | ROAD_OPENING | `EVT_BARRIER_OPEN` (from M5 timer) | `fsm_guard_road_opened()` — both barriers reached UP | IDLE | road=GREEN, marine=RED, motor STOP |

> **The hold-timeout event is not yet wired up in `fsm_engine.cpp` at the time of writing.** Adding it is one of your week-3 tasks (see Step 8 below).

### 4.2 The two universal transitions

These can happen from **any** state:

| Trigger | Where it leads |
|---|---|
| `EVT_ESTOP_PRESSED` (mushroom button) | STATE_ESTOP — motor coasts, all lights RED, buzzer alarm |
| `EVT_FAULT_RAISED` (any module sets `fault_flags`) | STATE_FAULT — motor brakes, all lights RED, buzzer fault pattern |

To leave STATE_ESTOP, the operator must release the mushroom button **AND** all faults must be clear. To leave STATE_FAULT, the operator presses CLEAR FAULT on the HMI.

---

## Step 5 — The serial test harness (lets you drive the FSM without any hardware)

You cannot wait for M2's motor to be done before testing your FSM. Add this dev-only injector to the **end of `loop()` in `main.cpp`**:

```cpp
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
    safety_watchdog_kick_main();

    // --- DEV-ONLY: serial event injection ---
    // Type single letters in monitor + press Enter to inject events.
    // Remove this block (or wrap in #ifdef DEV_HARNESS) before demo.
    static char buf[8]; static size_t blen = 0;
    while (Serial.available() && blen < sizeof(buf)-1) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            buf[blen] = 0; blen = 0;
            SystemEvent_t e = EVT_NONE;
            if      (!strcmp(buf, "v")) e = EVT_VEHICLE_DETECTED;
            else if (!strcmp(buf, "c")) e = EVT_VEHICLE_CLEARED;
            else if (!strcmp(buf, "t")) e = EVT_TOP_LIMIT_HIT;
            else if (!strcmp(buf, "b")) e = EVT_BOTTOM_LIMIT_HIT;
            else if (!strcmp(buf, "r")) e = EVT_OPERATOR_RAISE;
            else if (!strcmp(buf, "l")) e = EVT_OPERATOR_LOWER;
            else if (!strcmp(buf, "B")) e = EVT_BARRIER_CLOSED;
            else if (!strcmp(buf, "O")) e = EVT_BARRIER_OPEN;
            else if (!strcmp(buf, "k")) e = EVT_CW_READY;
            else if (!strcmp(buf, "f")) e = EVT_FAULT_RAISED;
            else if (!strcmp(buf, "x")) e = EVT_FAULT_CLEARED;
            else if (!strcmp(buf, "E")) e = EVT_ESTOP_PRESSED;
            else if (!strcmp(buf, "R")) e = EVT_ESTOP_RELEASED;
            if (e != EVT_NONE) xQueueSend(g_event_queue, &e, 0);
        } else { buf[blen++] = c; }
    }
}
```

### 5.1 Driving a full cycle from the keyboard

In the monitor, press the keys in order, pressing Enter after each:

```
v  ↵     → [fsm] 0 -> 1   (IDLE -> ROAD_CLEARING)
B  ↵     → no transition yet, waiting for counterweight
k  ↵     → [fsm] 1 -> 2   (ROAD_CLEARING -> RAISING)
t  ↵     → [fsm] 2 -> 3   (RAISING -> RAISED_HOLD)
l  ↵     → [fsm] 3 -> 4   (RAISED_HOLD -> LOWERING)
b  ↵     → [fsm] 4 -> 5   (LOWERING -> ROAD_OPENING)
O  ↵     → [fsm] 5 -> 0   (ROAD_OPENING -> IDLE)
```

If any of these arrows fails to fire, the next-state guard returned false. Inspect what `g_status` looks like at the moment of refusal:

```cpp
// Add this temporarily to fsm_engine.cpp's enter_state() to dump on every transition:
Serial.printf("[fsm] guards: barriers=%d/%d us_up=%d us_dn=%d cw=%d faults=0x%lx\n",
              g_status.barrier_left_target_reached,
              g_status.barrier_right_target_reached,
              g_status.ultrasonic.upstream_blocked,
              g_status.ultrasonic.downstream_blocked,
              g_status.counterweight.balanced,
              (unsigned long)g_status.fault_flags);
```

---

## Step 6 — Driving the cycle end-to-end with real peripherals (week 3 onwards)

Once M2/M3 deliver, the keys above replace themselves with real sources:
- `v` (vehicle detect) ← M3 vision JSON `{"v":1,...}` OR ultrasonic direction = APPROACHING
- `B` (barrier closed) ← M5's `interlocks_evaluate()` after 1 s servo timer
- `k` (counterweight ready) ← M2's `counterweight_tick()` posts `EVT_CW_READY` when both tanks within 2 ml of target
- `t`, `b` (limit hits) ← M2's `motor_driver_tick()` reads PIN_LIMIT_ANYHIT
- `O` (barrier open) ← same servo timer, expanded from DOWN to UP

Your job at integration time is to **leave the harness in place** and verify each natural source fires the same event. If `v` from keyboard works but the vision JSON doesn't fire `EVT_VEHICLE_DETECTED`, the bug is in `vision/vision_link.cpp` — talk to M3, not your FSM.

---

## Step 7 — Adding a feature: new state, new event, new guard, new action

Eventually your team will ask "can the bridge do X?". Here is the safe procedure to extend the FSM without breaking integration:

### 7.1 Adding a new event
1. Open `firmware/src/system_types.h`.
2. Find `SystemEvent_t`. Add the new event **before EVT_COUNT**:
   ```c
   typedef enum : uint8_t {
       ...
       EVT_BOAT_PASSED_SENSOR,    // ← your new one
       EVT_COUNT
   } SystemEvent_t;
   ```
3. Decide the producer. If it comes from a sensor, it lives in M3's task. If it comes from the HMI, M4 posts it. Update that owner.
4. Decide the consumer. In `fsm_engine.cpp`, add a `case EVT_BOAT_PASSED_SENSOR:` arm to whichever state(s) react to it.
5. Add to the dev harness so you can trigger by keyboard.

### 7.2 Adding a new guard
1. In `fsm_guards.h`, declare:
   ```c
   bool fsm_guard_boat_clear_of_channel(void);
   ```
2. In `fsm_guards.cpp`, define it. **Take the mutex briefly. Read `g_status`. Release. Return.** No I/O.
3. Use it from `fsm_engine.cpp` inside the relevant transition.

### 7.3 Adding a new state
**This is the highest-impact change in the FSM. Follow every step.**
1. Add to `SystemState_t` in `system_types.h`, *before* `STATE_COUNT`. Increment any state-counted code (FSM debug arrays, HMI screen banners).
2. In `fsm_engine.cpp`, add a `case STATE_NEW:` arm to the switch with its valid outgoing transitions.
3. In `fsm_actions.cpp`, add `case STATE_NEW:` arms to **both** `on_entry()` and `on_exit()`. If the state has no exit cleanup, leave the exit case empty with a `break;`.
4. Update M4's `screen_create_main()` colour mapping (banner colour for the new state).
5. Update `member_guides/M1` (this file) with the new transition row.
6. Run the dev harness through the full lifecycle to confirm the new state is reachable and exit-able.

### 7.4 Adding a new fault
1. Add to `FaultFlag_t` in `system_types.h`. Bit numbers must not collide with existing bits. The next free bit is bit 16 (since 0..15 are used and bit 11 is deprecated).
2. The owner module sets/clears it via `SET_FAULT(g_status.fault_flags, FAULT_NEW)` / `CLR_FAULT(...)`.
3. In `fault_register.cpp`, add an entry to `fault_register_first_name()` so serial logs are human-readable.
4. In M4's HMI faults screen, add a row to `kFaultRows[]` so it shows on the dashboard.

---

## Step 8 — Pending work in the FSM (you will do these)

The current `fsm_engine.cpp` has known gaps. Address them in this order:

1. **`EVT_HOLD_TIMEOUT` is referenced but never produced.** STATE_RAISED_HOLD will sit forever unless an operator presses LOWER. Wire it:
   ```cpp
   // In task_fsm or fsm_engine, when entering STATE_RAISED_HOLD, capture millis().
   // Each EVT_TICK_100MS while in STATE_RAISED_HOLD, check (millis() - s_entered_ms) > HOLD_TIMEOUT_MS.
   // If so, post EVT_HOLD_TIMEOUT to g_event_queue.
   ```
   Add the check inside `case STATE_RAISED_HOLD:` in `fsm_engine_handle()`:
   ```cpp
   case STATE_RAISED_HOLD:
       if (evt == EVT_TICK_100MS && fsm_engine_state_age_ms() > HOLD_TIMEOUT_MS) {
           enter_state(STATE_LOWERING);
       } else if (evt == EVT_HOLD_TIMEOUT || evt == EVT_OPERATOR_LOWER) {
           enter_state(STATE_LOWERING);
       }
       break;
   ```
2. **`EVT_OPERATOR_HOLD` is declared but unused.** If the operator presses HOLD on the HMI during RAISING or LOWERING, the bridge should freeze in place. Add handling in those two states.
3. **`STATE_INIT` to `STATE_IDLE` is currently driven by an `EVT_TICK_100MS` after init_complete returns true.** Verify in your monitor that this happens within 200 ms of boot.

---

## Step 9 — The first hardware integration session (week 3 — when M2 has a motor)

> M2 will say "I have the motor turning under PWM". You then schedule a 2-hour bench session.

### 9.1 What to bring
- Your laptop with the repo + USB cable for the ESP32 DevKit.
- A small notebook for the integration log.
- A multimeter for sanity-checking voltages.

### 9.2 Pre-session checklist
- M2 has flashed `motor_driver.cpp`'s calibration sketch and recorded `CAL_COUNTS_PER_MM`.
- M2 has wired the BTS7960 module to the ESP32 per `pin_config.h` (GPIO 25, 26, 34, 35).
- The motor is bolted to the rig (not waving free).

### 9.3 Procedure
1. Power on. Open serial monitor. Confirm boot banner.
2. Type `r` Enter (operator raise). Watch:
   - `[fsm] 0 -> 1` (IDLE → ROAD_CLEARING) immediately.
   - The bridge sits in ROAD_CLEARING because no `EVT_BARRIER_CLOSED` arrives from M5 (servos may not be there yet) and no `EVT_CW_READY` (counterweight is simulated and posts after ~4 seconds).
3. After ~4 seconds: `[fsm] 1 -> 2` (ROAD_CLEARING → RAISING). The motor should start spinning UP.
4. After motor reaches the top limit switch: `[fsm] 2 -> 3`. The motor should stop and brake.
5. Type `l` Enter. `[fsm] 3 -> 4`. Motor reverses, deck descends.
6. After bottom limit: `[fsm] 4 -> 5`. Motor stops.
7. Wait 1 s for barrier-up timer: `[fsm] 5 -> 0`. Cycle complete.
8. Total cycle time should be **18–25 seconds**. Note the time in your integration log.

### 9.4 Integration log entry format
Make a `docs/integration_log.md`. Each session gets one entry:
```markdown
## 2026-05-15 — M1+M2 motor integration session

**Present:** George, Eugene
**Goal:** Verify FSM RAISING/LOWERING fire motor commands correctly.

**What worked:**
- Boot to IDLE clean.
- Manual `r` triggers full cycle, total 22.4 s.
- Limit switches fire `EVT_TOP_LIMIT_HIT` / `EVT_BOTTOM_LIMIT_HIT`.

**What broke:**
- Bridge overshot top by ~5 mm before brake engaged. Eugene to investigate
  active-brake duty in `motor_driver_apply()`.
- LOWERING speed too high (4 s for 175 mm). Eugene to drop
  MOTOR_PWM_LOWER_DEFAULT from 4500 to 3500.

**Next session:** 2026-05-22 — add M5 e-stop in series.
```

---

## Step 10 — Watchdog management (your responsibility, not M5's)

The watchdog in `safety/watchdog.cpp` (M5-owned) catches stuck tasks. It needs every long-running task to call `safety_watchdog_kick_<task>()` periodically. **You** have to make sure the FSM task does this.

Confirm your `task_fsm` body has a kick at the bottom of every loop iteration:
```cpp
static void task_fsm(void* arg) {
    fsm_engine_init();
    SystemEvent_t evt;
    TickType_t last_tick = xTaskGetTickCount();
    for (;;) {
        if (xQueueReceive(g_event_queue, &evt, pdMS_TO_TICKS(100)) == pdTRUE) {
            fsm_engine_handle(evt);
        } else {
            fsm_engine_handle(EVT_TICK_100MS);
        }
        safety_watchdog_kick_fsm();   // <-- this line
        vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(20));
    }
}
```

If your FSM ever does heavy work synchronously (e.g. blocks in a state action), you risk missing the kick. Rule: **state actions must complete in < 100 ms**. If something needs to wait, it posts a future event, doesn't block.

---

## Step 11 — Demo script (you write this in `docs/demo_script.md`)

The lecturer demo is 5–8 minutes. You drive. Write the script in advance and rehearse it twice. Suggested structure:

```markdown
# Demo script — Vertical Lift Bridge, Group 7

## 0:00 — Setup (off-camera)
- Confirm bench supply at 12 V / 3 A.
- Confirm USB-C connected to ESP32 DevKit.
- Confirm ESP32-CAM 5 V LED is lit.
- Confirm TFT shows BOOT screen.

## 0:30 — Power-on banner
"Here's our boot sequence — you can see each subsystem report OK."
[Show serial monitor for 10 s, then point to TFT showing IDLE.]

## 1:30 — Vehicle detection
"I'll wave my hand in front of the camera, simulating a car approaching."
[Wave. ROAD_CLEARING transition; barriers close visibly; HMI screen
shows COUNTERWEIGHT BALANCING animation; lights turn amber then red.]

## 2:30 — Bridge raises
"Once both barriers report closed AND the simulated counterweights
finish balancing, the bridge raises."
[Bridge raises, marine traffic light goes green, hold timer starts.]

## 4:00 — Bridge lowers automatically
"After 8 seconds at the top, the hold timer fires and the bridge lowers
on its own."
[Bridge lowers, hits bottom limit, barriers reopen, road green again.]

## 5:00 — E-stop demo
"Now let me show emergency stop."
[Press the red mushroom button mid-cycle.]
"All motion stops within 50 ms. The HMI shows EMERGENCY STOP. To recover,
I twist the button to release and press CLEAR on the HMI."

## 6:30 — HMI tour
[Tap NEXT to cycle MAIN → TELEMETRY → FAULTS → SETTINGS, narrating each.]

## 7:30 — Q&A buffer
```

---

## Step 12 — Weekly milestones

| Week | Goal | Acceptance test (objective, measurable) |
|---:|------|-----------------|
| 1 | Workstation setup. Skeleton compiles and flashes. Boot log reaches IDLE. | All `[xxx] init OK` lines visible. `[fsm] 8 -> 0` line visible. |
| 2 | Serial test harness in place. Drive a full cycle from keyboard. | Log shows `0→1→2→3→4→5→0` in correct order with each keypress. |
| 3 | Integration with M2 motor. Real `EVT_TOP_LIMIT_HIT` and `EVT_BOTTOM_LIMIT_HIT` fire. | One full hardware cycle from keyboard `r`. Cycle time logged. |
| 4 | Integration with M3 vision + ultrasonics. Real `EVT_VEHICLE_DETECTED` fires. | Wave hand → cycle starts within 1 second. |
| 5 | Integration with M4 HMI + M5 safety. CLEAR_FAULT button works. E-stop hard test. | E-stop kills motor in < 50 ms (oscilloscope or video at 60 fps). |
| 6 | Hold-timeout wired. Operator HOLD wired. Fault recovery tested for all 15 active faults. | Each fault flag can be raised + cleared from harness. |
| 7 | Endurance — 10 back-to-back cycles, no manual intervention, no latching faults. | Pass log committed to `integration_log.md`. |
| 8 | Demo script rehearsed twice. All teammates know their part of the demo. | Recorded video of full rehearsal under 8 min, no panic resets. |

---

## Step 13 — Failure recovery cookbook

| Symptom | Most likely cause | First fix to try |
|---|---|---|
| `Connecting......_____` then upload timeout | Auto-reset failed | Hold BOOT, click Upload, release BOOT after "Connecting" appears |
| Boot loops with `Brownout detector triggered` | USB hub can't supply enough current | Plug DevKit directly into laptop USB, not through a hub |
| `[fault] raised: 0x00000020 (vision-lost)` immediately at boot | ESP32-CAM not connected | Expected on bench rig — ignore, or unplug CAM connector entirely |
| `[fault] raised: 0x00000040 (us-fail)` continuously | All 4 ultrasonics returning 0xFFFF | Check 5 V on HC-SR04 modules with multimeter (must be ≥ 4.8 V) |
| FSM stuck in ROAD_CLEARING forever | `EVT_BARRIER_CLOSED` never fires (M5 servos absent) | In `fsm_guards.cpp`, temporarily set `g_status.barrier_*_target_reached = true` at boot |
| FSM stuck in ROAD_CLEARING for > 5 s after barriers closed | `EVT_CW_READY` never fires | In serial harness, type `k` Enter to manually post it |
| FSM enters STATE_FAULT and won't leave | `fault_flags != 0` even after operator clears | Check `fault_register_snapshot()`. Either the fault is still being re-raised (find the producer) or the clear path is broken |
| Stack overflow `task_fsm` | Heavy `printf` deep in transition logic | Bump `STK_FSM` from 4096 to 6144 in `main.cpp` |
| `[wdt] HUNG: fsm=...` | Your FSM blocked > 1.5 s | Find the blocking call. Either offload to another task or break it into smaller steps |
| Two events arrive simultaneously and one is lost | Queue overflow | `xQueueSend` is non-blocking with timeout 0 — bump `g_event_queue` size from 32 to 64 in `main.cpp` setup() |

---

## Step 14 — Completion checklist (sign before demo day)

Mechanical / firmware:
- [ ] All 7 source files in `firmware/src/fsm/` and `main.cpp` build clean with `-Wall` (no warnings).
- [ ] All 9 FSM states reached at least once during dev test.
- [ ] All 17 events triggered at least once (use the dev harness for the obscure ones).
- [ ] All 5 guards exercised in both true and false paths.
- [ ] Hardware watchdog never trips on a good rig (uptime > 30 min).
- [ ] One full IDLE→...→IDLE cycle in < 25 s, no fault flags set at end.

Documentation:
- [ ] `docs/demo_script.md` exists and was rehearsed twice.
- [ ] `docs/integration_log.md` has at least 4 session entries.
- [ ] FSM diagram (state-transition diagram) drawn — paste into your section of the report.

Team:
- [ ] M2 confirms motor commands ack within 100 ms.
- [ ] M3 confirms vision events propagate within 200 ms of detection.
- [ ] M4 confirms HMI cmds (RAISE/LOWER/HOLD/CLEAR) post events that the FSM consumes.
- [ ] M5 confirms e-stop hard test cuts motor within 50 ms.

If every box ticks, the demo will go well.
