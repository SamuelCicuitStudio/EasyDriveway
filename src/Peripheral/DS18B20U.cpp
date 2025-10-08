/**************************************************************
 *  Project : EasyDriveway
 *  File    : DS18B20U.cpp
 **************************************************************/
#include "DS18B20U.h"
static inline void printAddrTo(String& out, const uint8_t rom[8]) {
  char buf[3];
  for (int i = 0; i < 8; ++i) {
    snprintf(buf, sizeof(buf), "%02X", rom[i]);
    out += buf;
    if (i < 7) out += ":";
  }
}
bool DS18B20U::begin() {
  if (!_ow) return false;

  _ow->reset_search();
  uint8_t rom[8]{};
  while (_ow->search(rom)) {
    // Family code 0x28 is DS18B20/DS18B20U
    if (rom[0] == 0x28) {
      memcpy(_addr, rom, 8);
      _hasSensor = true;
      return true;
    }
  }
  _hasSensor = false;
  return false;
}
bool DS18B20U::requestConversion() {
  if (!_ow || !_hasSensor) return false;
  _ow->reset();
  _ow->select(_addr);
  _ow->write(0x44);  // CONVERT T
  return true;
}
bool DS18B20U::readTemperature(float& tC) {
  if (!_ow || !_hasSensor) return false;

  uint8_t sp[9];
  if (!readScratchpad(sp)) return false;

  int16_t raw = (int16_t)((sp[1] << 8) | sp[0]);
  tC = (float)raw / 16.0f;        // 12-bit resolution
  _lastC = tC;
  return true;
}
String DS18B20U::addressString() const {
  String s;
  if (!_hasSensor) return String("NO-SENSOR");
  printAddrTo(s, _addr);
  return s;
}
void DS18B20U::startTask(uint32_t intervalMs) {
  stopTask();
  _intervalMs = intervalMs ? intervalMs : 1000;
  xTaskCreatePinnedToCore(
    DS18B20U::taskThunk,
    "DS18B20U_Task",
    2048,
    this,
    1,
    &_task,
    APP_CPU_NUM
  );
}
void DS18B20U::stopTask() {
  if (_task) {
    vTaskDelete(_task);
    _task = nullptr;
  }
}
void DS18B20U::taskThunk(void* self) {
  static_cast<DS18B20U*>(self)->taskLoop();
}
void DS18B20U::taskLoop() {
  for (;;) {
    requestConversion();
    // Typical max conversion time ~750 ms @ 12-bit; sleep a bit longer.
    vTaskDelay(pdMS_TO_TICKS(800));

    float t;
    (void)readTemperature(t);  // ignore error; keep last good value

    vTaskDelay(pdMS_TO_TICKS(_intervalMs));
  }
}
bool DS18B20U::readScratchpad(uint8_t sp[9]) {
  _ow->reset();
  _ow->select(_addr);
  _ow->write(0xBE);            // READ SCRATCHPAD
  _ow->read_bytes(sp, 9);

  // Verify CRC
  const uint8_t crc = OneWire::crc8(sp, 8);
  return (crc == sp[8]);
}
bool DS18B20U::crcOk(const uint8_t* data, uint8_t len, uint8_t crc) const {
  return OneWire::crc8(data, len) == crc;
}

