// ui.js — setup palette, legend, props, and controls
import { DEVICE_TYPES, getCSSColor } from './devices.js';
import { computeLinearChain } from './chain.js';
import { downloadJSON, readFileAsText } from './storage.js';

export function setupUI({board, els}){
  const { paletteEl, legendEl, chainListEl, props, fileInput } = els;
  const typeColor = t => getCSSColor(t.colorVar);

  // Palette
  paletteEl.innerHTML = "";
  for (const t of DEVICE_TYPES){
    const item = document.createElement("div");
    item.className = "pal-item";
    item.innerHTML = `
      <span class="pal-badge" style="background:${typeColor(t)}"></span>
      <span class="pal-title">${t.name}</span>
    `;
    item.addEventListener("click", ()=>{
      const node = board.addNode({
        type: t.key,
        name: t.name,
        color: typeColor(t),
        x: 60 + Math.round(Math.random()*120),
        y: 60 + Math.round(Math.random()*120),
      });
      selectNode(node);
      refreshChain();
    });
    paletteEl.appendChild(item);
  }

  // Legend
  legendEl.innerHTML = "";
  for (const t of DEVICE_TYPES){
    const li = document.createElement("li");
    li.innerHTML = `<span class="swatch" style="background:${typeColor(t)}"></span> ${t.name}`;
    legendEl.appendChild(li);
  }

  // Props binding
  const idSpan   = props.querySelector('[data-prop="id"]');
  const typeSpan = props.querySelector('[data-prop="type"]');
  const colorDot = props.querySelector('[data-prop="color"]');
  const nameInput = props.querySelector('[data-prop-input="name"]');
  const notesInput = props.querySelector('[data-prop-input="notes"]');
  const delBtn = document.getElementById("deleteNode");

  let selected = null;
  function selectNode(n){
    selected = n;
    delBtn.disabled = !selected;
    idSpan.textContent = n?.id ?? "—";
    typeSpan.textContent = n?.type ?? "—";
    colorDot.style.background = n?.color ?? "transparent";
    nameInput.value = n?.name ?? "";
    notesInput.value = n?.notes ?? "";
  }

  nameInput.addEventListener("input", ()=>{
    if (!selected) return;
    selected.name = nameInput.value;
    const nodeEl = document.querySelector(`.node[data-id="${selected.id}"] .title`);
    if (nodeEl) nodeEl.textContent = selected.name || selected.type;
    refreshChain();
  });
  notesInput.addEventListener("input", ()=>{
    if (!selected) return;
    selected.notes = notesInput.value;
    const nodeEl = document.querySelector(`.node[data-id="${selected.id}"] .body div`);
    if (nodeEl) nodeEl.textContent = selected.notes || "—";
  });
  delBtn.addEventListener("click", ()=>{
    if (!selected) return;
    const id = selected.id;
    board.deleteNode(id);
    selectNode(null);
    refreshChain();
  });

  board.onSelect = n => selectNode(n);

  // Chain list
  function refreshChain(){
    const chain = computeLinearChain(board.nodes, board.edges);
    chainListEl.innerHTML = "";
    for (const n of chain){
      const li = document.createElement("li");
      li.textContent = `${n.name} (${n.type})`;
      chainListEl.appendChild(li);
    }
  }

  // Topbar actions
  document.getElementById("newProject").addEventListener("click", ()=>{
    if (confirm("Start a new project? Unsaved changes will be lost.")){
      board.clear();
      selectNode(null);
      refreshChain();
    }
  });
  document.getElementById("saveProject").addEventListener("click", ()=>{
    const json = board.exportJSON();
    downloadJSON("topology_project.json", json);
  });
  document.getElementById("loadProject").addEventListener("click", ()=> fileInput.click());
  fileInput.addEventListener("change", async ()=>{
    const f = fileInput.files?.[0];
    if (!f) return;
    const txt = await readFileAsText(f);
    board.importJSON(txt);
    selectNode(null);
    refreshChain();
    fileInput.value = "";
  });

  document.getElementById("fitView").addEventListener("click", ()=> board.setZoom(1));
  document.getElementById("zoomIn").addEventListener("click", ()=> board.setZoom(board.zoom + 0.1));
  document.getElementById("zoomOut").addEventListener("click", ()=> board.setZoom(board.zoom - 0.1));

  // initial
  refreshChain();
}
