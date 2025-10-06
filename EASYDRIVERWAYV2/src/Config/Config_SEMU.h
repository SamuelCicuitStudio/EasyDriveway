/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Config_SEMU.h
 *  Purpose     : Sensor Emulator — NVS keys (6-char) + defaults.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef CONFIG_SEMU_H
#define CONFIG_SEMU_H

#include "Config_Common.h"

// Module log mapping (from common)
#ifndef LOGFILE_PATH
#define LOGFILE_PATH            LOGFILE_PATH_SEMU
#endif
#ifndef LOG_FILE_PATH_PREFIX
#define LOG_FILE_PATH_PREFIX    LOG_FILE_PREFIX_SEMU
#endif

/** @section SEMU role keys (not duplicating NVSConfig) */
// SCOUNT exists in NVSConfig.h (default 8).

/** @section Per-virtual default relay pulse (ms) */
#define VON_MS_KEY                  "VONMS_"
#define VON_MS_DEF                  600

/** @section Virtual lead behavior */
#define VLEAD_CT_KEY                "VLDCT_"
#define VLEAD_MS_KEY                "VLDMS_"
#define VLEAD_CT_DEF                3
#define VLEAD_MS_DEF                250

/** @section Emit env model for each virtual sensor (0/1) */
#define VENV_EN_KEY                 "VENVEN"
#define VENV_EN_DEF                 1

// Neighbor & mapping arrays exist in NVSConfig.h

#endif /* CONFIG_SEMU_H */
