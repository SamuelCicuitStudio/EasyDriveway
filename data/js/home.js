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

  const toast = $('toast');
  const toastMsg = (t)=>{ toast.textContent=t||'Saved'; toast.hidden=false; setTimeout(()=>toast.hidden=true,1600); };

  const fmtUptime = (ms) => {
    const s = Math.floor(ms/1000), d=Math.floor(s/86400), h=Math.floor(s%86400/3600), m=Math.floor(s%3600/60), ss=s%60;
    return `${d}d ${h}h ${m}m ${ss}s`;
  };
  const fmtEpoch = (e)=> e? new Date(e*1000).toISOString().replace('T',' ').replace('Z',''): '—';

  async function refresh() {
    try{
      const res = await fetch('/api/system/status', {cache:'no-store'});
      if (!res.ok) throw new Error('HTTP '+res.status);
      const j = await res.json();

      // Mode + Wi-Fi
      modeBadge.textContent = j.mode || 'AUTO';
      modeVal.textContent   = (j.mode === 'MANUAL') ? 'Manual' : (j.mode || 'Auto');
      const wifi = j.wifi || {};
      wifiVal.textContent = wifi.mode ? `${wifi.mode} ${wifi.ip||''}`.trim() : '—';

      // Power + Faults
      const pwr = j.power || {};
      pwrBadge.textContent = pwr.source || '—';
      pwrSrc.textContent = (pwr.source==='BATTERY')?'On battery':(pwr.source==='WALL'?'Wall power':'Unknown');
      pwrHlth.textContent = pwr.ok ? 'OK' : (pwr.detail || 'Check');

      faultList.innerHTML = '';
      const faults = j.health && Array.isArray(j.health.faults) ? j.health.faults : [];
      fltBadge.textContent = faults.length ? faults.length : 'OK';
      if (!faults.length) {
        const li=document.createElement('li'); li.className='ok'; li.textContent='No faults'; faultList.appendChild(li);
      } else {
        faults.forEach(f=>{ const li=document.createElement('li'); li.className='err'; li.textContent=f; faultList.appendChild(li); });
      }

      // Time & Uptime
      timeNow.textContent = (j.time && j.time.iso) || '—';
      if (typeof j.uptime_ms === 'number') uptime.textContent = fmtUptime(j.uptime_ms);

      // Buzzer
      if (typeof j.buzzer_enabled === 'boolean') buzzerToggle.checked = !!j.buzzer_enabled;

      // Cooling status
      const c = j.cooling || {};
      tempVal.textContent = (typeof c.tempC === 'number') ? c.tempC.toFixed(1) : '—';
      rtcTemp.textContent = (j.time && typeof j.time.tempC === 'number') ? j.time.tempC.toFixed(1) : '—';
      coolBadge.textContent = c.modeApplied || '—';
      coolApplied.textContent = c.modeApplied || '—';
      coolPct.textContent = (typeof c.speedPct === 'number') ? c.speedPct : 0;
      if (typeof c.speedPct === 'number') coolSpeed.value = c.speedPct;
      if (c.modeRequested) coolMode.value = c.modeRequested;

      // Sleep status
      const s = j.sleep || {};
      if (typeof s.timeout_sec === 'number') sleepTimeout.value = s.timeout_sec;
      sleepLeft.textContent = (typeof s.secs_to_sleep === 'number') ? s.secs_to_sleep : '—';
      sleepArmed.textContent = s.armed ? 'Yes' : 'No';
      sleepWake.textContent = s.next_wake_epoch ? fmtEpoch(s.next_wake_epoch) : '—';

    }catch(e){ console.error(e); }
  }

  // Cooling: mode select
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

  // Cooling: manual speed
  coolSpeed.addEventListener('input', ()=>{ coolPct.textContent = coolSpeed.value; });
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

  // Buzzer toggle
  buzzerToggle.addEventListener('change', async () => {
    try{
      const res = await fetch('/api/buzzer/set', {
        method: 'POST', headers: {'Content-Type':'application/json'},
        body: JSON.stringify({ enabled: buzzerToggle.checked })
      });
      if (!res.ok) throw new Error();
      toastMsg('Buzzer '+(buzzerToggle.checked?'enabled':'disabled'));
    }catch(e){ toastMsg('Failed'); }
  });

  // Reset / Restart
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

  // Sleep timer controls
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

  // initial + poll
  refresh();
  setInterval(refresh, 3000);
})();
