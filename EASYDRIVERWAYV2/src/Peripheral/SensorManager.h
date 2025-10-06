/**************************************************************
 *  Project     : EasyDriveway
 *  File        : SensorManager.h
 *  Purpose     : Centralized facade for TF-Luna (SENS/SEMU) and VEML7700 day/night,
 *                address/FPS setters persisted via NVS, with hub/wire-based bring-up.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include <vector>
#include "NVS/NVSManager.h"
#include "I2CBusHub.h"
#include "TFLunaManager.h"
#include "VEML7700Manager.h"

#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
/**
 * @class SensorManager
 * @brief Centralizes all sensor behavior: TF-Luna presence/direction (SENS/SEMU),
 *        VEML7700 day/night, and TF-Luna configuration helpers (address/FPS).
 */
class SensorManager {
public:
  /**
   * @enum Direction
   * @brief Direction of movement inferred from presence transitions.
   */
  enum Direction : uint8_t { DIR_NONE = 0, DIR_A_TO_B = 1, DIR_B_TO_A = 2 };

  /**
   * @struct PairReport
   * @brief Report for a single A/B pair: presence, direction, FPS, and raw samples.
   */
  struct PairReport {
    uint8_t   index;      ///< Pair index (SEMU: [0..count-1], SENS: 0)
    bool      presentA;   ///< Presence detected on sensor A
    bool      presentB;   ///< Presence detected on sensor B
    Direction direction;  ///< Direction inferred from last state
    uint16_t  rate_hz;    ///< Effective or averaged frame rate
    TFLunaManager::Sample A; ///< Raw sample from A
    TFLunaManager::Sample B; ///< Raw sample from B
  };

  /**
   * @struct Snapshot
   * @brief Snapshot containing ALS info and all pair reports.
   */
  struct Snapshot {
    float   lux = NAN;  ///< Ambient light in lux (if available)
    uint8_t isDay = 1;  ///< 1=day, 0=night
    std::vector<PairReport> pairs; ///< SENS: size=1; SEMU: size=pairCount
  };

  /**
   * @brief Construct SensorManager.
   * @param cfg Pointer to NVS manager for configuration persistence.
   */
  explicit SensorManager(NvsManager* cfg) : _cfg(cfg), _tfl(cfg), _als(cfg) {}

  /**
   * @brief Initialize using an I2CBusHub (prefers SYS for TF-Luna, ENV for ALS).
   * @param hub        Hub providing SYS/ENV wires.
   * @param useSYSforTF True to use SYS for TF-Luna; false uses ENV.
   * @param tfl_fps    Desired TF-Luna FPS (default 100).
   * @param tfl_cont   True: continuous mode; false: trigger mode.
   * @param muxAddr    SEMU: TCA9548A address (ignored for SENS).
   * @return true on success, false otherwise.
   */
  bool begin(I2CBusHub* hub, bool useSYSforTF = true, uint16_t tfl_fps = 100, bool tfl_cont = true, uint8_t muxAddr = 0x70);

  /**
   * @brief Initialize using provided TwoWire instances.
   * @param tflWire  Wire used for TF-Luna.
   * @param envWire  Wire for ALS (if null, tflWire is reused).
   * @param tfl_fps  TF-Luna FPS (default 100).
   * @param tfl_cont True: continuous; false: trigger.
   * @param muxAddr  SEMU: TCA9548A address (ignored for SENS).
   * @return true on success, false otherwise.
   */
  bool begin(TwoWire* tflWire, TwoWire* envWire, uint16_t tfl_fps = 100, bool tfl_cont = true, uint8_t muxAddr = 0x70);

  /**
   * @brief Poll all active pairs and ALS.
   * @param out Snapshot to fill (ALS + one report per pair).
   * @return true if read path executed (pairs may be skipped on errors).
   */
  bool poll(Snapshot& out);

  /**
   * @brief Poll a specific pair (SEMU) or idx=0 (SENS).
   * @param idx   Pair index.
   * @param outPr Output report.
   * @return true on success.
   */
  bool pollPair(uint8_t idx, PairReport& outPr);

  /**
   * @brief Read ALS and classify day/night.
   * @param luxOut  Output lux (may be NAN initially).
   * @param isDayOut Output 1=day, 0=night.
   * @return true (ALS optional; returns last cached if fresh read fails).
   */
  bool readALS(float& luxOut, uint8_t& isDayOut);

  /**
   * @brief Set TF-Luna I2C addresses for current context.
   * @param addrA New address for sensor A.
   * @param addrB New address for sensor B.
   * @param pairIndex SEMU pair index or -1 for current; SENS must be -1 or 0.
   * @return true on success.
   */
  bool setTFLAddresses(uint8_t addrA, uint8_t addrB, int pairIndex = -1);

  /**
   * @brief Set TF-Luna frame rate for current context.
   * @param fps Desired FPS.
   * @param pairIndex SEMU pair index or -1 for current; SENS must be -1 or 0.
   * @return true on success.
   */
  bool setTFLFrameRate(uint16_t fps, int pairIndex = -1);

  /**
   * @brief Get current TF-Luna address A after select/load.
   * @return I2C address of A.
   */
  uint8_t tflAddrA() const { return _tfl.addrA(); }

  /**
   * @brief Get current TF-Luna address B after select/load.
   * @return I2C address of B.
   */
  uint8_t tflAddrB() const { return _tfl.addrB(); }

  /**
   * @brief Number of SEMU pairs (from NVS); SENS returns 1.
   * @return Pair count.
   */
  uint8_t pairCount() const;

private:
  /**
   * @brief Infer direction based on presence edges for a pair.
   * @param idx Pair index.
   * @param nowA Current presence on A.
   * @param nowB Current presence on B.
   * @return Direction enum.
   */
  Direction inferDir(uint8_t idx, bool nowA, bool nowB);

  /**
   * @brief Read both sensors for the current pair and compute presence flags.
   * @param out Output report to fill.
   * @return true on success.
   */
  bool readCurrentPair(PairReport& out);

  /**
   * @brief Ensure SEMU pair is selected (no-op on SENS).
   * @param idx Pair index.
   * @return true if selected/valid.
   */
  bool selectPairIfNeeded(uint8_t idx);

private:
  NvsManager*       _cfg   = nullptr;   ///< NVS manager (not owned)
  I2CBusHub*        _hub   = nullptr;   ///< Hub (not owned)
  TwoWire*          _tflW  = nullptr;   ///< Wire for TF-Luna (not owned)
  TwoWire*          _envW  = nullptr;   ///< Wire for ALS (not owned)
  TFLunaManager     _tfl;               ///< TF-Luna manager
  VEML7700Manager   _als;               ///< ALS manager
  static constexpr uint8_t MAX_PAIRS = 8; ///< SEMU max pairs
  bool _lastA[MAX_PAIRS] = {false};     ///< Last presence A (per pair)
  bool _lastB[MAX_PAIRS] = {false};     ///< Last presence B (per pair)
  uint8_t _pairCount = 1;               ///< Cached SEMU count (SENS=1)
  bool _isSEMU =
  #if defined(NVS_ROLE_SEMU)
    true;
  #else
    false;
  #endif
};
#endif // defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
#endif // SENSORMANAGER_H
