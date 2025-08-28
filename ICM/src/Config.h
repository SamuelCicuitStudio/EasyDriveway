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
 **************************************************************/

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// [0] Device kind (compile-time label, not an NVS key)
// ============================================================================
#define DEVICE_KIND                 "ICM"

// ============================================================================
// [1] Core Identity / Flags  (NVS keys ≤ 6 chars)
// ============================================================================
#define DEVICE_ID_KEY               "DEVICE"   // 6
#define COUNTER_KEY                 "CTRR"     // 4
#define RESET_FLAG_KEY              "RSTFLG"   // 6
#define GOTO_CONFIG_KEY             "GTCFG"    // 4

// Defaults
#define DEVICE_ID_DEFAULT           "ICM01"
#define RESET_FLAG_DEFAULT          false
// [2] Identity (NVS keys ≤ 6 chars)
#define DEV_FNAME_KEY    "FNAME"   // Friendly name shown in UI
#define DEV_HOST_KEY     WIFI_STA_HOST_KEY  // reuse existing "STAHNM"
#define DEV_ID_KEY       DEVICE_ID_KEY      // reuse existing "DEVICE"

// [3] Versions (NVS keys ≤ 6 chars)
#define FW_VER_KEY       "FWVER"   // Firmware version string
#define SW_VER_KEY       "SWVER"   // Software version string
#define HW_VER_KEY       "HWVER"   // Hardware rev string, e.g., "A1"
#define BUILD_STR_KEY    "BUILD"   // Build date/hash string

// Defaults (customize as needed)
#define DEV_FNAME_DEFAULT    "ICM"
#define FW_VER_DEFAULT       "1.0.0"
#define SW_VER_DEFAULT       "1.0.0"
#define HW_VER_DEFAULT       "A1"
#define BUILD_STR_DEFAULT    ""

// ============================================================================
// [2] Wireless & BLE (NVS keys ≤ 6 chars)
// ============================================================================
// 2.1 BLE
#define DEVICE_BLE_NAME_KEY         "BLENAM"   // 6
#define DEVICE_BLE_AUTH_PASS_KEY    "BLPSWD"   // 6
#define BLE_CONNECTION_STATE_KEY    "BLECON"   // 6

#define DEVICE_BLE_NAME_DEFAULT     "ICM"
#define DEVICE_BLE_AUTH_PASS_DEFAULT 123457
#define BLE_CONNECTION_STATE_DEFAULT false

// 2.2 Wi-Fi AP (Hotspot)
#define DEVICE_WIFI_HOTSPOT_NAME_KEY "WIFNAM"  // 6
#define DEVICE_AP_AUTH_PASS_KEY       "APPASS" // 6

#define DEVICE_WIFI_HOTSPOT_NAME_DEFAULT "ICM_"
#define DEVICE_AP_AUTH_PASS_DEFAULT      "12345678"

// 2.3 Wi-Fi Station (STA)
#define WIFI_STA_SSID_KEY           "STASSI"   // 6
#define WIFI_STA_PASS_KEY           "STAPSK"   // 6
#define WIFI_STA_HOST_KEY           "STAHNM"   // 6
#define WIFI_STA_DHCP_KEY           "STADHC"   // 6  (1=DHCP, 0=Static; reserved for future)

#define WIFI_STA_SSID_DEFAULT       ""         // empty by default
#define WIFI_STA_PASS_DEFAULT       ""         // empty by default
#define WIFI_STA_HOST_DEFAULT       "ICM"      // WiFiManager can append MAC tail
#define WIFI_STA_DHCP_DEFAULT       1

// 2.4 ESP-NOW (persisted config)
#define ESPNOW_CH_KEY               "ESCHNL"   // 6   Wi-Fi/ESP-NOW channel
#define ESPNOW_MD_KEY               "ESMODE"   // 6   0=AUTO,1=MANUAL

#define ESPNOW_CH_DEFAULT           1
#define ESPNOW_MD_DEFAULT           0U

// 2.5 System Topology
#define SYS_TOPOLOGY_KEY     "TOPOLJ"
#define SYS_TOPOLOGY_DEFAULT "{}"
// ============================================================================
// [3] Timekeeping & Security (NVS keys ≤ 6 chars)
// ============================================================================
// ...existing keys...

// --- Web UI session credentials (stored in NVS) ---
#define WEB_USER_KEY                "SEUSER"   // 6  session username
#define WEB_PASS_KEY                "SEPASS"   // 6  session password

#define WEB_USER_DEFAULT            "admin"    // default username
#define WEB_PASS_DEFAULT            ""         // default password (empty => set via UI on first login)

// ============================================================================
// [3] Timekeeping & Security (NVS keys ≤ 6 chars)
// ============================================================================
#define LAST_TIME_SAVED_KEY         "LSTTIM"   // 6
#define CURRENT_TIME_SAVED_KEY      "CURTIM"   // 6
#define PASS_PIN_KEY                "PINCOD"   // 6

#define LAST_TIME_SAVED_DEFAULT     1736121600
#define CURRENT_TIME_SAVED_DEFAULT  1736121600
#define PASS_PIN_DEFAULT            "12345678"

// ============================================================================
// [4] Networking (compile time constants, not NVS)
// ============================================================================
#define LOCAL_IP                    IPAddress(192, 168, 4, 1)
#define GATEWAY                     IPAddress(192, 168, 4, 1)
#define SUBNET                      IPAddress(255, 255, 255, 0)
#define TIMEOFFSET_SECONDS          3600
#define NTP_SERVER                  "pool.ntp.org"
#define NTP_UPDATE_INTERVAL_MS      60000
#define HOTSPOT_WIFI_CHANNEL        6
#define ESPNOW_PEER_CHANNEL         0

// ============================================================================
// [5] Debug / Logging / System (compile-time constants)
// ============================================================================
#define DEBUGMODE                   true
#define SERIAL_BAUD_RATE            921600
#define SLEEP_TIMER_MS              60000
#define LOGFILE_PATH                "/Log/log.json"
#define LOG_FILE_PATH_PREFIX        "/logs/system_"
#define SLAVE_CONFIG_PATH           "/config/SlaveConfig.json"
#define CONFIG_PARTITION            "config"
#define BOOT_SW_PIN                 0

// ============================================================================
// [6] BLE GATT / Advertising (compile-time constants)
//   NOTE: UUIDs are compile-time constants (not NVS keys).
//         Update them to your production UUIDs as needed.
// ============================================================================
// Standard Battery service (kept from your original)
#define BATTERY_SERVICE                    BLEUUID((uint16_t)0x180F)
#define BATTERY_LEVEL_CHARACTERISTIC_UUID  BLEUUID((uint16_t)0x2A19)
#define BATTERY_LEVEL_DESCRIPTOR_UUID      BLEUUID((uint16_t)0x2A19)

// Legacy app UUIDs (kept for compatibility)
#define SERVICE_UUID                 "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define OLD_APP_CHAR_UUID            "4a37b01d-364e-44d9-9335-770e2025b29f"

// Advertising config
#define ADVERTISING_TIMEOUT_MS       60000
#define ADVERTISING_TIMEOUT          60000

// -------- ICM BLE Service & Characteristics (add/replace as needed) --------
#define ICM_BLE_SERVICE_UUID   "7e3b0001-3c6b-4c2a-9a4b-icm0serv0001"
#define ICM_CH_STATUS_UUID     "7e3b0002-3c6b-4c2a-9a4b-icm0stat0002"
#define ICM_CH_WIFI_UUID       "7e3b0003-3c6b-4c2a-9a4b-icm0wifi0003"
#define ICM_CH_PEERS_UUID      "7e3b0004-3c6b-4c2a-9a4b-icm0peer0004"
#define ICM_CH_TOPO_UUID       "7e3b0005-3c6b-4c2a-9a4b-icm0topo0005"
#define ICM_CH_SEQ_UUID        "7e3b0006-3c6b-4c2a-9a4b-icm0seq_0006"
#define ICM_CH_POWER_UUID      "7e3b0007-3c6b-4c2a-9a4b-icm0powr0007"
#define ICM_CH_EXPORT_UUID     "7e3b0008-3c6b-4c2a-9a4b-icm0expt0008"
#define ICM_CH_OLDAPP_UUID     "7e3b0009-3c6b-4c2a-9a4b-icm0olda0009"

// ============================================================================
// [7] Hardware Pin Mapping (NVS keys ≤ 6 chars)
//      Persist board pin mapping without recompiling.
// ============================================================================
// 7.1 SD card (SPI)
#define SD_CARD_MODEL_KEY            "SDMODL"  // 6 (model string)
#define SD_MISO_PIN_KEY              "SDMISO"  // 6
#define SD_MOSI_PIN_KEY              "SDMOSI"  // 6
#define SD_CS_PIN_KEY                "SDCS"    // 4
#define SD_SCK_PIN_KEY               "SDSCK"   // 5

// 7.2 External UART to Power Module
#define PWR_UART_RX_PIN_KEY          "PWRRX"   // 5
#define PWR_UART_TX_PIN_KEY          "PWRTX"   // 5
#define PWR_UART_BAUD_KEY            "PWRBD"   // 5

// 7.3 Fan & RGB status LEDs
#define FAN_PWM_PIN_KEY              "FANPWM"  // 6
#define LED_R_PIN_KEY                "LEDR"    // 4
#define LED_G_PIN_KEY                "LEDG"    // 4
#define LED_B_PIN_KEY                "LEDB"    // 4

// 7.4 RTC / I2C
#define RTC_MODEL_KEY                "RTCMOD"  // 6
#define RTC_INT_PIN_KEY              "RTCINT"  // 6
#define RTC_32K_PIN_KEY              "RTC32K"  // 6
#define I2C_SCL_PIN_KEY              "I2CSCL"  // 6
#define I2C_SDA_PIN_KEY              "I2CSDA"  // 6
#define RTC_RST_PIN_KEY              "RTCRST"  // 6

// 7.5 Sensors & Buzzer
#define TEMP_SENSOR_MODEL_KEY        "TSMODL"  // 6
#define TEMP_SENSOR_TYPE_KEY         "TSTYPE"  // 6
#define TEMP_SENSOR_PIN_KEY          "TSGPIO"  // 6
#define TEMP_SENSOR_PULLUP_KEY       "TSPULL"  // 6

#define BUZZER_MODEL_KEY             "BZMODL"  // 6
#define BUZZER_PIN_KEY               "BZGPIO"  // 6
#define BUZZER_ACTIVE_HIGH_KEY       "BZAH"    // 4
#define BUZZER_FEEDBACK_KEY      "BZFEED"   // 6 chars
#define BUZZER_FEEDBACK_DEFAULT  1          // enabled by default

// ============================================================================
// [8] Hardware Pin Defaults (compile-time)
// ============================================================================
#define SD_CARD_MODEL_DEFAULT        "MKDV8GIL-AST"
#define SD_MISO_PIN_DEFAULT          39
#define SD_MOSI_PIN_DEFAULT          38
#define SD_CS_PIN_DEFAULT            41
#define SD_SCK_PIN_DEFAULT           42

#define PWR_UART_RX_PIN_DEFAULT      16
#define PWR_UART_TX_PIN_DEFAULT      17
#define PWR_UART_BAUD_DEFAULT        115200

#define FAN_PWM_PIN_DEFAULT          8

#define LED_R_PIN_DEFAULT            5
#define LED_G_PIN_DEFAULT            6
#define LED_B_PIN_DEFAULT            7

#define RTC_MODEL_DEFAULT            "DS3231MZ+TRL"
#define RTC_INT_PIN_DEFAULT          19
#define RTC_32K_PIN_DEFAULT          20
#define I2C_SCL_PIN_DEFAULT          4
#define I2C_SDA_PIN_DEFAULT          5
#define RTC_RST_PIN_DEFAULT          40

#define TEMP_SENSOR_MODEL_DEFAULT    "DS18B20"
#define TEMP_SENSOR_TYPE_DEFAULT     "ONEWIRE"
#define TEMP_SENSOR_PIN_DEFAULT      18
#define TEMP_SENSOR_PULLUP_DEFAULT   1   // external 4.7k still recommended

#define BUZZER_MODEL_DEFAULT         "YS-MBZ12085C05R42"
#define BUZZER_PIN_DEFAULT           3
#define BUZZER_ACTIVE_HIGH_DEFAULT   1

// ============================================================================
// [9] Optional: Backward-compatibility aliases (temporary)
// Enable if you need to read legacy values once, then migrate & disable.
// ============================================================================
// #define LEGACY_COMPAT 1
#ifdef LEGACY_COMPAT
  // Example: map old long keys to the new short ones at load time in your ConfigManager.
  // (No string redefines here to avoid accidental double-writes.)
  // OLD → NEW (comment list)
  // "PSDMISO" → "SDMISO"
  // "PSDMOSI" → "SDMOSI"
  // "PLED_R"  → "LEDR"
  // "PLED_G"  → "LEDG"
  // "PLED_B"  → "LEDB"
  // "RTCMODL" → "RTCMOD"
  // "PRTCINT" → "RTCINT"
  // "PRTC32K" → "RTC32K"
  // "PI2CSCL" → "I2CSCL"
  // "PI2CSDA" → "I2CSDA"
  // "PRTCRST" → "RTCRST"
  // "PTSENS"  → "TSGPIO"
  // "PBZZR"   → "BZGPIO"
  // "PBZAH"   → "BZAH"
#endif

#endif // CONFIG_H
