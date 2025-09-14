/**************************************************************
 * File    : ICM_NwModule.cpp — ICM-side + node-side helpers
 * Update  : TOKEN=HEX STRING (32 ASCII chars) across all calls
 * Notes   : - All sender APIs are here.
 *           - ICM builds expose a static receive dispatcher
 *             Core::icmRecvCallback(...) registered via setRecvCallback.
 *           - Node builds (PMS/SENS/RELAY) expose their own dispatchers.
 **************************************************************/
#include "ICM_Nw.h"
#include "Now_API.h"
#include <vector>
#include <cstring>
#include <mbedtls/sha256.h>

using std::vector;

namespace NwCore {

// ======================= Small hex helpers =======================
static inline char nyb_to_hex(uint8_t v) { return (v < 10) ? char('0'+v) : char('a'+(v-10)); }
static inline void bytes_to_hex32(const uint8_t in[16], char out[32]) {
  for (int i=0;i<16;i++){ out[i*2] = nyb_to_hex(in[i]>>4); out[i*2+1] = nyb_to_hex(in[i]&0xF); }
}

// ======================= Shared cfg helpers (nodes) =======================
namespace {
static bool tok32_from_cfg(ConfigManager* cfg, char out[NOW_TOKEN_HEX_LEN]) {
  if (!cfg) return false;
  String t = cfg->GetString(NVS_KEY_ESP_TOKEN, "");
  if (t.length() != NOW_TOKEN_HEX_LEN) return false;
  for (int i = 0; i < NOW_TOKEN_HEX_LEN; ++i) out[i] = t[i];
  return true;
}
static bool icm_mac_from_cfg(ConfigManager* cfg, uint8_t out[6]) {
  if (!cfg) return false;
  String macHex = cfg->GetString(NVS_KEY_NET_ICMMAC, "");
  return NwCore::Core::macStrToBytes(macHex.c_str(), out);
}
} // namespace

// ========================================================================
//                                  ICM
// ========================================================================
#ifdef NVS_ROLE_ICM

// ======================= Unified sender =======================
esp_err_t Core::sendFrame(const uint8_t dstMac[6], uint8_t dom, uint8_t op,
                          const void* payload, size_t plen, uint32_t waitAckMs,
                          esp_now_send_status_t* outStatus) {
  if (!dstMac) return ESP_ERR_INVALID_ARG;

  char tok_hex[NOW_TOKEN_HEX_LEN];
  if (!expectedTokenForMac(dstMac, tok_hex)) return ESP_ERR_INVALID_STATE;

  NowMsgHdr h{};
  fillHeader(h, dom, op, tok_hex);

  const size_t frameLen = sizeof(NowMsgHdr) + plen;
  vector<uint8_t> frame(frameLen);
  memcpy(frame.data(), &h, sizeof(h));
  if (payload && plen) memcpy(frame.data() + sizeof(h), payload, plen);

  return send(dstMac, frame.data(), frame.size(), waitAckMs, outStatus);
}

// ---------------- Pairing / Provisioning ----------------
esp_err_t Core::prDevInfoQuery(const uint8_t mac[6], uint32_t waitAckMs) {
  return sendFrame(mac, NOW_DOM_PR, PR_DEVINFO, nullptr, 0, waitAckMs);
}
esp_err_t Core::prAssign(const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN],
                         uint8_t channel, uint32_t waitAckMs) {
  PrAssignPayload p{};
  for (int i=0;i<NOW_TOKEN_HEX_LEN;i++) p.token_hex[i] = token_hex[i];
  efuseMacBytes(p.icm_mac);  // ICM MAC
  p.channel = channel ? channel : _channel;
  return sendFrame(mac, NOW_DOM_PR, PR_ASSIGN, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::prCommit(const uint8_t mac[6], uint32_t waitAckMs) {
  return sendFrame(mac, NOW_DOM_PR, PR_COMMIT, nullptr, 0, waitAckMs);
}
esp_err_t Core::prRekey(const uint8_t mac[6], const char new_token_hex[NOW_TOKEN_HEX_LEN],
                        uint32_t waitAckMs) {
  PrAssignPayload p{};
  for (int i=0;i<NOW_TOKEN_HEX_LEN;i++) p.token_hex[i] = new_token_hex[i];
  efuseMacBytes(p.icm_mac);
  p.channel = _channel;
  return sendFrame(mac, NOW_DOM_PR, PR_REKEY, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::prUnpair(const uint8_t mac[6], uint32_t waitAckMs) {
  return sendFrame(mac, NOW_DOM_PR, PR_UNPAIR, nullptr, 0, waitAckMs);
}

// ---------------- System ----------------
esp_err_t Core::sysPing(const uint8_t mac[6], uint32_t waitAckMs) {
  return sendFrame(mac, NOW_DOM_SYS, SYS_PING, nullptr, 0, waitAckMs);
}
esp_err_t Core::sysModeSet(const uint8_t mac[6], uint8_t mode, uint32_t waitAckMs) {
  SysModePayload p{ mode };
  return sendFrame(mac, NOW_DOM_SYS, SYS_MODE_SET, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::sysSetChannel(const uint8_t mac[6], uint8_t newCh, uint32_t waitAckMs) {
  SysSetChPayload p{ newCh, {0,0,0} };
  return sendFrame(mac, NOW_DOM_SYS, SYS_SET_CH, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::sysTimeSync(const uint8_t mac[6], uint64_t epoch_ms, int32_t offset_ms, int16_t drift_ppm, uint8_t ver, uint32_t waitAckMs) {
  SysTimeSyncPayload p{};
  p.epoch_ms = epoch_ms;
  p.offset_ms = offset_ms;
  p.drift_ppm = drift_ppm;
  p.ver       = ver;
  return sendFrame(mac, NOW_DOM_SYS, SYS_TIME_SYNC, &p, sizeof(p), waitAckMs);
}

// ---------------- Indicators ----------------
esp_err_t Core::indBuzzerSet(const uint8_t mac[6], uint8_t en, uint16_t hz, uint8_t vol, uint16_t dur_ms, uint32_t waitAckMs) {
  BuzzerSetPayload p{ en, hz, vol, dur_ms };
  return sendFrame(mac, NOW_DOM_IND, IND_BUZ_SET, &p, sizeof(p), waitAckMs);
}

esp_err_t Core::indBuzzerSet(uint8_t on, uint8_t pat, uint16_t dur_ms) {
  // Ensure slots are populated from registry (safe to call repeatedly)
  slotsLoadFromRegistry();

  // Helper to send to one MAC if present
  auto try_send = [&](const uint8_t mac[6]) -> esp_err_t {
    if (Core::macIsZero(mac)) return ESP_ERR_INVALID_ARG;
    if (pat > 0) {
      // pattern: 1 repetition by default
      return indBuzzerPat(mac, pat, /*reps*/1, dur_ms, /*waitAckMs*/30);
    } else {
      // pat==0 → explicit off
      return indBuzzerSet(mac, /*en*/0, /*hz*/0, /*vol*/0, /*dur_ms*/dur_ms, /*waitAckMs*/30);
    }
  };

  esp_err_t last = ESP_ERR_INVALID_STATE;
  // broadcast to PMS (max 1), relays, sensors
  for (int i = 0; i < NOW_MAX_POWER;   ++i) { if (!Core::macIsZero(pms[i].mac))    last = try_send(pms[i].mac); }
  for (int i = 0; i < NOW_MAX_RELAYS;  ++i) { if (!Core::macIsZero(relays[i].mac)) last = try_send(relays[i].mac); }
  for (int i = 0; i < NOW_MAX_SENSORS; ++i) { if (!Core::macIsZero(sensors[i].mac)) last = try_send(sensors[i].mac); }

  return last; // ESP_OK if at least one send succeeded, otherwise last error
}

esp_err_t Core::indBuzzerPat(const uint8_t mac[6], uint8_t pat, uint8_t reps, uint16_t dur_ms, uint32_t waitAckMs) {
  BuzzerPatPayload p{ pat, reps, dur_ms };
  return sendFrame(mac, NOW_DOM_IND, IND_BUZ_PAT, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::indLedSet(const uint8_t mac[6], uint8_t r, uint8_t g, uint8_t b, uint8_t br, uint16_t dur_ms, uint32_t waitAckMs) {
  LedSetPayload p{ r,g,b,br,dur_ms };
  return sendFrame(mac, NOW_DOM_IND, IND_LED_SET, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::indLedPat(const uint8_t mac[6], uint8_t pat, uint16_t dur_ms, uint32_t waitAckMs) {
  LedPatPayload p{ pat, dur_ms };
  return sendFrame(mac, NOW_DOM_IND, IND_LED_PAT, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::indAlert(const uint8_t mac[6], uint8_t lvl, uint16_t dur_ms, uint32_t waitAckMs) {
  AlertPayload p{ lvl, dur_ms };
  return sendFrame(mac, NOW_DOM_IND, IND_ALERT, &p, sizeof(p), waitAckMs);
}

// ---------------- Power (PMS) ----------------
esp_err_t Core::pwrOnOff(const uint8_t mac[6], uint8_t on, uint32_t waitAckMs) {
  PwrOnOffPayload p{ on };
  return sendFrame(mac, NOW_DOM_PWR, PWR_ON_OFF, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::pwrSrcSet(const uint8_t mac[6], uint8_t src, uint32_t waitAckMs) {
  PwrSrcPayload p{ src };
  return sendFrame(mac, NOW_DOM_PWR, PWR_SRC_SET, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::pwrClrFlags(const uint8_t mac[6], uint32_t waitAckMs) {
  return sendFrame(mac, NOW_DOM_PWR, PWR_CLR_FLG, nullptr, 0, waitAckMs);
}
esp_err_t Core::pwrQuery(const uint8_t mac[6], uint32_t waitAckMs) {
  return sendFrame(mac, NOW_DOM_PWR, PWR_QRY, nullptr, 0, waitAckMs);
}

// ---------------- Relay ----------------
esp_err_t Core::relSet(const uint8_t mac[6], uint8_t ch, uint8_t on, uint32_t waitAckMs) {
  RelSetPayload p{ ch, on };
  return sendFrame(mac, NOW_DOM_REL, REL_SET, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::relOnFor(const uint8_t mac[6], uint8_t chMask, uint16_t on_ms, uint32_t waitAckMs) {
  RelOnForPayload p{ chMask, on_ms };
  return sendFrame(mac, NOW_DOM_REL, REL_ON_FOR, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::relSchedule(const uint8_t mac[6], uint32_t t0_ms,
                            uint16_t l_on_at, uint16_t l_off_at,
                            uint16_t r_on_at, uint16_t r_off_at,
                            uint32_t waitAckMs) {
  RelSchedPayload p{};
  p.t0_ms   = t0_ms;
  p.l_on_at = l_on_at; p.l_off_at = l_off_at;
  p.r_on_at = r_on_at; p.r_off_at = r_off_at;
  return sendFrame(mac, NOW_DOM_REL, REL_SCHED, &p, sizeof(p), waitAckMs);
}
esp_err_t Core::relQuery(const uint8_t mac[6], uint32_t waitAckMs) {
  return sendFrame(mac, NOW_DOM_REL, REL_QRY, nullptr, 0, waitAckMs);
}

// ---------------- Sensor ----------------
esp_err_t Core::senRequest(const uint8_t mac[6], uint32_t waitAckMs) {
  // Ask sensor to immediately report (SEN_TRIG)
  return sendFrame(mac, NOW_DOM_SEN, SEN_TRIG, nullptr, 0, waitAckMs);
}

// ---------------- Topology (JSON only) ----------------
esp_err_t Core::topSetSensorJSON(const uint8_t sensorMac[6], const char* json, size_t len, uint32_t waitAckMs) {
  return sendFrame(sensorMac, NOW_DOM_TOP, TOP_PUSH_SEN_JSON, json, len, waitAckMs);
}
esp_err_t Core::topSetRelayJSON (const uint8_t relayMac[6],  const char* json, size_t len, uint32_t waitAckMs) {
  return sendFrame(relayMac, NOW_DOM_TOP, TOP_PUSH_RLY_JSON, json, len, waitAckMs);
}

// ======================= Auto-pairing helper (ICM) =======================
static void derive_token_hex32(const uint8_t icm_mac[6], const uint8_t node_mac[6], char out_hex[NOW_TOKEN_HEX_LEN]) {
  uint8_t buf[6+6+8];
  memcpy(buf, icm_mac, 6);
  memcpy(buf+6, node_mac, 6);
  uint32_t t = (uint32_t)millis();
  uint32_t r = (uint32_t)esp_random();
  memcpy(buf+12, &t, 4);
  memcpy(buf+16, &r, 4);

  uint8_t dig[32];
  mbedtls_sha256(buf, sizeof(buf), dig, 0);
  bytes_to_hex32(dig, out_hex); // first 16 bytes → 32 hex chars (lowercase)
}

bool Core::auto_pair_from_devinfo(Core* self, const uint8_t mac[6], const DevInfoPayload* info) {
  if (!self || !info) return false;

  if (self->macInRegistry(mac)) return true;

  char tok[NOW_TOKEN_HEX_LEN];
  uint8_t icm_mac[6]; Core::efuseMacBytes(icm_mac);
  derive_token_hex32(icm_mac, mac, tok);

  bool ok = false;
  uint8_t idx = 0;
  switch (info->kind) {
    case NOW_KIND_SENS:  ok = self->icmRegistryAutoAddSensor(mac, tok, idx); break;
    case NOW_KIND_RELAY: ok = self->icmRegistryAutoAddRelay (mac, tok, idx); break;
    case NOW_KIND_PMS:   ok = self->icmRegistryAutoSetPower (mac, tok);      break;
    default: ok = false; break;
  }
  if (!ok) return false;

  self->addPeer(mac, self->channel(), /*encrypt*/false, /*lmk*/nullptr);
  self->prAssign(mac, tok, self->channel(), /*waitAckMs*/60);
  self->prCommit(mac, /*waitAckMs*/60);
  self->indAlert(mac, IND_ALERT_INFO, /*dur_ms*/200, 30);

  if (self->log()) {
    String m = String("AUTO-PAIRED ") + Core::macBytesToStr(mac) + " kind=" + String((int)info->kind) + " idx=" + String((int)idx);
    self->log()->event(ICMLogFS::DOM_SECURITY, ICMLogFS::EV_INFO, 805, m);
  }
  return true;
}

// ======================= ICM receive dispatcher =======================
static inline ICMLogFS::Domain logDomForNow(uint8_t nowDom) {
  switch (nowDom) {
    case NOW_DOM_SYS: return ICMLogFS::DOM_SYSTEM;
    case NOW_DOM_PWR: return ICMLogFS::DOM_POWER;
    case NOW_DOM_FW:  return ICMLogFS::DOM_FW;
    case NOW_DOM_LOG: return ICMLogFS::DOM_STORAGE;
    case NOW_DOM_PR:  return ICMLogFS::DOM_SECURITY;
    default:          return ICMLogFS::DOM_ESPNOW;
  }
}

/*static*/ void Core::icmRecvCallback(const uint8_t* mac, const uint8_t* data, int len)
{
  if (!s_self || !mac || !data || len < (int)sizeof(NowMsgHdr)) return;

  const NowMsgHdr& h = *reinterpret_cast<const NowMsgHdr*>(data);
  if (h.ver != NOW_HDR_VER) return;

  const uint8_t* body = data + sizeof(NowMsgHdr);
  const int      blen = len - (int)sizeof(NowMsgHdr);

  // Bypass token check ONLY for PR_DEVINFO (bootstrap)
  bool token_ok = (h.dom == NOW_DOM_PR && h.op == PR_DEVINFO) ? true : s_self->verifyIncomingForLocal(h);
  if (!token_ok) {
    if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_WARN, 401, "RX drop (bad token)");
    return;
  }

  // Presence mark
  s_self->slotMarkSeen(mac, /*state_flags*/0, /*rssi*/0, (uint32_t)millis());

  String macS = macBytesToStr(mac);
  const char* macStr = macS.c_str();
  ICMLogFS::Domain ldom = logDomForNow(h.dom);

  switch (h.dom) {
    case NOW_DOM_SYS: {
      switch (h.op) {
        case SYS_HB: {
          if (blen >= (int)sizeof(SysHeartbeatPayload)) {
            const auto* p = reinterpret_cast<const SysHeartbeatPayload*>(body);
            s_self->slotMarkSeen(mac, p->state_flags, /*rssi*/0, (uint32_t)millis());
            if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 110,
              "HB %s kind=%u state=0x%04x buz=%u/%u led=%u,%u,%u/%u",
              macStr, p->kind, p->state_flags, p->buz_on, p->buz_pat, p->led_r, p->led_g, p->led_b, p->led_pat);
          }
        } break;

        case SYS_STATE_EVT: {
          if (blen >= (int)sizeof(SysHeartbeatPayload)) {
            const auto* p = reinterpret_cast<const SysHeartbeatPayload*>(body);
            s_self->slotMarkSeen(mac, p->state_flags, /*rssi*/0, (uint32_t)millis());
            if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 111,
              "STATE %s flags=0x%04x", macStr, p->state_flags);
          }
        } break;

        case SYS_PING: {
          if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 112,
            "PING echo %s seq=%u", macStr, (unsigned)h.seq);
        } break;

        default:
          if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 199,
            "SYS op=0x%02x from %s (%dB)", (unsigned)h.op, macStr, blen);
          break;
      }
    } break;

    case NOW_DOM_PWR: {
      switch (h.op) {
        case PWR_REP: {
          if (blen >= (int)sizeof(PwrReportPayload)) {
            const auto* p = reinterpret_cast<const PwrReportPayload*>(body);
            if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_POWER, ICMLogFS::EV_INFO, 210,
              "PWR %s vbus=%umV vbat=%umV ibus=%umA ibat=%umA T=%.2fC flags=0x%02x",
              macStr, p->vbus_mV, p->vbat_mV, p->ibus_mA, p->ibat_mA, (float)p->temp_c_x100 / 100.0f, p->flags);
          }
        } break;

        default:
          if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_POWER, ICMLogFS::EV_INFO, 299,
            "PWR op=0x%02x from %s (%dB)", (unsigned)h.op, macStr, blen);
          break;
      }
    } break;

    case NOW_DOM_REL: {
      switch (h.op) {
        case REL_REP: {
          if (blen >= (int)sizeof(RelReportPayload)) {
            const auto* p = reinterpret_cast<const RelReportPayload*>(body);
            // Mirror into relayLive[]
            uint8_t idx;
            if (s_self->icmRegistryIndexOfRelayMac(mac, idx) && idx>=1 && idx<=NOW_MAX_RELAYS) {
              RelayLive& L = s_self->relayLive[idx-1];
              L.temp_c_x100 = p->temp_c_x100;
              L.updated_ms  = nowMs();
            }
            if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 320,
              "REL %s temp=%.2fC idx=%u", macStr, (float)p->temp_c_x100 / 100.0f, (unsigned)idx);
          }
        } break;

        default:
          if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 399,
            "REL op=0x%02x from %s (%dB)", (unsigned)h.op, macStr, blen);
          break;
      }
    } break;

    case NOW_DOM_SEN: {
      switch (h.op) {
        case SEN_REP: {
          if (blen >= (int)sizeof(SenReportPayload)) {
            const auto* p = reinterpret_cast<const SenReportPayload*>(body);
            // Mirror into sensLive[]
            uint8_t idx;
            if (s_self->icmRegistryIndexOfSensorMac(mac, idx) && idx>=1 && idx<=NOW_MAX_SENSORS) {
              SensorLive& S = s_self->sensLive[idx-1];
              S.t_c_x100 = p->t_c_x100;
              S.rh_x100  = p->rh_x100;
              S.p_Pa     = p->p_Pa;
              S.lux_x10  = p->lux_x10;
              S.is_day   = p->is_day;
              S.tfA_mm   = p->tfA_mm;
              S.tfB_mm   = p->tfB_mm;
              S.updated_ms = nowMs();
            }
            if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 330,
              "SEN %s T=%.2fC RH=%.2f%% P=%ldPa LUX=%.1f %s tfA=%umm tfB=%umm",
              macStr,
              (float)p->t_c_x100 / 100.0f, (float)p->rh_x100 / 100.0f, (long)p->p_Pa,
              (float)p->lux_x10 / 10.0f, (p->is_day ? "DAY" : "NIGHT"),
              (unsigned)p->tfA_mm, (unsigned)p->tfB_mm);
          }
        } break;

        default:
          if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 399,
            "SEN op=0x%02x from %s (%dB)", (unsigned)h.op, macStr, blen);
          break;
      }
    } break;

    case NOW_DOM_FW: {
      switch (h.op) {
        case FW_STATUS: {
          if (blen >= (int)sizeof(FwStatusPayload)) {
            const auto* p = reinterpret_cast<const FwStatusPayload*>(body);
            if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_FW, ICMLogFS::EV_INFO, 550,
              "FW %s state=%u rec=%lu/%lu slot=%u err=%u",
              macStr, p->state, (unsigned long)p->received, (unsigned long)p->size, p->slot, p->err);
          }
        } break;
        case FW_POST_OK:
        case FW_POST_FAIL: {
          if (blen >= (int)sizeof(FwPostBootPayload)) {
            const auto* p = reinterpret_cast<const FwPostBootPayload*>(body);
            if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_FW, ICMLogFS::EV_INFO, 551,
              "FW POST %s ok=%u slot=%u ver=%s", macStr, p->ok, p->slot, p->ver);
          }
        } break;
        default:
          if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_FW, ICMLogFS::EV_INFO, 599,
            "FW op=0x%02x from %s (%dB)", (unsigned)h.op, macStr, blen);
          break;
      }
    } break;

    case NOW_DOM_LOG: {
      switch (h.op) {
        case LOG_STAT: {
          if (blen >= (int)sizeof(LogStatPayload)) {
            const auto* p = reinterpret_cast<const LogStatPayload*>(body);
            if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_STORAGE, ICMLogFS::EV_INFO, 660,
              "LOG %s cap=%ukB used=%ukB rec=%u", macStr, p->cap_kb, p->used_kb, p->rec_cnt);
          }
        } break;
        case LOG_TAIL:
        case LOG_FETCH: {
          if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_STORAGE, ICMLogFS::EV_INFO, 661,
            "LOG %s chunk len=%d (op=0x%02x)", macStr, blen, (unsigned)h.op);
        } break;
        default:
          if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_STORAGE, ICMLogFS::EV_INFO, 699,
            "LOG op=0x%02x from %s (%dB)", (unsigned)h.op, macStr, blen);
          break;
      }
    } break;

    case NOW_DOM_PR: {
      switch (h.op) {
        case PR_DEVINFO: {
          if (blen >= (int)sizeof(DevInfoPayload)) {
            const auto* p = reinterpret_cast<const DevInfoPayload*>(body);
            auto_pair_from_devinfo(s_self, mac, p);
            if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SECURITY, ICMLogFS::EV_INFO, 800,
              "PR %s kind=%u devid=%s hw=%s sw=%s build=%s",
              macStr, p->kind, p->devid, p->hwrev, p->swver, p->build);
          }
        } break;

        default:
          if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SECURITY, ICMLogFS::EV_INFO, 899,
            "PR op=0x%02x from %s (%dB)", (unsigned)h.op, macStr, blen);
          break;
      }
    } break;

    case NOW_DOM_CFG: {
      if (s_self->log()) s_self->log()->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 900,
        "CFG from %s op=0x%02x (%dB)", macStr, (unsigned)h.op, blen);
    } break;

    default:
      if (s_self->log()) s_self->log()->eventf(ldom, ICMLogFS::EV_INFO, 999,
        "UNKNOWN dom=0x%02x op=0x%02x from %s (%dB)", (unsigned)h.dom, (unsigned)h.op, macStr, blen);
      break;
  }
}

#endif // NVS_ROLE_ICM

// ========================================================================
//                                  PMS
// ========================================================================
#if defined(NVS_ROLE_PMS)

bool Core::pmsIsPaired() const {
  return _cfg ? _cfg->GetBool(PMS_PAIRED_KEY, PMS_PAIRED_DEF) : PMS_PAIRED_DEF;
}
bool Core::pmsInPairingMode() const {
  return _cfg ? _cfg->GetBool(PMS_PAIRING_KEY, PMS_PAIRING_DEF) : PMS_PAIRING_DEF;
}
esp_err_t Core::pmsSysPing(uint32_t waitAckMs) {
  if (!_cfg) return ESP_ERR_INVALID_STATE;

  char tok[NOW_TOKEN_HEX_LEN];
  if (!tok32_from_cfg(_cfg, tok)) return ESP_ERR_INVALID_STATE;

  uint8_t icmMac[6];
  if (!icm_mac_from_cfg(_cfg, icmMac)) return ESP_ERR_INVALID_ARG;

  NowMsgHdr h; Core::fillHeader(h, NOW_DOM_SYS, SYS_PING, tok);
  return send(icmMac, &h, sizeof(h), waitAckMs);
}

esp_err_t Core::pmsSendReport(uint32_t waitAckMs) {
  ConfigManager* cfg = _cfg;
  if (!cfg) return ESP_ERR_INVALID_STATE;

  uint8_t icmMac[6];
  if (!icm_mac_from_cfg(cfg, icmMac)) return ESP_ERR_INVALID_ARG;

  char myTok[NOW_TOKEN_HEX_LEN];
  if (!tok32_from_cfg(cfg, myTok)) return ESP_ERR_INVALID_STATE;

  NowMsgHdr h; Core::fillHeader(h, NOW_DOM_PWR, PWR_REP, myTok);

  PwrReportPayload pl{};
  pl.vbus_mV      = PMS.vbus_mV;
  pl.vbat_mV      = PMS.vbat_mV;
  pl.ibus_mA      = PMS.ibus_mA;
  pl.ibat_mA      = PMS.ibat_mA;
  pl.temp_c_x100  = PMS.temp_c_x100;
  pl.flags        = PMS.flags;
  pl.rsv          = 0;

  uint8_t buf[sizeof(NowMsgHdr) + sizeof(PwrReportPayload)];
  memcpy(buf, &h, sizeof(h));
  memcpy(buf + sizeof(h), &pl, sizeof(pl));

  return send(icmMac, buf, sizeof(buf), waitAckMs);
}

// Apply ICM controls locally on PMS (hook HW here)
void Core::pmsApplyOnOff(uint8_t on)       { PMS.on = on ? 1 : 0; }
void Core::pmsApplySource(uint8_t src)     { PMS.src = src; }
void Core::pmsApplyClearFlags(uint8_t msk) { PMS.flags = (uint8_t)(PMS.flags & ~msk); }

// ---------------- PMS dispatcher ----------------
void Core::pmsRecvCallback(const uint8_t* mac, const uint8_t* data, int len) {
  if (!s_self || !mac || !data || len < (int)sizeof(NowMsgHdr)) return;

  const NowMsgHdr& h = *reinterpret_cast<const NowMsgHdr*>(data);
  if (h.ver != NOW_HDR_VER) return;

  const int blen            = len - (int)sizeof(NowMsgHdr);
  const uint8_t* body       = data + sizeof(NowMsgHdr);
  const bool paired         = s_self->pmsIsPaired();
  const bool pairing        = s_self->pmsInPairingMode();

  // Pairing / Provisioning (no token required when not yet committed)
  if (h.dom == NOW_DOM_PR) {
    switch (h.op) {
      case PR_ASSIGN: {
        if (blen < (int)sizeof(PrAssignPayload)) return;
        const PrAssignPayload* p = reinterpret_cast<const PrAssignPayload*>(body);
        if (!Core::macEq(p->icm_mac, mac)) return; // sender must match payload
        if (s_self->nodeApplyPairing(p->icm_mac, p->channel, p->token_hex)) {
          if (s_self->_cfg) {
            s_self->_cfg->PutBool(PMS_PAIRING_KEY, true);
            s_self->_cfg->PutBool(PMS_PAIRED_KEY,  false);
          }
          s_self->addPeer(p->icm_mac, p->channel, /*encrypt*/false, /*lmk*/nullptr);
        }
      } break;

      case PR_COMMIT: {
        if (s_self->_cfg) {
          s_self->_cfg->PutBool(PMS_PAIRED_KEY,  true);
          s_self->_cfg->PutBool(PMS_PAIRING_KEY, false);
        }
      } break;

      case PR_REKEY: {
        if (blen < (int)sizeof(PrAssignPayload)) return;
        const PrAssignPayload* p = reinterpret_cast<const PrAssignPayload*>(body);
        if (!Core::macEq(p->icm_mac, mac)) return;
        if (s_self->nodeApplyPairing(p->icm_mac, p->channel, p->token_hex)) {
          if (s_self->_cfg) s_self->_cfg->PutBool(PMS_PAIRED_KEY, true);
        }
      } break;

      case PR_UNPAIR: {
        if (s_self->nodeClearPairing() && s_self->_cfg) {
          s_self->_cfg->PutBool(PMS_PAIRED_KEY,  PMS_PAIRED_DEF);
          s_self->_cfg->PutBool(PMS_PAIRING_KEY, PMS_PAIRING_DEF);
        }
      } break;

      default: break;
    }
    return;
  }

  // Non-PR domains: require token when paired and not in a pairing session
  if (paired && !pairing) {
    if (!s_self->verifyIncomingForLocal(h)) return;
  }

  switch (h.dom) {
    case NOW_DOM_PWR: {
      switch (h.op) {
        case PWR_QRY:     s_self->pmsSendReport(30); break;
        case PWR_ON_OFF:  if (blen >= 1) s_self->pmsApplyOnOff(body[0]); break;
        case PWR_SRC_SET: if (blen >= 1) s_self->pmsApplySource(body[0]); break;
        case PWR_CLR_FLG: if (blen >= 1) s_self->pmsApplyClearFlags(body[0]); break;
        default: break;
      }
    } break;

    case NOW_DOM_SYS: {
      // Accept SYS_SET_CH etc. if you want, as needed.
    } break;

    default: break;
  }
}

#endif // NVS_ROLE_PMS

// ========================================================================
//                                 SENSOR
// ========================================================================
#ifdef NVS_ROLE_SENS

// Publish current live measurements as SEN_REP
esp_err_t Core::sensSendReport(uint32_t waitAckMs) {
  if (!_cfg) return ESP_ERR_INVALID_STATE;

  uint8_t icmMac[6];
  if (!icm_mac_from_cfg(_cfg, icmMac)) return ESP_ERR_INVALID_ARG;

  char myTok[NOW_TOKEN_HEX_LEN];
  if (!tok32_from_cfg(_cfg, myTok)) return ESP_ERR_INVALID_STATE;

  NowMsgHdr h; Core::fillHeader(h, NOW_DOM_SEN, SEN_REP, myTok);

  SenReportPayload pl{};
  pl.t_c_x100 = (int16_t)SEN_LIVE.t_c_x100;
  pl.rh_x100  = (uint16_t)SEN_LIVE.rh_x100;
  pl.p_Pa     = (int32_t)SEN_LIVE.p_Pa;
  pl.lux_x10  = (uint16_t)SEN_LIVE.lux_x10;
  pl.is_day   = (uint8_t)SEN_LIVE.is_day;
  pl.rsv      = 0;
  pl.tfA_mm   = (uint16_t)SEN_LIVE.tfA_mm;
  pl.tfB_mm   = (uint16_t)SEN_LIVE.tfB_mm;

  uint8_t buf[sizeof(NowMsgHdr) + sizeof(SenReportPayload)];
  memcpy(buf, &h, sizeof(h));
  memcpy(buf + sizeof(h), &pl, sizeof(pl));
  return send(icmMac, buf, sizeof(buf), waitAckMs);
}

// Apply ICM mode and trigger
void Core::sensApplyMode(uint8_t mode) { SEN_CFG.mode = mode ? 1 : 0; }
void Core::sensHandleTrigger() {
  // Set a latch for your main loop to act on, then clear after sending
  SEN_CFG.forceReport = 1;
}

// Store ICM-pushed topology JSON
void Core::sensStoreTopologyJSON(const char* json, size_t len) {
  if (!json) { SEN_CFG.topo_len = 0; return; }
  if (len > (SENS_TOPO_JSON_MAX - 1)) len = SENS_TOPO_JSON_MAX - 1;
  memcpy(SEN_CFG.topo_json, json, len);
  SEN_CFG.topo_json[len] = '\0';
  SEN_CFG.topo_len = len;
}

bool Core::sensIsPaired() const     { return _cfg ? _cfg->GetBool(SENS_PAIRED_KEY,  SENS_PAIRED_DEF)  : SENS_PAIRED_DEF; }
bool Core::sensInPairingMode() const{ return _cfg ? _cfg->GetBool(SENS_PAIRING_KEY, SENS_PAIRING_DEF) : SENS_PAIRING_DEF; }

esp_err_t Core::sensSysPing(uint32_t waitAckMs) {
  if (!_cfg) return ESP_ERR_INVALID_STATE;
  char tok[NOW_TOKEN_HEX_LEN]; if (!tok32_from_cfg(_cfg, tok)) return ESP_ERR_INVALID_STATE;
  uint8_t icmMac[6]; if (!icm_mac_from_cfg(_cfg, icmMac)) return ESP_ERR_INVALID_ARG;
  NowMsgHdr h; Core::fillHeader(h, NOW_DOM_SYS, SYS_PING, tok);
  return send(icmMac, &h, sizeof(h), waitAckMs);
}

// SENSOR dispatcher
/*static*/ void Core::sensRecvCallback(const uint8_t* mac, const uint8_t* data, int len) {
  if (!s_self || !mac || !data || len < (int)sizeof(NowMsgHdr)) return;
  const NowMsgHdr& h = *reinterpret_cast<const NowMsgHdr*>(data);
  if (h.ver != NOW_HDR_VER) return;

  const int blen      = len - (int)sizeof(NowMsgHdr);
  const uint8_t* body = data + sizeof(NowMsgHdr);

  const bool paired  = s_self->sensIsPaired();
  const bool pairing = s_self->sensInPairingMode();

  // Pairing (no token required before commit)
  if (h.dom == NOW_DOM_PR) {
    switch (h.op) {
      case PR_ASSIGN: {
        if (blen < (int)sizeof(PrAssignPayload)) return;
        const PrAssignPayload* p = reinterpret_cast<const PrAssignPayload*>(body);
        if (!Core::macEq(p->icm_mac, mac)) return;
        if (s_self->nodeApplyPairing(p->icm_mac, p->channel, p->token_hex)) {
          if (s_self->_cfg) {
            s_self->_cfg->PutBool(SENS_PAIRING_KEY, true);
            s_self->_cfg->PutBool(SENS_PAIRED_KEY,  false);
          }
          s_self->addPeer(p->icm_mac, p->channel, /*encrypt*/false, /*lmk*/nullptr);
        }
      } break;
      case PR_COMMIT:
        if (s_self->_cfg) { s_self->_cfg->PutBool(SENS_PAIRED_KEY, true); s_self->_cfg->PutBool(SENS_PAIRING_KEY, false); }
        break;
      case PR_REKEY: {
        if (blen < (int)sizeof(PrAssignPayload)) return;
        const PrAssignPayload* p = reinterpret_cast<const PrAssignPayload*>(body);
        if (!Core::macEq(p->icm_mac, mac)) return;
        if (s_self->nodeApplyPairing(p->icm_mac, p->channel, p->token_hex)) {
          if (s_self->_cfg) s_self->_cfg->PutBool(SENS_PAIRED_KEY, true);
        }
      } break;
      case PR_UNPAIR:
        if (s_self->nodeClearPairing() && s_self->_cfg) {
          s_self->_cfg->PutBool(SENS_PAIRED_KEY,  SENS_PAIRED_DEF);
          s_self->_cfg->PutBool(SENS_PAIRING_KEY, SENS_PAIRING_DEF);
        }
        break;
      default: break;
    }
    return;
  }

  // Non-PR: require token when paired and not actively pairing
  if (paired && !pairing) {
    if (!s_self->verifyIncomingForLocal(h)) return;
  }

  switch (h.dom) {
    case NOW_DOM_SYS:
      if (h.op == SYS_MODE_SET && blen >= (int)sizeof(SysModePayload)) {
        const auto* p = reinterpret_cast<const SysModePayload*>(body);
        s_self->sensApplyMode(p->mode);
      } else if (h.op == SYS_PING) {
        // Optional: echo or light indicator
      }
      break;

    case NOW_DOM_SEN:
      if (h.op == SEN_TRIG) {
        s_self->sensHandleTrigger(); // main loop should call sensSendReport() then clear
      }
      break;

    case NOW_DOM_TOP:
      if (h.op == TOP_PUSH_SEN_JSON) {
        s_self->sensStoreTopologyJSON((const char*)body, (size_t)blen);
      }
      break;

    default: break;
  }
}

#endif // NVS_ROLE_SENS

// ========================================================================
//                                 RELAY
// ========================================================================
#ifdef NVS_ROLE_RELAY

esp_err_t Core::relaySendReport(uint32_t waitAckMs) {
  if (!_cfg) return ESP_ERR_INVALID_STATE;

  uint8_t icmMac[6];
  if (!icm_mac_from_cfg(_cfg, icmMac)) return ESP_ERR_INVALID_ARG;

  char myTok[NOW_TOKEN_HEX_LEN];
  if (!tok32_from_cfg(_cfg, myTok)) return ESP_ERR_INVALID_STATE;

  NowMsgHdr h; Core::fillHeader(h, NOW_DOM_REL, REL_REP, myTok);

  RelReportPayload pl{};
  pl.temp_c_x100 = REL_LIVE.temp_c_x100;

  uint8_t buf[sizeof(NowMsgHdr) + sizeof(RelReportPayload)];
  memcpy(buf, &h, sizeof(h));
  memcpy(buf + sizeof(h), &pl, sizeof(pl));
  return send(icmMac, buf, sizeof(buf), waitAckMs);
}

void Core::relayApplySet(uint8_t ch, uint8_t on) {
  // Hook hardware: set LEFT/RIGHT immediately
  if (ch & REL_CH_LEFT)  REL_CFG.chL_on = on ? 1 : 0;
  if (ch & REL_CH_RIGHT) REL_CFG.chR_on = on ? 1 : 0;
}
void Core::relayApplyOnFor(uint8_t chMask, uint16_t on_ms) {
  // Hook hardware: pulse channels for on_ms; record last requested
  if (chMask & REL_CH_LEFT)  REL_CFG.chL_on = 1;
  if (chMask & REL_CH_RIGHT) REL_CFG.chR_on = 1;
  (void)on_ms;
}
void Core::relayApplySchedule(uint32_t t0_ms, uint16_t l_on_at, uint16_t l_off_at,
                              uint16_t r_on_at, uint16_t r_off_at) {
  REL_CFG.t0_ms   = t0_ms;
  REL_CFG.l_on_at = l_on_at; REL_CFG.l_off_at = l_off_at;
  REL_CFG.r_on_at = r_on_at; REL_CFG.r_off_at = r_off_at;
}
void Core::relayApplyMode(uint8_t mode) { REL_CFG.mode = mode ? 1 : 0; }

void Core::relayStoreTopologyJSON(const char* json, size_t len) {
  if (!json) { REL_CFG.topo_len = 0; return; }
  if (len > (RELAY_TOPO_JSON_MAX - 1)) len = RELAY_TOPO_JSON_MAX - 1;
  memcpy(REL_CFG.topo_json, json, len);
  REL_CFG.topo_json[len] = '\0';
  REL_CFG.topo_len = len;
}

bool Core::relayIsPaired() const      { return _cfg ? _cfg->GetBool(REL_PAIRED_KEY,  REL_PAIRED_DEF)  : REL_PAIRED_DEF; }
bool Core::relayInPairingMode() const { return _cfg ? _cfg->GetBool(REL_PAIRING_KEY, REL_PAIRING_DEF) : REL_PAIRING_DEF; }

esp_err_t Core::relaySysPing(uint32_t waitAckMs) {
  if (!_cfg) return ESP_ERR_INVALID_STATE;
  char tok[NOW_TOKEN_HEX_LEN]; if (!tok32_from_cfg(_cfg, tok)) return ESP_ERR_INVALID_STATE;
  uint8_t icmMac[6]; if (!icm_mac_from_cfg(_cfg, icmMac)) return ESP_ERR_INVALID_ARG;
  NowMsgHdr h; Core::fillHeader(h, NOW_DOM_SYS, SYS_PING, tok);
  return send(icmMac, &h, sizeof(h), waitAckMs);
}

// RELAY dispatcher
/*static*/ void Core::relayRecvCallback(const uint8_t* mac, const uint8_t* data, int len) {
  if (!s_self || !mac || !data || len < (int)sizeof(NowMsgHdr)) return;
  const NowMsgHdr& h = *reinterpret_cast<const NowMsgHdr*>(data);
  if (h.ver != NOW_HDR_VER) return;

  const int blen      = len - (int)sizeof(NowMsgHdr);
  const uint8_t* body = data + sizeof(NowMsgHdr);

  const bool paired  = s_self->relayIsPaired();
  const bool pairing = s_self->relayInPairingMode();

  // Pairing
  if (h.dom == NOW_DOM_PR) {
    switch (h.op) {
      case PR_ASSIGN: {
        if (blen < (int)sizeof(PrAssignPayload)) return;
        const PrAssignPayload* p = reinterpret_cast<const PrAssignPayload*>(body);
        if (!Core::macEq(p->icm_mac, mac)) return;
        if (s_self->nodeApplyPairing(p->icm_mac, p->channel, p->token_hex)) {
          if (s_self->_cfg) {
            s_self->_cfg->PutBool(REL_PAIRING_KEY, true);
            s_self->_cfg->PutBool(REL_PAIRED_KEY,  false);
          }
          s_self->addPeer(p->icm_mac, p->channel, /*encrypt*/false, /*lmk*/nullptr);
        }
      } break;
      case PR_COMMIT:
        if (s_self->_cfg) { s_self->_cfg->PutBool(REL_PAIRED_KEY, true); s_self->_cfg->PutBool(REL_PAIRING_KEY, false); }
        break;
      case PR_REKEY: {
        if (blen < (int)sizeof(PrAssignPayload)) return;
        const PrAssignPayload* p = reinterpret_cast<const PrAssignPayload*>(body);
        if (!Core::macEq(p->icm_mac, mac)) return;
        if (s_self->nodeApplyPairing(p->icm_mac, p->channel, p->token_hex)) {
          if (s_self->_cfg) s_self->_cfg->PutBool(REL_PAIRED_KEY, true);
        }
      } break;
      case PR_UNPAIR:
        if (s_self->nodeClearPairing() && s_self->_cfg) {
          s_self->_cfg->PutBool(REL_PAIRED_KEY,  REL_PAIRED_DEF);
          s_self->_cfg->PutBool(REL_PAIRING_KEY, REL_PAIRING_DEF);
        }
        break;
      default: break;
    }
    return;
  }

  // Non-PR: require token when paired and not pairing
  if (paired && !pairing) {
    if (!s_self->verifyIncomingForLocal(h)) return;
  }

  switch (h.dom) {
    case NOW_DOM_SYS:
      if (h.op == SYS_MODE_SET && blen >= (int)sizeof(SysModePayload)) {
        const auto* p = reinterpret_cast<const SysModePayload*>(body);
        s_self->relayApplyMode(p->mode);
      } else if (h.op == SYS_PING) {
        // Optional: echo indicator
      }
      break;

    case NOW_DOM_REL:
      switch (h.op) {
        case REL_SET:
          if (blen >= (int)sizeof(RelSetPayload)) {
            const auto* p = reinterpret_cast<const RelSetPayload*>(body);
            s_self->relayApplySet(p->ch, p->on);
          }
          break;
        case REL_ON_FOR:
          if (blen >= (int)sizeof(RelOnForPayload)) {
            const auto* p = reinterpret_cast<const RelOnForPayload*>(body);
            s_self->relayApplyOnFor(p->chMask, p->on_ms);
          }
          break;
        case REL_SCHED:
          if (blen >= (int)sizeof(RelSchedPayload)) {
            const auto* p = reinterpret_cast<const RelSchedPayload*>(body);
            s_self->relayApplySchedule(p->t0_ms, p->l_on_at, p->l_off_at, p->r_on_at, p->r_off_at);
          }
          break;
        case REL_QRY:
          s_self->relaySendReport(30);
          break;
        default: break;
      }
      break;

    case NOW_DOM_TOP:
      if (h.op == TOP_PUSH_RLY_JSON) {
        s_self->relayStoreTopologyJSON((const char*)body, (size_t)blen);
      }
      break;

    default: break;
  }
}

#endif // NVS_ROLE_RELAY

} // namespace NwCore
