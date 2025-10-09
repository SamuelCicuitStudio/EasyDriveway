// utils.js — small helpers & a lightweight event bus (ES module)
export const bus = (() => {
  const map = new Map();
  return {
    on: (type, fn) => { if (!map.has(type)) map.set(type, new Set()); map.get(type).add(fn); },
    off: (type, fn) => { if (map.has(type)) map.get(type).delete(fn); },
    emit: (type, detail) => { if (map.has(type)) for (const fn of map.get(type)) try { fn(detail); } catch (e) { console.error(e); } }
  };
})();

export const uid = (prefix = "id") => `${prefix}_${Math.random().toString(36).slice(2, 9)}`;

export const clamp = (v, min, max) => Math.min(max, Math.max(min, v));

export const el = (tag, props={}, ...children) => {
  const node = Object.assign(document.createElement(tag), props);
  for (const c of children) node.append(c);
  return node;
};

// Simple throttle for pointermove
export function throttle(fn, ms=16) {
  let last = 0;
  let pending;
  return (...args) => {
    const now = performance.now();
    if (now - last >= ms) {
      last = now;
      fn(...args);
    } else {
      pending && cancelAnimationFrame(pending);
      pending = requestAnimationFrame(() => {
        last = performance.now();
        fn(...args);
      });
    }
  };
}
