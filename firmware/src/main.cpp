// ============================================================================
// main.cpp — Vertical Lift Bridge Control System (Group 7)
// Entry point. Initialises peripherals, creates FreeRTOS tasks, mutex, queues.
// Owner: George (M1)
// ============================================================================
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>

#include "pin_config.h"
#include "system_types.h"

// Module headers
#include "fsm/fsm_engine.h"
#include "motor/motor_driver.h"
#include "sensors/ultrasonic.h"
#include "vision/vision_link.h"
#include "traffic/traffic_lights.h"
#include "traffic/buzzer.h"
#include "hmi/display.h"
#include "hmi/input.h"
#include "safety/watchdog.h"
#include "safety/fault_register.h"
#include "safety/interlocks.h"

// ---------------------------------------------------------------------------
// Globals (declared extern in system_types.h)
// ---------------------------------------------------------------------------
SharedStatus_t      g_status            = {};
SemaphoreHandle_t   g_status_mutex      = nullptr;
QueueHandle_t       g_event_queue       = nullptr;
QueueHandle_t       g_motor_cmd_queue   = nullptr;
QueueHandle_t       g_hmi_cmd_queue     = nullptr;

// ---------------------------------------------------------------------------
// Task handles — kept for stack inspection / debug.
// ---------------------------------------------------------------------------
static TaskHandle_t h_fsm           = nullptr;
static TaskHandle_t h_motor         = nullptr;
static TaskHandle_t h_sensors       = nullptr;
static TaskHandle_t h_vision        = nullptr;
static TaskHandle_t h_safety        = nullptr;
static TaskHandle_t h_hmi           = nullptr;
static TaskHandle_t h_telemetry     = nullptr;

// ---------------------------------------------------------------------------
// Task stack sizes (bytes) — tuned, then bumped 25% for headroom.
// ---------------------------------------------------------------------------
static constexpr uint32_t STK_FSM       = 4096;
static constexpr uint32_t STK_MOTOR     = 3072;
static constexpr uint32_t STK_SENSORS   = 3072;
static constexpr uint32_t STK_VISION    = 4096;
static constexpr uint32_t STK_SAFETY    = 3072;
static constexpr uint32_t STK_HMI       = 8192;     // LVGL needs space
static constexpr uint32_t STK_TELEM     = 2048;

// ---------------------------------------------------------------------------
// Task priorities — higher = more urgent. Keep safety highest.
// ---------------------------------------------------------------------------
static constexpr UBaseType_t PRI_SAFETY     = 5;
static constexpr UBaseType_t PRI_FSM        = 4;
static constexpr UBaseType_t PRI_MOTOR      = 4;
static constexpr UBaseType_t PRI_SENSORS    = 3;
static constexpr UBaseType_t PRI_VISION     = 3;
static constexpr UBaseType_t PRI_HMI        = 2;
static constexpr UBaseType_t PRI_TELEM      = 1;

// ---------------------------------------------------------------------------
// Core affinity — Core 0 reserved for safety + comms; Core 1 for HMI.
// ---------------------------------------------------------------------------
static constexpr BaseType_t CORE_0 = 0;
static constexpr BaseType_t CORE_1 = 1;

// ---------------------------------------------------------------------------
// Forward declarations of task functions.
// ---------------------------------------------------------------------------
static void task_fsm        (void* arg);
static void task_motor      (void* arg);
static void task_sensors    (void* arg);
static void task_vision     (void* arg);
static void task_safety     (void* arg);
static void task_telemetry  (void* arg);

// HMI task is implemented in hmi/display.cpp because it owns LVGL tick + flush.
extern void task_hmi(void* arg);

// ===========================================================================
// setup()
// ===========================================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n\n=== VLB Group 7 — Boot ===");
    Serial.printf("ESP32 SDK: %s | Sketch: %s\n",
                  ESP.getSdkVersion(), __DATE__);

    // -- Sync & queues -----------------------------------------------------
    g_status_mutex      = xSemaphoreCreateMutex();
    g_event_queue       = xQueueCreate(32, sizeof(SystemEvent_t));
    g_motor_cmd_queue   = xQueueCreate(8,  sizeof(MotorCommand_t));
    g_hmi_cmd_queue     = xQueueCreate(16, sizeof(uint8_t));
    configASSERT(g_status_mutex);
    configASSERT(g_event_queue);
    configASSERT(g_motor_cmd_queue);
    configASSERT(g_hmi_cmd_queue);

    // Init shared state
    g_status.state = STATE_INIT;
    g_status.uptime_ms = 0;

    // -- Watchdog ---------------------------------------------------------
    safety_watchdog_init();        // Configures hardware TWDT
    fault_register_init();
    interlocks_init();

    // -- Peripherals (all called from Core 0) ----------------------------
    motor_driver_init();
    sensors_ultrasonic_init();
    vision_link_init();
    traffic_lights_init();
    buzzer_init();

    // HMI is started inside task_hmi (must run on Core 1 for LVGL).
    Serial.println("[boot] Peripherals OK");

    // -- Task creation ----------------------------------------------------
    xTaskCreatePinnedToCore(task_safety,    "safety",   STK_SAFETY,
                            nullptr, PRI_SAFETY, &h_safety,    CORE_0);
    xTaskCreatePinnedToCore(task_fsm,       "fsm",      STK_FSM,
                            nullptr, PRI_FSM,    &h_fsm,       CORE_0);
    xTaskCreatePinnedToCore(task_motor,     "motor",    STK_MOTOR,
                            nullptr, PRI_MOTOR,  &h_motor,     CORE_0);
    xTaskCreatePinnedToCore(task_sensors,   "sensors",  STK_SENSORS,
                            nullptr, PRI_SENSORS, &h_sensors,  CORE_0);
    xTaskCreatePinnedToCore(task_vision,    "vision",   STK_VISION,
                            nullptr, PRI_VISION, &h_vision,    CORE_0);
    xTaskCreatePinnedToCore(task_telemetry, "telem",    STK_TELEM,
                            nullptr, PRI_TELEM,  &h_telemetry, CORE_0);

    // HMI task — pinned to Core 1 (Arduino's loop core).
    xTaskCreatePinnedToCore(task_hmi,       "hmi",      STK_HMI,
                            nullptr, PRI_HMI,    &h_hmi,       CORE_1);

    Serial.println("[boot] Tasks created — entering scheduler");

    // Transition to IDLE once tasks are spinning.
    SystemEvent_t e = EVT_NONE;
    xQueueSend(g_event_queue, &e, 0);
}

// ===========================================================================
// loop() — Arduino runs this on Core 1; we keep it idle.
// ===========================================================================
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
    safety_watchdog_kick_main();
}

// ===========================================================================
// task_fsm — pulls events from queue, runs FSM, dispatches motor/light cmds.
// ===========================================================================
static void task_fsm(void* arg) {
    fsm_engine_init();
    SystemEvent_t evt;
    TickType_t last_tick = xTaskGetTickCount();
    for (;;) {
        // Wait up to 100 ms for an event
        if (xQueueReceive(g_event_queue, &evt, pdMS_TO_TICKS(100)) == pdTRUE) {
            fsm_engine_handle(evt);
        } else {
            fsm_engine_handle(EVT_TICK_100MS);
        }
        safety_watchdog_kick_fsm();
        vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(20));
    }
}

// ===========================================================================
// task_motor — closed-loop on deck position; current monitoring.
// ===========================================================================
static void task_motor(void* arg) {
    MotorCommand_t cmd;
    for (;;) {
        if (xQueueReceive(g_motor_cmd_queue, &cmd, pdMS_TO_TICKS(20)) == pdTRUE) {
            motor_driver_apply(cmd);
        }
        motor_driver_tick();   // ADC read, position update, stall check
        safety_watchdog_kick_motor();
    }
}

// ===========================================================================
// task_sensors — HC-SR04 dual-beam ranging at 20 Hz.
// ===========================================================================
static void task_sensors(void* arg) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        sensors_ultrasonic_tick();
        safety_watchdog_kick_sensors();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(50));   // 20 Hz
    }
}

// ===========================================================================
// task_vision — UART2 RX from ESP32-CAM, parse JSON, update VisionStatus.
// ===========================================================================
static void task_vision(void* arg) {
    for (;;) {
        vision_link_tick();    // Blocks up to 50 ms inside on UART
        safety_watchdog_kick_vision();
    }
}

// ===========================================================================
// task_safety — evaluates fault flags, drives E-stop relay, latches faults.
// ===========================================================================
static void task_safety(void* arg) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        interlocks_evaluate();
        fault_register_evaluate();
        safety_watchdog_check_all();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(50));   // 20 Hz
    }
}

// ===========================================================================
// task_telemetry — uptime, CPU load, voltage rails. 1 Hz.
// ===========================================================================
static void task_telemetry(void* arg) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_status.uptime_ms = millis();
            // CPU load: read FreeRTOS runtime stats if enabled, else placeholder.
            xSemaphoreGive(g_status_mutex);
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
    }
}
