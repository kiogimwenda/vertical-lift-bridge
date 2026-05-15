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
   - **Power:** VCC to 5V (required for 40kHz ping), GND to GND.
   - **Trigger:** Shared TRIG pin for all 4 sensors connected to ESP32 GPIO 5.
   - **Echo (with 1k/2k voltage dividers dropping 5V to 3.3V):** US1 (Upstream A) to GPIO 18, US2 (Upstream B) to GPIO 19, US3 (Downstream A) to GPIO 21, US4 (Downstream B) to GPIO 22.
2. **ESP32-CAM (UART Link):**
   - **Power:** 5V to dedicated 5V buck converter (to prevent brownouts), GND to common GND.
   - **UART to Main ESP32:** CAM U0T (TX) to ESP32 GPIO 16 (RX), CAM U0R (RX) to ESP32 GPIO 17 (TX).

## Firmware Execution
1. Power up both ESP32s.
2. **Test 1 (Vision):** Wave your hand in front of the ESP32-CAM. Monitor the Main ESP32 Serial console. You should see `[vision] Parsed: Vehicle=1, Confidence=XX%`.
3. **Test 2 (Ultrasonic Direction):** 
   - Place your hand in front of Upstream Beam A, then quickly move it to Beam B.
   - The Serial Monitor should output: `Vessel APPROACHING`.
   - Move your hand from Beam B to Beam A. It should output: `Vessel LEAVING`.

## Pass Criteria
Main ESP32 successfully parses live JSON without buffer overflows, and the quad-sensors accurately discern hand movement direction in the physical world without ghost echoes.