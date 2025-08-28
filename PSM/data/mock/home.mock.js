/* ICM Home — Full Mock (routes + colorful binder)
 * Use this as a standalone instead of js/home.js when testing w/o firmware.
 * In /home.html:
 *   <script src="js/home.mock.js"></script>
 *
 * Tips:
 *   - Add ?unauth=1 to the URL to test 401 redirect to login.
 *   - State persists in localStorage for buzzer, fan mode/speed, and sleep timeout.
 */

/* ---------------------------
   0) Helpers
----------------------------*/
const nowSec = () => Math.floor(Date.now() / 1000);
const isoNow = () => new Date().toISOString();
const clamp  = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
const q      = new URLSearchParams(location.search);

/* ---------------------------
   1) Mocked device state
----------------------------*/
const LS = window.localStorage;
const LS_BUZZ = 'icm_buzzer';
const LS_FANM = 'icm_fan_mode';
const LS_FANP = 'icm_fan_pct';
const LS_SLP  = 'icm_sleep_to';

const MOCK = {
  // System
  mode: 'AUTO',                           // AUTO | MANUAL
  wifi: { mode: 'AP', ip: '192.168.4.1', rssi: -40, ch: 6 },
  power: { source: 'WALL', ok: true, detail: 'nominal' },
  health: { ok: true, faults: [] },
  buzzer_enabled: (LS.getItem(LS_BUZZ) ?? '1') === '1',
  time: { iso: isoNow(), tempC: 28.4, lost_power: false },
  uptime_ms: 0,

  // Cooling
  cooling: {
    tempC: 36.8,
    speedPct: parseInt(LS.getItem(LS_FANP) || '42', 10),
    modeRequested: LS.getItem(LS_FANM) || 'AUTO',   // AUTO|ECO|NORMAL|FORCED|STOPPED
    modeApplied:   LS.getItem(LS_FANM) || 'AUTO',
  },

  // Sleep
  sleep: {
    timeout_sec: parseInt(LS.getItem(LS_SLP) || '600', 10),
    secs_to_sleep: 540,
    last_activity_epoch: nowSec() - 60,
    next_wake_epoch: nowSec() + 2 * 3600,
    armed: true,
  },
};

/* Animate values so the UI stays lively */
setInterval(() => {
  MOCK.uptime_ms += 1000;
  MOCK.time.iso = isoNow();

  // Minute pulse: flip between AP/STA occasionally (visual test)
  if ((MOCK.uptime_ms / 1000) % 60 === 0) {
    const toSTA = Math.random() > 0.5;
    MOCK.wifi.mode = toSTA ? 'STA' : 'AP';
    MOCK.wifi.ip   = toSTA ? '192.168.1.97' : '192.168.4.1';
    MOCK.wifi.rssi = toSTA ? -52 : -40;
    MOCK.wifi.ch   = toSTA ? 11 : 6;
  }

  // Small temp drift
  const drift = (Math.random() - 0.5) * 0.08;
  MOCK.cooling.tempC = clamp(MOCK.cooling.tempC + drift, 30, 58);
  MOCK.time.tempC    = clamp(MOCK.time.tempC + drift * 0.4, 20, 42);

  // Random transient fault (clears itself)
  if (Math.random() < 0.03) {
    MOCK.health.faults.push('Transient sensor glitch');
  }
  if (MOCK.health.faults.length && Math.random() < 0.3) {
    MOCK.health.faults.shift();
  }
  MOCK.health.ok = MOCK.health.faults.length === 0;

  // Sleep countdown
  if (MOCK.sleep.armed && MOCK.sleep.secs_to_sleep > 0) {
    MOCK.sleep.secs_to_sleep -= 1;
  }
}, 1000);

/* ---------------------------
   2) Full mock fetch router
----------------------------*/
(function installMockFetch(){
  const realFetch = window.fetch ? window.fetch.bind(window) : null;
  const UNAUTH = q.get('unauth') === '1';

  async function mockFetch(url, opts = {}) {
    const method = (opts.method || 'GET').toUpperCase();
    const respond = (data, status = 200, headers = {'Content-Type':'application/json'}) => ({
      ok: status >= 200 && status < 300,
      status,
      headers: new Headers(headers),
      json: async () => data,
      text: async () => (typeof data === 'string' ? data : JSON.stringify(data))
    });

    // Parse JSON body if present
    let body = {};
    try {
      if (opts.body) body = JSON.parse(opts.body);
    } catch { /* ignore */ }

    // 401 test hook
    if (UNAUTH && url.startsWith('/api/system/status')) {
      return respond({err:'auth'}, 401);
    }

    /* ===== Auth (parity) ===== */
    if (url.startsWith('/api/auth/status') && method === 'GET') {
      return respond({ ok: !UNAUTH });
    }
    if (url === '/logout' && method === 'POST') {
      // mimic server logout & redirect
      LS.removeItem(LS_BUZZ); LS.removeItem(LS_FANM); LS.removeItem(LS_FANP); LS.removeItem(LS_SLP);
      setTimeout(()=>{ location.href = '/login.html'; }, 50);
      return respond({ ok: true });
    }

    /* ===== Home APIs ===== */

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

    // System reset / restart
    if (url.startsWith('/api/system/reset') && method === 'POST') {
      // pretend to set a factory reset flag
      return respond({ ok: true, note: 'reset flag set (mock)' });
    }
    if (url.startsWith('/api/system/restart') && method === 'POST') {
      // echo then "reboot": send you to login
      setTimeout(()=>{ location.href = '/login.html'; }, 800);
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
      // manual speed implies FORCED (unless stopped)
      if (MOCK.cooling.modeApplied !== 'STOPPED') {
        MOCK.cooling.modeApplied = 'FORCED';
        MOCK.cooling.modeRequested = 'FORCED';
        LS.setItem(LS_FANM, 'FORCED');
      }
      LS.setItem(LS_FANP, String(pct));
      return respond({ ok: true, pct });
    }

    // Sleep status / timeout / reset / schedule
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

    /* ===== Default passthrough for static files ===== */
    return realFetch ? realFetch(url, opts) : respond({error:'mock: no route'}, 404);
  }

  // Install override
  window.fetch = mockFetch;
})();

/* ---------------------------
   3) Colorful Home binder (same as home.js)
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

  // Logout (works in mock too)
  const logoutBtn = $('logoutBtn');

  // Toast
  const toast = $('toast');
  const toastMsg = (t)=>{ toast.textContent=t||'Saved'; toast.hidden=false; setTimeout(()=>toast.hidden=true,1600); };

  // Helpers
  const fmtUptime = (ms) => {
    const s = Math.floor(ms/1000), d=Math.floor(s/86400), h=Math.floor(s%86400/3600), m=Math.floor(s%3600/60), ss=s%60;
    return `${d}d ${h}h ${m}m ${ss}s`;
  };
  const fmtEpoch = (e)=> e? new Date(e*1000).toISOString().replace('T',' ').replace('Z',''): '—';

  // Color helpers (match /home.css)
  const BADGE_STATES = ['badge-ok','badge-warn','badge-danger','badge-info'];
  const TEXT_STATES  = ['text-ok','text-warn','text-danger','text-info','text-muted'];
  const setBadge = (el, state)=>{ if(!el)return; el.classList.remove(...BADGE_STATES); if(state) el.classList.add(`badge-${state}`); };
  const setText  = (el, text, state)=>{ if(!el)return; el.textContent=(text ?? '—'); el.classList.remove(...TEXT_STATES); if(state) el.classList.add(`text-${state}`); el.classList.add('value'); };

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

  // -------- Buzzer --------
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

  // -------- Sleep --------
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

  // -------- Logout (server-side redirect in mock) --------
  const logoutBtnEl = document.getElementById('logoutBtn');
  if (logoutBtnEl) {
    logoutBtnEl.addEventListener('click', () => {
      const f = document.createElement('form');
      f.method = 'POST'; f.action = '/logout';
      document.body.appendChild(f); f.submit();
    });
  }

  // Start
  refresh();
  setInterval(refresh, 3000);
})();
