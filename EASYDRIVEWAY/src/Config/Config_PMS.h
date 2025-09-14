/**************************************************************
 *  Project     : easydriveway
 *  File        : Config_PMS.h — PMS-only configuration
 *  Purpose     : Centralized PMS configuration keys, defaults,
 *                and PMS hardware pin mapping include.
 *                Keys are EXACTLY 6 chars (UPPERCASE).
 *
 *  Notes:
 *   • This header is for the PMS role only (defines NVS_ROLE_PMS).
 *   • Admission tokens and ESPNOW protocol are defined in Now_API.h.
 *   • Pins are declared in PMS_PinsNVS.h (NVS overrides allowed).
 **************************************************************/

#pragma once


// ============================================================================
// [1] Time / NTP (constants; not NVS keys)
// ============================================================================
#define TIMEOFFSET_SECONDS          3600          // UTC+1 default
#define NTP_SERVER                  "pool.ntp.org"
#define NTP_UPDATE_INTERVAL_MS      60000

// ============================================================================
// [2] Debug / Logging / System (some are constants, one NVS key below)
// ============================================================================
#define DEBUGMODE                   true
#define SERIAL_BAUD_RATE            921600
#define LOGFILE_PATH                "/logs/pms.json"
#define LOG_FILE_PATH_PREFIX        "/logs/pms_"
#define CONFIG_PARTITION            "config"      // NVS partition name for config

// Optional boot mode (NVS boolean)
#define SERIAL_ONLY_FLAG_KEY        "SRLONL"
#define SERIAL_ONLY_FLAG_DEFAULT    false

// ============================================================================
// [3] Measurement Scaling (ADC → real units) & Fault Thresholds (NVS keys)
//      Scalers are NUM/DEN; defaults are 1/1 (identity).
// ============================================================================
// Scaling keys (mV/mA fixed-point conversions)
#define V48_SCALE_NUM_KEY           "V48SN"   // uint32_t
#define V48_SCALE_DEN_KEY           "V48SD"   // uint32_t
#define VBAT_SCALE_NUM_KEY          "VBTSN"   // uint32_t
#define VBAT_SCALE_DEN_KEY          "VBTSD"   // uint32_t

#define V48_SCALE_NUM_DEFAULT       1
#define V48_SCALE_DEN_DEFAULT       1
#define VBAT_SCALE_NUM_DEFAULT      1
#define VBAT_SCALE_DEN_DEFAULT      1

// Fault thresholds (mV / mA / °C)
#define VBUS_OVP_MV_KEY             "VBOVP"   // uint32_t
#define VBUS_UVP_MV_KEY             "VBUVP"   // uint32_t
#define IBUS_OCP_MA_KEY             "BIOCP"   // uint32_t
#define VBAT_OVP_MV_KEY             "VBOVB"   // uint32_t
#define VBAT_UVP_MV_KEY             "VBUVB"   // uint32_t
#define IBAT_OCP_MA_KEY             "BAOCP"   // uint32_t
#define OTP_C_KEY                   "OTPC_"   // uint32_t (deg C limit)

#define VBUS_OVP_MV_DEFAULT         56000     // 56 V
#define VBUS_UVP_MV_DEFAULT         36000     // 36 V
#define IBUS_OCP_MA_DEFAULT         20000     // 20 A
#define VBAT_OVP_MV_DEFAULT         14600     // ~4S Li-ion max
#define VBAT_UVP_MV_DEFAULT         11000     // UVP ~11.0 V
#define IBAT_OCP_MA_DEFAULT         10000     // 10 A
#define OTP_C_DEFAULT               85        // °C (board/charger)

// ============================================================================
// [4] Telemetry/Report cadence (PMS → ICM), smoothing, heartbeat (NVS keys)
// ============================================================================
#define PMS_TEL_MS_KEY              "TELMSP"  // telemetry sample period (ms)
#define PMS_TEL_MS_DEFAULT          200

#define PMS_REP_MS_KEY              "REPMS_"  // PWR_REP interval (ms)
#define PMS_REP_MS_DEFAULT          1000

#define PMS_HB_MS_KEY               "HBPMS_"  // heartbeat/status push (ms)
#define PMS_HB_MS_DEFAULT           3000

// Simple IIR smoothing (0..100 %) for measurements
#define PMS_SMOOTH_KEY              "SMOOTH"
#define PMS_SMOOTH_DEFAULT          20       // 20% new, 80% old

// ============================================================================
// [5] Task sizing for PMS firmware (constants)
// ============================================================================
#define SWITCH_TASK_STACK_SIZE      2048
#define SWITCH_TASK_CORE            PRO_CPU_NUM
#define SWITCH_TASK_PRIORITY        1

// ============================================================================
// [6] Compatibility aliases (kept for old includes)
// ============================================================================
#define SLAVE_CONFIG_PATH           "/config/PMS_Config.json"
#define LOGFILE_PATH_COMPAT         LOGFILE_PATH
#define LOG_FILE_PATH_PREFIX_COMPAT LOG_FILE_PATH_PREFIX
// ============================================================================
// [2b] PMS pairing state (NVS booleans; 1 byte each)
// ============================================================================
#define PMS_PAIRING_KEY   "PMSPRG"   // in "pairing mode" (accept PR_* without token)
#define PMS_PAIRED_KEY    "PMSPRD"   // device has been paired (token+ICM MAC present)

// Defaults:
#define PMS_PAIRING_DEF   1          // ship/boot in pairing mode by default
#define PMS_PAIRED_DEF    0          // not paired by default

