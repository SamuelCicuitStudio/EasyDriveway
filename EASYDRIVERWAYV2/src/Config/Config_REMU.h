/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Config_REMU.h
 *  Purpose     : Relay Emulator — NVS keys (6-char) + defaults.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef CONFIG_REMU_H
#define CONFIG_REMU_H

#include "Config_Common.h"

// Module log mapping (from common)
#ifndef LOGFILE_PATH
#define LOGFILE_PATH            LOGFILE_PATH_REMU
#endif
#ifndef LOG_FILE_PATH_PREFIX
#define LOG_FILE_PATH_PREFIX    LOG_FILE_PREFIX_REMU
#endif

/** @section REMU role keys (not duplicating NVSConfig) */
// RCOUNT exists in NVSConfig.h (default 16).

/** @section Default pulse for all virtual relays (ms) */
#define RPULSE_MS_KEY               "RPULMS"
#define RPULSE_MS_DEF               500

/** @section Interlock groups bitmap (JSON) */
#define RILOCK_JS_KEY               "RILCKJ"
#define RILOCK_JS_DEF               "[]"

/** @section Report cadence (ms) */
#define RREP_MS_KEY                 "RREPMS"
#define RREP_MS_DEF                 1000

#endif /* CONFIG_REMU_H */
