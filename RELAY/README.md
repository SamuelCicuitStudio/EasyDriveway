# Relay Module Firmware (Boundary Relay)

**Relay Module** — an ESP32‑based dual‑channel driver for EasyDriveWay.  
Each relay sits **between two sensors** (A/B) and executes lighting schedules it receives from those sensors. It supports **split‑lane** operation (Left/Right channels), token‑based authentication over ESP‑NOW, and the **zero‑centered topology** managed by the ICM.

This document explains the hardware, the boundary topology it consumes, how it executes schedules, and how it cooperates with neighboring sensors and the ICM.

---

## TL;DR

- Two independent outputs (**Left**/**Right**) per relay for split traffic.
- Receives from ICM a **TopoRelayBoundary** record: the two boundary sensors (A/B) with tokens and a **split rule**.
- Accepts **commands only** from the ICM **or** from those **two sensors** (token + MAC check).
- Executes **time‑stamped schedules** per channel; queues, ACKs, TTL, and watchdog timeouts.
- Reports minimal telemetry (alive, temperature?, supply) to ICM.

---

## Hardware overview

- **MCU**: ESP32 module.
- **Outputs (2x):** choose a power stage per build:
  - DC MOSFET (PWM capable) for LED strips,
  - SSR / triac for AC loads,
  - or low‑side drivers into external power modules.
- **Supply**: 5–24 V in → buck to 3V3; per‑channel fuses recommended.
- **Protection**: TVS on supply, flyback protection for inductive loads.
- **Status LEDs**: **RUN** (alive), **L**, **R** (output state blips).
- **Optional sensors**: thermistor or digital temp for thermal derating, current sense for diagnostics.

> Physical “Left/Right” labels refer to the two independent channels. Actual lane mapping is provided by messages or by the split rule — wiring can stay consistent across installs.

---

## Boundary topology (what the relay receives)

ICM pushes a **TopoRelayBoundary** blob to each relay. Conceptually:

```jsonc
{
  "myIdx": 9,
  "hasA": true,
  "aSensIdx": 254,
  "aSensMac": "02:..:07",
  "aSensTok16": "<16b>",
  "hasB": true,
  "bSensIdx": 1,
  "bSensMac": "02:..:06",
  "bSensTok16": "<16b>",
  "splitRule": 0 // e.g. MAC_ASC_IS_POS_LEFT
}
```

- **A/B** are the nearest sensors on each side.
- **Tokens** allow the relay to authenticate schedule commands from those sensors.
- **splitRule** provides a deterministic mapping for compatibility packets that specify **side** as “positive/negative” rather than **Left/Right** (see below).

Relays don’t need the full network or section layout — just A, B, and the rule.

---

## Pairing & security

- ICM is the **master**: it pairs the relay, allocates a 16‑byte **relay token**, and later pushes A/B **sensor tokens** inside the boundary record.
- **Relay accepts a schedule only if ALL are true:**
  1. The message includes this relay’s **token** (to prevent third‑party control).
  2. The **sender MAC** matches **A** or **B** from boundary info.
  3. The message carries the **ICM master MAC** in its header.
- Topology and tokens are stored in NVS so the relay can boot standalone and still validate commands.

---

## Channel mapping & split rule

There are two ways the relay learns “which side” to drive:

1. **Explicit channels** (preferred): schedules specify `ch: "L"` or `"R"`. The relay drives that physical output and no mapping is needed.

2. **Compatibility side** (optional): older senders may specify `side: "+"|"-"` or `posNeg`. In that case the relay uses **splitRule** to map `+` to **Left** or **Right** deterministically. Example rule:
   - `MAC_ASC_IS_POS_LEFT (0)`: compare `aSensMac` vs `bSensMac`; if `a < b`, treat `+` as **Left**, else **Right**.

> Use mode (1) for new firmware (sensors choose L/R). Keep (2) for mixed fleets.

---

## Schedules (sensor → relay commands)

A schedule is a compact, time‑stamped list of steps for **one channel**. Binary on the wire; shown as JSON for clarity:

```jsonc
{
  "cmd": "sched",
  "icm": "ICM_MAC",
  "relayIdx": 9,
  "tok16": "<relayToken>",
  "baseTs": 123456789, // synchronized timebase
  "ch": "L", // "L" or "R" (or omitted if using side:+/-)
  "steps": [
    { "at": 0, "dur": 150 }, // ON window 1
    { "at": 180, "dur": 150 }, // ON window 2
    { "at": 360, "dur": 150 } // ON window 3
  ],
  "ttl": 2000 // discard if time has passed
}
```

**Execution rules**

- Relay maintains two **per‑channel queues** (L/R). Steps are relative to `baseTs` (monotonic ticks).
- If a new schedule arrives for a channel:
  - If it **starts later** than the current one, it’s **queued** (or replaces, depending on config).
  - If it **overlaps** or is **newer** (greater `baseTs`), **newer wins** for that channel.
- Schedules for **L** and **R** are independent and can overlap (two cars meeting).
- A **watchdog** clears output if no step triggers for a configured idle period.
- Relay **ACKs** schedule acceptance to the sender; errors include bad token, bad sender, expired TTL.

---

## Timebase & drift

- ICM periodically emits a time beacon; relay slews its local counter to keep `baseTs` coherent with sensors.
- Relay tolerates small deltas; a large skew causes the relay to **reject** schedules (expired) and request a resync from ICM.

---

## Telemetry

- Optional periodic report to ICM:
  - `{ rIdx, fw, temp?, vIn?, lastErr, qL, qR }`
- Faults (over‑temp, brown‑out) can trigger a **soft‑off** and a recovery backoff.

---

## NVS keys (relay module)

Indicative set; align with your `ConfigManager`:

- `R_IDX` — this relay index
- `R_AMAC`, `R_BMAC` — boundary sensor MACs
- `R_ATK`, `R_BTK` — A/B 16‑byte tokens
- `R_SPLT` — split rule byte
- `PIN_L`, `PIN_R` — physical output pins
- `PWM_L`, `PWM_R` — enable PWM (if DC LED)
- `WDTMS` — idle watchdog (ms)
- `DRVMD` — driver mode (MOSFET/SSR/TTL)
- Telemetry thresholds: `TMAX`, `VINMIN`

---

## Boot sequence

1. Initialize NVS & load boundary/topology.
2. Join ESP‑NOW (channel from NVS); wait for ICM beacon.
3. Apply **safe state** (outputs off).
4. Accept boundary updates from ICM (optional) and confirm via ACK.
5. Start schedule engine; begin telemetry if enabled.

---

## Interop with sensors & ICM

- **From ICM**: boundary updates, timebase beacons, global overrides (e.g., “all off”).
- **From sensors A/B**: schedules for **Left/Right** (or `side:+/-` in compatibility mode).
- **To ICM**: ACK/NACK for topology & schedules, telemetry.

> The relay **does not** talk directly to other relays; coordination happens via sensor timing and ICM timebase.

---

## Test procedure

1. Pair relay with ICM and verify it appears in `/api/peers/list`.
2. Apply topology so relay gets **A/B and splitRule** (`/api/topology/get`).
3. Send a test schedule for **Left** channel (from UI or sensor emulation) → observe output.
4. Send overlapping **Right** channel schedule → verify concurrent execution.
5. Verify token refusal: try to send from a non‑boundary MAC or with a wrong token → expect NACK.
6. Check watchdog clears outputs after idle period.

---

## Troubleshooting

- **“Schedule rejected (token)”**: relay token mismatch or not provisioned; re‑push topology from ICM.
- **“Sender not allowed”**: sender MAC isn’t A or B for this relay.
- **Output flicker**: PWM mode at too low frequency; increase or disable PWM for AC SSRs.
- **Missed steps**: timebase skew too high or TTL too low; resync and increase TTL.
- **Channel swapped**: if using `side:+/-`, adjust `splitRule`; or switch to explicit `ch: "L"/"R"` in schedules.

---

## Roadmap

- Per‑channel ramp profiles (fade in/out) and brightness levels.
- Queue merging policy (append vs replace) configurable per channel.
- Over‑the‑air FW via ICM.

---

## License

Proprietary — © ICM / EasyDriveWay
