// EspNowHB.cpp
#include "EspNowHB.h"
#include <stdio.h>   // snprintf

namespace espnow {

// ---- local helper (portable MAC -> cstr) ----
static inline void fmt_mac(const uint8_t mac[6], char out[18]) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ---------------- public ----------------
void Heartbeat::begin(Stack* stack,
                      Peers* peers,
                      RTCManager* rtc,
                      const NowAuth128& selfAuthToken,
                      const NowTopoToken128* topoTokenOrNull,
                      uint32_t periodMs,
                      uint8_t missedLimit,
                      LogFS* log)
{
  _stack = stack; _peers = peers; _rtc = rtc; _log = log;
  _auth = selfAuthToken;
  _hasTopo = (topoTokenOrNull != nullptr);
  if (_hasTopo) _topo = *topoTokenOrNull;
  _period = periodMs;
  _missedLimit = missedLimit;
  _lastBeatMs = (uint32_t)now_millis();
  now_get_mac_sta(_selfMac);
  _selfRole = _peers ? _peers->getSelfRole() : NOW_ROLE_ICM;
  _seq = 1;
}

void Heartbeat::onRx(const uint8_t mac[6], const Packet& pkt, int rssiHint) {
  auto& st = ensureState(mac);
  st.lastSeenMs = (uint32_t)now_millis();
  st.lastRssi = rssiHint;
  if (st.missed) { st.missed = 0; st.lossLogged = false; }

  // If a TIME_SYNC arrives and we’re not authority, set RTC epoch.
  if (_rtc && !_isAuthority() && pkt.hdr && pkt.hdr->msg_type == NOW_MSG_TIME_SYNC) {
    if (pkt.body && pkt.bodyLen >= sizeof(NowTimeSync)) {
      const NowTimeSync* ts = reinterpret_cast<const NowTimeSync*>(pkt.body);
      uint64_t epochMs = (uint64_t)ts->epoch_ms_lo | ((uint64_t)ts->epoch_ms_hi << 32);
      _rtc->setUnixTime((unsigned long)(epochMs / 1000ULL));
      if (_log) _log->eventf(LogFS::DOM_RTC, LogFS::EV_INFO, 2001,
                              "RTC synced from TIME_SYNC: %lu", (unsigned long)(epochMs/1000ULL));
    }
  }
}

void Heartbeat::tick() {
  const uint32_t nowMs = (uint32_t)now_millis();
  if (nowMs - _lastBeatMs < _period) {
    checkLoss(nowMs);
    return;
  }
  _lastBeatMs = nowMs;

  // Send PING to all enabled peers; also count misses if no RX since prior tick.
  if (_stack && _peers) {
    for (const auto& p : _peers->all()) {
      if (!p.enabled) continue;
      sendPingTo(p.mac);
      auto& st = ensureState(p.mac);
      if (nowMs - st.lastSeenMs >= _period) st.missed++;
    }
  }

  // If we are the time authority, and RTC looks valid, broadcast TIME_SYNC.
  if (_isAuthority() && _rtc && _rtcTimeValid()) {
    broadcastTimeSync();
  }

  checkLoss(nowMs);
}

// ---------------- private ----------------
Heartbeat::PeerState& Heartbeat::ensureState(const uint8_t mac[6]) {
  for (auto& s : _states) if (now_same_mac(s.mac, mac)) return s;
  PeerState s; now_copy_mac(s.mac, mac); _states.push_back(s);
  return _states.back();
}

bool Heartbeat::_isAuthority() const {
  if (_authorityOverride) return true;
  return (_selfRole == NOW_ROLE_ICM);
}

bool Heartbeat::_rtcTimeValid() const {
  if (!_rtc) return false;
  const unsigned long t = _rtc->getUnixTime();
  // Consider valid if after 2020-01-01 (1577836800)
  return t >= 1577836800UL;
}

void Heartbeat::fillHeader(NowHeader& h, uint8_t msg, uint16_t flags, uint16_t topoVer) {
  memset(&h, 0, sizeof(h));
  h.proto_ver = NOW_PROTO_VER;
  h.msg_type  = msg;
  h.flags     = flags | (_hasTopo ? NOW_FLAGS_HAS_TOPO : 0);
  h.seq       = _seq++;
  h.topo_ver  = topoVer;
  h.virt_id   = NOW_VIRT_PHY;
  const uint64_t ms = now_millis();
  for (int i=0;i<6;++i) h.ts_ms[i] = (uint8_t)((ms >> (8*i)) & 0xFF);
  memcpy(h.sender_mac, _selfMac, 6);
  h.sender_role = _selfRole;
}

void Heartbeat::sendPingTo(const uint8_t mac[6]) {
  if (!_stack) return;
  NowHeader hdr; fillHeader(hdr, NOW_MSG_PING, /*flags*/0, /*topoVer*/0);
  NowPing   ping{}; // compact, zero-initialized
  Packet    pkt{};
  if (!compose(pkt, hdr, _auth, _hasTopo ? &_topo : nullptr, &ping, sizeof(ping))) return;
  _stack->send(mac, pkt, /*reliable*/false);
}

void Heartbeat::broadcastTimeSync() {
  if (!_stack || !_peers) return;
  const uint64_t epochMs = (uint64_t)_rtc->getUnixTime() * 1000ULL;
  NowTimeSync ts{};
  ts.epoch_ms_lo = (uint32_t)(epochMs & 0xFFFFFFFFu);
  ts.epoch_ms_hi = (uint32_t)((epochMs >> 32) & 0xFFFFFFFFu);
  ts.drift_ms = 0; ts.reserved = 0;

  NowHeader hdr; fillHeader(hdr, NOW_MSG_TIME_SYNC, /*flags*/0, /*topoVer*/0);
  Packet pkt{};
  if (!compose(pkt, hdr, _auth, _hasTopo ? &_topo : nullptr, &ts, sizeof(ts))) return;

  for (const auto& p : _peers->all()) {
    if (!p.enabled) continue;
    _stack->send(p.mac, pkt, /*reliable*/false);
  }
}

void Heartbeat::checkLoss(uint32_t nowMs) {
  if (!_peers) return;
  for (const auto& p : _peers->all()) {
    if (!p.enabled) continue;
    auto& st = ensureState(p.mac);

    const bool overWindow = (nowMs - st.lastSeenMs) >= (_period * (uint32_t)_missedLimit);
    if ((st.missed >= _missedLimit || overWindow) && !st.lossLogged) {
      if (_log) {
        char macStr[18]; fmt_mac(p.mac, macStr);
        _log->eventf(LogFS::DOM_WIFI, LogFS::EV_WARN, 3101,
                     "HB lost: mac=%s missed=%u period=%u",
                     macStr, (unsigned)st.missed, (unsigned)_period);
      }
      st.lossLogged = true;
    }
  }
}

} // namespace espnow
