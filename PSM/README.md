# EasyDriveWay – Power Supply Module (PSM)

**PSM** is the power node of the EasyDriveWay system. It is **fully commanded by the Interface Control Module (ICM)** over **ESP-NOW**; PSM does not expose a standalone UI and is expected to follow ICM’s orders (power on/off, status, telemetry). PSM also logs locally to SD, monitors temperatures and fan, and reports detailed power metrics and fault bits back to ICM. &#x20;

---

## High-level architecture

* **ESP-NOW slave/agent**: `PSMEspNowManager` handles radio setup, token check, message receive/send, and composes replies for power/temperature/status per the shared command protocol. It provides **instance methods** for measurements (`measure48V_mV/mA`, `measureBat_mV/mA`), fault reads, and helpers to switch 48 V/5 V rails by delegating to `PowerManager`.&#x20;
* **Power control & metering**: `PowerManager` controls rail enable GPIOs, reads mains sense and ADCs for **VBUS/VBAT** and two **ACS781** hall sensors (**IBUS** and **IBAT**), and computes a **fault bitfield** (OVP/UVP/OCP/OTP/Brownout). Scaling and thresholds are configurable via NVS keys.&#x20;
* **Cooling & board temperature**: `CoolingManager` runs a DS18B20 probe + PWM fan with auto/forced modes and exposes the latest °C reading (used by PSM for OTP and telemetry).&#x20;
* **Logging**: `ICMLogFS` initializes SD from config pins and writes structured logs per domain (POWER/ESPNOW/etc.). Useful for field diagnostics.&#x20;
* **RTC**: `RTCManager` keeps the same external API but now uses the **ESP32 internal RTC** only; all I²C RTC specifics are stubbed to keep dependencies working.&#x20;
* **UX peripherals**: optional **RGB LED** status helper and **buzzer** event beeps; both are config-driven. &#x20;

---

## Command & Telemetry protocol (shared with ICM)

All messages are defined in the shared **`CommandAPI.h`**. Domains include `SYS`, `POWER`, `RELAY`, `SENS`, `TOPO`, `SEQ`. PSM primarily handles:

* `SYS_INIT/SYS_ACK/SYS_HB/SYS_CAPS/SYS_SET_CH` for bring-up and housekeeping.
* `PWR_GET/PWR_SET/PWR_REQSDN/PWR_CLRF/PWR_GET_TEMP` for power control and telemetry.
  The common header `IcmMsgHdr` carries version, domain/opcode, flags, timestamp, counter, and the 16-byte token used for authentication.&#x20;

**Power status payload** returned by PSM (`PWR_GET` and async status) contains:
`on`, `fault` bits, `vbus_mV`, `ibus_mA`, `vbat_mV`, `ibat_mA`, and optional `tC_x100`.&#x20;

---

## Configuration keys & hardware mapping

All keys live in **`Config.h`** (≤6 chars to keep NVS compact). Key groups:

* **Identity & networking**: device id, Wi-Fi AP/STA, BLE, ESP-NOW channel & mode, and PSM-side mirrors `PCH/PTOK/PMAC` for channel, token, master MAC.&#x20;
* **Pins**: SD (`SD*`), I²C (`I2CSCL/I2CSDA`), cooling & RGB, temperature sensor, rail enables, mains sense, voltage ADCs (`V48AD/VBATAD`), **current ADCs** (`I48AD` for IBUS and **`IBATAD`** for battery current).&#x20;
* **Scaling & thresholds** (used by `PowerManager`): `V48SN/V48SD`, `VBTSN/VBTSD` scale ADC mV to real volts; thresholds include `VBOVP/VBUVP/BIOCP` and `VBOVB/VBUVB/BAOCP`, plus `OTPC`.&#x20;
* **ACS781 calibration**: legacy generic `ACS_*` keys are provided, plus **per-sensor overrides** for bus (`AB*`) and battery (`BA*`) if you want different sensitivity/zero/atten/avg/invert.&#x20;

> See `ConfigManager::initializeVariables()` in your app to seed defaults into NVS at first run (mirrors the keys above). *(Your provided initializer already populates the pin map, scaling, thresholds, and both ACS blocks.)*&#x20;

---

## Module details

### PSMEspNowManager (radio/control surface)

* Ctor wires `ConfigManager`, `ICMLogFS`, `RTCManager`, `PowerManager`, and optionally `CoolingManager`.
* Public helpers expose measurements and controls (no globals):
  `hwIs48VOn()`, `readFaultBits()`, `measure48V_mV/mA`, `measureBat_mV/mA`, `set48V()`, `set5V()`, `mainsPresent()`, and `readBoardTempC()`.
* Implements send/receive thunks, token check, channel apply, and composite replies (`sendPowerStatus`, `sendTempReply`).&#x20;

### PowerManager (rails + metering + faults)

* **Controls**: GPIOs for 48 V / 5 V rails; reads **mains** presence.
* **Voltages**: `V48AD`, `VBATAD` with per-rail scaling from NVS.
* **Currents**: two **ACS781** instances — **IBUS** on `I48AD`, **IBAT** on `IBATAD` (optional if not wired).
* **Faults**: builds an 8-bit map (OVP/UVP/OCP/OTP/Brownout) using thresholds from NVS.&#x20;

### ACS781 (current sensor helper)

* Reads calibrated ADC mV → amps using sensitivity (µV/A), zero offset, averaging, and attenuation from NVS. Supports inversion and on-device zero calibration.&#x20;

### CoolingManager (temperature + fan)

* Periodic DS18B20 sampling, PWM fan control (LEDC), modes (AUTO/ECO/NORM/FORCED), and log throttling. PSM’s temp telemetry reads from here (or an optional callback).&#x20;

### RTCManager (ESP32 internal RTC)

* Keeps previous class interface but stubs I²C RTC plumbing; all time comes from system/`time.h`. Handy for timestamped logs and message headers.&#x20;

### ICMLogFS (SD logging)

* Loads SD pins from config, creates/rotates per-domain logs, streams files over UART, and exposes a small command server.&#x20;

### RGBLed / BuzzerManager (optional UX)

* **RGBLed**: config-driven pins, simple patterns, and direct RGB writes for system feedback.
* **Buzzer**: event-based beep sequences; NVS toggle `BZFEED` to enable/disable. &#x20;

---

## Build & dependencies

This is an **ESP32 Arduino** project. From headers you’ll need (typical via PlatformIO or Arduino Library Manager):

* Core: `WiFi.h`, `esp_now.h`, `esp_wifi.h`
* FS/SD: `FS.h`, `SD.h`, `SPI.h`
* Sensors: `OneWire`, `DallasTemperature` (for DS18B20)
* Time: `RTClib` (for `DateTime` types only; actual time from `time.h`)
  All are referenced in the module headers above.  &#x20;

---

## Bring-up sequence (suggested)

```cpp
// Create managers (inject ConfigManager everywhere)
RTCManager       rtc(&cfg);
ICMLogFS         log(Serial, &cfg);
CoolingManager   cool(&cfg, &log);
PowerManager     power(&cfg);
PSMEspNowManager pnow(&cfg, &log, &rtc, &power, &cool);

// Init order
rtc.begin();                     // internal RTC
log.beginFromConfig();           // mount SD
cool.begin();                    // temp + fan PWM
power.begin();                   // pins, scales, thresholds, ACS781
pnow.begin(/*channelDefault*/1); // ESP-NOW slave; token will come from ICM
```

PSM will then accept ICM opcodes and return telemetry using the `PowerStatusPayload` structure. &#x20;

---

## Configuration quick reference

* **Rails & sense pins**: `P48EN`, `P5VEN`, `MSNS`, `V48AD`, `VBATAD`, `I48AD`, `IBATAD`. Defaults are defined; override in NVS if your board differs.&#x20;
* **Scaling**: `V48SN/V48SD` and `VBTSN/VBTSD` (numerator/denominator).&#x20;
* **Fault thresholds**: `VBOVP/VBUVP/BIOCP` (bus) and `VBOVB/VBUVB/BAOCP` (battery), plus `OTPC`.&#x20;
* **ACS781**: common `ACS_*` keys, or per-sensor `AB*` (bus) / `BA*` (battery).&#x20;
* **ESP-NOW**: `ESCHNL`, `ESMODE`, and PSM mirrors `PCH/PTOK/PMAC`.&#x20;

---

## File map

* `PSMEspNowManager.h/.cpp` – ESP-NOW control/telemetry surface toward ICM.&#x20;
* `PowerManager.h/.cpp` – rails, ADC & ACS781 readings, fault bits.&#x20;
* `ACS781.h/.cpp` – hall current sensor helper.&#x20;
* `CoolingManager.h/.cpp` – DS18B20 + PWM fan + auto logic.&#x20;
* `RTCManager.h/.cpp` – internal ESP32 RTC wrapper (compat API).&#x20;
* `ICMLogFS.h/.cpp` – SD logging system.&#x20;
* `RGBLed.h/.cpp`, `BuzzerManager.h/.cpp` – optional UX. &#x20;
* `CommandAPI.h` – shared message header, opcodes, and payloads.&#x20;
* `Config.h` – all keys/defaults for pins, scaling, thresholds, ESP-NOW.&#x20;

---

## Notes

* PSM is **ICM-driven**: treat PSM as a radio-attached peripheral. It should not change rails autonomously except for local safety (e.g., OTP/OCP). Command→reply flows and payloads are strictly those in `CommandAPI.h`.&#x20;
* Temperature is sourced from `CoolingManager` (or a temp callback) and included in power status when available.&#x20;

