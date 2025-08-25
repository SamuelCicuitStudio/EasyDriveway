/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : WiFiManager.h
 *  Purpose     : Web UI + REST API for pairing, topology, control
 **************************************************************/

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include "Config.h"
#include "ConfigManager.h"
#include "ESPNowManager.h"
#include "ICMLogFS.h"
#include "WiFiAPI.h"

class WiFiManager {
public:
    // NOTE: WFi argument lets you inject a mock or the global WiFi singleton.
    WiFiManager(ConfigManager* cfg,
                WiFiClass*     wfi,
                ESPNowManager* espnw,
                ICMLogFS*      log);

    // lifecycle
    void begin();
    void disableWiFiAP(); // full stop

    // AP credentials quick setter
    void setAPCredentials(const char* ssid, const char* password);

    // SPIFFS->Prefs config sync (imported schema)
    void syncSpifToPrefs();

    // singleton accessor for upload callback
    static WiFiManager* instance;

private:
    // net bring-up
    void startAccessPoint();
    void registerRoutes();
    void addCORS();

    // route handlers (HTML)
    void handleRoot(AsyncWebServerRequest* request);
    void handleThanks(AsyncWebServerRequest* request);
    void handleSettings(AsyncWebServerRequest* request);
    void handleWiFiPage(AsyncWebServerRequest* request);

    // REST handlers (JSON)
    void hCfgLoad(AsyncWebServerRequest* req);
    void hCfgSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void hCfgExport(AsyncWebServerRequest* req);
    void hCfgImport(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void hCfgFactoryReset(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    void hPeersList(AsyncWebServerRequest* req);
    void hPeerPair (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void hPeerRemove(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    void hModeSet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void hRelaySet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void hSensorMode(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void hSensorTrigger(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    void hTopologyGet(AsyncWebServerRequest* req);
    void hTopologySet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    void hSeqStart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void hSeqStop (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    void hPowerInfo(AsyncWebServerRequest* req);
    void hPowerCmd (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    // helpers
    void sendJSON(AsyncWebServerRequest* req, const JsonDocument& doc, int code=200);
    void sendError(AsyncWebServerRequest* req, const char* msg, int code=400);
    void sendOK(AsyncWebServerRequest* req);

    // file upload (chunked)
    friend void handleFileUpload(AsyncWebServerRequest *request,
                                 const String& filename, size_t index,
                                 uint8_t *data, size_t len, bool final);

private:
    ConfigManager*  _cfg;
    WiFiClass*      _wifi;
    ESPNowManager*  _esn;
    ICMLogFS*       _log;

    AsyncWebServer  _server{80};
    bool            _apOn = false;
};

#endif // WIFI_MANAGER_H
