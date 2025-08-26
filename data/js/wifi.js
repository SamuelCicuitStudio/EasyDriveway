(() => {
  'use strict';
  const $ = (id) => document.getElementById(id);

  // Status elements
  const netBadge = $('netBadge'), netMode = $('netMode'), netIp = $('netIp'), netRssi = $('netRssi'), netCh = $('netCh');

  // Form elements
  const staSsid = $('staSsid'), staPass = $('staPass');
  const apSsid  = $('apSsid'),  apPass  = $('apPass');
  const esnCh   = $('esnCh');
  const bleName = $('bleName'), blePin = $('blePin');

  // Buttons
  const btnSave = $('btnSave'), btnReload = $('btnReload'), btnFactory = $('btnFactory');
  const logoutBtn = $('logoutBtn');

  // Toast
  const toast = $('toast');
  const toastMsg = (t)=>{ toast.textContent=t||'Saved'; toast.hidden=false; setTimeout(()=>toast.hidden=true,1800); };

  const BADGE_STATES = ['badge-ok','badge-warn','badge-danger','badge-info'];
  const TEXT_STATES  = ['text-ok','text-warn','text-danger','text-info','text-muted'];
  function setBadge(el, state){ if(!el) return; el.classList.remove(...BADGE_STATES); if(state) el.classList.add('badge-'+state); }
  function setText(el, text, state){ if(!el) return; el.textContent=(text??'—'); el.classList.remove(...TEXT_STATES); if(state) el.classList.add('text-'+state); el.classList.add('value'); }

  async function loadStatus(){
    try{
      const r = await fetch('/api/wifi/mode', {cache:'no-store'});
      if (r.status === 401) { location.href = '/login.html'; return; }
      if (!r.ok) throw new Error('mode http '+r.status);
      const j = await r.json();
      const mode = j.mode || 'OFF';
      setText(netMode, mode, mode==='STA'?'ok':(mode==='AP'?'info':'danger'));
      setBadge(netBadge, mode==='STA'?'ok':(mode==='AP'?'info':'danger'));
      setText(netIp, j.ip || '—', j.ip ? 'info' : 'muted');
      setText(netRssi, (typeof j.rssi==='number')? j.rssi+' dBm':'—', (typeof j.rssi==='number')?'info':'muted');
      setText(netCh, j.ch ?? '—', (typeof j.ch==='number')?'ok':'muted');
    }catch(e){ console.error(e); }
  }

  async function loadConfig(){
    try{
      const r = await fetch('/api/config/load', {cache:'no-store'});
      if (!r.ok) throw new Error('cfg http '+r.status);
      const j = await r.json();

      // Map JSON fields to inputs
      if (j.ssid) staSsid.value = j.ssid;
      if (j.password) staPass.value = j.password;
      if (j.ap_ssid) apSsid.value = j.ap_ssid;
      if (j.ap_password) apPass.value = j.ap_password;
      if (typeof j.esn_ch === 'number') esnCh.value = j.esn_ch;
      if (j.ble_name) bleName.value = j.ble_name;
      if (typeof j.ble_password === 'number') blePin.value = String(j.ble_password).padStart(6,'0');
      else if (typeof j.ble_password === 'string') blePin.value = j.ble_password;
    }catch(e){ console.error(e); toastMsg('Load failed'); }
  }

  function validate(){
    const ch = parseInt(esnCh.value,10);
    if (!(ch>=1 && ch<=13)) { esnCh.focus(); throw new Error('Channel must be 1..13'); }
    if (apPass.value && apPass.value.length > 0 && apPass.value.length < 8) { apPass.focus(); throw new Error('AP password must be ≥ 8 chars'); }
    if (blePin.value && !/^\d{6}$/.test(blePin.value)) { blePin.focus(); throw new Error('BLE PIN must be 6 digits'); }
    return { ch };
  }

  async function saveConfig(){
    try{
      const { ch } = validate();
      const body = {
        // Wi‑Fi
        ssid: (staSsid.value||''),
        password: (staPass.value||''),
        ap_ssid: (apSsid.value||''),
        ap_password: (apPass.value||''),
        // ESP‑NOW
        esn_ch: ch,
        // BLE
        ble_name: (bleName.value||''),
        ble_password: blePin.value? parseInt(blePin.value,10) : 0
      };
      const r = await fetch('/api/config/save', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify(body)
      });
      if (!r.ok) throw new Error('save http '+r.status);
      toastMsg('Saved to NVS');
    }catch(e){ toastMsg(e.message || 'Save failed'); }
  }

  async function doFactoryReset(){
    if (!confirm('Factory reset configuration?')) return;
    try{
      const r = await fetch('/api/config/factory_reset', {method:'POST'});
      if (!r.ok) throw new Error('factory http '+r.status);
      toastMsg('Factory reset OK'); setTimeout(loadConfig, 800);
    }catch(e){ toastMsg('Factory reset failed'); }
  }

  // Events
  btnReload.addEventListener('click', loadConfig);
  btnSave.addEventListener('click', saveConfig);
  btnFactory.addEventListener('click', doFactoryReset);
  if (logoutBtn) {
    logoutBtn.addEventListener('click', () => {
      const f = document.createElement('form');
      f.method = 'POST'; f.action = '/logout';
      document.body.appendChild(f); f.submit();
    });
  }

  // Kickoff
  loadStatus();
  loadConfig();
  setInterval(loadStatus, 3000);
})();