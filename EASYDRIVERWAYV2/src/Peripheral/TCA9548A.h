/**************************************************************
 *  Project     : EasyDriveway
 *  File        : TCA9548A.h
 *  Purpose     : Lightweight driver for TCA9548A I²C mux.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef TCA9548A_H
#define TCA9548A_H

#include <Arduino.h>
#include <Wire.h>
#include "I2CBusHub.h"

class I2CBusHub;

/**
 * @class TCA9548A
 * @brief Minimal TCA9548A I²C multiplexer helper with optional I2CBusHub integration.
 * @details Supports injection of an existing TwoWire instance, legacy pin-based begin(),
 *          or automatic acquisition of the system bus from I2CBusHub. Channel operations
 *          are mask-based and preserve the original, lightweight behavior.
 */
class TCA9548A {
public:
  /**
   * @brief Default constructor.
   */
  TCA9548A() = default;

  /**
   * @brief Construct with optional hub and wire injection.
   * @param hub  Optional I2CBusHub pointer used to acquire the SYS bus.
   * @param wire Optional TwoWire pointer used directly when provided.
   */
  explicit TCA9548A(I2CBusHub* hub, TwoWire* wire = nullptr) : _hub(hub), _wire(wire) {}

  /**
   * @brief Initialize using an existing TwoWire.
   * @param wire  Reference to an initialized TwoWire bus.
   * @param addr  I²C address of the TCA9548A (default 0x70).
   * @param probe If true, perform a read to confirm presence.
   * @return true on success.
   */
  bool begin(TwoWire& wire, uint8_t addr = 0x70, bool probe = true);

  /**
   * @brief Legacy initializer that configures Wire on given pins/frequency.
   * @param sda   SDA pin.
   * @param scl   SCL pin.
   * @param freq  I²C clock frequency (Hz).
   * @param addr  I²C address (default 0x70).
   * @param probe If true, perform a read to confirm presence.
   * @return true on success.
   */
  bool begin(int sda, int scl, uint32_t freq = 400000UL, uint8_t addr = 0x70, bool probe = true);

  /**
   * @brief Initialize using hub SYS bus if available, otherwise static hub SYS bus.
   * @param addr  I²C address (default 0x70).
   * @param probe If true, perform a read to confirm presence.
   * @return true on success.
   */
  bool begin(uint8_t addr = 0x70, bool probe = true);

  /**
   * @brief Select a single channel by index.
   * @param chn Channel number [0..7].
   * @return true on success.
   */
  bool select(uint8_t chn);

  /**
   * @brief Write a raw 8-bit channel mask (bit n enables channel n).
   * @param mask Channel bitmask.
   * @return true on success.
   */
  bool writeMask(uint8_t mask);

  /**
   * @brief Read back the current channel mask from the device.
   * @param outMask Output reference for the mask.
   * @return true on success.
   */
  bool readMask(uint8_t& outMask) const;

  /**
   * @brief Disable all channels (write mask 0x00).
   * @return true on success.
   */
  bool disableAll();

  /**
   * @brief Set the I2CBusHub used to acquire the SYS TwoWire.
   * @param hub I2CBusHub pointer.
   */
  void setHub(I2CBusHub* hub) { _hub = hub; }

  /**
   * @brief Set/override the internal TwoWire pointer.
   * @param w TwoWire pointer.
   */
  void setWire(TwoWire* w) { _wire = w; }

  /**
   * @brief Get the configured device I²C address.
   * @return 7-bit I²C address.
   */
  inline uint8_t address() const { return _addr; }

  /**
   * @brief Check if the instance has an active TwoWire assigned.
   * @return true if initialized.
   */
  inline bool initialized() const { return _wire != nullptr; }

  /**
   * @brief Retrieve the last mask successfully written (cache).
   * @return Last written mask.
   */
  inline uint8_t lastMask() const { return _lastMask; }

private:
  /**
   * @brief Low-level single-byte write to the control register.
   * @param val Byte value to write.
   * @return true on success.
   */
  bool _writeByte(uint8_t val) const;

  /**
   * @brief Low-level single-byte read helper.
   * @param val Output reference for the read byte.
   * @return true on success.
   */
  bool _readByte(uint8_t& val) const;

private:
  I2CBusHub* _hub   = nullptr;  ///< Optional hub used to fetch SYS bus.
  TwoWire*   _wire  = nullptr;  ///< Active I²C interface.
  uint8_t    _addr  = 0x70;     ///< Device address.
  uint8_t    _lastMask = 0x00;  ///< Cached last written mask.
};

#endif // TCA9548A_H
