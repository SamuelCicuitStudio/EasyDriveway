# Sensor Module Firmware (Presence Board) — HW Profile: TF-Luna ×2 + BME + VEML7700 + Buzzer + Fan

ESP32-based **presence/direction** node for EasyDriveWay.
This hardware profile uses:

- **2× Benewake TF-Luna** (A/B distance probes)
- **1× BME** (BME280/BME680) for **temperature, humidity, pressure**
- **1× VEML7700-TR** for **ambient light** (day/night)
- **Buzzer** (events/alerts) and **Fan** (thermal management)

The node participates in the ESP-NOW mesh managed by ICM, consumes a **zero-centered topology**, and sends **token-gated** schedules to relays.

---

## TL;DR

- Two **TF-Luna** modules provide **A/B distances (mm)** used to detect **presence, direction (A→B / B→A), speed**, **stop/resume**.
- **BME** supplies `tC`, `rh`, `p_Pa` (°C, %RH, Pa).
- **VEML7700** provides `lux`; day/night derived in firmware using configured thresholds.
- **Buzzer** signals pairing, faults, and optional motion events; **Fan** auto-controls on thermal thresholds.
- Sensor reports are exposed via the same UI/debug endpoints the app polls every 1 s.

---

## Hardware Overview

### Sensors

- **TF-Luna x2** (A & B):

  - **Interface:** UART (default 115200 8N1) or I²C (if your module/bridge supports it).
  - **Outputs (per sensor):** distance **mm**, signal strength (amplitude), and a quality/valid bit.
  - **Mounting:** A and B are placed along the traffic flow (≈25–50 cm apart). Direction derives from which probe trips first.

- **BME** (BME280/BME680):

  - **Interface:** I²C (typ. `0x76` or `0x77`), SPI optional.
  - **Outputs:** temperature (°C), humidity (%), pressure (Pa).

- **VEML7700-TR**:

  - **Interface:** I²C (`0x10`).
  - **Outputs:** ambient light (lux). Firmware applies day/night thresholds with hysteresis.

### Actuators

- **Buzzer:** driven by GPIO (optional short PWM beeps).
- **Fan:** on/off or PWM pin; governed by temperature thresholds (with hysteresis).

> Provide your exact pins in `hw_pins.h` (UART RX/TX for each TF-Luna, I²C SDA/SCL, buzzer, fan).

---

## Zero-Centered Topology (ICM → Sensor)

Sensor receives a **TopoZeroCenteredSensor** from ICM:

```jsonc
{
  "sensIdx": 7,
  "hasPrev": true,
  "prevIdx": 6,
  "prevMac": "02:..:A6",
  "prevTok16": "<16b>",
  "hasNext": true,
  "nextIdx": 8,
  "nextMac": "02:..:A8",
  "nextTok16": "<16b>",
  "neg": [
    { "relayIdx": 12, "pos": -1, "relayMac": "02:..:C1", "relayTok16": "<16b>" }
  ],
  "pos": [
    { "relayIdx": 13, "pos": +1, "relayMac": "02:..:C2", "relayTok16": "<16b>" }
  ]
}
```

- `neg[]`: ordered relays to the **left** (−1 nearest)
- `pos[]`: ordered relays to the **right** (+1 nearest)
- Each neighbor/relay carries a **16-byte token** for authenticated ESP-NOW commands.

Relays also receive **boundary links** so they can drive **Left/Right** outputs independently.

---

## Pairing & Security

- ICM pairs each peer (sensor/relay) and issues tokens.
- Sensor **stores topology + tokens** in NVS.
- All sensor→relay/sensor→sensor frames include ICM MAC, sender/recipient, and **recipient token** (see Command API in firmware).

---

## Runtime Logic (This Hardware)

### A/B motion & direction (TF-Luna)

- Each loop captures **`distA_mm`, `ampA`, `okA`** and **`distB_mm`, `ampB`, `okB`**.
- Presence triggers when distance crosses a **range gate** (configurable `TF_NEAR_MM … TF_FAR_MM`) and `ok*` is true.
- **Direction** is derived from first-to-trigger ordering (A then B → A→B).
- **Speed** from known A↔B spacing and Δt.
- **Stop/Hold** if distance stabilizes in-range for `stop_timeout_ms`.

### Environment (BME)

- Read **`tC`**, **`rh`**, **`p_Pa`** each cycle.
- Firmware may optionally low-pass filter or median filter readings.

### Day/Night (VEML7700)

- Read **`lux`**, compare to thresholds:

  - `ALS_T0` (day→night down-cross), `ALS_T1` (night→day up-cross) with hysteresis.

- Export `is_day = 1|0`.

### Buzzer & Fan

- **Buzzer:** short patterns for **pair OK**, **topology received**, **fault** (sensor read timeout, token mismatch).
- **Fan:** governed by temperature:

  - `FAN_T_ON` (°C) to engage; `FAN_T_OFF` (°C) to disengage.
  - If PWM, clamp duty between `FAN_PWM_MIN…FAN_PWM_MAX` based on `tC`.

---

## Firmware ↔ UI (Debug Proxies)

The UI polls once per second when this sensor is selected:

- `POST /api/sensor/env { mac }` →
  `{ ok:true, tC: 28.3, rh: 51.2, p_Pa: 100870, lux: 320, is_day: 1 }`
- `POST /api/sensor/tfraw { mac, which: 2 }` →
  `{ ok:true, distA_mm: 740, ampA: 182, okA: 1, distB_mm: 1180, ampB: 175, okB: 1 }`
- (Fallback for local mock) `POST /api/sensor/read { mac }` →
  `{ tempC, humidity, pressure_hPa, lux, tf_a, tf_b }`

> If your firmware exposes only ESP-NOW, the ICM bridge should **map your telemetry** to the fields above so the UI renders `Temp/Humidity/Pressure/Lux` and `TF-A/TF-B`.

---

## Schedules (Sensor → Relay)

On **ACTIVE**:

- Build per-relay timelines toward `pos[]` (**forward**) or `neg[]` (**reverse**).
- Initial **all-on flash** then step **+1→+N** (or **−1→−N**).
- HOLD freezes; RESUME continues.
- Binary packet carries `{ relayIdx, tok16, baseTs, steps[], ttl }`.
- Relays verify token, queue, and ACK; late packets are dropped.

---

## NVS Keys (this profile)

- `S_IDX` — sensor index (0..N, 254=ENTR, 255=PARK)
- `S_PIDX`/`S_NIDX`, `S_PMAC`/`S_NMAC`, `S_PTK`/`S_NTK`
- `Z_NEG`, `Z_POS` — local relay map (serialized entries with `{idx,pos,mac,tk}`)
- `ALS_T0`, `ALS_T1` — lux thresholds (hysteresis)
- `TF_NEAR_MM`, `TF_FAR_MM`, `CONFIRM_MS`, `STOP_MS`
- `FAN_T_ON`, `FAN_T_OFF`, `FAN_PWM_MIN`, `FAN_PWM_MAX`
- `BUZ_EN`, `BUZ_VOL` (if applicable)

---

## Recommended Defaults

- `CONFIRM_MS`: **140 ms** (TF-Luna)
- `STOP_MS`: **1200 ms**
- `A_B_SPACING_MM`: **350 mm**
- `ALS_T0/T1 (lux)`: **180 / 300** (tune for your site)
- `FAN_T_ON/OFF (°C)`: **55 / 45** (enclosure & fan dependent)

---

## LEDs & Diagnostics

- **RUN:** heartbeat; rapid during pairing
- **TRG:** pulses on A/B triggers; steady while HOLD
- UART optional: logs A/B edges, direction, speed, env telemetry, fan state

---

## Test Checklist

1. Pair with ICM; verify `/api/topology/get` shows `neg/pos` and neighbor tokens.
2. Move a target past **A then B** → **A→B** direction & forward schedule (pos\[]) fires.
3. Move **B then A** → reverse schedule (neg\[]).
4. Stop between A/B → HOLD; moving again resumes.
5. Shade ALS → `is_day` flips; UI shows Day/Night.
6. Heat board past `FAN_T_ON` → fan engages; cool below `FAN_T_OFF` → off.
7. Buzzer beeps on pair/topology; fault pattern on sensor timeout (if enabled).

---

## Troubleshooting

- **No A/B distances:** check TF-Luna UART wiring/baud (or I²C mode).
- **Env missing:** verify BME address (`0x76/0x77`) and pull-ups.
- **Lux 0:** check VEML7700 address (`0x10`) and integration time.
- **UI shows “—”:** ensure the bridge returns the expected fields for `/api/sensor/env` and `/api/sensor/tfraw`.
- **Fan always on:** lower `FAN_T_ON`/raise `FAN_T_OFF` hysteresis; verify temp source (BME vs. internal).

---

## License

Proprietary — © ICM / EasyDriveWay

---
