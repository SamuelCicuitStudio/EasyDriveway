#include "SensorEspNowManager.h"

// Small helper: 16-byte token -> 32-hex string
static String tok16ToHex_(const uint8_t tok[16]) {
  static const char* hexd = "0123456789ABCDEF";
  char out[33]; out[32] = 0;
  for (int i=0;i<16;++i) {
    out[i*2+0] = hexd[(tok[i] >> 4) & 0xF];
    out[i*2+1] = hexd[(tok[i]     ) & 0xF];
  }
  return String(out);
}
static inline String tok16Hex_(const uint8_t tok[16]) { return tok16ToHex_(tok); }
static inline String tok16toHex(const uint8_t tok[16]) { return tok16ToHex_(tok); }

// ================== static thunks ==================
static SensorEspNowManager* s_self = nullptr;
void SensorEspNowManager::onRecvThunk(const uint8_t *mac, const uint8_t *data, int len){
  if (s_self) s_self->onRecv_(mac, data, len);
}
void SensorEspNowManager::onSentThunk(const uint8_t *mac, esp_now_send_status_t status){
  if (s_self) s_self->onSent_(mac, status);
}

// ================== public ==================
bool SensorEspNowManager::begin(uint8_t channel, const char* pmk16){
  channel_ = channel;
  s_self = this;

  WiFi.mode(WIFI_STA);
  // Match ICM channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) return false;
  if (pmk16 && strlen(pmk16) == 16) esp_now_set_pmk((const uint8_t*)pmk16);

  esp_now_register_recv_cb(&SensorEspNowManager::onRecvThunk);
  esp_now_register_send_cb(&SensorEspNowManager::onSentThunk);

  if (haveIcm_) ensurePeer_(icmMac_);
  return true;
}

void SensorEspNowManager::setIcmMac(const uint8_t mac[6]){
  memcpy(icmMac_, mac, 6);
  haveIcm_ = true;
  ensurePeer_(icmMac_);
}

void SensorEspNowManager::setNodeToken16(const uint8_t token16[16]){
  memcpy(token16_, token16, 16);
  haveTok_ = true;
}

void SensorEspNowManager::poll(){
  // Execute scheduled channel switch, if any
  if (chSwitchPending_ && millis() >= chSwitchDueMs_) {
    chSwitchPending_ = false;
    channel_ = chTarget_;
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    // Re-add ICM + all relay peers we know
    ensurePeer_(icmMac_);
    for (auto &rp : relNeg_) ensurePeer_(rp.mac);
    for (auto &rp : relPos_) ensurePeer_(rp.mac);
    // Persist
    persistChannel_(channel_);
  }
}

// Replies to ICM
bool SensorEspNowManager::sendSysAck(uint16_t ctr, uint8_t code){
  SysAckPayload ap{}; ap.ctr = ctr; ap.code = code; ap.rsv = 0;
  return sendToIcm_(CmdDomain::SYS, SYS_ACK, &ap, sizeof(ap), false);
}
bool SensorEspNowManager::sendDayNight(){
  uint8_t is_day = 255;
  makeDayNight_(is_day);
  return sendToIcm_(CmdDomain::SENS, SENS_GET_DAYNIGHT, &is_day, 1, false);
}
bool SensorEspNowManager::sendTfRaw(uint8_t which){
  TfLunaRawPayload p{};
  if (!makeTfRaw_(which, p)) return false;
  return sendToIcm_(CmdDomain::SENS, SENS_GET_TFRAW, &p, sizeof(p), false);
}
bool SensorEspNowManager::sendEnv(){
  SensorEnvPayload p{};
  if (!makeEnv_(p)) return false;
  return sendToIcm_(CmdDomain::SENS, SENS_GET_ENV, &p, sizeof(p), false);
}

// Relay send helpers
bool SensorEspNowManager::sendRelayRawByPos(int8_t relPos, CmdDomain dom, uint8_t op, const void* body, size_t blen, bool requireAck){
  RelayPeer rp;
  if (!findRelayByPos_(relPos, rp)) return false;
  ensurePeer_(rp.mac);
  return sendToMacWithToken_(rp.mac, rp.tok16, dom, op, body, blen, requireAck);
}
bool SensorEspNowManager::sendRelayRawByIdx(uint8_t relayIdx, CmdDomain dom, uint8_t op, const void* body, size_t blen, bool requireAck){
  RelayPeer rp;
  if (!findRelayByIdx_(relayIdx, rp)) return false;
  ensurePeer_(rp.mac);
  return sendToMacWithToken_(rp.mac, rp.tok16, dom, op, body, blen, requireAck);
}

// ================== utils ==================
String SensorEspNowManager::macBytesToStr(const uint8_t mac[6]){
  char b[20]; snprintf(b,sizeof(b),"%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  return String(b);
}

// ================== internals: send ==================
bool SensorEspNowManager::sendToIcm_(CmdDomain dom, uint8_t op, const void* body, size_t blen, bool ackReq){
  if (!haveIcm_) return false;
  return sendToMacWithToken_(icmMac_, token16_, dom, op, body, blen, ackReq);
}

bool SensorEspNowManager::sendToMacWithToken_(const uint8_t mac[6], const uint8_t tok16[16],
                                              CmdDomain dom, uint8_t op, const void* body, size_t blen, bool ackReq){
  uint8_t frame[sizeof(IcmMsgHdr) + 224] = {0};
  IcmMsgHdr* h = (IcmMsgHdr*)frame;
  fillHdr_(*h, dom, op, ackReq, tok16);
  if (blen && body) memcpy(frame + sizeof(IcmMsgHdr), body, blen);
  size_t len = sizeof(IcmMsgHdr) + blen;
  return esp_now_send(mac, frame, len) == ESP_OK;
}

void SensorEspNowManager::fillHdr_(IcmMsgHdr& h, CmdDomain dom, uint8_t op, bool ackReq, const uint8_t tok16[16]){
  h.ver   = 1;
  h.dom   = (uint8_t)dom;
  h.op    = op;
  h.flags = ackReq ? HDR_FLAG_ACKREQ : 0;
  h.ts    = unixNow_();                  // <<< use saved RTC time
  h.ctr   = 0;
  memcpy(h.tok16, tok16, 16);            // receiver’s token (ICM/relay)
}

void SensorEspNowManager::ensurePeer_(const uint8_t mac[6]){
  if (!mac) return;
  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, mac, 6);
  p.ifidx = WIFI_IF_STA;
  p.channel = channel_;
  p.encrypt = 0;
  // Re-add in case already present
  esp_now_del_peer(mac);
  esp_now_add_peer(&p);
}

// ================== internals: callbacks ==================
void SensorEspNowManager::onSent_(const uint8_t *mac, esp_now_send_status_t status){
  (void)mac; (void)status;
}

void SensorEspNowManager::onRecv_(const uint8_t *mac, const uint8_t *data, int len){
  if (len < (int)sizeof(IcmMsgHdr)) return;
  const IcmMsgHdr* h   = (const IcmMsgHdr*)data;
  const uint8_t*   pl  = data + sizeof(IcmMsgHdr);
  const int        plen= len  - (int)sizeof(IcmMsgHdr);

  // Learn ICM MAC on first packet if not set
  if (!haveIcm_) { memcpy(icmMac_, mac, 6); haveIcm_ = true; ensurePeer_(icmMac_); }

  // Header must carry the RECEIVER token; for frames ICM→sensor that token should be THIS sensor's token
  if (haveTok_ && memcmp(h->tok16, token16_, 16) != 0) {
    return; // token mismatch, drop
  }

  // If ACK requested, send ASAP
  if (h->flags & HDR_FLAG_ACKREQ) sendSysAck(h->ctr, 0);

  switch ((CmdDomain)h->dom) {
    case CmdDomain::SYS:  handleSys_(*h, pl, plen);  break;
    case CmdDomain::SENS: handleSens_(*h, pl, plen); break;
    case CmdDomain::TOPO: handleTopo_(*h, pl, plen); break;
    default: break;
  }
}

// ================== handlers ==================
void SensorEspNowManager::handleSys_(const IcmMsgHdr& h, const uint8_t* payload, int plen){
  if (h.op == SYS_PING) {
    // 1) Sync local RTC from ICM's authoritative UNIX time in header (if provided)
    if (rtc_ && h.ts) {
      rtc_->setUnixTime((unsigned long)h.ts);
    }

    // 2) Anti ping-pong: if this is our own ping echo (nonce matches), consume it and DO NOT echo back
    if (payload && plen == (int)sizeof(uint32_t) && pendingPing_ != 0) {
      uint32_t n = 0;
      memcpy(&n, payload, sizeof(n));
      if (n == pendingPing_) {
        lastPingRttMs_ = millis() - pingSentMs_;
        pendingPing_   = 0;
        return; // swallow reply; no echo
      }
    }

    // Otherwise, master-initiated ping → echo back as-is (no ACK requested)
    sendToIcm_(CmdDomain::SYS, SYS_PING, payload, plen, false);
    return;
  }

  if (h.op == SYS_SET_CH && plen >= (int)sizeof(SysSetChPayload)) {
    const SysSetChPayload* sp = (const SysSetChPayload*)payload;
    uint8_t newCh = sp->new_ch;
    if (newCh >= 1 && newCh <= 13) {
      // 3) Use our saved epoch (RTC if available) as the reference time
      const uint32_t now_epoch = unixNow_();  // helper returns rtc_->getUnixTime() or millis()/1000 fallback

      // If switchover_ts is in the future vs OUR clock, schedule; else switch immediately.
      if (sp->switchover_ts && sp->switchover_ts > now_epoch) {
        const uint32_t delta_s = sp->switchover_ts - now_epoch;
        chTarget_         = newCh;
        chWindowS_        = sp->window_s;
        chSwitchDueMs_    = millis() + delta_s * 1000UL;
        chSwitchPending_  = true;
      } else {
        chSwitchPending_ = false;
        channel_ = newCh;
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
        // Re-add ICM + all relay peers we know
        ensurePeer_(icmMac_);
        for (auto &rp : relNeg_) ensurePeer_(rp.mac);
        for (auto &rp : relPos_) ensurePeer_(rp.mac);
        // Persist
        persistChannel_(channel_);
      }
    }
    return;
  }

  if (h.op == SYS_MODE && plen >= (int)sizeof(SysModePayload)) {
    const SysModePayload* mp = (const SysModePayload*)payload;
    cfg_->PutInt(ESPNOW_MD_KEY, mp->mode); // 0 auto / 1 manual
    return;
  }
}



void SensorEspNowManager::handleSens_(const IcmMsgHdr& h, const uint8_t* payload, int plen){
  (void)payload; (void)plen;
  switch (h.op) {
    case SENS_GET:           sendEnv(); break;
    case SENS_SET_MODE:      if (plen>=1) cfg_->PutInt("SENS_MODE", payload[0]); break;
    case SENS_TRIG:          /* app-specific hook */ break;
    case SENS_GET_DAYNIGHT:  sendDayNight(); break;
    case SENS_GET_TFRAW:     { uint8_t which = (plen>=1)? payload[0] : 0; sendTfRaw(which); } break;
    case SENS_GET_ENV:       sendEnv(); break;
    default: break;
  }
}

void SensorEspNowManager::handleTopo_(const IcmMsgHdr& h, const uint8_t* payload, int plen){
  (void)h;
  if (plen <= 0 || payload == nullptr) return;
  // Parse plus persist raw blob
  parseAndMirrorZcSensor_(payload, plen);
  //cfg_->PutBlob(NVS_ZC_SENSOR_BLOB, payload, (size_t)plen);
  // Additionally persist human-readable MAC strings for diagnostics
  saveTopoMacStrings_();
}

// ================== payload builders ==================
bool SensorEspNowManager::makeDayNight_(uint8_t &is_day_out){
  is_day_out = 255;
  if (!als_) return false;
  float lux = 0;
  if (!als_->read(lux)) return false;

  int t0 = cfg_->GetInt(ALS_T1_LUX_KEY, ALS_T1_LUX_DEFAULT); // up-cross
  int t1 = cfg_->GetInt(ALS_T0_LUX_KEY, ALS_T0_LUX_DEFAULT); // down-cross
  static uint8_t state = 255;
  uint8_t s = state;
  if (s == 255) s = (lux > t0) ? 1 : 0;
  else {
    if      (lux > t0) s = 1;
    else if (lux < t1) s = 0;
  }
  state = s;
  is_day_out = s;
  return true;
}

bool SensorEspNowManager::makeTfRaw_(uint8_t which, TfLunaRawPayload& out){
  memset(&out, 0, sizeof(out));
  out.ver = 1;
  out.which = which;
  out.t_ms = millis();
  if (!tfFetch_) return false;
  TfSample a{0,0,false}, b{0,0,false}; uint16_t rate=0;
  if (!tfFetch_(which, a, b, rate)) return false;
  out.rate_hz  = rate;
  out.distA_mm = a.dist_mm; out.ampA = a.amp; out.okA = a.ok ? 1 : 0;
  out.distB_mm = b.dist_mm; out.ampB = b.amp; out.okB = b.ok ? 1 : 0;
  return true;
}

bool SensorEspNowManager::makeEnv_(SensorEnvPayload& out){
  memset(&out, 0, sizeof(out));
  out.ver = 1;
  if (bme_) {
    float t=0, rh=0, pPa=0;
    if (bme_->read(t, rh, pPa)) {
      out.tC_x100 = (int16_t)roundf(t * 100.0f); out.okT = 1;
      out.rh_x100 = (uint16_t)roundf(rh * 100.0f); out.okH = 1;
      out.p_Pa    = (int32_t)roundf(pPa); out.okP = 1;
    }
  }
  uint8_t dn=255; float lux=0;
  if (als_ && als_->read(lux)) {
    out.lux_x10 = (uint16_t)roundf(lux * 10.0f);
    out.okL = 1;
    if (makeDayNight_(dn)) out.is_day = dn; else out.is_day = 255;
  } else {
    out.is_day = 255;
  }
  return true;
}

// ================== topology helpers ==================
void SensorEspNowManager::clearTopology_(){
  sensIdx_ = 0xFF;
  relNeg_.clear();
  relPos_.clear();
}

void SensorEspNowManager::parseAndMirrorZcSensor_(const uint8_t* payload, int plen){
  clearTopology_();
  if (plen < (int)sizeof(TopoZeroCenteredSensor)) return;
  const TopoZeroCenteredSensor* z = (const TopoZeroCenteredSensor*)payload;

  sensIdx_ = z->sensIdx;
  // Persist neighbor flags, indices, MACs, and tokens
  cfg_->PutInt (NVS_ZC_SENSOR_INDEX, sensIdx_);
  cfg_->PutInt (NVS_ZC_HAS_PREV, z->hasPrev ? 1 : 0);
  if (z->hasPrev) {
    cfg_->PutInt   (NVS_ZC_PREV_INDEX, z->prevSensIdx);
    cfg_->PutString(NVS_ZC_PREV_MAC, macBytesToStr(z->prevSensMac).c_str());
    cfg_->PutString(NVS_ZC_PREV_TOKEN16, tok16ToHex_(z->prevSensTok16).c_str());
  } else {
    cfg_->PutString(NVS_ZC_PREV_MAC, "");
    cfg_->PutString(NVS_ZC_PREV_TOKEN16, "");
  }
  cfg_->PutInt (NVS_ZC_HAS_NEXT, z->hasNext ? 1 : 0);
  if (z->hasNext) {
    cfg_->PutInt   (NVS_ZC_NEXT_INDEX, z->nextSensIdx);
    cfg_->PutString(NVS_ZC_NEXT_MAC, macBytesToStr(z->nextSensMac).c_str());
    cfg_->PutString(NVS_ZC_NEXT_TOKEN16, tok16ToHex_(z->nextSensTok16).c_str());
  } else {
    cfg_->PutString(NVS_ZC_NEXT_MAC, "");
    cfg_->PutString(NVS_ZC_NEXT_TOKEN16, "");
  }

  // Persist neighbor MACs (strings) and flags
  cfg_->PutInt(NVS_ZC_HAS_PREV, z->hasPrev ? 1 : 0);
  if (z->hasPrev) {
    cfg_->PutInt(NVS_ZC_PREV_INDEX, z->prevSensIdx);
    cfg_->PutString(NVS_ZC_PREV_MAC, macBytesToStr(z->prevSensMac).c_str());
  } else {
    cfg_->PutString(NVS_ZC_PREV_MAC, "");
  }
  cfg_->PutInt(NVS_ZC_HAS_NEXT, z->hasNext ? 1 : 0);
  if (z->hasNext) {
    cfg_->PutInt(NVS_ZC_NEXT_INDEX, z->nextSensIdx);
    cfg_->PutString(NVS_ZC_NEXT_MAC, macBytesToStr(z->nextSensMac).c_str());
  } else {
    cfg_->PutString(NVS_ZC_NEXT_MAC, "");
  }


  // the arrays follow the header
  const uint8_t* p = payload + sizeof(TopoZeroCenteredSensor);
  // Read neg list
  for (uint8_t i = 0; i < z->nNeg; ++i) {
    if (p + sizeof(ZcRelEntry) > payload + plen) return;
    const ZcRelEntry* e = (const ZcRelEntry*)p;
    RelayPeer rp; rp.relayIdx = e->relayIdx; rp.relPos = e->relPos;
    memcpy(rp.mac, e->relayMac, 6);
    memcpy(rp.tok16, e->relayTok16, 16);
    relNeg_.push_back(rp);
    ensurePeer_(rp.mac);
    p += sizeof(ZcRelEntry);
  }
  // Read pos list
  for (uint8_t i = 0; i < z->nPos; ++i) {
    if (p + sizeof(ZcRelEntry) > payload + plen) return;
    const ZcRelEntry* e = (const ZcRelEntry*)p;
    RelayPeer rp; rp.relayIdx = e->relayIdx; rp.relPos = e->relPos;
    memcpy(rp.mac, e->relayMac, 6);
    memcpy(rp.tok16, e->relayTok16, 16);
    relPos_.push_back(rp);
    ensurePeer_(rp.mac);
    p += sizeof(ZcRelEntry);
  }

  // Persist human-readable MAC strings for diagnostics
  saveTopoMacStrings_();
}

bool SensorEspNowManager::findRelayByPos_(int8_t relPos, RelayPeer& out) const{
  if (relPos < 0) {
    for (auto &rp : relNeg_) if (rp.relPos == relPos) { out = rp; return true; }
  } else if (relPos > 0) {
    for (auto &rp : relPos_) if (rp.relPos == relPos) { out = rp; return true; }
  }
  return false;
}

bool SensorEspNowManager::findRelayByIdx_(uint8_t relayIdx, RelayPeer& out) const{
  for (auto &rp : relNeg_) if (rp.relayIdx == relayIdx) { out = rp; return true; }
  for (auto &rp : relPos_) if (rp.relayIdx == relayIdx) { out = rp; return true; }
  return false;
}

bool SensorEspNowManager::getRelayByPos(int8_t relPos, RelayPeer& out) const{
  return findRelayByPos_(relPos, out);
}
bool SensorEspNowManager::getRelayByIdx(uint8_t relayIdx, RelayPeer& out) const{
  return findRelayByIdx_(relayIdx, out);
}

// ================== NVS helpers ==================
void SensorEspNowManager::persistChannel_(uint8_t ch){
  cfg_->PutInt(ESPNOW_CH_KEY, ch);
}

// Persist MAC strings for neighbors and relays to NVS

void SensorEspNowManager::saveTopoMacStrings_(){
  // Store sensor index and counts
  cfg_->PutInt(NVS_ZC_SENSOR_INDEX, sensIdx_);
  cfg_->PutInt(NVS_ZC_NEG_COUNT, (int)relNeg_.size());
  cfg_->PutInt(NVS_ZC_POS_COUNT, (int)relPos_.size());

  auto putKV = [&](const char* fmtKey, unsigned val, const String& vstr){
    char key[12];  // enough for "ZRM9999" etc.
    snprintf(key, sizeof(key), fmtKey, val);
    cfg_->PutString(key, vstr.c_str());
  };

  // Negative side: 1..N (relPos = -1,-2,...)
  for (size_t i = 0; i < relNeg_.size(); ++i) {
    const auto& e = relNeg_[i];
    String macStr = macBytesToStr(e.mac);
    String tokStr = tok16Hex_(e.tok16);
    // by side+ordinal
    putKV(NVS_ZC_NEG_MAC_FMT,     (unsigned)(i + 1), macStr);
    putKV(NVS_ZC_NEG_TOKEN16_FMT, (unsigned)(i + 1), tokStr);
    // by global idx
    putKV(NVS_ZC_RELAY_MAC_BYIDX_FMT, (unsigned)e.relayIdx, macStr);
    putKV(NVS_ZC_RELAY_TOK_BYIDX_FMT, (unsigned)e.relayIdx, tokStr);
  }

  // Positive side: 1..N (relPos = +1,+2,...)
  for (size_t i = 0; i < relPos_.size(); ++i) {
    const auto& e = relPos_[i];
    String macStr = macBytesToStr(e.mac);
    String tokStr = tok16Hex_(e.tok16);
    // by side+ordinal
    putKV(NVS_ZC_POS_MAC_FMT,     (unsigned)(i + 1), macStr);
    putKV(NVS_ZC_POS_TOKEN16_FMT, (unsigned)(i + 1), tokStr);
    // by global idx
    putKV(NVS_ZC_RELAY_MAC_BYIDX_FMT, (unsigned)e.relayIdx, macStr);
    putKV(NVS_ZC_RELAY_TOK_BYIDX_FMT, (unsigned)e.relayIdx, tokStr);
  }
}

bool SensorEspNowManager::playWave(uint8_t lane, int8_t dir, uint16_t speed_mmps,
                                   uint16_t spacing_mm, uint16_t on_ms,
                                   uint16_t all_on_ms, uint16_t ttl_ms, bool requireAck){
  // Validate
  if (!hasTopology()) return false;
  if (lane > 1) lane = 0; // default to Left
  if (dir == 0) dir = +1;
  if (speed_mmps == 0) speed_mmps = 1000; // fallback 1 m/s to avoid div-by-zero
  if (spacing_mm == 0) spacing_mm = 1500; // fallback 1.5m typical
  if (on_ms == 0) on_ms = 150;

  // Channel mask from lane
  uint8_t chMask = (lane == 0) ? REL_CH_LEFT : REL_CH_RIGHT;

  // Step cadence from speed and spacing
  // step_ms = spacing / speed; clamp to a sensible range
  uint32_t step_ms = (uint32_t)((uint64_t)spacing_mm * 1000ULL / (uint64_t)speed_mmps);
  if (step_ms < 80)  step_ms = 80;
  if (step_ms > 300) step_ms = 300;

  // Compute default TTL if not provided: all_on + N*step + final pulse + margin
  size_t N = (dir > 0) ? relPos_.size() : relNeg_.size();
  uint32_t total_ms = (uint32_t)all_on_ms + (uint32_t)(N ? (N-1) : 0) * step_ms + on_ms + 400;
  if (ttl_ms == 0) ttl_ms = (uint16_t)(total_ms > 0xFFFF ? 0xFFFF : total_ms);

  // Optional: initial "all-on" flash for the chosen side
  if (all_on_ms > 0) {
    if (dir > 0) {
      for (size_t i=0; i<relPos_.size(); ++i) {
        const auto& rp = relPos_[i];
        RelOnForPayload p{}; p.ver=1; p.chMask=chMask; p.on_ms=all_on_ms; p.delay_ms=0; p.ttl_ms=ttl_ms;
        ensurePeer_(rp.mac);
        sendToMacWithToken_(rp.mac, rp.tok16, CmdDomain::RELAY, REL_ON_FOR, &p, sizeof(p), requireAck);
      }
    } else {
      for (size_t i=0; i<relNeg_.size(); ++i) {
        const auto& rp = relNeg_[i];
        RelOnForPayload p{}; p.ver=1; p.chMask=chMask; p.on_ms=all_on_ms; p.delay_ms=0; p.ttl_ms=ttl_ms;
        ensurePeer_(rp.mac);
        sendToMacWithToken_(rp.mac, rp.tok16, CmdDomain::RELAY, REL_ON_FOR, &p, sizeof(p), requireAck);
      }
    }
  }

  // Now step 1-2-3 (dir>0) or 3-2-1 (dir<0) with per-relay delay
  uint32_t baseDelay = all_on_ms;
  if (dir > 0) {
    for (size_t i=0; i<relPos_.size(); ++i) {
      const auto& rp = relPos_[i];
      uint32_t dly = baseDelay + (uint32_t)i * step_ms;
      RelOnForPayload p{}; p.ver=1; p.chMask=chMask; p.on_ms=on_ms;
      p.delay_ms = (uint16_t)(dly > 0xFFFF ? 0xFFFF : dly);
      p.ttl_ms   = ttl_ms;
      ensurePeer_(rp.mac);
      sendToMacWithToken_(rp.mac, rp.tok16, CmdDomain::RELAY, REL_ON_FOR, &p, sizeof(p), requireAck);
    }
  } else {
    for (size_t ri=0; ri<relNeg_.size(); ++ri) {
      // relNeg_ is ordered [-1, -2, -3, ...]; index 0 is -1 (closest), so stepping "3-2-1" means iterate from far to near
      size_t i = relNeg_.size() - 1 - ri;
      const auto& rp = relNeg_[i];
      uint32_t dly = baseDelay + (uint32_t)ri * step_ms;
      RelOnForPayload p{}; p.ver=1; p.chMask=chMask; p.on_ms=on_ms;
      p.delay_ms = (uint16_t)(dly > 0xFFFF ? 0xFFFF : dly);
      p.ttl_ms   = ttl_ms;
      ensurePeer_(rp.mac);
      sendToMacWithToken_(rp.mac, rp.tok16, CmdDomain::RELAY, REL_ON_FOR, &p, sizeof(p), requireAck);
    }
  }
  return true;
}


bool SensorEspNowManager::sendPing(){
  if (!haveIcm_ || !haveTok_) return false;

  // Generate a non-zero nonce
  uint32_t n = ++pingNonce_;
  if (n == 0) n = ++pingNonce_;

  pendingPing_   = n;
  pingSentMs_    = millis();
  // Payload carries the nonce; ICM should echo payload unchanged
  return sendToIcm_(CmdDomain::SYS, SYS_PING, &n, sizeof(n), false);
}

uint32_t SensorEspNowManager::unixNow_() const {
  // If you track validity, check it here; otherwise getUnixTime()==0 implies “not synced”
  uint32_t t = rtc_ ? (uint32_t)rtc_->getUnixTime() : 0;
  return t ? t : (uint32_t)(millis() / 1000);
}
