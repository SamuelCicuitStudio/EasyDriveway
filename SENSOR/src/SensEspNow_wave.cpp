
#include "SensEspNow.h"
#include <string.h>
#include <stdio.h>

// local parsers
static bool parseMacStr_(const String& s, uint8_t mac[6]){
  if (s.length() != 17) return false;
  unsigned v[6];
  if (sscanf(s.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
             &v[0],&v[1],&v[2],&v[3],&v[4],&v[5]) != 6) return false;
  for (int i=0;i<6;i++) mac[i] = (uint8_t)v[i];
  return true;
}
static int hexVal_(char c){
  if (c>='0'&&c<='9') return c-'0';
  if (c>='A'&&c<='F') return c-'A'+10;
  if (c>='a'&&c<='f') return c-'a'+10;
  return -1;
}
static bool parseTok16Hex_(const String& hex, uint8_t out[16]){
  if (hex.length() < 32) return false;
  for (int i=0;i<16;i++){
    int hi = hexVal_(hex[i*2+0]);
    int lo = hexVal_(hex[i*2+1]);
    if (hi<0||lo<0) return false;
    out[i] = (uint8_t)((hi<<4)|lo);
  }
  return true;
}

// Minimal impls: send a compact wave header to prev/next sensor if present in NVS
bool SensorEspNowManager::sendWaveHdrToPrev(uint8_t lane, int8_t dir, uint16_t speed_mmps,
                        uint32_t eta_ms, uint8_t wave_id, bool requireAck){
  String macS = cfg_->GetString(NVS_ZC_PREV_MAC, "");
  String tokS = cfg_->GetString(NVS_ZC_PREV_TOKEN16, "");
  uint8_t mac[6]; uint8_t tok[16];
  if (macS.isEmpty() || tokS.isEmpty()) return false;
  if (!parseMacStr_(macS, mac)) return false;
  if (!parseTok16Hex_(tokS, tok)) return false;
  // Compose a small payload; if you already have a WaveHdr type in CommandAPI.h, use it instead.
  struct WaveHdr {
    uint8_t ver=1, lane=0; int8_t dir=0; uint8_t wave_id=0;
    uint16_t speed_mmps=0; uint32_t eta_ms=0;
  } __attribute__((packed));
  WaveHdr wh; wh.ver=1; wh.lane=lane; wh.dir=dir; wh.wave_id=wave_id; wh.speed_mmps=speed_mmps; wh.eta_ms=eta_ms;
  ensurePeer_(mac);
  return sendToMacWithToken_(mac, tok, CmdDomain::SENS, SENS_WAVE_HDR, &wh, sizeof(wh), requireAck);
}

bool SensorEspNowManager::sendWaveHdrToNext(uint8_t lane, int8_t dir, uint16_t speed_mmps,
                        uint32_t eta_ms, uint8_t wave_id, bool requireAck){
  String macS = cfg_->GetString(NVS_ZC_NEXT_MAC, "");
  String tokS = cfg_->GetString(NVS_ZC_NEXT_TOKEN16, "");
  uint8_t mac[6]; uint8_t tok[16];
  if (macS.isEmpty() || tokS.isEmpty()) return false;
  if (!parseMacStr_(macS, mac)) return false;
  if (!parseTok16Hex_(tokS, tok)) return false;
  struct WaveHdr {
    uint8_t ver=1, lane=0; int8_t dir=0; uint8_t wave_id=0;
    uint16_t speed_mmps=0; uint32_t eta_ms=0;
  } __attribute__((packed));
  WaveHdr wh; wh.ver=1; wh.lane=lane; wh.dir=dir; wh.wave_id=wave_id; wh.speed_mmps=speed_mmps; wh.eta_ms=eta_ms;
  ensurePeer_(mac);
  return sendToMacWithToken_(mac, tok, CmdDomain::SENS, SENS_WAVE_HDR, &wh, sizeof(wh), requireAck);
}

// Read caches (no producer in current refactor; keep safe behavior)
bool SensorEspNowManager::getWaveFromPrev(uint8_t lane, uint16_t& speed_mmps, int8_t& dir, uint32_t& eta_ms,
                       uint8_t& wave_id, uint32_t& age_ms, uint8_t srcMac[6]) const{
  const auto& wc = wavePrev_[lane & 1];
  if (!wc.valid) return false;
  speed_mmps = wc.speed_mmps; dir = wc.dir; eta_ms = wc.eta_ms; wave_id = wc.wave_id;
  age_ms = millis() - wc.recv_ms;
  memcpy(srcMac, wc.srcMac, 6);
  return true;
}
bool SensorEspNowManager::getWaveFromNext(uint8_t lane, uint16_t& speed_mmps, int8_t& dir, uint32_t& eta_ms,
                       uint8_t& wave_id, uint32_t& age_ms, uint8_t srcMac[6]) const{
  const auto& wc = waveNext_[lane & 1];
  if (!wc.valid) return false;
  speed_mmps = wc.speed_mmps; dir = wc.dir; eta_ms = wc.eta_ms; wave_id = wc.wave_id;
  age_ms = millis() - wc.recv_ms;
  memcpy(srcMac, wc.srcMac, 6);
  return true;
}
