# Vertical Lift Bridge — Perfboard Assembly Guide

> **Note on Imagery:** As an AI, I cannot generate a physical, Fritzing-style wire-routing image without risking overlapping artifacts that could cause you to short-circuit your board. Instead, I have created a **Professional Electronics Netlist & Layout Strategy**. This is how real-world engineers assemble complex perfboards without making errors.

## 1. Spatial Layout Strategy (9x15cm Board)
Do not start soldering immediately. Place your components on the board to plan the space.

1. **Bottom Left (Power Supply):** Mount the Barrel Jack, LM2596 buck converter, AMS1117, and capacitors here. This keeps the noisy power conversion away from logic signals. 
2. **Center-Left (ESP32):** Solder two 19-pin female header strips for the ESP32. Do not solder the ESP32 directly to the board; plug it into the headers so you can remove it if needed.
3. **Top Right (Traffic Logic):** Mount the 74HC595 shift register IC (use an IC socket if you have one). Place the 6 traffic LEDs and their 220Ω resistors right next to it.
4. **Bottom Right & Edges (I/O Headers):** Solder male header pins along the edges of the board. These will act as "plugs" for the external components that sit on the physical bridge (Servos, Lasers, LDRs, Limit Switches, Potentiometer, E-Stop).

---

## 2. Step-by-Step Soldering Workflow

### Phase A: The Power Planes (CRITICAL FIRST STEP)
*Before connecting any chips, you must build the power supply and verify the voltages with a multimeter.*
1. Solder the Barrel Jack.
2. Follow the `PCB_PowerSupply.png` schematic: Route 12V from the Jack to the LM2596 input.
3. Route the 5V output from the LM2596 to create a **5V Copper Rail** (a long line of solder/wire along the bottom of the board).
4. Route the 5V to the AMS1117-3.3 input. Route its output to create a **3.3V Copper Rail**.
5. Connect all Grounds to a massive **Common GND Rail** that runs around the perimeter of the board.
6. **TEST:** Plug in 12V. Use a multimeter to confirm the 5V rail reads 5.0V and the 3.3V rail reads 3.3V. **Do not proceed if this is wrong, or you will fry the ESP32.**

### Phase B: ESP32 Power & Core
1. Connect the ESP32's `VIN` (or 5V) pin to the **5V Rail**.
2. Connect the ESP32's multiple `GND` pins to the **GND Rail**.

### Phase C: Routing the Netlist
Use thin wrapping wire to connect the points below. Cross off each line as you solder it.

#### The 74HC595 Shift Register & Traffic Lights
| From | To | Purpose |
| :--- | :--- | :--- |
| 595 Pin 16 (VCC) | 3.3V Rail | Logic Power |
| 595 Pin 8 (GND) | GND Rail | Common Ground |
| 595 Pin 10 (SRCLR) | 3.3V Rail | Prevent Reset |
| 595 Pin 13 (OE) | GND Rail | Enable Outputs |
| 595 Pin 14 (DATA) | ESP32 GPIO 23 | SPI Data |
| 595 Pin 11 (CLOCK) | ESP32 GPIO 4 | SPI Clock |
| 595 Pin 12 (LATCH) | ESP32 GPIO 27 | SPI Latch |
| 595 Pin 15 (Q0) | RED LEDs (Anodes) | Road Red |
| 595 Pin 1 (Q1) | YELLOW LEDs (Anodes) | Road Yellow |
| 595 Pin 2 (Q2) | GREEN LEDs (Anodes) | Road Green |
| LED Cathodes (All) | 220Ω Resistor -> GND Rail | Current Limiting |

#### The L298N Motor Driver (External Module)
*Assuming the L298N module is mounted off-board, run wires from headers.*
| From | To | Purpose |
| :--- | :--- | :--- |
| L298N 12V | 12V from Barrel Jack | Motor Power |
| L298N GND | GND Rail | Common Ground |
| L298N IN1 | ESP32 GPIO 25 | Motor Up PWM |
| L298N IN2 | ESP32 GPIO 26 | Motor Down PWM |
| L298N ENA | 5V Rail (or jumper cap on module) | Enable Channel A |

#### Servo Barriers (SG90)
| From | To | Purpose |
| :--- | :--- | :--- |
| Servo RED Wires | 5V Rail | High-current Power |
| Servo BROWN Wires | GND Rail | Common Ground |
| Servo ORANGE Wires | ESP32 GPIO 3 | Shared PWM Signal |

#### Laser Break-Beam Sensors (LDR Modules)
| From | To | Purpose |
| :--- | :--- | :--- |
| All LDR VCC Pins | 5V (or 3.3V depending on module) | Sensor Power |
| All LDR GND Pins | GND Rail | Common Ground |
| LDR 1 (Upstream A) Out | ESP32 GPIO 18 | Direction sensing |
| LDR 2 (Upstream B) Out | ESP32 GPIO 19 | Direction sensing |
| LDR 3 (Downstream A) Out | ESP32 GPIO 21 | Direction sensing |
| LDR 4 (Downstream B) Out | ESP32 GPIO 22 | Direction sensing |
| All Lasers VCC | 5V Rail | Laser Power |
| All Lasers GND | GND Rail | Laser Ground |

#### Safety & Control (E-Stop, Limits, Potentiometer)
| From | To | Purpose |
| :--- | :--- | :--- |
| E-Stop Button | ESP32 GPIO 36 & GND Rail | Interrupt Trigger |
| Limit Switches (Both) | ESP32 GPIO 39 & GND Rail | Any-hit Trigger |
| GPIO 39 | 10kΩ Resistor -> 3.3V Rail | Pull-up for Limits |
| Potentiometer Pin 1 | GND Rail | Reference Low |
| Potentiometer Pin 3 | 3.3V Rail | Reference High |
| Potentiometer Pin 2 (Wiper)| ESP32 GPIO 35 | Absolute Position ADC |
| E-Stop Relay Module IN | ESP32 GPIO 32 | Motor Power Cutoff |

#### TFT Screen & Touch (SPI)
| From | To | Purpose |
| :--- | :--- | :--- |
| TFT VCC | 3.3V Rail | Screen Power |
| TFT GND | GND Rail | Common Ground |
| TFT MOSI | ESP32 GPIO 13 | SPI Data Out |
| TFT MISO | ESP32 GPIO 12 | SPI Data In |
| TFT SCK / CLK | ESP32 GPIO 14 | SPI Clock |
| TFT CS | ESP32 GPIO 15 | Screen Chip Select |
| TFT DC | ESP32 GPIO 2 | Data/Command |
| TOUCH CS | ESP32 GPIO 33 | Touch Chip Select |
| TFT RST | ESP32 EN (or 3.3V) | Reset |
| TFT BL / LED | 3.3V Rail | Backlight Power |

---
## 3. Best Practices for Soldering without Errors
1. **Never cross uninsulated wires.** If wires must cross paths, use insulated jumper wire.
2. **Use Headers for Everything External.** Do not solder the long cables from the servos, lasers, or screen directly into the perfboard holes. Solder male header pins to the board, and use female dupont connectors on the component wires. This allows you to replace a broken servo or screen instantly without desoldering.
3. **Double Check the 5V vs 3.3V.** The ESP32 logic pins operate at 3.3V. Sending 5V directly into a GPIO pin (e.g., from an unprotected 5V sensor output) will instantly destroy the pin. Ensure your LDR modules have a 3.3V logic output, or power their logic side with 3.3V.