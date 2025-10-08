/**************************************************************
 *  Project     : EasyDriveway
 *  File        : BME280Manager.h
 *  Purpose     : Thin wrapper around Adafruit_BME280 using hub/caller I²C.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef BME280MANAGER_H
#define BME280MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "NVS/NVSManager.h"
#include "I2CBusHub.h"

class I2CBusHub;

/**
 * @class BME280Manager
 * @brief Minimal manager for the Adafruit_BME280 sensor.
 * @details Uses injected TwoWire or acquires ENV bus from I2CBusHub (instance or static).
 *          Role-guarded to SENS/SEMU builds.
 */
#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
class BME280Manager {
public:
  /**
   * @brief Construct with optional NVS config, hub, and TwoWire.
   * @param cfg  Pointer to NvsManager (can be nullptr).
   * @param hub  Optional I2CBusHub to fetch ENV bus from.
   * @param wire Optional TwoWire; if null, ENV bus is used.
   */
  explicit BME280Manager(NvsManager* cfg, I2CBusHub* hub = nullptr, TwoWire* wire = nullptr) : cfg_(cfg), hub_(hub), wire_(wire) {}

  /**
   * @brief Set/override the active TwoWire.
   * @param w TwoWire pointer to use.
   */
  void setWire(TwoWire* w) { wire_ = w; }

  /**
   * @brief Set/override the hub used to fetch the ENV bus.
   * @param h I2CBusHub pointer.
   */
  void setHub(I2CBusHub* h) { hub_ = h; }

  /**
   * @brief Initialize the BME280 sensor on the given address.
   * @param addr I²C address (default BME280_I2C_ADDR).
   * @return true on success, false otherwise.
   */
  bool begin(uint8_t addr = BME280_I2C_ADDR);

  /**
   * @brief Read temperature (°C), humidity (%RH), and pressure (Pa).
   * @param tC   Output temperature in Celsius.
   * @param rh   Output relative humidity in %.
   * @param p_Pa Output pressure in Pascals.
   * @return true if read succeeded and values are finite.
   */
  bool read(float& tC, float& rh, float& p_Pa);

  /**
   * @brief Get last temperature in °C.
   * @return Last temperature value.
   */
  float temperatureC() const { return last_tC_; }

  /**
   * @brief Get last humidity in %RH.
   * @return Last humidity value.
   */
  float humidityRH() const { return last_rh_; }

  /**
   * @brief Get last pressure in Pascals.
   * @return Last pressure value.
   */
  float pressurePa() const { return last_pPa_; }

  /**
   * @brief Health flag indicating begin() succeeded.
   * @return true if initialized.
   */
  bool isHealthy() const { return initialized_; }

  /**
   * @brief Millis timestamp of last successful read.
   * @return Milliseconds since boot.
   */
  uint32_t lastReadMs() const { return last_read_ms_; }

  /**
   * @brief Access the underlying Adafruit_BME280 instance.
   * @return Reference to the driver.
   */
  Adafruit_BME280& raw() { return bme_; }

private:
  NvsManager*     cfg_ = nullptr;              ///< Optional NVS manager
  I2CBusHub*      hub_ = nullptr;              ///< Optional ENV bus provider
  TwoWire*        wire_ = nullptr;             ///< Active I²C wire
  Adafruit_BME280 bme_;                        ///< Sensor driver
  bool            initialized_ = false;        ///< Init state
  uint8_t         i2c_addr_ = BME280_I2C_ADDR; ///< I²C address
  float           last_tC_ = NAN;              ///< Last temperature
  float           last_rh_ = NAN;              ///< Last humidity
  float           last_pPa_ = NAN;             ///< Last pressure
  uint32_t        last_read_ms_ = 0;           ///< Last read timestamp
};
#endif

#endif // BME280MANAGER_H
