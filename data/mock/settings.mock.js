
/**
 * settings.mock.js
 * Drop-in mock to populate ICM Settings page without a backend.
 * Include BEFORE js/settings.js:
 *   <script src="mock/settings.mock.js"></script>
 *   <script src="js/settings.js"></script>
 */
(function () {
  if (window.__ICM_SETTINGS_MOCK__) return;
  window.__ICM_SETTINGS_MOCK__ = true;

  // Example info and config state held in-memory
  const info = {
    device_id: "ICM-DEV-42A7",
    firmware: "v1.4.2",
    build: "2025-10-10T12:34:56Z",
    espnow_channel: 6,
    uptime: "03:21:45"
  };

  const config = {
    pair_mode: true,
    refresh_topology_interval: 8,
    broadcast_retry_count: 3,
    allowed_roles: "PMS,SENSOR,RELAY,EMULATOR",
    time_sync_interval: 120,
    rtc_sync_source: "NTP",
    auto_time_sync: true,
    timezone_offset: 1,
    sync_on_boot: true,

    relay_command_mode: "DIRECT",
    manual_override: true,
    pms_auto_switch: false,
    relay_default_state: "OFF",

    icm_temp_threshold: 65,
    icm_temp_warning: 55,
    fan_mode: "AUTO",
    buzzer_mode: "ALERT",
    safety_lockout: false,
    fault_alert_enable: true,

    rgb_mode: "STATUS",
    rgb_color_idle: "#00ffaa",
    rgb_color_alert: "#ff3366",
    led_brightness: 180,
    blink_on_activity: true,

    logging_enabled: true,
    log_detail_level: "INFO",
    log_rotation_size: 512,
    log_auto_cleanup_days: 14,

    auto_reconnect_nodes: true,
    fault_retry_limit: 5,
    priority_override: "CCID",
    alert_on_new_device: true,
    web_update_enable: true,

    web_theme: "dark",
    refresh_interval_ui: 1000,
    admin_password: "admin",
    auto_save_config: false,

    espnow_channel: 6,
    broadcast_mac: "FF:FF:FF:FF:FF:FF",
    ping_interval: 10,
    ping_timeout: 3,
    restart_delay: 500
  };

  const snapshot = {
    generated_at: new Date().toISOString(),
    chain: ["ICM", "PMS", "RELAY-1", "SENSOR-TH1"],
    nodes: [
      { mac: "AA:BB:CC:DD:EE:01", role: "PMS", status: "online" },
      { mac: "AA:BB:CC:DD:EE:02", role: "RELAY", status: "online" },
      { mac: "AA:BB:CC:DD:EE:03", role: "SENSOR", status: "sleep" }
    ]
  };

  const originalFetch = window.fetch.bind(window);

  function jsonResponse(data, status = 200) {
    return new Response(JSON.stringify(data), {
      status,
      headers: { "Content-Type": "application/json" }
    });
  }

  // Small helper to parse bodies if present
  async function parseBody(init) {
    if (!init || !init.body) return {};
    try { return JSON.parse(init.body); } catch { return {}; }
  }

  // Intercept calls for the known endpoints
  window.fetch = async function (input, init = {}) {
    const url = typeof input === "string" ? input : input.url;
    const method = (init.method || "GET").toUpperCase();
    const path = (new URL(url, window.location.origin)).pathname;

    switch (path) {
      case "/api/icm/info":
        if (method === "GET") return jsonResponse(info);
        break;

      case "/api/icm/config":
        if (method === "GET") return jsonResponse(config);
        if (method === "POST") {
          const delta = await parseBody(init);
          Object.assign(config, delta);
          return jsonResponse({ ok: true, saved: true });
        }
        break;

      case "/api/icm/remove": {
        const body = await parseBody(init);
        return jsonResponse({ ok: true, removed: body.mac || null });
      }

      case "/api/icm/reset_all":
        return jsonResponse({ ok: true, message: "reset broadcast issued" });

      case "/api/icm/restart":
        return jsonResponse({ ok: true, message: "icm restarting" });

      case "/api/icm/export_topology":
        return jsonResponse(snapshot);
    }

    // Fallback to the real network for anything else
    return originalFetch(input, init);
  };
})();
