
#include "SensEspNow.h"
#include <string.h>

void SensorEspNowManager::onSent_(const uint8_t *mac, esp_now_send_status_t status){
  (void)mac; (void)status;
}

void SensorEspNowManager::onRecv_(const uint8_t *mac, const uint8_t *data, int len){
  if (len < (int)sizeof(IcmMsgHdr)) return;
  const IcmMsgHdr* h   = (const IcmMsgHdr*)data;
  const uint8_t*   pl  = data + sizeof(IcmMsgHdr);
  const int        plen= len  - (int)sizeof(IcmMsgHdr);

  // Learn ICM MAC on first valid packet (existing behavior)
  if (!haveIcm_) {
    memcpy(icmMac_, mac, 6);
    haveIcm_ = true;
    ensurePeer_(icmMac_);
  }

  // ----- First-contact pairing: save token if not paired yet -----
  // We only accept the token from the MAC we consider the ICM.
  const bool isPaired = cfg_->GetBool(DEVICE_PAIRED_KEY, DEVICE_PAIRED_DEFAULT);
  if (!isPaired && !haveTok_ && memcmp(mac, icmMac_, 6) == 0) {
    // Persist the token (also sets haveTok_ = true internally)
    setNodeToken16(h->tok16);
    // Mark paired so we never re-learn blindly
    cfg_->PutBool(DEVICE_PAIRED_KEY, true);
    // (No extra ACK here; we keep the normal ACK below.)
  }

  // Token gate (unchanged): once we have a token, only accept matching frames
  if (haveTok_ && memcmp(h->tok16, token16_, 16) != 0) {
    return;
  }

  // Honor ACK requests
  if (h->flags & HDR_FLAG_ACKREQ) sendSysAck(h->ctr, 0);

  // Dispatch by domain
  switch ((CmdDomain)h->dom) {
    case CmdDomain::SYS:  handleSys_(*h, pl, plen);  break;
    case CmdDomain::SENS: handleSens_(*h, pl, plen); break;
    case CmdDomain::TOPO: handleTopo_(*h, pl, plen); break;
    default: break;
  }
}


// --- setters (persist) ---
void SensorEspNowManager::setIcmMac(const uint8_t mac[6]){
  memcpy(icmMac_, mac, 6);
  haveIcm_ = true;
  ensurePeer_(icmMac_);
  cfg_->PutString(SSM_MASTER_MAC_KEY, macBytesToStr(icmMac_).c_str());
}

void SensorEspNowManager::setNodeToken16(const uint8_t token16[16]){
  memcpy(token16_, token16, 16);
  haveTok_ = true;
  // store as 32-hex
  static auto tok16ToHex_ = [](const uint8_t tok[16])->String{
    static const char* hexd = "0123456789ABCDEF";
    char out[33]; out[32] = 0;
    for (int i=0;i<16;++i) {
      out[i*2+0] = hexd[(tok[i] >> 4) & 0xF];
      out[i*2+1] = hexd[(tok[i]     ) & 0xF];
    }
    return String(out);
  };
  cfg_->PutString(SSM_TOKEN16_KEY, tok16ToHex_(token16_).c_str());
}
