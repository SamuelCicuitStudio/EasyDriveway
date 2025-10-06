/**************************************************************
 *  Project     : EasyDriveway
 *  File        : VEML7700Manager.h
 *  Purpose     : Unified wrapper for the SparkFun VEML7700 ALS sensor.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef VEML7700MANAGER_H
#define VEML7700MANAGER_H

#include <Arduino.h>
#include "SparkFun_VEML7700_Arduino_Library.h"
#include "NVS/NVSManager.h"
#include "I2CBusHub.h"

class I2CBusHub;

#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
/**
 * @class VEML7700Manager
 * @brief Minimal manager for VEML7700 ambient light sensor with hub/wire injection and NVS thresholds.
 * @details Uses caller- or hub-provided I²C without pin reads, supports optional day/night hysteresis via NVS.
 */
class VEML7700Manager {
public:
  /**
   * @brief Construct with optional NVS config, hub, and TwoWire.
   * @param cfg  Pointer to NvsManager configuration (can be nullptr).
   * @param hub  Optional I2CBusHub used to acquire ENV bus.
   * @param wire Optional TwoWire; if null, ENV bus from hub/static hub is used.
   */
  explicit VEML7700Manager(NvsManager* cfg, I2CBusHub* bus = nullptr, TwoWire* wire = nullptr) : _cfg(cfg), _hub(hub), _wire(wire) {}

  /**
   * @brief Set/override the active TwoWire.
   * @param wire TwoWire pointer (must remain valid while in use).
   */
  void setWire(TwoWire* wire) { _wire = wire; }

  /**
   * @brief Set/override the hub used to fetch the ENV bus.
   * @param hub I2CBusHub pointer.
   */
  void setHub(I2CBusHub* bus) { _bus = bus; }

  /**
   * @brief Initialize the sensor on the given address.
   * @param addr I²C address (default VEML7700_I2C_ADDR).
   * @return true on success, false on probe/init failure.
   */
  bool begin(uint8_t addr = VEML7700_I2C_ADDR);

  /**
   * @brief Read current illuminance into a reference.
   * @param lux Output lux value.
   * @return true if read succeeded.
   */
  bool read(float& lux);

  /**
   * @brief Get the last lux value read (may be NAN if never read).
   * @return Last lux.
   */
  float lux() const { return _lastLux; }

  /**
   * @brief Indicate if the device initialized properly.
   * @return true if initialized.
   */
  bool isHealthy() const { return _initialized; }

  /**
   * @brief Millis timestamp of the last successful read.
   * @return Milliseconds since boot at last read.
   */
  uint32_t lastReadMs() const { return _lastReadMs; }

  /**
   * @brief Compute day/night state with hysteresis using configured thresholds.
   * @param luxNow Current lux reading.
   * @return 1 for day, 0 for night.
   */
  uint8_t computeDayNight(float luxNow);

  /**
   * @brief Access the underlying SparkFun driver instance.
   * @return Reference to VEML7700 object.
   */
  VEML7700& raw() { return _als; }

private:
  /**
   * @brief Load ALS thresholds from NVS (keeps defaults if keys missing).
   */
  void loadThresholds();

private:
  NvsManager* _cfg  = nullptr;   ///< Optional NVS config
  I2CBusHub*  _bus  = nullptr;   ///< Optional hub (ENV bus)
  TwoWire*    _wire = nullptr;   ///< Active I²C wire
  VEML7700    _als;              ///< SparkFun driver

  bool     _initialized = false; ///< Init state
  uint8_t  _i2cAddr     = VEML7700_I2C_ADDR; ///< I²C address
  uint32_t _i2cHz       = 400000UL;          ///< Not used here but preserved

  int      _alsT0 = 50;   ///< Night threshold (go night below)
  int      _alsT1 = 100;  ///< Day threshold (go day above)
  uint8_t  _isDay = 1;    ///< Current day(1)/night(0) state

  float    _lastLux    = NAN; ///< Last lux value
  uint32_t _lastReadMs = 0;   ///< Last read timestamp
};
#endif

#endif // VEML7700MANAGER_H
