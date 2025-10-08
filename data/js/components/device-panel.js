import * as api from '../api/espnow-api.js';
import { state } from '../model/state.js';

export function mountDevicePanel(el){
  const mac = state.selection?.mac;
  if(!mac){ el.innerHTML = '<div class="card">Select a device…</div>'; return; }
  el.innerHTML = `
    <div class="card">
      <div class="row"><strong>Device</strong><span class="muted">${mac}</span></div>
      <div class="row" style="gap:6px; margin-top:8px">
        <button class="btn" id="buzz">Buzz</button>
        <button class="btn" id="led">LED</button>
        <button class="btn" id="soft">Soft Reset</button>
      </div>
      <div class="row" style="gap:6px; margin-top:8px">
        <button class="btn" id="get-fan">Get Fan</button>
        <button class="btn" id="set-fan">Set Fan: AUTO</button>
      </div>
      <div class="row" style="gap:6px; margin-top:8px">
        <button class="btn" id="logs">Logs</button>
        <button class="btn" id="faults">Faults</button>
      </div>
    </div>`;
  el.querySelector('#buzz').onclick=()=>api.cmdBuzz(mac).catch(console.warn);
  el.querySelector('#led').onclick=()=>api.cmdLed(mac).catch(console.warn);
  el.querySelector('#soft').onclick=()=>api.cmdResetSoft(mac).catch(console.warn);
  el.querySelector('#get-fan').onclick=()=>api.getFan(mac).then(console.log).catch(console.warn);
  el.querySelector('#set-fan').onclick=()=>api.cmdFan(mac,'auto').catch(console.warn);
  el.querySelector('#logs').onclick=()=>api.getLogs(mac,0,256).then(console.log).catch(console.warn);
  el.querySelector('#faults').onclick=()=>api.getFaults(mac).then(console.log).catch(console.warn);
}
