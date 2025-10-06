/**************************************************************
 *  Project     : EasyDriveway
 *  File        : RelayManager.h
 *  Purpose     : Unified relay control for RELAY / PMS / REMU (74HC595 or GPIO)
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-06
 *  Version     : 1.0.0
 **************************************************************/
#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include <Arduino.h>
#include "NVS/NVSConfig.h"
#include "NVS/NvsManager.h"
#include "74HC595.h"
#include "RGBLed.h"
#include "BuzzerManager.h"
#include "LogFS.h"

#if defined(NVS_ROLE_REMU)
  #include "Hardware/HardwareE_REMU.h"   // SR_* + REL_CH_COUNT
#elif defined(NVS_ROLE_RELAY)
  #include "Hardware/Hardware_REL.h"    // RELAYx_OUT_PIN
#elif defined(NVS_ROLE_PMS)
  #include "Hardware/Hardware_PMS.h"    // REL_SRC_* pins
#endif

/**
 * @class RelayManager
 * @brief Controls relay channels for RELAY, PMS, REMU roles. Uses 74HC595 when available.
 * @details
 *  - REMU: always via 74HC595 (2 chips, 16 outputs). Supports assignment map.
 *  - RELAY/PMS: tries 74HC595 when SR_* pins exist; otherwise discrete GPIOs.
 *  - Optional feedback: RGBLed blink + Buzzer bip on state changes.
 */
class RelayManager {
public:
  /**
   * @brief Construct with dependencies.
   * @param cfg NVS manager (not owned)
   * @param log LogFS logger (nullable)
   */
  RelayManager(NvsManager* cfg, LogFS* log=nullptr);

  /**
   * @brief Initialize according to active role (auto-detect SR vs GPIO).
   * @return true on success
   */
  bool begin();

  /**
   * @brief Attach optional feedback drivers.
   * @param led RGB LED pointer (nullable)
   * @param buz Buzzer manager pointer (nullable)
   */
  void attachFeedback(RGBLed* led, BuzzerManager* buz){_led=led;_buz=buz;}

  /**
   * @brief Set LED feedback policy.
   * @param onColor  Blink color when turning ON (0xRRGGBB)
   * @param offColor Blink color when turning OFF (0xRRGGBB)
   * @param blinkMs  Duration per blink phase
   */
  void enableLedFeedback(uint32_t onColor,uint32_t offColor,uint16_t blinkMs);

  /**
   * @brief Enable/disable buzzer single bip on transitions.
   * @param on True to enable, false to disable.
   */
  void enableBuzzerFeedback(bool on){_buzOnChange=on;}

  /**
   * @brief Assign a logical relay channel to a physical output bit (SR mode).
   * @param logical 0..channels()-1
   * @param physical 0..channels()-1 (physical SR bit)
   * @return true if mapped (no-op in GPIO mode)
   */
  bool assignRelayToOutput(uint16_t logical,uint16_t physical);

  /**
   * @brief Reset mapping to identity (SR mode).
   */
  void resetMapping();

  /**
   * @brief Set one channel ON/OFF.
   * @param idx Channel index (logical).
   * @param on  True=energize, False=de-energize.
   */
  void set(uint16_t idx,bool on);

  /**
   * @brief Toggle one channel.
   * @param idx Channel index.
   */
  void toggle(uint16_t idx);

  /**
   * @brief Read shadow state of a channel.
   * @param idx Channel index.
   * @return true if ON.
   */
  bool get(uint16_t idx) const;

  /**
   * @brief Write all channels from a bitmask (LSB=ch0).
   * @param mask Bit mask.
   */
  void writeMask(uint32_t mask);

  /**
   * @brief Number of channels available under current role.
   * @return Channel count.
   */
  uint16_t channels() const { return _count; }

private:
  void applySR(uint16_t idx,bool on);
  void applyGPIO(uint16_t idx,bool on);
  void onFeedback(bool turnedOn);
  void logRelay(uint16_t idx,bool on);

private:
  NvsManager*   _cfg=nullptr;
  LogFS*        _log=nullptr;
  RGBLed*       _led=nullptr;
  BuzzerManager*_buz=nullptr;

  SR74HC595     _sr;
  bool          _useSR=false;
  uint16_t      _count=0;
  uint32_t      _shadow=0;

  bool          _ledEnabled=false;
  uint32_t      _ledOnColor=0x00FF00;
  uint32_t      _ledOffColor=0xFF0000;
  uint16_t      _ledBlinkMs=120;
  bool          _buzOnChange=true;
};

#endif /* RELAY_MANAGER_H */
