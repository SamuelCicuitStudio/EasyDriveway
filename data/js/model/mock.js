export const MOCK_DEVICE = {
  name:'ICM-EDW-S3', mac:'24:6F:28:AA:BB:CC', fw:'1.0.0', chip:'ESP32-S3', uptime:123456
};
export const MOCK_PEERS = [
  { mac:'24:6F:28:00:00:01', role:2, rssi:-48, lastSeenMs:5000, label:'PMS-01' },
  { mac:'24:6F:28:00:00:02', role:3, rssi:-60, lastSeenMs:12000, label:'SEN-ENV-1' },
  { mac:'24:6F:28:00:00:03', role:3, rssi:-72, lastSeenMs:30000, label:'SEN-LUX-1' },
  { mac:'24:6F:28:00:00:04', role:4, rssi:-40, lastSeenMs:4000, label:'RELAY-A' },
  { mac:'24:6F:28:00:00:05', role:6, rssi:-55, lastSeenMs:6000, label:'REL-EMU', emuCount:3 },
  { mac:'24:6F:28:00:00:06', role:5, rssi:-67, lastSeenMs:9000, label:'SEN-EMU', emuCount:2 }
];
export const MOCK_TOPO = {
  token:'1234567890ABCDEF1234567890ABCDEF',
  nodes:[
    {mac:'24:6F:28:00:00:02', role:3, y:100, label:'SEN-ENV-1'},
    {mac:'24:6F:28:00:00:04', role:4, y:260, label:'RELAY-A'}
  ],
  edges:[{srcMac:'24:6F:28:00:00:02', dstMac:'24:6F:28:00:00:04'}],
  roleParamsB64:''
};
