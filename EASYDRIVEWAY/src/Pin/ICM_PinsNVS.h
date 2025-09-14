/**************************************************************
 * Project : Driveway Lighting System
 * File    : ICM_PinsNVS.h — ICM pin map & NVS keys (strict)
 * Purpose : Only ICM pin assignments + ICM NVS keys & defaults.
 *
 * Notes:
 *  • Namespaces ≤6 chars (UPPERCASE), keys exactly 6 chars (UPPERCASE).
 *  • Non-empty defaults for all keys.
 *  • This file is intentionally scoped to ICM only.
 **************************************************************/
#pragma once
// ============================================================
// ICM — Pin Assignments (compile-time defaults) AND
//       NVS keys (so the map can be overridden in the field).
// Keys are 6-char UPPERCASE under PIN namespace.
// ============================================================
// SD card (SPI)
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

// UART to PMS
#define PIN_PMS_RX_DEFAULT    16
#define PIN_PMS_TX_DEFAULT    17
#define PMS_BAUD_DEFAULT      115200
#define NVS_PIN_PMS_RX            "PWRRX_"
#define NVS_PIN_PMS_TX            "PWRTX_"
#define NVS_PIN_PMS_BAUD          "PWRBD_"

// Fan PWM
#define PIN_FAN_PWM_DEFAULT   8
#define NVS_PIN_FAN_PWM           "FANPWM"

// Onboard RGB (status)
#define PIN_LED_R_DEFAULT     5
#define PIN_LED_G_DEFAULT     6
#define PIN_LED_B_DEFAULT     7
#define NVS_PIN_LED_R             "LEDR__"
#define NVS_PIN_LED_G             "LEDG__"
#define NVS_PIN_LED_B             "LEDB__"

// RTC / I2C
#define PIN_RTC_INT_DEFAULT   19
#define PIN_RTC_32K_DEFAULT   20
#define PIN_I2C_SCL_DEFAULT   4
#define PIN_I2C_SDA_DEFAULT   5
#define PIN_RTC_RST_DEFAULT   40
#define NVS_PIN_RTC_INT           "RTCINT"
#define NVS_PIN_RTC_32K           "RTC32K"
#define NVS_PIN_I2C_SCL           "I2CSCL"
#define NVS_PIN_I2C_SDA           "I2CSDA"
#define NVS_PIN_RTC_RST           "RTCRST"
#define NVS_RTC_MODEL_KEY             "RTCMOD"
#define RTC_MODEL_DEFAULT            "DS3231MZ+TRL"

// Temperature sensor (DS18B20) & Buzzer
#define TEMP_SENSOR_PULLUP_DEFAULT true
#define PIN_TS_GPIO_DEFAULT   18
#define PIN_BUZ_GPIO_DEFAULT  3
#define NVS_PIN_TS_GPIO           "TSGPIO"
#define NVS_PIN_BUZ_GPIO          "BZGPIO"
#define TEMP_SENSOR_PULLUP_KEY    "TMPULL"

// Buzzer active-high (logic polarity)
#define BUZ_AH_DEFAULT        1
#define NVS_PIN_BUZ_AH            "BZAH__"

// Optional: buzzer feedback enable
#define BUZ_FBK_DEFAULT       1
#define NVS_PIN_BUZ_FBK           "BZFEED"

/* Implementation notes:
   - At boot, ICM should attempt to read each PIN:* key from NVS;
     if not present, it should write the default value, then use it.
   - This allows field override without rebuilding firmware.
   - All keys are exactly 6 chars and uppercase to match project rules.
*/

// End of ICM_PinsNVS.h
