(() => {
  'use strict';
  const $ = (id) => document.getElementById(id);

  // System
  const modeBadge = $('modeBadge'), modeVal = $('modeVal'), wifiVal = $('wifiVal');
  const pwrBadge  = $('pwrBadge'),  pwrSrc  = $('pwrSrc'),  pwrHlth = $('pwrHlth');
  const fltBadge  = $('fltBadge'),  faultList = $('faultList');
  const timeNow   = $('timeNow'),   uptime = $('uptime');
  const buzzerToggle = $('buzzerToggle');
  const btnReset = $('btnReset'), btnRestart = $('btnRestart');

  // Cooling
  const coolBadge = $('coolBadge'), tempVal = $('tempVal'), rtcTemp = $('rtcTemp');
  const coolMode  = $('coolMode'),  coolApplied = $('coolApplied');
  const coolSpeed = $('coolSpeed'), coolPct = $('coolPct');

  // Sleep
  const sleepBadge = $('sleepBadge'), sleepTimeout = $('sleepTimeout');
  const sleepApply = $('sleepApply'), sleepAfter = $('sleepAfter');
  const sleepSchedule = $('sleepSchedule'), sleepReset = $('sleepReset');
  const sleepLeft = $('sleepLeft'), sleepArmed = $('sleepArmed'), sleepWake = $('sleepWake');

  
  // Logout
  const logoutBtn = $('logoutBtn');

  // Toast
  const toast = $('toast');
  const toastMsg = (t)=>{ toast.textContent=t||'Saved'; toast.hidden=false; setTimeout(()=>toast.hidden=true,1600); };
  // ---- Audio feedback (WebAudio) ----
  let __audioCtx = null;
  function __ensureAudioCtx() {
    try{
      if (!__audioCtx) __audioCtx = new (window.AudioContext || window.webkitAudioContext)();
      if (__audioCtx.state === 'suspended') __audioCtx.resume().catch(()=>{});
    }catch(e){}
    return __audioCtx;
  }
  function playBeep(freq=880, dur=0.12, type='square', vol=0.1) {
    try{
      const ctx = __ensureAudioCtx(); if (!ctx) return;
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      osc.type = type;
      osc.frequency.value = freq;
      gain.gain.setValueAtTime(vol, ctx.currentTime);
      osc.connect(gain); gain.connect(ctx.destination);
      osc.start(); osc.stop(ctx.currentTime + dur);
    }catch(e){}
  }


  // Helpers
  const fmtUptime = (ms) => {
    const s = Math.floor(ms/1000), d=Math.floor(s/86400), h=Math.floor(s%86400/3600), m=Math.floor(s%3600/60), ss=s%60;
    return `${d}d ${h}h ${m}m ${ss}s`;
  };
  const fmtEpoch = (e)=> e? new Date(e*1000).toISOString().replace('T',' ').replace('Z',''): '—';

  // Color helpers
  const BADGE_STATES = ['badge-ok','badge-warn','badge-danger','badge-info'];
  const TEXT_STATES  = ['text-ok','text-warn','text-danger','text-info','text-muted'];

  function setBadge(el, state /* ok|warn|danger|info */) {
    if (!el) return;
    el.classList.remove(...BADGE_STATES);
    if (state) el.classList.add(`badge-${state}`);
  }
  function setText(el, text, state /* ok|warn|danger|info|muted */) {
    if (!el) return;
    el.textContent = (text ?? '—');
    el.classList.remove(...TEXT_STATES);
    if (state) el.classList.add(`text-${state}`);
    el.classList.add('value');
  }

  // -------- Refresh --------
  async function refresh() {
    try{
      const res = await fetch('/api/system/status', {cache:'no-store'});
      if (res.status === 401) { location.href = '/login.html'; return; }
      if (!res.ok) throw new Error('HTTP '+res.status);
      const j = await res.json();

      // Mode + Wi-Fi
      const mode = j.mode || 'AUTO';
      setBadge(modeBadge, mode === 'MANUAL' ? 'warn' : 'ok');
      setText(modeVal, mode === 'MANUAL' ? 'Manual' : 'Auto', mode === 'MANUAL' ? 'warn' : 'ok');

      const wifi = j.wifi || {};
      const wifiMode = wifi.mode || 'OFF';
      const wifiStr = wifiMode ? `${wifiMode} ${wifi.ip||''}`.trim() : '—';
      const wifiColor = (wifiMode === 'STA') ? 'ok' : (wifiMode === 'AP' ? 'info' : 'danger');
      setText(wifiVal, wifiStr, wifiColor);

      // Power + Faults
      const pwr = j.power || {};
      const psrc = pwr.source || 'Unknown';
      setBadge(pwrBadge, psrc === 'BATTERY' ? 'warn' : (psrc === 'WALL' ? 'ok' : 'info'));
      setText(pwrSrc, (psrc==='BATTERY')?'On battery':(psrc==='WALL'?'Wall power':'Unknown'),
              psrc === 'BATTERY' ? 'warn' : (psrc === 'WALL' ? 'ok' : 'info'));
      setText(pwrHlth, pwr.ok ? 'OK' : (pwr.detail || 'Check'), pwr.ok ? 'ok' : 'danger');

      // Faults list
      faultList.innerHTML = '';
      const faults = j.health && Array.isArray(j.health.faults) ? j.health.faults : [];
      setBadge(fltBadge, faults.length ? 'danger' : 'ok');
      fltBadge.textContent = faults.length ? faults.length : 'OK';
      if (!faults.length) {
        const li=document.createElement('li'); li.className='ok'; li.textContent='No faults'; faultList.appendChild(li);
      } else {
        faults.forEach(f=>{ const li=document.createElement('li'); li.className='err'; li.textContent=f; faultList.appendChild(li); });
      }

      // Time & Uptime
      setText(timeNow, (j.time && j.time.iso) || '—', 'info');
      if (typeof j.uptime_ms === 'number') setText(uptime, fmtUptime(j.uptime_ms), 'muted');

      // Buzzer
      if (typeof j.buzzer_enabled === 'boolean') buzzerToggle.checked = !!j.buzzer_enabled;

      // Cooling status
      const c = j.cooling || {};
      setText(tempVal, (typeof c.tempC === 'number') ? c.tempC.toFixed(1) : '—',
              (typeof c.tempC === 'number' && c.tempC >= 55) ? 'danger' :
              (typeof c.tempC === 'number' && c.tempC >= 45) ? 'warn' : 'ok');
      setText(rtcTemp, (j.time && typeof j.time.tempC === 'number') ? j.time.tempC.toFixed(1) : '—', 'muted');

      const coolState = (c.modeApplied || 'AUTO');
      const coolColor = (coolState === 'FORCED') ? 'warn' :
                        (coolState === 'STOPPED') ? 'danger' :
                        (coolState === 'ECO') ? 'info' : 'ok';
      setBadge(coolBadge, coolColor);
      setText(coolApplied, coolState, coolColor);

      if (typeof c.speedPct === 'number') {
        coolSpeed.value = c.speedPct;
        setText(coolPct, c.speedPct, (c.speedPct >= 80) ? 'warn' : 'info');
      }
      if (c.modeRequested) coolMode.value = c.modeRequested;

      // Sleep status
      const s = j.sleep || {};
      if (typeof s.timeout_sec === 'number') sleepTimeout.value = s.timeout_sec;
      setText(sleepLeft, (typeof s.secs_to_sleep === 'number') ? s.secs_to_sleep : '—',
              (typeof s.secs_to_sleep === 'number' && s.secs_to_sleep < 30) ? 'warn' : 'muted');
      setText(sleepArmed, s.armed ? 'Yes' : 'No', s.armed ? 'info' : 'muted');
      setText(sleepWake, s.next_wake_epoch ? fmtEpoch(s.next_wake_epoch) : '—', 'info');

    }catch(e){ console.error(e); }
  }

  // -------- Cooling --------
  coolMode.addEventListener('change', async () => {
    const mode = coolMode.value;
    try{
      const r = await fetch('/api/cooling/mode', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({mode})
      });
      if (!r.ok) throw new Error();
      toastMsg('Cooling mode set: '+mode);
      setTimeout(refresh, 300);
    }catch(e){ toastMsg('Failed'); }
  });

  coolSpeed.addEventListener('input', ()=>{ setText(coolPct, coolSpeed.value, (coolSpeed.value>=80)?'warn':'info'); });
  coolSpeed.addEventListener('change', async () => {
    try{
      const pct = parseInt(coolSpeed.value,10) || 0;
      const r = await fetch('/api/cooling/speed', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({pct})
      });
      if (!r.ok) throw new Error();
      toastMsg('Fan '+pct+'%');
      setTimeout(refresh, 300);
    }catch(e){ toastMsg('Failed'); }
  });

  // -------- Buzzer toggle --------
  (function initBuzzer() {
    if (!buzzerToggle) return;

    async function postJSON(url, body, timeoutMs=2500) {
      const ctrl = new AbortController();
      const t = setTimeout(()=>ctrl.abort(), timeoutMs);
      try {
        const res = await fetch(url, {
          method:'POST',
          headers:{'Content-Type':'application/json'},
          body: JSON.stringify(body),
          signal: ctrl.signal,
          cache:'no-store',
          credentials:'same-origin'
        });
        if (res.ok) return true;
        return false;
      } finally {
        clearTimeout(t);
      }
    }
    async function getQuery(url, params, timeoutMs=2500) {
      const ctrl = new AbortController();
      const t = setTimeout(()=>ctrl.abort(), timeoutMs);
      const qp = new URLSearchParams(params).toString();
      try {
        const res = await fetch(`${url}?${qp}`, {
          method:'GET',
          signal: ctrl.signal,
          cache:'no-store',
          credentials:'same-origin'
        });
        return res.ok;
      } finally {
        clearTimeout(t);
      }
    }

    async function sendBuzzer(enabled) {
      // Try common API variants until one works
      const tries = [
        () => postJSON('/api/buzzer/set', { enabled }),
        () => postJSON('/api/buzzer',     { enabled }),
        () => postJSON('/api/buzzer/set', { on: enabled }),
        () => postJSON('/api/buzzer',     { on: enabled }),
        () => getQuery('/api/buzzer/set', { enabled }),
        () => getQuery('/api/buzzer',     { enabled }),
      ];
      for (const tryFn of tries) {
        try {
          const ok = await tryFn();
          if (ok) return true;
        } catch (e) {
          /* keep trying */
        }
      }
      return false;
    }

    function clickBeepSuccess(enabled) {
      playBeep(enabled ? 1200 : 600, 0.09, 'square', 0.08);
    }
    function clickBeepLocal() {
      playBeep(880, 0.08, 'square', 0.08);
    }

    async function handleToggle(next) {
      // immediate local feedback so UI feels responsive
      clickBeepLocal();
      const ok = await sendBuzzer(next);
      if (ok) {
        toastMsg('Buzzer ' + (next ? 'enabled' : 'disabled'));
        clickBeepSuccess(next);
      } else {
        // revert UI on failure
        buzzerToggle.checked = !next;
        toastMsg('Failed');
      }
    }

    // Events: checkbox, plus the entire switch container (for CSS overlay cases)
    const container = buzzerToggle.closest('.switch');
    buzzerToggle.addEventListener('change', () => handleToggle(buzzerToggle.checked));
    if (container) {
      container.addEventListener('click', (ev) => {
        if (ev.target === buzzerToggle) return; // native change will handle
        ev.preventDefault();
        const next = !buzzerToggle.checked;
        buzzerToggle.checked = next;
        handleToggle(next);
      });
    }
  })();

  // -------- Reset / Restart --------

  btnReset.addEventListener('click', async () => {
    if (!confirm('Factory reset settings?')) return;
    try{
      const r = await fetch('/api/system/reset', {method:'POST'});
      if (!r.ok) throw new Error();
      toastMsg('Reset flag set');
    }catch(e){ toastMsg('Failed'); }
  });

  btnRestart.addEventListener('click', async () => {
    if (!confirm('Restart device now?')) return;
    try{
      const r = await fetch('/api/system/restart', {method:'POST'});
      if (!r.ok) throw new Error();
      toastMsg('Restarting…');
      setTimeout(()=>location.href='/login.html', 2500);
    }catch(e){ toastMsg('Failed'); }
  });

  // -------- Sleep controls --------
  sleepApply.addEventListener('click', async () => {
    try{
      const timeout_sec = parseInt(sleepTimeout.value, 10) || 60;
      const r = await fetch('/api/sleep/timeout', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({timeout_sec})
      });
      if (!r.ok) throw new Error();
      toastMsg('Sleep timeout set');
      setTimeout(refresh, 300);
    }catch(e){ toastMsg('Failed'); }
  });

  sleepSchedule.addEventListener('click', async () => {
    try{
      const after_sec = parseInt(sleepAfter.value, 10) || 1;
      const r = await fetch('/api/sleep/schedule', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({after_sec})
      });
      if (!r.ok) throw new Error();
      toastMsg('Sleep scheduled');
      setTimeout(refresh, 300);
    }catch(e){ toastMsg('Failed'); }
  });

  sleepReset.addEventListener('click', async () => {
    try{
      const r = await fetch('/api/sleep/reset', {method:'POST'});
      if (!r.ok) throw new Error();
      toastMsg('Inactivity reset');
      setTimeout(refresh, 300);
    }catch(e){ toastMsg('Failed'); }
  });

  // -------- Logout (server-side redirect) --------
  if (logoutBtn) {
    logoutBtn.addEventListener('click', () => {
      const f = document.createElement('form');
      f.method = 'POST'; f.action = '/logout';
      document.body.appendChild(f); f.submit();
    });
  }

  // Kickoff
  refresh();
  setInterval(refresh, 3000);
})();
