/**************************************************************
 *  File   : ICMLogFS_Commands.h
 *  Purpose: UART command names + quick reference for users.
 *  Format : ASCII lines. Responses use prefixes:
 *           - "OK [msg]\n"
 *           - "ERR [msg]\n"
 *           - "INFO [k=v or notes]\n"
 *           - "DATA <len>\n<raw-bytes...>"
 **************************************************************/
#ifndef ICM_LOG_FS_COMMANDS_H
#define ICM_LOG_FS_COMMANDS_H

// ---- Response prefixes ----
#define MKSD_RESP_OK    "OK"
#define MKSD_RESP_ERR   "ERR"
#define MKSD_RESP_INFO  "INFO"
#define MKSD_RESP_DATA  "DATA"

// ---------- Config keys (persisted in Preferences) ----------
#ifndef CFG_SD_MODEL_KEY
  #define CFG_SD_MODEL_KEY   "SD_MODEL"
#endif
#ifndef CFG_SD_CS_PIN_KEY
  #define CFG_SD_CS_PIN_KEY  "SD_CS"
#endif
#ifndef CFG_SD_SCK_PIN_KEY
  #define CFG_SD_SCK_PIN_KEY "SD_SCK"
#endif
#ifndef CFG_SD_MISO_PIN_KEY
  #define CFG_SD_MISO_PIN_KEY "SD_MISO"
#endif
#ifndef CFG_SD_MOSI_PIN_KEY
  #define CFG_SD_MOSI_PIN_KEY "SD_MOSI"
#endif
// ---- FS browsing ----
// FS.INFO                  -> card type/size/usage (INFO lines)
// FS.LS [path] [levels]    -> list directory (default: cwd, levels=1)
// FS.TREE [path] [levels]  -> recursive tree (levels default=2)
// FS.STAT [path]           -> print file/dir + size
// FS.CWD [path]            -> change cwd; no arg prints current
// FS.PWD                   -> print cwd

// ---- Log management ----
// LOG.NEW [base]           -> create /logs/<base>_YYYYMMDD_HHMMSS.log (returns path)
// LOG.APPENDLN <path> <text...> -> append one line (with timestamp prefix)
// LOG.GET <path>           -> send file as: DATA <len>\n<bytes>
// LOG.LS [levels]          -> list /logs
// LOG.PURGE [MAXCNT n] | [MAXDAYS d] -> set limits (optional) and purge now

// ---- Event ingestion ----
// LOG.EVENT <DOMAIN> <SEV> <CODE> <TEXT...>
//   DOMAIN: BATTERY|BLE|WIFI|ESPNOW|USB|POWER|SYSTEM|SECURITY|STORAGE|RTC|OTA|FW|USER
//   SEV   : DEBUG|INFO|WARN|ERROR|CRITICAL
//   CODE  : integer (module-specific)
//   TEXT  : free text
//
// Example:
//   LOG.EVENT WIFI INFO 200 Connected to AP "HomeNet"

// ---- Config / misc ----
// CFG.SHOW                 -> show LOGDIR, MAXSZ, MAXCNT, MAXDAYS, CHUNK, PERDOMAIN
// CFG.SET LOGDIR <path>    -> set log directory (mkdir as needed)
// CFG.SET MAXSZ <bytes>    -> per-file max (rotation threshold)
// CFG.SET MAXCNT <n>       -> keep newest N logs (after rotation/purge)
// CFG.SET MAXDAYS <n>      -> delete logs older than N days (0=off)
// CFG.SET PERDOMAIN <0|1>  -> single log file (0) or per-domain logs (1)
// CHUNK <n>                -> set UART stream chunk size (bytes)

#endif // ICM_LOG_FS_COMMANDS_H
