// EspNowPeers.h
#pragma once
/**
 * EspNowPeers — Peer/Token database backed by NvsManager.
 *
 * Role:
 *  - Persist and cache peers: { mac, role, name, token128, enabled, topoVer }.
 *  - Admission policy: known MAC && enabled && token match (no HMAC).
 *  - Mirror enabled peers into the ESP-NOW radio table.
 *  - Persist channel hint and self role.
 *
 * Notes:
 *  - Keys are kept 6 chars to match your NvsManager constraints.
 *  - Token is stored as 32-hex string; MAC as 12-hex string (uppercase).
 */

#include <Arduino.h>
#include <vector>
#include "EspNowCompat.h"  // WiFi/esp_now + FreeRTOS + helpers
#include "NVS/NVSManager.h"
class NvsManager; // forward
class LogFS;      // forward

namespace espnow {

struct Peer {
  uint8_t  mac[6]{};
  uint8_t  role{0};
  bool     enabled{false};
  uint32_t topoVer{0};
  char     name[16]{};      // zero-terminated
  uint8_t  token[16]{};     // 128-bit device token
};

class Peers {
public:
  // ---- Lifecycle ----
  bool begin(NvsManager* nvs, LogFS* log);

  // ---- CRUD ----
  bool addPeer(const uint8_t mac[6], uint8_t role,
               const uint8_t token128[16],
               const char* name, bool enabled=true);

  bool enablePeer(const uint8_t mac[6], bool en);
  bool removePeer(const uint8_t mac[6]);

  // ---- Lookups ----
  Peer*       findByMac(const uint8_t mac[6]);
  const Peer* findByMac(const uint8_t mac[6]) const;

  bool tokenMatches(const uint8_t mac[6], const uint8_t token128[16]) const;

  // ---- Self / radio hints ----
  uint8_t  getSelfRole() const { return _selfRole; }
  void     setSelfRole(uint8_t r);           // persisted
  uint8_t  getChannel()  const { return _channel; }
  void     setChannel(uint8_t ch);           // persisted

  // ---- Topology (device-wide) ----
  bool     hasTopoToken() const { return _hasTopo; }
  bool     getTopoToken(uint8_t out[16]) const;
  void     setTopoToken(const uint8_t tok[16]);  // persisted
  uint16_t getTopoVersion() const { return _topoVer; }
  void     setTopoVersion(uint16_t v);           // persisted
  bool     topoTokenMatches(const uint8_t tok[16]) const;

  const std::vector<Peer>& all() const { return _peers; }

private:
  // Persistence helpers
  bool loadAll();
  bool saveAll();
  bool saveSlot(unsigned idx, const Peer& p);
  void clearStaleFrom(unsigned startIdx);

  // Radio sync
  bool syncRadioPeer(const Peer& p, bool add) const;

private:
  std::vector<Peer> _peers;
  NvsManager* _nvs{nullptr};
  LogFS*      _log{nullptr};
  uint8_t     _selfRole{0};
  uint8_t     _channel{NOW_DEFAULT_CHANNEL};

  // Device-wide topology token/version
  bool         _hasTopo{false};
  uint8_t      _topoToken[16]{};
  uint16_t     _topoVer{0};
};

} // namespace espnow
