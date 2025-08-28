/**************************************************************
 *  Project     : EasyDriveWay - Power Supply Module (PSM)
 *  File        : PSMEspNowManager.cpp  (FIXED to align with ICM CommandAPI)
 **************************************************************/
#include "PSMEspNowManager.h"

// ---- Small logging helpers ----
#define LOGI(code, fmt, ...) do{ if(_log) _log->eventf(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_INFO,  (code), "[PSM] " fmt, ##__VA_ARGS__);}while(0)
#define LOGW(code, fmt, ...) do{ if(_log) _log->eventf(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_WARN,  (code), "[PSM] " fmt, ##__VA_ARGS__);}while(0)
#define LOGE(code, fmt, ...) do{ if(_log) _log->eventf(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_ERROR, (code), "[PSM] " fmt, ##__VA_ARGS__);}while(0)

PSMEspNowManager* PSMEspNowManager::s_inst = nullptr;

PSMEspNowManager::PSMEspNowManager(ConfigManager* cfg, ICMLogFS* log, RTCManager* rtc)
: _cfg(cfg), _log(log), _rtc(rtc) {
  s_inst = this;
}

bool PSMEspNowManager::begin(uint8_t channelDefault, const char* pmk16) {
  // Load persisted channel
  uint8_t ch = (uint8_t)_cfg->GetInt(keyCh().c_str(), (int)channelDefault);
  if (ch < 1 || ch > 13) ch = 1;
  _channel = ch;

  // Load token & master mac (if any)
  String tokHex = _cfg->GetString(keyTok().c_str(), "");
  if (tokHex.length() >= 32) hexToBytes(tokHex, _token16, 16);
  String mm = _cfg->GetString(keyMac().c_str(), "");
  macStrToBytes(mm, _masterMac);

  // Bring Wi-Fi up and fix channel
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  // ESP-NOW init
  if (esp_now_init() != ESP_OK) { LOGE(2001, "esp_now_init failed"); return false; }
  if (pmk16 && strlen(pmk16)==16) { strncpy(_pmk, pmk16, 16); _pmk[16]=0; esp_now_set_pmk((uint8_t*)_pmk); }
  esp_now_register_recv_cb(&PSMEspNowManager::onRecvThunk);
  esp_now_register_send_cb(&PSMEspNowManager::onSentThunk);

  // If we already know master mac, add as peer
  if (_masterMac[0] || _masterMac[1] || _masterMac[2] || _masterMac[3] || _masterMac[4] || _masterMac[5]) {
    esp_now_peer_info_t p{};
    memcpy(p.peer_addr, _masterMac, 6);
    p.encrypt = (_pmk[0] != 0);
    p.ifidx = WIFI_IF_STA;
    p.channel = _channel;
    esp_now_add_peer(&p); // best effort
  }

  _started = true;
  LOGI(2000, "Started on ch=%u token=%s master=%s", _channel, bytesToHex(_token16,16).c_str(), masterMacStr().c_str());
  return true;
}

void PSMEspNowManager::end() {
  if (!_started) return;
  esp_now_deinit();
  _started = false;
}

void PSMEspNowManager::poll() {
  // no periodic work yet; heartbeats could be sent from here
}

String PSMEspNowManager::masterMacStr() const { return macBytesToStr(_masterMac); }
String PSMEspNowManager::myMacStr() const {  uint8_t m[6]; esp_read_mac(m, ESP_MAC_WIFI_STA); return macBytesToStr(m);}

bool PSMEspNowManager::sendHeartbeat() {
  // Empty heartbeat – just header only
  return sendToMaster(CmdDomain::SYS, SYS_HB, nullptr, 0, 0, false);
}

bool PSMEspNowManager::sendCaps(uint8_t maj, uint8_t min, bool hasTemp, bool hasCharger, uint32_t features) {
  PsmCapsPayload c{}; c.ver_major=maj; c.ver_minor=min; c.has_temp=hasTemp?1:0; c.has_charger=hasCharger?1:0; c.features=features;
  return sendToMaster(CmdDomain::SYS, SYS_CAPS, &c, sizeof(c), 0, false);
}

// ==================== Callbacks ====================
void PSMEspNowManager::onRecvThunk(const uint8_t *mac, const uint8_t *data, int len) {
  if (s_inst) s_inst->onRecv(mac,data,len);
}
void PSMEspNowManager::onSentThunk(const uint8_t *mac, esp_now_send_status_t status) {
  if (s_inst) s_inst->onSent(mac,status);
}

void PSMEspNowManager::onSent(const uint8_t *mac, esp_now_send_status_t status) {
  (void)mac;
  if (status != ESP_NOW_SEND_SUCCESS) {
    LOGW(2101, "Send status=%d", (int)status);
  }
}

bool PSMEspNowManager::tokenMatches(const IcmMsgHdr& h) const {
  // If we don't have a token yet, accept only SYS_INIT and SYS_PING
  bool have = hasToken();
  if (!have) return (h.dom==(uint8_t)CmdDomain::SYS) && (h.op==SYS_INIT || h.op==SYS_PING);
  return (memcmp(h.tok16, _token16, 16) == 0);
}

void PSMEspNowManager::onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < (int)sizeof(IcmMsgHdr)) return;
  const IcmMsgHdr* h = (const IcmMsgHdr*)data;
  const uint8_t* payload = data + sizeof(IcmMsgHdr);
  int plen = len - (int)sizeof(IcmMsgHdr);

  // Basic validation
  if (h->ver != 1) { LOGW(2200, "Drop: bad ver=%u", h->ver); return; }
  if (!tokenMatches(*h)) { LOGW(2201, "Token mismatch dom=%u op=%u from %s", h->dom, h->op, macBytesToStr(mac).c_str()); return; }

  // Learn/remember master MAC on first valid packet
  if (!( _masterMac[0]|_masterMac[1]|_masterMac[2]|_masterMac[3]|_masterMac[4]|_masterMac[5] )) {
    memcpy(_masterMac, mac, 6);
    _cfg->PutString(keyMac().c_str(), masterMacStr());
    esp_now_peer_info_t p{}; memcpy(p.peer_addr, _masterMac, 6); p.encrypt = (_pmk[0]!=0); p.ifidx=WIFI_IF_STA; p.channel=_channel;
    esp_now_add_peer(&p);
    LOGI(2202, "Master learned: %s", masterMacStr().c_str());
  }

  // ---- Time sync from header (only if skew >= ~120s) ----
  if (_rtc) {
    const uint32_t ts_icm = h->ts;                                // UNIX seconds from ICM
    const uint32_t ts_psm = _rtc->getUnixTime();                  // local UNIX seconds
    // sanity floor to avoid epoch garbage; 2023-01-01 = 1672531200
    const uint32_t sane_floor = 1672531200UL;
    const uint32_t skew_thresh = 120UL;                           // 2 minutes
    if (ts_icm >= sane_floor) {
      int32_t skew = (int32_t)ts_icm - (int32_t)ts_psm;
      if (skew >= (int32_t)skew_thresh || skew <= -(int32_t)skew_thresh) {
        _rtc->setUnixTime(ts_icm);
        LOGI(2210, "RTC sync: ICM=%u PSM=%u skew=%ld -> set", (unsigned)ts_icm, (unsigned)ts_psm, (long)skew);
      }
    }
  }

  // Optional ACK-on-receive (app-level). We'll ACK after processing.
  bool ackReq = (h->flags & HDR_FLAG_ACKREQ) != 0;
  uint8_t ackCode = 0; // 0=OK; other codes impl-defined

  switch ((CmdDomain)h->dom) {
    case CmdDomain::SYS: {
      switch (h->op) {
        case SYS_INIT: {
          // Expect PsmSysInitPayload (token + optional channel)
          if (plen >= (int)sizeof(PsmSysInitPayload)) {
            const PsmSysInitPayload* si = (const PsmSysInitPayload*)payload;
            memcpy(_token16, si->token16, 16);
            _cfg->PutString(keyTok().c_str(), bytesToHex(_token16,16));
            if (si->channel>=1 && si->channel<=13 && si->channel != _channel) {
              applyChannel(si->channel, /*persist=*/true);
            }
            LOGI(2300, "SYS_INIT token=%s ch=%u", bytesToHex(_token16,16).c_str(), (unsigned)si->channel);
          } else if (plen >= 16) {
            memcpy(_token16, payload, 16);
            _cfg->PutString(keyTok().c_str(), bytesToHex(_token16,16));
            LOGI(2301, "SYS_INIT token(legacy)=%s", bytesToHex(_token16,16).c_str());
          } else {
            ackCode = 1; // bad payload
            LOGW(2302, "SYS_INIT too short (%d)", plen);
          }
          break;
        }
        case SYS_SET_CH: {
          if (plen >= (int)sizeof(SysSetChPayload)) {
            const SysSetChPayload* sc = (const SysSetChPayload*)payload;
            // For simplicity, if switch time is in the past or 0, switch now.
            uint32_t now = _rtc ? _rtc->getUnixTime() : (uint32_t)time(nullptr);
            if (sc->switchover_ts==0 || sc->switchover_ts <= now) {
              applyChannel(sc->new_ch, /*persist=*/true);
              LOGI(2303, "Channel changed immediately to %u", sc->new_ch);
            } else {
              // Best-effort: if within window, also switch.
              uint32_t delta = (sc->switchover_ts > now) ? (sc->switchover_ts - now) : 0;
              if (delta <= 3) { applyChannel(sc->new_ch, true); LOGI(2304,"Channel switched within small delta");}
              else { LOGW(2305,"Deferred channel switch requested for ts=%u (not scheduled here)", sc->switchover_ts); }
            }
          } else {
            ackCode = 1;
            LOGW(2306, "SYS_SET_CH short=%d", plen);
          }
          break;
        }
        case SYS_PING: {
          // Nothing to do
          break;
        }
        default: break;
      }
      break;
    }
    case CmdDomain::POWER: {
      switch (h->op) {
        case PWR_SET: {
          bool wantOn = (plen>=1) ? (payload[0]!=0) : false;
          bool ok = _onSetPower ? _onSetPower(wantOn) : false;
          ackCode = ok ? 0 : 2;
          LOGI(2400, "PWR_SET on=%d -> %s", (int)wantOn, ok?"OK":"FAIL");
          break;
        }
        case PWR_GET: {
          (void)sendPowerStatus(h->ctr);
          break;
        }
        case PWR_CLRF: {
          bool ok = _onClearFault ? _onClearFault() : true;
          ackCode = ok ? 0 : 2;
          LOGI(2401, "PWR_CLRF -> %s", ok?"OK":"FAIL");
          break;
        }
        case PWR_REQSDN: {
          bool ok = _onReqShutdown ? _onReqShutdown() : true;
          ackCode = ok ? 0 : 2;
          LOGI(2402, "PWR_REQSDN -> %s", ok?"OK":"FAIL");
          break;
        }
        case PWR_GET_TEMP: {
          (void)sendTempReply(h->ctr);
          break;
        }
        default:
          LOGW(2410, "Unknown POWER op=0x%02X", (unsigned)h->op);
          break;
      }
      break;
    }
    default:
      LOGW(2250, "Unknown domain dom=0x%02X op=0x%02X", (unsigned)h->dom, (unsigned)h->op);
      break;
  }

  if (ackReq) sendAck(h->ctr, ackCode);
}

bool PSMEspNowManager::sendAck(uint16_t ctr, uint8_t code) {
  SysAckPayload a{}; a.ctr = ctr; a.code = code; a.rsv = 0;
  return sendToMaster(CmdDomain::SYS, SYS_ACK, &a, sizeof(a), ctr, false);
}

bool PSMEspNowManager::sendPowerStatus(uint16_t ctr) {
  PowerStatusPayload s{};
  bool ok = _onComposeStatus ? _onComposeStatus(s) : false;
  if (!ok) { s.ok=0; } else { s.ok=1; }
  s.ver = 1;

  // Fill from your ADC/PMIC (user-implemented functions declared in header):
  s.on        = hwIs48VOn();
  s.fault     = readFaultBits();
  s.vbus_mV   = measure48V_mV();
  s.ibus_mA   = measure48V_mA();
  s.vbat_mV   = measureBat_mV();
  s.ibat_mA   = measureBat_mA();
  s.tC_x100   = (int16_t)lrintf(readBoardTempC()*100.0f);
  s.src       = 0;

  return sendToMaster(CmdDomain::POWER, PWR_GET, &s, sizeof(s), ctr, false);
}

bool PSMEspNowManager::sendTempReply(uint16_t ctr) {
  float tC = NAN;
  bool have = _onReadTemp ? _onReadTemp(tC) : false;
  TempPayload tp = makeTempPayload(have ? tC : 0.0f, /*raw*/0, /*src*/0, /*ok*/have);
  return sendToMaster(CmdDomain::POWER, PWR_GET_TEMP, &tp, sizeof(tp), ctr, false);
}

bool PSMEspNowManager::sendToMaster(CmdDomain dom, uint8_t op, const void* payload, size_t len, uint16_t ctrEcho, bool ackReq) {
  if (!_started) return false;
  if (!(_masterMac[0]|_masterMac[1]|_masterMac[2]|_masterMac[3]|_masterMac[4]|_masterMac[5])) return false;

  uint8_t buf[sizeof(IcmMsgHdr) + 64]; // our payloads are small
  if (len > 64) return false;

  IcmMsgHdr* h = (IcmMsgHdr*)buf;
  h->ver = 1;
  h->dom = (uint8_t)dom;
  h->op  = op;
  h->flags = ackReq ? HDR_FLAG_ACKREQ : 0;
  h->ts = _rtc ? _rtc->getUnixTime() : (uint32_t)time(nullptr);
  h->ctr = ctrEcho;  // echo master's ctr when replying
  memcpy(h->tok16, _token16, 16);
  if (payload && len) memcpy(buf + sizeof(IcmMsgHdr), payload, len);

  esp_err_t e = esp_now_send(_masterMac, buf, sizeof(IcmMsgHdr) + len);
  if (e != ESP_OK) { LOGW(2601, "send dom=%u op=0x%02X err=%d", (unsigned)h->dom, (unsigned)h->op, (int)e); return false; }
  return true;
}

void PSMEspNowManager::applyChannel(uint8_t ch, bool persist) {
  if (ch < 1 || ch > 13) return;
  if (ch == _channel) return;
  _channel = ch;
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  // re-add peer with new channel
  if (_masterMac[0]|_masterMac[1]|_masterMac[2]|_masterMac[3]|_masterMac[4]|_masterMac[5]) {
    esp_now_del_peer(_masterMac);
    esp_now_peer_info_t p{}; memcpy(p.peer_addr,_masterMac,6); p.encrypt = (_pmk[0]!=0); p.ifidx=WIFI_IF_STA; p.channel=_channel;
    esp_now_add_peer(&p);
  }
  if (persist) _cfg->PutInt(keyCh().c_str(), (int)_channel);
}

String PSMEspNowManager::macBytesToStr(const uint8_t mac[6]) {
  char b[18];
  sprintf(b, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(b);
}
bool PSMEspNowManager::macStrToBytes(const String& s, uint8_t mac[6]) {
  if (s.length() < 17) return false;
  unsigned v[6];
  if (sscanf(s.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &v[0],&v[1],&v[2],&v[3],&v[4],&v[5])!=6) return false;
  for (int i=0;i<6;++i) mac[i]=(uint8_t)v[i];
  return true;
}
String PSMEspNowManager::bytesToHex(const uint8_t* b, size_t n) {
  String s; s.reserve(n*2);
  static const char* H="0123456789ABCDEF";
  for(size_t i=0;i<n;++i){ s+=H[(b[i]>>4)&0xF]; s+=H[b[i]&0xF]; }
  return s;
}
bool PSMEspNowManager::hexToBytes(const String& hex, uint8_t* out, size_t n) {
  if (hex.length() < (int)(n*2)) return false;
  auto hexVal=[](char c)->int{ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; };
  for (size_t i=0;i<n;i++) {
    int hi = hexVal(hex[2*i]); int lo = hexVal(hex[2*i+1]);
    if (hi<0||lo<0) return false; out[i]=(uint8_t)((hi<<4)|lo);
  }
  return true;
}
