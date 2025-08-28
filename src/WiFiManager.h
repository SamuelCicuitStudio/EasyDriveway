/**************************************************************
 *  Project  : ICM (Interface Control Module)
 *  Headers  : WiFiManager.h
 *  Note     : Readability regrouping only — no API changes.
 *             All macros, types, methods preserved verbatim.
 **************************************************************/
// ============================== WiFiManager.h ==============================
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "esp_mac.h"

#include "SleepTimer.h"
#include "Config.h"          // NVS keys, IPs, ESPNOW_CH_KEY, WEB_USER_KEY, WEB_PASS_KEY...
#include "ConfigManager.h"   // GetString/PutString/GetInt/PutInt/GetBool/PutBool
#include "ESPNowManager.h"   // setChannel(), serializePeers(), pair(), configureTopology(), exportConfiguration()
#include "ICMLogFS.h"
#include "CoolingManager.h"

class WiFiManager {
public: // ======================= LIFECYCLE & CORE =======================
  enum NetMode : uint8_t { NET_AP=0, NET_STA=1 };

  WiFiManager(ConfigManager* cfg,
              WiFiClass*     wfi,
              ESPNowManager* espnw,
              ICMLogFS*      log,
              SleepTimer*    _slp)
  : _cfg(cfg), _wifi(wfi), _esn(espnw), _log(log), _slp(_slp) {}

  void begin();
  void disableWiFiAP();
  void setAPCredentials(const char* ssid, const char* password);
  void syncSpifToPrefs();

  static WiFiManager* instance; // singleton pointer for easy access

  void setCooling(CoolingManager* c) { _cool = c; }

private: // ======================= BRING‑UP / RADIO =======================
  bool tryConnectSTAFromNVS(uint32_t timeoutMs = 15000);
  void startAccessPoint();
  void alignESPNOWToCurrentChannel();

private: // ======================= ROUTER / CORS =========================
  void registerRoutes();
  void addCORS();

private: // ======================= HTML PAGES ============================
  void handleRoot(AsyncWebServerRequest* request);
  void handleHome(AsyncWebServerRequest* request);
  void handleSettings(AsyncWebServerRequest* request);
  void handleWiFiPage(AsyncWebServerRequest* request);
  void handleTopology(AsyncWebServerRequest* request);
  void handleLive(AsyncWebServerRequest* request);
  void handleThanks(AsyncWebServerRequest* request);

private: // ======================= JSON: CONFIG ==========================
  void hCfgLoad(AsyncWebServerRequest* req);
  void hCfgSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hCfgExport(AsyncWebServerRequest* req);
  void hCfgImport(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hCfgFactoryReset(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

private: // ======================= JSON: WIFI ============================
  void hWiFiMode(AsyncWebServerRequest* req);
  void hWiFiStaConnect(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hWiFiStaDisconnect(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hWiFiApStart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hWiFiScan(AsyncWebServerRequest* req);

private: // ======================= JSON: PEERS/TOPO ======================
  void hPeersList(AsyncWebServerRequest* req);
  void hPeerPair (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hPeerRemove(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hTopoSet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hTopoGet(AsyncWebServerRequest* req);
  void hSensorDayNight(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

private: // ======================= JSON: SEQUENCE ========================
  void hSeqStart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hSeqStop (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

private: // ======================= JSON: POWER ===========================
  void hPowerInfo(AsyncWebServerRequest* req);
  void hPowerCmd (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

private: // ======================= JSON: LIVE / STATUS ===================
  void hLiveStatus(AsyncWebServerRequest* req);
  void hSysStatus(AsyncWebServerRequest* req);

private: // ======================= JSON: QUICK CONTROLS ==================
  void hBuzzerSet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hSystemReset(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hSystemRestart(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hSystemMode(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

private: // ======================= JSON: COOLING =========================
  void hCoolStatus(AsyncWebServerRequest* req);
  void hCoolMode(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hCoolSpeed(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

private: // ======================= JSON: SLEEP ===========================
  void hSleepStatus(AsyncWebServerRequest* req);
  void hSleepTimeout(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void hSleepReset(AsyncWebServerRequest* req);
  void hSleepSchedule(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

private: // ======================= AUTH HELPERS ==========================
  bool   isAuthed(AsyncWebServerRequest* req) const;
  String readCookie(AsyncWebServerRequest* req, const String& name) const;
  String makeSessionToken() const;
  void   setSessionCookie(AsyncWebServerRequest* req, const String& token);
  void   clearSessionCookie(AsyncWebServerRequest* req);
  void   hLogin(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
  void   hLogout(AsyncWebServerRequest* req);

private: // ======================= JSON SENDERS & UTILS ==================
  void sendJSON(AsyncWebServerRequest* req, const JsonDocument& doc, int code=200);
  void sendError(AsyncWebServerRequest* req, const char* msg, int code=400);
  void sendOK(AsyncWebServerRequest* req);
  void attachTopologyLinks(DynamicJsonDocument& dst);
  bool applyExportedConfig(const JsonVariantConst& cfgObj);

private: // ======================= STATE ================================
  ConfigManager*  _cfg   = nullptr;
  WiFiClass*      _wifi  = nullptr;
  ESPNowManager*  _esn   = nullptr;
  ICMLogFS*       _log   = nullptr;
  SleepTimer*     _slp   = nullptr;
  CoolingManager* _cool  = nullptr;

  AsyncWebServer  _server{80};
  bool            _apOn  = false;
  String          _sessionToken; // simple RAM session
};

// upload handler (chunked)
void handleFileUpload(AsyncWebServerRequest *request,
                      const String& filename, size_t index,
                      uint8_t *data, size_t len, bool final);

#endif // WIFI_MANAGER_H