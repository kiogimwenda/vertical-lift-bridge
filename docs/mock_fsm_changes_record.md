# FSM Mock Cycle Testing — DEV MOCK Changes Record

This document serves as a record of the temporary modifications made to the firmware to allow Member 1 (George) to run a full mock cycle of the FSM on a breadboard without the complete mechanical/electrical assembly.

**Note:** These changes should be reverted before final integration with the physical hardware.

## 1. `firmware/src/main.cpp`
*   **Change:** Replaced the empty `loop()` function with an interactive serial event injection block.
*   **Reason:** Allows the developer to use the PlatformIO terminal monitor to type single characters (e.g., 'r', 'v', 't', 'b') to simulate hardware buttons and sensors without needing them physically wired. 
*   **Implementation:** Reads `Serial.read()` immediately without waiting for newlines, mapping characters to `SystemEvent_t` and pushing them to `g_event_queue`.

## 2. `firmware/src/motor/motor_driver.cpp`
*   **Change:** Changed `#define CAL_ADC_ZERO_DEFAULT` from `400` to `0`.
*   **Reason:** During breadboard testing, GPIO 35 (Deck Position) was grounded. With the default zero at 400, a grounded pin produced a negative coordinate (-28mm), instantly tripping the `pos-range` fault on boot and locking the FSM. Setting the baseline to 0 prevents this.
*   **Change:** Commented out `SET_FAULT(g_status.fault_flags, FAULT_STALL);`.
*   **Reason:** Since no motor or physical encoder is attached, the firmware's stall detector panics when the "motor" is commanded to move but the position value doesn't change. Disabling this allows the FSM to transition into and stay in the `RAISING` and `LOWERING` states freely.

## 3. `firmware/src/sensors/ultrasonic.cpp`
*   **Change:** Commented out `SET_FAULT(g_status.fault_flags, FAULT_ULTRASONIC_FAIL);`.
*   **Reason:** To prevent the system from halting due to the lack of physical HC-SR04 ultrasonic sensors connected to the breadboard.

## 4. `firmware/src/safety/interlocks.cpp`
*   **Change:** Commented out `digitalRead(PIN_ESTOP)` in `estop_isr` and `interlocks_evaluate`. Hardcoded `s_estop_now = false`.
*   **Reason:** GPIO 36 does not have an internal pull-up. Left floating on the breadboard, it picked up electrical noise and randomly triggered E-Stop events, crashing the FSM cycle.

## 5. Added Helper Scripts
*   **`firmware/auto_test.py`:** Added a python script to automatically run through the FSM serial commands if the IDE terminal proves unreliable.
*   **`firmware/monitor.py`:** Added a python script to purely monitor the serial output.