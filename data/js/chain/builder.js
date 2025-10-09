// chain/builder.js — renders nodes & links, handles dragging and connect mode
import { bus, el, throttle } from '../utils.js';
import { store, setSelected, updateNodePos, addLink } from '../state.js';
import { pathBetween } from './svg.js';

const NODE_W = 180;
const NODE_H = 80;

export function mountBuilder(root) {
  const chainCanvas = root.querySelector('#chain-canvas');
  const linkLayer = root.querySelector('#link-layer');

  const nodeEls = new Map();

  function colorClass(type) { return `badge--${type}`; }
  function dotClass(type) { return `dot--${type}`; }

  function renderLinks() {
    linkLayer.setAttribute('viewBox', `0 0 ${chainCanvas.clientWidth} ${chainCanvas.clientHeight}`);
    linkLayer.replaceChildren();
    for (const l of store.links) {
      const a = store.nodes.find(n => n.id === l.a);
      const b = store.nodes.find(n => n.id === l.b);
      if (!a || !b) continue;
      const ax = a.x + NODE_W/2, ay = a.y + NODE_H/2;
      const bx = b.x + NODE_W/2, by = b.y + NODE_H/2;
      const p = document.createElementNS('http://www.w3.org/2000/svg','path');
      p.setAttribute('d', pathBetween(ax, ay, bx, by));
      p.setAttribute('fill', 'none');
      p.setAttribute('stroke', 'rgba(170,200,255,0.7)');
      p.setAttribute('stroke-width', '2.5');
      p.setAttribute('class', 'link');
      p.addEventListener('click', (e) => {
        e.stopPropagation();
        setSelected('link', l.id);
      }, { passive: true });
      linkLayer.appendChild(p);
    }
  }

  function renderNodes() {
    const existingIds = new Set(store.nodes.map(n => n.id));
    // Remove stale
    for (const [id, elNode] of nodeEls) {
      if (!existingIds.has(id)) {
        elNode.remove();
        nodeEls.delete(id);
      }
    }
    // Add/update nodes
    for (const n of store.nodes) {
      let elNode = nodeEls.get(n.id);
      if (!elNode) {
        elNode = el('div', { className: 'node', tabIndex: 0, dataset: { id: n.id } });
        const header = el('div', { className: 'node__title' },
          el('span', { className: 'color-dot ' + dotClass(n.type), style: 'width:10px;height:10px;border-radius:50%;' }),
          el('span', { textContent: n.name }),
        );
        const meta = el('div', { className: 'node__meta' },
          el('span', { className: 'badge ' + colorClass(n.type), textContent: n.type }),
          el('span', { className: 'badge', textContent: `(${n.x}, ${n.y})` }),
        );
        elNode.append(header, meta);
        chainCanvas.append(elNode);
        nodeEls.set(n.id, elNode);

        // Pointer-based dragging
        let dragging = false, startX=0, startY=0, origX=0, origY=0;
        const onMove = throttle((e) => {
          if (!dragging) return;
          const dx = e.clientX - startX;
          const dy = e.clientY - startY;
          const nx = Math.max(0, Math.min(chainCanvas.clientWidth - NODE_W, origX + dx));
          const ny = Math.max(0, Math.min(chainCanvas.clientHeight - NODE_H, origY + dy));
          elNode.style.transform = `translate(${nx}px, ${ny}px)`;
          elNode.querySelector('.node__meta .badge:last-child').textContent = `(${nx}, ${ny})`;
          // live link redraw
          updateNodePos(n.id, nx, ny);
          renderLinks();
        }, 8);

        elNode.addEventListener('pointerdown', (e) => {
          e.preventDefault();
          dragging = true;
          startX = e.clientX; startY = e.clientY;
          origX = n.x; origY = n.y;
          elNode.setPointerCapture(e.pointerId);
          setSelected('node', n.id);
        });
        elNode.addEventListener('pointermove', onMove, { passive: true });
        const endDrag = () => { dragging = false; };
        elNode.addEventListener('pointerup', endDrag, { passive: true });
        elNode.addEventListener('pointercancel', endDrag, { passive: true });

        elNode.addEventListener('click', (e) => {
          e.stopPropagation();
          if (!store.connectMode) setSelected('node', n.id);
          else handleConnectClick(n.id);
        }, { passive: true });

        elNode.addEventListener('keydown', (e) => {
          if (e.key === 'Enter') {
            if (!store.connectMode) setSelected('node', n.id);
            else handleConnectClick(n.id);
          }
        });
      }
      // Position
      elNode.style.transform = `translate(${n.x}px, ${n.y}px)`;
      elNode.dataset.selected = (store.selected?.kind === 'node' && store.selected.id === n.id) ? 'true' : 'false';
      elNode.querySelector('.node__title span:nth-child(2)').textContent = n.name;
      elNode.querySelector('.node__meta .badge:last-child').textContent = `(${n.x}, ${n.y})`;
    }
  }

  // Connect mode: click A then B
  let connectFirst = null;
  function handleConnectClick(nodeId) {
    if (!connectFirst) {
      connectFirst = nodeId;
      highlightNode(nodeId, true);
    } else {
      if (connectFirst !== nodeId) {
        addLink(connectFirst, nodeId);
      }
      highlightNode(connectFirst, false);
      connectFirst = null;
    }
  }
  function highlightNode(id, on) {
    const elNode = nodeEls.get(id);
    if (elNode) elNode.style.boxShadow = on
      ? '0 0 0 2px rgba(79,180,119,0.55), 0 10px 30px rgba(0,0,0,0.35)'
      : '';
  }

  // Global listeners
  bus.on('nodes:changed', () => { renderNodes(); renderLinks(); });
  bus.on('nodes:moved', () => { /* links redraw handled in drag */ });
  bus.on('links:changed', () => renderLinks());
  bus.on('selection:changed', () => renderNodes());

  // Initial render
  renderNodes();
  renderLinks();

  // Deselect on background click
  chainCanvas.addEventListener('click', () => setSelected(null, null), { passive: true });

  // Keyboard shortcuts
  window.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && connectFirst) {
      highlightNode(connectFirst, false);
      connectFirst = null;
    }
  }, { passive: true });
}
