# ICM Controller Firmware

**Interface Control Module (ICM)** — the central ESP32 firmware that coordinates EasyDriveWay lighting and peripherals. It manages Wi‑Fi (STA/AP), ESP‑NOW pairing and secure messaging, the zero‑centered **topology** shared between relays and sensors, BLE control, SD logging, RTC timekeeping, cooling, sleep/wake, RGB status, and a configurable buzzer — all with NVS‑backed settings and export/import.

> This README documents the ICM firmware end‑to‑end, including the **new topology model** (zero‑centered sensors + relay boundaries) and how the web UI / REST / BLE glue together.

---

## Quick links

- [Overview](#overview)
- [System architecture](#system-architecture)
- [Features at a glance](#features-at-a-glance)
- [Repo layout](#repo-layout)
- [Build & flash](#build--flash)
- [Configuration (NVS keys)](#configuration-nvs-keys)
- [Networking modes (STA/AP) + ESP-NOW](#networking-modes-staap--esp-now)
- [BLE control](#ble-control)
- [REST API](#rest-api)
- [Peers, topology & sequences](#peers-topology--sequences)
- [Export / Import](#export--import)
- [Logging & diagnostics](#logging--diagnostics)
- [RTC & scheduling](#rtc--scheduling)
- [Cooling, sleep & power/buzzer cues](#cooling-sleep--powerbuzzer-cues)
- [Security model](#security-model)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [License](#license)

---

## Overview

ICM is the “brain” of **EasyDriveWay**: it listens to sensor modules, orchestrates relays in **waves** that follow motion (supporting **split‑lane** left/right), exposes control/config over **Wi‑Fi (REST/web UI)** and **BLE (GATT)**, and persists robust state in **NVS**. It’s designed as a hub with swappable expansion modules and clean JSON interfaces to the front‑end.

Key concept: **zero‑centered topology**

- Each **sensor** owns a local coordinate system with itself at **0**, relays to the left as **−1, −2, …** and to the right as **+1, +2, …**.
- Each **relay** knows only the **two boundary sensors** around it (A/B) and a **split rule** to decide which physical output is “left” vs “right” for split traffic.
- The **ICM** maintains and distributes this topology and pairing tokens. Sensors do local timing and send schedules to their relays; relays just execute.

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
   | ESPNowManager |<----------------+---- JSON peers/topology-+
   | (peers, topo, |                        & export/import
   |  schedules)   |<----------+
   +-------^-------+           |     +------------------+      +-----------------+
           |                   +---->|  BleICM (GATT)   |<---->|  ICMLogFS (SD)  |
           |                         |  WiFi/Peers/Topo |      |  logs/events    |
           v                         +------------------+      +-----------------+
       [Sensors/Relays via ESP‑NOW]                   [RTC] [Cooling] [Sleep] [RGB] [Buzzer]
```

---

## Features at a glance

- **Wi‑Fi STA/AP** with REST endpoints and permissive CORS for the local web UI.
- **ESP‑NOW**: secure pairing (per‑peer tokens), peer list export, **new topology model** distribution.
- **Split‑lane wave engine**: sensors can trigger independent left/right “waves”; relays have L/R outputs.
- **BLE (BleICM)**: phone‑friendly service for status, Wi‑Fi control, peers/topology, and export/import.
- **NVS configuration** with short keys; full device **export/import** JSON.
- **SD logging** via ICMLogFS; **RTC** timekeeping (DS3231); **sleep/cooling**, **RGB status**, **buzzer** cues.

---

## Repo layout

- `Config.h`, `ConfigManager.{h,cpp}` — NVS keys (≤6 chars), defaults, helpers
- `WiFiManager.{h,cpp}`, `WiFiAPI.h` — STA/AP control, REST routes, CORS
- `ESPNowManager.{h,cpp}` (split into core/modules/topology if desired) — peers, topology, schedules, export
- `BleICM.{h,cpp}` — BLE GATT (status, wifi, peers, topo, export/import)
- `ICMLogFS.{h,cpp}` — SD logging and utilities
- `RTCManager.{h,cpp}` — DS3231 timekeeping & sync
- `SleepTimer.{h,cpp}` — inactivity & alarm sleep
- `CoolingManager.{h,cpp}` — fan/temp policies
- `RGBLed.{h,cpp}` — status LED patterns
- `BuzzerManager.{h,cpp}` — event patterns + NVS enable/disable

---

## Build & flash

- **Target**: ESP32 (Arduino core).
- **Libs**: WiFi, ArduinoJson v7, NimBLE (or ESP32 BLE), SPIFFS/SD, RTClib, FreeRTOS.
- **Flash**: Arduino IDE or PlatformIO. Ensure partition supports SPIFFS/SD if used.

> ArduinoJson v7 uses `JsonArrayConst` / `JsonObjectConst` for read‑side casts. Adjust parsing accordingly.

---

## Configuration (NVS keys)

All keys are **≤6 chars**. Examples (not exhaustive):

- Wi‑Fi AP: `WIFNAM` (SSID), `APPASS` (pass)
- Wi‑Fi STA: `STASSI`, `STAPSK`, `STAHNM`, `STADHC`
- ESP‑NOW: `ESCHNL` (channel), `ESMODE` (mode)
- BLE: `BLENAM`, `BLPSWD`, `BLECON`
- Buzzer: `BZGPIO` (pin), `BZAH` (active‑high), `BZFEED` (enable)
- Topology: stored as JSON string under a dedicated key (see [Export / Import](#export--import))

See `Config.h` for the authoritative list & defaults.

---

## Networking modes (STA/AP) + ESP‑NOW

- **STA**: uses saved creds; on success, ESP‑NOW aligns to STA **channel**.
- **AP**: fallback or explicit; ESP‑NOW aligns to AP channel.
- ICM **persists** the chosen channel and mode; switching network re‑aligns peers automatically.

---

## BLE control

**Service** exposes write/read/notify characteristics with a compact JSON protocol, e.g.:

- **Status** — `{mode, ip, ch}`
- **WiFi** — `{"cmd":"sta_connect","ssid","password"}` / `{"cmd":"ap_start","ap_ssid","ap_password","ch"}` / `{"cmd":"scan"}`
- **Peers** — `{"cmd":"pair","mac","type"}` / `{"cmd":"remove","mac"}` / `{"cmd":"list"}`
- **Topo** — `{"cmd":"set","links":{...}}` / `{"cmd":"get"}`
- **Export** — `{"cmd":"export"}` → full config blob; `{"cmd":"import","config":{...}}`

Security: static passkey from NVS, SC+MITM pairing; notifications are opt‑in per characteristic.

---

## REST API

Base path served by `WiFiManager`.

### Peers

- `GET /api/peers/list` → `{ "peers": [ { "mac","type","idx"? }, ... ] }`  
  (`idx` present when the device has an assigned index in ESP‑NOW tables.)

### Topology

- `POST /api/topology/set` → body must include **`links`** (see below). If `push:true` present, ICM distributes to peers.
- `GET  /api/topology/get` → returns `{ "links": { ... } }` (wrapper always provided).

### Export

- `GET  /api/export` → full config snapshot (peers, topology, channel, mode, etc.)
- `POST /api/import` → apply a snapshot `{ "config": { ... } }`

CORS enabled for the local web UI.

---

## Peers, topology & sequences

### Peer model

- Each device is paired with a **peer record**: MAC, type (**sensor/relay/entrance/parking**), optional **idx**, and a 16‑byte token.
- ICM can **serializePeers()** for UI/diagnostics.

### Topology (new model)

**ICM accepts and serves a single JSON shape** wrapped as `{"links":{...}}`:

```jsonc
{
  "links": {
    "zc": [
      {
        "sensIdx": 1,
        "mac": "02:00:00:00:00:06",
        "hasPrev": true,
        "prevIdx": 254,
        "prevMac": "02:00:00:00:00:07",
        "hasNext": true,
        "nextIdx": 2,
        "nextMac": "02:00:00:00:00:08",
        "neg": [{ "relayIdx": 9, "pos": -1, "relayMac": "02:00:00:00:00:12" }],
        "pos": [{ "relayIdx": 10, "pos": +1, "relayMac": "02:00:00:00:00:13" }]
      }
    ],
    "boundaries": [
      {
        "relayIdx": 9,
        "splitRule": 0,
        "hasA": true,
        "aIdx": 254,
        "aMac": "02:00:00:00:00:07",
        "hasB": true,
        "bIdx": 1,
        "bMac": "02:00:00:00:00:06"
      }
    ]
  }
}
```

**Rules**

- _Sensors_ receive their **own zero‑centered map** (`neg`/`pos`) and neighbors (`prev`/`next` if present).
- _Relays_ receive their **two boundary sensors** and a **split rule** to map left/right outputs.
- _ICM_ persists the topology JSON to NVS and can **push** to peers on demand.

> **Compatibility**: the firmware accepts either the wrapped object (`{"links":{...}}`) or the bare object (`{"zc":...,"boundaries":...}`) internally; the REST **GET** always returns the wrapped form.

### Sequences / waves (high level)

- Sensors compute **direction & speed** per lane (left/right) from their on‑board sensing pairs and schedule the relays in their local section in “all‑on then 1‑2‑3” or “3‑2‑1”, depending on direction and measured speed.
- If two cars come from opposite directions, the wave **splits**; relays drive L/R channels independently until waves pass.
- If a car stops, the section holds; when it resumes or reverses, timing continues appropriately.

---

## Export / Import

### From device (authoritative snapshot)

- **GET `/api/export`** returns:
  ```jsonc
  {
    "peers": [ ... ],
    "topology": { "links": { "zc": [...], "boundaries": [...] } },
    "channel": 6,
    "mode": "STA"
  }
  ```
- **POST `/api/import`** accepts the same blob (or superset) and applies peers/topology/channel/mode.

### From the web UI (local file round‑trip)

- **Export** writes `{"links":{ "zc":[...], "boundaries":[...] }}` to a file you save.
- **Import** accepts:
  - legacy `{"links":[{type,mac,prev?,next?}, ...]}`
  - new `{"links":{"zc":[...], "boundaries":[...]}}`
  - bare `{"zc":[...], "boundaries":[...]}` (as found in NVS backups)
  - full device export blob (it extracts `.topology.links`)

Importer reconstructs the **visual chain** using `nextIdx` when present or **MAC neighbor fallback**: `nextMac → (find zc whose mac == nextMac)`; if absent, `(find zc whose prevMac == cur.mac)`. This prevents “unplaced” peers when importing your own exports.

---

## Logging & diagnostics

- Events grouped by **domain/severity** to SD (`ICMLogFS`) with rotation; optional UART streaming.
- Recommended to enable verbose logs during pairing and topology setup.

---

## RTC & scheduling

- **DS3231** timekeeping and system↔RTC synchronization.
- Hooks exist for future time‑based activations (e.g., sunset schedules).

---

## Cooling, sleep & power/buzzer cues

- **CoolingManager**: temp monitor + fan control.
- **SleepTimer**: inactivity sleeps & alarm wake.
- **BuzzerManager** (NVS‑driven): tones for startup, Wi‑Fi/BLE events, power states, low battery, faults, shutdown, success/fail.

---

## Security model

- **Wi‑Fi**: AP is passworded; STA creds stored in NVS.
- **BLE**: static passkey + SC MITM pairing; device name from NVS.
- **ESP‑NOW**: per‑peer tokens; topology populated only for paired devices.
- **Factory reset**: NVS wipe returning to AP config mode.

---

## Troubleshooting

- **Can’t import a topology file**: the UI accepts legacy arrays and the new wrapped/bare zc+boundaries. Ensure each `zc[]` entry includes **`mac`** or at least provides `nextMac/prevMac` so the importer can chain nodes. If some peers remain in the palette, verify there are no typos in MACs and that anchors (254/255) are present.
- **No activation on relays**: check that relays received boundary info (via `/api/topology/get`) and that sensors pushed schedules (watch logs).
- **Wrong ESP‑NOW channel**: switch STA/AP or reboot; channel re‑aligns and persists.
- **BLE notifications missing**: client must enable notifications per characteristic.

---

## Roadmap

- Web UI: SD log browser + live tail.
- OTA updates via BLE/Web.
- Guided peer provisioning and per‑section timing profiles.

---

## License

Proprietary — © ICM / EasyDriveWay
