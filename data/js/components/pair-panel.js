import { pair, removePeer } from '../api/espnow-api.js';
import { macNorm } from '../utils/format.js';
import { state } from '../model/state.js';

export function mountPairPanel(el){
  el.innerHTML = `
    <div class="card">
      <div class="row"><strong>Pairing</strong></div>
      <div class="row" style="gap:6px; margin-top:8px">
        <input id="mac" placeholder="AA:BB:CC:DD:EE:FF" style="flex:1; background:#0e131b; color:#e8eef4; border:1px solid #2a3142; border-radius:8px; padding:6px 8px">
        <button id="btn-pair" class="btn">Pair</button>
        <button id="btn-remove" class="btn">Remove</button>
      </div>
    </div>`;
  el.querySelector('#btn-pair').onclick=()=>{
    const mac=macNorm(el.querySelector('#mac').value);
    if(mac) pair(mac).catch(console.warn);
  };
  el.querySelector('#btn-remove').onclick=()=>{
    const mac=macNorm(el.querySelector('#mac').value);
    if(mac) removePeer(mac).catch(console.warn);
  };
}
