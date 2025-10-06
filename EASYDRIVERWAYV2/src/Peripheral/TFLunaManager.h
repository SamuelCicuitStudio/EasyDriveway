/**************************************************************
 *  Project     : EasyDriveway
 *  File        : TFLunaManager.h
 *  Purpose     : TF-Luna manager for SENS (single pair) and SEMU (8 pairs via TCA9548A)
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef TFLUNAMANAGER_H
#define TFLUNAMANAGER_H

#include <Arduino.h>
#include <TFLI2C.h>
#include "NVS/NVSConfig.h"
#include "NVS/NVSManager.h"
#include "I2CBusHub.h"
#ifdef ESP_PLATFORM
  #include "esp_mac.h"
#endif
#if defined(NVS_ROLE_SENS)
  #include "Config/Config_SENS.h"
  #include "Hardware/Hardware_SENS.h"
#endif
#if defined(NVS_ROLE_SEMU)
  #include "Config/Config_SEMU.h"
  #include "Hardware/Hardware_SEMU.h"
  #include "TCA9548A.h"
#endif

#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
/**
 * @class TFLunaManager
 * @brief Manages TF-Luna sensors in two roles:
 *        - SENS: single A/B sensors on one bus (no mux), global NVS keys.
 *        - SEMU: up to 8 A/B pairs through TCA9548A mux, per-pair prefixed keys.
 */
class TFLunaManager {
public:
  /**
   * @struct Sample
   * @brief One TF-Luna reading.
   */
  struct Sample {
    uint16_t dist_mm;        ///< Distance in millimeters
    uint16_t amp;            ///< Signal strength (flux)
    int16_t  temp_c_x100;    ///< Temperature in °C x 100
    bool     ok;             ///< True if read succeeded
  };

  /**
   * @brief Construct the manager with an NVS configuration manager.
   * @param cfg Pointer to NvsManager for loading/saving parameters.
   */
  explicit TFLunaManager(NvsManager* cfg) : cfg_(cfg) {}

  /**
   * @brief Initialize using an I2CBusHub owner (no Wire.begin() here).
   * @param hub        I2C hub providing SYS/ENV buses.
   * @param fps_hz     Desired frame rate (clamped 1..250). Default 100.
   * @param continuous True for continuous mode, false for trigger mode.
   * @param useSYSbus  True to use SYS bus, false for ENV bus.
   * @param muxAddr    SEMU only: TCA9548A address (ignored in SENS).
   * @return true on success.
   */
  bool begin(I2CBusHub* hub,
             uint16_t fps_hz = 100,
             bool continuous = true,
             bool useSYSbus = true,
             uint8_t muxAddr = 0x70);

  /**
   * @brief Initialize using a provided TwoWire instance (owner handles bring-up).
   * @param wire    Pre-initialized TwoWire pointer.
   * @param fps_hz  Desired frame rate (clamped 1..250). Default 100.
   * @param continuous True for continuous, false for trigger.
   * @param muxAddr SEMU only: TCA9548A address (ignored in SENS).
   * @return true on success.
   */
  bool begin(TwoWire* wire,
             uint16_t fps_hz = 100,
             bool continuous = true,
             uint8_t muxAddr = 0x70);

  /**
   * @brief Enable or disable both sensors in the active context.
   * @param en True to enable; false to disable.
   * @return true if both operations succeeded.
   */
  bool setEnable(bool en);

  /**
   * @brief Change and persist I2C addresses for A/B in the active context.
   * @param addrA New address for sensor A.
   * @param addrB New address for sensor B.
   * @return true on success.
   */
  bool setAddresses(uint8_t addrA, uint8_t addrB);

  /**
   * @brief Set frame rate for both sensors and persist it.
   * @param fps_hz Desired frame rate (clamped 1..250).
   * @return true on success.
   */
  bool setFrameRate(uint16_t fps_hz);

  /**
   * @brief Read one sample from sensor A.
   * @param s Output sample.
   * @return true if read succeeded.
   */
  bool readA(Sample& s) { return readOne(addrA_, s); }

  /**
   * @brief Read one sample from sensor B.
   * @param s Output sample.
   * @return true if read succeeded.
   */
  bool readB(Sample& s) { return readOne(addrB_, s); }

  /**
   * @brief Read both sensors and report the effective frame rate.
   * @param a           Output sample from A.
   * @param b           Output sample from B.
   * @param rate_hz_out Effective FPS (averaged if both available).
   * @return true if both reads succeeded.
   */
  bool readBoth(Sample& a, Sample& b, uint16_t& rate_hz_out);

  /**
   * @brief Read a single sensor by address.
   * @param addr I2C address to read.
   * @param s    Output sample.
   * @return true if read succeeded.
   */
  bool readOne(uint8_t addr, Sample& s);

  /**
   * @brief Presence helper for sensor A using current near/far thresholds.
   * @param s Sample from A.
   * @return true if within [near_mm_, far_mm_].
   */
  bool isPresentA(const Sample& s) const;

  /**
   * @brief Presence helper for sensor B using current near/far thresholds.
   * @param s Sample from B.
   * @return true if within [near_mm_, far_mm_].
   */
  bool isPresentB(const Sample& s) const;

  /**
   * @brief Adapter for higher-level fetch APIs.
   * @param which       Unused selector (kept for interface compatibility).
   * @param A           Output sample A.
   * @param B           Output sample B.
   * @param rate_hz_out Effective FPS.
   * @return true if both reads succeeded.
   */
  bool fetch(uint8_t which, Sample& A, Sample& B, uint16_t& rate_hz_out);

#if defined(NVS_ROLE_SEMU)
  /**
   * @brief Select active pair [0..7] on the mux and load its config.
   * @param pairIndex Pair index.
   * @return true on success.
   */
  bool selectPair(uint8_t pairIndex);

  /**
   * @brief Get current active pair index.
   * @return Pair [0..7].
   */
  uint8_t currentPair() const { return curPair_; }
#endif

  /**
   * @brief Get current A sensor address in the active context.
   * @return Address of A.
   */
  uint8_t addrA() const { return addrA_; }

  /**
   * @brief Get current B sensor address in the active context.
   * @return Address of B.
   */
  uint8_t addrB() const { return addrB_; }

private:
  /**
   * @brief Internal bring-up on the chosen TwoWire without calling begin().
   * @param fps_hz     Desired frame rate (clamped 1..250).
   * @param continuous True for continuous, false for trigger.
   * @param muxAddr    SEMU only: TCA9548A address (ignored in SENS).
   * @return true on success.
   */
  bool beginWithWire_(uint16_t fps_hz, bool continuous, uint8_t muxAddr);

#if defined(NVS_ROLE_SENS)
  /**
   * @brief Load global SENS configuration (addresses, near/far).
   */
  void loadConfig_();

  /**
   * @brief Persist global SENS addresses (A/B).
   */
  void saveAddresses_();
#endif

#if defined(NVS_ROLE_SEMU)
  /**
   * @brief Load SEMU per-pair configuration for idx (near/far, A/B).
   * @param idx Pair index [0..7].
   */
  void loadConfigPair_(uint8_t idx);

  /**
   * @brief Persist SEMU per-pair addresses for current idx.
   * @param idx Pair index [0..7].
   */
  void saveAddressesPair_(uint8_t idx);

  /**
   * @brief Ensure mux is initialized and switched to idx.
   * @param idx Pair index [0..7].
   * @return true if selected.
   */
  bool ensureMuxOnPair_(uint8_t idx);
#endif

private:
  NvsManager* cfg_ = nullptr;
  I2CBusHub*  hub_ = nullptr;   ///< Not owned
  TwoWire*    wire_ = nullptr;  ///< Not owned
  TFLI2C      tfl_;
  uint8_t  addrA_   = TFL_ADDR_A;
  uint8_t  addrB_   = TFL_ADDR_B;
  uint16_t near_mm_ = TF_NEAR_MM_DEFAULT;
  uint16_t far_mm_  = TF_FAR_MM_DEFAULT;
#if defined(NVS_ROLE_SEMU)
  TCA9548A mux_;
  bool     muxInited_ = false;
  uint8_t  muxAddr_   = 0x70;
  uint8_t  curPair_   = 0;
#endif
};
#endif // NVS_ROLE_SENS || NVS_ROLE_SEMU

#endif // TFLUNAMANAGER_H
