/**************************************************************
 * Project : Driveway Lighting System
 * File    : SEN_PinsNVS.h — SENSOR pin map & NVS pin keys (strict)
 * Purpose : ONLY SENSOR (SSM) pin assignments + 6‑char NVS keys & defaults.
 *
 * Rules:
 *  • Namespace ≤6 chars (UPPERCASE). This file uses PIN only.
 *  • Keys: exactly 6 chars (UPPERCASE).
 *  • Non-empty numeric defaults for all pins.
 *  • Derived from Sensor module mapping (TF‑Luna×2, BME280, VEML7700‑TR, SD, RGB, Buzzer, Fan).
 **************************************************************/
#pragma once

// ============================================================
// Namespace for persisted pin map
// ============================================================
#define NVS_NS_PIN  "PIN"   // SENSOR-only: persisted pin overrides

/* Implementation notes:
   - At boot, SENSOR should read each PIN:* key from NVS. If absent,
     write the compile-time default and use it.
   - Keys are exactly 6 chars to match project rules.
*/

// ============================================================
// 1) SD card (SPI) — on-node logging
// ============================================================
#define PIN_SD_MISO_DEFAULT   39
#define PIN_SD_MOSI_DEFAULT   38
#define PIN_SD_CS_DEFAULT     41
#define PIN_SD_SCK_DEFAULT    42
#define SD_CARD_MODEL_DEFAULT     "MKDV8GIL-AST"
#define NVS_PIN_SD_MISO           "SDMISO"
#define NVS_PIN_SD_MOSI           "SDMOSI"
#define NVS_PIN_SD_CS             "SDCS__"
#define NVS_PIN_SD_SCK            "SDSCK_"
#define NVS_SD_CARD_MODEL_KEY     "SDMODL"

// ============================================================
// 2) I2C buses
//    I2C1 → TF-Luna A/B (I2C mode)
// ============================================================
#define PIN_I2C1_SDA_DEFAULT  4
#define PIN_I2C1_SCL_DEFAULT  5
#define NVS_PIN_I2C1_SDA          "I1SDA_"
#define NVS_PIN_I2C1_SCL          "I1SCL_"

//    I2C2 → VEML7700-TR + BME280
#define PIN_I2C2_SDA_DEFAULT  16
#define PIN_I2C2_SCL_DEFAULT  17
#define NVS_PIN_I2C2_SDA          "I2SDA_"
#define NVS_PIN_I2C2_SCL          "I2SCL_"

// ============================================================
// 3) User-feedback & cooling
// ============================================================
// Fan PWM
#define PIN_FAN_PWM_DEFAULT   8
#define NVS_PIN_FAN_PWM           "FANPWM"

// RGB LED
#define PIN_LED_R_DEFAULT     6
#define PIN_LED_G_DEFAULT     7
#define PIN_LED_B_DEFAULT     8
#define NVS_PIN_LED_R             "LEDR__"
#define NVS_PIN_LED_G             "LEDG__"
#define NVS_PIN_LED_B             "LEDB__"

// Buzzer GPIO
#define PIN_BUZ_GPIO_DEFAULT  11
#define BUZ_AH_DEFAULT    1   // active-high logic
#define BUZ_FBK_DEFAULT   1   // feedback enable
#define NVS_PIN_BUZ_GPIO          "BZGPIO"
#define NVS_PIN_BUZ_AH            "BZAH__"
#define NVS_PIN_BUZ_FBK           "BZFEED"

// ============================================================
// 4) Optional: Boot/User button (if present on SENSOR board)
// ============================================================
#define PIN_BOOT_DEFAULT      0
#define NVS_PIN_BOOT              "BOOT__"

// End of SEN_PinsNVS.h
