/*! ICM Topology UI — unified 1s live polling on selection
 *  - Polls POWER, SENSOR, or RELAY every 1s while selected
 *  - Stops when nothing is selected
 *  - Click anywhere outside selectable elements to unselect
 *  - Refresh button reloads paired devices
 *
 *  NOTE: This version removes multi-sensor kinds (Entrance/Parking).
 *        Only SENSOR is supported for sensors.
 */
(() => {
  "use strict";

  // ------------------------ DOM helpers ------------------------
  const sensorDefaults = {
    name: "",
    tf_luna_threshold_A: 80,
    tf_luna_threshold_B: 120,
    day_night_threshold: 50,
    trigger_mode: "TOGGLE",
    fan_mode: "AUTO",
    buzzer_enabled: true,
    report_interval: 1000,
    time_sync: "", // ISO string or empty
    temperature_calibration: 0,
    pressure_offset: 0,
  };

  const relayDefaults = {
    name: "",
    relay_1_state: false,
    relay_2_state: false,
    relay_mode: "AUTO", // AUTO | MANUAL | FOLLOW_SENSOR
    fan_mode: "AUTO", // AUTO | ON | OFF
    buzzer_enabled: false,
    pulse_duration: 200, // ms
    time_sync: "", // ISO string
    temperature_limit: 70, // °C
  };
  // === Emulator defaults ===
  const semuxDefaults = {
    device_id: "",
    name: "",
    sensor_count: 2, // 1..8 typ
    emulation_mode: "STATIC", // STATIC | DYNAMIC | PATTERN
    report_interval: 1000,
    fan_mode: "AUTO",
    buzzer_enabled: false,
    log_level: "INFO",
    auto_restart: false,
    led_mode: "AUTO",
    time_sync: "",
    // Per-virtual arrays (fill to sensor_count)
    tf_luna_threshold_A: [],
    tf_luna_threshold_B: [],
    day_night_threshold: [],
  };

  const remuxDefaults = {
    device_id: "",
    name: "",
    relay_count: 4, // 1..16 typ
    report_interval: 1000,
    fan_mode: "AUTO",
    buzzer_enabled: false,
    log_level: "INFO",
    auto_restart: false,
    led_mode: "AUTO",
    time_sync: "",
    // Per-virtual arrays (fill to relay_count)
    relay_state: [],
    relay_mode: [], // AUTO | MANUAL | FOLLOW_SENSOR
    pulse_duration: [],
  };
  // === PMS Settings (Power Management System) ===
  const pmsDefaults = {
    name: "",
    power_mode: "AUTO", // AUTO | WALL | BATTERY
    relay_state: [false, false, false, false], // R1..R4
    fan_mode: "AUTO", // AUTO | ECO | FULL | OFF
    buzzer_enabled: false,
    time_sync: "", // ISO string
    temperature_limit: 70, // °C
    voltage_calibration: 0.0, // ΔV
    current_calibration: 0.0, // ΔA
  };

  // Sensor
  Object.assign(sensorDefaults, {
    device_id: "", // readonly display
    log_level: "INFO", // NONE | ERROR | INFO | DEBUG
    auto_restart: false, // Bool
    firmware_update: false, // trigger flag (we’ll use a button)
    led_mode: "AUTO", // Enum
  });

  // Relay
  Object.assign(relayDefaults, {
    device_id: "",
    log_level: "INFO",
    auto_restart: false,
    firmware_update: false,
    led_mode: "AUTO",
  });

  // PMS
  Object.assign(pmsDefaults, {
    device_id: "",
    log_level: "INFO",
    auto_restart: false,
    firmware_update: false,
    led_mode: "AUTO",
  });
  // per-type labels
  const TYPE_LABEL = {
    power: "POWER",
    relay: "RELAY",
    sensor: "SENSOR",
    semux: "SEMUX",
    remux: "REMUX",
  };
  // ---- Draft helpers (session-scoped, per MAC) ----
  const DRAFT_KEYS = {
    sensor: (mac) => "icm_draft_sensor_" + (mac || "default"),
    relay: (mac) => "icm_draft_relay_" + (mac || "default"),
    pms: (mac) => "icm_draft_pms_" + (mac || "default"),
  };

  const $ = (id) => document.getElementById(id);
  const palette = $("palette");
  const lane = $("lane");
  const peersTbl = $("peersTbl");
  const peersTblBody = peersTbl?.querySelector("tbody");
  const peersScroll = $("peersScroll");
  const peersCount = $("peersCount");
  const peerSearch = $("peerSearch");
  const toast = $("toast");

  const btnRefreshPeers = $("btnRefreshPeers");
  const btnClear = $("btnClear"),
    btnLoad = $("btnLoad"),
    btnSave = $("btnSave"),
    btnPush = $("btnPush");
  const btnExport = $("btnExport"),
    fileImport = $("fileImport");

  const pType = $("pType"),
    pMac = $("pMac"),
    btnPair = $("btnPair");
  const rMac = $("rMac"),
    btnRemove = $("btnRemove");

  const selNone = $("selNone"),
    selInfo = $("selInfo");
  const siKind = $("siKind"),
    siMac = $("siMac");

  const panelSensor = $("panelSensor");
  const panelRelay = $("panelRelay");
  const panelPower = $("panelPower");

  const siSensStatus = $("siSensStatus");
  const siDN = $("siDN");
  const siRelState = $("siRelState");

  const siHum = $("siHum"),
    siPress = $("siPress"),
    siLux = $("siLux"),
    siTfA = $("siTfA"),
    siTfB = $("siTfB");

  const tOn = $("tOn"),
    tOff = $("tOff");

  const pwrOnBtn = $("pwrOn"),
    pwrOffBtn = $("pwrOff");
  const gPwrVolt = $("gPwrVolt"),
    gPwrCurr = $("gPwrCurr"),
    gPwrTemp = $("gPwrTemp");
  const gRelTemp = $("gRelTemp");
  const gSensTemp = $("gSensTemp");

  const logoutBtn = $("logoutBtn");
  if (logoutBtn) {
    logoutBtn.addEventListener("click", () => {
      const f = document.createElement("form");
      f.method = "POST";
      f.action = "/logout";
      document.body.appendChild(f);
      f.submit();
    });
  }

  // ---------- Limits (tweak to taste) ----------
  const LIMITS = {
    tempC: { warn: 40, danger: 60 }, // sensor temp
    humidity: { warn: 70, danger: 85 }, // %
    pressure_hPa: {
      warnLow: 980,
      warnHigh: 1030, // ambient band
      dangerLow: 960,
      dangerHigh: 1040,
    },
    vbus_V: { warnLow: 44, warnHigh: 52, dangerLow: 42, dangerHigh: 54 },
    ibus_A: { warn: 4.0, danger: 6.0 },
    pwrTempC: { warn: 60, danger: 80 },
  };

  // ---- SEMUX/REMUX addr helpers ---------------------------------------------
  function encodeAddr(mac, vType, vIndex) {
    const m = macFormatColon(mac);
    if (!vType || !Number.isInteger(vIndex)) return null;
    const letter = String(vType).toLowerCase() === "semux" ? "S" : "R";
    return `${m}#${letter}${vIndex}`;
  }
  function parseEmuFromLink(link) {
    let vType = (link.vType || "").toLowerCase();
    let vIndex = Number.isInteger(link.vIndex) ? link.vIndex : null;
    let addr = link.addr || null;

    // derive from addr if needed: AA:..:FF#S3 or #R12
    if (!vType && typeof addr === "string") {
      const m = addr.match(/#([SR])(\d+)/i);
      if (m) {
        vType = m[1].toUpperCase() === "S" ? "semux" : "remux";
        vIndex = parseInt(m[2], 10);
      }
    }
    if (!addr && vType && Number.isInteger(vIndex)) {
      addr = encodeAddr(link.mac, vType, vIndex);
    }
    return { vType, vIndex, addr };
  }

  function setSelectedPeer(peer) {
    // peer = { type: 'sensor'|'relay'|'semux'|'remux', mac:'AA:..:FF', vIndex?:number }
    // Clear brick selection if we are selecting from palette
    selectedId = null;
    selectedPeer = peer || null;

    // Pill/child highlight reset
    document
      .querySelectorAll(".pill.selected")
      .forEach((el) => el.classList.remove("selected"));
    document
      .querySelectorAll(".emu-chan.selected")
      .forEach((el) => el.classList.remove("selected"));

    // If a child was provided, highlight it
    if (
      peer &&
      (peer.type === "semux" || peer.type === "remux") &&
      Number.isInteger(peer.vIndex)
    ) {
      const macFmt = macFormatColon(peer.mac);
      const vType = peer.type;
      const vIndex = peer.vIndex;
      const node = document.querySelector(
        `.emu-chan[data-mac="${macFmt}"][data-vtype="${vType}"][data-vindex="${vIndex}"]`
      );
      if (node) node.classList.add("selected");
    } else if (peer) {
      // Otherwise (parent/real), highlight the parent pill
      const macFmt = macFormatColon(peer.mac);
      const grp =
        peer.type === "semux"
          ? "#grp-semux"
          : peer.type === "remux"
          ? "#grp-remux"
          : null;
      if (grp) {
        const node = document.querySelector(
          `${grp} .parent-pill[data-mac="${macFmt}"]`
        );
        if (node) node.classList.add("selected");
      }
    }

    // Reflect in Selected panel (right card)
    updateSelectedPanelFromPeer(peer);
  }

  function updateSelectedPanelFromPeer(peer) {
    // Show proper panel and seed basic fields so user gets feedback immediately
    if (!peer) {
      // nothing selected
      selInfo.hidden = true;
      selNone.hidden = false;
      stopPolling && stopPolling();
      return;
    }

    const kindUp = BASE_KIND(KIND_UP(peer.type)); // SEMUX→SENSOR, REMUX→RELAY
    selNone.hidden = true;
    selInfo.hidden = false;

    siKind.textContent = kindUp; // POWER|SENSOR|RELAY
    siMac.textContent = macFormatColon(peer.mac); // MAC display

    // Toggle panels
    panelPower.hidden = kindUp !== "POWER";
    panelSensor.hidden = kindUp !== "SENSOR";
    panelRelay.hidden = kindUp !== "RELAY";

    // Seed gauges/status to avoid empty UI
    if (kindUp === "SENSOR") {
      setGauge(gSensTemp, 0, -20, 80, "°C");
      renderDayNight(undefined);
    }
    if (kindUp === "RELAY") {
      siRelState.textContent = "—";
    }

    // Start unified polling for this selection (if present in your codebase)
    if (typeof startPolling === "function") {
      pollTarget = {
        kindUp,
        mac: macFormatColon(peer.mac),
        vIndex: peer.vIndex ?? null,
        vType: peer.type,
      };
      startPolling();
    }
  }
  async function handleRelayToggle(on) {
    const ctx = getRelayCtx(); // { mac, emuType: 'REMUX'|null, vIndex: number|null }
    if (!ctx?.mac) {
      showToast("Select a relay first");
      return;
    }

    try {
      let ok = false;

      if (ctx.emuType === "REMUX") {
        // Emulator: send MAC + channel index
        const r = await fetch("/api/remux/relay/set", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            mac: ctx.mac,
            index: Number.isInteger(ctx.vIndex) ? ctx.vIndex : 0,
            on: !!on,
          }),
        });
        ok = r.ok;
      } else {
        // Real relay: same endpoint you already use
        const r = await fetch("/api/relay/set", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ mac: ctx.mac, on: !!on }),
        });
        ok = r.ok;
      }

      if (ok) {
        siRelState.textContent = on ? "ON" : "OFF";
        showToast(on ? "Relay ON" : "Relay OFF");
      } else {
        showToast("Command failed");
      }
    } catch {
      showToast("Command error");
    }
  }

  // --- Selected emulator-channel highlight helpers (palette) ---
  function clearEmuChanSelection() {
    document
      .querySelectorAll(".emu-chan.selected")
      .forEach((el) => el.classList.remove("selected"));
  }

  function highlightEmuChan(
    mac,
    vType /* 'semux'|'remux' */,
    vIndex /* number */
  ) {
    clearEmuChanSelection();
    const macFmt = macFormatColon(mac);
    const sel = document.querySelector(
      `.emu-chan[data-mac="${macFmt}"][data-vtype="${vType}"][data-vindex="${vIndex}"]`
    );
    if (sel) sel.classList.add("selected");
  }

  function getSensorCtx() {
    if (selectedId != null) {
      const b = bricks.find((x) => x.id === selectedId);
      if (b && BASE_KIND(b.kind) === "SENSOR") {
        return {
          mac: macFormatColon(b.mac),
          emuType: b.vType && KIND_UP(b.vType) === "SEMUX" ? "SEMUX" : null,
          vIndex: Number.isInteger(b.vIndex) ? b.vIndex : null,
        };
      }
    }
    if (selectedPeer && BASE_KIND(KIND_UP(selectedPeer.type)) === "SENSOR") {
      return {
        mac: macFormatColon(selectedPeer.mac),
        emuType: KIND_UP(selectedPeer.type) === "SEMUX" ? "SEMUX" : null,
        vIndex: Number.isInteger(selectedPeer.vIndex)
          ? selectedPeer.vIndex
          : null,
      };
    }
    return null;
  }

  // Ensure ON/OFF hit the selected channel for REMUX
  function wireRelayButtons() {
    if (tOn)
      tOn.onclick = (e) => {
        e.preventDefault();
        handleRelayToggle(true);
      };
    if (tOff)
      tOff.onclick = (e) => {
        e.preventDefault();
        handleRelayToggle(false);
      };
  }
  wireRelayButtons();
  // Provide selected context for ON/OFF etc.
  function getRelayCtx() {
    // Prefer selected brick
    if (selectedId != null) {
      const b = bricks.find((x) => x.id === selectedId);
      if (b && BASE_KIND(b.kind) === "RELAY") {
        return {
          mac: macFormatColon(b.mac),
          emuType: b.vType && KIND_UP(b.vType) === "REMUX" ? "REMUX" : null,
          vIndex: Number.isInteger(b.vIndex) ? b.vIndex : null,
        };
      }
    }
    // Then palette selection (parent/child)
    if (selectedPeer && BASE_KIND(KIND_UP(selectedPeer.type)) === "RELAY") {
      return {
        mac: macFormatColon(selectedPeer.mac),
        emuType: KIND_UP(selectedPeer.type) === "REMUX" ? "REMUX" : null,
        vIndex: Number.isInteger(selectedPeer.vIndex)
          ? selectedPeer.vIndex
          : null,
      };
    }
    return null;
  }

  function isEmu(type) {
    return type === "semux" || type === "remux";
  }
  function emuKey(mac) {
    return (mac || "default").toUpperCase();
  }
  function readDraft(kind, mac) {
    try {
      return JSON.parse(
        sessionStorage.getItem(DRAFT_KEYS[kind](mac)) || "null"
      );
    } catch {
      return null;
    }
  }
  function writeDraft(kind, mac, obj) {
    try {
      sessionStorage.setItem(DRAFT_KEYS[kind](mac), JSON.stringify(obj || {}));
    } catch {}
  }
  function clearDraft(kind, mac) {
    try {
      sessionStorage.removeItem(DRAFT_KEYS[kind](mac));
    } catch {}
  }
  function getSelectedPowerMac() {
    // Prefer selected brick; else selectedPeer; else pollTarget
    if (selectedId != null) {
      const b = bricks.find((x) => x.id === selectedId);
      if (b && String(b.kind).toUpperCase() === "POWER")
        return macFormatColon(b.mac);
    }
    if (selectedPeer && String(selectedPeer.type).toLowerCase() === "power")
      return macFormatColon(selectedPeer.mac);
    if (pollTarget && pollTarget.kindUp === "POWER")
      return macFormatColon(pollTarget.mac);
    // If no explicit power MAC, pick any paired power device
    const p = peers.find((pp) => String(pp.type).toLowerCase() === "power");
    return p ? macFormatColon(p.mac) : null;
  }

  function openPmsSettings() {
    const modal = document.getElementById("pmsSettingsModal");
    if (!modal) return;
    modal.setAttribute("aria-hidden", "false");
    const mac = getSelectedPowerMac();
    loadPmsSettings(mac).catch(() => {});
  }

  function closePmsSettings() {
    const modal = document.getElementById("pmsSettingsModal");
    if (!modal) return;
    modal.setAttribute("aria-hidden", "true");
  }

  async function loadPmsSettings(mac) {
    const form = document.getElementById("pmsSettingsForm");
    if (!form) return;

    let cfg = null;
    // Try backend first
    try {
      const r = await fetch("/api/pms/settings/get", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mac }),
      });
      if (r.ok) cfg = await r.json();
    } catch {}

    // Fallback to local
    if (!cfg) {
      try {
        cfg = JSON.parse(
          localStorage.getItem("icm_pms_cfg_" + (mac || "default")) || "null"
        );
      } catch {}
    }
    cfg = { ...pmsDefaults, ...(cfg || {}) };

    const draft = readDraft("pms", mac);
    if (draft) cfg = { ...cfg, ...draft };

    form.pms_name.value = cfg.name ?? "";
    form.pms_mode.value = cfg.power_mode ?? "AUTO";
    document.getElementById("pms_r1").checked = !!cfg.relay_state?.[0];
    document.getElementById("pms_r2").checked = !!cfg.relay_state?.[1];
    document.getElementById("pms_r3").checked = !!cfg.relay_state?.[2];
    document.getElementById("pms_r4").checked = !!cfg.relay_state?.[3];
    form.pms_fan.value = cfg.fan_mode ?? "AUTO";
    form.pms_buzz.value = String(!!cfg.buzzer_enabled);
    form.pms_time.value = cfg.time_sync ? cfg.time_sync.slice(0, 19) : "";
    form.pms_tlimit.value = cfg.temperature_limit ?? 70;
    form.pms_vcal.value = cfg.voltage_calibration ?? 0;
    form.pms_ical.value = cfg.current_calibration ?? 0;

    // Common (PMS)
    form.querySelector("#pms_devid").textContent = cfg.device_id || "—";
    form.pms_log.value = cfg.log_level ?? "INFO";
    form.pms_auto.value = String(!!cfg.auto_restart);
    form.pms_led.value = cfg.led_mode ?? "AUTO";
  }

  async function savePmsSettings() {
    const form = document.getElementById("pmsSettingsForm");
    const mac = getSelectedPowerMac();
    if (!form || !mac) {
      showToast("Select a power module first");
      return;
    }

    const cfg = {
      name: form.pms_name.value.trim(),
      power_mode: String(form.pms_mode.value || "AUTO"),
      relay_state: [
        document.getElementById("pms_r1").checked,
        document.getElementById("pms_r2").checked,
        document.getElementById("pms_r3").checked,
        document.getElementById("pms_r4").checked,
      ],
      fan_mode: String(form.pms_fan.value || "AUTO"),
      buzzer_enabled: form.pms_buzz.value === "true",
      time_sync: form.pms_time.value
        ? new Date(form.pms_time.value).toISOString()
        : "",
      temperature_limit: Number(form.pms_tlimit.value || 70),
      voltage_calibration: Number(form.pms_vcal.value || 0),
      current_calibration: Number(form.pms_ical.value || 0),
      log_level: String(form.pms_log.value || "INFO"),
      auto_restart: form.pms_auto.value === "true",
      led_mode: String(form.pms_led.value || "AUTO"),
    };

    let ok = false;
    // Try backend save
    try {
      const r = await fetch("/api/pms/settings/set", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mac, cfg }),
      });
      ok = r.ok;
    } catch {}

    // Always persist locally too
    try {
      localStorage.setItem("icm_pms_cfg_" + mac, JSON.stringify(cfg));
      if (!ok) ok = true;
    } catch {}

    showToast(ok ? "Settings saved" : "Save failed");
    if (ok) {
      clearDraft("pms", mac);
      closePmsSettings();
    }
  }

  async function resetPmsNode() {
    const mac = getSelectedPowerMac();
    if (!mac) {
      showToast("Select a power module first");
      return;
    }
    if (!confirm("Reset PMS node now?")) return;
    try {
      const r = await fetch("/api/pms/reset", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mac }),
      });
      showToast(r.ok ? "PMS reset sent" : "PMS reset failed");
    } catch {
      showToast("PMS reset error");
    }
  }
  function getSelectedRelayMac() {
    // Prefer selected brick if it's a RELAY; else selectedPeer; else pollTarget
    if (selectedId != null) {
      const b = bricks.find((x) => x.id === selectedId);
      if (b && String(b.kind).toUpperCase() === "RELAY")
        return macFormatColon(b.mac);
    }
    if (selectedPeer && String(selectedPeer.type).toLowerCase() === "relay")
      return macFormatColon(selectedPeer.mac);
    if (pollTarget && pollTarget.kindUp === "RELAY")
      return macFormatColon(pollTarget.mac);
    return null;
  }

  function openRelaySettings() {
    const modal = document.getElementById("relaySettingsModal");
    if (!modal) return;
    modal.setAttribute("aria-hidden", "false");
    const mac = getSelectedRelayMac();
    loadRelaySettings(mac).catch(() => {});
  }

  function closeRelaySettings() {
    const modal = document.getElementById("relaySettingsModal");
    if (!modal) return;
    modal.setAttribute("aria-hidden", "true");
  }

  // =================== SENSOR: SAVE (supports SEMUX) ===================
  async function saveSensorSettings() {
    const form = document.getElementById("sensorSettingsForm");
    const ctx = getSensorCtx();
    const mac = ctx?.mac;
    if (!form || !mac) {
      showToast("Select a sensor first");
      return;
    }
    const isEMU = ctx?.emuType === "SEMUX";

    // Build config object from form (shared for both real & emu)
    const baseCfg = {
      name: (form.ss_name?.value || "").trim(),
      tf_luna_threshold_A: Number(form.ss_tf_a?.value || 0),
      tf_luna_threshold_B: Number(form.ss_tf_b?.value || 0),
      day_night_threshold: Number(form.ss_dn?.value || 0),
      trigger_mode: String(form.ss_trigger?.value || "TOGGLE"),
      fan_mode: String(form.ss_fan?.value || "AUTO"),
      buzzer_enabled: (form.ss_buzzer?.value || "false") === "true",
      report_interval: Math.max(250, Number(form.ss_report?.value || 1000)),
      time_sync: form.ss_time?.value
        ? new Date(form.ss_time.value).toISOString()
        : "",
      temperature_calibration: Number(form.ss_temp_cal?.value || 0),
      pressure_offset: Number(form.ss_press_off?.value || 0),
      log_level: String(form.ss_log?.value || "INFO"),
      auto_restart: (form.ss_auto?.value || "false") === "true",
      led_mode: String(form.ss_led?.value || "AUTO"),
    };

    if (isEMU) {
      // Emulator-only fields (shared across all SEMUX channels for this MAC)
      baseCfg.sensor_count = Math.max(
        1,
        Number(document.getElementById("ss_count")?.value || 8)
      );
      baseCfg.emulation_mode = String(
        document.getElementById("ss_mode")?.value || "STATIC"
      );

      // Persist locally under SEMUX key
      try {
        localStorage.setItem("icm_semux_cfg_" + mac, JSON.stringify(baseCfg));
      } catch {}
      clearDraft("sensor", mac);
      closeSensorSettings();
      renderPalette(); // if counts changed → refresh dropdown availability
      showToast("SEMUX settings saved");
      return;
    }

    // Real sensor: try backend, then local fallback
    let ok = false;
    try {
      const r = await fetch("/api/sensor/settings/set", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mac, cfg: baseCfg }),
      });
      ok = r.ok;
    } catch {}
    try {
      localStorage.setItem("icm_sensor_cfg_" + mac, JSON.stringify(baseCfg));
      if (!ok) ok = true;
    } catch {}
    showToast(ok ? "Settings saved" : "Save failed");
    if (ok) {
      clearDraft("sensor", mac);
      closeSensorSettings();
    }
  }

  // =================== SENSOR: LOAD (supports SEMUX) ===================
  async function loadSensorSettings(macFromClick) {
    const form = document.getElementById("sensorSettingsForm");
    if (!form) return;

    const ctx = getSensorCtx(); // { mac, emuType: 'SEMUX'|null, vIndex }
    const mac = macFormatColon(macFromClick || ctx?.mac || "");
    const isEMU = ctx?.emuType === "SEMUX";

    // Toggle emulator-only UI (modal only)
    if (isEMU) form.dataset.emu = "semux";
    else form.removeAttribute("data-emu");

    let cfg = null;

    if (isEMU) {
      // SEMUX: shared settings per parent MAC
      try {
        cfg = JSON.parse(
          localStorage.getItem("icm_semux_cfg_" + mac) || "null"
        );
      } catch {}
      cfg = {
        ...(typeof semuxDefaults === "object" ? semuxDefaults : {}),
        ...(cfg || {}),
      };
    } else {
      // Real sensor: backend then local
      try {
        const r = await fetch("/api/sensor/settings/get", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ mac }),
        });
        if (r.ok) cfg = await r.json();
      } catch {}
      if (!cfg) {
        try {
          cfg = JSON.parse(
            localStorage.getItem("icm_sensor_cfg_" + mac) || "null"
          );
        } catch {}
      }
      cfg = {
        ...(typeof sensorDefaults === "object" ? sensorDefaults : {}),
        ...(cfg || {}),
      };
    }

    // Overlay any draft (same MAC key)
    const draft = readDraft("sensor", mac);
    if (draft) cfg = { ...cfg, ...draft };

    // Common fields
    if (form.ss_name) form.ss_name.value = cfg.name ?? "";
    if (form.ss_tf_a) form.ss_tf_a.value = cfg.tf_luna_threshold_A ?? 80;
    if (form.ss_tf_b) form.ss_tf_b.value = cfg.tf_luna_threshold_B ?? 120;
    if (form.ss_dn) form.ss_dn.value = cfg.day_night_threshold ?? 50;
    if (form.ss_trigger) form.ss_trigger.value = cfg.trigger_mode ?? "TOGGLE";
    if (form.ss_fan) form.ss_fan.value = cfg.fan_mode ?? "AUTO";
    if (form.ss_buzzer) form.ss_buzzer.value = String(!!cfg.buzzer_enabled);
    if (form.ss_report) form.ss_report.value = cfg.report_interval ?? 1000;
    if (form.ss_time)
      form.ss_time.value = cfg.time_sync ? cfg.time_sync.slice(0, 19) : "";
    if (form.ss_temp_cal)
      form.ss_temp_cal.value = cfg.temperature_calibration ?? 0;
    if (form.ss_press_off) form.ss_press_off.value = cfg.pressure_offset ?? 0;

    // Common meta
    const idEl = form.querySelector("#ss_devid");
    if (idEl) idEl.textContent = cfg.device_id || "—";
    if (form.ss_log) form.ss_log.value = cfg.log_level ?? "INFO";
    if (form.ss_auto) form.ss_auto.value = String(!!cfg.auto_restart);
    if (form.ss_led) form.ss_led.value = cfg.led_mode ?? "AUTO";

    // Emulator-only extras for SEMUX
    if (isEMU) {
      const cEl = document.getElementById("ss_count");
      const mEl = document.getElementById("ss_mode");
      if (cEl) cEl.value = cfg.sensor_count ?? 8;
      if (mEl) mEl.value = cfg.emulation_mode ?? "STATIC";
    }
  }

  // =================== RELAY: LOAD (supports REMUX) ===================
  async function loadRelaySettings(macFromClick) {
    const form = document.getElementById("relaySettingsForm");
    if (!form) return;

    const ctx = getRelayCtx(); // { mac, emuType:'REMUX'|null, vIndex }
    const mac = macFormatColon(macFromClick || ctx?.mac || "");
    const isEMU = ctx?.emuType === "REMUX";
    const idx = Number.isInteger(ctx?.vIndex) ? ctx.vIndex : 0;

    // Toggle emulator-only UI (modal only)
    if (isEMU) form.dataset.emu = "remux";
    else form.removeAttribute("data-emu");

    let cfg = null;

    if (isEMU) {
      // REMUX: shared + per-channel arrays
      try {
        cfg = JSON.parse(
          localStorage.getItem("icm_remux_cfg_" + mac) || "null"
        );
      } catch {}
      cfg = {
        ...(typeof remuxDefaults === "object" ? remuxDefaults : {}),
        ...(cfg || {}),
      };

      // Ensure arrays sized to relay_count
      const N = Math.max(1, Number(cfg.relay_count || 16));
      const ensureLen = (arr, fill) => {
        const a = Array.isArray(arr) ? arr.slice() : [];
        while (a.length < N) a.push(fill);
        return a;
      };
      cfg.relay_state = ensureLen(cfg.relay_state, false);
      cfg.relay_mode = ensureLen(cfg.relay_mode, "AUTO");
      cfg.pulse_duration = ensureLen(cfg.pulse_duration, 200);

      // Fill form (shared/common)
      if (form.rl_name) form.rl_name.value = cfg.name ?? "";
      if (form.rl_fan) form.rl_fan.value = cfg.fan_mode ?? "AUTO";
      if (form.rl_buzz) form.rl_buzz.value = String(!!cfg.buzzer_enabled);
      if (form.rl_time)
        form.rl_time.value = cfg.time_sync ? cfg.time_sync.slice(0, 19) : "";
      if (form.rl_tlimit) form.rl_tlimit.value = cfg.temperature_limit ?? 70;

      // Per-channel fields – focus on the selected index
      if (form.rl_mode) form.rl_mode.value = cfg.relay_mode[idx] ?? "AUTO";
      if (form.rl_pulse) form.rl_pulse.value = cfg.pulse_duration[idx] ?? 200;

      // Meta / emulator-only
      const dev = form.querySelector("#rl_devid");
      if (dev) dev.textContent = cfg.device_id || "—";
      if (form.rl_log) form.rl_log.value = cfg.log_level ?? "INFO";
      if (form.rl_auto) form.rl_auto.value = String(!!cfg.auto_restart);
      if (form.rl_led) form.rl_led.value = cfg.led_mode ?? "AUTO";

      const cEl = document.getElementById("rl_count");
      if (cEl) cEl.value = N;

      // Map state picker to *this channel* only; reuse rl_r1; disable rl_r2 in emulator
      if (form.rl_r1) form.rl_r1.value = String(!!cfg.relay_state[idx]);
      if (form.rl_r2) {
        form.rl_r2.disabled = true;
        form.rl_r2.parentElement?.classList.add("disabled");
      }

      const lbl = form.querySelector("label[for='rl_r1']");
      if (lbl) lbl.textContent = `Relay (Ch ${idx + 1}) State`;

      return;
    }

    // ---- Real Relay path (unchanged) ----
    let rCfg = null;
    try {
      const r = await fetch("/api/relay/settings/get", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mac }),
      });
      if (r.ok) rCfg = await r.json();
    } catch {}
    if (!rCfg) {
      try {
        rCfg = JSON.parse(
          localStorage.getItem("icm_relay_cfg_" + mac) || "null"
        );
      } catch {}
    }
    rCfg = {
      ...(typeof relayDefaults === "object" ? relayDefaults : {}),
      ...(rCfg || {}),
    };
    const draft = readDraft("relay", mac);
    if (draft) rCfg = { ...rCfg, ...draft };

    if (form.rl_name) form.rl_name.value = rCfg.name ?? "";
    if (form.rl_r1) form.rl_r1.value = String(!!rCfg.relay_1_state);
    if (form.rl_r2) form.rl_r2.value = String(!!rCfg.relay_2_state);
    if (form.rl_mode) form.rl_mode.value = rCfg.relay_mode ?? "AUTO";
    if (form.rl_fan) form.rl_fan.value = rCfg.fan_mode ?? "AUTO";
    if (form.rl_buzz) form.rl_buzz.value = String(!!rCfg.buzzer_enabled);
    if (form.rl_pulse) form.rl_pulse.value = rCfg.pulse_duration ?? 200;
    if (form.rl_time)
      form.rl_time.value = rCfg.time_sync ? rCfg.time_sync.slice(0, 19) : "";
    if (form.rl_tlimit) form.rl_tlimit.value = rCfg.temperature_limit ?? 70;

    const idEl = form.querySelector("#rl_devid");
    if (idEl) idEl.textContent = rCfg.device_id || "—";
    if (form.rl_log) form.rl_log.value = rCfg.log_level ?? "INFO";
    if (form.rl_auto) form.rl_auto.value = String(!!rCfg.auto_restart);
    if (form.rl_led) form.rl_led.value = rCfg.led_mode ?? "AUTO";
  }

  // =================== RELAY: SAVE (supports REMUX) ===================
  async function saveRelaySettings() {
    const form = document.getElementById("relaySettingsForm");
    const ctx = getRelayCtx();
    const mac = ctx?.mac;
    if (!form || !mac) {
      showToast("Select a relay first");
      return;
    }
    const isEMU = ctx?.emuType === "REMUX";
    const idx = Number.isInteger(ctx?.vIndex) ? ctx.vIndex : 0;

    if (isEMU) {
      // Load existing cfg, update shared + channel-specific
      let cfg = null;
      try {
        cfg = JSON.parse(
          localStorage.getItem("icm_remux_cfg_" + mac) || "null"
        );
      } catch {}
      cfg = {
        ...(typeof remuxDefaults === "object" ? remuxDefaults : {}),
        ...(cfg || {}),
      };

      cfg.name = (form.rl_name?.value || "").trim();
      cfg.fan_mode = String(form.rl_fan?.value || "AUTO");
      cfg.buzzer_enabled = (form.rl_buzz?.value || "false") === "true";
      cfg.time_sync = form.rl_time?.value
        ? new Date(form.rl_time.value).toISOString()
        : "";
      cfg.temperature_limit = Number(form.rl_tlimit?.value || 70);
      cfg.log_level = String(form.rl_log?.value || "INFO");
      cfg.auto_restart = (form.rl_auto?.value || "false") === "true";
      cfg.led_mode = String(form.rl_led?.value || "AUTO");

      const N = Math.max(
        1,
        Number(
          document.getElementById("rl_count")?.value || cfg.relay_count || 16
        )
      );
      cfg.relay_count = N;

      // Ensure arrays and patch this channel index
      const ensure = (arr, fill) => {
        const a = Array.isArray(arr) ? arr.slice() : [];
        while (a.length < N) a.push(fill);
        return a.slice(0, N); // keep exactly N
      };
      cfg.relay_state = ensure(cfg.relay_state, false);
      cfg.relay_mode = ensure(cfg.relay_mode, "AUTO");
      cfg.pulse_duration = ensure(cfg.pulse_duration, 200);

      cfg.relay_state[idx] = (form.rl_r1?.value || "false") === "true";
      cfg.relay_mode[idx] = String(form.rl_mode?.value || "AUTO");
      cfg.pulse_duration[idx] = Math.max(
        10,
        Number(form.rl_pulse?.value || 200)
      );

      // Persist locally
      try {
        localStorage.setItem("icm_remux_cfg_" + mac, JSON.stringify(cfg));
      } catch {}
      clearDraft("relay", mac);
      closeRelaySettings();
      renderPalette(); // counts may have changed
      showToast("REMUX settings saved");
      return;
    }

    // ---- Real Relay path (unchanged) ----
    const rcfg = {
      name: (form.rl_name?.value || "").trim(),
      relay_1_state: (form.rl_r1?.value || "false") === "true",
      relay_2_state: (form.rl_r2?.value || "false") === "true",
      relay_mode: String(form.rl_mode?.value || "AUTO"),
      fan_mode: String(form.rl_fan?.value || "AUTO"),
      buzzer_enabled: (form.rl_buzz?.value || "false") === "true",
      pulse_duration: Math.max(10, Number(form.rl_pulse?.value || 200)),
      time_sync: form.rl_time?.value
        ? new Date(form.rl_time.value).toISOString()
        : "",
      temperature_limit: Number(form.rl_tlimit?.value || 70),
      log_level: String(form.rl_log?.value || "INFO"),
      auto_restart: (form.rl_auto?.value || "false") === "true",
      led_mode: String(form.rl_led?.value || "AUTO"),
    };

    let ok = false;
    try {
      const r = await fetch("/api/relay/settings/set", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mac, cfg: rcfg }),
      });
      ok = r.ok;
    } catch {}
    try {
      localStorage.setItem("icm_relay_cfg_" + mac, JSON.stringify(rcfg));
      if (!ok) ok = true;
    } catch {}
    showToast(ok ? "Settings saved" : "Save failed");
    if (ok) {
      clearDraft("relay", mac);
      closeRelaySettings();
    }
  }

  function getSelectedSensorMac() {
    // Prefer selected brick; else selectedPeer; else pollTarget
    if (selectedId != null) {
      const b = bricks.find((x) => x.id === selectedId);
      if (b && String(b.kind).toUpperCase() === "SENSOR")
        return macFormatColon(b.mac);
    }
    if (selectedPeer && String(selectedPeer.type).toLowerCase() === "sensor")
      return macFormatColon(selectedPeer.mac);
    if (pollTarget && pollTarget.kindUp === "SENSOR")
      return macFormatColon(pollTarget.mac);
    return null;
  }

  function openSensorSettings() {
    const modal = document.getElementById("sensorSettingsModal");
    if (!modal) return;
    modal.setAttribute("aria-hidden", "false");
    // Load values each time we open
    const mac = getSelectedSensorMac();
    loadSensorSettings(mac).catch(() => {});
  }

  function closeSensorSettings() {
    const modal = document.getElementById("sensorSettingsModal");
    if (!modal) return;
    modal.setAttribute("aria-hidden", "true");
  }

  function snapshotPmsFormToDraft() {
    const form = document.getElementById("pmsSettingsForm");
    const mac = getSelectedPowerMac();
    if (!form || !mac) return;
    const draft = {
      name: form.pms_name.value.trim(),
      power_mode: String(form.pms_mode.value || "AUTO"),
      relay_state: [
        document.getElementById("pms_r1").checked,
        document.getElementById("pms_r2").checked,
        document.getElementById("pms_r3").checked,
        document.getElementById("pms_r4").checked,
      ],
      fan_mode: String(form.pms_fan.value || "AUTO"),
      buzzer_enabled: form.pms_buzz.value === "true",
      time_sync: form.pms_time.value
        ? new Date(form.pms_time.value).toISOString()
        : "",
      temperature_limit: Number(form.pms_tlimit.value || 70),
      voltage_calibration: Number(form.pms_vcal.value || 0),
      current_calibration: Number(form.pms_ical.value || 0),
      log_level: String(form.pms_log.value || "INFO"),
      auto_restart: form.pms_auto.value === "true",
      led_mode: String(form.pms_led.value || "AUTO"),
    };
    writeDraft("pms", mac, draft);
  }

  function snapshotRelayFormToDraft() {
    const form = document.getElementById("relaySettingsForm");
    const mac = getSelectedRelayMac();
    if (!form || !mac) return;
    const draft = {
      name: form.rl_name.value.trim(),
      relay_1_state: form.rl_r1.value === "true",
      relay_2_state: form.rl_r2.value === "true",
      relay_mode: String(form.rl_mode.value || "AUTO"),
      fan_mode: String(form.rl_fan.value || "AUTO"),
      buzzer_enabled: form.rl_buzz.value === "true",
      pulse_duration: Math.max(10, Number(form.rl_pulse.value || 200)),
      time_sync: form.rl_time.value
        ? new Date(form.rl_time.value).toISOString()
        : "",
      temperature_limit: Number(form.rl_tlimit.value || 70),
      log_level: String(form.rl_log.value || "INFO"),
      auto_restart: form.rl_auto.value === "true",
      led_mode: String(form.rl_led.value || "AUTO"),
    };
    writeDraft("relay", mac, draft);
  }

  function snapshotSensorFormToDraft() {
    const form = document.getElementById("sensorSettingsForm");
    const mac = getSelectedSensorMac();
    if (!form || !mac) return;
    const draft = {
      name: form.ss_name.value.trim(),
      tf_luna_threshold_A: Number(form.ss_tf_a.value || 0),
      tf_luna_threshold_B: Number(form.ss_tf_b.value || 0),
      day_night_threshold: Number(form.ss_dn.value || 0),
      trigger_mode: String(form.ss_trigger.value || "TOGGLE"),
      fan_mode: String(form.ss_fan.value || "AUTO"),
      buzzer_enabled: form.ss_buzzer.value === "true",
      report_interval: Math.max(250, Number(form.ss_report.value || 1000)),
      time_sync: form.ss_time.value
        ? new Date(form.ss_time.value).toISOString()
        : "",
      temperature_calibration: Number(form.ss_temp_cal.value || 0),
      pressure_offset: Number(form.ss_press_off.value || 0),
      log_level: String(form.ss_log.value || "INFO"),
      auto_restart: form.ss_auto.value === "true",
      led_mode: String(form.ss_led.value || "AUTO"),
    };
    writeDraft("sensor", mac, draft);
  }

  // ---------- Class helpers ----------
  function setStateClass(el, level) {
    if (!el) return;
    el.classList.remove("state-ok", "state-warn", "state-danger");
    if (level) el.classList.add(`state-${level}`);
  }
  function levelHi(v, warn, danger) {
    if (!Number.isFinite(v)) return null;
    if (v >= danger) return "danger";
    if (v >= warn) return "warn";
    return "ok";
  }
  function levelBand(v, { warnLow, warnHigh, dangerLow, dangerHigh }) {
    if (!Number.isFinite(v)) return null;
    if (v <= dangerLow || v >= dangerHigh) return "danger";
    if (v <= warnLow || v >= warnHigh) return "warn";
    return "ok";
  }
  function setLoading(el, on) {
    if (!el) return;
    el.classList.toggle("loading", !!on);
  }

  // ------------------------ MAC helpers ------------------------
  const macNormalize12 = (s) =>
    String(s || "")
      .replace(/[^0-9a-f]/gi, "")
      .toUpperCase()
      .slice(0, 12);

  function macFormatColon(s12) {
    const h = macNormalize12(s12);
    return h.match(/.{1,2}/g)?.join(":") ?? "";
  }
  const macIsCompleteColon = (s) => /^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(s);
  const macEqual = (a, b) => macNormalize12(a) === macNormalize12(b);

  function bindMacFormatter(inputEl) {
    if (!inputEl) return;
    inputEl.setAttribute("maxlength", "17");
    inputEl.setAttribute("autocomplete", "off");
    inputEl.setAttribute("spellcheck", "false");
    inputEl.placeholder = "AA:BB:CC:DD:EE:FF";
    const fmt = () => (inputEl.value = macFormatColon(inputEl.value));
    inputEl.addEventListener("input", fmt);
    inputEl.addEventListener("blur", fmt);
    inputEl.addEventListener("paste", (e) => {
      e.preventDefault();
      const text = (e.clipboardData || window.clipboardData).getData("text");
      inputEl.value = macFormatColon(text);
    });
  }
  bindMacFormatter(pMac);
  bindMacFormatter(rMac);

  // ------------------------ Gauges ------------------------
  function setGauge(el, value, min, max, unit) {
    if (!el) return;
    const v = Number(value ?? 0);
    const lo = Number(min ?? 0);
    const hi = Number(max ?? 100);
    const pct = Math.max(0, Math.min(1, (v - lo) / (hi - lo)));
    el.style.setProperty("--val", v.toString());
    el.style.setProperty("--min", lo.toString());
    el.style.setProperty("--max", hi.toString());
    el.style.setProperty("--pct", pct.toString());
    el.innerHTML = `<div class="value">${
      Number.isFinite(v)
        ? unit === "V" || unit === "A"
          ? v.toFixed(1)
          : v.toFixed(0)
        : "—"
    }<small>${unit ? " " + unit : ""}</small></div>`;
  }

  function renderDayNight(isDay) {
    if (!siDN) return;
    siDN.textContent = isDay === 1 ? "Day" : isDay === 0 ? "Night" : "—";
    siDN.classList.remove("day", "night");
    if (isDay === 1) siDN.classList.add("day");
    if (isDay === 0) siDN.classList.add("night");
  }

  // ------------------------ State ------------------------
  let bricks = []; // [{id, kind, mac}]
  let brickSeq = 1;
  let selectedId = null;
  let selectedPeer = null; // {type, mac}
  let peers = []; // /api/peers/list
  let pollTimer = null;
  let pollTarget = null; // {kindUp, mac}

  const KIND_UP = (t) => String(t || "").toUpperCase();

  // If you already have KIND_UP, add a thin base-kind mapper:
  const BASE_KIND = (k) => {
    const u = String(k || "").toUpperCase();
    if (u === "SEMUX") return "SENSOR";
    if (u === "REMUX") return "RELAY";
    return u;
  };
  const kindLabel = (k) =>
    ({
      POWER: "Power",
      RELAY: "Relay",
      SENSOR: "Sensor",
    }[k] || k);
  const kindTag = (k) =>
    ({
      POWER: "PWR",
      RELAY: "REL",
      SENSOR: "SNS",
    }[k] || k);
  function isChainPlaceable(kindOrType) {
    // SEMUX should behave like SENSOR; REMUX like RELAY
    const base = BASE_KIND(KIND_UP(kindOrType));
    return base === "SENSOR" || base === "RELAY";
  }

  // ------------------------ Toast ------------------------
  const showToast = (msg) => {
    if (!toast) return;
    toast.textContent = msg || "OK";
    toast.hidden = false;
    setTimeout(() => (toast.hidden = true), 1400);
  };

  // ------------------------ Global selection/unselection ------------------------
  function unselectAll() {
    selectedId = null;
    selectedPeer = null;
    document
      .querySelectorAll(".brick.selected")
      .forEach((e) => e.classList.remove("selected"));
    document
      .querySelectorAll(".pill.selected")
      .forEach((e) => e.classList.remove("selected"));

    // ADD:
    clearEmuChanSelection();

    clearPeerRowSelection();
    clearSelectionPanel();
    stopPolling();
  }

  // Click anywhere outside selectable things → unselect
  document.addEventListener("click", (e) => {
    const safe = e.target.closest(
      ".brick, .pill, #peersTbl tbody tr, .card-selected, .card-palette, .card-chain, .card-peers, .card-pair, .modal"
    );
    if (!safe) unselectAll();
  });

  // ------------------------ Table/Palette selection helpers ------------------------
  function clearPeerRowSelection() {
    peersTbl
      ?.querySelectorAll("tbody tr")
      .forEach((tr) => tr.classList.remove("selected"));
  }
  function selectPeerRowByMac(mac) {
    if (!peersTbl) return;
    const macFmt = macFormatColon(mac);
    let found = null;
    peersTbl.querySelectorAll("tbody tr").forEach((tr) => {
      const tdMac = tr.querySelector("td:nth-child(2)");
      const same = (tdMac?.textContent || "").trim().toUpperCase() === macFmt;
      if (same) {
        tr.classList.add("selected");
        found = tr;
      } else tr.classList.remove("selected");
    });
    if (found && peersScroll)
      found.scrollIntoView({ block: "nearest", behavior: "smooth" });
  }
  function selectPalettePillByMac(mac) {
    const macFmt = macFormatColon(mac);
    let found = null;
    document.querySelectorAll(".pill").forEach((pill) => {
      const m = pill
        .querySelector(".pill-mac")
        ?.textContent?.trim()
        ?.toUpperCase();
      if (m === macFmt) {
        pill.classList.add("selected");
        found = pill;
      } else pill.classList.remove("selected");
    });
    if (found) found.scrollIntoView({ block: "nearest", behavior: "smooth" });
  }
  function scrollBrickIntoView(id) {
    const el = [...document.querySelectorAll(".brick")].find(
      (e) => e.dataset.id == id
    );
    if (el)
      el.scrollIntoView({
        block: "nearest",
        inline: "nearest",
        behavior: "smooth",
      });
  }

  // ------------------------ Unified polling ------------------------
  function stopPolling() {
    if (pollTimer) {
      clearInterval(pollTimer);
      pollTimer = null;
    }
    pollTarget = null;
  }

  function startPolling(kindUp, mac) {
    stopPolling();
    if (!macIsCompleteColon(mac)) return;

    pollTarget = { kindUp: KIND_UP(kindUp), mac: macFormatColon(mac) };

    const tick = async () => {
      if (!pollTarget) return;
      const t = pollTarget.kindUp;
      if (t === "POWER") {
        await readPower().catch(() => {});
      } else if (t === "RELAY") {
        // Demo animation for relay temp
        const base = Number(siRelState.textContent === "ON" ? 36 : 26);
        setGauge(gRelTemp, base + Math.random() * 4, -20, 100, "°C");
      } else if (t === "SENSOR") {
        await readSensor({ mac: pollTarget.mac, type: t.toLowerCase() }).catch(
          () => {}
        );
      }
    };

    tick();
    pollTimer = setInterval(tick, 1000);
  }

  // ------------------------ Selection panel ------------------------
  function updateSelectionPanel(kindUp, mac) {
    siKind.textContent = kindLabel(kindUp);
    siMac.textContent = macFormatColon(mac) || "—";

    panelSensor.hidden = panelRelay.hidden = panelPower.hidden = true;

    if (kindUp === "SENSOR") {
      panelSensor.hidden = false;
      setLoading(panelSensor, true); // show loading tint/shimmer
      setGauge(gSensTemp, 0, -20, 80, "°C"); // initial gauge scaffold
      renderDayNight(undefined); // DN pill placeholder
      // Optional: clear old values visually
      [siHum, siPress, siLux, siTfA, siTfB, siSensStatus].forEach(
        (n) => n && (n.textContent = "—")
      );
    } else if (kindUp === "RELAY") {
      panelRelay.hidden = false;
      setGauge(gRelTemp, 0, -20, 100, "°C");
      siRelState.textContent = siRelState.textContent || "OFF";
    } else if (kindUp === "POWER") {
      panelPower.hidden = false;
      setGauge(gPwrVolt, 0, 0, 60, "V");
      setGauge(gPwrCurr, 0, 0, 10, "A");
      setGauge(gPwrTemp, 0, 0, 100, "°C");
    }

    selNone.hidden = true;
    selInfo.hidden = false;
    showTypeMac(true);
    startPolling(kindUp, mac);
  }
  function showTypeMac(show) {
    const wrap = (el) => el?.closest?.(".kv") || el?.parentElement || null;
    const kindWrap = wrap(siKind);
    const macWrap = wrap(siMac);
    if (kindWrap) kindWrap.style.display = show ? "" : "none";
    if (macWrap) macWrap.style.display = show ? "" : "none";
  }

  function clearSelectionPanel() {
    selInfo.hidden = true;
    selNone.hidden = false;
    panelSensor.hidden = panelRelay.hidden = panelPower.hidden = true;
    showTypeMac(false);
  }

  // ------------------------ Lane (chain) rendering & DnD ------------------------
  function redrawLane() {
    lane.innerHTML = "";
    if (!bricks.length) {
      const hint = document.createElement("div");
      hint.className = "lane-hint";
      hint.innerHTML =
        "Drag paired items here in order. The order defines <b>prev/next</b>.";
      lane.appendChild(hint);
      return;
    }
    bricks.forEach((b, i) => {
      const el = document.createElement("div");
      el.className = "brick " + String(b.kind || "").toLowerCase();
      el.draggable = true;
      el.dataset.id = b.id;

      const tag = document.createElement("span");
      tag.className = "tag";
      tag.textContent = kindTag(b.kind);

      const title = document.createElement("span");
      title.className = "title";
      // Tag: SMU/RMU for emulator children; else SNS/REL/PWR
      const macDisp = macFormatColon(b.mac);
      const isSemu = b?.vType === "SEMUX";
      const isRemu = b?.vType === "REMUX";
      tag.textContent = isSemu ? "SMU" : isRemu ? "RMU" : kindTag(b.kind);

      // Color hint class on the brick (optional visual)
      if (isSemu) el.classList.add("semux-virt");
      if (isRemu) el.classList.add("remux-virt");

      // Title: include channel position for emulator children
      let titleText = `${kindLabel(b.kind)} · ${macDisp}`;
      if (isSemu || isRemu) {
        const total = isSemu ? getSemuxCount(macDisp) : getRemuxCount(macDisp);
        titleText = `${isSemu ? "-" : "-"} ${
          (b.vIndex ?? 0) + 1
        } / ${total} · ${macDisp}`;
      }
      title.textContent = titleText;

      el.appendChild(tag);
      el.appendChild(title);
      el.title = macDisp || "";

      el.addEventListener("click", (ev) => {
        ev.stopPropagation();
        selectBrick(b.id);
      });
      el.addEventListener("dragstart", (ev) => {
        ev.dataTransfer.setData("text/icm-brick", String(b.id));
        ev.dataTransfer.effectAllowed = "move";
      });

      if (selectedId === b.id) el.classList.add("selected");

      lane.appendChild(el);
      if (i < bricks.length - 1) {
        const curr = b.kind.toUpperCase();
        const next = bricks[i + 1].kind.toUpperCase();
        const isRelay = (k) => k === "RELAY";
        const isSensorish = (k) => k === "SENSOR";
        if (
          (isRelay(curr) && isSensorish(next)) ||
          (isSensorish(curr) && isRelay(next))
        ) {
          const conn = document.createElement("div");
          conn.className = "connector";
          lane.appendChild(conn);
        }
      }
    });
  }

  function selectBrick(id) {
    selectedId = id;
    selectedPeer = null;

    // Clear previous visual selections
    document
      .querySelectorAll(".brick")
      .forEach((e) => e.classList.remove("selected"));
    document
      .querySelectorAll(".pill.selected")
      .forEach((e) => e.classList.remove("selected"));
    clearEmuChanSelection && clearEmuChanSelection();

    const b = bricks.find((x) => x.id === id);
    const el = [...document.querySelectorAll(".brick")].find(
      (e) => e.dataset.id == id
    );

    if (el) el.classList.add("selected");
    if (!b) {
      unselectAll && unselectAll();
      return;
    }

    // Determine base kind and emulator info
    const baseKind = BASE_KIND(KIND_UP(b.kind)); // -> "SENSOR" | "RELAY" | "POWER"
    const vType = b?.vType ? String(b.vType).toLowerCase() : null; // "semux" | "remux" | null
    const isEmu = vType === "semux" || vType === "remux";
    const vIndex = Number.isInteger(b?.vIndex) ? b.vIndex : null;
    const macFmt = macFormatColon(b.mac);

    // If emulator child brick -> highlight matching palette item and open its parent
    if (isEmu && Number.isInteger(vIndex)) {
      // add selected class on the exact child
      if (typeof highlightEmuChan === "function") {
        highlightEmuChan(b.mac, vType, vIndex);
      } else {
        // fallback highlight if helper not present
        const childSel = `.emu-chan[data-mac="${macFmt}"][data-vtype="${vType}"][data-vindex="${vIndex}"]`;
        const childEl = document.querySelector(childSel);
        if (childEl) childEl.classList.add("selected");
      }
      // ensure dropdown is open
      const childEl = document.querySelector(
        `.emu-chan[data-mac="${macFmt}"][data-vtype="${vType}"][data-vindex="${vIndex}"]`
      );
      if (childEl) {
        const list = childEl.closest(".emu-channels");
        const parent = list && list.previousElementSibling;
        if (list) list.hidden = false;
        if (parent && parent.classList.contains("parent-pill")) {
          parent.setAttribute("aria-expanded", "true");
        }
      }
    }

    // Toggle emulator visibility in Selected panel
    if (typeof selInfo !== "undefined" && selInfo) {
      if (isEmu) selInfo.setAttribute("data-emu", vType);
      else selInfo.removeAttribute("data-emu");
    }

    // Update right panel + side lists
    updateSelectionPanel(baseKind, b.mac);
    selectPeerRowByMac && selectPeerRowByMac(b.mac);
    selectPalettePillByMac && selectPalettePillByMac(b.mac);

    // Make sure it’s visible in the lane
    scrollBrickIntoView && scrollBrickIntoView(id);
  }

  function findAfterId(clientY) {
    const rect = lane.getBoundingClientRect();
    const y = clientY - rect.top + lane.scrollTop;
    let bestDy = Infinity,
      bestId = null;
    const elems = [...lane.querySelectorAll(".brick")];
    if (!elems.length) return null;
    elems.forEach((el) => {
      const r = el.getBoundingClientRect();
      const mid = r.top - rect.top + lane.scrollTop + r.height / 2;
      const dy = Math.abs(y - mid);
      if (dy < bestDy) {
        bestDy = dy;
        bestId = parseInt(el.dataset.id, 10);
      }
    });
    const bestEl = elems.find((e) => parseInt(e.dataset.id, 10) === bestId);
    if (!bestEl) return null;
    const br = bestEl.getBoundingClientRect();
    const midBest = br.top - rect.top + lane.scrollTop + br.height / 2;
    return y >= midBest ? bestId : getPrevId(bestId);
  }
  const getPrevId = (id) => {
    const i = bricks.findIndex((b) => b.id === id);
    return i <= 0 ? null : bricks[i - 1].id;
  };

  function reorderBrick(srcId, afterId) {
    const iSrc = bricks.findIndex((b) => b.id === srcId);
    if (iSrc < 0) return;
    const node = bricks.splice(iSrc, 1)[0];
    if (afterId === null) bricks.splice(0, 0, node);
    else {
      const iAfter = bricks.findIndex((b) => b.id === afterId);
      bricks.splice(iAfter + 1, 0, node);
    }
    redrawLane();
    scrollBrickIntoView(node.id);
  }

  // Lane DnD target
  lane.addEventListener("dragover", (ev) => ev.preventDefault());
  lane.addEventListener("drop", (ev) => {
    ev.preventDefault();
    // New: Emulator channel drop (SEMUX/REMUX)
    const emuJson = ev.dataTransfer.getData(DND_EMU_CHAN);
    if (emuJson) {
      try {
        const d = JSON.parse(emuJson); // { type: 'semux'|'remux', mac, vIndex }
        if (!d?.mac || (d.type !== "semux" && d.type !== "remux")) return;
        const mac = macFormatColon(d.mac);
        const addr = `${mac}#${d.type === "semux" ? "S" : "R"}${d.vIndex}`;

        // Prevent duplicates of the same channel
        if (bricks.some((b) => b.addr === addr)) {
          showToast("Channel already placed");
          return;
        }

        // Create brick (kind stays SENSOR/RELAY so the rest of UI works)
        const kindUp = d.type === "semux" ? "SENSOR" : "RELAY";
        bricks.push({
          id: brickSeq++,
          kind: kindUp,
          mac,
          vType: d.type.toUpperCase(), // "SEMUX"/"REMUX"
          vIndex: Number(d.vIndex),
          addr, // "<MAC>#S3" or "<MAC>#R12"
        });

        redrawLane();
        renderPalette(); // refresh dropdown → channel becomes disabled
        return;
      } catch {
        // ignore malformed payloads
      }
    }
    const peerJson = ev.dataTransfer.getData("text/icm-peer");

    if (peerJson) {
      try {
        const p = JSON.parse(peerJson);
        if (!p.mac || !p.type) return;
        const typeUp = KIND_UP(p.type);
        if (!isChainPlaceable(typeUp)) {
          showToast("Power modules are not placed in chain");
          return;
        }
        if (bricks.some((b) => macEqual(b.mac, p.mac))) {
          showToast("Already in chain");
          return;
        }
        const baseKind = BASE_KIND(typeUp); // SENSOR for SEMUX, RELAY for REMUX
        const t = String(p.type).toLowerCase();
        bricks.push({
          id: brickSeq++,
          kind: baseKind,
          mac: macFormatColon(p.mac),
          // optional visual tag so emulator bricks can get a special color
          emu: t === "semux" || t === "remux",
          emuType: t === "semux" ? "SEMUX" : t === "remux" ? "REMUX" : null,
        });
        redrawLane();
        renderPalette();
      } catch {}
    }
  });

  btnClear?.addEventListener("click", (e) => {
    e.stopPropagation();
    bricks = [];
    unselectAll();
    redrawLane();
    renderPalette();
  });
  // ---- Emulator helpers (SEMUX/REMUX) ----
  const DND_EMU_CHAN = "text/icm-emu-chan";

  function getSemuxCount(mac) {
    try {
      const cfg = JSON.parse(
        localStorage.getItem("icm_semux_cfg_" + mac) || "{}"
      );
      const n = parseInt(cfg.sensor_count ?? 8, 10);
      return Number.isFinite(n) && n > 0 ? n : 8;
    } catch {
      return 8;
    }
  }
  function getRemuxCount(mac) {
    try {
      const cfg = JSON.parse(
        localStorage.getItem("icm_remux_cfg_" + mac) || "{}"
      );
      const n = parseInt(cfg.relay_count ?? 16, 10);
      return Number.isFinite(n) && n > 0 ? n : 16;
    } catch {
      return 16;
    }
  }

  /** Scan current lane and return used channel indices per MAC. */
  function collectUsedChannels() {
    const used = new Map(); // mac -> { S:Set, R:Set }
    bricks.forEach((b) => {
      if (b?.vType === "SEMUX" || b?.vType === "REMUX") {
        const key = b.vType === "SEMUX" ? "S" : "R";
        const mac = macFormatColon(b.mac);
        const bucket = used.get(mac) || { S: new Set(), R: new Set() };
        if (Number.isInteger(b.vIndex)) bucket[key].add(b.vIndex);
        used.set(mac, bucket);
      }
    });
    return used;
  }

  /** Append one emulator group (REMUX or SEMUX) with a parent pill and a dropdown of channels. */
  function appendEmuGroup(root, title, items, vType /* 'semux' | 'remux' */) {
    if (!items?.length) return;

    const group = document.createElement("div");
    group.className = "palette-group";

    const head = document.createElement("div");
    head.className = "palette-title";
    head.textContent = title;
    group.appendChild(head);

    const usedMap = collectUsedChannels();

    items.forEach((p) => {
      const mac = macFormatColon(p.mac || "");
      const total = vType === "semux" ? getSemuxCount(mac) : getRemuxCount(mac);
      const used = usedMap.get(mac) || { S: new Set(), R: new Set() };
      const set = vType === "semux" ? used.S : used.R;
      const avail = total - set.size;

      // Parent pill (not draggable) — TOGGLES + SELECTS
      const parent = document.createElement("button");
      parent.type = "button";
      parent.className = `pill parent-pill ${
        vType === "semux" ? "t-semux" : "t-remux"
      }`;
      parent.setAttribute("aria-expanded", "false");
      parent.innerHTML =
        `<span class="pill-title">${
          vType === "semux" ? "SEMUX" : "REMUX"
        }</span>` +
        `<span class="pill-mac mono">${mac}</span>` +
        `<span class="pill-count">${avail}/${total}</span>`;

      const list = document.createElement("div");
      list.className = "emu-channels";
      list.hidden = true;

      parent.addEventListener("click", (ev) => {
        ev.stopPropagation();
        // Toggle open/closed
        const open = parent.getAttribute("aria-expanded") === "true";
        parent.setAttribute("aria-expanded", String(!open));
        list.hidden = open;

        // SELECT parent (base kind: SENSOR for SEMUX, RELAY for REMUX)
        selectedId = null;
        selectedPeer = { type: vType, mac }; // parent (no vIndex here)
        const baseKind = BASE_KIND(KIND_UP(vType)); // "SENSOR" or "RELAY"
        updateSelectionPanel(baseKind, mac);
        selectPeerRowByMac(mac);
        selectPalettePillByMac(mac);
        // NOTE: do NOT call highlightEmuChan here (no vIndex).
      });

      // Channel items (draggable + SELECT on click)
      for (let i = 0; i < total; i++) {
        const disabled = set.has(i);
        const tag = vType === "semux" ? "SMU" : "RMU";

        const btn = document.createElement("button");
        btn.type = "button";
        btn.className = `emu-chan ${vType === "semux" ? "t-semux" : "t-remux"}${
          disabled ? " disabled" : ""
        }`;
        btn.draggable = !disabled;
        btn.dataset.mac = mac;
        btn.dataset.vtype = vType;
        btn.dataset.vindex = String(i);
        btn.innerHTML = `<span class="tag">${tag}</span><span class="title">${tag} ${
          i + 1
        } / ${total}</span>`;

        if (!disabled) {
          // Drag payload
          btn.addEventListener("dragstart", (ev) => {
            ev.dataTransfer.setData(
              DND_EMU_CHAN,
              JSON.stringify({ type: vType, mac, vIndex: i })
            );
            ev.dataTransfer.effectAllowed = "copy";
          });

          // CLICK → select this channel (so settings know the index)
          btn.addEventListener("click", (ev) => {
            ev.stopPropagation();
            selectedId = null;
            // Carry vIndex on selectedPeer so getRelayCtx/getSensorCtx can read it
            selectedPeer = { type: vType, mac, vIndex: i };
            const baseKind = BASE_KIND(KIND_UP(vType)); // "SENSOR" or "RELAY"
            updateSelectionPanel(baseKind, mac);
            selectPeerRowByMac(mac);
            selectPalettePillByMac(mac);
          });
        }

        list.appendChild(btn);
      }

      group.appendChild(parent);
      group.appendChild(list);
    });

    root.appendChild(group);
  }

  function renderPalette() {
    if (!palette) return;
    palette.innerHTML = "";

    const typeOf = (p) => String(p.type || "").toLowerCase();
    const notUsed = (p) => !bricks.some((b) => macEqual(b.mac, p.mac)); // real devices only

    // ---- legacy groups (Power / Sensors / Relays) ----
    const powerPeers = peers.filter((p) => typeOf(p) === "power");
    appendGroup(
      palette,
      "Power (not placeable)",
      powerPeers.map((p) => makePill(p, "Power", "t-power"))
    );

    const sensors = peers
      .filter((p) => typeOf(p) === "sensor" && notUsed(p))
      .map((p) => makePill(p, "Sensor", "t-sens"));
    appendGroup(palette, "Sensors", sensors);

    const relays = peers
      .filter((p) => typeOf(p) === "relay" && notUsed(p))
      .map((p) => makePill(p, "Relay", "t-relay"));
    appendGroup(palette, "Relays", relays);

    // ---- emulator groups (always listed; no notUsed filter) ----
    // NOTE: appendEmuGroup builds parent pill + dropdown and wires click/drag
    appendEmuGroup(
      palette,
      "Relay Emulators (REMUX)",
      peers.filter((p) => typeOf(p) === "remux"),
      "remux"
    );
    appendEmuGroup(
      palette,
      "Sensor Emulators (SEMUX)",
      peers.filter((p) => typeOf(p) === "semux"),
      "semux"
    );

    // ---- restore selection/highlight AFTER DOM exists ----
    if (
      selectedPeer &&
      (selectedPeer.type === "semux" || selectedPeer.type === "remux")
    ) {
      const macFmt = macFormatColon(selectedPeer.mac);
      const vType = selectedPeer.type;

      // If a specific channel is selected, ensure its dropdown is open and highlight it.
      if (Number.isInteger(selectedPeer.vIndex)) {
        highlightEmuChan(
          selectedPeer.mac,
          selectedPeer.type,
          selectedPeer.vIndex
        );
        const childSel = `.emu-chan[data-mac="${macFmt}"][data-vtype="${vType}"][data-vindex="${selectedPeer.vIndex}"]`;
        const childEl = palette.querySelector(childSel);

        if (childEl) {
          // open its parent dropdown
          const list = childEl.closest(".emu-channels");
          const parent = list && list.previousElementSibling;
          if (parent && parent.classList.contains("parent-pill")) {
            parent.setAttribute("aria-expanded", "true");
          }
          if (list) list.hidden = false;

          // highlight selected child
          childEl.classList.add("selected");
        }
      } else {
        // Parent-only selection: open its dropdown and highlight the parent pill
        const anyChild = palette.querySelector(
          `.emu-chan[data-mac="${macFmt}"][data-vtype="${vType}"]`
        );
        const list = anyChild && anyChild.closest(".emu-channels");
        const parent = list && list.previousElementSibling;
        if (parent && parent.classList.contains("parent-pill")) {
          parent.setAttribute("aria-expanded", "true");
          parent.classList.add("selected");
        }
        if (list) list.hidden = false;
      }
    }
  }

  // ------------------------ Palette (grouped) ------------------------
  const isUsed = (mac) => bricks.some((b) => macEqual(b.mac, mac));

  function makePill(p, titleText, cls) {
    const pill = document.createElement("button");
    pill.className = "pill " + cls;
    const fullMac = macFormatColon(p.mac || "");
    pill.innerHTML = `<span class="pill-title">${titleText} . ${fullMac}</span>`;

    // Draggable only if placeable (SENSOR/RELAY or their emulators)
    pill.draggable = isChainPlaceable(p.type);
    pill.addEventListener("dragstart", (ev) => {
      ev.dataTransfer.setData(
        "text/icm-peer",
        JSON.stringify({ type: p.type, mac: fullMac })
      );
      ev.dataTransfer.effectAllowed = "copy";
    });

    // Click: select peer and open correct Selected panel
    pill.addEventListener("click", (ev) => {
      ev.stopPropagation();
      selectedId = null;
      selectedPeer = { type: p.type, mac: fullMac };
      const baseKind = BASE_KIND(KIND_UP(p.type)); // SENSOR for SEMUX, RELAY for REMUX
      updateSelectionPanel(baseKind, fullMac);
      selectPeerRowByMac(fullMac);
      selectPalettePillByMac(fullMac);
    });

    return pill;
  }

  // ---- shared helpers for legacy groups ----
  function appendGroup(root, title, items) {
    if (!items || !items.length) return;
    const group = document.createElement("div");
    group.className = "palette-group";
    const t = document.createElement("div");
    t.className = "palette-title";
    t.textContent = title;
    group.appendChild(t);
    items.forEach((el) => group.appendChild(el));
    root.appendChild(group);
  }

  // ------------------------ Peers ------------------------
  async function fetchPeers() {
    const r = await fetch("/api/peers/list", { cache: "no-store" });
    if (!r.ok) throw new Error("peers list");
    const j = await r.json();
    peers = Array.isArray(j.peers) ? j.peers : [];
    renderPeers();
    renderPalette();
  }

  function renderPeers() {
    if (!peersTblBody) return;
    const q = (peerSearch?.value || "").trim().toUpperCase();
    peersTblBody.innerHTML = "";
    const filtered = peers.filter((p) => {
      const mac = macFormatColon(p.mac || "").toUpperCase();
      return !q || mac.includes(q);
    });
    filtered.forEach((p) => {
      const tr = document.createElement("tr");
      tr.draggable = isChainPlaceable(p.type);
      if (tr.draggable) {
        tr.addEventListener("dragstart", (ev) => {
          ev.dataTransfer.setData(
            "text/icm-peer",
            JSON.stringify({ type: p.type, mac: macFormatColon(p.mac || "") })
          );
          ev.dataTransfer.effectAllowed = "copy";
        });
      }

      const tdT = document.createElement("td");
      tdT.textContent = (p.type || "").toUpperCase();
      const tdM = document.createElement("td");
      tdM.textContent = macFormatColon(p.mac || "") || "—";
      const tdO = document.createElement("td");
      tdO.textContent = p.online ? "Yes" : "No";

      const tdTest = document.createElement("td");
      const base = BASE_KIND(KIND_UP(p.type)); // RELAY for REMUX, SENSOR for SEMUX
      if (base === "RELAY") {
        const bOn = document.createElement("button");
        bOn.className = "btn";
        bOn.textContent = "ON";
        const bOff = document.createElement("button");
        bOff.className = "btn";
        bOff.textContent = "OFF";
        bOn.addEventListener("click", (ev) => {
          ev.stopPropagation();
          testRelay({ type: p.type, mac: macFormatColon(p.mac || "") });
        });
        bOff.addEventListener("click", (ev) => {
          ev.stopPropagation();
          stopSeq();
        });
        tdTest.appendChild(bOn);
        tdTest.appendChild(bOff);
      } else {
        const bRead = document.createElement("button");
        bRead.className = "btn";
        bRead.textContent = "Read";
        bRead.addEventListener("click", (ev) => {
          ev.stopPropagation();
          readSensor({ type: p.type, mac: macFormatColon(p.mac || "") });
        });
        tdTest.appendChild(bRead);
      }

      const tdR = document.createElement("td");
      const bRm = document.createElement("button");
      bRm.className = "btn danger";
      bRm.textContent = "X";
      bRm.addEventListener("click", (ev) => {
        ev.stopPropagation();
        removePeer(macFormatColon(p.mac || ""));
      });
      tdR.appendChild(bRm);

      tr.addEventListener("click", (ev) => {
        ev.stopPropagation();
        const macDisp = macFormatColon(p.mac || "");
        const baseKind = BASE_KIND(KIND_UP(p.type)); // <—
        if (isChainPlaceable(p.type)) {
          const b = bricks.find((bb) => macEqual(bb.mac, macDisp));
          if (b) selectBrick(b.id);
          else {
            selectedId = null;
            selectedPeer = { type: p.type, mac: macDisp };
            updateSelectionPanel(baseKind, macDisp); // <—
          }
        } else {
          selectedId = null;
          selectedPeer = { type: p.type, mac: macDisp };
          updateSelectionPanel("POWER", macDisp);
        }
        selectPeerRowByMac(macDisp);
        selectPalettePillByMac(macDisp);
      });

      tr.append(tdT, tdM, tdO, tdTest, tdR);
      peersTblBody.appendChild(tr);
    });
    if (peersCount) peersCount.textContent = String(peers.length);
  }

  peerSearch?.addEventListener("input", () => renderPeers());

  // Refresh button: reload paired devices (list + palette)
  btnRefreshPeers?.addEventListener("click", (e) => {
    e.stopPropagation();
    fetchPeers().catch(() => showToast("Peers refresh failed"));
  });

  async function pairPeer() {
    const mac = macFormatColon(pMac.value);
    if (!macIsCompleteColon(mac))
      return showToast("Invalid MAC (use AA:BB:CC:DD:EE:FF)");
    const type = String(pType.value || "").toLowerCase();
    const r = await fetch("/api/peer/pair", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ mac, type }),
    });
    showToast(r.ok ? "Paired" : "Pair failed");
    if (r.ok) {
      pMac.value = "";
      fetchPeers().catch(() => {});
    }
  }
  btnPair?.addEventListener("click", (e) => {
    e.stopPropagation();
    pairPeer().catch(() => showToast("Pair failed"));
  });

  async function removePeer(mac) {
    if (!confirm("Remove peer " + mac + " ?")) return;
    const r = await fetch("/api/peer/remove", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ mac }),
    });
    showToast(r.ok ? "Removed" : "Remove failed");
    if (r.ok) fetchPeers().catch(() => {});
  }
  btnRemove?.addEventListener("click", (e) => {
    e.stopPropagation();
    const mac = macFormatColon(rMac.value);
    if (!macIsCompleteColon(mac)) return showToast("Invalid MAC");
    removePeer(mac);
  });

  // ------------------------ Topology build/save ------------------------
  function buildLinks() {
    // base nodes
    const arr = bricks
      .filter((b) => isChainPlaceable(b.kind))
      .map((b) => {
        const o = {
          type: KIND_UP(b.kind), // SENSOR / RELAY
          mac: macFormatColon(b.mac), // normalized
        };
        // Preserve emulator identity when present
        if (b.vType && Number.isInteger(b.vIndex)) {
          o.vType = String(b.vType).toLowerCase(); // 'semux' | 'remux'
          o.vIndex = b.vIndex; // channel
          o.addr = encodeAddr(o.mac, o.vType, o.vIndex); // AA:..:FF#S3 / #R12
        }
        return o;
      });

    // link list
    const byId = {};
    bricks.forEach((b, i) => {
      if (!isChainPlaceable(b.kind)) return;
      byId[i] = arr.shift();
    });

    const links = Object.values(byId).map((node, i, a) => ({
      ...node,
      prev: a[i - 1] ? a[i - 1].mac : null,
      next: a[i + 1] ? a[i + 1].mac : null,
    }));

    return links;
  }

  async function loadFromDevice() {
    const r = await fetch("/api/topology/get", { cache: "no-store" });
    if (!r.ok) {
      showToast("Load failed");
      return;
    }
    const j = await r.json();
    let arr = [];
    if (Array.isArray(j.links)) arr = j.links;
    else if (j && Array.isArray(j)) arr = j;
    else if (j && j.links && j.links.links && Array.isArray(j.links.links))
      arr = j.links.links;

    brickSeq = 1;
    bricks = (arr || [])
      .filter((l) => isChainPlaceable(l.type))
      .map((l) => ({
        id: brickSeq++,
        kind: KIND_UP(l.type || "RELAY"),
        mac: macFormatColon(l.mac || ""),
      }));
    unselectAll();
    redrawLane();
    showToast("Loaded");
    renderPalette();
  }
  btnLoad?.addEventListener("click", (e) => {
    e.stopPropagation();
    loadFromDevice().catch(() => showToast("Load failed"));
  });

  function hasAtLeastOnePower() {
    return peers.some((p) => String(p.type).toLowerCase() === "power");
  }

  async function saveToDevice(pushAlso = false) {
    if (!hasAtLeastOnePower()) {
      showToast("Pair at least one Power module first");
      return;
    }
    const payload = {
      links: buildLinks(),
      ...(pushAlso ? { push: true } : {}),
    };
    const r = await fetch("/api/topology/set", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    showToast(r.ok ? (pushAlso ? "Saved & pushed" : "Saved") : "Save failed");
  }
  btnSave?.addEventListener("click", (e) => {
    e.stopPropagation();
    saveToDevice(false).catch(() => showToast("Save failed"));
  });
  btnPush?.addEventListener("click", (e) => {
    e.stopPropagation();
    saveToDevice(true).catch(() => showToast("Push failed"));
  });

  // Export/import
  // --- Export / Import (v2 – supports SEMUX/REMUX) -----------------------------

  // helper: encode "<MAC>#S3" / "<MAC>#R12"
  if (typeof encodeAddr !== "function") {
    function encodeAddr(mac, vType, vIndex) {
      const letter = String(vType).toLowerCase() === "semux" ? "S" : "R";
      return `${mac}#${letter}${vIndex}`;
    }
  }

  // helper: pull vType/vIndex from link ({vType,vIndex} or addr "#S#R")
  if (typeof parseEmuFromLink !== "function") {
    function parseEmuFromLink(l) {
      let vType = null,
        vIndex = null;
      if (l && l.vType != null && Number.isInteger(l.vIndex)) {
        const t = String(l.vType).toLowerCase();
        if (t === "semux" || t === "remux") {
          vType = t;
          vIndex = Number(l.vIndex);
        }
      } else if (l && typeof l.addr === "string") {
        const m = l.addr.match(/#([SR])(\d+)$/i);
        if (m) {
          vType = m[1].toUpperCase() === "S" ? "semux" : "remux";
          vIndex = parseInt(m[2], 10);
        }
      }
      return { vType, vIndex };
    }
  }

  // Export current chain (includes emulator identity when present)
  btnExport?.addEventListener("click", (e) => {
    e.stopPropagation();
    const links = buildLinks(); // must include {addr,vType,vIndex} when present
    const payload = {
      schema: "icm.topology.v2",
      exportedAt: new Date().toISOString(),
      links,
    };
    const blob = new Blob([JSON.stringify(payload, null, 2)], {
      type: "application/json",
    });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "topology.json";
    a.click();
    URL.revokeObjectURL(a.href);
    showToast("Topology exported.");
  });

  // Import a file and rebuild bricks (preserves SEMUX/REMUX children)
  fileImport?.addEventListener("change", async (ev) => {
    const f = ev.target.files?.[0];
    if (!f) return;
    try {
      const txt = await f.text();
      const json = JSON.parse(txt);

      // tolerate plain array or {links:[...]} wrapper
      const links = Array.isArray(json)
        ? json
        : Array.isArray(json?.links)
        ? json.links
        : [];

      // wipe current chain
      bricks = [];
      brickSeq = 1;

      // rebuild from links
      links.forEach((l) => {
        if (!l || !l.mac || !l.type) return;
        const mac = macFormatColon(l.mac);
        const baseKind = KIND_UP(BASE_KIND(l.type)); // SENSOR/RELAY

        // pull emulator extension (from vType/vIndex or from addr)
        const emu = parseEmuFromLink({
          mac,
          vType: l.vType,
          vIndex: l.vIndex,
          addr: l.addr,
        });

        // Kind to render: SEMUX children are SENSOR bricks, REMUX children are RELAY bricks
        const kind =
          emu.vType === "semux"
            ? "SENSOR"
            : emu.vType === "remux"
            ? "RELAY"
            : baseKind;

        const b = { id: brickSeq++, kind, mac };
        if (emu.vType && Number.isInteger(emu.vIndex)) {
          b.vType = emu.vType.toUpperCase(); // "SEMUX"/"REMUX" for consistency
          b.vIndex = emu.vIndex;
          b.addr = encodeAddr(mac, emu.vType, emu.vIndex);
        }
        bricks.push(b);
      });

      // refresh UI
      unselectAll();
      redrawLane(); // <— was renderChain(), fixed
      renderPalette();
      updateSelectionPanel(null, null);
      if (typeof unselectAll === "function") unselectAll(); // clears bricks/pills & right panel
      // Also ensure Selected panel shows "None"
      if (typeof clearSelectionPanel === "function") clearSelectionPanel();
      showToast("Topology imported.");
    } catch (e) {
      console.error(e);
      showToast("Invalid topology file.");
    } finally {
      fileImport.value = "";
    }
  });

  // ------------------------ Actions (Relay & Power) ------------------------
  async function testRelay(peerLike) {
    const start = macFormatColon(peerLike.mac || "");
    if (!macIsCompleteColon(start)) return showToast("Missing MAC");
    await fetch("/api/sequence/start", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ start, direction: "UP" }),
    });
    showToast("Relay test ON");
    siRelState.textContent = "ON";
  }
  async function stopSeq() {
    await fetch("/api/sequence/stop", { method: "POST" });
    showToast("Relay OFF");
    siRelState.textContent = "OFF";
  }

  // Relay toolbar
  tOn?.addEventListener("click", (e) => {
    e.stopPropagation();
    handleRelayToggle(true);
    let target = null;
    if (selectedId != null) {
      const b = bricks.find((x) => x.id === selectedId);
      if (b) target = peers.find((pp) => macEqual(pp.mac || "", b.mac || ""));
    }
    if (
      !target &&
      selectedPeer &&
      String(selectedPeer.type).toLowerCase() === "relay"
    )
      target = selectedPeer;
    if (target) testRelay(target);
    else showToast("Select a relay");
  });
  tOff?.addEventListener("click", (e) => {
    e.stopPropagation();
    handleRelayToggle(false);
    stopSeq();
  });

  // ------------------------ Sensors (day/night + env + tfraw; mock fallbacks) ------------------------
  async function readDayNight(peerLike) {
    const mac = macFormatColon(peerLike?.mac || "");
    if (!macIsCompleteColon(mac)) {
      renderDayNight(undefined);
      return;
    }
    try {
      const r = await fetch("/api/sensor/daynight", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mac }),
      });
      if (!r.ok) throw new Error();
      const j = await r.json();
      const isDay =
        typeof j?.is_day === "number" ? (j.is_day ? 1 : 0) : undefined;
      renderDayNight(isDay);
    } catch {
      renderDayNight(undefined);
    }
  }

  function putText(idEl, text) {
    if (idEl) idEl.textContent = text;
  }

  async function readSensor(peerLike) {
    const mac = macFormatColon(peerLike?.mac || "");
    if (!macIsCompleteColon(mac)) {
      siSensStatus.textContent = "—";
      return;
    }

    siSensStatus.textContent = "…";

    // Preferred: /api/sensor/env
    let env = null,
      gotIsDay = undefined;
    try {
      const r = await fetch("/api/sensor/env", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mac }),
      });
      if (r.ok) {
        const j = await r.json();
        env = j || {};
        if (typeof j?.is_day === "number") gotIsDay = j.is_day ? 1 : 0;
      } else throw new Error();
    } catch {
      // Fallback: /api/sensor/read (mock)
      try {
        const rf = await fetch("/api/sensor/read", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ mac }),
        });
        if (rf.ok) {
          const jf = await rf.json();
          env = {
            tC: typeof jf?.tempC === "number" ? jf.tempC : undefined,
            rh: typeof jf?.humidity === "number" ? jf.humidity : undefined,
            p_Pa:
              typeof jf?.pressure_hPa === "number"
                ? Math.round(jf.pressure_hPa * 100)
                : undefined,
            lux: jf?.lux,
            tf_a: jf?.tf_a,
            tf_b: jf?.tf_b,
          };
        }
      } catch {}
    }

    // TF raw: try /api/sensor/tfraw; fallback to env.tf_a/tf_b or /api/sensor/read
    let tfA, tfB;
    try {
      const r = await fetch("/api/sensor/tfraw", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mac, which: 2 }),
      });
      if (r.ok) {
        const j = await r.json();
        tfA = typeof j?.distA_mm === "number" ? j.distA_mm : tfA;
        tfB = typeof j?.distB_mm === "number" ? j.distB_mm : tfB;
      } else throw new Error();
    } catch {
      if (typeof env?.tf_a === "number") tfA = env.tf_a;
      if (typeof env?.tf_b === "number") tfB = env.tf_b;
      if (tfA === undefined || tfB === undefined) {
        try {
          const rf = await fetch("/api/sensor/read", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ mac }),
          });
          if (rf.ok) {
            const jf = await rf.json();
            if (typeof jf?.tf_a === "number") tfA = jf.tf_a;
            if (typeof jf?.tf_b === "number") tfB = jf.tf_b;
          }
        } catch {}
      }
    }

    // Update UI
    try {
      const tC = Number(env?.tC);
      setGauge(gSensTemp, Number.isFinite(tC) ? tC : 0, -20, 80, "°C");
      setStateClass(
        gSensTemp,
        levelHi(tC, LIMITS.tempC.warn, LIMITS.tempC.danger)
      );

      if (typeof env?.rh === "number") {
        putText(siHum, env.rh.toFixed(1) + " %");
        setStateClass(
          siHum,
          levelHi(env.rh, LIMITS.humidity.warn, LIMITS.humidity.danger)
        );
      }

      if (typeof env?.p_Pa === "number") {
        const hPa = env.p_Pa / 100;
        putText(siPress, hPa.toFixed(1) + " hPa");
        setStateClass(siPress, levelBand(hPa, LIMITS.pressure_hPa));
      }

      if (typeof env?.lux === "number")
        putText(siLux, String(Math.round(env.lux)));
      if (typeof tfA === "number") putText(siTfA, Math.round(tfA) + " mm");
      if (typeof tfB === "number") putText(siTfB, Math.round(tfB) + " mm");

      if (gotIsDay === 0 || gotIsDay === 1) renderDayNight(gotIsDay);
      else await readDayNight({ mac });

      siSensStatus.textContent = "OK";
      setLoading(panelSensor, false);
    } catch {
      siSensStatus.textContent = "—";
    }
  }

  // ------------------------ POWER: info & control ------------------------
  function displayPower(j) {
    const v = Number(j?.vbus_mV ?? 0) / 1000;
    const i = Number(j?.ibus_mA ?? 0) / 1000;
    const t = Number.isFinite(Number(j?.tempC)) ? Number(j.tempC) : NaN;
    setGauge(gPwrVolt, v, 0, 60, "V");
    setGauge(gPwrCurr, i, 0, 10, "A");
    setGauge(gPwrTemp, Number.isFinite(t) ? t : 0, 0, 100, "°C");

    setStateClass(gPwrVolt, levelBand(v, LIMITS.vbus_V));
    setStateClass(
      gPwrCurr,
      levelHi(i, LIMITS.ibus_A.warn, LIMITS.ibus_A.danger)
    );
    setStateClass(
      gPwrTemp,
      levelHi(t, LIMITS.pwrTempC.warn, LIMITS.pwrTempC.danger)
    );

    if (j && typeof j.on === "boolean") {
      pwrOnBtn?.classList.toggle("active", j.on);
      pwrOffBtn?.classList.toggle("active", !j.on);
    }
  }

  async function readPower() {
    try {
      const r = await fetch("/api/power/info", { cache: "no-store" });
      if (!r.ok) throw new Error("pwr info");
      const j = await r.json();
      displayPower(j);
    } catch {
      setGauge(gPwrVolt, 0, 0, 60, "V");
      setGauge(gPwrCurr, 0, 0, 10, "A");
      setGauge(gPwrTemp, 0, 0, 100, "°C");
    }
  }

  async function powerCmd(act) {
    try {
      const r = await fetch("/api/power/cmd", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ pwr_action: String(act || "").toLowerCase() }),
      });
      if (!r.ok) {
        showToast("Power cmd failed");
        return;
      }
      await r.json().catch(() => null);
      showToast("Power " + String(act).toUpperCase());
      await readPower();
    } catch {
      showToast("Power cmd error");
    }
  }

  pwrOnBtn?.addEventListener("click", (e) => {
    e.stopPropagation();
    powerCmd("on");
  });
  pwrOffBtn?.addEventListener("click", (e) => {
    e.stopPropagation();
    powerCmd("off");
  });

  // ------------------------ Init ------------------------
  async function initTopologyPage() {
    // Load peers and configuration
    await fetchPeers();
    await loadFromDevice();

    // Auto-select power module if present
    const pwr = peers.find((p) => String(p.type).toLowerCase() === "power");
    if (pwr) {
      selectedId = null;
      selectedPeer = { type: pwr.type, mac: macFormatColon(pwr.mac || "") };
      updateSelectionPanel("POWER", selectedPeer.mac);
    }

    // ---------------- Sensor Settings ----------------
    const btnSensorSettings = document.getElementById("btnSensorSettings");
    const btnSensorSave = document.getElementById("sensorSettingsSave");
    const btnSensorClose = document.getElementById("sensorSettingsClose");
    const btnSensorCancel = document.getElementById("sensorSettingsCancel");
    const modalSensor = document.getElementById("sensorSettingsModal");

    btnSensorSettings?.addEventListener("click", (e) => {
      e.preventDefault();
      e.stopPropagation();
      openSensorSettings();
    });

    btnSensorClose?.addEventListener("click", (e) => {
      e.stopPropagation();
      closeSensorSettings();
    });

    btnSensorCancel?.addEventListener("click", (e) => {
      e.preventDefault();
      e.stopPropagation();
      snapshotSensorFormToDraft(); // preserve unsaved picks
      closeSensorSettings();
    });

    btnSensorSave?.addEventListener("click", async (e) => {
      e.preventDefault();
      e.stopPropagation();
      try {
        await saveSensorSettings();
      } catch (_) {
        // handle silently or show toast
      }
    });

    modalSensor?.addEventListener("click", (e) => {
      if (e.target?.dataset?.close) {
        e.stopPropagation();
        snapshotSensorFormToDraft(); // preserve on backdrop close
        closeSensorSettings();
      }
    });

    // Close on Esc
    document.addEventListener("keydown", (e) => {
      if (
        e.key === "Escape" &&
        modalSensor &&
        modalSensor.getAttribute("aria-hidden") === "false"
      ) {
        snapshotSensorFormToDraft();
        closeSensorSettings();
      }
    });

    // ---------------- Relay Settings ----------------
    const btnRelaySettings = document.getElementById("btnRelaySettings");
    const btnRelaySave = document.getElementById("relaySettingsSave");

    btnRelaySettings?.addEventListener("click", (e) => {
      e.preventDefault();
      e.stopPropagation();
      openRelaySettings();
    });

    btnRelaySave?.addEventListener("click", async (e) => {
      e.preventDefault();
      e.stopPropagation();
      try {
        await saveRelaySettings();
      } catch (_) {
        // handle silently or show toast
      }
    });
  }

  // Relay Settings button under Selected → Relay
  document
    .getElementById("btnRelaySettings")
    ?.addEventListener("click", (e) => {
      e.stopPropagation();
      openRelaySettings();
    });

  // Modal controls
  document
    .getElementById("relaySettingsClose")
    ?.addEventListener("click", (e) => {
      e.stopPropagation(); // <-- add
      closeRelaySettings();
    });
  document
    .getElementById("relaySettingsCancel")
    ?.addEventListener("click", (e) => {
      e.preventDefault();
      e.stopPropagation();
      snapshotRelayFormToDraft();
      closeRelaySettings();
    });
  document
    .getElementById("relaySettingsSave")
    ?.addEventListener("click", (e) => {
      e.preventDefault();
      saveRelaySettings().catch(() => {});
    });

  // Backdrop click to close
  document
    .getElementById("relaySettingsModal")
    ?.addEventListener("click", (e) => {
      e.stopPropagation();
      if (e.target?.dataset?.close) {
        snapshotRelayFormToDraft();
        closeRelaySettings();
      }
    });

  // Under Selected → Power
  document.getElementById("btnPmsSettings")?.addEventListener("click", (e) => {
    e.stopPropagation();
    openPmsSettings();
  });

  document
    .getElementById("pmsSettingsClose")
    ?.addEventListener("click", (e) => {
      e.stopPropagation(); // <-- add
      closePmsSettings();
    });
  document
    .getElementById("pmsSettingsCancel")
    ?.addEventListener("click", (e) => {
      e.preventDefault();
      e.stopPropagation();
      snapshotPmsFormToDraft();
      closePmsSettings();
    });
  document.getElementById("pmsSettingsSave")?.addEventListener("click", (e) => {
    e.preventDefault();
    savePmsSettings().catch(() => {});
  });
  document
    .getElementById("pmsSettingsReset")
    ?.addEventListener("click", (e) => {
      e.preventDefault();
      resetPmsNode().catch(() => {});
    });
  document
    .getElementById("pmsSettingsModal")
    ?.addEventListener("click", (e) => {
      e.stopPropagation();
      if (e.target?.dataset?.close) {
        snapshotPmsFormToDraft();
        closePmsSettings();
      }
    });

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", () => {
      initTopologyPage().catch(() => {});
    });
  } else {
    initTopologyPage().catch(() => {});
  }

  // Delete key removes selected brick
  document.addEventListener("keydown", (e) => {
    if (e.key === "Delete" && selectedId != null) {
      const i = bricks.findIndex((b) => b.id === selectedId);
      if (i >= 0) {
        bricks.splice(i, 1);
        unselectAll();
        redrawLane();
        renderPalette();
      }
    }
  });
})();

// ---- UX: lightweight table filter for peers ----
(function () {
  const filter = document.getElementById("peerFilter");
  const peerSearch = document.getElementById("peerSearch");
  if (!filter || !peerSearch) return;
  function mirror() {
    if (peerSearch.value !== filter.value) {
      peerSearch.value = filter.value;
      const ev = new Event("input", { bubbles: true });
      peerSearch.dispatchEvent(ev);
    }
  }
  filter.addEventListener("input", mirror);
  setTimeout(mirror, 0);
})();

// ---- UX: bind topbar refresh to existing reload logic ----
(function () {
  const btn = document.getElementById("refreshPairs");
  if (!btn) return;
  btn.addEventListener("click", () => {
    if (typeof reloadPairs === "function") reloadPairs();
    else if (typeof loadPairs === "function") loadPairs();
  });
})();
