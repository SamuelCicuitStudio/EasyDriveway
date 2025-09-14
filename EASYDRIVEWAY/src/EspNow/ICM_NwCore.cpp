/**************************************************************
 * File    : ICM_NwCore.cpp — ESP-NOW core utilities (TOKEN=HEX STRING)
 * Purpose : Role-agnostic ESPNOW core with ICM-only registry
 *           (16 Sensors, 16 Relays, 1 PMS) managed by NVS keys.
 *           - Existence-based registry (Iskey/RemoveKey)
 *           - Auto-slot assignment (no index required by callers)
 *           - Send with optional HW-ACK wait + status cache
 *           - MAC utilities, tokens (hex string), sequencing, callbacks
 *
 * Notes   :
 *   • Admission tokens are 32 ASCII hex chars (no NUL on wire)
 *   • All NVS persistence for tokens is via PutString/GetString
 *   • Comparison is case-insensitive; values are normalized to lowercase
 **************************************************************/

#include "ICM_Nw.h"
#include <mbedtls/sha256.h>

namespace NwCore {

// ======================================================================
// Static state
// ======================================================================
Core* Core::s_self = nullptr;
static volatile uint16_t s_seq = 1; // sequence starts at 1

// ======================================================================
// Small helpers for token normalization and compare (case-insensitive)
// ======================================================================
static inline char toLowerHex(char c) {
  if (c >= 'A' && c <= 'F') return (char)(c - 'A' + 'a');
  return c;
}

static inline bool isHexChar(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Normalize any String to exactly 32 lowercase hex chars into out[32].
// Returns false if input lacks 32 hex chars.
static bool normalizeToken32(const String& in, char out[NOW_TOKEN_HEX_LEN]) {
  // Fast path: already 32 characters
  if (in.length() == NOW_TOKEN_HEX_LEN) {
    for (int i = 0; i < NOW_TOKEN_HEX_LEN; ++i) {
      char c = in[i];
      if (!isHexChar(c)) return false;
      out[i] = toLowerHex(c);
    }
    return true;
  }
  // Slow path: strip non-hex and collect until 32
  char buf[NOW_TOKEN_HEX_LEN];
  int k = 0;
  for (size_t i = 0; i < in.length() && k < NOW_TOKEN_HEX_LEN; ++i) {
    char c = in[i];
    if (isHexChar(c)) buf[k++] = toLowerHex(c);
  }
  if (k != NOW_TOKEN_HEX_LEN) return false;
  for (int i = 0; i < NOW_TOKEN_HEX_LEN; ++i) out[i] = buf[i];
  return true;
}

// Compare two 32-char hex tokens case-insensitively.
static bool tokenEq32(const char a[NOW_TOKEN_HEX_LEN], const char b[NOW_TOKEN_HEX_LEN]) {
  for (int i = 0; i < NOW_TOKEN_HEX_LEN; ++i) {
    char ca = toLowerHex(a[i]);
    char cb = toLowerHex(b[i]);
    if (ca != cb) return false;
  }
  return true;
}

// ======================================================================
// Construction / attachment
// ======================================================================
Core::Core(ConfigManager* cfg, ICMLogFS* log, RTCManager* rtc)
: _cfg(cfg), _log(log), _rtc(rtc) { clearStats(); }

Core& Core::attachCfg (ConfigManager* cfg) { _cfg = cfg; return *this; }
Core& Core::attachLog (ICMLogFS* log)      { _log = log; return *this; }
Core& Core::attachRtc (RTCManager* rtc)    { _rtc = rtc; return *this; }
Core& Core::attach    (ConfigManager* cfg, ICMLogFS* log, RTCManager* rtc) {
  _cfg = cfg; _log = log; _rtc = rtc; return *this;
}

// ======================================================================
// Lifecycle
// ======================================================================
bool Core::begin(uint8_t channel, const uint8_t* pmk16_or_null) {
  _channel = channel;

//  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    if (_log) _log->event(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_ERROR, 100, "esp_now_init failed");
    return false;
  }
  if (pmk16_or_null) esp_now_set_pmk(pmk16_or_null);

  s_self = this;
  // ---- Register ESP-NOW receive callback for the active role
  #if defined(NVS_ROLE_ICM)
    setRecvCallback(&NwCore::Core::icmRecvCallback);
  #elif defined(NVS_ROLE_PMS)
    setRecvCallback(&NwCore::Core::pmsRecvCallback);
  #elif defined(NVS_ROLE_SENS)
    setRecvCallback(&NwCore::Core::sensRecvCallback);
  #elif defined(NVS_ROLE_RELAY)
    setRecvCallback(&NwCore::Core::relayRecvCallback);
  #else
    #error "Define one of NVS_ROLE_ICM, NVS_ROLE_PMS, NVS_ROLE_SENS, NVS_ROLE_RELAY"
  #endif


  esp_now_register_send_cb(&Core::onSendThunk);
  esp_now_register_recv_cb(&Core::onRecvThunk);

  if (_log) _log->event(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_INFO, 101, String("ESPNOW started on ch ") + String(channel));
  return true;
}

void Core::end() {
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
  esp_now_deinit();
  if (_log) _log->event(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_INFO, 102, "ESPNOW stopped");
}

// ======================================================================
// Callback hookup
// ======================================================================
void Core::setRecvCallback(RecvCb cb) { _userRecv = cb; }

// ======================================================================
// Peer management
// ======================================================================
bool Core::addPeer(const uint8_t mac[6], uint8_t channel, bool encrypt, const uint8_t* lmk16_or_null) {
  if (!mac) return false;
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, 6);
  peer.ifidx   = WIFI_IF_STA;
  peer.channel = channel ? channel : _channel;
  peer.encrypt = encrypt ? 1 : 0;
  if (encrypt && lmk16_or_null) memcpy(peer.lmk, lmk16_or_null, 16);

  esp_now_del_peer(mac); // idempotent replace
  esp_err_t err = esp_now_add_peer(&peer);
  if (_log) {
    String m = String("peer ") + macBytesToStr(mac) + (err==ESP_OK ? " added" : " add failed");
    _log->event(ICMLogFS::DOM_ESPNOW, err==ESP_OK?ICMLogFS::EV_INFO:ICMLogFS::EV_ERROR, 110, m);
  }
  return err == ESP_OK;
}

void Core::delPeer(const uint8_t mac[6]) {
  if (!mac) return;
  esp_now_del_peer(mac);
  if (_log) {
    String m = String("peer ") + macBytesToStr(mac) + " deleted";
    _log->event(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_INFO, 111, m);
  }
}

void Core::clearPeers() {
  // Re-init clears peer table; re-hook callbacks & stats.
  esp_now_deinit();
  esp_now_init();
  esp_now_register_send_cb(&Core::onSendThunk);
  esp_now_register_recv_cb(&Core::onRecvThunk);
  clearStats();
  if (_log) _log->event(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_WARN, 112, "peer table cleared via reinit");
}

// ======================================================================
// Send + optional HW ACK wait
// ======================================================================
esp_err_t Core::send(const uint8_t mac[6],
                     const void* payload,
                     size_t len,
                     uint32_t waitAckMs,
                     esp_now_send_status_t* outStatus) {
  if (!mac || !payload || !len) return ESP_ERR_INVALID_ARG;

  SendStat* st = findOrAllocStat(mac);
  if (st) { st->seen = false; st->ts_ms = nowMs(); }

  esp_err_t err = esp_now_send(mac, (const uint8_t*)payload, len);
  if (err != ESP_OK) {
    if (_log) {
      String m = String("send immediate fail to ") + macBytesToStr(mac) + " err=" + String((int)err);
      _log->event(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_ERROR, 120, m);
    }
    return err;
  }

  if (waitAckMs == 0) return ESP_OK;

  uint32_t start = nowMs();
  while (nowMs() - start < waitAckMs) {
    if (st && st->seen) {
      if (outStatus) *outStatus = st->status;
      if (st->status == ESP_NOW_SEND_SUCCESS) return ESP_OK;
      return ESP_FAIL;
    }
    delay(1);
  }
  if (outStatus && st) *outStatus = st->seen ? st->status : ESP_NOW_SEND_FAIL;
  if (_log) _log->event(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_WARN, 121, String("send timeout to ") + macBytesToStr(mac));
  return ESP_ERR_TIMEOUT;
}

bool Core::lastSendStatus(const uint8_t mac[6], esp_now_send_status_t& status, uint32_t& ts_ms) const {
  const SendStat* st = findStat(mac);
  if (!st || !st->seen) return false;
  status = st->status; ts_ms = st->ts_ms; return true;
}

// ======================================================================
// MAC utilities
// ======================================================================
static inline uint8_t hexNibble(char c) {
  if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
  if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
  return 0xFF;
}

bool Core::macStrToBytes(const char* in, uint8_t out[6]) {
  if (!in || !out) return false;
  size_t n = strlen(in);
  if (n == 17) { // "AA:BB:CC:DD:EE:FF"
    unsigned v[6];
    if (sscanf(in, "%02x:%02x:%02x:%02x:%02x:%02x", &v[0],&v[1],&v[2],&v[3],&v[4],&v[5]) != 6) return false;
    for (int i=0;i<6;i++) out[i] = (uint8_t)v[i];
    return true;
  } else if (n == 12) { // "AABBCCDDEEFF"
    for (int i=0;i<6;i++) {
      uint8_t hi = hexNibble(in[i*2]);
      uint8_t lo = hexNibble(in[i*2+1]);
      if (hi > 0x0F || lo > 0x0F) return false;
      out[i] = (uint8_t)((hi<<4) | lo);
    }
    return true;
  }
  return false;
}

String Core::macBytesToStr(const uint8_t mac[6]) {
  char b[18];
  snprintf(b,sizeof(b),"%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  return String(b);
}

bool Core::macEq   (const uint8_t a[6], const uint8_t b[6]) { return memcmp(a,b,6) == 0; }
int  Core::macCmp  (const uint8_t a[6], const uint8_t b[6]) { return memcmp(a,b,6); }
bool Core::macEqStr(const uint8_t mac[6], const char* mac12or17) {
  if (!mac12or17) return false;
  uint8_t tmp[6]; if (!macStrToBytes(mac12or17, tmp)) return false;
  return macEq(mac, tmp);
}

void Core::efuseMacBytes(uint8_t out[6]) {
  uint64_t m = ESP.getEfuseMac();
  out[0] = (uint8_t)(m >> 40);
  out[1] = (uint8_t)(m >> 32);
  out[2] = (uint8_t)(m >> 24);
  out[3] = (uint8_t)(m >> 16);
  out[4] = (uint8_t)(m >>  8);
  out[5] = (uint8_t)(m >>  0);
}

String Core::efuseMac12() {
  uint8_t b[6]; efuseMacBytes(b);
  char s[13];
  snprintf(s, sizeof(s), "%02X%02X%02X%02X%02X%02X", b[0],b[1],b[2],b[3],b[4],b[5]);
  return String(s);
}

// ======================================================================
// Tokens / verification
// ======================================================================
// DEPRECATED: low16 from SHA256 over icmMac|nodeMac|counter, non-zero
uint16_t Core::generateToken16(const uint8_t icmMac[6], const uint8_t nodeMac[6], uint32_t counter) {
  uint8_t buf[16];
  memcpy(buf,       icmMac,  6);
  memcpy(buf + 6,   nodeMac, 6);
  buf[12] = (uint8_t)(counter >> 24);
  buf[13] = (uint8_t)(counter >> 16);
  buf[14] = (uint8_t)(counter >> 8);
  buf[15] = (uint8_t)(counter >> 0);

  uint8_t dig[32];
  mbedtls_sha256(buf, sizeof(buf), dig, 0);

  uint16_t t = (uint16_t)((dig[30] << 8) | dig[31]);
  if (t == 0) t = 1;
  return t;
}

// Validate incoming header for THIS node (all roles) using string token.
bool Core::verifyIncomingForLocal(const NowMsgHdr& h) const {
  if (!_cfg) return false;
  String myTok = _cfg->GetString(NVS_KEY_ESP_TOKEN, "");
  char myHex[NOW_TOKEN_HEX_LEN];
  if (!normalizeToken32(myTok, myHex)) return false;

  // Compare 32 bytes (case-insensitive)
  return tokenEq32(h.token_hex, myHex);
}

// Resolve expected token for a peer MAC (role-aware) → ASCII hex (32)
bool Core::expectedTokenForMac(const uint8_t peerMac[6], char token_hex_out[NOW_TOKEN_HEX_LEN]) const {
  if (!_cfg) return false;

#ifdef NVS_ROLE_ICM
  return readRegistryTokenForMac_(peerMac, token_hex_out);

#elif defined(NVS_ROLE_SENS)
  // Prev
  {
    String macHex = _cfg->GetString(NVS_KEY_TOPO_PRVMAC, "");
    uint8_t reg[6];
    if (macStrToBytes(macHex.c_str(), reg) && macEq(peerMac, reg)) {
      String tok = _cfg->GetString(NVS_KEY_TOPO_PRVTOK, "");
      return normalizeToken32(tok, token_hex_out);
    }
  }
  // Next
  {
    String macHex = _cfg->GetString(NVS_KEY_TOPO_NXTMAC, "");
    uint8_t reg[6];
    if (macStrToBytes(macHex.c_str(), reg) && macEq(peerMac, reg)) {
      String tok = _cfg->GetString(NVS_KEY_TOPO_NXTTOK, "");
      return normalizeToken32(tok, token_hex_out);
    }
  }
  // Relay lists handled by higher layer
  return false;

#elif defined(NVS_ROLE_RELAY)
  // Boundary A
  {
    String macHex = _cfg->GetString(NVS_KEY_BND_SAMAC, "");
    uint8_t reg[6];
    if (macStrToBytes(macHex.c_str(), reg) && macEq(peerMac, reg)) {
      String tok = _cfg->GetString(NVS_KEY_BND_SATOK, "");
      return normalizeToken32(tok, token_hex_out);
    }
  }
  // Boundary B
  {
    String macHex = _cfg->GetString(NVS_KEY_BND_SBMAC, "");
    uint8_t reg[6];
    if (macStrToBytes(macHex.c_str(), reg) && macEq(peerMac, reg)) {
      String tok = _cfg->GetString(NVS_KEY_BND_SBTOK, "");
      return normalizeToken32(tok, token_hex_out);
    }
  }
  return false;

#else
  // PMS role — no peer tokens modeled (ICM only peer via web pairing normally)
  return false;
#endif
}

// ======================================================================
// Header helpers / sequencing
// ======================================================================
uint16_t Core::nextSeq() {
  uint16_t v = s_seq + 1;
  s_seq = (v == 0) ? 1 : v;
  return s_seq;
}

// ======================================================================
// Send status cache (tiny LRU-ish)
// ======================================================================
void Core::clearStats() {
  for (int i=0;i<kMaxStats;i++) {
    memset(_stats[i].mac, 0, 6);
    _stats[i].status = ESP_NOW_SEND_FAIL;
    _stats[i].ts_ms = 0;
    _stats[i].seen = false;
  }
}

Core::SendStat* Core::findOrAllocStat(const uint8_t mac[6]) {
  // existing
  for (int i=0;i<kMaxStats;i++) if (memcmp(_stats[i].mac, mac, 6) == 0) return &_stats[i];
  // empty or oldest
  int idx = -1; uint32_t oldestTs = 0xFFFFFFFFUL;
  for (int i=0;i<kMaxStats;i++) {
    if (_stats[i].ts_ms == 0) { idx = i; break; }
    if (_stats[i].ts_ms < oldestTs) { oldestTs = _stats[i].ts_ms; idx = i; }
  }
  if (idx >= 0) {
    memcpy(_stats[idx].mac, mac, 6);
    _stats[idx].seen = false;
    _stats[idx].ts_ms = nowMs();
    return &_stats[idx];
  }
  return nullptr;
}

const Core::SendStat* Core::findStat(const uint8_t mac[6]) const {
  for (int i=0;i<kMaxStats;i++) if (memcmp(_stats[i].mac, mac, 6) == 0) return &_stats[i];
  return nullptr;
}

// ======================================================================
// ESPNOW callbacks (thunks → instance)
// ======================================================================
void Core::onSendThunk(const uint8_t* mac, esp_now_send_status_t status) { if (s_self) s_self->onSend(mac, status); }
void Core::onRecvThunk(const uint8_t* mac, const uint8_t* data, int len)  { if (s_self) s_self->onRecv(mac, data, len); }

// ======================================================================
// ESPNOW handlers
// ======================================================================
void Core::onSend(const uint8_t* mac, esp_now_send_status_t status) {
  SendStat* st = findOrAllocStat(mac);
  if (st) { st->status = status; st->seen = true; st->ts_ms = nowMs(); }
  if (_log) {
    String m = String("send ") + (status==ESP_NOW_SEND_SUCCESS ? "OK " : "FAIL ") + macBytesToStr(mac);
    _log->event(ICMLogFS::DOM_ESPNOW,
                status==ESP_NOW_SEND_SUCCESS ? ICMLogFS::EV_INFO : ICMLogFS::EV_WARN,
                status==ESP_NOW_SEND_SUCCESS ? 130 : 131, m);
  }
}

void Core::onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (_log) _log->event(ICMLogFS::DOM_ESPNOW, ICMLogFS::EV_DEBUG, 140, String("rx from ") + macBytesToStr(mac) + " len=" + String(len));
  if (_userRecv) _userRecv(mac, data, len);
}

// ======================================================================
// Pairing helpers (node side, non-ICM roles)
// ======================================================================
bool Core::nodeApplyPairing(const uint8_t icmMac[6], uint8_t channel, const char token_hex[NOW_TOKEN_HEX_LEN]) {
  if (!_cfg) return false;

  // Normalize token and persist as lowercase hex
  char tokNorm[NOW_TOKEN_HEX_LEN];
  String tokStr(token_hex, NOW_TOKEN_HEX_LEN);
  if (!normalizeToken32(tokStr, tokNorm)) return false;

  String macStr = macBytesToStr(icmMac); macStr.replace(":", ""); // 12-hex compact
  _cfg->PutString(NVS_KEY_NET_ICMMAC, macStr);
  _cfg->PutInt   (NVS_KEY_NET_CHAN,   (int)channel);

  // Write exactly 32 lowercase hex chars
  String tokLower; tokLower.reserve(NOW_TOKEN_HEX_LEN);
  for (int i=0;i<NOW_TOKEN_HEX_LEN;i++) tokLower += tokNorm[i];
  _cfg->PutString(NVS_KEY_ESP_TOKEN, tokLower);

  _cfg->PutInt   (NVS_KEY_NET_PAIRED, 1);
  return true;
}

bool Core::nodeClearPairing() {
  if (!_cfg) return false;
  _cfg->PutInt(NVS_KEY_NET_PAIRED, 0);
  return true;
}

// ======================================================================
// ICM-only registry & live slots (existence-based; no sentinels)
// ======================================================================
#ifdef NVS_ROLE_ICM

// ---------- Queries using key existence ----------
bool Core::matchMacAgainstRegistry_(const uint8_t mac[6]) const {
  if (!_cfg) return false;

  // Sensors S01..S16
  for (uint8_t i=1;i<=NVS_MAX_SENS;i++){
    char kMac[7]; snprintf(kMac,sizeof(kMac),NVS_REG_S_MAC_FMT,i);
    if (!_cfg->Iskey(kMac)) continue;
    String macHex = _cfg->GetString(kMac, "");
    uint8_t reg[6]; if (macStrToBytes(macHex.c_str(), reg) && macEq(mac,reg)) return true;
  }
  // Relays R01..R16
  for (uint8_t i=1;i<=NVS_MAX_RELAY;i++){
    char kMac[7]; snprintf(kMac,sizeof(kMac),NVS_REG_R_MAC_FMT,i);
    if (!_cfg->Iskey(kMac)) continue;
    String macHex = _cfg->GetString(kMac, "");
    uint8_t reg[6]; if (macStrToBytes(macHex.c_str(), reg) && macEq(mac,reg)) return true;
  }
  // PMS P01
  if (_cfg->Iskey(NVS_REG_P_MAC_CONST)) {
    String macHex = _cfg->GetString(NVS_REG_P_MAC_CONST, "");
    uint8_t reg[6]; if (macStrToBytes(macHex.c_str(), reg) && macEq(mac,reg)) return true;
  }
  return false;
}

bool Core::readRegistryTokenForMac_(const uint8_t mac[6], char token_hex_out[NOW_TOKEN_HEX_LEN]) const {
  if (!_cfg) return false;

  // Sensors
  for (uint8_t i=1;i<=NVS_MAX_SENS;i++){
    char kMac[7], kTok[7]; snprintf(kMac,sizeof(kMac),NVS_REG_S_MAC_FMT,i); snprintf(kTok,sizeof(kTok),NVS_REG_S_TOK_FMT,i);
    if (!_cfg->Iskey(kMac)) continue;
    String macHex = _cfg->GetString(kMac, "");
    uint8_t reg[6]; if (macStrToBytes(macHex.c_str(), reg) && macEq(mac,reg)) {
      if (!_cfg->Iskey(kTok)) return false;
      String tok = _cfg->GetString(kTok, "");
      return normalizeToken32(tok, token_hex_out);
    }
  }
  // Relays
  for (uint8_t i=1;i<=NVS_MAX_RELAY;i++){
    char kMac[7], kTok[7]; snprintf(kMac,sizeof(kMac),NVS_REG_R_MAC_FMT,i); snprintf(kTok,sizeof(kTok),NVS_REG_R_TOK_FMT,i);
    if (!_cfg->Iskey(kMac)) continue;
    String macHex = _cfg->GetString(kMac, "");
    uint8_t reg[6]; if (macStrToBytes(macHex.c_str(), reg) && macEq(mac,reg)) {
      if (!_cfg->Iskey(kTok)) return false;
      String tok = _cfg->GetString(kTok, "");
      return normalizeToken32(tok, token_hex_out);
    }
  }
  // PMS
  if (_cfg->Iskey(NVS_REG_P_MAC_CONST)) {
    String macHex = _cfg->GetString(NVS_REG_P_MAC_CONST, "");
    uint8_t reg[6]; if (macStrToBytes(macHex.c_str(), reg) && macEq(mac,reg)) {
      if (!_cfg->Iskey(NVS_REG_P_TOK_CONST)) return false;
      String tok = _cfg->GetString(NVS_REG_P_TOK_CONST, "");
      return normalizeToken32(tok, token_hex_out);
    }
  }
  return false;
}

// Public wrappers
bool Core::macInRegistry     (const uint8_t mac[6]) const { return matchMacAgainstRegistry_(mac); }
bool Core::findSlaveToken    (const uint8_t mac[6], char token_hex_out[NOW_TOKEN_HEX_LEN]) const { return readRegistryTokenForMac_(mac, token_hex_out); }
bool Core::tokenValidForSlave(const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN]) const {
  char tok[NOW_TOKEN_HEX_LEN];
  if (!readRegistryTokenForMac_(mac, tok)) return false;
  return tokenEq32(tok, token_hex);
}
bool Core::macIsZero(const uint8_t mac[6]) {
  if (!mac) return true;          // treat null as “zero” for callers’ convenience
  for (int i = 0; i < 6; ++i) {
    if (mac[i] != 0) return false;
  }
  return true;
}

// ---------- Registry set/clear (erase on unpair) ----------
bool Core::icmRegistrySetSensor(uint8_t idx1to16, const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN]) {
  if (!_cfg || idx1to16 < 1 || idx1to16 > NVS_MAX_SENS) return false;
  char kMac[7], kTok[7];
  snprintf(kMac, sizeof(kMac), NVS_REG_S_MAC_FMT, idx1to16);
  snprintf(kTok, sizeof(kTok), NVS_REG_S_TOK_FMT, idx1to16);
  String macHex = macBytesToStr(mac); macHex.replace(":", "");

  char tokNorm[NOW_TOKEN_HEX_LEN];
  String tokStr(token_hex, NOW_TOKEN_HEX_LEN);
  if (!normalizeToken32(tokStr, tokNorm)) return false;

  String tokLower; tokLower.reserve(NOW_TOKEN_HEX_LEN);
  for (int i=0;i<NOW_TOKEN_HEX_LEN;i++) tokLower += tokNorm[i];

  _cfg->PutString(kMac, macHex);
  _cfg->PutString(kTok, tokLower);
  return true;
}

bool Core::icmRegistrySetRelay(uint8_t idx1to16, const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN]) {
  if (!_cfg || idx1to16 < 1 || idx1to16 > NVS_MAX_RELAY) return false;
  char kMac[7], kTok[7];
  snprintf(kMac, sizeof(kMac), NVS_REG_R_MAC_FMT, idx1to16);
  snprintf(kTok, sizeof(kTok), NVS_REG_R_TOK_FMT, idx1to16);
  String macHex = macBytesToStr(mac); macHex.replace(":", "");

  char tokNorm[NOW_TOKEN_HEX_LEN];
  String tokStr(token_hex, NOW_TOKEN_HEX_LEN);
  if (!normalizeToken32(tokStr, tokNorm)) return false;

  String tokLower; tokLower.reserve(NOW_TOKEN_HEX_LEN);
  for (int i=0;i<NOW_TOKEN_HEX_LEN;i++) tokLower += tokNorm[i];

  _cfg->PutString(kMac, macHex);
  _cfg->PutString(kTok, tokLower);
  return true;
}

bool Core::icmRegistrySetPower(const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN]) {
  if (!_cfg) return false;
  String macHex = macBytesToStr(mac); macHex.replace(":", "");

  char tokNorm[NOW_TOKEN_HEX_LEN];
  String tokStr(token_hex, NOW_TOKEN_HEX_LEN);
  if (!normalizeToken32(tokStr, tokNorm)) return false;

  String tokLower; tokLower.reserve(NOW_TOKEN_HEX_LEN);
  for (int i=0;i<NOW_TOKEN_HEX_LEN;i++) tokLower += tokNorm[i];

  _cfg->PutString(NVS_REG_P_MAC_CONST, macHex);
  _cfg->PutString(NVS_REG_P_TOK_CONST, tokLower);
  return true;
}

bool Core::icmRegistryClearSensor(uint8_t idx1to16) {
  if (!_cfg || idx1to16 < 1 || idx1to16 > NVS_MAX_SENS) return false;
  char kMac[7], kTok[7];
  snprintf(kMac,sizeof(kMac),NVS_REG_S_MAC_FMT,idx1to16);
  snprintf(kTok,sizeof(kTok),NVS_REG_S_TOK_FMT,idx1to16);
  bool any=false;
  if (_cfg->Iskey(kMac)) { _cfg->RemoveKey(kMac); any=true; }
  if (_cfg->Iskey(kTok)) { _cfg->RemoveKey(kTok); any=true; }
  return any;
}

bool Core::icmRegistryClearRelay(uint8_t idx1to16) {
  if (!_cfg || idx1to16 < 1 || idx1to16 > NVS_MAX_RELAY) return false;
  char kMac[7], kTok[7];
  snprintf(kMac,sizeof(kMac),NVS_REG_R_MAC_FMT,idx1to16);
  snprintf(kTok,sizeof(kTok),NVS_REG_R_TOK_FMT,idx1to16);
  bool any=false;
  if (_cfg->Iskey(kMac)) { _cfg->RemoveKey(kMac); any=true; }
  if (_cfg->Iskey(kTok)) { _cfg->RemoveKey(kTok); any=true; }
  return any;
}

bool Core::icmRegistryClearPower() {
  if (!_cfg) return false;
  bool any=false;
  if (_cfg->Iskey(NVS_REG_P_MAC_CONST)) { _cfg->RemoveKey(NVS_REG_P_MAC_CONST); any=true; }
  if (_cfg->Iskey(NVS_REG_P_TOK_CONST)) { _cfg->RemoveKey(NVS_REG_P_TOK_CONST); any=true; }
  return any;
}

// ---------- Live slot cache ----------
void Core::slotsReset() {
  for (int i=0;i<NOW_MAX_SENSORS;i++){ memset(&sensors[i],0,sizeof(NodeSlot)); sensors[i].kind=NOW_KIND_SENS;  sensors[i].index=i+1; }
  for (int i=0;i<NOW_MAX_RELAYS; i++){ memset(&relays [i],0,sizeof(NodeSlot)); relays [i].kind=NOW_KIND_RELAY; relays [i].index=i+1; }
  for (int i=0;i<NOW_MAX_POWER;  i++){ memset(&pms    [i],0,sizeof(NodeSlot)); pms    [i].kind=NOW_KIND_PMS;   pms    [i].index=i+1; }
}

bool Core::slotsLoadFromRegistry() {
  if (!_cfg) return false;
  slotsReset();

  // Sensors
  for (uint8_t i=1;i<=NVS_MAX_SENS;i++){
    char kMac[7], kTok[7]; snprintf(kMac,sizeof(kMac),NVS_REG_S_MAC_FMT,i); snprintf(kTok,sizeof(kTok),NVS_REG_S_TOK_FMT,i);
    if (!_cfg->Iskey(kMac)) continue;
    String macHex = _cfg->GetString(kMac, "");
    uint8_t mac[6]; if (!macStrToBytes(macHex.c_str(), mac)) continue;
    String tok = _cfg->GetString(kTok, "");
    char tokHex[NOW_TOKEN_HEX_LEN]; if (!normalizeToken32(tok, tokHex)) continue;
    memcpy(sensors[i-1].mac, mac, 6);
    for (int j=0;j<NOW_TOKEN_HEX_LEN;j++) sensors[i-1].token_hex[j] = tokHex[j];
    sensors[i-1].present = false;
  }

  // Relays
  for (uint8_t i=1;i<=NVS_MAX_RELAY;i++){
    char kMac[7], kTok[7]; snprintf(kMac,sizeof(kMac),NVS_REG_R_MAC_FMT,i); snprintf(kTok,sizeof(kTok),NVS_REG_R_TOK_FMT,i);
    if (!_cfg->Iskey(kMac)) continue;
    String macHex = _cfg->GetString(kMac, "");
    uint8_t mac[6]; if (!macStrToBytes(macHex.c_str(), mac)) continue;
    String tok = _cfg->GetString(kTok, "");
    char tokHex[NOW_TOKEN_HEX_LEN]; if (!normalizeToken32(tok, tokHex)) continue;
    memcpy(relays[i-1].mac, mac, 6);
    for (int j=0;j<NOW_TOKEN_HEX_LEN;j++) relays[i-1].token_hex[j] = tokHex[j];
    relays[i-1].present = false;
  }

  // PMS
  if (_cfg->Iskey(NVS_REG_P_MAC_CONST)) {
    String macHex = _cfg->GetString(NVS_REG_P_MAC_CONST, "");
    uint8_t mac[6]; if (macStrToBytes(macHex.c_str(), mac)) {
      String tok = _cfg->GetString(NVS_REG_P_TOK_CONST, "");
      char tokHex[NOW_TOKEN_HEX_LEN]; if (!normalizeToken32(tok, tokHex)) {
        // keep PMS slot empty if token missing/invalid
      } else {
        memcpy(pms[0].mac, mac, 6);
        for (int j=0;j<NOW_TOKEN_HEX_LEN;j++) pms[0].token_hex[j] = tokHex[j];
        pms[0].present = false;
      }
    }
  }
  return true;
}

NodeSlot* Core::slotFindByMac(const uint8_t mac[6]) {
  for (int i=0;i<NOW_MAX_SENSORS;i++) if (macEq(mac,sensors[i].mac)) return &sensors[i];
  for (int i=0;i<NOW_MAX_RELAYS;i++)  if (macEq(mac,relays [i].mac)) return &relays [i];
  for (int i=0;i<NOW_MAX_POWER;i++)   if (macEq(mac,pms    [i].mac)) return &pms    [i];
  return nullptr;
}

void Core::slotMarkSeen(const uint8_t mac[6], uint16_t state_flags, int8_t rssi, uint32_t ts_ms) {
  NodeSlot* s = slotFindByMac(mac); if (!s) return;
  s->state_flags = state_flags;
  s->lastRSSI    = rssi;
  s->lastSeenMs  = ts_ms;
  s->present     = true;
}

// ---------- Auto-slot helpers (no index passed by caller) ----------
bool Core::icmRegistryIndexOfSensorMac(const uint8_t mac[6], uint8_t& idxOut) {
  if (!_cfg) return false;
  for (uint8_t i=1;i<=NVS_MAX_SENS;i++){
    char kMac[7]; snprintf(kMac,sizeof(kMac),NVS_REG_S_MAC_FMT,i);
    if (!_cfg->Iskey(kMac)) continue;
    String macHex = _cfg->GetString(kMac, "");
    uint8_t reg[6]; if (macStrToBytes(macHex.c_str(), reg) && macEq(mac,reg)) { idxOut=i; return true; }
  }
  return false;
}

bool Core::icmRegistryIndexOfRelayMac(const uint8_t mac[6], uint8_t& idxOut) {
  if (!_cfg) return false;
  for (uint8_t i=1;i<=NVS_MAX_RELAY;i++){
    char kMac[7]; snprintf(kMac,sizeof(kMac),NVS_REG_R_MAC_FMT,i);
    if (!_cfg->Iskey(kMac)) continue;
    String macHex = _cfg->GetString(kMac, "");
    uint8_t reg[6]; if (macStrToBytes(macHex.c_str(), reg) && macEq(mac,reg)) { idxOut=i; return true; }
  }
  return false;
}

bool Core::icmRegistryFindFreeSensor(uint8_t& idxOut) {
  if (!_cfg) return false;
  for (uint8_t i=1;i<=NVS_MAX_SENS;i++){
    char kMac[7]; snprintf(kMac,sizeof(kMac),NVS_REG_S_MAC_FMT,i);
    if (!_cfg->Iskey(kMac)) { idxOut = i; return true; }
  }
  return false;
}

bool Core::icmRegistryFindFreeRelay(uint8_t& idxOut) {
  if (!_cfg) return false;
  for (uint8_t i=1;i<=NVS_MAX_RELAY;i++){
    char kMac[7]; snprintf(kMac,sizeof(kMac),NVS_REG_R_MAC_FMT,i);
    if (!_cfg->Iskey(kMac)) { idxOut = i; return true; }
  }
  return false;
}

bool Core::icmRegistryAutoAddSensor(const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN], uint8_t& idxOut) {
  if (!_cfg || !mac) return false;
  if (icmRegistryIndexOfSensorMac(mac, idxOut)) {
    char kTok[7]; snprintf(kTok,sizeof(kTok),NVS_REG_S_TOK_FMT,idxOut);
    // Normalize and write token string
    char tokNorm[NOW_TOKEN_HEX_LEN];
    String tokStr(token_hex, NOW_TOKEN_HEX_LEN);
    if (!normalizeToken32(tokStr, tokNorm)) return false;
    String tokLower; tokLower.reserve(NOW_TOKEN_HEX_LEN);
    for (int i=0;i<NOW_TOKEN_HEX_LEN;i++) tokLower += tokNorm[i];
    _cfg->PutString(kTok, tokLower);
    return true;
  }
  if (!icmRegistryFindFreeSensor(idxOut)) return false;       // 16/16 used
  return icmRegistrySetSensor(idxOut, mac, token_hex);
}

bool Core::icmRegistryAutoAddRelay(const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN], uint8_t& idxOut) {
  if (!_cfg || !mac) return false;
  if (icmRegistryIndexOfRelayMac(mac, idxOut)) {
    char kTok[7]; snprintf(kTok,sizeof(kTok),NVS_REG_R_TOK_FMT,idxOut);
    char tokNorm[NOW_TOKEN_HEX_LEN];
    String tokStr(token_hex, NOW_TOKEN_HEX_LEN);
    if (!normalizeToken32(tokStr, tokNorm)) return false;
    String tokLower; tokLower.reserve(NOW_TOKEN_HEX_LEN);
    for (int i=0;i<NOW_TOKEN_HEX_LEN;i++) tokLower += tokNorm[i];
    _cfg->PutString(kTok, tokLower);
    return true;
  }
  if (!icmRegistryFindFreeRelay(idxOut)) return false;        // 16/16 used
  return icmRegistrySetRelay(idxOut, mac, token_hex);
}

bool Core::icmRegistryAutoSetPower(const uint8_t mac[6], const char token_hex[NOW_TOKEN_HEX_LEN]) {
  if (!_cfg || !mac) return false;
  // Single PMS slot; overwrite or create
  return icmRegistrySetPower(mac, token_hex);
}

bool Core::icmRegistryCountSensors(uint8_t& countOut) {
  if (!_cfg) return false;
  uint8_t c=0; for (uint8_t i=1;i<=NVS_MAX_SENS;i++){ char kMac[7]; snprintf(kMac,sizeof(kMac),NVS_REG_S_MAC_FMT,i); if (_cfg->Iskey(kMac)) ++c; }
  countOut = c; return true;
}

bool Core::icmRegistryCountRelays(uint8_t& countOut) {
  if (!_cfg) return false;
  uint8_t c=0; for (uint8_t i=1;i<=NVS_MAX_RELAY;i++){ char kMac[7]; snprintf(kMac,sizeof(kMac),NVS_REG_R_MAC_FMT,i); if (_cfg->Iskey(kMac)) ++c; }
  countOut = c; return true;
}

#endif // NVS_ROLE_ICM

} // namespace NwCore
