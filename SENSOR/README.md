# Sensor Module (Presence/Direction Node) — TF-Luna ×2 + BME280 + VEML7700 + Buzzer + Fan

ESP32-based sensor node that detects motion and direction on a single section, computes speed from two ToF probes, and drives nearby relays in a chaser pattern (**1-2-3** or **3-2-1**) via ESP-NOW. The node also reports environment (temp/humidity/pressure), ambient light (lux, day/night), and manages its own cooling. It participates in an ICM-managed mesh and persists its topology & tokens to NVS.&#x20;

---

## Hardware profile (this build)

- **TF-Luna ×2** on **I²C1** for presence/direction/speed
  SDA = **GPIO4**, SCL = **GPIO5** (addresses configurable by the board; see provisioning below)
- **BME280** on **I²C2** for environment (temp °C, humidity %, pressure Pa)
  SDA = **GPIO16**, SCL = **GPIO17**
- **VEML7700-TR** on **I²C2** for ambient light (lux → day/night)
  Address typically `0x10` (configurable)&#x20;
- **Buzzer** on **GPIO11** for user feedback (pairing, provisioning, faults)
- **Fan (PWM)** for enclosure cooling (same pin as before; PWM controlled by CoolingManager)&#x20;
- RGB status LED pins unchanged from previous board

> Storage: external flash “MKDV8GIL” (unchanged pins), used by logging/NVS as before.

---

## What the board does (at a glance)

1. **Detects traffic:** reads TF-Luna **A/B** distances, derives **direction** (A→B or B→A), **speed** (Δx/Δt), and **stop/resume**.
2. **Drives relays:** uses the **ICM-pushed topology** to find relays on its **negative (−1, −2, …)** and **positive (+1, +2, …)** sides and sends token-authenticated commands to play a wave (**1-2-3** or **3-2-1**) with the correct per-relay delay based on speed.
3. **Shares wave headers:** optionally sends neighbors a compact **wave header** (lane, dir, speed, ETA) so they can pre-arm instead of re-computing.
4. **Reports environment & light:** BME280 → temp/humidity/pressure; VEML7700 → lux with day/night hysteresis. &#x20;
5. **Manages cooling:** CoolingManager reads BME temperature and sets PWM duty to the fan according to thresholds/hysteresis (AUTO or manual).&#x20;
6. **Gives feedback:** buzzer beeps on pairing/topology receipt, provisioning steps, and fault conditions.

---

## Topology (what the sensor receives from ICM)

- **Neighbors:** `hasPrev/prevIdx/prevMac/prevTok16` and `hasNext/nextIdx/nextMac/nextTok16` (stored in NVS as compact keys).
- **Local relays:** two ordered lists:

  - `neg[]` → **left** side (−1 nearest), each with `{ relayIdx, pos, relayMac, relayTok16 }`
  - `pos[]` → **right** side (+1 nearest), same fields

- The class persists MACs/tokens and adds peers to ESP-NOW, so later sends are one-liners that automatically use the **receiver’s token** (relay or neighbor). (See _Relay control_ and _Neighbor wave handoff_ below.)

---

## Relay control (sensor → relay)

When the board detects motion, it builds a wave based on **direction** and **speed**:

- Direction **A→B** → go to **`pos[]`** and play **1-2-3** (nearest to farthest).
- Direction **B→A** → go to **`neg[]`** and play **3-2-1** (farthest to nearest).
- Step interval: `step_ms = spacing_mm / speed_mmps`, clamped for smoothness (e.g., 80..300 ms).
- Optional “all-on” flash before stepping, to make the wave feel snappier.

The sensor sends a centralized “turn on for duration” command to each relay with a per-relay `delay_ms` that creates the chaser pattern. (A helper fans this out across the side lists using the stored MACs and tokens, so you don’t hand-assemble frames.)

---

## Neighbor wave handoff (sensor → sensor)

Upon confirming direction/speed, the board can ping **prev/next** sensor with a small header containing:

- `lane` (0=Left, 1=Right), `dir` (+1 toward +side / −1 toward −side)
- `speed_mmps`, and `eta_ms` (to the neighbor boundary)

This lets the neighbor prepare its own wave immediately, without waiting to re-estimate speed locally.

---

## TF-Luna address provisioning (double/triple-tap wizard)

To simplify wiring and keep addresses consistent:

- **Double-tap** the user button → “**Assign A**” wizard
  Expect **exactly one** TF-Luna connected on I²C1; the board scans, assigns the NVS-configured **A address**, saves, and beeps OK (or error).
- **Triple-tap** the user button → “**Assign B**” wizard
  Same procedure for the **B address**.

You can connect the first sensor, double-tap to make it **A**, then connect the second and triple-tap to make it **B**. The buzzer provides entry/success/error tones during the procedure.

---

## Environment sensing (BME280)

- The **BME280Manager** provides `tC`, `rh`, and `p_Pa`.
- Cooling and telemetry use this data; CoolingManager’s AUTO mode adjusts fan PWM vs. temperature thresholds with hysteresis.&#x20;

---

## Ambient light (VEML7700-TR)

- The **VEML7700Manager** initializes the ALS on **I²C2** using pins/addr from NVS, reads **lux**, and computes **day/night** with hysteresis (`ALS_T0/ALS_T1`).
- You can fetch current lux and a debounced day/night state for UI or brightness profiles.&#x20;

---

## Cooling (fan PWM) logic

- **CoolingManager** runs as an RTOS task, reads **BME** temperature, and selects **ECO / NORMAL / FORCED / STOPPED / AUTO** modes.
- In **AUTO**, it applies thresholds like `COOL_TEMP_ECO_ON_C`, `COOL_TEMP_NORM_ON_C`, `COOL_TEMP_FORCE_ON_C`, with **hysteresis**, mapping to preset duty cycles (e.g., 30/60/100%).
- It logs changes at a throttled cadence and exposes last temperature/pressure/humidity and applied duty.&#x20;

---

## UI & polling

- The UI (or ICM bridge) typically polls this sensor every **1 s** when selected:

  - **Environment:** `{ tC, rh, p_Pa, lux, is_day }`
  - **ToF raw:** `{ distA_mm, ampA, okA, distB_mm, ampB, okB }`

- The board can also push or expose summarized state for quick diagnostics.&#x20;

---

## Commissioning checklist

1. **Pair** with ICM so the sensor knows its master and tokens.
2. **Provision TF-Luna addresses:** connect one sensor at a time on I²C1 → **double-tap** (A), **triple-tap** (B); buzzer confirms.
3. **Verify topology** was received (neighbors + relay lists).
4. **Walk test:** move target A→B and B→A; verify chaser plays on the correct side with coherent speed.
5. Cover/uncover ALS to confirm **day/night** transitions; warm the enclosure to see **fan** engage.

---

## Troubleshooting

- **No ToF data:** verify only one unit is connected during provisioning; confirm I²C1 pull-ups and addresses; try re-assigning A/B.
- **Relays don’t light:** check topology (this sensor’s `neg/pos` lists), token mismatch, or timebase skew (late packets dropped).
- **Fan always on/off:** check thresholds/hysteresis and that BME is healthy; verify PWM pin and LEDC channel.&#x20;
- **Lux stuck:** confirm VEML7700 address and I²C2 wiring; integration settings in ALS driver.&#x20;

---

## Source modules in this repo

- **VEML7700Manager** — ALS init, read, and day/night helper (I²C2)&#x20;
- **CoolingManager** — fan PWM control with BME-based thermostat (RTOS task)&#x20;
- **(This README baseline)** — earlier profile summary retained & expanded here&#x20;

---

**Proprietary — © ICM / EasyDriveWay**
