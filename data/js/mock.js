/* mock.js — Fake data for local testing (rewritten)
 * - Same shape used by app.js / api.js
 * - Add a couple more peers so you can test long chains
 */
window.MOCK = {
  wifi: { mode: 'AP', ip: '192.168.4.1', ch: 6 },

  peers: [
    { id: 'A1', type: 'relay',  label: 'Gate Left',     mac: '24:6F:28:AA:AA:A1' },
    { id: 'A2', type: 'sensor', label: 'Entrance PIR',  mac: '24:6F:28:BB:BB:B2' },
    { id: 'A3', type: 'relay',  label: 'Gate Right',    mac: '24:6F:28:CC:CC:C3' },
    { id: 'A4', type: 'relay',  label: 'Path 1',        mac: '24:6F:28:DD:DD:D4' },
    { id: 'A5', type: 'relay',  label: 'Path 2',        mac: '24:6F:28:EE:EE:E5' },
    { id: 'S6', type: 'sensor', label: 'Mid PIR',       mac: '24:6F:28:FF:FF:F6' },
    { id: 'A7', type: 'relay',  label: 'Porch Light',   mac: '24:6F:28:11:11:17' }
  ],

  topology: {
    links: [
      { prev: { id: 'A1' }, next: { id: 'A2' } },
      { prev: { id: 'A2' }, next: { id: 'A3' } },
      { prev: { id: 'A3' }, next: { id: 'A4' } },
      { prev: { id: 'A4' }, next: { id: 'A5' } },
      { prev: { id: 'A5' }, next: { id: 'S6' } },
      { prev: { id: 'S6' }, next: { id: 'A7' } }
    ]
  }
};
