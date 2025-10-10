/* ICM Settings Page Logic — keeps original IDs and API calls.
   Enhanced for:
   - Collapsible sections
   - Settings filter (search)
   - Right-rail scroll spy
*/
(function () {
  const $ = (sel) => document.querySelector(sel);
  const $$ = (sel) => Array.from(document.querySelectorAll(sel));

  const toast = $("#toast");
  const dirtyFlag = $("#dirtyFlag");
  const form = $("#icmForm");

  const infoFields = {
    sys_devid: "device_id",
    sys_fw: "firmware",
    sys_build: "build",
    sys_channel: "espnow_channel",
    sys_uptime: "uptime",
  };

  const configKeys = [
    "pair_mode",
    "refresh_topology_interval",
    "broadcast_retry_count",
    "allowed_roles",
    "time_sync_interval",
    "rtc_sync_source",
    "auto_time_sync",
    "timezone_offset",
    "sync_on_boot",
    "relay_command_mode",
    "manual_override",
    "pms_auto_switch",
    "relay_default_state",
    "icm_temp_threshold",
    "icm_temp_warning",
    "fan_mode",
    "buzzer_mode",
    "safety_lockout",
    "fault_alert_enable",
    "rgb_mode",
    "rgb_color_idle",
    "rgb_color_alert",
    "led_brightness",
    "blink_on_activity",
    "logging_enabled",
    "log_detail_level",
    "log_rotation_size",
    "log_auto_cleanup_days",
    "auto_reconnect_nodes",
    "fault_retry_limit",
    "priority_override",
    "alert_on_new_device",
    "web_update_enable",
    "web_theme",
    "refresh_interval_ui",
    "admin_password",
    "auto_save_config",
    "espnow_channel",
    "broadcast_mac",
    "ping_interval",
    "ping_timeout",
    "restart_delay",
  ];

  const endpoints = {
    info: "/api/icm/info",
    get: "/api/icm/config",
    set: "/api/icm/config",
    remove: "/api/icm/remove",
    resetAll: "/api/icm/reset_all",
    reboot: "/api/icm/restart",
    exportTopo: "/api/icm/export_topology",
  };

  function showToast(msg, ms = 1600) {
    if (!toast) return;
    toast.textContent = msg;
    toast.hidden = false;
    window.clearTimeout(showToast._t);
    showToast._t = window.setTimeout(() => (toast.hidden = true), ms);
  }
  function markDirty() {
    if (dirtyFlag) dirtyFlag.hidden = false;
  }

  function readForm() {
    const data = {};
    configKeys.forEach((k) => {
      const el = document.getElementById(k);
      if (!el) return;
      data[k] = el.value;
    });
    [
      "pair_mode",
      "auto_time_sync",
      "sync_on_boot",
      "manual_override",
      "pms_auto_switch",
      "safety_lockout",
      "fault_alert_enable",
      "blink_on_activity",
      "logging_enabled",
      "auto_reconnect_nodes",
      "alert_on_new_device",
      "web_update_enable",
      "auto_save_config",
    ].forEach((k) => {
      if (data[k] === "true") data[k] = true;
      if (data[k] === "false") data[k] = false;
    });
    [
      "refresh_topology_interval",
      "broadcast_retry_count",
      "time_sync_interval",
      "timezone_offset",
      "icm_temp_threshold",
      "icm_temp_warning",
      "led_brightness",
      "log_rotation_size",
      "log_auto_cleanup_days",
      "fault_retry_limit",
      "refresh_interval_ui",
      "espnow_channel",
      "ping_interval",
      "ping_timeout",
      "restart_delay",
    ].forEach((k) => {
      if (data[k] !== "" && data[k] != null) data[k] = Number(data[k]);
    });
    return data;
  }

  function writeForm(cfg) {
    configKeys.forEach((k) => {
      const el = document.getElementById(k);
      if (!el) return;
      const v = cfg[k];
      if (v == null) return;
      el.value = String(v);
    });
    if (dirtyFlag) dirtyFlag.hidden = true;
  }

  function bindDirty() {
    $$("#icmForm input, #icmForm select").forEach((el) => {
      el.addEventListener("change", markDirty);
      el.addEventListener("input", markDirty);
    });
  }

  async function fetchJSON(url, init) {
    const res = await fetch(url, init);
    if (!res.ok) throw new Error(await res.text());
    const ct = res.headers.get("content-type") || "";
    if (ct.includes("application/json")) return res.json();
    return res.text();
  }

  async function loadAll() {
    try {
      const [info, cfg] = await Promise.all([
        fetchJSON(endpoints.info).catch(() => ({})),
        fetchJSON(endpoints.get).catch(() => ({})),
      ]);
      Object.entries(infoFields).forEach(([id, key]) => {
        const el = document.getElementById(id);
        if (el) el.textContent = info[key] ?? "—";
      });
      writeForm(cfg);
    } catch (e) {
      console.error(e);
      showToast("Failed to load settings");
    }
  }

  async function save() {
    try {
      await fetchJSON(endpoints.set, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(readForm()),
      });
      if (dirtyFlag) dirtyFlag.hidden = true;
      showToast("Settings saved");
    } catch (e) {
      console.error(e);
      showToast("Save failed");
    }
  }

  function exportConfig() {
    const blob = new Blob([JSON.stringify(readForm(), null, 2)], {
      type: "application/json",
    });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "icm-config.json";
    a.click();
    URL.revokeObjectURL(a.href);
  }

  function importConfigFile(file) {
    const fr = new FileReader();
    fr.onload = () => {
      try {
        writeForm(JSON.parse(fr.result));
        showToast("Config imported (not saved)");
      } catch {
        showToast("Invalid JSON");
      }
    };
    fr.readAsText(file);
  }

  async function removeByMac() {
    const mac = (document.getElementById("remove_mac")?.value || "").trim();
    if (!mac) return showToast("Enter MAC");
    try {
      await fetchJSON(endpoints.remove, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ mac }),
      });
      showToast("Device removal requested");
    } catch {
      showToast("Remove failed");
    }
  }

  async function resetAll() {
    try {
      await fetchJSON(endpoints.resetAll, { method: "POST" });
      showToast("Reset command sent");
    } catch {
      showToast("Reset failed");
    }
  }
  async function reboot() {
    try {
      await fetchJSON(endpoints.reboot, { method: "POST" });
      showToast("Rebooting…");
    } catch {
      showToast("Reboot failed");
    }
  }
  async function exportTopo() {
    try {
      const data = await fetchJSON(endpoints.exportTopo);
      const a = document.createElement("a");
      a.href = URL.createObjectURL(
        new Blob([JSON.stringify(data, null, 2)], { type: "application/json" })
      );
      a.download = "topology-snapshot.json";
      a.click();
      URL.revokeObjectURL(a.href);
    } catch {
      showToast("Export failed");
    }
  }

  // Wire up buttons
  form.addEventListener("submit", (e) => {
    e.preventDefault();
    save();
  });
  document.getElementById("btnReset")?.addEventListener("click", loadAll);
  document.getElementById("btnExport")?.addEventListener("click", exportConfig);
  document
    .getElementById("btnImport")
    ?.addEventListener("click", () =>
      document.getElementById("fileImport")?.click()
    );
  document
    .getElementById("fileImport")
    ?.addEventListener(
      "change",
      (e) => e.target.files?.[0] && importConfigFile(e.target.files[0])
    );
  document
    .getElementById("btnRemoveMac")
    ?.addEventListener("click", removeByMac);
  document.getElementById("btnResetAll")?.addEventListener("click", resetAll);
  document.getElementById("btnReboot")?.addEventListener("click", reboot);
  document
    .getElementById("btnExportTopo")
    ?.addEventListener("click", exportTopo);

  bindDirty();
  loadAll();

  /* === Time & Clock Enhancements (unchanged; adapts to new layout) === */
  (function clockEnhancements() {
    const nowEl = document.getElementById("now_time");
    const dateEl = document.getElementById("now_date");
    const uptimeBig = document.getElementById("uptime_big");
    const analog = document.getElementById("analogClock");
    const small = document.getElementById("sys_uptime");

    const pad = (n) => String(n).padStart(2, "0");
    const fmtNow = (d = new Date()) =>
      `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
    const fmtDate = (d = new Date()) =>
      d.toLocaleDateString(undefined, {
        weekday: "long",
        year: "numeric",
        month: "short",
        day: "numeric",
      });

    function drawAnalog(d = new Date()) {
      if (!analog) return;
      const ctx = analog.getContext("2d");
      const ratio = window.devicePixelRatio || 1;
      const cssSize = analog.clientWidth;
      const size = Math.round(cssSize * ratio);
      if (analog.width !== size || analog.height !== size) {
        analog.width = size;
        analog.height = size;
      }
      const w = analog.width,
        h = analog.height,
        r = Math.min(w, h) / 2;

      ctx.clearRect(0, 0, w, h);
      ctx.save();
      ctx.translate(w / 2, h / 2);
      ctx.scale(r / 120, r / 120);

      ctx.beginPath();
      ctx.arc(0, 0, 116, 0, Math.PI * 2);
      ctx.lineWidth = 8;
      ctx.strokeStyle = "rgba(255,255,255,0.2)";
      ctx.stroke();
      for (let i = 0; i < 60; i++) {
        const ang = (i * Math.PI) / 30;
        const len = i % 5 === 0 ? 12 : 6;
        ctx.beginPath();
        ctx.moveTo(98 * Math.cos(ang), 98 * Math.sin(ang));
        ctx.lineTo((98 - len) * Math.cos(ang), (98 - len) * Math.sin(ang));
        ctx.lineWidth = i % 5 === 0 ? 3 : 1;
        ctx.strokeStyle = "rgba(255,255,255,0.5)";
        ctx.stroke();
      }

      const hrs = d.getHours() % 12,
        mins = d.getMinutes(),
        secs = d.getSeconds();

      let ang = ((hrs + mins / 60) * Math.PI) / 6;
      ctx.save();
      ctx.rotate(ang);
      ctx.beginPath();
      ctx.moveTo(-8, 0);
      ctx.lineTo(60, 0);
      ctx.lineWidth = 6;
      ctx.strokeStyle = "rgba(255,255,255,0.9)";
      ctx.stroke();
      ctx.restore();

      ang = ((mins + secs / 60) * Math.PI) / 30;
      ctx.save();
      ctx.rotate(ang);
      ctx.beginPath();
      ctx.moveTo(-12, 0);
      ctx.lineTo(86, 0);
      ctx.lineWidth = 4;
      ctx.strokeStyle = "rgba(255,255,255,0.85)";
      ctx.stroke();
      ctx.restore();

      ang = (secs * Math.PI) / 30;
      ctx.save();
      ctx.rotate(ang);
      ctx.beginPath();
      ctx.moveTo(-14, 0);
      ctx.lineTo(92, 0);
      ctx.lineWidth = 2;
      ctx.strokeStyle = "rgba(0,255,170,0.9)";
      ctx.stroke();
      ctx.restore();

      ctx.beginPath();
      ctx.arc(0, 0, 4, 0, Math.PI * 2);
      ctx.fillStyle = "rgba(255,255,255,0.95)";
      ctx.fill();
      ctx.restore();
    }

    function tickNow() {
      const d = new Date();
      if (nowEl) nowEl.textContent = fmtNow(d);
      if (dateEl) dateEl.textContent = fmtDate(d);
      drawAnalog(d);
    }
    tickNow();
    setInterval(tickNow, 1000);

    let baseSeconds = null,
      bootMs = null;
    function parseUptime(str) {
      if (!str) return null;
      if (/^\d+$/.test(str)) return Number(str);
      const m = str.match(/(?:(\d+)\s*d)?\s*(\d{1,2}):(\d{2}):(\d{2})/i);
      if (!m) return null;
      const days = Number(m[1] || 0),
        h = Number(m[2]),
        min = Number(m[3]),
        s = Number(m[4]);
      return days * 86400 + h * 3600 + min * 60 + s;
    }
    function fmtUptime(total) {
      const days = Math.floor(total / 86400);
      let t = total % 86400;
      const h = Math.floor(t / 3600);
      t %= 3600;
      const m = Math.floor(t / 60);
      const s = t % 60;
      const core = `${String(h).padStart(2, "0")}:${String(m).padStart(
        2,
        "0"
      )}:${String(s).padStart(2, "0")}`;
      return days > 0 ? `${days}d ${core}` : core;
    }
    function tickUptime() {
      if (!uptimeBig) return;
      if (baseSeconds == null) {
        uptimeBig.textContent =
          document.getElementById("sys_uptime")?.textContent.trim() || "—";
        return;
      }
      const elapsed = Math.floor((Date.now() - bootMs) / 1000);
      uptimeBig.textContent = fmtUptime(baseSeconds + elapsed);
    }

    if (small) {
      const obs = new MutationObserver(() => {
        const v = small.textContent.trim();
        const s = parseUptime(v);
        if (s != null) {
          baseSeconds = s;
          bootMs = Date.now();
        } else {
          if (uptimeBig) uptimeBig.textContent = v || "—";
        }
        tickUptime();
      });
      obs.observe(small, {
        childList: true,
        characterData: true,
        subtree: true,
      });
    }
    setInterval(tickUptime, 1000);
  })();

  /* === Collapsible Sections === */
  function setSectionCollapsed(section, collapsed) {
    section.classList.toggle("collapsed", collapsed);
    const btn = section.querySelector(".collapse-toggle");
    const gridId = btn?.getAttribute("aria-controls");
    const grid = gridId
      ? document.getElementById(gridId)
      : section.querySelector(".settings-grid");
    if (btn) btn.setAttribute("aria-expanded", (!collapsed).toString());
    if (grid) grid.hidden = collapsed;
  }
  $$(".settings-section").forEach((sec) => {
    const btn = sec.querySelector(".collapse-toggle");
    if (!btn) return;
    btn.additionallistener = btn.addEventListener("click", () => {
      const isOpen = btn.getAttribute("aria-expanded") !== "false";
      setSectionCollapsed(sec, isOpen);
    });
  });
  $("#btnCollapseAll")?.addEventListener("click", () =>
    $$(".settings-section").forEach((s) => setSectionCollapsed(s, true))
  );
  $("#btnExpandAll")?.addEventListener("click", () =>
    $$(".settings-section").forEach((s) => setSectionCollapsed(s, false))
  );

  /* === Settings Filter === */
  const filterInput = $("#settingsFilter");
  function normalize(s) {
    return (s || "").toLowerCase();
  }
  function matches(el, q) {
    if (!q) return true;
    const text = normalize(el.textContent);
    if (text.includes(q)) return true;
    // also include current values
    const inputs = el.querySelectorAll("input, select");
    for (const i of inputs) {
      if (
        (i.value && normalize(i.value).includes(q)) ||
        (i.placeholder && normalize(i.placeholder).includes(q))
      )
        return true;
    }
    return false;
  }
  function applyFilter() {
    const q = normalize(filterInput?.value);
    $$(".settings-section").forEach((sec) => {
      let any = false;
      sec.querySelectorAll(".settings-grid > .kv").forEach((kv) => {
        const show = matches(kv, q);
        kv.style.display = show ? "" : "none";
        if (show) any = true;
      });
      // If nothing in section matches, collapse/hide grid but keep header visible
      const collapsed = !any;
      setSectionCollapsed(sec, collapsed);
      sec.style.opacity = any || !q ? "" : "0.6";
      sec.style.filter = any || !q ? "" : "grayscale(0.3)";
    });
  }
  filterInput?.addEventListener("input", applyFilter);

  /* === Scroll Spy for right rail === */
  const links = $$("#settingsIndex a");
  const map = new Map(links.map((a) => [a.getAttribute("href"), a]));
  const obs = new IntersectionObserver(
    (entries) => {
      entries.forEach((e) => {
        const id = "#" + e.target.id;
        const link = map.get(id);
        if (!link) return;
        if (e.isIntersecting) {
          links.forEach((l) => l.classList.remove("active"));
          link.classList.add("active");
        }
      });
    },
    { root: $("#icmForm"), rootMargin: "0px 0px -70% 0px", threshold: 0.01 }
  );
  $$(".settings-section[id]").forEach((sec) => obs.observe(sec));
})();
