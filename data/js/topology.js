/*! ICM Topology UI — unified 1s live polling on selection
 *  - Polls POWER, SENSOR/ENTRANCE/PARKING, or RELAY every 1s while selected
 *  - Stops when nothing is selected
 *  - Click anywhere outside selectable elements to unselect
 *  - Refresh button reloads paired devices
 */
(() => {
  "use strict";

  // ------------------------ DOM helpers ------------------------
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
    vbus_V: {
      warnLow: 44,
      warnHigh: 52, //  48V nominal
      dangerLow: 42,
      dangerHigh: 54,
    },
    ibus_A: { warn: 4.0, danger: 6.0 }, // current
    pwrTempC: { warn: 60, danger: 80 }, // PSU temp
  };

  // ---------- Class helpers ----------
  function setStateClass(el, level) {
    if (!el) return;
    el.classList.remove("state-ok", "state-warn", "state-danger");
    if (level) el.classList.add(`state-${level}`);
  }
  // One-sided thresholds (only high side matters)
  function levelHi(v, warn, danger) {
    if (!Number.isFinite(v)) return null;
    if (v >= danger) return "danger";
    if (v >= warn) return "warn";
    return "ok";
  }
  // Two-sided band thresholds (low/high)
  function levelBand(v, { warnLow, warnHigh, dangerLow, dangerHigh }) {
    if (!Number.isFinite(v)) return null;
    if (v <= dangerLow || v >= dangerHigh) return "danger";
    if (v <= warnLow || v >= warnHigh) return "warn";
    return "ok";
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
  let selectedId = null; // selected chain brick id
  let selectedPeer = null; // {type, mac} when selection comes from peers (incl. POWER)
  let peers = []; // /api/peers/list
  let pollTimer = null; // unified 1s polling timer
  let pollTarget = null; // {kindUp, mac}

  const KIND_UP = (t) => String(t || "").toUpperCase();
  const kindLabel = (k) =>
    ({
      POWER: "Power",
      ENTRANCE: "Entrance",
      RELAY: "Relay",
      PARKING: "Parking",
      SENSOR: "Sensor",
    }[k] || k);
  const kindTag = (k) =>
    ({
      POWER: "PWR",
      ENTRANCE: "ENT",
      RELAY: "REL",
      PARKING: "PRK",
      SENSOR: "SNS",
    }[k] || k);
  const isChainPlaceable = (kindUp) =>
    ["ENTRANCE", "RELAY", "PARKING", "SENSOR"].includes(KIND_UP(kindUp));

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
    clearPeerRowSelection();
    clearSelectionPanel();
    stopPolling();
  }

  // Click anywhere outside selectable things → unselect
  document.addEventListener("click", (e) => {
    const safe = e.target.closest(
      // things that count as “inside a selection context”
      ".brick, .pill, #peersTbl tbody tr, .card-selected, .card-palette, .card-chain, .card-peers, .card-pair"
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
        // If a relay info endpoint exists, call it here.
        // For mock/demo, lightly animate the relay temp gauge and keep last state text.
        const base = Number(siRelState.textContent === "ON" ? 36 : 26);
        setGauge(gRelTemp, base + Math.random() * 4, -20, 100, "°C");
      } else if (t === "SENSOR" || t === "ENTRANCE" || t === "PARKING") {
        await readSensor({ mac: pollTarget.mac, type: t.toLowerCase() }).catch(
          () => {}
        );
      }
    };

    // immediate tick + 1s interval
    tick();
    pollTimer = setInterval(tick, 1000);
  }

  // ------------------------ Selection panel ------------------------
  function updateSelectionPanel(kindUp, mac) {
    siKind.textContent = kindLabel(kindUp);
    siMac.textContent = macFormatColon(mac) || "—";

    panelSensor.hidden = panelRelay.hidden = panelPower.hidden = true;

    if (kindUp === "SENSOR" || kindUp === "ENTRANCE" || kindUp === "PARKING") {
      panelSensor.hidden = false;
      setGauge(gSensTemp, 0, -20, 80, "°C");
      renderDayNight(undefined);
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
    showTypeMac(true); // <— make Type/MAC visible when selected
    // Start 1s polling for the selected thing
    startPolling(kindUp, mac);
  }
  // Hide/show the whole Type & MAC blocks (their .kv wrappers)
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
    showTypeMac(false); // <— hide the Type/MAC row entirely
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
      const macDisp = macFormatColon(b.mac);
      title.textContent = `${kindLabel(b.kind)} · ${macDisp}`;

      el.appendChild(tag);
      el.appendChild(title);
      el.title = macDisp || "";

      el.addEventListener("click", (ev) => {
        ev.stopPropagation(); // prevent global unselect
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
        const isSensorish = (k) =>
          k === "SENSOR" || k === "ENTRANCE" || k === "PARKING";
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
    const b = bricks.find((x) => x.id === id);
    document
      .querySelectorAll(".brick")
      .forEach((e) => e.classList.remove("selected"));
    const el = [...document.querySelectorAll(".brick")].find(
      (e) => e.dataset.id == id
    );
    if (el) el.classList.add("selected");

    if (!b) {
      unselectAll();
      return;
    }
    updateSelectionPanel(KIND_UP(b.kind), b.mac);
    selectPeerRowByMac(b.mac);
    selectPalettePillByMac(b.mac);
    scrollBrickIntoView(id);
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
    const brickId = ev.dataTransfer.getData("text/icm-brick");
    if (brickId) {
      const afterId = findAfterId(ev.clientY);
      reorderBrick(parseInt(brickId, 10), afterId);
      return;
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
        if (
          typeUp === "ENTRANCE" &&
          bricks.some((b) => b.kind === "ENTRANCE")
        ) {
          showToast("Only one Entrance allowed");
          return;
        }
        if (typeUp === "PARKING" && bricks.some((b) => b.kind === "PARKING")) {
          showToast("Only one Parking allowed");
          return;
        }
        bricks.push({
          id: brickSeq++,
          kind: typeUp,
          mac: macFormatColon(p.mac),
        });
        redrawLane();
        renderPalette(); // update used/unused
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

  // ------------------------ Palette (grouped) ------------------------
  const isUsed = (mac) => bricks.some((b) => macEqual(b.mac, mac));

  function makePill(p, titleText, cls) {
    const pill = document.createElement("button");
    pill.className = "pill " + cls;
    const fullMac = macFormatColon(p.mac || "");
    pill.innerHTML = `<span class="pill-title">${titleText}</span><span class="pill-mac mono">${fullMac}</span>`;
    pill.draggable = isChainPlaceable(p.type);
    pill.addEventListener("dragstart", (ev) => {
      ev.dataTransfer.setData(
        "text/icm-peer",
        JSON.stringify({ type: p.type, mac: fullMac })
      );
      ev.dataTransfer.effectAllowed = "copy";
    });
    pill.addEventListener("click", (ev) => {
      ev.stopPropagation();
      selectedId = null;
      selectedPeer = { type: p.type, mac: fullMac };
      updateSelectionPanel(KIND_UP(p.type), fullMac);
      selectPeerRowByMac(fullMac);
      selectPalettePillByMac(fullMac);
    });
    return pill;
  }

  function appendGroup(root, title, items) {
    const group = document.createElement("div");
    group.className = "palette-group";
    const t = document.createElement("div");
    t.className = "palette-title";
    t.textContent = title;
    group.appendChild(t);
    items.forEach((el) => group.appendChild(el));
    root.appendChild(group);
  }

  function renderPalette() {
    palette.innerHTML = "";

    // Power (reference only)
    const powerPeers = peers.filter(
      (p) => String(p.type).toLowerCase() === "power"
    );
    appendGroup(
      palette,
      "Power (not placeable)",
      powerPeers.map((p) => makePill(p, "Power", "t-power"))
    );

    // Grouped placeables, excluding ones already in chain
    const sensors = peers
      .filter(
        (p) => String(p.type).toLowerCase() === "sensor" && !isUsed(p.mac)
      )
      .map((p) => makePill(p, "Regular sensor", "t-sens"));
    const relays = peers
      .filter((p) => String(p.type).toLowerCase() === "relay" && !isUsed(p.mac))
      .map((p) => makePill(p, "Relay", "t-relay"));
    const entr = peers
      .filter(
        (p) => String(p.type).toLowerCase() === "entrance" && !isUsed(p.mac)
      )
      .map((p) => makePill(p, "Entrance", "t-entr"));
    const park = peers
      .filter(
        (p) => String(p.type).toLowerCase() === "parking" && !isUsed(p.mac)
      )
      .map((p) => makePill(p, "Parking", "t-park"));

    appendGroup(palette, "Regular sensors", sensors);
    appendGroup(palette, "Relays", relays);
    appendGroup(palette, "Entrance", entr);
    appendGroup(palette, "Parking", park);
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
      if ((p.type || "").toLowerCase() === "relay") {
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
        if (isChainPlaceable(p.type)) {
          const b = bricks.find((bb) => macEqual(bb.mac, macDisp));
          if (b) selectBrick(b.id);
          else {
            selectedId = null;
            selectedPeer = { type: p.type, mac: macDisp };
            updateSelectionPanel(KIND_UP(p.type), macDisp);
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
    const chain = bricks.filter((b) => isChainPlaceable(b.kind));
    return chain.map((b, i) => {
      const prev = chain[i - 1],
        next = chain[i + 1];
      const o = { type: b.kind, mac: macFormatColon(b.mac) };
      if (prev) o.prev = { type: prev.kind, mac: macFormatColon(prev.mac) };
      if (next) o.next = { type: next.kind, mac: macFormatColon(next.mac) };
      return o;
    });
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
      arr = j.links.links; // tolerant
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
  btnExport?.addEventListener("click", (e) => {
    e.stopPropagation();
    const obj = { links: buildLinks() };
    const blob = new Blob([JSON.stringify(obj, null, 2)], {
      type: "application/json",
    });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "icm-topology.json";
    a.click();
    URL.revokeObjectURL(a.href);
  });

  fileImport?.addEventListener("change", () => {
    const f = fileImport.files[0];
    if (!f) return;
    const rd = new FileReader();
    rd.onload = () => {
      try {
        const j = JSON.parse(rd.result);
        let arr = null;
        if (Array.isArray(j)) arr = j;
        else if (Array.isArray(j?.links)) arr = j.links;
        else if (j?.links?.links && Array.isArray(j.links.links))
          arr = j.links.links;
        if (!arr) {
          showToast("Invalid file (no links)");
          return;
        }
        brickSeq = 1;
        bricks = arr
          .filter((l) => isChainPlaceable(l.type))
          .map((l) => ({
            id: brickSeq++,
            kind: KIND_UP(l.type || "RELAY"),
            mac: macFormatColon(l.mac || ""),
          }));
        unselectAll();
        redrawLane();
        renderPalette();
        showToast("Imported (not saved)");
      } catch {
        showToast("Parse error");
      }
    };
    rd.readAsText(f);
    fileImport.value = "";
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
      // Temperature
      const tC = Number(env?.tC);
      setGauge(gSensTemp, Number.isFinite(tC) ? tC : 0, -20, 80, "°C");
      setStateClass(
        gSensTemp,
        levelHi(tC, LIMITS.tempC.warn, LIMITS.tempC.danger)
      );

      // Humidity
      if (typeof env?.rh === "number") {
        putText(siHum, env.rh.toFixed(1) + " %");
        setStateClass(
          siHum,
          levelHi(env.rh, LIMITS.humidity.warn, LIMITS.humidity.danger)
        );
      }

      // Pressure (Pa → hPa)
      if (typeof env?.p_Pa === "number") {
        const hPa = env.p_Pa / 100;
        putText(siPress, hPa.toFixed(1) + " hPa");
        setStateClass(siPress, levelBand(hPa, LIMITS.pressure_hPa));
      }

      // Lux / TF raw
      if (typeof env?.lux === "number")
        putText(siLux, String(Math.round(env.lux)));
      if (typeof tfA === "number") putText(siTfA, Math.round(tfA) + " mm");
      if (typeof tfB === "number") putText(siTfB, Math.round(tfB) + " mm");

      // Day/Night
      if (gotIsDay === 0 || gotIsDay === 1) renderDayNight(gotIsDay);
      else await readDayNight({ mac });

      siSensStatus.textContent = "OK";
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

    // Apply state colors
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

  async function powerCmd(act /* 'on'|'off'|'status'|'refresh' */) {
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
    await fetchPeers();
    await loadFromDevice();
    // Optionally preselect a power peer (starts polling automatically)
    const pwr = peers.find((p) => String(p.type).toLowerCase() === "power");
    if (pwr) {
      selectedId = null;
      selectedPeer = { type: pwr.type, mac: macFormatColon(pwr.mac || "") };
      updateSelectionPanel("POWER", selectedPeer.mac);
    }
  }

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
(function() {
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
  // initialize mirror on load
  setTimeout(mirror, 0);
})();

// ---- UX: bind topbar refresh to existing reload logic ----
(function() {
  const btn = document.getElementById("refreshPairs");
  if (!btn) return;
  btn.addEventListener("click", () => {
    if (typeof reloadPairs === "function") reloadPairs();
    else if (typeof loadPairs === "function") loadPairs();
  });
})();
