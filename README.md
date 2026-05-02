# Automatic Vertical Lift Bridge Control System

**Group 7  ·  EEE 2412 Microprocessors II  ·  JKUAT  ·  May 2026**

A scale-model vertical-lift bridge that autonomously raises its deck for marine traffic and lowers it for road traffic, controlled by a dual-ESP32 architecture (main controller + ESP32-CAM vision companion), with a TFT operator dashboard, a hardware safety chain, and a fully parametric mechanical and electronic design.

> **New to this repo?** Jump straight to [§9 Getting Started — Your First Day](#9-getting-started--your-first-day) and follow it top-to-bottom. Everything else is reference material.

---

## Table of Contents

1. [Group Members and Roles](#1-group-members-and-roles)
2. [What This Project Does](#2-what-this-project-does)
3. [System Architecture](#3-system-architecture)
4. [Hardware Overview](#4-hardware-overview)
5. [Firmware Overview](#5-firmware-overview)
6. [Repository Structure](#6-repository-structure)
7. [Documentation Map](#7-documentation-map)
8. [Tools and Software You Need](#8-tools-and-software-you-need)
9. [Getting Started — Your First Day](#9-getting-started--your-first-day)
10. [Setting Up Your Machine](#10-setting-up-your-machine)
11. [GitHub Authentication](#11-github-authentication)
12. [Cloning the Repository](#12-cloning-the-repository)
13. [Day-to-Day Git Workflow](#13-day-to-day-git-workflow)
14. [Working with the Firmware](#14-working-with-the-firmware)
15. [Working with the CAD](#15-working-with-the-cad)
16. [Working with the PCB](#16-working-with-the-pcb)
17. [Per-Member Quick-Reference](#17-per-member-quick-reference)
18. [Communication and Escalation](#18-communication-and-escalation)
19. [Demo Day Checklist](#19-demo-day-checklist)
20. [Troubleshooting](#20-troubleshooting)
21. [Glossary](#21-glossary)
22. [License](#22-license)

---

## 1. Group Members and Roles

| # | Name | Adm. No. | Role | Primary directories |
|---|------|----------|------|---------------------|
| M1 | Ochiere George John | ENE212-0218/2021 | System Architect & FSM | `firmware/src/fsm/`, `firmware/src/main.cpp`, `firmware/src/system_types.h`, `firmware/src/pin_config.h` |
| M2 | Mugaisi Eugene | ENE212-0083/2020 | Mechanism & Motor | `cad/`, `firmware/src/motor/`, `mechanical/` |
| M3 | Koech Cindy Chebet | ENE212-0069/2020 | Vision & Sensors | `firmware/src/sensors/`, `firmware/src/vision/`, `firmware_cam/` |
| M4 | Kirui Abigael Cherotich | ENE212-0070/2020 | Traffic Lights, HMI, Alarm | `firmware/src/traffic/`, `firmware/src/hmi/`, `firmware/assets/` |
| M5 | Kiogora Ian Mwenda | ENE212-0251/2021 | PCB, Power, Safety | `pcb/`, `firmware/src/safety/`, `bom/` |

Every member has a dedicated walkthrough in [`member_guides/`](./member_guides/) — open yours **before** writing any code or running any tool.

**File-ownership convention.** When a file lives inside a member's primary directory, that member is the **owner** — they make the calls on design and implementation. If you need to edit a file in someone else's directory, coordinate with them in the group chat first. The two cross-cutting files are `firmware/src/system_types.h` and `firmware/src/pin_config.h`: M1 has authority over both, but notify the group before changing either because changes ripple across the codebase.

---

## 2. What This Project Does

The system runs a complete bridge-opening cycle without human input. In plain English:

1. The system sits in **idle state** with the bridge deck down, traffic lights green for road traffic, red for marine traffic.
2. **Vehicle detection.** When a model vehicle approaches, the ESP32-CAM running frame-difference motion detection raises a vehicle flag over a UART link, and dual HC-SR04 ultrasonic sensors confirm direction (approaching vs. leaving) using beam-arrival timing on a five-sample history ring.
3. **Stop the road.** Traffic lights cycle green → amber → red. Servo barriers swing down. A buzzer chirps twice as a courtesy alert.
4. **Balance counterweights.** A simulated dynamic counterweight system models two water tanks being filled/drained by pumps and valves to balance the deck mass. The firmware simulates water levels, pump status, and drain valve state — all visualised in real time on the TFT dashboard. The physical counterweights remain static (lead-filled boxes); the simulation runs in parallel for demonstration purposes. The FSM waits for the simulated counterweights to report "balanced" before proceeding.
5. **Verify clearance.** The system confirms both barriers reached their down position, no vehicle remains in the bridge zone, and the simulated counterweights are balanced before raising.
6. **Raise the bridge.** A JGA25-370 12 V gearmotor drives a Ø30 mm aluminium drum, winding cables that lift the deck. Two 120 g static counterweights run on opposite cables over top pulleys to balance the deck mass. A Hall-effect encoder on the motor shaft tracks deck height in millimetres.
7. **Hold for marine traffic.** When the top limit switch trips, the motor brakes electronically (BTS7960 dynamic short brake). Marine traffic lights turn green. The bridge holds for 8 seconds (`HOLD_TIMEOUT_MS`, configurable in `system_types.h`).
8. **Lower the bridge.** The motor drives down at lower duty (gravity-assisted) until the bottom limit switch trips. Counts/mm calibration auto-zeroes the encoder at the bottom.
9. **Reopen the road.** Barriers raise, traffic lights cycle amber → green for road, red for marine. The system returns to idle.

A 2.8-inch TFT touch dashboard mirrors the system state in real time and provides operator controls (manual raise/lower, fault clear, brightness, screen navigation). A mushroom emergency-stop button cuts motor power through a hardware relay independent of the firmware. A 16-flag fault register catches stalls, overcurrent, undervoltage, sensor link loss, watchdog timeouts, and barrier reach failures.

This README is non-prescriptive about what every screen of the dashboard looks like — that is M4's creative-liberty zone. The firmware ships every screen as a working LVGL stub with infrastructure complete (TFT_eSPI driver, double-buffered DMA flush, XPT2046 touch, async-call thread-safety, screen registry); M4 designs the visuals.

---

## 3. System Architecture

The control logic runs on **two ESP32 microcontrollers** that communicate over a one-way UART link. The main board owns the FSM, motor control, sensors, lights, HMI, and safety chain. The companion ESP32-CAM owns vision only.

```
                  ┌──────────────────────────────────┐
                  │       ESP32-CAM (companion)      │
                  │   OV2640 grayscale QQVGA @10Hz   │
                  │   Frame-difference motion algo   │
                  └───────────────┬──────────────────┘
                                  │ UART2 @ 115200 (JSON lines)
                                  ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ESP32-WROOM-32E (main)                       │
│  ┌─────────────┐    ┌──────────────┐    ┌──────────────────┐    │
│  │  FSM Task   │◄───│ Event Queue  │◄───│ Sensors / Vision │    │
│  │ (9 states)  │    │ (32 events)  │    │ Watchdog / IRQ   │    │
│  └──────┬──────┘    └──────────────┘    └──────────────────┘    │
│         │                                                       │
│  ┌──────▼─────────┐  ┌───────────────┐  ┌───────────────┐       │
│  │  Motor Task    │  │ Safety Task   │  │   HMI Task    │       │
│  │  BTS7960 PWM   │  │ E-stop, fault │  │   LVGL/TFT    │       │
│  │  Hall encoder  │  │ Watchdog kick │  │   Touch input │       │
│  └────────────────┘  └───────────────┘  └───────────────┘       │
│       Core 0              Core 0              Core 1            │
└─────────────────────────────────────────────────────────────────┘
       │                      │                     │
       ▼                      ▼                     ▼
   ┌───────┐         ┌──────────────┐         ┌──────────┐
   │ Motor │         │ Relay (NC    │         │ TFT 2.8" │
   │ + Enc │         │ E-stop chain)│         │ ILI9341  │
   └───────┘         └──────────────┘         └──────────┘
```

**Key architectural choices** (rationale in [`docs/audit_report.md`](./docs/audit_report.md)):
- **Two towers, not four.** A vertical-lift mechanism with cable + counterweight is mechanically simpler than the original four-tower lead-screw + GT2 belt design — one fewer alignment surface to mis-align.
- **Cable + counterweight, not lead-screw.** Counterweights reduce the motor's nominal lift current from ~1500 mA to ~600 mA, allowing a smaller motor and BTS7960 well below its 5.5 A drop-out.
- **ESP32-CAM, not IR retro-reflective.** Vision over UART JSON is observable and debuggable; IR analog levels are not, especially under fluorescent lab lighting.
- **Hardware limit switches, not software estimate alone.** Microswitches at top and bottom of one tower override any firmware estimate, fail-safe to STOP.
- **Hardware E-stop relay, not just GPIO interrupt.** The relay sits in series with motor V+ on the BTS7960, so a wire cut, firmware crash, or stuck task cannot prevent stopping.
- **HMI on Core 1, everything else on Core 0.** LVGL is non-reentrant and benefits from a dedicated core. All cross-core HMI calls go through `lv_async_call()`.

---

## 4. Hardware Overview

| Subsystem | Components | Notes |
|-----------|------------|-------|
| Frame | 2× printed towers, MGN12 200 mm rails, MDF base 1200 × 600 × 12 mm | Towers PLA, 40% gyroid infill |
| Drive | JGA25-370 12 V gearmotor, Ø30 mm aluminium drum, 1 mm braided steel cable | ~600 mA nominal at full lift |
| Counterweights | 2× printed boxes, 4× 30 g lead, 608ZZ pulley bearings, M8×80 axles | 120 g per side (static). Firmware simulates dynamic water-tank counterweights with pump/drain — displayed on TFT |
| Sensors | 2× HC-SR04 ultrasonics, ESP32-CAM OV2640, 4× KW11-3Z limit switches | Ultrasonic ECHO line via 1 kΩ/2 kΩ divider — 5 V down to 3.3 V |
| Actuators | 2× SG90 servos (barriers), 6× SMD LEDs (traffic), passive piezo buzzer | LEDs driven by 74HC595 chain |
| Display | ILI9341 2.8" 240×320 TFT + XPT2046 touch | HSPI bus on the main ESP32 |
| Safety | 16 mm mushroom NC E-stop, SRD-05VDC-SL-C relay, 2N7000 coil driver | Relay coil de-energised by default |
| Power | 12 V/3 A barrel input, LM2596 12→5 V buck, AMS1117 5→3.3 V LDO | TVS, polyfuse, Schottky reverse-protection |
| Compute | ESP32-WROOM-32E DevKit (main), ESP32-CAM AI-Thinker (vision) | CAM 5 V from a separate buck — never share with main |

Procurement targets Kenyan suppliers — full priced list in [`bom/VLB_Group7_BOM.xlsx`](./bom/VLB_Group7_BOM.xlsx). Grand total **KES 24,825**, broken into Mechanical (12,510), Electronics + TFT + CAM (5,995), PCB Fab (4,110), and Consumables (1,850). All formulas are live — change a unit price or quantity and the spreadsheet recalculates.

---

## 5. Firmware Overview

The main-board firmware is a PlatformIO project at [`firmware/`](./firmware/) targeting the ESP32-WROOM-32E DevKit. The companion firmware at [`firmware_cam/`](./firmware_cam/) targets the AI-Thinker ESP32-CAM. Both build with `pio run` and flash with `pio run -t upload`.

The main firmware is structured as a collection of FreeRTOS tasks pinned to specific cores:

| Task | Core | Priority | Stack | Purpose |
|------|-----:|---------:|------:|---------|
| `task_safety` | 0 | 5 | 3 KB | Watchdog poll, fault evaluate, interlocks |
| `task_fsm` | 0 | 4 | 4 KB | Pull events from queue, run FSM, dispatch motor + light + barrier commands |
| `task_motor` | 0 | 4 | 3 KB | Apply motor commands, read ADC current, update position |
| `task_sensors` | 0 | 3 | 3 KB | Tick HC-SR04 ultrasonics at 20 Hz, infer direction |
| `task_vision` | 0 | 3 | 4 KB | UART2 RX, parse ESP32-CAM JSON, heartbeat timeout |
| `task_counterweight` | 0 | 2 | 2 KB | Simulated dynamic counterweight — pump/drain/level at 20 Hz |
| `task_telemetry` | 0 | 1 | 2 KB | Uptime, CPU load, voltage rails — 1 Hz |
| `task_hmi` | 1 | 2 | 8 KB | LVGL tick, screen refresh, touch + button input handler |

State is shared through a single mutex-guarded `SharedStatus_t` struct (defined in `firmware/src/system_types.h`) — no task touches another's globals directly. Inter-task communication uses three FreeRTOS queues: `g_event_queue` (FSM events), `g_motor_cmd_queue` (motor direction + duty), and `g_hmi_cmd_queue` (operator inputs).

Safety is layered:
- **Hardware Task Watchdog Timer (TWDT)** — 5 s panic timeout; if `loop()` ever hangs, the chip resets.
- **Per-task software watchdog** — each major task kicks a timestamp every cycle. `task_safety` polls all timestamps at 20 Hz; if any exceeds 1.5 s, it forces the relay open and raises `FAULT_WATCHDOG`.
- **Fault register** — 16-bit bitmask in `g_status.fault_flags`, set by any module on detection. Edge-triggered `EVT_FAULT_RAISED` and `EVT_FAULT_CLEARED` events drive the FSM to `STATE_FAULT` or back.
- **E-stop ISR** — pin-change interrupt on `PIN_ESTOP` (NC mushroom switch, INPUT_PULLUP). Pressed = HIGH (NC opens) → relay coil cut, motor power killed, FSM forced to `STATE_ESTOP`.

Module-level reference is in each member guide. The architectural deep-dive is [`firmware/src/system_types.h`](./firmware/src/system_types.h) — start there if you want to understand how tasks talk to each other.

---

## 6. Repository Structure

The full annotated tree as of `main`:

```
vertical-lift-bridge/
│
├── README.md                    # ← this file
├── .gitignore                   # PlatformIO + KiCad + IDE temp files
│
├── bom/                         ◄── M5 owner
│   └── VLB_Group7_BOM.xlsx      # 51 line items, 4 sections, KES totals
│
├── cad/                         ◄── M2 owner
│   ├── README_cad.md            # Print order, slicer profile, materials
│   ├── scad/                    # 14 OpenSCAD parametric source files
│   │   ├── 00_common.scad           # Shared parameters (deck height, drum dia)
│   │   ├── 01_tower_left.scad
│   │   ├── 02_tower_right.scad
│   │   ├── 03_bridge_deck_section.scad
│   │   ├── 04_counterweight_left.scad
│   │   ├── 05_counterweight_right.scad
│   │   ├── 06_pulley_wheel.scad
│   │   ├── 07_motor_mount.scad
│   │   ├── 08_cable_guide.scad
│   │   ├── 09_road_barrier.scad
│   │   ├── 10_base_plate_section.scad
│   │   ├── 11_pcb_enclosure.scad
│   │   ├── 12_camera_mount.scad
│   │   ├── 13_tft_display_mount.scad
│   │   └── assembly.scad        # Renders all parts in their assembled positions
│   ├── stl/                     # 13 STLs ready for slicer (~1.05 kg PLA, ~70h print)
│   └── step/                    # Selected 3MF assemblies for review
│
├── docs/                        ◄── M1 lead, all members contribute
│   ├── 02_revised_proposal.md   # Updated proposal (vertical-lift mechanism)
│   ├── 05_electronics_design.md # Full electronics spec (M5)
│   ├── audit_report.md          # Original vs revised mechanism comparison
│   ├── proposals/               # Original / superseded proposal docs
│   └── figures/                 # Diagrams, photos, screenshots
│
├── firmware/                    ◄── M1 owns build/system; per-module ownership below
│   ├── platformio.ini           # PlatformIO config (env vlb_main, ESP32 dev board)
│   ├── include/                 # (reserved for shared headers — currently empty)
│   ├── lib/                     # (reserved for project-private libs — currently empty)
│   ├── test/                    # (reserved for unit tests — currently empty)
│   ├── assets/                  ◄── M4
│   │   ├── README.md            # Font/icon conversion instructions
│   │   ├── fonts/               # Drop converted LVGL .c font files here
│   │   └── icons/               # Drop converted LVGL .c icon files here
│   └── src/
│       ├── main.cpp             # ◄── M1 — boot, task creation, mutex/queue init
│       ├── pin_config.h         # ◄── M1 authority — every GPIO assignment
│       ├── system_types.h       # ◄── M1 authority — shared types/enums/tunables
│       ├── fsm/                 # ◄── M1
│       │   ├── fsm_engine.h/.cpp    # Table-driven 9-state FSM
│       │   ├── fsm_guards.h/.cpp    # 5 boolean preconditions
│       │   └── fsm_actions.h/.cpp   # Entry/exit side-effects
│       ├── counterweight/       # ◄── M2 (simulation logic)
│       │   └── counterweight.h/.cpp # Simulated pump/drain water tanks
│       ├── motor/               # ◄── M2
│       │   └── motor_driver.h/.cpp  # BTS7960 PWM, encoder, current sense
│       ├── sensors/             # ◄── M3
│       │   └── ultrasonic.h/.cpp    # Dual HC-SR04, direction inference
│       ├── vision/              # ◄── M3
│       │   └── vision_link.h/.cpp   # UART2 JSON parser, heartbeat
│       ├── traffic/             # ◄── M4
│       │   ├── traffic_lights.h/.cpp # 74HC595 LED chain
│       │   └── buzzer.h/.cpp        # LEDC ch5 piezo + patterns
│       ├── hmi/                 # ◄── M4 — TEMPLATE, screens are creative-liberty
│       │   ├── display.h/.cpp       # LVGL + TFT_eSPI bringup, 5 screen stubs
│       │   └── input.h/.cpp         # Resistor-ladder front-panel buttons
│       └── safety/              # ◄── M5
│           ├── watchdog.h/.cpp      # TWDT + per-task software watchdog
│           ├── fault_register.h/.cpp # 16-flag fault evaluation
│           └── interlocks.h/.cpp    # E-stop ISR, relay, barrier servos
│
├── firmware_cam/                ◄── M3 — separate PlatformIO project
│   ├── platformio.ini           # env esp32cam (AI-Thinker board)
│   └── src/
│       └── main.cpp             # OV2640 grayscale, frame-diff motion, JSON line out
│
├── mechanical/                  ◄── M2 — physical build documentation
│   ├── dimensions/              # Drawings, measurements
│   └── photos/                  # Build log photos (commit per assembly milestone)
│
├── member_guides/               ◄── M1 maintains, each member writes their own
│   ├── M1_George_System_FSM.md       # Zero-assumption setup + workflow for M1
│   ├── M2_Eugene_Mechanism.md        # Print pipeline, motor calibration, assembly
│   ├── M3_Cindy_Vision_Sensors.md    # Arduino IDE, ESP32-CAM flash, ROI tuning
│   ├── M4_Abigael_Traffic_HMI.md     # LVGL design, font/icon converters, themes
│   └── M5_Ian_PCB_Power_Safety.md    # KiCad workflow, JLCPCB, soldering, watchdog
│
└── pcb/                         ◄── M5
    ├── gerbers/                 # JLCPCB-ready Gerber + drill files
    └── kicad-project/           # KiCad 8 schematic + layout files
```

**Empty directories** (`firmware/include/`, `firmware/lib/`, `firmware/test/`) are placeholders held in Git via `.gitkeep`. They are wired into the PlatformIO build path so any file dropped into them compiles automatically.

**We work directly on `main`** — this is a single-branch repository to keep things simple. See [§13 Day-to-Day Git Workflow](#13-day-to-day-git-workflow).

---

## 7. Documentation Map

| If you want to... | Read this |
|-------------------|-----------|
| Get going as a group member | Your file in [`member_guides/`](./member_guides/) |
| Understand the bridge mechanism in depth | [`docs/02_revised_proposal.md`](./docs/02_revised_proposal.md) |
| See the electronics spec | [`docs/05_electronics_design.md`](./docs/05_electronics_design.md) |
| Understand why the design changed | [`docs/audit_report.md`](./docs/audit_report.md) |
| Print the parts | [`cad/README_cad.md`](./cad/README_cad.md) |
| Procure components | [`bom/VLB_Group7_BOM.xlsx`](./bom/VLB_Group7_BOM.xlsx) |
| Know which GPIO does what | [`firmware/src/pin_config.h`](./firmware/src/pin_config.h) |
| Know which states/events the FSM uses | [`firmware/src/system_types.h`](./firmware/src/system_types.h) |
| Customise the operator dashboard | [`firmware/src/hmi/display.cpp`](./firmware/src/hmi/display.cpp) and the M4 guide |
| Add fonts or icons to the dashboard | [`firmware/assets/README.md`](./firmware/assets/README.md) |

---

## 8. Tools and Software You Need

These are the canonical versions every member should install. Mismatched versions are the most common cause of "it builds on my machine but not yours".

| Tool | Version | Purpose | Required by |
|------|---------|---------|-------------|
| Git | 2.40 or newer | Version control | Everyone |
| VS Code | 1.85 or newer | Primary editor | Everyone |
| PlatformIO IDE extension | Latest | Build/upload firmware | Everyone who touches firmware |
| Python | 3.11.x (NOT 3.12) | PlatformIO requirement | Everyone who touches firmware |
| ESP32 USB driver (CH340 or CP2102) | Latest | Serial enumeration | Everyone who flashes hardware |
| Arduino IDE | 2.3.x | Easier ESP32-CAM upload than PIO | M3 |
| OpenSCAD | 2021.01 or newer | Edit `.scad` files | M2, M5 |
| PrusaSlicer | 2.7.x | Convert STL to G-code | M2 |
| KiCad | 8.0.x | Schematic + PCB editor | M5 |

Skip what doesn't apply to you — for example, M4 doesn't need KiCad, M5 doesn't need a slicer.

---

## 9. Getting Started — Your First Day

This is the sequential checklist for a member who just got added to the repo. Each step links to a deeper section if you need more help.

- [ ] **Step 1.** Install Git ([§10.1](#101-install-git)).
- [ ] **Step 2.** Install VS Code ([§10.2](#102-install-vs-code)).
- [ ] **Step 3.** Configure your Git identity ([§10.3](#103-configure-your-git-identity)).
- [ ] **Step 4.** Set up GitHub authentication — pick **either** PAT or SSH ([§11](#11-github-authentication)). You only need to do this once per machine.
- [ ] **Step 5.** Clone the repository ([§12](#12-cloning-the-repository)).
- [ ] **Step 6.** Install the rest of the tools you need from [§8](#8-tools-and-software-you-need) (PlatformIO if you touch firmware, KiCad if you're M5, OpenSCAD if you're M2, etc.).
- [ ] **Step 7.** Open your guide in `member_guides/` and follow the role-specific setup steps inside it.
- [ ] **Step 8.** Read [§13 Day-to-Day Git Workflow](#13-day-to-day-git-workflow) end-to-end before making your first change.
- [ ] **Step 9.** Make a tiny test change (fix a typo in your member guide), commit it, and push to `main`. This validates your whole setup. Don't skip — the time to discover that your auth is broken is on your first commit, not on a deadline night.
- [ ] **Step 10.** Post in the group chat: "I'm set up — onto real work."

---

## 10. Setting Up Your Machine

Instructions assume **Windows 11**. Notes for macOS / Linux at the end of each subsection.

### 10.1 Install Git

**Windows:**
1. Open Edge. Navigate to `https://git-scm.com/download/win`.
2. The 64-bit installer downloads automatically. Run it.
3. Take all defaults except:
   - **Default editor**: choose "Use Visual Studio Code as Git's default editor" (after you've installed VS Code in §10.2 — re-run the installer if you go out of order).
   - **Initial branch name**: choose "Override the default branch name for new repositories" → enter `main`.
4. Open Command Prompt (`Win+R`, type `cmd`, Enter) and run `git --version`. Expected output: `git version 2.43.0.windows.1` or newer.

**macOS:** `brew install git` (after installing Homebrew from `https://brew.sh`). **Linux:** `sudo apt install git` (Debian/Ubuntu) or your distro equivalent.

### 10.2 Install VS Code

1. Open Edge → `https://code.visualstudio.com` → "Download for Windows".
2. Run `VSCodeUserSetup-x64-*.exe`.
3. On the "Select Additional Tasks" screen, tick all four "Other" boxes:
   - Add "Open with Code" to file context menu
   - Add "Open with Code" to directory context menu
   - Register Code as an editor for supported file types
   - Add to PATH
4. Click Install. Launch VS Code when finished.

### 10.3 Configure Your Git Identity

In a fresh terminal (Command Prompt, PowerShell, or VS Code's integrated terminal):

```sh
git config --global user.name "Your Full Name"
git config --global user.email "your.email@students.jkuat.ac.ke"
```

Use the same email that's tied to your GitHub account (check at `https://github.com/settings/emails`) — otherwise GitHub won't link your commits to your profile.

Verify:

```sh
git config --global --list
```

You should see your name, email, and `init.defaultBranch=main`.

### 10.4 Useful Optional Settings

These three save real time over the course of the project:

```sh
git config --global pull.rebase false           # merge commits on git pull (less surprising)
git config --global core.autocrlf input         # don't mangle line endings
git config --global init.defaultBranch main     # already set above; here for completeness
```

If you're on Windows and find yourself fighting with line endings (CRLF vs LF), `core.autocrlf input` keeps everything LF in the repo, which is what the codebase expects.

---

## 11. GitHub Authentication

You're already a verified collaborator on the repo — you don't need to be added or accept any invitation. What you **do** need is to prove to GitHub that the Git client on your machine is acting on your behalf when it pushes.

GitHub no longer accepts plain passwords over HTTPS. You have two choices:

- **Personal Access Token (PAT)** — easier to set up, works over HTTPS like a long password. Recommended for first-time setup.
- **SSH key** — more secure, no expiry hassle once configured. Recommended for long-term use.

You only need **one** of the two. Pick whichever sounds easier.

### 11.1 Option A — Personal Access Token (HTTPS)

#### Generate the token on GitHub

1. Go to `https://github.com` and sign in.
2. Click your profile picture (top-right) → **Settings**.
3. In the left sidebar, scroll to the bottom: **Developer settings**.
4. Click **Personal access tokens** → **Tokens (classic)** → **Generate new token** → **Generate new token (classic)**.
5. Fill in:
   - **Note**: something descriptive like `vlb-laptop-may-2026`.
   - **Expiration**: 90 days is reasonable for this project — don't choose "No expiration".
   - **Scopes**: tick the entire **repo** group (this gives push/pull access to private and public repos you're a collaborator on).
6. Click **Generate token** at the bottom.
7. **Copy the token immediately** — GitHub will not show it again. It looks like `ghp_AbCd1234...`.

#### Configure Git to use the token

The cleanest way is to let Windows Credential Manager remember it the first time you push.

1. Configure the credential helper (one-time, per machine):
   ```sh
   git config --global credential.helper manager
   ```
2. The next time you `git push` to this repository, Windows will pop up a dialog asking for your username and password.
   - **Username**: your GitHub username (e.g. `kiogimwenda`).
   - **Password**: paste the **token**, not your GitHub password.
3. Tick "Save credentials" if offered. From now on, pushes happen without prompts.

#### Test it

After cloning the repo (§12), run:

```sh
git fetch
git push --dry-run origin main
```

If you see `Everything up-to-date` or a list of refs without an authentication error, you're set. If you get `Authentication failed`, your token was either typed wrong or doesn't have the `repo` scope — regenerate it.

#### When the token expires

GitHub will email you ~7 days before expiry. Generate a new token, then on Windows clear the saved credential:

1. **Win+R** → `control /name Microsoft.CredentialManager` → Enter.
2. Find the entry `git:https://github.com` → expand → **Remove**.
3. The next push will re-prompt. Paste the new token.

### 11.2 Option B — SSH Key

#### Generate the key

1. Open Git Bash (installed with Git for Windows — search "Git Bash" in the Start menu).
2. Run:
   ```sh
   ssh-keygen -t ed25519 -C "your.email@students.jkuat.ac.ke"
   ```
3. When prompted "Enter file in which to save the key", press **Enter** to accept the default `~/.ssh/id_ed25519`.
4. When prompted for a passphrase, you can leave it empty (press Enter twice) for convenience, or set one for security. If you set one, you'll be prompted for it once per session unless you add the key to `ssh-agent` (see step 5).
5. Optional but recommended — start the SSH agent so you don't re-enter the passphrase:
   ```sh
   eval "$(ssh-agent -s)"
   ssh-add ~/.ssh/id_ed25519
   ```

#### Add the public key to GitHub

1. Print your public key:
   ```sh
   cat ~/.ssh/id_ed25519.pub
   ```
2. Copy the entire output, starting with `ssh-ed25519` and ending with your email.
3. Go to GitHub → profile picture → **Settings** → **SSH and GPG keys** → **New SSH key**.
4. **Title**: descriptive, e.g. `vlb-laptop`.
5. **Key type**: Authentication Key.
6. **Key**: paste.
7. Click **Add SSH key**.

#### Tell Git to use SSH for this repo

If you've already cloned with HTTPS, switch the remote:

```sh
cd path/to/vertical-lift-bridge
git remote set-url origin git@github.com:kiogimwenda/vertical-lift-bridge.git
```

If you haven't cloned yet, use the SSH URL directly in §12.

#### Test it

```sh
ssh -T git@github.com
```

Expected: `Hi <your-username>! You've successfully authenticated, but GitHub does not provide shell access.` That message is success.

Then:

```sh
git fetch
git push --dry-run origin main
```

### 11.3 Which to Choose?

| Criterion | PAT (HTTPS) | SSH key |
|-----------|------------|---------|
| Time to set up | 5 minutes | 10 minutes |
| Need to renew | Yes — every 90 days | No |
| Works through restrictive firewalls | Yes (port 443) | Sometimes blocks port 22 |
| Different per repo | One token can cover all repos | One key covers all repos |

If you're on a corporate or campus network that blocks SSH (rare but possible), use PAT. Otherwise SSH is set-and-forget.

---

## 12. Cloning the Repository

### 12.1 Choose a stable location

Pick a folder you won't move or rename. Common choices:

- Windows: `C:\Users\<you>\Documents\` or `D:\Projects\`
- macOS: `~/Documents/` or `~/Projects/`
- Linux: `~/projects/`

**Avoid OneDrive, iCloud Drive, Google Drive, or Dropbox.** Their auto-sync conflicts with Git's `.git` directory and corrupts repos.

### 12.2 Clone

Open a terminal in the parent folder. For example, on Windows:

```sh
cd C:\Users\%USERNAME%\Documents
```

Then clone using the URL matching your auth method:

**HTTPS (PAT):**
```sh
git clone https://github.com/kiogimwenda/vertical-lift-bridge.git
```

**SSH:**
```sh
git clone git@github.com:kiogimwenda/vertical-lift-bridge.git
```

You'll see progress like `Receiving objects: 100% (XXX/XXX), done.` Then:

```sh
cd vertical-lift-bridge
git status
```

Expected output: `On branch main / Your branch is up to date with 'origin/main' / nothing to commit, working tree clean`.

### 12.3 Open in VS Code

```sh
code .
```

(The `code` command works because we ticked "Add to PATH" during VS Code install.) PlatformIO will detect the `firmware/platformio.ini` and start indexing — first time takes a couple of minutes.

### 12.4 Pull the latest changes

Whenever you sit down to work, run this **before** doing anything else:

```sh
git checkout main
git pull origin main
```

This pulls any commits other members have merged since you last looked. Skipping it is the #1 cause of merge conflicts.

---

## 13. Day-to-Day Git Workflow

We use a **single-branch workflow** — everyone works directly on `main`. This keeps things simple for a small team where most members are new to Git. No branches, no pull requests, no merge conflicts from stale branches.

### 13.1 The Cycle

```
        ┌─────────────────────────────────────────────┐
        │       1. git pull origin main               │
        │       (ALWAYS do this before you start)     │
        └───────────────────┬─────────────────────────┘
                            ▼
        ┌─────────────────────────────────────────────┐
        │  2. Edit files, build, test on bench        │
        └───────────────────┬─────────────────────────┘
                            ▼
        ┌─────────────────────────────────────────────┐
        │     3. git add <files> && git commit -m "…" │
        └───────────────────┬─────────────────────────┘
                            ▼
        ┌─────────────────────────────────────────────┐
        │       4. git pull origin main               │
        │       (catch anything pushed while you      │
        │        were working — resolve conflicts     │
        │        if any)                              │
        └───────────────────┬─────────────────────────┘
                            ▼
        ┌─────────────────────────────────────────────┐
        │       5. git push origin main               │
        └─────────────────────────────────────────────┘
```

**The golden rule: always pull before you push.** This avoids most conflicts. If two people edit the same file at the same time, Git will ask you to resolve the conflict — open VS Code's merge editor and pick the right lines.

### 13.2 Commit Message Convention

Use **Conventional Commits** — every message starts with a type, then a colon, then a short imperative sentence. The scope (in parentheses) clarifies which module:

```
firmware(motor): add stall detection at 2 s no-progress threshold
fix(safety): clear FAULT_OVERCURRENT after operator clears
docs(readme): update repo structure section after cad/ reorg
chore(ci): bump platformio image to 6.7.1
```

Why we bother:
- It's machine-parseable (we can generate changelogs later).
- Reviewers can scan a 50-commit PR list and immediately know what each does.
- It forces you to think about what the commit *actually does* before writing it.

Anti-patterns:
- ❌ `update`
- ❌ `fix bug`
- ❌ `WIP`
- ❌ `asdf`
- ✅ `firmware(fsm): correct transition from RAISED_HOLD on EVT_HOLD_TIMEOUT`

### 13.3 What If I Make a Mistake?

| Mistake | Fix |
|---------|-----|
| Pushed something broken to `main` | Tell the group **immediately**. Run `git revert HEAD` to undo the last commit, then push the revert |
| `git push` rejected ("non-fast-forward") | You forgot to pull first. Run `git pull origin main`, resolve any conflicts, then push again |
| Merge conflict after pulling | Open VS Code's merge editor, pick the right lines, then `git add .` and `git commit` |
| Want to undo a local change before commit | `git checkout -- path/to/file` (discards changes to that file) |
| Accidentally deleted a file | `git checkout -- path/to/file` to restore it from the last commit |

If you ever run a command and it gives you a warning about `--force` or losing history, **stop and ask in the group chat**. Force-pushing is never the right answer for us.

---

## 14. Working with the Firmware

### 14.1 Build the Main Firmware

```sh
cd firmware
pio run
```

Or in VS Code: PlatformIO sidebar (alien-head icon) → **Project Tasks** → **vlb_main** → **General** → **Build**.

First build downloads ~200 MB of toolchain — be patient. Subsequent builds are < 30 seconds.

### 14.2 Flash the Main Firmware

With the ESP32 DevKit plugged in via USB:

```sh
pio run -t upload
```

Or VS Code: **Project Tasks** → **vlb_main** → **General** → **Upload**.

If the upload fails with `Failed to connect`, hold the **BOOT** button on the ESP32 board, press **EN** briefly, then release **BOOT**. Try again.

### 14.3 Monitor Serial Output

```sh
pio device monitor -b 115200
```

Or VS Code: **Project Tasks** → **vlb_main** → **General** → **Monitor**.

The expected boot sequence:

```
=== VLB Group 7 — Boot ===
ESP32 SDK: vX.Y.Z | Sketch: <date>
[wdt] init OK (5s hw, 1.5s sw)
[fault] init OK
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
[fsm] 8 -> 0       <-- INIT to IDLE (success)
```

Press **Ctrl+T** then **Ctrl+X** to exit the monitor.

### 14.4 Build and Flash the ESP32-CAM Companion

The CAM uses a separate PlatformIO project:

```sh
cd firmware_cam
pio run -t upload
```

The ESP32-CAM has no USB port — you flash through an FTDI/CP2102 adapter with the **IO0 pin grounded** during reset (see [`member_guides/M3_Cindy_Vision_Sensors.md`](./member_guides/M3_Cindy_Vision_Sensors.md) for wiring).

### 14.5 Common Tasks

**Add a library:**
Edit `firmware/platformio.ini`:
```
lib_deps =
    ...existing...
    bblanchon/ArduinoJson@^7.2.1     ; example
```
Then `pio run` — PIO downloads it into `.pio/libdeps/`.

**Change a tunable:**
All numeric tunables (timeouts, thresholds, calibration constants) live in `firmware/src/system_types.h` near the bottom. Edit there, rebuild, flash.

**Add a new fault flag:**
1. Add the bit to the `FaultFlag_t` enum in `system_types.h`.
2. Add the human-readable name to `fault_register_first_name()` in `firmware/src/safety/fault_register.cpp`.
3. Set the flag from the module that detects it.
4. Document it in your member guide.

---

## 15. Working with the CAD

CAD lives in [`cad/`](./cad/). Source-of-truth is the OpenSCAD `.scad` files; STL/3MF outputs are derivatives committed for convenience.

To open a part:

```sh
openscad cad/scad/01_tower_left.scad
```

Or right-click the file in VS Code → "Reveal in File Explorer" → double-click.

To re-render after editing:

1. Open in OpenSCAD.
2. **F6** to render (full evaluation, slow).
3. **File → Export → Export as STL** — save to `cad/stl/01_tower_left.stl`.

Slicer profile and print order are documented in [`cad/README_cad.md`](./cad/README_cad.md). Detailed assembly + post-processing live in [`member_guides/M2_Eugene_Mechanism.md`](./member_guides/M2_Eugene_Mechanism.md).

---

## 16. Working with the PCB

PCB design lives in [`pcb/kicad-project/`](./pcb/kicad-project/). To open:

1. Launch KiCad 8.
2. **File → Open Project** → navigate to `pcb/kicad-project/` and pick the `.kicad_pro` file.

Both schematic and PCB editors open from the project window.

To regenerate Gerbers (after edits):

1. PCB Editor → **File → Plot**.
2. Output directory: `pcb/gerbers/`.
3. See `member_guides/M5_Ian_PCB_Power_Safety.md` §5 for the exact layer/option settings JLCPCB expects.

---

## 17. Per-Member Quick-Reference

A one-line "what to type in the terminal each time you start work" for each member. Open your member guide for the full picture.

| Member | First-time setup | Daily |
|--------|------------------|-------|
| M1 | `cd firmware && pio run` | `git pull → edit fsm/ → build → commit → push` |
| M2 | Install OpenSCAD + PrusaSlicer | `git pull → edit cad/scad/ or motor/ → commit → push` |
| M3 | Install Arduino IDE + ESP32 board package | `cd firmware_cam && pio run -t upload` for vision; `firmware/` for sensors |
| M4 | Install PlatformIO; bookmark LVGL converters | `cd firmware && pio run` after editing hmi/ or traffic/ |
| M5 | Install KiCad 8 + JLCPCB plugin | `git pull → edit pcb/ or safety/ → commit → push` |

---

## 18. Communication and Escalation

### 18.1 Group Chat

Day-to-day coordination, quick questions, "I'm pushing X now", "I broke main, sorry, M1 fixing".

### 18.2 GitHub Issues

For anything that needs to be tracked across days. Things that should be issues, not chat:
- "Motor stalls 2 cm before top limit — repro: every cycle"
- "TFT shows ghosting on boot screen — looks like buffer alignment"
- "BOM line 23 — Pixel Electric out of stock on 2N7000, need substitute"

Don't put these in the chat alone — chat history evaporates, issues persist.

### 18.3 GitHub Issues

For design discussions and implementation choices ("should we use `lv_arc` or `lv_meter` for current?"), open an issue so the rationale is preserved — chat history evaporates, issues persist.

### 18.4 Escalation Path

| Severity | Who to ping | When |
|----------|-------------|------|
| "Main is broken" | M1 + group chat @everyone | Immediately |
| Hardware safety issue (smoke, heat, sparks) | Everyone, in person | Immediately, then unplug |
| Blocked > 1 hour by a missing part | M5 (procurement) | Same day |
| Blocked > 1 hour by a build/auth issue | M1 (system) | Same day |
| Need a design decision | All members + lecturer if architectural | Within 24 h |

---

## 19. Demo Day Checklist

Print this and tick boxes the morning of.

- [ ] All five members have flashed the latest `main` to the bench rig at least once.
- [ ] Spare ESP32 DevKit + ESP32-CAM in the kit bag.
- [ ] Spare cable, spare counterweight pre-filled, in kit bag.
- [ ] Bench rig 12 V power supply tested at 11 V undervolt — fault triggers and clears.
- [ ] E-stop pressed mid-cycle — motor stops in < 50 ms, FSM in `STATE_ESTOP`.
- [ ] All 9 FSM states reached during a rehearsal run.
- [ ] At least one full IDLE → RAISED → IDLE cycle in < 25 s.
- [ ] No fault flags set at end of cycle.
- [ ] Demo script (`docs/demo_script.md`) on a clipboard or phone.
- [ ] M4 dashboard rendering at expected brightness in demo room lighting.
- [ ] Group photo taken **before** demo (in case anything explodes).

---

## 20. Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `git push` says "Permission denied (publickey)" | SSH key not added to GitHub or wrong remote URL | §11.2 to verify, or switch to PAT in §11.1 |
| `git push` says "Authentication failed" or "could not read Username" | PAT wrong, expired, or lacks `repo` scope | Regenerate PAT, clear Windows Credential Manager entry |
| `git pull` shows merge conflicts | You and another member edited the same file | Open VS Code, use the merge editor, resolve, `git add`, `git commit` |
| PlatformIO "Connecting…" timeout on upload | Auto-reset failed | Hold BOOT, press EN, release BOOT, retry |
| Boot loops with `Brownout detector triggered` | USB hub or PC port underpowering | Use a powered hub or directly into a PC port |
| Build fails with `fatal error: lvgl.h: No such file` | Library not yet downloaded | First `pio run` downloads libs — wait for it; or run `pio pkg install` |
| KiCad complains about missing footprint | Library tables not configured | KiCad  → Preferences → Manage Footprint Libraries → re-add `kicad-jlcpcb-tools` |
| OpenSCAD `cannot find module` | Renamed/moved a `.scad` file | `include <00_common.scad>` — paths in `.scad` are relative |
| Repo is multi-GB after a few weeks | Someone committed a huge file | Don't commit STL files larger than 50 MB; split, or use Git LFS — ask M1 |
| `firmware/.pio/` keeps appearing in `git status` | Missing in `.gitignore` | Should already be there; pull main to refresh |

For deeper firmware debugging, every module logs to Serial with a `[xxx]` prefix — grep monitor output for the module name (`[motor]`, `[fsm]`, `[fault]`, etc.).

---

## 21. Glossary

| Term | Meaning |
|------|---------|
| FSM | Finite State Machine — the bridge has 9 states (IDLE, RAISING, etc.) |
| LVGL | Light and Versatile Graphics Library — the GUI framework on the TFT |
| PWM | Pulse-Width Modulation — how the motor speed and LED brightness are controlled |
| LEDC | The ESP32 hardware peripheral that generates PWM |
| HSPI | Hardware SPI bus on the ESP32 — TFT and touch share this |
| UART | Serial port — the main board talks to the CAM over UART2 |
| TWDT | Task Watchdog Timer — ESP32 hardware safety reset if a task hangs |
| BTS7960 | The 43 A H-bridge motor driver IC |
| HC-SR04 | The ultrasonic distance sensor module |
| OV2640 | The 2 MP camera sensor on the ESP32-CAM |
| MGN12 | A 12 mm-wide miniature linear rail |
| Hall encoder | Magnetic rotary encoder built into the JGA25-370 motor |
| PAT | Personal Access Token — GitHub's password-replacement for HTTPS auth |
| Counts/mm | Encoder pulses per millimetre of deck travel — the calibration constant |
| ROI | Region Of Interest — the patch of the camera frame where motion is checked |

---

## 22. License

MIT. See [`LICENSE`](./LICENSE) if present, otherwise this work is © 2026 Group 7, JKUAT, EEE 2412 Microprocessors II, released under MIT terms.

---

> **Maintainer note:** this README is the entry point for every new member. If you change the repo structure, update §6. If you change the workflow, update §13. If a new tool becomes necessary, update §8. This is a single-branch repo — everything goes directly to `main`. Don't let the docs drift.
