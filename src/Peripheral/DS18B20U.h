/**************************************************************
 *  Project     : EasyDriveway
 *  Module      : Sensors
 *  File        : DS18B20U.h
 *  Brief       : DS18B20/DS18B20U temperature sensor (OneWire) helper.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/

#ifndef DS18B20U_H
#define DS18B20U_H

#include <Arduino.h>
#include <OneWire.h>
#include "NVS/NVSManager.h"  

/**
 * @class DS18B20U
 * @brief Minimal driver that discovers the first DS18B20 on a OneWire bus and provides
 *        blocking/non-blocking temperature acquisition, optional RTOS background loop,
 *        and helpers for address formatting and last-value caching.
 *
 * @details
 * - Discovers the *first* 0x28-family device on the bus and caches its 8-byte ROM.
 * - `requestConversion()` kicks a conversion; `readTemperature()` fetches & converts raw.
 * - Optional FreeRTOS task periodically requests + reads and updates `lastCelsius()`.
 * - CRC checked against the scratchpad’s 9th byte before parsing raw temperature.
 */
class DS18B20U {
public:
  /* -------------------------- Lifecycle -------------------------- */
  /**
   * @brief Construct with NVS and OneWire handles.
   * @param cfg NVS manager pointer (kept for project-wide parity; not used for pins).
   * @param ow  OneWire bus handle used for discovery and I/O.
   */
  explicit DS18B20U(NvsManager* cfg, OneWire* ow)
  : _cfg(cfg), _ow(ow) {}

  /**
   * @brief Probe the bus and cache the *first* DS18B20 address (family 0x28).
   * @return true if a sensor was found and its ROM address stored; false otherwise.
   * @post On success, `isReady()` returns true and `address()` becomes valid.
   */
  bool begin();

  /* ------------------------- Acquisition ------------------------- */
  /**
   * @brief Issue a CONVERT T command to the cached sensor (non-blocking).
   * @return true if the command was sent; false if no bus/device selected.
   * @note Typical max conversion time is ~750 ms at 12-bit resolution.
   */
  bool requestConversion();

  /**
   * @brief Read temperature (°C) from the cached sensor’s scratchpad.
   * @param tC Out parameter receiving the temperature in degrees Celsius.
   * @return true on success (CRC OK and parse done); false otherwise.
   * @post On success, also updates the `lastCelsius()` cache.
   */
  bool readTemperature(float& tC);

  /* --------------------------- Helpers --------------------------- */
  /**
   * @brief Whether a DS18B20 has been discovered and cached.
   * @return true if `begin()` succeeded; false otherwise.
   */
  bool isReady() const { return _hasSensor; }

  /**
   * @brief Last successfully read temperature (°C).
   * @return Cached value or NaN if never read successfully.
   */
  float lastCelsius() const { return _lastC; }

  /**
   * @brief Return the ROM code as "28:xx:..:..:..:..:..:crc".
   * @return Human-readable address string, or "NO-SENSOR" if not ready.
   */
  String addressString() const;

  /**
   * @brief Get the 8-byte ROM address pointer (valid if `isReady()`).
   * @return Pointer to internal 8-byte buffer.
   */
  const uint8_t* address() const { return _addr; }

  /* --------------------- Background conversion ------------------- */
  /**
   * @brief Start a periodic background task (request→wait→read→sleep).
   * @param intervalMs Additional sleep between reads (default 3000 ms).
   * @note Uses FreeRTOS; safe to call repeatedly (will restart the task).
   */
  void startTask(uint32_t intervalMs = 3000);

  /**
   * @brief Stop the background task if running.
   * @post No further background conversions are attempted.
   */
  void stopTask();

private:
  /* --------------------- Internal task/plumbing ------------------ */
  /**
   * @brief FreeRTOS task thunk.
   * @param self Instance pointer (this).
   * @internal Do not call directly.
   */
  static void taskThunk(void* self);

  /**
   * @brief Periodic loop: request → wait (~800 ms) → read → sleep.
   * @internal Ignores transient read errors; preserves last good value.
   */
  void taskLoop();

  /* ---------------------------- Helpers -------------------------- */
  /**
   * @brief Read the 9-byte scratchpad of the cached sensor.
   * @param sp Caller-provided 9-byte buffer.
   * @return true if CRC matches the 9th byte; false otherwise.
   */
  bool readScratchpad(uint8_t sp[9]);

  /**
   * @brief Validate CRC8 for a data buffer.
   * @param data Pointer to data.
   * @param len  Number of bytes.
   * @param crc  Expected CRC8 value.
   * @return true if CRC matches; false otherwise.
   */
  bool crcOk(const uint8_t* data, uint8_t len, uint8_t crc) const;

  /* ---------------------------- Members -------------------------- */
private:
  NvsManager*  _cfg        = nullptr;    ///< Present for parity; not used for pins
  OneWire*     _ow         = nullptr;    ///< OneWire bus handle

  bool         _hasSensor  = false;      ///< Set true after a successful begin()
  uint8_t      _addr[8]    = {0};        ///< Cached ROM code (valid if _hasSensor)
  float        _lastC      = NAN;        ///< Last good temperature (°C), NaN if none

  TaskHandle_t _task       = nullptr;    ///< Background task handle
  uint32_t     _intervalMs = 2000;       ///< Extra sleep between cycles
};

#endif /* DS18B20U_H */
