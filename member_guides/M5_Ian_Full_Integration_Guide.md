# M5: Full System Breadboard Prototype Guide (Ian)

This guide is for the final integration test. You will combine all modules onto a large breadboard array to test the complete electrical flow before soldering the PCB.

## Objective
Verify power distribution, shared ground loops, logic-level shifting, and complete FSM operation with all hardware connected simultaneously.

## Master Hardware List
- 1x Main ESP32
- 1x ESP32-CAM
- 1x L293D Driver + 12V Motor
- 4x HC-SR04 Sensors
- 1x ILI9341 TFT Display
- 1x 74HC595 Shift Register + LEDs
- 2x SG90 Micro Servos (Barriers)
- 1x Physical E-Stop Mushroom Button
- 1x 5V Relay Module

## Power Architecture (CRITICAL)
Before wiring data pins, establish the power rails:
1. **12V Rail:** Connect the main PSU to the L293D motor power.
2. **5V Rail:** Use a buck converter (LM2596) to step 12V down to 5V. 
   - Connect the 5V Rail to: ESP32 `VIN`, ESP32-CAM `5V`, HC-SR04 `VCC` x4, Relay `VCC`, Shift Register `VCC`, Servos `VCC`.
3. **3.3V Rail:** Use the ESP32's onboard 3.3V pin to power the ILI9341 `VCC` and `LED` (if not using PWM).
4. **Common Ground:** Tie ALL grounds together (12V GND, 5V GND, ESP32 GND, Breakout GNDs). **Failure to tie grounds will destroy logic pins.**

## E-Stop Hardware Integration
The E-Stop must physically cut power to dangerous components, bypassing software.
1. Wire the 12V PSU positive wire through the **Normally Closed (NC)** contacts of the E-Stop button before it reaches the Motor Driver.
2. Tap a secondary wire from the NC contact, through a voltage divider (12V -> 3.3V), to `GPIO 36`.
3. *Test:* Hitting the E-stop must instantly kill the motor physically, and the ESP32 should register `EVT_ESTOP_PRESSED` via GPIO 36, turning the HMI screen Red.

## Step-by-Step Integration
Do not connect everything at once. Build and test sequentially:
1. **Power & Brains:** Connect the 5V rail to the Main ESP32. Flash firmware. Ensure it prints over Serial.
2. **HMI:** Connect the TFT. Ensure the FSM loop is running and updating the screen.
3. **Safety Loop:** Connect the E-stop logic pin. Verify hitting it triggers the Red Fault screen.
4. **Peripherals:** Connect the Shift Register and Servos. Trigger a state change via HMI and verify lights/barriers actuate.
5. **Sensors:** Connect the 4x HC-SR04s using the **Shared Trigger Design** (all 4 TRIG pins tied to GPIO 5). Verify echo voltage dividers are outputting exactly 3.3V on a multimeter before connecting to the ESP32.
6. **Vision:** Power the ESP32-CAM. Connect the UART crossover. Verify JSON heartbeats on the Telemetry page.
7. **Position Encoding:** Wire a 10k potentiometer to `GPIO 35` to simulate the bridge's physical position encoder.
8. **High Power:** Finally, connect the 12V rail and L293D. 

## Final Validation
With everything wired, set the HMI to **AUTO MODE**. Walk your hand across the upstream ultrasonic sensors. The system must:
1. Turn lights amber, then red.
2. Drop servo barriers.
3. Spin the motor UP. Manually turn the potentiometer to simulate the deck rising.
4. When the pot simulates crossing `175mm`, the motor must stop. Hold for 8 seconds.
5. Spin motor DOWN. Manually turn the potentiometer back towards `0mm`.
6. When the pot hits `0mm`, raise barriers, turn lights green.

If this cycle completes seamlessly, your breadboard prototype is a **100% PASS** and you are ready for PCB fabrication.