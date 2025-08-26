// settings.mock.js - drop-in fetch adapter to test Settings page without firmware
(() => {
  const realFetch = window.fetch.bind(window);
  const sleep = (ms)=>new Promise(r=>setTimeout(r,ms));
  const nowIso = ()=> new Date().toISOString().replace('T',' ').replace('Z','');

  const state = {
    mode: 'AUTO',
    wifi: {mode:'AP', ip:'192.168.4.1', ch:6},
    uptime_ms: 1234567,
    time: { iso: nowIso() },
    buzzer_enabled: true,
    cfg: {
      dev_id: 'ICM01',
      host_name: 'ICM-1A2B3C',
      friendly_name: 'ICM_12X34W',
      ble_name: 'ICM_12X34W',
      ble_password: 123457,
      pass_pin: '12345678',
      fw_ver: '1.2.3',
      sw_ver: '2.3.4',
      hw_ver: 'A2',
      build: '2025-08-25T10:45Z #deadbeef'
    }
  };

  window.fetch = async (url, opts={}) => {
    // Simulate latency
    await sleep(120);

    if (url.startsWith('/api/system/status') && (!opts.method || opts.method==='GET')) {
      return new Response(JSON.stringify({
        mode: state.mode, wifi: state.wifi, uptime_ms: state.uptime_ms, time: state.time,
        buzzer_enabled: state.buzzer_enabled
      }), {status:200, headers:{'Content-Type':'application/json'}});
    }

    if (url.startsWith('/api/system/mode') && opts.method==='POST') {
      const body = JSON.parse(opts.body||'{}');
      state.mode = (body.manual===true) ? 'MANUAL' : 'AUTO';
      return new Response(JSON.stringify({ok:true, mode: state.mode}), {status:200, headers:{'Content-Type':'application/json'}});
    }

    if (url.startsWith('/api/buzzer/set') && opts.method==='POST') {
      const body = JSON.parse(opts.body||'{}');
      state.buzzer_enabled = !!body.enabled;
      return new Response(JSON.stringify({ok:true}), {status:200, headers:{'Content-Type':'application/json'}});
    }

    if (url.startsWith('/api/config/load') && (!opts.method || opts.method==='GET')) {
      return new Response(JSON.stringify(state.cfg), {status:200, headers:{'Content-Type':'application/json'}});
    }

    if (url.startsWith('/api/config/save') && opts.method==='POST') {
      const body = JSON.parse(opts.body||'{}');
      Object.assign(state.cfg, body);
      return new Response(JSON.stringify({ok:true}), {status:200, headers:{'Content-Type':'application/json'}});
    }

    if (url.startsWith('/api/config/factory_reset') && opts.method==='POST') {
      return new Response(JSON.stringify({ok:true}), {status:200, headers:{'Content-Type':'application/json'}});
    }

    if (url.startsWith('/api/system/restart') && opts.method==='POST') {
      return new Response(JSON.stringify({ok:true}), {status:200, headers:{'Content-Type':'application/json'}});
    }

    // fallback to real fetch
    return realFetch(url, opts);
  };
})();