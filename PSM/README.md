# EasyDriveWay – Power Supply Module (PSM)

**PSM** is the power node of the EasyDriveWay system. It is **fully commanded by the Interface Control Module (ICM)** over **ESP-NOW**; PSM does not expose a standalone UI and is expected to follow ICM’s orders (power on/off, status, telemetry). PSM logs locally to SD, monitors temperature and fan, and reports detailed power metrics and fault bits back to ICM.

---

## High-level architecture

* **ESP-NOW slave/agent** — `PSMEspNowManager` handles radio setup, token check, command parsing, and status/telemetry replies. It delegates rail control and measurements to `PowerManager`.
* **Power control & metering** — `PowerManager` controls rail enables (48 V, 5 V), **charger enable**, reads **mains sense**, measures **VBUS/VBAT** and **IBUS/IBAT** (two ACS781 sensors), and computes a **fault bitfield** (OVP/UVP/OCP/OTP/Brownout).
* **Cooling & temperature** — `CoolingManager` reads a DS18B20 and drives a PWM fan. Its temperature feeds OTP checks and telemetry.
* **Logging** — `ICMLogFS` initializes SD and writes structured logs (POWER / ESPNOW / etc.).
* **RTC** — `RTCManager` uses ESP32’s internal RTC for timestamps.
* **UX peripherals** — optional **RGB LED** and **Buzzer** for quick visual/audible state cues.

---

## Power-path topology (48 V system)

**Goal:** Seamless priority OR-ing between **Charger 48 V** and **Battery 48 V**, protected low-voltage generation, and two controlled relays (load on/off, battery charge connect).

```
          ┌──────────── 48 V INPUTS ────────────┐
          │                                     │
          │  Charger 48 V In      Battery 48 V In
          │   (wall PSU)              (pack)
          │   Fuse + TVS               Fuse + TVS
          │       │                        │
          │     IN1                       IN2
          │      │                         │
          └──────┴───────── LTC4355 (Ideal-Diode OR) ───────────┐
                           Q1‖Q2       Q3‖Q4                    │
                                            OUT (48 V BUS)      │
                                                                │
                              (A) Low-voltage branch            │
                              ----------------------            │
                              LM74800  →  LM5161  →  5 V BUS ───┼───┐
                           (RPP/Inrush)   (48→5 buck)           │   │
                                                                │   │
                                                          USB 5 V  ─┘ (OR into 5 V BUS via ideal-diode/power-mux)
                                                                │
                                                        TPSM863252 (5→3.3 buck)
                                                                │
                                                           3.3 V BUS (ESP32/logic)

                              (B) 48 V distribution
                              ---------------------
                         Direct 48 V → ICM
                         48 V → Relay #1 (LOAD ON/OFF) → Other 48 V Loads

                              (C) Charge path
                              --------------
   Charger 48 V + ── Relay #2 (CHARGE, CHEN) ──► Battery +
   (PSU priority via LTC4355; relay simply connects charger to pack for charge)
```

**Control & sense (ESP32):**

* **GPIO21** → Relay #1 (LOAD ON/OFF coil driver)
* **GPIO10 (CHEN)** → Relay #2 (CHARGE coil driver)  ← *new Charger Enable*
* **GPIO22** → 5 V enable (if used by board)
* **GPIO23** → Mains presence (digital or via divider)
* **ADC**: VBUS(48 V), VBAT via divider + 3.6 V clamp; IBUS/IBAT via ACS781

**Notes:**

* **LTC4355** performs ideal-diode OR-ing of Charger and Battery; the higher source supplies the **48 V BUS**.
* **LM74800** adds reverse-polarity protection/inrush control to the low-voltage branch before **LM5161** (48→5 V).
* **USB 5 V** is **OR-ed** with the local 5 V via ideal-diode/power-mux (avoid hard paralleling).
* **Relays**: use MOSFET low-side drivers with proper flyback/TVS; coils sized for 48 V.

---

## Command & telemetry (shared with ICM)

All messages are defined in **`CommandAPI.h`** (domains `SYS`, `POWER`, `RELAY`, `SENS`, `TOPO`, `SEQ`).

Key flows handled by PSM:

* `SYS_INIT / SYS_ACK / SYS_HB / SYS_CAPS / SYS_SET_CH`
* `PWR_GET / PWR_SET / PWR_REQSDN / PWR_CLRF / PWR_GET_TEMP`

**Power status payload** includes: rail on/off, **fault bits**, `vbus_mV`, `ibus_mA`, `vbat_mV`, `ibat_mA`, optional `tC_x100`.
(*You can extend it with a `chg_en` boolean if the ICM wants explicit charger-enable state.*)

---

## Configuration keys & hardware mapping

All keys live in **`Config.h`** (≤6 chars). Highlights:

* **Pins**
  SD: `SD*` • I²C: `I2CSCL/I2CSDA` • Cooling/RGB pins
  Temperature: DS18B20 pin/pullup
  Rails & sense: `P48EN` (48 V enable), `P5VEN` (5 V enable), `MSNS` (mains sense),
  ADCs: `V48AD` (VBUS), `VBATAD` (VBAT), `I48AD` (IBUS), `IBATAD` (IBAT),
  **Charger enable:** `CHEN` (**CHARGER\_EN\_PIN\_KEY**) **← new**

* **Scaling & thresholds** (used by `PowerManager`)
  Voltage scale: `V48SN/V48SD`, `VBTSN/VBTSD`
  Fault limits: `VBOVP/VBUVP/BIOCP` (bus), `VBOVB/VBUVB/BAOCP` (battery), and `OTPC`.

* **ESP-NOW**
  `ESCHNL`/`ESMODE`, plus PSM mirrors `PCH/PTOK/PMAC` (channel, token, master MAC).

> Seed defaults in `ConfigManager::initializeVariables()` at first boot. Ensure you add:
> `PutInt(CHARGER_EN_PIN_KEY, CHARGER_EN_PIN_DEFAULT);`

---

## Module details

### PSMEspNowManager

* Injects `ConfigManager`, `ICMLogFS`, `RTCManager`, `PowerManager`, `CoolingManager`.
* Exposes helpers: `hwIs48VOn()`, `readFaultBits()`, `measure48V_mV/mA`, `measureBat_mV/mA`, `set48V()`, `set5V()`, `mainsPresent()`, `readBoardTempC()`.

### PowerManager

* **Controls**: `set48V()`, `set5V()`, **`setChargeEnable()`** (new)
* **State**: `is48VOn()`, `mainsPresent()`, **`isChargeEnabled()`** (new), `readFaultBits()`
* **Measurements**: VBUS/VBAT (dividers), IBUS/IBAT (ACS781)
* **Faults**: 8-bit map (OVP/UVP/OCP/OTP/Brownout)

### CoolingManager / RTCManager / ICMLogFS / RGBLed / Buzzer

* DS18B20 + PWM fan • ESP32 RTC timestamps • SD logs • status LED/blips

---

## Build & dependencies

ESP32 Arduino (PlatformIO or Arduino IDE). Typical libs:

* Core: `WiFi.h`, `esp_now.h`, `esp_wifi.h`
* FS/SD: `FS.h`, `SD.h`, `SPI.h`
* Sensors: `OneWire`, `DallasTemperature`
* Time: standard `time.h` (RTC wrapper only provides API compatibility)

---

## Bring-up

```cpp
RTCManager       rtc(&cfg);
ICMLogFS         log(Serial, &cfg);
CoolingManager   cool(&cfg, &log);
PowerManager     power(&cfg);
PSMEspNowManager pnow(&cfg, &log, &rtc, &power, &cool);

rtc.begin();
log.beginFromConfig();
cool.begin();
power.begin();                // sets up pins, loads scales/thresholds
pnow.begin(cfg.GetInt(ESPNOW_CH_KEY, 1));
```

Example **charger policy** (enable only when mains is present):

```cpp
bool onMains = power.mainsPresent();
power.setChargeEnable(onMains);
```

---

## Configuration quick reference

* **Pins**: `P48EN`, `P5VEN`, `MSNS`, `V48AD`, `VBATAD`, `I48AD`, `IBATAD`, **`CHEN`**
* **Scaling**: `V48SN/V48SD` and `VBTSN/VBTSD`
* **Fault thresholds**: `VBOVP/VBUVP/BIOCP` (bus), `VBOVB/VBUVB/BAOCP` (battery), `OTPC`
* **ESP-NOW**: `ESCHNL`, `ESMODE` (+ mirrors `PCH/PTOK/PMAC`)

---

## File map

* `PSMEspNowManager.h/.cpp` — ESP-NOW control/telemetry toward ICM
* `PowerManager.h/.cpp` — rails, ADC & ACS781 readings, faults, **charger enable**
* `ACS781.h/.cpp` — hall current sensor helper
* `CoolingManager.h/.cpp` — DS18B20 + PWM fan
* `RTCManager.h/.cpp` — internal RTC wrapper
* `ICMLogFS.h/.cpp` — SD logging
* `RGBLed.h/.cpp`, `BuzzerManager.h/.cpp` — optional UX
* `CommandAPI.h` — shared message structures/opcodes
* `Config.h` — all keys/defaults (pins, scale, thresholds, ESP-NOW)

---

## Notes

* PSM is **ICM-driven**: it should not autonomously toggle rails except for safety (OTP/OCP/UVP).
* The **Charger Enable** (`CHEN`) controls **Relay #2** to connect the charger to the battery for charging; source selection between charger and battery feeding the BUS is handled by the **LTC4355**.
* Protect the 48 V rail with **TVS + fuses**, use proper **ADC dividers + clamps**, and drive relays with MOSFETs and **flyback/TVS** snubbers.

---
