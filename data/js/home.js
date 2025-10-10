// Home page logic (split from HTML). Uses real APIs with graceful fallback to HOME_MOCK.
const fmt = {
  v: (mV) => (mV == null ? "—" : (mV / 1000).toFixed(2)),
  a: (mA) => (mA == null ? "—" : (mA / 1000).toFixed(2)),
  t: (c) => (c == null ? "—" : c.toFixed(1)),
};
function postLogout() {
  const f = document.createElement("form");
  f.method = "POST";
  f.action = "/logout";
  document.body.appendChild(f);
  f.submit();
}

for (const id of ["logoutBtn", "logoutBtnLegacy"]) {
  const btn = document.getElementById(id);
  if (btn) btn.addEventListener("click", postLogout);
}

// Static badge for read-only page
const icmModeEl = document.getElementById("icmMode");
if (icmModeEl) icmModeEl.textContent = "Read‑only";

async function fetchJSON(url) {
  try {
    const r = await fetch(url);
    if (!r.ok) throw new Error("HTTP " + r.status);
    return await r.json();
  } catch (e) {
    return null;
  }
}

async function loadPower() {
  let d = await fetchJSON("/api/power/info");
  if (!d && window.HOME_MOCK && window.HOME_MOCK.powerInfo)
    d = await window.HOME_MOCK.powerInfo();
  if (!d) return;
  document.getElementById("mV").textContent = fmt.v(d.vbus_mV);
  document.getElementById("mA").textContent = fmt.a(d.ibus_mA);
  document.getElementById("mTemp").textContent = fmt.t(d.tempC);
  document.getElementById("mPowerState").textContent = d.on ? "ON" : "OFF";
}

async function loadPeers() {
  let data = await fetchJSON("/api/peers/list");
  if (!data && window.HOME_MOCK && window.HOME_MOCK.peersList)
    data = await window.HOME_MOCK.peersList();
  const peers = data.peers ?? [];
  const c = (type) =>
    peers.filter((p) => String(p.type).toLowerCase() === type).length;
  document.getElementById("mPeers").textContent = peers.length;
  document.getElementById("mCountPower").textContent = c("power");
  document.getElementById("mCountSens").textContent = c("sensor");
  document.getElementById("mCountRelay").textContent = c("relay");
  document.getElementById("mCountSemux").textContent = c("semux");
  document.getElementById("mCountRemux").textContent = c("remux");
}

async function loadTopology() {
  let data = await fetchJSON("/api/topology/get");
  if (!data && window.HOME_MOCK && window.HOME_MOCK.topologyGet)
    data = await window.HOME_MOCK.topologyGet();
  const len = Array.isArray(data.links) ? data.links.length : 0;
  document.getElementById("mChain").textContent =
    len + " link" + (len === 1 ? "" : "s");
}

async function loadIdentity() {
  let d = await fetchJSON("/api/sys/identity");
  if (!d && window.HOME_MOCK && window.HOME_MOCK.identity)
    d = await window.HOME_MOCK.identity();
  if (!d) return;
  document.getElementById("idDevice").textContent = d.device_id ?? "—";
  document.getElementById("idKind").textContent = d.kind ?? "—";
  document.getElementById("idDefName").textContent = d.def_name ?? "—";
  document.getElementById("idHwRev").textContent = d.hwrev ?? "—";
  document.getElementById("idSwVer").textContent = d.swver ?? "—";
  document.getElementById("idBuild").textContent = d.build ?? "—";
}

async function loadCommonConfig() {
  let d = await fetchJSON("/api/config/common");
  if (!d && window.HOME_MOCK && window.HOME_MOCK.common)
    d = await window.HOME_MOCK.common();
  if (!d) return;
  document.getElementById("idCfgPart").textContent = d.config_partition ?? "—";
  document.getElementById("idLogDir").textContent = d.log_dir ?? "—";
  document.getElementById("idLogFile").textContent =
    d.log_file_path_icm ?? d.log_file_path ?? "—";
  document.getElementById("idLogPref").textContent =
    d.log_file_prefix_icm ?? d.log_file_prefix ?? "—";
}

async function loadNetInfo() {
  let d = await fetchJSON("/api/net/info");
  if (!d && window.HOME_MOCK && window.HOME_MOCK.net)
    d = await window.HOME_MOCK.net();
  if (!d) return;
  document.getElementById("idChan").textContent = d.channel ?? "—";
}

async function loadPolicy() {
  let d = await fetchJSON("/api/policy/indicator");
  if (!d && window.HOME_MOCK && window.HOME_MOCK.policy)
    d = await window.HOME_MOCK.policy();
  if (!d) return;
  const b = (x) => (x ? "Yes" : "No");
  document.getElementById("polLed").textContent = b(!!d.led_disabled);
  document.getElementById("polBuzz").textContent = b(!!d.buzzer_disabled);
  document.getElementById("polRgbAL").textContent = b(!!d.rgb_active_low);
  document.getElementById("polRgbFb").textContent = b(!!d.rgb_feedback);
  document.getElementById("polBzAH").textContent = b(!!d.buzzer_active_high);
  document.getElementById("polBzFb").textContent = b(!!d.buzzer_feedback);
}

// DROP-IN FIX: replace your adjustCatalogMaxHeight() with this
function adjustCatalogMaxHeight() {
  const card = document.querySelector(".card.card-catalog.grid-full");
  const el = document.getElementById("catalogScroll");
  if (!el) return;

  // If a fixed card height is defined in CSS, let CSS handle it
  if (card) {
    const csCard = getComputedStyle(card);
    const fixedCard = csCard.getPropertyValue("--catalog-card-height").trim();
    if (fixedCard) {
      el.style.maxHeight = "";
      return;
    }
  }

  // Compute available viewport space and clamp inner scroller
  const rect = el.getBoundingClientRect();
  const vh =
    (window.visualViewport && window.visualViewport.height) ||
    window.innerHeight ||
    document.documentElement.clientHeight ||
    600;
  const pad = 16; // breathing room
  const h = Math.max(200, Math.floor(vh - rect.top - pad));
  el.style.maxHeight = h + "px";
}

window.addEventListener("resize", adjustCatalogMaxHeight);
window.addEventListener("orientationchange", adjustCatalogMaxHeight);

if (typeof ResizeObserver !== "undefined") {
  const ro = new ResizeObserver(() => adjustCatalogMaxHeight());
  const topbar = document.querySelector(".topbar");
  const hero = document.querySelector(".hero-card");
  if (topbar) ro.observe(topbar);
  if (hero) ro.observe(hero);
}

// run once after DOM + after first data refresh
document.addEventListener("DOMContentLoaded", () => {
  setTimeout(adjustCatalogMaxHeight, 0);
});

async function refreshAll() {
  await Promise.all([
    loadPower(),
    loadPeers(),
    loadTopology(),
    loadIdentity(),
    loadCommonConfig(),
    loadNetInfo(),
    loadPolicy(),
  ]);

  try {
    adjustCatalogMaxHeight();
  } catch (e) {}
}

var _btnReload = document.getElementById("btnReload");
if (_btnReload) _btnReload.addEventListener("click", refreshAll);
refreshAll();
const timer = setInterval(refreshAll, 1000);
window.addEventListener("beforeunload", () => clearInterval(timer));
