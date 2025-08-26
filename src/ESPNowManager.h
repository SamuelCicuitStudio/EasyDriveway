/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : ESPNowManager.h   (token- & topology-aware rewrite)
 **************************************************************/
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

#include "ConfigManager.h"
#include "ICMLogFS.h"
#include "RTCManager.h"
#include "CommandAPI.h"  // opcodes / payloads / limits

class ESPNowManager {
public:
  // ----- Types -----
  enum class ModuleType : uint8_t { POWER=0, RELAY=1, PRESENCE=2 };

  struct PeerRec {
    bool       used = false;
    ModuleType type = ModuleType::RELAY;
    uint8_t    index = 0;
    uint8_t    mac[6] = {0};
    uint8_t    token16[16] = {0};   // token this peer expects for frames addressed to it
    bool       online = false;
    uint8_t    consecFails = 0;
    int        activeTx = -1;
    uint32_t   lastSeen = 0;
  };

  // In-RAM topology mirrors (what we push to slaves)
  struct RelayLink {
    bool     used      = false;
    uint8_t  nextMac[6]= {0};
    uint32_t nextIPv4  = 0;
    // NEW: track preceding sensor for context/acks
    bool     hasPrev   = false;
    uint8_t  prevSensIdx = 0xFF;
    uint8_t  prevSensMac[6] = {0};
  };
  struct SensorDep {
    bool     used = false;
    uint8_t  targetType = 1;  // relay
    uint8_t  targetIdx = 0;
    uint8_t  targetMac[6] = {0};
    uint32_t targetIPv4 = 0;
  };

  // ACK callback types
  typedef void (*OnAckFn)(const uint8_t mac[6], uint16_t ctr, uint8_t code);
  typedef void (*OnPowerFn)(const uint8_t mac[6], const uint8_t* payload, size_t len);
  typedef void (*OnRelayFn)(const uint8_t mac[6], uint8_t relayIdx, const uint8_t* payload, size_t len);
  typedef void (*OnPresenceFn)(const uint8_t mac[6], uint8_t sensIdx, const uint8_t* payload, size_t len);
  typedef void (*OnUnknownFn)(const uint8_t mac[6], const IcmMsgHdr& h, const uint8_t* payload, size_t len);

public:
  ESPNowManager(ConfigManager* cfg, ICMLogFS* log, RTCManager* rtc);

  // ----- Lifecycle -----
  bool begin(uint8_t channelDefault=1, const char* pmk16=nullptr);
  void end();
  void poll();  // call frequently from loop()

  // ----- Callbacks -----
  void setOnAck      (OnAckFn      fn) { _onAck = fn; }
  void setOnPower    (OnPowerFn    fn) { _onPower = fn; }
  void setOnRelay    (OnRelayFn    fn) { _onRelay = fn; }
  void setOnPresence (OnPresenceFn fn) { _onPresence = fn; }
  void setOnUnknown  (OnUnknownFn  fn) { _onUnknown = fn; }

  // ----- Peers / Pairing API -----
  bool pairPower(const String& macStr);
  bool pairRelay(uint8_t idx, const String& macStr);
  bool pairPresence(uint8_t idx, const String& macStr);
  bool pairPresenceEntrance(const String& macStr);
  bool pairPresenceParking (const String& macStr);
  bool unpairByMac(const String& macStr);

  // Auto-index helpers (first free slot, with NVS cursor)
  bool pairRelayAuto(const String& macStr, uint8_t* outIdx=nullptr);
  bool pairPresenceAuto(const String& macStr, uint8_t* outIdx=nullptr);

  // --------- Helpers asked by WiFiManager ----------
  // (1) CRUD / housekeeping
  bool pair(const String& mac, const String& type);        // "power", "relay" or "relayN", "sensor" or "sensorN", "entrance", "parking"
  bool removePeer(const String& mac) { return unpairByMac(mac); }
  void removeAllPeers();                                   // clear peers in esp-now (RAM only)
  void clearAll();                                         // also clear NVS MAC+token+mode+channel+topology

  // (2) Query/serialize
  String serializePeers() const;                           // JSON of peers + mode/channel + online
  String serializeTopology() const;                        // JSON of current in-RAM topology links (with prev/next)
  String exportConfiguration() const;                      // peers + topology + ch/mode (PSRAM buffer if large)
  String getEntranceSensorMac() const;                     // "" if not set

  // (3) Channel / mode
  bool setChannel(uint8_t ch, bool persist=true);
  bool setSystemModeAuto(bool persist);
  bool setSystemModeManual(bool persist);

  // (4) Manual controls
  bool powerGetStatus();
  bool powerSetOutput(bool on);
  bool powerRequestShutdown();
  bool powerClearFault();
  bool powerCommand(const String& action);                 // "on","off","shutdown","clear","status"
  bool relayGetStatus(uint8_t idx);
  bool relaySet(uint8_t idx, uint8_t ch, bool on);
  bool relaySetMode(uint8_t idx, uint8_t ch, uint8_t mode);
  bool relayManualSet(const String& mac, uint8_t ch, bool on); // via MAC
  bool presenceGetStatus(uint8_t idx);
  bool presenceSetMode(uint8_t idx, uint8_t mode);
  bool presenceGetDayNight(uint8_t idx);  // send SENS_GET_DAYNIGHT
  bool presenceGetDayNightByMac(const String& mac);
  int8_t lastDayFlagByMac(const String& mac, uint32_t* outMs=nullptr) const;
  bool sensorSetMode(const String& mac, bool autoMode);    // via MAC
  bool sensorTestTrigger(const String& mac);               // SENS_TRIG (test)

  // (5) Topology
  bool topoSetRelayNext(uint8_t relayIdx, const uint8_t nextMac[6], uint32_t nextIPv4);
  bool topoSetSensorDependency(uint8_t sensIdx, uint8_t targetRelayIdx, const uint8_t targetMac[6], uint32_t targetIPv4);
  bool topoSetRelayPrevFromSensor(uint8_t relayIdx, uint8_t sensIdx, const uint8_t sensMac[6]); // NEW: track reverse
  bool topoClearPeer(ModuleType t, uint8_t idx);
  bool configureTopology(const JsonVariantConst& links);   // array of objects; live push enabled

  // PUSH token/IP bundles derived from the in-RAM topology:
  bool topoPushRelay(uint8_t relayIdx);
  bool topoPushSensor(uint8_t sensIdx);
  bool topoPushAll();

  // (6) Sequence
  bool sequenceStart(SeqDir dir);
  bool sequenceStop();
  bool startSequence(const String& anchor, bool up);       // anchor is advisory; we broadcast anyway

  // (7) Info
  bool getPowerModuleInfo(JsonVariant out);

  // ----- Utils -----
  static String macBytesToStr(const uint8_t mac[6]);
  static bool   macStrToBytes(const String& mac, uint8_t out[6]);
  String icmMacStr();
  bool isPeerOnline(ModuleType t, uint8_t index) const;
  RTCManager* rtc() const { return _rtc; };
  // ----- (4) Manual controls / queries (add these) -----
  bool powerGetTemperature();                 // send PWR_GET_TEMP
  bool relayGetTemperature(uint8_t idx);      // send REL_GET_TEMP to relay[idx]

  // ----- (7) Info (add getters) -----
  float lastPowerTempC() const { return _pwrTempC; }
  float lastRelayTempC(uint8_t idx) const {
    return (idx < ICM_MAX_RELAYS) ? _relTempC[idx] : NAN;
  }
  uint32_t lastTempUpdateMsPower() const { return _pwrTempMs; }
  uint32_t lastTempUpdateMsRelay(uint8_t idx) const {
    return (idx < ICM_MAX_RELAYS) ? _relTempMs[idx] : 0;
  }


  // Day/Night caches
  // Returns: -1 = unknown, 0 = night, 1 = day
  int8_t  lastSensorDayFlag(uint8_t idx) const { return (idx < ICM_MAX_SENSORS) ? _sensDayNight[idx] : -1; }
  uint32_t lastSensorDayFlagUpdateMs(uint8_t idx) const { return (idx < ICM_MAX_SENSORS) ? _sensDNMs[idx] : 0; }
private:
  // ---------- NVS keys (<= 6 chars each) ----------
  String keyMd() const   { return String("ESMODE"); }  // system mode
  String keyCh() const   { return String("ESCHNL"); }  // channel
  String keyTok(ModuleType t, uint8_t index);          // <=6
  String keyMac(ModuleType t, uint8_t index);          // <=6
  String keyTopo() const { return String("TOPOLJ"); }  // topology JSON blob
  String keyRNext()const { return String("RLNEXT"); }  // next relay index hint
  String keySNext()const { return String("SNNEXT"); }  // next sensor index hint
  String keyCtr() const  { return String("CNTTOK"); }  // token counter

  // ---------- Peers ----------
  bool addOrUpdatePeer(ModuleType t, uint8_t index, const uint8_t mac[6]);
  PeerRec* findPeer(ModuleType t, uint8_t index);
  PeerRec* findPeerByMac(const uint8_t mac[6]);
  bool ensurePeer(ModuleType t, uint8_t index, PeerRec*& out);

  // ---------- Tokens ----------
  void tokenCompute(const String& icmMac, const String& nodeMac, uint32_t counter, String& tokenHex32);
  void tokenHexTo16(const String& hex, uint8_t out[16]);
  bool loadOrCreateToken(ModuleType t, uint8_t index, const String& macStr, String& tokenHexOut, uint8_t token16Out[16]);
  uint32_t takeAndBumpTokenCounter();

  // ---------- Frame / Send ----------
  struct PendingTx {
    bool     used = false;
    uint8_t  mac[6] = {0};
    uint8_t  frame[250];
    uint16_t len = 0;
    uint8_t  dom = 0;
    uint8_t  op = 0;
    uint16_t ctr = 0;
    bool     requireAck = true;
    uint8_t  retriesLeft = 0;
    uint32_t deadlineMs = 0;
    uint32_t backoffMs = 0;
  };
  static constexpr int MAX_PENDING = 8;

  void   fillHeader(IcmMsgHdr& h, CmdDomain dom, uint8_t op, uint8_t flags, const uint8_t token16[16]);
  bool   enqueueToPeer(PeerRec* pr, CmdDomain dom, uint8_t op, const uint8_t* body, size_t blen, bool requireAck);
  void   startSend(int pidx);
  void   scheduleRetry(PendingTx& tx);
  void   markPeerFail(PeerRec* pr);
  void   markPeerOk(PeerRec* pr);
  int    allocPending();
  void   freePending(int idx);
  int    findPendingForPeer(const uint8_t mac[6]);

  // ---------- Callbacks from ESP-NOW ----------
  static void onRecvThunk(const uint8_t *mac, const uint8_t *data, int len);
  static void onSentThunk(const uint8_t *mac, esp_now_send_status_t status);
  void onRecv(const uint8_t *mac, const uint8_t *data, int len);
  void onSent(const uint8_t *mac, esp_now_send_status_t status);
  void handleAck(const uint8_t mac[6], const IcmMsgHdr& h, const uint8_t* payload, int plen);
  bool tokenMatches(const PeerRec* pr, const IcmMsgHdr& h) const;

  // ---------- Small helpers ----------
  uint16_t nextCtr() { return ++_ctr; }
  void reAddAllPeersOnChannel();
  uint8_t nextFreeRelayIndex() const;
  uint8_t nextFreeSensorIndex() const;
  void    rebuildReverseLinks();  // set relay.prev from sensor deps
  bool    saveTopologyToNvs() const;
  bool    loadTopologyFromNvs();

private:
  // Singleton for static callbacks
  static ESPNowManager* s_inst;

  ConfigManager* _cfg = nullptr;
  ICMLogFS*      _log = nullptr;
  RTCManager*    _rtc = nullptr;

  // peers
  PeerRec  _power;
  PeerRec  _relays[ICM_MAX_RELAYS];
  PeerRec  _sensors[ICM_MAX_SENSORS];
  PeerRec  _entrance; // special
  PeerRec  _parking;  // special

  // topology mirrors
  RelayLink _relayTopo[ICM_MAX_RELAYS];
  SensorDep _sensorTopo[ICM_MAX_SENSORS];
  SensorDep _entrTopo;   // entrance
  SensorDep _parkTopo;   // parking

  // send queue
  PendingTx _pending[MAX_PENDING];
  uint8_t   _maxRetries     = 3;
  uint32_t  _ackTimeoutMs   = 300;  // wait for ACK after MAC success
  uint32_t  _retryBackoffMs = 150;  // next try

  // config/runtime
  uint8_t   _channel = 1;
  uint8_t   _mode    = MODE_AUTO;
  bool      _started = false;
  uint16_t  _ctr     = 0;
  char      _pmk[17] = {0};

  // temperature caches (updated on replies)
  float    _pwrTempC = NAN;
  uint32_t _pwrTempMs = 0;

  // Day/Night caches (updated on replies)
  int8_t   _sensDayNight[ICM_MAX_SENSORS];   // -1 unknown, 0 night, 1 day
  uint32_t _sensDNMs[ICM_MAX_SENSORS];       // last update timestamp (ms)


  float    _relTempC[ICM_MAX_RELAYS] = { NAN };
  uint32_t _relTempMs[ICM_MAX_RELAYS] = { 0 };

  // entrance/parking day-night caches
  int8_t   _entrDNFlag = -1; uint32_t _entrDNMs = 0;
  int8_t   _parkDNFlag = -1; uint32_t _parkDNMs = 0;

  // user callbacks
  OnAckFn      _onAck = nullptr;
  OnPowerFn    _onPower = nullptr;
  OnRelayFn    _onRelay = nullptr;
  OnPresenceFn _onPresence = nullptr;
  OnUnknownFn  _onUnknown = nullptr;
};
