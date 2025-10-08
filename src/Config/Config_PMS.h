/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Config_PMS.h
 *  Purpose     : PMS NVS keys (6-char) + defaults (no pins).
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef CONFIG_PMS_H
#define CONFIG_PMS_H

#include "Config_Common.h"

// Module log mapping (from common)
#ifndef LOGFILE_PATH
#define LOGFILE_PATH            LOGFILE_PATH_PMS
#endif
#ifndef LOG_FILE_PATH_PREFIX
#define LOG_FILE_PATH_PREFIX    LOG_FILE_PREFIX_PMS
#endif

/** @section Pairing state (booleans; 1 byte) */
#define PMS_PAIRING_KEY   "PMSPRG"
#define PMS_PAIRED_KEY    "PMSPRD"
#define PMS_PAIRING_DEF   1
#define PMS_PAIRED_DEF    0

/** @section Measurement scaling & thresholds */
#define V48_SCALE_NUM_KEY           "V48SN"
#define V48_SCALE_DEN_KEY           "V48SD"
#define VBAT_SCALE_NUM_KEY          "VBTSN"
#define VBAT_SCALE_DEN_KEY          "VBTSD"
#define V48_SCALE_NUM_DEFAULT       1
#define V48_SCALE_DEN_DEFAULT       1
#define VBAT_SCALE_NUM_DEFAULT      1
#define VBAT_SCALE_DEN_DEFAULT      1

#define VBUS_OVP_MV_KEY             "VBOVP"
#define VBUS_UVP_MV_KEY             "VBUVP"
#define IBUS_OCP_MA_KEY             "BIOCP"
#define VBAT_OVP_MV_KEY             "VBOVB"
#define VBAT_UVP_MV_KEY             "VBUVB"
#define IBAT_OCP_MA_KEY             "BAOCP"
#define OTP_C_KEY                   "OTPC_"
#define VBUS_OVP_MV_DEFAULT         56000
#define VBUS_UVP_MV_DEFAULT         36000
#define IBUS_OCP_MA_DEFAULT         20000
#define VBAT_OVP_MV_DEFAULT         14600
#define VBAT_UVP_MV_DEFAULT         11000
#define IBAT_OCP_MA_DEFAULT         10000
#define OTP_C_DEFAULT               85

/** @section Telemetry cadence & smoothing */
#define PMS_TEL_MS_KEY              "TELMSP"
#define PMS_TEL_MS_DEFAULT          200
#define PMS_REP_MS_KEY              "REPMS_"
#define PMS_REP_MS_DEFAULT          1000
#define PMS_HB_MS_KEY               "HBPMS_"
#define PMS_HB_MS_DEFAULT           3000
#define PMS_SMOOTH_KEY              "SMOOTH"
#define PMS_SMOOTH_DEFAULT          20

/** @section Source policy */
#define PWR_WMIN_KEY                "PWMINV"   // mV
#define PWR_BMIN_KEY                "PBMINV"   // mV
#define PWR_WMIN_DEF                42000
#define PWR_BMIN_DEF                11000

/** @section Fan/buzzer */
#define FAN_ON_C_KEY                "FANONC"
#define FAN_OFF_C_KEY               "FANOFF"
#define FAN_ON_C_DEFAULT            55
#define FAN_OFF_C_DEFAULT           45
#define BUZZER_ENABLE_KEY           "BUZEN"
#define BUZZER_VOLUME_KEY           "BUZVOL"
#define BUZZER_ENABLE_DEFAULT       1
#define BUZZER_VOLUME_DEFAULT       3

#endif /* CONFIG_PMS_H */
