// security/security_hmac.cpp
// Self-contained SHA-256 + HMAC-SHA256 (portable, no external libs)
// Implements:
//   - verifyHmac(...)
//   - signHmac(...)
//   - hmac_sha256(...) used internally and available to other modules
//
// HMAC input order (spec):
//   NowHeader || NowAuth128 || [NowTopoToken128?] || payload || nonce
//
// Key derivation (bring-up KDF; replace later if desired):
//   app_key = HMAC-SHA256( pmk || lmk , token128 || salt )   // 32 bytes
//
// Notes:
// - Truncates HMAC to 12 bytes for the wire tag (NOW_HMAC_TAG_LEN).
// - Includes a tiny SHA-256 implementation (FIPS 180-4 style).
// - If you later want to use mbedTLS, you can #ifdef out the local code and call mbedtls_md_hmac.

#include "EspNow/EspNowStack.h"
#include "EspNow/EspNowAPI.h"

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace espnow {

// ======== Size guards (wire compatibility) ===================================
static_assert(sizeof(NowHeader)       == 23, "NowHeader must be 23 bytes");
static_assert(sizeof(NowAuth128)      == 16, "NowAuth128 must be 16 bytes");
static_assert(sizeof(NowTopoToken128) == 16, "NowTopoToken128 must be 16 bytes");
static_assert(sizeof(NowSecTrailer)   == (NOW_HMAC_NONCE_LEN + NOW_HMAC_TAG_LEN),
              "NowSecTrailer must be 18 bytes");

// ======== Minimal SHA-256 implementation =====================================
// Tiny, portable, public-domain style SHA-256 (straightforward FIPS 180-4).
// Good enough for firmware use; replace with HW accel/mbedTLS later if desired.

struct sha256_ctx {
  uint32_t state[8];
  uint64_t bitlen;
  uint8_t  buffer[64];
  size_t   buffer_len;
};

static inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z){ return (x & y) ^ (~x & z); }
static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z){ return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t bsig0(uint32_t x){ return rotr(x,2) ^ rotr(x,13) ^ rotr(x,22); }
static inline uint32_t bsig1(uint32_t x){ return rotr(x,6) ^ rotr(x,11) ^ rotr(x,25); }
static inline uint32_t ssig0(uint32_t x){ return rotr(x,7) ^ rotr(x,18) ^ (x >> 3); }
static inline uint32_t ssig1(uint32_t x){ return rotr(x,17) ^ rotr(x,19) ^ (x >> 10); }

static const uint32_t K256[64] = {
  0x428a2f98ul,0x71374491ul,0xb5c0fbcful,0xe9b5dba5ul,0x3956c25bul,0x59f111f1ul,0x923f82a4ul,0xab1c5ed5ul,
  0xd807aa98ul,0x12835b01ul,0x243185beul,0x550c7dc3ul,0x72be5d74ul,0x80deb1feul,0x9bdc06a7ul,0xc19bf174ul,
  0xe49b69c1ul,0xefbe4786ul,0x0fc19dc6ul,0x240ca1ccul,0x2de92c6ful,0x4a7484aaul,0x5cb0a9dcul,0x76f988daul,
  0x983e5152ul,0xa831c66dul,0xb00327c8ul,0xbf597fc7ul,0xc6e00bf3ul,0xd5a79147ul,0x06ca6351ul,0x14292967ul,
  0x27b70a85ul,0x2e1b2138ul,0x4d2c6dfcul,0x53380d13ul,0x650a7354ul,0x766a0abbul,0x81c2c92eul,0x92722c85ul,
  0xa2bfe8a1ul,0xa81a664bul,0xc24b8b70ul,0xc76c51a3ul,0xd192e819ul,0xd6990624ul,0xf40e3585ul,0x106aa070ul,
  0x19a4c116ul,0x1e376c08ul,0x2748774cul,0x34b0bcb5ul,0x391c0cb3ul,0x4ed8aa4aul,0x5b9cca4ful,0x682e6ff3ul,
  0x748f82eeul,0x78a5636ful,0x84c87814ul,0x8cc70208ul,0x90befffaul,0xa4506cebul,0xbef9a3f7ul,0xc67178f2ul
};


static void sha256_init(sha256_ctx& c) {
  c.state[0]=0x6a09e667ul; c.state[1]=0xbb67ae85ul; c.state[2]=0x3c6ef372ul; c.state[3]=0xa54ff53aul;
  c.state[4]=0x510e527ful; c.state[5]=0x9b05688cul; c.state[6]=0x1f83d9abul; c.state[7]=0x5be0cd19ul;
  c.bitlen = 0;
  c.buffer_len = 0;
}

static void sha256_compress(sha256_ctx& c, const uint8_t block[64]) {
  uint32_t w[64];
  for (int i=0;i<16;++i) {
    w[i] = (uint32_t)block[i*4+0]<<24 | (uint32_t)block[i*4+1]<<16 | (uint32_t)block[i*4+2]<<8 | (uint32_t)block[i*4+3];
  }
  for (int i=16;i<64;++i) w[i] = ssig1(w[i-2]) + w[i-7] + ssig0(w[i-15]) + w[i-16];

  uint32_t a=c.state[0],b=c.state[1],c0=c.state[2],d=c.state[3],e=c.state[4],f=c.state[5],g=c.state[6],h=c.state[7];

  for (int i=0;i<64;++i) {
    uint32_t t1 = h + bsig1(e) + ch(e,f,g) + K256[i] + w[i];
    uint32_t t2 = bsig0(a) + maj(a,b,c0);
    h=g; g=f; f=e; e=d + t1; d=c0; c0=b; b=a; a=t1 + t2;
  }

  c.state[0] += a; c.state[1] += b; c.state[2] += c0; c.state[3] += d;
  c.state[4] += e; c.state[5] += f; c.state[6] += g; c.state[7] += h;
}

static void sha256_update(sha256_ctx& c, const uint8_t* data, size_t len) {
  while (len > 0) {
    size_t to_copy = 64 - c.buffer_len;
    if (to_copy > len) to_copy = len;
    std::memcpy(c.buffer + c.buffer_len, data, to_copy);
    c.buffer_len += to_copy;
    data += to_copy;
    len  -= to_copy;

    if (c.buffer_len == 64) {
      sha256_compress(c, c.buffer);
      c.bitlen += 512; // 64 bytes * 8
      c.buffer_len = 0;
    }
  }
}

static void sha256_final(sha256_ctx& c, uint8_t out[32]) {
  // append 0x80, then pad with zeros, then length (big-endian)
  uint64_t total_bits = c.bitlen + (uint64_t)c.buffer_len * 8ull;

  // append 0x80
  c.buffer[c.buffer_len++] = 0x80;

  // pad with zeros until we have room for 8-byte length
  if (c.buffer_len > 56) {
    while (c.buffer_len < 64) c.buffer[c.buffer_len++] = 0x00;
    sha256_compress(c, c.buffer);
    c.buffer_len = 0;
  }
  while (c.buffer_len < 56) c.buffer[c.buffer_len++] = 0x00;

  // length (big-endian)
  for (int i=7;i>=0;--i) c.buffer[c.buffer_len++] = (uint8_t)((total_bits >> (i*8)) & 0xFF);
  sha256_compress(c, c.buffer);

  // output (big-endian words)
  for (int i=0;i<8;++i) {
    out[i*4+0] = (uint8_t)((c.state[i] >> 24) & 0xFF);
    out[i*4+1] = (uint8_t)((c.state[i] >> 16) & 0xFF);
    out[i*4+2] = (uint8_t)((c.state[i] >>  8) & 0xFF);
    out[i*4+3] = (uint8_t)((c.state[i] >>  0) & 0xFF);
  }
}

// ======== HMAC-SHA256 (multi-part helper as used earlier) ====================

bool hmac_sha256(const uint8_t* key, size_t key_len,
                 const uint8_t* in1, size_t in1_len,
                 const uint8_t* in2, size_t in2_len,
                 const uint8_t* in3, size_t in3_len,
                 uint8_t out32[32])
{
  // block size for SHA-256
  const size_t B = 64;

  // Prepare K0 (block-sized key)
  uint8_t K0[64];
  if (key_len > B) {
    // hash the long key first
    sha256_ctx t; sha256_init(t);
    sha256_update(t, key, key_len);
    uint8_t dk[32]; sha256_final(t, dk);
    std::memset(K0, 0, B);
    std::memcpy(K0, dk, 32);
  } else {
    std::memset(K0, 0, B);
    if (key && key_len) std::memcpy(K0, key, key_len);
  }

  uint8_t ipad[64], opad[64];
  for (size_t i=0;i<B;++i) { ipad[i] = K0[i] ^ 0x36; opad[i] = K0[i] ^ 0x5C; }

  // inner = SHA256( (K0 ^ ipad) || in1 || in2 || in3 )
  sha256_ctx in; sha256_init(in);
  sha256_update(in, ipad, B);
  if (in1 && in1_len) sha256_update(in, in1, in1_len);
  if (in2 && in2_len) sha256_update(in, in2, in2_len);
  if (in3 && in3_len) sha256_update(in, in3, in3_len);
  uint8_t inner[32]; sha256_final(in, inner);

  // outer = SHA256( (K0 ^ opad) || inner )
  sha256_ctx out; sha256_init(out);
  sha256_update(out, opad, B);
  sha256_update(out, inner, 32);
  sha256_final(out, out32);
  return true;
}

// helper for a single contiguous msg
static inline bool hmac_sha256_single(const uint8_t* key, size_t key_len,
                                      const uint8_t* msg, size_t msg_len,
                                      uint8_t out32[32]) {
  return hmac_sha256(key, key_len, msg, msg_len, nullptr, 0, nullptr, 0, out32);
}

// ======== Secrets / Bring-up KDF =============================================

struct Secrets {
  uint8_t pmk[16];
  uint8_t lmk[16];
  uint8_t salt[16];
};

// Defaults are placeholders (ASCII-ish), safe for bring-up only.
static Secrets g_secrets = {{
  0x50,0x4D,0x4B,0x2D,0x44,0x45,0x46,0x41,0x55,0x4C,0x54,0x2D,0x50,0x4D,0x4B,0x21
},{
  0x4C,0x4D,0x4B,0x2D,0x44,0x45,0x46,0x41,0x55,0x4C,0x54,0x2D,0x4C,0x4D,0x4B,0x21
},{
  0x53,0x41,0x4C,0x54,0x2D,0x44,0x45,0x46,0x41,0x55,0x4C,0x54,0x2D,0x53,0x41,0x4C
}};

void setSecuritySecrets(const uint8_t pmk[16], const uint8_t lmk[16], const uint8_t salt[16]) {
  if (pmk)  std::memcpy(g_secrets.pmk,  pmk,  16);
  if (lmk)  std::memcpy(g_secrets.lmk,  lmk,  16);
  if (salt) std::memcpy(g_secrets.salt, salt, 16);
}

static bool derive_app_key(const uint8_t token128[16], uint8_t out32[32]) {
  uint8_t key[32];
  std::memcpy(key,     g_secrets.pmk,  16);
  std::memcpy(key+16,  g_secrets.lmk,  16);

  uint8_t msg[32];
  std::memcpy(msg,     token128,       16);
  std::memcpy(msg+16,  g_secrets.salt, 16);

  return hmac_sha256_single(key, sizeof(key), msg, sizeof(msg), out32);
}

// ======== Concatenation helper ===============================================

static size_t build_mac_message(const NowHeader& h,
                                const NowAuth128& a,
                                const NowTopoToken128* topo_or_null,
                                const uint8_t* payload, uint16_t payload_len,
                                const uint8_t nonce[NOW_HMAC_NONCE_LEN],
                                uint8_t* out, size_t out_cap)
{
  const size_t need = sizeof(NowHeader) + sizeof(NowAuth128)
                    + (topo_or_null ? sizeof(NowTopoToken128) : 0)
                    + payload_len + NOW_HMAC_NONCE_LEN;
  if (out_cap < need) return 0;

  uint8_t* p = out;
  std::memcpy(p, &h, sizeof(h)); p += sizeof(h);
  std::memcpy(p, &a, sizeof(a)); p += sizeof(a);
  if (topo_or_null) {
    std::memcpy(p, topo_or_null, sizeof(NowTopoToken128));
    p += sizeof(NowTopoToken128);
  }
  if (payload && payload_len) {
    std::memcpy(p, payload, payload_len);
    p += payload_len;
  }
  std::memcpy(p, nonce, NOW_HMAC_NONCE_LEN); p += NOW_HMAC_NONCE_LEN;
  return static_cast<size_t>(p - out);
}

// ======== Public API: verify/sign ============================================

bool verifyHmac(const NowHeader* ph,
                const NowAuth128* pa,
                const NowTopoToken128* topo_or_null,
                const uint8_t* payload, uint16_t payload_len,
                const NowSecTrailer* sec)
{
  if (!ph || !pa || !sec) return false;

  uint8_t app_key[32];
  if (!derive_app_key(pa->device_token128, app_key)) return false;

  uint8_t buf[512];
  const size_t msg_len = build_mac_message(*ph, *pa, topo_or_null,
                                           payload, payload_len,
                                           sec->nonce, buf, sizeof(buf));
  if (msg_len == 0) return false;

  uint8_t dig[32];
  if (!hmac_sha256_single(app_key, sizeof(app_key), buf, msg_len, dig)) return false;

  return std::memcmp(dig, sec->tag, NOW_HMAC_TAG_LEN) == 0;
}

bool signHmac(const NowHeader& h,
              const NowAuth128& a,
              const NowTopoToken128* topo_or_null,
              const uint8_t* payload, uint16_t payload_len,
              NowSecTrailer& sec_out)
{
  uint8_t app_key[32];
  if (!derive_app_key(a.device_token128, app_key)) return false;

  uint8_t buf[512];
  const size_t msg_len = build_mac_message(h, a, topo_or_null,
                                           payload, payload_len,
                                           sec_out.nonce, buf, sizeof(buf));
  if (msg_len == 0) return false;

  uint8_t dig[32];
  if (!hmac_sha256_single(app_key, sizeof(app_key), buf, msg_len, dig)) return false;

  std::memcpy(sec_out.tag, dig, NOW_HMAC_TAG_LEN);
  return true;
}

} // namespace espnow
