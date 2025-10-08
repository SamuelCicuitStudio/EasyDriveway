import { emit } from '../utils/bus.js';
export function connectWS(){
  const ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onopen=()=>emit('ws:status','up');
  ws.onclose=()=>emit('ws:status','down');
  ws.onmessage=(e)=>{ try{ const m=JSON.parse(e.data); emit(`ws:${m.t}`, m); }catch{} };
  return ws;
}
