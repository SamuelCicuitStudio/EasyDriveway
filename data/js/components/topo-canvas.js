import { state } from '../model/state.js';
import { ROLES } from '../model/roles.js';

export function mountTopo(el){
  el.innerHTML = '';
  el.ondragover=(e)=>{ e.preventDefault(); };
  el.ondrop=(e)=>{
    e.preventDefault();
    const data = JSON.parse(e.dataTransfer.getData('text/plain')||'{}');
    const y = e.offsetY;
    const n = { mac:data.mac, role:data.role, y };
    state.draft.nodes.push(n);
    render(el);
  };
  render(el);
  // keyboard delete
  document.addEventListener('keydown', (ev)=>{
    if(ev.key==='Delete' && state.selection?.mac){
      state.draft.nodes = state.draft.nodes.filter(n=>n.mac!==state.selection.mac);
      state.draft.edges = state.draft.edges.filter(e=>e.srcMac!==state.selection.mac && e.dstMac!==state.selection.mac);
      state.selection={mac:null,idx:null}; render(el);
    }
  });
}

export function render(el){
  el.innerHTML='';
  for(const n of state.draft.nodes){
    const d=document.createElement('div');
    d.className='node '+(ROLES[n.role]?.cls||'');
    d.style.top=(n.y||0)+'px';
    d.style.left='16px';
    d.textContent=(n.label||ROLES[n.role]?.name||'Node')+' '+(n.mac||'');
    d.onclick=()=>{ state.selection={mac:n.mac,idx:n.idx||null}; document.dispatchEvent(new CustomEvent('selection')); };
    // simple drag
    let startY=0, startTop=0;
    d.onmousedown=(ev)=>{ d.classList.add('selected'); startY=ev.clientY; startTop=parseInt(d.style.top||'0'); document.onmousemove=(mv)=>{
      const dy = mv.clientY-startY; d.style.top=(startTop+dy)+'px'; n.y=startTop+dy;
    }; document.onmouseup=()=>{ d.classList.remove('selected'); document.onmousemove=null; document.onmouseup=null; }; };
    el.appendChild(d);
  }
}
