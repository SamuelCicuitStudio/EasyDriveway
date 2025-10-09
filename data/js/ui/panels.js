// ui/panels.js — peers list, details, selected panel, and top controls
import { bus, el } from '../utils.js';
import { DeviceTypes, store, addNode, removeSelected, setSelected, renameNode, exportJSON, importJSONFile, autoArrange } from '../state.js';

export function mountPanels() {
  // Fill device type dropdown
  const typeSelect = document.getElementById('device-type');
  for (const t of DeviceTypes) {
    const opt = el('option', { value: t.type, textContent: t.label });
    typeSelect.append(opt);
  }

  // Buttons
  const addBtn = document.getElementById('add-device');
  const connectBtn = document.getElementById('connect-mode');
  const removeBtn = document.getElementById('remove-selected');
  const exportBtn = document.getElementById('export-json');
  const importInput = document.getElementById('import-json');
  const arrangeBtn = document.getElementById('auto-arrange');

  addBtn.addEventListener('click', () => {
    const type = typeSelect.value || 'client';
    const node = { id: crypto.randomUUID?.() || `node_${Math.random().toString(36).slice(2,9)}`,
                   type, name: `${type[0].toUpperCase()+type.slice(1)} ${store.nodes.length+1}`,
                   x: 200 + Math.round(Math.random()*300), y: 160 + Math.round(Math.random()*240) };
    addNode(node);
    setSelected('node', node.id);
  });

  connectBtn.addEventListener('click', () => {
    const pressed = connectBtn.getAttribute('aria-pressed') === 'true';
    connectBtn.setAttribute('aria-pressed', String(!pressed));
    store.connectMode = !pressed;
  });

  removeBtn.addEventListener('click', () => removeSelected());

  exportBtn.addEventListener('click', () => {
    const blob = new Blob([exportJSON()], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = el('a', { href: url, download: 'topology.json' });
    document.body.append(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  });

  importInput.addEventListener('change', async (e) => {
    const file = e.target.files?.[0];
    if (!file) return;
    try {
      await importJSONFile(file);
    } catch (err) {
      alert('Import failed: ' + err.message);
    } finally {
      importInput.value = '';
    }
  });

  arrangeBtn.addEventListener('click', () => autoArrange());

  // Peers List
  const peersList = document.getElementById('peers-list');
  const peerFilter = document.getElementById('peer-filter');

  function renderPeers() {
    const filter = (peerFilter.value || '').toLowerCase();
    peersList.replaceChildren();
    for (const n of store.nodes) {
      if (filter && !(`${n.name} ${n.type}`.toLowerCase().includes(filter))) continue;
      const li = el('li', { dataset: { id: n.id, selected: (store.selected?.kind==='node' && store.selected.id===n.id) ? 'true':'false' }});
      const dot = el('div', { className: 'color-dot dot--' + n.type });
      const name = el('div', { textContent: n.name });
      const meta = el('div', { className: 'badge', textContent: n.type });
      li.append(dot, name, meta);
      li.addEventListener('click', () => setSelected('node', n.id), { passive: true });
      peersList.append(li);
    }
  }

  // Details & Selected panels
  const detailsBody = document.getElementById('details-body');
  const selectedBody = document.getElementById('selected-body');

  function renderDetails() {
    detailsBody.replaceChildren();
    if (!store.selected) {
      detailsBody.append(el('p', { textContent: 'Select a node or link to see details.' }));
      return;
    }
    if (store.selected.kind === 'node') {
      const n = store.nodes.find(x => x.id === store.selected.id);
      if (!n) return;
      const t = el('div', {},
        el('h3', { textContent: n.name }),
        el('p', { innerHTML: `<strong>Type:</strong> ${n.type}` }),
        el('p', { innerHTML: `<strong>Position:</strong> (${n.x}, ${n.y})` }),
        el('pre', { textContent: JSON.stringify(n, null, 2) }),
      );
      detailsBody.append(t);
    } else {
      const l = store.links.find(x => x.id === store.selected.id);
      if (!l) return;
      const t = el('div', {},
        el('h3', { textContent: 'Link' }),
        el('p', { innerHTML: `<strong>ID:</strong> ${l.id}` }),
        el('p', { innerHTML: `<strong>Between:</strong> ${l.a} ↔ ${l.b}` }),
        el('pre', { textContent: JSON.stringify(l, null, 2) }),
      );
      detailsBody.append(t);
    }
  }

  function renderSelected() {
    selectedBody.replaceChildren();
    if (!store.selected) { selectedBody.append(el('span', { textContent: 'No selection.' })); return; }

    if (store.selected.kind === 'node') {
      const n = store.nodes.find(x => x.id === store.selected.id);
      const nameInput = el('input', { value: n.name, 'aria-label': 'Rename node' });
      nameInput.addEventListener('change', () => renameNode(n.id, nameInput.value));
      const meta = el('span', { className: 'badge', textContent: n.type });
      selectedBody.append(el('span', { textContent: 'Selected:' }), meta, nameInput);
    } else {
      const l = store.links.find(x => x.id === store.selected.id);
      selectedBody.append(el('span', { textContent: `Selected link ${l.id}` }));
    }
  }

  // Bus subscriptions
  bus.on('nodes:changed', () => { renderPeers(); renderDetails(); renderSelected(); });
  bus.on('links:changed', () => { renderDetails(); renderSelected(); });
  bus.on('selection:changed', () => { renderPeers(); renderDetails(); renderSelected(); });

  // Initial render
  renderPeers(); renderDetails(); renderSelected();

  // Filter
  peerFilter.addEventListener('input', () => renderPeers(), { passive: true });
}
