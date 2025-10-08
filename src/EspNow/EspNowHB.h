// EspNowHB.h
#pragma once
/**
 * EspNowHB — Heartbeats + Time Sync helper (optional)
 *
 * State:
 *  - Tracks last RSSI/time per peer + missed HB counters.
 *
 * Tick():
 *  - Every HB_PERIOD_MS broadcasts PING to enabled peers.
 *  - If node is time authority (ICM && RTC time looks valid), broadcasts TIME_SYNC.
 *
 * On RX:
 *  - Update last-seen/RSSI by MAC.
 *  - If TIME_SYNC received and we’re non-authority, set RTC epoch from payload.
 *
 * Acceptance:
 *  - HB loss beyond N periods emits a one-time log entry; clears once peer is seen again.
 */

#include <stdint.h>
#include <vector>
#include <string.h>

#include "EspNowAPI.h"
#include "EspNowCodec.h"
#include "EspNowCompat.h"
#include "EspNowPeers.h"
#include "EspNowStack.h"
#include "Peripheral/RTCManager.h"
#include "Peripheral/LogFS.h"

namespace espnow {

#ifndef HB_PERIOD_MS
  #define HB_PERIOD_MS 2000
#endif

#ifndef HB_MISSED_LIMIT
  #define HB_MISSED_LIMIT 3
#endif

class Heartbeat {
public:
  Heartbeat() = default;

  // Provide self tokens (device required, topology optional) for composing frames.
  void begin(Stack* stack,
             Peers* peers,
             RTCManager* rtc,
             const NowAuth128& selfAuthToken,
             const NowTopoToken128* topoTokenOrNull = nullptr,
             uint32_t periodMs   = HB_PERIOD_MS,
             uint8_t missedLimit = HB_MISSED_LIMIT,
             LogFS* log          = nullptr);

  // Feed every admitted/parsed packet (post-admission) to refresh last seen/RSSI.
  void onRx(const uint8_t mac[6], const Packet& pkt, int rssiHint = 0);

  // Call this from your main loop (after Stack.loop()).
  void tick();

  // Optional manual override if you want to force authority on/off at runtime.
  void setAuthorityOverride(bool on) { _authorityOverride = on; }

  // Adjust HB knobs
  void setPeriod(uint32_t ms)      { _period = ms ? ms : HB_PERIOD_MS; }
  void setMissedLimit(uint8_t lim) { _missedLimit = lim ? lim : HB_MISSED_LIMIT; }

private:
  struct PeerState {
    uint8_t  mac[6]{};
    uint32_t lastSeenMs{0};
    int      lastRssi{0};
    uint16_t missed{0};
    bool     lossLogged{false};
  };

  PeerState& ensureState(const uint8_t mac[6]);
  bool _isAuthority() const;
  bool _rtcTimeValid() const;
  void fillHeader(NowHeader& h, uint8_t msg, uint16_t flags = 0, uint16_t topoVer = 0);
  void sendPingTo(const uint8_t mac[6]);
  void broadcastTimeSync();
  void checkLoss(uint32_t nowMs);

private:
  Stack*      _stack{nullptr};
  Peers*      _peers{nullptr};
  RTCManager* _rtc{nullptr};
  LogFS*      _log{nullptr};

  NowAuth128       _auth{};
  NowTopoToken128  _topo{};
  bool             _hasTopo{false};

  uint8_t  _selfMac[6]{};
  uint8_t  _selfRole{NOW_ROLE_ICM};
  uint16_t _seq{0};

  uint32_t _period{HB_PERIOD_MS};
  uint32_t _lastBeatMs{0};
  uint8_t  _missedLimit{HB_MISSED_LIMIT};
  bool     _authorityOverride{false};

  std::vector<PeerState> _states;
};

} // namespace espnow
