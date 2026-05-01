// ============================================================================
// fsm/fsm_actions.cpp — Side-effects on state entry/exit.
// Owner: George (M1) (calls into M2/M3/M4/M5 modules)
// ============================================================================
#include "fsm_actions.h"
#include "../system_types.h"
#include "../motor/motor_driver.h"
#include "../traffic/traffic_lights.h"
#include "../traffic/buzzer.h"
#include "../safety/interlocks.h"
#include "../hmi/display.h"
#include <Arduino.h>

// ---- helpers ---------------------------------------------------------------
static void send_motor(MotorDirection_t dir, uint16_t duty) {
    static uint32_t s_req_id = 0;
    MotorCommand_t c = { dir, duty, ++s_req_id };
    xQueueSend(g_motor_cmd_queue, &c, 0);
}

static void barriers_close(void) {
    // Member 4 owns the servo lib; we just request via interlocks.
    interlocks_request_barriers(BARRIER_DOWN_ANGLE);
}
static void barriers_open(void) {
    interlocks_request_barriers(BARRIER_UP_ANGLE);
}

// ---- entry actions --------------------------------------------------------
void fsm_action_on_entry(SystemState_t s) {
    switch (s) {
    case STATE_INIT:
        traffic_lights_set_road  (TL_OFF);
        traffic_lights_set_marine(TL_OFF);
        send_motor(MOTOR_STOP, 0);
        break;

    case STATE_IDLE:
        traffic_lights_set_road  (TL_GREEN);
        traffic_lights_set_marine(TL_RED);
        send_motor(MOTOR_STOP, 0);
        barriers_open();
        display_notify_state_change(STATE_IDLE);
        break;

    case STATE_ROAD_CLEARING:
        traffic_lights_set_road  (TL_AMBER);
        traffic_lights_set_marine(TL_RED);
        buzzer_chirp(2);
        barriers_close();
        display_notify_state_change(STATE_ROAD_CLEARING);
        break;

    case STATE_RAISING:
        traffic_lights_set_road  (TL_RED);
        traffic_lights_set_marine(TL_RED);
        send_motor(MOTOR_UP, MOTOR_PWM_RAISE_DEFAULT);
        display_notify_state_change(STATE_RAISING);
        break;

    case STATE_RAISED_HOLD:
        send_motor(MOTOR_BRAKE, 0);
        traffic_lights_set_road  (TL_RED);
        traffic_lights_set_marine(TL_GREEN);
        display_notify_state_change(STATE_RAISED_HOLD);
        break;

    case STATE_LOWERING:
        traffic_lights_set_marine(TL_AMBER);
        send_motor(MOTOR_DOWN, MOTOR_PWM_LOWER_DEFAULT);
        display_notify_state_change(STATE_LOWERING);
        break;

    case STATE_ROAD_OPENING:
        send_motor(MOTOR_STOP, 0);
        traffic_lights_set_road  (TL_AMBER);
        traffic_lights_set_marine(TL_RED);
        buzzer_chirp(1);
        barriers_open();
        display_notify_state_change(STATE_ROAD_OPENING);
        break;

    case STATE_FAULT:
        send_motor(MOTOR_BRAKE, 0);
        traffic_lights_set_road  (TL_RED);
        traffic_lights_set_marine(TL_RED);
        buzzer_pattern_fault();
        display_notify_state_change(STATE_FAULT);
        break;

    case STATE_ESTOP:
        send_motor(MOTOR_COAST, 0);   // Hardware E-stop also kills relay
        traffic_lights_set_road  (TL_RED);
        traffic_lights_set_marine(TL_RED);
        buzzer_pattern_estop();
        display_notify_state_change(STATE_ESTOP);
        break;

    default: break;
    }
}

// ---- exit actions ---------------------------------------------------------
void fsm_action_on_exit(SystemState_t s) {
    switch (s) {
    case STATE_RAISING:
    case STATE_LOWERING:
        send_motor(MOTOR_BRAKE, 0);
        break;
    case STATE_FAULT:
    case STATE_ESTOP:
        buzzer_off();
        break;
    default: break;
    }
}
