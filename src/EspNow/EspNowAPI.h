#pragma once
/**
 * EasyDriveway ESP-NOW API — v3-T (Token-Only)
 * Canonical wire contract: header, tokens, flags, roles, and payload structs.
 * This header intentionally contains NO transport code.
 *
 * Profile notes (v3-T):
 *  - Every frame carries a 16-byte Device Token (admission).
 *  - Topology-bound ops also carry a 16-byte Topology Token and set HAS_TOPO.
 *  - No HMAC/signatures. Integrity is best-effort (length checks, CRC in FW).
 *
 * All integers are little-endian. All structs are packed.
 */

#include <stdint.h>
#include <stddef.h>

// ------------------------------------------------------------
// Versioning
// ------------------------------------------------------------
static constexpr uint8_t NOW_PROTO_VER = 0x31; // v3-T

// ------------------------------------------------------------
#if defined(__GNUC__)
  #define NOW_PACKED __attribute__((packed))
#else
  #pragma pack(push,1)
  #define NOW_PACKED
#endif

// ------------------------------------------------------------
// Roles
// ------------------------------------------------------------
enum : uint8_t {
  NOW_ROLE_ICM  = 0,  // Interface Control Module (admin/control plane)
  NOW_ROLE_PMS  = 1,  // Power Management
  NOW_ROLE_REL  = 2,  // Physical Relay node
  NOW_ROLE_REMU = 3,  // Relay Emulator
  NOW_ROLE_SEMU = 4,  // Sensor Emulator
  NOW_ROLE_SENS = 5   // Physical Sensor node
};

// Virtual address: 0xFF means the physical device instance.
static constexpr uint8_t NOW_VIRT_PHY = 0xFF;

// ------------------------------------------------------------
// Message types (catalog)
// ------------------------------------------------------------
enum NowMsgType : uint8_t {
  NOW_MSG_PAIR_REQ    = 0x01,
  NOW_MSG_PAIR_ACK    = 0x02,
  NOW_MSG_PING        = 0x03,
  NOW_MSG_PING_REPLY  = 0x04,
  NOW_MSG_TIME_SYNC   = 0x05,

  NOW_MSG_TOPO_PUSH   = 0x10,
  NOW_MSG_NET_SET_CHAN= 0x11,

  NOW_MSG_CTRL_RELAY  = 0x20,
  NOW_MSG_RLY_STATE   = 0x21,

  NOW_MSG_SENS_REPORT = 0x30,
  NOW_MSG_PMS_STATUS  = 0x31,

  NOW_MSG_CONFIG_WRITE= 0x40,

  NOW_MSG_FW_BEGIN    = 0x50,
  NOW_MSG_FW_CHUNK    = 0x51,
  NOW_MSG_FW_STATUS   = 0x52,
  NOW_MSG_FW_COMMIT   = 0x53,
  NOW_MSG_FW_ABORT    = 0x54
};

// ------------------------------------------------------------
// Flags
// ------------------------------------------------------------
enum : uint16_t {
  NOW_FLAGS_RELIABLE = (1u << 0),   // expect reply; scheduler retries
  NOW_FLAGS_URGENT   = (1u << 1),   // route via urgent queue
  NOW_FLAGS_HAS_TOPO = (1u << 2)    // Topology Token follows Device Token
};

// ------------------------------------------------------------
// Header (23 bytes) — packed
// ------------------------------------------------------------
#pragma pack(push,1)
struct NOW_PACKED NowHeader {
  uint8_t  proto_ver;     // NOW_PROTO_VER (0x31)
  uint8_t  msg_type;      // NowMsgType
  uint16_t flags;         // NOW_FLAGS_*
  uint16_t seq;           // per-sender rolling sequence
  uint16_t topo_ver;      // topology version/epoch
  uint8_t  virt_id;       // 0xFF = physical; else virtual instance
  uint8_t  reserved;      // must be 0
  uint8_t  ts_ms[6];      // sender timestamp (48-bit ms since boot; diagnostic)
  uint8_t  sender_mac[6]; // sender MAC (copy for quick policy checks)
  uint8_t  sender_role;   // NOW_ROLE_*
};
#pragma pack(pop)
static_assert(sizeof(NowHeader) == 23, "NowHeader must be 23 bytes");
//enum { NOW_PROTO_VER = 0x31 };
enum NowFlags : uint16_t {
  NOW_FLAGS_RELIABLE = 1<<0, NOW_FLAGS_URGENT = 1<<1, NOW_FLAGS_HAS_TOPO = 1<<2
};
// ------------------------------------------------------------
// Tokens (always little-endian byte arrays)
// ------------------------------------------------------------
struct NOW_PACKED NowAuth128      { uint8_t bytes[16]; }; // Device Token — always present
struct NOW_PACKED NowTopoToken128 { uint8_t bytes[16]; }; // Topology Token — iff HAS_TOPO
static_assert(sizeof(NowAuth128)      == 16, "NowAuth128 size");
static_assert(sizeof(NowTopoToken128) == 16, "NowTopoToken128 size");

// ------------------------------------------------------------
// Common small payloads
// ------------------------------------------------------------

// PING → optional fields filled by adapters (state bits, etc.)
// Keep tiny so it fits everywhere without fragmentation.
struct NOW_PACKED NowPing {
  uint16_t state_bits;   // adapter-defined status flags
  uint16_t temp_c_x10;   // optional temperature *10 (e.g., 253 = 25.3°C)
  uint16_t uptime_s;     // seconds (clamped)
  uint16_t reserved;     // align to 8 bytes
};
static_assert(sizeof(NowPing) == 8, "NowPing must be 8 bytes");

struct NOW_PACKED NowPingReply {
  uint16_t state_bits;
  uint16_t temp_c_x10;
  uint16_t uptime_s;
  uint16_t reserved;
};
static_assert(sizeof(NowPingReply) == 8, "NowPingReply must be 8 bytes");

// TIME_SYNC — ICM → others (epoch ms low/high, optional drift)
struct NOW_PACKED NowTimeSync {
  uint32_t epoch_ms_lo;
  uint32_t epoch_ms_hi; // full 64-bit ms if desired
  int16_t  drift_ms;    // optional correction hint
  uint16_t reserved;
};
static_assert(sizeof(NowTimeSync) == 12, "NowTimeSync must be 12 bytes");

// NET_SET_CHAN — ICM hint
struct NOW_PACKED NowNetSetChan {
  uint8_t channel; // 1..13 typical
  uint8_t reserved[3];
};
static_assert(sizeof(NowNetSetChan) == 4, "NowNetSetChan must be 4 bytes");

// ------------------------------------------------------------
// REL control / state
// ------------------------------------------------------------
enum NowRlyOp : uint8_t { NOW_RLY_OP_OFF=0, NOW_RLY_OP_ON=1, NOW_RLY_OP_TOGGLE=2 };

// CTRL_RELAY — ICM/ICM-equiv → REL/REMU (HAS_TOPO required)
struct NOW_PACKED NowCtrlRelay {
  uint8_t  channel;     // index
  uint8_t  op;          // NowRlyOp
  uint16_t pulse_ms;    // 0 for none; when >0, apply ON for N ms then OFF
};
static_assert(sizeof(NowCtrlRelay) == 4, "NowCtrlRelay must be 4 bytes");

// RLY_STATE — REL/REMU → ICM (or on request)
struct NOW_PACKED NowRlyState {
  uint32_t mask;        // bit i = channel i ON
  uint16_t topo_ver;    // echo local topo version
  uint8_t  count;       // number of channels mapped
  uint8_t  reserved;    // align to 8
};
static_assert(sizeof(NowRlyState) == 8, "NowRlyState must be 8 bytes");

// ------------------------------------------------------------
// SENS / PMS compact payloads (adapters may pack tighter structs)
// ------------------------------------------------------------
struct NOW_PACKED NowSensReportHdr {
  uint16_t bytes;   // length of following sensor blob
  uint16_t fmt;     // adapter-defined format version
};
static_assert(sizeof(NowSensReportHdr) == 4, "NowSensReportHdr must be 4 bytes");

struct NOW_PACKED NowPmsStatus {
  int16_t  temp_c_x10;  // board temperature *10
  uint16_t vbus_mV;     // input voltage
  uint16_t vsys_mV;     // system rail
  int16_t  iout_mA;     // output current (signed for charge/regen)
  uint16_t faults;      // bitmask
};
static_assert(sizeof(NowPmsStatus) == 10, "NowPmsStatus must be 10 bytes");

// ------------------------------------------------------------
// CONFIG_WRITE (HAS_TOPO required)
// key = 6 bytes (namespace+id), len followed by raw bytes.
// ------------------------------------------------------------
struct NOW_PACKED NowConfigWrite {
  uint8_t  key[6];
  uint16_t len;     // N bytes follow in payload
  // uint8_t data[N]; // appended by sender
};
static_assert(sizeof(NowConfigWrite) == 8, "NowConfigWrite must be 8 bytes");

// ------------------------------------------------------------
// Firmware transport (CRC-only integrity)
// ------------------------------------------------------------
enum NowFwState : uint8_t { NOW_FW_IDLE=0, NOW_FW_RECEIVING=1, NOW_FW_READY=2, NOW_FW_APPLYING=3, NOW_FW_ERROR=4 };
enum NowFwErr   : uint8_t { NOW_FW_OK=0, NOW_FW_SIZE=1, NOW_FW_CRC=2, NOW_FW_ROLE=3, NOW_FW_OPERATOR=4, NOW_FW_INTERNAL=5 };

struct NOW_PACKED NowFwBegin {
  uint8_t  target_role;   // NOW_ROLE_*
  uint8_t  reserved;
  uint32_t total_len;
  uint32_t crc32;
};
static_assert(sizeof(NowFwBegin) == 10, "NowFwBegin must be 10 bytes");

struct NOW_PACKED NowFwChunk {
  uint32_t offset;
  uint16_t len;           // L bytes follow
  // uint8_t data[L];
};
static_assert(sizeof(NowFwChunk) == 6, "NowFwChunk must be 6 bytes");

struct NOW_PACKED NowFwCommit {
  uint8_t  apply_after_reboot; // 0/1
  uint8_t  reserved[3];
};
static_assert(sizeof(NowFwCommit) == 4, "NowFwCommit must be 4 bytes");

struct NOW_PACKED NowFwStatus {
  uint8_t  state;    // NowFwState
  uint8_t  error;    // NowFwErr
  uint16_t progress; // 0..1000 (0.1% steps)
};
static_assert(sizeof(NowFwStatus) == 4, "NowFwStatus must be 4 bytes");

// ------------------------------------------------------------
// Compile-time guards for maximum body sizes (conservative)
// ------------------------------------------------------------
static constexpr uint16_t NOW_MAX_BODY  = 200;  // safe body size (no fragmentation)
static constexpr uint16_t NOW_MAX_FRAME = 250;  // ESPNOW MTU guard

// ------------------------------------------------------------
#if !defined(__GNUC__)
  #pragma pack(pop)
#endif
