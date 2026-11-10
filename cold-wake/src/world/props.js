// Reusable set dressing. Every function builds in the builder's current local
// frame: origin on the deck, +Z is the front of the prop.
import { G } from '../engine/builder.js';

const PI = Math.PI;

// Angled operator console with a raised monitor and a lit key deck.
export function consoleDesk(b, ctx, o = {}) {
  const { w = 1.6, screen = 'diag', accent = 0x5fe3ff, always = false, solid = true, keys = true } = o;
  b.box('trimDark', 0, 0.42, -0.12, w, 0.84, 0.5);
  b.box('metalDark', 0, 0.06, -0.06, w + 0.14, 0.12, 0.7, { fine: false });
  b.box('metalDark', 0, 0.92, 0.1, w + 0.08, 0.07, 0.82, { rx: 0.3 });
  b.box('black', 0, 0.955, 0.12, w - 0.12, 0.02, 0.62, { rx: 0.3, fine: false });
  // side cheeks
  for (const s of [-1, 1]) {
    b.box('trimDark', s * (w / 2 + 0.02), 0.72, 0.02, 0.07, 0.62, 0.86, { rx: 0.3, fine: false });
    b.gbox(G(accent, 2, always ? 2 : 0), s * (w / 2 + 0.058), 0.74, 0.05, 0.012, 0.03, 0.6, { rx: 0.3 });
  }
  // monitor
  b.box('black', 0, 1.42, -0.3, w - 0.1, 0.62, 0.07, { rx: -0.16, fine: false });
  b.box('trimDark', 0, 1.1, -0.3, 0.26, 0.2, 0.1, { fine: false });
  ctx.screen(screen, 0, 1.42, -0.262, w - 0.2, 0.52, { rx: -0.16, always });
  if (keys) {
    // key deck: rows of softly lit pads
    const cols = Math.max(4, Math.round(w / 0.16));
    for (let r = 0; r < 3; r++) {
      for (let c = 0; c < cols; c++) {
        if ((r * 7 + c * 3) % 5 === 0) continue;
        const x = -w / 2 + 0.16 + (c / (cols - 1)) * (w - 0.32);
        const zz = -0.02 + r * 0.14;
        const yy = 0.972 - (zz - 0.12) * Math.tan(0.3);
        const col = (r + c) % 7 === 0 ? 0xffb347 : accent;
        b.gbox(G(col, 1.3, always ? 2 : 0), x, yy, zz, 0.09, 0.012, 0.08, { rx: 0.3 });
      }
    }
  }
  b.light(0, 1.3, 0.7, accent, 0.4, 3.2);
  if (solid) b.solidBox(0, -0.05, w + 0.1, 0.9);
}

// Wall-mounted display. Call inside onWall (z = 0 is the wall face).
export function wallScreen(b, ctx, channel, y = 1.7, w = 1.3, h = 0.8, o = {}) {
  b.box('black', 0, y, 0.04, w + 0.12, h + 0.12, 0.08, { fine: false });
  b.box('trimDark', 0, y - h / 2 - 0.1, 0.05, w * 0.5, 0.06, 0.1, { fine: false });
  ctx.screen(channel, 0, y, 0.085, w, h, { always: !!o.always, gain: o.gain });
  b.gbox(G(o.accent || 0x5fe3ff, 2, o.always ? 2 : 0), w / 2 - 0.08, y - h / 2 - 0.1, 0.102, 0.05, 0.02, 0.01);
  if (!o.noLight) b.light(0, y, 0.9, o.accent || 0x5fe3ff, 0.3, 3);
}

export function crate(b, x, z, s = 1, mat = 'crate', o = {}) {
  const w = o.w || s;
  const h = o.h || s;
  const d = o.d || s;
  const y = (o.y || 0) + h / 2;
  b.box(mat, x, y, z, w, h, d, { uv: 'fit', ry: o.ry || 0, fine: w > 1.8 });
  if (!o.noSolid && !o.y) {
    const r = Math.abs(Math.sin(o.ry || 0)) > 0.3 && Math.abs(Math.cos(o.ry || 0)) > 0.3 ? 1.25 : 1;
    b.solidBox(x, z, w * r, d * r);
  }
}

export function barrel(b, x, z, mat = 'blue', o = {}) {
  const r = o.r || 0.3;
  const h = o.h || 0.92;
  b.cyl(mat, x, h / 2, z, r, h, { seg: 14 });
  b.cyl('metalDark', x, h * 0.3, z, r + 0.015, 0.05, { seg: 14 });
  b.cyl('metalDark', x, h * 0.7, z, r + 0.015, 0.05, { seg: 14 });
  b.cyl('metalDark', x, h + 0.01, z, r * 0.92, 0.03, { seg: 14 });
  b.cyl('metal', x + r * 0.5, h + 0.04, z, 0.05, 0.05, { seg: 8 });
  if (!o.noSolid) b.solidBox(x, z, r * 2, r * 2);
}

// Personnel locker; front at +Z. Returns nothing; use ctx.dyn for an opening door.
export function locker(b, o = {}) {
  const { w = 0.62, h = 2.05, d = 0.5, mat = 'gray', label = null, open = false, accent = 0x5fe3ff } = o;
  b.box('metalDark', 0, h / 2, -d / 2 + 0.02, w, h, 0.04, { fine: false });
  b.box(mat, -w / 2 + 0.02, h / 2, 0, 0.04, h, d, { fine: false });
  b.box(mat, w / 2 - 0.02, h / 2, 0, 0.04, h, d, { fine: false });
  b.box(mat, 0, h - 0.02, 0, w, 0.04, d, { fine: false });
  b.box(mat, 0, 0.05, 0, w, 0.1, d, { fine: false });
  if (!open) lockerDoor(b, w, h, d, mat, accent);
  if (label) b.sign(label, 0, h - 0.2, d / 2 + 0.012, 0.07, { px: 48, tint: 0xdfefff, i: 0.8, type: 3 });
}

export function lockerDoor(b, w, h, d, mat = 'gray', accent = 0x5fe3ff) {
  b.box(mat, 0, h / 2, d / 2 - 0.015, w - 0.03, h - 0.06, 0.03, { fine: false });
  for (let i = 0; i < 4; i++) b.box('black', 0, h - 0.42 - i * 0.05, d / 2 + 0.002, w * 0.6, 0.018, 0.006, { fine: false });
  b.box('metal', w * 0.32, h * 0.5, d / 2 + 0.02, 0.03, 0.16, 0.04, { fine: false });
  b.gbox(G(accent, 1.6, 2), w * 0.32, h * 0.62, d / 2 + 0.004, 0.03, 0.03, 0.006);
}

// Slatted ventilation grille. Call inside onWall.
export function ventGrille(b, y = 2.4, w = 1.0, h = 0.5) {
  b.box('trimDark', 0, y, 0.025, w + 0.1, h + 0.1, 0.05, { fine: false });
  b.box('black', 0, y, 0.03, w, h, 0.05, { fine: false });
  const n = Math.max(3, Math.round(h / 0.07));
  for (let i = 0; i < n; i++) b.box('metal', 0, y - h / 2 + (h / n) * (i + 0.5), 0.062, w - 0.04, 0.022, 0.03, { rx: 0.5, fine: false });
}

// Wall junction box with status lights. Call inside onWall.
export function junctionBox(b, y = 1.4, o = {}) {
  const c = o.color || 0x6dffb0;
  b.box('gray', 0, y, 0.06, 0.42, 0.56, 0.12, { fine: false });
  b.box('metalDark', 0, y, 0.125, 0.36, 0.5, 0.012, { fine: false });
  b.box('metal', 0.13, y, 0.14, 0.03, 0.12, 0.03, { fine: false });
  b.gbox(G(c, 2.2, 0), -0.1, y + 0.18, 0.134, 0.035, 0.035, 0.008);
  b.gbox(G(0xffb347, 2.2, 0, 3), -0.04, y + 0.18, 0.134, 0.035, 0.035, 0.008);
  b.tube('rubber', [[0, y + 0.28, 0.06], [0, y + 0.7, 0.06], [0.3, y + 0.9, 0.05]], 0.022, { seg: 6 });
  b.tube('rubber', [[-0.1, y - 0.28, 0.06], [-0.1, 0.1, 0.06]], 0.018, { seg: 6 });
}

// Horizontal pipe bundle along a wall. Call inside onWall; runs along local X.
export function pipeBundle(b, x0, x1, y = 2.9, o = {}) {
  const mats = o.mats || ['copper', 'metal', 'orange'];
  mats.forEach((m, i) => {
    const r = [0.06, 0.045, 0.035][i % 3];
    const yy = y - i * 0.14;
    const zz = 0.1 + (i % 2) * 0.03;
    b.cyl(m, (x0 + x1) / 2, yy, zz, r, Math.abs(x1 - x0), { axis: 'x', seg: 8 });
  });
  const n = Math.max(2, Math.round(Math.abs(x1 - x0) / 2.2));
  for (let i = 0; i <= n; i++) {
    const x = x0 + ((x1 - x0) / n) * i;
    b.box('metalDark', x, y - 0.14, 0.09, 0.07, 0.5, 0.18, { fine: false });
  }
}

// Swivel seat facing +Z.
export function chair(b, o = {}) {
  const mat = o.mat || 'fabricDark';
  b.cyl('metalDark', 0, 0.03, 0, 0.3, 0.06, { seg: 12 });
  b.cyl('chrome', 0, 0.25, 0, 0.045, 0.4, { seg: 8 });
  b.box('trimDark', 0, 0.47, 0, 0.5, 0.07, 0.5, { fine: false });
  b.box(mat, 0, 0.54, 0.01, 0.46, 0.09, 0.46, { fine: false });
  b.box('trimDark', 0, 0.98, -0.25, 0.48, 0.92, 0.07, { rx: -0.12, fine: false });
  b.box(mat, 0, 0.98, -0.215, 0.42, 0.8, 0.06, { rx: -0.12, fine: false });
  b.box(mat, 0, 1.5, -0.28, 0.28, 0.2, 0.09, { rx: -0.12, fine: false });
  for (const s of [-1, 1]) {
    b.box('trimDark', s * 0.28, 0.7, 0.02, 0.06, 0.05, 0.4, { fine: false });
    b.box('trimDark', s * 0.28, 0.6, -0.14, 0.05, 0.2, 0.05, { fine: false });
    if (o.glow) b.gbox(G(o.glow, 1.6, 0), s * 0.28, 0.728, 0.1, 0.02, 0.006, 0.16);
  }
  if (o.solid !== false) b.solidBox(0, 0, 0.5, 0.5);
}

export function stool(b, x, z) {
  b.cyl('metalDark', x, 0.02, z, 0.2, 0.04, { seg: 10 });
  b.cyl('chrome', x, 0.26, z, 0.03, 0.46, { seg: 6 });
  b.cyl('fabricDark', x, 0.52, z, 0.19, 0.08, { seg: 12 });
}

export function table(b, w = 1.8, d = 0.9, o = {}) {
  const h = o.h || 0.78;
  b.box(o.top || 'white', 0, h, 0, w, 0.05, d);
  b.box('trimDark', 0, h - 0.045, 0, w - 0.08, 0.04, d - 0.08, { fine: false });
  for (const s of [-1, 1]) {
    b.box('metalDark', s * (w / 2 - 0.25), h / 2 - 0.02, 0, 0.09, h - 0.08, d * 0.5, { fine: false });
    b.box('metalDark', s * (w / 2 - 0.25), 0.025, 0, 0.16, 0.05, d * 0.8, { fine: false });
  }
  if (o.solid !== false) b.solidBox(0, 0, w, d);
}

// Two-tier bunk set into a wall alcove; head at -X.
export function bunk(b, o = {}) {
  const L = 2.05;
  const W = 0.95;
  for (const y of [0.42, 1.52]) {
    b.box('trimDark', 0, y, 0, L, 0.1, W);
    b.box(o.sheet || 'fabricBlue', 0.02, y + 0.11, 0.02, L - 0.1, 0.12, W - 0.08);
    b.box('white', -L / 2 + 0.24, y + 0.2, 0.02, 0.36, 0.08, W - 0.3, { fine: false });
    b.box(o.blanket || 'fabric', 0.3, y + 0.185, 0.02, L * 0.55, 0.03, W - 0.06, { fine: false });
    b.gbox(G(0xffc890, 1.8, 0), -L / 2 + 0.05, y + 0.62, -W / 2 + 0.06, 0.02, 0.05, 0.28);
    b.light(-L / 2 + 0.4, y + 0.6, 0, 0xffc890, 0.3, 2.2);
  }
  for (const s of [-1, 1]) b.box('trimDark', s * (L / 2 - 0.03), 1.1, W / 2 - 0.03, 0.06, 2.2, 0.06, { fine: false });
  // ladder
  b.box('metal', L / 2 - 0.28, 0.98, W / 2 - 0.03, 0.03, 1.2, 0.03, { fine: false });
  for (let i = 0; i < 4; i++) b.box('metal', L / 2 - 0.15, 0.55 + i * 0.28, W / 2 - 0.03, 0.26, 0.025, 0.03, { fine: false });
  b.solidBox(0, 0, L, W);
}

// Upright stasis capsule; front (glass) faces +Z.
export function cryoPod(b, ctx, o = {}) {
  const { occupied = true, open = false, label = '01', accent = 0x66ccff } = o;
  const g = (i, fx = 0) => G(accent, i, 2, fx);
  // plinth and crown
  b.cyl('metalDark', 0, 0.09, 0, 0.72, 0.18, { seg: 16 });
  b.cyl('trimDark', 0, 0.22, 0, 0.62, 0.1, { seg: 16 });
  b.gtor(g(2.2, 2), 0, 0.185, 0, 0.7, 0.014, { axis: 'y', seg: 28 });
  b.cyl('trimDark', 0, 2.5, -0.05, 0.6, 0.22, { seg: 16 });
  b.cyl('metalDark', 0, 2.66, -0.05, 0.42, 0.12, { seg: 16 });
  // back shell
  b.box('wallDark', 0, 1.36, -0.42, 1.12, 2.2, 0.26, { tile: 2 });
  b.box('trimDark', -0.54, 1.36, -0.12, 0.1, 2.2, 0.62);
  b.box('trimDark', 0.54, 1.36, -0.12, 0.1, 2.2, 0.62);
  b.gbox(g(2.4), -0.596, 1.36, 0.05, 0.012, 1.7, 0.04);
  b.gbox(g(2.4), 0.596, 1.36, 0.05, 0.012, 1.7, 0.04);
  // interior padding
  b.box('fabricDark', 0, 1.36, -0.27, 0.9, 2.0, 0.06, { fine: false });
  b.gbox(g(0.9), 0, 2.32, -0.2, 0.7, 0.03, 0.03);
  if (occupied) {
    b.sph('skin', 0, 1.9, -0.12, 0.12, { seg: 10 });
    b.box('fabricBlue', 0, 1.42, -0.14, 0.44, 0.62, 0.2, { fine: false });
    b.box('fabricBlue', -0.12, 0.74, -0.14, 0.17, 0.78, 0.17, { fine: false });
    b.box('fabricBlue', 0.12, 0.74, -0.14, 0.17, 0.78, 0.17, { fine: false });
    b.box('fabricBlue', -0.3, 1.38, -0.14, 0.12, 0.62, 0.14, { fine: false });
    b.box('fabricBlue', 0.3, 1.38, -0.14, 0.12, 0.62, 0.14, { fine: false });
  }
  if (!open) {
    // frosted canopy: a half cylinder of glass
    b.cyl(occupied ? 'glassFrost' : 'glassTint', 0, 1.36, -0.02, 0.5, 2.1, { seg: 14, open: true });
  }
  // hoses and status plate
  b.tube('rubber', [[-0.45, 2.45, -0.3], [-0.6, 2.9, -0.4], [-0.6, 3.3, -0.5]], 0.04, { seg: 6 });
  b.tube('rubber', [[0.45, 2.45, -0.3], [0.62, 2.8, -0.45], [0.62, 3.3, -0.5]], 0.03, { seg: 6 });
  b.box('black', 0.66, 1.3, 0.2, 0.05, 0.34, 0.26, { fine: false });
  ctx.screen(occupied ? 'vitals' : 'diag', 0.69, 1.3, 0.2, 0.22, 0.3, { ry: PI / 2, always: true, gain: 1.1 });
  b.sign(`POD ${label}`, 0, 2.5, 0.56, 0.1, { px: 56, tint: accent, i: 1.6, type: 2 });
  b.light(0, 1.4, 0.9, accent, 0.55, 3.6, { emerg: false });
  if (open) {
    // hollow: the player can stand inside
    b.solid(-0.62, -0.56, -0.48, 0.42);
    b.solid(0.48, -0.56, 0.62, 0.42);
    b.solid(-0.62, -0.56, 0.62, -0.32);
  } else {
    b.solid(-0.62, -0.56, 0.62, 0.42);
  }
}

// Diagnostic bed with a scanner arch; head at -X, front +Z.
export function medBed(b, ctx, o = {}) {
  const accent = o.accent || 0x7dffc0;
  b.box('trimDark', 0, 0.3, 0, 2.1, 0.6, 0.8);
  b.box('white', 0, 0.66, 0, 2.2, 0.12, 0.92);
  b.box('fabric', 0.05, 0.76, 0, 1.95, 0.08, 0.8);
  b.box('white', -0.82, 0.83, 0, 0.36, 0.07, 0.5, { fine: false });
  b.gbox(G(accent, 1.8, 0), 0, 0.59, 0.47, 1.9, 0.02, 0.012);
  // scanner arch
  b.tor('white', -0.1, 0.72, 0, 0.66, 0.07, { axis: 'x', arc: PI, seg: 18 });
  b.gtor(G(accent, 2.6, 0, 2), -0.1, 0.72, 0, 0.6, 0.014, { axis: 'x', arc: PI, seg: 18 });
  b.box('trimDark', -0.1, 0.4, 0.66, 0.2, 0.8, 0.12, { fine: false });
  b.box('trimDark', -0.1, 0.4, -0.66, 0.2, 0.8, 0.12, { fine: false });
  // bedside monitor arm
  b.cyl('chrome', -1.0, 0.85, -0.5, 0.03, 1.7, { seg: 6 });
  b.box('black', -0.86, 1.7, -0.5, 0.5, 0.36, 0.05, { ry: 0.5, fine: false });
  ctx.screen('vitals', -0.845, 1.7, -0.47, 0.44, 0.3, { ry: 0.5 });
  b.light(0, 1.6, 0.4, accent, 0.4, 3.4);
  b.solidBox(0, 0, 2.2, 0.95);
}

// Cylindrical specimen tank.
export function tank(b, x, z, o = {}) {
  const { r = 0.42, h = 2.0, color = 0x7dffc0, intensity = 1.6, shape = 0 } = o;
  b.cyl('trimDark', x, 0.2, z, r + 0.1, 0.4, { seg: 16 });
  b.cyl('trimDark', x, 0.4 + h + 0.12, z, r + 0.1, 0.24, { seg: 16 });
  b.gtor(G(color, 2.2, 0), x, 0.41, z, r + 0.02, 0.014, { axis: 'y', seg: 24 });
  b.gcyl(G(color, 0.35 * intensity, 0, 2), x, 0.4 + h / 2, z, r * 0.86, h * 0.98, { seg: 14 });
  b.cyl('glass', x, 0.4 + h / 2, z, r, h, { seg: 16, open: true });
  // suspended specimen
  if (shape === 0) {
    b.sph('skin', x, 0.4 + h * 0.55, z, r * 0.36, { seg: 10, sy: 1.5 });
  } else if (shape === 1) {
    b.tor('green', x, 0.4 + h * 0.5, z, r * 0.36, 0.05, { axis: 'x', seg: 12 });
    b.tor('green', x, 0.4 + h * 0.62, z, r * 0.24, 0.04, { axis: 'z', seg: 12 });
  } else {
    for (let i = 0; i < 4; i++) b.box('copper', x + Math.sin(i * 2.1) * 0.1, 0.4 + h * (0.3 + i * 0.13), z + Math.cos(i * 1.7) * 0.1, 0.14, 0.14, 0.14, { ry: i, rx: i * 0.7, fine: false });
  }
  b.light(x, 0.4 + h / 2, z, color, 0.5 * intensity, 4);
  b.solidBox(x, z, r * 2 + 0.2, r * 2 + 0.2);
}

// Rack of blinking compute blades; front +Z.
export function serverRack(b, o = {}) {
  const { w = 0.8, h = 2.4, d = 0.9, color = 0x6f8cff, type = 0 } = o;
  b.box('black', 0, h / 2, 0, w, h, d);
  b.box('trimDark', 0, h / 2, d / 2 + 0.01, w + 0.04, h + 0.02, 0.03);
  const rows = Math.floor((h - 0.3) / 0.16);
  for (let r = 0; r < rows; r++) {
    const y = 0.2 + r * 0.16;
    b.box('metalDark', 0, y, d / 2 + 0.03, w - 0.12, 0.12, 0.02, { fine: false });
    const n = 5;
    for (let i = 0; i < n; i++) {
      const k = (r * 13 + i * 7) % 11;
      if (k < 3) continue;
      const c = k === 10 ? 0xffb347 : k === 9 ? 0x6dffb0 : color;
      b.gbox(G(c, 2.4, type, k % 3 === 0 ? 1 : k % 3 === 1 ? 3 : 5), -w / 2 + 0.14 + i * 0.07, y, d / 2 + 0.043, 0.035, 0.022, 0.006);
    }
    b.gbox(G(color, 1.1, type, 5), w / 2 - 0.22, y, d / 2 + 0.043, 0.18, 0.012, 0.006);
  }
  b.light(0, h * 0.5, d / 2 + 0.7, color, 0.42, 3.6);
  if (o.solid !== false) b.solidBox(0, 0, w, d);
}

// Guard rail along local X.
export function railing(b, x0, x1, z, o = {}) {
  const h = o.h || 1.05;
  const len = Math.abs(x1 - x0);
  const n = Math.max(1, Math.round(len / 1.6));
  for (let i = 0; i <= n; i++) {
    const x = x0 + ((x1 - x0) / n) * i;
    b.box('metalDark', x, h / 2, z, 0.06, h, 0.06, { fine: false });
  }
  b.cyl(o.mat || 'yellow', (x0 + x1) / 2, h, z, 0.035, len, { axis: 'x', seg: 8 });
  b.cyl('metal', (x0 + x1) / 2, h * 0.55, z, 0.022, len, { axis: 'x', seg: 6 });
  if (o.solid !== false) b.solid(Math.min(x0, x1), z - 0.06, Math.max(x0, x1), z + 0.06);
}

// Hydroponic bed with a grow-light bar. Runs along local X.
export function planter(b, len = 4, o = {}) {
  const { kind = 0, seed = 1, lamp = 0xff58d8 } = o;
  const w = 1.0;
  b.box('white', 0, 0.42, 0, len, 0.5, w);
  b.box('trimDark', 0, 0.12, 0, len - 0.2, 0.24, w - 0.2);
  b.box('soil', 0, 0.685, 0, len - 0.12, 0.04, w - 0.12, { fine: true });
  b.gbox(G(0x6dffb0, 1.4, 0), 0, 0.3, w / 2 + 0.004, len - 0.3, 0.02, 0.01);
  // lamp bar on posts
  const lh = 2.25;
  for (const s of [-1, 1]) b.box('metalDark', s * (len / 2 - 0.08), lh / 2 + 0.3, 0, 0.06, lh - 0.3, 0.06, { fine: false });
  b.box('trimDark', 0, lh + 0.18, 0, len, 0.08, 0.34, { fine: false });
  b.gbox(G(lamp, 3.2, 0), 0, lh + 0.13, 0, len - 0.2, 0.02, 0.22);
  const nl = Math.max(1, Math.round(len / 2.2));
  for (let i = 0; i < nl; i++) b.light(-len / 2 + (len / nl) * (i + 0.5), lh - 0.4, 0, lamp, 0.75, 3.6);
  // plants
  let s = seed * 9301 + 49297;
  const R = () => {
    s = (s * 9301 + 49297) % 233280;
    return s / 233280;
  };
  const n = Math.round(len * 2.6);
  for (let i = 0; i < n; i++) {
    const x = -len / 2 + 0.3 + (i / (n - 1)) * (len - 0.6) + (R() - 0.5) * 0.12;
    const z = (R() - 0.5) * 0.5;
    const hgt = kind === 1 ? 1.0 + R() * 0.5 : kind === 2 ? 0.45 + R() * 0.2 : 0.65 + R() * 0.35;
    const wd = hgt * (kind === 1 ? 0.5 : 0.85);
    const ry = R() * PI;
    const mat = kind === 2 && i % 3 === 0 ? 'leafRed' : 'leaf';
    b.plane(mat, x, 0.7 + hgt / 2, z, wd, hgt, { ry });
    b.plane(mat, x, 0.7 + hgt / 2, z, wd, hgt, { ry: ry + PI / 2 });
    if (kind === 0 && i % 2 === 0) {
      b.sph('red', x + 0.1, 0.72 + hgt * 0.55, z + 0.08, 0.055, { seg: 8 });
      b.sph('red', x - 0.08, 0.72 + hgt * 0.38, z - 0.06, 0.045, { seg: 8 });
    }
    if (kind === 1 && i % 2 === 0) b.sph('yellow', x + 0.04, 0.7 + hgt * 0.6, z, 0.05, { seg: 8, sy: 2.2 });
  }
  if (o.solid !== false) b.solidBox(0, 0, len, w);
}

// Round projector table.
export function holoTable(b, r = 1.3, o = {}) {
  const accent = o.accent || 0x58e0d0;
  b.cyl('trimDark', 0, 0.4, 0, r * 0.55, 0.8, { seg: 20, top: 0.8 });
  b.cyl('metalDark', 0, 0.86, 0, r, 0.14, { seg: 28 });
  b.cyl('black', 0, 0.935, 0, r * 0.86, 0.012, { seg: 28 });
  b.gtor(G(accent, 2.6, 0), 0, 0.94, 0, r * 0.9, 0.018, { axis: 'y', seg: 40 });
  b.gtor(G(accent, 1.6, 0, 2), 0, 0.945, 0, r * 0.55, 0.012, { axis: 'y', seg: 32 });
  b.gcyl(G(accent, 2.2, 0), 0, 0.945, 0, r * 0.12, 0.012, { seg: 12 });
  b.gtor(G(accent, 1.8, 0), 0, 0.5, 0, r * 0.5, 0.012, { axis: 'y', seg: 28 });
  for (let i = 0; i < 8; i++) {
    const a = (i / 8) * PI * 2;
    b.box('trimDark', Math.cos(a) * r * 0.98, 0.86, Math.sin(a) * r * 0.98, 0.16, 0.2, 0.1, { ry: -a + PI / 2, fine: false });
  }
  b.light(0, 1.7, 0, accent, 0.9, 5.5);
  b.solidBox(0, 0, r * 1.7, r * 1.7);
}

// Glowing data slate for collectible logs. Lies flat; build inside ctx.dyn.
export function dataPad(b, color = 0x5fe3ff) {
  b.box('black', 0, 0.012, 0, 0.26, 0.024, 0.18, { fine: false });
  b.gbox(G(color, 1.5, 2, 2), 0, 0.026, 0, 0.22, 0.004, 0.14);
  b.gbox(G(0xffffff, 2.6, 2, 3), 0.09, 0.03, -0.05, 0.02, 0.004, 0.02);
}

// Fusion cell canister (pickup / socketed). Stands upright.
export function fusionCell(b, lit = true) {
  b.cyl('metalDark', 0, 0.04, 0, 0.11, 0.08, { seg: 12 });
  b.cyl('metalDark', 0, 0.46, 0, 0.11, 0.08, { seg: 12 });
  b.cyl('glass', 0, 0.25, 0, 0.095, 0.34, { seg: 12, open: true });
  b.gcyl(G(0x66e8ff, lit ? 3.2 : 0.4, 2, 5), 0, 0.25, 0, 0.06, 0.32, { seg: 10 });
  for (let i = 0; i < 3; i++) {
    const a = (i / 3) * PI * 2;
    b.box('chrome', Math.cos(a) * 0.1, 0.25, Math.sin(a) * 0.1, 0.02, 0.42, 0.02, { fine: false });
  }
  b.box('orange', 0, 0.52, 0, 0.14, 0.03, 0.05, { fine: false });
}

// Wall cabinet with a fire extinguisher. Call inside onWall.
export function extinguisherCabinet(b) {
  b.box('red', 0, 1.25, 0.09, 0.5, 0.9, 0.18, { fine: false });
  b.box('black', 0, 1.25, 0.182, 0.42, 0.8, 0.006, { fine: false });
  b.sign('FIRE', 0, 1.8, 0.02, 0.1, { px: 56, tint: 0xff6a50, i: 1.6, type: 2, icon: 'warn' });
}

export function extinguisher(b) {
  b.cyl('red', 0, 0.26, 0, 0.085, 0.5, { seg: 12 });
  b.sph('red', 0, 0.51, 0, 0.085, { seg: 12, half: true });
  b.cyl('metalDark', 0, 0.6, 0, 0.03, 0.1, { seg: 8 });
  b.box('black', 0.05, 0.66, 0, 0.16, 0.03, 0.04, { fine: false });
  b.tube('rubber', [[0, 0.62, 0.03], [0, 0.5, 0.12], [0, 0.2, 0.1]], 0.014, { seg: 5 });
  b.box('white', 0, 0.3, 0.086, 0.1, 0.12, 0.004, { fine: false });
}

// Hand wheel valve; faces +Z. Build inside ctx.dyn so it can turn (rotation.z).
export function valveWheel(b, color = 'red') {
  b.tor(color, 0, 0, 0.1, 0.2, 0.025, { seg: 20 });
  for (let i = 0; i < 3; i++) b.box(color, 0, 0, 0.1, 0.38, 0.03, 0.03, { rz: (i * PI) / 3, fine: false });
  b.cyl('metalDark', 0, 0, 0.06, 0.045, 0.12, { axis: 'z', seg: 10 });
}

// Damaged wall panel that needs a patch. Call inside onWall.
export function brokenPanel(b, y = 1.3) {
  b.box('black', 0, y, 0.01, 0.7, 0.9, 0.04, { fine: false });
  b.box('metalDark', -0.08, y + 0.05, 0.03, 0.5, 0.7, 0.02, { rz: 0.12, fine: false });
  b.tube('copper', [[-0.2, y + 0.3, 0.04], [-0.05, y, 0.1], [-0.22, y - 0.3, 0.05]], 0.015, { seg: 5 });
  b.tube('red', [[0.15, y + 0.3, 0.04], [0.2, y + 0.05, 0.12], [0.08, y - 0.2, 0.06]], 0.012, { seg: 5 });
  b.tube('yellow', [[0.02, y + 0.32, 0.04], [0.1, y + 0.1, 0.09], [0.24, y - 0.28, 0.05]], 0.012, { seg: 5 });
  b.gbox(G(0xffb347, 2.4, 2, 1), 0.2, y + 0.34, 0.05, 0.05, 0.03, 0.01);
}

// Ceiling-hung cable tray with conduit; runs along local X at height y.
export function cableTray(b, x0, x1, y, z = 0) {
  const len = Math.abs(x1 - x0);
  const mid = (x0 + x1) / 2;
  b.box('metalDark', mid, y, z, len, 0.03, 0.46);
  b.box('metalDark', mid, y + 0.05, z - 0.23, len, 0.1, 0.02, { fine: false });
  b.box('metalDark', mid, y + 0.05, z + 0.23, len, 0.1, 0.02, { fine: false });
  b.cyl('rubber', mid, y + 0.05, z - 0.1, 0.035, len, { axis: 'x', seg: 6 });
  b.cyl('orange', mid, y + 0.045, z + 0.02, 0.028, len, { axis: 'x', seg: 6 });
  b.cyl('rubber', mid, y + 0.04, z + 0.13, 0.025, len, { axis: 'x', seg: 6 });
  const n = Math.max(2, Math.round(len / 2.5));
  for (let i = 0; i <= n; i++) {
    const x = x0 + ((x1 - x0) / n) * i;
    b.box('metal', x, y + 0.2, z - 0.2, 0.02, 0.4, 0.02, { fine: false });
    b.box('metal', x, y + 0.2, z + 0.2, 0.02, 0.4, 0.02, { fine: false });
  }
}

// Shelf unit loaded with bins; front +Z.
export function shelving(b, o = {}) {
  const { w = 2.2, h = 2.4, d = 0.6, seed = 3 } = o;
  for (const s of [-1, 1]) {
    b.box('metalDark', s * (w / 2 - 0.03), h / 2, d / 2 - 0.03, 0.06, h, 0.06, { fine: false });
    b.box('metalDark', s * (w / 2 - 0.03), h / 2, -d / 2 + 0.03, 0.06, h, 0.06, { fine: false });
  }
  let s0 = seed * 7919;
  const R = () => {
    s0 = (s0 * 9301 + 49297) % 233280;
    return s0 / 233280;
  };
  const mats = ['crateBlue', 'crateOrange', 'crateDark', 'crateGreen', 'crate'];
  for (let i = 0; i < 4; i++) {
    const y = 0.25 + i * ((h - 0.3) / 4);
    b.box('metal', 0, y, 0, w, 0.04, d);
    let x = -w / 2 + 0.2;
    while (x < w / 2 - 0.3) {
      const bw = 0.3 + R() * 0.35;
      const bh = 0.2 + R() * 0.28;
      if (x + bw > w / 2 - 0.08) break;
      if (R() > 0.2) b.box(mats[Math.floor(R() * mats.length)], x + bw / 2, y + 0.02 + bh / 2, (R() - 0.5) * 0.08, bw, bh, d * 0.75, { uv: 'fit', fine: false });
      x += bw + 0.05;
    }
  }
  if (o.solid !== false) b.solidBox(0, 0, w, d);
}
