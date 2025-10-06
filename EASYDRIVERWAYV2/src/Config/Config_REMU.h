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

// Log mapping
#ifndef LOGFILE_PATH
  #define LOGFILE_PATH            LOGFILE_PATH_REMU
#endif
#ifndef LOG_FILE_PATH_PREFIX
  #define LOG_FILE_PATH_PREFIX    LOG_FILE_PREFIX_REMU
#endif

/** Global defaults (device-level) */
#define RPULSE_MS_KEY   "RPULMS"   // u16: default ON pulse for all relays (ms)
#define RPULSE_MS_DEF   500

#define RHOLD_MS_KEY    "RHLDMS"   // u16: default max ON (safety cap) (ms)
#define RHOLD_MS_DEF    30000

#define RREP_MS_KEY     "RREPMS"   // u16: report cadence (ms)
#define RREP_MS_DEF     1000

// Interlock groups as JSON array (device-level)
#define RILOCK_JS_KEY   "RILCKJ"   // json: e.g. [[0,1],[2,3,4]]
#define RILOCK_JS_DEF   "[]"

/** Per-virtual (prefix + index 0..RCOUNT-1) overrides */
#define RPULSE_MS_PFX   "RPLMS_"   // u16 per relay ON pulse override
#define RHOLD_MS_PFX    "RHOMS_"   // u16 per relay HOLD cap override
// (Boundary mapping per virtual relay is handled by formats in NVSConfig.h)

#endif /* CONFIG_REMU_H */
