(() => {
  'use strict';
  const $ = (id) => document.getElementById(id);

  // Elements
  const modeBadge = $('modeBadge'), modeToggle = $('modeToggle'), modeHint = $('modeHint');
  const wifiVal = $('wifiVal'), timeNow = $('timeNow'), uptime = $('uptime');
  const toast = $('toast');
  const devId = $('devId'), hostName = $('hostName'), friendlyName = $('friendlyName');
  const bleName = $('bleName'), blePin = $('blePin'), passPin = $('passPin');
  const fwVer = $('fwVer'), swVer = $('swVer'), hwVer = $('hwVer'), buildStr = $('buildStr');
  // New fields
  const wifiStaSsid = $('wifiStaSsid'), wifiStaPass = $('wifiStaPass'), wifiStaDhcp = $('wifiStaDhcp');
  const espCh = $('espCh'), espMode = $('espMode');
  const webUser = $('webUser'), webPass = $('webPass');
  const buzzerToggle = $('buzzerToggle');
  const btnSaveIdentity = $('btnSaveIdentity'), btnSaveVersions = $('btnSaveVersions');
  const btnSaveNetwork = $('btnSaveNetwork'), btnSaveRadio = $('btnSaveRadio'), btnSaveAccess = $('btnSaveAccess');
  const btnReset = $('btnReset'), btnRestart = $('btnRestart'), logoutBtn = $('logoutBtn');

  const BADGE_STATES = ['badge-ok','badge-warn','badge-danger','badge-info'];
  const TEXT_STATES  = ['text-ok','text-warn','text-danger','text-info','text-muted'];
  function setBadge(el, state){ if(!el) return; el.classList.remove(...BADGE_STATES); if(state) el.classList.add(`badge-${state}`); }
  function setText(el, text, state){ if(!el) return; el.textContent = (text ?? '—'); el.classList.remove(...TEXT_STATES); if(state) el.classList.add(`text-${state}`); el.classList.add('value'); }
  const fmtUptime=(ms)=>{ const s=Math.floor((ms||0)/1000),d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60),ss=s%60; return `${d}d ${h}h ${m}m ${ss}s`; };

  async function loadStatus(){
    const r = await fetch('/api/system/status',{cache:'no-store'});
    if (r.status===401){ location.href='/login.html'; return; }
    const j = await r.json();

    const mode = j.mode || 'AUTO';
    setBadge(modeBadge, mode==='MANUAL' ? 'warn' : 'ok');
    if (modeToggle) modeToggle.checked = (mode==='MANUAL');
    if (modeHint) modeHint.textContent = (mode==='MANUAL') ? 'Manual' : 'Auto';

    const wifi = j.wifi || {};
    const wifiMode = wifi.mode || 'OFF';
    const wifiStr = wifiMode ? `${wifiMode} ${wifi.ip||''}`.trim() : '—';
    const wifiColor = (wifiMode === 'STA') ? 'ok' : (wifiMode === 'AP' ? 'info' : 'danger');
    setText(wifiVal, wifiStr, wifiColor);

    if (j.time && j.time.iso) setText(timeNow, j.time.iso, 'info');
    if (typeof j.uptime_ms === 'number') setText(uptime, fmtUptime(j.uptime_ms), 'muted');

    if (typeof j.buzzer_enabled === 'boolean') buzzerToggle.checked = !!j.buzzer_enabled;

    // Networking (from /api/config/load use 'ssid' and 'password'; /api/system/status doesn't include these)
    // Keep displayed values as-is; actual fields are populated in loadConfig().
  }

  async function loadConfig(){
    const r = await fetch('/api/config/load',{cache:'no-store'});
    if (!r.ok) return;
    const j = await r.json();
    // Identity
    if (j.dev_id) devId.value = j.dev_id;
    if (j.host_name) hostName.value = j.host_name;
    if (j.friendly_name) friendlyName.value = j.friendly_name;
    if (j.ble_name) bleName.value = j.ble_name;
    if (typeof j.ble_password !== 'undefined') blePin.value = String(j.ble_password);
    if (j.pass_pin) passPin.value = j.pass_pin;

    // Versions
    if (j.fw_ver)  fwVer.value  = j.fw_ver;
    if (j.sw_ver)  swVer.value  = j.sw_ver;
    if (j.hw_ver)  hwVer.value  = j.hw_ver;
    if (j.build)   buildStr.value = j.build;

    // Networking form values (backend returns 'ssid' and 'password')
    if (j.ssid) wifiStaSsid.value = j.ssid;
    if (typeof j.password !== 'undefined') wifiStaPass.value = j.password;

    // ESP‑NOW channel (backend key is 'esn_ch')
    if (typeof j.esn_ch !== 'undefined') espCh.value = String(j.esn_ch);
  }

  function toastMsg(msg){ if(!toast) return; toast.textContent = msg || 'Saved'; toast.hidden=false; setTimeout(()=>toast.hidden=true, 1200); }

  // Handlers
  modeToggle?.addEventListener('change', async () => {
    if (modeToggle.disabled) return;
    const manual = !!modeToggle.checked;
    try{
      const r = await fetch('/api/system/mode', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({ manual })
      });
      if (!r.ok) throw new Error();
      const j = await r.json();
      setBadge(modeBadge, j.mode === 'MANUAL' ? 'warn' : 'ok');
      modeHint.textContent = (j.mode === 'MANUAL') ? 'Manual' : 'Auto';
      toastMsg('Mode: ' + j.mode);
      setTimeout(loadStatus, 400);
    }catch(e){ toastMsg('Failed'); }
  });

  btnSaveIdentity?.addEventListener('click', async () => {
    try{
      const payload = {
        dev_id: devId.value.trim(),
        host_name: hostName.value.trim(),
        friendly_name: friendlyName.value.trim(),
        ble_name: bleName.value.trim(),
        ble_password: parseInt(blePin.value,10),
        pass_pin: passPin.value.trim()
      };
      const r = await fetch('/api/config/save', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify(payload)
      });
      if (!r.ok) throw new Error();
      toastMsg('Identity saved'); 
    }catch(e){ toastMsg('Failed'); }
  });

  btnSaveVersions?.addEventListener('click', async () => {
    try{
      const payload = {
        fw_ver: fwVer.value.trim(),
        sw_ver: swVer.value.trim(),
        hw_ver: hwVer.value.trim(),
        build:  buildStr.value.trim()
      };
      const r = await fetch('/api/config/save', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify(payload)
      });
      if (!r.ok) throw new Error();
      toastMsg('Versions saved');
    }catch(e){ toastMsg('Failed'); }
  });

  // Save Networking — backend expects 'ssid' and 'password'
  btnSaveNetwork?.addEventListener('click', async () => {
    try{
      const payload = {
        ssid: wifiStaSsid.value.trim(),
        password: wifiStaPass.value
        // Note: DHCP flag not yet supported server-side; add handling to backend if needed.
      };
      const r = await fetch('/api/config/save', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify(payload)
      });
      if (!r.ok) throw new Error();
      toastMsg('Network saved');
    }catch(e){ toastMsg('Failed'); }
  });

  // Save Radio / ESP‑NOW — backend key is 'esn_ch'
  btnSaveRadio?.addEventListener('click', async () => {
    try{
      const payload = {
        esn_ch: parseInt(espCh.value||'0',10) || 0
      };
      const r = await fetch('/api/config/save', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify(payload)
      });
      if (!r.ok) throw new Error();
      toastMsg('Radio saved');
    }catch(e){ toastMsg('Failed'); }
  });

  // Save Web Access — NOTE: backend currently has no handler for web_user/web_pass
  btnSaveAccess?.addEventListener('click', async () => {
    try{
      const payload = {
        web_user: webUser.value.trim(),
        web_pass: webPass.value
      };
      const r = await fetch('/api/config/save', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify(payload)
      });
      // This will likely be ignored server-side until backend adds support.
      if (!r.ok) throw new Error();
      toastMsg('Web access sent');
    }catch(e){ toastMsg('Failed'); }
  });

  buzzerToggle?.addEventListener('change', async () => {
    try{
      const r = await fetch('/api/buzzer/set', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({enabled: !!buzzerToggle.checked})
      });
      if (!r.ok) throw new Error();
      toastMsg(buzzerToggle.checked ? 'Buzzer on' : 'Buzzer off');
    }catch(e){ toastMsg('Failed'); }
  });

  btnRestart?.addEventListener('click', async () => {
    if (!confirm('Restart device now?')) return;
    await fetch('/api/system/restart', {method:'POST'});
  });
  btnReset?.addEventListener('click', async () => {
    if (!confirm('Factory reset? This will erase configuration.')) return;
    await fetch('/api/config/factory_reset', {method:'POST'});
    toastMsg('Factory reset flagged');
  });

  logoutBtn?.addEventListener('click', async () => {
    await fetch('/logout', {method:'POST'});
    location.href = '/login.html';
  });

  // init
  loadStatus();
  loadConfig();
  setInterval(loadStatus, 3000);
})();