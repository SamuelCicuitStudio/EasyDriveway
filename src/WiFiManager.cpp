/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : WiFiManager.cpp
 **************************************************************/

#include "WiFiManager.h"
#include "WiFiAPI.h"

WiFiManager* WiFiManager::instance = nullptr;

// -----------------------------------------------------------
// ctor
// -----------------------------------------------------------
WiFiManager::WiFiManager(ConfigManager* cfg,
                         WiFiClass*     wfi,
                         ESPNowManager* espnw,
                         ICMLogFS*      log)
: _cfg(cfg), _wifi(wfi), _esn(espnw), _log(log) {
    instance = this;
}

// -----------------------------------------------------------
// public
// -----------------------------------------------------------
void WiFiManager::begin() {
    if (!SPIFFS.begin(true)) {
        if (_log) _log->eventf(ICMLogFS::DOM_STORAGE, ICMLogFS::EV_ERROR, 5100, "SPIFFS mount failed");
    }

    addCORS();
    startAccessPoint();
    registerRoutes();

    if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 5101, "WiFiManager started (AP+Web)", "WiFiMgr");
}

void WiFiManager::disableWiFiAP() {
    if (_apOn) {
        _wifi->softAPdisconnect(true);
        _wifi->mode(WIFI_OFF);
        _apOn = false;
        if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 5102, "AP disabled", "WiFiMgr");
    }
}

void WiFiManager::setAPCredentials(const char* ssid, const char* password) {
    _cfg->PutString(DEVICE_WIFI_HOTSPOT_NAME_KEY, ssid);
    _cfg->PutString(DEVICE_AP_AUTH_PASS_KEY, password);
    if (_log) _log->eventf(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 5103, "AP creds updated ssid=%s", ssid);
}

// -----------------------------------------------------------
// net bring-up & routing
// -----------------------------------------------------------
void WiFiManager::addCORS() {
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
}

void WiFiManager::startAccessPoint() {
    String apSsid = _cfg->GetString(DEVICE_WIFI_HOTSPOT_NAME_KEY, DEVICE_WIFI_HOTSPOT_NAME_DEFAULT);
    String apPass = _cfg->GetString(DEVICE_AP_AUTH_PASS_KEY,      DEVICE_AP_AUTH_PASS_DEFAULT);

    if (!_wifi->softAPConfig(LOCAL_IP, GATEWAY, SUBNET)) {
        if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_ERROR, 5200, "softAPConfig failed", "WiFiMgr");
        return;
    }
    if (!_wifi->softAP(apSsid.c_str(), apPass.c_str())) {
        if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_ERROR, 5201, "softAP start failed", "WiFiMgr");
        return;
    }
    _apOn = true;
    if (_log) _log->eventf(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 5202, "AP up %s pass=%s ip=%s",
                           apSsid.c_str(), apPass.c_str(), _wifi->softAPIP().toString().c_str());
}

void WiFiManager::registerRoutes() {
    // static html/ui
    _server.on(API_ROOT, HTTP_GET,
               [this](AsyncWebServerRequest* r){ handleRoot(r); });
    _server.on(API_SETTINGS_HTML, HTTP_GET,
               [this](AsyncWebServerRequest* r){ handleSettings(r); });
    _server.on(API_WIFI_PAGE_HTML, HTTP_GET,
               [this](AsyncWebServerRequest* r){ handleWiFiPage(r); });
    _server.on(API_THANKYOU_HTML, HTTP_GET,
               [this](AsyncWebServerRequest* r){ handleThanks(r); });

    // configuration, export/import
    _server.on(API_CFG_LOAD, HTTP_GET,
               [this](AsyncWebServerRequest* r){ hCfgLoad(r); });

    _server.on(API_CFG_SAVE, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgSave(r,d,l,i,t); });

    _server.on(API_CFG_EXPORT, HTTP_GET,
               [this](AsyncWebServerRequest* r){ hCfgExport(r); });

    _server.on(API_CFG_IMPORT, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgImport(r,d,l,i,t); });

    _server.on(API_CFG_FACTORY_RESET, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgFactoryReset(r,d,l,i,t); });

    // peers
    _server.on(API_PEERS_LIST, HTTP_GET,
               [this](AsyncWebServerRequest* r){ hPeersList(r); });

    _server.on(API_PEER_PAIR, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPeerPair(r,d,l,i,t); });

    _server.on(API_PEER_REMOVE, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPeerRemove(r,d,l,i,t); });

    // manual / auto, relays & sensors
    _server.on(API_MODE_SET, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hModeSet(r,d,l,i,t); });

    _server.on(API_RELAY_SET, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hRelaySet(r,d,l,i,t); });

    _server.on(API_SENSOR_MODE, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSensorMode(r,d,l,i,t); });

    _server.on(API_SENSOR_TRIGGER, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSensorTrigger(r,d,l,i,t); });

    // topology (prev/next mapping)
    _server.on(API_TOPOLOGY_GET, HTTP_GET,
               [this](AsyncWebServerRequest* r){ hTopologyGet(r); });

    _server.on(API_TOPOLOGY_SET, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hTopologySet(r,d,l,i,t); });

    // sequence control
    _server.on(API_SEQUENCE_START, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSeqStart(r,d,l,i,t); });

    _server.on(API_SEQUENCE_STOP, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSeqStop(r,d,l,i,t); });

    // power module
    _server.on(API_POWER_INFO, HTTP_GET,
               [this](AsyncWebServerRequest* r){ hPowerInfo(r); });

    _server.on(API_POWER_CMD, HTTP_POST, nullptr, nullptr,
               [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPowerCmd(r,d,l,i,t); });

    // upload, icons, fonts, favicon, static
    _server.on(API_FILE_UPLOAD, HTTP_POST,
        [this](AsyncWebServerRequest* req){ req->send(200,"text/plain","File received"); },
        handleFileUpload);

    _server.on(API_FAVICON, HTTP_GET,
               [this](AsyncWebServerRequest* req){ req->send(204); });

    _server.serveStatic(API_STATIC_ICONS, SPIFFS, "/icons/").setCacheControl("max-age=86400");
    _server.serveStatic(API_STATIC_FONTS, SPIFFS, "/icons/").setCacheControl("max-age=86400");
    _server.serveStatic("/",              SPIFFS, "/");

    _server.begin();
}

// -----------------------------------------------------------
// HTML pages
// -----------------------------------------------------------
void WiFiManager::handleRoot(AsyncWebServerRequest* req)     { req->send(SPIFFS, "/welcome.html", "text/html"); }
void WiFiManager::handleThanks(AsyncWebServerRequest* req)   { req->send(SPIFFS, "/thankyou.html", "text/html"); }
void WiFiManager::handleSettings(AsyncWebServerRequest* req) { req->send(SPIFFS, "/settings.html", "text/html"); }
void WiFiManager::handleWiFiPage(AsyncWebServerRequest* req) { req->send(SPIFFS, "/wifiCredentialsPage.html", "text/html"); }

// -----------------------------------------------------------
// JSON helpers
// -----------------------------------------------------------
void WiFiManager::sendJSON(AsyncWebServerRequest* req, const JsonDocument& doc, int code) {
    String body; serializeJson(doc, body);
    req->send(code, "application/json", body);
}
void WiFiManager::sendError(AsyncWebServerRequest* req, const char* msg, int code) {
    DynamicJsonDocument d(128); d[J_ERR] = msg; sendJSON(req, d, code);
}
void WiFiManager::sendOK(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(64); d[J_OK] = true; sendJSON(req, d, 200);
}

// -----------------------------------------------------------
// Config load/export/import/save
// -----------------------------------------------------------
void WiFiManager::hCfgLoad(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(1024);
    d["ap_ssid"]   = _cfg->GetString(DEVICE_WIFI_HOTSPOT_NAME_KEY, DEVICE_WIFI_HOTSPOT_NAME_DEFAULT);
    d["ap_pass"]   = _cfg->GetString(DEVICE_AP_AUTH_PASS_KEY,      DEVICE_AP_AUTH_PASS_DEFAULT);
    d["ble_name"]  = _cfg->GetString(DEVICE_BLE_NAME_KEY,          DEVICE_BLE_NAME_DEFAULT);
    d["ble_pass"]  = _cfg->GetInt   (DEVICE_BLE_AUTH_PASS_KEY,     DEVICE_BLE_AUTH_PASS_DEFAULT);
    d["esn_ch"]    = _cfg->GetInt   ("EN-CH", ESPNOW_PEER_CHANNEL);  // 6-char key constraint handled elsewhere
    JsonArray links = d.createNestedArray(J_LINKS);
    _esn->serializeTopology(links);
    d["entrance_sensor"] = _esn->getEntranceSensorMac();
    d["parking_sensor"]  = _esn->getParkingSensorMac();
    sendJSON(req, d);
}

void WiFiManager::hCfgExport(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(4096);
    _esn->exportConfiguration(d.createNestedObject(J_CONFIG));
    sendJSON(req, d);
}

void WiFiManager::hCfgImport(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(8192);
    DeserializationError err = deserializeJson(d, body);
    if (err) { sendError(req, "Invalid JSON"); return; }

    if (d.containsKey(J_CONFIG)) {
        if (_esn->importConfiguration(d[J_CONFIG])) {
            if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 5300, "Config import OK", "WiFiMgr");
            sendOK(req);
            return;
        }
    }
    sendError(req, "Import failed", 500);
}

void WiFiManager::hCfgSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(1024);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }

    if (d.containsKey(J_AP_SSID)) _cfg->PutString(DEVICE_WIFI_HOTSPOT_NAME_KEY, d[J_AP_SSID].as<const char*>());
    if (d.containsKey(J_AP_PSK))  _cfg->PutString(DEVICE_AP_AUTH_PASS_KEY,      d[J_AP_PSK].as<const char*>());
    if (d.containsKey(J_BLE_NAME))_cfg->PutString(DEVICE_BLE_NAME_KEY,          d[J_BLE_NAME].as<const char*>());
    if (d.containsKey(J_BLE_PASS))_cfg->PutInt   (DEVICE_BLE_AUTH_PASS_KEY,     d[J_BLE_PASS].as<int>());

    if (d.containsKey(J_ESN_CH)) {
        int ch = d[J_ESN_CH].as<int>();
        if (ch >= 0 && ch <= 13) {
            _cfg->PutInt("EN-CH", ch);
            _esn->setChannel(ch); // live apply
        }
    }
    if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 5301, "Saved config", "WiFiMgr");
    sendOK(req);
}

void WiFiManager::hCfgFactoryReset(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)data; (void)len; (void)index; (void)total;
    _esn->removeAllPeers();
    _esn->clearAll();
    _cfg->PutBool(RESET_FLAG_KEY, true);
    if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_WARN, 5302, "Factory reset requested", "WiFiMgr");
    sendOK(req);
}

// -----------------------------------------------------------
// Peers
// -----------------------------------------------------------
void WiFiManager::hPeersList(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(8192);
    _esn->serializePeers(d.to<JsonVariant>());
    sendJSON(req, d);
}

void WiFiManager::hPeerPair(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(512);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }

    const char* mac = d[J_MAC]  | nullptr;
    const char* type= d[J_TYPE] | nullptr;
    if (!mac || !type) { sendError(req, "Missing mac/type"); return; }

    if (_esn->pair(mac, type)) {
        if (_log) _log->eventf(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 5400, "Paired %s type=%s", mac, type);
        sendOK(req);
        return;
    }
    sendError(req, "Pair failed", 500);
}

void WiFiManager::hPeerRemove(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(256);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    const char* mac = d[J_MAC] | nullptr;
    if (!mac) { sendError(req, "Missing mac"); return; }

    if (_esn->removePeer(mac)) {
        if (_log) _log->eventf(ICMLogFS::DOM_WIFI, ICMLogFS::EV_INFO, 5401, "Removed %s", mac);
        sendOK(req);
        return;
    }
    sendError(req, "Remove failed", 500);
}

// -----------------------------------------------------------
// Modes & controls
// -----------------------------------------------------------
void WiFiManager::hModeSet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(128);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }

    String mode = d[J_MODE] | "";
    bool ok = false;
    if      (mode.equalsIgnoreCase(J_AUTO))   ok = _esn->setSystemModeAuto();
    else if (mode.equalsIgnoreCase(J_MANUAL)) ok = _esn->setSystemModeManual();
    else { sendError(req, "Mode must be AUTO|MANUAL"); return; }

    if (ok) {
        if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 5500, "System mode changed", "WiFiMgr");
        sendOK(req);
    } else {
        sendError(req, "Mode change failed", 500);
    }
}

void WiFiManager::hRelaySet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;
    DynamicJsonDocument d(256);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }

    const char* mac = d[J_MAC] | nullptr;
    int state = d[J_STATE] | -1;
    if (!mac || (state < 0)) { sendError(req, "Missing mac/state"); return; }
    if (_esn->relayManualSet(mac, state != 0)) { sendOK(req); }
    else { sendError(req, "Relay set failed", 500); }
}

void WiFiManager::hSensorMode(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;
    DynamicJsonDocument d(256);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }

    const char* mac = d[J_MAC] | nullptr;
    String mode = d[J_MODE] | "";
    if (!mac || mode.isEmpty()) { sendError(req, "Missing mac/mode"); return; }

    bool ok=false;
    if      (mode.equalsIgnoreCase(J_AUTO))   ok = _esn->sensorSetMode(mac, true);
    else if (mode.equalsIgnoreCase(J_MANUAL)) ok = _esn->sensorSetMode(mac, false);
    else { sendError(req, "Mode must be AUTO|MANUAL"); return; }

    if (ok) sendOK(req); else sendError(req, "Sensor mode failed", 500);
}

void WiFiManager::hSensorTrigger(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;
    DynamicJsonDocument d(256);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }

    const char* mac = d[J_MAC] | nullptr;
    if (!mac) { sendError(req, "Missing mac"); return; }

    if (_esn->sensorTestTrigger(mac)) sendOK(req); else sendError(req, "Trigger failed", 500);
}

// -----------------------------------------------------------
// Topology
// -----------------------------------------------------------
void WiFiManager::hTopologyGet(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(4096);
    JsonArray links = d.createNestedArray(J_LINKS);
    _esn->serializeTopology(links);
    d["entrance_sensor"] = _esn->getEntranceSensorMac();
    d["parking_sensor"]  = _esn->getParkingSensorMac();
    sendJSON(req, d);
}

void WiFiManager::hTopologySet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(8192);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }

    if (!d.containsKey(J_LINKS)) { sendError(req, "Missing 'links'"); return; }

    bool ok = _esn->configureTopology(d[J_LINKS]);
    if (d.containsKey("entrance_sensor")) _esn->setEntranceSensor(d["entrance_sensor"].as<const char*>());
    if (d.containsKey("parking_sensor"))  _esn->setParkingSensor (d["parking_sensor"].as<const char*>());

    if (ok) {
        if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 5600, "Topology set", "WiFiMgr");
        sendOK(req);
    } else {
        sendError(req, "Topology set failed", 500);
    }
}

// -----------------------------------------------------------
// Auto sequence
// -----------------------------------------------------------
void WiFiManager::hSeqStart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;

    DynamicJsonDocument d(512);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    String start = d[J_START] | "";
    String dir   = d[J_DIR]   | J_UP;

    bool up = !dir.equalsIgnoreCase(J_DOWN);
    bool ok = _esn->startSequence(start.c_str(), up); // ICM triggers sequence (even in AUTO)
    if (ok) {
        if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 5700, "Sequence start", "WiFiMgr");
        sendOK(req);
    } else {
        sendError(req, "Sequence start failed", 500);
    }
}

void WiFiManager::hSeqStop(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)data;(void)len;(void)index;(void)total;
    bool ok = _esn->stopSequence();
    if (ok) {
        if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 5701, "Sequence stop", "WiFiMgr");
        sendOK(req);
    } else {
        sendError(req, "Sequence stop failed", 500);
    }
}

// -----------------------------------------------------------
// Power module
// -----------------------------------------------------------
void WiFiManager::hPowerInfo(AsyncWebServerRequest* req) {
    DynamicJsonDocument d(512);
    _esn->getPowerModuleInfo(d.to<JsonVariant>());
    sendJSON(req, d);
}

void WiFiManager::hPowerCmd(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body; if (index == 0) body.clear();
    body += String((char*)data).substring(0, len);
    if (index + len < total) return;
    DynamicJsonDocument d(256);
    if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
    const char* act = d[J_PWRACT] | nullptr;
    if (!act) { sendError(req, "Missing action"); return; }
    bool ok = _esn->powerCommand(act);
    if (ok) sendOK(req); else sendError(req, "Power cmd failed", 500);
}

// -----------------------------------------------------------
// SPIFFS -> Prefs selective sync (legacy helper)
// -----------------------------------------------------------
void WiFiManager::syncSpifToPrefs() {
    if (!SPIFFS.begin()) return;
    File f = SPIFFS.open(SLAVE_CONFIG_PATH, "r");
    if (!f) return;
    DynamicJsonDocument d(8192);
    if (deserializeJson(d, f)) { f.close(); return; }
    f.close();

    if (_esn->importLegacyLayout(d.as<JsonObject>())) {
        if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 5800, "Legacy config synced to prefs", "WiFiMgr");
    }
}

// -----------------------------------------------------------
// Upload helper
// -----------------------------------------------------------
void handleFileUpload(AsyncWebServerRequest *request, const String& filename,
                      size_t index, uint8_t *data, size_t len, bool final) {
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
