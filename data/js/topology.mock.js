/* ICM Topology — Full Mock API (localStorage)
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
 *  - POST /logout               -> redirects to /login.html
 *
 * Data persists across reloads via localStorage.
 * Special rule mirrored from real UI:
 *  - At least one POWER peer must exist (but POWER is not part of the chain).
 *
 * Seed profile (v2):
 *  - Power:    1
 *  - Sensors:  5
 *  - Entrance: 1
 *  - Parking:  1
 *  - Relays:   16
 */

(function(){
  const realFetch = window.fetch ? window.fetch.bind(window) : null;

  // ----- Storage keys -----
  const LS_PEERS = 'icm_mock_peers';
  const LS_LINKS = 'icm_mock_links';
  const LS_SEQ   = 'icm_mock_seq';
  const LS_VER   = 'icm_mock_version';
  const VERSION  = 'v2';

  // ----- MAC helpers -----
  const macNormalize12 = s => String(s||'').replace(/[^0-9a-f]/gi,'').toUpperCase().slice(0,12);
  const macFormatColon = s12 => {
    const h = macNormalize12(s12);
    return h.match(/.{1,2}/g)?.join(':') ?? '';
  };
  const macIsComplete = s => /^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(s);

  const makeMacFromIndex = (idx) => {
    // Deterministic MACs in a private prefix (02:00:xx:xx:xx:xx)
    const b = (n) => n.toString(16).toUpperCase().padStart(2,'0');
    const a5 = idx & 0xFF;
    const a4 = (idx >> 8) & 0xFF;
    const a3 = (idx >> 16) & 0xFF;
    const a2 = (idx >> 24) & 0xFF;
    return `${b(0x02)}:${b(0x00)}:${b(a2)}:${b(a3)}:${b(a4)}:${b(a5)}`;
  };

  // ----- Seed peers -----
  function seedPeers(){
    const seed = [];
    let idx = 1;

    // Power (1)
    seed.push({ type:'power', mac: makeMacFromIndex(idx++), online:true });

    // Sensors (5)
    for (let i=0;i<5;i++) seed.push({ type:'sensor', mac: makeMacFromIndex(idx++), online:true });

    // Entrance (1)
    seed.push({ type:'entrance', mac: makeMacFromIndex(idx++), online:true });

    // Parking (1)
    seed.push({ type:'parking', mac: makeMacFromIndex(idx++), online:true });

    // Relays (16)
    for (let i=0;i<16;i++) seed.push({ type:'relay', mac: makeMacFromIndex(idx++), online:true });

    localStorage.setItem(LS_PEERS, JSON.stringify(seed));
    localStorage.setItem(LS_VER, VERSION);
  }

  // Seed on first run or when version changes
  (function ensureSeed(){
    const ver = localStorage.getItem(LS_VER);
    if (!localStorage.getItem(LS_PEERS) || ver !== VERSION) {
      seedPeers();
      // Reset topology when reseeding to avoid dangling MACs
      localStorage.removeItem(LS_LINKS);
    }
  })();

  // ----- Load/Save helpers -----
  const loadPeers = () => {
    try { return JSON.parse(localStorage.getItem(LS_PEERS) || '[]'); }
    catch { return []; }
  };
  const savePeers = arr => localStorage.setItem(LS_PEERS, JSON.stringify(arr||[]));

  const loadLinks = () => {
    try { return JSON.parse(localStorage.getItem(LS_LINKS) || 'null'); }
    catch { return null; }
  };
  const saveLinks = links => localStorage.setItem(LS_LINKS, JSON.stringify({links:Array.isArray(links)?links:[]}));

  const setSeqActive = (flag, anchor) => {
    if (flag) localStorage.setItem(LS_SEQ, anchor||'relay');
    else localStorage.removeItem(LS_SEQ);
  };

  // randomize online bit slightly
  function wobbleOnline(peers){
    peers.forEach(p => { if (Math.random() < 0.02) p.online = !p.online; });
  }

  // response helper
  const respond = (data, status=200, headers={'Content-Type':'application/json'}) => ({
    ok: status >= 200 && status < 300,
    status,
    headers: new Headers(headers),
    json: async () => data,
    text: async () => (typeof data === 'string' ? data : JSON.stringify(data))
  });

  function hasAtLeastOnePower(){
    return loadPeers().some(p => String(p.type).toLowerCase()==='power');
  }

  async function mockFetch(url, opts={}){
    const method = (opts.method || 'GET').toUpperCase();
    let body = {};
    try { if (opts.body) body = JSON.parse(opts.body); } catch {}

    // ---- Peers list ----
    if (url.startsWith('/api/peers/list') && method === 'GET') {
      const peers = loadPeers().map(p => ({...p, mac: macFormatColon(p.mac)}));
      wobbleOnline(peers);
      savePeers(peers);
      return respond({ peers });
    }

    // ---- Pair ----
    if (url.startsWith('/api/peer/pair') && method === 'POST') {
      const type = String(body.type||'').toLowerCase();
      const mac  = macFormatColon(body.mac||'');
      if (!type || !macIsComplete(mac)) return respond({ ok:false, error:'bad input' }, 400);

      const peers = loadPeers();
      const i = peers.findIndex(p => macNormalize12(p.mac) === macNormalize12(mac));
      if (i >= 0) { peers[i].type = type; peers[i].online = true; }
      else peers.push({ type, mac, online:true });
      savePeers(peers);
      return respond({ ok:true });
    }

    // ---- Remove ----
    if (url.startsWith('/api/peer/remove') && method === 'POST') {
      const mac = macFormatColon(body.mac||'');
      const peers = loadPeers().filter(p => macNormalize12(p.mac) !== macNormalize12(mac));
      savePeers(peers);
      // also remove from current topology if present
      const linksObj = loadLinks();
      if (linksObj && Array.isArray(linksObj.links)) {
        const cleaned = linksObj.links.filter(l => macNormalize12(l.mac||'') !== macNormalize12(mac));
        saveLinks(cleaned);
      }
      return respond({ ok:true, removed: mac });
    }

    // ---- Topology get ----
    if (url.startsWith('/api/topology/get') && method === 'GET') {
      const saved = loadLinks();
      if (saved) return respond(saved);
      return respond({ links: [] });
    }

    // ---- Topology set ----
    if (url.startsWith('/api/topology/set') && method === 'POST') {
      if (!hasAtLeastOnePower()) return respond({ ok:false, error:'no power paired' }, 409);
      const links = Array.isArray(body.links) ? body.links : [];
      // normalize MACs and silently drop POWER if present in payload
      const cleaned = links
        .filter(l => !/^\s*power\s*$/i.test(l.type||''))
        .map(l => {
          const out = { type: String(l.type||'').toUpperCase(), mac: macFormatColon(l.mac||'') };
          if (l.prev) out.prev = { type: String(l.prev.type||'').toUpperCase(), mac: macFormatColon(l.prev.mac||'') };
          if (l.next) out.next = { type: String(l.next.type||'').toUpperCase(), mac: macFormatColon(l.next.mac||'') };
          return out;
        });
      saveLinks(cleaned);
      if (body.push) return respond({ ok:true, pushed:true, count: cleaned.length });
      return respond({ ok:true, saved:true, count: cleaned.length });
    }

    // ---- Relay test start/stop ----
    if (url.startsWith('/api/sequence/start') && method === 'POST') {
      const start = macFormatColon(body.start||'');
      setSeqActive(true, start);
      return respond({ ok:true, start });
    }
    if (url.startsWith('/api/sequence/stop') && method === 'POST') {
      setSeqActive(false);
      return respond({ ok:true });
    }

    // ---- Logout ----
    if (url === '/logout' && method === 'POST') {
      setTimeout(()=>{ location.href = '/login.html'; }, 50);
      return respond({ ok:true });
    }

    // ---- Fallback: static files pass-through ----
    return realFetch ? realFetch(url, opts) : respond({ error:'mock: no route' }, 404);
  }

  // Install mock
  window.fetch = mockFetch;
})();
