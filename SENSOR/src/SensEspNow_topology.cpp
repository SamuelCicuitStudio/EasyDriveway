
#include "SensEspNow.h"
#include <string.h>

// local helpers for hex string conversion
static String tok16Hex_(const uint8_t tok[16]){
  static const char* hexd = "0123456789ABCDEF";
  char out[33]; out[32] = 0;
  for (int i=0;i<16;++i) {
    out[i*2+0] = hexd[(tok[i] >> 4) & 0xF];
    out[i*2+1] = hexd[(tok[i]     ) & 0xF];
  }
  return String(out);
}

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
  cfg_->PutInt (NVS_ZC_SENSOR_INDEX, sensIdx_);
  cfg_->PutInt (NVS_ZC_HAS_PREV, z->hasPrev ? 1 : 0);
  if (z->hasPrev) {
    cfg_->PutInt   (NVS_ZC_PREV_INDEX, z->prevSensIdx);
    cfg_->PutString(NVS_ZC_PREV_MAC, macBytesToStr(z->prevSensMac).c_str());
    cfg_->PutString(NVS_ZC_PREV_TOKEN16, tok16Hex_(z->prevSensTok16).c_str());
  } else {
    cfg_->PutString(NVS_ZC_PREV_MAC, "");
    cfg_->PutString(NVS_ZC_PREV_TOKEN16, "");
  }
  cfg_->PutInt (NVS_ZC_HAS_NEXT, z->hasNext ? 1 : 0);
  if (z->hasNext) {
    cfg_->PutInt   (NVS_ZC_NEXT_INDEX, z->nextSensIdx);
    cfg_->PutString(NVS_ZC_NEXT_MAC, macBytesToStr(z->nextSensMac).c_str());
    cfg_->PutString(NVS_ZC_NEXT_TOKEN16, tok16Hex_(z->nextSensTok16).c_str());
  } else {
    cfg_->PutString(NVS_ZC_NEXT_MAC, "");
    cfg_->PutString(NVS_ZC_NEXT_TOKEN16, "");
  }

  const uint8_t* p = payload + sizeof(TopoZeroCenteredSensor);
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

// Persist human-readable MACs/tokens per side and by global index for diagnostics
void SensorEspNowManager::saveTopoMacStrings_(){
  cfg_->PutInt(NVS_ZC_SENSOR_INDEX, sensIdx_);
  cfg_->PutInt(NVS_ZC_NEG_COUNT, (int)relNeg_.size());
  cfg_->PutInt(NVS_ZC_POS_COUNT, (int)relPos_.size());

  auto putKV = [&](const char* fmtKey, unsigned val, const String& vstr){
    char key[12];
    snprintf(key, sizeof(key), fmtKey, val);
    cfg_->PutString(key, vstr.c_str());
  };

  for (size_t i = 0; i < relNeg_.size(); ++i) {
    const auto& e = relNeg_[i];
    String macStr = macBytesToStr(e.mac);
    String tokStr = tok16Hex_(e.tok16);
    putKV(NVS_ZC_NEG_MAC_FMT,     (unsigned)(i + 1), macStr);
    putKV(NVS_ZC_NEG_TOKEN16_FMT, (unsigned)(i + 1), tokStr);
    putKV(NVS_ZC_RELAY_MAC_BYIDX_FMT, (unsigned)e.relayIdx, macStr);
    putKV(NVS_ZC_RELAY_TOK_BYIDX_FMT, (unsigned)e.relayIdx, tokStr);
  }

  for (size_t i = 0; i < relPos_.size(); ++i) {
    const auto& e = relPos_[i];
    String macStr = macBytesToStr(e.mac);
    String tokStr = tok16Hex_(e.tok16);
    putKV(NVS_ZC_POS_MAC_FMT,     (unsigned)(i + 1), macStr);
    putKV(NVS_ZC_POS_TOKEN16_FMT, (unsigned)(i + 1), tokStr);
    putKV(NVS_ZC_RELAY_MAC_BYIDX_FMT, (unsigned)e.relayIdx, macStr);
    putKV(NVS_ZC_RELAY_TOK_BYIDX_FMT, (unsigned)e.relayIdx, tokStr);
  }
}
