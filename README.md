---

# ICM Controller Firmware

**Interface Control Module (ICM)** — a cohesive ESP32 firmware that orchestrates Wi-Fi (STA/AP), ESP-NOW mesh control, BLE command/telemetry, SD-card logging, RTC timekeeping/alarms, RGB status, cooling, and configurable NVS-backed settings.

---

## Quick links

* [Overview](#overview)
* [System architecture](#system-architecture)
* [Features at a glance](#features-at-a-glance)
* [Module guide](#module-guide)

  * [ESPNowManager](#espnowmanager)
  * [WiFiManager (STA/AP + Web)](#wifimanager-staap--web)
  * [BLE control (BleICM)](#ble-control-bleicm)
  * [Logging (ICMLogFS)](#logging-icmlogfs)
  * [Configuration (ConfigManager)](#configuration-configmanager)
  * [RGB status LED](#rgb-status-led)
  * [RTC & time (RTCManager)](#rtc--time-rtcmanager)
  * [Sleep & wake (SleepTimer)](#sleep--wake-sleeptimer)
* [Configuration & NVS keys](#configuration--nvs-keys)
* [Boot & runtime flow](#boot--runtime-flow)
* [REST / Web UI pointers](#rest--web-ui-pointers)
* [Build & flash](#build--flash)
* [Examples](#examples)
* [Troubleshooting](#troubleshooting)
* [Roadmap](#roadmap)

---

## Overview

ICM supervises a **driveway / lighting chain** (entrance sensor → relays → parking sensor) plus other peripherals. Topology and peers are configurable via JSON, and sequences (UP/DOWN) can be started/stopped programmatically. The ESP-NOW manager exposes callbacks and JSON snapshots so your Web UI can visualize peers/topology and drive actions. &#x20;

---

## System architecture

```
+--------------------+       +--------------------+       +--------------------+
|  WiFiManager (AP/  |<----->|  Web UI / REST     |<----->|  ConfigManager     |
|  STA + CORS + FS)  |       |  (routes/JSON)     |       |  (NVS keys)        |
+---------^----------+       +---------^----------+       +---------^----------+
          |                              |                            |
          v                              |                            |
   +------+--------+                     |                            |
   |  ESPNowManager|<--------------------+----------------------------+
   |  (peers/topo, |                    JSON peers/topology/export    |
   |  sequences)   |<------+
   +-------^-------+       |   +-----------------+     +---------------------+
           |               +---|  BleICM (GATT)  |<--->|  ICMLogFS (SD Logs) |
           |                   |  Status/WiFi/   |     |  events, FS browse  |
           |                   |  Peers/Topo...  |     +---------------------+
           |                   +-----------------+            ^
           |                                                 |
           v                                                 |
   +-----------------+       +-------------------+           |
   | RTCManager      |       | SleepTimer        |-----------+
   | (DS3231, sync)  |       | (inactivity/alarm)|
   +-----------------+       +-------------------+
                 \                /
                  \              /
                   +------------+
                   | RGBLed     |
                   +------------+
```

*Key references:* Wi-Fi AP bring-up & CORS headers, Web UI integration, JSON peers/topology export, and logging/event APIs are all provided by the headers listed in this repo.   &#x20;

---

## Features at a glance

* **Wi-Fi AP/STA** with simple web endpoints and permissive CORS for UI development.&#x20;
* **ESP-NOW** peers/topology management + sequence control (UP/DOWN), JSON snapshots for UI. &#x20;
* **BLE GATT (BleICM)** service exposing status, Wi-Fi control, peers, topology, power, export/import, and an “Old App” back-compat channel.&#x20;
* **SD logging** with per-domain events, rotation/purge, and UART file streaming utilities. &#x20;
* **RTC (DS3231)** timekeeping, 32 kHz output helper, and system/RTC sync helpers. &#x20;
* **Inactivity sleep** scheduling and alarm-based wake control. &#x20;
* **Config-driven RGB LED** patterns and GPIO pins loaded from NVS (with defaults). &#x20;

---

## Module guide

### ESPNowManager

* **Topology:** define links via JSON and apply at once (`configureTopology(json["links"])`).&#x20;
* **Sequences:** `sequenceStart(UP/DOWN)` and `sequenceStop()`; a helper `startSequence(anchor, up)`. &#x20;
* **UI integration:** JSON snapshots for peers/topology/export to power a web UI.&#x20;

### WiFiManager (STA/AP + Web)

* **AP bring-up** with `softAPConfig()` + `softAP(ssid,pass)`; IP defaults are in `Config.h`. &#x20;
* **CORS** defaults allow easy local development.&#x20;
* **Lifecycle:** start, disable AP, register routes (your handlers). &#x20;

### BLE control (BleICM)

* **GATT characteristics** (Status, Wi-Fi, Peers, Topology, Sequences, Power, Export, OldApp).&#x20;
* **Callbacks in-header**: security/server/characteristic callbacks follow the CarLock style.&#x20;
* **Singleton & helpers**: `BleICM::instance`, `notifyStatus()`, advert LED task utilities. &#x20;

### Logging (ICMLogFS)

* **Domains & severities** for structured events; SD pins pulled from config. &#x20;
* **APIs:** `event()/eventf()`, new log rotation, FS browsing, UART streaming.  &#x20;

### Configuration (ConfigManager)

* **NVS helpers**: `Put*` / `Get*` for bool/int/float/string/uint64.&#x20;
* **System helpers**: restart/power-down simulation; countdown delays.&#x20;

### RGB status LED

* **Config-driven pins** + patterns (`startRainbow`, `startBlink`, `stop`), and direct RGB/hex control. &#x20;
* Internally loads pins from NVS and initializes PWM/outputs.&#x20;

### RTC & time (RTCManager)

* **I2C/DS3231 init** from config pins; time set/get and system<->RTC sync. &#x20;
* **Status utilities**: lost-power check, temperature read, 32 kHz output control.&#x20;

### Sleep & wake (SleepTimer)

* **Inactivity timer** with periodic check task; configurable timeout. &#x20;
* **Alarms**: schedule sleep until epoch/DateTime; DS3231 Alarm1 helpers. &#x20;

---

## Configuration & NVS keys

* **Rule:** All stored keys are **≤ 6 characters** (kept project-wide for NVS). Example Wi-Fi/ESP-NOW keys: `ESCHNL`, `ESMODE`.&#x20;
* **Network defaults:** AP IP/net and NTP settings reside in `Config.h`.&#x20;
* Many modules read pins, names, and modes from `ConfigManager` at start. See each header’s *begin* docs for specifics.  &#x20;

---

## Boot & runtime flow

1. **WiFiManager** mounts FS, sets CORS, starts **AP** (and registers routes).  &#x20;
2. Web UI (or BLE) pushes JSON **peers/topology**, which ESP-NOW applies.&#x20;
3. Optional: start a **sequence** (UP/DOWN) to drive the chain.&#x20;
4. **ICMLogFS** captures events per domain; logs are rotatable and streamable.&#x20;
5. **RTCManager/SleepTimer** maintain time and manage inactivity sleeps. &#x20;

---

## REST / Web UI pointers

The AP comes up with permissive CORS so you can build a browser UI locally without hassles; add REST endpoints in `WiFiManager` when registering routes. &#x20;

For UI data sources, use ESP-NOW JSON snapshots (peers/topology/export) exposed by the manager.&#x20;

---

## Build & flash

* **Target:** ESP32 (Arduino core).
* **Libraries used** (as seen in headers): `WiFi`, `ArduinoJson`, `RTClib`, BLE (`BLEDevice` et al.), SD/SPI, FreeRTOS tasks.    &#x20;
* **Storage:** SPIFFS for small JSON/config files; SD for log archives via ICMLogFS. &#x20;

---

## Examples

**Start a UP sequence** after configuring topology:

```cpp
espn.sequenceStart(ESPNowManager::SeqDir::UP);
// ... later ...
espn.sequenceStop();
```



**Build UI snapshots** for peers & topology:

```cpp
String peers   = espn.serializePeers();
String topo    = espn.serializeTopology();
String exportC = espn.serializeExport();
```



---

## Troubleshooting

* **AP didn’t start?** Check SPIFFS mount and AP bring-up logs in `ICMLogFS`. &#x20;
* **No UI data?** Ensure peers/topology are configured (JSON “links”) before sequences. &#x20;
* **Time drift?** Use RTC sync helpers to align system time and DS3231.&#x20;

---

## Roadmap

* BLE-driven OTA control endpoints (log + progress).
* Web UI for SD log browsing/streaming using ICMLogFS UART server.&#x20;

---

> *Tip:* Keep new NVS keys **≤ 6 chars** to match the current on-device convention and avoid namespace collisions in Preferences. See `Config.h` for examples.&#x20;

---
