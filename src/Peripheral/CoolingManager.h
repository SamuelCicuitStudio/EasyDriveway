/**************************************************************
 *  Project     : EasyDriveway
 *  File        : CoolingManager.h
 *  Purpose     : Role-aware cooling controller using DS18B20U or BME280 inputs and PWM fan output.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef COOLINGMANAGER_H
#define COOLINGMANAGER_H

#include <Arduino.h>
#include "NVS/NVSManager.h"
#include "LogFS.h"
#include "esp_mac.h"
#include "BME280Manager.h"
#include "DS18B20U.h"

// ==== RTOS task config ====
#ifndef COOLING_TASK_CORE
#  define COOLING_TASK_CORE      0
#endif
#ifndef COOLING_TASK_PRIORITY
#  define COOLING_TASK_PRIORITY  3
#endif
#ifndef COOLING_TASK_STACK
#  define COOLING_TASK_STACK     4096
#endif
#ifndef COOLING_TASK_PERIOD_MS
#  define COOLING_TASK_PERIOD_MS 10000   // 10s
#endif

// ==== PWM config (ESP32 LEDC) ====
#ifndef COOLING_LEDC_CHANNEL
#  define COOLING_LEDC_CHANNEL   2
#endif
#ifndef COOLING_LEDC_FREQUENCY
#  define COOLING_LEDC_FREQUENCY 25000   // 25 kHz
#endif
#ifndef COOLING_LEDC_RES_BITS
#  define COOLING_LEDC_RES_BITS  10      // 0..1023
#endif

// ==== Thermostat thresholds & behavior ====
#ifndef COOL_TEMP_ECO_ON_C
#  define COOL_TEMP_ECO_ON_C     40.0f
#endif
#ifndef COOL_TEMP_NORM_ON_C
#  define COOL_TEMP_NORM_ON_C    55.0f
#endif
#ifndef COOL_TEMP_FORCE_ON_C
#  define COOL_TEMP_FORCE_ON_C   70.0f
#endif
#ifndef COOL_TEMP_HYST_C
#  define COOL_TEMP_HYST_C       3.0f
#endif

// ==== Default preset speeds ====
#ifndef COOL_SPEED_ECO_PCT
#  define COOL_SPEED_ECO_PCT     30
#endif
#ifndef COOL_SPEED_NORM_PCT
#  define COOL_SPEED_NORM_PCT    60
#endif
#ifndef COOL_SPEED_FORCE_PCT
#  define COOL_SPEED_FORCE_PCT   100
#endif
#ifndef COOL_SPEED_STOP_PCT
#  define COOL_SPEED_STOP_PCT    0
#endif

// ==== Logging throttles ====
#ifndef COOL_LOG_DELTA_C
#  define COOL_LOG_DELTA_C       0.5f
#endif
#ifndef COOL_LOG_MIN_PERIODS
#  define COOL_LOG_MIN_PERIODS   6
#endif

class BME280Manager;
class DS18B20U;

/**
 * @class CoolingManager
 * @brief PWM fan controller with hysteresis; supports DS18B20U (non-SENS/SEMU) or BME280 (SENS/SEMU).
 * @details Creates an RTOS task to periodically read temperature and adjust fan speed or mode.
 */
class CoolingManager {
public:
  /**
   * @brief Operating modes for the cooling system.
   */
  enum Mode : uint8_t { COOL_STOPPED=0, COOL_ECO, COOL_NORMAL, COOL_FORCED, COOL_AUTO };

  /**
   * @brief Construct with dependency injection for sensors and logging.
   * @param cfg  Pointer to NvsManager (required).
   * @param ds18 Pointer to DS18B20U (used when NOT SENS/SEMU).
   * @param bme  Pointer to BME280Manager (used when SENS/SEMU).
   * @param log  Optional LogFS pointer.
   */
  CoolingManager(NvsManager* cfg, DS18B20U* ds18=nullptr, BME280Manager* bme=nullptr, LogFS* log=nullptr)
  : _cfg(cfg), _ds(ds18), _bme(bme), _log(log) {}

  /**
   * @brief Initialize PWM, verify sensor presence, start RTOS task, and log snapshot.
   * @return true on success, false if prerequisites fail.
   */
  bool begin();

  /**
   * @brief Stop task, set fan to 0%, and mark mode as stopped.
   */
  void end();

  /**
   * @brief Request a new cooling mode (applied on the next update).
   * @param m Target mode.
   */
  void setMode(Mode m);

  /**
   * @brief Manually set fan speed percentage and force NORMAL mode.
   * @param pct 0–100% PWM.
   */
  void setManualSpeedPct(uint8_t pct);

  /**
   * @brief Immediately stop the fan and set requested mode to STOPPED.
   */
  void stopFan();

  /**
   * @brief Set temperature thresholds and hysteresis.
   * @param ecoOn   Enter ECO at/above this °C.
   * @param normOn  Enter NORMAL at/above this °C.
   * @param forceOn Enter FORCED at/above this °C.
   * @param hyst    Hysteresis (°C), default COOL_TEMP_HYST_C.
   */
  void setThresholds(float ecoOn, float normOn, float forceOn, float hyst = COOL_TEMP_HYST_C);

  /**
   * @brief Set preset PWM percentages for ECO/NORMAL/FORCED.
   * @param ecoPct   ECO speed (0–100).
   * @param normPct  NORMAL speed (0–100).
   * @param forcePct FORCED speed (0–100).
   */
  void setPresetSpeeds(uint8_t ecoPct, uint8_t normPct, uint8_t forcePct);

  /**
   * @brief Get last measured temperature.
   * @return Last temperature in °C (NAN if unknown).
   */
  float lastTempC() const { return _lastTempC; }

  /**
   * @brief Get last relative humidity (BME builds only).
   * @return %RH value (NAN if unavailable).
   */
  float lastHumidityRH() const { return _lastRH; }

  /**
   * @brief Get last pressure (BME builds only).
   * @return Pressure in Pascals (NAN if unavailable).
   */
  float lastPressurePa() const { return _lastP; }

  /**
   * @brief Get last applied fan speed percentage.
   * @return 0–100%.
   */
  uint8_t lastSpeedPct() const { return _lastSpeedPct; }

  /**
   * @brief Get last applied mode.
   * @return Mode currently enforced.
   */
  Mode modeApplied() const { return _modeApplied; }

  /**
   * @brief Get last requested mode.
   * @return Mode requested by user/code.
   */
  Mode modeRequested() const { return _modeUser; }

  /**
   * @brief Attach a logger after construction.
   * @param log LogFS pointer.
   */
  void attachLogger(LogFS* log) { _log = log; }

  /**
   * @brief Attach a BME280 manager after construction (SENS/SEMU).
   * @param bme Pointer to BME280Manager.
   */
  void attachBME(BME280Manager* bme) { _bme = bme; }

  /**
   * @brief Attach a DS18B20U after construction (non-SENS/SEMU).
   * @param ds Pointer to DS18B20U.
   */
  void attachDS18(DS18B20U* ds) { _ds = ds; }

private:
  /**
   * @brief RTOS task entry thunk.
   * @param arg `this` pointer.
   */
  static void taskThunk(void* arg);

  /**
   * @brief RTOS task loop calling periodicUpdate() on schedule.
   */
  void taskLoop();

  /**
   * @brief Configure PWM backend (LEDC on ESP32) and set 0% duty.
   */
  void setupPWM();

  /**
   * @brief Write fan speed as percentage (0–100).
   * @param pct PWM percent.
   */
  void writeFanPercent(uint8_t pct);

  /**
   * @brief Convert percent to LEDC duty (0..(2^RES-1)).
   * @param pct PWM percent.
   * @return Duty value.
   */
  uint32_t pctToDuty(uint8_t pct) const { return (uint32_t)((_dutyMax * (uint32_t)pct) / 100U); }

  /**
   * @brief Prepare / verify the sensor for the active role.
   * @return true if sensor is ready.
   */
  bool setupSensor();

  /**
   * @brief Read temperature for the active role.
   * @param tC Output temperature in °C.
   * @return true on success.
   */
  bool readSensor(float& tC);

  /**
   * @brief Periodic control step: read, apply mode, and log.
   */
  void periodicUpdate();

  /**
   * @brief Apply a requested mode (non-AUTO) immediately.
   * @param m  Requested mode.
   * @param tC Current/last temperature.
   */
  void applyModeCommand(Mode m, float tC);

  /**
   * @brief AUTO control with hysteresis between ECO/NORMAL/FORCED.
   * @param tC Current temperature.
   */
  void applyAutoLogic(float tC);

  /**
   * @brief Log initialization snapshot.
   */
  void logInit();

  /**
   * @brief Throttled temperature log with optional humidity/pressure.
   * @param tC Temperature to log.
   */
  void logTempIfNeeded(float tC);

  /**
   * @brief Log a mode change with temperature and speed.
   * @param from Previous mode.
   * @param to   New mode.
   * @param tC   Temperature.
   * @param pct  Speed percentage.
   */
  void logModeChange(Mode from, Mode to, float tC, uint8_t pct);

  /**
   * @brief Log a sensor fault string.
   * @param what Fault description.
   */
  void logSensorFault(const char* what);

private:
  NvsManager*     _cfg  = nullptr;     ///< NVS config (required)
  DS18B20U*       _ds   = nullptr;     ///< DS18B20U when NOT SENS/SEMU
  BME280Manager*  _bme  = nullptr;     ///< BME280 when SENS/SEMU
  LogFS*          _log  = nullptr;     ///< Logger (optional)

  // Pins / HW
  int _pinFanPwm = FAN_PWM_PIN;

  // Task
  TaskHandle_t _task = nullptr;

  // Thresholds / presets
  float   _ecoOnC   = COOL_TEMP_ECO_ON_C;
  float   _normOnC  = COOL_TEMP_NORM_ON_C;
  float   _forceOnC = COOL_TEMP_FORCE_ON_C;
  float   _hystC    = COOL_TEMP_HYST_C;
  uint8_t _ecoPct   = COOL_SPEED_ECO_PCT;
  uint8_t _normPct  = COOL_SPEED_NORM_PCT;
  uint8_t _forcePct = COOL_SPEED_FORCE_PCT;

  // State
  Mode    _modeUser     = COOL_AUTO;
  Mode    _modeApplied  = COOL_STOPPED;
  float   _lastTempC    = NAN;
  float   _lastRH       = NAN;
  float   _lastP        = NAN;
  float   _lastLoggedTempC = NAN;
  uint8_t _lastSpeedPct = 0;
  uint32_t _dutyMax     = (1u << COOLING_LEDC_RES_BITS) - 1u;

  // Log throttling
  uint32_t _periodCounter = 0;
};

#endif // COOLINGMANAGER_H
