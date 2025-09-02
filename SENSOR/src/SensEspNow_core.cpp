
#include "SensEspNow.h"
#include <string.h>

// ---- Local helpers (file-local) ----
static String tok16ToHex_(const uint8_t tok[16]) {
  static const char* hexd = "0123456789ABCDEF";
  char out[33]; out[32] = 0;
  for (int i=0;i<16;++i) {
    out[i*2+0] = hexd[(tok[i] >> 4) & 0xF];
    out[i*2+1] = hexd[(tok[i]     ) & 0xF];
  }
  return String(out);
}

// ========== static thunks & self ==========
static SensorEspNowManager* s_self = nullptr;
void SensorEspNowManager::onRecvThunk(const uint8_t *mac, const uint8_t *data, int len){
  if (s_self) s_self->onRecv_(mac, data, len);
}
void SensorEspNowManager::onSentThunk(const uint8_t *mac, esp_now_send_status_t status){
  if (s_self) s_self->onSent_(mac, status);
}

// ========== Public: begin/poll ==========
bool SensorEspNowManager::begin(uint8_t channel, const char* pmk16){
  // Prefer persisted state (channel, ICM mac, token, topology slices)
  loadFromNvs();

  if (channel >= 1 && channel <= 13) channel_ = channel;

  s_self = this;
  WiFi.mode(WIFI_STA);

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) return false;
  if (pmk16 && strlen(pmk16) == 16) esp_now_set_pmk((const uint8_t*)pmk16);

  esp_now_register_recv_cb(&SensorEspNowManager::onRecvThunk);
  esp_now_register_send_cb(&SensorEspNowManager::onSentThunk);

  if (haveIcm_) ensurePeer_(icmMac_);
  for (auto &rp : relNeg_) ensurePeer_(rp.mac);
  for (auto &rp : relPos_) ensurePeer_(rp.mac);

  return true;
}

void SensorEspNowManager::poll(){
  if (chSwitchPending_ && millis() >= chSwitchDueMs_) {
    chSwitchPending_ = false;
    channel_ = chTarget_;
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    ensurePeer_(icmMac_);
    for (auto &rp : relNeg_) ensurePeer_(rp.mac);
    for (auto &rp : relPos_) ensurePeer_(rp.mac);
    persistChannel_(channel_);
  }
}

// ========== Utils ==========
String SensorEspNowManager::macBytesToStr(const uint8_t mac[6]){
  char b[20]; snprintf(b,sizeof(b),"%02X:%02X:%02X:%02X:%02X:%02X",
                       mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  return String(b);
}

uint32_t SensorEspNowManager::unixNow_() const {
  uint32_t t = rtc_ ? (uint32_t)rtc_->getUnixTime() : 0;
  return t ? t : (uint32_t)(millis() / 1000);
}

// ========== Low-level send & header fill ==========
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
  h.ts    = unixNow_();
  h.ctr   = 0;
  memcpy(h.tok16, tok16, 16);
}

void SensorEspNowManager::ensurePeer_(const uint8_t mac[6]){
  if (!mac) return;
  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, mac, 6);
  p.ifidx = WIFI_IF_STA;
  p.channel = channel_;
  p.encrypt = 0;
  esp_now_del_peer(mac);
  esp_now_add_peer(&p);
}
