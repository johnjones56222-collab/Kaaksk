// Minimal synchronous event bus.
const map = new Map();

export const bus = {
  on(type, fn) {
    if (!map.has(type)) map.set(type, new Set());
    map.get(type).add(fn);
    return () => map.get(type)?.delete(fn);
  },
  emit(type, a, b, c) {
    const set = map.get(type);
    if (!set) return;
    for (const fn of [...set]) {
      try {
        fn(a, b, c);
      } catch (err) {
        console.error(`[bus] handler for "${type}" failed`, err);
      }
    }
  },
};
