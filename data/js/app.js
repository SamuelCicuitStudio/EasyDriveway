/* app.js — ICM Topology Builder (rewritten)
 * - Tighter drag/drop & inspector updates
 * - LocalStorage persistence
 * - Import from peers/topology and simple export
 * - Works with API.useMock true/false
 */
(() => {
  'use strict';

  // DOM
  const railEl   = document.getElementById('rail');
  const trashEl  = document.getElementById('trash');
  const inspEl   = document.getElementById('inspectorPanel');

  const btnMock   = document.getElementById('btnMock');
  const btnSave   = document.getElementById('btnSave');
  const btnLoad   = document.getElementById('btnLoad');
  const btnExport = document.getElementById('btnExport');
  const btnClear  = document.getElementById('btnClear');

  const netMode = document.getElementById('netMode');
  const ipAddr  = document.getElementById('ipAddr');
  const esnCh   = document.getElementById('esnCh');

  // App state
  const State = {
    nodes: [],      // [{id, type: 'relay'|'sensor', label, mac}]
    selectedId: null,
  };

  const LS_KEY = 'icm_nodes_v2';

  // Utils
  const uid = () => Math.random().toString(36).slice(2, 8).toUpperCase();

  // Render rail as [slot][node][slot]...
  function renderRail() {
    railEl.innerHTML = '';
    appendSlot('start');
    State.nodes.forEach((n, i) => {
      railEl.appendChild(renderNode(n));
      appendSlot(String(i));
    });
  }

  function appendSlot(name) {
    const slot = document.createElement('div');
    slot.className = 'slot';
    slot.dataset.slot = name;

    slot.addEventListener('dragover', (e) => {
      e.preventDefault();
      slot.classList.add('dragover');
    });
    slot.addEventListener('dragleave', () => slot.classList.remove('dragover'));
    slot.addEventListener('drop', onDropToSlot);

    railEl.appendChild(slot);
  }

  function renderNode(n) {
    const el = document.createElement('div');
    el.className = 'node';
    el.draggable = true;
    el.dataset.id = n.id;

    el.innerHTML = `
      <div class="icon ${n.type}"></div>
      <div>
        <div class="title">${escapeHtml(n.label || (n.type === 'relay' ? 'Relay' : 'Sensor'))}</div>
        <div class="subtitle">${escapeHtml(n.mac || '')}</div>
      </div>
      <div class="badge">${n.type.toUpperCase()}</div>
    `;

    el.addEventListener('dragstart', (e) => {
      e.dataTransfer.setData(
        'application/json',
        JSON.stringify({ kind: 'move', id: n.id })
      );
      el.classList.add('dragging');
    });
    el.addEventListener('dragend', () => el.classList.remove('dragging'));
    el.addEventListener('click', () => selectNode(n.id));

    if (State.selectedId === n.id) el.classList.add('selected');
    return el;
  }

  function onDropToSlot(e) {
    e.preventDefault();
    const slot = e.currentTarget;
    slot.classList.remove('dragover');

    let data;
    try {
      data = JSON.parse(e.dataTransfer.getData('application/json'));
    } catch (_) { return; }

    const at = slot.dataset.slot;
    const insertIdx = at === 'start' ? 0 : parseInt(at, 10) + 1;

    if (data.kind === 'palette') {
      const node = {
        id: uid(),
        type: data.type,
        label: data.type === 'relay' ? 'Relay Module' : 'Sensor',
        mac: '',
      };
      State.nodes.splice(insertIdx, 0, node);
      renderRail(); selectNode(node.id); persistLocal();
      return;
    }

    if (data.kind === 'move') {
      const curIdx = State.nodes.findIndex(x => x.id === data.id);
      if (curIdx < 0) return;
      const [node] = State.nodes.splice(curIdx, 1);
      const adj = curIdx < insertIdx ? insertIdx - 1 : insertIdx;
      State.nodes.splice(adj, 0, node);
      renderRail(); selectNode(node.id); persistLocal();
      return;
    }
  }

  // Palette drag sources
  document.querySelectorAll('.palette-item').forEach((el) => {
    el.addEventListener('dragstart', (e) => {
      e.dataTransfer.setData(
        'application/json',
        JSON.stringify({ kind: 'palette', type: el.dataset.type })
      );
    });
  });

  // Trash drop target
  ['dragover', 'dragenter'].forEach((evt) =>
    trashEl.addEventListener(evt, (e) => { e.preventDefault(); trashEl.classList.add('dragover'); })
  );
  ['dragleave', 'drop'].forEach((evt) =>
    trashEl.addEventListener(evt, () => trashEl.classList.remove('dragover'))
  );
  trashEl.addEventListener('drop', (e) => {
    e.preventDefault();
    let data; try { data = JSON.parse(e.dataTransfer.getData('application/json')); } catch (_) {}
    if (data && data.kind === 'move') {
      const idx = State.nodes.findIndex(x => x.id === data.id);
      if (idx >= 0) {
        State.nodes.splice(idx, 1);
        if (State.selectedId === data.id) State.selectedId = null;
        renderRail(); clearInspector(); persistLocal();
      }
    }
  });

  // Inspector
  function selectNode(id) {
    State.selectedId = id;
    const n = State.nodes.find(x => x.id === id);
    if (!n) return clearInspector();

    inspEl.classList.remove('empty');
    inspEl.innerHTML = `
      <label>Label</label>
      <input id="fLabel" value="${escapeAttr(n.label || '')}" />
      <div class="row">
        <div>
          <label>Type</label>
          <select id="fType">
            <option value="relay"  ${n.type === 'relay'  ? 'selected' : ''}>Relay</option>
            <option value="sensor" ${n.type === 'sensor' ? 'selected' : ''}>Sensor</option>
          </select>
        </div>
        <div>
          <label>Short ID</label>
          <input id="fId" value="${escapeAttr(n.id)}" />
        </div>
      </div>
      <label>MAC (optional)</label>
      <input id="fMac" placeholder="24:6F:28:..." value="${escapeAttr(n.mac || '')}" />
    `;

    document.getElementById('fLabel').oninput = (e) => { n.label = e.target.value; updateNode(n.id); };
    document.getElementById('fType').onchange = (e) => { n.type  = e.target.value; updateNode(n.id); };
    document.getElementById('fId').oninput    = (e) => {
      const newId = e.target.value.trim();
      if (newId && newId !== n.id && !State.nodes.some(x => x.id === newId)) {
        n.id = newId; State.selectedId = newId; updateNode(newId);
      }
    };
    document.getElementById('fMac').oninput   = (e) => { n.mac   = e.target.value; updateNode(n.id); };
  }

  function updateNode(id) { renderRail(); selectNode(id); persistLocal(); }

  function clearInspector() {
    inspEl.classList.add('empty');
    inspEl.innerHTML = '<div class="empty-text">Select an item to edit its properties.</div>';
  }

  // Build links for API: linear chain prev→next
  function buildLinks() {
    const links = [];
    for (let i = 1; i < State.nodes.length; i++) {
      const a = State.nodes[i - 1], b = State.nodes[i];
      links.push({
        prev: { id: a.id, type: a.type, mac: a.mac },
        next: { id: b.id, type: b.type, mac: b.mac },
      });
    }
    return links;
  }

  // Buttons
  btnMock.addEventListener('click', () => {
    const on = API.setMock(!API.useMock);
    btnMock.textContent = on ? 'Using Mock' : 'Using Live';
    refreshNet();
  });

  btnSave.addEventListener('click', async () => {
    try {
      const links = buildLinks();
      const r = await API.setTopology(links);
      alert(r && r.ok ? 'Topology saved!' : 'Save failed.');
    } catch (e) { alert('Save failed.'); }
  });

  btnExport.addEventListener('click', () => {
    const payload = { links: buildLinks() };
    const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'topology.json';
    a.click();
    URL.revokeObjectURL(a.href);
  });

  btnLoad.addEventListener('click', () => loadFromSource());
  btnClear.addEventListener('click', () => {
    if (confirm('Clear all items?')) {
      State.nodes = [];
      State.selectedId = null;
      renderRail(); clearInspector(); persistLocal();
    }
  });

  // Keyboard delete to remove selected
  document.addEventListener('keydown', (e) => {
    if ((e.key === 'Delete' || e.key === 'Backspace') && State.selectedId) {
      const idx = State.nodes.findIndex(x => x.id === State.selectedId);
      if (idx >= 0) {
        State.nodes.splice(idx, 1);
        State.selectedId = null;
        renderRail(); clearInspector(); persistLocal();
      }
    }
  });

  // Persistence
  function persistLocal() {
    localStorage.setItem(LS_KEY, JSON.stringify(State.nodes));
  }
  function restoreLocal() {
    try {
      const v = JSON.parse(localStorage.getItem(LS_KEY) || '[]');
      if (Array.isArray(v)) State.nodes = v;
    } catch (_) {}
  }

  // Net status
  async function refreshNet() {
    try {
      const n = await API.getNet();
      netMode.textContent = `MODE: ${n.mode || '—'}`;
      ipAddr.textContent  = `IP: ${n.ip   || '—'}`;
      esnCh.textContent   = `CH: ${n.ch   || '—'}`;
    } catch {
      netMode.textContent = 'MODE: —';
      ipAddr.textContent  = 'IP: —';
      esnCh.textContent   = 'CH: —';
    }
  }

  // Load from API (mock/live)
  async function loadFromSource() {
    const peers = await API.getPeers();
    const topo  = await API.getTopology();

    if (topo && Array.isArray(topo.links) && topo.links.length) {
      // Build id→next map, and collect all ids
      const nextBy = new Map();
      const nextSet = new Set();
      const ids = new Set();

      topo.links.forEach(l => {
        const a = l.prev?.id, b = l.next?.id;
        if (!a || !b) return;
        nextBy.set(a, b);
        nextSet.add(b);
        ids.add(a); ids.add(b);
      });

      // Start is the id that never appears as a `next`
      let start = [...ids].find(id => !nextSet.has(id));
      const order = [];
      const guard = ids.size + 5;
      let steps = 0;

      while (start && steps++ < guard) {
        order.push(start);
        start = nextBy.get(start);
      }

      const peerById = new Map((peers.peers || []).map(p => [p.id, p]));
      State.nodes = order.map(id => {
        const p = peerById.get(id) || { id, type: 'relay', label: id, mac: '' };
        return { id: p.id, type: p.type || 'relay', label: p.label || p.id, mac: p.mac || '' };
      });
    } else {
      // Fallback to peers order
      const ps = peers.peers || [];
      State.nodes = ps.map(p => ({ id: p.id, type: p.type, label: p.label, mac: p.mac }));
    }

    renderRail(); clearInspector(); persistLocal();
  }

  // Simple HTML escaping
  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  }
  function escapeAttr(s) { return escapeHtml(s); }

  // Init
  restoreLocal();
  renderRail();
  clearInspector();
  btnMock.textContent = API.useMock ? 'Using Mock' : 'Using Live';
  refreshNet();
})();
