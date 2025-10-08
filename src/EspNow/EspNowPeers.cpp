// EspNowPeers.cpp
#include "EspNowPeers.h"
#include <esp_now.h>
#include <esp_wifi.h>

// NvsManager API (from your project)
// We only rely on these common methods to keep coupling light.
extern "C" {
  // None needed; use C++ methods directly.
}

namespace espnow {

// ------------------------- key layout (6 chars) -------------------------
// Global:
//   "PECNT0" : int  -> number of peers
//   "SELFRO" : int  -> self role
//   "NOWCHN" : int  -> channel hint (1..13)
//
// Per-peer slot i in [0..count-1], using 3-digit index:
//   "PEM%03u" : string -> MAC  as 12-hex (AABBCCDDEEFF)
//   "PER%03u" : int    -> role (uint8_t)
//   "PEN%03u" : string -> name (<=15 chars recommended)
//   "PET%03u" : string -> token as 32-hex
//   "PEE%03u" : bool   -> enabled
//   "PEV%03u" : int    -> topoVer (uint32)

// Key helpers
static inline void key_cnt(char out[7]) { snprintf(out, 7, "PECNT0"); }
static inline void key_self_role(char out[7]) { snprintf(out, 7, "SELFRO"); }
static inline void key_channel(char out[7]) { snprintf(out, 7, "NOWCHN"); }
static inline void key_topo_ver_g(char out[7]) { snprintf(out, 7, "TVER0"); }
static inline void key_topo_tok_g(char out[7]) { snprintf(out, 7, "TTOK0"); }
static inline void key_mac(char out[7], unsigned i) { snprintf(out, 7, "PEM%03u", i); }
static inline void key_role(char out[7], unsigned i){ snprintf(out, 7, "PER%03u", i); }
static inline void key_name(char out[7], unsigned i){ snprintf(out, 7, "PEN%03u", i); }
static inline void key_token(char out[7], unsigned i){snprintf(out, 7, "PET%03u", i); }
static inline void key_en(char out[7], unsigned i)  { snprintf(out, 7, "PEE%03u", i); }
static inline void key_topo(char out[7], unsigned i){ snprintf(out, 7, "PEV%03u", i); }

// ------------------------- small utils -------------------------
static void to_hex(const uint8_t* src, size_t n, char* outUpperNoSep) {
  static const char* hexd = "0123456789ABCDEF";
  for (size_t i=0;i<n;++i) {
    outUpperNoSep[i*2+0] = hexd[(src[i] >> 4) & 0xF];
    outUpperNoSep[i*2+1] = hexd[src[i] & 0xF];
  }
  outUpperNoSep[n*2] = 0;
}

static bool from_hex(const char* hex, uint8_t* out, size_t n) {
  auto val = [](char c)->int{
    if (c>='0'&&c<='9') return c-'0';
    if (c>='A'&&c<='F') return c-'A'+10;
    if (c>='a'&&c<='f') return c-'a'+10;
    return -1;
  };
  size_t L = strlen(hex);
  if (L != n*2) return false;
  for (size_t i=0;i<n;++i) {
    int hi = val(hex[i*2]), lo = val(hex[i*2+1]);
    if (hi<0 || lo<0) return false;
    out[i] = (uint8_t)((hi<<4) | lo);
  }
  return true;
}

// ------------------------- logging shim -------------------------
static void log_line(LogFS* /*log*/, const char* /*fmt*/, ...) {
  // TODO: integrate with LogFS when available
}

// ------------------------- public API -------------------------
bool Peers::begin(NvsManager* nvs, LogFS* log) {
  _nvs = nvs; _log = log;
  // Load global hints first
  if (_nvs) {
    char k[7];
    key_channel(k); _channel  = (uint8_t)_nvs->GetInt(k, NOW_DEFAULT_CHANNEL);
    key_self_role(k); _selfRole = (uint8_t)_nvs->GetInt(k, 0);
    // Load global topology token/version (optional)
    key_topo_ver_g(k); _topoVer = (uint16_t)_nvs->GetInt(k, 0);
    String tok = _nvs->GetString("TTOK0", "");
    if (tok.length() == 32) {
      if (from_hex(tok.c_str(), _topoToken, 16)) { _hasTopo = true; }
    }
  }
  // Load peers and mirror enabled ones to radio
  if (!loadAll()) return false;

  for (const auto& p : _peers) {
    if (p.enabled) syncRadioPeer(p, true);
  }
  return true;
}

bool Peers::addPeer(const uint8_t mac[6], uint8_t role,
                    const uint8_t token128[16],
                    const char* name, bool enabled) {
  // Guard: deduplicate by MAC
  if (findByMac(mac)) return true; // already there; treat as success

  Peer p{};
  memcpy(p.mac, mac, 6);
  p.role = role;
  p.enabled = enabled;
  p.topoVer = 0;
  strncpy(p.name, name ? name : "", sizeof(p.name)-1);
  memcpy(p.token, token128, 16);

  _peers.push_back(p);
  if (!saveAll()) return false;

  if (enabled) return syncRadioPeer(p, true);
  return true;
}

bool Peers::enablePeer(const uint8_t mac[6], bool en) {
  Peer* p = findByMac(mac);
  if (!p) return false;
  if (p->enabled == en) return true;

  p->enabled = en;
  if (!saveAll()) return false;
  return syncRadioPeer(*p, en);
}

bool Peers::removePeer(const uint8_t mac[6]) {
  for (size_t i=0;i<_peers.size();++i) {
    if (now_same_mac(_peers[i].mac, mac)) {
      // Remove from radio first if enabled
      if (_peers[i].enabled) syncRadioPeer(_peers[i], false);
      _peers.erase(_peers.begin()+i);
      return saveAll();
    }
  }
  return false;
}

Peer* Peers::findByMac(const uint8_t mac[6]) {
  for (auto& p : _peers) if (now_same_mac(p.mac, mac)) return &p;
  return nullptr;
}
const Peer* Peers::findByMac(const uint8_t mac[6]) const {
  for (const auto& p : _peers) if (now_same_mac(p.mac, mac)) return &p;
  return nullptr;
}

bool Peers::tokenMatches(const uint8_t mac[6], const uint8_t token128[16]) const {
  const Peer* p = findByMac(mac);
  if (!p || !p->enabled) return false;
  return memcmp(p->token, token128, 16) == 0;
}

void Peers::setSelfRole(uint8_t r) {
  _selfRole = r;
  if (_nvs) {
    char k[7]; key_self_role(k);
    _nvs->PutInt(k, (int)r);
  }
}

void Peers::setChannel(uint8_t ch) {
  if (!now_is_valid_channel(ch)) return;
  _channel = ch;
  if (_nvs) {
    char k[7]; key_channel(k);
    _nvs->PutInt(k, (int)ch);
  }
}

// ------------------------- persistence -------------------------
bool Peers::loadAll() {
  _peers.clear();
  if (!_nvs) return true; // allow RAM-only in tests

  char k[7];
  key_cnt(k);
  int count = _nvs->GetInt(k, 0);
  if (count < 0) count = 0;

  for (int i=0; i<count; ++i) {
    Peer p{};
    char ks[7];

    // MAC (required)
    key_mac(ks, i);
    String macHex = _nvs->GetString(ks, "");
    if (macHex.length() != 12) continue;
    if (!from_hex(macHex.c_str(), p.mac, 6)) continue;

    // Role
    key_role(ks, i);
    p.role = (uint8_t)_nvs->GetInt(ks, 0);

    // Name
    key_name(ks, i);
    String nm = _nvs->GetString(ks, "");
    strncpy(p.name, nm.c_str(), sizeof(p.name)-1);

    // Token
    key_token(ks, i);
    String tok = _nvs->GetString(ks, "");
    if (tok.length() == 32) {
      (void)from_hex(tok.c_str(), p.token, 16);
    } else {
      memset(p.token, 0, 16);
    }

    // Enabled
    key_en(ks, i);
    p.enabled = _nvs->GetBool(ks, false);

    // Topology version
    key_topo(ks, i);
    p.topoVer = (uint32_t)_nvs->GetInt(ks, 0);

    _peers.push_back(p);
  }
  return true;
}

bool Peers::saveSlot(unsigned i, const Peer& p) {
  if (!_nvs) return true;

  char ks[7];
  // MAC
  key_mac(ks, i);
  char macHex[13]; to_hex(p.mac, 6, macHex);
  _nvs->PutString(ks, macHex);

  // Role
  key_role(ks, i);
  _nvs->PutInt(ks, (int)p.role);

  // Name
  key_name(ks, i);
  _nvs->PutString(ks, p.name);

  // Token
  key_token(ks, i);
  char tokHex[33]; to_hex(p.token, 16, tokHex);
  _nvs->PutString(ks, tokHex);

  // Enabled
  key_en(ks, i);
  _nvs->PutBool(ks, p.enabled);

  // Topology version
  key_topo(ks, i);
  _nvs->PutInt(ks, (int)p.topoVer);

  return true;
}

void Peers::clearStaleFrom(unsigned startIdx) {
  if (!_nvs) return;
  char ks[7];
  for (unsigned i=startIdx; i<startIdx+64; ++i) { // wipe a small tail
    key_mac(ks,i);  _nvs->RemoveKey(ks);
    key_role(ks,i); _nvs->RemoveKey(ks);
    key_name(ks,i); _nvs->RemoveKey(ks);
    key_token(ks,i);_nvs->RemoveKey(ks);
    key_en(ks,i);   _nvs->RemoveKey(ks);
    key_topo(ks,i); _nvs->RemoveKey(ks);
  }
}

bool Peers::saveAll() {
  if (!_nvs) return true;

  // Write count first
  char kc[7]; key_cnt(kc);
  const int newCount = (int)_peers.size();
  const int oldCount = _nvs->GetInt(kc, 0);
  _nvs->PutInt(kc, newCount);

  // Write contiguous slots
  for (int i=0;i<newCount;++i) {
    if (!saveSlot((unsigned)i, _peers[(size_t)i])) return false;
  }

  // Clear stale slots if list shrank
  if (oldCount > newCount) clearStaleFrom((unsigned)newCount);
  return true;
}

// ------------------------- radio sync -------------------------
bool Peers::syncRadioPeer(const Peer& p, bool add) const {
  esp_now_peer_info_t info{};
  memcpy(info.peer_addr, p.mac, 6);
  info.ifidx = WIFI_IF_STA;
  info.encrypt = false;
  info.channel = _channel;

  if (add) {
    if (esp_now_is_peer_exist(info.peer_addr)) {
      (void)esp_now_del_peer(info.peer_addr);
    }
    return esp_now_add_peer(&info) == ESP_OK;
  } else {
    if (esp_now_is_peer_exist(info.peer_addr)) {
      return esp_now_del_peer(info.peer_addr) == ESP_OK;
    }
    return true;
  }
}


// ------------------------- topology (device-wide) -------------------------
bool Peers::getTopoToken(uint8_t out[16]) const {
  if (!_hasTopo) return false;
  memcpy(out, _topoToken, 16);
  return true;
}

void Peers::setTopoToken(const uint8_t tok[16]) {
  memcpy(_topoToken, tok, 16);
  _hasTopo = true;
  if (_nvs) {
    char hex[33]; to_hex(tok, 16, hex);
    _nvs->PutString("TTOK0", hex);
  }
}

void Peers::setTopoVersion(uint16_t v) {
  _topoVer = v;
  if (_nvs) {
    char k[7]; key_topo_ver_g(k);
    _nvs->PutInt(k, (int)v);
  }
}

bool Peers::topoTokenMatches(const uint8_t tok[16]) const {
  return _hasTopo && (memcmp(_topoToken, tok, 16) == 0);
}
} // namespace espnow
