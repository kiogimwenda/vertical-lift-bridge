// ============================================================================
// safety/interlocks.cpp — Hardware safety chain.
// Owner: M5 Ian
//
//   E-stop:  Normally-closed mushroom button between PIN_ESTOP and GND.
//            INPUT_PULLUP. If pin reads HIGH -> button pressed (or wire cut).
//            Wired into ISR for sub-millisecond response.
//
//   Relay:   Coil driven by PIN_RELAY through 2N7000 low-side switch.
//            Energised = motor power enabled. De-energised = power cut.
//
//   Barriers: Two SG90 servos on PIN_SERVO_L / PIN_SERVO_R (50 Hz LEDC).
// ============================================================================
#include "interlocks.h"
#include "../system_types.h"
#include "../pin_config.h"
#include "../safety/fault_register.h"
#include <Arduino.h>
#include <ESP32Servo.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static Servo s_servo_l;
static Servo s_servo_r;

static volatile bool s_estop_now = false;
static uint8_t s_target_angle = BARRIER_DOWN_ANGLE;
static uint32_t s_barrier_started_ms = 0;

static void IRAM_ATTR estop_isr(void) {
    s_estop_now = (digitalRead(PIN_ESTOP) == HIGH);   // HIGH = pressed (NC opened)
}

void interlocks_init(void) {
    pinMode(PIN_ESTOP, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), estop_isr, CHANGE);

    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);   // Start de-energised — motor power off

    s_servo_l.setPeriodHertz(50);
    s_servo_r.setPeriodHertz(50);
    s_servo_l.attach(PIN_SERVO_L, 500, 2400);
    s_servo_r.attach(PIN_SERVO_R, 500, 2400);
    s_servo_l.write(BARRIER_UP_ANGLE);
    s_servo_r.write(BARRIER_UP_ANGLE);
    s_target_angle = BARRIER_UP_ANGLE;

    Serial.println("[ilk] init OK (relay off until first OK eval)");
}

void interlocks_request_barriers(uint8_t angle) {
    s_target_angle = angle;
    s_barrier_started_ms = millis();
    s_servo_l.write(angle);
    s_servo_r.write(angle);
}

void interlocks_force_safe(void) {
    digitalWrite(PIN_RELAY, LOW);   // Cut motor power
    s_servo_l.write(BARRIER_DOWN_ANGLE);
    s_servo_r.write(BARRIER_DOWN_ANGLE);
}

bool interlocks_estop_active(void) { return s_estop_now; }

void interlocks_evaluate(void) {
    bool estop = s_estop_now || (digitalRead(PIN_ESTOP) == HIGH);

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

    // Barrier reach detection — open-loop time estimate (SG90 ~0.1 s/60°).
    bool barrier_done = ((millis() - s_barrier_started_ms) > 1000);
    if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        g_status.estop_active                = estop;
        g_status.barrier_left_angle          = s_target_angle;
        g_status.barrier_right_angle         = s_target_angle;
        g_status.barrier_left_target_reached = barrier_done;
        g_status.barrier_right_target_reached = barrier_done;
        if ((millis() - s_barrier_started_ms) > BARRIER_TIMEOUT_MS && !barrier_done) {
            SET_FAULT(g_status.fault_flags, FAULT_BARRIER_TIMEOUT);
        }
        xSemaphoreGive(g_status_mutex);
    }
}
