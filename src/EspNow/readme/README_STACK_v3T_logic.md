# EasyDriveway ESPNOW Stack — v3‑T (Token‑Only) Logic & Implementation Guide

> **Drop‑in replacement logic for the previous HMAC profile.**  
> This document fully describes the **runtime logic**, **module interfaces**, **state machines**, and **message flows** of the EasyDriveway ESPNOW stack in **token‑only** mode. It is deliberately long and explicit so it can serve as the canonical implementation reference for firmware developers across all roles (ICM, PMS, REL, SENS, REMU, SEMU).

---

## 0. Motivation & Scope

- **Motivation.** We want a compact, deterministic ESPNOW stack where **admission and authorization** are enforced only via **128‑bit tokens**. We intentionally **remove HMAC signing** and any cryptographic integrity protection to reduce code size, CPU time, and maintenance burden for constrained nodes.
- **Scope.** This file covers:
  - module decomposition,
  - data types and wire layout,
  - lifecycle of nodes,
  - routing & scheduling,
  - timers, retries, backoff,
  - pairing, topology distribution, and token rotation,
  - role adapters (ICM/PMS/REL/SENS/REMU/SEMU),
  - firmware update flows (CRC‑based only),
  - configuration mechanics,
  - diagnostics and testability,
  - migration from v2H to v3‑T.
- **Non‑goals.** Confidentiality, replay protection, and message integrity are **not** provided by this profile. If those are required, re‑enable the former HMAC trailer or run ESPNOW encrypted peers with per‑peer keys and counters outside of this spec.

---

## 1. Design Overview

### 1.1 Objectives

1. **Role‑agnostic core.** The same transport, headers, and dispatcher serve all six roles. Role logic is plugged in via adapters.  
2. **Token‑only security.** Every frame contains a **Device Token** (16 bytes). Topology‑bound operations additionally carry a **Topology Token** (16 bytes) and set the `HAS_TOPO` flag.  
3. **Determinism.** Fixed header of 23 bytes, fixed token fields, stable payload catalogs.  
4. **Small footprint.** No crypto signing or nonce management.  
5. **Predictable behavior.** Explicit state machines with timeouts and bounded retries.  
6. **Observability.** Lightweight stats, counters, and reason codes.

### 1.2 Roles (recap)

- **ICM** (Interface Control Module): controller/UX, topology/admin source, FW coordinator.
- **PMS**: power management/rails/thermal telem & limits.
- **REL**: physical relay node.
- **SENS**: physical sensor node.
- **REMU**: relay emulator (virtualized multiple relays under one MAC).
- **SEMU**: sensor emulator (virtualized sensors under one MAC).

Virtual instances are addressed with `virt_id` (1..N). `0xFF` addresses the **physical device** instance.

### 1.3 Core Modules

- **now_radio**: thin wrapper over ESPNOW (init, peer mgmt, send, rx callback).  
- **now_codec**: pack/unpack header + tokens + payloads.  
- **now_router**: endpoint routing, virt demux, MAC pinning, role gating.  
- **now_sched**: tx queues, priorities, retry policy, time‑based work.  
- **now_pair**: pairing, provisioning hints, token injection.  
- **now_topology**: topo versioning, token rotation, storage, validation.  
- **now_fw**: firmware transport (begin/chunk/commit/abort) + CRC verify.  
- **now_cfg**: config writes/limits per role.  
- **now_diag**: stats, counters, prints, error reasons.  
- **role_* adapters**: ICM, PMS, REL, SENS, REMU, SEMU behaviors.  

All modules are designed to be **freestanding** (no dynamic allocation required; optional).

---

## 2. Wire Data: Header, Tokens, Payloads

### 2.1 Header (23 bytes)

```
struct NowHeader {
  uint8_t  proto_ver;   // NOW_PROTO_VER = 0x31 (v3‑T)
  uint8_t  msg_type;    // NowMsgType
  uint16_t flags;       // NOW_FLAGS_RELIABLE | NOW_FLAGS_URGENT | NOW_FLAGS_HAS_TOPO
  uint16_t seq;         // per‑sender
  uint16_t topo_ver;    // topology epoch/version
  uint8_t  virt_id;     // virtual instance or 0xFF for physical
  uint8_t  reserved;    // 0
  uint8_t  ts_ms[6];    // sender timestamp (48‑bit ms; diag only)
  uint8_t  sender_mac[6];
  uint8_t  sender_role; // NowDeviceKind
} // 23B packed
```

### 2.2 Tokens

- **Device Token (NowAuth128, 16B)** — **always present**.  
  Purpose: admission control (is this sender a recognized member).

- **Topology Token (NowTopoToken128, 16B)** — present **iff** `flags & HAS_TOPO`.  
  Purpose: authorization for **topology‑bound** ops (relay control, config writes, etc.).

> There is **no HMAC trailer**, no nonce, and no signature. Integrity is best‑effort (length/CRC inside certain payloads).

### 2.3 Message Catalog (unchanged IDs)

- Discovery/lifecycle: `PAIR_REQ`, `PAIR_ACK`, `PING`, `PING_REPLY`, `TIME_SYNC`
- Topology/network (ICM): `TOPO_PUSH`, `NET_SET_CHAN`
- Control/reports: `CTRL_RELAY`, `RLY_STATE`, `SENS_REPORT`, `PMS_STATUS`
- Config: `CONFIG_WRITE`
- Firmware (ICM): `FW_BEGIN`, `FW_CHUNK`, `FW_STATUS`, `FW_COMMIT`, `FW_ABORT`

Payload binaries are identical to the previous profile **minus** any HMAC references.

---

## 3. Validation Pipeline (Receiver‑side)

On RX callback, the stack runs these **in order**; the first failing check **drops** the frame:

1. **Version/type/role** sanity.  
2. **Sequence check**: per‑MAC windowed duplicate rejection (`seq` monotonic; window size 16 by default).  
3. **Device Token** equality against stored record (per MAC/role).  
4. If `HAS_TOPO`: **Topology Token** equality vs. current device topo token.  
5. **Privilege gate**: ensure msg_type is allowed for `sender_role`.  
6. **Virt routing**: map `virt_id` to local adapter instance.  
7. **Dispatch** to role adapter or subsystem (cfg/fw/topo/net/time).

If a check fails, no reply is sent (silent drop) except for FW stages where an explicit `FW_STATUS` MAY be emitted **if the failure occurs after parsing and the firmware subsystem opted into verbose errors**.

---

## 4. Admission & Authorization

- **Admission (Device Token)**: a node joins the fleet by having the Device Token known to peers (out‑of‑band or during pairing).  
- **Authorization (Topology Token)**: topology‑bound operations must include the **Topology Token**. Nodes store the active `topo_ver` and token; mismatch drops frames.  
- Recommended: pin **ICM MAC** for admin operations as an additional local policy (non‑cryptographic).

---

## 5. Pairing & Provisioning

### 5.1 Out‑of‑Band Model

- Manufacture stage writes Device Token into flash (per device).  
- ICM side imports the mapping (CSV, QR, serial tool).  
- On first contact, ICM replies with `PAIR_ACK` and (optionally) channel hints.

### 5.2 PAIR_REQ / PAIR_ACK Model

1. New node sends `PAIR_REQ` with its role, MAC, and a readable name/version in payload (optional).  
2. ICM decides to accept; responds with `PAIR_ACK { ok=1, chan=preferred }`.  
3. ICM may also **include** the Topology Token (plaintext) in a following `TOPO_PUSH` or side channel.  
4. Node persists tokens to flash (wear‑safe).

> This profile moves all secrets in **plaintext**. Ensure your commissioning path is trusted or physically controlled.

---

## 6. Topology Management

### 6.1 Versioning

- ICM owns `topo_ver` (uint16). A change increments it and is broadcast via `TOPO_PUSH` along with an optional fresh **Topology Token** (rotate on install or personnel change).  
- Nodes accept a `TOPO_PUSH` only from ICM and update their local `topo_ver` & token.

### 6.2 Binding

- Commands with meaning tied to topology (e.g., which relay group to toggle) **MUST** be sent with `HAS_TOPO` and the current token.  
- Receivers drop frames if their stored `topo_ver` differs **and** the command requires topology gating.  
- The token facilitates quick invalidation if a rogue or stale controller transmits old plans.

---

## 7. Scheduling, Retries, and Backoff

### 7.1 Queues

- Two TX queues: **urgent** and **normal**. `URGENT` flag routes to urgent.  
- Maximum queue depth is configurable per role (default: 8 normal, 4 urgent).

### 7.2 Reliable Delivery (best‑effort)

- If `RELIABLE` flag is set on an outbound packet, the stack expects a response (type‑specific) and retries on timeout.  
- Defaults: `tx_retry_max = 3`, `tx_timeout_ms = 40`, `backoff = (+20 ms)`.  
- Firmware chunks always set `RELIABLE` and track progress.

### 7.3 RX Window

- Per MAC a **16‑entry window** stores the highest `seq` seen and a bitmask of recent seqs. Duplicate frames are dropped. This is **not** replay‑proof; it only reduces accidental duplicates or simple re‑sends.

---

## 8. Router & Dispatch

### 8.1 Address Resolution

- `sender_mac` is used for per‑peer state (seq window, stats, tokens).  
- `virt_id` selects a local adapter instance for REMU/SEMU; physical is `0xFF`.

### 8.2 Privilege Gates (default policy)

- **ICM‑only:** `TOPO_PUSH`, `NET_SET_CHAN`, `TIME_SYNC`, all `FW_*`.  
- **Topology‑bound:** `CTRL_RELAY`, `CONFIG_WRITE`.  
- **Always allowed by role:** `SENS_REPORT` from SENS/SEMU, `PMS_STATUS` from PMS.  
- Implementers may add **local allowlists** (e.g., REL accepts `CTRL_RELAY` only from the pinned ICM MAC).

---

## 9. Codec Rules

### 9.1 Packing Order (TX)

1. `NowHeader` (23B)  
2. `NowAuth128` (16B)  
3. (optional) `NowTopoToken128` (16B) if `HAS_TOPO`  
4. Payload struct header (if any) and inline bytes

### 9.2 Unpacking Order (RX)

1. Read 23B header and validate fixed fields.  
2. Read 16B Device Token and compare stored token for the peer.  
3. If `HAS_TOPO`, read 16B Topology Token and compare local topology token.  
4. Parse payload by `msg_type` (ensure length).

### 9.3 Alignment & Endianness

- Structures are **packed**; integers are little‑endian.  
- Avoid flexible arrays except where documented (`FW_CHUNK`, `CONFIG_WRITE`).

---

## 10. Firmware Update Flow (CRC‑only)

### 10.1 Stages

1. **FW_BEGIN** (ICM→Node): includes target role, `total_len`, and `crc32`.  
2. **FW_CHUNK** (ICM→Node): blob slices with offset + len.  
3. **FW_COMMIT** (ICM→Node): finalize and verify CRC; if OK, mark staged.  
4. **FW_STATUS** (Node→ICM): periodic or on‑error status updates.  
5. **FW_ABORT** (either side): halt and clean state.

### 10.2 Expectations

- Without HMAC, integrity relies on **CRC32** and image format validation.  
- Nodes must stage to a separate partition/slot, then apply on reboot.  
- ICM retries missing chunks and tracks acknowledgment by progress counter.

### 10.3 Error Reasons

- `SIZE`, `CRC`, `ROLE_MISM`, `OPERATOR`, `INTERNAL` — unchanged semantics.

---

## 11. Configuration Writes

- **Key**: 6 raw bytes (`NowConfigWrite.key`).  
- **Len + bytes**: typed by role adapter.  
- Requires `HAS_TOPO` + valid Topology Token.  
- Each role adapter validates ranges/units. On success, settings persist (NV).

---

## 12. Time, Channel, and PING

- **TIME_SYNC** (ICM) sets the epoch in ms; nodes accept if from ICM.  
- **NET_SET_CHAN** (ICM) hints preferred Wi‑Fi channel; nodes may follow immediately or defer to a maintenance window.  
- **PING/PING_REPLY**: cheap liveness with `state_bits` and temperature/uptime fields.

---

## 13. Role Adapters

### 13.1 Adapter Interface

Each adapter exposes:

- `bool on_rx(const NowHeader&, const void* payload, size_t len, ReplyBuilder&)`  
- `void on_tick(uint32_t ms)` — called from scheduler.  
- `void on_topology_updated(uint16_t topo_ver)`  
- `void on_pairing(bool accepted)`  
- `void fill_ping(NowPingReply&) const`

`ReplyBuilder` defers serialization until router decides transport flags (RELIABLE/URGENT).

### 13.2 ICM

- Owns topology; emits `TOPO_PUSH`, `NET_SET_CHAN`, `TIME_SYNC`.  
- Orchestrates FW sessions with progress control.  
- Maintains directory of devices, tokens, and MACs.  
- UI/backend binds here (MQTT/HTTP is out of scope).

### 13.3 PMS

- Periodic `PMS_STATUS` reports and alarm flags.  
- Applies config limits (max Iout, thermal thresholds).  
- Accepts FW; rejects control that’s not applicable.

### 13.4 REL / REMU

- Implements `CTRL_RELAY` with pulse/toggle, debounces physical lines.  
- Emits `RLY_STATE` on change or on request.  
- REMU maps virtual channels to internal bitmap.

### 13.5 SENS / SEMU

- Emits `SENS_REPORT` with compact packed samples.  
- SEMU generates synthetic data or mirrors upstream sensors.  
- Config keys tune sampling periods and reporting cadence.

---

## 14. Storage Layout (NV)

- **Device Token**: 16B (private)  
- **Topology Token**: 16B (current)  
- **Topo Ver**: 2B  
- **ICM MAC**: 6B (optional policy)  
- **Config namespace per role** (keys/values)  
- **FW staging metadata**

Writes must be wear‑aware; batch commits are preferred.

---

## 15. Error Handling & Diagnostics

### 15.1 Drop Reasons (counters)

- `ver` (bad proto_ver)  
- `type` (unknown msg_type)  
- `role` (unknown sender_role)  
- `seq` (duplicate/out‑of‑window)  
- `tok_dev` (device token mismatch)  
- `tok_topo` (topology token mismatch/absent)  
- `gate` (privilege gate)  
- `len` (payload length invalid)  
- `fw_state` (FW flow violation)

### 15.2 Telemetry

- `rx_ok`, `tx_ok`, `tx_retry`, `tx_fail`, `rx_drop_*`, `uptime_s`  
- Per‑peer last seen ms, last seq, RSSI (if available).

### 15.3 Logging

- Debug prints via level mask; compile‑out for smallest builds.  
- Hex dumps optional in dev builds only.

---

## 16. API Surfaces (Internal)

### 16.1 now_radio

- `bool init(channel, cb_rx)`  
- `bool add_peer(mac)` / `bool remove_peer(mac)`  
- `bool send(mac, const void* buf, size_t len)`  
- `int  rssi_last(mac)` (optional)

### 16.2 now_codec

- `size_t pack_header(...)`  
- `bool   unpack_header(...)`  
- `size_t pack_tokens(...)` / `bool unpack_tokens(...)`  
- `bool   pack_payload(msg_type, ...)` / `bool unpack_payload(...)`

### 16.3 now_router

- `void on_rx(raw, len)`  
- `void enqueue_tx(dst_mac, hdr, tokens, payload, flags)`  
- `void bind_adapter(role, virt, Adapter*)`  
- `void set_policy(policy_struct)`

### 16.4 now_sched

- `void tick(ms)`  
- `bool push(frame, urgent)`  
- `bool pop_next(frame&)`  
- `void on_ack(frame_id)` (type‑specific acks)

### 16.5 now_pair/topo/cfg/fw

- Role‑independent; callable by adapters through a context pointer.

---

## 17. Build‑Time Profiles

- **MINIMAL**: disable printing; tiny queues; no FW; single adapter.  
- **FIELD**: FW enabled; printing limited; watchdog hardening.  
- **LAB**: verbose prints; stat dumps; heap guards.

---

## 18. Migration from v2H (HMAC) to v3‑T (Token‑Only)

1. **Remove** `NowSecTrailer` serialization/validation and all HMAC calls.  
2. **Keep** `NowAuth128` (Device Token) and **add** `NowTopoToken128` only when `HAS_TOPO`.  
3. **Retain** message catalog and payloads as‑is.  
4. **Review** privilege gates; enforce MAC pinning for admin ops if desired.  
5. **Revisit** FW update: rely on CRC32 + slot validation.  
6. **Re‑test** duplicate rejection and retry flows (they’re unchanged).

---

## 19. Example Flows

### 19.1 Relay Toggle (ICM→REL)

1. UI requests relay 2 toggle.  
2. ICM builds header (type `CTRL_RELAY`, sets `HAS_TOPO`, `RELIABLE`, `URGENT`).  
3. Add Device Token + Topology Token.  
4. Payload: `{channel=2, op=TOGGLE, pulse_ms=0}`.  
5. Send → REL validates tokens and gate → actuates → sends `RLY_STATE`.  
6. ICM updates UI from reply or times out and retries up to 3x.

### 19.2 Sensor Report (SENS→ICM)

1. SENS timer fires.  
2. SENS packs header (`SENS_REPORT`), Device Token only (no topo).  
3. Sends sample; ICM updates charts; optional ACK not required.

### 19.3 Firmware Update

1. ICM sends `FW_BEGIN` (target role, len, CRC).  
2. REL replies `FW_STATUS=RECEIVING`.  
3. ICM streams `FW_CHUNK`s in order; REL keeps bitmap of received blocks.  
4. On complete, ICM sends `FW_COMMIT`; REL verifies CRC and marks slot.  
5. REL reboots at maintenance window; `FW_STATUS=READY/APPLYING/REBOOTING` along the way.

---

## 20. Pseudocode (Receiver Pipeline)

```c
void on_rx(const uint8_t* p, size_t n) {
  NowHeader h;
  if (!unpack_header(&h, p, n)) return drop(VER);
  if (!valid_type(h.msg_type) || !valid_role(h.sender_role)) return drop(TYPE);

  if (!seq_window_accept(h.sender_mac, h.seq)) return drop(SEQ);

  NowAuth128 dev;
  if (!unpack_dev_token(&dev, &p, &n)) return drop(LEN);
  if (!token_equal(dev, token_db_for(h.sender_mac, h.sender_role))) return drop(TOK_DEV);

  NowTopoToken128 topo;
  bool has_topo = (h.flags & NOW_FLAGS_HAS_TOPO);
  if (has_topo) {
    if (!unpack_topo_token(&topo, &p, &n)) return drop(LEN);
    if (!token_equal(topo, local_topo_token())) return drop(TOK_TOPO);
  }

  if (!priv_gate(h.msg_type, h.sender_role)) return drop(GATE);

  dispatch_to_adapter(h, p, n); // p now points to payload
}
```

---

## 21. Reference Configuration

```ini
# now_stack.ini (example defaults)
tx.retry_max = 3
tx.timeout_ms = 40
tx.backoff_ms = 20

rx.seq_window = 16

queue.normal = 8
queue.urgent = 4

policy.icm_mac_pin = true
policy.ctrl_requires_topo = true
```

---

## 22. Testing Strategy

- **Unit**: codec pack/unpack, seq window, token equality, payload bounds.  
- **Integration**: adapter hooks, router gates, retries timeouts.  
- **System**: multi‑role mesh, topology rotation, channel switch, FW session.  
- **Fault injection**: bad tokens, mixed topo_ver, chunk loss, duplicate bursts.

---

## 23. Security Notes (Token‑Only)

- Tokens act as **capabilities**; treat them as secrets.  
- Rotate **Topology Token** on install or staff change.  
- Consider ESPNOW encrypted peers for link privacy.  
- If you need integrity/replay guarantees, re‑introduce HMAC or use a TLS‑capable side channel for critical actions (e.g., FW).

---

## 24. Frequently Asked Questions

**Q: Why keep two tokens?**  
A: Device Token = admission (who). Topology Token = authorization (what/where in this deployment). This lets you rotate topology without re‑flashing all devices’ Device Tokens.

**Q: Can I run with Device Token only?**  
A: Yes, but then all topology‑bound ops lose an easy rotation lever. Not recommended for field ops.

**Q: How do I handle lost `RLY_STATE` acks?**  
A: Use `RELIABLE` and retry on timeout; the operation is idempotent for ON/OFF and safe for TOGGLE when you first fetch current state.

**Q: Replay attacks?**  
A: Not mitigated in this profile. Use HMAC or radio encryption + counters if needed.

**Q: Can I multicast?**  
A: Yes, ESPNOW supports broadcast; however, retries and acks become trickier. Prefer unicast for control, broadcast for heartbeats.

---

## 25. Glossary

- **Admission**: permission to send frames that will be processed (Device Token).  
- **Authorization**: permission to perform bounded ops in a given topology (Topology Token).  
- **Adapter**: role‑specific module; implements behavior for a device type.  
- **Windowed duplicate rejection**: sequence mask preventing rapid dupes.

---

## 26. Minimal Compliance Checklist

- [ ] Always include **Device Token**.  
- [ ] Include **Topology Token** for topology‑bound ops and set `HAS_TOPO`.  
- [ ] Enforce ICM‑only for admin ops.  
- [ ] Maintain `seq` and duplicate window per peer.  
- [ ] Validate payload lengths and types.  
- [ ] Provide counters for drops and reasons.  
- [ ] Persist tokens and `topo_ver` in NV.  
- [ ] Retry with bounded backoff when `RELIABLE` is set.

---

## 27. Example Frame Dumps (Hex, illustrative)

```
CTRL_RELAY (HAS_TOPO, RELIABLE, URGENT)
23‑byte header | 16B dev token | 16B topo token | 04 02 00 00
# payload: channel=0x02, op=TOGGLE(0x02), pulse_ms=0x0000
```

```
SENS_REPORT (Device Token only)
23‑byte header | 16B dev token | 24B sensor payload
```

---

## 28. Implementation Notes

- Prefer static buffers sized to MTU; avoid heap in ISR context.  
- Keep codec side‑effect‑free and re‑entrant.  
- Use compile‑time asserts for struct sizes.  
- Bound all lengths and treat all external data as hostile.  
- Put tokens in a small **Token DB** keyed by MAC+role for quick lookup.  
- Consider watchdog kicks in long FW flashes.

---

## 29. Future Extensions (Orthogonal)

- Optional **ACK catalog** for broadcast scenarios.  
- Optional **nonce + HMAC** additive profile (v3‑TH) fully orthogonal to v3‑T.  
- Optional **stream window** for lossier channels.  
- Optional **CBOR payloads** for richer config pages (size trade‑off).

---

## 30. Conclusion

v3‑T keeps the spirit of the original stack—deterministic, tight, role‑agnostic—while removing HMAC signing to deliver a smaller, simpler, and faster runtime. Use **Device Token** for admission, **Topology Token** for topology‑bound authorization, and keep your operational posture tight with MAC pinning and periodic token rotation.

**End of document.**
