// board.js — Board, Node, Edge management (no external libs)
import { getCSSColor } from './devices.js';

let nextNodeId = 1;

export class Board {
  constructor({workspaceEl, wiresSvg}){
    this.workspaceEl = workspaceEl;
    this.wiresSvg = wiresSvg;
    this.nodes = [];
    this.edges = []; // {from, to}
    this.zoom = 1;

    this._drag = { active:false, node:null, offsetX:0, offsetY:0 };
    this._linking = { active:false, fromNodeId:null, tempLine:null };

    this._bindWorkspaceEvents();
    this._observeResize();
  }

  setZoom(f){
    this.zoom = Math.min(2, Math.max(0.4, f));
    this.workspaceEl.style.transform = `scale(${this.zoom})`;
    this._redrawAllEdges();
  }

  addNode({type, name, color, x=40, y=40}){
    const id = String(nextNodeId++);
    const node = { id, type, name: name || type, color, x, y, notes: "" };
    this.nodes.push(node);
    this._renderNode(node);
    return node;
  }

  deleteNode(id){
    const nodeEl = this.workspaceEl.querySelector(`[data-id="${id}"]`);
    if (nodeEl) nodeEl.remove();
    this.nodes = this.nodes.filter(n => n.id !== id);
    this.edges = this.edges.filter(e => e.from !== id && e.to !== id);
    this._redrawAllEdges();
  }

  getNode(id){ return this.nodes.find(n => n.id === id) || null; }

  connect(fromId, toId){
    if (fromId === toId) return;
    // prevent duplicate edges and multiple inputs to the same node (keep linear-ish chains)
    if (this.edges.some(e => e.from === fromId && e.to === toId)) return;
    if (this.edges.some(e => e.to === toId)) return;
    this.edges.push({from: fromId, to: toId});
    this._drawEdge(fromId, toId);
  }

  exportJSON(){
    return JSON.stringify({nodes:this.nodes, edges:this.edges}, null, 2);
  }

  importJSON(jsonString){
    try{
      const data = JSON.parse(jsonString);
      this.clear();
      for (const n of data.nodes || []) {
        const node = { ...n };
        this.nodes.push(node);
        this._renderNode(node);
      }
      for (const e of data.edges || []) {
        this.edges.push({ ...e });
      }
      this._redrawAllEdges();
    }catch(err){
      alert("Invalid project file.");
    }
  }

  clear(){
    this.nodes = [];
    this.edges = [];
    this.workspaceEl.innerHTML = "";
    this._clearWires();
  }

  /* Internal rendering */
  _renderNode(node){
    const el = document.createElement("div");
    el.className = "node";
    el.dataset.id = node.id;
    el.style.left = `${node.x}px`;
    el.style.top = `${node.y}px`;

    const tagBg = node.color || "#aaa";

    el.innerHTML = `
      <div class="head">
        <div class="title">${escapeHtml(node.name)}</div>
        <div class="tag" style="background:${tagBg};box-shadow:0 0 12px ${tagBg}">${escapeHtml(node.type)}</div>
      </div>
      <div class="body">
        <div>${escapeHtml(node.notes || "—")}</div>
      </div>
      <div class="ports">
        <div class="port in" title="Input"></div>
        <div class="port out" title="Output"></div>
      </div>
    `;

    // Dragging
    el.addEventListener("pointerdown", (ev)=>{
      if ((ev.target).classList.contains("port")) return;
      this._drag.active = true;
      this._drag.node = node;
      this._drag.offsetX = ev.clientX - node.x;
      this._drag.offsetY = ev.clientY - node.y;
      el.setPointerCapture(ev.pointerId);
      this._selectNode(node.id);
    });
    el.addEventListener("pointermove", (ev)=>{
      if (!this._drag.active || this._drag.node?.id !== node.id) return;
      const nx = (ev.clientX - this._drag.offsetX);
      const ny = (ev.clientY - this._drag.offsetY);
      node.x = Math.max(8, Math.min(nx, this.workspaceEl.clientWidth - 8));
      node.y = Math.max(8, Math.min(ny, this.workspaceEl.clientHeight - 8));
      el.style.left = `${node.x}px`;
      el.style.top = `${node.y}px`;
      this._redrawEdgesFor(node.id);
    });
    el.addEventListener("pointerup", ()=>{
      this._drag.active = false;
      this._drag.node = null;
    });

    // Linking
    const inPort = el.querySelector(".port.in");
    const outPort = el.querySelector(".port.out");
    inPort.addEventListener("click", ()=>{
      if (!this._linking.active || !this._linking.fromNodeId) return;
      const fromId = this._linking.fromNodeId;
      const toId = node.id;
      this._endLink();
      this.connect(fromId, toId);
    });
    outPort.addEventListener("click", ()=>{
      this._startLink(node.id);
    });

    // Select on click
    el.addEventListener("click", ()=> this._selectNode(node.id));

    this.workspaceEl.appendChild(el);
  }

  _startLink(fromNodeId){
    this._linking.active = true;
    this._linking.fromNodeId = fromNodeId;
    // add a temp line
    const line = document.createElementNS("http://www.w3.org/2000/svg", "path");
    line.setAttribute("stroke", getCSSColor('--accent'));
    line.setAttribute("stroke-width", "3");
    line.setAttribute("fill", "none");
    line.setAttribute("opacity", "0.7");
    this.wiresSvg.appendChild(line);
    this._linking.tempLine = line;
  }

  _endLink(){
    if (this._linking.tempLine) this._linking.tempLine.remove();
    this._linking = { active:false, fromNodeId:null, tempLine:null };
  }

  _bindWorkspaceEvents(){
    // Update temp link path with mouse move over board
    this.workspaceEl.addEventListener("pointermove", (ev)=>{
      if (!this._linking.active || !this._linking.tempLine) return;
      const fromEl = this._nodeEl(this._linking.fromNodeId);
      if (!fromEl) return;
      const from = this._portCenter(fromEl.querySelector(".port.out"));
      const to = { x: ev.offsetX, y: ev.offsetY };
      const d = this._bezierPath(from, to);
      this._linking.tempLine.setAttribute("d", d);
    });

    // Zoom with Ctrl+wheel; otherwise overflow scroll
    this.workspaceEl.addEventListener("wheel", (ev)=>{
      if (ev.ctrlKey){
        ev.preventDefault();
        const delta = ev.deltaY > 0 ? -0.08 : 0.08;
        this.setZoom(this.zoom + delta);
      }
    }, { passive:false });
  }

  _observeResize(){
    const ro = new ResizeObserver(()=> this._redrawAllEdges());
    ro.observe(this.workspaceEl);
  }

  _nodeEl(id){ return this.workspaceEl.querySelector(`.node[data-id="${id}"]`); }

  _portCenter(portEl){
    const wsRect = this.workspaceEl.getBoundingClientRect();
    const r = portEl.getBoundingClientRect();
    return { x: (r.left - wsRect.left) / this.zoom + r.width/2, y: (r.top - wsRect.top)/this.zoom + r.height/2 };
  }

  _drawEdge(fromId, toId){
    const fromEl = this._nodeEl(fromId);
    const toEl = this._nodeEl(toId);
    if (!fromEl || !toEl) return;
    const from = this._portCenter(fromEl.querySelector(".port.out"));
    const to = this._portCenter(toEl.querySelector(".port.in"));
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.dataset.from = fromId;
    path.dataset.to = toId;
    path.setAttribute("d", this._bezierPath(from, to));
    path.setAttribute("stroke", getCSSColor('--accent-2'));
    path.setAttribute("stroke-width", "3");
    path.setAttribute("fill", "none");
    path.setAttribute("opacity", "0.85");
    path.style.filter = "drop-shadow(0 2px 2px rgba(0,0,0,0.5))";
    this.wiresSvg.appendChild(path);
  }

  _redrawEdgesFor(nodeId){
    // Remove & redraw any edges connected to nodeId
    const paths = Array.from(this.wiresSvg.querySelectorAll(`path[data-from="${nodeId}"], path[data-to="${nodeId}"]`));
    for (const p of paths) p.remove();
    for (const e of this.edges) {
      if (e.from === nodeId || e.to === nodeId){
        this._drawEdge(e.from, e.to);
      }
    }
  }

  _redrawAllEdges(){
    this._clearWires();
    for (const e of this.edges) this._drawEdge(e.from, e.to);
  }

  _clearWires(){ this.wiresSvg.innerHTML = ""; }

  _bezierPath(a, b){
    const dx = Math.max(80, Math.abs(b.x - a.x) * 0.5);
    const c1 = { x: a.x + dx, y: a.y };
    const c2 = { x: b.x - dx, y: b.y };
    return `M ${a.x} ${a.y} C ${c1.x} ${c1.y}, ${c2.x} ${c2.y}, ${b.x} ${b.y}`;
  }

  _selectNode(id){
    Array.from(this.workspaceEl.querySelectorAll(".node")).forEach(n => n.classList.remove("selected"));
    const el = this._nodeEl(id);
    if (el) el.classList.add("selected");
    this.onSelect?.(this.getNode(id));
  }
}

// simple HTML escaper
function escapeHtml(s){
  return String(s).replace(/[&<>"']/g, m=>({ "&":"&amp;", "<":"&lt;", ">":"&gt;", '"':"&quot;", "'":"&#039;" }[m]));
}
