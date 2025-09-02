
#include "SensEspNow.h"

// ---- Public reply helpers ----
bool SensorEspNowManager::sendSysAck(uint16_t ctr, uint8_t code){
  SysAckPayload ap{}; ap.ctr = ctr; ap.code = code; ap.rsv = 0;
  return sendToIcm_(CmdDomain::SYS, SYS_ACK, &ap, sizeof(ap), false);
}
bool SensorEspNowManager::sendDayNight(){
  uint8_t is_day = 255;
  if (!makeDayNight_(is_day)) return false;
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

bool SensorEspNowManager::sendPing(){
  if (!haveIcm_ || !haveTok_) return false;
  uint32_t n = ++pingNonce_;
  if (n == 0) n = ++pingNonce_;
  pendingPing_   = n;
  pingSentMs_    = millis();
  return sendToIcm_(CmdDomain::SYS, SYS_PING, &n, sizeof(n), false);
}

// ---- Handlers ----
void SensorEspNowManager::handleSys_(const IcmMsgHdr& h, const uint8_t* payload, int plen){
  if (h.op == SYS_PING) {
    if (rtc_ && h.ts) rtc_->setUnixTime((unsigned long)h.ts);
    if (payload && plen == (int)sizeof(uint32_t) && pendingPing_ != 0) {
      uint32_t n = 0; memcpy(&n, payload, sizeof(n));
      if (n == pendingPing_) {
        lastPingRttMs_ = millis() - pingSentMs_;
        pendingPing_   = 0;
        return;
      }
    }
    sendToIcm_(CmdDomain::SYS, SYS_PING, payload, plen, false);
    return;
  }

  if (h.op == SYS_SET_CH && plen >= (int)sizeof(SysSetChPayload)) {
    const SysSetChPayload* sp = (const SysSetChPayload*)payload;
    uint8_t newCh = sp->new_ch;
    if (newCh >= 1 && newCh <= 13) {
      const uint32_t now_epoch = unixNow_();
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
        ensurePeer_(icmMac_);
        for (auto &rp : relNeg_) ensurePeer_(rp.mac);
        for (auto &rp : relPos_) ensurePeer_(rp.mac);
        persistChannel_(channel_);
      }
    }
    return;
  }

  if (h.op == SYS_MODE && plen >= (int)sizeof(SysModePayload)) {
    const SysModePayload* mp = (const SysModePayload*)payload;
    cfg_->PutInt(ESPNOW_MD_KEY, mp->mode);
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
  parseAndMirrorZcSensor_(payload, plen);
  // For diagnostics: human-readable mirrors
  saveTopoMacStrings_();
}
