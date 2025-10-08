/**************************************************************
 *  Project     : EasyDriveway
 *  File        : NvsManager.h
 *  Purpose     : Preferences (NVS) manager: read/write helpers,
 *                defaults initialization, and system control utilities.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

/***********************
 * INCLUDES
 ***********************/
#include <time.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <NTPClient.h>
#include <esp_task_wdt.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "Utils.h"
#include "esp_system.h"
#include "NVSConfig.h"
#include "Config/Config_Common.h"

#if   defined(NVS_ROLE_ICM)
  #include "Hardware/Hardware_ICM.h"
  #include "Config/Config_ICM.h"
#elif defined(NVS_ROLE_PMS)
  #include "Hardware/Hardware_PMS.h"
  #include "Config/Config_PMS.h"
#elif defined(NVS_ROLE_SENS)
  #include "Hardware/Hardware_SENS.h"
  #include "Config/Config_SENS.h"
#elif defined(NVS_ROLE_RELAY)
  #include "Hardware/Hardware_REL.h"
  #include "Config/Config_REL.h"
#elif defined(NVS_ROLE_SEMU)
  #include "Hardware/Hardware_SEMU.h"
  #include "Config/Config_SEMU.h"
#elif defined(NVS_ROLE_REMU)
  #include "Hardware/Hardware_REMU.h"
  #include "Config/Config_REMU.h"
#else
  #error "No role matched. Define one of: NVS_ROLE_ICM / NVS_ROLE_PMS / NVS_ROLE_SENS / NVS_ROLE_RELAY / NVS_ROLE_SEMU / NVS_ROLE_REMU."
#endif

#define RESET_FLAG_KEY "RST"
#define DEFAULT_RESET_FLAG true

/**
 * @class NvsManager
 * @brief Wrapper around ESP32 Preferences with strict 6-char keys.
 *        Handles role-based default initialization and simple
 *        system control helpers (restart, sleep, blocking countdown).
 */
class NvsManager {
public:
  /**
   * @brief Construct the manager with default namespace.
   */
  NvsManager();
  /**
   * @brief Destructor; ensures preferences are closed.
   */
  ~NvsManager();

  /**
   * @brief Initialize configuration: open preferences and apply defaults if requested.
   */
  void begin();
  /**
   * @brief Close the preferences namespace.
   */
  void end();

  /**
   * @brief Store a boolean value.
   * @param key Six-char key.
   * @param value Boolean to save.
   */
  void PutBool(const char* key, bool value);
  /**
   * @brief Store a signed integer.
   * @param key Six-char key.
   * @param value Integer to save.
   */
  void PutInt(const char* key, int value);
  /**
   * @brief Store a float.
   * @param key Six-char key.
   * @param value Float to save.
   */
  void PutFloat(const char* key, float value);
  /**
   * @brief Store a string.
   * @param key Six-char key.
   * @param value String to save.
   */
  void PutString(const char* key, const String& value);
  /**
   * @brief Store an unsigned integer.
   * @param key Six-char key.
   * @param value Unsigned value to save.
   */
  void PutUInt(const char* key, int value);
  /**
   * @brief Store a 64-bit unsigned value.
   * @param key Six-char key.
   * @param value 64-bit value to save.
   */
  void PutULong64(const char* key, int value);

  /**
   * @brief Read a boolean value.
   * @param key Six-char key.
   * @param defaultValue Default if missing.
   * @return Stored or default value.
   */
  bool GetBool(const char* key, bool defaultValue);
  /**
   * @brief Read an integer value.
   * @param key Six-char key.
   * @param defaultValue Default if missing.
   * @return Stored or default value.
   */
  int GetInt(const char* key, int defaultValue);
  /**
   * @brief Read a 64-bit unsigned value.
   * @param key Six-char key.
   * @param defaultValue Default if missing.
   * @return Stored or default value.
   */
  uint64_t GetULong64(const char* key, int defaultValue);
  /**
   * @brief Read a float value.
   * @param key Six-char key.
   * @param defaultValue Default if missing.
   * @return Stored or default value.
   */
  float GetFloat(const char* key, float defaultValue);
  /**
   * @brief Read a string value.
   * @param key Six-char key.
   * @param defaultValue Default if missing.
   * @return Stored or default value.
   */
  String GetString(const char* key, const String& defaultValue);

  /**
   * @brief Remove a specific key if it exists.
   * @param key Six-char key.
   */
  void RemoveKey(const char* key);
  /**
   * @brief Clear the active namespace.
   */
  void ClearKey();
  /**
   * @brief Check if a key exists.
   * @param key Six-char key.
   * @return true if present.
   */
  bool Iskey(const char* key);

  /**
   * @brief Restart system after a delay (with progress print).
   * @param delayTime Milliseconds.
   */
  void RestartSysDelay(unsigned long delayTime);
  /**
   * @brief Simulated power-down, then restart sequence (with countdown).
   * @param delayTime Milliseconds.
   */
  void RestartSysDelayDown(unsigned long delayTime);
  /**
   * @brief Enter deep sleep briefly to simulate power-down.
   */
  void simulatePowerDown();
  /**
   * @brief Blocking countdown (prints 32 '#' segments).
   * @param delayTime Milliseconds.
   */
  void CountdownDelay(unsigned long delayTime);

  /**
   * @brief Open preferences in read-write mode.
   */
  void startPreferencesReadWrite();
  /**
   * @brief Open preferences in read-only mode.
   */
  void startPreferencesRead();

private:
  /**
   * @brief Initialize default key/values for the active role.
   */
  void initializeDefaults();
  /**
   * @brief Populate all persisted variables for the active role.
   */
  void initializeVariables();
  /**
   * @brief Read reset flag from preferences.
   * @return true if reset is requested.
   */
  bool getResetFlag();

private:
  Preferences pref;           //!< Preferences instance
  const char* namespaceName;  //!< Active namespace name
};

#endif // NVS_MANAGER_H
