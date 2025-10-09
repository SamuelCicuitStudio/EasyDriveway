// state.js — data model & store for nodes/links
import { uid, el } from './utils.js';
import { bus } from './utils.js';

export const DeviceTypes = [
  { type: 'router',  label: 'Router',  colorVar: '--color-router' },
  { type: 'switch',  label: 'Switch',  colorVar: '--color-switch' },
  { type: 'gateway', label: 'Gateway', colorVar: '--color-gateway' },
  { type: 'sensor',  label: 'Sensor',  colorVar: '--color-sensor' },
  { type: 'camera',  label: 'Camera',  colorVar: '--color-camera' },
  { type: 'actuator',label: 'Actuator',colorVar: '--color-actuator' },
  { type: 'server',  label: 'Server',  colorVar: '--color-server' },
  { type: 'client',  label: 'Client',  colorVar: '--color-client' },
];

export const store = {
  nodes: [], // {id, type, name, x, y}
  links: [], // {id, a, b}
  selected: null, // { kind: 'node'|'link', id }
  connectMode: false,
};

export function initDemoData() {
  const centerX = 600, centerY = 400;
  const n1 = createNode('router', centerX - 200, centerY - 60, 'Router A');
  const n2 = createNode('switch', centerX, centerY - 60, 'Switch B');
  const n3 = createNode('server', centerX + 220, centerY - 60, 'Server C');
  const n4 = createNode('sensor', centerX, centerY + 140, 'Sensor D');

  store.nodes.push(n1, n2, n3, n4);
  store.links.push(createLink(n1.id, n2.id), createLink(n2.id, n3.id), createLink(n2.id, n4.id));
}

export function createNode(type, x=100, y=100, name='Node') {
  return { id: uid('node'), type, x, y, name };
}
export function createLink(a, b) {
  return { id: uid('link'), a, b };
}

export function addNode(node) {
  store.nodes.push(node);
  bus.emit('nodes:changed', store.nodes);
}
export function addLink(a, b) {
  if (a === b) return;
  if (store.links.some(l => (l.a===a && l.b===b) || (l.a===b && l.b===a))) return;
  const link = createLink(a,b);
  store.links.push(link);
  bus.emit('links:changed', store.links);
}

export function removeSelected() {
  if (!store.selected) return;
  if (store.selected.kind === 'node') {
    const id = store.selected.id;
    store.nodes = store.nodes.filter(n => n.id !== id);
    store.links = store.links.filter(l => l.a !== id && l.b !== id);
    store.selected = null;
    bus.emit('nodes:changed', store.nodes);
    bus.emit('links:changed', store.links);
    bus.emit('selection:changed', store.selected);
  } else {
    const id = store.selected.id;
    store.links = store.links.filter(l => l.id !== id);
    store.selected = null;
    bus.emit('links:changed', store.links);
    bus.emit('selection:changed', store.selected);
  }
}

export function setSelected(kind, id) {
  store.selected = kind && id ? { kind, id } : null;
  bus.emit('selection:changed', store.selected);
}

export function updateNodePos(id, x, y) {
  const n = store.nodes.find(n => n.id === id);
  if (!n) return;
  n.x = x; n.y = y;
  bus.emit('nodes:moved', { id, x, y });
}

export function renameNode(id, name) {
  const n = store.nodes.find(n => n.id === id);
  if (!n) return;
  n.name = name;
  bus.emit('nodes:changed', store.nodes);
}

export function exportJSON() {
  return JSON.stringify({ nodes: store.nodes, links: store.links }, null, 2);
}

export async function importJSONFile(file) {
  const text = await file.text();
  const data = JSON.parse(text);
  if (!Array.isArray(data.nodes) || !Array.isArray(data.links)) throw new Error('Invalid JSON structure');
  store.nodes = data.nodes;
  store.links = data.links;
  store.selected = null;
  bus.emit('nodes:changed', store.nodes);
  bus.emit('links:changed', store.links);
  bus.emit('selection:changed', store.selected);
}

export function autoArrange() {
  // Simple layered layout: group by type, spread horizontally
  const types = [...new Set(store.nodes.map(n => n.type))];
  const rows = types.length;
  const width = 2000, height = 1200;
  const rowGap = height / (rows + 1);

  types.forEach((t, rowIdx) => {
    const rowNodes = store.nodes.filter(n => n.type === t);
    const gap = width / (rowNodes.length + 1);
    rowNodes.forEach((n, i) => {
      n.x = Math.round(gap * (i + 1) + 100);
      n.y = Math.round(rowGap * (rowIdx + 1));
    });
  });
  bus.emit('nodes:changed', store.nodes);
}
