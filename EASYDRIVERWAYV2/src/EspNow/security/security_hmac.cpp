// security/security_hmac.cpp
//
// HMAC + replay helpers for the ESPNOW v2H stack.
// Tag = Trunc_96( HMAC-SHA256(K_app, NowHeader||NowAuth128||[Topo?+Payload]||Nonce) )

#if __has_include("EspNowStack.h")
  #include "EspNowStack.h"
#else
  #include "EspNow/EspNowStack.h"
#endif

#if __has_include("EspNowAPI.h")
  #include "EspNowAPI.h"
#else
  #include "EspNow/EspNowAPI.h"
#endif

#include <cstdint>
#include <cstddef>
#include <cstring>

#if defined(ESP_PLATFORM) || (defined(ARDUINO) && defined(ESP32))
  #include <mbedtls/md.h>
  #include <mbedtls/sha256.h>
  #define ESPNOW_HAVE_MBEDTLS 1
#else
  #if __has_include(<mbedtls/md.h>)
    #include <mbedtls/md.h>
    #include <mbedtls/sha256.h>
    #define ESPNOW_HAVE_MBEDTLS 1
  #else
    #define ESPNOW_HAVE_MBEDTLS 0
  #endif
#endif

namespace espnow {

// ---------------------------
// Internal state / constants
// ---------------------------

static EspNowSecrets g_secrets{};
static uint8_t g_app_key[32] = {0};
static bool    g_app_key_ready = false;

// Only used by the stub (when PMK/LMK aren’t provisioned yet).
static const uint8_t k_stub_salt[16] = {
  0x45,0x44,0x5F,0x76,0x32,0x48,0x5F,0x53,0x41,0x4C,0x54,0x00,0x00,0x00,0x00,0x01
};

void security_set_secrets(const EspNowSecrets& s) {
  g_secrets = s;
  g_app_key_ready = false;
}

// Replay window per peer (tiny table; replace with a map if you need more)
struct PeerNonceState {
  uint8_t mac[6];
  uint64_t last48;
  bool in_use;
};
static PeerNonceState g_nonce_tbl[8];

static inline uint64_t clamp48(uint64_t v) { return (v & 0xFFFFFFFFFFFFULL); }

static PeerNonceState* find_or_add_peer(const uint8_t mac[6]) {
  for (auto& s : g_nonce_tbl) {
    if (s.in_use && std::memcmp(s.mac, mac, 6) == 0) return &s;
  }
  for (auto& s : g_nonce_tbl) {
    if (!s.in_use) {
      std::memcpy(s.mac, mac, 6);
      s.last48 = 0;
      s.in_use = true;
      return &s;
    }
  }
  std::memcpy(g_nonce_tbl[0].mac, mac, 6);
  g_nonce_tbl[0].last48 = 0;
  g_nonce_tbl[0].in_use = true;
  return &g_nonce_tbl[0];
}

static bool replay_ok_and_update(const uint8_t mac[6], const uint8_t nonce6[NOW_HMAC_NONCE_LEN], uint16_t window) {
  uint64_t n = 0;
  for (int i = 0; i < NOW_HMAC_NONCE_LEN; ++i) n |= (static_cast<uint64_t>(nonce6[i]) << (8 * i));
  n = clamp48(n);

  PeerNonceState* st = find_or_add_peer(mac);
  const uint64_t last = clamp48(st->last48);

  if (n > last) { st->last48 = n; return true; }
  if (window > 0) {
    const uint64_t low = (last >= window) ? (last - window) : 0;
    if (n >= low && n <= last) return true;
  }
  return false;
}

// Router can use these if you want stronger gates there
static bool is_privileged(uint8_t msg_type) {
  return (msg_type == NOW_MT_TOPO_PUSH)    ||
         (msg_type == NOW_MT_NET_SET_CHAN) ||
         (msg_type == NOW_MT_TIME_SYNC)    ||
         (msg_type == NOW_MT_FW_BEGIN)     ||
         (msg_type == NOW_MT_FW_CHUNK)     ||
         (msg_type == NOW_MT_FW_COMMIT)    ||
         (msg_type == NOW_MT_FW_ABORT);
}
static bool privileged_allowed(const NowHeader& h, const uint8_t icm_mac[6]) {
  if (!is_privileged(h.msg_type)) return true;
  if (h.sender_role != NOW_KIND_ICM) return false;
  return (std::memcmp(h.sender_mac, icm_mac, 6) == 0);
}

// ---------------------------
// HMAC glue
// ---------------------------

#if ESPNOW_HAVE_MBEDTLS
static bool hmac_sha256(const uint8_t* key, size_t key_len,
                        const uint8_t* a, size_t alen,
                        const uint8_t* b, size_t blen,
                        const uint8_t* c, size_t clen,
                        const uint8_t* d, size_t dlen,
                        uint8_t out32[32]) {
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!info) return false;

  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);

  if (mbedtls_md_setup(&ctx, info, 1) != 0) { mbedtls_md_free(&ctx); return false; }
  if (mbedtls_md_hmac_starts(&ctx, key, key_len) != 0) { mbedtls_md_free(&ctx); return false; }

  if (a && alen) mbedtls_md_hmac_update(&ctx, a, alen);
  if (b && blen) mbedtls_md_hmac_update(&ctx, b, blen);
  if (c && clen) mbedtls_md_hmac_update(&ctx, c, clen);
  if (d && dlen) mbedtls_md_hmac_update(&ctx, d, dlen);

  const int fin = mbedtls_md_hmac_finish(&ctx, out32);
  mbedtls_md_free(&ctx);
  return fin == 0;
}
#else
static bool hmac_sha256(const uint8_t* /*key*/, size_t /*key_len*/,
                        const uint8_t* /*a*/, size_t /*alen*/,
                        const uint8_t* /*b*/, size_t /*blen*/,
                        const uint8_t* /*c*/, size_t /*clen*/,
                        const uint8_t* /*d*/, size_t /*dlen*/,
                        uint8_t out32[32]) {
  std::memset(out32, 0x00, 32);
  return true;
}
#endif

// ---- KDFs -------------------------------------------------------------------

// Fallback KDF (only during bring-up without PMK/LMK provisioned)
static void derive_app_key_stub(const NowHeader& h, const NowAuth128& a, uint8_t out32[32]) {
#if ESPNOW_HAVE_MBEDTLS
  uint8_t msg[16 + 6 + 1];
  std::memcpy(msg, a.device_token128, 16);
  std::memcpy(msg + 16, h.sender_mac, 6);
  msg[22] = h.sender_role;

  (void)hmac_sha256(k_stub_salt, sizeof(k_stub_salt),
                    msg, sizeof(msg),
                    nullptr, 0,
                    nullptr, 0,
                    nullptr, 0,
                    out32);
#else
  std::memset(out32, 0xA5, 32);
#endif
}

// Real K_app when PMK/LMK are provisioned:
//   K_app = HMAC-SHA256( (PMK||"APPK"),
//                        (LMK||device_token),
//                        SALT,
//                        nil )
static void kdf_app_key(const NowAuth128& a, uint8_t out32[32]) {
  uint8_t k[16 + 4]; // PMK || "APPK"
  std::memcpy(k, g_secrets.pmk, 16);
  k[16] = 'A'; k[17] = 'P'; k[18] = 'P'; k[19] = 'K';

  uint8_t msg[16 + 16]; // LMK || device_token
  std::memcpy(msg,       g_secrets.lmk,        16);
  std::memcpy(msg + 16,  a.device_token128,    16);

  (void)hmac_sha256(k, sizeof(k),
                    msg, sizeof(msg),
                    g_secrets.salt, sizeof(g_secrets.salt),
                    nullptr, 0,
                    nullptr, 0,        // <-- FIX: add the 4th (empty) data segment
                    out32);
}

// ---------------------------------------------------------------------------

void deriveKeys() {
  g_app_key_ready = false; // derive per-frame (needs device_token from NowAuth)
}

bool verifyHmac(const NowHeader& h,
                const NowAuth128& a,
                const NowSecTrailer& s,
                const unsigned char* payload,
                unsigned short payload_len) {
  // Replay guard
  if (!replay_ok_and_update(h.sender_mac, s.nonce, /*window=*/64)) {
    return false;
  }

  // Derive per-frame K_app (uses device_token from NowAuth)
  uint8_t app_key[32];
  if (g_secrets.has_pmk && g_secrets.has_lmk) {
    kdf_app_key(a, app_key);
  } else {
    derive_app_key_stub(h, a, app_key);
  }

  // HMAC over: Header || Auth || [Topo?+Payload (as provided)] || Nonce
  const uint8_t* ph = reinterpret_cast<const uint8_t*>(&h);
  const uint8_t* pa = reinterpret_cast<const uint8_t*>(&a);
  const uint8_t* pn = reinterpret_cast<const uint8_t*>(s.nonce);

  uint8_t digest[32];
  if (!hmac_sha256(app_key, sizeof(app_key),
                   ph, sizeof(NowHeader),
                   pa, sizeof(NowAuth128),
                   payload, static_cast<size_t>(payload_len),
                   pn, NOW_HMAC_NONCE_LEN,
                   digest)) {
    return false;
  }

  // Constant-time compare (truncate to 96 bits)
  volatile uint8_t diff = 0;
  for (int i = 0; i < NOW_HMAC_TAG_LEN; ++i) diff |= (digest[i] ^ s.tag[i]);
  return (diff == 0);
}

} // namespace espnow
