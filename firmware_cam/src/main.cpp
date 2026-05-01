// ============================================================================
// firmware_cam/src/main.cpp — ESP32-CAM companion.
// Owner: M3 Cindy
//
// Captures grayscale QQVGA (160x120) frames at ~10 Hz, runs a frame-difference
// algorithm to detect motion in a configurable ROI, and emits one JSON line
// per frame to UART0 (serial pins of the AI-Thinker module's bottom header).
//
// JSON format:
//   {"t":<millis>,"v":<0|1>,"c":<0..100>,"x":<bbox.x>,"y":<bbox.y>,
//    "w":<bbox.w>,"h":<bbox.h>}
//
// Hardware: AI-Thinker ESP32-CAM, OV2640 camera, 5V supply.
// Wiring to ESP32 main board:
//   CAM TX0  -> ESP32 GPIO16 (UART2 RX)
//   CAM RX0  -> ESP32 GPIO17 (UART2 TX)   [optional, not used in v1]
//   CAM GND  -> ESP32 GND (common)
//   CAM 5V   -> 12V→5V buck output (separate from ESP32 5V to avoid brownout)
// ============================================================================
#include "esp_camera.h"
#include <Arduino.h>

// AI-Thinker pinout
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define ROI_X    20
#define ROI_Y    30
#define ROI_W   120
#define ROI_H    60
#define MOTION_THRESHOLD   24      // Per-pixel intensity delta
#define DETECT_PCT_THRESH  6       // % pixels above threshold => "vehicle"
#define DETECT_HOLD_MS     400     // Latch detection for this long

static uint8_t  s_prev[ROI_W * ROI_H];
static bool     s_have_prev = false;
static uint32_t s_last_detect_ms = 0;

void setup() {
    Serial.begin(115200);
    delay(200);

    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size   = FRAMESIZE_QQVGA;     // 160x120
    config.pixel_format = PIXFORMAT_GRAYSCALE; // 1 byte / pixel
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST;
    config.jpeg_quality = 12;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("{\"err\":\"cam_init_%d\"}\n", err);
        while (true) delay(1000);
    }
    Serial.println("{\"boot\":\"ok\"}");
}

void loop() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        delay(100);
        return;
    }

    // ROI extraction
    static uint8_t roi[ROI_W * ROI_H];
    for (int y = 0; y < ROI_H; y++) {
        const uint8_t* src = fb->buf + (ROI_Y + y) * fb->width + ROI_X;
        memcpy(roi + y * ROI_W, src, ROI_W);
    }
    esp_camera_fb_return(fb);

    bool vehicle = false;
    int  motion_pct = 0;
    int  bbox_x1 = ROI_W, bbox_y1 = ROI_H, bbox_x2 = 0, bbox_y2 = 0;

    if (s_have_prev) {
        int hits = 0;
        for (int y = 0; y < ROI_H; y++) {
            for (int x = 0; x < ROI_W; x++) {
                int idx = y * ROI_W + x;
                int d = (int)roi[idx] - (int)s_prev[idx];
                if (d < 0) d = -d;
                if (d > MOTION_THRESHOLD) {
                    hits++;
                    if (x < bbox_x1) bbox_x1 = x;
                    if (y < bbox_y1) bbox_y1 = y;
                    if (x > bbox_x2) bbox_x2 = x;
                    if (y > bbox_y2) bbox_y2 = y;
                }
            }
        }
        motion_pct = (hits * 100) / (ROI_W * ROI_H);
        if (motion_pct > DETECT_PCT_THRESH) {
            vehicle = true;
            s_last_detect_ms = millis();
        } else if (millis() - s_last_detect_ms < DETECT_HOLD_MS) {
            vehicle = true;     // Hold detection briefly
        }
    }
    memcpy(s_prev, roi, ROI_W * ROI_H);
    s_have_prev = true;

    int bbox_w = (bbox_x2 > bbox_x1) ? (bbox_x2 - bbox_x1) : 0;
    int bbox_h = (bbox_y2 > bbox_y1) ? (bbox_y2 - bbox_y1) : 0;

    Serial.printf("{\"t\":%lu,\"v\":%d,\"c\":%d,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}\n",
                  millis(), vehicle ? 1 : 0, motion_pct,
                  bbox_x1 + ROI_X, bbox_y1 + ROI_Y, bbox_w, bbox_h);

    delay(80);   // ~10 Hz cap (camera capture itself is the floor)
}
