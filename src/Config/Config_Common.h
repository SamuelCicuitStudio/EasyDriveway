/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Config_Common.h
 *  Purpose     : Common compile-time config shared by all roles.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef CONFIG_COMMON_H
#define CONFIG_COMMON_H


/**
 * @name Global Toggles
 * @brief Safe defaults; modules may override.
 * @{ */
#ifndef DEBUGMODE
#define DEBUGMODE                   true
#endif
#ifndef SERIAL_BAUD_RATE
#define SERIAL_BAUD_RATE            921600
#endif
#ifndef CONFIG_PARTITION
#define CONFIG_PARTITION            "config"
#endif
/** @} */

/**
 * @name Log Files & Prefixes
 * @brief Default paths per role (overridable).
 * @{ */
#ifndef LOG_DIR
#define LOG_DIR                     "/logs"
#endif
#ifndef LOGFILE_PATH_ICM
#define LOGFILE_PATH_ICM            LOG_DIR "/icm.json"
#endif
#ifndef LOG_FILE_PREFIX_ICM
#define LOG_FILE_PREFIX_ICM         LOG_DIR "/icm_"
#endif
#ifndef LOGFILE_PATH_PMS
#define LOGFILE_PATH_PMS            LOG_DIR "/pms.json"
#endif
#ifndef LOG_FILE_PREFIX_PMS
#define LOG_FILE_PREFIX_PMS         LOG_DIR "/pms_"
#endif
#ifndef LOGFILE_PATH_SENS
#define LOGFILE_PATH_SENS           LOG_DIR "/sensor.json"
#endif
#ifndef LOG_FILE_PREFIX_SENS
#define LOG_FILE_PREFIX_SENS        LOG_DIR "/sensor_"
#endif
#ifndef LOGFILE_PATH_REL
#define LOGFILE_PATH_REL            LOG_DIR "/relay.json"
#endif
#ifndef LOG_FILE_PREFIX_REL
#define LOG_FILE_PREFIX_REL         LOG_DIR "/relay_"
#endif
#ifndef LOGFILE_PATH_SEMU
#define LOGFILE_PATH_SEMU           LOG_DIR "/semu.json"
#endif
#ifndef LOG_FILE_PREFIX_SEMU
#define LOG_FILE_PREFIX_SEMU        LOG_DIR "/semu_"
#endif
#ifndef LOGFILE_PATH_REMU
#define LOGFILE_PATH_REMU           LOG_DIR "/remu.json"
#endif
#ifndef LOG_FILE_PREFIX_REMU
#define LOG_FILE_PREFIX_REMU        LOG_DIR "/remu_"
#endif
/** @} */

/**
 * @name Shared Helpers
 * @brief Utility macros used project-wide.
 * @{ */
#ifndef NVS_KEY_LEN
#define NVS_KEY_LEN                 6
#endif
#ifndef COUNT_OF
#define COUNT_OF(arr)               (sizeof(arr) / sizeof((arr)[0]))
#endif
#ifndef STR_HELPER
#define STR_HELPER(x)               #x
#endif
#ifndef STR
#define STR(x)                      STR_HELPER(x)
#endif
/** @} */

#endif // CONFIG_COMMON_H
