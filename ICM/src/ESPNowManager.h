/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : ESPNowManager.h — NEW-ONLY (Zero-Centered + Boundaries)
 *  Notes       : This header matches the rewritten ESPNowManager.cpp.
 *                Legacy topology (links/next-hop/dependency) is removed.
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
  // Module kinds managed by the ICM over ESP-NOW
  enum class ModuleType : uint8_t { POWER=0, RELAY=1, PRESENCE=2 };

  // For each sensor: its neighbors + two ordered lists of relays (neg/pos)
  struct ZcSensorMirror {
    bool     used = false;
    uint8_t  sensIdx = 0xFF;

    // neighbors
    bool     hasPrev = false, hasNext = false;
    uint8_t  prevSensIdx = 0xFF, nextSensIdx = 0xFF;
    uint8_t  prevSensMac[6] = {0}, nextSensMac[6] = {0};
    uint8_t  prevSensTok16[16] = {0}, nextSensTok16[16] = {0};

    // ordered lists (local coordinates)
    ZcRelEntry neg[ICM_MAX_RELAYS]; uint8_t nNeg = 0;
    ZcRelEntry pos[ICM_MAX_RELAYS]; uint8_t nPos = 0;
  };

  // For each relay: two boundary sensors allowed to command it
  struct RelayBoundaryMirror {
    bool     used = false;
    uint8_t  relayIdx = 0xFF;

    bool     hasA = false, hasB = false;
    uint8_t  aSensIdx = 0xFF, bSensIdx = 0xFF;
    uint8_t  aSensMac[6] = {0}, bSensMac[6] = {0};
    uint8_t  aSensTok16[16] = {0}, bSensTok16[16] = {0};

    uint8_t  splitRule = SPLIT_RULE_MAC_ASC_IS_POS_LEFT;
  };

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

public:  // ============================ LIFECYCLE & GLOBAL ============================
  ESPNowManager(ConfigManager* cfg, ICMLogFS* log, RTCManager* rtc);

  // Startup/shutdown/polling
  bool begin(uint8_t channelDefault=1, const char* pmk16=nullptr);
  void end();
  void poll();  // call frequently from loop()

  // Register callbacks
  typedef void (*OnAckFn)(const uint8_t mac[6], uint16_t ctr, uint8_t code);
  typedef void (*OnPowerFn)(const uint8_t mac[6], const uint8_t* payload, size_t len);
  typedef void (*OnRelayFn)(const uint8_t mac[6], uint8_t relayIdx, const uint8_t* payload, size_t len);
  typedef void (*OnPresenceFn)(const uint8_t mac[6], uint8_t sensIdx, const uint8_t* payload, size_t len);
  typedef void (*OnUnknownFn)(const uint8_t mac[6], const IcmMsgHdr& h, const uint8_t* payload, size_t len);

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

  bool presenceGetTfRaw(uint8_t idx);  // sends SENS_GET_TFRAW
  bool presenceGetEnv(uint8_t idx);    // sends SENS_GET_ENV

  // Cached last raw values (per sensor board)
  float   lastTfDistA_mm(uint8_t idx) const { return (idx<ICM_MAX_SENSORS)? _tfDistA_mm[idx] : NAN; }
  float   lastTfDistB_mm(uint8_t idx) const { return (idx<ICM_MAX_SENSORS)? _tfDistB_mm[idx] : NAN; }
  uint16_t lastTfAmpA(uint8_t idx)    const { return (idx<ICM_MAX_SENSORS)? _tfAmpA[idx]    : 0; }
  uint16_t lastTfAmpB(uint8_t idx)    const { return (idx<ICM_MAX_SENSORS)? _tfAmpB[idx]    : 0; }
  uint32_t lastTfUpdateMs(uint8_t idx) const { return (idx<ICM_MAX_SENSORS)? _tfMs[idx]     : 0; }

  float   lastEnvTempC(uint8_t idx)   const { return (idx<ICM_MAX_SENSORS)? _envTempC[idx]  : NAN; }
  float   lastEnvRh(uint8_t idx)      const { return (idx<ICM_MAX_SENSORS)? _envRh[idx]     : NAN; }
  float   lastEnvPressPa(uint8_t idx) const { return (idx<ICM_MAX_SENSORS)? _envPa[idx]     : NAN; }
  float   lastEnvLux(uint8_t idx)     const { return (idx<ICM_MAX_SENSORS)? _envLux[idx]    : NAN; }
  uint32_t lastEnvUpdateMs(uint8_t idx) const { return (idx<ICM_MAX_SENSORS)? _envMs[idx]   : 0; }

public:  // ============================ PEERS / PAIRING ============================
  bool pairPower(const String& macStr);
  bool pairRelay(uint8_t idx, const String& macStr);
  bool pairPresence(uint8_t idx, const String& macStr);
  bool pairPresenceEntrance(const String& macStr);
  bool pairPresenceParking (const String& macStr);
  bool unpairByMac(const String& macStr);

  // Auto-index helpers (first free slot, with NVS cursor)
  bool pairRelayAuto(const String& macStr, uint8_t* outIdx=nullptr);
  bool pairPresenceAuto(const String& macStr, uint8_t* outIdx=nullptr);

  // WiFiManager helpers (CRUD/housekeeping)
  bool pair(const String& mac, const String& type);          // "power", "relay"/"relayN", "sensor"/"sensorN", "entrance", "parking"
  bool removePeer(const String& mac) { return unpairByMac(mac); }
  void removeAllPeers();                                     // clear peers in esp-now (RAM only)
  void clearAll();                                           // also clear NVS MAC+token+mode+channel+topology

public:  // ============================ TOPOLOGY (NEW-ONLY) ============================
  // Channel orchestration across the mesh
  bool orchestrateChannelChange(uint8_t newCh, uint8_t delay_s=2, uint8_t window_s=2, bool persist=true);

  // === Zero-centered model (sensor-centric) ===
  bool topoSetSensorNeighbors(uint8_t sensIdx,
                              bool hasPrev, uint8_t prevIdx, const uint8_t prevMac[6],
                              bool hasNext, uint8_t nextIdx, const uint8_t nextMac[6]);

  bool topoSetSensorRelaysZeroCentered(uint8_t sensIdx,
                                       const ZcRelEntry* negList, uint8_t nNeg,
                                       const ZcRelEntry* posList, uint8_t nPos);

  bool topoSetRelayBoundaries(uint8_t relayIdx,
                              bool hasA, uint8_t aIdx, const uint8_t aMac[6],
                              bool hasB, uint8_t bIdx, const uint8_t bMac[6],
                              uint8_t splitRule = SPLIT_RULE_MAC_ASC_IS_POS_LEFT);

  // Push to devices (new-only)
  bool topoPushZeroCenteredSensor(uint8_t sensIdx);
  bool topoPushBoundaryRelay(uint8_t relayIdx);
  bool topoPushAllZeroCentered();

  // Query / serialize
  String serializePeers() const;                             // peers + mode/channel + online
  String serializeTopology() const;                          // new-only zc/boundaries
  String exportConfiguration() const;                        // peers + topology + ch/mode (PSRAM buffer if large)
  String getEntranceSensorMac() const;                       // "" if not set

  // Accept new-only topology JSON and persist
  bool configureTopology(const JsonVariantConst& topo);

public:  // ============================ SEQUENCE CONTROL ============================
  bool sequenceStart(SeqDir dir);
  bool sequenceStop();
  bool startSequence(const String& anchor, bool up);         // advisory anchor; broadcast anyway

private: // ============================ NVS KEY HELPERS ============================
  String keyMd() const   { return ESPNOW_MD_KEY; }           // system mode
  String keyCh() const   { return ESPNOW_CH_KEY; }           // channel
  String keyTok(ModuleType t, uint8_t index);                // <=6
  String keyMac(ModuleType t, uint8_t index);                // <=6
  String keyTopo() const { return String("TOPOLJ"); }        // topology JSON blob
  String keyRNext()const { return String("RLNEXT"); }        // next relay index hint
  String keySNext()const { return String("SNNEXT"); }        // next sensor index hint
  String keyCtr() const  { return String("CNTTOK"); }        // token counter

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

  // Convenience power helpers used by high-level API (same semantics)
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

private: // ============================ ESP-NOW CALLBACKS ============================
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

  // New mirrors (sensor-centric model)
  ZcSensorMirror      _zcSensors[ICM_MAX_SENSORS+2];   // +2 for FE/FF if you keep specials
  RelayBoundaryMirror _boundaries[ICM_MAX_RELAYS];

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

  float   _tfDistA_mm[ICM_MAX_SENSORS] = {NAN};
  float   _tfDistB_mm[ICM_MAX_SENSORS] = {NAN};
  uint16_t _tfAmpA[ICM_MAX_SENSORS]    = {0};
  uint16_t _tfAmpB[ICM_MAX_SENSORS]    = {0};
  uint32_t _tfMs[ICM_MAX_SENSORS]      = {0};

  float   _envTempC[ICM_MAX_SENSORS]   = {NAN};
  float   _envRh[ICM_MAX_SENSORS]      = {NAN};
  float   _envPa[ICM_MAX_SENSORS]      = {NAN};
  float   _envLux[ICM_MAX_SENSORS]     = {NAN};
  uint32_t _envMs[ICM_MAX_SENSORS]     = {0};


  // Specials: entrance/parking day-night caches
  int8_t   _entrDNFlag = -1; uint32_t _entrDNMs = 0;
  int8_t   _parkDNFlag = -1; uint32_t _parkDNMs = 0;

  // User callbacks
  OnAckFn      _onAck = nullptr;
  OnPowerFn    _onPower = nullptr;
  OnRelayFn    _onRelay = nullptr;
  OnPresenceFn _onPresence = nullptr;
  OnUnknownFn  _onUnknown = nullptr;
};
