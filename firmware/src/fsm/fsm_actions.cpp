// ============================================================================
// fsm/fsm_actions.cpp — Side-effects on state entry/exit.
// Owner: George (M1) (calls into M2/M3/M4/M5 modules)
// ============================================================================
#include "fsm_actions.h"
#include "../system_types.h"
#include "../motor/motor_driver.h"
#include "../counterweight/counterweight.h"
#include "../traffic/traffic_lights.h"
#include "../traffic/buzzer.h"
#include "../safety/interlocks.h"
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
        send_motor(MOTOR_STOP, 0);
        break;

    case STATE_IDLE:
        // Bridge down, road open to vehicles.
        traffic_lights_set_road  (TL_GREEN);
        send_motor(MOTOR_STOP, 0);
        barriers_open();
        break;

    case STATE_ROAD_CLEARING:
        traffic_lights_set_road  (TL_AMBER);
        buzzer_chirp(2);
        barriers_close();
        // Arm the counterweight ready-gate. Tank levels are slaved to deck
        // height (deck at 0 mm here -> tanks at 0 ml), so this just guarantees
        // one EVT_CW_READY edge once the (already-settled) counterweight reports
        // balanced, letting the FSM advance to RAISING.
        counterweight_prepare();
        break;

    case STATE_RAISING:
        // Bridge still moving up. Solid (not blinking) so the slow 74HC595
        // bit-bang only runs on the state transition, keeping the safety task
        // deterministic during motion.
        traffic_lights_set_road  (TL_RED);
        send_motor(MOTOR_UP, MOTOR_PWM_RAISE_DEFAULT);
        break;

    case STATE_RAISED_HOLD:
        // Bridge fully up and holding.
        send_motor(MOTOR_BRAKE, 0);
        traffic_lights_set_road  (TL_RED);
        break;

    case STATE_LOWERING:
        // Bridge coming down.
        send_motor(MOTOR_DOWN, MOTOR_PWM_LOWER_DEFAULT);
        traffic_lights_set_road  (TL_RED);
        break;

    case STATE_ROAD_OPENING:
        send_motor(MOTOR_STOP, 0);
        traffic_lights_set_road  (TL_AMBER);
        buzzer_chirp(1);
        barriers_open();
        break;

    case STATE_FAULT:
        send_motor(MOTOR_BRAKE, 0);
        traffic_lights_set_road  (TL_RED);
        buzzer_pattern_fault();
        break;

    case STATE_ESTOP:
        send_motor(MOTOR_COAST, 0);   // Hardware E-stop also kills relay
        traffic_lights_set_road  (TL_RED);
        buzzer_pattern_estop();
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
