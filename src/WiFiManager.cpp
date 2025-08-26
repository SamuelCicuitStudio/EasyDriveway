/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : WiFiManager.cpp
 **************************************************************/
#include "WiFiManager.h"
#include "WiFiAPI.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

WiFiManager* WiFiManager::instance = nullptr;
static const char* COOKIE_NAME = "ICMSESS";

static String isoFromRTC(RTCManager* rtc) {
  if (!rtc) return String();
  return rtc->iso8601String();  // "YYYY-MM-DDTHH:MM:SS" (UTC by design) 
}


static String isoFromSystemClock() {
  time_t now = time(nullptr);
  if (now <= 0) return String("unsynced");
  struct tm tmv; gmtime_r(&now, &tmv);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
  return String(buf);
}
/* ==================== bring-up ==================== */
void WiFiManager::begin() {
    instance = this;

    if (!SPIFFS.begin(true)) {
        if (_log) _log->event(ICMLogFS::DOM_STORAGE, ICMLogFS::EV_ERROR, 6000, "SPIFFS mount failed", "WiFiMgr");
    }

    addCORS();
    registerRoutes();

    const bool gotoCfg = _cfg ? _cfg->GetBool(GOTO_CONFIG_KEY, false) : false;
    bool staOk = false;

    if (!gotoCfg) staOk = tryConnectSTAFromNVS();
    if (!staOk)   startAccessPoint();

    if (_log) {
        _log->event(
            ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 6001,
            _apOn ? "WiFiMgr: AP mode ready" : "WiFiMgr: STA connected",
            _apOn ? "AP" : "STA"
        );
    }
}

void WiFiManager::disableWiFiAP() {
    if (_apOn) {
        _wifi->softAPdisconnect(true);
        _wifi->mode(WIFI_OFF);
        _apOn = false;
    }
}

void WiFiManager::setAPCredentials(const char* ssid, const char* password) {
    if (_cfg) {
        _cfg->PutString(DEVICE_WIFI_HOTSPOT_NAME_KEY, ssid);
        _cfg->PutString(DEVICE_AP_AUTH_PASS_KEY,      password);
    }
}

/* = SPIFFS ⇄ config import (legacy JSON) = */
void WiFiManager::syncSpifToPrefs() {
    if (!SPIFFS.exists(SLAVE_CONFIG_PATH)) return;
    File f = SPIFFS.open(SLAVE_CONFIG_PATH, FILE_READ);
    if (!f) return;

    DynamicJsonDocument d(32768);
    if (deserializeJson(d, f)) { f.close(); return; }
    f.close();

    JsonVariantConst root = d.as<JsonVariantConst>();
    if (!root.isNull() && root.containsKey(J_CONFIG)) applyExportedConfig(root[J_CONFIG]);
    if (!root.isNull() && root.containsKey(J_LINKS))  if (_esn) _esn->configureTopology(root[J_LINKS]);
}

/* ==================== Wi-Fi bring-up ==================== */
bool WiFiManager::tryConnectSTAFromNVS(uint32_t timeoutMs) {
    // Prefer STA creds from ConfigManager
    const String ssid = _cfg ? _cfg->GetString(WIFI_STA_SSID_KEY, "") : "";
    const String psk  = _cfg ? _cfg->GetString(WIFI_STA_PASS_KEY,  "") : "";

    _wifi->mode(WIFI_STA);
    _wifi->setAutoConnect(true);
    _wifi->setAutoReconnect(true);
    _wifi->disconnect(true);
    delay(50);

    if (ssid.length()) {
        _wifi->persistent(false);
        if (psk.length()) _wifi->begin(ssid.c_str(), psk.c_str());
        else              _wifi->begin(ssid.c_str());
    } else {
        _wifi->persistent(true);
        _wifi->begin(); // use WiFi NVS
    }

    const uint32_t start = millis();
    while (_wifi->status() != WL_CONNECTED && (millis() - start) < timeoutMs) delay(100);

    if (_wifi->status() != WL_CONNECTED) {
        if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_WARN, 6010, "STA connect failed; will use AP", "WiFiMgr");
        return false;
    }

    alignESPNOWToCurrentChannel();
    return true;
}

void WiFiManager::startAccessPoint() {
    String apSsid = _cfg ? _cfg->GetString(DEVICE_WIFI_HOTSPOT_NAME_KEY, DEVICE_WIFI_HOTSPOT_NAME_DEFAULT)
                         : String(DEVICE_WIFI_HOTSPOT_NAME_DEFAULT);
    String apPass = _cfg ? _cfg->GetString(DEVICE_AP_AUTH_PASS_KEY,      DEVICE_AP_AUTH_PASS_DEFAULT)
                         : String(DEVICE_AP_AUTH_PASS_DEFAULT);
    int ch = _cfg ? _cfg->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT) : ESPNOW_CH_DEFAULT;
    if (ch < 1 || ch > 13) ch = ESPNOW_CH_DEFAULT;

    _wifi->mode(WIFI_AP);
    if (!_wifi->softAPConfig(LOCAL_IP, GATEWAY, SUBNET)) return;
    if (!_wifi->softAP(apSsid.c_str(), apPass.c_str(), ch)) return;
    _apOn = true;

    if (_esn) _esn->setChannel((uint8_t)ch);
    if (_cfg) _cfg->PutInt(ESPNOW_CH_KEY, ch);
}

void WiFiManager::alignESPNOWToCurrentChannel() {
    uint8_t ch = _wifi->channel();
    if (ch < 1 || ch > 13) ch = ESPNOW_CH_DEFAULT;
    if (_esn) _esn->setChannel(ch);
    if (_cfg) _cfg->PutInt(ESPNOW_CH_KEY, (int)ch);
}

/* ==================== routing ==================== */
void WiFiManager::addCORS() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin",  "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
}

void WiFiManager::registerRoutes() {
    // ---------- HTML pages ----------
    _server.on(API_ROOT,      HTTP_GET, [this](AsyncWebServerRequest* r){ handleRoot(r); });
    _server.on(PAGE_HOME,     HTTP_GET, [this](AsyncWebServerRequest* r){ handleHome(r); });
    _server.on(PAGE_SETTINGS, HTTP_GET, [this](AsyncWebServerRequest* r){ handleSettings(r); });
    _server.on(PAGE_WIFI,     HTTP_GET, [this](AsyncWebServerRequest* r){ handleWiFiPage(r); });
    _server.on(PAGE_TOPO,     HTTP_GET, [this](AsyncWebServerRequest* r){ handleTopology(r); });
    _server.on(PAGE_LIVE,     HTTP_GET, [this](AsyncWebServerRequest* r){ handleLive(r); });
    _server.on(PAGE_THANKYOU, HTTP_GET, [this](AsyncWebServerRequest* r){ handleThanks(r); });
    // Home/status + controls
    _server.on(API_SYS_STATUS,  HTTP_GET,  [this](AsyncWebServerRequest* r){ hSysStatus(r); });
    _server.on(API_BUZZ_SET,    HTTP_POST, nullptr, nullptr,
    [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hBuzzerSet(r,d,l,i,t); });
    _server.on(API_SYS_RESET,   HTTP_POST, nullptr, nullptr,
    [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSystemReset(r,d,l,i,t); });
    _server.on(API_SYS_RESTART, HTTP_POST, nullptr, nullptr,
    [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSystemRestart(r,d,l,i,t); });

    // Cooling
    _server.on(API_COOL_STATUS, HTTP_GET, [this](AsyncWebServerRequest* r){ hCoolStatus(r); });
    _server.on(API_COOL_MODE,   HTTP_POST, nullptr, nullptr,
    [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCoolMode(r,d,l,i,t); });
    _server.on(API_COOL_SPEED,  HTTP_POST, nullptr, nullptr,
    [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCoolSpeed(r,d,l,i,t); });

    // Sleep
    _server.on(API_SLEEP_STATUS,  HTTP_GET,  [this](AsyncWebServerRequest* r){ hSleepStatus(r); });
    _server.on(API_SLEEP_TIMEOUT, HTTP_POST, nullptr, nullptr,
    [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSleepTimeout(r,d,l,i,t); });
    _server.on(API_SLEEP_RESET,   HTTP_POST, [this](AsyncWebServerRequest* r){ hSleepReset(r); });
    _server.on(API_SLEEP_SCHED,   HTTP_POST, nullptr, nullptr,
    [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSleepSchedule(r,d,l,i,t); });

    // ---------- Auth ----------
    _server.on(API_LOGIN_ENDPOINT,  HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hLogin(r,d,l,i,t); });
    _server.on(API_LOGOUT_ENDPOINT, HTTP_POST, [this](AsyncWebServerRequest* r){ hLogout(r); });
    _server.on(API_AUTH_STATUS,     HTTP_GET,  [this](AsyncWebServerRequest* r){ hLiveStatus(r); });

    // System mode (AUTO/MANUAL)
    _server.on(API_SYS_MODE,    HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSystemMode(r,d,l,i,t); });
    // Optional alias kept for front-end attempts
    _server.on(API_ESPNOW_MODE, HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSystemMode(r,d,l,i,t); });

    // ---------- Wi-Fi control ----------
    _server.on(API_WIFI_MODE,           HTTP_GET,  [this](AsyncWebServerRequest* r){ hWiFiMode(r); });
    _server.on(API_WIFI_STA_CONNECT,    HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hWiFiStaConnect(r,d,l,i,t); });
    _server.on(API_WIFI_STA_DISCONNECT, HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hWiFiStaDisconnect(r,d,l,i,t); });
    _server.on(API_WIFI_AP_START,       HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hWiFiApStart(r,d,l,i,t); });
    _server.on(API_WIFI_SCAN,           HTTP_GET,  [this](AsyncWebServerRequest* r){ hWiFiScan(r); });

    // ---------- Config ----------
    _server.on(API_CFG_LOAD,           HTTP_GET,  [this](AsyncWebServerRequest* r){ hCfgLoad(r); });
    _server.on(API_CFG_SAVE,           HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgSave(r,d,l,i,t); });
    _server.on(API_CFG_EXPORT,         HTTP_GET,  [this](AsyncWebServerRequest* r){ hCfgExport(r); });
    _server.on(API_CFG_IMPORT,         HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgImport(r,d,l,i,t); });
    _server.on(API_CFG_FACTORY_RESET,  HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgFactoryReset(r,d,l,i,t); });

    // ---------- Peers / Topology ----------
    _server.on(API_SENSOR_DAYNIGHT,    HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSensorDayNight(r,d,l,i,t); });
    _server.on(API_PEERS_LIST,         HTTP_GET,  [this](AsyncWebServerRequest* r){ hPeersList(r); });
    _server.on(API_PEER_PAIR,          HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPeerPair(r,d,l,i,t); });
    _server.on(API_PEER_REMOVE,        HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPeerRemove(r,d,l,i,t); });
    _server.on(API_TOPOLOGY_SET,       HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hTopoSet(r,d,l,i,t); });
    _server.on(API_TOPOLOGY_GET,       HTTP_GET,  [this](AsyncWebServerRequest* r){ hTopoGet(r); });

    // ---------- Sequences ----------
    _server.on(API_SEQUENCE_START,     HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSeqStart(r,d,l,i,t); });
    _server.on(API_SEQUENCE_STOP,      HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSeqStop(r,d,l,i,t); });

    // ---------- Power ----------
    _server.on(API_POWER_INFO,         HTTP_GET,  [this](AsyncWebServerRequest* r){ hPowerInfo(r); });
    _server.on(API_POWER_CMD,          HTTP_POST, nullptr, nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPowerCmd(r,d,l,i,t); });

    // ---------- Uploads & assets ----------
    _server.on(API_FILE_UPLOAD, HTTP_POST,
        [this](AsyncWebServerRequest* req){ req->send(200, "text/plain", "File received"); }, handleFileUpload);
    _server.on(API_FAVICON, HTTP_GET, [this](AsyncWebServerRequest* req){ req->send(204); });

    // Static passthrough (images/fonts/css/js)
    _server.serveStatic(API_STATIC_ICONS, SPIFFS, "/icons/").setCacheControl("max-age=86400");
    _server.serveStatic(API_STATIC_FONTS, SPIFFS, "/fonts/").setCacheControl("max-age=86400");
    _server.serveStatic("/",              SPIFFS, "/");

    _server.begin();
}
/* ==================== HTML handlers ==================== */
void WiFiManager::handleRoot(AsyncWebServerRequest* request) {
    if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
    request->redirect(PAGE_HOME);
}
void WiFiManager::handleHome(AsyncWebServerRequest* request) {
    if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
    if (SPIFFS.exists(PAGE_HOME)) request->send(SPIFFS, PAGE_HOME, String());
    else request->send(200, "text/plain", "ICM — Home");
}
void WiFiManager::handleSettings(AsyncWebServerRequest* request) {
    if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
    if (SPIFFS.exists(PAGE_SETTINGS)) request->send(SPIFFS, PAGE_SETTINGS, String());
    else request->send(200, "text/plain", "Settings");
}
void WiFiManager::handleWiFiPage(AsyncWebServerRequest* request) {
    if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
    if (SPIFFS.exists(PAGE_WIFI)) request->send(SPIFFS, PAGE_WIFI, String());
    else request->send(200, "text/plain", "Wi-Fi Credentials");
}
void WiFiManager::handleTopology(AsyncWebServerRequest* request) {
    if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
    if (SPIFFS.exists(PAGE_TOPO)) request->send(SPIFFS, PAGE_TOPO, String());
    else request->send(200, "text/plain", "Topology");
}
void WiFiManager::handleLive(AsyncWebServerRequest* request) {
    if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
    if (SPIFFS.exists(PAGE_LIVE)) request->send(SPIFFS, PAGE_LIVE, String());
    else request->send(200, "text/plain", "Live Session");
}
void WiFiManager::handleThanks(AsyncWebServerRequest* request) {
    if (SPIFFS.exists(PAGE_THANKYOU)) request->send(SPIFFS, PAGE_THANKYOU, String());
    else request->send(200, "text/plain", "Thanks");
}

/* ==================== Config ==================== */
void WiFiManager::hCfgLoad(AsyncWebServerRequest* req) {
  DynamicJsonDocument d(1024);

  // ---- STA (wifi.html form expects these) ----
  d[J_WIFI_SSID] = _cfg ? _cfg->GetString(WIFI_STA_SSID_KEY, WIFI_STA_SSID_DEFAULT)
                        : String(WIFI_STA_SSID_DEFAULT);
  d[J_WIFI_PSK]  = _cfg ? _cfg->GetString(WIFI_STA_PASS_KEY, WIFI_STA_PASS_DEFAULT)
                        : String(WIFI_STA_PASS_DEFAULT);

  // ---- AP (existing) ----
  d[J_AP_SSID] = _cfg ? _cfg->GetString(DEVICE_WIFI_HOTSPOT_NAME_KEY, DEVICE_WIFI_HOTSPOT_NAME_DEFAULT)
                      : String(DEVICE_WIFI_HOTSPOT_NAME_DEFAULT);
  d[J_AP_PSK]  = _cfg ? _cfg->GetString(DEVICE_AP_AUTH_PASS_KEY,      DEVICE_AP_AUTH_PASS_DEFAULT)
                      : String(DEVICE_AP_AUTH_PASS_DEFAULT);

  // ---- ESP-NOW channel (existing) ----
  d[J_ESN_CH]  = _cfg ? _cfg->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT) : ESPNOW_CH_DEFAULT;

  // ---- BLE (wifi.html form expects these) ----
  d[J_BLE_NAME] = _cfg ? _cfg->GetString(DEVICE_BLE_NAME_KEY, DEVICE_BLE_NAME_DEFAULT)
                       : String(DEVICE_BLE_NAME_DEFAULT);
  d[J_BLE_PASS] = _cfg ? _cfg->GetInt(DEVICE_BLE_AUTH_PASS_KEY, DEVICE_BLE_AUTH_PASS_DEFAULT)
                       : DEVICE_BLE_AUTH_PASS_DEFAULT;

  // ---- Read-only network status card (existing shape) ----
  JsonObject net = d.createNestedObject("net");
  if (_apOn) {
    net[J_MODE] = "AP";
    net["ip"]   = _wifi ? _wifi->softAPIP().toString() : String();
    net["ch"]   = _cfg ? _cfg->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT) : ESPNOW_CH_DEFAULT;
  } else if (_wifi && _wifi->status() == WL_CONNECTED) {
    net[J_MODE] = "STA";
    net["ip"]   = _wifi->localIP().toString();
    net["ch"]   = _wifi->channel();
    net["rssi"] = _wifi->RSSI();
  } else {
    net[J_MODE] = "OFF";
  }

  sendJSON(req, d);
}

void WiFiManager::hCfgSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(2048);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  JsonVariantConst root = d.as<JsonVariantConst>();
  if (root.isNull()) { sendError(req, "Empty JSON"); return; }

  // ---- AP ----
  if (root.containsKey(J_AP_SSID)) _cfg->PutString(DEVICE_WIFI_HOTSPOT_NAME_KEY, root[J_AP_SSID].as<const char*>());
  if (root.containsKey(J_AP_PSK))  _cfg->PutString(DEVICE_AP_AUTH_PASS_KEY,      root[J_AP_PSK].as<const char*>());

  // ---- BLE ----
  if (root.containsKey(J_BLE_NAME)) _cfg->PutString(DEVICE_BLE_NAME_KEY,      root[J_BLE_NAME].as<const char*>());
  if (root.containsKey(J_BLE_PASS)) _cfg->PutInt   (DEVICE_BLE_AUTH_PASS_KEY, root[J_BLE_PASS].as<int>());

  // ---- ESP-NOW channel (validate 1..13) ----
  if (root.containsKey(J_ESN_CH)) {
    int ch = root[J_ESN_CH].as<int>();
    if (ch >= 1 && ch <= 13) {
      _cfg->PutInt(ESPNOW_CH_KEY, ch);
      // If currently in AP, keep ESP-NOW aligned immediately (optional, keeps live preview right)
      if (_apOn && _esn) _esn->setChannel((uint8_t)ch);
    }
  }

  // ---- STA (save only; DO NOT connect here) ----
  if (root.containsKey(J_WIFI_SSID)) _cfg->PutString(WIFI_STA_SSID_KEY, root[J_WIFI_SSID].as<const char*>());
  if (root.containsKey(J_WIFI_PSK))  _cfg->PutString(WIFI_STA_PASS_KEY, root[J_WIFI_PSK].as<const char*>());

  // No immediate STA connect: wifi.html is a setup page.
  sendOK(req);
}

void WiFiManager::hCfgExport(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(65536);
    String blob = _esn ? _esn->exportConfiguration() : "{}";
    d[J_EXPORT] = blob;
    sendJSON(req, d);
}

void WiFiManager::hCfgImport(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(65536);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    JsonVariantConst root = d.as<JsonVariantConst>();

    if (!root.isNull() && root.containsKey(J_CONFIG)) {
        if (applyExportedConfig(root[J_CONFIG])) { sendOK(req); return; }
        sendError(req, "Config import failed", 500);
        return;
    }
    sendError(req, "Missing 'config' field");
}

void WiFiManager::hCfgFactoryReset(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)data; (void)len; (void)index; (void)total;
    if (_esn) { _esn->removeAllPeers(); _esn->clearAll(); }
    if (_cfg) _cfg->PutBool(RESET_FLAG_KEY, true);
    sendOK(req);
}

/* ==================== Wi-Fi control ==================== */
void WiFiManager::hWiFiMode(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(256);
    if (_apOn) {
        d[J_MODE] = "AP";
        d["ip"]   = _wifi->softAPIP().toString();
        d["ch"]   = _cfg ? _cfg->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT) : ESPNOW_CH_DEFAULT;
    } else if (_wifi->status() == WL_CONNECTED) {
        d[J_MODE] = "STA";
        d["ip"]   = _wifi->localIP().toString();
        d["ch"]   = _wifi->channel();
        d["rssi"] = _wifi->RSSI();
    } else {
        d[J_MODE] = "OFF";
    }
    sendJSON(req, d);
}

void WiFiManager::hWiFiStaConnect(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(512);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    JsonVariantConst root = d.as<JsonVariantConst>();
    if (root.isNull() || !root.containsKey(J_WIFI_SSID) || !root.containsKey(J_WIFI_PSK)) { sendError(req, "Missing ssid/password"); return; }

    String ssid = root[J_WIFI_SSID].as<String>();
    String psk  = root[J_WIFI_PSK].as<String>();

    _wifi->mode(WIFI_STA);
    _wifi->persistent(true);
    _wifi->disconnect(true);
    delay(50);
    _wifi->begin(ssid.c_str(), psk.c_str());

    uint32_t start = millis();
    while (_wifi->status() != WL_CONNECTED && millis() - start < 15000) delay(100);

    DynamicJsonDocument res(256);
    if (_wifi->status() == WL_CONNECTED) {
        _apOn = false;
        alignESPNOWToCurrentChannel();
        res[J_OK] = true;
        res["ip"] = _wifi->localIP().toString();
    } else {
        res[J_OK] = false;
    }
    sendJSON(req, res);
}

void WiFiManager::hWiFiStaDisconnect(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)data; (void)len; (void)index; (void)total;
    _wifi->disconnect(true);
    sendOK(req);
}

void WiFiManager::hWiFiApStart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(512);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    JsonVariantConst root = d.as<JsonVariantConst>();

    String apSsid = root.containsKey(J_AP_SSID) ? root[J_AP_SSID].as<String>()
                                                : _cfg->GetString(DEVICE_WIFI_HOTSPOT_NAME_KEY, DEVICE_WIFI_HOTSPOT_NAME_DEFAULT);
    String apPass = root.containsKey(J_AP_PSK)  ? root[J_AP_PSK].as<String>()
                                                : _cfg->GetString(DEVICE_AP_AUTH_PASS_KEY,      DEVICE_AP_AUTH_PASS_DEFAULT);

    if (root.containsKey(J_AP_SSID)) _cfg->PutString(DEVICE_WIFI_HOTSPOT_NAME_KEY, apSsid.c_str());
    if (root.containsKey(J_AP_PSK))  _cfg->PutString(DEVICE_AP_AUTH_PASS_KEY,      apPass.c_str());

    if (root.containsKey(J_ESN_CH)) {
        int ch = root[J_ESN_CH].as<int>();
        if (ch >= 1 && ch <= 13) _cfg->PutInt(ESPNOW_CH_KEY, ch);
    }

    disableWiFiAP();
    startAccessPoint();
    sendOK(req);
}

void WiFiManager::hWiFiScan(AsyncWebServerRequest* req) {
    int n = _wifi->scanNetworks();
    DynamicJsonDocument d(4096);
    JsonArray arr = d.createNestedArray("aps");
    for (int i=0; i<n; i++) {
        JsonObject o = arr.createNestedObject();
        o["ssid"] = _wifi->SSID(i);
        o["rssi"] = _wifi->RSSI(i);
        o["ch"]   = _wifi->channel(i);
        o["enc"]  = _wifi->encryptionType(i);
    }
    _wifi->scanDelete();
    sendJSON(req, d);
}


void WiFiManager::hSensorDayNight(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(512);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    JsonVariantConst root = d.as<JsonVariantConst>();
    const String mac = root.containsKey(J_MAC) ? String(root[J_MAC].as<const char*>()) : String();
    if (!mac.length()) { sendError(req, "Missing mac"); return; }

    bool requested = false;
    if (_esn) requested = _esn->presenceGetDayNightByMac(mac);

    uint32_t when = 0;
    int8_t flag = _esn ? _esn->lastDayFlagByMac(mac, &when) : -1;

    DynamicJsonDocument res(256);
    res[J_OK] = true;
    if (flag >= 0) res["is_day"] = (int)flag;
    else res["is_day"] = -1;
    res["updated_ms"] = when;
    res["requested"]  = requested;
    sendJSON(req, res);
}

/* ==================== Peers / Topology ==================== */
void WiFiManager::hPeersList(AsyncWebServerRequest* req) {
  DynamicJsonDocument in(8192);
  String src = _esn ? _esn->serializePeers() : "{}";
  if (deserializeJson(in, src)) { sendError(req, "Peers serialization error", 500); return; }

  DynamicJsonDocument out(8192);
  JsonVariantConst root = in.as<JsonVariantConst>();
  if (root.is<JsonArrayConst>()) {
    // If serializePeers() returned a bare array, wrap it
    out["peers"] = root;
  } else if (!root.isNull() && root.containsKey("peers")) {
    out["peers"] = root["peers"];
  } else {
    out.createNestedArray("peers"); // empty
  }
  sendJSON(req, out);
}

void WiFiManager::hPeerPair(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(512);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    JsonVariantConst root = d.as<JsonVariantConst>();

    String mac  = root.containsKey(J_MAC)  ? root[J_MAC].as<String>()  : String();
    String type = root.containsKey(J_TYPE) ? root[J_TYPE].as<String>() : String();
    bool ok = _esn ? _esn->pair(mac, type) : false;
    if (ok) sendOK(req); else sendError(req, "Pair failed", 500);
}

void WiFiManager::hPeerRemove(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(256);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    JsonVariantConst root = d.as<JsonVariantConst>();

    String mac = root.containsKey(J_MAC) ? root[J_MAC].as<String>() : String();
    bool ok = _esn ? _esn->removePeer(mac) : false;
    if (ok) sendOK(req); else sendError(req, "Remove failed", 500);
}

void WiFiManager::hTopoSet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(65536);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    JsonVariantConst root = d.as<JsonVariantConst>();

    if (!root.isNull() && root.containsKey(J_LINKS)) {
        bool ok = _esn ? _esn->configureTopology(root[J_LINKS]) : false;
        if (ok) sendOK(req); else sendError(req, "Topology set failed", 500);
        return;
    }
    sendError(req, "Missing links[]");
}

void WiFiManager::hTopoGet(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(65536);
    String topo = _esn ? _esn->serializeTopology() : "{\"links\":[]}";
    if (!deserializeJson(d, topo)) sendJSON(req, d);
    else sendError(req, "Topology serialization error", 500);
}

/* ==================== Sequences (stubs) ==================== */
void WiFiManager::hSeqStart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)data;(void)len;(void)index;(void)total;
    sendOK(req);
}
void WiFiManager::hSeqStop(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)data;(void)len;(void)index;(void)total;
    sendOK(req);
}

/* ==================== Power (stubs) ==================== */
void WiFiManager::hPowerInfo(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(256); d["status"] = "ok"; sendJSON(req, d);
}
void WiFiManager::hPowerCmd(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)data;(void)len;(void)index;(void)total; sendOK(req);
}

/* ==================== Live/status ==================== */
void WiFiManager::hLiveStatus(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(64);
    d[J_OK] = isAuthed(req);
    sendJSON(req, d);
}

/* ==================== Auth ==================== */
// ---- Login: JSON or FORM; server-side redirects for normal forms
// FORM or JSON → server decides redirect targets (no client redirects)
void WiFiManager::hLogin(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((const char*)data).substring(0, len);
    if (index + len < total) return;

    // Parse JSON
    String username, password; bool parsed = false;
    DynamicJsonDocument d(1024);
    if (!deserializeJson(d, body)) {
        JsonVariantConst root = d.as<JsonVariantConst>();
        auto pick = [&](const char* a, const char* b, const char* c)->String{
            if (!root.isNull() && root.containsKey(a)) return root[a].as<String>();
            if (!root.isNull() && b && root.containsKey(b)) return root[b].as<String>();
            if (!root.isNull() && c && root.containsKey(c)) return root[c].as<String>();
            return String();
        };
        username = pick("username","user","login");
        password = pick("password","pass",nullptr);
        parsed   = username.length() || password.length();
    }

    // Fallback: x-www-form-urlencoded
    if (!parsed) {
        auto getParam = [&](const String& key)->String{
            String needle = key + "=";
            int s = body.indexOf(needle); if (s < 0) return String();
            s += needle.length();
            int e = body.indexOf('&', s);
            String v = e >= 0 ? body.substring(s, e) : body.substring(s);
            v.replace('+',' ');
            String out; out.reserve(v.length());
            for (size_t i=0;i<v.length();++i){
                if (v[i]=='%' && i+2<v.length()){ char h[3]={v[i+1],v[i+2],0}; out += (char)strtol(h,nullptr,16); i+=2; }
                else out += v[i];
            }
            return out;
        };
        username = getParam("username"); if (!username.length()) username = getParam("user"); if (!username.length()) username = getParam("login");
        password = getParam("password"); if (!password.length()) password = getParam("pass");
    }

    const String expUser = _cfg ? _cfg->GetString(WEB_USER_KEY, WEB_USER_DEFAULT) : String(WEB_USER_DEFAULT);
    const String expPass = _cfg ? _cfg->GetString(WEB_PASS_KEY, WEB_PASS_DEFAULT) : String(WEB_PASS_DEFAULT);
    const bool ok = (username == expUser) && (password == expPass);

    const bool wantsJson =
        (req->hasHeader("Accept") && req->header("Accept").indexOf("application/json") >= 0) ||
        (req->hasHeader("X-Requested-With") && req->header("X-Requested-With").indexOf("XMLHttpRequest") >= 0) ||
        (req->hasHeader("Content-Type") && req->header("Content-Type").indexOf("application/json") >= 0);

    if (!ok) {
        if (wantsJson) {
            DynamicJsonDocument res(64); res[J_OK] = false;
            String s; serializeJson(res, s);
            auto r = req->beginResponse(401, "application/json", s);
            req->send(r);
        } else {
            auto r = req->beginResponse(302, "text/plain", "");
            r->addHeader("Location", PAGE_LOGIN_FAIL);
            req->send(r);
        }
        return;
    }

    // Success → set cookie and redirect to /home.html (non-AJAX)
    _sessionToken = makeSessionToken();
    String cookie = String(COOKIE_NAME) + "=" + _sessionToken + "; Path=/; HttpOnly; SameSite=Lax";

    if (wantsJson) {
        auto r = req->beginResponse(200, "application/json", "{\"ok\":true}");
        r->addHeader("Set-Cookie", cookie);
        req->send(r);
    } else {
        auto r = req->beginResponse(302, "text/plain", "");
        r->addHeader("Set-Cookie", cookie);
        r->addHeader("Location", PAGE_HOME);
        req->send(r);
    }
}

void WiFiManager::hLogout(AsyncWebServerRequest* req) {
    _sessionToken.clear();
    auto r = req->beginResponse(302, "text/plain", "");
    r->addHeader("Set-Cookie", String(COOKIE_NAME) + "=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
    r->addHeader("Location", PAGE_LOGIN);
    req->send(r);
}

bool WiFiManager::isAuthed(AsyncWebServerRequest* req) const {
    if (_sessionToken.isEmpty()) return false;
    String c = readCookie(req, COOKIE_NAME);
    return c.length() && c == _sessionToken;
}

String WiFiManager::readCookie(AsyncWebServerRequest* req, const String& name) const {
    if (!req->hasHeader("Cookie")) return String();
    String cookie = req->header("Cookie");
    String key = name + "=";
    int p = cookie.indexOf(key);
    if (p < 0) return String();
    p += key.length();
    int end = cookie.indexOf(';', p);
    return end >= 0 ? cookie.substring(p, end) : cookie.substring(p);
}

String WiFiManager::makeSessionToken() const {
    uint32_t r1 = (uint32_t)esp_random();
    uint32_t r2 = (uint32_t)esp_random();
    char buf[17];
    snprintf(buf, sizeof(buf), "%08X%08X", r1, r2);
    return String(buf);
}
/* ==================== uploads ==================== */
void handleFileUpload(AsyncWebServerRequest *request,
                      const String& filename, size_t index,
                      uint8_t *data, size_t len, bool final) {
    static File up;
    if (index == 0) {
        if (SPIFFS.exists(SLAVE_CONFIG_PATH)) SPIFFS.remove(SLAVE_CONFIG_PATH);
        up = SPIFFS.open(SLAVE_CONFIG_PATH, FILE_WRITE);
        if (!up) { request->send(500, "text/plain", "open failed"); return; }
    }
    if (len) up.write(data, len);
    if (final) {
        up.close();
        if (WiFiManager::instance) WiFiManager::instance->syncSpifToPrefs();
        request->send(200, "text/plain", "OK");
    }
}

/* ==================== helpers ==================== */
void WiFiManager::sendJSON(AsyncWebServerRequest* req, const JsonDocument& doc, int code) {
    String s; serializeJson(doc, s);
    req->send(code, "application/json", s);
}
void WiFiManager::sendError(AsyncWebServerRequest* req, const char* msg, int code) {
    DynamicJsonDocument d(128); d[J_ERR] = msg; sendJSON(req, d, code);
}
void WiFiManager::sendOK(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(64); d[J_OK] = true; sendJSON(req, d, 200);
}

void WiFiManager::attachTopologyLinks(DynamicJsonDocument& dst) {
    if (!_esn) return;
    String topo = _esn->serializeTopology();
    DynamicJsonDocument t(65536);
    if (!deserializeJson(t, topo)) dst[J_LINKS] = t[J_LINKS];
}

bool WiFiManager::applyExportedConfig(const JsonVariantConst& cfgObj) {
    bool ok = true;
    if (!cfgObj.isNull() && cfgObj.containsKey(J_LINKS) && _esn) ok &= _esn->configureTopology(cfgObj[J_LINKS]);
    if (!cfgObj.isNull() && cfgObj.containsKey(J_ESN_CH)) {
        int ch = cfgObj[J_ESN_CH].as<int>();
        if (ch >= 1 && ch <= 13) {
            if (_cfg) _cfg->PutInt(ESPNOW_CH_KEY, ch);
            if (_apOn && _esn) _esn->setChannel((uint8_t)ch);
        }
    }
    return ok;
}

// ---- WiFiManager.cpp — complete hSysStatus(.) ----
void WiFiManager::hSysStatus(AsyncWebServerRequest* req) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }

  // Helper: stringify CoolingManager::Mode
  auto modeToStr = [](CoolingManager::Mode m) -> const char* {
    switch (m) {
      case CoolingManager::COOL_STOPPED: return "STOPPED";
      case CoolingManager::COOL_ECO:     return "ECO";
      case CoolingManager::COOL_NORMAL:  return "NORMAL";
      case CoolingManager::COOL_FORCED:  return "FORCED";
      case CoolingManager::COOL_AUTO:    return "AUTO";
      default:                           return "UNKNOWN";
    }
  };

  DynamicJsonDocument d(2048);

  // ------------------------------------------------------------------------
  // System MODE (AUTO / MANUAL)
  // Source of truth: NVS key ESPNOW_MD_KEY as int (0=AUTO, 1=MANUAL).
  // The UI (home.js) reads a simple string "mode": "AUTO" | "MANUAL".
  // We also mirror boolean "manual" and numeric "mode_code" for flexibility.
  // ------------------------------------------------------------------------
  const int mdCode = _cfg ? _cfg->GetInt(ESPNOW_MD_KEY, ESPNOW_MD_DEFAULT) : ESPNOW_MD_DEFAULT;
  const bool isManual = (mdCode == 1);
  d["mode"]      = isManual ? "MANUAL" : "AUTO";
  d["manual"]    = isManual;
  d["mode_code"] = mdCode;

  // ------------------------------------------------------------------------
  // Wi-Fi summary (AP / STA / OFF)
  // ------------------------------------------------------------------------
  {
    JsonObject wifi = d.createNestedObject("wifi");
    if (_apOn) {
      wifi["mode"] = "AP";
      wifi["ip"]   = (_wifi ? _wifi->softAPIP().toString() : String());
      wifi["ch"]   = _cfg ? _cfg->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT) : ESPNOW_CH_DEFAULT;
    } else if (_wifi && _wifi->status() == WL_CONNECTED) {
      wifi["mode"] = "STA";
      wifi["ip"]   = _wifi->localIP().toString();
      wifi["ch"]   = _wifi->channel();
      wifi["rssi"] = _wifi->RSSI();
    } else {
      wifi["mode"] = "OFF";
    }
  }

  // ------------------------------------------------------------------------
  // Power + Health (basic placeholders — adjust if you have real sensors)
  // ------------------------------------------------------------------------
  {
    JsonObject power = d.createNestedObject("power");
    power["source"] = "WALL";   // or "BATTERY" if you detect it
    power["ok"]     = true;
    power["detail"] = "nominal";

    JsonObject health = d.createNestedObject("health");
    health.createNestedArray("faults"); // empty => “No faults”
  }

  // ------------------------------------------------------------------------
  // Time: prefer RTC if available; otherwise system clock
  // ------------------------------------------------------------------------
  {
    JsonObject t = d.createNestedObject("time");
    bool haveRTC = false;

    RTCManager* rtcMgr = (_esn ? _esn->rtc() : nullptr);
    if (rtcMgr && !rtcMgr->lostPower()) {
      t["iso"] = isoFromRTC(rtcMgr);               // "YYYY-MM-DDTHH:MM:SSZ"
      bool tok = false;
      float trtc = rtcMgr->readTemperatureC(&tok); // DS3231 die temperature
      if (tok && isfinite(trtc)) t["tempC"] = trtc;
      haveRTC = true;
    }
    if (!haveRTC) {
      t["iso"] = isoFromSystemClock();             // formatted system time
    }
  }

  // ------------------------------------------------------------------------
  // Uptime
  // ------------------------------------------------------------------------
  d["uptime_ms"] = (uint32_t)millis();

  // ------------------------------------------------------------------------
  // Buzzer toggle (from NVS)
  // ------------------------------------------------------------------------
  d["buzzer_enabled"] = _cfg ? _cfg->GetBool(BUZZER_FEEDBACK_KEY, BUZZER_FEEDBACK_DEFAULT)
                             : BUZZER_FEEDBACK_DEFAULT;

  // ------------------------------------------------------------------------
  // Cooling snapshot
  // ------------------------------------------------------------------------
  {
    JsonObject c = d.createNestedObject("cooling");
    if (_cool) {
      const float tc = _cool->lastTempC();
      if (!isnan(tc)) c["tempC"] = tc;
      c["speedPct"]      = (int)_cool->lastSpeedPct();
      c["modeApplied"]   = modeToStr(_cool->modeApplied());
      c["modeRequested"] = modeToStr(_cool->modeRequested());
    } else {
      c["modeApplied"] = "STOPPED";
      c["speedPct"]    = 0;
    }
  }

  // ------------------------------------------------------------------------
  // Sleep / inactivity (if present)
  // ------------------------------------------------------------------------
  if (_slp) {
    JsonObject s = d.createNestedObject("sleep");

    const long secsLeft = _slp->secondsUntilSleep(); // negative => due/armed
    uint32_t timeoutSec = 0;
    if (secsLeft >= 0) {
      const uint32_t now  = _slp->nowEpoch();
      const uint32_t last = _slp->lastActivityEpoch();
      timeoutSec = (uint32_t)secsLeft + (now - last);
    }
    s["timeout_sec"]         = timeoutSec;            // used to prefill input
    s["secs_to_sleep"]       = (int32_t)secsLeft;
    s["last_activity_epoch"] = _slp->lastActivityEpoch();
    s["next_wake_epoch"]     = _slp->nextWakeEpoch();
    s["armed"]               = _slp->isArmed();
  }

  // Send JSON (helper ensures proper headers)
  sendJSON(req, d);
}

// POST { "enabled": true|false }
void WiFiManager::hBuzzerSet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }

  static String body; if (index == 0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(128);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  JsonVariantConst root = d.as<JsonVariantConst>();
  if (root.isNull() || !root.containsKey("enabled")) { sendError(req, "Missing 'enabled'"); return; }

  bool en = root["enabled"].as<bool>();

  if (_cfg) _cfg->PutBool(BUZZER_FEEDBACK_KEY, en);

  DynamicJsonDocument res(64);
  res["ok"] = true;
  res["enabled"] = en;
  sendJSON(req, res);
}

void WiFiManager::hSystemReset(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  (void)data; (void)len; (void)index; (void)total;
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }

  if (_cfg) _cfg->PutBool(RESET_FLAG_KEY, true);
  sendOK(req);
}

void WiFiManager::hSystemRestart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  (void)data; (void)len; (void)index; (void)total;
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }

  sendOK(req);
  delay(150);
  ESP.restart();
}

void WiFiManager::hCoolStatus(AsyncWebServerRequest* req) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  DynamicJsonDocument d(256);
  if (_cool) {
    float t = _cool->lastTempC();
    if (!isnan(t)) d["tempC"] = t;
    d["speedPct"] = _cool->lastSpeedPct();
    auto modeToStr = [](CoolingManager::Mode m)->const char*{
      switch(m){
        case CoolingManager::COOL_STOPPED: return "STOPPED";
        case CoolingManager::COOL_ECO:     return "ECO";
        case CoolingManager::COOL_NORMAL:  return "NORMAL";
        case CoolingManager::COOL_FORCED:  return "FORCED";
        case CoolingManager::COOL_AUTO:    return "AUTO";
        default: return "UNKNOWN";
      }
    };
    d["modeApplied"]   = modeToStr(_cool->modeApplied());
    d["modeRequested"] = modeToStr(_cool->modeRequested());
  }
  sendJSON(req, d);
}

static bool parseJsonBody(AsyncWebServerRequest* req, String& out) {
  // accumulate body from chunked upload
  // NOTE: call this in the lambda—here we only define helper for clarity
  return false;
}

void WiFiManager::hCoolMode(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  static String body; if (index==0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(128);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  const char* m = d["mode"] | nullptr;
  if (!m) { sendError(req, "Missing 'mode'"); return; }

  if (_cool) {
    auto strToMode = [](const String& s)->CoolingManager::Mode{
      if (s=="AUTO")    return CoolingManager::COOL_AUTO;
      if (s=="ECO")     return CoolingManager::COOL_ECO;
      if (s=="NORMAL")  return CoolingManager::COOL_NORMAL;
      if (s=="FORCED")  return CoolingManager::COOL_FORCED;
      if (s=="STOPPED") return CoolingManager::COOL_STOPPED;
      return CoolingManager::COOL_AUTO;
    };
    _cool->setMode(strToMode(String(m)));   // applies on next periodic update
  }
  sendOK(req);
}

void WiFiManager::hCoolSpeed(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  static String body; if (index==0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(128);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  int pct = d["pct"] | -1;
  if (pct < 0 || pct > 100) { sendError(req, "pct 0..100"); return; }

  if (_cool) _cool->setManualSpeedPct((uint8_t)pct);
  sendOK(req);
}


void WiFiManager::hSleepStatus(AsyncWebServerRequest* req) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  DynamicJsonDocument d(256);
  if (_slp) {
    d["timeout_sec"]       = SLEEP_TIMEOUT_SEC_DEFAULT; // if you store it, emit stored value
    d["secs_to_sleep"]     = _slp->secondsUntilSleep();
    d["last_activity_epoch"]= _slp->lastActivityEpoch();
    d["next_wake_epoch"]   = _slp->nextWakeEpoch();
    d["armed"]             = _slp->isArmed();
  }
  sendJSON(req, d);
}

void WiFiManager::hSleepTimeout(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  static String body; if (index==0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(128);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  uint32_t sec = d["timeout_sec"] | 0;
  if (!sec) { sendError(req, "timeout_sec > 0"); return; }

  if (_slp) _slp->setInactivityTimeoutSec(sec);
  sendOK(req);
}

void WiFiManager::hSleepReset(AsyncWebServerRequest* req) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  if (_slp) _slp->resetActivity();
  sendOK(req);
}

void WiFiManager::hSleepSchedule(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  static String body; if (index==0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(128);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }

  bool ok = false;
  if (d.containsKey("after_sec")) {
    uint32_t s = d["after_sec"] | 1;
    if (_slp) ok = _slp->sleepAfterSeconds(s);
  } else if (d.containsKey("wake_epoch")) {
    uint32_t e = d["wake_epoch"] | 0;
    if (_slp && e) ok = _slp->sleepUntilEpoch(e);
  } else {
    sendError(req, "Need 'after_sec' or 'wake_epoch'"); return;
  }

  if (!ok) { sendError(req, "Schedule failed", 500); return; }
  sendOK(req);
}
void WiFiManager::hSystemMode(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;

  // Parse either JSON body or query string (?mode=AUTO/MANUAL or ?manual=0/1)
  String modeStr;      // "AUTO" or "MANUAL"
  bool   hasManual = false;
  bool   manualBool = false;

  // 1) JSON
  bool jsonParsed = false;
  {
    DynamicJsonDocument d(256);
    if (!deserializeJson(d, body)) {
      jsonParsed = true;
      JsonVariantConst root = d.as<JsonVariantConst>();
      if (!root.isNull()) {
        if (root.containsKey(J_MODE))   modeStr = String(root[J_MODE].as<const char*>());
        if (root.containsKey("manual")) { hasManual = true; manualBool = root["manual"].as<bool>(); }
      }
    }
  }

  // 2) Query fallback
  if (!modeStr.length() && !hasManual) {
    if (req->hasParam(J_MODE))   modeStr = req->getParam(J_MODE)->value();
    if (req->hasParam("manual")) { hasManual = true; manualBool = (req->getParam("manual")->value() == "1" || req->getParam("manual")->value() == "true"); }
  }

  // Normalize
  modeStr.toUpperCase();
  if (!modeStr.length() && hasManual) modeStr = manualBool ? J_MANUAL : J_AUTO;

  // Validate
  if (modeStr != J_AUTO && modeStr != J_MANUAL) { sendError(req, "mode must be AUTO or MANUAL"); return; }

  // Apply to ESP-NOW manager and persist to NVS
  bool ok = false;
  if (_esn) {
    if (modeStr == J_AUTO)   ok = _esn->setSystemModeAuto(true);
    else                     ok = _esn->setSystemModeManual(true);
  }
  // Also mirror to ConfigManager so /api/system/status can read it
  if (_cfg) _cfg->PutString(ESPNOW_MD_KEY, modeStr.c_str());

  DynamicJsonDocument res(128);
  res[J_OK]   = ok;
  res[J_MODE] = modeStr;
  sendJSON(req, res);
}
