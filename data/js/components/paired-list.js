import { ROLES } from '../model/roles.js';
import { state } from '../model/state.js';
export function mountPaired(el){
  // group by role
  const groups = new Map();
  for(const [mac,p] of state.peersByMac){
    const role = p.role||0; (groups.get(role) || groups.set(role,[]).get(role)).push(p);
  }
  el.innerHTML = '';
  for(const [role, items] of groups){
    const g = document.createElement('div'); g.className='card list '+(ROLES[role]?.cls||'');
    g.innerHTML = `<div class="row"><strong>${ROLES[role]?.name||('Role '+role)}</strong><span class="muted">${items.length}</span></div>`;
    for(const it of items){
      const r = document.createElement('div'); r.className='row';
      r.draggable = true;
      r.dataset.mac = it.mac;
      r.dataset.role = it.role;
      r.innerHTML = `<div>${it.label||it.mac}${it.emuCount?` <span class="badge">×${it.emuCount}</span>`:''}</div><div class="muted">${it.mac}</div>`;
      r.addEventListener('dragstart', ev=>{
        ev.dataTransfer.setData('text/plain', JSON.stringify({mac:it.mac,role:it.role,emuCount:it.emuCount||0}));
      });
      g.appendChild(r);
    }
    el.appendChild(g);
  }
}
