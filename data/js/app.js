
/* ICM Topology — Fresh UI & Logic
 * - Different internal logic, same endpoints & assets.
 * - 5 containers: paired device, peers, selected, pair/remove, messages.
 * - Works with topology.mock.js (intercepts fetch).
 */

const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

// Basic logger
const log = (msg, cls = "muted") => {
  const line = document.createElement("div");
  line.className = `log-line ${cls}`;
  line.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
  $("#log").prepend(line);
};

// Store
const Store = (() => {
  const state = {
    peers: [],         // full list from server
    selected: new Set(), // macs chosen for actions
    paired: [],        // currently paired (from /api/topology/get)
    filter: { text: "", type: "" },
  };
  const listeners = new Set();
  const emit = () => listeners.forEach(fn => fn(state));
  return {
    get: () => state,
    setPeers(list) { state.peers = list; emit(); },
    setPaired(list) { state.paired = list; emit(); },
    toggleSelect(mac) {
      state.selected.has(mac) ? state.selected.delete(mac) : state.selected.add(mac);
      emit();
    },
    clearSelection() { state.selected.clear(); emit(); },
    setFilterText(t) { state.filter.text = t; emit(); },
    setFilterType(t) { state.filter.type = t; emit(); },
    subscribe(fn) { listeners.add(fn); return () => listeners.delete(fn); }
  };
})();

// API client (same endpoints as mock)
const api = {
  async peersList() {
    const r = await fetch("/api/peers/list");
    if (!r.ok) throw new Error("peers/list failed");
    return r.json();
  },
  async pair(mac, type) {
    const r = await fetch("/api/peer/pair", { method:"POST", headers:{'Content-Type':'application/json'}, body: JSON.stringify({mac, type}) });
    if (!r.ok) throw new Error("pair failed");
    return r.json();
  },
  async remove(mac) {
    const r = await fetch("/api/peer/remove", { method:"POST", headers:{'Content-Type':'application/json'}, body: JSON.stringify({mac}) });
    if (!r.ok) throw new Error("remove failed");
    return r.json();
  },
  async topoGet() {
    const r = await fetch("/api/topology/get");
    if (!r.ok) throw new Error("topology/get failed");
    return r.json();
  },
  async topoSet(links) {
    const r = await fetch("/api/topology/set", { method:"POST", headers:{'Content-Type':'application/json'}, body: JSON.stringify({links}) });
    if (!r.ok) throw new Error("topology/set failed");
    return r.json();
  },
  async pwrInfo() {
    const r = await fetch("/api/power/info");
    if (!r.ok) throw new Error("power/info failed");
    return r.json();
  }
};

// Render helpers
const iconForType = (type) => {
  const map = {
    power: "ac-power.png",
    sensor: "temp.png",
    relay: "relay.png",
    entrance: "road.png",
    parking: "parking.png",
  };
  return `./icons/${map[type] || "chip.png"}`;
};

const itemNode = (peer, selectedSet) => {
  const el = document.createElement("div");
  el.className = "item";
  el.dataset.mac = peer.mac;
  el.innerHTML = `
    <img class="icon" src="${iconForType(peer.type)}" alt="${peer.type}" />
    <div class="grow">
      <div class="row" style="justify-content:space-between">
        <strong>${peer.mac}</strong>
        <span class="pill">${peer.type}</span>
      </div>
      <div class="muted">${peer.online ? "online" : "offline"}</div>
    </div>
    <input type="checkbox" ${selectedSet.has(peer.mac) ? "checked" : ""} />
  `;
  el.addEventListener("click", (e) => {
    if (e.target.tagName !== "INPUT") {
      el.querySelector("input").checked = !el.querySelector("input").checked;
    }
    Store.toggleSelect(peer.mac);
  });
  return el;
};

const renderPeers = () => {
  const { peers, selected, filter } = Store.get();
  const list = $("#peer-list");
  list.innerHTML = "";
  const q = filter.text.trim().toLowerCase();
  const type = filter.type.trim();
  peers
    .filter(p => (!q || p.mac.toLowerCase().includes(q) || p.type.toLowerCase().includes(q)))
    .filter(p => (!type || p.type === type))
    .forEach(p => list.appendChild(itemNode(p, selected)));
};

const renderSelected = () => {
  const { peers, selected } = Store.get();
  const list = $("#selected-list");
  list.innerHTML = "";
  const chosen = peers.filter(p => selected.has(p.mac));
  chosen.forEach(p => list.appendChild(itemNode(p, selected)));
};

const renderPaired = () => {
  const { paired } = Store.get();
  const list = $("#paired-list");
  list.innerHTML = "";
  if (!paired || !paired.length) {
    const empty = document.createElement("div");
    empty.className = "muted";
    empty.textContent = "No topology set yet.";
    list.appendChild(empty);
    return;
  }
  paired.forEach((lk, i) => {
    const row = document.createElement("div");
    row.className = "item";
    row.innerHTML = `
      <img class="icon" src="./icons/chain.png" alt="link" />
      <div class="grow">
        <div><strong>${lk.from}</strong></div>
        <div class="muted">→ ${lk.to} (${lk.type})</div>
      </div>
      <span class="badge">#${i+1}</span>
    `;
    list.appendChild(row);
  });
};

// Sync online indicator
async function heartbeat() {
  try {
    const info = await api.pwrInfo();
    $("#net-dot").classList.add("online");
    $("#net-label").textContent = `48V BUS ${info.vbus_mV/1000}V • ${info.ibus_mA}mA • ${info.on ? "ON" : "OFF"}`;
  } catch (e) {
    $("#net-dot").classList.remove("online");
    $("#net-label").textContent = "offline";
  } finally {
    setTimeout(heartbeat, 3000);
  }
}

// Initial load
async function loadPeers() {
  const res = await api.peersList();
  Store.setPeers(res.peers || []);
  log(`Loaded peers: ${res.peers?.length ?? 0}`, "ok");
}
async function loadTopology() {
  const res = await api.topoGet();
  Store.setPaired(res.links || []);
  log("Topology loaded", "ok");
}

// Actions
async function doPairSelected() {
  const { peers, selected } = Store.get();
  const chosen = peers.filter(p => selected.has(p.mac));
  if (!chosen.length) { log("Nothing selected", "err"); return; }
  for (const p of chosen) {
    try {
      await api.pair(p.mac, p.type);
      log(`Paired ${p.mac}`, "ok");
    } catch (e) {
      log(`Pair failed for ${p.mac}: ${e.message}`, "err");
    }
  }
  Store.clearSelection();
  loadPeers();
}

async function doRemoveSelected() {
  const { peers, selected } = Store.get();
  const chosen = peers.filter(p => selected.has(p.mac));
  if (!chosen.length) { log("Nothing selected", "err"); return; }
  for (const p of chosen) {
    try {
      await api.remove(p.mac);
      log(`Removed ${p.mac}`, "ok");
    } catch (e) {
      log(`Remove failed for ${p.mac}: ${e.message}`, "err");
    }
  }
  Store.clearSelection();
  loadPeers();
}

async function doSaveTopology() {
  // Simple chain: connect selected peers in their MAC order
  const { peers, selected } = Store.get();
  const chosen = peers.filter(p => selected.has(p.mac)).sort((a,b)=>a.mac.localeCompare(b.mac));
  if (chosen.length < 2) { log("Select at least 2 peers to form links", "err"); return; }
  const links = [];
  for (let i=0; i<chosen.length-1; i++) {
    links.push({ from: chosen[i].mac, to: chosen[i+1].mac, type: "chain" });
  }
  try {
    await api.topoSet(links);
    log(`Saved ${links.length} link(s)`, "ok");
    Store.clearSelection();
    loadTopology();
  } catch (e) {
    log(`Save topology failed: ${e.message}`, "err");
  }
}

// Wiring
function wireUI() {
  $("#btnPair").addEventListener("click", doPairSelected);
  $("#btnRemove").addEventListener("click", doRemoveSelected);
  $("#btnReset").addEventListener("click", () => Store.clearSelection());
  $("#btnLoad").addEventListener("click", loadPeers);
  $("#btnGetTopo").addEventListener("click", loadTopology);
  $("#btnSaveTopo").addEventListener("click", doSaveTopology);
  $("#search").addEventListener("input", (e)=>Store.setFilterText(e.target.value));
  $("#typeFilter").addEventListener("change", (e)=>Store.setFilterType(e.target.value));

  Store.subscribe(renderPeers);
  Store.subscribe(renderSelected);
  Store.subscribe(renderPaired);
}

// Boot
window.addEventListener("DOMContentLoaded", async () => {
  wireUI();
  heartbeat();
  await loadPeers();
  await loadTopology();
});
