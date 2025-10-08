/**************************************************************
 *  Project : EasyDriveway
 *  File    : TCA9548A.cpp
 **************************************************************/
#include "TCA9548A.h"
#include "I2CBusHub.h"

bool TCA9548A::begin(uint8_t addr, bool probe) {
  if (!_wire) {
    if (_hub) { _hub->bringUpSYS(); _wire = &_hub->busSYS(); }
    else { I2CBusHub::beginSYS(); _wire = &I2CBusHub::sys(); }
  }
  return begin(*_wire, addr, probe);
}
bool TCA9548A::begin(TwoWire& wire, uint8_t addr, bool probe) {
  _wire = &wire; _addr = addr;
  if (probe) { uint8_t m = 0; if (!readMask(m)) { _wire = nullptr; return false; } _lastMask = m; }
  return true;
}
bool TCA9548A::begin(int sda, int scl, uint32_t freq, uint8_t addr, bool probe) {
  Wire.begin(sda, scl, freq);
  return begin(Wire, addr, probe);
}
bool TCA9548A::select(uint8_t chn) {
  if (!_wire || chn > 7) return false;
  const uint8_t mask = (uint8_t)(1U << chn);
  return writeMask(mask);
}
bool TCA9548A::writeMask(uint8_t mask) {
  if (!_wire) return false;
  if (!_writeByte(mask)) return false;
  _lastMask = mask;
  uint8_t rb = 0;
  if (readMask(rb)) { (void)rb; }
  return true;
}
bool TCA9548A::readMask(uint8_t& outMask) const {
  if (!_wire) return false;
  _wire->beginTransmission(_addr);
  const uint8_t txStatus = _wire->endTransmission(false);
  if (txStatus != 0) return false;
  const int n = _wire->requestFrom((int)_addr, 1);
  if (n != 1) return false;
  outMask = _wire->read();
  return true;
}
bool TCA9548A::disableAll() {
  return writeMask(0x00);
}
bool TCA9548A::_writeByte(uint8_t val) const {
  _wire->beginTransmission(_addr);
  _wire->write(val);
  return (_wire->endTransmission() == 0);
}
bool TCA9548A::_readByte(uint8_t& val) const {
  const int n = _wire->requestFrom((int)_addr, 1);
  if (n != 1) return false;
  val = _wire->read();
  return true;
}


