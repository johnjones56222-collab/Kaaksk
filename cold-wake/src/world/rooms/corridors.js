// The two spine corridors share one kit: arch frames, cable tray, deck
// guide-lights that chase toward the bow, hand rails and service panels.
import { G } from '../../engine/builder.js';
import { onWall } from '../shell.js';
import { DOORS } from '../layout.js';
import { cableTray, wallScreen, ventGrille, junctionBox, extinguisherCabinet, extinguisher } from '../props.js';
import { has } from '../../core/state.js';
import { servicePanel, logPad, PI } from './common.js';

function corridor(ctx, o = {}) {
  const { b, room } = ctx;
  const { x0, x1, z0, z1, h } = room;
  const accent = room.color;
  ctx.setAmbient([0.04, 0.05, 0.06]);
  ctx.shell({
    ribs: 0,
    beams: 0,
    cove: false,
    ceilLights: { spacing: 4, intensity: 0.8, color: 0xdcecff },
    lightRange: 6,
  });
  // side doors (walls of constant X) - keep arches clear of them
  const sideDoors = DOORS.filter((d) => d.axis === 'x' && (d.a === room.id || d.b === room.id));
  const len = z1 - z0;
  const n = Math.round(len / 2.66);
  for (let i = 0; i <= n; i++) {
    const z = z0 + 0.3 + ((len - 0.6) / n) * i;
    if (sideDoors.some((d) => Math.abs(d.z - z) < d.w / 2 + 0.45)) continue;
    // arch frame
    for (const s of [-1, 1]) {
      const x = s * (x1 - 0.25 - 0.13);
      b.box('trimDark', x, h / 2, z, 0.26, h, 0.3);
      b.box('metalDark', s * (x1 - 0.72), h - 0.46, z, 0.95, 0.2, 0.3, { rz: -s * (PI / 4) });
      b.gbox(G(accent, 1.6, 0), s * (x1 - 0.25 - 0.265), 1.5, z, 0.012, 1.5, 0.05);
      b.gbox(G(accent, 1.4, 0), s * (x1 - 0.79), h - 0.55, z, 0.7, 0.02, 0.05, { rz: -s * (PI / 4) });
      b.light(s * (x1 - 1.0), 1.6, z, accent, 0.4, 3.2);
    }
    b.box('trimDark', 0, h - 0.12, z, (x1 - x0) - 0.5, 0.24, 0.3);
  }
  // overhead services
  b.at(0, 0, room.cz, PI / 2, (bb) => cableTray(bb, -len / 2 + 0.4, len / 2 - 0.4, h - 0.62, 0.9));
  b.cyl('metal', -1.05, h - 0.32, room.cz, 0.09, len - 0.6, { axis: 'z', seg: 8 });
  b.cyl('copper', -0.78, h - 0.3, room.cz, 0.05, len - 0.6, { axis: 'z', seg: 8 });
  // deck: grated centre lane with chasing guide lights
  b.box('grate', 0, 0.006, room.cz, 1.5, 0.012, len - 1.9, { fine: true });
  for (const s of [-1, 1]) {
    b.box('trimDark', s * 0.8, 0.01, room.cz, 0.1, 0.02, len - 1.9, { fine: false });
    const seg = Math.round((len - 2) / 0.8);
    for (let i = 0; i < seg; i++) {
      const z = z0 + 1.2 + i * ((len - 2.4) / (seg - 1));
      b.gbox({ hex: accent, i: 2.6, type: 0, fx: 4, ph: 0 }, s * 0.8, 0.022, z, 0.05, 0.006, 0.4);
    }
  }
  // hand rails between arches
  for (const s of [-1, 1]) {
    const runs = [];
    let start = z0 + 0.7;
    const blocks = sideDoors
      .filter((d) => (s < 0 ? Math.abs(d.x - x0) < 0.01 : Math.abs(d.x - x1) < 0.01))
      .map((d) => [d.z - d.w / 2 - 0.5, d.z + d.w / 2 + 0.5])
      .sort((p, q) => p[0] - q[0]);
    for (const [a, c] of blocks) {
      if (a - start > 1) runs.push([start, a]);
      start = c;
    }
    if (z1 - 0.7 - start > 1) runs.push([start, z1 - 0.7]);
    for (const [a, c] of runs) {
      b.cyl('chrome', s * (x1 - 0.25 - 0.1), 1.02, (a + c) / 2, 0.025, c - a, { axis: 'z', seg: 6 });
      const k = Math.max(1, Math.round((c - a) / 1.4));
      for (let i = 0; i <= k; i++) b.box('metalDark', s * (x1 - 0.25 - 0.05), 1.02, a + ((c - a) / k) * i, 0.1, 0.04, 0.04, { fine: false });
    }
  }
  if (o.sign) {
    b.sign(o.sign, 0, 0.014, room.cz + (o.signZ || 0), 0.3, { rx: -PI / 2, px: 96, tint: accent, i: 0.8, type: 3, spacing: 0.3 });
  }
}

export function buildSpineB(ctx) {
  const { b, room } = ctx;
  corridor(ctx, { sign: 'DECK 1  /  SECTION C', signZ: -1.5 });
  onWall(b, room, 'W', -3, (bb) => wallScreen(bb, ctx, 'warn', 1.75, 1.3, 0.5, { always: true, accent: 0xff5040 }));
  onWall(b, room, 'E', 0.5, (bb) => ventGrille(bb, 2.3, 1, 0.4));
  onWall(b, room, 'W', 4.4, (bb) => junctionBox(bb, 1.4));
  // fire point: the extinguisher lives here until the medbay needs it
  onWall(b, room, 'W', 7.2, (bb) => {
    extinguisherCabinet(bb);
    const ext = ctx.dyn(0, 0.92, 0.1, 0, (db) => extinguisher(db));
    ctx.sync(() => {
      ext.visible = !has('hasExt');
    });
    ctx.anchor('extinguisher', 0, 1.25, 0.3);
  });
  servicePanel(ctx, 1, 'E', -2.6);
  b.sign('ENGINEERING', 0, 0.014, 7.6, 0.24, { rx: -PI / 2, ry: PI, px: 96, tint: 0xffa040, i: 0.9, type: 3, spacing: 0.3, icon: 'up' });
  b.sign('CENTRAL HUB', 0, 0.014, -3.9, 0.24, { rx: -PI / 2, px: 96, tint: 0x58e0d0, i: 0.9, type: 3, spacing: 0.3, icon: 'up' });
}

export function buildSpineA(ctx) {
  const { b, room } = ctx;
  corridor(ctx, { sign: 'DECK 1  /  SECTION A', signZ: 2.2 });
  onWall(b, room, 'E', -32.5, (bb) => wallScreen(bb, ctx, 'log', 1.75, 1.2, 0.75, { always: false }));
  onWall(b, room, 'W', -25, (bb) => ventGrille(bb, 2.3, 1, 0.4));
  onWall(b, room, 'E', -23.6, (bb) => junctionBox(bb, 1.4));
  servicePanel(ctx, 2, 'W', -34.2);
  b.sign('BRIDGE', 0, 0.014, -33.6, 0.26, { rx: -PI / 2, px: 96, tint: 0x58b8ff, i: 0.9, type: 3, spacing: 0.3, icon: 'up' });
  logPad(ctx, 'corridor', 1.45, 0.012, -24.4, 0.9);
}
