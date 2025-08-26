/* ICM Home — Mocked API driver
 * Drop-in replacement for js/home.js to demo the UI with no firmware.
 * To enable: use <script src="js/home.mock.js"></script> instead of home.js
 */

/* ---------------------------
   0) Small helpers
----------------------------*/
const nowSec = () => Math.floor(Date.now() / 1000);
const isoNow = () => new Date().toISOString();
const clamp  = (v, lo, hi) => Math.max(lo, Math.min(hi, v));

/* ---------------------------
   1) Mocked device state
----------------------------*/
const LS = window.localStorage;
const LS_BUZZ = 'icm_buzzer';
const LS_FANM = 'icm_fan_mode';
const LS_FANP = 'icm_fan_pct';
const LS_SLP  = 'icm_sleep_to';

const MOCK = {
  mode: 'AUTO',                           // AUTO | MANUAL
  wifi: { mode: 'AP', ip: '192.168.4.1', rssi: -40, ch: 6 },
  power: { source: 'WALL', ok: true, detail: 'nominal' },
  health: { ok: true, faults: [] },       // push strings for faults
  buzzer_enabled: (LS.getItem(LS_BUZZ) ?? '1') === '1',
  time: { iso: isoNow(), tempC: 28.4 },   // RTC temperature shown on the UI
  uptime_ms: 0,

  // Cooling block
  cooling: {
    tempC: 36.8,
    speedPct: parseInt(LS.getItem(LS_FANP) || '42', 10),
    modeRequested: LS.getItem(LS_FANM) || 'AUTO',   // AUTO|ECO|NORMAL|FORCED|STOPPED
    modeApplied:   LS.getItem(LS_FANM) || 'AUTO',
  },

  // Sleep block
  sleep: {
    timeout_sec: parseInt(LS.getItem(LS_SLP) || '600', 10),
    secs_to_sleep: 540,
    last_activity_epoch: nowSec() - 60,
    next_wake_epoch: nowSec() + 2 * 3600,
    armed: true,
  },
};

/* Keep things moving so the UI feels alive */
setInterval(() => {
  MOCK.uptime_ms += 1000;
  MOCK.time.iso = isoNow();

  // Small temp drift
  const drift = (Math.random() - 0.5) * 0.06;
  MOCK.cooling.tempC = clamp(MOCK.cooling.tempC + drift, 30, 55);
  MOCK.time.tempC = clamp(MOCK.time.tempC + drift * 0.4, 20, 40);

  // Sleep countdown
  if (MOCK.sleep.armed && MOCK.sleep.secs_to_sleep > 0) {
    MOCK.sleep.secs_to_sleep -= 1;
  }
}, 1000);

/* ---------------------------
   2) Mock fetch router
----------------------------*/
async function mockFetch(url, opts = {}) {
  const method = (opts.method || 'GET').toUpperCase();
  const respond = (data, status = 200) => ({
    ok: status >= 200 && status < 300,
    status,
    json: async () => data
  });

  // Parse JSON body if present
  let body = {};
  try {
    if (opts.body) body = JSON.parse(opts.body);
  } catch { /* ignore */ }

  // --- Routing ---
  // System status
  if (url.startsWith('/api/system/status') && method === 'GET') {
    return respond({
      mode: MOCK.mode,
      wifi: MOCK.wifi,
      power: MOCK.power,
      health: MOCK.health,
      buzzer_enabled: MOCK.buzzer_enabled,
      time: MOCK.time,
      uptime_ms: MOCK.uptime_ms,
      cooling: MOCK.cooling,
      sleep: MOCK.sleep,
    });
  }

  // Buzzer toggle
  if (url.startsWith('/api/buzzer/set') && method === 'POST') {
    MOCK.buzzer_enabled = !!body.enabled;
    LS.setItem(LS_BUZZ, MOCK.buzzer_enabled ? '1' : '0');
    return respond({ ok: true, enabled: MOCK.buzzer_enabled });
  }

  // System reset / restart (no-ops here)
  if (url.startsWith('/api/system/reset') && method === 'POST') {
    return respond({ ok: true, note: 'reset flag set (mock)' });
  }
  if (url.startsWith('/api/system/restart') && method === 'POST') {
    return respond({ ok: true, note: 'restart scheduled (mock)' });
  }

  // Cooling status / mode / speed
  if (url.startsWith('/api/cooling/status') && method === 'GET') {
    return respond(MOCK.cooling);
  }
  if (url.startsWith('/api/cooling/mode') && method === 'POST') {
    const m = String(body.mode || 'AUTO').toUpperCase();
    const valid = ['AUTO','ECO','NORMAL','FORCED','STOPPED'];
    MOCK.cooling.modeRequested = valid.includes(m) ? m : 'AUTO';
    MOCK.cooling.modeApplied   = MOCK.cooling.modeRequested;
    LS.setItem(LS_FANM, MOCK.cooling.modeApplied);
    // If stopped, force speed 0 in mock
    if (MOCK.cooling.modeApplied === 'STOPPED') MOCK.cooling.speedPct = 0;
    return respond({ ok: true, mode: MOCK.cooling.modeApplied });
  }
  if (url.startsWith('/api/cooling/speed') && method === 'POST') {
    const pct = clamp(parseInt(body.pct ?? 0, 10) || 0, 0, 100);
    MOCK.cooling.speedPct = pct;
    // In mock we treat manual speed as FORCED mode
    if (MOCK.cooling.modeApplied !== 'STOPPED') {
      MOCK.cooling.modeApplied = 'FORCED';
      MOCK.cooling.modeRequested = 'FORCED';
      LS.setItem(LS_FANM, 'FORCED');
    }
    LS.setItem(LS_FANP, String(pct));
    return respond({ ok: true, pct });
  }

  // Sleep status / set timeout / reset / schedule
  if (url.startsWith('/api/sleep/status') && method === 'GET') {
    return respond(MOCK.sleep);
  }
  if (url.startsWith('/api/sleep/timeout') && method === 'POST') {
    const sec = Math.max(5, parseInt(body.timeout_sec ?? 0, 10) || 0);
    MOCK.sleep.timeout_sec = sec;
    MOCK.sleep.secs_to_sleep = sec;
    MOCK.sleep.last_activity_epoch = nowSec();
    MOCK.sleep.armed = true;
    LS.setItem(LS_SLP, String(sec));
    return respond({ ok: true, timeout_sec: sec });
  }
  if (url.startsWith('/api/sleep/reset') && method === 'POST') {
    MOCK.sleep.secs_to_sleep = MOCK.sleep.timeout_sec;
    MOCK.sleep.last_activity_epoch = nowSec();
    MOCK.sleep.armed = true;
    return respond({ ok: true });
  }
  if (url.startsWith('/api/sleep/schedule') && method === 'POST') {
    if (typeof body.after_sec === 'number') {
      const s = Math.max(1, body.after_sec|0);
      MOCK.sleep.secs_to_sleep = s;
      MOCK.sleep.armed = true;
      return respond({ ok: true, after_sec: s });
    }
    if (typeof body.wake_epoch === 'number') {
      MOCK.sleep.next_wake_epoch = body.wake_epoch|0;
      MOCK.sleep.armed = true;
      return respond({ ok: true, wake_epoch: MOCK.sleep.next_wake_epoch });
    }
    return respond({ ok: false, error: "Need 'after_sec' or 'wake_epoch'" }, 400);
  }

  // Default: pretend not found
  return respond({ error: 'mock: no route' }, 404);
}

/* Override window.fetch with our mock */
window.fetch = mockFetch;

/* ---------------------------
   3) Reuse the real UI binder
   (your original home.js logic)
----------------------------*/
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
