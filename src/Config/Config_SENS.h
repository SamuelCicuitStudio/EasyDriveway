/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Config_SENS.h
 *  Purpose     : Sensor role — NVS keys (6-char) + defaults.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef CONFIG_SENS_H
#define CONFIG_SENS_H

#include "Config_Common.h"

// Module log mapping (from common)
#ifndef LOGFILE_PATH
#define LOGFILE_PATH            LOGFILE_PATH_SENS
#endif
#ifndef LOG_FILE_PATH_PREFIX
#define LOG_FILE_PATH_PREFIX    LOG_FILE_PREFIX_SENS
#endif

/** @section Pairing state */
#define SENS_PAIRING_KEY   "SNSPRG"
#define SENS_PAIRED_KEY    "SNSPRD"
#define SENS_PAIRING_DEF   1
#define SENS_PAIRED_DEF    0

/** @section Direction & gating thresholds */
#define TF_NEAR_MM_KEY              "TFNMM"
#define TF_FAR_MM_KEY               "TFFMM"
#define TF_NEAR_MM_DEFAULT          200
#define TF_FAR_MM_DEFAULT           3200

/** @section A↔B spacing (mm) */
#define AB_SPACING_MM_KEY           "ABSPMM"
#define AB_SPACING_MM_DEFAULT       350

/** @section Day/Night hysteresis (lux) */
#define ALS_T0_LUX_KEY              "ALS_T0"
#define ALS_T1_LUX_KEY              "ALS_T1"
#define ALS_T0_LUX_DEFAULT          180
#define ALS_T1_LUX_DEFAULT          300

/** @section Motion windows (ms) */
#define CONFIRM_MS_KEY              "CONFMS"
#define STOP_MS_KEY                 "STOPMS"
#define CONFIRM_MS_DEFAULT          140
#define STOP_MS_DEFAULT             1200

/** @section Relay pulse defaults (ms) */
#define RLY_ON_MS_KEY               "RLYONM"
#define RLY_OFF_MS_KEY              "RLYOFF"
#define RLY_ON_MS_DEFAULT           600
#define RLY_OFF_MS_DEFAULT          0

/** @section Lead behavior */
#define LEAD_CNT_KEY                "LEADCT"   // u8
#define LEAD_STP_MS_KEY             "LEADMS"   // u16
#define LEAD_CNT_DEFAULT            3
#define LEAD_STP_MS_DEFAULT         250

/** @section TFluna Address behavior */
#define TFL_A_ADDR_KEY              "TFAADR"   // I2C addr for TF-Luna A
#define TFL_B_ADDR_KEY              "TFBADR"   // I2C addr for TF-Luna B


// Neighbor & mapping arrays exist in NVSConfig.h

#endif /* CONFIG_SENS_H */
