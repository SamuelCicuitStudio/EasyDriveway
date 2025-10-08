/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Config_ICM.h
 *  Purpose     : NVS keys (6-char) + defaults for ICM role only.
 *                No hardware pin names here.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef CONFIG_ICM_H
#define CONFIG_ICM_H

#include "Config_Common.h"

// Module log mapping (from common)
#ifndef LOGFILE_PATH
#define LOGFILE_PATH            LOGFILE_PATH_ICM
#endif
#ifndef LOG_FILE_PATH_PREFIX
#define LOG_FILE_PATH_PREFIX    LOG_FILE_PREFIX_ICM
#endif

/** @section ICM compile-time extras */
#define ADVERTISING_TIMEOUT_MS  60000
#define ADVERTISING_TIMEOUT     60000

/** @section ICM NVS keys (NOT in NVSConfig.h) */
// UI theme
#define ICM_UI_THM_KEY   "UITHEM"    // "dark" | "light"
#define ICM_UI_THM_DEF   "dark"

// Topology sequence counter (u32)
#define ICM_SEQ_KEY      "TOSEQ_"
#define ICM_SEQ_DEF      1

// Peer cache TTL (sec, u16)
#define ICM_PTTL_KEY     "PEERTL"
#define ICM_PTTL_DEF     300

// Max peers (u16)
#define ICM_PMAX_KEY     "PEERMX"
#define ICM_PMAX_DEF     64

// Auto-save topology (0/1)
#define ICM_TSAVE_KEY    "TPSAVE"
#define ICM_TSAVE_DEF    1

// Export format
#define ICM_XFMT_KEY     "XPFMT_"
#define ICM_XFMT_DEF     "json"

#endif /* CONFIG_ICM_H */
