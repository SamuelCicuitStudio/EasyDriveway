export async function jget(url){
  const r=await fetch(url); const j=await r.json();
  if(!j.ok) throw new Error(j.err||url); return j.data;
}
export async function jpost(url, body){
  const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  const j=await r.json(); if(!j.ok) throw new Error(j.err||url); return j.data;
}
export async function jdel(url){
  const r=await fetch(url,{method:'DELETE'});
  const j=await r.json(); if(!j.ok) throw new Error(j.err||url); return j.data;
}
