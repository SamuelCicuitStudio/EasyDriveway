/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : Config.h
 *  Purpose     : Centralized configuration keys, defaults, and
 *                hardware pin assignment mapping for ICM firmware.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 **************************************************************/

#ifndef CONFIG_H
#define CONFIG_H

// -----------------------------------------------------------------------------
//  [1] Device Identity & Core Keys
// -----------------------------------------------------------------------------
#define DEVICE_KIND                          "ICM"
#define DEVICE_ID_KEY                        "DEVICE"
#define SECRET_KEY                           "ICMCONFIG"
#define COUNTER_KEY                          "CTRR"
#define RESET_FLAG_KEY                       "RSTFLG"
#define GOTO_CONFIG_KEY                      "GTCFG"

// -----------------------------------------------------------------------------
//  [2] Wireless / BLE / AP Keys (Preferences)
// -----------------------------------------------------------------------------
#define DEVICE_BLE_NAME_KEY                  "BLENAM"
#define DEVICE_BLE_AUTH_PASS_KEY             "BLPSWD"
#define DEVICE_WIFI_HOTSPOT_NAME_KEY         "WIFNAM"
#define DEVICE_AP_AUTH_PASS_KEY              "APPASS"
#define BLE_CONNECTION_STATE_KEY             "BLECON"

// -----------------------------------------------------------------------------
//  [3] Timekeeping Keys (Preferences)
// -----------------------------------------------------------------------------
#define LAST_TIME_SAVED_KEY                  "LSTTIM"
#define CURRENT_TIME_SAVED_KEY               "CURTIM"
#define PASS_PIN_KEY                         "PINCOD"

// -----------------------------------------------------------------------------
//  [4] Defaults (Preferences)
// -----------------------------------------------------------------------------
#define DEVICE_ID_DEFAULT                    "ICM01"
#define DEVICE_WIFI_HOTSPOT_NAME_DEFAULT     "ICM_"
#define DEVICE_BLE_NAME_DEFAULT              "ICM"
#define DEVICE_BLE_AUTH_PASS_DEFAULT         123457
#define DEVICE_AP_AUTH_PASS_DEFAULT          "12345678"
#define RESET_FLAG_DEFAULT                   false
#define BLE_CONNECTION_STATE_DEFAULT         false
#define LAST_TIME_SAVED_DEFAULT              1736121600
#define CURRENT_TIME_SAVED_DEFAULT           1736121600
#define PASS_PIN_DEFAULT                     "12345678"

// -----------------------------------------------------------------------------
//  [5] Networking / NTP
// -----------------------------------------------------------------------------
#define LOCAL_IP                              IPAddress(192, 168, 4, 1)
#define GATEWAY                               IPAddress(192, 168, 4, 1)
#define SUBNET                                IPAddress(255, 255, 255, 0)
#define TIMEOFFSET_SECONDS                    3600
#define NTP_SERVER                            "pool.ntp.org"
#define NTP_UPDATE_INTERVAL_MS                60000
#define HOTSPOT_WIFI_CHANNEL                  6
#define ESPNOW_PEER_CHANNEL                   0
// ---- WiFi / ESP-NOW persisted config ----
// ≤6-char NVS keys (rule respected)
#define ESPNOW_CH_KEY          "ESCHNL"   // stored Wi-Fi/ESPNOW channel
#define ESPNOW_MD_KEY          "ESMODE"   // stored system mode (0=AUTO,1=MAN)

// Compile-time defaults
#define ESPNOW_CH_DEFAULT      1          // default Wi-Fi channel
#define ESPNOW_MD_DEFAULT      0U  // from CommandAPI.h (0)
// -----------------------------------------------------------------------------
//  [6] Debug / Logging / System
// -----------------------------------------------------------------------------
#define DEBUGMODE                             true
#define SERIAL_BAUD_RATE                      921600
#define SLEEP_TIMER_MS                        60000
#define LOGFILE_PATH                          "/Log/log.json"
#define LOG_FILE_PATH_PREFIX                  "/logs/system_"
#define SLAVE_CONFIG_PATH                     "/config/SlaveConfig.json"
#define CONFIG_PARTITION                      "config"
#define BOOT_SW_PIN                           0

// -----------------------------------------------------------------------------
//  [7] BLE GATT UUIDs / Advertising
// -----------------------------------------------------------------------------
#define BATTERY_SERVICE                       BLEUUID((uint16_t)0x180F)
#define BATTERY_LEVEL_CHARACTERISTIC_UUID     BLEUUID((uint16_t)0x2A19)
#define BATTERY_LEVEL_DESCRIPTOR_UUID         BLEUUID((uint16_t)0x2A19)
#define SERVICE_UUID                          "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define OLD_APP_CHAR_UUID                     "4a37b01d-364e-44d9-9335-770e2025b29f"
#define ADVERTISING_TIMEOUT_MS                60000
#define ADVERTISING_TIMEOUT                   60000

// -----------------------------------------------------------------------------
//  [8] ICM Hardware Pin Assignment KEYS (stored in NVS/Preferences)
//      Use these keys to persist board pin mapping without recompiling.
// -----------------------------------------------------------------------------
#define SD_CARD_MODEL_KEY                     "SDMODL"
#define SD_MISO_PIN_KEY                       "PSDMISO"
#define SD_MOSI_PIN_KEY                       "PSDMOSI"
#define SD_CS_PIN_KEY                         "PSDCS"
#define SD_SCK_PIN_KEY                        "PSDSCK"

#define PWR_UART_RX_PIN_KEY                   "PPWRRX"
#define PWR_UART_TX_PIN_KEY                   "PPWRTX"
#define PWR_UART_BAUD_KEY                     "PPWRBD"

#define FAN_PWM_PIN_KEY                       "PFANPW"

#define LED_R_PIN_KEY                         "PLED_R"
#define LED_G_PIN_KEY                         "PLED_G"
#define LED_B_PIN_KEY                         "PLED_B"

#define RTC_MODEL_KEY                         "RTCMODL"
#define RTC_INT_PIN_KEY                       "PRTCINT"
#define RTC_32K_PIN_KEY                       "PRTC32K"
#define I2C_SCL_PIN_KEY                       "PI2CSCL"
#define I2C_SDA_PIN_KEY                       "PI2CSDA"
#define RTC_RST_PIN_KEY                       "PRTCRST"

// --- NEW: Sensors & Buzzer (persisted keys) ---
#define TEMP_SENSOR_MODEL_KEY                 "TSMODL"   // e.g. "DS18B20"
#define TEMP_SENSOR_TYPE_KEY                  "TSTYPE"   // e.g. "ONEWIRE"
#define TEMP_SENSOR_PIN_KEY                   "PTSENS"   // 1-Wire data pin
#define TEMP_SENSOR_PULLUP_KEY                "TSPULL"   // 0/1 (internal pull-up enable hint)

#define BUZZER_MODEL_KEY                      "BZMODL"   // e.g. "YS-MBZ12085C05R42"
#define BUZZER_PIN_KEY                        "PBZZR"    // buzzer drive pin
#define BUZZER_ACTIVE_HIGH_KEY                "PBZAH"    // 1=active-high, 0=active-low

// -----------------------------------------------------------------------------
//  [9] ICM Hardware Pin Default Assignments (Board Rev A1)
//      These are compile-time defaults; firmware may override via the KEYS above.
// -----------------------------------------------------------------------------
#define SD_CARD_MODEL_DEFAULT                 "MKDV8GIL-AST"
#define SD_MISO_PIN_DEFAULT                   39
#define SD_MOSI_PIN_DEFAULT                   38
#define SD_CS_PIN_DEFAULT                     41
#define SD_SCK_PIN_DEFAULT                    42

#define PWR_UART_RX_PIN_DEFAULT               16
#define PWR_UART_TX_PIN_DEFAULT               17
#define PWR_UART_BAUD_DEFAULT                 115200

#define FAN_PWM_PIN_DEFAULT                   8

// --- NEW: Buzzer defaults ---
#define BUZZER_MODEL_DEFAULT                  "YS-MBZ12085C05R42"
#define BUZZER_PIN_DEFAULT                    3
#define BUZZER_ACTIVE_HIGH_DEFAULT            1  // drive HIGH to sound

// --- NEW: DS18B20 temperature sensor defaults ---
#define TEMP_SENSOR_MODEL_DEFAULT             "DS18B20"
#define TEMP_SENSOR_TYPE_DEFAULT              "ONEWIRE"
#define TEMP_SENSOR_PIN_DEFAULT               18
#define TEMP_SENSOR_PULLUP_DEFAULT            1  // enable internal pull-up hint (external 4.7k recommended)

// Keep LED defaults
#define LED_R_PIN_DEFAULT                     5
#define LED_G_PIN_DEFAULT                     6
#define LED_B_PIN_DEFAULT                     7

#define RTC_MODEL_DEFAULT                     "DS3231MZ+TRL"
#define RTC_INT_PIN_DEFAULT                   19
#define RTC_32K_PIN_DEFAULT                   20
#define I2C_SCL_PIN_DEFAULT                   4
#define I2C_SDA_PIN_DEFAULT                   5
#define RTC_RST_PIN_DEFAULT                   40

// -----------------------------------------------------------------------------
//  [10] Backward-compatibility aliases (optional, for existing code)
// -----------------------------------------------------------------------------
#define BUZZER_PIN                             BUZZER_PIN_DEFAULT
#define TEMP_SENSOR_PIN                        TEMP_SENSOR_PIN_DEFAULT

#endif // CONFIG_H
