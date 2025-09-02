/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : CommandAPI.h — regrouped by domain/concern
 *  Note        : Backward-compatible update. Adds zero-centered
 *                sensor topology and relay boundary bundles.
 **************************************************************/
#pragma once
#include <Arduino.h>
#include <stdint.h>

// ============================================================================
// LIMITS & CORE TYPES
// ============================================================================
#ifndef ICM_MAX_RELAYS
#define ICM_MAX_RELAYS   16
#endif
#ifndef ICM_MAX_SENSORS
#define ICM_MAX_SENSORS   5   // middle sensors; entrance/parking are special
#endif

// Special indices for presence sensors (entrance/parking)
static constexpr uint8_t PRES_IDX_ENTRANCE = 0xFE;  // special A
static constexpr uint8_t PRES_IDX_PARKING  = 0xFF;  // special B

// Global sequence direction (legacy; still supported)
enum class SeqDir : uint8_t { UP = 0, DOWN = 1 };

// Command domains
enum class CmdDomain : uint8_t {
  SYS   = 0x00,
  POWER = 0x01,
  RELAY = 0x02,
  SENS  = 0x03,
  TOPO  = 0x04,
  SEQ   = 0x05
};

// ============================================================================
// DOMAIN OPCODES
// ============================================================================
// --- System/Common ops ---
enum : uint8_t {
  SYS_INIT      = 0x01,   // token exchange/hello
  SYS_ACK       = 0x02,   // app-level ack payload carries CTR
  SYS_HB        = 0x03,   // heartbeat
  SYS_CAPS      = 0x04,   // capabilities blob
  SYS_MODE      = 0x05,   // set mode: 0=auto,1=manual
  SYS_PING      = 0x06,   // echo
  SYS_SET_CH    = 0x07    // tell node to switch ESP-NOW channel
};

// --- Power ops ---
enum : uint8_t {
  PWR_GET       = 0x10,
  PWR_SET       = 0x11,   // body: uint8_t on(0/1)
  PWR_REQSDN    = 0x12,
  PWR_CLRF      = 0x13,
  PWR_GET_TEMP  = 0x14
};

// --- Relay ops ---
enum : uint8_t {
  REL_GET       = 0x20,
  REL_SET_CH    = 0x21,   // body: uint8_t ch, uint8_t on
  REL_SET_MODE  = 0x22,   // body: uint8_t ch, uint8_t mode(0 auto/1 manual)
  REL_GET_TEMP  = 0x23
};

// --- Sensor ops ---
enum : uint8_t {
  SENS_GET         = 0x30,
  SENS_SET_MODE    = 0x31, // body: uint8_t mode(0 auto/1 manual)
  SENS_TRIG        = 0x32, // optional details
  SENS_GET_DAYNIGHT= 0x33,  // query only the day/night state
  SENS_GET_TFRAW   = 0x34,  // one-shot: TF-Luna A/B raw samples
  SENS_GET_ENV     = 0x35   // one-shot: BME280 + ALS (+ day/night flag)
};

// --- Topology ops ---
enum : uint8_t {
  TOPO_PUSH_ZC_SENSOR      = 0x44,   // zero-centered bundle to a sensor
  TOPO_PUSH_BOUNDARY_RELAY = 0x45    // boundary sensors bundle to a relay
};

// --- Sequence ops (legacy) ---
enum : uint8_t {
  SEQ_START = 0x50,       // payload: SeqStartPayload (dir)
  SEQ_STOP  = 0x51
};

// ============================================================================
// MESSAGE HEADER (common)
// ============================================================================
struct __attribute__((packed)) IcmMsgHdr {
  uint8_t  ver;      // =1
  uint8_t  dom;      // CmdDomain
  uint8_t  op;       // per-domain opcode
  uint8_t  flags;    // bit0: ACK requested
  uint32_t ts;       // unix time
  uint16_t ctr;      // rolling counter (from master)
  uint8_t  tok16[16];// first 16B of token (receiver's token)
};
static constexpr uint8_t HDR_FLAG_ACKREQ = 1 << 0;

// ============================================================================
// SYSTEM PAYLOADS
// ============================================================================
enum : uint8_t { MODE_AUTO=0, MODE_MAN=1 };
struct __attribute__((packed)) SysModePayload { uint8_t mode; };
struct __attribute__((packed)) SysAckPayload  { uint16_t ctr; uint8_t code; uint8_t rsv; };

// Channel change instruction to nodes
struct __attribute__((packed)) SysSetChPayload {
  uint8_t  new_ch;        // 1..13
  uint8_t  window_s;      // grace window after switchover_ts (seconds)
  uint16_t rsv;           // align
  uint32_t switchover_ts; // UNIX time when node should switch
};

// ============================================================================
// SEQUENCE PAYLOADS (legacy)
// ============================================================================
struct __attribute__((packed)) SeqStartPayload {
  uint8_t dir;     // SeqDir::UP or SeqDir::DOWN
  uint8_t rsv[3];  // align/future
};

// ============================================================================
// TELEMETRY PAYLOADS (shared by domains)
// ============================================================================
// Temperature reply (used by PWR_GET_TEMP and REL_GET_TEMP)
struct __attribute__((packed)) TempPayload {
  int16_t tC_x100;   // °C × 100
  int16_t raw;       // optional raw ADC / code (or 0)
  uint8_t ok;        // 1=valid, 0=N/A
  uint8_t src;       // module-local source index/type
};

// Day/Night reply (used by SENS_GET_DAYNIGHT)
struct __attribute__((packed)) DayNightPayload {
  uint8_t is_day;   // 1=day, 0=night
  uint8_t ok;       // 1=valid, 0=N/A
  uint16_t raw;     // optional raw ADC/code
  uint8_t src;      // module-local source index/type
};

// Power status block (for PWR_GET / async status)
struct __attribute__((packed)) PowerStatusPayload {
  uint8_t  ver;        // =1 (bump if layout changes)
  uint8_t  on;         // 1=48V rail ON, 0=OFF
  uint8_t  fault;      // bitfield (OVP/UVP/OCP/OTP/…)
  uint8_t  rsv;        // align

  uint16_t vbus_mV;    // 48V bus (mV)
  uint16_t ibus_mA;    // 48V current (mA)
  uint16_t vbat_mV;    // battery (mV)
  uint16_t ibat_mA;    // battery (mA), +chg / –dischg

  int16_t  tC_x100;    // board temp *100 (optional; 0 if N/A)
  uint8_t  ok;         // 1=valid, 0=N/A
  uint8_t  src;        // sensor id/source or 0
};

// ============================================================================
// NEW TOPOLOGY PAYLOADS (zero-centered sensor model & relay boundaries)
// ============================================================================

// Relay coordinate entry as seen from a sensor (zero-centered index)
struct __attribute__((packed)) ZcRelEntry {
  uint8_t  relayIdx;       // 0..N-1 (ICM index)
  int8_t   relPos;         // ...,-3,-2,-1, +1,+2,+3,... (0 is reserved for the sensor itself)
  uint8_t  relayMac[6];    // MAC of that relay
  uint8_t  relayTok16[16]; // token that RELAY expects in header (so sensor can talk to it)
};

// Zero-centered bundle to a sensor: neighbors + ordered lists (neg and pos)
struct __attribute__((packed)) TopoZeroCenteredSensor {
  uint8_t  sensIdx;        // sensor index (0..ICM_MAX_SENSORS-1) or 0xFE/0xFF for specials

  // Neighboring sensors for handoff (optional if at ends)
  uint8_t  hasPrev;        // 0/1
  uint8_t  prevSensIdx;
  uint8_t  prevSensMac[6];
  uint8_t  prevSensTok16[16]; // token that PREV SENSOR expects (if you want sensor->sensor acks)

  uint8_t  hasNext;        // 0/1
  uint8_t  nextSensIdx;
  uint8_t  nextSensMac[6];
  uint8_t  nextSensTok16[16];

  // Counts for variable-length arrays
  uint8_t  nNeg;           // number of entries on the negative side (…,-3,-2,-1)
  uint8_t  nPos;           // number on the positive side (+1,+2,+3,…)
  uint16_t rsv;            // align

  // Followed in the frame by: ZcRelEntry neg[nNeg], then ZcRelEntry pos[nPos]
  // (ICM builds the two arrays contiguously)
};

// Deterministic split rule for the meeting of waves (optional hint)
enum : uint8_t {
  SPLIT_RULE_MAC_ASC_IS_POS_LEFT = 0,  // MAC(low)→MAC(high) defines + direction; map + to Left
  SPLIT_RULE_WAVEID_PARITY       = 1   // even wave_id→Left, odd→Right
};

// Boundary bundle to a relay: who can command me (two sensors flanking me)
struct __attribute__((packed)) TopoRelayBoundary {
  uint8_t  myIdx;          // relay index 0..N-1

  uint8_t  hasA;           // 0/1 — first boundary sensor (directionless naming)
  uint8_t  aSensIdx;
  uint8_t  aSensMac[6];
  uint8_t  aSensTok16[16]; // optional: if relay will send ACKs/status to sensor

  uint8_t  hasB;           // 0/1 — second boundary sensor
  uint8_t  bSensIdx;
  uint8_t  bSensMac[6];
  uint8_t  bSensTok16[16]; // optional

  uint8_t  splitRule;      // deterministic split hint (see enum)
  uint8_t  rsv[3];
};
// ===== NEW SENSOR RAW PAYLOADS =====
struct __attribute__((packed)) TfLunaRawPayload {
  uint8_t  ver;        // =1
  uint8_t  which;      // 0=both, 1=A, 2=B (sensor may still fill both)
  uint16_t rate_hz;    // configured device rate (optional; 0 if N/A)

  // A
  uint16_t distA_mm;   // 0 if invalid
  uint16_t ampA;       // TF-Luna strength/AMP if available, else 0
  uint8_t  okA;        // 1=valid sample, 0=N/A

  // B
  uint16_t distB_mm;   // 0 if invalid
  uint16_t ampB;
  uint8_t  okB;

  uint32_t t_ms;       // sensor-local monotonic ms when sampled
};

struct __attribute__((packed)) SensorEnvPayload {
  uint8_t  ver;        // =1
  // BME280
  int16_t  tC_x100;    // °C *100 (from BME280)
  uint16_t rh_x100;    // %RH *100
  int32_t  p_Pa;       // pressure in Pa (or 0 if N/A)

  // ALS (+ day/night mirror)
  uint16_t lux_x10;    // lux *10 (or 0 if N/A)
  uint8_t  is_day;     // 1=day, 0=night, 255=unknown

  // Validity bits to be explicit
  uint8_t  okT;        // temp valid
  uint8_t  okH;        // humidity valid
  uint8_t  okP;        // pressure valid
  uint8_t  okL;        // lux valid
};

// ============================================================================
// EXTENSIONS: Sensor↔Relay & Sensor↔Sensor commands
// ============================================================================

// --- Relay ops (extensions) ---
enum : uint8_t {
  REL_ON_FOR   = 0x24   // body: RelOnForPayload (L/R/Both on for duration)
};

// --- Sensor ops (extensions) ---
enum : uint8_t {
  SENS_WAVE_HDR  = 0x36   // body: WaveHdrPayload (lane, dir, speed, eta)
};

// ===== NEW INTERACTION PAYLOADS =====

// Simple "turn-on for duration" command from a Sensor to a Relay.
// Relay should enable the requested channel(s) immediately (or after `delay_ms`) for `on_ms`
// then turn off automatically. TTL allows the relay to drop late frames if >0.
enum : uint8_t { REL_CH_LEFT = 1<<0, REL_CH_RIGHT = 1<<1, REL_CH_BOTH = (REL_CH_LEFT|REL_CH_RIGHT) };
struct __attribute__((packed)) RelOnForPayload {
  uint8_t  ver;        // =1
  uint8_t  chMask;     // bit0=Left, bit1=Right (REL_CH_*)
  uint16_t on_ms;      // how long to stay ON before auto-OFF
  uint16_t delay_ms;   // optional start delay (0 = immediate)
  uint16_t ttl_ms;     // drop if received after now+ttl (0 = no TTL)
  uint8_t  rsv[2];
};

// Compact wave header from Sensor -> next/prev Sensor to pre-arm the section.
// Lets the neighbor skip recalculation and follow quickly.
struct __attribute__((packed)) WaveHdrPayload {
  uint8_t  ver;          // =1
  uint8_t  lane;         // 0=Left, 1=Right
  int8_t   dir;          // +1 = toward +pos relays, -1 = toward -neg
  uint8_t  wave_id;      // optional identifier (0 if not used)
  uint16_t speed_mmps;   // vehicle speed in mm/s
  uint32_t eta_ms;       // estimated time until it reaches the neighbor boundary
  uint8_t  rsv[2];
};

