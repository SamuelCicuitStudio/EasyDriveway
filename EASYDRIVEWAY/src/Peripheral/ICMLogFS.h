/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : ICMLogFS.h
 *  Purpose     : SD-based Log Files Manager with UART command API.
 *                - Structured events (per-domain)
 *                - Creates/rotates/purges logs under /logs (default)
 *                - Streams files over UART
 *                - Read-only FS browsing helpers
 *                - Pulls SD pin assignment from ConfigManager
 **************************************************************/

#ifndef ICM_LOG_FS_H
#define ICM_LOG_FS_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <HardwareSerial.h>

#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdarg>

#include "RGBLed.h"
#include "ICMLogFS_Commands.h"
#include "RTCManager.h"
#include "ConfigManager.h"

// Forward-declare to avoid circular include
class RGBLed;

class ICMLogFS {
public:
    // ---------- Event domains ----------
    enum Domain : uint8_t {
        DOM_BATTERY = 0, DOM_BLE, DOM_WIFI, DOM_ESPNOW, DOM_USB,
        DOM_POWER, DOM_SYSTEM, DOM_SECURITY, DOM_STORAGE, DOM_RTC,
        DOM_OTA, DOM_FW, DOM_USER, DOM__COUNT,DOM_REL,DOM_SEN,DOM_CFG
    };

    // ---------- Event severity ----------
    enum Severity : uint8_t { EV_DEBUG=0, EV_INFO=1, EV_WARN=2, EV_ERROR=3, EV_CRITICAL=4 };

public:
    // New: constructor that takes ConfigManager (recommended)
    explicit ICMLogFS(HardwareSerial& uart, ConfigManager* cfg, RGBLed* led = nullptr)
        : _uart(uart), _cfg(cfg), _led(led) {}

    // Back-compat constructor (you can later call attachConfig())
    explicit ICMLogFS(HardwareSerial& uart, RGBLed* led = nullptr)
        : _uart(uart), _led(led) {}

    // Provide/replace ConfigManager at runtime
    void attachConfig(ConfigManager* cfg) { _cfg = cfg; }

    // ---- Init SD card ----
    // 1) Preferred: use pins from ConfigManager (with Config.h defaults)
    bool beginFromConfig(uint32_t hz = 20000000UL);
    // 2) Manual pins (kept for flexibility)
    bool begin(uint8_t cs,
               int sck,
               int miso,
               int mosi,
               uint32_t hz = 20000000UL);

    // ---- Card info (prints INFO lines) ----
    void cardInfo();

    // ---- RTC integration ----
    void attachRTC(RTCManager* rtc) { _rtc = rtc; }

    // ---- Config setters/getters (runtime; no reboot) ----
    void setLogDir(const String& d)        { _logDir = (d.length() ? d : "/logs"); mkdirs(_logDir.c_str()); }
    void setMaxLogBytes(size_t b)          { if (b) _maxLogBytes = b; }
    void setMaxLogFiles(uint16_t n)        { _maxLogFiles = n; }
    void setRetentionDays(uint16_t days)   { _retentionDays = days; }
    void setPerDomainLogs(bool en)         { _perDomainLogs = en; }
    void setDefaultBase(const char* base)  { _defaultBase = (base && *base) ? base : "icm"; }

    const String& logDir()           const { return _logDir; }
    size_t        maxLogBytes()      const { return _maxLogBytes; }
    uint16_t      maxLogFiles()      const { return _maxLogFiles; }
    uint16_t      retentionDays()    const { return _retentionDays; }
    bool          perDomainLogs()    const { return _perDomainLogs; }

    // Expose loaded SD config (for diagnostics/UI)
    const String& sdModel() const { return _sdModel; }
    int  sdPinCS()   const { return _sdCS; }
    int  sdPinSCK()  const { return _sdSCK; }
    int  sdPinMISO() const { return _sdMISO; }
    int  sdPinMOSI() const { return _sdMOSI; }

    // ---- Log file helpers ----
    String newLog(const char* base);                       // /logs/<base>_YYYYMMDD_HHMMSS.log
    bool   appendLine(const char* path, const String& line, bool withTimestamp = true);
    bool   rotateIfNeeded(const char* path);
    uint16_t purgeOld();                                   // purge policy across _logDir

    // ---- Event helpers (public API for other modules) ----
    String activeLogPath(Domain dom, bool createIfMissing = true);
    bool   event(Domain dom, Severity sev, int code, const String& message, const char* source = nullptr);
    bool   eventf(Domain dom, Severity sev, int code, const char* fmt, ...);

    // ---- File system browsing (read-only) ----
    void   listDir(const char* path = "/", uint8_t levels = 1, bool human = true);
    void   tree(const char* path = "/", uint8_t levels = 2);
    bool   exists(const char* path);
    bool   isFile(const char* path);
    bool   isDir (const char* path);
    String resolvePath(const String& p);
    bool   chdir(const char* path);
    const String& cwd() const { return _cwd; }

    // ---- Streaming ----
    size_t readFileTo(const char* path, Stream& out);
    size_t sendFile(const char* path) { return readFileTo(path, _uart); }
    void   setChunkSize(size_t n) { _chunk = n ? n : 512; }
    size_t chunkSize() const { return _chunk; }

    // ---- UART command server ----
    void serveOnce(uint32_t rxTimeoutMs = 5);
    void serveLoop();

    // ---- Convenience: string converters ----
    static const char* domainToStr(Domain d);
    static const char* sevToStr(Severity s);
    static bool        strToDomain(const String& s, Domain& out);
    static bool        strToSev(const String& s, Severity& out);

private:
    // Load SD pins/model from ConfigManager (with Config.h defaults)
    void  loadSDPinsFromConfig();

    // Low-level helpers
    bool   mkdirs(const char* path);
    bool   openForRead(const char* path, File& f);
    size_t streamFile(File& f, Stream& out);
    void   printEntryLine(File& f, const String& parent, bool human);
    void   doTree(File dir, const String& parent, uint8_t levels, bool human);

    // Command parsing
    bool   readLine(Stream& in, String& line, uint32_t timeoutMs);
    bool   handleCommandLine(const String& line);

    // Protocol responses
    void   sendOK(const String& msg = "");
    void   sendERR(const String& msg);
    void   sendINFO(const String& msg);

    // Utils
    String timestampNow();                 // "YYYYMMDD_HHMMSS"
    String timestampHuman();               // "YYYY-MM-DD HH:MM:SS "
    String makeEventJson(Domain dom, Severity sev, int code, const String& message, const char* source);
    static int  compareNames(const String& a, const String& b);
    bool   collectFilesSorted(const String& dir, std::vector<String>& out);

private:
    HardwareSerial& _uart;
    ConfigManager*  _cfg = nullptr;
    RGBLed*         _led = nullptr;
    RTCManager*     _rtc = nullptr;

    SPIClass        _spi;
    String          _cwd = "/";

    // Streaming
    size_t          _chunk = 512;

    // Log policy
    String          _logDir = "/logs";
    size_t          _maxLogBytes  = 256UL * 1024UL;  // 256 KB/file
    uint16_t        _maxLogFiles  = 100;             // keep newest 100 by name
    uint16_t        _retentionDays = 0;              // 0 = disabled
    bool            _perDomainLogs = true;           // per-domain log files

    // Active log paths per domain
    String          _activePath[DOM__COUNT];
    const char*     _defaultBase = "icm";

    // Loaded SD configuration
    String _sdModel = SD_CARD_MODEL_DEFAULT;
    int    _sdCS   = PIN_SD_CS_DEFAULT;
    int    _sdSCK  = PIN_SD_SCK_DEFAULT;
    int    _sdMISO = PIN_SD_MISO_DEFAULT;
    int    _sdMOSI = PIN_SD_MOSI_DEFAULT;
};

// ---------- Convenience macros for callers ----------
#define ICM_EVT(fs, dom, sev, code, fmt, ...) (fs).eventf((dom), (sev), (code), (fmt), ##__VA_ARGS__)
#define ICM_BAT_INFO(fs, code, fmt, ...)      ICM_EVT((fs), ICMLogFS::DOM_BATTERY, ICMLogFS::EV_INFO, (code), fmt, ##__VA_ARGS__)
#define ICM_BLE_WARN(fs, code, fmt, ...)      ICM_EVT((fs), ICMLogFS::DOM_BLE,     ICMLogFS::EV_WARN, (code), fmt, ##__VA_ARGS__)
#define ICM_WIFI_ERR(fs, code, fmt, ...)      ICM_EVT((fs), ICMLogFS::DOM_WIFI,    ICMLogFS::EV_ERROR,(code), fmt, ##__VA_ARGS__)
#define ICM_NOW_DBG(fs, code, fmt, ...)       ICM_EVT((fs), ICMLogFS::DOM_ESPNOW,  ICMLogFS::EV_DEBUG,(code), fmt, ##__VA_ARGS__)
#define ICM_USB_INFO(fs, code, fmt, ...)      ICM_EVT((fs), ICMLogFS::DOM_USB,     ICMLogFS::EV_INFO, (code), fmt, ##__VA_ARGS__)
#define ICM_PWR_INFO(fs, code, fmt, ...)      ICM_EVT((fs), ICMLogFS::DOM_POWER,   ICMLogFS::EV_INFO, (code), fmt, ##__VA_ARGS__)
#define ICM_SYS_CRIT(fs, code, fmt, ...)      ICM_EVT((fs), ICMLogFS::DOM_SYSTEM,  ICMLogFS::EV_CRITICAL,(code), fmt, ##__VA_ARGS__)

#endif // ICM_LOG_FS_H
