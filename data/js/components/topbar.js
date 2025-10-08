import { emit, on } from '../utils/bus.js';
export function mountTopbar(el, device){
  el.innerHTML = `
    <div class="row">
      <div><strong>${device?.name||'ICM'}</strong> <span class="muted">(${device?.mac||''})</span></div>
      <div class="row" style="gap:8px">
        <button id="mode-toggle" class="btn">Mode: <span id="mode-label">Auto</span></button>
        <span id="ws-pill" class="pill muted">WS: offline</span>
      </div>
    </div>`;
  const btn = el.querySelector('#mode-toggle');
  const label = el.querySelector('#mode-label');
  let mode='auto';
  btn.onclick=()=>{ mode = (mode==='auto'?'manual':'auto'); label.textContent = mode[0].toUpperCase()+mode.slice(1); emit('mode:change',mode); };
  on('ws:status', s=>{
    const pill=el.querySelector('#ws-pill');
    pill.textContent = 'WS: ' + (s==='up'?'online':'offline');
    pill.style.borderColor = s==='up'?'#2d7d46':'#7d2d2d';
  });
}
