/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : ICMLogFS.cpp
 *  Purpose     : SD-based Log Files Manager with UART command API.
 **************************************************************/

#include "ICMLogFS.h"
#include <vector>
#include <algorithm>
#include <cstdarg>

// -------------------------------
// Load SD pins/model from ConfigManager
// -------------------------------
void ICMLogFS::loadSDPinsFromConfig() {
    // Use persisted values if available; fall back to Config.h defaults
    if (_cfg) {
        _sdModel = _cfg->GetString(CFG_SD_MODEL_KEY, String(SD_CARD_MODEL_DEFAULT));
        _sdCS    = _cfg->GetInt   (CFG_SD_CS_PIN_KEY,   SD_CS_PIN_DEFAULT);
        _sdSCK   = _cfg->GetInt   (CFG_SD_SCK_PIN_KEY,  SD_SCK_PIN_DEFAULT);
        _sdMISO  = _cfg->GetInt   (CFG_SD_MISO_PIN_KEY, SD_MISO_PIN_DEFAULT);
        _sdMOSI  = _cfg->GetInt   (CFG_SD_MOSI_PIN_KEY, SD_MOSI_PIN_DEFAULT);
    } else {
        // No ConfigManager attached—defaults already set in members
    }
}

// -------------------------------
// Init SD / SPI
// -------------------------------
bool ICMLogFS::beginFromConfig(uint32_t hz) {
    loadSDPinsFromConfig();
    return begin((uint8_t)_sdCS, _sdSCK, _sdMISO, _sdMOSI, hz);
}

bool ICMLogFS::begin(uint8_t cs, int sck, int miso, int mosi, uint32_t hz) {
    _spi.begin(sck, miso, mosi, cs);
    if (!SD.begin(cs, _spi, hz)) { sendERR("SD init failed"); return false; }
    mkdirs(_logDir.c_str());

    // Optional: print which pins/model were used
    _uart.print(MKSD_RESP_INFO); _uart.print(" SDModel="); _uart.println(_sdModel);
    _uart.print(MKSD_RESP_INFO); _uart.print(" SD Pins CS/SCK/MISO/MOSI=");
    _uart.print(cs); _uart.print("/"); _uart.print(sck); _uart.print("/");
    _uart.print(miso); _uart.print("/"); _uart.println(mosi);

    sendOK("SD initialized");
    return true;
}

void ICMLogFS::cardInfo() {
    uint8_t ct = SD.cardType();
    _uart.print(MKSD_RESP_INFO); _uart.print(" CardType=");
    if (ct == CARD_NONE)      _uart.println("None");
    else if (ct == CARD_MMC)  _uart.println("MMC");
    else if (ct == CARD_SD)   _uart.println("SDSC");
    else if (ct == CARD_SDHC) _uart.println("SDHC/SDXC");
    else                      _uart.println("Unknown");

    _uart.print(MKSD_RESP_INFO); _uart.print(" CardSizeMB=");
    _uart.println((uint64_t)SD.cardSize() / (1024ULL * 1024ULL));
    _uart.print(MKSD_RESP_INFO); _uart.print(" TotalMB=");
    _uart.println((uint64_t)SD.totalBytes()/ (1024ULL * 1024ULL));
    _uart.print(MKSD_RESP_INFO); _uart.print(" UsedMB=");
    _uart.println((uint64_t)SD.usedBytes() / (1024ULL * 1024ULL));
}

// -------------------------------
// String converters (Domain / Severity)
// -------------------------------
const char* ICMLogFS::domainToStr(Domain d) {
    switch (d) {
        case DOM_BATTERY:  return "BATTERY";
        case DOM_BLE:      return "BLE";
        case DOM_WIFI:     return "WIFI";
        case DOM_ESPNOW:   return "ESPNOW";
        case DOM_USB:      return "USB";
        case DOM_POWER:    return "POWER";
        case DOM_SYSTEM:   return "SYSTEM";
        case DOM_SECURITY: return "SECURITY";
        case DOM_STORAGE:  return "STORAGE";
        case DOM_RTC:      return "RTC";
        case DOM_OTA:      return "OTA";
        case DOM_FW:       return "FW";
        case DOM_USER:     return "USER";
        default:           return "SYSTEM";
    }
}
const char* ICMLogFS::sevToStr(Severity s) {
    switch (s) {
        case EV_DEBUG:    return "DEBUG";
        case EV_INFO:     return "INFO";
        case EV_WARN:     return "WARN";
        case EV_ERROR:    return "ERROR";
        case EV_CRITICAL: return "CRITICAL";
        default:          return "INFO";
    }
}
bool ICMLogFS::strToDomain(const String& s, Domain& out) {
    String u = s; u.toUpperCase();
    for (uint8_t i=0;i<DOM__COUNT;i++) {
        if (u == domainToStr((Domain)i)) { out=(Domain)i; return true; }
    }
    return false;
}
bool ICMLogFS::strToSev(const String& s, Severity& out) {
    String u = s; u.toUpperCase();
    if (u=="DEBUG"){out=EV_DEBUG;return true;}
    if (u=="INFO"){out=EV_INFO;return true;}
    if (u=="WARN"){out=EV_WARN;return true;}
    if (u=="ERROR"){out=EV_ERROR;return true;}
    if (u=="CRITICAL"){out=EV_CRITICAL;return true;}
    return false;
}

// -------------------------------
// FS helpers
// -------------------------------
bool ICMLogFS::mkdirs(const char* path) {
    if (!path || !*path) return false;
    if (SD.exists(path)) return true;
    String p = path;
    if (!p.startsWith("/")) p = "/" + p;
    String cur = "";
    for (int i = 1; i < p.length(); ++i) {
        char c = p[i];
        if (c == '/') {
            if (cur.length() && !SD.exists(cur.c_str())) SD.mkdir(cur.c_str());
        }
        cur += c;
    }
    if (!SD.exists(p.c_str())) SD.mkdir(p.c_str());
    return SD.exists(p.c_str());
}

bool ICMLogFS::exists(const char* path) {
    return SD.exists(path);
}
bool ICMLogFS::isFile(const char* path) {
    File f = SD.open(path, FILE_READ);
    bool ok = f && !f.isDirectory();
    if (f) f.close();
    return ok;
}
bool ICMLogFS::isDir(const char* path) {
    File f = SD.open(path, FILE_READ);
    bool ok = f && f.isDirectory();
    if (f) f.close();
    return ok;
}

String ICMLogFS::resolvePath(const String& p) {
    if (p.length() == 0) return _cwd;
    if (p[0] == '/') return p;
    if (_cwd == "/") return "/" + p;
    return _cwd + "/" + p;
}

bool ICMLogFS::chdir(const char* path) {
    String tgt = resolvePath(path ? String(path) : String("/"));
    if (isDir(tgt.c_str())) { _cwd = tgt; return true; }
    return false;
}

// -------------------------------
// Timestamps (RTC-backed)
// -------------------------------
String ICMLogFS::timestampNow() {
    time_t t = 0;
    if (_rtc) {
        unsigned long ts = _rtc->getUnixTime();
        if (ts) t = (time_t)ts;
    } else {
    #if defined(ESP32)
        time(&t);
    #endif
    }

    struct tm tmv{};
    if (t <= 100000) { // fallback when time not set
        uint32_t ms = millis();
        char buf[32];
        snprintf(buf, sizeof(buf), "U%010lu_%06lu",
                 (unsigned long)(ms / 1000),
                 (unsigned long)((ms % 1000) * 1000));
        return String(buf);
    }

    localtime_r(&t, &tmv);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return String(buf);
}

String ICMLogFS::timestampHuman() {
    time_t t = 0;
    if (_rtc) {
        unsigned long ts = _rtc->getUnixTime();
        if (ts) t = (time_t)ts;
    } else {
    #if defined(ESP32)
        time(&t);
    #endif
    }

    if (t <= 100000) return String("UNSET-TIME ");

    struct tm tmv{};
    localtime_r(&t, &tmv);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d ",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return String(buf);
}

// -------------------------------
// Log helpers
// -------------------------------
String ICMLogFS::newLog(const char* base) {
    mkdirs(_logDir.c_str());
    String b = (base && *base) ? base : _defaultBase;
    String fname = b + "_" + timestampNow() + ".log";

    String full = _logDir;
    if (!full.endsWith("/")) full += "/";
    full += fname;

    File f = SD.open(full.c_str(), FILE_WRITE);
    if (!f) { sendERR("Open fail"); return ""; }
    String header = "# ICM log created " + timestampHuman() + "\n";
    f.print(header);
    f.close();

    sendOK(String("NEW ") + full);
    return full;
}

bool ICMLogFS::appendLine(const char* path, const String& line, bool withTimestamp) {
    if (!path || !*path) return false;
    File f = SD.open(path, FILE_APPEND);
    if (!f) { sendERR("Open fail"); return false; }
    if (withTimestamp) f.print(timestampHuman());
    f.println(line);
    f.close();
    rotateIfNeeded(path);
    return true;
}

bool ICMLogFS::rotateIfNeeded(const char* path) {
    if (!path || !*path) return false;
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    size_t sz = f.size();
    f.close();
    if (sz <= _maxLogBytes) return false;

    String p = path;
    int dot = p.lastIndexOf('.');
    String stem = (dot > 0) ? p.substring(0, dot) : p;
    uint16_t idx = 1;
    String candidate;
    do { candidate = stem + "." + String(idx++); } while (SD.exists(candidate.c_str()));

    if (SD.rename(path, candidate.c_str())) { sendOK(String("ROTATE ") + candidate); purgeOld(); return true; }
    sendERR("Rotate failed");
    return false;
}

static bool endsWithLog(const String& n) { return n.endsWith(".log"); }

int ICMLogFS::compareNames(const String& a, const String& b) {
    int c = a.compareTo(b);
    return (c < 0) ? -1 : (c > 0 ? 1 : 0);
}

bool ICMLogFS::collectFilesSorted(const String& dir, std::vector<String>& out) {
    File d = SD.open(dir.c_str(), FILE_READ);
    if (!d || !d.isDirectory()) return false;

    File e = d.openNextFile();
    while (e) {
        String nm = e.name();
        String full = dir;
        if (!full.endsWith("/")) full += "/";
        full += nm;
        if (!e.isDirectory()) out.push_back(full);
        e = d.openNextFile();
    }
    d.close();
    std::sort(out.begin(), out.end(), [](const String& A, const String& B){
        return A.compareTo(B) < 0;
    });
    return true;
}

uint16_t ICMLogFS::purgeOld() {
    std::vector<String> files;
    collectFilesSorted(_logDir, files);
    if (files.empty()) return 0;

    // Only log-like files
    std::vector<String> logs;
    logs.reserve(files.size());
    for (auto& f : files) {
        if (f.endsWith(".log") || f.lastIndexOf(".log.") > 0) logs.push_back(f);
    }
    if (logs.empty()) return 0;

    uint16_t removed = 0;

    // Count-based purge
    if (_maxLogFiles && logs.size() > _maxLogFiles) {
        size_t excess = logs.size() - _maxLogFiles;
        for (size_t i = 0; i < excess; ++i) {
            if (SD.remove(logs[i].c_str())) { ++removed; sendINFO("PURGE " + logs[i]); }
        }
        logs.erase(logs.begin(), logs.begin() + excess);
    }

    // Time-based purge
    if (_retentionDays > 0) {
        time_t nowT = 0;
    #if defined(ESP32)
        time(&nowT);
    #endif
        if (nowT > 100000) {
            const time_t cutoff = nowT - (time_t)_retentionDays * 24 * 3600;
            for (auto& f : logs) {
                // ".../name_YYYYMMDD_HHMMSS.log"
                int us = f.lastIndexOf('_');
                int dot = f.lastIndexOf('.');
                if (us > 7 && dot > us + 6) {
                    String date = f.substring(us + 1, us + 9); // YYYYMMDD
                    struct tm tmv{}; memset(&tmv, 0, sizeof(tmv));
                    tmv.tm_year = date.substring(0,4).toInt() - 1900;
                    tmv.tm_mon  = date.substring(4,6).toInt() - 1;
                    tmv.tm_mday = date.substring(6,8).toInt();
                    time_t ft = mktime(&tmv);
                    if (ft && ft < cutoff) {
                        if (SD.remove(f.c_str())) { ++removed; sendINFO("PURGE " + f); }
                    }
                }
            }
        }
    }
    if (removed) sendOK(String("PURGED ") + String(removed));
    return removed;
}

// -------------------------------
// Listing / tree printing
// -------------------------------
void ICMLogFS::printEntryLine(File& f, const String& parent, bool human) {
    String nm = f.name();
    String full = parent;
    if (!full.endsWith("/")) full += "/";
    full += nm;

    if (f.isDirectory()) {
        _uart.print(MKSD_RESP_INFO); _uart.print(" DIR  ");
        _uart.println(full);
    } else {
        _uart.print(MKSD_RESP_INFO); _uart.print(" FILE ");
        _uart.print(full);
        _uart.print(" ");
        _uart.println((uint32_t)f.size());
    }
}

void ICMLogFS::listDir(const char* path, uint8_t levels, bool human) {
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) { sendERR("Not a directory"); return; }

    _uart.print(MKSD_RESP_INFO); _uart.print(" Listing "); _uart.println(path);
    File entry = dir.openNextFile();
    while (entry) {
        printEntryLine(entry, String(path), human);
        if (levels && entry.isDirectory()) {
            String sub = String(path);
            if (!sub.endsWith("/")) sub += "/";
            sub += entry.name();
            listDir(sub.c_str(), levels - 1, human);
        }
        entry = dir.openNextFile();
    }
    dir.close();
    sendOK();
}

void ICMLogFS::doTree(File dir, const String& parent, uint8_t levels, bool human) {
    _uart.print(MKSD_RESP_INFO); _uart.print(" TREE "); _uart.println(parent);
    File entry = dir.openNextFile();
    while (entry) {
        printEntryLine(entry, parent, human);
        if (levels && entry.isDirectory()) {
            String sub = parent;
            if (!sub.endsWith("/")) sub += "/";
            sub += entry.name();
            File subdir = SD.open(sub);
            if (subdir && subdir.isDirectory()) {
                doTree(subdir, sub, levels - 1, human);
                subdir.close();
            }
        }
        entry = dir.openNextFile();
    }
}

void ICMLogFS::tree(const char* path, uint8_t levels) {
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) { sendERR("Not a directory"); return; }
    doTree(dir, String(path), levels, true);
    dir.close();
    sendOK();
}

// -------------------------------
// Streaming
// -------------------------------
bool ICMLogFS::openForRead(const char* path, File& f) {
    f = SD.open(path, FILE_READ);
    return (bool)f;
}

size_t ICMLogFS::streamFile(File& f, Stream& out) {
    size_t total = 0;
    static uint8_t buf[1024];
    while (true) {
        size_t toRead = _chunk;
        if (toRead > sizeof(buf)) toRead = sizeof(buf);
        size_t n = f.read(buf, toRead);
        if (!n) break;
        total += out.write(buf, n);
    }
    return total;
}

size_t ICMLogFS::readFileTo(const char* path, Stream& out) {
    File f;
    if (!openForRead(path, f)) { sendERR(String("Open fail: ") + path); return 0; }

    _uart.print(MKSD_RESP_DATA); _uart.print(" ");
    _uart.println((uint32_t)f.size());

    size_t sent = streamFile(f, out);
    f.close();
    sendOK(String("Bytes=") + String((uint32_t)sent));
    return sent;
}

// -------------------------------
// Event helpers
// -------------------------------
String ICMLogFS::activeLogPath(Domain dom, bool createIfMissing) {
    if (_activePath[dom].length() && SD.exists(_activePath[dom].c_str())) return _activePath[dom];
    if (!createIfMissing) return String("");

    const char* base = _perDomainLogs ? domainToStr(dom) : _defaultBase;
    String path = newLog(base);
    _activePath[dom] = path;
    return _activePath[dom];
}

static String jsonEscape(const String& in) {
    String out; out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); ++i) {
        char c = in[i];
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\r': out += "\\r";  break;
            case '\n': out += "\\n";  break;
            case '\t': out += "\\t";  break;
            default: out += c; break;
        }
    }
    return out;
}

String ICMLogFS::makeEventJson(Domain dom, Severity sev, int code, const String& message, const char* source) {
    String th = timestampHuman(); th.trim();
    String src = source ? source : domainToStr(dom);
    String msg = jsonEscape(message);

    // Compact JSON line
    String j; j.reserve(96 + src.length() + msg.length());
    j += "{\"ts\":\"";   j += th;               j += "\",";        // includes space
    j += "\"dom\":\"";   j += domainToStr(dom); j += "\",";        // domain
    j += "\"sev\":\"";   j += sevToStr(sev);    j += "\",";        // severity
    j += "\"src\":\"";   j += jsonEscape(src);  j += "\",";        // source
    j += "\"code\":";    j += String(code);     j += ",";          // numeric code
    j += "\"msg\":\"";   j += msg;              j += "\"}";        // message
    return j;
}

bool ICMLogFS::event(Domain dom, Severity sev, int code, const String& message, const char* source) {
    String path = activeLogPath(dom, true);
    if (!path.length()) { sendERR("no-active-log"); return false; }
    String json = makeEventJson(dom, sev, code, message, source);
    // Line already contains ts -> no extra prefix
    return appendLine(path.c_str(), json, /*withTimestamp=*/false);
}

bool ICMLogFS::eventf(Domain dom, Severity sev, int code, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return event(dom, sev, code, String(buf), nullptr);
}

// -------------------------------
// UART command server
// -------------------------------
void ICMLogFS::serveOnce(uint32_t rxTimeoutMs) {
    String line;
    if (readLine(_uart, line, rxTimeoutMs)) {
        handleCommandLine(line);
    }
}
void ICMLogFS::serveLoop() { while (true) { serveOnce(10); delay(1); } }

bool ICMLogFS::readLine(Stream& in, String& line, uint32_t timeoutMs) {
    line = "";
    uint32_t start = millis();
    while ((millis() - start) < timeoutMs) {
        while (in.available()) {
            char c = (char)in.read();
            if (c == '\r') continue;
            if (c == '\n') { line.trim(); return line.length() > 0; }
            line += c;
        }
    }
    return false;
}

bool ICMLogFS::handleCommandLine(const String& ln) {
    String cmd = ln; cmd.trim();
    if (cmd.length() == 0) return false;

    // split into tokens: op a1 a2 a3 rest
    String op, a1, a2, a3, rest;
    int s1 = cmd.indexOf(' ');
    if (s1 < 0) op = cmd;
    else {
        op = cmd.substring(0, s1);
        int s2 = cmd.indexOf(' ', s1 + 1);
        if (s2 < 0) a1 = cmd.substring(s1 + 1);
        else {
            a1 = cmd.substring(s1 + 1, s2);
            int s3 = cmd.indexOf(' ', s2 + 1);
            if (s3 < 0) a2 = cmd.substring(s2 + 1);
            else {
                a2 = cmd.substring(s2 + 1, s3);
                int s4 = cmd.indexOf(' ', s3 + 1);
                if (s4 < 0) a3 = cmd.substring(s3 + 1);
                else {
                    a3 = cmd.substring(s3 + 1, s4);
                    rest = cmd.substring(s4 + 1);
                }
            }
        }
    }
    String opU = op; opU.toUpperCase();

    // ---- FS.* ----
    if (opU == "FS.INFO") { cardInfo(); sendOK(); return true; }
    if (opU == "FS.LS")   { String p=(a1.length()?resolvePath(a1):_cwd); uint8_t lv=(a2.length()?a2.toInt():1); listDir(p.c_str(), lv, true); return true; }
    if (opU == "FS.TREE") { String p=(a1.length()?resolvePath(a1):_cwd); uint8_t lv=(a2.length()?a2.toInt():2); tree(p.c_str(), lv); return true; }
    if (opU == "FS.STAT") {
        String p=(a1.length()?resolvePath(a1):_cwd);
        if (!exists(p.c_str())) { sendERR("Not found"); return true; }
        if (isDir(p.c_str())) {
            _uart.print(MKSD_RESP_INFO); _uart.print(" DIR ");  _uart.println(p);
        } else {
            File f=SD.open(p, FILE_READ);
            _uart.print(MKSD_RESP_INFO); _uart.print(" FILE "); _uart.print(p); _uart.print(" ");
            _uart.println(f ? (uint32_t)f.size() : 0);
            if (f) f.close();
        }
        sendOK(); return true;
    }
    if (opU == "FS.CWD")  {
        if (!a1.length()) { _uart.print(MKSD_RESP_INFO); _uart.print(" CWD "); _uart.println(_cwd); sendOK(); }
        else { if (chdir(a1.c_str())) { _uart.print(MKSD_RESP_INFO); _uart.print(" CWD "); _uart.println(_cwd); sendOK(); } else sendERR("chdir"); }
        return true;
    }
    if (opU == "FS.PWD")  { _uart.print(MKSD_RESP_INFO); _uart.print(" CWD "); _uart.println(_cwd); sendOK(); return true; }

    // ---- LOG.* ----
    if (opU == "LOG.NEW") {
        String base = a1.length()? a1 : _defaultBase;
        String full = newLog(base.c_str());
        if (full.length()==0) { sendERR("create"); } else { _uart.print(MKSD_RESP_INFO); _uart.print(" NEW "); _uart.println(full); sendOK(); }
        return true;
    }
    if (opU == "LOG.APPENDLN") {
        String path = resolvePath(a1);
        String text = (a2.length() || a3.length() || rest.length()) ? (a2 + (a3.length()?" "+a3:"") + (rest.length()?" "+rest:"")) : "";
        if (!text.length()) { sendERR("no text"); return true; }
        if (!exists(path.c_str())) { sendERR("nf"); return true; }
        if (appendLine(path.c_str(), text, true)) sendOK("APPENDED"); else sendERR("append");
        return true;
    }
    if (opU == "LOG.GET") {
        String p=(a1.length()?resolvePath(a1):"");
        if (!p.length() || !exists(p.c_str()) || isDir(p.c_str())) { sendERR("nf"); return true; }
        readFileTo(p.c_str(), _uart); return true;
    }
    if (opU == "LOG.LS") { listDir(_logDir.c_str(), a1.length()? a1.toInt():1, true); return true; }
    if (opU == "LOG.PURGE") {
        if (a1.equalsIgnoreCase("MAXCNT")) {
            long v = a2.toInt(); if (v < 0) v = 0; _maxLogFiles = (uint16_t)v;
        } else if (a1.equalsIgnoreCase("MAXDAYS")) {
            long v = a2.toInt(); if (v < 0) v = 0; _retentionDays = (uint16_t)v;
        }
        uint16_t n = purgeOld();
        sendOK(String("REMOVED=") + String(n));
        return true;
    }

    // ---- LOG.EVENT <DOMAIN> <SEV> <CODE> <TEXT...> ----
    if (opU == "LOG.EVENT") {
        Domain d; Severity s;
        if (!strToDomain(a1, d)) { sendERR("domain"); return true; }
        if (!strToSev(a2, s))    { sendERR("sev");    return true; }
        int code = a3.toInt();
        String msg = rest.length()? rest : String("");
        if (event(d, s, code, msg, nullptr)) sendOK(); else sendERR("event");
        return true;
    }

    // ---- CFG.* ----
    if (opU == "CFG.SHOW") {
        _uart.print(MKSD_RESP_INFO); _uart.print(" LOGDIR ");   _uart.println(_logDir);
        _uart.print(MKSD_RESP_INFO); _uart.print(" MAXSZ ");    _uart.println((uint32_t)_maxLogBytes);
        _uart.print(MKSD_RESP_INFO); _uart.print(" MAXCNT ");   _uart.println(_maxLogFiles);
        _uart.print(MKSD_RESP_INFO); _uart.print(" MAXDAYS ");  _uart.println(_retentionDays);
        _uart.print(MKSD_RESP_INFO); _uart.print(" CHUNK ");    _uart.println((uint32_t)_chunk);
        _uart.print(MKSD_RESP_INFO); _uart.print(" PERDOMAIN ");_uart.println((int)_perDomainLogs);
        _uart.print(MKSD_RESP_INFO); _uart.print(" SDMODEL ");  _uart.println(_sdModel);
        _uart.print(MKSD_RESP_INFO); _uart.print(" SD PINS CS="); _uart.print(_sdCS);
        _uart.print(" SCK="); _uart.print(_sdSCK);
        _uart.print(" MISO="); _uart.print(_sdMISO);
        _uart.print(" MOSI="); _uart.println(_sdMOSI);
        sendOK(); return true;
    }
    if (opU == "CFG.SET") {
        if (a1.equalsIgnoreCase("LOGDIR"))        setLogDir(resolvePath(a2));
        else if (a1.equalsIgnoreCase("MAXSZ"))    setMaxLogBytes((size_t)a2.toInt());
        else if (a1.equalsIgnoreCase("MAXCNT"))   setMaxLogFiles((uint16_t)a2.toInt());
        else if (a1.equalsIgnoreCase("MAXDAYS"))  setRetentionDays((uint16_t)a2.toInt());
        else if (a1.equalsIgnoreCase("PERDOMAIN"))setPerDomainLogs(a2.toInt()!=0);
        else { sendERR("arg"); return true; }
        sendOK(); return true;
    }

    // ---- Misc ----
    if (opU == "CHUNK") {
        if (a1.length()) setChunkSize((size_t)a1.toInt());
        _uart.print(MKSD_RESP_INFO); _uart.print(" CHUNK "); _uart.println((uint32_t)_chunk);
        sendOK(); return true;
    }

    sendERR("Unknown cmd");
    return false;
}

// -------------------------------
// Response helpers
// -------------------------------
void ICMLogFS::sendOK(const String& msg) {
    _uart.print(MKSD_RESP_OK);
    if (msg.length()) { _uart.print(" "); _uart.print(msg); }
    _uart.print("\n");
}
void ICMLogFS::sendERR(const String& msg) {
    _uart.print(MKSD_RESP_ERR);
    if (msg.length()) { _uart.print(" "); _uart.print(msg); }
    _uart.print("\n");
}
void ICMLogFS::sendINFO(const String& msg) {
    _uart.print(MKSD_RESP_INFO);
    if (msg.length()) { _uart.print(" "); _uart.print(msg); }
    _uart.print("\n");
}
