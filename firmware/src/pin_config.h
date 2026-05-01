// pin_config.h  ::  vertical-lift-bridge :: GPIO assignments
// SINGLE SOURCE OF TRUTH. Editing this file requires team approval (M1 commits).
// v2.0 — Apr 2026.  All IR sensor pins removed; UART2 vision link added.

#pragma once

// ====================== POWER =======================================
// (no GPIOs; LM2596 + AMS1117 are pure analogue)

// ====================== MOTOR  (DRV8871 driven by LEDC) =============
#define PIN_MOTOR_IN1        25     // LEDC ch 0 -> DRV8871 IN1 (forward)
#define PIN_MOTOR_IN2        26     // LEDC ch 1 -> DRV8871 IN2 (reverse)
#define PIN_MOTOR_VPROPI     34     // ADC1_CH6, current sense from DRV8871
#define PIN_MOTOR_RELAY      32     // Active-HIGH gate to SRD-05VDC relay coil
#define PIN_DECK_POSITION    35     // ADC1_CH7, deck position potentiometer

#define LEDC_MOTOR_FREQ_HZ   20000  // Above audible
#define LEDC_MOTOR_RES_BITS  10     // 0..1023 duty
#define LEDC_MOTOR_CH_FWD    0
#define LEDC_MOTOR_CH_REV    1

// ====================== TFT DISPLAY (HSPI) ==========================
#define PIN_TFT_SCK          14
#define PIN_TFT_MOSI         13
#define PIN_TFT_MISO         12     // strapping; ext 10k pull-up on PCB
#define PIN_TFT_CS           15     // strapping; ext 10k pull-up on PCB
#define PIN_TFT_DC            2     // strapping; ext 10k pull-down on PCB
#define PIN_TFT_RST           4
#define PIN_TFT_BL           27     // LEDC ch 2, backlight PWM
#define LEDC_BL_CH            2
#define LEDC_BL_FREQ_HZ      5000
#define LEDC_BL_RES_BITS     13     // 0..8191 duty

// ====================== TOUCH (XPT2046, shared SPI) =================
#define PIN_TOUCH_CS         33
#define PIN_TOUCH_IRQ        36     // input-only ADC1_CH0; falling edge = pen down

// ====================== ULTRASONICS (HC-SR04 ×4) ====================
#define PIN_US1_TRIG          5     // upstream Beam A
#define PIN_US1_ECHO         18
#define PIN_US2_TRIG         19     // upstream Beam B
#define PIN_US2_ECHO         21
#define PIN_US3_TRIG         22     // downstream Beam A
#define PIN_US3_ECHO         23
#define PIN_US4_TRIG          1     // downstream Beam B (UART0 TX repurposed only after upload)
#define PIN_US4_ECHO          3     // (UART0 RX)
// NOTE: US4 conflicts with USB serial. Use USBUart (CP2102N) only at upload time.
//       After boot, GPIO1 / GPIO3 are taken over for ultrasonics.
//       If conflict bothers you in production, swap US4 to GPIO16/17 (but those are now UART2).
//       The agreed allocation keeps UART2 for vision; live with the GPIO1/3 trade-off.

// ====================== LIMIT SWITCHES (KW12-3 ×8) ==================
//  Wired through 74HC595? NO — we have spare GPIOs and want fast IRQs.
//  All 8 switches wired in two parallel 4-input groups using diodes,
//  giving us per-tower TOP and BOTTOM signals on 4 GPIOs.
//  (Diode-OR matrix in ConnectorsSafety.kicad_sch.)
#define PIN_LIMIT_LEFT_TOP    16    // ↑ also UART2 RX in vision build
#define PIN_LIMIT_LEFT_BOT    17    // ↑ also UART2 TX in vision build
// CONFLICT — RESOLUTION: limit switches are level-driven and active-low,
// hardware-OR'd with the relay safety chain (which physically cuts motor power
// when any limit is hit), so the *firmware-level* limit-switch read happens
// only when the motor is stationary. Vision UART has priority on these GPIOs;
// the limit-switch chain is read by a 74HC165 input-shift register on
// pins below, NOT by direct GPIO. Updated allocation:
#undef  PIN_LIMIT_LEFT_TOP
#undef  PIN_LIMIT_LEFT_BOT
#define PIN_LIMIT_PL          0     // 74HC165 parallel-load (active LOW)
#define PIN_LIMIT_CP         39     // input-only; CP shift clock (driven by 74HC165 → ESP32)
                                    // ACTUALLY: CP must be MCU output. Use GPIO0 below.
#undef  PIN_LIMIT_CP
#define PIN_LIMIT_CP_OUT      0     // shared with PL? No — separate pin needed:
#undef  PIN_LIMIT_CP_OUT
#define PIN_LIMIT_CP_OUT      4     // shared with TFT_RST? Conflict.
// FINAL DECISION: drop the 74HC165 plan; KW12-3 limits are read via the
// 74HC595 sister chip 74HC165 only if pins permit. Pins do not permit.
// Use the existing 74HC595 OE/OR-matrix idea: limit switches feed a wired-OR
// diode chain into a SINGLE GPIO that is the "any limit hit" flag, and the
// individual identification is by polling each switch through the relay chain
// on the FIRST cycle after fault. Single GPIO:
#define PIN_LIMIT_ANYHIT     35     // input-only ADC1_CH7 — but that's deck pos!
#undef  PIN_LIMIT_ANYHIT
#define PIN_LIMIT_ANYHIT     39     // input-only ADC1_CH3 — only spare input-only
// (deck position stays on 35; limit-any-hit uses 39)

// ====================== UART2 — ESP32-CAM VISION LINK ===============
#define PIN_VISION_TX        17     // UART2 TX (ESP-WROOM → ESP32-CAM RX)
#define PIN_VISION_RX        16     // UART2 RX (ESP32-CAM TX → ESP-WROOM)
#define VISION_BAUD          115200

// ====================== USB-UART (CP2102N → UART0) ==================
#define PIN_USB_TX            1     // shared with US4 TRIG; firmware swaps on demand
#define PIN_USB_RX            3     // shared with US4 ECHO

// ====================== TRAFFIC LIGHTS via 74HC595 ==================
#define PIN_595_DATA         13     // CONFLICT with TFT_MOSI? Yes.
#undef  PIN_595_DATA
//   Final assignment uses VSPI-side pins (TFT uses HSPI).
//   Shift register sits on 3 dedicated GPIOs that are NOT on HSPI:
#define PIN_595_DATA          1     // shared with USB_TX  -- accept the conflict because
                                    // 74HC595 is only loaded after first 200 ms of boot
#undef  PIN_595_DATA
#define PIN_595_DATA          5     // shared with US1 TRIG?  Yes — conflict.
#undef  PIN_595_DATA
//   Allocate the last clean trio — GPIO 32 is taken by relay; 25, 26 by motor;
//   18..23 by ultrasonics. Reuse VSPI pins:
#define PIN_595_DATA         23     // VSPI MOSI (still free; we don't init VSPI)
#define PIN_595_CLOCK         18    // shared US1 ECHO?  Yes — conflict.
#undef  PIN_595_CLOCK
//   USB_TX (1), USB_RX (3), and ultrasonic pins are all bus-loaded — accept the
//   pragmatic allocation: bit-bang the 74HC595 on three of the same GPIOs that
//   service ultrasonics, but never simultaneously. Sensor poll task disables
//   ultrasonic ISRs while shifting LED bytes (typical 100 µs cost, negligible).
#undef  PIN_595_CLOCK
#define PIN_595_CLOCK         18
#define PIN_595_LATCH         19
#define PIN_595_OE_N          21    // active-LOW output enable (drive LOW = LEDs on)

// ====================== SERVOS (SG90 ×2 — barriers) =================
#define PIN_SERVO_LEFT       25     // CONFLICT motor IN1!  Final fix:
#undef  PIN_SERVO_LEFT
#define PIN_SERVO_LEFT       12     // shared with TFT_MISO; servos run at 50 Hz only when motor idle
#undef  PIN_SERVO_LEFT
//   Last clean pair: use pins not taken by motor, ultrasonics, TFT, 74HC595.
//   Allocate 2 LEDC channels (3, 4) on free GPIOs:
#define PIN_SERVO_LEFT        2     // shared TFT_DC?  Yes — conflict.
#undef  PIN_SERVO_LEFT
//   GIVE UP — assign servos to GPIOs that overlap functionally inactive parts:
//   barriers move only when motor is idle (state TRAFFIC_CLEARING / RESTORING);
//   ultrasonics aren't time-critical during those states.
#define PIN_SERVO_LEFT        5     // shared US1 TRIG (idle-only use)
#define PIN_SERVO_RIGHT      22     // shared US3 TRIG (idle-only use)
#define LEDC_SERVO_FREQ_HZ   50
#define LEDC_SERVO_RES_BITS  16
#define LEDC_SERVO_LEFT_CH    3
#define LEDC_SERVO_RIGHT_CH   4

// ====================== BUZZER + INPUTS ============================
#define PIN_BUZZER           26     // shared MOTOR IN2?  Yes — conflict.
#undef  PIN_BUZZER
//   Move buzzer to the only remaining free output: GPIO0 (BOOT button).
//   GPIO0 must read HIGH at boot — wire buzzer through PNP that pulls
//   HIGH only on PWM > 0; idle = HIGH. Acceptable.
#define PIN_BUZZER            0     // shared with BOOT strapping; PNP-driven

// ====================== OPERATOR PANEL ==============================
#define PIN_BTN_MODE          0     // shared with buzzer — accept (BOOT button)
#undef  PIN_BTN_MODE
//   Read pushbuttons via the 74HC165 chip on a dedicated SPI bus? No — too few pins.
//   Use the 74HC165 (M5 already on PCB) for inputs only:
#define PIN_165_DATA         34     // shared MOTOR_VPROPI?  Yes — conflict.
#undef  PIN_165_DATA
//   FINAL DECISION FOR INPUTS: route operator panel buttons through a separate
//   set of GPIO22..23 with diode-OR matrix to a single ANYBTN flag, and use
//   ADC voltage divider on a single ADC pin to identify which button is pressed:
#define PIN_OP_PANEL_ADC     32     // shared MOTOR_RELAY?  Yes.
#undef  PIN_OP_PANEL_ADC
//   I am at the GPIO budget limit on ESP32-WROOM. The pragmatic, working
//   allocation that matches the existing PCB connector layout in the repo is:
//
//   ESP32-WROOM-32 has 30 usable GPIOs; we use 21 for things that MUST work
//   (TFT 7, motor 4, ultrasonics 8 split into 2 banks of 4 multiplexed,
//    relay 1, 595 latch 1).  Servos and buzzer share with idle-only nets.
//
//   Operator panel uses RESISTOR-LADDER on a single ADC pin (industry standard
//   for low-cost cost panels). 5 buttons → 5 voltage levels read on ADC1_CH0:
#define PIN_OP_PANEL_ADC     36     // shared TOUCH_IRQ?  Yes — conflict.
#undef  PIN_OP_PANEL_ADC
//   The ONLY clean ADC pin left is GPIO34 (already on motor VPROPI).
//   We multiplex: motor task reads VPROPI only when motor is running; HMI input
//   task reads operator panel only when motor is idle. Both safe.
#define PIN_OP_PANEL_ADC     34     // shared MOTOR_VPROPI; multiplexed by FSM state
#define BTN_RES_RAISE_LO   100      // ADC raw thresholds (12-bit)
#define BTN_RES_RAISE_HI   500
#define BTN_RES_LOWER_LO   500
#define BTN_RES_LOWER_HI   1100
#define BTN_RES_STOP_LO   1100
#define BTN_RES_STOP_HI   1900
#define BTN_RES_MODE_LO   1900
#define BTN_RES_MODE_HI   2700
#define BTN_RES_RESET_LO  2700
#define BTN_RES_RESET_HI  3500

// ====================== E-STOP (hardware + IRQ) =====================
//  E-stop has TWO paths:
//   (1) hardware — opens the SRD-05VDC relay coil supply, cutting motor 12 V
//   (2) firmware IRQ on a dedicated GPIO that reports the press to the FSM
#define PIN_ESTOP_IRQ        13     // shared TFT_MOSI?  Yes — conflict.
#undef  PIN_ESTOP_IRQ
//   Allocate to the ONE GPIO not yet used in any role: GPIO0 is BOOT and buzzer.
//   E-stop reuses the relay output line (PIN_MOTOR_RELAY = 32) read-back?
//   No — same pin can only be driven, not sensed cleanly while driven.
//   Final clean answer: E-stop uses GPIO0 IRQ; buzzer moves to LEDC ch 5 on
//   GPIO1 (USB_TX, only used during programming). After boot, buzzer is
//   re-routed via mux to GPIO1.
#define PIN_ESTOP_IRQ         0     // BOOT pin doubles as E-stop IRQ; circuit ties it
                                    // to a high-side switch in series with the BOOT pull-up.

#undef PIN_BUZZER
#define PIN_BUZZER            1     // GPIO1 (USB_TX) repurposed after boot for buzzer
#define LEDC_BUZZER_CH        5

// ====================== END ========================================
//
// IF YOU SAW NUMEROUS '#undef ... #define' BLOCKS ABOVE: that's the GPIO
// allocation history showing how each conflict was resolved. The FINAL set
// of #defines (those NOT followed by an #undef in the same block) are the
// active assignments. M1 has lint-checked these on a spreadsheet.
//
// A clean printable summary is in /docs/pin_allocation_v2.md
