/**************************************************************
 * Project : Driveway Lighting System
 * File    : ICM_Nw.h — ESP-NOW network core (TOKEN=HEX STRING)
 * Purpose : Role-agnostic core + per-role helpers (ICM, PMS, SENSOR, RELAY)
 **************************************************************/
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_mac.h>

#include "Now_API.h"
#include "Config/NVSConfig.h"
#include "ConfigManager.h"
#include "Peripheral/ICMLogFS.h"
#include "Peripheral/RTCManager.h"

namespace NwCore {

using RecvCb = void (*)(const uint8_t* mac, const uint8_t* data, int len);

// ============================== ICM ONLY ==============================
#ifdef NVS_ROLE_ICM
struct NodeSlot {
  uint8_t  mac[6];
  char     token_hex[NOW_TOKEN_HEX_LEN];
  uint8_t  kind;
  uint8_t  index;        // 1..16 (S/R), 1 for PMS
  uint16_t state_flags;  // NOW_STATE_*
  int8_t   lastRSSI;
  uint32_t lastSeenMs;
  bool     present;
};

struct SensorLive {
  volatile int32_t  t_c_x100;
  volatile uint32_t rh_x100;
  volatile int32_t  p_Pa;
  volatile uint32_t lux_x10;
  volatile uint8_t  is_day;
  volatile uint16_t tfA_mm;
  volatile uint16_t tfB_mm;
  volatile uint32_t updated_ms;
};

struct RelayLive {
  volatile int16_t  temp_c_x100;
  volatile uint16_t state_flags;
  volatile uint32_t updated_ms;
};
#endif // NVS_ROLE_ICM

// ============================== PMS ONLY ==============================
#ifdef NVS_ROLE_PMS
struct PmsState {
  volatile uint16_t vbus_mV, vbat_mV, ibus_mA, ibat_mA;
  volatile int16_t  temp_c_x100;
  volatile uint8_t  flags, on, src, _rsv;
};
#endif

// ============================= SENSOR ONLY ============================
#ifdef NVS_ROLE_SENS
// Public parameters that ICM sets (other classes can read/update/live-apply)
#ifndef SENS_TOPO_JSON_MAX
  #define SENS_TOPO_JSON_MAX 768
#endif
struct SensParams {
  // Topology JSON pushed by ICM (raw, stringified)
  volatile size_t topo_len;
  char            topo_json[SENS_TOPO_JSON_MAX];

  // Mode and triggers pushed via SYS/SEN
  volatile uint8_t mode;        // 0=Auto,1=Manual (from SYS_MODE_SET)
  volatile uint8_t forceReport; // set on SEN_TRIG; main loop clears when handled
};

// Public live telemetry prepared by your sensor managers (ToF/BME/VEML)
struct SensLiveNode {
  volatile int32_t  t_c_x100;
  volatile uint32_t rh_x100;
  volatile int32_t  p_Pa;
  volatile uint32_t lux_x10;
  volatile uint8_t  is_day;
  volatile uint16_t tfA_mm, tfB_mm;
};

// SENSOR node-side API
class Core; // fwd
#endif // NVS_ROLE_SENS

// ============================== RELAY ONLY ============================
#ifdef NVS_ROLE_RELAY
#ifndef RELAY_TOPO_JSON_MAX
  #define RELAY_TOPO_JSON_MAX 512
#endif
struct RelayParams {
  // Topology JSON pushed by ICM (raw, stringified)
  volatile size_t topo_len;
  char            topo_json[RELAY_TOPO_JSON_MAX];

  // Control parameters set by ICM
  volatile uint8_t mode;      // 0=Auto,1=Manual (from SYS_MODE_SET)
  volatile uint8_t chL_on;    // last requested LEFT state (REL_SET / REL_ON_FOR outcome)
  volatile uint8_t chR_on;    // last requested RIGHT state

  // Schedule (from REL_SCHED)
  volatile uint32_t t0_ms;
  volatile uint16_t l_on_at, l_off_at, r_on_at, r_off_at;
};

// Relay live state exposed by the module (e.g., internal thermistor)
struct RelayLiveNode {
  volatile int16_t temp_c_x100;
};
#endif // NVS_ROLE_RELAY

// =============================== CORE =================================
class Core {
public:
  explicit Core(ConfigManager* cfg=nullptr, ICMLogFS* log=nullptr, RTCManager* rtc=nullptr);
  Core& attachCfg (ConfigManager* cfg);
  Core& attachLog (ICMLogFS* log);
  Core& attachRtc (RTCManager* rtc);
  Core& attach    (ConfigManager* cfg, ICMLogFS* log, RTCManager* rtc);

  bool begin(uint8_t channel, const uint8_t* pmk16_or_null=nullptr);
  void end();

  void setRecvCallback(RecvCb cb);

  bool addPeer (const uint8_t mac[6], uint8_t channel, bool encrypt=false, const uint8_t* lmk16_or_null=nullptr);
  void delPeer (const uint8_t mac[6]);
  void clearPeers();

  esp_err_t send(const uint8_t mac[6], const void* payload, size_t len,
                 uint32_t waitAckMs=0, esp_now_send_status_t* outStatus=nullptr);
  bool lastSendStatus(const uint8_t mac[6], esp_now_send_status_t& status, uint32_t& ts_ms) const;

  static bool   macStrToBytes(const char* in, uint8_t out[6]);
  static String macBytesToStr(const uint8_t mac[6]);
  static String efuseMac12();
  static void   efuseMacBytes(uint8_t out[6]);
  static bool   macEq(const uint8_t a[6], const uint8_t b[6]);
  static int    macCmp(const uint8_t a[6], const uint8_t b[6]);
  static bool   macEqStr(const uint8_t mac[6], const char* mac12or17);
  static bool   macIsZero(const uint8_t mac[6]);

  static uint16_t generateToken16(const uint8_t icmMac[6], const uint8_t nodeMac[6], uint32_t counter); // deprecated
  bool verifyIncomingForLocal(const NowMsgHdr& h) const;
  bool expectedTokenForMac(const uint8_t peerMac[6], char token_hex_out[NOW_TOKEN_HEX_LEN]) const;
  inline bool tokenMatchesMac(const uint8_t peerMac[6], const char token_hex[NOW_TOKEN_HEX_LEN]) const {
    char t[NOW_TOKEN_HEX_LEN]; if (!expectedTokenForMac(peerMac, t)) return false;
    for (int i=0;i<NOW_TOKEN_HEX_LEN;i++) if (t[i]!=token_hex[i]) return false; return true;
  }

  bool nodeApplyPairing(const uint8_t icmMac[6], uint8_t channel, const char token_hex[NOW_TOKEN_HEX_LEN]);
  bool nodeClearPairing();

// --------------------------- ICM ROLE ONLY ----------------------------
#ifdef NVS_ROLE_ICM
  bool macInRegistry(const uint8_t mac[6]) const;
  bool findSlaveToken(const uint8_t mac[6], char token_hex_out[NOW_TOKEN_HEX_LEN]) const;
  bool tokenValidForSlave(const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN]) const;

  bool icmRegistrySetSensor(uint8_t idx1to16, const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN]);
  bool icmRegistrySetRelay (uint8_t idx1to16, const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN]);
  bool icmRegistrySetPower (const uint8_t mac[6],  const char token_hex[NOW_TOKEN_HEX_LEN]);
  bool icmRegistryClearSensor(uint8_t idx1to16);
  bool icmRegistryClearRelay (uint8_t idx1to16);
  bool icmRegistryClearPower ();

  bool icmRegistryIndexOfSensorMac(const uint8_t mac[6], uint8_t& idxOut);
  bool icmRegistryIndexOfRelayMac (const uint8_t mac[6], uint8_t& idxOut);
  bool icmRegistryFindFreeSensor  (uint8_t& idxOut);
  bool icmRegistryFindFreeRelay   (uint8_t& idxOut);
  bool icmRegistryAutoAddSensor   (const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN], uint8_t& idxOut);
  bool icmRegistryAutoAddRelay    (const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN], uint8_t& idxOut);
  bool icmRegistryAutoSetPower    (const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN]);

  bool icmRegistryCountSensors(uint8_t& countOut);
  bool icmRegistryCountRelays (uint8_t& countOut);

  void      slotsReset();
  bool      slotsLoadFromRegistry();
  NodeSlot* slotFindByMac(const uint8_t mac[6]);
  void      slotMarkSeen(const uint8_t mac[6], uint16_t state_flags, int8_t rssi, uint32_t ts_ms);

  esp_err_t sendFrame(const uint8_t dstMac[6], uint8_t dom, uint8_t op,
                      const void* payload, size_t plen, uint32_t waitAckMs=30,
                      esp_now_send_status_t* outStatus=nullptr);

  // PR
  esp_err_t prDevInfoQuery(const uint8_t mac[6], uint32_t waitAckMs=30);
  esp_err_t prAssign      (const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN], uint8_t channel, uint32_t waitAckMs=30);
  esp_err_t prCommit      (const uint8_t mac[6], uint32_t waitAckMs=30);
  esp_err_t prRekey       (const uint8_t mac[6], const char new_token_hex[NOW_TOKEN_HEX_LEN], uint32_t waitAckMs=30);
  esp_err_t prUnpair      (const uint8_t mac[6], uint32_t waitAckMs=30);
  static bool auto_pair_from_devinfo(Core* self, const uint8_t mac[6], const DevInfoPayload* info);

  // SYS / IND
  esp_err_t sysPing     (const uint8_t mac[6], uint32_t waitAckMs=30);
  esp_err_t sysModeSet  (const uint8_t mac[6], uint8_t mode, uint32_t waitAckMs=30);
  esp_err_t sysSetChannel(const uint8_t mac[6], uint8_t newCh, uint32_t waitAckMs=30);
  esp_err_t sysTimeSync (const uint8_t mac[6], uint64_t epoch_ms, int32_t offset_ms, int16_t drift_ppm,
                         uint8_t ver=1, uint32_t waitAckMs=0);

  esp_err_t indBuzzerSet(const uint8_t mac[6], uint8_t en, uint16_t hz, uint8_t vol, uint16_t dur_ms, uint32_t waitAckMs=30);
  esp_err_t indBuzzerPat(const uint8_t mac[6], uint8_t pat, uint8_t reps, uint16_t dur_ms, uint32_t waitAckMs=30);
  esp_err_t indLedSet   (const uint8_t mac[6], uint8_t r, uint8_t g, uint8_t b, uint8_t br, uint16_t dur_ms, uint32_t waitAckMs=30);
  esp_err_t indLedPat   (const uint8_t mac[6], uint8_t pat, uint16_t dur_ms, uint32_t waitAckMs=30);
  esp_err_t indAlert    (const uint8_t mac[6], uint8_t lvl, uint16_t dur_ms, uint32_t waitAckMs=30);
  esp_err_t indBuzzerSet(uint8_t on, uint8_t pat, uint16_t dur_ms);
  // PMS
  esp_err_t pwrOnOff    (const uint8_t mac[6], uint8_t on,  uint32_t waitAckMs=30);
  esp_err_t pwrSrcSet   (const uint8_t mac[6], uint8_t src, uint32_t waitAckMs=30);
  esp_err_t pwrClrFlags (const uint8_t mac[6], uint32_t waitAckMs=30);
  esp_err_t pwrQuery    (const uint8_t mac[6], uint32_t waitAckMs=30);

  // RELAY (control + query)
  esp_err_t relSet      (const uint8_t mac[6], uint8_t ch, uint8_t on, uint32_t waitAckMs=30);
  esp_err_t relOnFor    (const uint8_t mac[6], uint8_t chMask, uint16_t on_ms, uint32_t waitAckMs=30);
  esp_err_t relSchedule (const uint8_t mac[6], uint32_t t0_ms,
                         uint16_t l_on_at, uint16_t l_off_at,
                         uint16_t r_on_at, uint16_t r_off_at,
                         uint32_t waitAckMs=0);
  esp_err_t relQuery    (const uint8_t mac[6], uint32_t waitAckMs=30);

  // SENSOR (query/trigger)
  esp_err_t senRequest  (const uint8_t mac[6], uint32_t waitAckMs=30);

  // TOPO (JSON)
  esp_err_t topSetSensorJSON(const uint8_t sensorMac[6], const char* json, size_t len, uint32_t waitAckMs=50);
  esp_err_t topSetRelayJSON (const uint8_t relayMac [6], const char* json, size_t len, uint32_t waitAckMs=50);

  // ICM RX dispatcher
  static void icmRecvCallback(const uint8_t* mac, const uint8_t* data, int len);

  // ICM slots + mirrors
  NodeSlot   sensors[NOW_MAX_SENSORS];
  NodeSlot   relays [NOW_MAX_RELAYS];
  NodeSlot   pms    [NOW_MAX_POWER];
  SensorLive sensLive[NOW_MAX_SENSORS];
  RelayLive  relayLive[NOW_MAX_RELAYS];
#endif // NVS_ROLE_ICM
// --------------------------- PMS ROLE ONLY ----------------------------
#ifdef NVS_ROLE_PMS
  // Live PMS telemetry (drivers/tasks write here; ESPNOW code snapshots it)
  PmsState PMS;

  // Tiny sugar setters (optional to call from ISRs/tasks)
  inline void pmsSetVBUSmV(uint16_t v)     { PMS.vbus_mV = v; }
  inline void pmsSetVBATmV(uint16_t v)     { PMS.vbat_mV = v; }
  inline void pmsSetIBUSmA(uint16_t v)     { PMS.ibus_mA = v; }
  inline void pmsSetIBATmA(uint16_t v)     { PMS.ibat_mA = v; }
  inline void pmsSetTempC_x100(int16_t v)  { PMS.temp_c_x100 = v; }
  inline void pmsSetFlags(uint8_t f)       { PMS.flags = f; }
  inline void pmsOrFlags(uint8_t f)        { PMS.flags = (uint8_t)(PMS.flags | f); }
  inline void pmsClearFlags(uint8_t f)     { PMS.flags = (uint8_t)(PMS.flags & ~f); }
  inline void pmsSetOn(uint8_t v)          { PMS.on = v; }
  inline void pmsSetSrc(uint8_t v)         { PMS.src = v; }

  // PMS node-side: send a PWR_REP using current PMS fields to paired ICM
  esp_err_t pmsSendReport(uint32_t waitAckMs=0);

  // Apply ICM controls locally on PMS (called from pmsRecvCallback)
  void pmsApplyOnOff(uint8_t on);
  void pmsApplySource(uint8_t src);
  void pmsApplyClearFlags(uint8_t mask);

  // Convenience: read NVS pairing flags
  bool pmsIsPaired() const;       // reads PMS_PAIRED_KEY
  bool pmsInPairingMode() const;  // reads PMS_PAIRING_KEY

  // PMS-side dispatcher (register via setRecvCallback in begin() for PMS builds)
  static void pmsRecvCallback(const uint8_t* mac, const uint8_t* data, int len);
  esp_err_t pmsSysPing(uint32_t waitAckMs=30);
#endif // NVS_ROLE_PMS
// --------------------------- SENSOR ROLE ONLY -------------------------
#ifdef NVS_ROLE_SENS
  // Public state/params (ICM writes them; other classes consume them)
  SensParams   SEN_CFG;   // params set by ICM (JSON topo, mode, trigger)
  SensLiveNode SEN_LIVE;  // live measurements written by sensor managers

  // SENSOR node-side helpers
  // Send a SEN_REP using current SEN_LIVE values
  esp_err_t sensSendReport(uint32_t waitAckMs=0);

  // Apply controls set by ICM (called from sensRecvCallback)
  void sensApplyMode(uint8_t mode);  // 0=Auto,1=Manual
  void sensHandleTrigger();          // set/clear forceReport as needed

  // Topology receiver (JSON string pushed by ICM)
  void sensStoreTopologyJSON(const char* json, size_t len); // copies into SEN_CFG.topo_json

  // Pairing flags (SENSOR role)
  bool sensIsPaired() const;
  bool sensInPairingMode() const;

  // SENSOR-side dispatcher (register via setRecvCallback in begin())
  static void sensRecvCallback(const uint8_t* mac, const uint8_t* data, int len);

  // Convenience: ping the ICM from Sensor node
  esp_err_t sensSysPing(uint32_t waitAckMs=30);
#endif // NVS_ROLE_SENS
// --------------------------- RELAY ROLE ONLY --------------------------
#ifdef NVS_ROLE_RELAY
  // Public state/params (ICM writes them; other classes consume/apply)
  RelayParams   REL_CFG;   // params set by ICM (JSON topo, mode, controls, schedule)
  RelayLiveNode REL_LIVE;  // live temperature (filled by your relay driver)

  // RELAY node-side helpers
  // Send a REL_REP using current REL_LIVE values
  esp_err_t relaySendReport(uint32_t waitAckMs=0);

  // Apply ICM controls locally (called from relayRecvCallback)
  void relayApplySet(uint8_t ch, uint8_t on);                      // immediately set LEFT/RIGHT
  void relayApplyOnFor(uint8_t chMask, uint16_t on_ms);            // pulse for duration
  void relayApplySchedule(uint32_t t0_ms, uint16_t l_on_at, uint16_t l_off_at,
                          uint16_t r_on_at, uint16_t r_off_at);    // update schedule params
  void relayApplyMode(uint8_t mode);                               // 0=Auto,1=Manual

  // Topology receiver (JSON string pushed by ICM)
  void relayStoreTopologyJSON(const char* json, size_t len);       // copies into REL_CFG.topo_json

  // Pairing flags (RELAY role)
  bool relayIsPaired() const;
  bool relayInPairingMode() const;

  // RELAY-side dispatcher (register via setRecvCallback in begin())
  static void relayRecvCallback(const uint8_t* mac, const uint8_t* data, int len);

  // Convenience: ping the ICM from Relay node
  esp_err_t relaySysPing(uint32_t waitAckMs=30);
#endif // NVS_ROLE_RELAY

  // ------------------------ header utilities -------------------------
  static uint16_t nextSeq();
  static inline uint32_t nowMs() { return millis(); }
  static inline void fillHeader(NowMsgHdr& h, uint8_t dom, uint8_t op, const char token_hex[NOW_TOKEN_HEX_LEN]) {
    h.ver = NOW_HDR_VER; h.dom = dom; h.op = op; h.ts_ms = nowMs(); h.seq = nextSeq();
    for (int i=0;i<NOW_TOKEN_HEX_LEN;i++) h.token_hex[i] = token_hex[i];
  }

  inline ConfigManager* cfg() const { return _cfg; }
  inline ICMLogFS*      log() const { return _log; }
  inline RTCManager*    rtc() const { return _rtc; }
  inline uint8_t        channel() const { return _channel; }

private:
  struct SendStat {
    uint8_t mac[6];
    volatile esp_now_send_status_t status;
    volatile uint32_t ts_ms;
    volatile bool seen;
  };
  static const int kMaxStats = 24;
  SendStat _stats[kMaxStats];

  SendStat*       findOrAllocStat(const uint8_t mac[6]);
  const SendStat* findStat       (const uint8_t mac[6]) const;
  void            clearStats     ();

  static void onSendThunk(const uint8_t* mac, esp_now_send_status_t status);
  static void onRecvThunk(const uint8_t* mac, const uint8_t* data, int len);
  void onSend(const uint8_t* mac, esp_now_send_status_t status);
  void onRecv(const uint8_t* mac, const uint8_t* data, int len);

#ifdef NVS_ROLE_ICM
  bool matchMacAgainstRegistry_(const uint8_t mac[6]) const;
  bool readRegistryTokenForMac_(const uint8_t mac[6], char token_hex_out[NOW_TOKEN_HEX_LEN]) const;
#endif

  ConfigManager* _cfg   = nullptr;
  ICMLogFS*      _log   = nullptr;
  RTCManager*    _rtc   = nullptr;
  RecvCb         _userRecv = nullptr;
  uint8_t        _channel  = 1;

  static Core*   s_self;
};

} // namespace NwCore
