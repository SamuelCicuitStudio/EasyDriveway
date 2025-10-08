import { jget, jpost, jdel } from './http.js';

export const getDevice = () => jget('/api/device');
export const getPeers  = () => jget('/api/peers');
export const addPeer   = (mac,encrypt=false,lmk=null)=> jpost('/api/peers',{mac,encrypt,lmk});
export const removePeer= (mac)=> jdel(`/api/peers/${encodeURIComponent(mac)}`);

export const getTopology = () => jget('/api/topology');
export const importTopology = (tlvB64)=> jpost('/api/topology/import',{tlvB64});
export const pushTopology = (mac,tlvB64)=> jpost('/api/topology/push',{mac,tlvB64});
export const pushTopologyAll = (tlvB64)=> jpost('/api/topology/pushAll',{tlvB64});

export const pair   = (mac)=> jpost('/api/pair',{mac});
export const unpair = (mac)=> jpost('/api/unpair',{mac});

export const cmdBuzz = (mac)=> jpost('/api/cmd/buzz',{mac});
export const cmdLed  = (mac)=> jpost('/api/cmd/led',{mac});
export const cmdFan  = (mac,mode)=> jpost('/api/cmd/fan',{mac,mode});
export const cmdSetTime = (mac,unix)=> jpost('/api/cmd/time',{mac,unix});
export const cmdResetSoft = (mac)=> jpost('/api/cmd/resetSoft',{mac});
export const cmdRestartHard= (mac)=> jpost('/api/cmd/restartHard',{mac});
export const cmdSilence = (mac)=> jpost('/api/cmd/silence',{mac});

export const getTemp = (mac)=> jget(`/api/telemetry/temp?mac=${mac}`);
export const getTime = (mac)=> jget(`/api/telemetry/time?mac=${mac}`);
export const getFan  = (mac)=> jget(`/api/telemetry/fan?mac=${mac}`);
export const getLogs = (mac,off,max)=> jpost(`/api/telemetry/logs`,{mac,off,max});
export const getFaults = (mac)=> jget(`/api/telemetry/faults?mac=${mac}`);
export const getTopoOf = (mac)=> jget(`/api/topology/of?mac=${mac}`);

export const relayStates = (mac)=> jget(`/api/relay/states?mac=${mac}`);
export const relaySet = (mac,ch,on,ms=0)=> jpost(`/api/relay/set`,{mac,ch,on,ms});

export const tfluna = (mac)=> jget(`/api/sensor/tfluna?mac=${mac}`);
export const env    = (mac)=> jget(`/api/sensor/env?mac=${mac}`);
export const lux    = (mac)=> jget(`/api/sensor/lux?mac=${mac}`);
export const setThresholds = (mac,bytes)=> jpost(`/api/sensor/thresholds`,{mac,bytesB64:btoa(String.fromCharCode(...bytes))});

export const pmsVI = (mac)=> jget(`/api/pms/vi?mac=${mac}`);
export const pmsSource = (mac)=> jget(`/api/pms/source?mac=${mac}`);
export const pmsGroups = (mac,bytes)=> jpost(`/api/pms/groups`,{mac,bytesB64:btoa(String.fromCharCode(...bytes))});
