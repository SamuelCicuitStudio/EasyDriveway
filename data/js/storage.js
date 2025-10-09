// storage.js — Save/Load helpers
export function downloadJSON(filename, jsonString){
  const blob = new Blob([jsonString], {type:'application/json'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  setTimeout(()=>{
    a.remove();
    URL.revokeObjectURL(url);
  }, 0);
}

export function readFileAsText(file){
  return new Promise((resolve, reject)=>{
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result || ""));
    reader.onerror = reject;
    reader.readAsText(file);
  });
}
