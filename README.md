# ESPNowManager — README (ICM Head-End)

This document explains how to use, configure, and extend the `ESPNowManager` class that powers the ICM (Interface Control Module) side of your driveway-lighting system. It covers the data model, persistent storage, pairing and token auth, live control vs auto sequencing, topology configuration, callbacks, JSON import/export, and troubleshooting.

This README refers to the actual source in `ESPNowManager.h/.cpp`. &#x20;

---

## 1) What ESPNowManager does

`ESPNowManager` is the ICM’s orchestration layer for all ESP-NOW traffic:

* Starts/owns ESP-NOW on a persistent Wi-Fi **channel** (configurable at runtime).
* Maintains a registry of **peers** (one Power module, up to N Relay modules, and up to M Presence Sensor modules, plus two special sensors: **Entrance** and **Parking**).
* Assigns and persists **per-peer auth tokens** (computed by the ICM at pair time) and MAC addresses.
* Provides **manual** control helpers (turn relay channel on/off, set modes) and **auto** mode helpers (propagate system mode, topology, and start/stop sequence commands).
* Exposes **callbacks** so your app/UI can react to power/relay/sensor responses.
* Serializes/deserializes peer lists & topology to JSON for UI management.

---

## 2) Key concepts

### Module types

* **Power**: single head-end power module (48 V feed, status, shutdown control).
* **Relay**: lighting zone nodes (each has 3 outputs, supports MANUAL/AUTO).
* **Presence**: sensor nodes (regular middle sensors), **plus** two *special* ones:

  * `Entrance` sensor
  * `Parking` sensor
    Special sensors are addressed distinctly and commonly act as sequence anchors.&#x20;

### Modes

* **AUTO**: ICM configures the chain and triggers a *sequence start*; sensors/relays forward triggers downstream based on their configured dependencies.
* **MANUAL**: ICM sends direct commands to individual relays/sensors; no chain logic.&#x20;

### Tokens (Auth)

* On pairing, ICM computes a unique **token** from `(icm_mac | node_mac | counter)` using SHA-256, persists the 64-hex string in NVS, and also stores a 16-byte compact form in RAM for frame headers. Peers must echo it back in headers.&#x20;

### Topology

* **Relay next-hop**: each relay knows who comes next (MAC + IPv4 hint) to forward sequence in AUTO.
* **Sensor dependency**: each sensor (incl. Entrance/Parking) knows which relay it should trigger.&#x20;

---

## 3) Persistent configuration (NVS keys)

All NVS keys are ≤ 6 characters.

* **Global**

  * `ESCHNL` – ESP-NOW channel
  * `ESMODE` – System mode (AUTO/MAN)
* **Per-peer (examples)**

  * Power MAC: `PWMAC` ; Power token: `PWTOK`
  * Relay #3 MAC: `RM03MC` ; token: `RT03TK`
  * Sensor #2 MAC: `SM02MC` ; token: `ST02TK`
  * Entrance MAC: `SETNMC` ; token: `SETNTK`
  * Parking MAC: `SPRKMC` ; token: `SPRKTK`&#x20;

> Tip: the ICM’s key counter `CTRR` (already in your config system) is used as part of token derivation for uniqueness across devices.&#x20;

---

## 4) Dependencies

* Arduino core for ESP32 (WiFi, FreeRTOS)
* ESP-NOW (`esp_now.h`), `esp_wifi.h`
* `ArduinoJson` for JSON import/export
* Your own:

  * `ConfigManager` (NVS abstraction)
  * `ICMLogFS` (for logging events)
  * `RTCManager` (for timestamps in message headers)
  * `CommandAPI.h` (shared opcodes/payload layouts & limits)&#x20;

---

## 5) Lifecycle & Quick-start

### Construct & start

```cpp
#include "ESPNowManager.h"
#include "ConfigManager.h"
#include "ICMLogFS.h"
#include "RTCManager.h"

ConfigManager cfg;
ICMLogFS      log(Serial);
RTCManager    rtc(&cfg);   // your RTC setup elsewhere

ESPNowManager espn(&cfg, &log, &rtc);

void setup() {
  Serial.begin(115200);
  // init cfg, log.begin(), rtc.begin(...) etc.

  // Start ESP-NOW. Channel is loaded from NVS (ESCHNL) or fallback to 1.
  if (!espn.begin(/*channelDefault=*/1, /*pmk16=*/"0123456789ABCDEF")) {
    // handle error
  }

  // Optionally set callbacks:
  espn.setOnAck([](const uint8_t mac[6], uint16_t ctr, uint8_t code){ /* ... */ });
  espn.setOnRelay([](const uint8_t mac[6], uint8_t idx, const uint8_t* pl, size_t len){ /* ... */ });
  espn.setOnPresence([](const uint8_t mac[6], uint8_t idx, const uint8_t* pl, size_t len){ /* ... */ });
  espn.setOnPower([](const uint8_t mac[6], const uint8_t* pl, size_t len){ /* ... */ });
}

void loop() {
  espn.poll(); // must be called frequently for retries/timeouts
}
```

* `begin()` pulls channel & mode from NVS, sets Wi-Fi to STA, sets the RF channel, initializes ESP-NOW, and registers callbacks.&#x20;
* `poll()` handles ACK timeouts and retry back-off for in-flight messages.&#x20;

---

## 6) Pairing & tokens

You can pair by explicit API (UI calls these after entering a MAC):

```cpp
espn.pairPower("24:6F:28:AA:BB:CC");
espn.pairRelay(3, "24:6F:28:11:22:33");
espn.pairPresenceEntrance("24:6F:28:44:55:66");
espn.pairPresenceParking("24:6F:28:77:88:99");
espn.pairPresence(2, "24:6F:28:DE:AD:BE");
```

* Pairing writes MAC + token to NVS. Peers are added to ESP-NOW with the current channel.&#x20;
* To remove:

```cpp
espn.unpairByMac("24:6F:28:11:22:33"); // removes peer runtime; NVS MAC remains unless you clear it
espn.removeAllPeers();   // runtime only
espn.clearAll();         // runtime + NVS (peers/tokens/mode/channel) reset
```



---

## 7) Channel & Mode

```cpp
espn.setChannel(6, /*persist=*/true);      // changes RF channel, re-adds peers on new channel
espn.setSystemModeAuto(/*persist=*/true);  // pushes SYS_MODE to all peers
espn.setSystemModeManual(true);            // ditto, for manual
```

* Channel and system mode are kept in NVS as `ESCHNL` and `ESMODE`. Changing channel re-adds all peers with the new channel value.&#x20;

---

## 8) Manual control helpers (ICM is the operator)

**Power**

```cpp
espn.powerCommand("status");   // or "on","off","shutdown","clear"
```

**Relays**

```cpp
espn.relayGetStatus(3);
espn.relaySet(3, /*ch=*/1, /*on=*/true);
espn.relaySetMode(3, /*ch=*/2, /*mode=*/1 /*AUTO*/);
espn.relayManualSet("AA:BB:...:FF", /*ch=*/3, /*on=*/false); // by MAC
```

**Sensors**

```cpp
espn.presenceGetStatus(2);
espn.presenceSetMode(2, /*mode=*/0 /*AUTO*/);
espn.sensorSetMode("AA:..", /*autoMode=*/true);
espn.sensorTestTrigger("AA:.."); // test frame
```

These APIs package the right command domain/opcodes/payload and queue the message with retries/ack tracking.&#x20;

---

## 9) Auto mode & topology

In **AUTO**, the ICM still **starts** the sequence; but the *propagation* is decentralized based on topology:

### Relay → Next hop

```cpp
uint8_t nextMac[6];
ESPNowManager::macStrToBytes("24:6F:28:...:99", nextMac);
espn.topoSetRelayNext(/*relayIdx=*/3, nextMac, /*nextIPv4=*/0);  // IPv4 optional hint
```

### Sensor → Target relay

```cpp
uint8_t targetMac[6];
ESPNowManager::macStrToBytes("24:6F:28:...:77", targetMac);
espn.topoSetSensorDependency(/*sensIdx=*/2, /*targetRelayIdx=*/5, targetMac, /*IPv4=*/0);
```

> Entrance/Parking sensors use the same API but their indices are handled internally as special constants; you can also configure them via JSON “links”.&#x20;

### JSON batch configuration

```cpp
// Example "links" array you pass to configureTopology(json["links"])
[
  {"type":"relay",   "idx":0, "next_mac":"AA:BB:..:01", "next_ip":0},
  {"type":"relay",   "idx":1, "next_mac":"AA:BB:..:02", "next_ip":0},
  {"type":"entrance","target_idx":0, "target_mac":"AA:BB:..:00", "target_ip":0},
  {"type":"sensor",  "idx":2, "target_idx":1, "target_mac":"AA:BB:..:01", "target_ip":0},
  {"type":"parking", "target_idx":N, "target_mac":"...", "target_ip":0}
]
```

Then:

```cpp
DynamicJsonDocument doc(2048);
// ... fill doc["links"] as above ...
espn.configureTopology(doc["links"]);
```

`configureTopology()` iterates each link and calls the right topo setter for you.&#x20;

---

## 10) Starting/Stopping sequences

Even in AUTO mode, **the ICM must send the “start” command** to kick off the chain:

```cpp
espn.sequenceStart(ESPNowManager::SeqDir::UP);    // or DOWN
// … later …
espn.sequenceStop();
```

There’s also a convenience:

```cpp
espn.startSequence(/*anchor=*/"entrance", /*up=*/true);
// anchor is advisory; the manager currently broadcasts start to all sensors + relays
```

The manager packages and sends SEQ\_START/SEQ\_STOP to sensors (incl. Entrance/Parking) and relays. Sensors/relays forward/act according to the configured topology and their local logic.&#x20;

---

## 11) Callbacks

Register callbacks to receive asynchronous responses:

```cpp
espn.setOnAck([](const uint8_t mac[6], uint16_t ctr, uint8_t code){
  // ACK for a specific frame counter
});

espn.setOnPower([](const uint8_t mac[6], const uint8_t* pl, size_t len){
  // parse power status payload
});

espn.setOnRelay([](const uint8_t mac[6], uint8_t relayIdx, const uint8_t* pl, size_t len){
  // relay status or result
});

espn.setOnPresence([](const uint8_t mac[6], uint8_t sensIdx, const uint8_t* pl, size_t len){
  // presence status or detection event
});

espn.setOnUnknown([](const uint8_t mac[6], const IcmMsgHdr& h, const uint8_t* pl, size_t len){
  // anything not in the known domains
});
```

* Internally, the manager verifies the **token** on each inbound frame, updates peer online status, and then dispatches to the right callback.&#x20;

---

## 12) JSON: peers & topology (UI integration)

For your web UI / `WiFiManager`, you can fetch JSON snapshots:

```cpp
String peersJson   = espn.serializePeers();     // includes channel, mode, online flags
String topoJson    = espn.serializeTopology();  // links
String exportJson  = espn.exportConfiguration(); // peers + topology + ch/mode
```

You can also **clear** or **remove** peers as part of admin workflows:

```cpp
espn.removeAllPeers(); // runtime ESP-NOW peers only
espn.clearAll();       // also clears NVS (peers/tokens/mode/channel)
```



---

## 13) Reliability details

* Each outbound message becomes a **pending TX** with a unique counter, retry budget, deadline, and ack flag.
* `poll()` enforces **ACK timeouts** and **back-off retries** (tunable via private members).
* Peers are marked `online=false` after consecutive failures; recovered on success.
* When changing channel, peers are **re-added** on the new channel automatically.&#x20;

---

## 14) Security notes

* Tokens are **ICM-generated** (SHA-256) and stored in NVS on both sides (ICM + node).
* The 16-byte token is copied into every header (`IcmMsgHdr`) and checked on RX; mismatches are dropped.
* Optionally set a PMK (16-byte) on `begin()` to enable ESP-NOW link encryption if your nodes support it.&#x20;

---

## 15) Special sensors (Entrance / Parking)

* Use `pairPresenceEntrance()` and `pairPresenceParking()` for these anchors.
* They are tracked in dedicated slots and may be used to drive sequence direction or to seed the first relay.
* JSON topology accepts `"type":"entrance"` and `"type":"parking"` items.&#x20;

---

## 16) Typical workflows

**First-time setup**

1. `begin()` → loads mode/channel from NVS, starts ESP-NOW.
2. Pair all devices (Power, Relays, Sensors, Entrance, Parking).
3. Use `configureTopology(links)` to set next-hops & dependencies.
4. Set `AUTO` mode (`setSystemModeAuto(true)`), then `sequenceStart(UP)` (or `DOWN`).

**Manual operation**

1. `setSystemModeManual(true)`.
2. Use `relaySet(idx, ch, on)` and `presenceSetMode(idx, mode)` for direct control.

**Change channel**

1. `setChannel(6, true)` → re-adds peers automatically, persists `ESCHNL`.

**Backup/Restore**

1. `exportConfiguration()` → download JSON.
2. Later, reconstruct by pairing peers from JSON + `configureTopology()`.

---

## 17) Troubleshooting

* **No RX/ACK** after channel change → ensure all nodes also switch to the same channel; ICM re-adds peers automatically, but nodes must join on that channel too.&#x20;
* **Token mismatch** warnings → device was re-flashed or NVS cleared. Re-pair to re-establish tokens.&#x20;
* **Peer offline** → consecutive send failures exceeded threshold; will recover on next success.
* **Sequence doesn’t propagate** in AUTO → verify:

  * Entrance/Parking paired & present
  * Relay next-hop set for each link
  * Sensor dependencies point to the correct relay indices
  * ICM sent **`sequenceStart()`** (AUTO still requires ICM kickoff)

---

## 18) Extending

* Add new **domains/opcodes** in `CommandAPI.h` (shared across ICM and nodes).
* Create new **payload structs** and send helpers like `relaySet()` for your new features.
* Use `serializePeers()`/`serializeTopology()` to keep the UI and firmware in sync.
* Leverage logging via `ICMLogFS` for field diagnostics (already integrated).&#x20;

---

## 19) Minimal example

```cpp
// 1) Bring up ESP-NOW
espn.begin(1, "0123456789ABCDEF");

// 2) Pair devices
espn.pairPower("24:6F:28:AA:BB:CC");
espn.pairRelay(0, "24:6F:28:01:02:03");
espn.pairRelay(1, "24:6F:28:04:05:06");
espn.pairPresenceEntrance("24:6F:28:11:22:33");

// 3) Topology
uint8_t r1Mac[6]; ESPNowManager::macStrToBytes("24:6F:28:04:05:06", r1Mac);
espn.topoSetRelayNext(0, r1Mac, 0);

uint8_t r0Mac[6]; ESPNowManager::macStrToBytes("24:6F:28:01:02:03", r0Mac);
espn.topoSetSensorDependency(ESPNowManager::PRES_IDX_ENTRANCE, 0, r0Mac, 0);

// 4) Auto mode + start the chain
espn.setSystemModeAuto(true);
espn.sequenceStart(ESPNowManager::SeqDir::UP);

// 5) In loop
void loop(){ espn.poll(); /* … */ }
```

---

## 20) File reference

* **ESPNowManager.h** — public API, data structures, constants, and callbacks.&#x20;
* **ESPNowManager.cpp** — implementation: pairing, tokens, topology, message queueing/ACKs, RX/TX handlers, JSON helpers.&#x20;

---