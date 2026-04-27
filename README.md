# Automatic Vertical Lift Bridge Control System

**Group 5 | EEE 2412 Microprocessors II | JKUAT | April 2026**

| Member | Name | Adm. No. | Role |
|--------|------|----------|------|
| M1 | Mugaisi Eugene Muruli | ENE212-0083/2020 | Firmware Lead & FSM |
| M2 | Ochiere George John | ENE212-0218/2021 | Bridge Mechanism & Motor |
| M3 | Koech Cindy Chebet | ENE212-0069/2020 | Sensors & Detection |
| M4 | Kirui Abigael Cherotich | ENE212-0070/2020 | Traffic, HMI & Alarm |
| M5 | Kiogora Ian Mwenda | ENE212-0251/2021 | PCB, Power & Safety |

---

## What This Project Does

This system automates the complete operation of a scale-model vertical lift bridge. When a vessel approaches the waterway, the system autonomously:

1. Detects the vessel and determines it is **approaching** (not departing) using dual-beam ultrasonic sensors
2. Transitions road traffic lights from green → yellow → red
3. Lowers servo-actuated road barriers to block vehicle traffic
4. Verifies the bridge deck is clear of vehicles using infrared sensors
5. Raises the bridge deck vertically between four tower structures using a single motor driving four synchronised lead screws via a GT2 timing belt
6. Holds the deck in the raised position until the vessel passes through the downstream sensor station
7. Lowers the deck, raises barriers, and restores green traffic lights

The entire cycle runs without human intervention. An operator panel with an emergency stop button, mode switch (AUTO/MANUAL), and control buttons provides local override. A 2.8-inch TFT display shows real-time system status.

---

## How the Mechanism Works

A single **JGA25-370 geared DC motor** (12V, 60 RPM) is mounted at the base of Tower A-Left. Its shaft connects through a flexible coupling to a **T8 trapezoidal lead screw** (8mm lead, 4-start) running vertically through the tower. At the tower top, a **GT2 20-tooth pulley** engages a **GT2 6mm timing belt** that forms a closed rectangular loop connecting identical pulleys at the tops of all four towers.

```
        Tower A-L ──── GT2 Belt ──── Tower A-R
            │         (Bank A)          │
            │                           │
     GT2 Belt (crosses          GT2 Belt (crosses
     waterway at top)           waterway at top)
            │                           │
            │                           │
        Tower B-L ──── GT2 Belt ──── Tower B-R
                      (Bank B)
```

Because all four pulleys are locked to the same belt loop, all four lead screws turn at exactly the same rate. The deck rides on threaded brass nut blocks (one per screw) and is guided by aluminium angle rails at all four corners. The trapezoidal thread profile is self-locking — the deck holds its position when power is removed, with no counterweights or brakes needed.

**Key specifications:**
- Deck travel: 180mm at 8mm/s (≈22 seconds for full raise)
- Deck dimensions: 200mm × 120mm × 5mm acrylic
- Base platform: 900mm × 600mm × 12mm plywood
- Tower height: 300mm
- Waterway width: 150mm
- Motor torque safety factor: 15×

---

## How Vessel Detection Works

Each end of the waterway (upstream and downstream) has a **dual-beam sensor station**: two HC-SR04 ultrasonic sensors mounted 30mm apart on a common bracket, both aimed across the waterway channel.

When a vessel passes a station, it breaks Beam A and Beam B in sequence. The firmware timestamps each beam break and determines direction:

| Beam A triggers first | Beam B triggers first | Both within 50ms |
|---|---|---|
| Vessel **approaching** → raise bridge | Vessel **departing** → ignore | Noise / debris → discard |

Each station operates independently, so the system correctly handles simultaneous bidirectional traffic. The downstream station's detection of a vessel passing through generates the event that begins the bridge lowering sequence.

---

## Electronics

All electronics are integrated onto a single custom PCB (100mm × 80mm, 2-layer FR4, fabricated by JLCPCB):

| Component | Part Number | Function |
|-----------|-------------|----------|
| Microcontroller | ESP32-WROOM-32 | Dual-core 240MHz, FreeRTOS, 34 GPIO |
| Motor driver | DRV8871 | H-bridge with current sense (VPROPI) |
| Shift register | 74HC595 | 8 traffic LEDs from 3 GPIO pins |
| Buck converter | LM2596-5.0 | 12V → 5V / 3A for servos, sensors |
| LDO regulator | AMS1117-3.3 | 5V → 3.3V / 800mA for ESP32, logic |
| USB-UART bridge | CP2102N | Programming and serial debug via USB-C |
| Safety relay | SRD-05VDC | Hardware motor power gate via MOSFET |

Off-board peripherals connect via JST-XH headers: 4× HC-SR04 ultrasonic sensors, 2× FC-51 IR sensors, 8× KW12-3 limit switches, 1× potentiometer, 2× SG90 servos, 1× ILI9341 TFT display, 1× operator button panel, 1× E-stop button.

---

## Firmware Architecture

The ESP32 runs **7 concurrent FreeRTOS tasks** across dual cores:

| Task | Core | Priority | Period | Owner |
|------|------|----------|--------|-------|
| Safety Watchdog | 1 | 25 | 5ms | M5 |
| FSM Engine | 1 | 24 | 10ms | M1 |
| Motor Control | 1 | 23 | 10ms | M2 |
| Sensor Poll | 1 | 22 | 20ms | M3 |
| Traffic Control | 0 | 15 | 50ms | M4 |
| HMI Input | 0 | 12 | 50ms | M4 |
| HMI Display | 0 | 10 | 200ms | M4 |

Tasks communicate through:
- **Event queue** (`eventQueue`): sensors and HMI post events, FSM reads them
- **Motor command queue** (`motorCmdQueue`): FSM posts commands, motor task executes them
- **Shared status struct** (`SharedStatus_t`): mutex-protected, all tasks can read/write
- **Hardware interrupts**: limit switches and E-stop trigger ISRs for immediate motor cutoff

The system is governed by a **10-state finite state machine** with table-driven transitions:

```
IDLE → TRAFFIC_CLEARING → BARRIERS_LOWERING → BRIDGE_RAISING → BRIDGE_UP
  ↑                                                                   │
  └── RESTORING_TRAFFIC ← BARRIERS_RAISING ← BRIDGE_LOWERING ←───────┘

Any state → EMERGENCY_STOP (E-stop pressed)
Any state → FAULT (watchdog detects fault)
```

---

## Repository Structure

```
vertical-lift-bridge/
│
├── firmware/                          ← PlatformIO project
│   ├── platformio.ini                     [M1] Board config, libraries, build flags
│   ├── src/
│   │   ├── main.cpp                       [M1] Task creation, setup(), globals
│   │   ├── pin_config.h                   [M1] ALL GPIO assignments (do not edit without team approval)
│   │   ├── system_types.h                 [M1] Shared enums (states, events) and structs
│   │   ├── fsm/                           [M1] Finite state machine
│   │   │   ├── fsm_engine.h/.cpp              Transition table, task loop
│   │   │   ├── fsm_guards.h/.cpp              Guard condition functions
│   │   │   └── fsm_actions.h/.cpp             Transition action functions
│   │   ├── motor/                         [M2] Motor control
│   │   │   └── motor_driver.h/.cpp            PWM, ADC height, current read
│   │   ├── sensors/                       [M3] Sensor drivers
│   │   │   ├── ultrasonic.h/.cpp              Dual-beam direction detection
│   │   │   └── ir_deck.h/.cpp                 FC-51 deck clearance check
│   │   ├── traffic/                       [M4] Traffic management
│   │   │   ├── traffic_lights.h/.cpp          74HC595 LED patterns, servo barriers
│   │   │   └── buzzer.h/.cpp                  Alarm tone sequences
│   │   ├── hmi/                           [M4] Human Machine Interface
│   │   │   ├── display.h/.cpp                 TFT rendering (state, height bar, faults)
│   │   │   └── input.h/.cpp                   Button debounce, mode switch, events
│   │   └── safety/                        [M5] Safety systems
│   │       ├── watchdog.h/.cpp                E-stop, overcurrent, anti-jam, limits
│   │       ├── fault_register.h/.cpp          32-bit fault register with get/set/clear
│   │       └── interlocks.h/.cpp              Pre-condition checks for each state
│   ├── lib/                               PlatformIO managed (leave empty)
│   ├── include/                           PlatformIO managed (leave empty)
│   └── test/                              Unit test sketches
│       ├── test_fault_register.cpp        [M5]
│       ├── test_motor_driver.cpp          [M2]
│       ├── test_ultrasonic.cpp            [M3]
│       └── test_shift_register.cpp        [M4]
│
├── pcb/                               ← KiCad 8 project [M5 owns entirely]
│   ├── kicad-project/
│   │   ├── vertical-lift-pcb.kicad_pro
│   │   ├── vertical-lift-pcb.kicad_sch    Root schematic (hierarchical)
│   │   ├── vertical-lift-pcb.kicad_pcb    PCB layout
│   │   ├── sub-sheets/                    6 hierarchical sub-sheets:
│   │   │   ├── Power_Supply.kicad_sch         12V input, LM2596, AMS1117
│   │   │   ├── ESP32_Module.kicad_sch         ESP32 + strapping resistors
│   │   │   ├── USB_UART_Bridge.kicad_sch      CP2102N + USB-C + auto-reset
│   │   │   ├── Motor_Driver.kicad_sch         DRV8871 + current sense
│   │   │   ├── Shift_Register_LEDs.kicad_sch  74HC595 + 8 LEDs + resistors
│   │   │   └── Connectors_Safety.kicad_sch    JST-XH headers, relay, E-stop
│   │   └── symbols/                       Custom KiCad symbols if needed
│   ├── gerbers/                           Generated Gerber + drill files
│   │   └── JLCPCB_order.zip              Ready-to-upload archive
│   └── BOM.xlsx                           Full bill of materials with costs
│
├── mechanical/                        ← CAD and build drawings [M2 owns]
│   ├── bridgeModel.FCStd                  FreeCAD assembly model
│   ├── dimensions/                        Dimensioned drawings (PNG exports)
│   │   ├── base_platform.png
│   │   ├── tower_panels.png
│   │   ├── deck_drawing.png
│   │   └── belt_path.png
│   └── photos/                            Build progress photos
│
└── docs/                              ← All documentation [ALL contribute]
    ├── proposal.docx                      Submitted project proposal
    ├── build_guide.docx                   Complete step-by-step build guide
    ├── member5_pcb_guide.docx             Isolated PCB & safety guide
    ├── final_report.docx                  Technical report (Week 8)
    ├── demo_script.md                     12-minute demonstration script
    ├── presentation.pptx                  Slides for demo day
    └── figures/                           Diagram PNGs used in documents
```

---

## Member Work Areas

### Member 1 — George (Firmware Lead & FSM)

**Your directories:** `firmware/src/main.cpp`, `firmware/src/pin_config.h`, `firmware/src/system_types.h`, `firmware/src/fsm/`

**What you do:**
- Create `pin_config.h` on Day 1 — this is the single source of truth for all GPIO assignments. No other member may change pin assignments without your approval.
- Create `system_types.h` on Day 1 — all shared enums (`SystemState_t`, `SystemEvent_t`) and structs (`SharedStatus_t`, `MotorCommand_t`) that every module includes.
- Create `platformio.ini` with board configuration, library dependencies, and TFT build flags.
- Write `main.cpp` with FreeRTOS task creation, queue/mutex initialisation, and the boot sequence. You uncomment each task as the owning member confirms their code compiles.
- Implement the FSM engine in `fsm/`: the transition table, guard functions (e.g., `guard_auto_mode()` checks `sharedStatus.autoMode && !sharedStatus.estopActive`), and action functions (e.g., `action_raise_bridge()` posts a RAISE command to `motorCmdQueue`).

**Do not touch:** `motor/`, `sensors/`, `traffic/`, `hmi/`, `safety/`, `pcb/`, `mechanical/`

---

### Member 2 — Eugene (Bridge Mechanism & Motor)

**Your directories:** `firmware/src/motor/`, `mechanical/`, `firmware/test/test_motor_driver.cpp`

**What you do:**
- Build the entire physical bridge: base platform, four towers, guide rails, lead screws, bearings, GT2 pulleys, timing belt, deck, motor mounting, potentiometer, and limit switch positioning.
- Document all dimensions with drawings in `mechanical/dimensions/` and take progress photos in `mechanical/photos/`.
- Maintain the FreeCAD model (`bridgeModel.FCStd`) as the bridge evolves.
- Write `motor_driver.cpp`: `motor_init()` sets up LEDC PWM channels, `motor_set(speed)` drives the DRV8871, `motor_read_height()` reads the potentiometer via ADC and converts to millimetres, `motor_read_current()` reads VPROPI and converts to amps.
- Calibrate the potentiometer by recording ADC values at deck bottom and top, and update the `ADC_BOTTOM` and `ADC_TOP` constants.

**Do not touch:** `fsm/`, `sensors/`, `traffic/`, `hmi/`, `safety/`, `pcb/`

---

### Member 3 — Cindy (Sensors & Detection)

**Your directories:** `firmware/src/sensors/`, `firmware/test/test_ultrasonic.cpp`

**What you do:**
- Test all 4 HC-SR04 sensors individually on a breadboard. Verify each reads distance accurately (±1cm at 5–20cm range). Label each sensor physically.
- Test both FC-51 IR sensors. Adjust their onboard potentiometers for 5–8cm detection range.
- Write `ultrasonic.cpp`: implement the dual-beam direction detection algorithm. For each station, track which beam breaks first, calculate the time delta, and return +1 (approaching), -1 (departing), or 0 (noise/no detection).
- Write `ir_deck.cpp`: debounce the FC-51 output, implement the 2-second sustained-clear check before reporting the deck as safe.
- Write `task_sensor_poll()`: runs every 20ms, sequences ultrasonic measurements (Beam A then B at each station), reads IR sensors, and posts events (`EVT_VESSEL_DETECTED`, `EVT_VESSEL_PASSED`, `EVT_DECK_CLEAR_CONFIRMED`) to the event queue.

**Do not touch:** `fsm/`, `motor/`, `traffic/`, `hmi/`, `safety/`, `pcb/`, `mechanical/`

---

### Member 4 — Abigael (Traffic, HMI & Alarm)

**Your directories:** `firmware/src/traffic/`, `firmware/src/hmi/`, `firmware/test/test_shift_register.cpp`

**What you do:**
- Test the 74HC595 shift register on a breadboard with 8 LEDs. Verify the `shiftByte()` function lights the correct LED for each bit position.
- Test both SG90 servos. Verify they sweep smoothly between 0° (barrier up) and 90° (barrier down).
- Write `traffic_lights.cpp`: define LED bit patterns for each system state (`PATTERN_IDLE`, `PATTERN_CLEARING`, `PATTERN_ACTIVE`, etc.), implement the `shiftByte()` function that outputs to the 74HC595, and implement `task_traffic_control()` that reads `sharedStatus.state`, outputs the corresponding LED pattern, sweeps servos in 3° increments, and drives buzzer alarm patterns.
- Write `display.cpp`: initialise the ILI9341 TFT via TFT_eSPI, render the main screen showing current state name, deck height bar gauge (0–180mm), motor current, sensor status icons, active fault codes, and operating mode.
- Write `input.cpp`: debounce the Mode, Raise, Lower, and Reset pushbuttons (20ms debounce window), read the mode toggle switch, and post manual command events to the event queue.

**Do not touch:** `fsm/`, `motor/`, `sensors/`, `safety/`, `pcb/`, `mechanical/`

---

### Member 5 — Ian (PCB, Power & Safety)

**Your directories:** `pcb/` (entirely), `firmware/src/safety/`, `firmware/test/test_fault_register.cpp`

**What you do:**
- Design the complete PCB schematic in KiCad 8 with 6 hierarchical sub-sheets: Power Supply, ESP32 Module, USB-UART Bridge, Motor Driver, Shift Register & LEDs, Connectors & Safety.
- Route the PCB layout: 100mm × 80mm, bottom ground plane, trace widths per spec (0.5mm signal, 1.0mm 5V, 1.5mm 12V), thermal vias under DRV8871, 7mm antenna keepout.
- Generate Gerbers and order from JLCPCB before end of Week 1.
- Lead the PCB soldering session in Week 3. Follow the strict soldering sequence: passives → AMS1117 → LM2596 → CP2102N → DRV8871 → 74HC595 → ESP32 → connectors → relay. Test each power rail before proceeding.
- Manage the BOM (`pcb/BOM.xlsx`) with component costs and procurement status.
- Write `watchdog.cpp`: the highest-priority task (5ms period, Core 1, Priority 25) that checks E-stop state, motor overcurrent (VPROPI > 2.5V for 200ms), anti-jam (current flowing but position not changing for 500ms), limit switch violations, and feeds the FreeRTOS Task Watchdog Timer.
- Write `fault_register.cpp`: 32-bit register where each bit maps to a fault (bit 0 = E-stop, bit 1 = overcurrent, bit 2 = jam, etc.), with functions for set, clear, auto-clear (transient faults), and operator reset.
- Write `interlocks.cpp`: functions like `check_safe_to_raise()` that verify deck clear, barriers down, no faults, no E-stop before allowing the FSM to proceed.
- Wire the E-stop relay circuit during integration week.

**Do not touch:** `fsm/`, `motor/`, `sensors/`, `traffic/`, `hmi/`, `mechanical/`

---

## Shared Files (Handle With Care)

These files are edited **only by Member 1** after team discussion:

| File | Purpose | Rule |
|------|---------|------|
| `pin_config.h` | All GPIO assignments | No member may change a pin without M1 approval |
| `system_types.h` | Shared enums and structs | Adding a new event or state requires M1 to update |
| `main.cpp` | Task creation and boot sequence | M1 uncomments a task only after the owner confirms it compiles |
| `platformio.ini` | Build configuration | M1 adds libraries as members request them |

If you need a change to a shared file, message the group chat and let M1 make the edit. This prevents merge conflicts.

---

## How to Build and Upload

1. Open VS Code. Click **File → Open Folder** and select the `firmware/` directory.
2. PlatformIO will auto-detect `platformio.ini` and configure the project.
3. Connect the ESP32 board via USB. Note the COM port in Device Manager.
4. Click the **checkmark** (✓) icon in the bottom PlatformIO toolbar to **build**.
5. Click the **right arrow** (→) icon to **upload** to the ESP32.
6. Click the **plug** icon to open the **Serial Monitor** at 115200 baud.

---

## Git Workflow

Each member works on their own files. Since ownership is clearly separated by directory, merge conflicts should be rare. Follow these rules:

1. **Pull before you start working:** `git pull origin main`
2. **Commit often** with descriptive messages: `git commit -m "M2: Add motor_set() with speed ramp"`
3. **Push when you finish a session:** `git push origin main`
4. **Never force push:** `git push --force` is banned. If push fails, pull first, resolve conflicts, then push.
5. **Prefix commit messages** with your member number: `M1:`, `M2:`, `M3:`, `M4:`, `M5:`

---

## Timeline

| Week | Milestone |
|------|-----------|
| 1 | M1: `pin_config.h`, `system_types.h`, task skeleton. M5: PCB schematic → layout → Gerbers → JLCPCB order. All: breadboard sensor tests. |
| 2 | M1: Module headers published. M2: Towers, rails, screws, belt, deck, motor built. M3/M4: Sensor and actuator tests complete. |
| 3 | M5: PCB arrives, leads soldering session. M1–M4: Firmware modules developed and unit-tested. |
| 4 | All firmware modules complete. Individual unit tests passing. |
| 5 | M2: All mechanical assembly complete, wired to PCB. Integration begins. |
| 6 | Full system integration. FSM walk-through. Safety testing. 20 consecutive auto cycles. |
| 7 | Parameter tuning. Robustness hardening. |
| 8 | Report, slides, demo. Live demonstration to lecturer. |

---

## Budget

Total estimated cost: **KES 17,318** (~KES 3,464 per member).

See `pcb/BOM.xlsx` for the complete bill of materials with per-item costs sourced from Nerokas, Pixel Electronics, ASK Electronics, K-Technics, and Sagana/Luthuli Avenue.

---

## Licence

This project is academic coursework for EEE 2412 Microprocessors II at JKUAT. Not licensed for commercial use.
