// pin_config.h  ::  vertical-lift-bridge :: GPIO assignments
// SINGLE SOURCE OF TRUTH. 
// v3.0 — May 2026. Major refactor to resolve all L4/L8 pin conflicts.
// Shared US trigger enables quad-ultrasonic setup without sacrificing pins.
// Input-only pins perfectly utilized for safety and ADCs.

#pragma once

// ====================== MOTOR =======================================
#define PIN_MOTOR_IN1        25     // LEDC ch 0
#define PIN_MOTOR_IN2        26     // LEDC ch 1
#define PIN_MOTOR_RELAY      32     // Safety relay
#define PIN_DECK_POSITION    35     // ADC1_CH7, pot

#define LEDC_MOTOR_FREQ_HZ   20000
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

// ====================== ULTRASONICS (HC-SR04 ×4) ====================
#define PIN_US_TRIG           5     // Shared trigger for all 4 sensors
#define PIN_US1_ECHO         18     // Upstream A
#define PIN_US2_ECHO         19     // Upstream B
#define PIN_US3_ECHO         21     // Downstream A
#define PIN_US4_ECHO         22     // Downstream B

// ====================== LIMIT SWITCHES ==============================
#define PIN_LIMIT_ANYHIT     39     // input-only

// ====================== UART2 — ESP32-CAM ===========================
#define PIN_VISION_TX        17
#define PIN_VISION_RX        16
#define VISION_BAUD          115200

// ====================== TRAFFIC LIGHTS (74HC595) ====================
#define PIN_595_DATA         23
#define PIN_595_CLOCK         4
#define PIN_595_LATCH        27
#define PIN_595_OE_N         -1     // Hardware tie to GND

// ====================== SERVOS (SG90 ×2) ============================
#define PIN_SERVO_LEFT        0
#define PIN_SERVO_RIGHT      27     // Wait, 27 is 595_LATCH! Let's tie Servos together on 0!
#undef PIN_SERVO_RIGHT
#define PIN_SERVO_RIGHT       0     // Both servos driven by same PWM pin
#define LEDC_SERVO_FREQ_HZ   50
#define LEDC_SERVO_RES_BITS  16
#define LEDC_SERVO_LEFT_CH    3
#define LEDC_SERVO_RIGHT_CH   3

// ====================== BUZZER ======================================
#define PIN_BUZZER            1     // GPIO1 (USB_TX) repurposed after boot for buzzer
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
#define PIN_UART2_TX         PIN_VISION_TX
#define PIN_UART2_RX         PIN_VISION_RX
#define PIN_595_OE           PIN_595_OE_N
