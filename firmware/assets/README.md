# firmware/assets/

This folder holds compiled C arrays for fonts and icons consumed by LVGL.

## fonts/

Drop converted .c files here. Generate them with:
https://lvgl.io/tools/fontconverter

Recommended: **Inter** (free, Google Fonts). Sizes 12 / 16 / 24 / 36.
Output settings:
  - Bpp: 4 (anti-aliased)
  - Format: C array
  - Symbols: 0x0020-0x007F + any extras you need

After downloading the .c file, add it to `platformio.ini` build_src_filter
(or just place it under `src/` so PlatformIO compiles it automatically).

In `display.cpp`, declare extern and use:
```c
extern const lv_font_t inter_24;
lv_obj_set_style_text_font(label, &inter_24, 0);
```

## icons/

Drop converted .c files here. Generate with:
https://lvgl.io/tools/imageconverter

Recommended: 32×32, 4-bit indexed (CF_INDEXED_4BIT). Suggested set:
  - bridge_clear.png, bridge_vehicle.png, bridge_lifting.png,
    bridge_lowering.png, motor_active.png, camera_active.png, warning.png

In `display.cpp`:
```c
LV_IMG_DECLARE(bridge_clear);
lv_obj_t* img = lv_image_create(parent);
lv_image_set_src(img, &bridge_clear);
```

> M4: this is your creative space. Branding, colors, and animations are
> entirely up to you.
