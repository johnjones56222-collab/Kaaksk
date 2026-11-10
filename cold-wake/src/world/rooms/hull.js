// Dorsal hull - the EVA route. A railed catwalk runs from the airlock hatch
// to the comms dish, past three breached plates. Lit by the real sun.
import * as THREE from 'three';
import { G } from '../../engine/builder.js';
import { HULL_ORIGIN } from '../layout.js';
import { railing, consoleDesk } from '../props.js';
import { FX } from '../../engine/particles.js';
import { has } from '../../core/state.js';
import { rng } from '../../core/utils.js';
import { PI } from './common.js';

export function buildHull(ctx) {
  const { b } = ctx;
  const X = HULL_ORIGIN.x;
  ctx.setAmbient([0.035, 0.04, 0.055]);
  b.segLen = 6;
  // ---- hull body ----
  b.box('hull', X, -0.6, -25, 44, 1.2, 170, { uv: 'world' });
  for (const s of [-1, 1]) {
    b.box('hull', X + s * 27, -3.4, -25, 12, 1.2, 170, { rz: s * -0.5, uv: 'world' });
    b.box('hullDark', X + s * 33.5, -10, -25, 3, 12, 170, { uv: 'world' });
    // long spine conduits
    b.cyl('hullDark', X + s * 9.5, 0.4, -25, 0.5, 150, { axis: 'z', seg: 10 });
    b.cyl('metalDark', X + s * 10.6, 0.25, -25, 0.25, 150, { axis: 'z', seg: 8 });
    b.box('hullDark', X + s * 15, 0.5, -25, 3, 1.0, 120, { uv: 'world' });
  }
  b.segLen = 1.5;
  // ---- catwalk ----
  b.box('grate', X, 0.03, -15, 2.6, 0.06, 70, { fine: true });
  b.box('trimDark', X - 1.36, 0.06, -15, 0.12, 0.12, 70);
  b.box('trimDark', X + 1.36, 0.06, -15, 0.12, 0.12, 70);
  for (let z = 18; z >= -48; z -= 4) {
    b.gbox(G(0x5fe3ff, 2.6, 2, 4), X - 1.36, 0.13, z, 0.06, 0.02, 0.5);
    b.gbox(G(0x5fe3ff, 2.6, 2, 4), X + 1.36, 0.13, z, 0.06, 0.02, 0.5);
    if (z % 8 === 2) b.light(X, 1.2, z, 0x5fe3ff, 0.55, 5.5);
  }
  // rails: local X of the helper runs along world -Z after a quarter turn
  const rail = (x, zA, zB) => b.at(x, 0, (zA + zB) / 2, PI / 2, (bb) => railing(bb, -Math.abs(zA - zB) / 2, Math.abs(zA - zB) / 2, 0, { mat: 'yellow' }));
  rail(X - 1.4, 20, 12);
  rail(X - 1.4, 8, -26);
  rail(X - 1.4, -30, -50);
  rail(X + 1.4, 20, -8);
  rail(X + 1.4, -12, -50);
  // tether line
  b.cyl('orange', X + 1.15, 1.25, -15, 0.012, 70, { axis: 'z', seg: 5 });
  for (let z = 20; z >= -50; z -= 10) b.cyl('metalDark', X + 1.15, 0.65, z, 0.025, 1.2, { seg: 6 });

  // ---- side platforms at the breaches ----
  const sites = [
    { n: 1, s: -1, z: 10 },
    { n: 2, s: 1, z: -10 },
    { n: 3, s: -1, z: -28 },
  ];
  for (const { n, s, z } of sites) {
    const px = X + s * 2.9;
    b.box('grate', px, 0.03, z, 3.2, 0.06, 4.0, { fine: true });
    b.box('trimDark', px, -0.02, z, 3.3, 0.06, 4.1);
    b.at(px, 0, z - 2.0, 0, (bb) => railing(bb, -1.6, 1.6, 0, { mat: 'yellow' }));
    b.at(px, 0, z + 2.0, 0, (bb) => railing(bb, -1.6, 1.6, 0, { mat: 'yellow' }));
    b.at(X + s * 4.5, 0, z, PI / 2, (bb) => railing(bb, -2.0, 2.0, 0, { mat: 'yellow' }));
    // work light on a post
    b.cyl('metalDark', X + s * 4.3, 1.4, z + 1.8, 0.04, 2.8, { seg: 6 });
    b.box('trimDark', X + s * 4.3, 2.85, z + 1.6, 0.3, 0.14, 0.5, { fine: false });
    b.gbox(G(0xfff2d8, 3.4, 2), X + s * 4.3, 2.77, z + 1.6, 0.24, 0.02, 0.4);
    b.light(X + s * 4.6, 2.2, z + 0.6, 0xfff2d8, 1.1, 6.5);
    // the wound in the plating, just beyond the rail
    const bx = X + s * 6.0;
    b.cyl('black', bx, 0.012, z, 1.2, 0.02, { seg: 12 });
    const r = rng(n * 77);
    for (let i = 0; i < 9; i++) {
      const a = (i / 9) * PI * 2;
      const d = 1.0 + r() * 0.5;
      b.box('hullDark', bx + Math.cos(a) * d, 0.18 + r() * 0.2, z + Math.sin(a) * d, 0.9, 0.06, 0.6, { ry: -a + r(), rz: 0.4 + r() * 0.5, rx: r() - 0.5 });
      b.gbox(G(0xff7a2a, 2.6, 2, 1), bx + Math.cos(a) * (d - 0.45), 0.08, z + Math.sin(a) * (d - 0.45), 0.5, 0.03, 0.04, { ry: -a + PI / 2 });
    }
    const gas = ctx.emitter({ rate: 26, make: FX.vent(), dx: -s * 0.25, dy: 1, dz: 0.1 }, bx, 0.2, z);
    const sparks = ctx.emitter({ rate: 5, make: FX.sparks(1.2) }, bx + 0.4, 0.3, z - 0.3);
    const patch = ctx.dyn(bx, 0.24, z, 0.3 * n, (db) => {
      db.box('trim', 0, 0, 0, 2.8, 0.08, 2.8, { fine: false });
      db.box('metalDark', 0, 0.05, 0, 2.2, 0.03, 2.2, { fine: false });
      for (const [ex, ez, w, d] of [[0, 1.4, 2.8, 0.05], [0, -1.4, 2.8, 0.05], [1.4, 0, 0.05, 2.8], [-1.4, 0, 0.05, 2.8]]) db.gbox(G(0xffb060, 2.2, 2, 5), ex, 0.045, ez, w, 0.012, d);
      for (const [cx, cz] of [[-1.2, -1.2], [1.2, -1.2], [-1.2, 1.2], [1.2, 1.2]]) db.cyl('chrome', cx, 0.06, cz, 0.06, 0.04, { seg: 6 });
    });
    ctx.sync(() => {
      const sealed = has(`breach${n}`);
      patch.visible = sealed;
      gas.on = !sealed;
      sparks.on = !sealed;
    });
    ctx.world.anchors[`breach${n}`] = new THREE.Vector3(bx - s * 0.6, 0.5, z);
    ctx.world.anchors[`breachStand${n}`] = new THREE.Vector3(px + s * 0.6, 0, z);
    b.sign(`HULL BREACH ${n}`, px - s * 1.55, 0.6, z - 1.94, 0.12, { px: 56, tint: 0xff9a70, i: 1.6, type: 2, icon: 'warn' });
  }

  // ---- airlock hatch housing at the stern end ----
  b.cyl('hull', X, 0.7, 22.6, 1.9, 1.4, { seg: 20, su: 4, sv: 1 });
  b.cyl('trimDark', X, 1.45, 22.6, 1.5, 0.12, { seg: 20 });
  b.cyl('hazard', X, 1.52, 22.6, 1.2, 0.03, { seg: 20 });
  b.cyl('metalDark', X, 1.0, 20.75, 0.8, 0.14, { axis: 'z', seg: 20 });
  b.tor('yellow', X, 1.0, 20.67, 0.82, 0.05, { seg: 24 });
  b.gtor(G(0x6dffb0, 2.8, 2, 2), X, 1.0, 20.66, 0.66, 0.02, { seg: 28 });
  b.tor('chrome', X, 1.0, 20.62, 0.26, 0.03, { seg: 16 });
  b.sign('AIRLOCK 2', X, 1.95, 20.72, 0.16, { ry: PI, px: 64, tint: 0xdff6ff, i: 1.6, type: 2, spacing: 0.3 });
  b.cyl('metalDark', X - 1.1, 1.9, 21.6, 0.05, 1.0, { seg: 6 });
  b.gcyl(G(0xff4030, 4, 2, 3), X - 1.1, 2.45, 21.6, 0.08, 0.14, { seg: 8 });
  b.light(X, 1.6, 19.4, 0x6dffb0, 0.9, 6);
  b.solid(X - 2.4, 20.6, X + 2.4, 25);
  ctx.anchor('hatch', X, 1.1, 20.5);
  ctx.anchor('evaSpawn', X, 0, 19.0);

  // ---- comms dish platform at the bow end ----
  b.box('grate', X, 0.03, -53.5, 7.2, 0.06, 7.0, { fine: true });
  b.box('trimDark', X, -0.02, -53.5, 7.3, 0.06, 7.1);
  b.at(X, 0, -57, 0, (bb) => railing(bb, -3.6, 3.6, 0, { mat: 'yellow' }));
  b.at(X - 3.6, 0, -53.5, PI / 2, (bb) => railing(bb, -3.5, 3.5, 0, { mat: 'yellow' }));
  b.at(X + 3.6, 0, -53.5, PI / 2, (bb) => railing(bb, -3.5, 3.5, 0, { mat: 'yellow' }));
  b.at(X - 2.5, 0, -50, 0, (bb) => railing(bb, -1.1, 1.1, 0, { mat: 'yellow' }));
  b.at(X + 2.5, 0, -50, 0, (bb) => railing(bb, -1.1, 1.1, 0, { mat: 'yellow' }));
  // pedestal and yoke
  const dz = -55.2;
  b.cyl('hullDark', X, 0.6, dz, 0.9, 1.2, { seg: 16, top: 0.75 });
  b.cyl('metalDark', X, 1.5, dz, 0.45, 0.7, { seg: 12 });
  b.gtor(G(0x5fe3ff, 2.4, 2), X, 1.22, dz, 0.72, 0.02, { axis: 'y', seg: 28 });
  b.solidBox(X, dz, 1.8, 1.8);
  const dish = ctx.dyn(X, 2.0, dz, 0, (db) => {
    const profile = [];
    for (let i = 0; i <= 10; i++) {
      const r = (i / 10) * 2.7;
      profile.push(new THREE.Vector2(r, r * r * 0.11));
    }
    const lathe = new THREE.LatheGeometry(profile, 28);
    db.geo('white', lathe, 0, 0.5, 0, { su: 4, sv: 2 });
    db.geo('hullDark', lathe, 0, 0.44, 0, { su: 4, sv: 2, sx: 1.02, sz: 1.02 });
    lathe.dispose();
    db.cyl('metalDark', 0, 0.3, 0, 0.3, 0.6, { seg: 10 });
    for (let i = 0; i < 3; i++) {
      const a = (i / 3) * PI * 2;
      db.tube('chrome', [[Math.cos(a) * 2.2, 1.05, Math.sin(a) * 2.2], [0, 2.3, 0]], 0.03, { seg: 5 });
    }
    db.cyl('metalDark', 0, 2.3, 0, 0.16, 0.3, { seg: 10 });
    db.gsph(G(0x6dffb0, 3, 2, 2), 0, 2.1, 0, 0.07);
  });
  dish.rotation.set(0.95, 0.9, 0.25);
  ctx.ref('dish', dish);
  ctx.tick((dt) => {
    // misaligned: hunting back and forth. aligned: steady on the relay.
    const k = Math.min(1, dt * 1.2);
    if (has('antennaAligned')) {
      dish.rotation.x += (-0.55 - dish.rotation.x) * k;
      dish.rotation.y += (-0.5 - dish.rotation.y) * k;
      dish.rotation.z += (0 - dish.rotation.z) * k;
    } else {
      dish.rotation.y += dt * 0.12;
    }
  });
  b.at(X + 2.2, 0, -51.6, PI, (bb) => {
    consoleDesk(bb, ctx, { w: 1.5, screen: 'comms', accent: 0x6dffb0, always: true });
    ctx.anchor('dish', 0, 1.2, 0.6);
  });
  b.light(X, 2.2, -52.5, 0xfff2d8, 1.0, 7);
  b.sign('LONG RANGE ARRAY', X, 0.07, -50.9, 0.2, { rx: -PI / 2, px: 64, tint: 0xdff6ff, i: 1.2, type: 2, spacing: 0.3 });

  // ---- point-defence turret ahead of the dish ----
  b.cyl('hullDark', X, 0.7, -68, 2.6, 1.4, { seg: 20, top: 0.8 });
  b.sph('hull', X, 1.6, -68, 1.7, { seg: 18, sy: 0.75 });
  for (const s of [-1, 1]) {
    b.cyl('metalDark', X + s * 0.6, 1.9, -70.8, 0.2, 4.2, { axis: 'z', seg: 10 });
    b.cyl('trimDark', X + s * 0.6, 1.9, -72.6, 0.28, 0.6, { axis: 'z', seg: 10 });
    b.gcyl(G(0xff7a4a, 2.4, 2, 2), X + s * 0.6, 1.9, -72.92, 0.14, 0.02, { axis: 'z', seg: 10 });
  }
  ctx.anchor('turretCam', X, 4.2, -66);

  // ---- distant superstructure: bridge tower forward, engines aft ----
  b.segLen = 8;
  b.box('hull', X, 6, -98, 30, 12, 22, { uv: 'world' });
  b.box('hull', X, 15, -100, 18, 6, 14, { uv: 'world' });
  b.box('hullDark', X, 19, -101, 10, 2.4, 8, { uv: 'world' });
  for (let i = 0; i < 9; i++) b.gbox(G(0xffe2b0, 2.2, 2), X - 7 + i * 1.75, 15.6, -92.95, 1.2, 0.5, 0.06);
  for (let i = 0; i < 14; i++) if (i % 4 !== 1) b.gbox(G(0xffe2b0, 1.6, 2), X - 13 + i * 2, 7.5, -86.95, 1.1, 0.45, 0.06);
  b.cyl('metalDark', X + 4, 26, -101, 0.12, 12, { seg: 6 });
  b.gsph(G(0xff3020, 6, 2, 3), X + 4, 32.2, -101, 0.25);
  b.box('hullDark', X, 7, 52, 40, 16, 16, { uv: 'world' });
  for (const s of [-1, 0, 1]) {
    b.cyl('metalDark', X + s * 12, 7, 62, 5, 6, { axis: 'z', seg: 20, top: 0.7 });
    b.gcyl(G(0x66b8ff, 2.2, 2, 5), X + s * 12, 7, 65.05, 3.3, 0.1, { axis: 'z', seg: 20 });
  }
  b.box('hull', X - 17, 14, 50, 0.8, 14, 10, { rz: 0.3 });
  b.box('hull', X + 17, 14, 50, 0.8, 14, 10, { rz: -0.3 });
  // radiator fins glowing dull red at the root
  for (const s of [-1, 1]) {
    for (const z of [-8, -34]) {
      b.box('hullDark', X + s * 21, 4.5, z, 0.3, 10, 15, { rz: s * -0.45 });
      b.gbox(G(0xff5a2a, 1.3, 2, 2), X + s * 19.2, 0.9, z, 0.35, 0.5, 14);
    }
  }
  b.segLen = 1.5;
  // sensor mast
  b.cyl('metalDark', X + 7.5, 8, -38, 0.22, 16, { seg: 8 });
  for (let i = 0; i < 5; i++) b.box('metalDark', X + 7.5, 4 + i * 2.6, -38, 2.6 - i * 0.4, 0.08, 0.08, { ry: i * 0.6, fine: false });
  b.sph('white', X + 7.5, 16.4, -38, 0.6, { seg: 12 });
  b.gsph(G(0xff3020, 6, 2, 3), X + 7.5, 17.2, -38, 0.14);
  // observation dome and propellant spheres
  b.sph('glassTint', X - 9, 0, 2, 3.6, { seg: 20, half: true });
  b.tor('trimDark', X - 9, 0.1, 2, 3.6, 0.16, { axis: 'y', seg: 28 });
  b.gsph(G(0x8dff6a, 0.6, 2), X - 9, 0.4, 2, 1.2, { sy: 0.4 });
  for (let i = 0; i < 3; i++) {
    b.sph('white', X + 10.5, 2.2, -2 + i * 5, 2.2, { seg: 16 });
    b.cyl('trimDark', X + 10.5, 0.4, -2 + i * 5, 1.4, 0.8, { seg: 12 });
    b.tor('metalDark', X + 10.5, 2.2, -2 + i * 5, 2.22, 0.07, { axis: 'y', seg: 24 });
  }
  // greebles scattered over the plating
  const r = rng(2024);
  for (let i = 0; i < 150; i++) {
    const gx = X + (r() < 0.5 ? -1 : 1) * (3.2 + r() * 17);
    const gz = -78 + r() * 110;
    if (Math.abs(gx - (X - 9)) < 4.5 && Math.abs(gz - 2) < 4.5) continue;
    if (Math.abs(gx - (X + 10.5)) < 3.2 && gz > -5 && gz < 11) continue;
    if (sites.some((s) => Math.abs(gx - (X + s.s * 5)) < 3.2 && Math.abs(gz - s.z) < 3.2)) continue;
    const w = 0.4 + r() * 2.2;
    const d = 0.4 + r() * 2.2;
    const hh = 0.15 + r() * r() * 1.2;
    b.box(r() < 0.5 ? 'hullDark' : r() < 0.6 ? 'hull' : 'metalDark', gx, hh / 2, gz, w, hh, d, { fine: false, uv: 'world' });
    if (r() < 0.12) b.gbox(G(r() < 0.5 ? 0xffb347 : 0x5fe3ff, 3, 2, 3), gx, hh + 0.04, gz, 0.08, 0.08, 0.08);
  }
  // running lights
  b.gsph(G(0xff3020, 6, 2, 3), X - 21.5, 0.6, -60, 0.3);
  b.gsph(G(0x30ff60, 6, 2, 3), X + 21.5, 0.6, -60, 0.3);
  b.gsph(G(0xffffff, 6, 2, 3), X, 0.4, -105, 0.3);
  b.sign('ISV ASTRAEA', X - 6.2, 0.02, -18, 2.2, { rx: -PI / 2, ry: PI / 2, px: 128, tint: 0x20262e, i: 1.0, type: 2, weight: 800, spacing: 0.2 });
  b.sign('UCS 7-1142', X + 6.0, 0.02, -30, 1.1, { rx: -PI / 2, ry: -PI / 2, px: 128, tint: 0x20262e, i: 1.0, type: 2, weight: 800, spacing: 0.2 });

  // ---- keep the walker on the route ----
  b.solid(X - 40, -80, X - 4.7, 30);
  b.solid(X + 4.7, -80, X + 40, 30);
  b.solid(X - 40, -80, X + 40, -57.2);
  b.solid(X - 40, 24, X + 40, 30);
  // the gaps beside the catwalk between platforms
  const strip = (s, zA, zB) => b.solid(X + s * 1.46, Math.min(zA, zB), X + s * 4.7, Math.max(zA, zB));
  strip(-1, 20.6, 12.05);
  strip(-1, 7.95, -25.95);
  strip(-1, -30.05, -49.9);
  strip(1, 20.6, -7.95);
  strip(1, -12.05, -49.9);
  b.solid(X - 4.7, -50.05, X - 3.66, -49.9);
  b.solid(X + 3.66, -50.05, X + 4.7, -49.9);
}
