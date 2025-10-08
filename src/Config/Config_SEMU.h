/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Config_SEMU.h
 *  Purpose     : Sensor Emulator — NVS keys (6-char) + defaults.
 *                SEMU emulates many SENS: 8 TF-Luna pairs via mux.
 *                - 1 global pairing state (device-level)
 *                - 1 global lux sensor (ALS thresholds)
 *                - 8x per-pair near/far thresholds (mm)
 *                - 8x per-pair A↔B spacing (mm)
 *                - Per-pair TF-Luna I2C addresses + FPS
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Created     : 2025-10-06
 *  Version     : 1.1.0
 **************************************************************/
#ifndef CONFIG_SEMU_H
#define CONFIG_SEMU_H

#include "Config_Common.h"

// Module log mapping (from common)
#ifndef LOGFILE_PATH
  #define LOGFILE_PATH            LOGFILE_PATH_SEMU
#endif
#ifndef LOG_FILE_PATH_PREFIX
  #define LOG_FILE_PATH_PREFIX    LOG_FILE_PREFIX_SEMU
#endif

/* ============================================================
 * Device-level (global) keys
 * ==========================================================*/

/** @section Pairing state (GLOBAL for SEMU) */
#define SEMU_PAIRING_KEY            "SEMPRG"   // u8  (1=in pairing)
#define SEMU_PAIRED_KEY             "SEMPRD"   // u8  (1=paired)
#define SEMU_PAIRING_DEF            1
#define SEMU_PAIRED_DEF             0

/** @section Day/Night hysteresis (lux) — one ALS shared by all pairs */
#define ALS_T0_LUX_KEY              "ALS_T0"   // u16 (enter-night threshold)
#define ALS_T1_LUX_KEY              "ALS_T1"   // u16 (exit-night threshold)
#define ALS_T0_LUX_DEFAULT          180
#define ALS_T1_LUX_DEFAULT          300

/** @section Emit ENV model for each virtual sensor (0/1) */
#define VENV_EN_KEY                 "VENVEN"   // u8
#define VENV_EN_DEF                 1

/* ============================================================
 * Per-virtual (per TF-Luna pair) keys — use prefix + index 0..7
 * Final keys look like "TFNMM_0", "TFFMM_0", "ABSPM_0", etc.
 * ==========================================================*/

/** @section Direction & gating thresholds (mm) — per pair */
#define TF_NEAR_MM_KEY_PFX          "TFNMM_"   // u16, near distance
#define TF_FAR_MM_KEY_PFX           "TFFMM_"   // u16, far distance
#define TF_NEAR_MM_DEFAULT          200
#define TF_FAR_MM_DEFAULT           3200

/** @section A↔B spacing (mm) — per pair */
#define AB_SPACING_MM_KEY_PFX       "ABSPM_"   // u16 (spacing per pair)
#define AB_SPACING_MM_DEFAULT       350

/** @section Relay pulse defaults (ms) — per virtual sensor */
#define VON_MS_KEY                  "VONMS_"   // u16 ON pulse ms
#define VON_MS_DEF                  600

/** @section Virtual lead behavior — per virtual sensor */
#define VLEAD_CT_KEY                "VLDCT_"   // u8  lead count
#define VLEAD_MS_KEY                "VLDMS_"   // u16 lead step ms
#define VLEAD_CT_DEF                3
#define VLEAD_MS_DEF                250

/* ============================================================
 * TF-Luna addressing + rate (per pair) — use prefix + index 0..7
 * ==========================================================*/

/** @section TF-Luna I2C address prefixes (append 0..7) */
#define TFL_A_ADDR_KEY_PFX          "TFAAD_"   // u8, TF-Luna A addr
#define TFL_B_ADDR_KEY_PFX          "TFBAD_"   // u8, TF-Luna B addr

/** @section TF-Luna default I2C addresses (per pair) */
#define TFL_ADDR_A_DEF              0x10
#define TFL_ADDR_B_DEF              0x11

/** @section Optional: per-pair FPS (append 0..7) */
#define TFL_FPS_KEY_PFX             "TFFPS_"   // u16 frames/s
#define TFL_FPS_DEF                 100

/* ============================================================
 * Notes:
 *  - SEMU hardware uses 8 pairs (see Hardware_SEMU.h -> TFL_PAIR_COUNT).
 *  - Keys ending with '_' are 6-char prefixes. Append index [0..7].
 *    Example:
 *      idx = 3
 *      "TFNMM_3" -> near threshold mm for pair 3
 *      "TFFMM_3" -> far threshold mm for pair 3
 *      "ABSPM_3" -> spacing mm for pair 3
 *      "TFAAD_3" -> TF-Luna A I2C addr for pair 3
 *      "TFBAD_3" -> TF-Luna B I2C addr for pair 3
 *      "TFFPS_3" -> FPS for pair 3
 *  - Global ALS thresholds (ALS_T0/ALS_T1) are shared by all pairs.
 *  - Global pairing keys (SEMPRG/SEMPRD) control the whole emulator.
 * ==========================================================*/

#endif /* CONFIG_SEMU_H */
