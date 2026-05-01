// ============================================================================
// vision/vision_link.cpp — Reads JSON detection frames from ESP32-CAM via UART2.
// Owner: M3 Cindy
//
// Protocol (line-delimited JSON, 115200 8N1):
//   { "t": <ms>, "v": <0|1>, "c": <0..100>, "x":N,"y":N,"w":N,"h":N }
//
// Heartbeat: ESP32-CAM emits at >= 5 Hz even when no vehicle (v=0).
// If no frame > VISION_HEARTBEAT_TIMEOUT_MS, link is flagged lost.
// ============================================================================
#include "vision_link.h"
#include "../pin_config.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define VISION_RX_BUF 256

static HardwareSerial s_uart(2);     // UART2
static char     s_rx[VISION_RX_BUF];
static size_t   s_rx_len = 0;
static VisionStatus_t s_vs = {};

static void parse_line(const char* line, size_t len) {
    StaticJsonDocument<256> doc;
    auto err = deserializeJson(doc, line, len);
    if (err) return;
    s_vs.vehicle_present = doc["v"]   | 0;
    s_vs.confidence      = doc["c"]   | 0;
    s_vs.bbox_x          = doc["x"]   | 0;
    s_vs.bbox_y          = doc["y"]   | 0;
    s_vs.bbox_w          = doc["w"]   | 0;
    s_vs.bbox_h          = doc["h"]   | 0;
    s_vs.last_seen_ms    = millis();
    s_vs.link_ok         = 1;

    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.vision = s_vs;
        CLR_FAULT(g_status.fault_flags, FAULT_VISION_LINK_LOST);
        xSemaphoreGive(g_status_mutex);
    }

    // Edge-trigger: rising vehicle_present -> EVT_VEHICLE_DETECTED
    static uint8_t s_prev_v = 0;
    if (s_vs.vehicle_present && !s_prev_v) {
        SystemEvent_t e = EVT_VEHICLE_DETECTED;
        xQueueSend(g_event_queue, &e, 0);
    } else if (!s_vs.vehicle_present && s_prev_v) {
        SystemEvent_t e = EVT_VEHICLE_CLEARED;
        xQueueSend(g_event_queue, &e, 0);
    }
    s_prev_v = s_vs.vehicle_present;
}

void vision_link_init(void) {
    s_uart.begin(115200, SERIAL_8N1, PIN_UART2_RX, PIN_UART2_TX);
    s_uart.setRxBufferSize(512);
    Serial.println("[vision] UART2 init @ 115200");
}

void vision_link_tick(void) {
    // Drain bytes (up to 200 per call) into line buffer
    int n = 0;
    while (s_uart.available() && n++ < 200) {
        int b = s_uart.read();
        if (b < 0) break;
        if (b == '\n' || b == '\r') {
            if (s_rx_len > 0) {
                s_rx[s_rx_len] = 0;
                parse_line(s_rx, s_rx_len);
                s_rx_len = 0;
            }
        } else if (s_rx_len < VISION_RX_BUF - 1) {
            s_rx[s_rx_len++] = (char)b;
        } else {
            s_rx_len = 0;   // overflow — discard
        }
    }

    // Heartbeat check
    if (s_vs.last_seen_ms != 0 &&
        (millis() - s_vs.last_seen_ms) > VISION_HEARTBEAT_TIMEOUT_MS) {
        s_vs.link_ok = 0;
        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_status.vision.link_ok = 0;
            SET_FAULT(g_status.fault_flags, FAULT_VISION_LINK_LOST);
            xSemaphoreGive(g_status_mutex);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
}

VisionStatus_t vision_link_get(void) { return s_vs; }
