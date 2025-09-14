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
// [4] Networking (compile time constants, not NVS)
// ============================================================================
#define LOCAL_IP                    IPAddress(192, 168, 4, 1)
#define GATEWAY                     IPAddress(192, 168, 4, 1)
#define SUBNET                      IPAddress(255, 255, 255, 0)
#define TIMEOFFSET_SECONDS          3600
#define NTP_SERVER                  "pool.ntp.org"
#define NTP_UPDATE_INTERVAL_MS      60000

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

// Optional boot mode (NVS boolean)
#define SERIAL_ONLY_FLAG_KEY        "SRLONL"
#define SERIAL_ONLY_FLAG_DEFAULT    false
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
// ───────────────────────────────────────────────
// Device operational states (optional enum)
// ───────────────────────────────────────────────
enum class DeviceState {
    Idle,
    Running,
    Error,
    Shutdown
};

#endif // CONFIG_H
