/**************************************************************
 *  Project     : Driveway Lighting System
 *  File        : Now_API.h — Shared ESP-NOW Application Protocol (TOKEN=HEX STRING)
 *  Purpose     : Common message header, domains/opcodes, and payload structs.
 *                JSON-only topology. No application-layer ACKs.
 **************************************************************/
#pragma once
#include <stdint.h>

#ifndef NOW_MAX_RELAYS
  #define NOW_MAX_RELAYS   16
#endif
#ifndef NOW_MAX_SENSORS
  #define NOW_MAX_SENSORS  16
#endif
#ifndef NOW_MAX_POWER
  #define NOW_MAX_POWER     1
#endif
#ifndef NOW_TOKEN_HEX_LEN
  #define NOW_TOKEN_HEX_LEN 32
#endif

enum NowDeviceKind : uint8_t {
  NOW_KIND_ICM   = 0x00,
  NOW_KIND_PMS   = 0x01,
  NOW_KIND_RELAY = 0x02,
  NOW_KIND_SENS  = 0x03,
};

enum : uint16_t {
  NOW_STATE_MODE_AUTO   = 1u << 0,
  NOW_STATE_MODE_MANUAL = 1u << 1,
  NOW_STATE_UPDATING    = 1u << 2,
  NOW_STATE_STARTING_UP = 1u << 3,
  NOW_STATE_BUSY        = 1u << 4,
  NOW_STATE_PAIRING     = 1u << 5,
  NOW_STATE_IDLE        = 1u << 6,
};

#if defined(__GNUC__)
  #define NOW_PACKED __attribute__((packed))
#else
  #pragma pack(push, 1)
  #define NOW_PACKED
#endif

struct NOW_PACKED NowMsgHdr {
  uint8_t  ver;                          // =1
  uint8_t  dom;                          // domain
  uint8_t  op;                           // opcode
  uint32_t ts_ms;                        // sender timestamp
  uint16_t seq;                          // sequence
  char     token_hex[NOW_TOKEN_HEX_LEN]; // 32 ASCII hex (no NUL on wire)
};
static const uint8_t NOW_HDR_VER = 1;

enum NowDomain : uint8_t {
  NOW_DOM_SYS  = 0x00,
  NOW_DOM_PWR  = 0x01,
  NOW_DOM_REL  = 0x02,
  NOW_DOM_SEN  = 0x03,
  NOW_DOM_TOP  = 0x04,
  NOW_DOM_FW   = 0x05,
  NOW_DOM_LOG  = 0x06,
  NOW_DOM_IND  = 0x07,
  NOW_DOM_PR   = 0x08,
  NOW_DOM_CFG  = 0x09,
};

// ---------- SYS ----------
enum : uint8_t {
  SYS_HB        = 0x01,
  SYS_MODE_SET  = 0x02,
  SYS_PING      = 0x03,
  SYS_SET_CH    = 0x04,
  SYS_TIME_SYNC = 0x05,
  SYS_STATE_EVT = 0x06,
};
struct NOW_PACKED SysModePayload   { uint8_t mode; };
struct NOW_PACKED SysSetChPayload  { uint8_t new_ch; uint8_t rsv[3]; };
struct NOW_PACKED SysTimeSyncPayload { uint64_t epoch_ms; int32_t offset_ms; int16_t drift_ppm; uint8_t ver; };
struct NOW_PACKED SysHeartbeatPayload {
  uint16_t state_flags; uint8_t kind;
  uint8_t buz_on, buz_pat; uint8_t led_r, led_g, led_b, led_pat;
};

// ---------- PWR ----------
enum : uint8_t { PWR_REP=0x10, PWR_ON_OFF=0x11, PWR_SRC_SET=0x12, PWR_CLR_FLG=0x13, PWR_QRY=0x14 };
enum : uint8_t { PWR_FLAG_PDOWN=1<<0, PWR_FLAG_BLOW=1<<1, PWR_FLAG_SURGE=1<<2 };
struct NOW_PACKED PwrReportPayload { uint16_t vbus_mV,vbat_mV,ibus_mA,ibat_mA; int16_t temp_c_x100; uint8_t flags,rsv; };
struct NOW_PACKED PwrOnOffPayload { uint8_t on; };
struct NOW_PACKED PwrSrcPayload   { uint8_t src; };

// ---------- REL (Relay) ----------
enum : uint8_t {
  REL_REP    = 0x20,  // node→ICM: temp
  REL_SET    = 0x21,  // ICM/Sensor→node: ch,on
  REL_ON_FOR = 0x22,  // ICM/Sensor→node: mask,duration
  REL_SCHED  = 0x23,  // Sensor→Relay: schedule
  REL_QRY    = 0x24,  // ICM→node: request REL_REP
};
enum : uint8_t { REL_CH_LEFT=1<<0, REL_CH_RIGHT=1<<1, REL_CH_BOTH=(REL_CH_LEFT|REL_CH_RIGHT) };
struct NOW_PACKED RelReportPayload { int16_t temp_c_x100; };
struct NOW_PACKED RelSetPayload    { uint8_t ch; uint8_t on; };
struct NOW_PACKED RelOnForPayload  { uint8_t chMask; uint16_t on_ms; };
struct NOW_PACKED RelSchedPayload  { uint32_t t0_ms; uint16_t l_on_at,l_off_at,r_on_at,r_off_at; };

// ---------- SEN (Sensor) ----------
enum : uint8_t { SEN_REP=0x30, SEN_TRIG=0x31 };
struct NOW_PACKED SenReportPayload {
  int16_t  t_c_x100; uint16_t rh_x100; int32_t p_Pa; uint16_t lux_x10;
  uint8_t  is_day; uint8_t rsv; uint16_t tfA_mm; uint16_t tfB_mm;
};

// ---------- TOP (Topology, JSON only) ----------
enum : uint8_t {
  TOP_PUSH_SEN_JSON = 0x40, // ICM→SENSOR
  TOP_PUSH_RLY_JSON = 0x41, // ICM→RELAY
};

// ---------- FW / LOG / IND / PR / CFG ----------
enum : uint8_t { FW_CAPS=0x50, FW_BEGIN=0x51, FW_DATA=0x52, FW_END=0x53, FW_COMMIT=0x54, FW_STATUS=0x55, FW_POST_OK=0x56, FW_POST_FAIL=0x57 };
struct NOW_PACKED FwCapsPayload{ uint8_t slots; uint16_t max_chunk; uint8_t window; uint8_t boot_ver; };
struct NOW_PACKED FwBeginPayload{ uint8_t target; uint32_t size; uint8_t sha256[32]; uint32_t resume_off; };
struct NOW_PACKED FwDataPayload { uint32_t off; uint16_t n; uint16_t csize; };
struct NOW_PACKED FwEndPayload  { uint8_t sha256[32]; };
struct NOW_PACKED FwStatusPayload { uint8_t state; uint32_t received,size; uint8_t slot,err; };
struct NOW_PACKED FwPostBootPayload { uint8_t ok,slot; char ver[12]; };

enum : uint8_t { LOG_STAT=0x60, LOG_LIST=0x61, LOG_TAIL=0x62, LOG_FETCH=0x63, LOG_CLEAR=0x64 };
struct NOW_PACKED LogStatPayload  { uint32_t cap_kb,used_kb,rec_cnt; };
struct NOW_PACKED LogFetchPayload { uint32_t off; uint16_t len; };

enum : uint8_t { IND_BUZ_SET=0x70, IND_BUZ_PAT=0x71, IND_LED_SET=0x72, IND_LED_PAT=0x73, IND_ALERT=0x74 };
struct NOW_PACKED BuzzerSetPayload { uint8_t en; uint16_t hz; uint8_t vol; uint16_t dur_ms; };
struct NOW_PACKED BuzzerPatPayload { uint8_t pat,reps; uint16_t dur_ms; };
struct NOW_PACKED LedSetPayload    { uint8_t r,g,b,br; uint16_t dur_ms; };
struct NOW_PACKED LedPatPayload    { uint8_t pat; uint16_t dur_ms; };
enum : uint8_t { IND_ALERT_INFO=0, IND_ALERT_WARN=1, IND_ALERT_ERR=2 };
struct NOW_PACKED AlertPayload     { uint8_t lvl; uint16_t dur_ms; };

enum : uint8_t { PR_DEVINFO=0x80, PR_ASSIGN=0x81, PR_COMMIT=0x82, PR_REKEY=0x83, PR_UNPAIR=0x84 };
struct NOW_PACKED DevInfoPayload   { uint8_t kind; char devid[16]; char hwrev[8]; char swver[12]; char build[16]; };
struct NOW_PACKED PrAssignPayload  { char token_hex[NOW_TOKEN_HEX_LEN]; uint8_t icm_mac[6]; uint8_t channel; };

enum : uint8_t { CFG_GET=0x90, CFG_SET=0x91, CFG_COMMIT=0x92 };

#ifndef __GNUC__
  #pragma pack(pop)
#endif
