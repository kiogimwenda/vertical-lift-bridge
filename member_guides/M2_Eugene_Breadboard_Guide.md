# M2: Breadboard Testing Guide — Mechanism & Motor (Eugene)

This guide covers testing the L293D Motor Driver and physical limits.

## Objective
Verify the motor driver receives PWM signals correctly, responds to limit switches, and the stall-current ADC reading functions.

## Hardware Needed
- ESP32
- L293D Motor Driver module
- 12V DC Motor (or a multimeter/oscilloscope to read output)
- 12V Power Supply
- Breadboard & wires
- 2x SG90 Micro Servos (Barriers)

## Wiring
1. **L293D to ESP32:**
   - `L_EN` & `R_EN` -> 3.3V (Enable both sides)
   - `RPWM` -> `GPIO 32` (Note: Actually moved to IN1/IN2 on L293D equivalents, check pin_config.h. Default: `GPIO 25` and `GPIO 26`)
2. **Motor:** Connect to motor output terminals.
3. **Power:** Connect 12V Supply. **DO NOT connect 12V to the ESP32 directly!**
4. **Position Potentiometer:** Wire a 10k pot's outer legs to 3.3V and GND. Wire the middle wiper leg to `GPIO 35`.
5. **Servo Barriers SG90:**
   - **Power:** VCC (Red) to dedicated 5V source (NOT ESP32 3.3V), GND (Brown/Black) to common GND.
   - **Signal:** PWM (Orange/Yellow) of BOTH servos connected to ESP32 GPIO 3.

## Firmware Execution
1. Flash the firmware.
2. In the Serial console, trigger a raise command (or use the HMI if connected).
3. **Validation 1 (Direction):** The motor should spin.
4. **Validation 2 (Absolute Encoding):** The firmware now uses the potentiometer as an absolute position encoder. Turn the pot manually. The UI/Serial should reflect the deck position changing immediately. The system no longer relies on auto-zeroing at boot.
5. **Validation 3 (Limit Simulation):** Turn the pot until the position exceeds `DECK_HEIGHT_MAX_MM` (175mm). The motor should brake immediately as the top limit is "hit" mathematically.

## Pass Criteria
Motor drives bidirectionally via PWM, brakes instantly upon limit switch assertion, and correctly identifies a stall current.