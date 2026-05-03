# Member 5 — Ian — PCB (from scratch), Power, ESP32-CAM Electronics & Safety Firmware

> **Your role.** You design the PCB **from a completely blank KiCad project**, route the power tree, build the safety chain (E-stop, relay, watchdog), maintain the fault registry, and host the ESP32-CAM electronics on the same board so M3's vision link is plug-and-play. The previous KiCad project has been deleted — you start fresh.
>
> **Your single deliverable.** A 100 × 80 mm 2-layer PCB, populated and tested, that:
> - Accepts a 12 V DC barrel jack input.
> - Generates 5 V (3 A LM2596) and 3.3 V (800 mA AMS1117) rails.
> - Generates a separate clean 5 V/2 A rail for the ESP32-CAM via a second LM2596 (electrically isolated to prevent brownout coupling).
> - Hosts the ESP32-WROOM-32E DevKit module on female headers.
> - Hosts the BTS7960 motor driver via screw terminals.
> - Hosts a 5 V SRD-05VDC relay in series with motor 12 V supply (E-stop chain).
> - Has connectors for: TFT (14-pin), ESP32-CAM (4-pin JST-XH), 4 × HC-SR04 (per-sensor JST-XH), barrier servos, traffic LED panel, mushroom E-stop, USB-C programming.
> - Passes ERC + DRC clean, with antenna keepout enforced.
> - Powers up cleanly: 12 V draws < 100 mA at idle, ESP32 boots, all `[xxx] init OK` lines appear.
> - Cuts motor power within 50 ms of E-stop press (verified on bench).

> **What's different in this guide.** The previous KiCad project was deleted on 2026-05-03 because it had drifted out of sync with the v2.1 firmware (rail-sense divider footprints in the wrong place; missing dedicated CAM buck; no proper antenna keepout). You're building a brand new project that exactly matches the current firmware pin map and the v2.1 known-limitations document.

---

## 0. What you are building (read this once, then refer back constantly)

Three separable subsystems on one board:

1. **Power tree.** 12 V in → reverse-polarity Schottky → polyfuse → TVS clamp → splits to motor (via relay) + LM2596 main (5 V, 3 A) + LM2596 CAM (5 V, isolated). 5 V main → AMS1117 → 3.3 V (800 mA).
2. **Compute + I/O.** ESP32-WROOM-32E DevKit on female headers. All GPIOs broken out to JST-XH connectors that ribbon-cable to off-board peripherals.
3. **Safety chain.** Mushroom NC E-stop in series with relay coil control via 2N7000 MOSFET. Relay contact in series with motor B+ supply. Watchdog firmware backs it up and forces SAFE state on hang.

The ESP32-CAM is **off-board** (mounted on the camera gantry above the road), but **its power lives on this PCB** — the dedicated CAM buck and a 4-pin JST-XH connector (J7) carry GND, V5_CAM, CAM RX (unused), and CAM TX (which becomes the main board's UART2 RX).

---

## Scope of your work — every asset you touch

| Asset | Where | Status |
|---|---|---|
| KiCad project (all sub-sheets, .kicad_pro, .kicad_pcb) | `pcb/kicad-project/` | ❌ **deleted — you create from scratch (Step 3)** |
| Gerber output | `pcb/gerbers/JLCPCB_order.zip` | ❌ generate (Step 9) |
| BOM | `bom/VLB_Group7_BOM.xlsx` | ✅ exists — you maintain |
| `firmware/src/safety/watchdog.{h,cpp}` | firmware | ✅ exists — you tune |
| `firmware/src/safety/fault_register.{h,cpp}` | firmware | ✅ exists — rail monitoring removed in v2.1 |
| `firmware/src/safety/interlocks.{h,cpp}` | firmware | ✅ exists — wire to your PCB |
| `docs/05_electronics_design.md` | docs | ✅ exists — you keep up to date as design changes |
| `docs/build_log/` | docs | ❌ create — top + bottom + integrated photos |
| `docs/procurement.md` | docs | ❌ create — record JLCPCB order, tracking, BOM Kenya suppliers |

---

## Step 1 — Workstation setup (Windows 11, no prior knowledge assumed)

> Estimated time: 2 hours including downloads.

### 1.1 Common tooling
Follow **steps 1.1 to 1.7 of M1's guide** first (VS Code, Git, Python 3.11, PlatformIO IDE inside VS Code, CH340 USB driver, repo clone). Come back here when done.

### 1.2 Install KiCad 10.x (the PCB design tool)
KiCad 10 ships with the new IPC API that the Claude MCP connector relies on. KiCad 9 also works — the schematic-capture and PCB-editor workflows are identical — but the MCP integration in Step 1.4 may have reduced functionality on 9.

1. Open Microsoft Edge. Type `kicad.org/download/windows`. Press Enter.
2. Click "Windows 64-bit Installer" under the latest 10.x release (~1.4 GB download — patient on Kenyan broadband; allow 30 min).
3. Run `kicad-10.0.x-x86_64.exe`. Click "Yes" on the UAC prompt.
4. **License** → I Agree.
5. **Choose Components** → tick **everything**:
   - ☑ KiCad EDA application
   - ☑ Schematic Symbol Libraries
   - ☑ PCB Footprint Libraries
   - ☑ 3D Models (~600 MB, but worth it for the 3D viewer)
   - ☑ Project Templates
   - ☑ Demos
6. **Install Location** → keep default `C:\Program Files\KiCad\10.0\`.
7. Click **Install**. Wait ~10 min.
8. After install, launch KiCad. The **Project Manager** opens.
9. **First-launch library sync** — KiCad downloads the official symbol/footprint libraries from GitHub. You'll see "Synchronizing libraries…" in the bottom status bar. Wait until it says "Ready" (~5 min).
10. **Confirm version:** Help → About KiCad → should show "KiCad 10.0.x".

### 1.3 Install JLCPCB component-library plugin (BOM cross-reference helper)
JLCPCB has its own component library with stock matching to LCSC part numbers. The plugin lets you tag components in your schematic with the exact LCSC part number so JLCPCB assembly (if you ever do PCBA) is easy.

1. Edge → `github.com/Bouni/kicad-jlcpcb-tools`.
2. Click the green **Code** button → **Download ZIP**.
3. Save the ZIP to `Downloads\`.
4. Open KiCad → top menu **Tools** → **Plugin and Content Manager**.
5. Click the **"Install from File…"** button (upper right of the dialog).
6. Select the ZIP you just downloaded → Open.
7. The plugin appears in the "Installed Packages" list. Click **Apply Pending Changes** → close.
8. Restart KiCad. Open the PCB editor (any project). The plugin now appears as a button in the toolbar (small yellow JLC icon).

### 1.4 Install the kicad-mcp connector (Claude ↔ KiCad bridge)

The community has built a Model Context Protocol (MCP) server for KiCad called **`kicad-mcp`**. It exposes your KiCad project files (schematics, PCB, BOM, DRC results) as queryable tools so Claude can read them, suggest improvements, and answer questions about your design. **As of mid-2026 this server is read-only** — it lets Claude inspect, not modify, your KiCad files. That's a feature, not a bug: the human (you) stays in the driver's seat.

> **The kicad-mcp ecosystem is moving fast.** Verify the latest README at `github.com/lamaaa30/kicad-mcp` before following the exact pip-install command below — the maintainer occasionally renames the package or changes the entry point. The general flow (clone, install Python deps, register in Claude Desktop's `claude_desktop_config.json`) stays the same.

#### 1.4.1 Prerequisites
You already have Python 3.11 from M1's Step 1.4. Confirm with:
```cmd
python --version
```
You need **Python 3.10 or newer** for kicad-mcp.

#### 1.4.2 Install Claude Desktop (if you don't already have it)
1. Edge → `claude.ai/download`. Pick "Windows".
2. Run the installer. Default options. Sign in with your Anthropic / Claude account.
3. After install, on first launch, open **Settings** → confirm you can see the menus. (You'll edit a JSON file in here in Step 1.4.5.)

#### 1.4.3 Install kicad-mcp
Open Command Prompt:
```cmd
cd C:\Users\%USERNAME%\Documents
git clone https://github.com/lamaaa30/kicad-mcp.git
cd kicad-mcp
pip install -e .
```

If `pip install -e .` complains about a missing build backend, run:
```cmd
pip install --upgrade pip setuptools wheel
pip install -e .
```

#### 1.4.4 Verify the server starts
```cmd
python -m kicad_mcp --help
```
Should print a usage message listing commands. If you get `ModuleNotFoundError: No module named 'kicad_mcp'`, the install didn't take — re-run `pip install -e .` from inside the cloned directory.

#### 1.4.5 Register the server with Claude Desktop
Claude Desktop reads MCP server configuration from a JSON file:
- Windows path: `%APPDATA%\Claude\claude_desktop_config.json`

Open that file in VS Code (create it if it doesn't exist):
```cmd
code %APPDATA%\Claude\claude_desktop_config.json
```

Paste this content (replace the path to the cloned repo with your actual path):
```json
{
  "mcpServers": {
    "kicad": {
      "command": "python",
      "args": [
        "-m",
        "kicad_mcp",
        "--project-dir",
        "C:/Users/<YOU>/Documents/vertical-lift-bridge/pcb/kicad-project"
      ]
    }
  }
}
```

Replace `<YOU>` with your Windows username. **Use forward slashes** in the path (Windows accepts them in JSON without escaping).

Save the file. **Quit and reopen Claude Desktop completely** (right-click the system tray icon → Quit, then re-launch).

#### 1.4.6 Test the connection
1. In Claude Desktop, start a new conversation.
2. Ask: "What KiCad MCP tools are available?"
3. Claude should list tools like `list_components`, `read_schematic`, `get_drc_results`, etc. If the tools list is empty, the server didn't start — check Claude Desktop's logs (Help → Show Logs) for Python errors.

#### 1.4.7 What you can ask Claude with the connector active
Once the project exists (after Step 3), you can ask things like:
- "List all components in my MotorDrivers schematic."
- "What's the net connecting GPIO 25 to?"
- "Are there any symbols without footprint assignments?"
- "Read the DRC report and summarise the unresolved issues."
- "Compare my BOM to the parts referenced in the schematic."

Claude reads your files; it does not edit them. You apply changes in KiCad based on its suggestions.

> **If the MCP install gives you trouble**, this guide works perfectly without it. The MCP is a productivity tool, not a requirement. Skip Step 1.4 entirely if you'd rather not deal with it.

### 1.5 Hardware tools (you cannot start the build without these)
- **Soldering iron with temperature control**, 0.4 mm or 0.6 mm chisel tip. Hakko FX-888D, Pinecil, or TS80P. Set 350 °C for 60/40 leaded solder, 380 °C for lead-free.
- **Solder.** 0.6 mm leaded 60/40 with rosin core (smoother flow than lead-free for hand assembly).
- **Liquid flux** (rosin-based) — even cheap flux dramatically improves drag-soldering of QFN/SOT packages.
- **Solder wick** (1.5 mm width) for cleanup of bridges.
- **Tweezers** — fine point + bent.
- **Multimeter with continuity beep** (UNI-T UT139C ~KES 2,500 from Pixel Electric).
- **Bench power supply with current limit** (RIDEN RD6006 or equivalent ~KES 4,500 from Pixel). **Without a current limit, your first short will fry traces.**
- **Helping hands or PCB vise** — a third-hand magnifier is fine.
- **Magnifying loupe (10×)** or USB microscope for SMD inspection.
- **Optional: Hot-air rework station** (Yihua 858D ~KES 4,500). Useful for SOT-223 and rework but not strictly required.

---

## Step 2 — KiCad 10 primer (read this once, reference later)

> Skip this if you've used KiCad before. Otherwise spend 30 minutes here — every step below assumes these basics.

### 2.1 KiCad project structure
A KiCad project is a folder containing:
- One `.kicad_pro` file — project definition (settings, library paths, net classes).
- One `.kicad_sch` file per **sheet** (root sheet + each sub-sheet).
- One `.kicad_pcb` file — the PCB layout.
- Optional `.kicad_prl` — local user preferences (zoom, layer visibility).
- Optional `fp-info-cache` — footprint cache for fast loading.
- Optional `<project>-backups/` — KiCad's auto-backups.

You open a project by double-clicking the `.kicad_pro` from File Explorer, or via KiCad Project Manager → File → Open Project.

### 2.2 The four KiCad editors
| Tool | Launched from | What it does |
|---|---|---|
| **Schematic Editor** (eeschema) | KiCad Project Manager → schematic icon | Draw the circuit. Add symbols, wires, labels. Run ERC. |
| **PCB Editor** (pcbnew) | KiCad Project Manager → PCB icon | Place footprints, route copper, generate Gerbers. Run DRC. |
| **Symbol Editor** | top menu Tools → Symbol Editor | Create custom schematic symbols (rare for this project) |
| **Footprint Editor** | top menu Tools → Footprint Editor | Create custom footprints (you'll do this once for the BTS7960 module) |

### 2.3 Hierarchical sheets (how the project splits across multiple files)
For a project this size you don't draw everything on one giant page. You split into **hierarchical sub-sheets**, each its own `.kicad_sch` file, connected to a **root sheet** by **sheet symbols**.

Each sub-sheet exposes named **hierarchical labels** at its boundary that map to **hierarchical pins** on the parent's sheet symbol. Power nets (VCC, GND) usually use **global labels** instead, so they cross sheet boundaries automatically.

**This project's hierarchy** (you'll create all of this in Step 3):
```
vertical_lift_pcb.kicad_sch        (root — just sheet symbols)
├── PowerSupply.kicad_sch          (12V → 5V → 3.3V tree, plus CAM-dedicated 5V)
├── ESP32Module.kicad_sch          (ESP32-WROOM headers, decoupling, GPIO 39 pull-up)
├── USBProgramming.kicad_sch       (CP2102N USB-C bridge for programming + auto-reset)
├── MotorDriver.kicad_sch          (BTS7960 connector, current-sense to GPIO 34)
├── ShiftRegLights.kicad_sch       (74HC595 + 6 LEDs)
├── ConnectorsSafety.kicad_sch     (JST-XH connectors, relay, MOSFET, E-stop, fuse)
└── TFT_Camera.kicad_sch           (TFT 14-pin header, ESP32-CAM 4-pin connector,
                                    backlight Q1, all decoupling)
```

### 2.4 Hotkeys you'll use 100 times each
| Key | In schematic editor | In PCB editor |
|---|---|---|
| `A` | Add symbol | Add footprint |
| `W` | Wire | — |
| `L` | Local label | — |
| `H` | Hierarchical label | — |
| `&` | Global label / Power port | — |
| `P` | Add power port | — |
| `M` | Move (drag along with wires) | Move |
| `R` | Rotate selected | Rotate selected |
| `G` | Drag (keep wires connected) | Drag (keep traces connected) |
| `X` | Mirror X axis | Flip to other side |
| `Y` | — | Mirror Y axis |
| `Delete` | Delete | Delete |
| `Esc` | Cancel current tool | Cancel current tool |
| `Ctrl+Z` | Undo | Undo |
| `Ctrl+Shift+I` | Run ERC | Run DRC |
| `B` | — | Refill all zones (recompute GND pour) |
| `F` | — | Footprint properties |
| `E` | Edit symbol properties | Edit footprint / track properties |
| `Ctrl+S` | Save | Save |
| `Alt+3` | — | Open 3D viewer |

### 2.5 Common mistakes to avoid
- **Floating wires.** A wire that doesn't end on a pin or junction is invisible to ERC. Always end with a junction (auto-placed when 3+ wires meet) or a pin.
- **Symbol vs footprint.** A schematic *symbol* is the abstract pin layout; a *footprint* is the physical pad layout. Every symbol must have a footprint assigned before you can lay out the PCB.
- **Net names from labels.** A label on a wire creates a net. Two wires with the same label name on the same sheet are connected. Two wires with the same **global label** name across different sheets are connected. Two wires with the same **hierarchical label** on different sheets are NOT connected unless wired through the parent sheet symbol.
- **Power flags.** ERC needs at least one **PWR_FLAG** symbol on every power net (V12, V5, V5_CAM, V3V3, GND) so it doesn't complain "Power input pin not driven by any output". Add them on the PowerSupply sheet.

---

## Step 3 — Create the brand-new project (build the empty skeleton)

> Estimated time: 60 min. This step creates all 8 schematic files (root + 7 sub-sheets) with their boundaries defined but no components inside.

### 3.1 Confirm the old project is gone
Open File Explorer → navigate to `D:\ECE_4.2\Microprocessors II\Course project\vertical-lift-bridge\pcb\`. You should see:
- `gerbers\` (empty, with `.gitkeep`)
- **No** `kicad-project\` folder.

If `kicad-project\` still exists, the deletion didn't complete. Close KiCad if open, delete the folder manually, then continue.

### 3.2 Create a new KiCad project
1. Launch KiCad (Start menu → KiCad 10.0).
2. Top menu **File** → **New Project**.
3. In the dialog, navigate to `D:\ECE_4.2\Microprocessors II\Course project\vertical-lift-bridge\pcb\`.
4. **File name:** type `vertical_lift_pcb` (no extension; KiCad adds `.kicad_pro`).
5. **Tick "Create a new folder"** so the project lives in its own subfolder.
6. Click **Save**.
7. KiCad creates `pcb\vertical_lift_pcb\` with `vertical_lift_pcb.kicad_pro`, `vertical_lift_pcb.kicad_sch`, and `vertical_lift_pcb.kicad_pcb` inside.

> **Folder name fix:** Step 1.4.5 told the MCP server to look at `pcb/kicad-project/`. To match, rename the new folder from `vertical_lift_pcb` to `kicad-project`:
> 1. Close KiCad.
> 2. In File Explorer, rename `pcb\vertical_lift_pcb\` to `pcb\kicad-project\`.
> 3. Re-open KiCad → File → Open Project → `pcb\kicad-project\vertical_lift_pcb.kicad_pro`.

### 3.3 Set up project metadata
1. In KiCad Project Manager, top menu **File** → **Page Settings**.
2. Fill in:
   - **Title block** title: `Vertical Lift Bridge — Group 7`
   - **Date:** today (auto-fills)
   - **Revision:** `v2.1`
   - **Company:** `JKUAT EEE 2412 — Group 7`
   - **Comment 1:** `Owner: Member 5 (Ian) — PCB / Power / Safety`
   - **Comment 2:** `Designed in KiCad 10.x`
3. OK. This metadata appears on every sheet's title block.

### 3.4 Open the schematic editor and lay out the root sheet
1. In KiCad Project Manager, double-click the schematic icon (or `vertical_lift_pcb.kicad_sch` in the file list).
2. The Schematic Editor opens with one blank A4-landscape page.
3. **Verify page size:** top menu **File** → **Page Settings** → confirm "A4" (210 × 297 mm landscape) is selected. KiCad's default is fine.
4. **Add a project title label** on the page: press `T` (or top menu Place → Text). Click somewhere centre-left. In the text box type:
   ```
   Vertical Lift Bridge — Root Sheet
   Hierarchy:
     PowerSupply | ESP32Module | USBProgramming | MotorDriver
     ShiftRegLights | ConnectorsSafety | TFT_Camera
   ```
   Set text size to ~3 mm. OK.

### 3.5 Create the 7 hierarchical sub-sheets
For each sub-sheet, follow this 4-step pattern. Do all 7 now so you can see the architecture before going component-by-component.

**Pattern:**
1. In the root schematic, press **S** (or top menu Place → Hierarchical Sheet Symbol).
2. Click somewhere on the page where you want the top-left corner of the sheet box. Drag and click again to set the bottom-right corner. KiCad opens the "Sheet Properties" dialog.
3. **File name:** the sub-sheet's `.kicad_sch` filename (table below).
4. **Sheet name:** the human-readable name shown on the box (table below).
5. **Hierarchy view:** OK.

| # | File name | Sheet name | Suggested position on root |
|---|---|---|---|
| 1 | `PowerSupply.kicad_sch` | `Power Supply` | top-left |
| 2 | `ESP32Module.kicad_sch` | `ESP32 Module` | top-centre |
| 3 | `USBProgramming.kicad_sch` | `USB Programming` | top-right |
| 4 | `MotorDriver.kicad_sch` | `Motor Driver` | mid-left |
| 5 | `ShiftRegLights.kicad_sch` | `Shift-Reg Lights` | mid-centre |
| 6 | `ConnectorsSafety.kicad_sch` | `Connectors & Safety` | mid-right |
| 7 | `TFT_Camera.kicad_sch` | `TFT + Camera` | bottom-spanning |

After step 7, your root sheet looks like a 3×3 grid of labelled boxes. Save with `Ctrl+S`. KiCad asks whether to save modified files — yes to all.

### 3.6 Confirm the sub-sheets exist on disk
Open File Explorer → `pcb\kicad-project\`. You should see all 7 `.kicad_sch` files alongside `vertical_lift_pcb.kicad_sch` and `vertical_lift_pcb.kicad_pro`. If any are missing, repeat the corresponding sub-step in 3.5.

### 3.7 Set up project-wide net classes (do this BEFORE laying out components)
Net classes define trace widths and clearances for groups of nets. You'll assign nets to classes later; the classes themselves are defined now.

1. In Schematic Editor, top menu **File** → **Schematic Setup**.
2. Left panel → **Project** → **Net Classes**.
3. The "Default" class is already there. Edit its values:
   - Clearance: **0.20 mm**
   - Track width: **0.25 mm**
   - Via diameter: **0.60 mm**
   - Via drill: **0.30 mm**
4. Click the **+** to add a new class. Repeat for each of the following:

| Class name | Track width | Clearance | Via dia / drill | Used for |
|---|---|---|---|---|
| `3V3_Power` | 0.40 mm | 0.20 mm | 0.60 / 0.30 | 3.3 V rail |
| `5V_Power` | 1.00 mm | 0.25 mm | 0.80 / 0.40 | 5 V main + V5_CAM |
| `12V_Motor` | 1.50 mm | 0.30 mm | 0.80 / 0.40 | 12 V to motor (peak 2.5 A) |
| `GND_Bus` | 0.40 mm | 0.20 mm | 0.60 / 0.30 | GND returns not on pour |

You'll assign nets to classes via the lower table on the same dialog after the schematic is captured. Apply → OK.

### 3.8 Set up the PCB board outline (placeholder; refined in Step 7)
1. KiCad Project Manager → double-click PCB icon (or `.kicad_pcb` file).
2. PCB Editor opens — empty board.
3. Left toolbar → click **Layer Manager** if not visible.
4. **Switch to Edge.Cuts layer** (click "Edge.Cuts" in the layer panel).
5. Top menu **Place** → **Draw Rectangle** (or hotkey if assigned).
6. Click at coordinates **(0, 0)** then **(100, 80)** to draw the 100 × 80 mm board outline. Read coordinates from the bottom status bar.
   - If you can't click precisely, use top menu **Place** → **Add Filled Rectangle** and type coordinates manually.
7. Save with `Ctrl+S`.

### 3.9 Add 4 mounting holes (4 × M3 at corners)
Mounting holes are special "footprints" with no electrical connection.
1. Press `A` (Add Footprint).
2. Search `MountingHole`. Pick `MountingHole_3.2mm_M3` (3.2 mm hole for M3 screw with clearance).
3. Click the cursor near each corner. Place 4 instances. Use the Properties dialog (`E`) to set positions to:
   - (3, 3), (97, 3), (3, 77), (97, 77)
4. Save.

### 3.10 First commit — empty skeleton
Open Command Prompt:
```cmd
cd D:\ECE_4.2\Microprocessors II\Course project\vertical-lift-bridge
git add pcb/kicad-project/
git commit -m "feat(pcb): create new KiCad project skeleton (v2.1)"
```

---

## Step 4 — Master footprint reference (every component you'll place)

> This is the single source of truth for which KiCad library footprint each schematic symbol uses. Refer back to this table whenever you assign footprints in the schematic.

### 4.1 Symbol vs Footprint
| Concept | Lives where | Example |
|---|---|---|
| **Symbol** | KiCad symbol library (or your project library) | `Device:R` for a resistor |
| **Footprint** | KiCad footprint library | `Resistor_SMD:R_0805_2012Metric` |
| The link | A symbol's "Footprint" property points at a footprint by `library:footprint` notation | When you click a symbol → press `F`, you assign a footprint |

### 4.2 Power components

| Refdes | Symbol | Footprint | Quantity |
|---|---|---|---|
| J1 (DC barrel jack) | `Connector:Barrel_Jack` | `Connector_BarrelJack:BarrelJack_Horizontal` | 1 |
| D1 (reverse-polarity Schottky) | `Diode:SS54` | `Diode_SMD:D_SMA` | 1 |
| F1 (polyfuse 1.5 A) | `Device:Fuse` | `Fuse:Fuse_1812_4532Metric` | 1 |
| D2 (TVS clamp) | `Diode_TVS:SMBJxxA` (pick `SMBJ16A`) | `Diode_SMD:D_SMB` | 1 |
| U2 (LM2596 main 5 V) | `Regulator_Switching:LM2596S-5` | `Package_TO_SOT_SMD:TO-263-5_TabPin3` | 1 |
| U2B (LM2596 CAM 5 V) | `Regulator_Switching:LM2596S-5` | `Package_TO_SOT_SMD:TO-263-5_TabPin3` | 1 |
| L2 (33 µH inductor for U2) | `Device:L` | `Inductor_SMD:L_12x12mm_H8mm` | 1 |
| L2B (33 µH inductor for U2B) | `Device:L` | `Inductor_SMD:L_12x12mm_H8mm` | 1 |
| D3 (Schottky for U2) | `Diode:SS34` | `Diode_SMD:D_SMA` | 1 |
| D3B (Schottky for U2B) | `Diode:SS34` | `Diode_SMD:D_SMA` | 1 |
| C2 (470 µF input cap, U2) | `Device:CP` | `Capacitor_THT:CP_Radial_D8.0mm_P3.50mm` | 1 |
| C2B (470 µF input cap, U2B) | `Device:CP` | `Capacitor_THT:CP_Radial_D8.0mm_P3.50mm` | 1 |
| C3 (220 µF output cap, U2) | `Device:CP` | `Capacitor_THT:CP_Radial_D8.0mm_P3.50mm` | 1 |
| C3B (220 µF output cap, U2B) | `Device:CP` | `Capacitor_THT:CP_Radial_D8.0mm_P3.50mm` | 1 |
| U3 (AMS1117-3.3 V LDO) | `Regulator_Linear:AMS1117-3.3` | `Package_TO_SOT_SMD:SOT-223-3_TabPin2` | 1 |
| C4 (10 µF U3 input) | `Device:C` | `Capacitor_SMD:C_1206_3216Metric` | 1 |
| C5 (10 µF U3 output, REQUIRED for stability) | `Device:C` | `Capacitor_SMD:C_1206_3216Metric` | 1 |
| Power flags (one per rail) | `power:PWR_FLAG` | (no footprint — schematic-only) | 5 (V12, V5, V5_CAM, V3V3, GND) |

### 4.3 ESP32 module + decoupling

| Refdes | Symbol | Footprint | Quantity |
|---|---|---|---|
| U1 (ESP32-WROOM-32E module on female header) | `Module:ESP32-DEVKITC-32E` | `Module:ESP32-DEVKITC-32` | 1 |
| (Mating headers for DevKit) | `Connector_Generic:Conn_01x19` ×2 | `Connector_PinSocket_2.54mm:PinSocket_1x19_P2.54mm_Vertical` | 2 |
| C1 (3.3 V decoupling, ESP32) | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | 1 (0.1 µF) |
| C1B (3.3 V bulk, ESP32) | `Device:CP` | `Capacitor_SMD:CP_Elec_5x5.4` | 1 (10 µF) |
| R_pu_39 (10 kΩ pull-up on GPIO 39) | `Device:R` | `Resistor_SMD:R_0805_2012Metric` | 1 |

### 4.4 USB programming interface (CP2102N + USB-C + auto-reset)

| Refdes | Symbol | Footprint | Quantity |
|---|---|---|---|
| J2 (USB-C connector) | `Connector:USB_C_Receptacle_USB2.0_16P` | `Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12` | 1 |
| U4 (CP2102N USB-UART) | `Interface_USB:CP2102N-A02-GQFN24` | `Package_DFN_QFN:QFN-24-1EP_4x4mm_P0.5mm_EP2.4x2.4mm` | 1 |
| C6 (CP2102N decoupling 0.1 µF) | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | 1 |
| C7 (CP2102N decoupling 4.7 µF) | `Device:C` | `Capacitor_SMD:C_0805_2012Metric` | 1 |
| C8 (USB VBUS bulk 10 µF) | `Device:C` | `Capacitor_SMD:C_0805_2012Metric` | 1 |
| Q3, Q4 (auto-reset BJT pair) | `Transistor_BJT:MMBT3904` ×2 | `Package_TO_SOT_SMD:SOT-23` | 2 |
| R_q34 (12 kΩ for auto-reset) | `Device:R` ×4 | `Resistor_SMD:R_0805_2012Metric` | 4 |
| D5, D6 (USB ESD protection) | `Power_Protection:USBLC6-2SC6` (or similar) | `Package_TO_SOT_SMD:SOT-23-6` | 1 (single chip protects both lines) |

### 4.5 Motor driver interface (BTS7960 connector)

| Refdes | Symbol | Footprint | Quantity |
|---|---|---|---|
| J3 (BTS7960 8-pin logic header) | `Connector_Generic:Conn_01x08` | `Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical` | 1 |
| J3B (BTS7960 motor screw terminal) | `Connector_Generic:Conn_01x04` | `TerminalBlock_RND:TerminalBlock_RND_205-00154_1x04_P5.00mm_Horizontal` | 1 |
| C9 (motor + bulk cap 100 µF) | `Device:CP` | `Capacitor_THT:CP_Radial_D6.3mm_P2.50mm` | 1 |

> The BTS7960 module itself is **off-board** — you only solder the connector that mates with its 8-pin logic strip. The screw terminal is for the motor wires (M+, M−).

### 4.6 Shift register + traffic LEDs

| Refdes | Symbol | Footprint | Quantity |
|---|---|---|---|
| U5 (74HC595 shift register) | `74xx:74HC595` | `Package_DIP:DIP-16_W7.62mm` (with separate socket) | 1 |
| (DIP-16 socket for U5) | (no schematic symbol — populate at order) | `Package_DIP:DIP-16_W7.62mm_Socket` | 1 |
| C10 (74HC595 decoupling 0.1 µF) | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | 1 |
| D_road_R, D_marine_R | `Device:LED` ×2 | `LED_THT:LED_D3.0mm` | 2 (red) |
| D_road_Y, D_marine_Y | `Device:LED` ×2 | `LED_THT:LED_D3.0mm` | 2 (yellow) |
| D_road_G, D_marine_G | `Device:LED` ×2 | `LED_THT:LED_D3.0mm` | 2 (green) |
| R_led1..6 (220 Ω current limit per LED) | `Device:R` ×6 | `Resistor_SMD:R_0805_2012Metric` | 6 |

> The 6 LEDs may also be off-board on a panel; in that case use a 2×4 connector and run wires. For the demo, on-board is simpler.

### 4.7 Connectors + safety chain

| Refdes | Symbol | Footprint | Quantity |
|---|---|---|---|
| J4 (Servo left, 3-pin JST-XH) | `Connector_Generic:Conn_01x03` | `Connector_JST:JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical` | 1 |
| J5 (Servo right, 3-pin JST-XH) | `Connector_Generic:Conn_01x03` | `Connector_JST:JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical` | 1 |
| J9 (E-stop mushroom 2-pin) | `Connector_Generic:Conn_01x02` | `Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical` | 1 |
| J10 (Limit switches left tower 4-pin) | `Connector_Generic:Conn_01x04` | `Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical` | 1 |
| J11 (Limit switches right tower 4-pin) | `Connector_Generic:Conn_01x04` | `Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical` | 1 |
| J12 (HC-SR04 ×4, 3-pin per sensor: TRIG, ECHO, GND/VCC pair) | `Connector_Generic:Conn_01x04` ×4 | `Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical` | 4 |
| J13 (Buzzer 2-pin) | `Connector_Generic:Conn_01x02` | `Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical` | 1 |
| K1 (relay SRD-05VDC) | `Relay:SRD-05VDC-SL-C` | `Relay_THT:Relay_SPDT_Omron_G5LE-1` | 1 |
| Q1 (relay driver 2N7000) | `Transistor_FET:2N7000` | `Package_TO_SOT_THT:TO-92_Inline` | 1 |
| D4 (relay flyback diode 1N4007) | `Diode:1N4007` | `Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal` | 1 |
| R_q1 (220 Ω gate series for Q1) | `Device:R` | `Resistor_SMD:R_0805_2012Metric` | 1 |
| R_estop (10 kΩ pull-up) | `Device:R` | `Resistor_SMD:R_0805_2012Metric` | 1 |
| R_lim_pu (10 kΩ pull-up GPIO 39) | `Device:R` | `Resistor_SMD:R_0805_2012Metric` | 1 (already counted in 4.3 — don't double-add) |
| D_lim1..8 (1N4148 diodes for limit OR matrix) | `Diode:1N4148` ×8 | `Diode_SMD:D_SOD-123` | 8 |
| R_us_div_a, R_us_div_b (1k + 2k voltage divider per HC-SR04 ECHO, ×4 sensors) | `Device:R` ×8 | `Resistor_SMD:R_0805_2012Metric` | 8 (4 × 1k + 4 × 2k) |

### 4.8 TFT + Camera connectors

| Refdes | Symbol | Footprint | Quantity |
|---|---|---|---|
| J6 (TFT 14-pin header) | `Connector_Generic:Conn_01x14` | `Connector_PinHeader_2.54mm:PinHeader_1x14_P2.54mm_Vertical` | 1 |
| J7 (ESP32-CAM 4-pin JST-XH) | `Connector_Generic:Conn_01x04` | `Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical` | 1 |
| Q-BL (AO3400A backlight MOSFET) | `Transistor_FET:AO3400A` | `Package_TO_SOT_SMD:SOT-23` | 1 |
| R_qbl_g (220 Ω gate series for Q-BL) | `Device:R` | `Resistor_SMD:R_0805_2012Metric` | 1 |
| R_qbl_pd (100 kΩ gate pull-down for Q-BL) | `Device:R` | `Resistor_SMD:R_0805_2012Metric` | 1 |
| R_tft_cs (10 kΩ pull-up GPIO 15) | `Device:R` | `Resistor_SMD:R_0805_2012Metric` | 1 |
| R_tft_dc (10 kΩ pull-down GPIO 2) | `Device:R` | `Resistor_SMD:R_0805_2012Metric` | 1 |
| R_tft_miso (10 kΩ pull-up GPIO 12) | `Device:R` | `Resistor_SMD:R_0805_2012Metric` | 1 |
| C_tft_dec (0.1 µF TFT VCC decoupling) | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | 1 |
| C_tft_bulk (10 µF TFT bulk) | `Device:C` | `Capacitor_SMD:C_1206_3216Metric` | 1 |
| C_cam_dec (100 nF V5_CAM decoupling at J7) | `Device:C` | `Capacitor_SMD:C_0603_1608Metric` | 1 |
| C_cam_bulk (470 µF V5_CAM bulk at J7) | `Device:CP` | `Capacitor_SMD:CP_Elec_8x10` | 1 |
| D_cam_pwr (3 mm green LED — V5_CAM indicator) | `Device:LED` | `LED_THT:LED_D3.0mm` | 1 |
| R_cam_pwr (1 kΩ for indicator LED) | `Device:R` | `Resistor_SMD:R_0805_2012Metric` | 1 |

### 4.9 Test points (helpful for bring-up)

| Refdes | Symbol | Footprint | What it probes |
|---|---|---|---|
| TP1 | `Connector:TestPoint` | `TestPoint:TestPoint_Pad_D2.0mm` | V12 |
| TP2 | `Connector:TestPoint` | `TestPoint:TestPoint_Pad_D2.0mm` | V5_main |
| TP3 | `Connector:TestPoint` | `TestPoint:TestPoint_Pad_D2.0mm` | V5_CAM |
| TP4 | `Connector:TestPoint` | `TestPoint:TestPoint_Pad_D2.0mm` | V3V3 |
| TP5 | `Connector:TestPoint` | `TestPoint:TestPoint_Pad_D2.0mm` | GND |
| TP6 | `Connector:TestPoint` | `TestPoint:TestPoint_Pad_D2.0mm` | VPROPI (motor current sense) |

---

## Step 5 — Schematic capture (sub-sheet by sub-sheet)

For each sub-sheet, follow the same workflow:
1. From the root, double-click the sheet box to descend into it.
2. Press `A` to add symbols. Type the search term in the symbol picker; press Enter to place.
3. Place all symbols on the sheet; rotate (`R`) or mirror (`Y`) to fit.
4. Press `W` to wire them. Click on a pin, click on the destination pin or junction.
5. Press `H` to add hierarchical labels at the sheet boundary for every signal that crosses sheet borders.
6. Press `&` (or top menu Place → Add Power Port) for power nets — pick `+5V`, `+3V3`, `GND`, etc.
7. For each symbol, press `F` to assign its footprint per the Step 4 tables.
8. `Ctrl+S` to save the sheet.

### 5.1 PowerSupply.kicad_sch
This is your most safety-critical sheet. Build it carefully.

**Required symbols (use the Step 4.2 table for footprints):**
- J1 (barrel jack) → drives `V12_RAW` net.
- D1 Schottky in series → output `V12_PROT`.
- F1 polyfuse in series → output `V12_FUSED`.
- D2 TVS in shunt to GND on `V12_FUSED`.
- Net `V12` is a global label tied to `V12_FUSED`.
- U2 + L2 + D3 + C2 + C3 → 5 V main switching regulator. Output is global label `+5V`.
- U2B + L2B + D3B + C2B + C3B → 5 V CAM switching regulator. Output is global label `V5_CAM`. Both bucks share Vin from `V12`.
- U3 + C4 + C5 → 3.3 V LDO from +5V. Output is global label `+3V3`.
- 5 × `PWR_FLAG` symbols on V12, +5V, V5_CAM, +3V3, GND so ERC is happy.

**Wiring sketch (text representation):**
```
   J1.+ ── D1 ── F1 ──┬── D2 cathode ── GND (D2 anode)
                       │
                       ├── PWR_FLAG (V12)
                       │
                       ├── U2.Vin (LM2596 main)
                       │     │
                       │     └── L2 → output → C3 → +5V (PWR_FLAG)
                       │                       D3 cathode (anode to GND)
                       │
                       └── U2B.Vin (LM2596 CAM)
                             │
                             └── L2B → output → C3B → V5_CAM (PWR_FLAG)
                                                D3B cathode

   +5V ── U3.Vin ── C4 ── GND (input cap)
              U3.Vout ── C5 ── GND (output cap, REQUIRED)
              U3.Vout → +3V3 (PWR_FLAG)
```

> Take your time. Match the U2 datasheet's typical-application circuit pin-for-pin. The Vin/GND/Output pin assignments differ between LM2596 variants — confirm against the symbol you placed.

**Hierarchical labels exiting this sheet to root:** none (all power crossings use global labels).

### 5.2 ESP32Module.kicad_sch
- Place 1 × `ESP32-DEVKITC-32E` symbol. Its 38 pins represent both rows of the DevKit.
- Add C1 (0.1 µF) and C1B (10 µF) on the 3V3 pin.
- Add R_pu_39 (10 kΩ) between GPIO 39 and +3V3 (the input-only pin needs an external pull-up).
- Place hierarchical labels for **every** GPIO that exits this sheet. Label name = the canonical name from `firmware/src/pin_config.h`. Examples:
  - GPIO 25 → `MOTOR_IN1`
  - GPIO 26 → `MOTOR_IN2`
  - GPIO 34 → `MOTOR_VPROPI`
  - GPIO 35 → `DECK_POSITION`
  - GPIO 39 → `LIMIT_ANYHIT`
  - GPIO 32 → `MOTOR_RELAY`
  - GPIO 16 → `VISION_RX`
  - GPIO 17 → `VISION_TX`
  - GPIO 1 → `BUZZER` (also USB TX during programming)
  - GPIO 3 → `US4_ECHO` (also USB RX)
  - GPIO 5 → `SERVO_LEFT` (also US1_TRIG — time-multiplexed)
  - GPIO 18 → `US1_ECHO` (also 595_CLOCK)
  - GPIO 19 → `595_LATCH`
  - GPIO 21 → `595_OE_N` (also US2_ECHO)
  - GPIO 22 → `SERVO_RIGHT` (also US3_TRIG)
  - GPIO 23 → `595_DATA`
  - GPIO 0 → `ESTOP_IRQ` (also BOOT)
  - GPIO 2 → `TFT_DC` (strapping)
  - GPIO 4 → `TFT_RST`
  - GPIO 12 → `TFT_MISO` (strapping)
  - GPIO 13 → `TFT_MOSI`
  - GPIO 14 → `TFT_SCK`
  - GPIO 15 → `TFT_CS` (strapping)
  - GPIO 27 → `TFT_BL`
  - GPIO 33 → `TOUCH_CS`
  - GPIO 36 → `TOUCH_IRQ`
- VCC pin → `+3V3` global label.
- GND pins → `GND` global label.
- EN → tied via 10 kΩ pull-up to +3V3, plus a small tact reset button (`SW_Push` symbol, `Button_Switch_THT:SW_Tactile_Straight_KSA0Axx1LFTR`).

### 5.3 USBProgramming.kicad_sch
- J2 USB-C connector (16 pins; USB 2.0 only uses VBUS, GND, D+, D−, CC1, CC2).
- U4 CP2102N (QFN-24).
- D5/D6 ESD protection on D+/D− lines (`USBLC6-2SC6`).
- C6 (0.1 µF) + C7 (4.7 µF) decoupling on CP2102N's 3.3 V regulator output.
- C8 (10 µF) bulk on USB VBUS.
- Auto-reset circuit: Q3 (MMBT3904) and Q4 (MMBT3904) plus 4 × 12 kΩ resistors as per the DTR/RTS → ESP32 EN/IO0 standard pattern.
- USB CC1 and CC2 pins each through 5.1 kΩ to GND (USB-C source-detect).
- Hierarchical labels exiting: `USB_TX → BUZZER`, `USB_RX → US4_ECHO`, `USB_DTR → ESP_EN_RST`, `USB_RTS → ESP_BOOT`.

> The USB-C symbol KiCad uses requires CC1/CC2 termination. Without the 5.1 kΩ resistors, your USB-C cable won't enumerate.

### 5.4 MotorDriver.kicad_sch
- J3 (8-pin male header, mates with the BTS7960 module's 8-pin logic strip):
  - Pin 1 (RPWM) → hierarchical label `MOTOR_IN1` (from ESP32 GPIO 25)
  - Pin 2 (LPWM) → `MOTOR_IN2` (GPIO 26)
  - Pin 3 (R_EN) → +5V
  - Pin 4 (L_EN) → +5V
  - Pin 5 (R_IS) → `MOTOR_VPROPI` (GPIO 34)
  - Pin 6 (L_IS) → tied to R_IS or marked NC
  - Pin 7 (VCC) → +5V
  - Pin 8 (GND) → GND
- J3B (4-pin screw terminal, motor power):
  - Pin 1 (B+) → net `MOTOR_VPP` (this comes from the relay NO contact, see 5.6)
  - Pin 2 (B−) → GND
  - Pin 3 (M+) → screw terminal to motor wire
  - Pin 4 (M−) → screw terminal to motor wire
- C9 (100 µF) across MOTOR_VPP / GND for inrush.

### 5.5 ShiftRegLights.kicad_sch
- U5 74HC595 (DIP-16):
  - Pin 14 (SER) → `595_DATA` hier label
  - Pin 11 (SRCLK) → `595_CLOCK`
  - Pin 12 (RCLK) → `595_LATCH`
  - Pin 13 (OE̅) → `595_OE_N`
  - Pin 10 (SRCLR̅) → +5V (always enabled)
  - Pin 16 (VCC) → +5V + C10 0.1 µF decoupling
  - Pin 8 (GND) → GND
- 6 LEDs:
  - Q0 → R_led1 (220 Ω) → D_road_R anode → cathode to GND
  - Q1 → R_led2 → D_road_Y → GND
  - Q2 → R_led3 → D_road_G → GND
  - Q3 → R_led4 → D_marine_R → GND
  - Q4 → R_led5 → D_marine_Y → GND
  - Q5 → R_led6 → D_marine_G → GND
- Q6, Q7 unused — leave open.

### 5.6 ConnectorsSafety.kicad_sch

#### E-stop chain
- J9 (2-pin JST-XH for off-board mushroom button):
  - Pin 1 → `ESTOP_IRQ` (GPIO 0) **and** through R_estop (10 kΩ) to +3V3.
  - Pin 2 → GND.
- The mushroom button NC contact closes pin 1 to GND when not pressed. When pressed (NC opens), pin 1 floats up via R_estop to +3V3 (= "pressed" in firmware).

#### Relay coil drive
- Q1 (2N7000) — N-MOSFET, low-side switch on relay coil.
  - Gate (G) → R_q1 (220 Ω series) → `MOTOR_RELAY` hier label (GPIO 32).
  - Drain (D) → relay coil pin 1.
  - Source (S) → GND.
  - Add an extra 100 kΩ pull-down from gate to GND so the relay stays off if the GPIO floats during boot.
- Relay K1 (SRD-05VDC-SL-C):
  - Coil pin 1 → Q1 drain.
  - Coil pin 2 → +5V.
  - D4 (1N4007) flyback diode: cathode → +5V, anode → coil pin 1. **Verify orientation.**
  - NO contact in series between V12 and `MOTOR_VPP` (which feeds J3B B+ on the MotorDriver sheet).
  - NC contact unused.
  - Common pole tied to V12.

#### Servos
- J4 (3-pin JST-XH for left barrier servo):
  - Pin 1 → +5V
  - Pin 2 → GND
  - Pin 3 → `SERVO_LEFT` (GPIO 5)
- J5 same for right (pin 3 → `SERVO_RIGHT` GPIO 22).
- Add a 470 µF electrolytic across +5V/GND near J4 and J5 (servo inrush).

#### Limit switches (diode-OR matrix)
- J10 (left tower 4-pin JST-XH):
  - Pins 1, 2 → 2 wires of left-top KW12-3 (one to +3V3 via the diode-OR matrix, one to GND).
  - Pins 3, 4 → 2 wires of left-bottom KW12-3.
- J11 same for right tower.
- Diode-OR network using D_lim1..D_lim8 (1N4148):
  - Each switch's "signal" wire goes to anode of a 1N4148; all cathodes tied together.
  - The combined cathode node → `LIMIT_ANYHIT` (GPIO 39).
  - GPIO 39 needs an external pull-up to +3V3 — **already added in ESP32Module.kicad_sch (R_pu_39)**.

#### Buzzer
- J13 (2-pin JST-XH):
  - Pin 1 → `BUZZER` (GPIO 1, but only after boot — see pin_config.h note)
  - Pin 2 → GND

### 5.7 TFT_Camera.kicad_sch

#### J6 — TFT 14-pin header
Use `Connector_Generic:Conn_01x14`, footprint `Connector_PinHeader_2.54mm:PinHeader_1x14_P2.54mm_Vertical`.

| TFT pin | Net (hier label) | Notes |
|---|---|---|
| 1 | +5V | TFT module's VCC accepts 5 V on this pin; check silkscreen of your specific module |
| 2 | GND | |
| 3 | TFT_CS | + R_tft_cs (10 kΩ pull-up to +3V3) |
| 4 | TFT_RST | |
| 5 | TFT_DC | + R_tft_dc (10 kΩ pull-down to GND) |
| 6 | TFT_MOSI | |
| 7 | TFT_SCK | |
| 8 | TFT_LED | through Q-BL backlight MOSFET (see below) |
| 9 | TFT_MISO | + R_tft_miso (10 kΩ pull-up to +3V3) |
| 10 | TFT_SCK | (shared with pin 7) |
| 11 | TOUCH_CS | |
| 12 | TFT_MOSI | (shared with pin 6) |
| 13 | TFT_MISO | (shared with pin 9) |
| 14 | TOUCH_IRQ | |

Add C_tft_dec (0.1 µF) within 5 mm of pin 1 and C_tft_bulk (10 µF) within 15 mm.

#### Backlight driver (Q-BL)
- Q-BL (AO3400A SOT-23 N-MOSFET):
  - Gate (G) → R_qbl_g (220 Ω) → `TFT_BL` hier label (GPIO 27).
  - Drain (D) → TFT pin 8 (LED return).
  - Source (S) → GND.
  - R_qbl_pd (100 kΩ) gate pull-down to GND.

#### J7 — ESP32-CAM 4-pin JST-XH
Use `Connector_Generic:Conn_01x04`, footprint `Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical`.

| Pin | Net | Notes |
|---|---|---|
| 1 | GND | Common ground with main board |
| 2 | V5_CAM | From the dedicated LM2596 (U2B) |
| 3 | (unused, mark NC) | Reserved for future bidirectional |
| 4 | VISION_RX | Goes to ESP32 GPIO 16 (CAM TX → ESP32 UART2 RX) |

Add C_cam_dec (100 nF) within 5 mm of pin 2 and C_cam_bulk (470 µF) within 15 mm.

#### CAM POWER OK indicator LED
- D_cam_pwr (3 mm green LED) anode → R_cam_pwr (1 kΩ) → V5_CAM. Cathode → GND.
- Lights when V5_CAM is up. Useful during bring-up — no multimeter needed.

---

## Step 6 — Run ERC (Electrical Rules Check)

1. From any sheet, top menu **Inspect** → **Electrical Rules Checker** (or `Ctrl+Shift+I`).
2. Click **Run ERC**. Wait.
3. **Goal: 0 errors, 0 warnings.** If you see warnings, read each carefully — most are real bugs (dangling wires, missing power flags) but some are stylistic (e.g. an output pin connected to another output pin, which can be intentional with diode OR).

Common ERC errors and fixes:
| Error | Fix |
|---|---|
| "Pin not connected" on a symbol pin | Either wire it or place a `NoConnect` cross from Place menu |
| "Power input pin not driven by any output" on +3V3 / +5V / V5_CAM / V12 / GND | Add a `PWR_FLAG` symbol on that net |
| "Two power outputs connected to the same net" | You wired two regulators to the same output. Disconnect one. |
| "Hierarchical sheet pin not connected" on a sub-sheet | Either delete the unused hier label inside the sub-sheet, or wire the sheet symbol pin on the parent |
| "Conflicts between net names" | Two nets ended up with same name through accidental wire merge — find the offending wires and re-route |

Do not proceed to PCB layout until ERC is clean.

> **Use the MCP connector here.** Ask Claude: "Read the ERC report from `pcb/kicad-project/` and explain each remaining warning." Claude can read the report file and explain each warning in context. Apply fixes in KiCad yourself.

---

## Step 7 — PCB layout

Open the PCB editor (KiCad Project Manager → PCB icon).

### 7.1 Pull footprints from the schematic
1. Top menu **Tools** → **Update PCB from Schematic** (or `F8`).
2. In the dialog tick:
   - ☑ Re-link footprints by reference
   - ☑ Delete tracks of removed nets
   - ☑ Refill all zones
3. Click **Update PCB**.
4. All 100+ footprints appear in the lower-left as a clump. You'll move them onto the 100 × 80 mm outline.

### 7.2 Layer stack (configure once)
1. Top menu **File** → **Board Setup**.
2. Left panel → **Board Stackup**:
   - Layer count: **2**
   - Substrate: FR-4
   - Copper weight: 1 oz on both layers
   - Total board thickness: 1.6 mm
3. Left panel → **Physical Stackup** is auto-populated from above.
4. Apply.

### 7.3 Design rules (defines DRC behaviour)
1. Same dialog → **Design Rules** → **Net Classes** — confirm the 5 classes you set up in Step 3.7 are still there.
2. Apply nets to classes:
   - All nets starting `V12` → `12V_Motor` class.
   - All nets `+5V`, `V5_CAM` → `5V_Power` class.
   - All nets `+3V3` → `3V3_Power` class.
   - `GND` → `GND_Bus` class.
   - Everything else stays in `Default`.

### 7.4 Antenna keepout zone
The ESP32-WROOM-32E module's PCB antenna sits at the **top edge** of the module footprint. Required keepout per the WROOM datasheet: **15 × 15 mm minimum**, no copper, no traces, no vias under the antenna.

1. Switch to the **User.Drawings** layer (click in the Layer panel on the right).
2. Top menu **Place** → **Add Keepout Area**.
3. Click 4 corners to draw a 15 × 15 mm rectangle that will sit under the antenna position when the WROOM module is placed.
4. In the dialog: tick **Tracks**, **Vias**, **Copper pour**. OK.
5. The zone appears as hatched outline. After you place the WROOM module footprint, drag the zone to align with the antenna location.

### 7.5 Place footprints — strategy
**Big things first.** Then small things in clusters of related nets.

Suggested placement (all coordinates in mm, origin at bottom-left):

| Component | Position | Why |
|---|---|---|
| ESP32-WROOM module headers | (50, 40) — centre of board | Most pins land here; central placement minimises trace lengths |
| BTS7960 logic header (J3) | (90, 60) — right edge | Short path to motor terminal |
| BTS7960 motor terminal (J3B) | (90, 30) — right edge | Same vicinity, motor wires exit |
| K1 relay | (75, 50) — between J3 and ESP32 | Short high-current path BTS7960 ← K1 |
| LM2596 main (U2) + L2 + caps | (15, 65) — top-left | Power input area |
| LM2596 CAM (U2B) + L2B + caps | (15, 50) — left, below U2 | Same input net |
| AMS1117 (U3) + caps | (35, 70) — near ESP32's 3V3 pin | Short trace to ESP32 |
| DC barrel jack (J1) | (5, 75) — top-left edge | Cable comes in from outside |
| USB-C (J2) | (50, 78) — top-centre edge | Programming cable position |
| CP2102N (U4) | (50, 70) — directly below USB-C | Short USB lines |
| 74HC595 (U5) socket | (25, 30) — bottom-left | LEDs nearby on bottom edge |
| 6 LEDs in a row | y=10, x = 10, 20, 30, 60, 70, 80 | Visible on the board front |
| TFT 14-pin (J6) | (5, 40) — left edge | Cable to off-board TFT |
| ESP32-CAM 4-pin (J7) | (95, 75) — top-right edge | Cable to off-board CAM |
| 4 × HC-SR04 connectors (J12 ×4) | y=5, x = 15, 35, 55, 75 | Bottom edge, 4 cables to off-board sensors |
| Servo connectors (J4, J5) | (5, 15) and (5, 25) | Left edge bottom |
| E-stop (J9) | (95, 15) | Right edge, off-board mushroom |
| Limit switches (J10, J11) | (95, 30) and (95, 45) | Right edge, towers |
| Buzzer (J13) | (95, 5) | Right edge bottom |
| Test points TP1..TP6 | (5..30, 5) | Bottom-left for probing |

To place a footprint at exact coordinates, click it once to select, press `M` to move with cursor, hover near the target then press `Enter` to drop. Or `E` to open properties and type X/Y manually.

### 7.6 Route traces — the methodical way
Switch to the **F.Cu (top copper)** layer in the Layer panel.

Hotkey **X** while not in route mode → starts routing a trace from the cursor's nearest pad.

**Order of routing:**
1. **Power first.** Route V12 from J1 to U2/U2B Vin pins. Use the 12V_Motor class width (1.5 mm). Route +5V from U2 output to AMS1117 Vin and to other +5V pins. Route V5_CAM separately from U2B output to J7 only.
2. **Ground returns.** For now, route GND wires using the GND class width. You'll convert most GND to a copper pour in Step 7.7.
3. **Critical signals next:** SPI (TFT_MOSI/MISO/SCK), UART2 (VISION_RX/TX), I2C (none here). Keep them parallel, equal length, on the same layer.
4. **Then GPIO breakouts** to all the JST-XH connectors. Use Default class (0.25 mm).
5. **Finally clock signals** — 595_CLOCK, 595_LATCH should be short and direct.

**45° corners only** (KiCad enforces this if you use the standard "Route Single Track" tool — no manual 90° corners).

### 7.7 GND copper pour (both sides)
1. Switch to **F.Cu** layer.
2. Top menu **Place** → **Add Filled Zone**.
3. Click 4 corners just inside the board edge (e.g. (1,1), (99,1), (99,79), (1,79)).
4. Net: **GND**. Layer: F.Cu. Click OK.
5. Repeat on **B.Cu** layer.
6. Press **B** to refill all zones. KiCad computes the pour, leaves clearance around all non-GND nets, fills around components.
7. Add **stitch vias** every 5 mm along high-current paths to tie F.Cu and B.Cu GND together. Use top menu Place → Via, or hotkey `V` while routing.

### 7.8 Reposition the antenna keepout zone
Now that the ESP32 module footprint is placed, drag the keepout zone you made in Step 7.4 to sit directly under the antenna section of the WROOM module (top edge of the module body). Verify in the 3D viewer (`Alt+3`) that no copper appears under the antenna.

### 7.9 Run DRC (Design Rules Check)
1. Top menu **Inspect** → **Design Rules Checker** (or `F7`).
2. Tick **Test tracks against zone fills** and **Test all DRC violations**.
3. Click **Run DRC**. Wait (~30 s for a board this size).
4. **Goal: 0 errors, 0 unconnected pads.** Warnings are OK if you understand them (e.g. silkscreen overlapping a pad — cosmetic).

Common DRC errors:
| Error | Fix |
|---|---|
| Clearance violation between two tracks | Re-route one of them with more separation |
| Unconnected pad on net X | A trace is missing — find the pad in the schematic, trace it on PCB |
| Hole too close to track | Move the via or change drill size |
| Silkscreen overlapping pad | Cosmetic; move silkscreen text in the footprint or accept |

> **Use the MCP connector here.** Ask Claude: "Read the DRC report from `pcb/kicad-project/` and group the errors by category." It can also tell you which footprints have most violations so you can re-place them.

### 7.10 3D preview
**View** → **3D Viewer** (`Alt+3`).

Inspect:
- All components present and oriented correctly.
- No part hanging off the board edge.
- Mounting holes free of copper.
- Antenna keepout visible.
- Connectors at expected board edges.

Take a screenshot. Save to `docs/build_log/2026-05-XX_pcb_3d_preview.png`.

### 7.11 Commit the schematic + layout
```cmd
cd D:\ECE_4.2\Microprocessors II\Course project\vertical-lift-bridge
git add pcb/kicad-project/
git commit -m "feat(pcb): complete schematic + layout, ERC + DRC clean"
```

---

## Step 8 — Generate Gerbers + Drill files for JLCPCB

### 8.1 Plot Gerbers
1. PCB editor → top menu **File** → **Plot**.
2. **Output directory:** browse to `pcb/gerbers/`.
3. **Plot format:** Gerber.
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

### 8.2 Generate drill files
1. Same dialog → **Generate Drill Files…** button.
2. Drill file format: **Excellon**.
3. Map file format: **Gerber X2**.
4. Drill origin: **Absolute**.
5. Click **Generate Drill File**.

### 8.3 Verify Gerbers in GerbView
1. KiCad Project Manager → click the GerbView icon (4th from left in the toolbar).
2. **File** → **Open Gerber Plot Files** → multi-select all `.gbr` files in `pcb/gerbers/` → Open.
3. **File** → **Open Excellon Drill Files** → select your `.drl` file.
4. Visually compare to your layout. All layers should look familiar.

### 8.4 ZIP the folder
1. In Windows File Explorer, navigate to `pcb/gerbers/`.
2. Select all `.gbr`, `.drl` files (NOT the parent folder).
3. Right-click → "Send to" → "Compressed (zipped) folder".
4. Rename the resulting ZIP to `JLCPCB_order.zip`.

> JLCPCB needs files at the **zip root**, not inside a subfolder. If you accidentally zipped the parent folder, re-do.

---

## Step 9 — Order from JLCPCB

1. Edge → `jlcpcb.com`. Sign up (free).
2. Click **Order PCBs** in the top nav.
3. Click **Add gerber file** → upload `JLCPCB_order.zip`. Wait for the 3D preview to render.
4. Confirm the preview matches your design (board outline, holes, silkscreen).
5. **Settings:**
   | Setting | Value |
   |---|---|
   | Layers | 2 |
   | Dimensions | 100 × 80 mm (auto-detected) |
   | PCB Qty | **5** (cheapest order, gives spares) |
   | PCB Color | **Matte Black** (per project brief) |
   | Surface finish | **HASL (with lead)** — cheapest |
   | Outer copper weight | 1 oz |
   | Material | FR-4 TG130 (default) |
   | Min hole size | 0.3 mm (default) |
   | Min track / spacing | 6/6 mil (default) |
   | Stencil | No (hand-soldering) |
6. Click "Save to Cart" → "Secure Checkout".
7. Shipping: **DHL Pickup Point Nairobi** — fastest. ~7 working days.
8. Pay (international card or M-Pesa via JLCPCB's Kenya partner). Total ≈ **KES 1,900** all-in for 5 boards.

Document the order in `docs/procurement.md`:
```markdown
## JLCPCB Order — 2026-05-XX
Order #: JLC2026XXXXXX
Tracking: DHL XXXXX
ETA: 2026-05-XX
Cost: KES 1,900
Notes: First production batch from new KiCad project.
```

---

## Step 10 — Source the BOM components (during the 7-day PCB lead time)

Use this checklist while the PCB is in transit. Each item maps to a footprint from Step 4. **Cross-check against `bom/VLB_Group7_BOM.xlsx`** — every line in this checklist must have a row in the BOM.

### 10.1 Active SMD components (Pixel Electric, Luthuli Avenue, or Aliexpress with 14-day lead)
| Part | Qty | KES each | Source |
|---|---|---|---|
| LM2596S-5.0 (TO-263-5) | 2 | 220 | Pixel |
| AMS1117-3.3 (SOT-223) | 1 | 30 | Pixel |
| AO3400A (SOT-23) | 1 | 20 | Aliexpress |
| 2N7000 (TO-92) | 1 | 15 | Pixel |
| MMBT3904 (SOT-23) | 2 | 15 | Pixel |
| SS54 Schottky (SMA) | 1 | 25 | Pixel |
| SS34 Schottky (SMA) | 2 | 25 | Pixel |
| SMBJ16A TVS (SMB) | 1 | 35 | Pixel |
| 1N4007 diode (DO-41) | 1 | 5 | Pixel |
| 1N4148 diode (SOD-123) | 8 | 5 | Pixel |
| 74HC595 (DIP-16) | 1 | 80 | Pixel |
| DIP-16 socket | 1 | 30 | Pixel |
| CP2102N (QFN-24) | 1 | 200 | Aliexpress |
| USBLC6-2SC6 (SOT-23-6) | 1 | 50 | Pixel |
| 33 µH 3 A inductor (12×12 mm) | 2 | 100 | Pixel |

### 10.2 Passives (one-time investment in 0805/0603 assortment kits)
- 0805 SMD resistor assortment kit (200 each of common values 1Ω–10MΩ) — KES 800 once.
- 0805 SMD ceramic capacitor kit — KES 800 once.
- 0603 SMD ceramic capacitor kit — KES 600 once.
These last several projects.

Specific values you need (cross-reference Step 4):
- 220 Ω 0805: 8 (6 for LEDs + 1 each for Q1 and Q-BL gates)
- 1 kΩ 0805: 5 (4 for HC-SR04 dividers + 1 for cam-pwr indicator)
- 2 kΩ 0805: 4 (HC-SR04 dividers)
- 5.1 kΩ 0805: 2 (USB-C CC1/CC2)
- 10 kΩ 0805: 6 (pull-ups for ESTOP, GPIO 39, TFT_CS, TFT_MISO, plus tact reset)
- 12 kΩ 0805: 4 (auto-reset)
- 100 kΩ 0805: 2 (gate pull-downs Q1 and Q-BL)
- 0.1 µF X7R 0603: 12 (decoupling everywhere)
- 4.7 µF X7R 0805: 1 (CP2102N)
- 10 µF X5R 1206: 4 (AMS1117 in/out, TFT bulk, ESP32 bulk)

### 10.3 Electrolytics + bulk
- 100 µF / 25 V (radial, 6.3 mm): 1 (motor inrush)
- 220 µF / 16 V (radial, 8 mm): 2 (LM2596 outputs)
- 470 µF / 25 V (radial, 8 mm): 4 (LM2596 inputs ×2 + servo bulks ×2)
- 470 µF / 10 V (SMD, 8×10 mm): 1 (V5_CAM bulk at J7)

### 10.4 Through-hole connectors and switches
| Part | Qty | KES |
|---|---|---|
| DC barrel jack 5.5×2.1 mm panel-mount | 1 | 50 |
| USB-C breakout (or panel-mount USB-C) | 1 | 150 |
| Polyfuse 1.5 A 1812 SMD | 1 | 80 |
| SRD-05VDC-SL-C 5 V relay (10 A contacts) | 1 | 100 |
| 14-pin 2.54 mm female header (TFT) | 1 | 30 |
| 1×19 female header for ESP32 module | 2 | 30 |
| 4-pin tact reset button | 1 | 10 |
| JST-XH connectors with crimp housings (assorted 2/3/4-pin) | 12 | total ~400 |
| 8-pin male header for BTS7960 | 1 | 20 |
| 4-pin screw terminal block 5 mm pitch | 1 | 50 |

### 10.5 LEDs and indicators
| Part | Qty | KES each |
|---|---|---|
| 3 mm Red LED (THT) | 2 | 5 |
| 3 mm Yellow LED | 2 | 5 |
| 3 mm Green LED | 3 | 5 (2 for traffic + 1 CAM-PWR indicator) |

### 10.6 Off-board components (NOT on the PCB; documented here for completeness)
- ESP32-WROOM-32E DevKit module: 1, KES 950
- BTS7960 module (board-level driver): 1, KES 750
- ESP32-CAM AI-Thinker: 1, KES 950
- ILI9341 2.8" TFT + XPT2046 module: 1, KES 1,300
- HC-SR04 modules: 4, KES 100 each = 400
- SG90 servos: 2, KES 200 each = 400
- KW12-3 microswitches: 8, KES 50 each = 400
- 16 mm mushroom NC button (XB2-BS542): 1, KES 350
- JGA25-370 12 V geared motor: 1, KES 950 (but in M2's BOM)

**Total for M5's BOM portion: ~KES 6,400.** Cross-check against `bom/VLB_Group7_BOM.xlsx`. The full BOM (all 5 members) sums to KES 24,825.

---

## Step 11 — Soldering sequence (when boards arrive)

> **Solder lowest-profile components first** so the board sits flat through the process.

### 11.1 Bench setup
- Anti-static mat under the PCB. Wrist strap if you have one.
- Soldering iron at 350 °C (60/40) or 380 °C (lead-free).
- Apply liquid flux to the first pad area before soldering.
- Magnifier loupe under good lamp light.
- Multimeter set to continuity beep, ready.
- Solder wick for cleanup of bridges.
- Tweezers for SMD placement.

### 11.2 Soldering order
| # | Group | Components | Technique |
|---|---|---|---|
| 1 | 0603 / 0805 SMD R + C | All passives | Tin one pad, place R/C with tweezers, melt the tinned pad. Solder the other side. |
| 2 | SOT-23 transistors | AO3400A, MMBT3904 ×2, 2N7000-SMD if used | Tin one pad, place, solder other 2 pads. |
| 3 | SOT-223 / SOT-23-6 | AMS1117, USBLC6-2SC6 | Tab/centre pad first (heat sink), then signal pins. |
| 4 | TO-263-5 | LM2596 ×2 | Tab first with plenty of flux — big tab needs heat. Then 5 signal pins. |
| 5 | QFN | CP2102N | Most challenging. Apply liberal flux, place chip, drag-solder one row at a time. Wick away bridges. Inspect with loupe. |
| 6 | SMA / SMB diodes | SS54, SS34 ×2, SMBJ16A | Mind the cathode (band) orientation — silkscreen marker shows direction. |
| 7 | SOD-123 | 1N4148 ×8 | Same — cathode band matters. |
| 8 | DIP-16 socket | For 74HC595 | Through-hole. Solder corner pin first, check seating, solder remaining. **Don't insert IC yet.** |
| 9 | THT diode (1N4007) | D4 (relay flyback) | THT — cathode band toward +5V net |
| 10 | THT inductors | L2, L2B (33 µH 12×12 mm) | Drag-solder the four under-pads with extra flux |
| 11 | THT female headers | 1×19 ×2 (ESP32), 1×14 (TFT) | Solder corner pin first, check spacing fits the DevKit by dry-fitting, solder remaining. |
| 12 | THT polarised connectors | DC jack, USB-C, all JST-XH | Verify orientation against silkscreen pin 1 marker |
| 13 | THT relay | K1 SRD-05VDC | 5 pins, all THT |
| 14 | THT tact button | Reset button | 2 pairs of pins |
| 15 | THT LEDs | 6 traffic + 1 CAM-PWR | Long lead = anode = goes to +5V via resistor; short lead = cathode = GND |
| 16 | THT screw terminal | J3B (BTS7960 motor) | 4-pin block |
| 17 | Test points | TP1..TP6 | Just pads — no components, but verify they're not bridged |
| 18 | DevKit + ICs into sockets | ESP32-WROOM, 74HC595 | **Push in, do not solder.** ESP32 might need swapping. |

### 11.3 Inter-step verification
After every group, with multimeter in continuity beep mode:
- Touch probe between V12 and GND → must NOT beep (no short).
- Touch probe between V5 and GND → must NOT beep.
- Touch probe between V5_CAM and GND → must NOT beep.
- Touch probe between V3V3 and GND → must NOT beep.

If anything beeps, **stop**. Find the short with the loupe before applying any power.

---

## Step 12 — First power-up procedure (the most critical 10 minutes of the project)

> Never plug 12 V into a freshly soldered board cold. Follow this sequence exactly.

### 12.1 Pre-power continuity check
Multimeter on continuity beep:
- 12 V to GND — **must be open** (no beep).
- V5 to GND — **must be open**.
- V5_CAM to GND — **must be open**.
- V3V3 to GND — **must be open**.
- USB 5 V to GND — **must be open**.

If any beeps: short. Find it before proceeding.

### 12.2 Bench supply test, no ICs in sockets, no DevKit
1. Bench supply set to **12 V output**, **current limit 100 mA** (catches shorts safely).
2. Connect to barrel jack input.
3. Observe current draw on supply display.
4. **If current is at the 100 mA limit** → there's a short. Power off, find it.
5. **If current is < 30 mA** → power tree is healthy at idle. Proceed.

### 12.3 Verify rails with multimeter (still no DevKit)
Probe each test point:
- TP2 (V5_main) → should read **5.0 ± 0.1 V**.
- TP3 (V5_CAM) → should read **5.0 ± 0.1 V**.
- TP4 (V3V3) → should read **3.30 ± 0.05 V**.

If a rail reads 0 V:
- LM2596 — verify Vin/GND/Vout pins under loupe; broken solder = open circuit.
- AMS1117 — verify Vin pin (3) connects to V5, GND pin (1) to GND, Vout pin (2) to V3V3.
- Check inductors L2/L2B for broken solder.

If a rail reads 4.5 V instead of 5 V:
- Adjustable LM2596 — tweak trim pot until 5.0 V (this guide assumes fixed-output LM2596S-5.0; if you used adjustable, you'll have a tiny pot to turn).

If a rail oscillates (multimeter jitters):
- Missing 10 µF on AMS1117 output (C5). Add one.

### 12.4 Verify CAM POWER OK indicator
The 3 mm green LED `D_cam_pwr` should be **lit** if V5_CAM is up. If it's off but TP3 reads 5 V, the LED is wrong polarity — desolder, flip, resolder.

### 12.5 Insert ICs and DevKit
1. Power off bench supply. Wait 30 s for caps to discharge.
2. Insert 74HC595 into its DIP-16 socket. Notch matches silkscreen marker (pin 1).
3. Plug ESP32-WROOM-32E DevKit into its female headers. Verify pin 1 alignment.
4. Power on, still 100 mA limit.
5. Current should jump to ~80 mA (DevKit boots, USB enumerates).

### 12.6 Verify ESP32 enumerates over USB
1. Plug USB-C into the board's USB-C port.
2. Windows: Device Manager → Ports — should show "Silicon Labs CP210x (COM<n>)".
3. If nothing shows, the CP2102N has a soldering issue. Reflow with hot air or fresh iron tip. Re-test.

### 12.7 Flash firmware
1. PlatformIO → vlb_main → Build → Upload. (Hold BOOT button if needed.)
2. Open monitor at 115200.
3. Expected boot log (you've memorised this from M1's guide):
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

### 12.8 Raise current limit progressively
1. Once boot is clean, raise bench supply current limit to **1 A**.
2. Test motor command via M1's harness (`r ↵`). Motor should attempt to spin.
3. If everything looks clean, raise to **3 A** for full motor under-load operation.

---

## Step 13 — Layered brownout protection (no firmware monitoring in v2.1)

The v2.1 firmware does NOT monitor the rail voltages — that hardware path was removed (see `docs/known_limitations.md` L1). Brownout is now detected by:

1. **ESP32 internal brownout detector** — built-in, trips at ~2.43 V on 3.3 V rail and resets the chip. Serial shows `Brownout detector triggered`.
2. **BTS7960 built-in undervoltage lockout** — trips at ~5.5 V on driver Vcc; the H-bridge stops switching, FSM sees this as a stall fault (`FAULT_STALL`) within `MOTOR_STALL_TIMEOUT_MS` (2 s).
3. **LM2596 thermal/current limit** — protects the 5 V rail from sustained overload independently of firmware.

Bench-test the layered protection:
1. Drop the bench supply slowly from 12 V toward 0.
2. At ~10.5 V the BTS7960 should stop driving the motor; firmware logs `[fault] raised: stall`.
3. Continue dropping. At some point the ESP32 itself resets — observe the reboot banner.

---

## Step 14 — Safety firmware deep dive (already coded — your responsibility to maintain)

### 14.1 `safety/watchdog.cpp`
- **Hardware TWDT:** ESP32 Task Watchdog, 5 s panic.
- **Software watchdog:** 5 task kicks (FSM, motor, sensors, vision, main). Each task records its last-kick timestamp.
- `task_safety` polls timestamps at 20 Hz; if any > 1.5 s old → sets `FAULT_WATCHDOG` and forces SAFE state via `interlocks_force_safe()`.
- **Tune** `WATCHDOG_MAX_INTERVAL_MS` only if a task legitimately needs more headroom (default 1500 ms is conservative).

### 14.2 `safety/fault_register.cpp`
- Aggregates the 16-bit fault flag bitmask from `g_status.fault_flags`.
- Edge-detects rising any flag → posts `EVT_FAULT_RAISED` to FSM.
- Edge-detects falling all flags → posts `EVT_FAULT_CLEARED`.
- `fault_register_first_name(flags)` returns human-readable name for serial logs.
- v2.1 change: rail reads removed; rail_*_volts fields stay at -1.0f sentinel.

### 14.3 `safety/interlocks.cpp`
- E-stop ISR on `PIN_ESTOP` (CHANGE trigger, INPUT_PULLUP). HIGH = pressed (mushroom NC opens).
- Relay control: energise (HIGH on `PIN_RELAY` = GPIO 32) only if `!estop && fault_flags == 0`.
- Barrier servo control via ESP32Servo lib on `PIN_SERVO_L` / `PIN_SERVO_R`.
- Open-loop barrier timing — assumes 1 s reach time. SG90 ≈ 0.6 s for 90°.

### 14.4 Things you tune on real hardware
- ADC current calibration — `ADC_TO_MA_NUMERATOR` / `ADC_TO_MA_DENOMINATOR` in `motor_driver.cpp`. Measure motor current with a multimeter under known load (e.g. 1 A draw), compare to firmware's reported `motor_current_ma`, scale.
- Relay flyback diode polarity — orientation matters; cathode goes to +5V side of coil.
- Barrier servo angles — `BARRIER_DOWN_ANGLE = 0`, `BARRIER_UP_ANGLE = 90` in `system_types.h`. Adjust if your physical barriers don't reach the right positions.

---

## Step 15 — End-to-end safety bench test matrix

Run all 5 tests before signing off. Record each in `docs/integration_log.md`.

| # | Test | Method | Pass criteria |
|---|---|---|---|
| 1 | E-stop hard cut | Bridge mid-cycle (raising). Press mushroom button. | Motor stops within 50 ms (verify with high-speed video at 60+ fps or scope on motor terminals). FSM enters STATE_ESTOP. |
| 2 | E-stop recovery | Twist mushroom to release. Tap CLEAR on HMI. | FSM returns to STATE_IDLE. Bridge fully operable again. |
| 3 | Watchdog | Add `delay(5000)` temporarily in `task_motor`'s loop. Build + flash. | `[wdt] HUNG: motor=...` log appears. FSM forced to STATE_FAULT. Remove the delay and reflash. |
| 4 | Overcurrent | Hold the deck firmly mid-travel during a raise. | Motor current rises (visible on telemetry). At ~2200 mA, `FAULT_OVERCURRENT` raised. FSM enters STATE_FAULT. |
| 5 | Stall | Block the deck so the motor cannot move. Issue raise command. | After 2 s, `FAULT_STALL` raised. FSM enters STATE_FAULT. |

---

## Step 16 — Weekly milestones

| Week | Goal | Sign-off |
|---|---|---|
| 1 | Workstation setup. KiCad 10 + MCP installed. Old project deleted. New project skeleton created (Step 3). All 7 sub-sheets exist as empty files. | First commit `feat(pcb): create new KiCad project skeleton (v2.1)` pushed |
| 2 | All 7 sub-sheets fully captured. ERC 0 errors. | Schematic peer-reviewed by M1 via the MCP connector |
| 3 | PCB layout complete. Antenna keepout enforced. DRC 0 errors. Gerbers generated. 3D preview screenshot in build_log. | Files in `pcb/gerbers/` |
| 4 | JLCPCB order placed. BOM cross-checked against schematic. Components ordered in parallel. | Order # in `docs/procurement.md` |
| 5 | Boards arrive. SMD assembly complete. First power-up: V5 / V5_CAM / V3V3 rails verified at test points. | Voltage measurements logged |
| 6 | DevKit + peripherals plug in. Boot banner clean. ESP32-CAM gets clean V5_CAM. CAM-PWR indicator lit. | Serial log committed; CAM boots without brownout loop |
| 7 | All 5 safety bench tests pass (Step 15). | `docs/integration_log.md` updated |
| 8 | Demo polish: cable management, label connectors, photos of finished board (top + bottom + integrated into rig). | Final photos committed |

---

## Step 17 — Failure recovery cookbook

| Symptom | Most likely cause | First fix to try |
|---|---|---|
| Bench supply hits current limit at 12 V power-on | Short on V12 or 5 V rail | Power off. Visually inspect under loupe. Multimeter probe along V+ traces, listen for beep with GND probe held. |
| 5 V rail reads 4.6 V | Adjustable LM2596 trim pot mis-set | Tweak trim pot until 5.0 V output. (If you used fixed LM2596S-5.0, this shouldn't happen.) |
| 3.3 V rail oscillates (multimeter jitters) | Missing 10 µF on AMS1117 output | Add 10 µF X5R 1206 cap at output pin |
| ESP32 doesn't enumerate over USB | DevKit not seated, or CP2102N solder issue | Re-seat DevKit. Reflow CP2102N pins with extra flux + iron under loupe. |
| Relay clicks then immediately drops | Coil voltage sag during pull-in | Bump bulk cap on +5V rail to 1000 µF |
| E-stop press doesn't kill motor | Relay NC contact wired in wrong line | Verify NO contact in series with **motor V+** trace, NOT in series with the gate signal |
| Watchdog fault every few minutes | Vision task slow (UART blocking) | `WATCHDOG_MAX_INTERVAL_MS * 2` for vision (already in code — verify) |
| Buzzer silent | LEDC channel collision | Confirm ch0/1 = motor, ch2 = backlight, ch3/4 = servos, ch5 = buzzer (all unique) |
| ESP32-CAM brownout loops with rest of system | CAM still on shared 5 V | Verify J7 pin 2 net is `V5_CAM` (separate buck), not `+5V` |
| GPIO 39 reads HIGH constantly | Floating input (no internal pull-up on 34–39) | Verify external 10 kΩ pull-up to +3V3 on PCB (R_pu_39) |
| Backlight on full brightness, no PWM control | Q-BL gate floating | Verify 100 kΩ pull-down on Q-BL gate is populated |
| MCP server shows no tools in Claude Desktop | Server didn't start | Check Claude Desktop logs (Help → Show Logs). Most often a Python path issue in `claude_desktop_config.json` |
| Multiple components placed at the same coordinate | KiCad placed all unrouted footprints in one clump | Manually drag each into position per Step 7.5 |

---

## Step 18 — Completion checklist

PCB design (do these before placing the JLCPCB order):
- [ ] All 7 schematic sub-sheets fully captured (no empty sheets, no TODOs).
- [ ] Schematic ERC: 0 errors, 0 unjustified warnings.
- [ ] Every symbol has a footprint assigned (use `Tools → Footprint Assignment` if you missed any).
- [ ] All 5 power flag symbols present (V12, +5V, V5_CAM, +3V3, GND).
- [ ] PCB DRC: 0 errors, 0 unconnected pads.
- [ ] Antenna keepout zone correctly positioned under the WROOM module's antenna.
- [ ] CAM-dedicated 5 V buck (U2B + L2B + caps + diode) present and routed.
- [ ] J7 ESP32-CAM 4-pin connector with 470 µF bulk cap and 100 nF decoupling.
- [ ] AO3400A backlight MOSFET + 100 kΩ pull-down + 220 Ω gate series.
- [ ] External 10 kΩ pull-up on GPIO 39 (R_pu_39).
- [ ] CAM-PWR indicator LED visible on top side.
- [ ] All 6 test points (TP1..TP6) accessible from the top side.
- [ ] Gerbers + drill files exported. Verified in GerbView.

Bring-up:
- [ ] Power-on test log: V5 / V5_CAM / V3V3 rails within ±5 % at the test points.
- [ ] CAM-PWR indicator LED lights when V5_CAM is up.
- [ ] CAM boots without brownout loops on its dedicated rail.
- [ ] Boot banner reaches `[fsm] 8 -> 0` clean.

Safety:
- [ ] E-stop test: pressing mushroom kills motor in < 50 ms.
- [ ] Watchdog test: `delay(5000)` in motor task triggers `[wdt] HUNG: motor=...`.
- [ ] Overcurrent test: stall motor → `FAULT_OVERCURRENT` raised in < 100 ms.
- [ ] Stall test: blocked deck → `FAULT_STALL` after 2 s.
- [ ] Layered brownout test: rail drop → BTS7960 UVLO → ESP32 brownout reset (all expected to fire).

Documentation:
- [ ] `docs/05_electronics_design.md` updated to reflect the new project (link to the new schematic files, document any deviations from this guide).
- [ ] `docs/procurement.md` records JLCPCB order # + tracking + cost.
- [ ] `docs/build_log/` has photos of: bare PCB top, bare PCB bottom, populated top, populated bottom, integrated into rig, 3D preview screenshot.
- [ ] Cable strain reliefs on every external connector (TFT, CAM, motor, servos, mushroom, ultrasonics).
- [ ] BOM `bom/VLB_Group7_BOM.xlsx` updated to reflect every component you actually populated.

---

## Appendix A — KiCad MCP useful prompts

Once the MCP server is running, here are prompts that have proved useful during the build:

- **"List all symbols in the PowerSupply sheet that don't have a footprint assigned."**
  Useful right after Step 5 to catch missing footprint assignments before ERC.

- **"Compare the GPIO labels in my ESP32Module sheet to the entries in `firmware/src/pin_config.h` and report any mismatches."**
  Catches drift between schematic and firmware.

- **"Read my latest DRC report and group violations by net class."**
  Helps prioritise which traces to re-route first.

- **"What's connected to net `MOTOR_RELAY` in my PCB?"**
  Verifies wiring matches your mental model.

- **"Read the BOM spreadsheet at `bom/VLB_Group7_BOM.xlsx` and tell me which components in my schematic don't have a BOM line."**

Always apply the changes in KiCad yourself — the MCP is read-only and does not edit your project files.

---

## Appendix B — If you skip the MCP setup

The MCP integration in Step 1.4 is optional. You can complete this entire guide without it. The benefit of the MCP is faster cross-referencing (Claude reads files for you instead of you copy-pasting), but everything KiCad does, you do manually with mouse + keyboard. Skip Step 1.4 if your time is tight or if the kicad-mcp install fights you.
