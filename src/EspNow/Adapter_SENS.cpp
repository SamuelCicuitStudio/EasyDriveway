// RoleAdapters/Adapter_SENS.cpp
#include "Adapter_SENS.h"
#include <string.h>
#include <algorithm>

namespace espnow {

Adapter_SENS::Adapter_SENS(SensorManager* sm,
                           DS18B20U* ds18,
                           RTCManager* rtc,
                           LogFS* log,
                           const NowAuth128& selfAuthToken,
                           const NowTopoToken128* topoTokenOrNull,
                           uint16_t topoVerHint)
: _sm(sm), _ds(ds18), _rtc(rtc), _log(log), _auth(selfAuthToken), _topoVer(topoVerHint)
{
  _hasTopo = (topoTokenOrNull != nullptr);
  if (_hasTopo) _topo = *topoTokenOrNull;
  now_get_mac_sta(_selfMac);
}

void Adapter_SENS::fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq, uint16_t flags) const {
  memset(&h, 0, sizeof(h));
  h.proto_ver = NOW_PROTO_VER;
  h.msg_type  = msg;
  h.flags     = flags | (_hasTopo ? NOW_FLAGS_HAS_TOPO : 0);
  h.seq       = echoSeq;
  h.topo_ver  = _topoVer;
  h.virt_id   = NOW_VIRT_PHY;
  const uint64_t ms = now_millis();
  for (int i=0;i<6;++i) h.ts_ms[i] = (uint8_t)((ms >> (8*i)) & 0xFF);
  memcpy(h.sender_mac, _selfMac, 6);
  h.sender_role = _selfRole;
}

void Adapter_SENS::refreshCacheIfStale() {
  if (!_sm) return;
  const uint32_t now = (uint32_t)now_millis();
  if (now - _lastPollMs < _minPollMs) return;

  SensorManager::Snapshot s{};
  if (_sm->poll(s)) {            // Poll TF-Luna pairs + ALS (day/night)
    _snap = std::move(s);
    _lastPollMs = now;
  }

  // Optional DS18B20: if available, refresh one probe (implementation-specific; keep optional)
  // If your DS18B20U exposes an API like readCelsius(index), use it here.
  // We keep it conservative: leave INT16_MIN when not available.
  // Example (adjust to your DS18B20U API):
  // float tC;
  // if (_ds && _ds->readFirstCelsius(tC)) _dsTempX10 = (int16_t)lrintf(tC * 10.0f);
}

uint16_t Adapter_SENS::buildBlobV1(BlobV1& b) {
  memset(&b, 0, sizeof(b));

  // Timestamp (RTC epoch ms if available, else 0)
  uint64_t epochMs = 0;
  if (_rtc) {
    const unsigned long t = _rtc->getUnixTime(); // seconds
    if (t >= 1577836800UL) { // consider >= 2020-01-01 as valid
      epochMs = (uint64_t)t * 1000ULL;
    }
  }
  b.epoch_ms_lo = (uint32_t)(epochMs & 0xFFFFFFFFu);
  b.epoch_ms_hi = (uint32_t)((epochMs >> 32) & 0xFFFFFFFFu);

  // ALS
  b.lux   = _snap.lux;
  b.isDay = _snap.isDay;

  // DS18 temperature (optional)
  b.temp_c_x10 = _dsTempX10;

  // Pairs
  const uint8_t n = (uint8_t)std::min<size_t>(_snap.pairs.size(), 8);
  b.nPairs = n;
  for (uint8_t i=0; i<n; ++i) {
    const auto& pr = _snap.pairs[i];
    b.pairs[i].index    = pr.index;
    b.pairs[i].presentA = pr.presentA ? 1 : 0;
    b.pairs[i].presentB = pr.presentB ? 1 : 0;
    b.pairs[i].direction= (uint8_t)pr.direction;
    b.pairs[i].rate_hz  = pr.rate_hz;
    b.pairs[i].reserved1= 0;
  }

  // Actual used size = header portion + 8 * n pairs
  const uint16_t used = (uint16_t)(offsetof(BlobV1, pairs) + n * sizeof(BlobV1::Pair));
  return used;
}

bool Adapter_SENS::handle(const uint8_t /*srcMac*/[6], const Packet& in, Packet& out) {
  if (!in.hdr) return false;

  switch (in.hdr->msg_type) {
    case NOW_MSG_PING: {
      // Reply with a tiny status snapshot
      refreshCacheIfStale();

      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_PING_REPLY, in.hdr->seq, /*flags*/0);
      NowPingReply pr{};
      pr.state_bits = (_snap.isDay ? 0x0001u : 0x0000u); // bit 0 = day
      // If you later expose a board temperature, put it here; DS18 optional cached value:
      pr.temp_c_x10 = (_dsTempX10 == INT16_MIN) ? 0 : (uint16_t)_dsTempX10;
      pr.uptime_s   = (uint16_t)(now_millis() / 1000ULL);
      pr.reserved   = 0;

      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &pr, sizeof(pr));
    }

    case NOW_MSG_SENS_REPORT: {
      // Respond with a compact structured blob under 200 bytes
      refreshCacheIfStale();

      BlobV1 blob{};
      const uint16_t blobLen = buildBlobV1(blob);

      NowSensReportHdr hdrBody{};
      hdrBody.bytes = blobLen;
      hdrBody.fmt   = kFmtV1;

      // Assemble [NowSensReportHdr][blob]
      uint8_t body[sizeof(NowSensReportHdr) + sizeof(BlobV1)];
      memcpy(body, &hdrBody, sizeof(hdrBody));
      memcpy(body + sizeof(hdrBody), &blob, blobLen);
      const uint16_t bodyLen = sizeof(hdrBody) + blobLen;

      // Compose header echoing caller seq (reply, no reliable flag required)
      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_SENS_REPORT, in.hdr->seq, /*flags*/0);

      // Ensure MTU safety (NOW_MAX_BODY enforces 200B budget)
      if (bodyLen > NOW_MAX_BODY) {
        // Truncate pairs if necessary
        BlobV1 tmp = blob;
        tmp.nPairs = std::min<uint8_t>(tmp.nPairs, (uint8_t)((NOW_MAX_BODY - sizeof(NowSensReportHdr) - (offsetof(BlobV1, pairs))) / sizeof(BlobV1::Pair)));
        const uint16_t truncBlobLen = (uint16_t)(offsetof(BlobV1, pairs) + tmp.nPairs * sizeof(BlobV1::Pair));
        memcpy(body, &hdrBody, sizeof(hdrBody));
        memcpy(body + sizeof(hdrBody), &tmp, truncBlobLen);
        return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, body, (uint16_t)(sizeof(NowSensReportHdr) + truncBlobLen));
      }

      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, body, bodyLen);
    }

    default:
      return false;
  }
}

void Adapter_SENS::tick() {
  // Maintain a fresh cache respecting _minPollMs to avoid over-polling TF-Luna FPS.
  refreshCacheIfStale();
}

} // namespace espnow
