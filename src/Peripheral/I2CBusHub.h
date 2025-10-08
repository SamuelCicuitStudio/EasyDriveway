/**************************************************************
 *  Project     : EasyDriveway
 *  File        : I2CBusHub.h
 *  Purpose     : Centralized I²C bus initialization and access.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef I2CBUSHUB_H
#define I2CBUSHUB_H

#include <Arduino.h>
#include <Wire.h>
#include "Hardware/Hardware_SENS.h"

/**
 * @class I2CBusHub
 * @brief Centralized manager for two logical I²C buses (SYS and ENV).
 * @details Provides both instance helpers and a legacy static API.
 *          - SYS: system/peripherals (e.g., RTC on ICM)
 *          - ENV: environmental sensors (VEML7700, BME280, etc.)
 */
class I2CBusHub {
public:
  /**
   * @brief Construct a hub with optional preferred frequencies.
   * @param sysHz Preferred SYS bus frequency (Hz), defaults to I2C_SYS_HZ.
   * @param envHz Preferred ENV bus frequency (Hz), defaults to I2C_ENV_HZ.
   * @param bringUpNow If true, caller intends to bring up later (kept for compatibility; no auto bring-up here).
   */
  explicit I2CBusHub(uint32_t sysHz = I2C_SYS_HZ, uint32_t envHz = I2C_ENV_HZ, bool bringUpNow = true);

  /**
   * @brief Bring up the SYS bus (idempotent).
   * @param hz Target frequency in Hz.
   * @return true on success.
   */
  bool bringUpSYS(uint32_t hz = I2C_SYS_HZ);

  /**
   * @brief Bring up the ENV bus (idempotent).
   * @param hz Target frequency in Hz.
   * @return true on success.
   */
  bool bringUpENV(uint32_t hz = I2C_ENV_HZ);

  /**
   * @brief Get a reference to the SYS TwoWire, ensuring it is initialized.
   * @return Reference to the SYS bus.
   */
  TwoWire& busSYS();

  /**
   * @brief Get a reference to the ENV TwoWire, ensuring it is initialized.
   * @return Reference to the ENV bus.
   */
  TwoWire& busENV();

  /**
   * @brief Check if the SYS bus has been initialized.
   * @return true if initialized.
   */
  bool isSYSReady() const;

  /**
   * @brief Check if the ENV bus has been initialized.
   * @return true if initialized.
   */
  bool isENVReady() const;

  /**
   * @brief Legacy static bring-up for the SYS bus (idempotent).
   * @param hz Target frequency in Hz.
   * @return true on success.
   */
  static bool beginSYS(uint32_t hz = I2C_SYS_HZ);

  /**
   * @brief Legacy static bring-up for the ENV bus (idempotent).
   * @param hz Target frequency in Hz.
   * @return true on success.
   */
  static bool beginENV(uint32_t hz = I2C_ENV_HZ);

  /**
   * @brief Get the SYS TwoWire singleton; brings it up if needed.
   * @return Reference to the SYS bus.
   */
  static TwoWire& sys();

  /**
   * @brief Get the ENV TwoWire singleton; brings it up if needed.
   * @return Reference to the ENV bus.
   */
  static TwoWire& env();

  /**
   * @brief Query static initialization status of the SYS bus.
   * @return true if initialized.
   */
  static bool initializedSYS();

  /**
   * @brief Query static initialization status of the ENV bus.
   * @return true if initialized.
   */
  static bool initializedENV();

private:
  // Static TwoWire instances for ESP32 peripherals (0 and 1)
  static TwoWire _twSys;  ///< I²C peripheral 0 (SYS)
  static TwoWire _twEnv;  ///< I²C peripheral 1 (ENV)
  static bool    _didSYS; ///< SYS bus initialized flag
  static bool    _didENV; ///< ENV bus initialized flag

  // Instance preferences (remember requested Hz)
  uint32_t _sysHz;        ///< Requested SYS frequency
  uint32_t _envHz;        ///< Requested ENV frequency
  bool     _autoBroughtUp;///< Compatibility flag (not used here)
};

#endif // I2CBUSHUB_H
