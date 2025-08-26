/* ICM Topology — Real Binder (uses device API) with fixed panels, grouped palette,
 * live MAC search, auto-scroll selection, uniqueness rules, and gauges.
 *
 * Endpoints (WiFiAPI.h):
 *  /api/peers/list   GET
 *  /api/peer/pair    POST {mac,type}
 *  /api/peer/remove  POST {mac}
 *  /api/topology/get GET
 *  /api/topology/set POST {links[,push]}
 *  /api/sequence/... relay test
 */
(() => {
  'use strict';

  // ---------- DOM ----------
  const $ = (id) => document.getElementById(id);
  const palette = $('palette');
  const lane = $('lane');
  const peersTbl = $('peersTbl');
  const peersTblBody = peersTbl?.querySelector('tbody');
  const peersScroll = $('peersScroll');
  const peersCount = $('peersCount');
  const peerSearch = $('peerSearch');
  const toast = $('toast');

  const btnRefreshPeers = $('btnRefreshPeers');
  const btnClear = $('btnClear'), btnLoad = $('btnLoad'), btnSave = $('btnSave'), btnPush = $('btnPush');
  const btnExport = $('btnExport'), fileImport = $('fileImport');

  const pType = $('pType'), pMac = $('pMac'), btnPair = $('btnPair');
  const rMac = $('rMac'), btnRemove = $('btnRemove');

  const selNone = $('selNone'), selInfo = $('selInfo');
  const siKind = $('siKind'), siMac = $('siMac');

  const panelSensor = $('panelSensor');
  const panelRelay  = $('panelRelay');
  const panelPower  = $('panelPower');

  const siSensStatus = $('siSensStatus');
  const siRelState = $('siRelState');

  const tOn = $('tOn'), tOff = $('tOff'), tRead = $('tRead');
  const pwrOn = $('pwrOn'), pwrOff = $('pwrOff');

  const logoutBtn = $('logoutBtn');
  if (logoutBtn) {
    logoutBtn.addEventListener('click', () => {
      const f = document.createElement('form');
      f.method = 'POST'; f.action = '/logout';
      document.body.appendChild(f); f.submit();
    });
  }

  // ---------- MAC helpers ----------
  function macNormalize12(s){
    return String(s||'').replace(/[^0-9a-f]/gi,'').toUpperCase().slice(0,12);
  }
  function macFormatColon(s12){
    const h = macNormalize12(s12);
    return h.match(/.{1,2}/g)?.join(':') ?? '';
  }
  function macIsCompleteColon(s){
    return /^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(s);
  }
  function macEqual(a,b){
    return macNormalize12(a) === macNormalize12(b);
  }
  function bindMacFormatter(inputEl){
    if (!inputEl) return;
    inputEl.setAttribute('maxlength','17');
    inputEl.setAttribute('autocomplete','off');
    inputEl.setAttribute('spellcheck','false');
    inputEl.placeholder = "AA:BB:CC:DD:EE:FF";
    const fmt = () => { inputEl.value = macFormatColon(inputEl.value); };
    inputEl.addEventListener('input', fmt);
    inputEl.addEventListener('blur', fmt);
    inputEl.addEventListener('paste', (e)=>{
      e.preventDefault();
      const text = (e.clipboardData || window.clipboardData).getData('text');
      inputEl.value = macFormatColon(text);
    });
  }
  bindMacFormatter(pMac);
  bindMacFormatter(rMac);

  // ---------- Gauges ----------
  function setGauge(el, value, min, max, unit){
    if (!el) return;
    const v = Number(value ?? 0);
    const lo = Number(min ?? 0);
    const hi = Number(max ?? 100);
    const pct = Math.max(0, Math.min(1, (v - lo) / (hi - lo)));
    el.style.setProperty('--val', v.toString());
    el.style.setProperty('--min', lo.toString());
    el.style.setProperty('--max', hi.toString());
    el.style.setProperty('--pct', pct.toString());

    // write label
    el.innerHTML = `<div class="value">${v.toFixed( (unit==='V'||unit==='A') ? 1 : 0 )}<small>${unit ? ' ' + unit : ''}</small></div>`;
  }

  // ---------- State ----------
  let bricks = [];      // [{id, kind:'ENTRANCE'|'RELAY'|'PARKING'|'SENSOR', mac:'AA:..'}]
  let brickSeq = 1;
  let selectedId = null;            // selected chain brick id
  let selectedPeer = null;          // {type, mac} when selection comes from peers (e.g., POWER)
  let peers = [];       // [{type:'power'|'relay'|'sensor'|'entrance'|'parking', mac, online}]

  const KIND_UP = (t) => String(t||'').toUpperCase();
  const kindLabel = (k) => ({POWER:'Power',ENTRANCE:'Entrance',RELAY:'Relay',PARKING:'Parking',SENSOR:'Sensor'}[k]||k);
  const kindTag   = (k) => ({POWER:'PWR', ENTRANCE:'ENT', RELAY:'REL', PARKING:'PRK', SENSOR:'SNS'}[k]||k);

  function isChainPlaceable(kindUp) {
    return ['ENTRANCE','RELAY','PARKING','SENSOR'].includes(KIND_UP(kindUp));
  }
  const hasKindInChain = (kindUp) => bricks.some(b => KIND_UP(b.kind) === KIND_UP(kindUp));

  // ---------- Toast ----------
  const showToast = (msg) => {
    if (!toast) return;
    toast.textContent = msg || 'OK';
    toast.hidden = false;
    setTimeout(()=>toast.hidden = true, 1500);
  };

  // ---------- Auto-scroll helpers ----------
  function clearPeerRowSelection(){
    peersTbl?.querySelectorAll('tbody tr').forEach(tr => tr.classList.remove('selected'));
  }
  function selectPeerRowByMac(mac){
    if (!peersTbl) return;
    const macFmt = macFormatColon(mac);
    let found = null;
    peersTbl.querySelectorAll('tbody tr').forEach(tr => {
      const tdMac = tr.querySelector('td:nth-child(2)');
      const same = (tdMac?.textContent || '').trim().toUpperCase() === macFmt;
      if (same){ tr.classList.add('selected'); found = tr; } else { tr.classList.remove('selected'); }
    });
    if (found && peersScroll) {
      found.scrollIntoView({block:'nearest', behavior:'smooth'});
    }
  }
  function selectPalettePillByMac(mac){
    const macFmt = macFormatColon(mac);
    let found = null;
    document.querySelectorAll('.pill').forEach(pill => {
      const m = pill.querySelector('.pill-mac')?.textContent?.trim().toUpperCase();
      if (m === macFmt) { pill.classList.add('selected'); found = pill; }
      else { pill.classList.remove('selected'); }
    });
    if (found) found.scrollIntoView({block:'nearest', behavior:'smooth'});
  }
  function scrollBrickIntoView(id){
    const el = [...document.querySelectorAll('.brick')].find(e => e.dataset.id == id);
    if (el) el.scrollIntoView({block:'nearest', inline:'nearest', behavior:'smooth'});
  }

  // ---------- Selection panel ----------
  function updateSelectionPanel(kindUp, mac){
    const kindName = kindLabel(kindUp);
    siKind.textContent = kindName;
    siMac.textContent  = macFormatColon(mac) || '—';

    panelSensor.hidden = panelRelay.hidden = panelPower.hidden = true;

    if (kindUp === 'SENSOR'){
      panelSensor.hidden = false;
      setGauge($('gSensTemp'), 0, -20, 80, '°C');
    } else if (kindUp === 'RELAY'){
      panelRelay.hidden = false;
      setGauge($('gRelTemp'), 0, -20, 100, '°C');
    } else if (kindUp === 'POWER'){
      panelPower.hidden = false;
      setGauge($('gPwrVolt'), 0, 0, 60, 'V');
      setGauge($('gPwrCurr'), 0, 0, 10, 'A');
      setGauge($('gPwrTemp'), 0, 0, 100, '°C');
    }

    selNone.hidden = true;
    selInfo.hidden = false;
  }

  function clearSelectionPanel(){
    selInfo.hidden = true;
    selNone.hidden = false;
    clearPeerRowSelection();
    document.querySelectorAll('.pill.selected').forEach(el => el.classList.remove('selected'));
  }

  // ---------- Lane (chain) rendering ----------
  function redrawLane(){
    lane.innerHTML = '';
    if (!bricks.length) {
      const hint = document.createElement('div');
      hint.className = 'lane-hint';
      hint.innerHTML = 'Drag paired items here in order. The order defines <b>prev/next</b>.';
      lane.appendChild(hint);
      return;
    }
    bricks.forEach((b, i) => {
const el = document.createElement('div');
      el.className = 'brick ' + (String(b.kind||'').toLowerCase());
      el.draggable = true;
      el.dataset.id = b.id;

      const tag = document.createElement('span');
      tag.className = 'tag';
      tag.textContent = kindTag(b.kind);

      const title = document.createElement('span');
      title.className = 'title';
      const macDisp = macFormatColon(b.mac);
      title.textContent = `${kindLabel(b.kind)} · ${macDisp}`;

      el.appendChild(tag);
      el.appendChild(title);
      el.title = macDisp || '';

      el.addEventListener('click', () => selectBrick(b.id));
      el.addEventListener('dragstart', (ev) => {
        ev.dataTransfer.setData('text/icm-brick', String(b.id));
        ev.dataTransfer.effectAllowed = 'move';
      });

      if (selectedId === b.id) el.classList.add('selected');
      
      lane.appendChild(el);
      if (i < bricks.length - 1) {
      const curr = b.kind.toUpperCase();
        const next = bricks[i+1].kind.toUpperCase();
        // Add connector only if one is RELAY and the other is SENSOR/ENTRANCE/PARKING
        const isRelay = (k) => k === 'RELAY';
        const isSensorish = (k) => (k === 'SENSOR' || k === 'ENTRANCE' || k === 'PARKING');
        if ((isRelay(curr) && isSensorish(next)) || (isSensorish(curr) && isRelay(next))) {
          const conn = document.createElement('div');
          conn.className = 'connector';
          lane.appendChild(conn);
        }
      }
    });
}

  function selectBrick(id){
    selectedId = id;
    selectedPeer = null;
    const b = bricks.find(x => x.id === id);
    document.querySelectorAll('.brick').forEach(e => e.classList.remove('selected'));
    const el = [...document.querySelectorAll('.brick')].find(e => e.dataset.id == id);
    if (el) el.classList.add('selected');

    if (!b) { clearSelectionPanel(); return; }
    updateSelectionPanel(KIND_UP(b.kind), b.mac);
    selectPeerRowByMac(b.mac);
    selectPalettePillByMac(b.mac);
    scrollBrickIntoView(id);
  }

  function findAfterId(clientY){
    const rect = lane.getBoundingClientRect();
    const y = clientY - rect.top + lane.scrollTop;
    let bestDy = Infinity, bestId = null;
    const elems = [...lane.querySelectorAll('.brick')];
    if (!elems.length) return null;
    elems.forEach(el => {
      const r = el.getBoundingClientRect();
      const mid = r.top - rect.top + lane.scrollTop + r.height/2;
      const dy = Math.abs(y - mid);
      if (dy < bestDy) { bestDy = dy; bestId = parseInt(el.dataset.id,10); }
    });
    const bestEl = elems.find(e => parseInt(e.dataset.id,10) === bestId);
    if (!bestEl) return null;
    const br = bestEl.getBoundingClientRect();
    const midBest = br.top - rect.top + lane.scrollTop + br.height/2;
    return y >= midBest ? bestId : getPrevId(bestId);
  }
const getPrevId = (id) => { const i = bricks.findIndex(b => b.id === id); return (i<=0)? null : bricks[i-1].id; };

  function reorderBrick(srcId, afterId){
    const iSrc = bricks.findIndex(b => b.id === srcId);
    if (iSrc < 0) return;
    const node = bricks.splice(iSrc, 1)[0];
    if (afterId === null) bricks.splice(0, 0, node);
    else {
      const iAfter = bricks.findIndex(b => b.id === afterId);
      bricks.splice(iAfter+1, 0, node);
    }
    redrawLane();
    scrollBrickIntoView(node.id);
  }

  // ----- Lane DnD target -----
  lane.addEventListener('dragover', (ev) => ev.preventDefault());
  lane.addEventListener('drop', (ev) => {
    ev.preventDefault();
    const brickId = ev.dataTransfer.getData('text/icm-brick');
    if (brickId) {
      const afterId = findAfterId(ev.clientY);
      reorderBrick(parseInt(brickId,10), afterId);
      return;
    }
    const peerJson = ev.dataTransfer.getData('text/icm-peer');
    if (peerJson) {
      try{
        const p = JSON.parse(peerJson);
        if (!p.mac || !p.type) return;
        const typeUp = KIND_UP(p.type);
        if (!isChainPlaceable(typeUp)) { showToast('Power modules are not placed in chain'); return; }
        if (bricks.some(b => macEqual(b.mac, p.mac))) { showToast('Already in chain'); return; }
        // Enforce uniqueness for Entrance & Parking
        if (typeUp === 'ENTRANCE' && hasKindInChain('ENTRANCE')) { showToast('Only one Entrance allowed'); return; }
        if (typeUp === 'PARKING'  && hasKindInChain('PARKING'))  { showToast('Only one Parking allowed'); return; }

        bricks.push({ id: brickSeq++, kind: typeUp, mac: macFormatColon(p.mac) });
        redrawLane();
        renderPalette(); // remove from Paired list once used
      }catch(e){}
    }
  });

  btnClear?.addEventListener('click', () => { bricks = []; selectedId=null; selectedPeer=null; redrawLane(); clearSelectionPanel(); renderPalette(); });

  // ---------- Palette (grouped) ----------
  function isUsed(mac){ return bricks.some(b => macEqual(b.mac, mac)); }
  function makePill(p, titleText, cls){
    const pill = document.createElement('button');
    pill.className = 'pill ' + cls;
    const fullMac = macFormatColon(p.mac||'');
    pill.innerHTML = `<span class="pill-title">${titleText}</span><span class="pill-mac mono">${fullMac}</span>`;
    pill.draggable = isChainPlaceable(p.type);
    pill.addEventListener('dragstart', (ev) => {
      ev.dataTransfer.setData('text/icm-peer', JSON.stringify({ type:p.type, mac: fullMac }));
      ev.dataTransfer.effectAllowed = 'copy';
    });
    pill.addEventListener('click', ()=> {
      selectedId = null;
      selectedPeer = { type:p.type, mac: fullMac };
      updateSelectionPanel(KIND_UP(p.type), fullMac);
      selectPeerRowByMac(fullMac);
      selectPalettePillByMac(fullMac);
    });
    return pill;
  }

  function appendGroup(root, title, items){
    const group = document.createElement('div');
    group.className = 'palette-group';
    const t = document.createElement('div');
    t.className = 'palette-title'; t.textContent = title;
    group.appendChild(t);
    items.forEach(el => group.appendChild(el));
    root.appendChild(group);
  }

  function renderPalette(){
    palette.innerHTML = '';

    // Power (reference only)
    const powerPeers = peers.filter(p => String(p.type).toLowerCase()==='power');
    appendGroup(palette, 'Power (not placeable)', powerPeers.map(p => makePill(p, 'Power', 't-power')));

    // Grouped placeables, excluding ones already in chain
    const sensors = peers.filter(p => String(p.type).toLowerCase()==='sensor'  && !isUsed(p.mac))
                         .map(p => makePill(p, 'Regular sensor', 't-sens'));
    const relays  = peers.filter(p => String(p.type).toLowerCase()==='relay'   && !isUsed(p.mac))
                         .map(p => makePill(p, 'Relay', 't-relay'));
    const entr    = peers.filter(p => String(p.type).toLowerCase()==='entrance'&& !isUsed(p.mac))
                         .map(p => makePill(p, 'Entrance', 't-entr'));
    const park    = peers.filter(p => String(p.type).toLowerCase()==='parking' && !isUsed(p.mac))
                         .map(p => makePill(p, 'Parking', 't-park'));

    appendGroup(palette, 'Regular sensors', sensors);
    appendGroup(palette, 'Relays', relays);
    appendGroup(palette, 'Entrance', entr);
    appendGroup(palette, 'Parking', park);
  }

  // ---------- Peers ----------
  async function fetchPeers(){
    const r = await fetch('/api/peers/list', {cache:'no-store'});
    if (!r.ok) throw new Error('peers list');
    const j = await r.json();
    peers = Array.isArray(j.peers) ? j.peers : [];
    renderPeers();
    renderPalette();
  }

  function renderPeers(){
    if (!peersTblBody) return;
    const q = (peerSearch?.value || '').trim().toUpperCase();
    peersTblBody.innerHTML = '';
    const filtered = peers.filter(p => {
      const mac = macFormatColon(p.mac||'').toUpperCase();
      return !q || mac.includes(q);
    });
    filtered.forEach(p => {
      const tr = document.createElement('tr');
      tr.draggable = isChainPlaceable(p.type);
      if (tr.draggable) {
        tr.addEventListener('dragstart', (ev) => {
          ev.dataTransfer.setData('text/icm-peer', JSON.stringify({ type:p.type, mac: macFormatColon(p.mac||'') }));
          ev.dataTransfer.effectAllowed = 'copy';
        });
      }

      const tdT = document.createElement('td'); tdT.textContent = (p.type||'').toUpperCase();
      const tdM = document.createElement('td'); tdM.textContent = macFormatColon(p.mac||'') || '—';
      const tdO = document.createElement('td'); tdO.textContent = p.online ? 'Yes' : 'No';

      const tdTest = document.createElement('td');
      if ((p.type||'').toLowerCase() === 'relay') {
        const bOn = document.createElement('button'); bOn.className='btn'; bOn.textContent='ON';
        const bOff= document.createElement('button'); bOff.className='btn'; bOff.textContent='OFF';
        bOn.addEventListener('click', ()=> testRelay({ type:p.type, mac: macFormatColon(p.mac||'') }));
        bOff.addEventListener('click',()=> stopSeq());
        tdTest.appendChild(bOn); tdTest.appendChild(bOff);
      } else {
        const bRead = document.createElement('button'); bRead.className='btn'; bRead.textContent='Read';
        bRead.addEventListener('click', ()=> readSensor({ type:p.type, mac: macFormatColon(p.mac||'') }));
        tdTest.appendChild(bRead);
      }

      const tdR = document.createElement('td');
      const bRm = document.createElement('button'); bRm.className='btn danger'; bRm.textContent='X';
      bRm.addEventListener('click', ()=> removePeer(macFormatColon(p.mac||'')));
      tdR.appendChild(bRm);

      // Clicking a row focuses selection
      tr.addEventListener('click', ()=> {
        const macDisp = macFormatColon(p.mac||'');
        if (isChainPlaceable(p.type)) {
          const b = bricks.find(bb => macEqual(bb.mac, macDisp));
          if (b) selectBrick(b.id);
          else { selectedId = null; selectedPeer = { type:p.type, mac: macDisp }; updateSelectionPanel(KIND_UP(p.type), macDisp); }
        } else {
          selectedId = null;
          selectedPeer = { type:p.type, mac: macDisp };
          updateSelectionPanel('POWER', macDisp);
        }
        selectPeerRowByMac(macDisp);
        selectPalettePillByMac(macDisp);
      });

      tr.append(tdT, tdM, tdO, tdTest, tdR);
      peersTblBody.appendChild(tr);
    });
    if (peersCount) peersCount.textContent = String(peers.length);
  }

  peerSearch?.addEventListener('input', () => renderPeers());

  btnRefreshPeers?.addEventListener('click', () => fetchPeers().catch(()=>showToast('Peers refresh failed')));

  async function pairPeer(){
    const mac = macFormatColon(pMac.value);
    if (!macIsCompleteColon(mac)) return showToast('Invalid MAC (use AA:BB:CC:DD:EE:FF)');
    const type = String(pType.value||'').toLowerCase();
    const r = await fetch('/api/peer/pair', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({ mac, type })
    });
    showToast(r.ok ? 'Paired' : 'Pair failed');
    if (r.ok) { pMac.value = ''; fetchPeers().catch(()=>{}); }
  }
  btnPair?.addEventListener('click', ()=> pairPeer().catch(()=>showToast('Pair failed')));

  async function removePeer(mac){
    if (!confirm('Remove peer '+mac+' ?')) return;
    const r = await fetch('/api/peer/remove', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({ mac })
    });
    showToast(r.ok ? 'Removed' : 'Remove failed');
    if (r.ok) fetchPeers().catch(()=>{});
  }
  btnRemove?.addEventListener('click', ()=> {
    const mac = macFormatColon(rMac.value);
    if (!macIsCompleteColon(mac)) return showToast('Invalid MAC');
    removePeer(mac);
  });

  // ---------- Build & persist topology ----------
  function buildLinks(){
    return bricks
      .filter(b => isChainPlaceable(b.kind))
      .map((b, i) => {
        const prev = bricks[i-1];
        const next = bricks[i+1];
        const o = { type: b.kind, mac: macFormatColon(b.mac) };
        if (prev && isChainPlaceable(prev.kind)) o.prev = { type: prev.kind, mac: macFormatColon(prev.mac) };
        if (next && isChainPlaceable(next.kind)) o.next = { type: next.kind, mac: macFormatColon(next.mac) };
        return o;
      });
  }

  function hasAtLeastOnePower(){
    return peers.some(p => String(p.type).toLowerCase()==='power');
  }

  async function loadFromDevice(){
    const r = await fetch('/api/topology/get', {cache:'no-store'});
    if (!r.ok) { showToast('Load failed'); return; }
    const j = await r.json(); // expect {links:[{type,mac,prev?,next?}]}
    const links = Array.isArray(j.links) ? j.links : [];
    bricks = links
      .filter(l => isChainPlaceable(l.type))
      .map(l => ({ id: brickSeq++, kind: KIND_UP(l.type || 'RELAY'), mac: macFormatColon(l.mac||'') }));
    selectedId = null; selectedPeer=null;
    redrawLane(); clearSelectionPanel();
    showToast('Loaded');
    renderPalette();
  }
  btnLoad?.addEventListener('click', ()=> loadFromDevice().catch(()=>showToast('Load failed')));

  async function saveToDevice(pushAlso=false){
    if (!hasAtLeastOnePower()){
      showToast('Pair at least one Power module first');
      return;
    }
    const links = buildLinks();
    const r = await fetch('/api/topology/set', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify(pushAlso ? {links, push:true} : {links})
    });
    showToast(r.ok ? (pushAlso?'Saved & pushed':'Saved') : 'Save failed');
  }
  btnSave?.addEventListener('click', ()=> saveToDevice(false).catch(()=>showToast('Save failed')));
  btnPush?.addEventListener('click', ()=> saveToDevice(true).catch(()=>showToast('Push failed')));

  // export/import
  btnExport?.addEventListener('click', () => {
    const obj = { links: buildLinks() };
    const blob = new Blob([JSON.stringify(obj, null, 2)], {type:'application/json'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'icm-topology.json';
    a.click();
    URL.revokeObjectURL(a.href);
  });

  fileImport?.addEventListener('change', () => {
    const f = fileImport.files[0]; if (!f) return;
    const rd = new FileReader();
    rd.onload = () => {
      try{
        const j = JSON.parse(rd.result);
        if (!Array.isArray(j.links)) { showToast('Invalid file'); return; }
        bricks = j.links
          .filter(l => isChainPlaceable(l.type))
          .map(l => ({ id: brickSeq++, kind: KIND_UP(l.type||'RELAY'), mac: macFormatColon(l.mac||'') }));
        redrawLane(); clearSelectionPanel();
        renderPalette();
        showToast('Imported (not saved)');
      }catch(e){ showToast('Parse error'); }
    };
    rd.readAsText(f);
    fileImport.value = '';
  });

  // ---------- Selected actions ----------
  async function testRelay(peerLike){
    const start = macFormatColon(peerLike.mac||'');
    if (!macIsCompleteColon(start)) return showToast('Missing MAC');
    await fetch('/api/sequence/start', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({ start, direction:'UP' })
    });
    showToast('Relay test ON');
    siRelState.textContent = 'ON';
    setGauge($('gRelTemp'), 35 + Math.random()*10, -20, 100, '°C');
  }
  async function stopSeq(){
    await fetch('/api/sequence/stop', {method:'POST'});
    showToast('Relay OFF');
    siRelState.textContent = 'OFF';
    setGauge($('gRelTemp'), 25 + Math.random()*5, -20, 100, '°C');
  }
  async function readSensor(peerLike){
    showToast('Sensor read requested');
    siSensStatus.textContent = 'OK';
    setGauge($('gSensTemp'), 20 + Math.floor(Math.random()*10), -20, 80, '°C');
  }
  async function powerControl(on){
    showToast(on ? 'Power ON' : 'Power OFF');
    // Demo values for gauges
    if (on){
      setGauge($('gPwrVolt'), 48.0, 0, 60, 'V');
      setGauge($('gPwrCurr'), 2.4, 0, 10, 'A');
      setGauge($('gPwrTemp'), 37, 0, 100, '°C');
    } else {
      setGauge($('gPwrVolt'), 0.0, 0, 60, 'V');
      setGauge($('gPwrCurr'), 0.0, 0, 10, 'A');
      setGauge($('gPwrTemp'), 25, 0, 100, '°C');
    }
  }

  tOn?.addEventListener('click', ()=> {
    let target = null;
    if (selectedId != null){
      const b = bricks.find(x => x.id === selectedId);
      if (b) target = peers.find(pp => macEqual(pp.mac||'', b.mac||''));
    }
    if (!target && selectedPeer && String(selectedPeer.type).toLowerCase()==='relay') {
      target = selectedPeer;
    }
    if (target) testRelay(target); else showToast('Select a relay');
  });
  tOff?.addEventListener('click', ()=> stopSeq());
  tRead?.addEventListener('click', ()=> {
    let target = null;
    if (selectedId != null){
      const b = bricks.find(x => x.id === selectedId);
      if (b) target = peers.find(pp => macEqual(pp.mac||'', b.mac||''));
    }
    if (!target && selectedPeer && String(selectedPeer.type).toLowerCase()==='sensor') {
      target = selectedPeer;
    }
    readSensor(target || {});
  });
  pwrOn?.addEventListener('click', ()=> powerControl(true));
  pwrOff?.addEventListener('click', ()=> powerControl(false));

  // ---------- Init ----------
  fetchPeers().catch(()=>{});
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Delete' && selectedId != null) {
      const i = bricks.findIndex(b => b.id === selectedId);
      if (i >= 0) { bricks.splice(i,1); selectedId=null; redrawLane(); clearSelectionPanel(); renderPalette(); }
    }
  });
})();
