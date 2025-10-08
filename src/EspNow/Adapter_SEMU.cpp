#include "Adapter_SEMU.h"


namespace espnow {

Adapter_SEMU::Adapter_SEMU(LogFS* log,
                           const NowAuth128& selfAuth,
                           const NowTopoToken128* topoToken,
                           uint16_t topoVerHint,
                           uint8_t maxVirtuals)
: _log(log), _auth(selfAuth), _topoVer(topoVerHint), _banks(maxVirtuals)
{
  _hasTopo = (topoToken != nullptr);
  if (_hasTopo) _topo = *topoToken;
  now_get_mac_sta(_selfMac);

  if (_banks == 0) _banks = 1;
  _phase.assign(_banks, Phase{0});
}

void Adapter_SEMU::fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq,
                                  uint8_t virt, uint16_t flags) const
{
  memset(&h, 0, sizeof(h));
  h.proto_ver = NOW_PROTO_VER;
  h.msg_type  = msg;
  h.flags     = flags | (_hasTopo ? NOW_FLAGS_HAS_TOPO : 0);
  h.seq       = echoSeq;
  h.topo_ver  = _topoVer;
  h.virt_id   = virt;               // echo caller's virt_id so their router maps it back
  const uint64_t ms = now_millis();
  for (int i=0;i<6;++i) h.ts_ms[i] = (uint8_t)((ms >> (8*i)) & 0xFF);
  memcpy(h.sender_mac, _selfMac, 6);
  h.sender_role = NOW_ROLE_SEMU;
}

// Simple deterministic waveforms without floats/rand
static inline uint16_t wrap_u16(uint32_t x, uint16_t mod) { return (uint16_t)(x % mod); }

Adapter_SEMU::SemuV1 Adapter_SEMU::makeSemuV1(uint8_t virt) const {
  const uint32_t tms   = (uint32_t)now_millis();
  const uint32_t seed  = (uint32_t)virt * 2654435761u;   // Knuth mix
  const uint32_t phase = _phase[virt].k * 1103515245u + seed;

  // Temperature between 22.0..28.0 C, humidity 35..65%, lux 50..950, distance 200..1800 mm
  int16_t  temp_x10 = (int16_t)(220 + (phase % 61));                     // 22.0..28.1
  uint16_t humi_x10 = (uint16_t)(350 + ((phase >> 4) % 301));            // 35.0..65.0
  uint16_t lux      = (uint16_t)(50  + ((phase >> 7) % 901));            // 50..950
  uint16_t dist_mm  = (uint16_t)(200 + ((phase >> 10) % 1601));          // 200..1800
  uint16_t status   = 0x000F;                                            // pretend 4 sensors present

  SemuV1 v{};
  v.t_ms       = tms;
  v.temp_c_x10 = temp_x10;
  v.humi_x10   = humi_x10;
  v.lux        = lux;
  v.dist_mm    = dist_mm;
  v.status     = status;
  return v;
}

NowPingReply Adapter_SEMU::makePing(uint8_t virt) const {
  NowPingReply pr{};
  const SemuV1 v = makeSemuV1(virt);
  pr.state_bits = v.status;                   // mini status flags
  pr.temp_c_x10 = (uint16_t)v.temp_c_x10;
  pr.uptime_s   = (uint16_t)((v.t_ms / 1000u) & 0xFFFF);
  pr.reserved   = 0;
  return pr;
}

bool Adapter_SEMU::handle(const uint8_t /*srcMac*/[6], const Packet& in, Packet& out) {
  if (!in.hdr) return false;

  const uint8_t virt = in.hdr->virt_id;
  if (virt == NOW_VIRT_PHY || !validBank(virt)) {
    // Not targeting a virtual instance → ignore
    return false;
  }

  switch (in.hdr->msg_type) {
    case NOW_MSG_SENS_REPORT: {
      // Compose: NowSensReportHdr + SEMU_FMT_V1 blob
      const SemuV1 v = makeSemuV1(virt);

      uint8_t body[sizeof(NowSensReportHdr) + sizeof(SemuV1)];
      NowSensReportHdr hdrBody{};
      hdrBody.bytes = (uint16_t)sizeof(SemuV1);
      hdrBody.fmt   = (uint16_t)SEMU_FMT_V1;

      memcpy(body, &hdrBody, sizeof(hdrBody));
      memcpy(body + sizeof(hdrBody), &v, sizeof(v));

      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_SENS_REPORT, in.hdr->seq, virt, /*flags*/0);
      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, body, sizeof(body));
    }

    case NOW_MSG_PING: {
      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_PING_REPLY, in.hdr->seq, virt, /*flags*/0);
      const NowPingReply pr = makePing(virt);
      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &pr, sizeof(pr));
    }

    default:
      return false;
  }
}

void Adapter_SEMU::tick() {
  // Advance phase deterministically; no busy work
  const uint32_t step = 1;
  for (auto& p : _phase) p.k += step;
}

} // namespace espnow
