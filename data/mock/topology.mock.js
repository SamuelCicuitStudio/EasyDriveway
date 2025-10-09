/* ICM Topology — Full Mock API (localStorage) v3
 * Load BEFORE topology.js to intercept fetch() calls.
 *
 * Implements:
 *  - GET  /api/peers/list
 *  - POST /api/peer/pair        {mac,type}
 *  - POST /api/peer/remove      {mac}
 *  - GET  /api/topology/get
 *  - POST /api/topology/set     {links[,push]}
 *  - POST /api/sequence/start   {start, direction}
 *  - POST /api/sequence/stop
 *  - POST /api/sensor/daynight  {mac}
 *  - POST /api/sensor/read      {mac}  // tempC, humidity, pressure_hPa, lux, tf_a, tf_b
 *  - GET  /api/power/info       // 48V nominal mock (vbus_mV, ibus_mA, tempC, on)
 *  - POST /api/power/cmd        {pwr_action:"on|off|status|refresh"}
 *  - POST /logout               -> redirects to /login.html
 *
 * Data persists across reloads via localStorage.
 * Special rule mirrored from real UI:
 *  - At least one POWER peer must exist (but POWER is not part of the chain).
 *
 * Seed profile (v2):
 *  - Adjusted for demo: 16 relays, 8 sensors, 4 PMS (mapped as 'parking')
 *  - Power:    1
 *  - Sensors:  5
 *  - Entrance: 1
 *  - Parking:  1
 *  - Relays:   16
 */
(function () {
  const realFetch = window.fetch ? window.fetch.bind(window) : null;

  // ----- Storage keys -----
  const LS_PEERS = "icm_mock_peers";
  const LS_LINKS = "icm_mock_links";
  const LS_SEQ = "icm_mock_seq";
  const LS_VER = "icm_mock_version";
  const VERSION = "v2";

  // ----- MAC helpers -----
  const macNormalize12 = (s) =>
    String(s || "")
      .replace(/[^0-9a-f]/gi, "")
      .toUpperCase()
      .slice(0, 12);

  const macFormatColon = (s12) => {
    const h = macNormalize12(s12);
    return h.match(/.{1,2}/g)?.join(":") ?? "";
  };

  const macIsComplete = (s) => /^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(s);

  const makeMacFromIndex = (idx) => {
    // Deterministic MACs in a locally administered prefix (02:00:xx:xx:xx:xx)
    const b = (n) => n.toString(16).toUpperCase().padStart(2, "0");
    const a5 = idx & 0xff;
    const a4 = (idx >> 8) & 0xff;
    const a3 = (idx >> 16) & 0xff;
    const a2 = (idx >> 24) & 0xff;
    return `${b(0x02)}:${b(0x00)}:${b(a2)}:${b(a3)}:${b(a4)}:${b(a5)}`;
  };

  // ----- Seed peers -----
  function seedPeers() {
    const seed = [];
    let idx = 1;

    // Power (1)
    seed.push({ type: "power", mac: makeMacFromIndex(idx++), online: true });

    // Sensors (8)
    for (let i = 0; i < 8; i++)
      seed.push({ type: "sensor", mac: makeMacFromIndex(idx++), online: true });

    // Entrance (1)
    seed.push({ type: "entrance", mac: makeMacFromIndex(idx++), online: true });

    // Parking (4)
    for (let i = 0; i < 4; i++)
      seed.push({ type: "parking", mac: makeMacFromIndex(idx++), online: true });

    // Relays (16)
    for (let i = 0; i < 16; i++)
      seed.push({ type: "relay", mac: makeMacFromIndex(idx++), online: true });

    localStorage.setItem(LS_PEERS, JSON.stringify(seed));
    localStorage.setItem(LS_VER, VERSION);
  }

  // Seed on first run or when version changes
  (function ensureSeed() {
    const ver = localStorage.getItem(LS_VER);
    if (!localStorage.getItem(LS_PEERS) || ver !== VERSION) {
      seedPeers();
      // Reset topology when reseeding to avoid dangling MACs
      localStorage.removeItem(LS_LINKS);
    }
  })();

  // ----- Load/Save helpers -----
  const loadPeers = () => {
    try {
      return JSON.parse(localStorage.getItem(LS_PEERS) || "[]");
    } catch {
      return [];
    }
  };
  const savePeers = (arr) =>
    localStorage.setItem(LS_PEERS, JSON.stringify(arr || []));

  const loadLinks = () => {
    try {
      return JSON.parse(localStorage.getItem(LS_LINKS) || "null");
    } catch {
      return null;
    }
  };
  const saveLinks = (links) =>
    localStorage.setItem(
      LS_LINKS,
      JSON.stringify({ links: Array.isArray(links) ? links : [] })
    );

  const setSeqActive = (flag, anchor) => {
    if (flag) localStorage.setItem(LS_SEQ, anchor || "relay");
    else localStorage.removeItem(LS_SEQ);
  };

  // Slightly randomize online bit for some liveliness
  function wobbleOnline(peers) {
    peers.forEach((p) => {
      if (Math.random() < 0.02) p.online = !p.online;
    });
  }

  // ----- Response helper -----
  const respond = (
    data,
    status = 200,
    headers = { "Content-Type": "application/json" }
  ) => ({
    ok: status >= 200 && status < 300,
    status,
    headers: new Headers(headers),
    json: async () => data,
    text: async () => (typeof data === "string" ? data : JSON.stringify(data)),
  });

  function hasAtLeastOnePower() {
    return loadPeers().some((p) => String(p.type).toLowerCase() === "power");
  }

  // ----- RNG utilities (for sensor read) -----
  function seededRndFromMac(mac) {
    // xorshift32 seeded from MAC + time (ms), deterministic per-request but varied across requests
    let seed = 0;
    const s = String(mac || "");
    for (let i = 0; i < s.length; i++)
      seed = (seed * 131 + s.charCodeAt(i)) | 0;
    seed ^= Date.now() & 0xffffffff;
    return () => {
      seed ^= seed << 13;
      seed ^= seed >>> 17;
      seed ^= seed << 5;
      seed |= 0;
      // 0..1
      return (Math.abs(seed) % 10000) / 10000;
    };
  }

  async function mockFetch(url, opts = {}) {
    const method = (opts.method || "GET").toUpperCase();
    let body = {};
    try {
      if (opts.body) body = JSON.parse(opts.body);
    } catch {}

    // ---- Peers list ----
    if (url.startsWith("/api/peers/list") && method === "GET") {
      const peers = loadPeers().map((p) => ({
        ...p,
        mac: macFormatColon(p.mac),
      }));
      wobbleOnline(peers);
      savePeers(peers);
      return respond({ peers });
    }

    // ---- Pair ----
    if (url.startsWith("/api/peer/pair") && method === "POST") {
      const type = String(body.type || "").toLowerCase();
      const mac = macFormatColon(body.mac || "");
      if (!type || !macIsComplete(mac))
        return respond({ ok: false, error: "bad input" }, 400);

      const peers = loadPeers();
      const i = peers.findIndex(
        (p) => macNormalize12(p.mac) === macNormalize12(mac)
      );
      if (i >= 0) {
        peers[i].type = type;
        peers[i].online = true;
      } else {
        peers.push({ type, mac, online: true });
      }
      savePeers(peers);
      return respond({ ok: true });
    }

    // ---- Remove ----
    if (url.startsWith("/api/peer/remove") && method === "POST") {
      const mac = macFormatColon(body.mac || "");
      const peers = loadPeers().filter(
        (p) => macNormalize12(p.mac) !== macNormalize12(mac)
      );
      savePeers(peers);
      // also remove from current topology if present
      const linksObj = loadLinks();
      if (linksObj && Array.isArray(linksObj.links)) {
        const cleaned = linksObj.links.filter(
          (l) => macNormalize12(l.mac || "") !== macNormalize12(mac)
        );
        saveLinks(cleaned);
      }
      return respond({ ok: true, removed: mac });
    }

    // ---- Topology get ----
    if (url.startsWith("/api/topology/get") && method === "GET") {
      const saved = loadLinks();
      if (saved) return respond(saved);
      return respond({ links: [] });
    }

    // ---- Topology set ----
    if (url.startsWith("/api/topology/set") && method === "POST") {
      if (!hasAtLeastOnePower())
        return respond({ ok: false, error: "no power paired" }, 409);
      const links = Array.isArray(body.links) ? body.links : [];
      // normalize MACs and silently drop POWER if present in payload
      const cleaned = links
        .filter((l) => !/^\s*power\s*$/i.test(l.type || ""))
        .map((l) => {
          const out = {
            type: String(l.type || "").toUpperCase(),
            mac: macFormatColon(l.mac || ""),
          };
          if (l.prev)
            out.prev = {
              type: String(l.prev.type || "").toUpperCase(),
              mac: macFormatColon(l.prev.mac || ""),
            };
          if (l.next)
            out.next = {
              type: String(l.next.type || "").toUpperCase(),
              mac: macFormatColon(l.next.mac || ""),
            };
          return out;
        });
      saveLinks(cleaned);
      if (body.push)
        return respond({ ok: true, pushed: true, count: cleaned.length });
      return respond({ ok: true, saved: true, count: cleaned.length });
    }

    // ---- Sensor Day/Night ----
    if (url.startsWith("/api/sensor/daynight") && method === "POST") {
      const mac = macFormatColon(body.mac || "");
      // Deterministic pseudo-random based on MAC for demo
      let hash = 0;
      for (let i = 0; i < mac.length; i++)
        hash = ((hash << 5) - hash + mac.charCodeAt(i)) | 0;
      const is_day = Math.abs(hash) % 2; // 0 or 1
      return respond({ ok: true, is_day });
    }

    // ---- Sensor Read (environment + TF-Luna raw distances) ----
    if (url.startsWith("/api/sensor/read") && method === "POST") {
      const mac = macFormatColon(body.mac || "");
      const rnd = seededRndFromMac(mac);

      // Use day/night to bias lux range a bit
      let isDayGuess = 1;
      {
        let h = 0;
        for (let i = 0; i < mac.length; i++)
          h = ((h << 5) - h + mac.charCodeAt(i)) | 0;
        isDayGuess = Math.abs(h) % 2; // 0 (night) or 1 (day)
      }

      const tempC = 18 + rnd() * 12; // 18–30 °C
      const humidity = 30 + rnd() * 60; // 30–90 %
      const pressure_hPa = 990 + rnd() * 40; // 990–1030 hPa
      const luxMax = isDayGuess ? 1800 : 80; // brighter in daytime
      const lux = Math.round(rnd() * luxMax); // 0–(80 or 1800) lx
      const tf_a = 200 + Math.round(rnd() * 3000); // 200–3200 mm
      const tf_b = 200 + Math.round(rnd() * 3000); // 200–3200 mm

      return respond({
        ok: true,
        mac,
        tempC: Number(tempC.toFixed(1)),
        humidity: Number(humidity.toFixed(1)),
        pressure_hPa: Number(pressure_hPa.toFixed(1)),
        lux,
        tf_a,
        tf_b,
      });
    }

    // ---- Power: state + endpoints (48V nominal) ----
    const LS_PWR = "icm_mock_power";

    function loadPower() {
      try {
        return (
          JSON.parse(localStorage.getItem(LS_PWR) || "null") || {
            on: true,
            vbus_mV: 48000, // 48V nominal (initial)
            ibus_mA: 1200,
            tempC: 32.0,
          }
        );
      } catch {
        return { on: true, vbus_mV: 48000, ibus_mA: 1200, tempC: 32.0 };
      }
    }
    function savePower(p) {
      localStorage.setItem(LS_PWR, JSON.stringify(p));
    }

    function jitter(n, span) {
      return n + (Math.random() * 2 - 1) * span;
    } // ±span

    function evolvePower(p) {
      const t = Date.now() / 1000;
      if (p.on) {
        // ~48V bus, 0.5–4.0A, 30–60°C with gentle movement
        const v = 48000 + Math.round(400 * Math.sin(t * 0.35));
        const i =
          800 +
          Math.round(1200 * Math.abs(Math.sin(t * 0.8)) + Math.random() * 600);
        const temp = 30 + 8 * Math.abs(Math.sin(t * 0.25)) + Math.random() * 3;
        p.vbus_mV = Math.max(0, Math.round(jitter(v, 120)));
        p.ibus_mA = Math.max(0, Math.round(jitter(i, 100)));
        p.tempC = Number(jitter(temp, 0.6).toFixed(1));
      } else {
        // off: rails near-zero, temp drifts around ambient
        p.vbus_mV = Math.max(0, Math.round(jitter(100, 60)));
        p.ibus_mA = Math.max(0, Math.round(jitter(20, 15)));
        const ambient = 25 + 2 * Math.sin(t * 0.1);
        p.tempC = Number(jitter(ambient, 0.4).toFixed(1));
      }
      return p;
    }

    // GET /api/power/info
    if (url.startsWith("/api/power/info") && method === "GET") {
      const p = evolvePower(loadPower());
      savePower(p);
      return respond({
        vbus_mV: p.vbus_mV,
        ibus_mA: p.ibus_mA,
        tempC: p.tempC,
        on: !!p.on,
      });
    }

    // POST /api/power/cmd  { pwr_action: "on"|"off"|"status"|"refresh" }
    if (url.startsWith("/api/power/cmd") && method === "POST") {
      const act = String((body && body.pwr_action) || "").toLowerCase();
      const p = loadPower();
      if (act === "on") p.on = true;
      else if (act === "off") p.on = false;
      // "status" and "refresh" just fall through to return the latest evolved values
      const out = evolvePower(p);
      savePower(out);
      return respond({
        ok: true,
        on: !!out.on,
        vbus_mV: out.vbus_mV,
        ibus_mA: out.ibus_mA,
        tempC: out.tempC,
      });
    }

    // ---- Relay sequence start/stop ----
    if (url.startsWith("/api/sequence/start") && method === "POST") {
      const start = macFormatColon(body.start || "");
      setSeqActive(true, start);
      return respond({ ok: true, start });
    }
    if (url.startsWith("/api/sequence/stop") && method === "POST") {
      setSeqActive(false);
      return respond({ ok: true });
    }

    // ---- Logout ----
    if (url === "/logout" && method === "POST") {
      setTimeout(() => {
        location.href = "/login.html";
      }, 50);
      return respond({ ok: true });
    }

    // ---- Fallback: static files pass-through ----
    return realFetch
      ? realFetch(url, opts)
      : respond({ error: "mock: no route" }, 404);
  }

  // Install mock
  window.fetch = mockFetch;
})();
