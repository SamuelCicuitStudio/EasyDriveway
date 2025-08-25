/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : WiFiManager.cpp (fixed + full rewrite)
 **************************************************************/
#include "WiFiManager.h"
#include "WiFiAPI.h"

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Use the macro path defined in Config.h:
//   #define SLAVE_CONFIG_PATH "/config/SlaveConfig.json"
// Do NOT redeclare a symbol with the same name here.

WiFiManager* WiFiManager::instance = nullptr;

// -----------------------------------------------------------
// lifecycle
// -----------------------------------------------------------
void WiFiManager::begin() {
    instance = this;

    if (!SPIFFS.begin(true)) {
        if (_log) _log->event(ICMLogFS::DOM_STORAGE, ICMLogFS::EV_ERROR, 6000, "SPIFFS mount failed", "WiFiMgr");
    }

    addCORS();
    registerRoutes();

    // Decide STA vs AP
    const bool gotoCfg = _cfg ? _cfg->GetBool(GOTO_CONFIG_KEY, false) : false;
    bool staOk = false;

    if (!gotoCfg) {
        staOk = tryConnectSTAFromNVS();
    }

    if (!staOk) {
        startAccessPoint();
    }

    if (_log) {
        _log->event(
            ICMLogFS::DOM_WIFI,
            ICMLogFS::EV_INFO,
            6001,
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
        if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 6002, "AP disabled", "WiFiMgr");
    }
}

void WiFiManager::setAPCredentials(const char* ssid, const char* password) {
    if (_cfg) {
        _cfg->PutString(DEVICE_WIFI_HOTSPOT_NAME_KEY, ssid);
        _cfg->PutString(DEVICE_AP_AUTH_PASS_KEY,      password);
    }
    if (_log) _log->eventf(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 6003, "AP creds updated ssid=%s", ssid);
}

void WiFiManager::syncSpifToPrefs() {
    if (!SPIFFS.exists(SLAVE_CONFIG_PATH)) return;
    File f = SPIFFS.open(SLAVE_CONFIG_PATH, FILE_READ);
    if (!f) return;

    DynamicJsonDocument d(32768);
    DeserializationError err = deserializeJson(d, f);
    f.close();
    if (err) return;

    JsonVariantConst root = d.as<JsonVariantConst>();
    if (!root.isNull() && root.containsKey(J_CONFIG)) {
        JsonVariantConst cfgObj = root[J_CONFIG];
        applyExportedConfig(cfgObj);
    }
    if (!root.isNull() && root.containsKey(J_LINKS)) {
        _esn->configureTopology(root[J_LINKS]);
    }
}

// -----------------------------------------------------------
// net bring-up & ESPNOW alignment (uses ConfigManager keys)
// -----------------------------------------------------------
bool WiFiManager::tryConnectSTAFromNVS(uint32_t timeoutMs) {
    // 1) Try ConfigManager-provided STA creds first
    const String ssid = _cfg ? _cfg->GetString(WIFI_STA_SSID_KEY, "") : "";
    const String psk  = _cfg ? _cfg->GetString(WIFI_STA_PASS_KEY,  "") : "";

    _wifi->mode(WIFI_STA);
    _wifi->setAutoConnect(true);
    _wifi->setAutoReconnect(true);
    _wifi->disconnect(true);
    delay(50);

    if (ssid.length()) {
        // Prefer your own NVS (ConfigManager) credentials
        // Avoid writing them into WiFi's internal NVS (keep your source of truth in _cfg)
        _wifi->persistent(false);
        if (psk.length()) _wifi->begin(ssid.c_str(), psk.c_str());
        else              _wifi->begin(ssid.c_str());
    } else {
        // Fallback: use WiFi's internal NVS (if you previously called WiFi.begin(ssid,pass))
        _wifi->persistent(true);
        _wifi->begin();  // no args => use WiFi NVS
    }

    // Wait for connect
    const uint32_t start = millis();
    while (_wifi->status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
        delay(100);
    }
    if (_wifi->status() != WL_CONNECTED) {
        if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_WARN, 6010,
                              "STA connect failed; will use AP", "WiFiMgr");
        return false;
    }

    if (_log) {
        _log->eventf(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 6011,
                     "STA connected ip=%s rssi=%d ch=%d",
                     _wifi->localIP().toString().c_str(), _wifi->RSSI(), _wifi->channel());
    }

    alignESPNOWToCurrentChannel();
    return true;
}

void WiFiManager::startAccessPoint() {
    // AP creds from ConfigManager (your NVS keys in Config.h)
    String apSsid = _cfg ? _cfg->GetString(DEVICE_WIFI_HOTSPOT_NAME_KEY, DEVICE_WIFI_HOTSPOT_NAME_DEFAULT)
                         : String(DEVICE_WIFI_HOTSPOT_NAME_DEFAULT);
    String apPass = _cfg ? _cfg->GetString(DEVICE_AP_AUTH_PASS_KEY,      DEVICE_AP_AUTH_PASS_DEFAULT)
                         : String(DEVICE_AP_AUTH_PASS_DEFAULT);

    int ch = _cfg ? _cfg->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT) : ESPNOW_CH_DEFAULT;
    if (ch < 1 || ch > 13) ch = ESPNOW_CH_DEFAULT;

    _wifi->mode(WIFI_AP);
    if (!_wifi->softAPConfig(LOCAL_IP, GATEWAY, SUBNET)) {
        if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_ERROR, 6020, "softAPConfig failed", "WiFiMgr");
        return;
    }
    if (!_wifi->softAP(apSsid.c_str(), apPass.c_str(), ch)) {
        if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_ERROR, 6021, "softAP start failed", "WiFiMgr");
        return;
    }
    _apOn = true;

    // Align ESP-NOW to AP channel and persist it via your ConfigManager key
    if (_esn) _esn->setChannel((uint8_t)ch);
    if (_cfg) _cfg->PutInt(ESPNOW_CH_KEY, ch);

    if (_log) {
        _log->eventf(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 6022,
                     "AP up %s pass=%s ip=%s ch=%d",
                     apSsid.c_str(), apPass.c_str(), _wifi->softAPIP().toString().c_str(), ch);
    }
}

void WiFiManager::alignESPNOWToCurrentChannel() {
    // When in STA, bind ESP-NOW to the router’s channel; persist it in your own key
    uint8_t ch = _wifi->channel();
    if (ch < 1 || ch > 13) ch = ESPNOW_CH_DEFAULT;
    if (_esn) _esn->setChannel(ch);
    if (_cfg) _cfg->PutInt(ESPNOW_CH_KEY, (int)ch);
}
// -----------------------------------------------------------
// routing
// -----------------------------------------------------------
void WiFiManager::addCORS() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin",  "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
}

void WiFiManager::registerRoutes() {
    // static html/ui
    _server.on(API_ROOT,           HTTP_GET, [this](AsyncWebServerRequest* r){ handleRoot(r); });
    _server.on(API_SETTINGS_HTML,  HTTP_GET, [this](AsyncWebServerRequest* r){ handleSettings(r); });
    _server.on(API_WIFI_PAGE_HTML, HTTP_GET, [this](AsyncWebServerRequest* r){ handleWiFiPage(r); });
    _server.on(API_THANKYOU_HTML,  HTTP_GET, [this](AsyncWebServerRequest* r){ handleThanks(r); });

    // Wi-Fi control
    _server.on(API_WIFI_MODE,          HTTP_GET, [this](AsyncWebServerRequest* r){ hWiFiMode(r); });
    _server.on(API_WIFI_STA_CONNECT,   HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hWiFiStaConnect(r,d,l,i,t); });
    _server.on(API_WIFI_STA_DISCONNECT,HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hWiFiStaDisconnect(r,d,l,i,t); });
    _server.on(API_WIFI_AP_START,      HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hWiFiApStart(r,d,l,i,t); });
    _server.on(API_WIFI_SCAN,          HTTP_GET, [this](AsyncWebServerRequest* r){ hWiFiScan(r); });

    // config, export/import
    _server.on(API_CFG_LOAD,           HTTP_GET, [this](AsyncWebServerRequest* r){ hCfgLoad(r); });
    _server.on(API_CFG_SAVE,           HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgSave(r,d,l,i,t); });
    _server.on(API_CFG_EXPORT,         HTTP_GET, [this](AsyncWebServerRequest* r){ hCfgExport(r); });
    _server.on(API_CFG_IMPORT,         HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgImport(r,d,l,i,t); });
    _server.on(API_CFG_FACTORY_RESET,  HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgFactoryReset(r,d,l,i,t); });

    // peers
    _server.on(API_PEERS_LIST,         HTTP_GET, [this](AsyncWebServerRequest* r){ hPeersList(r); });
    _server.on(API_PEER_PAIR,          HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPeerPair(r,d,l,i,t); });
    _server.on(API_PEER_REMOVE,        HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPeerRemove(r,d,l,i,t); });

    // topology
    _server.on(API_TOPOLOGY_SET,       HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hTopoSet(r,d,l,i,t); });
    _server.on(API_TOPOLOGY_GET,       HTTP_GET, [this](AsyncWebServerRequest* r){ hTopoGet(r); });

    // sequence
    _server.on(API_SEQUENCE_START,     HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSeqStart(r,d,l,i,t); });
    _server.on(API_SEQUENCE_STOP,      HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSeqStop(r,d,l,i,t); });

    // power
    _server.on(API_POWER_INFO,         HTTP_GET, [this](AsyncWebServerRequest* r){ hPowerInfo(r); });
    _server.on(API_POWER_CMD,          HTTP_POST, nullptr, nullptr,
        [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPowerCmd(r,d,l,i,t); });

    // uploads and assets
    _server.on(API_FILE_UPLOAD, HTTP_POST,
        [this](AsyncWebServerRequest* req){ req->send(200, "text/plain", "File received"); }, handleFileUpload);

    _server.on(API_FAVICON, HTTP_GET, [this](AsyncWebServerRequest* req){ req->send(204); });

    _server.serveStatic(API_STATIC_ICONS, SPIFFS, "/icons/").setCacheControl("max-age=86400");
    _server.serveStatic(API_STATIC_FONTS, SPIFFS, "/icons/").setCacheControl("max-age=86400");
    _server.serveStatic("/",              SPIFFS, "/");

    _server.begin();
}

// -----------------------------------------------------------
// HTML pages (fallback minimal text if files missing)
// -----------------------------------------------------------
void WiFiManager::handleRoot(AsyncWebServerRequest* request) {
    if (SPIFFS.exists("/index.html")) { request->send(SPIFFS, "/index.html", String()); return; }
    request->send(200, "text/plain", "ICM — WiFi Manager OK");
}
void WiFiManager::handleThanks(AsyncWebServerRequest* request) {
    if (SPIFFS.exists("/thankyou.html")) { request->send(SPIFFS, "/thankyou.html", String()); return; }
    request->send(200, "text/plain", "Thanks");
}
void WiFiManager::handleSettings(AsyncWebServerRequest* request) {
    if (SPIFFS.exists("/settings.html")) { request->send(SPIFFS, "/settings.html", String()); return; }
    request->send(200, "text/plain", "Settings page");
}
void WiFiManager::handleWiFiPage(AsyncWebServerRequest* request) {
    if (SPIFFS.exists("/wifi.html")) { request->send(SPIFFS, "/wifi.html", String()); return; }
    request->send(200, "text/plain", "Wi-Fi credentials page");
}

// -----------------------------------------------------------
// Config
// -----------------------------------------------------------
void WiFiManager::hCfgLoad(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(1024);
    d[J_AP_SSID] = _cfg ? _cfg->GetString(DEVICE_WIFI_HOTSPOT_NAME_KEY, DEVICE_WIFI_HOTSPOT_NAME_DEFAULT)
                        : String(DEVICE_WIFI_HOTSPOT_NAME_DEFAULT);
    d[J_AP_PSK]  = _cfg ? _cfg->GetString(DEVICE_AP_AUTH_PASS_KEY,      DEVICE_AP_AUTH_PASS_DEFAULT)
                        : String(DEVICE_AP_AUTH_PASS_DEFAULT);
    d[J_ESN_CH]  = _cfg ? _cfg->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT) : ESPNOW_CH_DEFAULT;

    JsonObject net = d.createNestedObject("net");
    if (_apOn) {
        net[J_MODE] = "AP";
        net["ip"]   = _wifi->softAPIP().toString();
        net["ch"]   = _cfg ? _cfg->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT) : ESPNOW_CH_DEFAULT;
    } else if (_wifi->status() == WL_CONNECTED) {
        net[J_MODE] = "STA";
        net["ip"]   = _wifi->localIP().toString();
        net["ch"]   = _wifi->channel();
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

    if (!root.isNull() && root.containsKey(J_AP_SSID)) _cfg->PutString(DEVICE_WIFI_HOTSPOT_NAME_KEY, root[J_AP_SSID].as<const char*>());
    if (!root.isNull() && root.containsKey(J_AP_PSK))  _cfg->PutString(DEVICE_AP_AUTH_PASS_KEY,      root[J_AP_PSK].as<const char*>());
    if (!root.isNull() && root.containsKey(J_BLE_NAME))_cfg->PutString(DEVICE_BLE_NAME_KEY,          root[J_BLE_NAME].as<const char*>());
    if (!root.isNull() && root.containsKey(J_BLE_PASS))_cfg->PutInt   (DEVICE_BLE_AUTH_PASS_KEY,     root[J_BLE_PASS].as<int>());

    if (!root.isNull() && root.containsKey(J_ESN_CH)) {
        int ch = root[J_ESN_CH].as<int>();
        if (ch >= 1 && ch <= 13) {
            _cfg->PutInt(ESPNOW_CH_KEY, ch);
            if (_apOn && _esn) _esn->setChannel((uint8_t)ch); // live apply in AP
        }
    }

    // Optional STA credentials — connect & persist via WiFi NVS
    if (!root.isNull() && root.containsKey(J_WIFI_SSID) && root.containsKey(J_WIFI_PSK)) {
        String ssid = root[J_WIFI_SSID].as<String>();
        String psk  = root[J_WIFI_PSK].as<String>();
        _wifi->mode(WIFI_STA);
        _wifi->persistent(true);
        _wifi->disconnect(true);
        delay(50);
        _wifi->begin(ssid.c_str(), psk.c_str());

        uint32_t start = millis();
        while (_wifi->status() != WL_CONNECTED && millis() - start < 15000) delay(100);
        if (_wifi->status() == WL_CONNECTED) {
            _apOn = false;
            alignESPNOWToCurrentChannel();
        }
    }

    if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 6100, "Saved config", "WiFiMgr");
    sendOK(req);
}

void WiFiManager::hCfgExport(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(65536);
    String blob = _esn->exportConfiguration(); // peers + topology + ch/mode (by your ESPNowManager)
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
    _esn->removeAllPeers();
    _esn->clearAll();
    _cfg->PutBool(RESET_FLAG_KEY, true);
    if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_WARN, 6110, "Factory reset requested", "WiFiMgr");
    sendOK(req);
}

// -----------------------------------------------------------
// Wi-Fi control
// -----------------------------------------------------------
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

    if (root.isNull() || !root.containsKey(J_WIFI_SSID) || !root.containsKey(J_WIFI_PSK)) {
        sendError(req, "Missing ssid/password");
        return;
    }

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

    String apSsid = root.containsKey(J_AP_SSID)
                    ? root[J_AP_SSID].as<String>()
                    : _cfg->GetString(DEVICE_WIFI_HOTSPOT_NAME_KEY, DEVICE_WIFI_HOTSPOT_NAME_DEFAULT);
    String apPass = root.containsKey(J_AP_PSK)
                    ? root[J_AP_PSK].as<String>()
                    : _cfg->GetString(DEVICE_AP_AUTH_PASS_KEY, DEVICE_AP_AUTH_PASS_DEFAULT);

    if (root.containsKey(J_AP_SSID)) _cfg->PutString(DEVICE_WIFI_HOTSPOT_NAME_KEY, apSsid.c_str());
    if (root.containsKey(J_AP_PSK))  _cfg->PutString(DEVICE_AP_AUTH_PASS_KEY,      apPass.c_str());

    if (root.containsKey(J_ESN_CH)) {
        int ch = root[J_ESN_CH].as<int>();
        if (ch >= 1 && ch <= 13) _cfg->PutInt(ESPNOW_CH_KEY, ch);
    }

    // restart AP
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

// -----------------------------------------------------------
// Peers
// -----------------------------------------------------------
void WiFiManager::hPeersList(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(8192);
    String peers = _esn->serializePeers();
    if (!deserializeJson(d, peers)) { sendJSON(req, d); }
    else { sendError(req, "Peers serialization error", 500); }
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
    bool ok = _esn->pair(mac, type);
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
    bool ok = _esn->removePeer(mac);
    if (ok) sendOK(req); else sendError(req, "Remove failed", 500);
}

// -----------------------------------------------------------
// Topology
// -----------------------------------------------------------
void WiFiManager::hTopoSet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(65536);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    JsonVariantConst root = d.as<JsonVariantConst>();

    if (!root.isNull() && root.containsKey(J_LINKS)) {
        bool ok = _esn->configureTopology(root[J_LINKS]);
        if (ok) {
            if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 6200, "Topology set", "WiFiMgr");
            sendOK(req);
        } else {
            sendError(req, "Topology set failed", 500);
        }
        return;
    }
    sendError(req, "Missing links[]");
}

void WiFiManager::hTopoGet(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(65536);
    String topo = _esn->serializeTopology();
    if (!deserializeJson(d, topo)) { sendJSON(req, d); }
    else { sendError(req, "Topology serialization error", 500); }
}

// -----------------------------------------------------------
// Auto sequence (stubs; hook into your sequence engine as needed)
// -----------------------------------------------------------
void WiFiManager::hSeqStart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;
    // TODO: forward to sequence controller
    sendOK(req);
}
void WiFiManager::hSeqStop(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)data;(void)len;(void)index;(void)total;
    // TODO: forward to sequence controller
    sendOK(req);
}

// -----------------------------------------------------------
// Power (passthrough)
// -----------------------------------------------------------
void WiFiManager::hPowerInfo(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(256);
    // TODO: hook to your power monitor
    d["status"] = "ok";
    sendJSON(req, d);
}

void WiFiManager::hPowerCmd(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)data;(void)len;(void)index;(void)total;
    // TODO: forward to power controller
    sendOK(req);
}

// -----------------------------------------------------------
// helpers
// -----------------------------------------------------------
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
    String topo = _esn->serializeTopology();
    DynamicJsonDocument t(65536);
    if (!deserializeJson(t, topo)) {
        dst[J_LINKS] = t[J_LINKS];
    }
}

bool WiFiManager::applyExportedConfig(const JsonVariantConst& cfgObj) {
    bool ok = true;
    if (!cfgObj.isNull() && cfgObj.containsKey(J_LINKS)) {
        ok &= _esn->configureTopology(cfgObj[J_LINKS]);
    }
    if (!cfgObj.isNull() && cfgObj.containsKey(J_ESN_CH)) {
        int ch = cfgObj[J_ESN_CH].as<int>();
        if (ch >= 1 && ch <= 13) {
            _cfg->PutInt(ESPNOW_CH_KEY, ch);
            if (_apOn && _esn) _esn->setChannel((uint8_t)ch);
        }
    }
    return ok;
}

// -----------------------------------------------------------
// file upload (chunked) — saves to SPIFFS then applies config
// -----------------------------------------------------------
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
