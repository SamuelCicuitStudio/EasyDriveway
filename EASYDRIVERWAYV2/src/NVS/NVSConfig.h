/**************************************************************
 *  Project     : EasyDriveway
 *  File        : NVSConfig.h
 *  Purpose     : Central NVS namespaces + EXACT 6-char keys with
 *                role-based defaults (ICM, PMS, REL, SENS, REMU, SEMU).
 *                Includes per-virtual formats for SEMU/REMU topology.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef NVS_CONFIG_H
#define NVS_CONFIG_H
#pragma once

/***********************
 * INCLUDES
 ***********************/
#include "Config/SetRole.h"

/**
 * @brief Utility defaults for NVS string and JSON placeholders.
 */
#define NVS_DEF_MAC_EMPTY  "000000000000"   //!< 12 hex chars
#define NVS_DEF_JSON_OBJ   "{}"             //!< Default empty JSON object
#define NVS_DEF_JSON_ARR   "[]"             //!< Default empty JSON array

/****************************************************
 * Namespaces (≤ 6 chars, UPPERCASE)
 ****************************************************/
/** @name NVS Namespaces */
/** @{ */
#define NVS_NS_SYS    "SYS"   //!< Identity
#define NVS_NS_NET    "NET"   //!< Channel, paired ICM MAC
#define NVS_NS_ESP    "ESP"   //!< Admission/pairing token with ICM
#define NVS_NS_IND    "IND"   //!< Indicator disables / policies
#define NVS_NS_TOPO   "TOPO"  //!< Neighbors, relay lists, per-virtual maps
#define NVS_NS_BND    "BND"   //!< Relay boundary mapping
#define NVS_NS_REG    "REG"   //!< ICM registry of peers
#define NVS_NS_WIFI   "WIFI"  //!< ICM Wi-Fi creds
#define NVS_NS_BLE    "BLE"   //!< ICM BLE name/key
#define NVS_NS_AUTH   "AUTH"  //!< ICM 6-digit PIN
/** @} */

/****************************************************
 * Common keys (ALL roles) — exactly 6 chars
 ****************************************************/
/** @name SYS (identity) */
/** @{ */
#define NVS_KEY_KIND   "KIND__"  //!< 0=ICM,1=PMS,2=REL,3=SENS,4=REMU,5=SEMU
#define NVS_KEY_DEVID  "DEVID_"
#define NVS_KEY_HWREV  "HWREV_"
#define NVS_KEY_SWVER  "SWVER_"
#define NVS_KEY_BUILD  "BUILD_"
#define NVS_KEY_DEFNM  "DEFNAM"
/** @} */

/** @name Role-chosen defaults */
/** @{ */
#if   defined(NVS_ROLE_ICM)
  #define NVS_DEF_KIND   0
  #define NVS_DEF_DEFNM  "ICM"
#elif defined(NVS_ROLE_PMS)
  #define NVS_DEF_KIND   1
  #define NVS_DEF_DEFNM  "PMS"
#elif defined(NVS_ROLE_RELAY)
  #define NVS_DEF_KIND   2
  #define NVS_DEF_DEFNM  "RELAY"
#elif defined(NVS_ROLE_SENS)
  #define NVS_DEF_KIND   3
  #define NVS_DEF_DEFNM  "SENSOR"
#elif defined(NVS_ROLE_REMU)
  #define NVS_DEF_KIND   4
  #define NVS_DEF_DEFNM  "REMU"
#elif defined(NVS_ROLE_SEMU)
  #define NVS_DEF_KIND   5
  #define NVS_DEF_DEFNM  "SEMU"
#else
  #define NVS_DEF_KIND   0
  #define NVS_DEF_DEFNM  "NODE"
#endif
/** @} */

/** @name Common default identity values */
/** @{ */
#define NVS_DEF_DEVID  "NODE-0000"
#define NVS_DEF_HWREV  "1"
#define NVS_DEF_SWVER  "0.0.0"
#define NVS_DEF_BUILD  "0"
/** @} */

/****************************************************
 * NET (pairing/link with ICM)
 ****************************************************/
/** @name NET */
/** @{ */
#define NVS_KEY_CHAN   "CHAN__"
#define NVS_DEF_CHAN   1
#define NVS_KEY_ICMMAC "ICMMAC"            //!< ICM MAC saved on all non-ICM
#define NVS_DEF_ICMMAC NVS_DEF_MAC_EMPTY
#define NVS_KEY_PAIRED "PAIRED"            //!< Paired state with ICM (bool 0/1)
#define NVS_DEF_PAIRED 0
/** @} */

/****************************************************
 * ESP (admission token used with ICM)
 ****************************************************/
#define NVS_KEY_TOKEN  "TOKEN_"
#define NVS_DEF_TOKEN  0

/****************************************************
 * IND (indicator policy)
 * (Note: keys below intentionally defined twice in source;
 *  preserved verbatim to keep original logic unchanged)
 ****************************************************/
#define NVS_KEY_LEDDIS "LEDDIS"   //!< LED disable (bool 0/1)
#define NVS_DEF_LEDDIS 0
#define NVS_KEY_BUZDIS "BUZDIS"   //!< Buzzer disable (bool 0/1)
#define NVS_DEF_BUZDIS 0

// Existing (duplicated by original source; kept as-is)
#define NVS_KEY_LEDDIS "LEDDIS"   //!< LED disable (bool 0/1)
#define NVS_DEF_LEDDIS 0
#define NVS_KEY_BUZDIS "BUZDIS"   //!< Buzzer disable (bool 0/1)
#define NVS_DEF_BUZDIS 0

/** @name Extra indicator polarity/feedback flags */
/** @{ */
#define NVS_KEY_RGBALW "RGBALW"   //!< RGB LED active-low (bool 0/1)
#define NVS_DEF_RGBALW 1
#define NVS_KEY_RGBFBK "RGBFBK"   //!< RGB LED feedback enabled (bool 0/1)
#define NVS_DEF_RGBFBK 1
#define NVS_KEY_BUZAHI "BUZAHI"   //!< Buzzer active-high (bool 0/1)
#define NVS_DEF_BUZAHI 1
#define NVS_KEY_BUZFBK "BUZFBK"   //!< Buzzer feedback enabled (bool 0/1)
#define NVS_DEF_BUZFBK 1
/** @} */

/****************************************************
 * SENSOR (production) — neighbors + dependent relays
 ****************************************************/
#ifdef NVS_ROLE_SENS
  /** @name SENSOR: Neighbor chain and dependent relays */
  /** @{ */
  #define NVS_KEY_PRVMAC "PRVMAC"
  #define NVS_DEF_PRVMAC NVS_DEF_MAC_EMPTY
  #define NVS_KEY_PRVTOK "PRVTOK"
  #define NVS_DEF_PRVTOK 0
  #define NVS_KEY_NXTMAC "NXTMAC"
  #define NVS_DEF_NXTMAC NVS_DEF_MAC_EMPTY
  #define NVS_KEY_NXTTOK "NXTTOK"
  #define NVS_DEF_NXTTOK 0

  #define NVS_KEY_POSRLS "POSRLS"  //!< JSON array
  #define NVS_DEF_POSRLS NVS_DEF_JSON_ARR
  #define NVS_KEY_NEGRLS "NEGRLS"  //!< JSON array
  #define NVS_DEF_NEGRLS NVS_DEF_JSON_ARR

  #define NVS_KEY_ROLE__ "ROLE__"  //!< Bitfield (reserved)
  #define NVS_DEF_ROLE__ 0
  /** @} */
#endif

/****************************************************
 * RELAY (production) — boundary sensor mapping
 ****************************************************/
#ifdef NVS_ROLE_RELAY
  #define NVS_KEY_SAMAC  "SAMAC_"   //!< Sensor A (left/prev) MAC
  #define NVS_DEF_SAMAC  NVS_DEF_MAC_EMPTY
  #define NVS_KEY_SATOK  "SATOK_"   //!< Sensor A token
  #define NVS_DEF_SATOK  0
  #define NVS_KEY_SBMAC  "SBMAC_"   //!< Sensor B (right/next) MAC
  #define NVS_DEF_SBMAC  NVS_DEF_MAC_EMPTY
  #define NVS_KEY_SBTOK  "SBTOK_"   //!< Sensor B token
  #define NVS_DEF_SBTOK  0
  #define NVS_KEY_SPLIT  "SPLIT_"   //!< Split boundary index
  #define NVS_DEF_SPLIT  0
#endif

/****************************************************
 * SEMU (sensor emulator) — virtual sensors + neighbors
 ****************************************************/
#ifdef NVS_ROLE_SEMU
  #define NVS_KEY_SCOUNT "SCOUNT"   //!< Number of virtual sensors
  #define NVS_DEF_SCOUNT 8

  // Global neighbors (device-level) — optional
  #define NVS_KEY_PRVMAC "PRVMAC"
  #define NVS_DEF_PRVMAC NVS_DEF_MAC_EMPTY
  #define NVS_KEY_PRVTOK "PRVTOK"
  #define NVS_DEF_PRVTOK 0
  #define NVS_KEY_NXTMAC "NXTMAC"
  #define NVS_DEF_NXTMAC NVS_DEF_MAC_EMPTY
  #define NVS_KEY_NXTTOK "NXTTOK"
  #define NVS_DEF_NXTTOK 0

  // Dependent relays (JSON arrays)
  #define NVS_KEY_POSRLS "POSRLS"
  #define NVS_DEF_POSRLS NVS_DEF_JSON_ARR
  #define NVS_KEY_NEGRLS "NEGRLS"
  #define NVS_DEF_NEGRLS NVS_DEF_JSON_ARR

  // Per-virtual sensor token (V01TOK..VxxTOK)
  #define NVS_SEMU_VTOK_FMT "V%02uTOK"

  // Per-virtual prev/next neighbors (P01MAC/P01TOK, N01MAC/N01TOK)
  #define NVS_SEMU_PMAC_FMT "P%02uMAC"
  #define NVS_SEMU_PTOK_FMT "P%02uTOK"
  #define NVS_SEMU_NMAC_FMT "N%02uMAC"
  #define NVS_SEMU_NTOK_FMT "N%02uTOK"
#endif

/****************************************************
 * REMU (relay emulator) — virtual relays + boundaries
 ****************************************************/
#ifdef NVS_ROLE_REMU
  #define NVS_KEY_RCOUNT "RCOUNT"   //!< Number of virtual relays
  #define NVS_DEF_RCOUNT 16

  // Global boundary defaults (device-level; optional)
  #define NVS_KEY_SAMAC  "SAMAC_"
  #define NVS_DEF_SAMAC  NVS_DEF_MAC_EMPTY
  #define NVS_KEY_SATOK  "SATOK_"
  #define NVS_DEF_SATOK  0
  #define NVS_KEY_SBMAC  "SBMAC_"
  #define NVS_DEF_SBMAC  NVS_DEF_MAC_EMPTY
  #define NVS_KEY_SBTOK  "SBTOK_"
  #define NVS_DEF_SBTOK  0
  #define NVS_KEY_SPLIT  "SPLIT_"
  #define NVS_DEF_SPLIT  0

  // Per-virtual relay token (O01TOK..OxxTOK)
  #define NVS_REMU_OTOK_FMT "O%02uTOK"

  // Per-virtual boundary sensors (A01MAC/A01TOK, B01MAC/B01TOK)
  #define NVS_REMU_AMAC_FMT "A%02uMAC"
  #define NVS_REMU_ATOK_FMT "A%02uTOK"
  #define NVS_REMU_BMAC_FMT "B%02uMAC"
  #define NVS_REMU_BTOK_FMT "B%02uTOK"
#endif

/****************************************************
 * PMS — required-only (relies on common keys)
 ****************************************************/
#ifdef NVS_ROLE_PMS
  // No additional persistent keys here (telemetry is live).
#endif

/****************************************************
 * ICM — credentials, topology, registry
 ****************************************************/
#ifdef NVS_ROLE_ICM
  // BLE name & passkey (6 digits)
  #define NVS_KEY_BLENM  "BLENAM"
  #define NVS_DEF_BLENM  "ICM"
  #define NVS_KEY_BLEPK  "BLEPK_"
  #define NVS_DEF_BLEPK  123456

  // Web/BLE pairing PIN (6 digits)
  #define NVS_KEY_PIN___ "PIN___"
  #define NVS_DEF_PIN___ 123456

  // Wi-Fi (AP + STA)
  #define NVS_KEY_APSID  "APSSID"
  #define NVS_DEF_APSID  "ICM_AP"
  #define NVS_KEY_APKEY  "APKEY_"
  #define NVS_DEF_APKEY  "12345678"
  #define NVS_KEY_STSID  "STSSID"
  #define NVS_DEF_STSID  "NONENONE"
  #define NVS_KEY_STKEY  "STKEY_"
  #define NVS_DEF_STKEY  "NONENONE"

  // Topology JSON + slave MAC registry
  #define NVS_KEY_TOPO__ "TOPO__"
  #define NVS_DEF_TOPO__ NVS_DEF_JSON_OBJ
  #define NVS_KEY_SLMACS "SLMACS"
  #define NVS_DEF_SLMACS NVS_DEF_JSON_ARR

  // ICM registry formats (exactly 6 when formatted)
  // Sensor i: "S%02uMC", "S%02uTK"
  // Relay  i: "R%02uMC", "R%02uTK"
  #define NVS_REG_S_MAC_FMT "S%02uMC"
  #define NVS_REG_S_TOK_FMT "S%02uTK"
  #define NVS_REG_R_MAC_FMT "R%02uMC"
  #define NVS_REG_R_TOK_FMT "R%02uTK"

  // Single PMS registry entries
  #define NVS_REG_P_MAC_KEY "P01MAC"
  #define NVS_REG_P_TOK_KEY "P01TOK"
#endif

#endif // NVS_CONFIG_H
