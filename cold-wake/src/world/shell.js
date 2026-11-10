// Generic room shell: deck, walls with door and window openings, ceiling with
// skylights, plus the standard architectural dressing (ribs, coves, skirting,
// ceiling fixtures, door frames and destination signs).
import { G } from '../engine/builder.js';
import { WALL_T, R, doorsOnSide } from './layout.js';

const T = WALL_T;
export const SIDES = ['N', 'S', 'W', 'E'];

// Geometry helpers for one wall of a room.
export function wallInfo(room, side) {
  switch (side) {
    case 'N':
      return { side, along: 'x', a: room.x0, b: room.x1, fixed: room.z0, inward: 1, ry: 0 };
    case 'S':
      return { side, along: 'x', a: room.x0, b: room.x1, fixed: room.z1, inward: -1, ry: Math.PI };
    case 'W':
      return { side, along: 'z', a: room.z0, b: room.z1, fixed: room.x0, inward: 1, ry: Math.PI / 2 };
    default:
      return { side, along: 'z', a: room.z0, b: room.z1, fixed: room.x1, inward: -1, ry: -Math.PI / 2 };
  }
}

// World position of a point on a wall: u along the wall (world coordinate),
// off = distance from the inner wall face into the room.
export function wallPoint(room, side, u, off = 0) {
  const w = wallInfo(room, side);
  const f = w.fixed + w.inward * (T + off);
  return w.along === 'x' ? { x: u, z: f, ry: w.ry } : { x: f, z: u, ry: w.ry };
}

// Build something against a wall. Inside fn: +Z points into the room, z = 0 is
// the wall face, y = 0 is the deck.
export function onWall(b, room, side, u, fn, off = 0, y = 0) {
  const p = wallPoint(room, side, u, off);
  b.at(p.x, y, p.z, p.ry, fn);
}

function wallBox(b, mat, w, u, y, off, lu, h, ld, o) {
  // off = distance of the box centre from the outer wall plane, towards the room
  const f = w.fixed + w.inward * off;
  if (w.along === 'x') b.box(mat, u, y, f, lu, h, ld, o);
  else b.box(mat, f, y, u, ld, h, lu, o);
}

function wallGlow(b, g, w, u, y, off, lu, h, ld) {
  const f = w.fixed + w.inward * off;
  if (w.along === 'x') b.gbox(g, u, y, f, lu, h, ld);
  else b.gbox(g, f, y, u, ld, h, lu);
}

// Rectangles covering [x0,x1]x[z0,z1] minus a set of holes.
function rectsMinusHoles(x0, x1, z0, z1, holes) {
  const cuts = new Set([z0, z1]);
  for (const h of holes) {
    cuts.add(Math.max(z0, Math.min(z1, h.z0)));
    cuts.add(Math.max(z0, Math.min(z1, h.z1)));
  }
  const zs = [...cuts].sort((p, q) => p - q);
  const out = [];
  for (let i = 0; i < zs.length - 1; i++) {
    const za = zs[i];
    const zb = zs[i + 1];
    if (zb - za < 1e-4) continue;
    const mid = (za + zb) / 2;
    const spans = holes
      .filter((h) => h.z0 < mid && h.z1 > mid)
      .map((h) => [h.x0, h.x1])
      .sort((p, q) => p[0] - q[0]);
    let cur = x0;
    for (const [a, c] of spans) {
      if (a > cur) out.push({ x0: cur, x1: a, z0: za, z1: zb });
      cur = Math.max(cur, c);
    }
    if (cur < x1) out.push({ x0: cur, x1, z0: za, z1: zb });
  }
  return out;
}

export function buildShell(b, room, opts = {}) {
  const o = {
    wall: 'wall',
    floor: 'floor',
    ceil: 'ceil',
    accent: room.color,
    accentI: 2.4,
    windows: [],
    skylights: [],
    ribs: 3.4,
    cove: true,
    skirt: true,
    skirtGlow: true,
    ceilLights: { spacing: 4.2, intensity: 0.95, color: 0xdfeeff },
    beams: 4.2,
    lightRange: null,
    emergency: true,
    ...opts,
  };
  const { x0, x1, z0, z1, h } = room;
  const cx = room.cx;
  const cz = room.cz;
  const accent = o.accent;
  const info = { walls: {}, clear: {} };

  // ---- deck ---------------------------------------------------------------
  b.box(o.floor, cx, -0.1, cz, room.w, 0.2, room.d, { uv: 'world' });

  // ---- ceiling ------------------------------------------------------------
  if (o.ceil) {
    const holes = o.skylights;
    for (const r of rectsMinusHoles(x0, x1, z0, z1, holes)) {
      b.box(o.ceil, (r.x0 + r.x1) / 2, h + 0.15, (r.z0 + r.z1) / 2, r.x1 - r.x0, 0.3, r.z1 - r.z0, { uv: 'world' });
    }
    for (const s of holes) {
      const sx = (s.x0 + s.x1) / 2;
      const sz = (s.z0 + s.z1) / 2;
      const sw = s.x1 - s.x0;
      const sd = s.z1 - s.z0;
      // frame and glazing
      b.box('trimDark', sx, h - 0.06, s.z0 + 0.06, sw, 0.14, 0.12);
      b.box('trimDark', sx, h - 0.06, s.z1 - 0.06, sw, 0.14, 0.12);
      b.box('trimDark', s.x0 + 0.06, h - 0.06, sz, 0.12, 0.14, sd);
      b.box('trimDark', s.x1 - 0.06, h - 0.06, sz, 0.12, 0.14, sd);
      const bars = Math.max(1, Math.round(sw / 2.2));
      for (let i = 1; i < bars; i++) b.box('trimDark', s.x0 + (sw / bars) * i, h + 0.05, sz, 0.08, 0.1, sd);
      b.plane('glass', sx, h + 0.12, sz, sw, sd, { rx: -Math.PI / 2 });
    }
  }

  // ---- walls --------------------------------------------------------------
  for (const side of SIDES) {
    const w = wallInfo(room, side);
    const doors = doorsOnSide(room, side);
    const openings = [];
    for (const d of doors) {
      const c = w.along === 'x' ? d.x : d.z;
      openings.push({ a: c - d.w / 2, b: c + d.w / 2, y0: 0, y1: d.h, door: d });
    }
    for (const win of o.windows) {
      if (win.side !== side) continue;
      openings.push({ a: win.a, b: win.b, y0: win.y0 ?? 1.0, y1: win.y1 ?? h - 0.7, window: win });
    }
    openings.sort((p, q) => p.a - q.a);
    info.walls[side] = { ...w, openings };
    const wallMat = (o.wallBy && o.wallBy[side]) || o.wall;

    // solid stretches between openings, and collision
    let cur = w.a;
    let colStart = w.a;
    const stretches = [];
    for (const op of openings) {
      if (op.a > cur + 1e-4) stretches.push([cur, op.a]);
      // sill and header
      if (op.y0 > 0.01) wallBox(b, wallMat, w, (op.a + op.b) / 2, op.y0 / 2, T / 2, op.b - op.a, op.y0, T, { uv: 'world' });
      if (op.y1 < h - 0.01) wallBox(b, wallMat, w, (op.a + op.b) / 2, (op.y1 + h) / 2, T / 2, op.b - op.a, h - op.y1, T, { uv: 'world' });
      if (op.door) {
        if (op.a > colStart) addWallCollider(b, w, colStart, op.a);
        colStart = op.b;
      }
      cur = op.b;
    }
    if (cur < w.b - 1e-4) stretches.push([cur, w.b]);
    if (w.b > colStart) addWallCollider(b, w, colStart, w.b);
    for (const [s, e] of stretches) wallBox(b, wallMat, w, (s + e) / 2, h / 2, T / 2, e - s, h, T, { uv: 'world' });
    info.clear[side] = stretches;

    // ---- windows: frame, mullions, glazing ----
    for (const op of openings) {
      if (!op.window) continue;
      const len = op.b - op.a;
      const mid = (op.a + op.b) / 2;
      const wy = (op.y0 + op.y1) / 2;
      const wh = op.y1 - op.y0;
      const fr = 0.14;
      wallBox(b, 'trimDark', w, mid, op.y0 + fr / 2 - 0.02, T / 2 + 0.03, len + 0.1, fr, T + 0.14);
      wallBox(b, 'trimDark', w, mid, op.y1 - fr / 2 + 0.02, T / 2 + 0.03, len + 0.1, fr, T + 0.14);
      wallBox(b, 'trimDark', w, op.a + fr / 2 - 0.02, wy, T / 2 + 0.03, fr, wh, T + 0.14);
      wallBox(b, 'trimDark', w, op.b - fr / 2 + 0.02, wy, T / 2 + 0.03, fr, wh, T + 0.14);
      const panes = Math.max(1, Math.round(len / (op.window.pane || 2.4)));
      for (let i = 1; i < panes; i++) wallBox(b, 'trimDark', w, op.a + (len / panes) * i, wy, T / 2, 0.09, wh, T + 0.06);
      // glass sheet in the middle of the wall
      const f = w.fixed + w.inward * (T / 2);
      if (op.window.noGlass) continue;
      if (w.along === 'x') b.plane('glass', mid, wy, f, len, wh, {});
      else b.plane('glass', f, wy, mid, len, wh, { ry: Math.PI / 2 });
      // thin sill light
      wallGlow(b, G(accent, 1.4, 0), w, mid, op.y0 + fr + 0.012, T + 0.085, len - 0.4, 0.02, 0.02);
    }

    // ---- door frames + destination signs ----
    for (const op of openings) {
      if (!op.door) continue;
      const d = op.door;
      const mid = (op.a + op.b) / 2;
      const jw = 0.22;
      const depth = T + 0.1;
      wallBox(b, 'trimDark', w, op.a - jw / 2 + 0.04, (d.h + 0.2) / 2, depth / 2, jw, d.h + 0.2, depth);
      wallBox(b, 'trimDark', w, op.b + jw / 2 - 0.04, (d.h + 0.2) / 2, depth / 2, jw, d.h + 0.2, depth);
      wallBox(b, 'trimDark', w, mid, d.h + 0.13, depth / 2, d.w + jw * 2 - 0.08, 0.26, depth);
      // chamfer blocks at the top corners
      wallBox(b, 'metalDark', w, op.a + 0.16, d.h - 0.16, depth / 2, 0.36, 0.1, depth - 0.04, w.along === 'x' ? { rz: Math.PI / 4 } : { rx: -Math.PI / 4 });
      wallBox(b, 'metalDark', w, op.b - 0.16, d.h - 0.16, depth / 2, 0.36, 0.1, depth - 0.04, w.along === 'x' ? { rz: -Math.PI / 4 } : { rx: Math.PI / 4 });
      // jamb light strips (always lit: battery-backed wayfinding)
      wallGlow(b, G(accent, 1.6, 2), w, op.a - jw / 2 + 0.04, d.h * 0.5, depth + 0.006, 0.035, d.h * 0.7, 0.012);
      wallGlow(b, G(accent, 1.6, 2), w, op.b + jw / 2 - 0.04, d.h * 0.5, depth + 0.006, 0.035, d.h * 0.7, 0.012);
      // threshold plate
      wallBox(b, 'tread', w, mid, 0.008, 0.45, d.w, 0.016, 0.9, { fine: false });
      wallBox(b, 'hazard', w, mid, 0.012, 0.93, d.w, 0.016, 0.08, { fine: false });
      // sign plate naming the room beyond
      const other = R[d.a === room.id ? d.b : d.a];
      const sy = d.h + 0.62;
      if (sy + 0.2 < h) {
        wallBox(b, 'black', w, mid, sy, depth / 2 + 0.03, Math.min(d.w + 0.4, 2.6), 0.36, depth, { fine: false });
        const p = wallPoint(room, side, mid, 0.1 + 0.032);
        b.sign(other.name.toUpperCase(), p.x, sy, p.z, 0.2, { ry: p.ry, px: 72, tint: other.color, i: 1.5, type: 2, weight: 600, spacing: 0.22 });
      }
      b.light(...lightAt(w, mid, d.h * 0.6, 0.9), accent, 0.35, 3.2);
    }

    // ---- ribs between openings ----
    if (o.ribs) {
      for (const [s, e] of stretches) {
        const len = e - s;
        if (len < 1.6) continue;
        const n = Math.max(1, Math.round(len / o.ribs));
        for (let i = 0; i < n; i++) {
          const u = s + (len / n) * (i + 0.5);
          if (o.ribSkip && o.ribSkip(side, u)) continue;
          wallBox(b, 'trimDark', w, u, h / 2, T + 0.07, 0.36, h, 0.14);
          wallBox(b, 'metalDark', w, u, h - 0.28, T + 0.16, 0.36, 0.5, 0.22, w.along === 'x' ? { rx: (Math.PI / 5) * w.inward } : { rz: (-Math.PI / 5) * w.inward });
          wallBox(b, 'metalDark', w, u, 0.2, T + 0.12, 0.46, 0.4, 0.2, { fine: false });
          wallGlow(b, G(accent, o.accentI, 0), w, u, h * 0.5, T + 0.145, 0.05, h * 0.5, 0.012);
          b.light(...lightAt(w, u, h * 0.5, 0.7), accent, 0.32, 3.4);
        }
      }
    }

    // ---- cove and skirting along solid stretches ----
    for (const [s, e] of stretches) {
      const len = e - s;
      const mid = (s + e) / 2;
      if (o.cove) {
        wallBox(b, 'trimDark', w, mid, h - 0.13, T + 0.11, len, 0.3, 0.3, w.along === 'x' ? { rx: (Math.PI / 4) * w.inward, fine: true } : { rz: (-Math.PI / 4) * w.inward, fine: true });
        wallGlow(b, G(accent, 1.5, 0), w, mid, h - 0.37, T + 0.05, len - 0.3, 0.035, 0.03);
        const n = Math.max(1, Math.round(len / 3.5));
        for (let i = 0; i < n; i++) b.light(...lightAt(w, s + (len / n) * (i + 0.5), h - 0.6, 0.6), accent, 0.26, 4.2);
      }
      if (o.skirt) {
        wallBox(b, 'black', w, mid, 0.11, T + 0.035, len, 0.22, 0.07, { fine: false });
        if (o.skirtGlow) {
          wallGlow(b, G(accent, 1.2, 0), w, mid, 0.235, T + 0.04, len - 0.2, 0.018, 0.02);
        }
      }
    }
  }

  // ---- ceiling beams and light fixtures ------------------------------------
  if (o.beams && o.ceil) {
    const alongZ = room.d >= room.w;
    const len = alongZ ? room.d : room.w;
    const n = Math.max(1, Math.round(len / o.beams));
    for (let i = 1; i < n; i++) {
      const t = (len / n) * i;
      if (alongZ) {
        const z = z0 + t;
        if (o.skylights.some((s) => z > s.z0 - 0.3 && z < s.z1 + 0.3)) continue;
        b.box('trimDark', cx, h - 0.11, z, room.w - T * 2, 0.22, 0.26);
      } else {
        const x = x0 + t;
        if (o.skylights.some((s) => x > s.x0 - 0.3 && x < s.x1 + 0.3)) continue;
        b.box('trimDark', x, h - 0.11, cz, 0.26, 0.22, room.d - T * 2);
      }
    }
  }
  if (o.ceilLights && o.ceil) {
    const cl = o.ceilLights;
    const nx = Math.max(1, Math.round((room.w - 2) / cl.spacing));
    const nz = Math.max(1, Math.round((room.d - 2) / cl.spacing));
    const range = o.lightRange || Math.min(9.5, Math.max(6.5, h * 1.6));
    for (let iz = 0; iz < nz; iz++) {
      for (let ix = 0; ix < nx; ix++) {
        const x = x0 + (room.w / nx) * (ix + 0.5);
        const z = z0 + (room.d / nz) * (iz + 0.5);
        if (o.skylights.some((s) => x > s.x0 - 0.8 && x < s.x1 + 0.8 && z > s.z0 - 0.8 && z < s.z1 + 0.8)) continue;
        if (cl.skip && cl.skip(x, z)) continue;
        ceilingFixture(b, x, h, z, cl.color, cl.intensity, range, room.d >= room.w);
      }
    }
  }

  // ---- emergency beacons ---------------------------------------------------
  if (o.emergency) {
    const pts = o.emergencyAt || [
      [x0 + 1.2, z0 + 1.2],
      [x1 - 1.2, z1 - 1.2],
      [x1 - 1.2, z0 + 1.2],
      [x0 + 1.2, z1 - 1.2],
    ].slice(0, room.w * room.d > 120 ? 4 : 2);
    for (const [ex, ez] of pts) emergencyBeacon(b, ex, h - 0.02, ez, Math.max(9.5, h * 1.7));
    // faint always-on glow so the room is never pitch black
    b.light(cx, Math.min(h, 3.6) - 0.5, cz, 0xff2a1a, 0.5, Math.max(room.w, room.d) * 0.9, { emerg: true });
  }

  return info;
}

function lightAt(w, u, y, off) {
  const f = w.fixed + w.inward * (T + off);
  return w.along === 'x' ? [u, y, f] : [f, y, u];
}

function addWallCollider(b, w, s, e) {
  const f0 = w.fixed;
  const f1 = w.fixed + w.inward * T;
  const lo = Math.min(f0, f1);
  const hi = Math.max(f0, f1);
  if (w.along === 'x') b.solid(s, lo, e, hi, { wall: true });
  else b.solid(lo, s, hi, e, { wall: true });
}

// Recessed ceiling light with a baked pool of light beneath it.
export function ceilingFixture(b, x, h, z, color = 0xdfeeff, intensity = 1.2, range = 7, alongZ = false, o = {}) {
  const lw = o.w ?? 1.5;
  const ld = o.d ?? 0.46;
  const a = alongZ ? [ld, lw] : [lw, ld];
  b.box('trimDark', x, h - 0.05, z, a[0] + 0.2, 0.1, a[1] + 0.2, { fine: false });
  b.box('metalDark', x, h - 0.09, z, a[0] + 0.08, 0.05, a[1] + 0.08, { fine: false });
  b.gbox(G(color, 3.2, 0, o.fx || 0), x, h - 0.118, z, a[0], 0.02, a[1]);
  b.light(x, h - 0.75, z, color, intensity, range, { key: true });
}

// Red rotating-style beacon used when a deck is on emergency power.
export function emergencyBeacon(b, x, y, z, range = 9.5) {
  b.cyl('metalDark', x, y - 0.04, z, 0.13, 0.08, { seg: 10 });
  b.gcyl(G(0xff2a12, 5, 1), x, y - 0.14, z, 0.085, 0.13, { seg: 10 });
  b.cyl('metalDark', x, y - 0.215, z, 0.095, 0.02, { seg: 10 });
  b.light(x, y - 0.5, z, 0xff2a12, 1.5, range, { emerg: true });
}
