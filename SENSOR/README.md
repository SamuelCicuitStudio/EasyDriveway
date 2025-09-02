# Sensor Module (Presence/Direction Node) — TF‑Luna ×2 + BME280 + VEML7700 + Buzzer + Fan

ESP32‑based sensor node that detects motion and direction over a single segment, computes speed from two time‑of‑flight probes, and drives nearby relays in a chaser pattern (**1‑2‑3** or **3‑2‑1**) via ESP‑NOW. The node also reports environment (temp/humidity/pressure), ambient light (lux & day/night), and manages its own cooling. It participates in an ICM‑managed mesh and persists its topology & tokens to NVS.

---

## 1) Hardware profile (this build)

- **MCU**: ESP32 (dual core, FreeRTOS)
- **TF‑Luna ×2** on **I²C1** for presence/direction/speed  
  SDA = **GPIO4**, SCL = **GPIO5** (addresses configurable; see provisioning)
- **BME280** on **I²C2** for environment (temp °C, humidity %, pressure Pa)  
  SDA = **GPIO16**, SCL = **GPIO17**
- **VEML7700‑TR** on **I²C2** for ambient light (lux → day/night), typical addr `0x10`
- **Buzzer** on **GPIO11** for user feedback (pairing, provisioning, faults)
- **Fan (PWM)** for enclosure cooling (pins unchanged; PWM via CoolingManager)
- **RGB status LED** pins unchanged from previous board
- **Storage**: external SD (model “MKDV8GIL”), same pins

> All pins are configurable via NVS keys; the above are defaults.

---

## 2) Software architecture (high‑level)

```
Device (RTOS controller)
 ├─ SensorEspNowManager  → ESP‑NOW I/O, topology, token gate, relay fanout
 ├─ TFLunaManager        → two ToF probes (A/B): presence, dist, amp
 ├─ BME280Manager        → tC, RH, pPa (5‑min periodic task)
 ├─ VEML7700Manager      → lux, day/night with hysteresis
 ├─ SwitchManager        → user button gestures (double/triple‑tap wizard for TF addresses)
 ├─ CoolingManager       → fan PWM control (AUTO / manual)
 ├─ RGBLed + Buzzer      → status and prompts
 └─ RTCManager           → system time (ESP32 internal; no external RTC)
```

Key files you’ll touch:

- `Device.h/.cpp` — state machine & RTOS tasks (BME every 5 min, TF watcher 15 ms)
- `SensEspNow.h/.cpps` — pairing, topology, relay control, neighbor wave handoff
- `main.h` — **one‑shot bring‑up** of NVS, I²C1/I²C2, managers, and Device
- `main.cpp` — tiny entry point calling `App_BringUpAll()` and `App_MainLoop()`

---

## 3) Boot & initialization sequence

1. **NVS & ConfigManager** start; default keys are created on first boot (device ID, Wi‑Fi AP name/pass, I²C pins, sensor addresses, thresholds, etc.).
2. **I²C buses** are brought up from NVS pins: I²C1 → TF‑Luna A/B; I²C2 → BME280 + VEML7700.
3. **Managers** begin in dependency order: RTC → LogFS → LED/Buzzer → BME → ALS → TF → Cooling → ESP‑NOW → Switch → Device.
4. **ESP‑NOW** loads any persisted state: channel, **ICM MAC**, **node token**, and last **topology** snapshot; it re‑adds peers when present.
5. **Device** starts two tasks:
   - **BME task** (periodic 5 min): read ENV and optionally report to ICM (`sendEnv()`).
   - **TF watcher task** (~15 ms): read A/B, detect vehicle, compute direction & speed, and **fan out** relay commands via topology.

---

## 4) State machine (lifecycle)

```
IDLE → PAIRING → CONFIG_TF → WAIT_TOPO → RUNNING
```

- **IDLE**: decide where to go based on NVS flags and cached topology.
- **PAIRING**: LED blinks blue; buzzer plays `EV_PAIR_REQUEST`. The first **ICM message** that arrives:
  - If device is **not yet paired** (`PAIRED=false`) and **no token** is loaded, the sensor **learns the token** from the ICM message header and persists it.
  - The ICM MAC is also learned (first sender) and stored.
  - **Token gating** activates for all future RX/TX.
- **CONFIG_TF**: prompt user to configure the two TF‑Luna addresses via **SwitchManager** (see §6). LED blinks green; buzzer plays `EV_CONFIG_PROMPT`.
- **WAIT_TOPO**: sensor waits for the ICM to push a valid **topology** (neighbors and local relay lists).
- **RUNNING**: all conditions met (paired + TF configured + topology present). The node detects vehicles, computes **speed** (Δx/Δt from A/B), derives **direction** (A→B = +1, B→A = −1), and fans out relay pulses with appropriate delays.

> **Persistence rule:** the sensor only sets `PAIRED=true` and `TFLCFG=true` **after** a topology has been received, ensuring the node is fully commissioned before becoming operational.

---

## 5) Pairing & security

- **What is paired?** The **ICM MAC** (for peer add) and a **16‑byte token** the ICM generates for this node.
- **How learned?** On the first valid ICM message:
  - The sensor **adopts the sender’s MAC** as its ICM.
  - If not paired and no token is present, it **saves the `tok16`** from the header to NVS and enables token gating.
- **How enforced?** Every inbound frame must carry the matching `tok16`; otherwise it’s dropped. All outbound frames **include the token** in the header.
- **Encryption:** ESP‑NOW per‑peer encryption is **disabled** by default; the token acts as an application‑layer gate. You can later extend to LMK‑based per‑peer encryption if required.

---

## 6) TF‑Luna address provisioning (double/triple‑tap wizard)

Use **SwitchManager** gestures on I²C1 to assign addresses deterministically:

- **Double‑tap** → “Assign **A**”
- **Triple‑tap** → “Assign **B**”

Procedure (per sensor):

1. Connect **only one** TF‑Luna on I²C1.
2. Trigger the gesture (double for A / triple for B).
3. The board scans, writes the configured address (`TFL_A_ADDR` or `TFL_B_ADDR`), persists, and confirms with a buzzer OK pattern.

> You can attach A first, then B. If you made a mistake, repeat the wizard with just that sensor connected.

---

## 7) Topology & relay control

- **Topology** arrives from the ICM and includes:
  - This node’s index (**ZC sensor index**), **prev/next** neighbor MAC+token (if any), and two ordered relay lists:
    - `neg[]` → relays on the **negative side** (−1 nearest → farther)
    - `pos[]` → relays on the **positive side** (+1 nearest → farther)
- The node stores peer MACs/tokens, registers peers, and exposes helpers:
  - `playWave(lane, dir, speed, spacing_mm, on_ms, all_on_ms, ttl_ms)`
  - `sendRelayOnForByPos/Idx(...)`

**Wave math**:

- Direction **A→B (+1)** → walk `pos[]` nearest→farthest (1‑2‑3).
- Direction **B→A (−1)** → walk `neg[]` farthest→nearest (3‑2‑1).
- `step_ms = clamp(spacing_mm / speed_mmps, 80..300)`
- Each relay receives a `REL_ON_FOR` with per‑relay `delay_ms` to create the chaser pattern. Optionally an **all‑on** pre‑flash can be sent.

**Neighbor handoff** (optional): send a compact wave header (lane, dir, `speed_mmps`, `eta_ms`) to prev/next sensors so they can pre‑arm.

---

## 8) Environment & light

- **BME task (5 min)** reads temperature, humidity, and pressure; can be sent upstream via `sendEnv()` automatically.
- **VEML7700** provides lux; a hysteresis pair (`ALS_T1` for day, `ALS_T0` for night) generates a robust **day/night** flag for UI or brightness control.

---

## 9) Cooling

- **CoolingManager** runs continuously and selects **ECO / NORMAL / FORCED / STOPPED / AUTO**.
- In **AUTO**: thresholds (°C) with hysteresis drive PWM duty bands (e.g., 30/60/100%). All thresholds come from NVS and can be tuned.

---

## 10) Bring‑up & usage

### Quick start

1. Flash the firmware and reset.
2. The node will **prompt pairing** (blue blink, pairing beep). Let the ICM send its first message; the token is learned automatically.
3. **Provision TF‑Luna** addresses: connect one unit on I²C1 → **double‑tap** for A; then connect the other → **triple‑tap** for B.
4. Wait for **topology** from ICM. Once received, the node marks itself **paired+configured** and transitions to **RUNNING**.
5. Perform a **walk/drive test**: verify **direction** and **speed** look correct and the chaser plays in the right order.

### Entry point

Your `main.cpp` is intentionally tiny:

```cpp
#include "main.h"

void setup() { App_BringUpAll(); }
void loop()  { App_MainLoop();   }
```

`main.h` constructs NVS/Preferences, brings up I²C1/I²C2, instantiates managers (RTC, LogFS, LED, Buzzer, BME, ALS, TF, Cooling, ESP‑NOW, Switch), and starts `Device` (RTOS tasks).

---

## 11) Configuration (key NVS parameters)

| Key                           | Meaning                     | Typical                       |
| ----------------------------- | --------------------------- | ----------------------------- |
| `PAIRED`                      | device paired flag          | `false`→`true` after topology |
| `TFLCFG`                      | TF‑Luna configured flag     | `false`→`true` after topology |
| `SSM_TOKEN16`                 | node token (hex, 32 chars)  | learned from ICM              |
| `SSM_MASTER_MAC`              | ICM MAC string              | learned                       |
| `ESPNOW_CH` / `SSM_ESPNOW_CH` | ESP‑NOW channel             | 1..13                         |
| `ESPNOW_MD`                   | AUTO/MAN mode (system mode) | `0` auto                      |
| `I2C1_SDA` / `I2C1_SCL`       | TF‑Luna bus pins            | 4 / 5                         |
| `I2C2_SDA` / `I2C2_SCL`       | BME/ALS bus pins            | 16 / 17                       |
| `TFL_A_ADDR` / `TFL_B_ADDR`   | TF‑Luna addresses           | `0x10` / `0x11`               |
| `AB_SPACING_MM`               | physical A‑B spacing        | ~350 mm                       |
| `CONFIRM_MS`                  | relay pulse ON time         | 140 ms                        |
| `STOP_MS`                     | TTL cap for waves           | 1200 ms                       |
| `ALS_T0` / `ALS_T1`           | night/day lux thresholds    | 180 / 300                     |
| `FAN_ON_C` / `FAN_OFF_C`      | cooling hysteresis          | 55 / 45                       |

> See `Config.h` for the full list; defaults are applied by `ConfigManager::initializeVariables()` on factory reset.

---

## 12) Diagnostics & feedback

- **LED**: slow blue blink → pairing; green blink → TF config; steady/heartbeat → normal.
- **Buzzer**: `EV_PAIR_REQUEST` = attention triple‑chirp; `EV_CONFIG_PROMPT` = short double chirp; additional patterns for faults and saves.
- **Logging**: `ICMLogFS` records key events (card optional); UART prints basic progress.

---

## 13) Troubleshooting

- **No pairing**: ensure ICM is on the same channel; verify ESP‑NOW initialized; check that first ICM message reaches the sensor.
- **Token mismatch**: after pairing, frames _must_ include the learned `tok16`; otherwise they are dropped silently.
- **TF‑Luna issues**: provision with only one device connected at a time; verify I²C1 wiring and pull‑ups; confirm addresses.
- **Relays silent**: ensure topology is present; confirm relay MACs/tokens are in NVS; verify step timing isn’t exceeding TTL.
- **ALS stuck**: check VEML7700 address; verify I²C2 lines; confirm lux changes in logs/telemetry.
- **Fan always on/off**: check BME readings and thresholds; ensure PWM pin/channel are correct.

---

## 14) Extending / customizing

- **Security**: enable ESP‑NOW per‑peer encryption with LMKs once commissioning is stable.
- **Timing**: tune `DEVICE_TF_POLL_MS`, `AB_SPACING_MM`, and min/max clamp for `step_ms` to fit your segment length.
- **Wave style**: add an **all‑on pre‑flash** or per‑lane effects.
- **Telemetry**: push periodic ENV/LUX upstream on schedule for analytics.
- **RTC**: current implementation uses ESP32 internal RTC; add NTP sync if the ICM isn’t time‑authoritative.

---

## 15) Repository layout (key modules)

- `Device.h/.cpp` — RTOS controller & state machine
- `SensEspNow_*.cpp` — ESP‑NOW core/API/callbacks/payloads/topology/NVS/relay/wave
- `TFLunaManager.*` — TF‑Luna abstraction (A/B, presence helpers)
- `BME280Manager.*` — BME env sensor
- `VEML7700Manager.*` — ALS sensor & day/night
- `CoolingManager.*` — fan PWM & thermostat
- `SwitchManager.*` — user gestures & TF address wizard
- `BuzzerManager.*` — tones for pairing/config/faults
- `RTCManager.*` — ESP32 time wrapper (no external RTC)
- `main.h/.cpp` — full bring‑up + tiny entry point

---

**© EasyDriveWay / ICM — Smart Runway Light Sensor Node**
