const subs = new Map();
export function on(evt, fn){ (subs.get(evt) || subs.set(evt,[]).get(evt)).push(fn); }
export function emit(evt, data){ (subs.get(evt)||[]).forEach(fn=>fn(data)); }
