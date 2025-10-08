export function macNorm(str){
  return (str||'').trim().toUpperCase().replace(/[^0-9A-F]/g,'')
    .match(/.{1,2}/g)?.join(':') || '';
}
export function timeAgo(ms){
  const s=Math.floor(ms/1000);
  if(s<60) return s+'s';
  const m=Math.floor(s/60); if(m<60) return m+'m';
  const h=Math.floor(m/60); if(h<24) return h+'h';
  return Math.floor(h/24)+'d';
}
export function hex(bytes){ return Array.from(bytes, b=>b.toString(16).padStart(2,'0')).join(''); }
export function b64(bytes){
  return btoa(String.fromCharCode(...bytes));
}
export function bytes(b64str){
  const bin=atob(b64str); const out=new Uint8Array(bin.length);
  for(let i=0;i<bin.length;i++) out[i]=bin.charCodeAt(i);
  return out;
}
