// Deck plan of the ISV Astraea. One deck; +X is starboard, -Z is the bow.
// Rooms are axis-aligned rectangles that share walls; doors sit on shared walls.

export const WALL_T = 0.25;

// Index 0 is reserved (shader default for geometry without a room).
export const ROOMS = [
  { id: 'none', name: '', x0: 0, x1: 0, z0: 0, z1: 0, h: 0, color: 0xffffff },
  { id: 'cryo', name: 'Cryo Bay', x0: -16, x1: -2, z0: -4, z1: 8, h: 3.8, color: 0x66ccff },
  { id: 'spineB', name: 'Mid Corridor', x0: -2, x1: 2, z0: -6, z1: 10, h: 3.4, color: 0x4fd8e8 },
  { id: 'hub', name: 'Central Hub', x0: -8, x1: 8, z0: -22, z1: -6, h: 6.5, color: 0x58e0d0 },
  { id: 'spineA', name: 'Fore Corridor', x0: -2, x1: 2, z0: -36, z1: -22, h: 3.4, color: 0x4fd8e8 },
  { id: 'aicore', name: 'AI Core', x0: -11, x1: -2, z0: -34, z1: -25, h: 4.4, color: 0x6f8cff },
  { id: 'quarters', name: 'Crew Quarters', x0: 2, x1: 13, z0: -35, z1: -24, h: 3.4, color: 0xffb060 },
  { id: 'medbay', name: 'Med Bay', x0: -21, x1: -8, z0: -20, z1: -8, h: 3.8, color: 0x7dffc0 },
  { id: 'lab', name: 'Science Lab', x0: 8, x1: 21, z0: -20, z1: -8, h: 3.8, color: 0xb388ff },
  { id: 'hydro', name: 'Hydroponics', x0: 2, x1: 19, z0: -5, z1: 9, h: 4.8, color: 0x8dff6a },
  { id: 'engineering', name: 'Engineering', x0: -10, x1: 10, z0: 10, z1: 30, h: 9, color: 0xffa040 },
  { id: 'cargo', name: 'Cargo Bay', x0: -12, x1: 12, z0: 30, z1: 50, h: 7.5, color: 0xffd24a },
  { id: 'airlock', name: 'Airlock', x0: 12, x1: 18, z0: 37, z1: 43, h: 3.4, color: 0xff7a4a },
  { id: 'bridge', name: 'Bridge', x0: -8, x1: 8, z0: -50, z1: -36, h: 5, color: 0x58b8ff },
  // The dorsal hull, reached through the airlock. Lives far from the deck plan.
  { id: 'hull', name: 'Dorsal Hull', x0: 270, x1: 330, z0: -80, z1: 30, h: 0, color: 0xffffff, exterior: true },
];

export const R = {};
ROOMS.forEach((r, i) => {
  r.index = i;
  r.cx = (r.x0 + r.x1) / 2;
  r.cz = (r.z0 + r.z1) / 2;
  r.w = r.x1 - r.x0;
  r.d = r.z1 - r.z0;
  R[r.id] = r;
});

export const HULL_ORIGIN = { x: 300, y: 0, z: 0 };

// axis 'x': the door is in a wall of constant X (you walk through along X).
// axis 'z': the door is in a wall of constant Z.
export const DOORS = [
  { id: 'd_cryo', a: 'cryo', b: 'spineB', x: -2, z: 1, axis: 'x', w: 2.0, h: 2.5 },
  { id: 'd_hydro', a: 'spineB', b: 'hydro', x: 2, z: 5, axis: 'x', w: 2.0, h: 2.5 },
  { id: 'd_hub_s', a: 'hub', b: 'spineB', x: 0, z: -6, axis: 'z', w: 2.4, h: 2.7 },
  { id: 'd_med', a: 'medbay', b: 'hub', x: -8, z: -14, axis: 'x', w: 2.0, h: 2.5 },
  { id: 'd_lab', a: 'hub', b: 'lab', x: 8, z: -14, axis: 'x', w: 2.0, h: 2.5 },
  { id: 'd_hub_n', a: 'spineA', b: 'hub', x: 0, z: -22, axis: 'z', w: 2.4, h: 2.7 },
  { id: 'd_ai', a: 'aicore', b: 'spineA', x: -2, z: -31, axis: 'x', w: 2.0, h: 2.5 },
  { id: 'd_quarters', a: 'spineA', b: 'quarters', x: 2, z: -27, axis: 'x', w: 2.0, h: 2.5 },
  { id: 'd_bridge', a: 'bridge', b: 'spineA', x: 0, z: -36, axis: 'z', w: 2.6, h: 2.8 },
  { id: 'd_eng', a: 'spineB', b: 'engineering', x: 0, z: 10, axis: 'z', w: 2.6, h: 2.8 },
  { id: 'd_cargo', a: 'engineering', b: 'cargo', x: 0, z: 30, axis: 'z', w: 3.2, h: 3.2 },
  { id: 'd_airlock', a: 'cargo', b: 'airlock', x: 12, z: 40, axis: 'x', w: 2.0, h: 2.5 },
];

export function roomAt(x, z) {
  for (let i = 1; i < ROOMS.length; i++) {
    const r = ROOMS[i];
    if (x >= r.x0 && x <= r.x1 && z >= r.z0 && z <= r.z1) return r;
  }
  return null;
}

// Doors on one side of a room. side: 'N' (z0), 'S' (z1), 'W' (x0), 'E' (x1).
export function doorsOnSide(room, side) {
  const out = [];
  for (const d of DOORS) {
    if (d.a !== room.id && d.b !== room.id) continue;
    if (d.axis === 'x') {
      if (side === 'W' && Math.abs(d.x - room.x0) < 0.01) out.push(d);
      if (side === 'E' && Math.abs(d.x - room.x1) < 0.01) out.push(d);
    } else {
      if (side === 'N' && Math.abs(d.z - room.z0) < 0.01) out.push(d);
      if (side === 'S' && Math.abs(d.z - room.z1) < 0.01) out.push(d);
    }
  }
  return out;
}

// Breadth-first route through the door graph. Returns the list of doors to pass.
export function routeDoors(fromId, toId, isOpenable = () => true) {
  if (fromId === toId) return [];
  const prev = new Map([[fromId, null]]);
  const queue = [fromId];
  while (queue.length) {
    const cur = queue.shift();
    if (cur === toId) break;
    for (const d of DOORS) {
      if (d.a !== cur && d.b !== cur) continue;
      if (!isOpenable(d)) continue;
      const other = d.a === cur ? d.b : d.a;
      if (prev.has(other)) continue;
      prev.set(other, { room: cur, door: d });
      queue.push(other);
    }
  }
  if (!prev.has(toId)) return null;
  const doors = [];
  let cur = toId;
  while (prev.get(cur)) {
    const p = prev.get(cur);
    doors.unshift(p.door);
    cur = p.room;
  }
  return doors;
}
