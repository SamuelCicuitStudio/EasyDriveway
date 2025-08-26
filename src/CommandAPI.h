/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : CommandAPI.h
 *  Purpose     : Shared command/enum catalog + packed payloads
 **************************************************************/
#pragma once
#include <Arduino.h>
#include <stdint.h>

// ======================= Limits ===============================
#ifndef ICM_MAX_RELAYS
#define ICM_MAX_RELAYS   16
#endif
#ifndef ICM_MAX_SENSORS
#define ICM_MAX_SENSORS   5     // middle sensors; entrance/parking are special
#endif

// ======================= Special Presence Indices =============
static constexpr uint8_t PRES_IDX_ENTRANCE = 0xFE;  // special A
static constexpr uint8_t PRES_IDX_PARKING  = 0xFF;  // special B

// ======================= Sequence Direction ===================
enum class SeqDir : uint8_t { UP = 0, DOWN = 1 };

// ======================= Domains ==============================
enum class CmdDomain : uint8_t {
  SYS   = 0x00,
  POWER = 0x01,
  RELAY = 0x02,
  SENS  = 0x03,
  TOPO  = 0x04,
  SEQ   = 0x05
};

// ======================= System/Common Ops ====================
enum : uint8_t {
  SYS_INIT      = 0x01,   // token exchange/hello
  SYS_ACK       = 0x02,   // app-level ack payload carries CTR
  SYS_HB        = 0x03,   // heartbeat
  SYS_CAPS      = 0x04,   // capabilities blob
  SYS_MODE      = 0x05,   // set mode: 0=auto,1=manual
  SYS_PING      = 0x06    // echo
};

// ======================= Power Ops ============================
enum : uint8_t {
  PWR_GET       = 0x10,
  PWR_SET       = 0x11,   // body: uint8_t on(0/1)
  PWR_REQSDN    = 0x12,
  PWR_CLRF      = 0x13,
  PWR_GET_TEMP  = 0x14
};

// ======================= Relay Ops ============================
enum : uint8_t {
  REL_GET       = 0x20,
  REL_SET_CH    = 0x21,   // body: uint8_t ch, uint8_t on
  REL_SET_MODE  = 0x22,    // body: uint8_t ch, uint8_t mode(0 auto/1 manual)
  REL_GET_TEMP  = 0x23
};

// ======================= Sensor Ops ===========================
enum : uint8_t {
  SENS_GET      = 0x30,
  SENS_SET_MODE = 0x31,   // body: uint8_t mode(0 auto/1 manual)
  SENS_TRIG     = 0x32    // optional details
};

// ======================= Topology Ops =========================
enum : uint8_t {
  TOPO_SET_NEXT = 0x40,   // relay: set "next" hop (MAC/IP)
  TOPO_SET_DEP  = 0x41,   // sensor: set target relay idx+MAC/IP
  TOPO_CLEAR    = 0x42,
  TOPO_PUSH     = 0x43
};

// ======================= Sequence Ops =========================
enum : uint8_t {
  SEQ_START = 0x50,       // payload: SeqStartPayload (dir)
  SEQ_STOP  = 0x51
};

struct __attribute__((packed)) SeqStartPayload {
  uint8_t dir;     // SeqDir::UP or SeqDir::DOWN
  uint8_t rsv[3];  // align/future
};

// ======================= Header + Flags =======================
struct __attribute__((packed)) IcmMsgHdr {
  uint8_t  ver;      // =1
  uint8_t  dom;      // CmdDomain
  uint8_t  op;       // per-domain opcode
  uint8_t  flags;    // bit0: ACK requested
  uint32_t ts;       // unix time
  uint16_t ctr;      // rolling counter (from master)
  uint8_t  tok16[16];// first 16B of token
};
static constexpr uint8_t HDR_FLAG_ACKREQ = 1 << 0;

// ======================= System payloads ======================
enum : uint8_t { MODE_AUTO=0, MODE_MAN=1 };
struct __attribute__((packed)) SysModePayload { uint8_t mode; };
struct __attribute__((packed)) SysAckPayload  { uint16_t ctr; uint8_t code; uint8_t rsv; };

// ======================= Topology payloads ====================
// Relay next hop (sent to a relay node)
struct __attribute__((packed)) TopoNextHop {
  uint8_t  myIdx;       // relay index on the ICM (0..N-1)
  uint8_t  reserved;    // align/future
  uint8_t  nextMac[6];  // next relay MAC
  uint32_t nextIPv4;    // next relay IPv4 (network order)
};

// Sensor→target dependency (sent to a sensor node)
struct __attribute__((packed)) TopoDependency {
  uint8_t  sensIdx;     // 0..(ICM_MAX_SENSORS-1) or 0xFE/0xFF
  uint8_t  targetType;  // 1=relay
  uint8_t  targetIdx;   // relay index 0..N-1
  uint8_t  reserved;    // align
  uint8_t  targetMac[6];
  uint32_t targetIPv4;  // network order
};

struct __attribute__((packed)) TopoSensorBundle {
  uint8_t  sensIdx;        // 0..(ICM_MAX_SENSORS-1) or 0xFE/0xFF
  uint8_t  targetIdx;      // relay index 0..N-1
  uint8_t  targetMac[6];   // where THIS sensor should send triggers
  uint8_t  targetTok16[16];// token that TARGET RELAY expects in header
  uint32_t targetIPv4;     // network order
};

struct __attribute__((packed)) TopoRelayBundle {
  uint8_t  myIdx;          // relay index 0..N-1
  uint8_t  hasPrev;        // 0/1
  uint8_t  prevSensIdx;    // sensor index (or FE/FF) that precedes this relay in chain
  uint8_t  prevSensMac[6]; // previous sensor MAC
  uint8_t  prevSensTok16[16]; // token that PREV SENSOR expects (for optional acks)
  uint8_t  hasNext;        // 0/1
  uint8_t  nextIdx;        // next relay index in chain
  uint8_t  nextMac[6];     // next relay MAC
  uint8_t  nextTok16[16];  // token that NEXT RELAY expects in header
  uint32_t nextIPv4;       // next relay IPv4 (network order)
};// ======================= Telemetry payloads ===================
// Temperature reply (used by PWR_GET_TEMP and REL_GET_TEMP)
struct __attribute__((packed)) TempPayload {
  int16_t tC_x100;   // temperature in °C × 100 (e.g. 3256 -> 32.56°C)
  int16_t raw;       // optional raw ADC / sensor code (or 0 if N/A)
  uint8_t ok;        // 1=valid reading, 0=not available
  uint8_t src;       // sensor index/type at the module (freeform)
};