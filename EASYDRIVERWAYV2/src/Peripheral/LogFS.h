/**************************************************************
 *  Project     : EasyDriveway
 *  File        : LogFS.h
 *  Purpose     : SD-based Log Files Manager with UART command API.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef LOGFS_H
#define LOGFS_H


#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <HardwareSerial.h>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdarg>
#include "LogFS_Commands.h"
#include "RTCManager.h"

class RTCManager;

/**
 * @brief SD-based log manager with UART command API and RTC-aware timestamps.
 * @details
 *  - Structured per-domain events
 *  - Creates/rotates/purges logs under /logs (default)
 *  - Streams files over UART
 *  - Read-only FS browsing helpers
 *  - Role-aware default SD pins (compiled via hardware headers)
 */
class LogFS {
public:
  /**
   * @brief Event domains for classification.
   */
  enum Domain : uint8_t {
    DOM_BATTERY = 0, DOM_BLE, DOM_WIFI, DOM_USB, DOM_POWER, DOM_SYSTEM, DOM_SECURITY, DOM_STORAGE, DOM_RTC, DOM_OTA, DOM_FW, DOM_USER, DOM_REL, DOM_SEN, DOM_CFG, DOM__COUNT
  };

  /**
   * @brief Event severity levels.
   */
  enum Severity : uint8_t { EV_DEBUG = 0, EV_INFO = 1, EV_WARN = 2, EV_ERROR = 3, EV_CRITICAL = 4 };

public:
  /**
   * @brief Construct with a reference to a HardwareSerial for I/O.
   * @param uart Reference to the UART stream used for commands and outputs.
   */
  explicit LogFS(HardwareSerial& uart) : _uart(uart) {}

  /**
   * @brief Initialize SD with role default pins and SPI frequency.
   * @param hz SPI clock (Hz), default 40 MHz.
   * @return true on success.
   */
  bool begin();

  /**
   * @brief Initialize SD with explicit pins and SPI frequency.
   * @param cs   Chip Select pin.
   * @param sck  SPI SCK pin.
   * @param miso SPI MISO pin.
   * @param mosi SPI MOSI pin.
   * @param hz   SPI clock (Hz), default 40 MHz.
   * @return true on success.
   */
  bool begin(uint8_t cs, int sck, int miso, int mosi, uint32_t hz = 40000000UL);

  /**
   * @brief Print SD card information to UART (INFO lines).
   */
  void cardInfo();

  /**
   * @brief Attach RTC manager for timestamp generation.
   * @param rtc Pointer to RTCManager.
   */
  void attachRTC(RTCManager* rtc) { _rtc = rtc; }

  /**
   * @brief Set log directory (runtime). Ensures directory exists.
   * @param d Directory path (e.g., "/logs").
   */
  void setLogDir(const String& d) { _logDir = (d.length() ? d : "/logs"); mkdirs(_logDir.c_str()); }

  /**
   * @brief Set maximum log file size (bytes) before rotation.
   * @param b Bytes threshold (non-zero).
   */
  void setMaxLogBytes(size_t b) { if (b) _maxLogBytes = b; }

  /**
   * @brief Set maximum number of log files to retain (count-based purge).
   * @param n Count limit.
   */
  void setMaxLogFiles(uint16_t n) { _maxLogFiles = n; }

  /**
   * @brief Set retention in days for time-based purge (0 disables).
   * @param days Retention days.
   */
  void setRetentionDays(uint16_t days) { _retentionDays = days; }

  /**
   * @brief Enable/disable per-domain separate log files.
   * @param en True to enable per-domain logs.
   */
  void setPerDomainLogs(bool en) { _perDomainLogs = en; }

  /**
   * @brief Set default base filename prefix for new logs.
   * @param base Base name (e.g., "node"); falls back to role default when null/empty.
   */
  void setDefaultBase(const char* base) { _defaultBase = (base && *base) ? base : roleDefaultBase(); }

  /**
   * @brief Get current log directory.
   * @return Directory string.
   */
  const String& logDir() const { return _logDir; }

  /**
   * @brief Get maximum log file size in bytes.
   * @return Size threshold.
   */
  size_t maxLogBytes() const { return _maxLogBytes; }

  /**
   * @brief Get maximum retained log files.
   * @return Count limit.
   */
  uint16_t maxLogFiles() const { return _maxLogFiles; }

  /**
   * @brief Get retention days.
   * @return Days value (0 = disabled).
   */
  uint16_t retentionDays() const { return _retentionDays; }

  /**
   * @brief Check if per-domain logs are enabled.
   * @return True if enabled.
   */
  bool perDomainLogs() const { return _perDomainLogs; }

  /**
   * @brief Get runtime SD CS pin used.
   * @return Pin number.
   */
  int sdPinCS() const { return _sdCS; }

  /**
   * @brief Get runtime SD SCK pin used.
   * @return Pin number.
   */
  int sdPinSCK() const { return _sdSCK; }

  /**
   * @brief Get runtime SD MISO pin used.
   * @return Pin number.
   */
  int sdPinMISO() const { return _sdMISO; }

  /**
   * @brief Get runtime SD MOSI pin used.
   * @return Pin number.
   */
  int sdPinMOSI() const { return _sdMOSI; }

  /**
   * @brief Create a new log file in the log directory.
   * @param base Base filename prefix (uses default if null/empty).
   * @return Full path to created log or empty string on failure.
   */
  String newLog(const char* base);

  /**
   * @brief Append a line to a file, optionally prefixed by a human timestamp.
   * @param path File path.
   * @param line Content to write.
   * @param withTimestamp True to prefix with human timestamp.
   * @return true on success.
   */
  bool appendLine(const char* path, const String& line, bool withTimestamp = true);

  /**
   * @brief Rotate file if it exceeds the size threshold.
   * @param path File path.
   * @return true if rotated.
   */
  bool rotateIfNeeded(const char* path);

  /**
   * @brief Purge old logs using count/time policy.
   * @return Number of files removed.
   */
  uint16_t purgeOld();

  /**
   * @brief Get/create active log path for a domain.
   * @param dom Domain.
   * @param createIfMissing True to create a new log if missing.
   * @return Active log path or empty string.
   */
  String activeLogPath(Domain dom, bool createIfMissing = true);

  /**
   * @brief Write a structured event line (JSON).
   * @param dom Domain.
   * @param sev Severity.
   * @param code Numeric code.
   * @param message Message text.
   * @param source Optional source tag.
   * @return true on success.
   */
  bool event(Domain dom, Severity sev, int code, const String& message, const char* source = nullptr);

  /**
   * @brief printf-style event writer.
   * @param dom Domain.
   * @param sev Severity.
   * @param code Numeric code.
   * @param fmt Format string.
   * @param ... Variadic args.
   * @return true on success.
   */
  bool eventf(Domain dom, Severity sev, int code, const char* fmt, ...);

  /**
   * @brief Helper: log LED state change.
   * @param on On/Off.
   * @param who Label (default "LED").
   * @return true on success.
   */
  bool logLed(bool on, const char* who = "LED");

  /**
   * @brief Helper: log buzzer state.
   * @param on On/Off.
   * @param volume Optional volume (>=0).
   * @param who Label (default "BUZZER").
   * @return true on success.
   */
  bool logBuzzer(bool on, int volume = -1, const char* who = "BUZZER");

  /**
   * @brief Helper: log fan activity.
   * @param mode Fan mode string.
   * @param pwm PWM value or -1.
   * @param tempC Temperature or INT_MIN.
   * @param who Label (default "FAN").
   * @return true on success.
   */
  bool logFan(const char* mode, int pwm = -1, int tempC = INT_MIN, const char* who = "FAN");

  /**
   * @brief Helper: log pairing start.
   * @param targetMac Target MAC address string.
   * @return true on success.
   */
  bool logPairingStart(const char* targetMac);

  /**
   * @brief Helper: log pairing success.
   * @param targetMac Target MAC address string.
   * @return true on success.
   */
  bool logPairingSuccess(const char* targetMac);

  /**
   * @brief Helper: log pairing failure with reason.
   * @param targetMac Target MAC address string.
   * @param reason Reason string.
   * @return true on success.
   */
  bool logPairingFail(const char* targetMac, const char* reason);

  /**
   * @brief Helper: log configuration change.
   * @param key   Config key.
   * @param fromVal Previous value.
   * @param toVal   New value.
   * @return true on success.
   */
  bool logConfigChange(const char* key, const String& fromVal, const String& toVal);

  /**
   * @brief Helper: log system boot reason.
   * @param reason Reason string.
   * @return true on success.
   */
  bool logBoot(const char* reason);

  /**
   * @brief Helper: log system restart reason.
   * @param reason Reason string.
   * @return true on success.
   */
  bool logRestart(const char* reason);

  /**
   * @brief Helper: log generic error.
   * @param code Error code.
   * @param msg  Message.
   * @param src  Source label (default "SYSTEM").
   * @return true on success.
   */
  bool logError(int code, const String& msg, const char* src = "SYSTEM");

  /**
   * @brief List a directory (read-only).
   * @param path   Directory path.
   * @param levels Recursion depth.
   * @param human  Human-readable flag.
   */
  void listDir(const char* path = "/", uint8_t levels = 1, bool human = true);

  /**
   * @brief Print a directory tree (read-only).
   * @param path   Directory path.
   * @param levels Recursion depth.
   */
  void tree(const char* path = "/", uint8_t levels = 2);

  /**
   * @brief Check path existence.
   * @param path File/dir path.
   * @return true if exists.
   */
  bool exists(const char* path);

  /**
   * @brief Check if path is a file.
   * @param path File path.
   * @return true if file.
   */
  bool isFile(const char* path);

  /**
   * @brief Check if path is a directory.
   * @param path Directory path.
   * @return true if directory.
   */
  bool isDir(const char* path);

  /**
   * @brief Resolve relative path against CWD.
   * @param p Relative or absolute path.
   * @return Absolute path.
   */
  String resolvePath(const String& p);

  /**
   * @brief Change current working directory.
   * @param path Target path.
   * @return true on success.
   */
  bool chdir(const char* path);

  /**
   * @brief Get current working directory.
   * @return CWD string.
   */
  const String& cwd() const { return _cwd; }

  /**
   * @brief Read file and stream to UART.
   * @param path File path.
   * @return Bytes sent.
   */
  size_t readFileTo(const char* path, Stream& out);

  /**
   * @brief Convenience: stream file to the manager's UART.
   * @param path File path.
   * @return Bytes sent.
   */
  size_t sendFile(const char* path) { return readFileTo(path, _uart); }

  /**
   * @brief Set chunk size for streaming.
   * @param n Bytes per chunk (defaults to 512 if 0).
   */
  void setChunkSize(size_t n) { _chunk = n ? n : 512; }

  /**
   * @brief Get current chunk size.
   * @return Bytes per chunk.
   */
  size_t chunkSize() const { return _chunk; }

  /**
   * @brief Handle one UART command line (with timeout).
   * @param rxTimeoutMs Timeout in ms.
   */
  void serveOnce(uint32_t rxTimeoutMs = 5);

  /**
   * @brief Continuously serve UART commands (blocking).
   */
  void serveLoop();

  /**
   * @brief Convert domain enum to string.
   * @param d Domain value.
   * @return String name.
   */
  static const char* domainToStr(Domain d);

  /**
   * @brief Convert severity enum to string.
   * @param s Severity value.
   * @return String name.
   */
  static const char* sevToStr(Severity s);

  /**
   * @brief Parse domain from string.
   * @param s Input string.
   * @param out Parsed domain.
   * @return true on success.
   */
  static bool strToDomain(const String& s, Domain& out);

  /**
   * @brief Parse severity from string.
   * @param s Input string.
   * @param out Parsed severity.
   * @return true on success.
   */
  static bool strToSev(const String& s, Severity& out);

private:
  /**
   * @brief Role-aware default base prefix (compiled per role).
   * @return Base name string.
   */
  static const char* roleDefaultBase();

  /**
   * @brief Create directories recursively.
   * @param path Absolute path.
   * @return true on success.
   */
  bool mkdirs(const char* path);

  /**
   * @brief Open file for reading.
   * @param path Path.
   * @param f    File handle out.
   * @return true on success.
   */
  bool openForRead(const char* path, File& f);

  /**
   * @brief Stream a file to an output stream in chunks.
   * @param f   Open file.
   * @param out Output stream.
   * @return Bytes written.
   */
  size_t streamFile(File& f, Stream& out);

  /**
   * @brief Print a single directory entry line.
   * @param f      Entry file.
   * @param parent Parent path.
   * @param human  Human-readable flag.
   */
  void printEntryLine(File& f, const String& parent, bool human);

  /**
   * @brief Recursive directory tree printer.
   * @param dir    Open directory.
   * @param parent Parent path.
   * @param levels Recursion depth.
   * @param human  Human-readable flag.
   */
  void doTree(File dir, const String& parent, uint8_t levels, bool human);

  /**
   * @brief Read a line from a stream with timeout.
   * @param in         Input stream.
   * @param line       Output line.
   * @param timeoutMs  Timeout (ms).
   * @return true if a line was read.
   */
  bool readLine(Stream& in, String& line, uint32_t timeoutMs);

  /**
   * @brief Parse and handle a command line.
   * @param line Command line.
   * @return true if handled.
   */
  bool handleCommandLine(const String& line);

  /**
   * @brief Send OK response (optionally with message).
   * @param msg Message text.
   */
  void sendOK(const String& msg = "");

  /**
   * @brief Send error response with message.
   * @param msg Message text.
   */
  void sendERR(const String& msg);

  /**
   * @brief Send info response with message.
   * @param msg Message text.
   */
  void sendINFO(const String& msg);

  /**
   * @brief Timestamp compact "YYYYMMDD_HHMMSS" or fallback when unset.
   * @return Timestamp string.
   */
  String timestampNow();

  /**
   * @brief Human timestamp "YYYY-MM-DD HH:MM:SS " or "UNSET-TIME ".
   * @return Timestamp string with trailing space.
   */
  String timestampHuman();

  /**
   * @brief Build compact JSON event line.
   * @param dom     Domain.
   * @param sev     Severity.
   * @param code    Code.
   * @param message Message.
   * @param source  Source tag.
   * @return JSON line string.
   */
  String makeEventJson(Domain dom, Severity sev, int code, const String& message, const char* source);

  /**
   * @brief Compare two Arduino Strings for sort (lexicographic).
   * @param a Left.
   * @param b Right.
   * @return -1/0/1.
   */
  static int compareNames(const String& a, const String& b);

  /**
   * @brief Collect files from a directory and sort ascending by name.
   * @param out Output vector of full paths.
   * @return true on success.
   */
  bool collectFilesSorted(const String& dir, std::vector<String>& out);

private:
  HardwareSerial& _uart;        /**< UART reference for I/O. */
  RTCManager*     _rtc = nullptr;/**< Optional RTC manager.  */
  SPIClass        _spi;          /**< SPI instance.          */
  String          _cwd = "/";    /**< Current working dir.   */

  /* Streaming */
  size_t          _chunk = 512;  /**< Stream chunk size.     */

  /* Log policy */
  String          _logDir = "/logs";
  size_t          _maxLogBytes  = 256UL * 1024UL;
  uint16_t        _maxLogFiles  = 100;
  uint16_t        _retentionDays = 0;
  bool            _perDomainLogs = true;

  /* Active log paths per domain */
  String          _activePath[DOM__COUNT];
  const char*     _defaultBase = "node";

  /* SD pins actually used at runtime */
  int _sdCS = -1;
  int _sdSCK = -1;
  int _sdMISO = -1;
  int _sdMOSI = -1;
};

/* Convenience macros for callers */
/**
 * @brief Variadic printf-style event.
 */
#define LOGFS_EVT(fs, dom, sev, code, fmt, ...) (fs).eventf((dom), (sev), (code), (fmt), ##__VA_ARGS__)

/**
 * @brief Power info event macro.
 */
#define LOGFS_PWR_INFO(fs, code, fmt, ...)      LOGFS_EVT((fs), LogFS::DOM_POWER,   LogFS::EV_INFO, (code), fmt, ##__VA_ARGS__)

/**
 * @brief WiFi error event macro.
 */
#define LOGFS_WIFI_ERR(fs, code, fmt, ...)      LOGFS_EVT((fs), LogFS::DOM_WIFI,    LogFS::EV_ERROR,(code), fmt, ##__VA_ARGS__)

/**
 * @brief System critical event macro.
 */
#define LOGFS_SYS_CRIT(fs, code, fmt, ...)      LOGFS_EVT((fs), LogFS::DOM_SYSTEM,  LogFS::EV_CRITICAL,(code), fmt, ##__VA_ARGS__)

#endif /* LOGFS_H */
