/**************************************************************
 * Project : Driveway Lighting System
 * File    : PMS_PinsNVS.h — PMS pin map & NVS pin keys (strict)
 * Purpose : ONLY PMS pin assignments + 6‑char NVS keys & defaults.
 *
 * Rules:
 *  • Namespaces ≤6 chars (UPPERCASE). This file uses PIN only.
 *  • Keys: exactly 6 chars (UPPERCASE).
 *  • Non-empty numeric defaults for all pins.
 *  • Derived from PMS Config (hardware mapping).
 **************************************************************/
#pragma once

// ============================================================
// Namespace for persisted pin map
// ============================================================
#define NVS_NS_PIN  "PIN"   // PMS-only: persisted pin overrides

/* Implementation notes:
   - At boot, PMS should read each PIN:* key from NVS. If absent,
     write the compile-time default and use it.
   - Keys are exactly 6 chars to match project rules.
*/

// ============================================================
// 1) SD card (SPI) — logging/media
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
// 2) I2C — charger/monitor ICs
// ============================================================
#define PIN_I2C_SCL_DEFAULT   4
#define PIN_I2C_SDA_DEFAULT   5
#define NVS_PIN_I2C_SCL           "I2CSCL"
#define NVS_PIN_I2C_SDA           "I2CSDA"

// ============================================================
// 3) User feedback — fan PWM, RGB LED, buzzer
// ============================================================
#define PIN_FAN_PWM_DEFAULT   8
#define NVS_PIN_FAN_PWM           "FANPWM"

// RGB LED
#define PIN_LED_R_DEFAULT     6
#define PIN_LED_G_DEFAULT     7
#define PIN_LED_B_DEFAULT     8
#define NVS_PIN_LED_R             "LEDR__"
#define NVS_PIN_LED_G             "LEDG__"
#define NVS_PIN_LED_B             "LEDB__"

// Buzzer
#define PIN_BUZ_GPIO_DEFAULT  11
#define BUZ_AH_DEFAULT    1   // active-high logic
#define BUZ_FBK_DEFAULT   1   // feedback enable
#define NVS_PIN_BUZ_GPIO          "BZGPIO"
#define NVS_PIN_BUZ_AH            "BZAH__"
#define NVS_PIN_BUZ_FBK           "BZFEED"

// ============================================================
// 4) Temperature sensor — board DS18B20
// ============================================================
#define PIN_TS_GPIO_DEFAULT   18
#define NVS_PIN_TS_GPIO           "TSGPIO"
#define TEMP_SENSOR_PULLUP_DEFAULT true
#define TEMP_SENSOR_PULLUP_KEY    "TMPULL"

// ============================================================
// 5) Power rails / sense ADCs / charger enable
// ============================================================
#define PIN_P48_EN_DEFAULT    21   // 48V enable
#define PIN_P5V_EN_DEFAULT    22   // 5V enable
#define PIN_MAINS_OK_DEFAULT  14   // mains presence
#define PIN_V48_ADC_DEFAULT   1    // 48V bus voltage ADC
#define PIN_VBAT_ADC_DEFAULT  9    // battery voltage ADC
#define PIN_I48_ADC_DEFAULT   2    // 48V bus current ADC
#define PIN_IBAT_ADC_DEFAULT  3    // battery current ADC
#define PIN_CH_EN_DEFAULT     10   // charger enable
#define NVS_PIN_P48_EN            "P48EN_"
#define NVS_PIN_P5V_EN            "P5VEN_"
#define NVS_PIN_MAINS_OK          "MSNS__"
#define NVS_PIN_V48_ADC           "V48AD_"
#define NVS_PIN_VBAT_ADC          "VBATAD"
#define NVS_PIN_I48_ADC           "I48AD_"
#define NVS_PIN_IBAT_ADC          "IBATAD"
#define NVS_PIN_CH_EN             "CHEN__"

// ============================================================
// 6) Buttons / boot (if present on PMS board)
// ============================================================
#define PIN_BOOT_DEFAULT      0    // boot/USER button
#define PIN_PWRBTN_DEFAULT    6    // power-on button
#define NVS_PIN_BOOT              "BOOT__"
#define NVS_PIN_PWRBTN            "PWRBTN"

// End of PMS_PinsNVS.h
