// pin_config.h  ::  vertical-lift-bridge :: GPIO assignments
// SINGLE SOURCE OF TRUTH. 
// v3.0 — May 2026. Major refactor to resolve all L4/L8 pin conflicts.
// Shared LDR input enables quad-laser setup without sacrificing pins.
// Input-only pins perfectly utilized for safety and ADCs.

#pragma once

// ====================== MOTOR (L298N dual H-bridge) =================
// ENA tied high on the module; PWM rides directly on IN1/IN2.
#define PIN_MOTOR_IN1        25     // LEDC ch 0  (L298N IN1, up)
#define PIN_MOTOR_IN2        26     // LEDC ch 1  (L298N IN2, down)
#define PIN_MOTOR_RELAY      32     // Safety relay (cuts L298N supply)
#define PIN_DECK_POSITION    35     // UNUSED — position pot omitted (timer-based positioning). Reserved spare; do not reassign.

#define LEDC_MOTOR_FREQ_HZ   4000
#define LEDC_MOTOR_RES_BITS  13
#define LEDC_MOTOR_CH_FWD    0
#define LEDC_MOTOR_CH_REV    1

// ====================== TFT DISPLAY & TOUCH =========================
#define PIN_TFT_SCK          14
#define PIN_TFT_MOSI         13
#define PIN_TFT_MISO         12
#define PIN_TFT_CS           15
#define PIN_TFT_DC            2
#define PIN_TFT_RST          -1     // Hardware tie to EN
#define PIN_TFT_BL           -1     // Hardware tie to 3.3V
#define PIN_TOUCH_CS         33
#define PIN_TOUCH_IRQ        -1     // SPI Polling

// ====================== LASER BREAK-BEAM (LDR ×4) ====================
#define PIN_LDR1_IN          18     // Upstream A
#define PIN_LDR2_IN          19     // Upstream B
#define PIN_LDR3_IN          21     // Downstream A
#define PIN_LDR4_IN          22     // Downstream B

// ====================== LIMIT SWITCHES (omitted) ====================
// Limit switches are omitted in this build — end-stops are timer-estimated.
//#define PIN_LIMIT_ANYHIT     39     // UNUSED (input-only). Reserved spare; do not reassign.

// ====================== SPARE PINS ==================================
// Vision module (ESP32-CAM) and its UART2 link are omitted from this build.
// GPIO16 carries the barrier servos (see SERVOS). GPIO17 is the only true spare.
#define PIN_SPARE_GPIO17     17     // unused / spare

// ====================== TRAFFIC LIGHTS (74HC595) ====================
#define PIN_595_DATA         23
#define PIN_595_CLOCK         4
#define PIN_595_LATCH        27
#define PIN_595_OE_N         -1     // Hardware tie to GND

// ====================== SERVOS (SG90 ×2) ============================
#define PIN_SERVO_LEFT       16     // Moved off GPIO3 (UART0 RX) to free serial monitor
#define PIN_SERVO_RIGHT      16     // Both servos driven by same PWM pin
#define LEDC_SERVO_FREQ_HZ   50
#define LEDC_SERVO_RES_BITS  16
#define LEDC_SERVO_LEFT_CH    3
#define LEDC_SERVO_RIGHT_CH   3

// ====================== BUZZER ======================================
#define PIN_BUZZER           -1     // Disabled to preserve UART TX0
#define LEDC_BUZZER_CH        5

// ====================== OPERATOR PANEL & ESTOP ======================
#define PIN_BTN_LADDER       34     // ADC1_CH6
#define PIN_ESTOP_IRQ        36     // Input-only

// ====================== ALIASES =====================================
#define PIN_MOT_IN1          PIN_MOTOR_IN1
#define PIN_MOT_IN2          PIN_MOTOR_IN2
#define PIN_ESTOP            PIN_ESTOP_IRQ
#define PIN_RELAY            PIN_MOTOR_RELAY
#define PIN_SERVO_L          PIN_SERVO_LEFT
#define PIN_SERVO_R          PIN_SERVO_RIGHT
#define PIN_595_OE           PIN_595_OE_N
