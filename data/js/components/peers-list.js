import { state } from '../model/state.js';
import { timeAgo } from '../utils/format.js';
export function mountPeers(el){
  let html = '<div class="card"><table class="table"><thead><tr><th>MAC</th><th>Role</th><th>RSSI</th><th>Seen</th></tr></thead><tbody>';
  for(const [mac,p] of state.peersByMac){
    const rssi = p.rssi||0;
    const badge = `<span class="badge" style="border-color:${rssi>-60?'#2d7d46':(rssi>-75?'#e1b12c':'#7d2d2d')}">${rssi}</span>`;
    html += `<tr data-mac="${mac}"><td>${mac}</td><td>${p.role}</td><td>${badge}</td><td>${timeAgo(p.lastSeenMs||0)}</td></tr>`;
  }
  html += '</tbody></table></div>';
  el.innerHTML = html;
  el.querySelectorAll('tr[data-mac]').forEach(tr=>{
    tr.onclick=()=>{ state.selection={ mac: tr.dataset.mac, idx:null }; document.dispatchEvent(new CustomEvent('selection')); };
  });
}
