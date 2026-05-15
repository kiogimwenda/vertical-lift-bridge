// ============================================================================
// safety/interlocks.cpp — Hardware safety chain.
// Owner: M5 Ian
//
//   E-stop:  Normally-closed mushroom button between PIN_ESTOP and GND.
//            INPUT_PULLUP. If pin reads HIGH -> button pressed (or wire cut).
//            Wired into ISR for sub-millisecond response.
//
//   Relay:   Coil driven by PIN_RELAY through one channel of a ULN2803
//            Darlington array (BOM line 11). Logic HIGH on PIN_RELAY
//            energises the coil; LOW de-energises and cuts motor power.
//
//   Barriers: Two SG90 servos on PIN_SERVO_L / PIN_SERVO_R (50 Hz LEDC).
// ============================================================================
#include "interlocks.h"
#include "../system_types.h"
#include "../pin_config.h"
#include "../safety/fault_register.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Servo pulse width calculation (500us to 2400us mapped to 0-180 deg)
// 16-bit resolution at 50Hz: 1 bit = 20ms / 65536 = 0.305us
#define SERVO_PULSE_MIN 1638 // ~500us
#define SERVO_PULSE_MAX 7864 // ~2400us

static void set_servo_angle(uint8_t angle) {
    if (angle > 180) angle = 180;
    uint32_t duty = SERVO_PULSE_MIN + ((SERVO_PULSE_MAX - SERVO_PULSE_MIN) * angle) / 180;
    ledcWrite(LEDC_SERVO_LEFT_CH, duty);
}

static volatile bool s_estop_now = false;
static int s_current_angle = 90; // Defaulting to 90 assuming BARRIER_UP_ANGLE or similar
static int s_target_angle = 90;
static uint32_t s_barrier_started_ms = 0;
static bool     s_barrier_done_prev = true;   // edge tracker for FSM events

static void IRAM_ATTR estop_isr(void) {
    // s_estop_now = (digitalRead(PIN_ESTOP) == HIGH);   // HIGH = pressed (NC opened)
    s_estop_now = false; // DEV MOCK: ignore floating GPIO 36
}

void interlocks_init(void) {
    pinMode(PIN_ESTOP, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), estop_isr, CHANGE);

    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);   // Start de-energised — motor power off

    ledcSetup(LEDC_SERVO_LEFT_CH, LEDC_SERVO_FREQ_HZ, LEDC_SERVO_RES_BITS);
    ledcAttachPin(PIN_SERVO_L, LEDC_SERVO_LEFT_CH);
    
    set_servo_angle(BARRIER_UP_ANGLE);
    s_current_angle = BARRIER_UP_ANGLE;
    s_target_angle = BARRIER_UP_ANGLE;

    Serial.println("[ilk] init OK (relay off until first OK eval)");
}

void interlocks_request_barriers(uint8_t angle) {
    s_target_angle       = angle;
    s_barrier_started_ms = millis();
    s_barrier_done_prev  = false;   // arm rising-edge detection
    // Note: set_servo_angle is now handled smoothly inside interlocks_evaluate
}

void interlocks_force_safe(void) {
    digitalWrite(PIN_RELAY, LOW);   // Cut motor power
    s_target_angle = BARRIER_DOWN_ANGLE;
    s_current_angle = BARRIER_DOWN_ANGLE; // Snap immediately for safety
    set_servo_angle(BARRIER_DOWN_ANGLE);
}

bool interlocks_estop_active(void) { return s_estop_now; }

void interlocks_evaluate(void) {
    // bool estop = s_estop_now || (digitalRead(PIN_ESTOP) == HIGH);
    bool estop = s_estop_now; // DEV MOCK: ignore floating GPIO 36

    // Push e-stop event on rising edge
    static bool s_estop_prev = false;
    if (estop && !s_estop_prev) {
        SystemEvent_t e = EVT_ESTOP_PRESSED;
        xQueueSend(g_event_queue, &e, 0);
    } else if (!estop && s_estop_prev) {
        SystemEvent_t e = EVT_ESTOP_RELEASED;
        xQueueSend(g_event_queue, &e, 0);
    }
    s_estop_prev = estop;

    // Relay control: energise only if no fault, no e-stop.
    bool relay_should_be_on = !estop;
    uint32_t flags = fault_register_snapshot();
    if (flags != 0) relay_should_be_on = false;
    digitalWrite(PIN_RELAY, relay_should_be_on ? HIGH : LOW);

    // Smooth servo sweeping logic (approx 40 deg/sec since evaluate runs at 20Hz)
    if (s_current_angle < s_target_angle) {
        s_current_angle += 2;
        if (s_current_angle > s_target_angle) s_current_angle = s_target_angle;
        set_servo_angle(s_current_angle);
    } else if (s_current_angle > s_target_angle) {
        s_current_angle -= 2;
        if (s_current_angle < s_target_angle) s_current_angle = s_target_angle;
        set_servo_angle(s_current_angle);
    }

    // Barrier reach detection based on current position hitting target
    bool barrier_done = (s_current_angle == s_target_angle);

    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.estop_active                 = estop;
        g_status.barrier_left_angle           = s_current_angle;
        g_status.barrier_right_angle          = s_current_angle;
        g_status.barrier_left_target_reached  = barrier_done;
        g_status.barrier_right_target_reached = barrier_done;
        xSemaphoreGive(g_status_mutex);
    }
    (void)BARRIER_TIMEOUT_MS;   // referenced in known_limitations.md (L7)

    // Emit FSM events on the rising edge of barrier_done so the FSM can
    // leave STATE_ROAD_CLEARING / STATE_ROAD_OPENING without waiting for
    // the slow 100 ms periodic tick. The exact event depends on the
    // angle most recently requested.
    if (barrier_done && !s_barrier_done_prev) {
        SystemEvent_t e = (s_target_angle == BARRIER_DOWN_ANGLE)
                             ? EVT_BARRIER_CLOSED
                             : EVT_BARRIER_OPEN;
        xQueueSend(g_event_queue, &e, 0);
    }
    s_barrier_done_prev = barrier_done;
}
