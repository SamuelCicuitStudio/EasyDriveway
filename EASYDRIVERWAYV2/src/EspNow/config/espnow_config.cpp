// config/espnow_config.cpp
#include "EspNow/EspNowStack.h"
#include "NVS/NvsManager.h"
#include "NVS/NVSConfig.h"

#include <cstring>  // std::memset, std::memcpy
#include <cctype>   // std::toupper

namespace espnow {

// --- small helpers -----------------------------------------------------------

static bool parse_mac_any(const String& s_in, uint8_t out[6]) {
  // Accept "AA:BB:CC:DD:EE:FF" or "AABBCCDDEEFF" (case-insensitive).
  String s = s_in;
  // Remove separators
  String flat;
  flat.reserve(s.length());
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == ':' || c == '-' || c == ' ') continue;
    flat += c;
  }
  if (flat.length() != 12) return false;

  auto hexv = [](char c)->int {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)std::toupper((unsigned char)c);
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
  };

  for (int i = 0; i < 6; ++i) {
    int hi = hexv(flat[2*i]);
    int lo = hexv(flat[2*i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

static bool parse_hex32_to_16(const String& s, uint8_t out[16]) {
  if (s.length() != 32) return false;
  auto hexv = [](char c)->int {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)std::toupper((unsigned char)c);
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
  };
  for (int i = 0; i < 16; ++i) {
    int hi = hexv(s[2*i]);
    int lo = hexv(s[2*i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

static void put_u32_le(uint32_t v, uint8_t out[4]) {
  out[0] = (uint8_t)(v & 0xFF);
  out[1] = (uint8_t)((v >> 8) & 0xFF);
  out[2] = (uint8_t)((v >> 16) & 0xFF);
  out[3] = (uint8_t)((v >> 24) & 0xFF);
}

// --- public loader -----------------------------------------------------------
//
// Loads KIND__/ICMMAC/CHAN__/TOKEN_/PAIRED into EspNowSettings.
// Returns true on success (even with safe defaults).
//
// Notes:
//  - proto_ver is set to NOW_PROTO_VER (v2H).
//  - sender_role comes from NVS_KEY_KIND (your roles already map to wire codes).
//  - device_token[16]:
//      * If there is a 32-hex string stored under NVS_KEY_TOKEN (future-proof), it is parsed.
//      * Else we read TOKEN_ as int and place it in the first 4 bytes (LE), rest zero.
//  - topo_ver is initialized to 0; ICM will bump it on TOPO_PUSH.
//
bool load_settings_from_nvs(NvsManager& nvs, EspNowSettings& out, bool* paired_opt /*=nullptr*/) {
  std::memset(&out, 0, sizeof(out));
  out.proto_ver = NOW_PROTO_VER;

  // KIND__ → sender_role (already your numeric role code)
  const int kind = nvs.GetInt(NVS_KEY_KIND, (int)NVS_DEF_KIND);
  out.sender_role = (uint8_t)(kind & 0xFF);

  // CHAN__ → channel (1..13; default from your NVS defaults)
  const int ch = nvs.GetInt(NVS_KEY_CHAN, (int)NVS_DEF_CHAN);
  out.channel = (uint8_t)((ch >= 1 && ch <= 13) ? ch : NVS_DEF_CHAN);

  // ICMMAC → controller MAC (string; accept AA:BB:.. or AABB..)
  std::memset(out.icm_mac, 0x00, sizeof(out.icm_mac));
  {
    const String icm = nvs.GetString(NVS_KEY_ICMMAC, NVS_DEF_ICMMAC);
    (void)parse_mac_any(icm, out.icm_mac); // if it fails, keeps zeros
  }

  // TOKEN_ → device token:
  // Try string-hex first (future pairing); else fallback to int.
  std::memset(out.device_token, 0x00, sizeof(out.device_token));
  {
    String tok_str = nvs.GetString(NVS_KEY_TOKEN, ""); // may be empty or numeric-as-string
    bool ok_hex = false;
    if (tok_str.length() == 32) {
      ok_hex = parse_hex32_to_16(tok_str, out.device_token);
    }
    if (!ok_hex) {
      // numeric path (legacy PutInt)
      const uint32_t tok_u32 = (uint32_t)nvs.GetInt(NVS_KEY_TOKEN, (int)NVS_DEF_TOKEN);
      put_u32_le(tok_u32, out.device_token); // first 4 bytes; rest remain zero
    }
  }

  // PAIRED → optional out flag (handy for your app's state machine)
  if (paired_opt) {
    *paired_opt = nvs.GetBool(NVS_KEY_PAIRED, (bool)NVS_DEF_PAIRED);
  }

  // topo_ver starts at 0; updated after first TOPO_PUSH
  out.topo_ver = 0;

  return true;
}

} // namespace espnow
