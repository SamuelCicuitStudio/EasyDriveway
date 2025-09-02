/**************************************************************
 *  Project  : EasyDriveWay - Sensor Module (SSM)
 *  File     : Config.h
 *  Purpose  : Centralized configuration keys, defaults, and
 *             hardware pin mapping for the SSM firmware.
 *  Notes    : NVS keys are ≤ 6 chars. No external I2C RTC pins;
 *             project uses ESP32 internal RTC only.
 **************************************************************/
#ifndef CONFIG_SSM_H
#define CONFIG_SSM_H

// ============================================================================
// [0] Device kind (compile-time label, not an NVS key)
// ============================================================================
#define DEVICE_KIND                 "SSM"   // Sensor Subsystem Module

// ============================================================================
// [1] Identity / Versioning / Flags (NVS ≤ 6 chars)
// ============================================================================
#define DEVICE_ID_KEY               "DEVICE"     // device id
#define COUNTER_KEY                 "CTRR"       // random counter
#define RESET_FLAG_KEY              "RSTFLG"     // reset flag
#define GOTO_CONFIG_KEY             "GTCFG"      // go-to-config flag

#define DEVICE_ID_DEFAULT           "SSM01"
#define RESET_FLAG_DEFAULT          false

// Human-readable identity & versions
#define DEV_FNAME_KEY               "FNAME"
#define FW_VER_KEY                  "FWVER"
#define SW_VER_KEY                  "SWVER"
#define HW_VER_KEY                  "HWVER"
#define BUILD_STR_KEY               "BUILD"

#define DEV_FNAME_DEFAULT           "SSM"
#define FW_VER_DEFAULT              "1.0.0"
#define SW_VER_DEFAULT              "1.0.0"
#define HW_VER_DEFAULT              "A1"
#define BUILD_STR_DEFAULT           ""

// ============================================================================
// [2] Connectivity (BLE / Wi-Fi / ESP-NOW)
// ============================================================================
// BLE
#define DEVICE_BLE_NAME_KEY         "BLENAM"
#define DEVICE_BLE_AUTH_PASS_KEY    "BLPSWD"
#define BLE_CONNECTION_STATE_KEY    "BLECON"
#define DEVICE_BLE_NAME_DEFAULT     "SSM"
#define DEVICE_BLE_AUTH_PASS_DEFAULT 123457
#define BLE_CONNECTION_STATE_DEFAULT false

// Wi-Fi AP (Hotspot)
#define DEVICE_WIFI_HOTSPOT_NAME_KEY "WIFNAM"
#define DEVICE_AP_AUTH_PASS_KEY       "APPASS"
#define DEVICE_WIFI_HOTSPOT_NAME_DEFAULT "SSM_"
#define DEVICE_AP_AUTH_PASS_DEFAULT      "12345678"

// Wi-Fi Station (STA)
#define WIFI_STA_SSID_KEY           "STASSI"
#define WIFI_STA_PASS_KEY           "STAPSK"
#define WIFI_STA_HOST_KEY           "STAHNM"
#define WIFI_STA_DHCP_KEY           "STADHC"
#define WIFI_STA_SSID_DEFAULT       ""
#define WIFI_STA_PASS_DEFAULT       ""
#define WIFI_STA_HOST_DEFAULT       "SSM"
#define WIFI_STA_DHCP_DEFAULT       1

// ESP-NOW (generic keys)
#define ESPNOW_CH_KEY               "ESCHNL"
#define ESPNOW_MD_KEY               "ESMODE"     // 0=AUTO, 1=MAN
#define ESPNOW_CH_DEFAULT           1
#define ESPNOW_MD_DEFAULT           0U

// SSM-side persisted mirrors (used by SensorEspNowManager)
#define SSM_ESPNOW_CH_KEY           "SCH"        // mirror of channel
#define SSM_TOKEN16_KEY             "STOK"       // token hex(16B)
#define SSM_MASTER_MAC_KEY          "SMAC"       // "AA:BB:CC:DD:EE:FF"

// Optional web creds
#define WEB_USER_KEY                "SEUSER"
#define WEB_PASS_KEY                "SEPASS"
#define WEB_USER_DEFAULT            "admin"
#define WEB_PASS_DEFAULT            ""

// ============================================================================
// [3] Timekeeping & Security (internal RTC only)
// ============================================================================
#define LAST_TIME_SAVED_KEY         "LSTTIM"
#define CURRENT_TIME_SAVED_KEY      "CURTIM"
#define PASS_PIN_KEY                "PINCOD"
#define LAST_TIME_SAVED_DEFAULT     1736121600
#define CURRENT_TIME_SAVED_DEFAULT  1736121600
#define PASS_PIN_DEFAULT            "12345678"

// ============================================================================
// [4] System / Logging
// ============================================================================
#define DEBUGMODE                   true
#define SERIAL_BAUD_RATE            921600
#define LOGFILE_PATH                "/Log/log.json"
#define LOG_FILE_PATH_PREFIX        "/logs/ssm_"
#define CONFIG_PARTITION            "config"
#define BOOT_SW_PIN                 0

// ============================================================================
// [5] Hardware Mapping (pins & attached peripherals)
// ============================================================================
// 5.1 SD / Log memory (SPI) — SAME PINS, model unchanged
#define SD_CARD_MODEL_KEY           "SDMODL"
#define SD_MISO_PIN_KEY             "SDMISO"
#define SD_MOSI_PIN_KEY             "SDMOSI"
#define SD_CS_PIN_KEY               "SDCS"
#define SD_SCK_PIN_KEY              "SDSCK"

#define SD_CARD_MODEL_DEFAULT       "MKDV8GIL"
#define SD_MISO_PIN_DEFAULT         39
#define SD_MOSI_PIN_DEFAULT         38
#define SD_CS_PIN_DEFAULT           41
#define SD_SCK_PIN_DEFAULT          42

// 5.2 I2C buses
// I2C1 → TF-Luna ×2 (A/B)  **as requested**
#define I2C1_SDA_PIN_KEY            "I1SDA"
#define I2C1_SCL_PIN_KEY            "I1SCL"
#define I2C1_SDA_PIN_DEFAULT        4
#define I2C1_SCL_PIN_DEFAULT        5

// I2C2 → VEML7700-TR + BME280  **as requested**
#define I2C2_SDA_PIN_KEY            "I2SDA"
#define I2C2_SCL_PIN_KEY            "I2SCL"
#define I2C2_SDA_PIN_DEFAULT        16
#define I2C2_SCL_PIN_DEFAULT        17

// 5.3 User-feedback & cooling — SAME pins for fan, RGB, buzzer
// Fan (same pin)
#define FAN_PWM_PIN_KEY             "FANPWM"
#define FAN_PWM_PIN_DEFAULT         8

// RGB LED (same pins; if your previous board shared B with FAN, keep as-is)
#define LED_R_PIN_KEY               "LEDR"
#define LED_G_PIN_KEY               "LEDG"
#define LED_B_PIN_KEY               "LEDB"
#define LED_R_PIN_DEFAULT           6
#define LED_G_PIN_DEFAULT           7
#define LED_B_PIN_DEFAULT           8   // NOTE: keep same as your existing board

// Buzzer (same model/pin)
#define BUZZER_MODEL_KEY            "BZMODL"
#define BUZZER_PIN_KEY              "BZGPIO"
#define BUZZER_ACTIVE_HIGH_KEY      "BZAH"
#define BUZZER_FEEDBACK_KEY         "BZFEED"
#define BUZZER_MODEL_DEFAULT        "YS-MBZ12085C05R42"
#define BUZZER_PIN_DEFAULT          11
#define BUZZER_ACTIVE_HIGH_DEFAULT  1
#define BUZZER_FEEDBACK_DEFAULT     1

// ============================================================================
// [6] Sensor stack configuration (addresses, thresholds, tuning)
// ============================================================================
// 6.1 TF-Luna (I2C mode) — two probes A/B on I2C1
#define TFL_MODE_KEY                "TFLMOD"   // "I2C" | "UART"
#define TFL_MODE_DEFAULT            "I2C"

#define TFL_A_ADDR_KEY              "TFAADR"   // I2C addr for TF-Luna A
#define TFL_B_ADDR_KEY              "TFBADR"   // I2C addr for TF-Luna B
#define TFL_A_ADDR_DEFAULT          0x10
#define TFL_B_ADDR_DEFAULT          0x11

// Range gate for presence (mm)
#define TF_NEAR_MM_KEY              "TFNMM"
#define TF_FAR_MM_KEY               "TFFMM"
#define TF_NEAR_MM_DEFAULT          200       // 0.2 m
#define TF_FAR_MM_DEFAULT           3200      // 3.2 m

// A↔B spacing (mm) used for speed computation
#define AB_SPACING_MM_KEY           "ABSPMM"
#define AB_SPACING_MM_DEFAULT       350

// 6.2 BME280 (I2C2)
#define BME_MODEL_KEY               "BMEMOD"
#define BME_ADDR_KEY                "BMEADR"
#define BME_MODEL_DEFAULT           "BME280"
#define BME_ADDR_DEFAULT            0x76

// 6.3 VEML7700-TR (I2C2)
#define ALS_MODEL_KEY               "ALSMOD"
#define ALS_ADDR_KEY                "ALSADR"
#define ALS_T0_LUX_KEY              "ALS_T0"   // day->night threshold (down-cross)
#define ALS_T1_LUX_KEY              "ALS_T1"   // night->day threshold (up-cross)
#define ALS_MODEL_DEFAULT           "VEML7700-TR"
#define ALS_ADDR_DEFAULT            0x10
#define ALS_T0_LUX_DEFAULT          180
#define ALS_T1_LUX_DEFAULT          300

// 6.4 Fan control thresholds (°C)
#define FAN_ON_C_KEY                "FANONC"
#define FAN_OFF_C_KEY               "FANOFF"
#define FAN_ON_C_DEFAULT            55
#define FAN_OFF_C_DEFAULT           45

// 6.5 Buzzer configuration
#define BUZZER_ENABLE_KEY           "BUZEN"
#define BUZZER_VOLUME_KEY           "BUZVOL"
#define BUZZER_ENABLE_DEFAULT       1
#define BUZZER_VOLUME_DEFAULT       3

// 6.6 Motion timing (TF-Luna confirmation/hold)
#define CONFIRM_MS_KEY              "CONFMS"
#define STOP_MS_KEY                 "STOPMS"
#define CONFIRM_MS_DEFAULT          140
#define STOP_MS_DEFAULT             1200

// ============================================================================
// [7] Networking / Time
// ============================================================================
#define NTP_SERVER                  "pool.ntp.org"
#define NTP_UPDATE_INTERVAL_MS      60000

// ───────────────────────────────────────────────
// Device operational states (optional enum)
// ───────────────────────────────────────────────
enum class DeviceState {
    Idle,
    Running,
    Error,
    Shutdown
};

// Boot option: serial-only CLI
#define SERIAL_ONLY_FLAG_KEY        "SRLONL"  // set true to boot into serial-only CLI

#endif // CONFIG_SSM_H
