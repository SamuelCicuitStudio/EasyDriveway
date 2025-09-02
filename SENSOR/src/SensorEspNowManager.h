/**************************************************************
 *  Project     : Sensor Node (Presence Board) — ESP-NOW Manager
 *  File        : SensorEspNowManager.h
 *  Notes       : Sensor-side counterpart that talks to ICM using
 *                CommandAPI.h + zero-centered topology.
 *                - Handles SYS (ack, ping, set_channel with switchover)
 *                - Handles SENS_* queries (day/night, TF raw, ENV)
 *                - Accepts TOPO_PUSH_ZC_SENSOR, parses & mirrors relays
 *                - Persists topology MACs to NVS as strings
 *                - Can send raw frames directly to relays using their token
 **************************************************************/
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <vector>

#include "CommandAPI.h"      // IcmMsgHdr, opcodes, payloads
#include "ConfigManager.h"
#include "BME280Manager.h"
#include "VEML7700Manager.h"
#include "TFLunaManager.h"
#include "RTCManager.h"   // new


// ======================= NVS KEY NAMES (topology + per-relay) =======================
// Raw zero-centered sensor blob (binary) for cold-boot continuity
#define NVS_ZC_SENSOR_BLOB          "ZCBLB"

// Sensor identity in the zero-centered map
#define NVS_ZC_SENSOR_INDEX         "ZSIDX"

// Neighbor sensors (flags/indices + MAC strings + tokens)
#define NVS_ZC_HAS_PREV             "ZHPV"
#define NVS_ZC_PREV_INDEX           "ZPIDX"
#define NVS_ZC_PREV_MAC             "ZPMAC"
#define NVS_ZC_PREV_TOKEN16         "ZPTOK"   // hex string (32 chars)

#define NVS_ZC_HAS_NEXT             "ZHNX"
#define NVS_ZC_NEXT_INDEX           "ZNIDX"
#define NVS_ZC_NEXT_MAC             "ZNMAC"
#define NVS_ZC_NEXT_TOKEN16         "ZNTOK"   // hex string (32 chars)

// Counts of local zero-centered relays
#define NVS_ZC_NEG_COUNT            "ZNCT"
#define NVS_ZC_POS_COUNT            "ZPCT"

// Per-relay storage by local coordinate (ordered lists):
//  - Negative side entries 1..N map to relPos = -1, -2, ...
//  - Positive side entries 1..N map to relPos = +1, +2, ...
#define NVS_ZC_NEG_MAC_FMT          "ZNM%u"     // 1-based ordinal; value: "AA:BB:CC:DD:EE:FF"
#define NVS_ZC_NEG_TOKEN16_FMT      "ZNT%u"     // 1-based ordinal; value: 32-hex string
#define NVS_ZC_POS_MAC_FMT          "ZPM%u"
#define NVS_ZC_POS_TOKEN16_FMT      "ZPT%u"

// Optional: per-relay storage by global ICM relay index
#define NVS_ZC_RELAY_MAC_BYIDX_FMT  "ZRM%u"
#define NVS_ZC_RELAY_TOK_BYIDX_FMT  "ZRT%u"

class SensorEspNowManager {
public:
  // ========== Types ==========
  struct TfSample { uint16_t dist_mm; uint16_t amp; bool ok; };
  typedef bool (*TfFetchFn)(uint8_t which /*0 both,1 A,2 B*/,
                            TfSample& a, TfSample& b, uint16_t& rateHz);

  struct RelayPeer {
    uint8_t  relayIdx = 0xFF;
    int8_t   relPos   = 0;         // -N..-1 or +1..+N
    uint8_t  mac[6]   = {0};
    uint8_t  tok16[16]= {0};       // token the RELAY expects in header
  };

public:
  explicit SensorEspNowManager(ConfigManager* cfg) : cfg_(cfg) {}

  // Dependency injection
  void attachBME(BME280Manager* bme)   { bme_ = bme; }
  void attachALS(VEML7700Manager* als) { als_ = als; }
  void attachTF(TFLunaManager* tf) { tf_ = tf; }
  void attachTfFetcher(TfFetchFn fn)   { tfFetch_ = fn; }
  void attachRTC(RTCManager* rtc) { rtc_ = rtc; }   // new

  // Begin ESP-NOW on a channel; opt PMK
  bool begin(uint8_t channel = 1, const char* pmk16 = nullptr);

  // ICM pairing info: store master MAC and this-node token16 (what ICM expects for THIS sensor)
  void setIcmMac(const uint8_t mac[6]);
  void setNodeToken16(const uint8_t token16[16]);

  // App loop — runs scheduled tasks (e.g., channel switchover)
  void poll();

  // -------- Replies to ICM (helpers) --------
  bool sendSysAck(uint16_t ctr, uint8_t code = 0);
  bool sendDayNight();
  bool sendTfRaw(uint8_t which = 0);
  bool sendEnv();
  // Public API (add near other public methods)
  bool     sendPing();                   // initiate a ping to ICM (payload = nonce)
  bool     hasPendingPing() const { return pendingPing_ != 0; }
  uint32_t lastPingRttMs() const { return lastPingRttMs_; }
  void     cancelPendingPing() { pendingPing_ = 0; }

  // -------- Relay communication (zero-centered topology) --------
  bool hasTopology() const { return sensIdx_ != 0xFF && (!relNeg_.empty() || !relPos_.empty()); }
  uint8_t sensorIndex() const { return sensIdx_; }
  size_t  countNeg() const { return relNeg_.size(); }
  size_t  countPos() const { return relPos_.size(); }

  // Access a relay by zero-centered position: -1..-N or +1..+N
  bool    getRelayByPos(int8_t relPos, RelayPeer& out) const;
  // Access a relay by global ICM relay index
  bool    getRelayByIdx(uint8_t relayIdx, RelayPeer& out) const;

  // Generic raw send to a relay (domain/op/body must match relay firmware).
  // This fills IcmMsgHdr with the RELAY's token (receiver rule) and sends to its MAC.
  bool    sendRelayRawByPos(int8_t relPos, CmdDomain dom, uint8_t op, const void* body, size_t blen, bool requireAck=false);
  bool    sendRelayRawByIdx(uint8_t relayIdx, CmdDomain dom, uint8_t op, const void* body, size_t blen, bool requireAck=false);

  // Utility
  static String macBytesToStr(const uint8_t mac[6]);

  // Sensor -> Relay ("turn on for duration"), by ±position or by global relay index
  bool sendRelayOnForByPos(int8_t relPos, uint8_t chMask, uint16_t on_ms,
                          uint16_t delay_ms=0, uint16_t ttl_ms=0, bool requireAck=false);
  bool sendRelayOnForByIdx(uint8_t relayIdx, uint8_t chMask, uint16_t on_ms,
                          uint16_t delay_ms=0, uint16_t ttl_ms=0, bool requireAck=false);

  // Sensor -> Sensor (compact wave header), using prev/next MAC+token from topology
  bool sendWaveHdrToPrev(uint8_t lane, int8_t dir, uint16_t speed_mmps,
                        uint32_t eta_ms, uint8_t wave_id=0, bool requireAck=false);
  bool sendWaveHdrToNext(uint8_t lane, int8_t dir, uint16_t speed_mmps,
                        uint32_t eta_ms, uint8_t wave_id=0, bool requireAck=false);

  // High-level: fan out a moving wave (1-2-3 or 3-2-1) on a lane using REL_ON_FOR commands.
  // lane: 0=Left, 1=Right
  // dir: +1 => +pos side (+1,+2,+3...), -1 => -neg side (-1,-2,-3...)
  // speed_mmps: vehicle speed; spacing_mm: distance between relays used for cadence
  // on_ms: step pulse width; all_on_ms: optional initial "all on" flash for that side
  
  // Read last received neighbor wave (prev/next) for a lane. Returns false if none cached.
  bool getWaveFromPrev(uint8_t lane, uint16_t& speed_mmps, int8_t& dir, uint32_t& eta_ms,
                       uint8_t& wave_id, uint32_t& age_ms, uint8_t srcMac[6]) const;
  bool getWaveFromNext(uint8_t lane, uint16_t& speed_mmps, int8_t& dir, uint32_t& eta_ms,
                       uint8_t& wave_id, uint32_t& age_ms, uint8_t srcMac[6]) const;
  void clearWaveCaches(){ wavePrev_[0].clear(); wavePrev_[1].clear(); waveNext_[0].clear(); waveNext_[1].clear(); }

  bool playWave(uint8_t lane, int8_t dir, uint16_t speed_mmps,
                uint16_t spacing_mm, uint16_t on_ms,
                uint16_t all_on_ms=0, uint16_t ttl_ms=0, bool requireAck=false);
private:
  // ===== Low-level =====
  static void onRecvThunk(const uint8_t *mac, const uint8_t *data, int len);
  static void onSentThunk(const uint8_t *mac, esp_now_send_status_t status);

  void onRecv_(const uint8_t *mac, const uint8_t *data, int len);
  void onSent_(const uint8_t *mac, esp_now_send_status_t status);

  uint32_t unixNow_() const;

  // Send using THIS SENSOR's token (ICM is the receiver)
  bool sendToIcm_(CmdDomain dom, uint8_t op, const void* body, size_t blen, bool ackReq=false);
  // Send to arbitrary MAC with an explicit receiver token (e.g., relay)
  bool sendToMacWithToken_(const uint8_t mac[6], const uint8_t tok16[16],
                           CmdDomain dom, uint8_t op, const void* body, size_t blen, bool ackReq=false);

  void fillHdr_(IcmMsgHdr& h, CmdDomain dom, uint8_t op, bool ackReq, const uint8_t tok16[16]);
  void ensurePeer_(const uint8_t mac[6]);

  // ===== Handlers =====
  void handleSys_(const IcmMsgHdr& h, const uint8_t* payload, int plen);
  void handleSens_(const IcmMsgHdr& h, const uint8_t* payload, int plen);
  void handleTopo_(const IcmMsgHdr& h, const uint8_t* payload, int plen);

  // ===== Compose payloads =====
  bool makeDayNight_(uint8_t &is_day_out);
  bool makeTfRaw_(uint8_t which, TfLunaRawPayload& out);
  bool makeEnv_(SensorEnvPayload& out);

  // ===== Topology helpers =====
  void clearTopology_();
  void parseAndMirrorZcSensor_(const uint8_t* payload, int plen);
  bool findRelayByPos_(int8_t relPos, RelayPeer& out) const;
  bool findRelayByIdx_(uint8_t relayIdx, RelayPeer& out) const;

  // ===== NVS helpers =====
  void saveTopoMacStrings_();    // persist prev/next/relays MACs as human-readable strings
  void persistChannel_(uint8_t ch);

private:
  // Managers
  ConfigManager*   cfg_  = nullptr;
  BME280Manager*   bme_  = nullptr;
  VEML7700Manager* als_  = nullptr;
  TFLunaManager*   tf_      = nullptr;
  TfFetchFn        tfFetch_ = nullptr;
  RTCManager*   rtc_  = nullptr;

  // ICM peer
  uint8_t icmMac_[6] = {0};
  uint8_t token16_[16] = {0};   // THIS sensor's token (what ICM expects)
  bool    haveIcm_ = false;
  bool    haveTok_ = false;
  uint8_t channel_ = 1;

  // Scheduled channel switch (from SYS_SET_CH master command)
  bool     chSwitchPending_ = false;
  uint8_t  chTarget_        = 1;
  uint32_t chSwitchDueMs_   = 0;
  uint8_t  chWindowS_       = 0;   // informational

  // Private state (add in the private section)
  uint32_t pingNonce_      = 0;  // monotonically increasing nonce generator
  uint32_t pendingPing_    = 0;  // 0 = none, else nonce waiting for echo
  uint32_t pingSentMs_     = 0;  // millis() when we sent the ping
  uint32_t lastPingRttMs_  = 0;  // last measured RTT in ms

  // Parsed zero-centered topology
  uint8_t sensIdx_ = 0xFF;
  std::vector<RelayPeer> relNeg_;  // ordered -1,-2,...
  std::vector<RelayPeer> relPos_;  // ordered +1,+2,...

  // ----- Stored neighbor wave handoff (per lane) -----
  struct WaveCache {
    uint8_t  valid = 0;     // 1 if a recent wave was received
    uint8_t  lane  = 0;     // 0=Left, 1=Right
    int8_t   dir   = 0;     // +1 or -1
    uint8_t  wave_id = 0;   // optional correlation
    uint16_t speed_mmps = 0;
    uint32_t eta_ms = 0;
    uint32_t recv_ms = 0;   // millis() when received
    uint8_t  srcMac[6] = {0};
    void clear(){ valid = 0; lane = 0; dir = 0; wave_id = 0; speed_mmps = 0; eta_ms = 0; recv_ms = 0; memset(srcMac,0,6); }
  };

  // Cached waves from neighbors
  WaveCache wavePrev_[2];
  WaveCache waveNext_[2];
};
