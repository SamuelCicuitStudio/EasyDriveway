# Sensor Module Firmware (Presence Board)

**Sensor Module** — an ESP32‑based presence/direction board for EasyDriveWay.  
It detects motion on **two half‑lanes (Left/Right)**, infers **direction**, **speed**, **stop/resume**, and drives the lighting “wave” by sending schedules to the relays between itself and its neighboring sensors. It participates in the ICM‑managed ESP‑NOW mesh using the **zero‑centered topology**.

This document explains hardware layout, firmware behavior, the topology it consumes from ICM, and how it communicates with relays and neighboring sensors.

---

## TL;DR (what this board does)

- Measures motion **independently per lane** (Left & Right) using two longitudinal sensing points (A/B) per lane.
- Computes **direction** (A→B or B→A), **speed** (`distance_AB / Δt`), **stop**, **resume**, **reversal**.
- Consumes **topology** pushed by ICM:
  - its **neighbor sensors** (prev/next) with tokens,
  - its **local zero‑center relay map** (`neg[] = −1, −2, …`, `pos[] = +1, +2, …`).
- Sends **time‑stamped schedules** directly to the relays in its section; relays execute with **left/right outputs**.
- Talks with **neighbor sensors** so waves propagate and split for two cars in opposite directions.
- Reports health/telemetry to ICM; stores tuning in NVS.

---

## Hardware overview

Two recommended sensing options; keep one per board:

### Option A — Magnetic (robust, outdoor‑proof)

- 4 × digital Hall sensors (built‑in hysteresis), arranged as **two pairs** per lane:
  - **Left lane:** L‑A (upstream), L‑B (downstream)
  - **Right lane:** R‑A (upstream), R‑B (downstream)
- **A↔B spacing:** 25–50 cm (pick and keep consistent for speed math)
- Pros: works in sun/rain, no optics; Cons: choose proper mounting height.

### Option B — Time‑of‑Flight (precise)

- 4 × VL53L1X (or similar) with short hood; 2 per lane (A/B).
- Pros: precise presence/stop; Cons: optics care, I²C addressing/mux.

**Common parts**

- ESP32 module, 5–12 V input → 3V3 buck, ESD on sensor lines.
- Ambient Light Sensor (ALS) for Day/Night.
- Optional temperature sensor.
- 2 status LEDs: **RUN** (alive), **TRG** (event).

---

## Zero‑centered topology (what the board receives)

ICM pushes a **TopoZeroCenteredSensor** blob to each sensor. Conceptually:

```jsonc
{
  "sensIdx": 1,
  "hasPrev": true,
  "prevIdx": 254,
  "prevMac": "02:..:07",
  "prevTok16": "<16 bytes>",
  "hasNext": true,
  "nextIdx": 2,
  "nextMac": "02:..:08",
  "nextTok16": "<16 bytes>",
  "neg": [
    { "relayIdx": 9, "pos": -1, "relayMac": "02:..:12", "relayTok16": "<16b>" }
  ],
  "pos": [
    { "relayIdx": 10, "pos": +1, "relayMac": "02:..:13", "relayTok16": "<16b>" }
  ]
}
```

- `sensIdx`: numeric ID (anchors: **254=ENTRANCE**, **255=PARKING**).
- `neg[]`: ordered relays to the **left** (−1 nearest).
- `pos[]`: ordered relays to the **right** (+1 nearest).
- Each neighbor and relay comes with a **16‑byte token** for secure ESP‑NOW commands.

Relays separately receive **boundary** info (from ICM) so they know the two sensors A/B around them and how to split **left/right** outputs.

---

## Pairing & security

- ICM is the **master**. Each peer (sensor/relay) pairs and gets a **token**.
- Topology and tokens are **pushed** to the sensor; the sensor stores them in NVS.
- All sensor→relay and sensor→sensor frames include:
  - **ICM master MAC**, **sender MAC**, **recipient MAC/idx**, and the **token** of the recipient.
  - Minimal authenticated fields to prevent spoofing (exact details per CommandAPI).

---

## Runtime behavior (state machine)

Per **lane** (Left/Right) the board runs:

```
IDLE
  └─(A or B crosses threshold)→ CANDIDATE(dir=A→B|B→A, t0)
      ├─(confirm within confirm_window_ms)→ ACTIVE(dir, speed_mps, t0)
      │      ├─(no movement for stop_timeout_ms)→ HOLD(dir)  // vehicle stopped
      │      ├─(reversal detected via untrigger order)→ ACTIVE(dir'=reversed, new t0)
      │      └─(exit sensor zone)→ DONE
      └─(no confirm)→ IDLE
```

**Speed** is computed from the known A↔B spacing and Δt on first pass.  
**Stop** is entered if the second sensor does not confirm within timeout or the distance reading stabilizes (ToF).

### Two cars, opposite directions

- The **Left lane** and **Right lane** are **independent**. You may have **two simultaneous waves**.
- The “meeting point” is handled naturally: relays drive **left** and **right** channels separately.

---

## Scheduling relays (sensor → relay commands)

When a lane enters **ACTIVE**, the sensor builds a schedule for its section:

- If **dir = forward** (toward `pos[]`): all relays in `pos[]` go **all‑on** briefly, then step **+1 → +N** (“1‑2‑3”).
- If **dir = reverse** (toward `neg[]`): mirror using `neg[]` (“3‑2‑1”).
- **Timing** derives from `speed_mps` and inter‑relay spacing (configurable).
- **Stop/Hold**: freeze current state; **Resume**: continue where left off.

A compact command is sent to each relay in the section with per‑channel timing:

```jsonc
{
  "cmd": "sched",
  "icm": "ICM_MAC",
  "relayIdx": 10,
  "tok16": "<relayToken>",
  "baseTs": 123456789, // ICM or local timebase
  "steps": [
    { "at": 0, "dur": 120, "ch": "L" }, // Left channel on at t0 for 120ms
    { "at": 120, "dur": 120, "ch": "L" },
    { "at": 240, "dur": 120, "ch": "L" }
  ],
  "ttl": 2000
}
```

- `ch`: `"L"` or `"R"` (split‑lane). If both lanes use the same relay concurrently, each channel gets its own timeline.
- Relays queue/execute with token verification and ACK; late packets (beyond `ttl`) are dropped.

> Exact packet layout is binary in firmware; JSON shown for readability.

---

## Sensor ↔ sensor communication

- On **first trigger** in a global direction, entrance/parking sensors propagate a **dir beacon** downstream.
- Each sensor also gossips **lane wave headers** (`{lane, dir, speed, eta}`) to the **next sensor** so it can pre‑arm schedules, improving perceived continuity.
- **Reversal** warning is sent when the board detects A/B **un‑trigger** order incompatible with current `dir`.

---

## Interaction with ICM

- **Receives**: topology updates, timebase beacons, configuration, over‑the‑air (future), and pairing management.
- **Sends**: health/telemetry (`{rssi, vbat?, temp?, events[]}`), optional debug traces.
- **Persists**: topology and tokens in NVS for cold boot.

---

## NVS keys (sensor module)

Short key names are used to fit Preferences constraints. Typical keys:

- `S_IDX` — this sensor index (0..N, 254=ENTR, 255=PARK)
- `S_PIDX`, `S_NIDX` — prev/next sensor indices
- `S_PMAC`, `S_NMAC` — prev/next MAC strings
- `S_PTK`, `S_NTK` — prev/next 16‑byte tokens (binary)
- `Z_NEG`, `Z_POS` — serialized arrays of local relay entries (`{idx,pos,mac,tk}`)
- `ALS_T0`, `ALS_T1` — day/night thresholds
- `H_DEB`, `H_WIN` — debounce and confirm window (ms)
- `H_STOP` — stop timeout (ms)
- `LANE_SP` — A↔B spacing (mm)

Names are indicative; align with your `ConfigManager` if different.

---

## Configuration parameters (recommended defaults)

- `confirm_window_ms`: **120 ms** (Hall) / **160 ms** (ToF) — must see A then B (or B then A)
- `stop_timeout_ms`: **1200 ms** — if second sensor doesn’t confirm, treat as stop
- `reverse_hysteresis_ms`: **250 ms** — to switch direction after stop
- `all_on_ms`: **180 ms** — initial “all on” flash for the section
- `step_ms`: derived from speed; clamp between **80..250 ms**
- `lane_spacing_cm`: **35 cm** — A↔B
- `relay_spacing_m`: **1.5 m** (used with speed to derive cadence)

---

## LED & diagnostics

- **RUN**: heartbeat (alive), rapid blink during pairing.
- **TRG**: blips on edge detection; steady while HOLD.
- Optional UART debug: prints lane state changes and computed speed.

---

## Test procedure

1. Pair with ICM, confirm topology in `/api/topology/get` (your `zc` has correct `neg/pos`).
2. Walk a magnet/vehicle on **left lane** A→B; observe **left** channels advancing +1,+2,…
3. Walk the opposite on **right lane**; observe independent **right** channels.
4. Stop mid‑section → lights hold. Resume → schedule continues.
5. Cross two lanes in opposite directions → waves split and pass.

---

## Troubleshooting

- **No reaction**: confirm this sensor received `TopoZeroCenteredSensor` (ICM push), and tokens exist.
- **Some relays dark**: check that those relays appear in your `neg/pos` lists and that their boundary info references this sensor.
- **Wave stutters**: raise `confirm_window_ms` or adjust debounce.
- **Two cars desync**: ensure relays support independent L/R scheduling; verify split rule on the boundary entry.

---

## Roadmap

- Multi‑hop pre‑arming (sensor pre‑sends tentative schedules to next‑next section for ultra‑smoothness).
- Lane occupancy maps for adaptive brightness.
- OTA tuning of debounce and timing per device.

---

## License

Proprietary — © ICM / EasyDriveWay
