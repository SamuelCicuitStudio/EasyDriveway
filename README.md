# ICM Controller Firmware

**Interface Control Module (ICM)** — the central ESP32 firmware that coordinates EasyDriveWay lighting and other peripherals. It manages Wi-Fi (STA/AP), ESP-NOW peers/topology, BLE control, SD logging, RTC timekeeping, cooling, sleep/wake, RGB status, and a configurable buzzer with NVS-backed settings.

> This README describes the full ICM *module* firmware. It incorporates the EasyDriveWay functional model (zones, sequences, sensors, Wi-Fi/BLE app control, config persistence) and maps those requirements onto ICM components.&#x20;

---

## Quick links

* [Overview](#overview)
* [System architecture](#system-architecture)
* [Features at a glance](#features-at-a-glance)
* [Repo layout](#repo-layout)
* [Build & flash](#build--flash)
* [Configuration (NVS keys)](#configuration-nvs-keys)
* [Networking modes (STA/AP) + ESP-NOW](#networking-modes-staap--esp-now)
* [BLE control (BleICM)](#ble-control-bleicm)
* [ESP-NOW peers, topology & sequences](#esp-now-peers-topology--sequences)
* [Logging & diagnostics](#logging--diagnostics)
* [RTC & scheduling](#rtc--scheduling)
* [Cooling, sleep & power/buzzer cues](#cooling-sleep--powerbuzzer-cues)
* [Security model](#security-model)
* [Spec trace to EasyDriveWay](#spec-trace-to-easydriveway)
* [Troubleshooting](#troubleshooting)
* [Roadmap](#roadmap)
* [License](#license)

---

## Overview

ICM is the “brain” of **EasyDriveWay**: it listens to sensors (PIR/day-night via modules), drives relays in **UP/DOWN sequences** across zones, exposes control/config over **Wi-Fi (REST/web UI)** and **BLE (GATT)**, and keeps robust state in **NVS**. It’s designed as a hub with swappable expansion modules and clean JSON interfaces to front-end apps.&#x20;

---

## System architecture

```
+------------------+      +-------------------+      +---------------------+
|  WiFiManager     |<---->|  Web UI / REST    |<---->|  ConfigManager      |
|  (STA/AP, CORS)  |      |  (browser/app)    |      |  (NVS, defaults)    |
+---------^--------+      +---------^---------+      +----------^----------+
          |                          |                         |
          v                          |                         |
   +------+--------+                 |                         |
   | ESPNowManager |<----------------+------ JSON topo/peers --+
   | (peers, topo, |                        & sequences
   |  sequences)   |<----------+
   +-------^-------+           |     +------------------+      +-----------------+
           |                   +---->|  BleICM (GATT)   |<---->|  ICMLogFS (SD)  |
           |                         |  WiFi/Peers/Topo |      |  logs/events    |
           v                         +------------------+      +-----------------+
       [Sensors/Relays via I2C/expansion boards]         [RTC] [Cooling] [Sleep] [RGB] [Buzzer]
```

---

## Features at a glance

* **Wi-Fi STA/AP** with simple REST endpoints and CORS for web UI.
* **ESP-NOW** mesh: peer management, topology, and **zone sequences** (UP/DOWN).
* **BLE (BleICM)** service for phone apps: status, Wi-Fi control, peers/topology, export/import.
* **ConfigManager** with short NVS keys (≤6 chars) and sensible defaults.
* **ICMLogFS** SD logging with domains/severities and log rotation.
* **RTCManager** (DS3231) for accurate time & scheduling hooks.
* **SleepTimer** inactivity + alarm wake.
* **RGBLed** status effects; **BuzzerManager** event tones (startup, Wi-Fi up, pairing, power lost, low battery).
* **Security**: BLE pairing with static passkey, Wi-Fi creds stored in NVS, AP passworded.

---

## Repo layout

* `Config.h`, `ConfigManager.h/.cpp` — NVS keys (≤6 chars), defaults, helpers
* `WiFiManager.h/.cpp`, `WiFiAPI.h` — STA/AP control, REST routes, CORS
* `ESPNowManager.h/.cpp` — peers, topology, sequences, export/snapshots
* `BleICM.h/.cpp` — BLE GATT (status, wifi, peers, topo, seq, power, export, compat)
* `ICMLogFS.h/.cpp` — SD logging and utilities
* `RTCManager.h/.cpp` — DS3231 timekeeping & sync
* `SleepTimer.h/.cpp` — inactivity & alarm sleep
* `CoolingManager.h/.cpp` — fan/temp policies
* `RGBLed.h/.cpp` — status LED patterns
* `BuzzerManager.h/.cpp` — event patterns + NVS enable/disable

*(File names reflect the code you maintain in this repo.)*

---

## Build & flash

* **Target:** ESP32 (Arduino core).
* **Libraries:** WiFi, ArduinoJson, BLE (NimBLE/ESP32 BLE), SPIFFS/SD, RTClib, FreeRTOS.
* **Flash:** Use Arduino IDE or PlatformIO. Ensure **Partition** supports SPIFFS/SD as configured.

---

## Configuration (NVS keys)

All persisted keys are **≤6 chars** to keep Preferences tidy. Examples:

* Wi-Fi AP: `WIFNAM` (SSID), `APPASS` (pass)
* Wi-Fi STA: `STASSI`, `STAPSK`, `STAHNM`, `STADHC`
* ESP-NOW: `ESCHNL` (channel), `ESMODE` (mode)
* BLE: `BLENAM`, `BLPSWD`, `BLECON`
* Buzzer: `BZGPIO` (pin), `BZAH` (active-high), `BZFEED` (enable)

See `Config.h` for the authoritative list and defaults.

---

## Networking modes (STA/AP) + ESP-NOW

* **STA**: ICM attempts NVS creds first; on success, aligns ESP-NOW to the connected **channel**.
* **AP**: if STA fails or config requested, ICM starts AP with NVS SSID/PASS and aligns ESP-NOW to AP channel.
* **Channel alignment** is automatic after either mode changes.

---

## BLE control (BleICM)

A single service exposes these characteristics (read/write/notify):

* **Status** — snapshot `{mode, ip, ch, rssi?}`
* **WiFi** — `{"cmd":"sta_connect","ssid","password"}` / `{"cmd":"ap_start","ap_ssid","ap_password","ch"}` / `{"cmd":"scan"}`
* **Peers** — `{"cmd":"pair","mac","type"}` / `{"cmd":"remove","mac"}` / `{"cmd":"list"}`
* **Topo** — `{"cmd":"set","links":[...]}` / `{"cmd":"get"}`
* **Seq** — `{"cmd":"start", ...}` / `{"cmd":"stop"}`
* **Power** — status hooks for PSM/battery
* **Export** — `{"cmd":"export"}` → blob, `{"cmd":"import","config":{...}}`
* **Compat** — basic string interface for legacy app commands

**Security**: static passkey (from NVS), SC+MITM pairing, notifications enabled per characteristic.

---

## ESP-NOW peers, topology & sequences

* **Peers**: add/remove and list all nodes participating in the mesh.
* **Topology**: JSON `links` defines zone connectivity (start→end).
* **Sequences**: trigger **UP (1-2-3)** or **DOWN (3-2-1)** activation per zone with per-step timings; the engine ignores opposite triggers while a direction is active (as per spec intent).&#x20;

---

## Logging & diagnostics

* Domain/severity events to SD (`ICMLogFS`) with rotation and optional UART streaming tools.
* Use this to trace AP/STA transitions, pairings, topology updates, and faults.

---

## RTC & scheduling

* DS3231 time set/get and **system↔RTC** synchronization.
* Hooks for future time-based activation (e.g., “lights on at 19:00”). The EasyDriveWay spec calls this out as a future enhancement; the firmware already provides RTC primitives to support it.&#x20;

---

## Cooling, sleep & power/buzzer cues

* **CoolingManager** monitors thermals and runs the fan; can raise over-temp events.
* **SleepTimer** handles inactivity sleeps and alarm-based wake.
* **BuzzerManager** (NVS-driven) plays event tones:

  * startup/ready, Wi-Fi connected/off, BLE pairing/paired/unpaired,
  * client connect/disconnect, **power lost** / **on battery** / **low battery**, over-temp, fault, shutdown, success/failure.
  * Enable/disable persisted via `BZFEED`.

---

## Security model

* **Wi-Fi**: AP protected by NVS password; STA creds stored in NVS.
* **BLE**: static passkey + SC MITM pairing; device name derived from NVS.
* **Config**: factory reset path clears NVS and returns to AP config mode (tie to your reset button/flow).

---

## Spec trace to EasyDriveWay

The EasyDriveWay system description identifies key functions and constraints; ICM maps them as follows:

* **Driveway detection & sequential lights** → ESP-NOW **topology + sequences** control.&#x20;
* **Wi-Fi interface for mobile app** → AP/STA + REST routes; permissive CORS for local UI.&#x20;
* **Bluetooth interface** → implemented as **BleICM** (GATT): control + telemetry channels.&#x20;
* **Timekeeping (future/NTP/RTC)** → **RTCManager** + NTP hooks in Wi-Fi layer.&#x20;
* **BIT LED / health** → **RGBLed** status patterns (heartbeat, fault).&#x20;
* **Reset to factory** → NVS wipe + AP mode for re-provisioning.&#x20;
* **Store configuration data** → **ConfigManager** (short keys).&#x20;
* **PSM / standby battery** → power cues and alarms surfaced via **BuzzerManager** + **Power** char.

---

## Troubleshooting

* **AP didn’t start**: check SPIFFS mount/logs; verify AP SSID/PASS in NVS.
* **No sequences**: ensure `links` topology is applied, then trigger **UP/DOWN**.
* **BLE “notify” not received**: confirm the client enabled notifications on each characteristic.
* **Wrong ESP-NOW channel**: re-align by reconnecting STA or restarting AP; ICM persists channel in NVS.

---

## Roadmap

* Web UI for SD log browsing & live tail.
* OTA update flow exposed over BLE/Web.
* Peer provisioning wizard and per-zone timing profiles.

---

## License

Proprietary — © ICM/EasyDriveWay 

