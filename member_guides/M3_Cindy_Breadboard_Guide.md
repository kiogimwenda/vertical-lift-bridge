# M3: Breadboard Testing Guide — Vision & Sensors (Cindy)

This guide covers physical testing of the Quad HC-SR04 setup and the ESP32-CAM UART link.

## Objective
Verify real-world ultrasonic echo timing and UART JSON parsing from the live camera.

## Hardware Needed
- Main ESP32
- ESP32-CAM module (flashed with `firmware_cam` code)
- 4x HC-SR04 Ultrasonic Sensors
- Breadboard & wires

## Wiring
1. **Ultrasonics (Shared Trigger Design):**
   - Wire all 4 `TRIG` pins together to the shared `GPIO 5`.
   - Wire the 4 `ECHO` pins: US1 -> `GPIO 18`, US2 -> `GPIO 19`, US3 -> `GPIO 21`, US4 -> `GPIO 22`.
   - **CRITICAL:** Use a voltage divider (e.g., 2k and 1k resistors) to step down the 5V Echo output to 3.3V before connecting to the ESP32.
2. **ESP32-CAM (UART Link):**
   - Connect ESP32-CAM `GND` to Main ESP32 `GND`.
   - Connect ESP32-CAM `U0TX` (Pin 1) to Main ESP32 `U2RX` (GPIO 16).
   - Connect ESP32-CAM `U0RX` (Pin 3) to Main ESP32 `U2TX` (GPIO 17).

## Firmware Execution
1. Power up both ESP32s.
2. **Test 1 (Vision):** Wave your hand in front of the ESP32-CAM. Monitor the Main ESP32 Serial console. You should see `[vision] Parsed: Vehicle=1, Confidence=XX%`.
3. **Test 2 (Ultrasonic Direction):** 
   - Place your hand in front of Upstream Beam A, then quickly move it to Beam B.
   - The Serial Monitor should output: `Vessel APPROACHING`.
   - Move your hand from Beam B to Beam A. It should output: `Vessel LEAVING`.

## Pass Criteria
Main ESP32 successfully parses live JSON without buffer overflows, and the quad-sensors accurately discern hand movement direction in the physical world without ghost echoes.