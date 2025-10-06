/**************************************************************
 *  Project : EasyDriveway
 *  File    : I2CBusHub.cpp
 **************************************************************/

#include "I2CBusHub.h"
TwoWire I2CBusHub::_twSys = TwoWire(0);
TwoWire I2CBusHub::_twEnv = TwoWire(1);
bool I2CBusHub::_didSYS = false;
bool I2CBusHub::_didENV = false;
I2CBusHub::I2CBusHub(uint32_t sysHz, uint32_t envHz, bool bringUpNow) : _sysHz(sysHz), _envHz(envHz), _autoBroughtUp(false) {}
bool I2CBusHub::bringUpSYS(uint32_t hz) { _sysHz = hz; return beginSYS(_sysHz); }
bool I2CBusHub::bringUpENV(uint32_t hz) { _envHz = hz; return beginENV(_envHz); }
TwoWire& I2CBusHub::busSYS() { if (!_didSYS) beginSYS(_sysHz ? _sysHz : I2C_SYS_HZ); return _twSys; }
TwoWire& I2CBusHub::busENV() { if (!_didENV) beginENV(_envHz ? _envHz : I2C_ENV_HZ); return _twEnv; }
bool I2CBusHub::isSYSReady() const { return _didSYS; }
bool I2CBusHub::isENVReady() const { return _didENV; }
bool I2CBusHub::beginSYS(uint32_t hz) { if (_didSYS) return true; _twSys.begin(I2C_SYS_SDA_PIN, I2C_SYS_SCL_PIN, hz); _didSYS = true; return true; }
TwoWire& I2CBusHub::sys() { if (!_didSYS) beginSYS(I2C_SYS_HZ); return _twSys; }
bool I2CBusHub::initializedSYS() { return _didSYS; }
bool I2CBusHub::beginENV(uint32_t hz) { if (_didENV) return true; _twEnv.begin(I2C_ENV_SDA_PIN, I2C_ENV_SCL_PIN, hz); _didENV = true; return true; }
TwoWire& I2CBusHub::env() { if (!_didENV) beginENV(I2C_ENV_HZ); return _twEnv; }
bool I2CBusHub::initializedENV() { return _didENV; }
