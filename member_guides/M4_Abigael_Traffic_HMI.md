# Member 4 — Abigael — Traffic Lights, Buzzer & HMI (Creative-Liberty Owner)

> **Your role.** You are the project's visual designer. The TFT dashboard, the traffic lights, the buzzer patterns — every human-facing surface of the bridge is yours.
>
> **You have full creative liberty over the dashboard appearance.** The firmware skeleton wires LVGL + TFT_eSPI + touch + DMA flush + thread-safe widget updates — you must not break that infrastructure. But what each screen *looks like*, what fonts are used, what colours, what animations — those are yours. Your name goes on the report's "UI design" section.
>
> **Your single deliverable.** Five fully-designed LVGL screens (BOOT / MAIN / TELEMETRY / FAULTS / SETTINGS) on the 2.8" ILI9341 TFT, with smooth screen transitions, working touch buttons, traffic light state synchronisation, and a counterweight tank animation that the lecturer can see clearly from across the room.

---

## 0. What you are building (read this once)

Three independent visual surfaces:

1. **Traffic lights (74HC595 + LEDs).** 6 LEDs total: 3 for road traffic (R/Y/G), 3 for marine traffic (R/Y/G). The state machine sets them via `traffic_lights_set_road()` / `traffic_lights_set_marine()`; you manage the 74HC595 chain and the bit map.
2. **Buzzer (LEDC ch 5).** Active piezo. Plays chirps on barrier close/open, fault pattern when in STATE_FAULT, urgent pattern when in STATE_ESTOP.
3. **TFT dashboard (LVGL on ILI9341 + XPT2046 touch).** 5 screens. Refreshed at 5 Hz from a thread-safe snapshot of `g_status`. All operator input is via the touchscreen (no hardware buttons in v2.1 — see Step 11).

---

## Scope of your work — every file you touch

| File | What it does | Status |
|---|---|---|
| `firmware/src/traffic/traffic_lights.{h,cpp}` | 74HC595 LED chain | ✅ exists — you tune timing only |
| `firmware/src/traffic/buzzer.{h,cpp}` | Piezo patterns on LEDC | ✅ exists — you may add new patterns |
| `firmware/src/hmi/display.{h,cpp}` | **TEMPLATE** — infrastructure done, screens are yours | ✅ skeleton exists, screens are TODO |
| `firmware/src/hmi/input.{h,cpp}` | Stubbed in v2.1 (touchscreen-only) | ✅ stub — leave alone |
| `firmware/assets/fonts/*.c` | Custom fonts (you generate via online tool) | ❌ create |
| `firmware/assets/icons/*.c` | Custom icons (you generate via online tool) | ❌ create |
| `docs/ui_design.md` | Sketches of each screen + design rationale | ❌ create |

**Not your work:** anything outside `hmi/` and `traffic/`. You consume `g_status` (thread-safe snapshot) and post `HmiCmd_t` events via `hmi_cmd_post()`. You don't touch the FSM, motor, sensors, or safety logic.

---

## Step 1 — Set up your machine (Windows 11)

Follow **steps 1.1 to 1.7 of M1's guide** first (VS Code, Git, Python 3.11, PlatformIO, CH340 driver, repo clone). Then add:

### 1.8 Pre-fetch LVGL libraries
PlatformIO will install LVGL on the first build, but to start exploring widgets immediately:
1. In VS Code, click the alien-head 👽 → PIO Home → Libraries (left panel).
2. Search `lvgl`. Install **lvgl by kisvegabor** version **9.2.2** (the project pins this version).
3. Search `TFT_eSPI`. Install latest by Bodmer.
4. Both will appear under `firmware\.pio\libdeps\vlb_main\`.

### 1.9 Install Inkscape (for icon design)
Free vector editor — best path to LVGL icons.
1. Edge → `inkscape.org/release/`. Download Windows 64-bit `.msi`.
2. Default install. Open it once to confirm.

### 1.10 Bookmark these online tools (open them in tabs and leave them open)
- **LVGL Font Converter:** `lvgl.io/tools/fontconverter`
- **LVGL Image Converter:** `lvgl.io/tools/imageconverter`
- **LVGL Widget Gallery (your inspiration board):** `docs.lvgl.io/master/widgets/`

---

## Step 2 — Read the template before designing (1 hour, no skipping)

Open `firmware/src/hmi/display.cpp`. Spend an hour reading it. The file structure is:

| Section | Lines | What |
|---|---|---|
| 1. Colour tokens | 60–82 | Suggested palette in `namespace ui_tokens`. Override freely. |
| 2. TFT + LVGL infrastructure | 84–180 | DMA flush, touch driver. **DO NOT TOUCH.** |
| 3. Snapshot helper | ~190 | `g_status_snapshot()` copies into `s_local` under mutex |
| 4. Screen factories | 200–330 | One function per screen — `screen_create_main()` etc. **THESE ARE YOURS.** |
| 5. Screen registry | 335–365 | `ensure_screen()` and `switch_to()` |
| 6. `refresh_active()` | 370–427 | Called at 5 Hz. **You add widget updates here.** |
| 7. Notification hooks | 432+ | FSM calls `display_notify_state_change()` |
| 8. `task_hmi()` | bottom | LVGL tick + flush loop. **DO NOT TOUCH.** |

**Critical rule (the only non-negotiable):** any call into LVGL from outside `display.cpp` (e.g. from FSM on Core 0) MUST go through `lv_async_call()`. Touching widgets directly from another task corrupts LVGL's internal state. The `display_notify_state_change()` hook already does this — preserve the pattern.

---

## Step 3 — First flash — see the placeholder screens

### 3.1 Wire the TFT (breadboard, before the PCB exists)
2.8" ILI9341 + XPT2046 module — 14 pins on a 0.1" header. Wire as follows:

| TFT pin | Label on module | ESP32 GPIO | `pin_config.h` constant |
|---|---|---|---|
| 1 | VCC | 5 V (or 3.3 V — module accepts both, check silkscreen) | — |
| 2 | GND | GND | — |
| 3 | CS | GPIO 15 | `PIN_TFT_CS` |
| 4 | RESET | GPIO 4 | `PIN_TFT_RST` |
| 5 | DC | GPIO 2 | `PIN_TFT_DC` |
| 6 | MOSI | GPIO 13 | `PIN_TFT_MOSI` |
| 7 | SCK | GPIO 14 | `PIN_TFT_SCK` |
| 8 | LED (BL) | GPIO 27 | `PIN_TFT_BL` |
| 9 | MISO | GPIO 12 | `PIN_TFT_MISO` |
| 10 | T_CLK | GPIO 14 (shared with SCK) | — |
| 11 | T_CS | GPIO 33 | `PIN_TOUCH_CS` |
| 12 | T_DIN | GPIO 13 (shared with MOSI) | — |
| 13 | T_DO | GPIO 12 (shared with MISO) | — |
| 14 | T_IRQ | GPIO 36 | `PIN_TOUCH_IRQ` |

Common pitfalls:
- **VCC voltage:** some modules accept only 3.3 V on the LED line — check silkscreen. If you connect 5 V to a 3.3-V-only LED, you fry the backlight transistor.
- **MISO floats at boot:** add a 10 kΩ pull-up from GPIO 12 to 3.3 V on breadboard. (PCB will have this.)
- **Wires too long:** keep < 30 cm. Long SPI lines pick up noise.

### 3.2 Build and upload
1. PlatformIO → vlb_main → Build → Upload.
2. After successful upload, the TFT should show a BOOT screen for 1.5 s, then auto-switch to a placeholder MAIN screen saying "MAIN (M4: design me)".
3. Tap the **on-screen "Next" button** (top-right) to cycle: MAIN → TELEMETRY → FAULTS → SETTINGS → MAIN.

> **The v2.1 build has no hardware buttons** — all input is via the XPT2046 touchscreen. (See Step 11 for why the resistor-ladder scheme was dropped.)

If the screen is blank but backlight is on:
- TFT_eSPI pin map mismatch. Check `User_Setup.h` flags in `platformio.ini` `build_flags` match the table above.

If white screen + no boot text:
- Wrong rotation. In `display.cpp` `task_hmi()` setup, change `s_tft.setRotation(0)` to `1` for landscape.

If you see `[fault] raised: tft-init` in serial:
- Module DOA or wiring wrong. Try a known-good module. Re-check VCC voltage (3.3 V vs 5 V).

### 3.3 Touch calibration (run once per panel, save the result)
The XPT2046 raw values vary between panel batches. Default `cal[5] = {318, 3505, 273, 3539, 6}` works for most TJCTM24028-SPI panels but if your touch is offset:

Create `firmware/src/hmi/cal_touch.cpp` (temporary):
```cpp
#include <TFT_eSPI.h>
TFT_eSPI tft;
void setup() {
    Serial.begin(115200);
    delay(2000);
    tft.init();
    tft.setRotation(0);
    uint16_t cal[5];
    tft.calibrateTouch(cal, TFT_WHITE, TFT_RED, 15);
    Serial.printf("uint16_t cal[5] = { %u, %u, %u, %u, %u };\n",
        cal[0], cal[1], cal[2], cal[3], cal[4]);
}
void loop() { delay(1000); }
```

**Procedure:**
1. Comment out your normal `setup()`/`loop()` temporarily, OR build this as a separate sketch.
2. Flash. Open monitor.
3. The TFT shows 4 small red crosshairs at corners. Tap each one precisely with a stylus or fingernail.
4. Monitor prints the calibration array. Copy it.
5. Paste into `display.cpp` where the calibration array is initialised.
6. Delete the temp file. Restore the normal firmware.
7. (Optional) Persist via `Preferences` so it survives flash:
   ```cpp
   #include <Preferences.h>
   Preferences prefs;
   prefs.begin("vlb-tft", false);
   prefs.putBytes("cal", cal, sizeof(cal));
   ```

---

## Step 4 — Design the BOOT screen (week 2)

This is your branding moment — the splash that lecturer sees first.

```cpp
static lv_obj_t* screen_create_boot(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(ui_tokens::BG), 0);

    // Logo image (after Step 10 you'll have a real logo)
    LV_IMG_DECLARE(vlb_logo);
    lv_obj_t* logo = lv_image_create(scr);
    lv_image_set_src(logo, &vlb_logo);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -40);

    // Group title
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "Vertical Lift Bridge\nGroup 7  |  EEE 2412");
    extern const lv_font_t inter_24;            // available after Step 9
    lv_obj_set_style_text_font(title, &inter_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(ui_tokens::TXT_PRIMARY), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 60);

    // Animated progress bar — fills 0→100% over 1.5 s
    lv_obj_t* bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 200, 6);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(bar, lv_color_hex(ui_tokens::INFO), LV_PART_INDICATOR);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_time(&a, 1500);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_bar_set_value);
    lv_anim_start(&a);

    return scr;
}
```

---

## Step 5 — Design the MAIN screen (week 3 — operator's everyday view)

Bind these `s_local` fields (cached snapshot of `g_status`) to widgets:

| `s_local` field | Suggested widget |
|---|---|
| `s_local.state` | Big banner label, colour-coded by state |
| `s_local.deck_position_mm` | Vertical `lv_bar`, range 0..`DECK_HEIGHT_MAX_MM` |
| `s_local.lights_road` | 3 small filled circles (R/Y/G dots) |
| `s_local.lights_marine` | 3 more dots |
| `s_local.vision.vehicle_present` | Icon + tiny label "Vehicle: yes/no" |
| `s_local.vision.confidence` | Tiny `lv_bar` 0..100 next to the icon |
| `s_local.cycle_count` | Footer label "Cycle #N" |
| `s_local.fault_flags != 0` | Warning chip top-right (red), tap → goes to FAULTS screen |

### 5.1 State → banner colour mapping
```cpp
const char* state_text;  uint32_t banner_colour;
switch (s_local.state) {
    case STATE_IDLE:          state_text = "ROAD OPEN";        banner_colour = ui_tokens::CLEAR; break;
    case STATE_ROAD_CLEARING: state_text = "STOP — CLEARING";  banner_colour = ui_tokens::WARN;  break;
    case STATE_RAISING:       state_text = "BRIDGE RAISING";   banner_colour = ui_tokens::INFO;  break;
    case STATE_RAISED_HOLD:   state_text = "BOAT PASSING";     banner_colour = ui_tokens::INFO;  break;
    case STATE_LOWERING:      state_text = "BRIDGE LOWERING";  banner_colour = ui_tokens::INFO;  break;
    case STATE_ROAD_OPENING:  state_text = "OPENING ROAD";     banner_colour = ui_tokens::WARN;  break;
    case STATE_FAULT:         state_text = "FAULT";            banner_colour = ui_tokens::ALERT; break;
    case STATE_ESTOP:         state_text = "EMERGENCY STOP";   banner_colour = ui_tokens::ALERT; break;
    default:                  state_text = "INIT";             banner_colour = ui_tokens::TXT_MUTED; break;
}
```

### 5.2 Persisting widget pointers across `refresh_active()` calls
Inside the screen factory, declare widgets as `static` so `refresh_active()` can update them:
```cpp
static lv_obj_t* s_main_banner = nullptr;
static lv_obj_t* s_main_pos_bar = nullptr;

static lv_obj_t* screen_create_main(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    s_main_banner = lv_label_create(scr);
    /* ... */
    s_main_pos_bar = lv_bar_create(scr);
    /* ... */
    return scr;
}
```

Then in `refresh_active()` `case HMI_SCREEN_MAIN:`:
```cpp
case HMI_SCREEN_MAIN:
    if (s_main_banner) lv_label_set_text(s_main_banner, state_text);
    if (s_main_pos_bar) lv_bar_set_value(s_main_pos_bar, s_local.deck_position_mm, LV_ANIM_ON);
    break;
```

### 5.3 Add the on-screen control buttons
On the MAIN screen, add four touch buttons:
- **RAISE** (calls `hmi_cmd_post(HMI_CMD_RAISE)` — fires `EVT_OPERATOR_RAISE` to FSM)
- **LOWER** (`HMI_CMD_LOWER`)
- **HOLD** (`HMI_CMD_HOLD`)
- **NEXT** (`HMI_CMD_NEXT_SCREEN`)

Pattern (the LVGL event callback):
```cpp
lv_obj_t* btn_raise = lv_btn_create(scr);
lv_obj_t* lbl = lv_label_create(btn_raise);
lv_label_set_text(lbl, LV_SYMBOL_UP " RAISE");
lv_obj_set_size(btn_raise, 80, 40);
lv_obj_align(btn_raise, LV_ALIGN_BOTTOM_LEFT, 10, -10);
lv_obj_add_event_cb(btn_raise, [](lv_event_t* e) {
    hmi_cmd_post(HMI_CMD_RAISE);
}, LV_EVENT_CLICKED, nullptr);
```

Repeat for LOWER, HOLD, NEXT with appropriate icons and positions.

---

## Step 6 — Design the TELEMETRY screen (week 4 — engineering view)

The telemetry screen has a **simulated dynamic counterweight panel** wired up as a stub in `display.cpp` lines 230–300. It shows:
- Two vertical bars for left/right water tank levels (`s_local.counterweight.left/right.water_level_ml`)
- Pump and drain status indicators per tank ("PUMP" / "DRAIN" / "IDLE")
- A "BALANCED" / "BALANCING..." status label

The counterweight simulation runs entirely in software (`firmware/src/counterweight/counterweight.cpp`) — no physical water tanks. The static lead counterweights on the rig are unchanged. The simulation demonstrates how a dynamic pump-and-drain system would work, and the TFT animates it in real time.

The stub panel works out of the box. **You have full creative liberty to restyle it** — change colours, use arc gauges instead of bars, add icons for pumps/valves, animate the fill/drain transitions.

### 6.1 Add motor current arc
```cpp
static lv_obj_t* s_arc_current = nullptr;
// In factory:
s_arc_current = lv_arc_create(scr);
lv_arc_set_range(s_arc_current, 0, MOTOR_OVERCURRENT_MA);
lv_arc_set_value(s_arc_current, 0);
lv_obj_set_size(s_arc_current, 100, 100);
lv_obj_align(s_arc_current, LV_ALIGN_TOP_LEFT, 10, 60);

// In refresh_active() case HMI_SCREEN_TELEMETRY:
if (s_arc_current) lv_arc_set_value(s_arc_current, abs(s_local.motor_current_ma));
```

### 6.2 Add motor-current sparkline
```cpp
static lv_obj_t* s_chart = nullptr;
static lv_chart_series_t* s_series = nullptr;

// In factory:
s_chart = lv_chart_create(scr);
lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
lv_chart_set_point_count(s_chart, 100);
s_series = lv_chart_add_series(s_chart,
    lv_color_hex(ui_tokens::MOTOR), LV_CHART_AXIS_PRIMARY_Y);
lv_obj_set_size(s_chart, 200, 80);
lv_obj_align(s_chart, LV_ALIGN_BOTTOM_MID, 0, -10);

// In refresh_active():
if (s_chart && s_series) {
    lv_chart_set_next_value(s_chart, s_series, abs(s_local.motor_current_ma));
}
```

### 6.3 Add deck-position bar
Already shown on MAIN, but add a more detailed version here with min/max overlays.

---

## Step 7 — Design the FAULTS screen (week 5 — safety-critical clarity)

### 7.1 Layout: scrollable list, one row per fault
```cpp
static lv_obj_t* s_fault_list = nullptr;

struct FaultRow { uint32_t flag; const char* name; };
static const FaultRow kFaultRows[] = {
    { FAULT_OVERCURRENT,       "Motor overcurrent"  },
    { FAULT_STALL,             "Motor stalled"      },
    { FAULT_LIMIT_BOTH,        "Both limits asserted" },
    { FAULT_POS_OUT_OF_RANGE,  "Position out of range" },
    { FAULT_WATCHDOG,          "Watchdog timeout"   },
    { FAULT_VISION_LINK_LOST,  "Camera link lost"   },
    { FAULT_ULTRASONIC_FAIL,   "Ultrasonics dead"   },
    { FAULT_TFT_INIT_FAIL,     "TFT init failed"    },
    { FAULT_TOUCH_INIT_FAIL,   "Touch init failed"  },
    { FAULT_BARRIER_TIMEOUT,   "Barrier timeout"    },
    { FAULT_RELAY_FEEDBACK,    "Relay feedback fail" },
    // FAULT_UNDERVOLT_12V deprecated in v2.1 — omit
    { FAULT_OVERTEMP,          "Overtemperature"    },
    { FAULT_CONFIG_CRC,        "Config CRC bad"     },
    { FAULT_QUEUE_OVERFLOW,    "Event queue overflow" },
    { FAULT_TASK_HANG,         "Task hung"          },
};

static lv_obj_t* screen_create_faults(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(ui_tokens::BG), 0);

    s_fault_list = lv_list_create(scr);
    lv_obj_set_size(s_fault_list, LV_PCT(100), LV_PCT(80));
    lv_obj_align(s_fault_list, LV_ALIGN_TOP_MID, 0, 5);

    for (auto& r : kFaultRows) {
        lv_obj_t* btn = lv_list_add_btn(s_fault_list, LV_SYMBOL_WARNING, r.name);
        lv_obj_set_user_data(btn, (void*)(uintptr_t)r.flag);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);   // hide initially
    }

    // Clear-Fault button at bottom
    lv_obj_t* clear_btn = lv_btn_create(scr);
    lv_obj_t* lbl = lv_label_create(clear_btn);
    lv_label_set_text(lbl, "CLEAR FAULTS");
    lv_obj_align(clear_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(clear_btn, [](lv_event_t* e) {
        hmi_cmd_post(HMI_CMD_CLEAR_FAULT);
    }, LV_EVENT_CLICKED, nullptr);

    return scr;
}
```

### 7.2 Hide/show rows in `refresh_active()`
```cpp
case HMI_SCREEN_FAULTS:
    if (s_fault_list) {
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(s_fault_list); i++) {
            lv_obj_t* row = lv_obj_get_child(s_fault_list, i);
            uint32_t flag = (uint32_t)(uintptr_t)lv_obj_get_user_data(row);
            if (flag == 0) continue;
            if (HAS_FAULT(s_local.fault_flags, flag))
                lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        }
    }
    break;
```

---

## Step 8 — Design the SETTINGS screen (week 6 — operator preferences)

```cpp
// Brightness slider
lv_obj_t* slider = lv_slider_create(scr);
lv_slider_set_range(slider, 10, 100);
lv_slider_set_value(slider, 80, LV_ANIM_OFF);
lv_obj_set_size(slider, 200, 20);
lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 60);
lv_obj_add_event_cb(slider, [](lv_event_t* e) {
    int v = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
    ledcWrite(2 /*LEDC_CH_BL*/, (1 << 13) * v / 100);
}, LV_EVENT_VALUE_CHANGED, nullptr);

// Manual mode toggle
lv_obj_t* toggle = lv_switch_create(scr);
lv_obj_align(toggle, LV_ALIGN_TOP_MID, 0, 120);
// Optional: this could enable an extra MAIN-screen panel with raw motor controls

// "About" footer
lv_obj_t* about = lv_label_create(scr);
lv_label_set_text(about,
    "Group 7  |  EEE 2412 — JKUAT\n"
    "Vertical Lift Bridge v2.1\n"
    "github.com/kiogimwenda/vertical-lift-bridge");
lv_obj_align(about, LV_ALIGN_BOTTOM_MID, 0, -10);
```

---

## Step 9 — Add custom fonts (Inter typeface)

LVGL ships with a default font that looks dated. Generate Inter:

1. Visit `lvgl.io/tools/fontconverter` in your browser.
2. Configure:
   - Name: `inter_24`
   - Size: **24** (px)
   - Bpp: **4** (smooth edges)
   - Range: `0x20-0x7F, 0xA0-0xFF` (covers ASCII + Latin-1; add `0x2200-0x22FF` if you want math symbols)
   - Font file: download Inter from `fonts.google.com/specimen/Inter`, upload `Inter-Regular.ttf`
3. Click **Convert** → downloads `inter_24.c` (~80 kB).
4. Drop the file at `firmware/assets/fonts/inter_24.c`.
5. **Tell PlatformIO to compile it.** In `platformio.ini`, add (M1 must approve this edit):
   ```
   build_src_filter = +<*> +<../assets/fonts/*.c>
   ```
6. In `display.cpp`:
   ```cpp
   extern const lv_font_t inter_24;
   /* ... */
   lv_obj_set_style_text_font(label, &inter_24, 0);
   ```

Repeat for sizes you want (12 for footers, 16 for body, 36 for banners).

---

## Step 10 — Add icons

1. Design or find PNG icons (32 × 32 px, transparent background — Inkscape can export PNGs).
2. Visit `lvgl.io/tools/imageconverter`.
3. Upload PNG. Output format: **CF_INDEXED_4BIT** (best size/quality balance).
4. Download `.c` file. Drop in `firmware/assets/icons/`.
5. Add the same PlatformIO `build_src_filter` line for icons (if not already covered).
6. Use:
   ```c
   LV_IMG_DECLARE(bridge_clear);
   lv_obj_t* img = lv_image_create(scr);
   lv_image_set_src(img, &bridge_clear);
   ```

Suggested icon set (design or source from a free icon pack — credit the source in `docs/ui_design.md`):
- `bridge_clear.png` — bridge down, road open
- `bridge_lifting.png` — bridge mid-rise
- `bridge_raised.png` — bridge fully up
- `vehicle.png` — car silhouette
- `boat.png` — boat silhouette
- `warning.png` — yellow triangle

---

## Step 11 — Front-panel buttons — REMOVED in v2.1 (read this once, then move on)

The original 5-button resistor-ladder ADC scheme was dropped in v2.1. PIN_BTN_LADDER aliased to GPIO 34, which is also the BTS7960 motor current sense (IS pin). The IS pin presents ~1 kΩ source impedance at all times, which swamps any high-impedance resistor ladder tied to the same pin — software multiplexing by FSM state cannot fix the impedance collision. See `docs/known_limitations.md` (L1) for the full analysis.

**What this means for your work:**
- All operator input must be implemented as on-screen LVGL buttons (Steps 5–8 above).
- Add a "Next" button (top-right) on every screen to cycle MAIN → TELEMETRY → FAULTS → SETTINGS.
- Add HMI_CMD_RAISE / HMI_CMD_LOWER / HMI_CMD_HOLD / HMI_CMD_CLEAR_FAULT as on-screen buttons in the appropriate screen.
- All call `hmi_cmd_post(HMI_CMD_*)` from their LVGL event callback.
- `input_init()` and `input_tick()` are kept as no-op stubs — do not touch them, do not call them.

**v3 fix path:** A 74HC4051 analog mux on the next PCB revision restores the front-panel buttons. Cost ~KES 80, see `docs/known_limitations.md`.

---

## Step 12 — Traffic lights (74HC595 + 6 LEDs)

You own this from the cable up.

### 12.1 The 74HC595 chain
A single 74HC595 shift register drives 8 outputs from 3 GPIOs. We use 6 of the 8 outputs for the traffic LEDs:

| 74HC595 output | LED | `traffic_lights_set_*()` source |
|---|---|---|
| Q0 | Road RED | road = TL_RED |
| Q1 | Road AMBER | road = TL_AMBER |
| Q2 | Road GREEN | road = TL_GREEN |
| Q3 | Marine RED | marine = TL_RED |
| Q4 | Marine AMBER | marine = TL_AMBER |
| Q5 | Marine GREEN | marine = TL_GREEN |
| Q6 / Q7 | unused | — |

74HC595 control pins:
| 595 pin | Function | ESP32 GPIO | `pin_config.h` constant |
|---|---|---|---|
| 14 (SER) | Serial data in | GPIO 23 | `PIN_595_DATA` |
| 11 (SRCLK) | Serial clock | GPIO 18 | `PIN_595_CLOCK` |
| 12 (RCLK) | Latch | GPIO 19 | `PIN_595_LATCH` |
| 13 (OE̅) | Output enable (active LOW) | GPIO 21 | `PIN_595_OE_N` (alias `PIN_595_OE`) |
| 10 (SRCLR̅) | Async clear | tied HIGH (5 V) | — |

### 12.2 Wiring on breadboard
1. Place 74HC595 (DIP-16) on breadboard.
2. Pin 16 (VCC) → 5 V.
3. Pin 8 (GND) → GND.
4. Pin 10 (SRCLR̅) → 5 V (always enabled).
5. Pin 13 (OE̅) → GPIO 21 (can also tie to GND for always-on, but PCB drives this).
6. Q0–Q5 → 220 Ω resistor → LED anode → LED cathode → GND.
7. Use **3 mm LEDs**: 2 red, 2 yellow, 2 green.

### 12.3 Verify LED behaviour
The boot log line `[lights] init OK` confirms the 595 init ran. Then drive the FSM through the cycle and watch lights:
- IDLE: Road = green, Marine = red.
- ROAD_CLEARING: Road = amber, Marine = red.
- RAISING: Road = red, Marine = red.
- RAISED_HOLD: Road = red, Marine = green.
- LOWERING: Road = red, Marine = amber.
- ROAD_OPENING: Road = amber, Marine = red.
- FAULT / ESTOP: All red.

If the lights are wrong (e.g. red lights up when amber should), your wiring is rotated by one bit position. Re-trace the Q0–Q5 wiring against the table.

### 12.4 Tuning blink rate
`traffic_lights_tick()` is called from a task that toggles `s_blink_phase` each call. To change blink rate, change the period the tick is called at in main.cpp's task loop. Default is fine.

---

## Step 13 — Buzzer patterns

Read `firmware/src/traffic/buzzer.cpp`. The buzzer has 5 public functions:
- `buzzer_init()` — once at boot.
- `buzzer_chirp(N)` — N short beeps.
- `buzzer_tone(freq, duration_ms)` — single tone.
- `buzzer_pattern_fault()` — repeating 2200 Hz beep, 250 ms on/off.
- `buzzer_pattern_estop()` — alternating 3500 / 1800 Hz urgent.
- `buzzer_off()` — stop.

These are called from `fsm_actions.cpp`:
- ROAD_CLEARING: `buzzer_chirp(2)` — two beeps to warn road traffic.
- ROAD_OPENING: `buzzer_chirp(1)` — one beep to signal "go".
- FAULT: `buzzer_pattern_fault()`.
- ESTOP: `buzzer_pattern_estop()`.
- Exit FAULT/ESTOP: `buzzer_off()`.

### 13.1 Adding a new pattern
1. In `buzzer.h`, add `void buzzer_pattern_pre_raise(void);`.
2. In `buzzer.cpp`, add `s_pattern = 3` for it, and a new branch in `buzzer_tick()`.
3. In `fsm_actions.cpp`, call it on entry to your chosen state.

---

## Step 14 — Weekly milestones

| Week | Goal | Sign-off |
|---|---|---|
| 1 | TFT lights up; placeholder screens cycle by tap | All 5 screens visible |
| 2 | BOOT screen designed with logo + animation | Lecturer approves splash |
| 3 | MAIN screen complete with state banner + position bar + on-screen RAISE/LOWER/HOLD | Bridge cycle is readable from across the room |
| 4 | TELEMETRY screen with counterweight panel restyled + ≥ 2 gauges + 1 chart | Live values match `g_status` |
| 5 | FAULTS screen + clear button. Inducing a fault auto-switches screen. | Inducing each fault shows the row; clear works |
| 6 | SETTINGS screen + 1 custom font + 3 icons. Brightness slider works. | Visual evaluation by team |
| 7 | Theme polish: animations, transitions, smooth fades | UI feels "designed", not breadboarded |
| 8 | Demo polish + screenshots for report | Lecturer demo |

---

## Step 15 — Failure recovery cookbook

| Symptom | Most likely cause | First fix to try |
|---|---|---|
| Screen blank, backlight on | TFT_eSPI pin map wrong | Check `User_Setup.h` build flags in `platformio.ini` match `pin_config.h` |
| White screen, no boot text | Wrong rotation | `s_tft.setRotation(0)` for portrait, `1` for landscape |
| Touch coords inverted/offset | Bad calibration | Re-run cal sketch from Step 3.3 |
| Touch dead | T_CS, T_IRQ, or shared SPI conflict | Verify GPIO 33 wired to T_CS, GPIO 36 to T_IRQ |
| Text shows boxes / question marks | Symbol not in font range | Re-convert font with extended range `0x20-0xFF` |
| Icons look pixelated | Wrong CF format | Convert as `CF_INDEXED_4BIT` (4 bpp), not `CF_INDEXED_1BIT` |
| LVGL crash on startup | Buffer too small | Bump `LV_BUF_LINES` from 20 to 30 in `display.cpp` (uses more RAM) |
| Stack overflow `task_hmi` | LVGL recursion deep | Increase `STK_HMI` from 8192 to 12288 in `main.cpp` |
| Lights blink wrong (e.g. red lights when amber state) | 74HC595 bit map rotated | Verify wiring: Q0=R road, Q1=Y road, Q2=G road, Q3=R marine, Q4=Y marine, Q5=G marine |
| Buzzer silent | LEDC channel collision | Confirm ch0/1 = motor, ch2 = backlight, ch3/4 = servos, ch5 = buzzer (all unique) |
| Touch button fires twice | Double-tap detection | Add `if (last_tap_ms + 200 > millis()) return;` in event callback |
| FAULTS screen scrolling laggy | Refresh rate too high | Change `refresh_active()` rate from 5 Hz to 2 Hz when on FAULTS |

---

## Completion checklist

Visual design:
- [ ] All 5 screens fully designed (no "design me" placeholders).
- [ ] At least 1 custom font + 3 icons in `firmware/assets/`.
- [ ] Touch calibration committed (or persisted to Preferences).
- [ ] Brightness slider works on SETTINGS.
- [ ] Fault state auto-switches to FAULTS screen.

Functional:
- [ ] On-screen RAISE / LOWER / HOLD buttons trigger FSM transitions (M1 confirms).
- [ ] CLEAR FAULTS button clears `g_status.fault_flags` (M5 confirms).
- [ ] All 6 traffic LEDs verified driving correctly through 74HC595.
- [ ] Buzzer chirps on barrier close, plays alarm on fault, urgent tone on E-stop.
- [ ] Screen swipes/taps don't drop characters or skip events (touch debounced).

Documentation:
- [ ] `docs/ui_design.md` includes screenshots of each screen.
- [ ] Design rationale documented (why these colours, why these layouts).
- [ ] Source attributions for any icon packs used.
