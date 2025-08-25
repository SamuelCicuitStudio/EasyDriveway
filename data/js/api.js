/* api.js — ICM UI API wrapper (rewritten)
 * - Persist mock/live toggle to localStorage
 * - Shared fetchJSON with timeout & basic 401 hook
 * - Same endpoints & method names used by app.js
 */
(() => {
  'use strict';

  const LS_USE_MOCK = 'icm_useMock';

  const API = {
    useMock: (localStorage.getItem(LS_USE_MOCK) ?? '1') === '1',

    endpoints: {
      WIFI_MODE: '/api/wifi/mode',
      PEERS:     '/api/peers/list',
      TOPO_GET:  '/api/topology/get',
      TOPO_SET:  '/api/topology/set',
    },

    setMock(on) {
      this.useMock = !!on;
      localStorage.setItem(LS_USE_MOCK, this.useMock ? '1' : '0');
      return this.useMock;
    },

    async fetchJSON(url, opts = {}) {
      const ctrl = new AbortController();
      const t = setTimeout(() => ctrl.abort(), opts.timeoutMs ?? 8000);

      try {
        const res = await fetch(url, {
          method: opts.method || 'GET',
          headers: Object.assign(
            { 'Content-Type': 'application/json' },
            opts.headers || {}
          ),
          body: opts.body ? JSON.stringify(opts.body) : undefined,
          signal: ctrl.signal,
          // credentials: 'include', // enable if your auth cookie is SameSite=Lax/None
        });

        if (res.status === 401) {
          // Optional: redirect to login screen
          // location.href = '/login.html';
        }
        if (!res.ok) throw new Error(`HTTP ${res.status}`);

        // Some endpoints may reply 204
        const text = await res.text();
        return text ? JSON.parse(text) : {};
      } finally {
        clearTimeout(t);
      }
    },

    async getNet() {
      if (this.useMock) return window.MOCK.wifi;
      return this.fetchJSON(this.endpoints.WIFI_MODE);
    },

    async getPeers() {
      if (this.useMock) return { peers: window.MOCK.peers };
      return this.fetchJSON(this.endpoints.PEERS);
    },

    async getTopology() {
      if (this.useMock) return window.MOCK.topology;
      return this.fetchJSON(this.endpoints.TOPO_GET);
    },

    async setTopology(links) {
      if (this.useMock) {
        console.debug('[MOCK] setTopology', links);
        return { ok: true };
      }
      return this.fetchJSON(this.endpoints.TOPO_SET, {
        method: 'POST',
        body: { links },
      });
    },
  };

  window.API = API;
})();
