# Member 4 — Abigael — Traffic Lights & HMI (Creative-Liberty Owner)

> Role: You're the project's **visual designer**. Traffic lights, buzzer, the TFT dashboard — the whole human-facing surface.
>
> **You have full creative liberty over the dashboard.** The firmware skeleton wires LVGL + TFT_eSPI + touch + animations + every helper you might need — what each screen *looks like* is yours to design. Your name will be on the demo.

---

## Scope of your work

| File | What you own |
|---|---|
| `firmware/src/traffic/traffic_lights.{h,cpp}` | 74HC595 LED chain — already coded, you tune timing |
| `firmware/src/traffic/buzzer.{h,cpp}` | Piezo patterns — coded, you may add new patterns |
| `firmware/src/hmi/display.{h,cpp}` | **TEMPLATE** — infrastructure done, **screens are yours** |
| `firmware/src/hmi/input.{h,cpp}` | **Stub in v2.1** — touchscreen is the sole input device (see Step 11) |
| `firmware/assets/fonts/*.c` | Drop converted Inter fonts here (LVGL converter) |
| `firmware/assets/icons/*.c` | Drop converted icons here |
| `docs/ui_design.md` | New file — sketch your screens, document choices |

---

## Step 1 — Set up your machine (Windows 11)

Follow steps 1.1–1.7 of M1's guide (VS Code, Git, Python 3.11, PlatformIO, CH340, repo). Then add:

### 1.8 Install PlatformIO library prerequisites for LVGL
PlatformIO will auto-install on first build, but to start exploring widgets immediately:
1. Open PIO Home → Libraries.
2. Search `lvgl`. Install version **9.2.2** (the project pins this version).
3. Search `TFT_eSPI`. Install latest by Bodmer.
4. Both will appear under `firmware/.pio/libdeps/vlb_main/`.

### 1.9 Install Inkscape (for icon design)
Free vector editor — best path to LVGL icons.
1. `https://inkscape.org/release/` → Windows 64-bit `.msi`.
2. Default install.

### 1.10 Bookmark these three online tools (open them in tabs and leave them open)
- **LVGL Font Converter:** `https://lvgl.io/tools/fontconverter`
- **LVGL Image Converter:** `https://lvgl.io/tools/imageconverter`
- **LVGL Widget Gallery (your inspiration board):** `https://docs.lvgl.io/master/widgets/`

---

## Step 2 — Read the template before designing

Open `firmware/src/hmi/display.cpp`. Spend an hour reading it. Pay attention to:

1. **`namespace ui_tokens`** (lines ≈ 50). Suggested colour palette. **Override freely** — these are starting points, not requirements.
2. **`screen_create_*()` functions** (5 of them). Each is a STUB with `lv_label_create()` saying "M4: design me". This is where your design lives.
3. **`refresh_active()`** (≈ line 250). Called at 5 Hz. This is where you read `s_local` (a thread-safe snapshot of `g_status`) and update your widgets.
4. **`task_hmi()`** loop (bottom). Don't change unless you really need to — the LVGL tick / event-handler / queue-drain pattern is correct as-is.
5. **`hmi_cmd_post()`**. Your touch-button callbacks call this to send commands to the FSM.

> **The infrastructure is non-negotiable** — touch read, DMA flush, async-call, mutex. Do not modify.
> **The screens are entirely yours** — layout, widgets, colours, animations, fonts.

---

## Step 3 — First flash — see the placeholder screens

1. PlatformIO panel → vlb_main → Build → Upload.
2. After "[hmi] task start (Core 1)", the TFT shows the BOOT screen for 1.5 s, then auto-switches to MAIN.
3. The placeholder MAIN screen says "MAIN (M4: design me)".
4. Tap the **on-screen "Next" button** (top-right of every screen) to cycle: MAIN → TELEMETRY → FAULTS → SETTINGS → MAIN. The v2.1 build has no hardware buttons — all input is via the XPT2046 touchscreen. (See Step 11 for why the resistor-ladder scheme was dropped.)

If the screen is blank: re-check TFT pin wiring (J6 14-pin connector — see M5's guide for pinout). The 14 pins are HSPI: MOSI/MISO/SCLK/CS/DC/RST + touch CS/IRQ + backlight + 3.3 V + GND.

### Touch calibration
The defaults `cal[5] = {318, 3505, 273, 3539, 6}` are typical for a TJCTM24028-SPI panel. If your touch is offset, run this one-shot calibration sketch in a temp file:

```cpp
#include <TFT_eSPI.h>
TFT_eSPI tft;
void setup() {
    Serial.begin(115200);
    tft.init(); tft.setRotation(0);
    uint16_t cal[5];
    tft.calibrateTouch(cal, TFT_WHITE, TFT_RED, 15);
    Serial.printf("cal = { %u, %u, %u, %u, %u };\n",
        cal[0], cal[1], cal[2], cal[3], cal[4]);
}
void loop() {}
```

Tap the four corner crosshairs. Copy the printed array into `display.cpp` line `uint16_t cal[5] = { ... };`.

For production, persist via `Preferences.h`:
```cpp
#include <Preferences.h>
Preferences prefs;
prefs.begin("vlb-tft", false);
prefs.putBytes("cal", cal, sizeof(cal));
```

---

## Step 4 — Design the BOOT screen

This is your branding moment. Suggestions (pick what excites you):

```cpp
static lv_obj_t* screen_create_boot(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(ui_tokens::BG), 0);

    // Logo image (if you create one)
    LV_IMG_DECLARE(vlb_logo);                  // declare extern from assets
    lv_obj_t* logo = lv_image_create(scr);
    lv_image_set_src(logo, &vlb_logo);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -40);

    // Group name
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "Vertical Lift Bridge\nGroup 7  |  EEE 2412");
    extern const lv_font_t inter_24;
    lv_obj_set_style_text_font(title, &inter_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(ui_tokens::TXT_PRIMARY), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 60);

    // Animated progress bar
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

## Step 5 — MAIN screen — the operator's everyday view

Bind these `s_local` fields (cached snapshot of `g_status`):

| Field | Suggested widget |
|---|---|
| `s_local.state` | Big banner label, colour-coded by state |
| `s_local.deck_position_mm` | `lv_bar` vertical, 0..DECK_HEIGHT_MAX_MM |
| `s_local.lights_road` | Three `lv_obj_t` circles (R/Y/G dots) |
| `s_local.lights_marine` | Three more dots |
| `s_local.vision.vehicle_present` | Icon + small label "Vehicle: yes/no" |
| `s_local.vision.confidence` | Tiny `lv_bar` 0..100 next to icon |
| `s_local.cycle_count` | Footer label "Cycle #N" |
| `s_local.fault_flags != 0` | Warning chip top-right (red), tap → goes to FAULTS |

State → banner colour mapping (suggested, override freely):

```cpp
const char* state_text;  uint32_t banner_colour;
switch (s_local.state) {
    case STATE_IDLE:          state_text = "ROAD OPEN";      banner_colour = ui_tokens::CLEAR; break;
    case STATE_ROAD_CLEARING: state_text = "STOP — CLEARING";  banner_colour = ui_tokens::WARN; break;
    case STATE_RAISING:       state_text = "BRIDGE RAISING";  banner_colour = ui_tokens::INFO; break;
    case STATE_RAISED_HOLD:   state_text = "BOAT PASSING";    banner_colour = ui_tokens::INFO; break;
    case STATE_LOWERING:      state_text = "BRIDGE LOWERING"; banner_colour = ui_tokens::INFO; break;
    case STATE_ROAD_OPENING:  state_text = "OPENING ROAD";    banner_colour = ui_tokens::WARN; break;
    case STATE_FAULT:         state_text = "FAULT";           banner_colour = ui_tokens::ALERT; break;
    case STATE_ESTOP:         state_text = "EMERGENCY STOP";  banner_colour = ui_tokens::ALERT; break;
    default:                  state_text = "INIT";            banner_colour = ui_tokens::TXT_MUTED; break;
}
```

Persist widget pointers as static `lv_obj_t*` inside the factory so `refresh_active()` can update them.

---

## Step 6 — TELEMETRY screen — gauges, history & counterweight simulation

This is the engineering view. It includes a **simulated dynamic counterweight panel** (already wired up as a stub in `display.cpp`) showing:

- Two vertical bars for left/right water tank levels (`s_local.counterweight.left/right.water_level_ml`)
- Pump and drain status indicators per tank (PUMP / DRAIN / IDLE)
- A "BALANCED" / "BALANCING..." status label

The counterweight simulation runs entirely in software — there are no physical water tanks. The static lead counterweights on the rig are unchanged. The simulation demonstrates how a dynamic pump-and-drain system would work, and the TFT dashboard animates it in real time.

The stub panel is already created in `screen_create_telemetry()` and refreshed in `refresh_active()`. You have full creative liberty to restyle it — change colours, use arc gauges instead of bars, add icons for pumps/valves, animate the fill/drain transitions, etc.

Additional suggested gauges:

```cpp
// Motor current arc
static lv_obj_t* arc_current = lv_arc_create(scr);
lv_arc_set_range(arc_current, 0, MOTOR_OVERCURRENT_MA);
lv_arc_set_value(arc_current, 0);
lv_obj_set_size(arc_current, 100, 100);

// Position bar
static lv_obj_t* bar_pos = lv_bar_create(scr);
lv_bar_set_range(bar_pos, 0, DECK_HEIGHT_MAX_MM);

// Sparkline of motor current over last 100 ticks
static lv_obj_t* chart = lv_chart_create(scr);
lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
lv_chart_set_point_count(chart, 100);
lv_chart_series_t* series = lv_chart_add_series(chart,
    lv_color_hex(ui_tokens::MOTOR), LV_CHART_AXIS_PRIMARY_Y);
```

In `refresh_active()`:
```cpp
lv_arc_set_value(arc_current, abs(s_local.motor_current_ma));
lv_bar_set_value(bar_pos, s_local.deck_position_mm, LV_ANIM_ON);
lv_chart_set_next_value(chart, series, abs(s_local.motor_current_ma));
```

---

## Step 7 — FAULTS screen — safety-critical clarity

Layout suggestion: scrollable list. Each entry: icon + fault name + bit number.

```cpp
struct FaultRow { uint32_t flag; const char* name; };
static const FaultRow kFaultRows[] = {
    { FAULT_OVERCURRENT,       "Motor overcurrent"  },
    { FAULT_STALL,             "Motor stalled"      },
    { FAULT_LIMIT_BOTH,        "Both limits asserted" },
    { FAULT_WATCHDOG,          "Watchdog timeout"   },
    { FAULT_VISION_LINK_LOST,  "Camera link lost"   },
    { FAULT_ULTRASONIC_FAIL,   "Ultrasonics dead"   },
    { FAULT_BARRIER_TIMEOUT,   "Barrier timeout"    },
    // FAULT_UNDERVOLT_12V was deprecated in v2.1 (rail monitoring removed) — omit from list
    // ... add more
};

// In factory:
lv_obj_t* list = lv_list_create(scr);
lv_obj_set_size(list, LV_PCT(100), LV_PCT(80));
for (auto& r : kFaultRows) {
    lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_WARNING, r.name);
    lv_obj_set_user_data(btn, (void*)(uintptr_t)r.flag);
}

// Clear-Fault button at the bottom
lv_obj_t* clear_btn = lv_btn_create(scr);
lv_obj_t* lbl = lv_label_create(clear_btn);
lv_label_set_text(lbl, "CLEAR FAULTS");
lv_obj_align(clear_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
lv_obj_add_event_cb(clear_btn, [](lv_event_t* e) {
    hmi_cmd_post(HMI_CMD_CLEAR_FAULT);
}, LV_EVENT_CLICKED, nullptr);
```

In `refresh_active()`, hide rows whose flag bit is clear:
```cpp
for (uint32_t i = 0; i < lv_obj_get_child_cnt(list); i++) {
    lv_obj_t* row = lv_obj_get_child(list, i);
    uint32_t flag = (uint32_t)(uintptr_t)lv_obj_get_user_data(row);
    if (flag == 0) continue;
    if (HAS_FAULT(s_local.fault_flags, flag))
        lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
}
```

---

## Step 8 — SETTINGS screen — operator preferences

Suggested controls:

```cpp
// Brightness slider
lv_obj_t* slider = lv_slider_create(scr);
lv_slider_set_range(slider, 10, 100);
lv_slider_set_value(slider, 80, LV_ANIM_OFF);
lv_obj_add_event_cb(slider, [](lv_event_t* e) {
    int v = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
    ledcWrite(2 /*LEDC_CH_BL*/, (1 << 13) * v / 100);
}, LV_EVENT_VALUE_CHANGED, nullptr);

// Manual mode toggle (raise/lower buttons)
lv_obj_t* btn_raise = lv_btn_create(scr);
lv_obj_t* lbl_r = lv_label_create(btn_raise);
lv_label_set_text(lbl_r, LV_SYMBOL_UP " RAISE");
lv_obj_add_event_cb(btn_raise, [](lv_event_t* e) {
    hmi_cmd_post(HMI_CMD_RAISE);
}, LV_EVENT_CLICKED, nullptr);
```

---

## Step 9 — Add custom fonts (Inter typeface)

1. Visit `https://lvgl.io/tools/fontconverter` in your browser.
2. Configure:
   - Name: `inter_24`
   - Size: 24
   - Bpp: 4
   - Range: `0x20-0x7F, 0xA0-0xFF` (Latin-1)
   - Font file: download Inter from `https://fonts.google.com/specimen/Inter`, upload `Inter-Regular.ttf`
3. Click "Convert" → downloads `inter_24.c` (~80 kB).
4. Drop file in `firmware/assets/fonts/inter_24.c`.
5. PlatformIO needs to compile it. In `platformio.ini`, add:
   ```
   build_src_filter = +<*> +<../assets/fonts/*.c>
   ```
6. In `display.cpp`:
   ```c
   extern const lv_font_t inter_24;
   ...
   lv_obj_set_style_text_font(label, &inter_24, 0);
   ```

Repeat for sizes 12, 16, 36 if you want more typography hierarchy.

---

## Step 10 — Add icons

1. Design or find PNG icons (32 × 32 px, transparent background).
2. Visit `https://lvgl.io/tools/imageconverter`.
3. Upload PNG. Output format: **CF_INDEXED_4BIT** (best size/quality balance).
4. Download generated `.c` file. Drop in `firmware/assets/icons/`.
5. Use:
   ```c
   LV_IMG_DECLARE(bridge_clear);
   lv_obj_t* img = lv_image_create(scr);
   lv_image_set_src(img, &bridge_clear);
   ```

Suggested icon set (design or source from a free icon pack — credit the source):
- `bridge_clear.png` — bridge down, road open
- `bridge_lifting.png` — bridge mid-rise
- `bridge_raised.png` — bridge fully up
- `vehicle.png` — car silhouette
- `boat.png` — boat silhouette
- `warning.png` — yellow triangle

---

## Step 11 — Front-panel buttons — REMOVED in v2.1

The original 5-button resistor-ladder ADC scheme was dropped in v2.1.
PIN_BTN_LADDER aliased to GPIO 34, which is also the BTS7960 motor current
sense (IS pin). The IS pin presents ~1 kΩ source impedance at all times,
which swamps any high-impedance resistor ladder tied to the same pin —
software multiplexing by FSM state cannot fix the impedance collision.
See `docs/known_limitations.md` (L1) for the full analysis.

**What this means for your work:**
- All operator input must be implemented as on-screen LVGL buttons.
- Add a "Next" button (top-right) on every screen to cycle MAIN → TELEMETRY → FAULTS → SETTINGS.
- Add HMI_CMD_RAISE / HMI_CMD_LOWER / HMI_CMD_HOLD / HMI_CMD_CLEAR_FAULT as on-screen buttons in the appropriate screen (RAISE/LOWER/HOLD on MAIN, CLEAR_FAULT on FAULTS).
- All call `hmi_cmd_post(HMI_CMD_*)` from their LVGL event callback — same as the existing CLEAR pattern in `screen_create_faults()`.
- `input_init()` and `input_tick()` are kept as no-op stubs and are not called from any task.

**v3 fix path:** A 74HC4051 analog mux on the next PCB revision restores the
front-panel buttons (and the rail monitoring). Cost ~KES 80, see `docs/known_limitations.md`.

---

## Step 12 — Weekly milestones

| Week | Goal | Sign-off |
|---|---|---|
| 1 | TFT lights up; placeholder screens cycle | All 5 screens visible |
| 2 | BOOT screen designed with logo + animation | Lecturer approves splash |
| 3 | MAIN screen complete with state banner + position bar | Bridge cycle is readable from across room |
| 4 | TELEMETRY screen with at least 2 gauges + 1 chart | Live values match `g_status` |
| 5 | FAULTS screen + clear button | Inducing a fault shows it; clear works |
| 6 | SETTINGS screen + 1 custom font + 3 icons | Brightness control works |
| 7 | Theme polish: animations, transitions | UI feels "designed", not breadboarded |
| 8 | Demo polish + photos for report | Lecturer demo |

---

## Step 13 — Failure recovery cookbook

| Symptom | Cause | Fix |
|---|---|---|
| Screen blank, backlight on | TFT_eSPI pins mismatch | Check `User_Setup.h` matches `pin_config.h` HSPI mapping |
| White screen, no boot text | Wrong rotation | `s_tft.setRotation(0)` for portrait, `1` for landscape |
| Touch coords off / inverted | Bad calibration | Re-run calibration sketch |
| Text shows boxes / question marks | Symbol not in font range | Re-convert font with `0x20-0xFF` range |
| Icons look pixelated | Wrong CF format | Convert as `CF_INDEXED_4BIT` (4 bpp), not `CF_INDEXED_1BIT` |
| LVGL crash on startup | Buffer too small | Increase `LV_BUF_LINES` from 40 to 80 in display.cpp |
| Stack overflow `task_hmi` | LVGL recursion deep | Increase `STK_HMI` from 8192 to 12288 in main.cpp |
| Lights blink wrong (e.g. red on green state) | 74HC595 bit map wrong | Verify wiring: Q0=R road, Q1=Y road, Q2=G road, Q3=R marine, Q4=Y marine, Q5=G marine |

---

## Completion checklist

- [ ] All 5 screens fully designed (no "design me" placeholders).
- [ ] At least 1 custom font + 3 icons in `assets/`.
- [ ] Touch calibration committed.
- [ ] Brightness slider works.
- [ ] Fault state auto-switches to FAULTS screen.
- [ ] All 6 traffic LEDs verified driving correctly through 74HC595.
- [ ] Buzzer chirps on barrier close, plays alarm on fault, urgent tone on E-stop.
- [ ] `docs/ui_design.md` includes screenshots of each screen + design rationale.
