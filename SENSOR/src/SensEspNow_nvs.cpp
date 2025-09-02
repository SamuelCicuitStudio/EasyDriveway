
#include "SensEspNow.h"
#include <string.h>
#include <stdio.h>

// --- parse "AA:BB:CC:DD:EE:FF" ---
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

void SensorEspNowManager::persistChannel_(uint8_t ch){
  cfg_->PutInt(ESPNOW_CH_KEY, ch);
}

bool SensorEspNowManager::loadFromNvs(){
  channel_ = (uint8_t)cfg_->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT);

  {
    String macStr = cfg_->GetString(SSM_MASTER_MAC_KEY, "");
    if (macStr.length() == 17 && parseMacStr_(macStr, icmMac_)) {
      haveIcm_ = true;
      ensurePeer_(icmMac_);
    }
  }

  {
    String t = cfg_->GetString(SSM_TOKEN16_KEY, "");
    uint8_t tmp[16];
    if (parseTok16Hex_(t, tmp)) {
      memcpy(token16_, tmp, 16);
      haveTok_ = true;
    }
  }

  sensIdx_ = (uint8_t)cfg_->GetInt(NVS_ZC_SENSOR_INDEX, 0xFF);

  relNeg_.clear();
  relPos_.clear();

  const int nNeg = cfg_->GetInt(NVS_ZC_NEG_COUNT, 0);
  const int nPos = cfg_->GetInt(NVS_ZC_POS_COUNT, 0);

  auto getStr = [&](const char* fmtKey, unsigned ord)->String{
    char key[12]; snprintf(key, sizeof(key), fmtKey, ord);
    return cfg_->GetString(key, "");
  };

  for (int i = 1; i <= nNeg; ++i){
    String macS = getStr(NVS_ZC_NEG_MAC_FMT,     (unsigned)i);
    String tokS = getStr(NVS_ZC_NEG_TOKEN16_FMT, (unsigned)i);
    if (macS.isEmpty() || tokS.isEmpty()) continue;
    RelayPeer rp; rp.relayIdx = 0xFF; rp.relPos = -i;
    if (!parseMacStr_(macS, rp.mac)) continue;
    if (!parseTok16Hex_(tokS, rp.tok16)) continue;
    relNeg_.push_back(rp);
    ensurePeer_(rp.mac);
  }

  for (int i = 1; i <= nPos; ++i){
    String macS = getStr(NVS_ZC_POS_MAC_FMT,     (unsigned)i);
    String tokS = getStr(NVS_ZC_POS_TOKEN16_FMT, (unsigned)i);
    if (macS.isEmpty() || tokS.isEmpty()) continue;
    RelayPeer rp; rp.relayIdx = 0xFF; rp.relPos = +i;
    if (!parseMacStr_(macS, rp.mac)) continue;
    if (!parseTok16Hex_(tokS, rp.tok16)) continue;
    relPos_.push_back(rp);
    ensurePeer_(rp.mac);
  }
  return true;
}
