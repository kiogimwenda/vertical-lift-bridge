#include <SDL2/SDL.h>
#include <lvgl.h>
#include <unistd.h>
#include <stdio.h>
#include "system_types.h"

// --- Mock Shared Status ---
SharedStatus_t g_status = {};

// --- SDL2 Display and Input Handlers ---
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SDL_HOR_RES * SDL_VER_RES / 10];

static void hal_init(void);
void ui_init(void);
void ui_tick(void);

int main(int argc, char** argv) {
    lv_init();
    hal_init();

    // Default status
    g_status.state = STATE_IDLE;
    g_status.hmi_brightness = 80;
    g_status.deck_position_mm = 0;
    g_status.motor_current_ma = 0;

    ui_init();

    while (1) {
        lv_timer_handler();
        ui_tick();
        usleep(5000); // 5ms
    }

    return 0;
}

// --- Mock Hardware Logic ---
void ui_tick() {
    static uint32_t last_tick = 0;
    if (SDL_GetTicks() - last_tick < 50) return;
    last_tick = SDL_GetTicks();

    g_status.uptime_ms = last_tick;

    // Simulate bridge movement
    if (g_status.state == STATE_RAISING) {
        if (g_status.deck_position_mm < DECK_HEIGHT_MAX_MM) {
            g_status.deck_position_mm += 2;
            g_status.motor_current_ma = 650 + (rand() % 50);
        } else {
            g_status.state = STATE_RAISED_HOLD;
            g_status.top_limit_hit = true;
            g_status.motor_current_ma = 0;
        }
    } else if (g_status.state == STATE_LOWERING) {
        if (g_status.deck_position_mm > 0) {
            g_status.deck_position_mm -= 3;
            g_status.motor_current_ma = 400 + (rand() % 30);
        } else {
            g_status.state = STATE_ROAD_OPENING;
            g_status.bottom_limit_hit = true;
            g_status.motor_current_ma = 0;
        }
    }

    // Mock sensors
    g_status.vision.vehicle_present = (rand() % 100 > 95);
    g_status.ultrasonic.distance_us1_cm = 50 + (rand() % 10);
}

// --- Display / Input HAL ---
static void hal_init(void) {
    // Note: In a real native SDL setup with LVGL 8.3.11, 
    // we would use the SDL driver provided by LVGL.
    // For this mock, we assume the environment is set up for SDL.
}

// UI implementation goes in a separate file for clarity
