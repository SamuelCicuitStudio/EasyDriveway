/**************************************************************
 *  Project     : EasyDriveWay - Power Supply Module (PSM)
 *  File        : PSMEspNowManager.h  (FIXED to align with ICM CommandAPI)
 *  Purpose     : ESP-NOW slave for PSM + SD-card logging via ICMLogFS
 **************************************************************/
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "Config.h"
#include "ConfigManager.h"
#include "ICMLogFS.h"
#include "RTCManager.h"
#include "CommandAPI.h"
#include "PSMCommandAPI.h"

// ---------- Hardware measurement function prototypes (user will implement) ----------
bool     hwIs48VOn();       // returns true if 48V rail is ON
uint8_t  readFaultBits();   // bitfield (OVP/UVP/OCP/OTP/brownout…)
uint16_t measure48V_mV();   // 48V bus voltage (mV)
uint16_t measure48V_mA();   // 48V bus current (mA)
uint16_t measureBat_mV();   // battery voltage (mV)
uint16_t measureBat_mA();   // battery current (mA), +chg / –dischg
float    readBoardTempC();  // local board temperature in °C

class PSMEspNowManager {
public:
  // Callback types for integrating with the actual PSM hardware/logic
  typedef bool (*OnSetPowerFn)(bool on);                   // return true on success
  typedef bool (*OnClearFaultFn)();
  typedef bool (*OnRequestShutdownFn)();
  typedef bool (*OnComposeStatusFn)(PowerStatusPayload& out);// fill status, return ok  (FIXED type)
  typedef bool (*OnReadTempFn)(float& tC);                 // optional

public:
  PSMEspNowManager(ConfigManager* cfg, ICMLogFS* log, RTCManager* rtc);

  bool begin(uint8_t channelDefault = 1, const char* pmk16 = nullptr);
  void end();

  // Attach hardware callbacks
  void setOnSetPower(OnSetPowerFn fn)            { _onSetPower = fn; }
  void setOnClearFault(OnClearFaultFn fn)        { _onClearFault = fn; }
  void setOnRequestShutdown(OnRequestShutdownFn fn){ _onReqShutdown = fn; }
  void setOnComposeStatus(OnComposeStatusFn fn)  { _onComposeStatus = fn; }
  void setOnReadTemp(OnReadTempFn fn)            { _onReadTemp = fn; }

  // For main loop
  void poll();

  // Info
  String masterMacStr() const;
  String myMacStr() const;
  uint8_t channel() const { return _channel; }
  bool hasToken() const { for(int i=0;i<16;++i){ if(_token16[i]) return true; } return false; }

  // Manual utilities
  bool sendHeartbeat();
  bool sendCaps(uint8_t maj, uint8_t min, bool hasTemp, bool hasCharger, uint32_t features);

  // Non-volatile keys (<=6 chars each recommended)
  String keyCh()   const { return String("PCH"); }
  String keyTok()  const { return String("PTOK"); }
  String keyMac()  const { return String("PMAC"); }

private:
  // Static thunks
  static void onRecvThunk(const uint8_t *mac, const uint8_t *data, int len);
  static void onSentThunk(const uint8_t *mac, esp_now_send_status_t status);

  // Instance handlers
  void onRecv(const uint8_t *mac, const uint8_t *data, int len);
  void onSent(const uint8_t *mac, esp_now_send_status_t status);

  // Sending helpers
  bool sendToMaster(CmdDomain dom, uint8_t op, const void* payload, size_t len, uint16_t ctrEcho=0, bool ackReq=false);
  bool sendAck(uint16_t ctr, uint8_t code);
  bool sendPowerStatus(uint16_t ctr);
  bool sendTempReply(uint16_t ctr);

  // Channel switching
  void applyChannel(uint8_t ch, bool persist);

  // Token helpers
  bool tokenMatches(const IcmMsgHdr& h) const;

  // Tiny utils
  static String macBytesToStr(const uint8_t mac[6]);
  static bool   macStrToBytes(const String& s, uint8_t mac[6]);
  static String bytesToHex(const uint8_t* b, size_t n);
  static bool   hexToBytes(const String& hex, uint8_t* out, size_t n);

private:
  static PSMEspNowManager* s_inst;

  ConfigManager* _cfg = nullptr;
  ICMLogFS*      _log = nullptr;
  RTCManager*    _rtc = nullptr;

  // state
  bool     _started = false;
  uint8_t  _channel = 1;
  uint8_t  _masterMac[6] = {0};
  uint8_t  _token16[16] = {0};
  char     _pmk[17] = {0};

  // callbacks
  OnSetPowerFn        _onSetPower = nullptr;
  OnClearFaultFn      _onClearFault = nullptr;
  OnRequestShutdownFn _onReqShutdown = nullptr;
  OnComposeStatusFn   _onComposeStatus = nullptr;
  OnReadTempFn        _onReadTemp = nullptr;
};
