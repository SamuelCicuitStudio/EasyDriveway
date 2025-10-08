/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Config_REL.h
 *  Purpose     : Relay role — NVS keys (6-char) + defaults.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef CONFIG_REL_H
#define CONFIG_REL_H

#include "Config_Common.h"

// Module log mapping (from common)
#ifndef LOGFILE_PATH
#define LOGFILE_PATH            LOGFILE_PATH_REL
#endif
#ifndef LOG_FILE_PATH_PREFIX
#define LOG_FILE_PATH_PREFIX    LOG_FILE_PREFIX_REL
#endif

/** @section Pairing state */
#define REL_PAIRING_KEY   "RLYPRG"
#define REL_PAIRED_KEY    "RLYPRD"
#define REL_PAIRING_DEF   1
#define REL_PAIRED_DEF    0

/** @section Relay behavior */
#define PULSE_MS_KEY                "PULSMS"   // ON pulse (ms)
#define HOLD_MS_KEY                 "HOLDMS"   // max ON (ms)
#define PULSE_MS_DEFAULT            500
#define HOLD_MS_DEFAULT             30000

/** @section Interlock (0/1) */
#define INTERLCK_KEY                "INTRLC"
#define INTERLCK_DEFAULT            1

/** @section Thermal guard limit (°C) */
#define RTLIM_C_KEY                 "RTLIMC"
#define RTLIM_C_DEFAULT             80

// Boundary mapping keys live in NVSConfig.h (SAMAC/SATOK/SBMAC/SBTOK/SPLIT_)

#endif /* CONFIG_REL_H */
