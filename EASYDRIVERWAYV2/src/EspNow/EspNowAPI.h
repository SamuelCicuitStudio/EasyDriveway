/**************************************************************
 *  Project     : EasyDriveway
 *  File        : EspNowAPI.h
 *  Purpose     : Canonical enums and wire payload structures for all roles
 *                (ICM, PMS, REL, SENS, SEMU, REMU) — Hardened v2H protocol
 *                without backward compatibility (128-bit tokens, mandatory
 *                HMAC trailer, authenticated topology push).
 *
 *  HARD SECURITY POLICY (MUST):
 *  - Every ESPNOW peer (ICM and all nodes) MUST be added as ENCRYPTED peer
 *    (PMK+LMK, encrypt=true). Unencrypted peers MUST NOT be used.
 *  - LMK MUST be derived per peer (no OTA LMK). Suggested derivation:
 *      LMK = Trunc16( HMAC-SHA256( PMK,
 *             MAC_lo || MAC_hi || DEV_TOK_A || DEV_TOK_B || SALT ))
 *    where MAC_lo = min(MAC_A, MAC_B), MAC_hi = max(MAC_A, MAC_B),
 *          DEV_TOK_* are 128-bit device tokens, SALT is ICM-only deployment salt.
 *
 *  TOKENS (NO LEGACY):
 *  - device_token128: 16 bytes (raw) — present in EVERY frame except PAIR_REQ.
 *  - topology_token128: 16 bytes (raw) — present ONLY on topology-dependent commands
 *    (e.g., CTRL_RELAY). TOPO_PUSH is authenticated via TLV HMAC/SIGNATURE, not a topo token.
 *
 *  HMAC TRAILER (MANDATORY):
 *  - Every frame except PAIR_REQ MUST carry NowSecTrailer (HMAC-SHA256 truncated tag).
 *    Frames without a valid tag MUST be rejected.
 *
 *  PRIVILEGED OPS ROLE/MAC ENFORCEMENT (MUST):
 *  - TOPO_PUSH, NET_SET_CHAN, TIME_SYNC, FW_* : accept ONLY if (sender_role==ICM AND sender_mac==ICMMAC).
 *
 *  REMU RLY_STATE SEMANTICS (MUST):
 *  - REMU replies set header.virt_id == targeted virtual,
 *    and NowRlyState.bitmask reports the ENTIRE device outputs (truth for UI).
 *
 *  CONFIG_WRITE RULES (MUST):
 *  - Exactly one 6-char key per frame; STR6 is 6 raw bytes (no NUL).
 *  - BIN length MUST be bounded by a role-specific cap (stack-enforced).
 *
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-07
 *  Version     : 3.0.0  // v2H: no-legacy, 128-bit tokens, mandatory HMAC, topo auth TLV
 **************************************************************/
#ifndef ESPNOW_API_H
#define ESPNOW_API_H

#include <stdint.h>
#include <stddef.h>

/**
 * @def NOW_PACKED
 * @brief Compiler-agnostic packed attribute for wire structures.
 */
#if defined(__GNUC__)
#  define NOW_PACKED __attribute__((packed))
#else
#  define NOW_PACKED
#endif

/*======================================================================
=                           PROTOCOL CONSTANTS                          =
======================================================================*/

/** @brief Protocol version carried in all headers. */
enum : uint8_t { NOW_PROTO_VER = 3 };

/**
 * @brief Virtual-ID for physical endpoints (non-virtual).
 *        SEMU/REMU use 1..SCOUNT/RCOUNT as virtual indices.
 */
enum : uint8_t { NOW_VIRT_PHYSICAL = 0xFF };

/** @brief Header flags bitfield (wire). */
enum : uint16_t {
  NOW_FLAGS_NONE     = 0x0000, /**< No flags. */
  NOW_FLAGS_HAS_TOPO = 0x0001, /**< Frame includes NowTopoToken128 (CTRL_RELAY etc.). */
  NOW_FLAGS_URGENT   = 0x0002, /**< Scheduler hint: higher priority. */
  NOW_FLAGS_RSV      = 0x8000  /**< Reserved. */
};

/* Mandatory HMAC trailer parameters (receivers enforce). */
enum : uint8_t { NOW_HMAC_TAG_LEN = 12 };     /* 96-bit truncated tag */
enum : uint8_t { NOW_HMAC_NONCE_LEN = 6  };   /* 48-bit nonce (ts or counter) */

/*======================================================================
=                               ROLES                                   =
======================================================================*/

/** @brief Device roles in the system. */
enum NowDeviceKind : uint8_t {
  NOW_KIND_ICM   = 0x00, /**< Interface Control Module (controller/UI). */
  NOW_KIND_PMS   = 0x01, /**< Power Management Unit (rails, thermal, source). */
  NOW_KIND_RELAY = 0x02, /**< Production Relay module. */
  NOW_KIND_SENS  = 0x03, /**< Production Sensor module (TF-Luna pair + ALS/ENV). */
  NOW_KIND_REMU  = 0x05, /**< Relay Emulator (virtual relays share one MAC). */
  NOW_KIND_SEMU  = 0x06  /**< Sensor Emulator (virtual sensors share one MAC). */
};

/** @brief Node runtime state flags (reported in PING_REPLY). */
enum : uint16_t {
  NOW_STATE_MODE_AUTO   = 1u << 0,
  NOW_STATE_MODE_MANUAL = 1u << 1,
  NOW_STATE_UPDATING    = 1u << 2,
  NOW_STATE_STARTING_UP = 1u << 3,
  NOW_STATE_BUSY        = 1u << 4,
  NOW_STATE_PAIRING     = 1u << 5,
  NOW_STATE_IDLE        = 1u << 6,
};

/*======================================================================
=                               OPCODES                                 =
======================================================================*/

/** @brief Message types (opcodes) for all wire interactions. */
enum NowMsgType : uint8_t {
  /* Pairing & Control Plane */
  NOW_MT_PAIR_REQ     = 0x00, /**< Device -> ICM: pairing request (NO tokens/HMAC). */
  NOW_MT_PAIR_ACK     = 0x01, /**< ICM -> Device: device_token128 + channel (HMAC). */
  NOW_MT_TOPO_PUSH    = 0x02, /**< ICM -> Nodes: authoritative topology blob (TLV+AUTH, HMAC). */
  NOW_MT_NET_SET_CHAN = 0x03, /**< ICM -> Node : change ESPNOW channel with grace delay (HMAC). */

  /* Control & Live */
  NOW_MT_CTRL_RELAY   = 0x10, /**< ICM/SENS/SEMU -> REL/REMU: REQUIRES topology token (HMAC). */
  NOW_MT_SENS_REPORT  = 0x20, /**< SENS/SEMU -> ICM: TF-Luna A/B + ALS/ENV live report (HMAC). */
  NOW_MT_RLY_STATE    = 0x21, /**< REL/REMU  -> ICM: relay state & result (HMAC). */
  NOW_MT_PMS_STATUS   = 0x22, /**< PMS       -> ICM: power/thermal/rails report (HMAC). */
  NOW_MT_CONFIG_WRITE = 0x30, /**< ICM -> Node: write one 6-char config key/value (HMAC). */

  /* Service */
  NOW_MT_PING         = 0x40, /**< ICM -> Node: liveness ping (HMAC). */
  NOW_MT_PING_REPLY   = 0x41, /**< Node -> ICM: liveness reply + NOW_STATE_* (HMAC). */
  NOW_MT_TIME_SYNC    = 0x42, /**< ICM -> Node: time synchronization (HMAC). */

  /* Firmware Update (ICM -> Node, no forwarding; all HMAC) */
  NOW_MT_FW_BEGIN     = 0x50, /**< Declare image/params; reset progress. */
  NOW_MT_FW_CHUNK     = 0x51, /**< Transfer a chunk (windowed). */
  NOW_MT_FW_STATUS    = 0x52, /**< Node -> ICM: progress/next_needed. */
  NOW_MT_FW_COMMIT    = 0x53, /**< Request verify+apply (signature required). */
  NOW_MT_FW_ABORT     = 0x54  /**< Abort transfer. */
};

/** @brief Relay control operations. */
enum NowRelayOp : uint8_t { NOW_RLY_NOP=0, NOW_RLY_OFF=1, NOW_RLY_ON=2, NOW_RLY_PULSE=3 };

/*======================================================================
=                           WIRE STRUCTURES                             =
======================================================================*/

/**
 * @brief Standard fixed header (present on every frame).
 * @note All integers are LE. Tokens are raw 16-byte arrays (no endianness).
 * @note Timestamp is compact 6 bytes to save MTU (sender-local ms, lower 48 bits).
 */
struct NOW_PACKED NowHeader {
  uint8_t  proto_ver;     /**< Protocol version (NOW_PROTO_VER). */
  uint8_t  msg_type;      /**< Message type (NowMsgType). */
  uint16_t flags;         /**< Flags (NOW_FLAGS_*). */
  uint16_t seq;           /**< Per-sender sequence (dup window + monotonic guard). */
  uint16_t topo_ver;      /**< Topology version (ICM increments on TOPO_PUSH). */
  uint8_t  virt_id;       /**< Virtual index or NOW_VIRT_PHYSICAL (0xFF). */
  uint8_t  reserved;      /**< Reserved/alignment. */
  uint8_t  ts_ms[6];      /**< Sender timestamp (ms, 48-bit). */
  uint8_t  sender_mac[6]; /**< Sender MAC. */
  uint8_t  sender_role;   /**< Sender role (NowDeviceKind). */
};
static_assert(sizeof(NowHeader) == 23, "NowHeader must be 23 bytes (packed)");

/** @brief Per-frame device authentication (MUST be present on all frames except PAIR_REQ). */
struct NOW_PACKED NowAuth128 {
  uint8_t device_token128[16]; /**< Admission token (16B, raw). */
};
static_assert(sizeof(NowAuth128) == 16, "NowAuth128 must be 16 bytes");

/**
 * @brief Topology token (present on topology-dependent commands; see NOW_FLAGS_HAS_TOPO).
 */
struct NOW_PACKED NowTopoToken128 {
  uint8_t token128[16]; /**< Topology authorization token (16B, raw). */
};
static_assert(sizeof(NowTopoToken128) == 16, "NowTopoToken128 must be 16 bytes");

/**
 * @brief App-layer HMAC trailer (MANDATORY on all frames except PAIR_REQ).
 * @details tag = Trunc_96( HMAC-SHA256(K_app, NowHeader||NowAuth128||[NowTopoToken128?]||payload||nonce) )
 *          where K_app is per-peer key derived from PMK, LMK, device token, SALT.
 */
struct NOW_PACKED NowSecTrailer {
  uint8_t nonce[NOW_HMAC_NONCE_LEN]; /**< 48-bit sender nonce (or ms). */
  uint8_t tag[NOW_HMAC_TAG_LEN];     /**< 96-bit HMAC tag. */
};
static_assert(sizeof(NowSecTrailer) == (NOW_HMAC_NONCE_LEN + NOW_HMAC_TAG_LEN),
              "NowSecTrailer size mismatch");

/* -------------------- Pairing & Channel -------------------- */

/** @brief Pair acknowledge payload (ICM -> Device). */
struct NOW_PACKED NowPairAck {
  uint8_t  icm_mac[6];
  uint8_t  channel;        /**< Initial ESPNOW channel. */
  uint8_t  reserved;
  uint8_t  device_token128[16]; /**< Assigned 128-bit device token (raw). */
};
static_assert(sizeof(NowPairAck) == 24, "NowPairAck must be 24 bytes");

/**
 * @brief Request a node to switch to a new ESPNOW channel after a grace delay.
 * @details Privileged op: receiver MUST enforce (sender_role==ICM && sender_mac==ICMMAC).
 *          Flow:
 *           1) ICM sends on CURRENT channel with {new_channel, wait_ms}.
 *           2) Node validates admission & HMAC, persists "CHAN__", waits wait_ms, re-inits on new_channel.
 *           3) ICM switches its radio at (wait_ms-~100ms) and probes with PING.
 */
struct NOW_PACKED NowNetSetChan {
  uint8_t  new_channel; /**< Target ESPNOW channel (region-valid). */
  uint8_t  reserved;    /**< Reserved. */
  uint16_t wait_ms;     /**< Grace delay before node switches. */
};
static_assert(sizeof(NowNetSetChan) == 4, "NowNetSetChan must be 4 bytes");

/* -------------------- Topology Push -------------------- */

/** @brief Topology blob format selector. */
enum : uint8_t { NOW_TOPO_FMT_TLV_V1 = 1 };

/** @brief Topology TLV item tags. */
enum : uint8_t {
  NOW_TLV_NODE_ENTRY     = 0x10, /**< Node/virtual descriptors, neighbors, lists (role-specific). */
  NOW_TLV_TOPO_VERSION   = 0x11, /**< u16 version (redundant with header, useful in blob). */

  /* v2H authentication tags (MANDATORY: one of them MUST be present) */
  NOW_TLV_TOPO_AUTH_HMAC = 0xF0, /**< HMAC(tag_len=16, tag_bytes[16]) over TLV (excluding this item). */
  NOW_TLV_TOPO_AUTH_SIG  = 0xF1  /**< Signature(sig_len, sig_bytes) over TLV digest (Ed25519/ECDSA). */
};

/**
 * @brief Topology push header (ICM -> Nodes).
 * @details Followed by TLV blob with SENS, SEMU(virt), REL, REMU entries and TOPO_AUTH item.
 * @note Topology token is NOT carried here; authorization is by role/MAC and TLV auth.
 */
struct NOW_PACKED NowTopoPush {
  uint8_t  topo_fmt;  /**< NOW_TOPO_FMT_TLV_V1. */
  uint8_t  reserved;
  uint16_t topo_len;  /**< Length of following TLV blob. */
  /* uint8_t topo_bytes[topo_len]; */
};
static_assert(sizeof(NowTopoPush) == 4, "NowTopoPush must be 4 bytes");

/* -------------------- Control: Relay -------------------- */

/**
 * @brief Relay control payload for REL/REMU.
 * @note Requires NowTopoToken128 + NOW_FLAGS_HAS_TOPO.
 * @note For REMU, target virtual relay via header.virt_id (1..RCOUNT).
 */
struct NOW_PACKED NowCtrlRelay {
  uint8_t  channel;  /**< REL: physical index; REMU: bit index. */
  uint8_t  op;       /**< NowRelayOp. */
  uint16_t pulse_ms; /**< Used when op == NOW_RLY_PULSE. */
};
static_assert(sizeof(NowCtrlRelay) == 4, "NowCtrlRelay must be 4 bytes");

/* -------------------- Reports: SENS/SEMU -------------------- */

/** @brief TF-Luna single-sensor sample. */
struct NOW_PACKED NowTFPairSample {
  int16_t  dist_mm;     /**< -1 if invalid. */
  uint16_t amp;         /**< Raw amplitude. */
  int16_t  temp_c_x100; /**< e.g., 2534 => 25.34°C. */
  uint8_t  ok;          /**< 1=valid. */
  uint8_t  rsv;         /**< Reserved. */
};
static_assert(sizeof(NowTFPairSample) == 8, "NowTFPairSample must be 8 bytes");

/**
 * @brief Sensor report payload (SENS and SEMU virtuals).
 * @note For SEMU, report is identical; disambiguate by header.virt_id.
 */
struct NOW_PACKED NowSensReport {
  NowTFPairSample A;  /**< TF-Luna A. */
  NowTFPairSample B;  /**< TF-Luna B. */
  uint16_t lux;       /**< VEML7700 lux. */
  int16_t  t_c_x100;  /**< BME280 temp ×100. */
  uint16_t rh_x100;   /**< BME280 RH% ×100. */
  uint32_t press_pa;  /**< BME280 Pa. */
  uint16_t fps;       /**< Effective FPS. */
  uint8_t  present_flags; /**< bit0: forward; bit1: reverse. */
  uint8_t  health;    /**< Health bitfield. */
};
static_assert(sizeof(NowSensReport) == 30, "NowSensReport must be 30 bytes");

/* -------------------- Reports: REL/REMU -------------------- */

enum NowActResult : uint8_t {
  NOW_ACT_OK        = 0,
  NOW_ACT_INTERLOCK = 1,
  NOW_ACT_THERMAL   = 2,
  NOW_ACT_RATE      = 3,
  NOW_ACT_DENIED    = 4,
  NOW_ACT_TOPO_MISM = 5,
  NOW_ACT_OTHER     = 15
};

/** @brief Relay state report (REL & REMU). */
struct NOW_PACKED NowRlyState {
  uint16_t bitmask; /**< For REMU: ENTIRE device outputs; for REL: outputs mask. */
  uint8_t  result;  /**< NowActResult. */
  uint8_t  rsv;     /**< Reserved. */
};
static_assert(sizeof(NowRlyState) == 4, "NowRlyState must be 4 bytes");

/* -------------------- Reports: PMS -------------------- */

/** @brief PMS telemetry report. */
struct NOW_PACKED NowPmsStatus {
  uint8_t  source_sel; /**< 0=WALL, 1=BAT. */
  uint8_t  rails;      /**< Rail enable bitmask. */
  uint16_t vbus_mv;    /**< mV. */
  uint16_t ibus_ma;    /**< mA. */
  uint16_t vbat_mv;    /**< mV. */
  uint16_t ibat_ma;    /**< mA. */
  int16_t  temp_c_x10; /**< ×10 (e.g., 253=25.3°C). */
  uint8_t  fan_pwm;    /**< 0..100%. */
  uint16_t fan_rpm;    /**< RPM. */
  uint16_t faults;     /**< OVP/UVP/OCP/OTP/... */
};
static_assert(sizeof(NowPmsStatus) == 17, "NowPmsStatus must be 17 bytes");

/* -------------------- Config Write -------------------- */

enum NowConfigType : uint8_t {
  NOW_CFG_U8   = 1,
  NOW_CFG_U16  = 2,
  NOW_CFG_U32  = 3,
  NOW_CFG_I16  = 4,
  NOW_CFG_I32  = 5,
  NOW_CFG_STR6 = 6,
  NOW_CFG_BIN  = 7
};

/**
 * @brief Config write header (value bytes follow).
 * @details Exactly one 6-char key per frame. For BIN/STR6, len applies.
 */
struct NOW_PACKED NowConfigWrite {
  char    key6[6]; /**< 6-char key (no NUL). */
  uint8_t type;    /**< NowConfigType. */
  uint8_t len;     /**< For BIN/STR6; implied for fixed-width types. */
  /* value bytes follow:
     - U8:1B, U16/I16:2B LE, U32/I32:4B LE,
     - STR6: 6B raw, BIN: len bytes (builder-bounded by role cap) */
};
static_assert(sizeof(NowConfigWrite) == 8, "NowConfigWrite must be 8 bytes");

/* -------------------- Ping / Time Sync -------------------- */

struct NOW_PACKED NowPing {
  uint16_t echo_seq;
};
static_assert(sizeof(NowPing) == 2, "NowPing must be 2 bytes");

struct NOW_PACKED NowPingReply {
  uint16_t echo_seq;
  uint8_t  role;         /**< NowDeviceKind. */
  uint16_t state_flags;  /**< NOW_STATE_*. */
};
static_assert(sizeof(NowPingReply) == 5, "NowPingReply must be 5 bytes");

struct NOW_PACKED NowTimeSync {
  uint64_t icm_epoch_ms;
};
static_assert(sizeof(NowTimeSync) == 8, "NowTimeSync must be 8 bytes");

/* -------------------- Firmware Update -------------------- */

enum NowFwTargetRole : uint8_t {
  NOW_FW_ICM   = NOW_KIND_ICM,
  NOW_FW_PMS   = NOW_KIND_PMS,
  NOW_FW_RELAY = NOW_KIND_RELAY,
  NOW_FW_SENS  = NOW_KIND_SENS,
  NOW_FW_REMU  = NOW_KIND_REMU,
  NOW_FW_SEMU  = NOW_KIND_SEMU
};

enum NowFwState : uint8_t {
  NOW_FW_IDLE        = 0,
  NOW_FW_RECEIVING   = 1,
  NOW_FW_READY       = 2,
  NOW_FW_VERIFYING   = 3,
  NOW_FW_APPLYING    = 4,
  NOW_FW_REBOOTING   = 5,
  NOW_FW_ERROR       = 15
};

enum NowFwAbortReason : uint8_t {
  NOW_FW_ABORT_OPERATOR = 0,
  NOW_FW_ABORT_ROLE_MISM= 1,
  NOW_FW_ABORT_VERSION  = 2,
  NOW_FW_ABORT_SPACE    = 3,
  NOW_FW_ABORT_CRC      = 4,
  NOW_FW_ABORT_DIGEST   = 5,
  NOW_FW_ABORT_INTERNAL = 15
};

/** @brief Firmware begin (ICM -> Node). */
struct NOW_PACKED NowFwBegin {
  uint32_t image_id;
  uint8_t  target_role;   /**< NowFwTargetRole. */
  uint8_t  sig_algo;      /**< 1=Ed25519, 2=ECDSA-P256 (implementation-defined). */
  uint16_t window_size;   /**< Chunks per window (e.g., 8..32). */

  uint32_t total_size;
  uint16_t chunk_size;
  uint16_t total_chunks;

  uint32_t target_version;/**< (maj<<24)|(min<<16)|(pat<<8)|build. */
  uint8_t  sha256[32];    /**< Expected image digest. */
};
/* FIX: size is 52 bytes (not 44) */
static_assert(sizeof(NowFwBegin) == 52, "NowFwBegin must be 52 bytes");

/** @brief Firmware chunk (ICM -> Node). */
struct NOW_PACKED NowFwChunk {
  uint32_t image_id;
  uint32_t chunk_index;  /**< 0..total_chunks-1. */
  uint16_t data_len;     /**< <= chunk_size. */
  uint16_t crc16_ccitt;  /**< CRC of chunk payload. */
  /* uint8_t data[data_len]; */
};
static_assert(sizeof(NowFwChunk) == 12, "NowFwChunk must be 12 bytes");

/** @brief Firmware status (Node -> ICM). */
struct NOW_PACKED NowFwStatus {
  uint32_t image_id;
  uint32_t next_needed;
  uint32_t received_bytes;
  uint8_t  state;        /**< NowFwState. */
  uint8_t  rsv0;
  uint16_t last_error;   /**< 0 OK; else error code. */
};
/* FIX: size is 16 bytes */
static_assert(sizeof(NowFwStatus) == 16, "NowFwStatus must be 16 bytes");

/** @brief Firmware commit (ICM -> Node). */
struct NOW_PACKED NowFwCommit {
  uint32_t image_id;
  uint8_t  apply_at_boot;/**< 1=pending+reboot, 0=verify only. */
  uint8_t  sig_len;      /**< Signature length in bytes. */
  uint16_t rsv0;
  /* uint8_t signature[sig_len];  // Signature over SHA-256(image) */
};
static_assert(sizeof(NowFwCommit) == 8, "NowFwCommit must be 8 bytes"); /* FIX: add missing semicolon */

/** @brief Firmware abort (ICM -> Node). */
struct NOW_PACKED NowFwAbort {
  uint32_t image_id;
  uint8_t  reason;       /**< NowFwAbortReason. */
  uint8_t  rsv0;
  uint16_t rsv1;
};
static_assert(sizeof(NowFwAbort) == 8, "NowFwAbort must be 8 bytes");

/*======================================================================
=                          VARIANTS & VIEWS                            =
======================================================================*/

/**
 * @def NOW_ENABLE_VARIANTS
 * @brief Enable unified in-memory variants for easy routing (not wire format).
 */
#ifndef NOW_DISABLE_VARIANTS
#define NOW_ENABLE_VARIANTS 1
#endif

#if NOW_ENABLE_VARIANTS

/** @brief Lightweight span for variable-length payload tails. */
struct NowSpan { const uint8_t* data; uint16_t len; };

/** @brief View over a TOPO_PUSH (header + TLV blob). */
struct NowTopoPushView { NowTopoPush hdr; NowSpan blob; };

/** @brief View over a CONFIG_WRITE (header + value data). */
struct NowConfigWriteView { NowConfigWrite hdr; NowSpan value; };

/** @brief View over a FW_CHUNK (header + chunk data). */
struct NowFwChunkView { NowFwChunk hdr; NowSpan data; };

/** @brief Unified kinds for endpoint->ICM reports. */
enum NowEndpointReportKind : uint8_t {
  NOW_ER_SENS_REPORT = NOW_MT_SENS_REPORT,
  NOW_ER_RLY_STATE   = NOW_MT_RLY_STATE,
  NOW_ER_PMS_STATUS  = NOW_MT_PMS_STATUS,
  NOW_ER_PING_REPLY  = NOW_MT_PING_REPLY,
  NOW_ER_FW_STATUS   = NOW_MT_FW_STATUS
};

/** @brief Union of endpoint->ICM payloads (reports). */
struct NowEndpointReport {
  uint8_t kind; /**< Must equal NowHeader.msg_type of received frame. */
  union {
    NowSensReport sens_report;
    NowRlyState   rly_state;
    NowPmsStatus  pms_status;
    NowPingReply  ping_reply;
    NowFwStatus   fw_status;
  } u;
};

/** @brief Unified kinds for ICM->endpoint commands. */
enum NowIcmCommandKind : uint8_t {
  NOW_IC_CMD_PAIR_ACK     = NOW_MT_PAIR_ACK,
  NOW_IC_CMD_TOPO_PUSH    = NOW_MT_TOPO_PUSH,
  NOW_IC_CMD_NET_SET_CHAN = NOW_MT_NET_SET_CHAN,
  NOW_IC_CMD_CTRL_RELAY   = NOW_MT_CTRL_RELAY,
  NOW_IC_CMD_CONFIG_WRITE = NOW_MT_CONFIG_WRITE,
  NOW_IC_CMD_PING         = NOW_MT_PING,
  NOW_IC_CMD_TIME_SYNC    = NOW_MT_TIME_SYNC,
  NOW_IC_CMD_FW_BEGIN     = NOW_MT_FW_BEGIN,
  NOW_IC_CMD_FW_CHUNK     = NOW_MT_FW_CHUNK,
  NOW_IC_CMD_FW_COMMIT    = NOW_MT_FW_COMMIT,
  NOW_IC_CMD_FW_ABORT     = NOW_MT_FW_ABORT
};

/** @brief Union of ICM->endpoint payloads (commands). */
struct NowIcmCommand {
  uint8_t kind; /**< Must equal NowHeader.msg_type of outbound frame. */
  union {
    NowPairAck         pair_ack;
    NowTopoPushView    topo_push;
    NowNetSetChan      net_set_chan;
    NowCtrlRelay       ctrl_relay;
    NowConfigWriteView cfg_write;
    NowPing            ping;
    NowTimeSync        time_sync;
    NowFwBegin         fw_begin;
    NowFwChunkView     fw_chunk;
    NowFwCommit        fw_commit;
    NowFwAbort         fw_abort;
  } u;
};

/**
 * @brief Parsed frame view (convenience holder after decoding).
 * @details For variable-length payloads (TOPO, CFG value, FW chunk), only the
 *          fixed headers are present here; the data sits in the corresponding View.
 * @note     In v2H, all frames except PAIR_REQ include auth + HMAC.
 */
struct NowFrameView {
  NowHeader     hdr;

  /* Present on all frames except PAIR_REQ: */
  NowAuth128    auth;

  /* Present when (hdr.flags & NOW_FLAGS_HAS_TOPO): */
  uint8_t       has_topo;
  NowTopoToken128 topo;

  union {
    NowEndpointReport endpoint_report;
    NowIcmCommand     icm_command;

    /* Optional direct access to fixed-size payloads */
    NowPairAck        pair_ack;
    NowNetSetChan     net_set_chan;
    NowCtrlRelay      ctrl_relay;
    NowSensReport     sens_report;
    NowRlyState       rly_state;
    NowPmsStatus      pms_status;
    NowConfigWrite    cfg_write_hdr;
    NowPing           ping;
    NowPingReply      ping_reply;
    NowTimeSync       time_sync;
    NowFwBegin        fw_begin;
    NowFwChunk        fw_chunk_hdr;
    NowFwStatus       fw_status;
    NowFwCommit       fw_commit;
    NowFwAbort        fw_abort;
    NowTopoPush       topo_hdr;
  } pl;

  /* Mandatory on all frames except PAIR_REQ: */
  NowSecTrailer sec;
};
#endif /* NOW_ENABLE_VARIANTS */

/*======================================================================
=                          INLINE UTILITIES                             =
======================================================================*/

/**
 * @brief Returns true if @p virt_id denotes a physical endpoint.
 * @param virt_id Virtual index carried in the header.
 * @return true if @p virt_id == NOW_VIRT_PHYSICAL; false otherwise.
 */
static inline bool now_is_physical(uint8_t virt_id) { return virt_id == NOW_VIRT_PHYSICAL; }

#endif /* ESPNOW_API_H */
