export const state = {
  device:null,
  peersByMac:new Map(),
  draft:{
    nodes:[],
    edges:[],
    token:'',
    roleParamsB64:'',
  },
  selection:{ mac:null, idx:null },
  mode:'auto',
  drag:null,
  history:[],
};
export function resetDraft(){ state.draft={nodes:[],edges:[],token:'',roleParamsB64:''}; }
