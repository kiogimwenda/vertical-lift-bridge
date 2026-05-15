# M4: Breadboard Testing Guide — Traffic & HMI (Abigael)

This guide covers the ILI9341 display and 74HC595 traffic light logic.

## Objective
Verify SPI graphics rendering, touch responsiveness, and shift-register LED control.

## Hardware Needed
- Main ESP32
- 2.8" ILI9341 SPI TFT + XPT2046 Touch
- 1x 74HC595 Shift Register
- 6x LEDs (2 Red, 2 Amber, 2 Green)
- 6x 220-ohm resistors

## Wiring
1. **Display (SPI):**
   - MOSI -> GPIO 13, MISO -> GPIO 12, SCK -> GPIO 14, CS -> GPIO 15, DC -> GPIO 2, RST -> EN/RST.
   - Touch CS -> GPIO 33.
2. **Shift Register (74HC595):**
   - **Power:** Pin 16 (VCC) to 3.3V, Pin 8 (GND) to GND.
   - **Hardwired:** Pin 10 (SRCLR) to 3.3V, Pin 13 (OE) to GND.
   - **SPI to ESP32:** Pin 14 (DATA) to GPIO 23, Pin 11 (CLOCK) to GPIO 4, Pin 12 (LATCH) to GPIO 27.
   - **LEDs:** Q0 (Pin 15) to RED Anode, Q1 (Pin 1) to AMBER Anode, Q2 (Pin 2) to GREEN Anode. Cathodes to GND via 220-ohm resistors.

## Firmware Execution
1. Flash the firmware.
2. **Validation 1 (HMI):** The screen should boot, display the Light Mode interface, and respond to touch. Click the "RAISE" button.
3. **Validation 2 (Traffic Lights):** When you click "RAISE", the FSM enters `ROAD_CLEARING`. Observe the physical LEDs. The Green LED should turn off, Amber should turn on for 2 seconds, then Red should turn on.
4. **Validation 3 (Brightness):** Navigate to the SET tab on the screen. Drag the brightness slider. The physical TFT backlight should dim and brighten via the LEDC PWM pin.

## Pass Criteria
LVGL renders without tearing (DMA working), touch is calibrated, and the shift register accurately mirrors the software traffic states.