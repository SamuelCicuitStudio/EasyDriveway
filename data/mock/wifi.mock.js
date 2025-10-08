// wifi.mock.js — dev-only mock of the REST API used by wifi.js
// Drop this file in /js/ and include it under wifi.js during local testing.
// It monkey-patches window.fetch for the endpoints used by wifi.js only.

(() => {
  'use strict';
  const USE_MOCK = true; // toggle to false to disable

  if (!USE_MOCK) return;

  const STORE_KEY = 'icm.mock.cfg';
  const now = () => new Date().toISOString();

  // default config blob
  const defaults = {
    // matching WiFiAPI.h JSON field names
    ssid: 'HomeNet',
    password: 'secretpass',
    ap_ssid: 'ICM_1A2B3C',
    ap_password: '12345678',
    esn_ch: 6,
    ble_name: 'ICM_1A2B3C',
    ble_password: 123456
  };

  function loadCfg() {
    try {
      const j = JSON.parse(localStorage.getItem(STORE_KEY) || 'null');
      if (j && typeof j === 'object') return j;
    } catch(_){}
    return { ...defaults };
  }
  function saveCfg(obj) {
    localStorage.setItem(STORE_KEY, JSON.stringify(obj));
  }

  const state = {
    cfg: loadCfg(),
    net: { mode:'AP', ip:'192.168.4.1', ch: loadCfg().esn_ch, rssi: -35 }
  };

  const originalFetch = window.fetch.bind(window);

  window.fetch = async (input, init={}) => {
    const url = (typeof input === 'string') ? input : input.url;
    const mth = (init && init.method) ? init.method.toUpperCase() : 'GET';

    // MODE/status
    if (url.endsWith('/api/wifi/mode') && mth === 'GET') {
      return mkJson({ ok:true, ...state.net });
    }

    // CONFIG LOAD
    if (url.endsWith('/api/config/load') && mth === 'GET') {
      return mkJson({ ok:true, ...state.cfg, net: state.net });
    }

    // CONFIG SAVE
    if (url.endsWith('/api/config/save') && mth === 'POST') {
      const body = await readJson(init && init.body);
      if (typeof body.esn_ch === 'number') state.net.ch = body.esn_ch;
      state.cfg = { ...state.cfg, ...body };
      saveCfg(state.cfg);
      return mkJson({ ok:true, saved_at: now() });
    }

    // FACTORY RESET
    if (url.endsWith('/api/config/factory_reset') && mth === 'POST') {
      state.cfg = { ...defaults };
      saveCfg(state.cfg);
      return mkJson({ ok:true, reset_at: now() });
    }

    // fall back to network for anything else
    return originalFetch(input, init);
  };

  function mkJson(obj, code=200) {
    return new Response(JSON.stringify(obj), {
      status: code,
      headers: { 'Content-Type':'application/json' }
    });
  }

  async function readJson(body) {
    if (!body) return {};
    if (typeof body === 'string') return JSON.parse(body);
    if (body instanceof Blob)     return JSON.parse(await body.text());
    try { return JSON.parse(String(body)); } catch(_){ return {}; }
  }
})();