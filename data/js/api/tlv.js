import { b64 as bytesToB64JS, bytes as b64ToBytesJS } from './http.js'; // not actually used; we'll implement local

export function macStrToBytes(str){
  const hex = str.replace(/[^0-9A-Fa-f]/g,'').toUpperCase();
  if(hex.length!==12) return new Uint8Array(6);
  const out=new Uint8Array(6);
  for(let i=0;i<6;i++) out[i]=parseInt(hex.slice(i*2,i*2+2),16);
  return out;
}
export function macBytesToStr(bytes){
  return Array.from(bytes).map(b=>b.toString(16).padStart(2,'0')).join(':').toUpperCase();
}
export function bytesToB64(bytes){ return btoa(String.fromCharCode(...bytes)); }
export function b64ToBytes(b64){ const bin=atob(b64); return Uint8Array.from(bin, c=>c.charCodeAt(0)); }

// TLV types
const T_DEVICE_TOKEN=0x01;
const T_ROLE=0x02;
const T_NEIGHBORS=0x03;
const T_ROLE_PARAMS=0x04;
const T_EMU_COUNT=0x05;

// topologyObj: { token, nodes:[{mac,role,idx?,label,y,pinned?}], edges:[{srcMac,dstMac}], roleParamsB64 }
export function encode(top){
  const parts=[];
  // token
  if(top.token){
    const t = new TextEncoder().encode(top.token);
    parts.push(Uint8Array.from([T_DEVICE_TOKEN,t.length]), t);
  }
  // nodes as per-role mini-TLVs
  for(const n of top.nodes){
    // ROLE (MAC + role + optional idx)
    const mac=macStrToBytes(n.mac);
    const role=new Uint8Array([n.role & 0xFF]);
    const idx = (n.idx!=null)? new Uint8Array([n.idx & 0xFF]) : new Uint8Array([]);
    const payload = new Uint8Array(mac.length + role.length + idx.length);
    payload.set(mac,0); payload.set(role,6); if(idx.length) payload.set(idx,7);
    parts.push(Uint8Array.from([T_ROLE,payload.length]), payload);
  }
  // neighbors from edges (unique macs)
  const set = new Set();
  for(const e of top.edges){ set.add(e.srcMac); set.add(e.dstMac); }
  if(set.size){
    const all = Array.from(set).map(macStrToBytes);
    const flat = new Uint8Array(all.length*6); all.forEach((b,i)=>flat.set(b,i*6));
    parts.push(Uint8Array.from([T_NEIGHBORS, flat.length]), flat);
  }
  // role params
  if(top.roleParamsB64){
    const raw=b64ToBytes(top.roleParamsB64);
    parts.push(Uint8Array.from([T_ROLE_PARAMS, raw.length]), raw);
  }
  // emu count: highest idx+1 per mac (optional heuristic)
  const emuMap=new Map();
  for(const n of top.nodes){
    if(n.idx!=null){
      const m=n.mac; const c=Math.max((emuMap.get(m)||0), n.idx+1);
      emuMap.set(m,c);
    }
  }
  for(const [m,c] of emuMap){
    parts.push(Uint8Array.from([T_EMU_COUNT,2]), Uint8Array.from([macStrToBytes(m)[5], c&0xFF]));
  }
  // concat
  let totalLen=0; for(const p of parts) totalLen+=p.length;
  const out=new Uint8Array(totalLen); let off=0; for(const p of parts){ out.set(p,off); off+=p.length; }
  return out;
}

export function decode(bytes){
  const out={ token:'', nodes:[], edges:[], roleParamsB64:'' };
  for(let i=0;i<bytes.length;){
    const t=bytes[i++]; const len=bytes[i++]; const view=bytes.slice(i,i+len); i+=len;
    if(t===T_DEVICE_TOKEN){ out.token=new TextDecoder().decode(view); }
    else if(t===T_ROLE){
      const mac=macBytesToStr(view.slice(0,6));
      const role=view[6]||0;
      const idx=view.length>7? view[7] : undefined;
      out.nodes.push({mac,role,idx, y:0});
    }else if(t===T_NEIGHBORS){
      // ignore for now (edges not encoded); neighbors used only for push planning
    }else if(t===T_ROLE_PARAMS){
      out.roleParamsB64=bytesToB64(view);
    }else if(t===T_EMU_COUNT){
      // optional hint, ignore
    }
  }
  return out;
}
