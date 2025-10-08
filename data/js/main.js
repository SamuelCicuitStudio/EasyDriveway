import { mountTopbar } from './components/topbar.js';
import { mountPaired } from './components/paired-list.js';
import { mountPeers } from './components/peers-list.js';
import { mountDevicePanel } from './components/device-panel.js';
import { mountPairPanel } from './components/pair-panel.js';
import { mountTopo, render as renderTopo } from './components/topo-canvas.js';
import * as api from './api/espnow-api.js';
import { connectWS } from './api/ws.js';
import { state, resetDraft } from './model/state.js';
import { on, emit } from './utils/bus.js';
import { encode, decode, bytesToB64, b64ToBytes } from './api/tlv.js';
import { MOCK_DEVICE, MOCK_PEERS, MOCK_TOPO } from './model/mock.js';

function toast(msg){ const t=document.createElement('div'); t.className='toast'; t.textContent=msg; document.body.appendChild(t); setTimeout(()=>t.remove(),2000); }

async function boot(){
  // Topbar
  let device;
  try{ device = await api.getDevice(); }catch{ device = MOCK_DEVICE; }
  state.device = device;
  mountTopbar(document.querySelector('#topbar'), device);

  // Peers
  let peers;
  try{ peers = await api.getPeers(); }catch{ peers = MOCK_PEERS; }
  state.peersByMac.clear(); peers.forEach(p=>state.peersByMac.set(p.mac,p));

  // Left lists
  mountPaired(document.querySelector('#paired'));
  mountPeers(document.querySelector('#peers'));

  // Right panels
  mountPairPanel(document.querySelector('#pair'));
  document.addEventListener('selection', ()=>{
    mountDevicePanel(document.querySelector('#device'));
  });

  // Center topo
  mountTopo(document.querySelector('#topo'));

  // Toolbar
  mountToolbar();

  // WS
  try{ connectWS(); }catch{ /* ok in mock */ }

  on('mode:change', m=>{ state.mode=m; toast('Mode: '+m); });
}

function mountToolbar(){
  const bar=document.querySelector('#topo-toolbar');
  const mkBtn=(label,fn)=>{ const b=document.createElement('button'); b.textContent=label; b.className='toolbar'; b.onclick=fn; return b; };
  bar.append(
    mkBtn('New', toolbarNew),
    mkBtn('Validate', toolbarValidate),
    mkBtn('Import', toolbarImport),
    mkBtn('Export', toolbarExport),
    mkBtn('Save', toolbarSave),
    mkBtn('PushSel', toolbarPushSel),
    mkBtn('PushAll', toolbarPushAll),
    mkBtn('AutoLayout', toolbarAutoLayout),
    mkBtn('ClearLinks', toolbarClearLinks),
    mkBtn('Simulate', toolbarSim),
    mkBtn('Diff', toolbarDiff),
    mkBtn('Token', toolbarToken),
    mkBtn('Undo', toolbarUndo),
  );
}

export function toolbarNew(){ if(confirm('Clear current draft?')){ resetDraft(); renderTopo(document.querySelector('#topo')); } }
export function toolbarValidate(){
  // minimal: token length check
  if(state.draft.token && state.draft.token.length!==32){ toast('Token must be 32 ASCII'); return; }
  toast('Draft looks OK');
}
export async function toolbarImport(){
  let tlvB64;
  try{ const r=await api.getTopology(); tlvB64=r.tlvB64; }
  catch{ tlvB64 = bytesToB64(encode(MOCK_TOPO)); }
  const obj = decode(b64ToBytes(tlvB64));
  state.draft.nodes = obj.nodes||[]; state.draft.edges=obj.edges||[]; state.draft.token=obj.token||''; state.draft.roleParamsB64=obj.roleParamsB64||'';
  renderTopo(document.querySelector('#topo'));
  toast('Imported');
}
export function toolbarExport(){
  const bytes = encode(state.draft); const b64 = bytesToB64(bytes);
  // download .tlv
  const a=document.createElement('a'); a.href='data:application/octet-stream;base64,'+b64; a.download='topology.tlv'; a.click();
  navigator.clipboard.writeText(Array.from(bytes).map(b=>b.toString(16).padStart(2,'0')).join(' ')).catch(()=>{});
  toast('Exported & hex copied');
}
export async function toolbarSave(){
  const b64 = bytesToB64(encode(state.draft));
  try{ await api.importTopology(b64); toast('Saved'); }catch(e){ toast('Save failed'); }
}
export async function toolbarPushSel(){
  const sel=state.selection?.mac; if(!sel){ toast('Select a device'); return; }
  const b64 = bytesToB64(encode(state.draft));
  try{ await api.pushTopology(sel,b64); toast('Pushed to '+sel); }catch(e){ toast('Push failed'); }
}
export async function toolbarPushAll(){
  const b64 = bytesToB64(encode(state.draft));
  try{ await api.pushTopologyAll(b64); toast('Pushed to all'); }catch(e){ toast('PushAll failed'); }
}
export function toolbarAutoLayout(){
  // even spacing by order
  state.draft.nodes.sort((a,b)=>(a.y||0)-(b.y||0));
  state.draft.nodes.forEach((n,i)=>n.y=60+i*90);
  renderTopo(document.querySelector('#topo'));
}
export function toolbarClearLinks(){ state.draft.edges.length=0; toast('Links cleared'); }
export function toolbarSim(){ alert('Simulation placeholder (visual propagation TBD)'); }
export async function toolbarDiff(){
  try{
    const cur=await api.getTopology();
    alert('Diff length: '+(cur.tlvB64?.length||0)+' vs draft '+(encode(state.draft).length));
  }catch{ alert('No backend; diff skipped'); }
}
export function toolbarToken(){
  const s=Array.from(crypto.getRandomValues(new Uint8Array(16)), b=>b.toString(16).padStart(2,'0')).join('').toUpperCase();
  state.draft.token=s; toast('Token set');
}
export function toolbarUndo(){
  // simple: no-op for now
  toast('Nothing to undo');
}

boot();
