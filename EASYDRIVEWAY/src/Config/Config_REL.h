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
// [6] Relay stack configuration (addresses, thresholds, tuning)
// ============================================================================




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

// ============================================================================
// [2b] Relay pairing state (NVS booleans; 1 byte each)
// ============================================================================
#define REL_PAIRING_KEY   "PMSPRG"   // in "pairing mode" (accept PR_* without token)
#define REL_PAIRED_KEY    "PMSPRD"   // device has been paired (token+ICM MAC present)

// Defaults:
#define REL_PAIRING_DEF   1          // ship/boot in pairing mode by default
#define REL_PAIRED_DEF    0          // not paired by default

