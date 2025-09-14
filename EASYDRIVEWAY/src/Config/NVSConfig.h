/**************************************************************
 * Project : Driveway Lighting System
 * File    : NVSConfig.h — Shared NVS key definitions (strict)
 * Purpose : Required NVS namespaces/keys with DEFAULT values
 *           and clear comments on how each key is used.
 *
 * Rules:
 *   • Namespaces: ≤ 6 chars, UPPERCASE.
 *   • Keys:       exactly 6 chars, UPPERCASE.
 *   • Required-only per spec; no optional knobs.
 *   • All defaults are NON-EMPTY and sensible for first boot.
 *
 * Usage (define one before include):
 *   // #define NVS_ROLE_ICM
 *   // #define NVS_ROLE_SENS
 *   // #define NVS_ROLE_RELAY
 *   // #define NVS_ROLE_PMS
 **************************************************************/
#pragma once

// ============================================================
// Role selection (define exactly one before including this file)
// (Determines some role-specific defaults like KIND__/DEFNAM)
// ============================================================
#define NVS_ROLE_ICM
//#define NVS_ROLE_SENS
//#define NVS_ROLE_RELAY
//#define NVS_ROLE_PMS

// ============================================================
// System limits
// ============================================================
#ifndef NVS_MAX_SENS
  #define NVS_MAX_SENS   16   // Max number of Sensor nodes the ICM manages
#endif
#ifndef NVS_MAX_RELAY
  #define NVS_MAX_RELAY  16   // Max number of Relay nodes the ICM manages
#endif
#ifndef NVS_MAX_PMS
  #define NVS_MAX_PMS     1   // Only one Power module at a time
#endif

// ============================================================
// Namespaces (≤ 6 chars, UPPERCASE)
// ============================================================
#define NVS_NS_SYS   "SYS"    // Device identity & kind
#define NVS_NS_NET   "NET"    // ESP‑NOW channel / ICM MAC / paired flag
#define NVS_NS_ESP   "ESP"    // Admission token16
#define NVS_NS_IND   "IND"    // Permanent disable flags for indicators
#define NVS_NS_TOPO  "TOPO"   // Topology & neighbor/relay lists
#define NVS_NS_BND   "BND"    // Relay boundary mapping
#define NVS_NS_REG   "REG"    // ICM registry for peers
#define NVS_NS_WIFI  "WIFI"   // ICM Wi‑Fi credentials (AP/STA)
#define NVS_NS_BLE   "BLE"    // ICM BLE name
#define NVS_NS_AUTH  "AUTH"   // ICM pairing PIN
#define RESET_FLAG_KEY "RST"

// Utility defaults for MAC-like strings / JSON
#define NVS_DEF_MAC_EMPTY   "000000000000"  // 12 hex chars placeholder MAC
#define NVS_DEF_JSON_EMPTY  "{}"
#define NVS_DEF_JSON_ARRAY  "[]"

// ============================================================
// Common keys (ALL modules) — REQUIRED + DEFAULTS (non-empty)
// ============================================================
// SYS (identity) — persisted on every node and the ICM
#define NVS_KEY_SYS_KIND   "KIND__"  /* DEVICE_KIND: 0=ICM,1=PMS,2=RELAY,3=SENSOR
                                        Set by firmware build role or during provisioning.
                                        ICM uses this to classify nodes in the registry. */
#if   defined(NVS_ROLE_ICM)
  #define NVS_DEF_SYS_KIND   0
  #define NVS_DEF_SYS_DEFNM  "ICM"
#elif defined(NVS_ROLE_PMS)
  #define NVS_DEF_SYS_KIND   1
  #define NVS_DEF_SYS_DEFNM  "PMS"
#elif defined(NVS_ROLE_RELAY)
  #define NVS_DEF_SYS_KIND   2
  #define NVS_DEF_SYS_DEFNM  "RELAY"
#elif defined(NVS_ROLE_SENS)
  #define NVS_DEF_SYS_KIND   3
  #define NVS_DEF_SYS_DEFNM  "SENSOR"
#else
  #define NVS_DEF_SYS_KIND   0
  #define NVS_DEF_SYS_DEFNM  "NODE"
#endif

#define NOW_KIND_UNKNOWN 4
 
#define NVS_KEY_SYS_DEVID  "DEVID_"  /* Device ID/Serial shown in UI & logs.
                                        Set at manufacturing or first boot. */
#define NVS_DEF_SYS_DEVID  "NODE-0000"

#define NVS_KEY_SYS_HWREV  "HWREV_"  /* Hardware revision string (board rev). */
#define NVS_DEF_SYS_HWREV  "1"

#define NVS_KEY_SYS_SWVER  "SWVER_"  /* Firmware version string (semver). */
#define NVS_DEF_SYS_SWVER  "0.0.0"

#define NVS_KEY_SYS_BUILD  "BUILD_"  /* Build/commit identifier (short). */
#define NVS_DEF_SYS_BUILD  "0"

#define NVS_KEY_SYS_DEFNM  "DEFNAM"  /* Default human-readable name for UI. */
// DEFNAM default is chosen above via role (NVS_DEF_SYS_DEFNM).

// NET (pairing/link) — maintained on nodes
#define NVS_KEY_NET_CHAN   "CHAN__"  /* ESP‑NOW channel. Set by ICM via pairing/config. */
#define NVS_DEF_NET_CHAN   1
#define NVS_KEY_NET_ICMMAC "ICMMAC"  /* Paired ICM MAC (12-hex string). */
#define NVS_DEF_NET_ICMMAC NVS_DEF_MAC_EMPTY
#define NVS_KEY_NET_PAIRED "PAIRED"  /* 0/1: set to 1 once token/channel/ICM MAC are stored. */
#define NVS_DEF_NET_PAIRED 0

// ESP (admission) — nodes only
#define NVS_KEY_ESP_TOKEN  "TOKEN_"  /* 16-bit admission token assigned by ICM. */
#define NVS_DEF_ESP_TOKEN  0

// 2.4 ESP-NOW (persisted config)
#define NVS_KEY_MODE               "ESMODE"   // 6   0=AUTO,1=MANUAL
#define NVS_DEF_MODE           0U


// IND (permanent disable from ICM) — nodes only
#define NVS_KEY_IND_LEDDIS "LEDDIS"  /* 0/1: 1 = node MUST ignore IND_LED_* commands. */
#define NVS_DEF_IND_LEDDIS 0
#define NVS_KEY_IND_BUZDIS "BUZDIS"  /* 0/1: 1 = node MUST ignore IND_BUZ_* commands. */
#define NVS_DEF_IND_BUZDIS 0

// ============================================================
// SENSOR-only — neighbors + relay lists (REQUIRED)
// ============================================================
#ifdef NVS_ROLE_SENS
  // Neighbor sensors (stored on Sensor)
  #define NVS_KEY_TOPO_PRVMAC "PRVMAC"  /* Prev sensor MAC (12-hex). Used for handoff. */
  #define NVS_DEF_TOPO_PRVMAC NVS_DEF_MAC_EMPTY
  #define NVS_KEY_TOPO_PRVTOK "PRVTOK"  /* Prev sensor token16. */
  #define NVS_DEF_TOPO_PRVTOK 0
  #define NVS_KEY_TOPO_NXTMAC "NXTMAC"  /* Next sensor MAC (12-hex). Used for handoff. */
  #define NVS_DEF_TOPO_NXTMAC NVS_DEF_MAC_EMPTY
  #define NVS_KEY_TOPO_NXTTOK "NXTTOK"  /* Next sensor token16. */
  #define NVS_DEF_TOPO_NXTTOK 0

  // Relay lists around sensor (stringified JSON arrays)
  // Format: [{"mac":"A1B2C3D4E5F6","tok":123,"idx":1}, ...]
  #define NVS_KEY_TOPO_POSRLS "POSRLS"  /* Relays on positive direction from this sensor. */
  #define NVS_DEF_TOPO_POSRLS NVS_DEF_JSON_ARRAY
  #define NVS_KEY_TOPO_NEGRLS "NEGRLS"  /* Relays on negative direction from this sensor. */
  #define NVS_DEF_TOPO_NEGRLS NVS_DEF_JSON_ARRAY

  // Role flags (bitfield) — input/output designation if used by effects
  #define NVS_KEY_TOPO_ROLE   "ROLE__"
  #define NVS_DEF_TOPO_ROLE   0
#endif

// ============================================================
// RELAY-only — boundary mapping (REQUIRED on Relay)
// ============================================================
#ifdef NVS_ROLE_RELAY
  #define NVS_KEY_BND_SAMAC   "SAMAC_"  /* Boundary sensor A MAC (12-hex). */
  #define NVS_DEF_BND_SAMAC   NVS_DEF_MAC_EMPTY
  #define NVS_KEY_BND_SATOK   "SATOK_"  /* Boundary sensor A token16. */
  #define NVS_DEF_BND_SATOK   0
  #define NVS_KEY_BND_SBMAC   "SBMAC_"  /* Boundary sensor B MAC (12-hex). */
  #define NVS_DEF_BND_SBMAC   NVS_DEF_MAC_EMPTY
  #define NVS_KEY_BND_SBTOK   "SBTOK_"  /* Boundary sensor B token16. */
  #define NVS_DEF_BND_SBTOK   0
  #define NVS_KEY_BND_SPLIT   "SPLIT_"  /* Split rule enum for LEFT/RIGHT mapping. */
  #define NVS_DEF_BND_SPLIT   0
#endif

// ============================================================
// PMS-only — required-only (none beyond Common)
// ============================================================
#ifdef NVS_ROLE_PMS
  // Power module uses only Common keys per the system spec.
#endif

// ============================================================
// ICM-only — credentials, topology, per-device registry
// ============================================================
#ifdef NVS_ROLE_ICM

  #define GOTO_CONFIG_KEY             "GTCFG"    // 4
  #define RESET_FLAG_DEFAULT          false
  #define WEB_USER_KEY                "SEUSER"   // 6  session username
  #define WEB_PASS_KEY                "SEPASS"   // 6  session password

  #define WEB_USER_DEFAULT            "admin"    // default username
  #define WEB_PASS_DEFAULT            ""         // default password (empty => set via UI on first login)
  #define PASS_PIN_KEY                "PINCOD"   // 6
  #define PASS_PIN_DEFAULT            "12345678"

  // BLE (ICM broadcast name)
  #define NVS_KEY_BLE_NAME     "NAME__"  /* BLE advertising name shown to installers. */
  #define NVS_DEF_BLE_NAME     "ICM"
  #define NVS_KEY_BLE_PASSK "PASKEY"   // NEW: BLE passkey (6-digit)
  #define NVS_DEF_BLE_PASSK 123456
  #define NVS_KEY_AUTH_PIN  "PIN___"   // Web/BLE pairing PIN (6-digit)
  #define NVS_DEF_AUTH_PIN  123456

  // AUTH (password PIN for pairing UI)
  #define NVS_KEY_AUTH_PIN     "PIN___"  /* Numeric PIN required to authorize pairing. */
  #define NVS_DEF_AUTH_PIN     123456

  // WIFI (AP + STA) — used only by ICM
  #define NVS_KEY_WIFI_APSSID  "APSSID"  /* ICM hotspot SSID for local Web UI access. */
  #define NVS_DEF_WIFI_APSSID  "ICM_AP"
  #define NVS_KEY_WIFI_APKEY   "APKEY_"  /* ICM hotspot WPA2 key. */
  #define NVS_DEF_WIFI_APKEY   "12345678"
  #define NVS_KEY_WIFI_STSSID  "STSSID"  /* Station SSID for uplink (if used). */
  #define NVS_DEF_WIFI_STSSID  "NONENONE"
  #define NVS_KEY_WIFI_STKEY   "STKEY_"  /* Station key for uplink (if used). */
  #define NVS_DEF_WIFI_STKEY   "NONENONE"

  // Topology (stringified JSON) — authored by ICM and pushed to nodes
  #define NVS_KEY_TOPO_STRING  "TOPO__"
  #define NVS_DEF_TOPO_STRING  "{}"

  // Registry (all slave MACs as stringified JSON array)
  #define NVS_KEY_REG_SLMACS   "SLMACS"  /* Used by ICM to restore peer list at boot. */
  #define NVS_DEF_REG_SLMACS   NVS_DEF_JSON_ARRAY

  // Per-device registry (ICM) — fixed 6-char keys with number suffix
  // Use snprintf(key, 7, "S%02uMAC")/("S%02uTOK") and R%02u… for Relays.
  #define NVS_REG_S_MAC_FMT    "S%02uMAC"  /* Sensor i MAC (12-hex) */
  #define NVS_DEF_REG_S_MAC    NVS_DEF_MAC_EMPTY
  #define NVS_REG_S_TOK_FMT    "S%02uTOK"  /* Sensor i token16 */
  #define NVS_DEF_REG_S_TOK    0

  #define NVS_REG_R_MAC_FMT    "R%02uMAC"  /* Relay i MAC (12-hex) */
  #define NVS_DEF_REG_R_MAC    NVS_DEF_MAC_EMPTY
  #define NVS_REG_R_TOK_FMT    "R%02uTOK"  /* Relay i token16 */
  #define NVS_DEF_REG_R_TOK    0

  // Power module (only P01)
  #define NVS_REG_P_MAC_CONST  "P01MAC"   /* Power module MAC (12-hex) */
  #define NVS_DEF_REG_P_MAC    NVS_DEF_MAC_EMPTY
  #define NVS_REG_P_TOK_CONST  "P01TOK"   /* Power module token16 */
  #define NVS_DEF_REG_P_TOK    0
#endif

// ============================================================
// End of NVSConfig.h
// ============================================================
