/**************************************************************
 *  Project  : EasyDriveWay - Power Supply Module (PSM)
 *  File     : Config.h
 *  Purpose  : Centralized configuration keys, defaults, and
 *             hardware pin mapping for the PSM firmware.
 *  Notes    : NVS keys are ≤ 6 chars. No external I2C RTC pins;
 *             project uses ESP32 internal RTC only.
 **************************************************************/
#ifndef CONFIG_PSM_H
#define CONFIG_PSM_H

// ============================================================================
// [0] Device kind (compile-time label, not an NVS key)
// ============================================================================
#define DEVICE_KIND                 "PSM"

// ============================================================================
// [1] Identity / Versioning / Flags (NVS ≤ 6 chars)
// ============================================================================
#define DEVICE_ID_KEY               "DEVICE"     // device id
#define COUNTER_KEY                 "CTRR"       // random counter
#define RESET_FLAG_KEY              "RSTFLG"     // reset flag
#define GOTO_CONFIG_KEY             "GTCFG"      // go-to-config flag
#define DEVICE_ID_DEFAULT           "PSM01"
#define RESET_FLAG_DEFAULT          false

// Human-readable identity & versions
#define DEV_FNAME_KEY               "FNAME"
#define FW_VER_KEY                  "FWVER"
#define SW_VER_KEY                  "SWVER"
#define HW_VER_KEY                  "HWVER"
#define BUILD_STR_KEY               "BUILD"
#define DEV_FNAME_DEFAULT           "PSM"
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
#define DEVICE_BLE_NAME_DEFAULT     "PSM"
#define DEVICE_BLE_AUTH_PASS_DEFAULT 123457
#define BLE_CONNECTION_STATE_DEFAULT false

// Wi-Fi AP (Hotspot)
#define DEVICE_WIFI_HOTSPOT_NAME_KEY "WIFNAM"
#define DEVICE_AP_AUTH_PASS_KEY       "APPASS"
#define DEVICE_WIFI_HOTSPOT_NAME_DEFAULT "PSM_"
#define DEVICE_AP_AUTH_PASS_DEFAULT      "12345678"

// Wi-Fi Station (STA)
#define WIFI_STA_SSID_KEY           "STASSI"
#define WIFI_STA_PASS_KEY           "STAPSK"
#define WIFI_STA_HOST_KEY           "STAHNM"
#define WIFI_STA_DHCP_KEY           "STADHC"
#define WIFI_STA_SSID_DEFAULT       ""
#define WIFI_STA_PASS_DEFAULT       ""
#define WIFI_STA_HOST_DEFAULT       "PSM"
#define WIFI_STA_DHCP_DEFAULT       1

// ESP-NOW
#define ESPNOW_CH_KEY               "ESCHNL"
#define ESPNOW_MD_KEY               "ESMODE"     // 0=AUTO, 1=MAN
#define ESPNOW_CH_DEFAULT           1
#define ESPNOW_MD_DEFAULT           0U

// PSM-side persisted mirrors (used by PSMEspNowManager)
#define PSM_ESPNOW_CH_KEY           "PCH"        // mirror of channel
#define PSM_TOKEN16_KEY             "PTOK"       // token hex(16B)
#define PSM_MASTER_MAC_KEY          "PMAC"       // "AA:BB:CC:DD:EE:FF"

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
// [4] System / Logging (compile-time constants)
// ============================================================================
#define DEBUGMODE                   true
#define SERIAL_BAUD_RATE            921600
#define LOGFILE_PATH                "/Log/log.json"
#define LOG_FILE_PATH_PREFIX        "/logs/psm_"
#define CONFIG_PARTITION            "config"
#define BOOT_SW_PIN                 0

// ============================================================================
// [5] Hardware Mapping (NVS keys ≤ 6 chars) + Defaults
// ============================================================================
// 5.1 SD card (SPI) – for ICMLogFS logging
#define SD_CARD_MODEL_KEY           "SDMODL"
#define SD_MISO_PIN_KEY             "SDMISO"
#define SD_MOSI_PIN_KEY             "SDMOSI"
#define SD_CS_PIN_KEY               "SDCS"
#define SD_SCK_PIN_KEY              "SDSCK"
#define SD_CARD_MODEL_DEFAULT       "MKDV8GIL-AST"
#define SD_MISO_PIN_DEFAULT         39
#define SD_MOSI_PIN_DEFAULT         38
#define SD_CS_PIN_DEFAULT           41
#define SD_SCK_PIN_DEFAULT          42

// 5.2 I2C (for charger/monitor ICs)
#define I2C_SCL_PIN_KEY             "I2CSCL"
#define I2C_SDA_PIN_KEY             "I2CSDA"
#define I2C_SCL_PIN_DEFAULT         4
#define I2C_SDA_PIN_DEFAULT         5

// 5.3 User-feedback peripherals (Cooling, RGB LED, Buzzer)
#define FAN_PWM_PIN_KEY             "FANPWM"
#define FAN_PWM_PIN_DEFAULT         8

#define LED_R_PIN_KEY               "LEDR"
#define LED_G_PIN_KEY               "LEDG"
#define LED_B_PIN_KEY               "LEDB"
// Non-conflicting defaults (avoid I2C_SDA=5)
#define LED_R_PIN_DEFAULT           12
#define LED_G_PIN_DEFAULT           13
#define LED_B_PIN_DEFAULT           14

// Buzzer
#define BUZZER_MODEL_KEY            "BZMODL"
#define BUZZER_PIN_KEY              "BZGPIO"
#define BUZZER_ACTIVE_HIGH_KEY      "BZAH"
#define BUZZER_FEEDBACK_KEY         "BZFEED"
#define BUZZER_MODEL_DEFAULT        "YS-MBZ12085C05R42"
#define BUZZER_PIN_DEFAULT          11      // moved off IBATADC=3
#define BUZZER_ACTIVE_HIGH_DEFAULT  1
#define BUZZER_FEEDBACK_DEFAULT     1

// 5.4 Temperature sensor (board)
#define TEMP_SENSOR_MODEL_KEY       "TSMODL"
#define TEMP_SENSOR_TYPE_KEY        "TSTYPE"
#define TEMP_SENSOR_PIN_KEY         "TSGPIO"
#define TEMP_SENSOR_PULLUP_KEY      "TSPULL"
#define TEMP_SENSOR_MODEL_DEFAULT   "DS18B20"
#define TEMP_SENSOR_TYPE_DEFAULT    "ONEWIRE"
#define TEMP_SENSOR_PIN_DEFAULT     18
#define TEMP_SENSOR_PULLUP_DEFAULT  1

// 5.5 Power rails / voltage & current sensing
#define PWR48_EN_PIN_KEY            "P48EN"     // 48V enable
#define PWR5V_EN_PIN_KEY            "P5VEN"     // 5V enable
#define MAINS_SENSE_PIN_KEY         "MSNS"      // mains OK
#define V48_ADC_PIN_KEY             "V48AD"     // 48V bus voltage ADC
#define VBAT_ADC_PIN_KEY            "VBATAD"    // battery voltage ADC
#define I48_ADC_PIN_KEY             "I48AD"     // 48V bus current ADC (ACS781 #1)
#define IBAT_ADC_PIN_KEY            "IBATAD"    // battery current ADC   (ACS781 #2)
#define CHARGER_INT_PIN_KEY         "CHINT"     // charger IRQ/status (opt.)
#define PWR48_EN_PIN_DEFAULT        21
#define PWR5V_EN_PIN_DEFAULT        22
#define MAINS_SENSE_PIN_DEFAULT     23
#define V48_ADC_PIN_DEFAULT         1
#define VBAT_ADC_PIN_DEFAULT        9
#define I48_ADC_PIN_DEFAULT         2
#define IBAT_ADC_PIN_DEFAULT        3
#define CHARGER_INT_PIN_DEFAULT     10

// ============================================================================
// [6] Measurement Scaling & Fault Thresholds (used by PowerManager)
// ============================================================================
// ADC mV -> real mV: (mV * NUM / DEN)
#define V48_SCALE_NUM_KEY           "V48SN"
#define V48_SCALE_DEN_KEY           "V48SD"
#define VBAT_SCALE_NUM_KEY          "VBTSN"
#define VBAT_SCALE_DEN_KEY          "VBTSD"
#define V48_SCALE_NUM_DEFAULT       1
#define V48_SCALE_DEN_DEFAULT       1
#define VBAT_SCALE_NUM_DEFAULT      1
#define VBAT_SCALE_DEN_DEFAULT      1

// Fault thresholds (mV / mA / °C)
#define VBUS_OVP_MV_KEY             "VBOVP"
#define VBUS_UVP_MV_KEY             "VBUVP"
#define IBUS_OCP_MA_KEY             "BIOCP"
#define VBAT_OVP_MV_KEY             "VBOVB"
#define VBAT_UVP_MV_KEY             "VBUVB"
#define IBAT_OCP_MA_KEY             "BAOCP"
#define OTP_C_KEY                   "OTPC"
#define VBUS_OVP_MV_DEFAULT         56000   // 56 V
#define VBUS_UVP_MV_DEFAULT         36000   // 36 V
#define IBUS_OCP_MA_DEFAULT         20000   // 20 A
#define VBAT_OVP_MV_DEFAULT         14600   // ~4S Li-ion max
#define VBAT_UVP_MV_DEFAULT         11000   // UVP ~11.0 V
#define IBAT_OCP_MA_DEFAULT         10000   // 10 A
#define OTP_C_DEFAULT               85      // °C

// ============================================================================
// [7] ACS781 Current Sensors (calibration keys)
// ============================================================================
// 7.0 Generic (applies if per-sensor overrides absent)
#define ACS_MODEL_KEY               "ACSMOD"    // e.g., "ACS781-100B"
#define ACS_VREF_MV_KEY             "AVREF"     // board Vref (mV)
#define ACS_ZERO_MV_KEY             "AZERO"     // zero-current offset (mV)
#define ACS_SENS_UVPA_KEY           "ASNSUV"    // sensitivity (uV/A)
#define ACS_AVG_KEY                 "AAVG"      // samples per read
#define ACS_INV_KEY                 "AINVRT"    // invert sign (0/1)
#define ACS_ATTEN_KEY               "AATTN"     // ADC atten: 0/2.5/6/11 dB
#define ACS_MODEL_DEFAULT           "ACS781-100B-T"
#define ACS_VREF_MV_DEFAULT         3300
#define ACS_ZERO_MV_DEFAULT         1650
#define ACS_SENS_UVPA_DEFAULT       13200      // 13.2 mV/A
#define ACS_AVG_DEFAULT             16
#define ACS_INV_DEFAULT             0
#define ACS_ATTEN_DEFAULT           3

// 7.1 IBUS (48V bus) per-sensor overrides
#define ACS_BUS_MODEL_KEY           "ABMODL"
#define ACS_BUS_VREF_MV_KEY         "ABVREF"
#define ACS_BUS_ZERO_MV_KEY         "ABZERO"
#define ACS_BUS_SENS_UVPA_KEY       "ABSNSU"
#define ACS_BUS_AVG_KEY             "ABAVG"
#define ACS_BUS_INV_KEY             "ABINV"
#define ACS_BUS_ATTEN_KEY           "ABATTN"
#define ACS_BUS_MODEL_DEFAULT       "ACS781-100B-T"
#define ACS_BUS_VREF_MV_DEFAULT     3300
#define ACS_BUS_ZERO_MV_DEFAULT     1650
#define ACS_BUS_SENS_UVPA_DEFAULT   13200
#define ACS_BUS_AVG_DEFAULT         16
#define ACS_BUS_INV_DEFAULT         0
#define ACS_BUS_ATTEN_DEFAULT       3

// 7.2 IBAT (battery) per-sensor overrides
#define ACS_BAT_MODEL_KEY           "BAMODL"
#define ACS_BAT_VREF_MV_KEY         "BAVREF"
#define ACS_BAT_ZERO_MV_KEY         "BAZERO"
#define ACS_BAT_SENS_UVPA_KEY       "BASNSU"
#define ACS_BAT_AVG_KEY             "BAAVG"
#define ACS_BAT_INV_KEY             "BAINV"
#define ACS_BAT_ATTEN_KEY           "BAATTN"
#define ACS_BAT_MODEL_DEFAULT       "ACS781-100B-T"
#define ACS_BAT_VREF_MV_DEFAULT     3300
#define ACS_BAT_ZERO_MV_DEFAULT     1650
#define ACS_BAT_SENS_UVPA_DEFAULT   13200
#define ACS_BAT_AVG_DEFAULT         16
#define ACS_BAT_INV_DEFAULT         0
#define ACS_BAT_ATTEN_DEFAULT       3

// ============================================================================
// [8] Networking (compile-time constants)
// ============================================================================
#define NTP_SERVER                  "pool.ntp.org"
#define NTP_UPDATE_INTERVAL_MS      60000

#endif // CONFIG_PSM_H
