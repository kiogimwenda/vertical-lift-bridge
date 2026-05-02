# Member 1 — George — System Architecture & Finite-State Machine

> Role: System architect, FSM author, integrator. You are the **integration owner** — when modules from M2/M3/M4/M5 don't talk to each other properly, the buck stops with you.

---

## Scope of your work

| File | Lines (approx) | What you own |
|------|---:|--------------|
| `firmware/platformio.ini` | 50 | Build config — already created, you maintain |
| `firmware/src/pin_config.h` | 280 | Already created — you sign off on changes |
| `firmware/src/system_types.h` | 270 | Already created — single source of truth |
| `firmware/src/main.cpp` | 200 | Boot, FreeRTOS task creation |
| `firmware/src/fsm/fsm_engine.{h,cpp}` | 200 | Transition table |
| `firmware/src/fsm/fsm_guards.{h,cpp}` | 100 | Boolean preconditions |
| `firmware/src/fsm/fsm_actions.{h,cpp}` | 120 | Side-effects on entry/exit |

**Not your work:** motor (M2), sensors/vision (M3), HMI/lights (M4), PCB/safety (M5). You wire their public APIs together — you don't implement their internals.

---

## Step 1 — Set up your development machine (Windows 11)

You're starting from a fresh Windows install. Do all of these in order.

### 1.1 Install VS Code
1. Open Edge, navigate to `https://code.visualstudio.com`.
2. Click "Download for Windows" — the `.exe` lands in `C:\Users\<you>\Downloads\`.
3. Double-click `VSCodeUserSetup-x64-*.exe`.
4. Accept license. Tick **all four** "Other" boxes:
   - ☑ Add "Open with Code" to file context menu
   - ☑ Add "Open with Code" to directory context menu
   - ☑ Register Code as an editor for supported file types
   - ☑ Add to PATH
5. Click Install. When done, click "Launch Visual Studio Code".

### 1.2 Install Git
1. Navigate to `https://git-scm.com/download/win`.
2. The 64-bit Windows installer downloads automatically.
3. Run it. Take all defaults except:
   - On "Choosing the default editor" pick **"Use Visual Studio Code as Git's default editor"**.
   - On "Adjusting the name of the initial branch" pick **"Override the default… `main`"**.
4. Open Command Prompt (`Win+R`, type `cmd`, Enter). Run:
   ```cmd
   git --version
   ```
   Expected: `git version 2.43.0.windows.1` (or newer).

### 1.3 Configure Git identity
In the same Command Prompt:
```cmd
git config --global user.name "George Mwangi"
git config --global user.email "george@students.jkuat.ac.ke"
```

### 1.4 Install Python 3.11
1. Navigate to `https://www.python.org/downloads/windows/`.
2. Download "Windows installer (64-bit)" for **3.11.x** (NOT 3.12 — PlatformIO compatibility).
3. Run the installer. **CRITICAL:** tick ☑ "Add python.exe to PATH" before clicking Install Now.
4. Verify:
   ```cmd
   python --version
   pip --version
   ```

### 1.5 Install PlatformIO IDE inside VS Code
1. Open VS Code.
2. Press `Ctrl+Shift+X` (Extensions panel).
3. In the search box type: `platformio ide`.
4. Click the first result (publisher = PlatformIO). Click **Install**.
5. Wait for the orange status-bar progress to finish. VS Code restarts itself.
6. Once restarted you'll see the alien-head 👽 icon on the left sidebar — that's the PIO Home button. Click it. Wait for "Welcome to PlatformIO IDE!" page to load.

### 1.6 Install ESP32 USB driver
1. Plug your ESP32 DevKit board into a USB port.
2. Open Device Manager (`Win+X` → Device Manager).
3. Look under **Ports (COM & LPT)**. If you see "USB-SERIAL CH340 (COM8)" or similar — done.
4. If instead you see a yellow ⚠ "Unknown device":
   - Download CH340 driver: `https://www.wch-ic.com/downloads/CH341SER_EXE.html`
   - Run `CH341SER.EXE` → Install.
   - Unplug + replug the board. Verify Device Manager shows `USB-SERIAL CH340 (COMxx)`.
5. **Write down the COM port number** — you'll set it in `platformio.ini`.

### 1.7 Clone the repo
1. Open Command Prompt. `cd` to where you want the project:
   ```cmd
   cd C:\Users\%USERNAME%\Documents
   git clone https://github.com/kiogimwenda/vertical-lift-bridge.git
   cd vertical-lift-bridge
   ```
2. In VS Code: File → Open Folder → select `vertical-lift-bridge`.
3. PlatformIO will detect the project and start indexing (status bar at bottom shows "Initializing tasks…"). Wait until it finishes (~2 min on first open).

---

## Step 2 — Understand the existing scaffolding

Before writing a single line, **read these files in this order** so you understand the architecture:

1. `firmware/src/pin_config.h` — every GPIO assignment with rationale.
2. `firmware/src/system_types.h` — every enum and struct used across modules.
3. `firmware/src/main.cpp` — task creation order and core affinity.
4. `firmware/src/fsm/fsm_engine.cpp` — transition logic.
5. `firmware/src/fsm/fsm_guards.cpp` — what each guard checks.
6. `firmware/src/fsm/fsm_actions.cpp` — what fires on entry/exit.

**Time-box: 3 hours** for this read-through with notes. If something is unclear, write the question down — you'll use those questions to write your acceptance tests.

---

## Step 3 — Compile-test the skeleton

You haven't written code yet, but the whole thing must build cleanly first.

1. In VS Code, click the PIO 👽 icon → "Project Tasks" → **vlb_main** → "General" → **Build**.
2. First build downloads toolchain (~5 min). Subsequent builds < 30 s.
3. Expected result: green "SUCCESS" line with bytes-used summary.
4. If you see *any* compile error:
   - Read the very first error in the log (later errors are usually cascade).
   - The error has the form `path/file.cpp:42:10: error: 'foo' was not declared`.
   - Open that file, jump to that line (`Ctrl+G` then type the number).
   - Fix the cause, rebuild. Don't guess — search the symbol with `Ctrl+Shift+F`.
5. Once green: **commit immediately** with message `chore: skeleton compiles clean`.

---

## Step 4 — First flash, observe boot

You don't need motors or sensors connected for this — just the bare ESP32 + USB cable.

1. PlatformIO panel → **Upload** (right-arrow icon at the bottom).
2. While "Connecting…" appears in serial output, hold the **BOOT** button on the ESP32 for 1 second, then release.
3. After "Hard resetting…" you'll see the boot banner.
4. Click **Monitor** (plug icon). Expected log:
   ```
   === VLB Group 7 — Boot ===
   ESP32 SDK: v4.4.x | Sketch: ...
   [wdt] init OK (5s hw, 1.5s sw)
   [fault] init OK
   [ilk] init OK (relay off until first OK eval)
   [motor] init OK
   [us] init OK
   [vision] UART2 init @ 115200
   [lights] init OK
   [buz] init OK
   [boot] Peripherals OK
   [boot] Tasks created — entering scheduler
   [hmi] task start (Core 1)
   [fsm] 8 -> 0       <-- INIT to IDLE
   ```
5. If no scrolling output: check baud rate is **115200** in PlatformIO monitor settings.

---

## Step 5 — Weekly milestones

| Week | Goal | Acceptance test |
|---:|------|-----------------|
| 1 | Skeleton compiles + flashes; boot log to IDLE | All `[xxx] init OK` lines present |
| 2 | FSM cycles IDLE → CLEARING → RAISING when EVT_VEHICLE_DETECTED is injected from serial | `fsm_engine_handle()` test harness logs every transition |
| 3 | Hand-off: M2 motor module integrates via `g_motor_cmd_queue` | Press serial `r` triggers EVT_OPERATOR_RAISE, motor turns |
| 4 | Hand-off: M3 sensors integrate; ultrasonic blocking auto-triggers cycle | Hand crosses sensors → bridge raises within 200 ms |
| 5 | Hand-off: M4 HMI integrate; M5 safety integrate; full bench-top loop | One complete IDLE → RAISED → IDLE cycle in < 25 s |
| 6 | Fault injection: stall, overcurrent, e-stop tests pass | Every fault flag reachable + recoverable from HMI |
| 7 | On-rig test: full bridge + counterweights + cable | 10 cycles back-to-back without latching fault |
| 8 | Demo prep: scripted scenario rehearsal | Lecturer demo runs in 5 min, no panic resets |

---

## Step 6 — Test harness (Week 2)

Add a serial command interface for injecting events (you'll delete this for the demo, keep it for dev). Add this to the **end of `main.cpp`** inside `loop()`:

```cpp
// --- DEV-ONLY: serial injection (remove for production) ---
static char buf[8]; static size_t blen = 0;
while (Serial.available() && blen < sizeof(buf)-1) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
        buf[blen] = 0; blen = 0;
        SystemEvent_t e = EVT_NONE;
        if      (!strcmp(buf, "v"))  e = EVT_VEHICLE_DETECTED;
        else if (!strcmp(buf, "c"))  e = EVT_VEHICLE_CLEARED;
        else if (!strcmp(buf, "t"))  e = EVT_TOP_LIMIT_HIT;
        else if (!strcmp(buf, "b"))  e = EVT_BOTTOM_LIMIT_HIT;
        else if (!strcmp(buf, "r"))  e = EVT_OPERATOR_RAISE;
        else if (!strcmp(buf, "l"))  e = EVT_OPERATOR_LOWER;
        else if (!strcmp(buf, "f"))  e = EVT_FAULT_RAISED;
        else if (!strcmp(buf, "x"))  e = EVT_FAULT_CLEARED;
        if (e != EVT_NONE) xQueueSend(g_event_queue, &e, 0);
    } else { buf[blen++] = c; }
}
```

Now in monitor, type `v` + Enter → bridge raises. Type `t` + Enter → reaches top.

---

## Step 7 — Failure recovery cookbook

| Symptom | Cause | Fix |
|---|---|---|
| `Connecting......_____` then timeout on upload | Auto-reset failed | Hold BOOT, press EN+release, release BOOT |
| Boot loops with `Brownout detector triggered` | USB hub power | Use a powered hub, or move to direct PC port |
| `[fault] raised: 0x00000020 (vision-lost)` at boot | ESP32-CAM not connected | Expected on dev rig — ignore until M3 connects CAM |
| Stack overflow in `task_xxx` | Heavy printf in deep call | Increase that task's `STK_*` constant in main.cpp by 2048 |
| FSM stuck in CLEARING | Barrier servos absent | Set `g_status.barrier_*_target_reached = true` in fsm_guards for now |

---

## Completion checklist (sign before integration)

- [ ] All 7 of your files compile clean with `-Wall -Wextra` (no warnings).
- [ ] All 9 FSM states reached at least once during dev test.
- [ ] All 12 events listed in `system_types.h` triggered at least once.
- [ ] All 4 guards exercised (ok + fail path).
- [ ] Hardware watchdog never trips on a good rig (uptime > 10 min).
- [ ] One full cycle in < 25 s, no fault flags set at end.
- [ ] Demo script (text file) committed to `docs/demo_script.md`.
