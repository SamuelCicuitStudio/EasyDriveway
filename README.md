# EasyDriveWay — Sequential Driveway Lighting System

> Automated, token-secured ESP-NOW mesh that detects vehicles and lights your driveway in graceful sequences — with a central **ICM** controller and a **PSM** that powers & monitors the 48 V rail.&#x20;

---

## 1) What this project is

**EasyDriveWay** turns PIR detections into **sequential light animations** across driveway zones.
It’s built as a set of self-contained modules:

* **ICM (Interface Control Module)** — the brain. Connects to Wi-Fi for configuration, orchestrates sequences, manages peers, caches telemetry, and enforces security.
* **PSM (Power Supply Module)** — provides the 48 V bus and 5 V to the ICM, charges the standby battery, exposes live power telemetry and faults to the ICM.
* **Presence sensors** — PIR (and a Day/Night input) feeding the ICM.
* **Relay nodes** — switch light segments; sequenced “up” (1-2-3) or “down” (3-2-1) per zone trigger.

Target environment: outdoors (-25 °C to +55 °C; high humidity), surge-protected interfaces, 5-year MTBF design goal.&#x20;

---

## 2) High-level architecture

```
       ┌─────────────────────────┐
       │      Mobile App*        │
       │  (config & monitoring)  │
       └───────────▲─────────────┘
                   │ Wi-Fi (ICM STA/AP)
┌──────────────────┴──────────────────┐
│            ICM (ESP32-S3)           │
│  • Wi-Fi config / REST UI*          │
│  • ESP-NOW master (tokens)          │
│  • Pairing: PSM / Relays / Sensors  │
│  • Day/Night logic cache            │
│  • Sequence control (UP/DOWN)       │
│  • Topology push & orchestration    │
│  • Logs (ICMLogFS), RTC sync        │
└───────┬───────────┬───────────┬─────┘
        │           │           │  ESP-NOW (fixed channel)
   ┌────▼───┐  ┌────▼───┐  ┌────▼───┐
   │ PSM    │  │Relays  │  │Sensors │
   │ (Power)│  │(1..N)  │  │(PIR +  │
   │ 48V/5V │  │        │  │Day/Night)
   └────────┘  └────────┘  └────────┘
```

\* Mobile app/time-scheduling are planned features; scaffolding is present in the ICM.&#x20;

---

## 3) Core behaviors

### Presence → Sequence

* Each **zone** has **3 outputs**; on a trigger the ICM runs a timed sequence:
  **UP:** 1 → 2 → 3, **DOWN:** 3 → 2 → 1 (default 10 s per step, configurable).
* Triggers are **edge-qualified** (inactive→active with basic debouncing).
* Day/Night input suppresses sequences during daylight.&#x20;

### Power & telemetry (PSM)

* PSM reports **on/off**, **faults**, **Vbus/Vbat (mV)**, **Ibus/Ibat (mA)** and **board temp**.
* ICM can **turn 48 V on/off**, **request shutdown**, **clear faults**, and read PSM temp.&#x20;

### Configuration & health

* Wi-Fi credentials, module addresses, counts, and sequence settings are stored in NVS.
* **BIT LED** indicates system health (pulsing = OK). **Reset button** restores factory defaults.&#x20;

---

## 4) Communication & security

### Shared Command API

Both sides include a shared `CommandAPI.h` that defines:

* **Message header** (`IcmMsgHdr`): version, domain, opcode, flags (ACK), unix time, counter, and a **16-byte token** fragment for auth.
* **Domains & opcodes:** `SYS`, `POWER`, `RELAY`, `SENS`, `TOPO`, `SEQ`.
* **Payloads:** `PowerStatusPayload`, `TempPayload`, `DayNightPayload`, `SeqStartPayload`, etc.

This guarantees the ICM<→PSM contract and avoids payload drift. (Project sources reference this shared header.)

### Tokens, ACKs, and channel control

* ICM computes and assigns per-peer tokens; **every frame carries token16**.
* PSM accepts only `SYS_INIT`/`SYS_PING` before a token is set; afterward, token-mismatch frames are dropped.
* Critical ops request **app-level ACK** (`SYS_ACK` echoes the message counter).
* ICM can **orchestrate ESP-NOW channel changes** (`SYS_SET_CH`); nodes re-add peers on the new channel.

---

## 5) Topology & sequencing model

* **Topology mirrors** live in the ICM (previous/next relay, sensor→relay dependencies).
* ICM can **push bundles** (MACs, optional IPv4, and expected tokens) to nodes so sensors can address the correct relay quickly.
* Entrance/parking sensors are handled as **special indices** for lane-direction logic.

---

## 6) Hardware overview

* **PSM**: Supplies **48 V DC** (lights) and **5 V** (ICM), includes battery **charger/monitor**, current/voltage sensing, and board temperature.
* **ICM**: ESP32-S3 + external antenna, USB-C for service logs/firmware, BIT (green) + FAULT (red) LEDs, reset/factory button, optional RTC.
* **I/O**: I²C relay modules (scalable to 32 relays), PIR inputs via I²C I/O, surge protection on external lines.
* **Environment**: Designed for **–25 °C…+55 °C** operation, **93 % RH** damp heat, and EMI/EMC considerations; sensors are “exposed to weather.”&#x20;

---

## 7) Software components

* **ICM firmware**

  * **ESP-NOW Master**: pairing, tokens, retries/backoff, ACK management, topology push, sequence control, caches (power, temps, day/night).
  * **Wi-Fi Manager**: STA/AP setup, configuration REST endpoints\*, credential storage, safe reset/factory restore.
  * **Logging/RTC**: `ICMLogFS` for on-device logs; wall-clock in headers for traceability.

* **PSM firmware**

  * **ESP-NOW Slave**: processes `PWR_*` commands, composes `PowerStatusPayload`, replies to `GET_TEMP`, supports `SYS_INIT`/`SYS_SET_CH`.
  * **Hardware hooks**: `hwIs48VOn`, `readFaultBits`, `measure48V_mV/mA`, `measureBat_mV/mA`, `readBoardTempC` — **you implement these** against your analog front-end/PMIC.

\* The mobile app and time-schedule logic are future items; scaffolding exists in the spec for later integration.&#x20;

---

## 8) Build & flash

* **Tooling**: ESP32 toolchain (Arduino core or PlatformIO).
* **Targets**: ESP32-S3 for ICM; ESP32 (S2/S3) for PSM (adjust in `platformio.ini` or Arduino Board Manager).
* **Steps**:

  1. Open the ICM project, set Wi-Fi credentials (or first-boot AP), set ESP-NOW channel.
  2. Open the PSM project, verify pin mapping for voltage/current/temperature inputs; leave hardware functions as stubs initially.
  3. Flash both; power the PSM, then the ICM.
  4. Pair modules via the ICM UI/serial commands (ICM assigns tokens & stores MACs).
  5. Test: `PWR_GET`, `PWR_SET`, sensor triggers, and a simple 3-relay sequence.

---

## 9) Repository layout (suggested)

```
/docs/
  EasyDriveWay-Spec.pdf
/firmware/
  /icm/
    src/ … (WiFiManager, ESPNowManager, RTCManager, ICMLogFS)
    include/CommandAPI.h
  /psm/
    src/ PSMEspNowManager.cpp
    include/ PSMEspNowManager.h
    include/ PSMCommandAPI.h   // PSM-only payloads; uses shared CommandAPI payloads
/shared/
  CommandAPI.h                  // single source of truth for header/opcodes/payloads
```

---

## 10) Configuration data (stored in NVS)

* Wi-Fi SSID/password, application credentials
* Number/addresses of relay & I/O modules
* Sequence timings & mode (auto/manual)
* Peer MACs & per-peer token fragments
* Topology JSON mirror (sensor→relay, relay next-hops)&#x20;

---

## 11) Security hardening

* **No default credentials** for Wi-Fi/app.
* **Brute-force throttling** and IP filtering (UI/API side).
* **Token-gated** ESP-NOW traffic; unknown tokens dropped silently.
* Firmware upgrade via USB service port; keep device off public networks where possible.&#x20;

---

## 12) Roadmap

* Mobile app UI (status, manual control, notifications)
* Time-based schedules (RTC/NTP)
* Bluetooth provisioning (optional)
* Extended relay topologies and richer effects
* Field diagnostics & richer BITE&#x20;

---

## 13) License

Add your chosen license here (e.g., MIT/Apache-2.0).

---

