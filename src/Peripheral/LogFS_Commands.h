/**************************************************************
 *  Project     : EasyDriveway
 *  File        : LogFS_Commands.h
 *  Purpose     : UART command names and quick reference for users.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef LOG_FS_COMMANDS_H
#define LOG_FS_COMMANDS_H

// INCLUDES
// (none)

/**
 * @section protocol_responses Response prefixes
 * @brief String prefixes used by the serial protocol for structured replies.
 *
 * - `OK [msg]\n`
 * - `ERR [msg]\n`
 * - `INFO [k=v or notes]\n`
 * - `DATA <len>\n<raw-bytes...>`
 */
#define MKSD_RESP_OK    "OK"
#define MKSD_RESP_ERR   "ERR"
#define MKSD_RESP_INFO  "INFO"
#define MKSD_RESP_DATA  "DATA"

/**
 * @section fs_browsing Filesystem browsing commands
 * @brief High-level commands to inspect and navigate storage.
 *
 * @verbatim
 * FS.INFO                  -> card type/size/usage (INFO lines)
 * FS.LS [path] [levels]    -> list directory (default: cwd, levels=1)
 * FS.TREE [path] [levels]  -> recursive tree (levels default=2)
 * FS.STAT [path]           -> print file/dir + size
 * FS.CWD [path]            -> change cwd; no arg prints current
 * FS.PWD                   -> print cwd
 * @endverbatim
 */

/**
 * @section log_mgmt Log management commands
 * @brief Create, append, fetch, and purge logs.
 *
 * @verbatim
 * LOG.NEW [base]                -> create /logs/<base>_YYYYMMDD_HHMMSS.log (returns path)
 * LOG.APPENDLN <path> <text...> -> append one line (with timestamp prefix)
 * LOG.GET <path>                -> send file as: DATA <len>\n<bytes>
 * LOG.LS [levels]               -> list /logs
 * LOG.PURGE [MAXCNT n] | [MAXDAYS d] -> set limits (optional) and purge now
 * @endverbatim
 */

/**
 * @section event_ingest Event ingestion
 * @brief Structured event entry format for all modules.
 *
 * @verbatim
 * LOG.EVENT <DOMAIN> <SEV> <CODE> <TEXT...>
 *   DOMAIN: BATTERY|BLE|WIFI|USB|POWER|SYSTEM|SECURITY|STORAGE|RTC|OTA|FW|USER|REL|SEN|CFG
 *   SEV   : DEBUG|INFO|WARN|ERROR|CRITICAL
 *   CODE  : integer (module-specific)
 *   TEXT  : free text
 * @endverbatim
 */

/**
 * @section cfg_misc Configuration & misc
 * @brief Runtime configuration of LogFS behavior and stream chunk size.
 *
 * @verbatim
 * CFG.SHOW                 -> show LOGDIR, MAXSZ, MAXCNT, MAXDAYS, CHUNK, PERDOMAIN
 * CFG.SET LOGDIR <path>    -> set log directory (mkdir as needed)
 * CFG.SET MAXSZ <bytes>    -> per-file max (rotation threshold)
 * CFG.SET MAXCNT <n>       -> keep newest N logs (after rotation/purge)
 * CFG.SET MAXDAYS <n>      -> delete logs older than N days (0=off)
 * CFG.SET PERDOMAIN <0|1>  -> single log file (0) or per-domain logs (1)
 * CHUNK <n>                -> set UART stream chunk size (bytes)
 * @endverbatim
 */

#endif // LOG_FS_COMMANDS_H
