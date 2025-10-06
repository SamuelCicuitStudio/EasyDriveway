# Driveway Lighting System — README (ICM_Nw / Now_API aligned)

> ESP‑NOW, token‑gated network with a central **ICM** and modules: **PMS** (Power), **Sensors** (dual ToF + env), and **Relays**. Directional motion triggers an auto‑follow lighting wave. This README supersedes older "EasyDriveWay" notes and matches the current headers and sources.

---

## 1) What this project is

**Driveway Lighting System** detects presence + direction and lights driveway segments in a smooth wave. The ICM coordinates everything over ESP‑NOW, mirrors telemetry for a UI/API, and pushes topology/config to nodes.

**Modules**

- **ICM (Interface / Coordinator Module)** — orchestrates ESP‑NOW, pairing, time sync, config/topology push, live mirrors, and OTA.
- **PMS (Power Management System)** — powers the rails (e.g., 48 V + 5 V), reports volts/amps/temperature, handles alarms and source selection.
- **Sensors** — dual ToF (A/B) for direction + BME/VEML environment; initiate & hand off the wave in Auto mode.
- **Relays** — actuate LEFT/RIGHT channels per commands from Sensor (Auto) or ICM (Manual/tests).

Target: outdoors (–25 °C…+55 °C, high humidity). Surge‑protected I/O. 5‑year MTBF design goal.

---

## 2) High‑level architecture

```
        Mobile/Web UI (future)
            ▲  Wi‑Fi (ICM STA/AP)
            │
┌────────────┴───────────────────────────────────┐
│                ICM (ESP32‑S3)                  │
│  • ESP‑NOW master (token = 32‑hex)             │
│  • Pairing: PMS / Relays / Sensors             │
│  • Topology (JSON) push + orchestration        │
│  • Time sync, logging, OTA                     │
│  • Live mirrors (power, sensors, relays)       │
└───────┬──────────────┬──────────────┬──────────┘
        │              │              │   ESP‑NOW (fixed channel)
   ┌────▼───┐     ┌────▼───┐     ┌────▼───┐
   │  PMS   │     │ Relays │     │Sensors │
   │ Power  │     │ 1..N   │     │ ToF+Env│
   └────────┘     └────────┘     └────────┘
```

---

## 3) Core behaviors

### Auto‑follow wave (directional)

1. Sensor detects motion + **direction** (ToF A/B edges).
2. Drives its **per‑direction relay list** (Left/Right/Both with duration, optional ramp).
3. Hands off to **Prev/Next neighbor** to continue the wave.
4. End sensors terminate or forward to special nodes (Entrance/Parking).

### Power & telemetry (PMS)

- Reports `vbus_mV, vbat_mV, ibus_mA, ibat_mA, temp_c_x100, flags`.
- ICM can `PWR_ON_OFF`, select source, clear flags, and query status.

### Modes & indicators

- **Auto**: sensors drive relays; ICM limits indicators to alerts.
- **Manual/Test**: ICM can directly set relays and indicators for diagnostics.

---

## 4) Communication protocol (Now_API.h)

**Header:** `NowMsgHdr{ ver, dom, op, ts_ms, seq, token_hex[32] }` — 32 ASCII hex (no NUL on wire).

**Domains:** `SYS, PWR, REL, SEN, TOP, FW, LOG, IND, PR, CFG`.

**Ops (high‑level):**

- `SYS`: `HB, MODE_SET, PING, SET_CH, TIME_SYNC, STATE_EVT`
- `PWR`: `REP, ON_OFF, SRC_SET, CLR_FLG, QRY`
- `REL`: `REP, SET, ON_FOR, SCHED, QRY`
- `SEN`: `REP, TRIG`
- `TOP`: `PUSH_SEN_JSON, PUSH_RLY_JSON`
- `FW/LOG/IND/PR/CFG`: OTA, logging, indicators, pairing, and config

**Acking:** No application‑layer ACKs; rely on hardware ACK and explicit reports/queries.

---

## 5) Pairing & security

- **Admission token**: 32‑char lowercase hex; required in every header (except `PR_DEVINFO` during bootstrap).
- **Web‑UI pairing**: ICM assigns token/channel/ICM‑MAC via `PR_ASSIGN` → `PR_COMMIT`; supports `PR_REKEY`, `PR_UNPAIR`.
- **Auto‑pair**: unknown node sends `PR_DEVINFO`; ICM allocates a slot and responds with assign/commit.
- **Optional**: future session HMAC; signed OTA images.

---

## 6) Topology (JSON only)

- **Sensor‑centric JSON**: Prev/Next neighbor sensors + relay lists for **POS/NEG** direction.
- **Relay boundary JSON**: boundary sensors A/B and a split rule → consistent Left/Right normalization.
- ICM holds the **authoritative mirror** and pushes updates down (`TOP_PUSH_*_JSON`).

---

## 7) Telemetry mirrors (ICM)

- `sensLive[idx] ← SEN_REP` → `{t_c_x100, rh_x100, p_Pa, lux_x10, is_day, tfA_mm, tfB_mm}`
- `relayLive[idx] ← REL_REP` → `{temp_c_x100, state_flags}`
- `pmsLive     ← PWR_REP` → `{vbus_mV, vbat_mV, ibus_mA, ibat_mA, temp_c_x100, flags}`

---

## 8) Build & flash

- **Tooling**: PlatformIO or Arduino‑ESP32.
- **Targets**: ESP32‑S3 (ICM), ESP32/‑S3 (others).
- **Steps**:

  1. Configure ESP‑NOW **channel**, Wi‑Fi/AP (optional), and device roles.
  2. Flash PMS and verify hardware hooks for V/I/Temp.
  3. Flash Relays and Sensors; confirm indicators and basic self‑report.
  4. Power PMS → ICM; pair devices; push topology JSON.
  5. Trigger tests: `PWR_QRY`, `REL_ON_FOR`, `SEN_TRIG` and verify wave.

---

## 9) Configuration (NVS)

- Common: `net:chan, net:icmmac, esp:token, mode:op, time:*`
- Sensor: `topo:prev*, topo:next*, topo:posrl, topo:negrl`
- Relay: `bnd:sAmac, bnd:sBmac, bnd:split`
- PMS: power thresholds & policies
- ICM: registry mirrors and global `topo:str`

---

## 10) Security hardening

- No default Wi‑Fi credentials; throttle UI login attempts.
- Token‑gated traffic; token mismatch is dropped silently.
- Local/USB firmware service recommended for field updates; keep devices off public networks.

---

## 11) Repository layout

```
/Now_API.h              # shared protocol (domains/ops/payloads)
/ICM_Nw.h               # core + role helpers (ICM/PMS/SEN/REL)
/ICM_NwCore.cpp         # network core, registry, send helpers
/ICM_NwModule.cpp       # per‑domain dispatchers, pairing flows
/README.md              # this file
/Docs/Driveway_Spec.md  # drop‑in spec (client‑facing)
```

---

## 12) Roadmap

- Mobile/Web UI for monitoring & manual control
- Time‑based schedules (RTC/NTP)
- Bluetooth provisioning (optional)
- Extended effects + seasonal thresholds
- Diagnostics: richer BITE and self‑tests

---

## 13) License

Choose a license (MIT/Apache‑2.0).
