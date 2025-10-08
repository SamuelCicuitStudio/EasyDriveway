// RoleAdapters/Adapter_PMS.cpp
#include "Adapter_PMS.h"
#include <string.h>
#include <math.h>

namespace espnow {

Adapter_PMS::Adapter_PMS(CoolingManager* cool,
                         DS18B20U* ds18,
                         LogFS* log,
                         const Telemetry& tele,
                         const NowAuth128& selfAuthToken,
                         const NowTopoToken128* topoTokenOrNull,
                         uint16_t topoVerHint)
: _cool(cool), _ds(ds18), _log(log), _t(tele), _auth(selfAuthToken), _topoVer(topoVerHint)
{
  _hasTopo = (topoTokenOrNull != nullptr);
  if (_hasTopo) _topo = *topoTokenOrNull;
  now_get_mac_sta(_selfMac);
}

void Adapter_PMS::fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq, uint16_t flags) const {
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

NowPmsStatus Adapter_PMS::makeStatus() const {
  NowPmsStatus st{};

  // Local board temperature from DS18B20U (°C*10)
  int16_t tX10 = 0;
  if (_ds && _ds->isReady()) {
    float tC = _ds->lastCelsius();        // non-blocking cache
    if (isnan(tC)) {
      float tC2;
      if (_ds->readTemperature(tC2)) tC = tC2;
    }
    if (!isnan(tC)) tX10 = (int16_t)lrintf(tC * 10.0f);
  }
  st.temp_c_x10 = tX10;

  // Electrical telemetry (callbacks optional)
  st.vbus_mV = _t.readVbus_mV ? _t.readVbus_mV() : 0;
  st.vsys_mV = _t.readVsys_mV ? _t.readVsys_mV() : 0;
  st.iout_mA = _t.readIout_mA ? _t.readIout_mA() : 0;
  st.faults  = _t.readFaults  ? _t.readFaults()  : 0;

  return st;
}

bool Adapter_PMS::tryHandleCoolSet(const NowConfigWrite& cfg,
                                   const uint8_t* bodyEnd,
                                   NowPmsStatus& outStatus) const
{
  // Accept namespace "COOL" (first 4 chars)
  if (!(cfg.key[0]=='C' && cfg.key[1]=='O' && cfg.key[2]=='O' && cfg.key[3]=='L')) return false;

  // Data must have at least one byte: pct (0..100)
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&cfg) + sizeof(NowConfigWrite);
  if (data + cfg.len > bodyEnd || cfg.len < 1) return false;

  const uint8_t pct = data[0];

  if (_cool) {
    _cool->setManualSpeedPct(pct); // correct CoolingManager API
    if (_log) _log->eventf(LogFS::DOM_POWER, LogFS::EV_INFO, 4201, "COOL_SET %u%%", (unsigned)pct);
  }

  outStatus = makeStatus(); // ACK body = fresh status
  return true;
}

bool Adapter_PMS::handle(const uint8_t /*srcMac*/[6], const Packet& in, Packet& out) {
  if (!in.hdr) return false;

  switch (in.hdr->msg_type) {
    case NOW_MSG_PMS_STATUS: {
      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_PMS_STATUS, in.hdr->seq, /*flags*/0);
      NowPmsStatus st = makeStatus();
      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &st, sizeof(st));
    }

    case NOW_MSG_CONFIG_WRITE: {
      // Router should enforce HAS_TOPO; we still parse safely
      if (!in.body || in.bodyLen < sizeof(NowConfigWrite)) return false;
      const NowConfigWrite& cfg = *reinterpret_cast<const NowConfigWrite*>(in.body);
      NowPmsStatus st{};
      if (!tryHandleCoolSet(cfg, in.body + in.bodyLen, st)) return false;

      // Echo PMS_STATUS as ACK with updated telemetry
      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_PMS_STATUS, in.hdr->seq, /*flags*/0);
      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &st, sizeof(st));
    }

    case NOW_MSG_PING: {
      // Mini status reply
      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_PING_REPLY, in.hdr->seq, /*flags*/0);
      NowPingReply pr{};
      const NowPmsStatus st = makeStatus();
      pr.temp_c_x10 = (uint16_t)st.temp_c_x10;
      pr.uptime_s   = (uint16_t)(now_millis()/1000ULL);
      // state_bits: bit0=fault present, bit1=fan active (>0%)
      uint16_t state = 0;
      if (st.faults) state |= 0x0001;
      if (_cool && _cool->lastSpeedPct() > 0) state |= 0x0002; // correct API usage
      pr.state_bits = state;
      pr.reserved   = 0;

      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &pr, sizeof(pr));
    }

    default:
      return false;
  }
}

} // namespace espnow
