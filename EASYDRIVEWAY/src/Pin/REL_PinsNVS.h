/**************************************************************
 * Project : Driveway Lighting System
 * File    : REL_PinsNVS.h — RELAY pin map & NVS pin keys (strict)
 * Purpose : Contains ONLY RELAY pin assignments + 6-char NVS keys & defaults.
 *
 * Rules:
 *  • Namespace ≤6 chars (UPPERCASE). This file uses PIN only.
 *  • Keys: exactly 6 chars (UPPERCASE).
 *  • Non-empty numeric defaults for all pins.
 **************************************************************/
#pragma once

// ============================================================
// NAMESPACE DEFINITION
// ============================================================
#define NVS_NS_PIN   "PIN"    // RELAY-only: persisted pin overrides

/* Implementation notes:
   - At boot, firmware reads each PIN:* key from NVS.
   - If missing, the default value is written & used.
   - All keys are 6 chars, per project rules.
*/


// ============================================================
// 1) SD CARD (SPI) — On-node logging
// ============================================================
// Defaults
#define PIN_SD_MISO_DEFAULT    39
#define PIN_SD_MOSI_DEFAULT    38
#define PIN_SD_CS_DEFAULT      41
#define PIN_SD_SCK_DEFAULT     42
#define SD_CARD_MODEL_DEFAULT      "MKDV8GIL-AST"
// NVS Keys
#define NVS_PIN_SD_MISO            "SDMISO"
#define NVS_PIN_SD_MOSI            "SDMOSI"
#define NVS_PIN_SD_CS              "SDCS__"
#define NVS_PIN_SD_SCK             "SDSCK_"
#define NVS_SD_CARD_MODEL_KEY      "SDMODL"


// ============================================================
// 2) RELAY DRIVER CHANNELS (SRD-48VDC-SL-C)
// ============================================================
// Relay channels
#define PIN_LEFT_DEFAULT           17   // LEFT channel GPIO
#define PIN_RIGHT_DEFAULT          16   // RIGHT channel GPIO
#define NVS_PIN_RELAY_LEFT         "RLYLFT"
#define NVS_PIN_RELAY_RIGHT        "RLYRGT"
// RGB LED
#define PIN_LED_R_DEFAULT          6
#define PIN_LED_G_DEFAULT          7
#define PIN_LED_B_DEFAULT          15
#define NVS_PIN_LED_R              "LEDR__"
#define NVS_PIN_LED_G              "LEDG__"
#define NVS_PIN_LED_B              "LEDB__"


// ============================================================
// 3) BUZZER & TEMPERATURE SENSOR (DS18B20)
// ============================================================
// Buzzer
#define PIN_BUZ_GPIO_DEFAULT       11   // Active-high by HW
#define BUZ_AH_DEFAULT             1    // Active-high logic
#define BUZ_FBK_DEFAULT            1    // Feedback enable
#define NVS_PIN_BUZ_GPIO           "BZGPIO"
#define NVS_PIN_BUZ_AH             "BZAH__"
#define NVS_PIN_BUZ_FBK            "BZFEED"
// Temperature sensor
#define PIN_TS_GPIO_DEFAULT        3    // OneWire data pin
#define TEMP_SENSOR_PULLUP_DEFAULT true
#define TEMP_SENSOR_PULLUP_KEY     "TMPULL"
#define NVS_PIN_TS_GPIO            "TSGPIO"
// Fan PWM
#define PIN_FAN_PWM_DEFAULT        8
#define NVS_PIN_FAN_PWM            "FANPWM"


// ============================================================
// 4) BOOT / USER BUTTON
// ============================================================
#define PIN_BOOT_DEFAULT           0
#define NVS_PIN_BOOT               "BOOT__"


// ============================================================
// END OF FILE
// ============================================================
