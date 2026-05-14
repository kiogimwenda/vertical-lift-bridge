# M1: Breadboard Testing Guide — System FSM & Logic (George)

This guide walks you through physically testing the FSM engine on a breadboard without the full mechanical assembly.

## Objective
Verify that `fsm_engine.cpp` transitions correctly across all 9 states in response to mocked events from the queue.

## Hardware Needed
- Main ESP32 Dev Board
- 2x Push Buttons (for mock Limit Switches)
- 1x Push Button (for Operator "Raise" command)
- Jumper wires

## Wiring
Since you are testing the core logic, we will mock the physical triggers using buttons wired to the ESP32.
1. **Limit Switch Mock:** Wire a single button between `GND` and `GPIO 39` (`PIN_LIMIT_ANYHIT`) (ensure a 10k pull-up resistor to 3.3V is used since GPIO 39 has no internal pull-up).
2. **Operator Raise Mock:** We will trigger this via the Serial terminal instead of a button to keep wiring simple.

## Firmware Adjustments for Testing
To test the FSM in isolation, we need to artificially inject events.
1. Open `firmware/src/main.cpp`.
2. Inside `loop()`, add a small block to read serial commands:
   ```cpp
   if (Serial.available() > 0) {
       char c = Serial.read();
       SystemEvent_t e = EVT_NONE;
       if (c == 'r') e = EVT_OPERATOR_RAISE;
       if (c == 'l') e = EVT_OPERATOR_LOWER;
       if (c == 'v') e = EVT_VEHICLE_DETECTED;
       if (e != EVT_NONE) xQueueSend(g_event_queue, &e, 0);
   }
   ```

## Execution
1. Flash the firmware to the ESP32.
2. Open the Serial Monitor at 115200 baud.
3. **Test Cycle:**
   - The system should boot into `STATE_IDLE`.
   - Type `v` and press Enter to simulate `EVT_VEHICLE_DETECTED`.
   - Observe the FSM transition to `STATE_ROAD_CLEARING`.
   - Wait for the simulated barriers/road logic to pass; it should enter `STATE_RAISING`.
   - Press your Limit Switch button. The FSM should log `EVT_TOP_LIMIT_HIT` (or equivalent any-hit) and transition to `STATE_RAISED_HOLD`.
   - Wait 8 seconds. The FSM should print `EVT_HOLD_TIMEOUT` and enter `STATE_LOWERING`.
   - Press your Limit Switch button again. The FSM enters `STATE_ROAD_OPENING`, then back to `STATE_IDLE`.

## Pass Criteria
The FSM traverses the entire 9-state cycle strictly according to the guard conditions without hanging or rebooting.