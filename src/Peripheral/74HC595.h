/**************************************************************
 *  Project     : EasyDriveway
 *  File        : 74HC595.h
 *  Purpose     : 74HC595 shift-register driver (self-contained per role)
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-06
 *  Version     : 1.0.0
 **************************************************************/
#ifndef SR_74HC595_H
#define SR_74HC595_H

#include <Arduino.h>
#include "NVS/NVSConfig.h"
#include "LogFS.h"

#if defined(NVS_ROLE_REMU)
  #include "Hardware/Hardware_REMU.h"
#elif defined(NVS_ROLE_RELAY)
  #include "Hardware/Hardware_REL.h"
#elif defined(NVS_ROLE_PMS)
  #include "Hardware/Hardware_PMS.h"
#endif

/**
 * @class SR74HC595
 * @brief Multi-chip 74HC595 driver with shadow register and role-aware auto-init.
 * @details
 *  - Uses bit-banged SER/SRCLK/RCLK (+OE/MR optional).
 *  - When available, auto-reads SR_* pins from the active role's hardware header.
 *  - Maintains a shadow bitmap and supports LSB-first cascading across chips.
 *  - Provides a logical→physical map so higher layers can “assign relay to output.”
 */
class SR74HC595 {
public:
  /**
   * @brief Construct driver.
   * @param log Optional LogFS for diagnostics.
   */
  explicit SR74HC595(LogFS* log = nullptr);

  /**
   * @brief Initialize from explicit pins.
   * @param ser Data in (SER)
   * @param sck Shift clock (SRCLK)
   * @param rck Latch clock (RCLK)
   * @param oe  Output enable (active LOW, -1 if hard-tied LOW)
   * @param mr  Master reset (active LOW, -1 if hard-tied HIGH)
   * @param chips Number of cascaded 74HC595 chips (1..4 typical)
   * @return true on success
   */
  bool begin(int ser,int sck,int rck,int oe=-1,int mr=-1,uint8_t chips=1);

  /**
   * @brief Initialize using role hardware defaults if available.
   * @param chips If 0, uses sensible defaults (REMU → 2 chips for 16 relays).
   * @return true if pins were found and initialized
   */
  bool beginAuto(uint8_t chips=0);

  /**
   * @brief Enable (true) or Hi-Z (false) the outputs (OE is active-LOW).
   * @param enable True to drive outputs; false to tri-state.
   */
  void setEnabled(bool enable);

  /**
   * @brief Clear (shadow=0) and latch to outputs.
   */
  void clear();

  /**
   * @brief Write one logical bit and latch.
   * @param logicalIndex Logical index (0..bitCount-1), passed through the mapping table.
   * @param on True=1, False=0
   */
  void writeLogical(uint16_t logicalIndex,bool on);

  /**
   * @brief Assign a logical channel to a physical output index (Q pin).
   * @param logicalIndex 0..bitCount-1 (logical)
   * @param physicalIndex 0..bitCount-1 (physical bit position across chips)
   * @return true if assigned
   */
  bool assignLogicalToPhysical(uint16_t logicalIndex,uint16_t physicalIndex);

  /**
   * @brief Reset the logical→physical mapping to identity.
   */
  void resetMapping();

  /**
   * @brief Overwrite all logical outputs with a mask (LSB→logical 0).
   * @param mask 32-bit mask; extra bits ignored
   */
  void writeMask(uint32_t mask);

  /**
   * @brief Total bits available.
   */
  uint16_t bitCount() const { return (uint16_t)_chips*8u; }

  /**
   * @brief Current shadow mask (logical).
   */
  uint32_t shadow() const { return _shadow; }

  /**
   * @brief Whether driver has valid pins and is initialized.
   */
  bool ok() const { return _ok; }

private:
  void pulse(int pin);
  void latch();
  void shiftOutPhysical(uint32_t physicalMask);
  uint16_t mapToPhysical(uint16_t logical) const;

private:
  LogFS*   _log = nullptr;
  int      _pinSER=-1,_pinSCK=-1,_pinRCK=-1,_pinOE=-1,_pinMR=-1;
  uint8_t  _chips=0;
  uint32_t _shadow=0;          // logical shadow
  bool     _enabled=true;
  bool     _ok=false;
  uint16_t _map[32];           // logical→physical (up to 4 chips)
};

#endif /* SR_74HC595_H */
