/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : ESPNowManager.h  — regrouped by domains (power / relay / sensors / topology)
 *  Note        : This is a **structural re‑ordering** of declarations only.
 *                All public APIs and private members from the original header
 *                are preserved; only grouped and annotated for clarity.
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
public:  // ============================ TYPES & CALLBACKS ============================
  // Module kinds managed by the ICM over ESP‑NOW
  enum class ModuleType : uint8_t { POWER=0, RELAY=1, PRESENCE=2 };

  // Peer record for a remote module (power/relay/sensor)
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

  // In‑RAM topology mirrors (what we push to slaves)
  struct RelayLink {
    bool     used      = false;
    uint8_t  nextMac[6]= {0};
    uint32_t nextIPv4  = 0;
    // track preceding sensor for context/acks
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

  // App‑level callbacks
  typedef void (*OnAckFn)(const uint8_t mac[6], uint16_t ctr, uint8_t code);
  typedef void (*OnPowerFn)(const uint8_t mac[6], const uint8_t* payload, size_t len);
  typedef void (*OnRelayFn)(const uint8_t mac[6], uint8_t relayIdx, const uint8_t* payload, size_t len);
  typedef void (*OnPresenceFn)(const uint8_t mac[6], uint8_t sensIdx, const uint8_t* payload, size_t len);
  typedef void (*OnUnknownFn)(const uint8_t mac[6], const IcmMsgHdr& h, const uint8_t* payload, size_t len);

public:  // ============================ LIFECYCLE & GLOBAL ============================
  ESPNowManager(ConfigManager* cfg, ICMLogFS* log, RTCManager* rtc);

  // Startup/shutdown/polling
  bool begin(uint8_t channelDefault=1, const char* pmk16=nullptr);
  void end();
  void poll();  // call frequently from loop()

  // Register callbacks
  void setOnAck      (OnAckFn      fn) { _onAck = fn; }
  void setOnPower    (OnPowerFn    fn) { _onPower = fn; }
  void setOnRelay    (OnRelayFn    fn) { _onRelay = fn; }
  void setOnPresence (OnPresenceFn fn) { _onPresence = fn; }
  void setOnUnknown  (OnUnknownFn  fn) { _onUnknown = fn; }

  // Channel & system mode
  bool setChannel(uint8_t ch, bool persist=true);
  bool setSystemModeAuto(bool persist);
  bool setSystemModeManual(bool persist);

  // Utilities
  static String macBytesToStr(const uint8_t mac[6]);
  static bool   macStrToBytes(const String& mac, uint8_t out[6]);
  String icmMacStr();
  bool isPeerOnline(ModuleType t, uint8_t index) const;
  RTCManager* rtc() const { return _rtc; }

public:  // ============================ POWER MODULE (PSM) ============================
  // --- Commands ---
  bool powerGetStatus();
  bool powerSetOutput(bool on);
  bool powerRequestShutdown();
  bool powerClearFault();
  bool powerCommand(const String& action);                 // "on","off","shutdown","clear","status"
  bool powerGetTemperature();                              // send PWR_GET_TEMP

  // --- Cached telemetry getters (updated on replies) ---
  bool     pwrIsOn()        const { return _pwrOn; }
  uint8_t  pwrFault()       const { return _pwrFault; }
  uint16_t pwrVbus_mV()     const { return _pwrVbus_mV; }
  uint16_t pwrIbus_mA()     const { return _pwrIbus_mA; }
  uint16_t pwrVbat_mV()     const { return _pwrVbat_mV; }
  uint16_t pwrIbat_mA()     const { return _pwrIbat_mA; }
  float    pwrTempC()       const { return _pwrTempC; }
  uint32_t pwrLastUpdateMs()const { return _pwrStatMs; }
  float    lastPowerTempC() const { return _pwrTempC; }
  uint32_t lastTempUpdateMsPower() const { return _pwrTempMs; }

  // Structured info helper
  bool getPowerModuleInfo(JsonVariant out);

public:  // ============================ RELAY MODULES ============================
  bool relayGetStatus(uint8_t idx);
  bool relaySet(uint8_t idx, uint8_t ch, bool on);
  bool relaySetMode(uint8_t idx, uint8_t ch, uint8_t mode);
  bool relayManualSet(const String& mac, uint8_t ch, bool on); // via MAC
  bool relayGetTemperature(uint8_t idx);                        // REL_GET_TEMP

  // Relay temperature caches
  float    lastRelayTempC(uint8_t idx) const { return (idx < ICM_MAX_RELAYS) ? _relTempC[idx] : NAN; }
  uint32_t lastTempUpdateMsRelay(uint8_t idx) const { return (idx < ICM_MAX_RELAYS) ? _relTempMs[idx] : 0; }

public:  // ============================ PRESENCE / SENSORS ============================
  bool presenceGetStatus(uint8_t idx);
  bool presenceSetMode(uint8_t idx, uint8_t mode);
  bool presenceGetDayNight(uint8_t idx);                     // SENS_GET_DAYNIGHT
  bool presenceGetDayNightByMac(const String& mac);
  int8_t lastDayFlagByMac(const String& mac, uint32_t* outMs=nullptr) const; // -1 unknown, 0 night, 1 day
  bool sensorSetMode(const String& mac, bool autoMode);       // via MAC
  bool sensorTestTrigger(const String& mac);                  // SENS_TRIG (test)

  // Day/Night caches per middle sensor index
  int8_t   lastSensorDayFlag(uint8_t idx) const { return (idx < ICM_MAX_SENSORS) ? _sensDayNight[idx] : -1; }
  uint32_t lastSensorDayFlagUpdateMs(uint8_t idx) const { return (idx < ICM_MAX_SENSORS) ? _sensDNMs[idx] : 0; }

public:  // ============================ PEERS / PAIRING ============================
  bool pairPower(const String& macStr);
  bool pairRelay(uint8_t idx, const String& macStr);
  bool pairPresence(uint8_t idx, const String& macStr);
  bool pairPresenceEntrance(const String& macStr);
  bool pairPresenceParking (const String& macStr);
  bool unpairByMac(const String& macStr);

  // Auto‑index helpers (first free slot, with NVS cursor)
  bool pairRelayAuto(const String& macStr, uint8_t* outIdx=nullptr);
  bool pairPresenceAuto(const String& macStr, uint8_t* outIdx=nullptr);

  // WiFiManager helpers (CRUD/housekeeping)
  bool pair(const String& mac, const String& type);          // "power", "relay"/"relayN", "sensor"/"sensorN", "entrance", "parking"
  bool removePeer(const String& mac) { return unpairByMac(mac); }
  void removeAllPeers();                                     // clear peers in esp‑now (RAM only)
  void clearAll();                                           // also clear NVS MAC+token+mode+channel+topology

public:  // ============================ TOPOLOGY ============================
  // Program links
  bool topoSetRelayNext(uint8_t relayIdx, const uint8_t nextMac[6], uint32_t nextIPv4);
  bool topoSetSensorDependency(uint8_t sensIdx, uint8_t targetRelayIdx, const uint8_t targetMac[6], uint32_t targetIPv4);
  bool topoSetRelayPrevFromSensor(uint8_t relayIdx, uint8_t sensIdx, const uint8_t sensMac[6]); // reverse link (derived)
  bool topoClearPeer(ModuleType t, uint8_t idx);
  bool configureTopology(const JsonVariantConst& links);     // array of link objects; live push enabled

  // Push topology token/IP bundles derived from in‑RAM mirrors
  bool topoPushRelay(uint8_t relayIdx);
  bool topoPushSensor(uint8_t sensIdx);
  bool topoPushAll();

  // Channel orchestration across the mesh
  bool orchestrateChannelChange(uint8_t newCh, uint8_t delay_s=2, uint8_t window_s=2, bool persist=true);

  // Query / serialize
  String serializePeers() const;                             // peers + mode/channel + online
  String serializeTopology() const;                          // current links (prev/next)
  String exportConfiguration() const;                        // peers + topology + ch/mode (PSRAM buffer if large)
  String getEntranceSensorMac() const;                       // "" if not set

public:  // ============================ SEQUENCE CONTROL ============================
  bool sequenceStart(SeqDir dir);
  bool sequenceStop();
  bool startSequence(const String& anchor, bool up);         // advisory anchor; broadcast anyway

public:  // ============================ STATIC UTILS ============================
  // (left intentionally empty; see above for mac helpers)

private: // ============================ NVS KEY HELPERS ============================
  String keyMd() const   { return ESPNOW_MD_KEY; }           // system mode
  String keyCh() const   { return ESPNOW_CH_KEY; }           // channel
  String keyTok(ModuleType t, uint8_t index);                // <=6
  String keyMac(ModuleType t, uint8_t index);                // <=6
  String keyTopo() const { return String("TOPOLJ"); }       // topology JSON blob
  String keyRNext()const { return String("RLNEXT"); }       // next relay index hint
  String keySNext()const { return String("SNNEXT"); }       // next sensor index hint
  String keyCtr() const  { return String("CNTTOK"); }       // token counter

private: // ============================ PEER/TOKEN INTERNALS ============================
  // Peer table helpers
  bool addOrUpdatePeer(ModuleType t, uint8_t index, const uint8_t mac[6]);
  PeerRec* findPeer(ModuleType t, uint8_t index);
  PeerRec* findPeerByMac(const uint8_t mac[6]);
  bool ensurePeer(ModuleType t, uint8_t index, PeerRec*& out);

  // Token helpers
  void tokenCompute(const String& icmMac, const String& nodeMac, uint32_t counter, String& tokenHex32);
  void tokenHexTo16(const String& hex, uint8_t out[16]);
  bool loadOrCreateToken(ModuleType t, uint8_t index, const String& macStr, String& tokenHexOut, uint8_t token16Out[16]);
  uint32_t takeAndBumpTokenCounter();

  // Convenience power helpers used by high‑level API (same semantics)
  bool requestPowerStatus(bool requireAck=true);
  bool powerOn(bool requireAck=true);
  bool powerOff(bool requireAck=true);
  uint8_t  powerFault() const      { return _pwrFault; }
  uint16_t powerBus_mV() const     { return _pwrVbus_mV; }
  uint16_t powerBus_mA() const     { return _pwrIbus_mA; }
  uint16_t powerBat_mV() const     { return _pwrVbat_mV; }
  uint16_t powerBat_mA() const     { return _pwrIbat_mA; }
  uint32_t powerStatAgeMs() const  { return millis() - _pwrStatMs; }

private: // ============================ FRAME / SEND QUEUE ============================
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

private: // ============================ ESP‑NOW CALLBACKS ============================
  static void onRecvThunk(const uint8_t *mac, const uint8_t *data, int len);
  static void onSentThunk(const uint8_t *mac, esp_now_send_status_t status);
  void onRecv(const uint8_t *mac, const uint8_t *data, int len);
  void onSent(const uint8_t *mac, esp_now_send_status_t status);
  void handleAck(const uint8_t mac[6], const IcmMsgHdr& h, const uint8_t* payload, int plen);
  bool tokenMatches(const PeerRec* pr, const IcmMsgHdr& h) const;

private: // ============================ SMALL HELPERS & TOPO PERSIST ============================
  uint16_t nextCtr() { return ++_ctr; }
  void reAddAllPeersOnChannel();
  uint8_t nextFreeRelayIndex() const;
  uint8_t nextFreeSensorIndex() const;
  void    rebuildReverseLinks();              // set relay.prev from sensor deps
  bool    saveTopologyToNvs() const;
  bool    loadTopologyFromNvs();

private: // ============================ STATE ============================
  // Singleton for static callbacks
  static ESPNowManager* s_inst;

  // Managers
  ConfigManager* _cfg = nullptr;
  ICMLogFS*      _log = nullptr;
  RTCManager*    _rtc = nullptr;

  // Peers
  PeerRec  _power;
  PeerRec  _relays[ICM_MAX_RELAYS];
  PeerRec  _sensors[ICM_MAX_SENSORS];
  PeerRec  _entrance; // special
  PeerRec  _parking;  // special

  // Topology mirrors
  RelayLink _relayTopo[ICM_MAX_RELAYS];
  SensorDep _sensorTopo[ICM_MAX_SENSORS];
  SensorDep _entrTopo;   // entrance
  SensorDep _parkTopo;   // parking

  // Send queue config/state
  PendingTx _pending[MAX_PENDING];
  uint8_t   _maxRetries     = 3;
  uint32_t  _ackTimeoutMs   = 300;  // wait for ACK after MAC success
  uint32_t  _retryBackoffMs = 150;  // next try

  // Config/runtime
  uint8_t   _channel = 1;
  uint8_t   _mode    = MODE_AUTO;
  bool      _started = false;
  uint16_t  _ctr     = 0;
  char      _pmk[17] = {0};

  // Temperature caches (updated on replies)
  float    _pwrTempC = NAN;
  uint32_t _pwrTempMs = 0;

  // Power telemetry cache
  bool     _pwrOn       = false;
  uint8_t  _pwrFault    = 0;
  uint16_t _pwrVbus_mV  = 0, _pwrIbus_mA = 0;
  uint16_t _pwrVbat_mV  = 0, _pwrIbat_mA = 0;
  uint32_t _pwrStatMs   = 0;

  // Day/Night caches (middle sensors)
  int8_t   _sensDayNight[ICM_MAX_SENSORS];   // -1 unknown, 0 night, 1 day
  uint32_t _sensDNMs[ICM_MAX_SENSORS];       // last update timestamp (ms)

  // Relay temps
  float    _relTempC[ICM_MAX_RELAYS] = { NAN };
  uint32_t _relTempMs[ICM_MAX_RELAYS] = { 0 };

  // Specials: entrance/parking day‑night caches
  int8_t   _entrDNFlag = -1; uint32_t _entrDNMs = 0;
  int8_t   _parkDNFlag = -1; uint32_t _parkDNMs = 0;

  // User callbacks
  OnAckFn      _onAck = nullptr;
  OnPowerFn    _onPower = nullptr;
  OnRelayFn    _onRelay = nullptr;
  OnPresenceFn _onPresence = nullptr;
  OnUnknownFn  _onUnknown = nullptr;
};
