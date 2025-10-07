# ESP-NOW Driveway Light Control System (ESP32-S3 / Arduino + ESP-IDF via PlatformIO)

## TL;DR

A modular, distributed lighting system for a driveway built on **ESP-NOW**. It comprises six device roles—**ICM** (master), **PMS** (power mgmt), **SENSOR**, **RELAY**, and their **EMULATOR** counterparts—sharing a common hardware/software baseline and exposing live telemetry + control. The repo already includes a production-quality **ESP-NOW stack** (transport, security, TLV topology, role APIs), **peripheral managers** (sensors, relays, time, storage), and **configuration layers** per role. The **main application** and the **WiFi Manager** are intentionally not shipped yet; this is an ongoing project.

---

## 1) High-Level Architecture

### Roles

- **ICM (Control Unit)**: Master coordinator with web UI (planned), topology builder, pairing & provisioning.
- **PMS (Power Management Unit)**: Source selection (wall/battery), current/voltage monitoring, channel power gating.
- **SENSOR**: Directional vehicle detection via TF-LUNA pairs + ambient sensing (BME280, VEML7700).
- **RELAY**: Controls the physical light segments.
- **SENSOR EMULATOR**/**RELAY EMULATOR**: Single physical node presenting multiple virtual sensors/relays for lab testing.

### Shared runtime features

- Device metadata (ID/Name/Type/HW/SW/Role)
- Live telemetry on request (temp, time, fan mode, logs, faults, topology)
- Live control (buzzer/LED ping, fan control, reset/restart, set system time)

### Role-specific highlights

- **RELAY**: Read/drive relay states.
- **SENSOR**: Read pressure/temp/humidity, TF-LUNA AB pairs, day/night levels; set thresholds.
- **PMS**: Report power source, voltages/currents; enable/disable power to role groups.
- **ICM**: Push config/topology and manage pairing; no extra telemetry by itself.
- **Emulators**: Expose N virtual instances per physical MAC (great for topology tests).

### Topology & Modes

- **Topology**: Assembled on ICM UI, chaining SENSOR→RELAY→… with explicit neighbor & dependency maps. Devices receive a per-node token and role params.
- **Auto**: SENSORs detect direction and command downstream RELAYs with timed activations.
- **Manual**: ICM can directly actuate devices (planned via web).

---

## 2) Project Layout

> The codebase is intentionally layered: **ESP-NOW stack** (protocols, security, transport), **Role logic**, **Peripherals**, **Config**, and **Hardware**.

```
src/
├─ Config/                    # Role & common configuration
│  ├─ Config_Common.h
│  ├─ Config_ICM.h
│  ├─ Config_PMS.h
│  ├─ Config_REL.h
│  ├─ Config_REMU.h
│  ├─ Config_SEMU.h
│  ├─ Config_SENS.h
│  ├─ RGBConfig.h
│  ├─ RTCConfig.h
│  └─ SetRole.h               # Compile-time role selection hooks
│
├─ EspNow/                    # Complete ESP-NOW stack
│  ├─ EspNowAPI.h             # Public API surface per role
│  ├─ EspNowStack.h           # Facade/wiring
│  ├─ codec/                  # Message builders/parsers (control, fw, etc.)
│  ├─ config/                 # Network & radio config helpers
│  ├─ core/                   # Core dispatch, routing, callbacks
│  ├─ fw/                     # Firmware mgmt / OTA message helpers
│  ├─ roles/                  # Role glue (role_icm.cpp, role_pms.cpp, ...)
│  ├─ security/               # HMAC, token validation, integrity
│  ├─ topology/               # TLV store, token, topology encoding/decoding
│  ├─ transport/              # espnow_radio.cpp (thin radio wrapper)
│  └─ util/                   # time/byte helpers, common utilities
│
├─ Hardware/                  # Per-role hardware pinouts/abstractions
│  ├─ Hardware_ICM.h
│  ├─ Hardware_PMS.h
│  ├─ Hardware_REL.h
│  ├─ Hardware_REMU.h
│  ├─ Hardware_SEMU.h
│  └─ Hardware_SENS.h
│
├─ Peripheral/                # Sensor/actuator managers & system services
│  ├─ ACS781Manager.{h,cpp}   # Current sensor (100A version in PMS)
│  ├─ BME280Manager.{h,cpp}
│  ├─ BuzzerManager.{h,cpp}
│  ├─ DallasTempManager.{h,cpp}# DS18B20
│  ├─ FanManager.{h,cpp}
│  ├─ LEDManager.{h,cpp}
│  ├─ NTPManager.{h,cpp}
│  ├─ RelayManager.{h,cpp}
│  ├─ RTCManager.{h,cpp}      # DS3231
│  ├─ SDManager.{h,cpp}       # Logging to SPIFFS/SD
│  ├─ SensorManager.{h,cpp}   # TF-LUNA pairs, VEML7700, BME280 integration
│  ├─ SleepTimer.{h,cpp}
│  ├─ SwitchManager.{h,cpp}
│  ├─ TCA9548A.{h,cpp}        # I2C mux, if present
│  ├─ TFLunaManager.{h,cpp}
│  ├─ VEML7700Manager.{h,cpp}
│  └─ WiFiManager.h           # NOTE: interface only; implementation TBD
│
├─ Utils.{h,cpp}              # Common helpers (blink params, etc.)
└─ (no main.cpp yet)          # Application entrypoint is intentionally pending
```

**Notes**

- The **ESP-NOW stack** is already production-shaped: transport, message codec layers, topology TLV, and HMAC security exist and are intended to be role-agnostic.
- **Role files** (e.g., `roles/role_icm.cpp`) are declarations or thin stubs—actual behavior is designed to be implemented alongside the future main app.
- `Peripheral/WiFiManager.h` defines the external interface; its `.cpp` will arrive with the web UI work.

---

## 3) PlatformIO & Build Environment

This project targets **`esp32-s3-devkitc1-n16r8`** (16 MB flash / 8 MB PSRAM) and leverages **Arduino + ESP-IDF** together. Serial monitor is set to **921600** baud. Key build flags enable PSRAM and FreeRTOS run-time stats; libraries cover storage, JSON, time/NTP, Dallas/OneWire, VEML7700, BME280, RTC, TF-Luna, etc.

> **Why Arduino+ESP-IDF?** You keep Arduino ergonomics while reaching into ESP-IDF features when needed (ESP-NOW, partitions, PSRAM, etc.).

### Required libraries (from `platformio.ini`)

- SdFat (Adafruit fork), ArduinoJson, MAX1704x fuel-gauge (SparkFun), Time, NTPClient, EspSoftwareSerial, DallasTemperature, SparkFun VEML7700, Adafruit BME280, RTClib, **TFLuna-I2C** (Bud Ryerson).

### Flash & filesystem

- Flash mode: **QIO @ 80 MHz**, filesystem: **SPIFFS**, partitions: **custom CSV**.

---

## 4) Flash Layout (Custom Partitions)

`partitions_16M.csv`

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,data,nvs,0x9000,0x5000,
factory,app,factory,0x10000,0x690000,
config,data,nvs,0x6A0000,0x6D000,
spiffs,data,spiffs,0x70D000,0x8D8000,
coredump,data,coredump,0xFE5000,0x1B000,
```

- **App**: ~6.59 MB at `0x10000`
- **SPIFFS**: ~9.0 MB starting at `0x70D000` (ample space for logs/topologies/exports)
- **NVS (config)**: dedicated area at `0x6A0000`
- **Coredump**: reserved at the tail to aid post-mortem debugging

> This layout is tuned for large binaries (Arduino+ESP-IDF) and **generous SPIFFS** for device logs, topology snapshots, and emulation datasets.

---

## 5) How Things Fit Together

1. **Peripherals** provide a stable HAL for sensors/actuators:

   - PMS uses **ACS781** for currents, voltage ADC channels (in `Peripheral` managers).
   - SENSOR nodes pair **TF-LUNA** (A/B) for direction, plus **BME280** and **VEML7700** for environment/light.
   - RELAY nodes drive outputs via `RelayManager`.

2. **ESP-NOW stack** transports:

   - **Transport** wraps radio init, peer mgmt, RX/TX.
   - **Codec** builds/parses role/control/fw messages.
   - **Topology** encodes TLV neighbors, dependencies, and a **per-node 32-char token** for integrity.
   - **Security/HMAC** authenticates messages and tokens.

3. **ICM** (planned main app) will:

   - Pair devices (incl. **emulated** virtual instances per MAC).
   - Build & push chain topology.
   - Provide a web UI for **Auto/Manual** mode, gauges, logs, and search/remove by MAC.

---

## 6) Current Status & Roadmap

✅ Already in the tree

- ESP-NOW transport + TLV topology + HMAC stubs
- Role scaffolding (ICM/PMS/SENS/REL + emulators)
- Sensor & actuator managers (TF-LUNA, BME280, VEML7700, DS18B20, relays, fan, buzzer)
- RTC/NTP/time unification
- Storage managers (SdFat/SPIFFS hooks)

🧩 In progress / missing by design

- **`main.cpp`**: application entrypoint that wires role selection (`Config/SetRole.h`), sets up the managers, enters the control loop, and registers ESP-NOW callbacks.
- **`WiFiManager.cpp`**: actual Wi-Fi station/AP logic and the future web UI (the header exists; implementation TBD).

🗺️ Next milestones

- Implement **main loop**: init peripherals based on role, mount FS, start ESP-NOW, register handlers, enter run loop.
- Finalize **WiFiManager**: AP/STA, credential mgmt, HTTP server (AsyncWebServer or IDF HTTPD), REST endpoints, and a compact web UI for topology + live controls.
- Hook **ICM UI** actions to **ESP-NOW** commands (push topology, trigger relays, query status).
- Add **persistent config** writes to the dedicated `config` NVS partition.
- Define a **message schema** doc (IDs, TLVs, field types) under `docs/`.

---

## 7) Building, Uploading & Monitoring

1. **Install**: VS Code + PlatformIO.
2. **Open** the project folder; select the environment **`esp32-s3-devkitc1-n16r8`**.
3. **Build** (Ctrl/Cmd+Shift+B in PIO).
4. **Upload** firmware (USB). Monitor at **921600** baud.
5. **Upload FS** (when you add web assets/logs): use PlatformIO’s _Upload File System Image_ with `board_build.filesystem = spiffs`.

> If you enable OTA later, the existing **fw codec** hooks can be extended for chunked image transfer and staged apply.

---

## 8) Configuration Conventions

- **Role selection**: `Config/SetRole.h` drives which `Hardware_*.h` + `Config_*.h` are pulled in at build time.
- **Per-role configs**: keep thresholds (TF-LUNA, day/night), timing (relay activation), and I2C addresses here.
- **Tokens & pairing**: topology push includes a per-node token (see `EspNow/topology/*` + `EspNow/security/*`).

---

## 9) Logging & Telemetry

- **SPIFFS**: default place for logs, exportable by the ICM UI later.
- **Live telemetry** (on demand): time/temp/fan/faults/topology snapshots across roles.
- **Coredump**: on crash, retrieve from the dedicated partition and symbolize with IDF tools.

---

## 10) Testing with Emulators

- **Sensor Emulator**: one device can emulate many virtual A/B pairs; each appears as a draggable unit in the chain builder.
- **Relay Emulator**: one device exposes multiple virtual relay outputs; great for validating chase sequences and timings.

---

## 11) Coding Guidelines (already reflected)

- Keep **transport** and **role logic** separated.
- Use **TLV** for topology and **strict schemas** in codec.
- Prefer **non-blocking** peripherals (timers/FreeRTOS tasks) over long delays.
- Keep **Arduino** ergonomics; drop to **ESP-IDF** for radio/partition specifics when necessary.

---

## 12) FAQ

**Q: Why no `main.cpp` yet?**
A: The stack and managers are stabilized first. `main.cpp` will wire roles, initialize managers, register RX handlers, and publish the web UI endpoints (once `WiFiManager.cpp` is added).

**Q: Can I run a single board with multiple virtual relays/sensors?**
A: Yes—use the **emulator roles**; the topology treats each virtual instance independently.

**Q: Will this run without Wi-Fi?**
A: Core **ESP-NOW** pieces run fine. Wi-Fi is for UI/NTP/OTA and is gated behind the future `WiFiManager.cpp`.

---

## 13) License & Contributions

This is an ongoing engineering effort. Contributions welcome—especially around the **main app wiring**, **web UI**, and **message schema docs**.

---

_Build env & libs derived from your `platformio.ini`; flash & FS derived from your `partitions_16M.csv`._
