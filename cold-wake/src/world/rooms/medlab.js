// Med Bay and Science Lab - the two clean rooms flanking the hub.
import * as THREE from 'three';
import { G } from '../../engine/builder.js';
import { onWall } from '../shell.js';
import { medBed, consoleDesk, wallScreen, tank, fusionCell, ventGrille, table } from '../props.js';
import { FX } from '../../engine/particles.js';
import { has } from '../../core/state.js';
import { logPad, memento, PI } from './common.js';

export function buildMedbay(ctx) {
  const { b, room } = ctx;
  const { z0, z1, h } = room;
  const accent = room.color;
  ctx.setAmbient([0.05, 0.06, 0.06]);
  ctx.shell({
    wall: 'wallLight',
    floor: 'floorLight',
    windows: [{ side: 'W', a: -17, b: -11, y0: 1.05, y1: 3.0 }],
    ceilLights: { spacing: 4.2, intensity: 0.85, color: 0xeafff6 },
    ribSkip: (side) => side === 'N',
  });
  // three diagnostic beds along the bow wall, with privacy fins
  [-18.4, -15.0, -11.6].forEach((x, i) => {
    b.at(x, 0, z0 + 0.25 + 0.6, 0, (bb) => medBed(bb, ctx, { accent }));
    if (i < 2) {
      b.box('white', x + 1.7, 1.1, z0 + 0.95, 0.06, 2.2, 1.4);
      b.gbox(G(accent, 1.4, 0), x + 1.7, 2.22, z0 + 0.95, 0.07, 0.02, 1.3);
    }
    b.sign(`BED ${i + 1}`, x, 2.75, z0 + 0.27, 0.11, { px: 56, tint: accent, i: 1.4, type: 0, spacing: 0.3, icon: 'cross' });
  });
  // ceiling surgical arm over the middle of the room
  b.cyl('trimDark', -15, h - 0.12, -14.4, 0.4, 0.24, { seg: 14 });
  b.cyl('white', -15, h - 0.7, -14.4, 0.09, 1.0, { seg: 8 });
  b.sph('white', -15, h - 1.2, -14.4, 0.14, { seg: 10 });
  b.box('white', -14.55, h - 1.45, -14.4, 1.0, 0.12, 0.12, { rz: -0.5 });
  b.sph('white', -14.1, h - 1.7, -14.4, 0.11, { seg: 10 });
  b.box('white', -14.05, h - 2.05, -14.2, 0.1, 0.7, 0.1, { rx: 0.4 });
  b.gsph(G(accent, 3, 0), -14.05, h - 2.4, -14.05, 0.05);
  b.cyl('trimDark', -15, h - 0.3, -14.4, 0.6, 0.06, { seg: 20 });
  b.gtor(G(0xffffff, 2.6, 0), -15, h - 0.34, -14.4, 0.52, 0.03, { axis: 'y', seg: 28 });

  // pharmacy wall
  const sz = z1 - 0.25;
  for (let i = 0; i < 3; i++) {
    const x = -19.6 + i * 1.25;
    b.box('white', x, 1.1, sz - 0.25, 1.15, 2.2, 0.5);
    b.box('black', x, 1.45, sz - 0.505, 0.95, 1.2, 0.01, { fine: false });
    for (let r = 0; r < 4; r++) {
      b.box('metal', x, 1.0 + r * 0.3, sz - 0.4, 0.95, 0.015, 0.2, { fine: false });
      for (let c = 0; c < 6; c++) {
        const col = [0xff5a7a, 0x5affc0, 0x5a9bff, 0xffd24a][(r * 5 + c * 3 + i) % 4];
        if ((r + c + i) % 4 === 0) continue;
        b.gcyl(G(col, 0.9, 0), x - 0.38 + c * 0.15, 1.08 + r * 0.3, sz - 0.42, 0.035, 0.13, { seg: 6 });
      }
    }
    b.gbox(G(accent, 1.6, 0), x, 2.12, sz - 0.5, 0.95, 0.02, 0.02);
  }
  b.solid(-20.3, sz - 0.55, -16.4, sz);
  // compound synthesiser
  b.at(-14, 0, sz - 0.75, PI, (bb) => {
    consoleDesk(bb, ctx, { w: 1.8, screen: 'med', accent });
    ctx.anchor('synth', 0, 1.2, 0.6);
  });
  // scrub station
  b.box('white', -10.4, 0.45, sz - 0.3, 1.5, 0.9, 0.6);
  b.box('metal', -10.4, 0.92, sz - 0.3, 1.56, 0.04, 0.64, { fine: false });
  b.tube('chrome', [[-10.4, 0.94, sz - 0.12], [-10.4, 1.3, sz - 0.12], [-10.4, 1.3, sz - 0.34]], 0.02, { seg: 6 });
  b.solid(-11.2, sz - 0.62, -9.6, sz);
  onWall(b, room, 'E', -17.4, (bb) => wallScreen(bb, ctx, 'vitals', 1.8, 1.5, 0.95, { accent }));
  onWall(b, room, 'E', -10.2, (bb) => ventGrille(bb, 2.6, 1.1, 0.5));
  onWall(b, room, 'W', -9.6, (bb) => {
    bb.sign('MEDICAL', 0, 2.2, 0.02, 0.2, { px: 72, tint: accent, i: 1.6, type: 0, spacing: 0.35, icon: 'cross' });
  });

  // ---- fires: four seats that burn once the reactor surge hits ----
  const fires = [[-17.2, -13.0], [-12.2, -16.6], [-19.3, -16.4], [-10.6, -11.0]];
  fires.forEach(([fx, fz], i) => {
    const n = i + 1;
    // scorched debris
    b.cyl('black', fx, 0.008, fz, 1.0, 0.016, { seg: 16 });
    b.box('metalDark', fx + 0.2, 0.2, fz - 0.1, 0.7, 0.4, 0.5, { ry: 0.5 + i, rz: 0.2 });
    b.box('black', fx - 0.3, 0.1, fz + 0.2, 0.5, 0.2, 0.35, { ry: 1.1 * i });
    b.box('white', fx - 0.1, 0.34, fz - 0.2, 0.5, 0.06, 0.4, { ry: 0.3 * i, rx: 0.3 });
    const em = ctx.emitter({ rate: 46, make: FX.fire(0.42), scale: 1 }, fx, 0.15, fz);
    em.on = false;
    ctx.world.anchors[`fire${n}`] = new THREE.Vector3(fx, 0.7, fz);
    ctx.ref(`fire${n}`, em);
    b.solidBox(fx, fz, 0.9, 0.9);
    ctx.sync(() => {
      const burning = has('fireStarted') && !has(`fire${n}`);
      em.on = burning;
      if (burning && em.scale <= 0) em.scale = 1;
    });
  });

  // small desk with the doctor's notes
  b.at(-9.4, 0, -18.6, -PI / 2, (bb) => table(bb, 1.4, 0.7, { top: 'white' }));
  logPad(ctx, 'med', -9.4, 0.815, -18.5, 1.2, 0x7dffc0);
  // Raman's stethoscope on bed 3
  memento(ctx, 5, -11.3, 0.81, z0 + 0.9, (db) => {
    db.tor('black', 0, 0.01, 0, 0.1, 0.008, { axis: 'y', seg: 14 });
    db.cyl('chrome', 0.1, 0.012, 0, 0.03, 0.02, { seg: 10 });
    db.tube('black', [[-0.1, 0.01, 0], [-0.16, 0.01, 0.1], [-0.12, 0.01, 0.18]], 0.006, { seg: 4 });
  });
  b.sign('MED BAY', -14.5, 0.012, -11.2, 0.26, { rx: -PI / 2, ry: PI, px: 96, tint: 0x2f8a68, i: 1.0, type: 3, spacing: 0.3 });
}

export function buildLab(ctx) {
  const { b, room } = ctx;
  const { z0, z1, h } = room;
  const accent = room.color;
  ctx.setAmbient([0.05, 0.05, 0.065]);
  ctx.shell({
    wall: 'wallLight',
    floor: 'floorLight',
    windows: [{ side: 'E', a: -17, b: -11, y0: 1.05, y1: 3.0 }],
    ceilLights: { spacing: 4.2, intensity: 0.8, color: 0xf0eaff },
    ribSkip: (side) => side === 'N',
  });
  // specimen tanks along the bow wall
  const cols = [0x7dffc0, 0xb388ff, 0xffb347, 0x5fe3ff, 0xff7ad8];
  [10.2, 12.4, 14.6, 16.8, 19.0].forEach((x, i) => tank(b, x, z0 + 0.25 + 0.62, { color: cols[i], shape: i % 3 }));
  b.sign('XENOBIOLOGY  /  SAMPLE ARCHIVE', 14.6, 3.25, z0 + 0.27, 0.14, { px: 64, tint: accent, i: 1.6, type: 0, spacing: 0.3, icon: 'atom' });

  // containment plinth with the Lattice fragment
  const px = 14.8;
  const pz = -14;
  b.cyl('trimDark', px, 0.45, pz, 0.95, 0.9, { seg: 20, top: 0.85 });
  b.cyl('metalDark', px, 0.93, pz, 0.86, 0.06, { seg: 20 });
  b.gtor(G(accent, 2.6, 0), px, 0.97, pz, 0.72, 0.02, { axis: 'y', seg: 36 });
  b.cyl('glass', px, 1.75, pz, 0.7, 1.55, { seg: 20, open: true });
  b.cyl('trimDark', px, 2.62, pz, 0.82, 0.2, { seg: 20 });
  b.cyl('metalDark', px, (2.72 + h) / 2, pz, 0.22, h - 2.72, { seg: 10 });
  b.gtor(G(accent, 2.2, 0, 2), px, 2.5, pz, 0.66, 0.016, { axis: 'y', seg: 32 });
  b.light(px, 1.8, pz, accent, 1.0, 6, { key: true });
  b.solidBox(px, pz, 1.6, 1.6);
  const shard = new THREE.Group();
  shard.position.set(px, 1.75, pz);
  const dark = new THREE.MeshStandardMaterial({ color: 0x15121e, roughness: 0.25, metalness: 0.95 });
  const glow = new THREE.MeshBasicMaterial({ color: new THREE.Color(1.6, 0.5, 3.2) });
  const body = new THREE.Mesh(new THREE.OctahedronGeometry(0.24, 0), dark);
  body.scale.set(0.7, 1.7, 0.7);
  const veins = new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.OctahedronGeometry(0.245, 0)), new THREE.LineBasicMaterial({ color: new THREE.Color(2.4, 0.8, 4.5) }));
  veins.scale.copy(body.scale);
  const heart = new THREE.Mesh(new THREE.OctahedronGeometry(0.06, 0), glow);
  shard.add(body, veins, heart);
  ctx.add(shard);
  ctx.emitter({ rate: 6, make: FX.motes(0.4, 0.6, 0.4, [0.9, 0.4, 1.6]) }, px, 1.75, pz);
  ctx.tick((dt, t) => {
    shard.rotation.y += dt * 0.5;
    shard.position.y = 1.75 + Math.sin(t * 1.1) * 0.07;
    const p = 0.6 + 0.4 * Math.pow(0.5 + 0.5 * Math.sin(t * 1.9), 3);
    glow.color.setRGB(1.6 * p, 0.5 * p, 3.2 * p);
    heart.scale.setScalar(0.8 + p * 0.6);
  });

  // spectrometer console facing the fragment
  b.at(px, 0, -11.5, 0, (bb) => {
    consoleDesk(bb, ctx, { w: 1.9, screen: 'wave', accent });
    ctx.anchor('spectro', 0, 1.2, 0.6);
  });
  // bench with microscope
  const sz = z1 - 0.25;
  b.box('white', 10.6, 0.45, sz - 0.36, 3.6, 0.9, 0.7);
  b.box('metal', 10.6, 0.92, sz - 0.36, 3.7, 0.04, 0.76);
  b.box('black', 9.6, 1.0, sz - 0.4, 0.3, 0.12, 0.34, { fine: false });
  b.cyl('white', 9.6, 1.22, sz - 0.48, 0.05, 0.4, { seg: 8, rx: 0.3 });
  b.cyl('black', 9.6, 1.42, sz - 0.36, 0.045, 0.16, { seg: 8, rx: 0.9 });
  b.gcyl(G(accent, 2.2, 0), 9.6, 1.07, sz - 0.34, 0.05, 0.01, { seg: 10 });
  for (let i = 0; i < 5; i++) b.gcyl(G(cols[i], 1.0, 0), 11.2 + i * 0.16, 1.03, sz - 0.5, 0.03, 0.16, { seg: 6 });
  b.box('metalDark', 11.52, 0.95, sz - 0.5, 0.8, 0.03, 0.12, { fine: false });
  b.solid(8.8, sz - 0.74, 12.45, sz);
  logPad(ctx, 'lab', 10.5, 0.945, sz - 0.3, -0.3, 0xb388ff);
  // containment locker holding a spare fusion cell
  b.at(19.4, 0, sz - 0.4, PI, (bb) => {
    bb.box('trimDark', 0, 1.1, 0, 1.0, 2.2, 0.72);
    bb.box('black', 0, 1.2, 0.33, 0.7, 1.0, 0.1, { fine: false });
    bb.cyl('glass', 0, 1.2, 0.36, 0.3, 0.9, { seg: 12, open: true });
    bb.gbox(G(0xffb347, 2, 0, 2), 0, 1.85, 0.365, 0.6, 0.03, 0.01);
    bb.sign('ENERGY STORAGE', 0, 2.02, 0.365, 0.07, { px: 48, tint: 0xffc060, i: 1.3, type: 2, icon: 'bolt' });
    const cell = ctx.dyn(0, 0.92, 0.36, 0, (db) => fusionCell(db));
    ctx.sync(() => {
      cell.visible = !has('cellLab');
    });
    bb.light(0, 1.3, 1.0, 0x66e8ff, 0.4, 3);
    bb.solidBox(0, 0, 1.05, 0.75);
    ctx.anchor('cellLab', 0, 1.2, 0.55);
  });
  // cold storage under the viewport
  onWall(b, room, 'E', -14, (bb) => {
    bb.box('white', 0, 0.42, 0.32, 5.6, 0.84, 0.6);
    bb.box('metal', 0, 0.86, 0.32, 5.7, 0.04, 0.66, { fine: false });
    for (let i = 0; i < 5; i++) {
      bb.box('chrome', -2.2 + i * 1.1, 0.6, 0.63, 0.5, 0.02, 0.02, { fine: false });
      bb.gbox(G(0x9fdcff, 1.6, 0), -2.55 + i * 1.1, 0.74, 0.625, 0.08, 0.03, 0.006);
    }
    bb.solid(-2.85, 0, 2.85, 0.66);
  });
  onWall(b, room, 'W', -18, (bb) => wallScreen(bb, ctx, 'wave', 1.8, 1.5, 0.95, { accent }));
  onWall(b, room, 'W', -10, (bb) => {
    bb.box('white', 0, 1.7, 0.02, 1.9, 1.2, 0.04, { fine: false });
    bb.sign('f = 1420.405 MHz  ->  REPEAT 7.3 s', 0, 2.05, 0.045, 0.085, { px: 44, tint: 0x3050a0, i: 1.0, type: 3 });
    bb.sign('NOT NATURAL.  WHO IS IT TALKING TO?', 0, 1.75, 0.045, 0.075, { px: 44, tint: 0xa03040, i: 1.0, type: 3 });
    bb.sign('ASK VEGA TO RUN PHASE MATCH  - T.L.', 0, 1.42, 0.045, 0.065, { px: 44, tint: 0x304050, i: 1.0, type: 3 });
  });
  b.sign('SCIENCE LAB', 14.5, 0.012, -9.9, 0.26, { rx: -PI / 2, ry: PI, px: 96, tint: 0x6a4fb0, i: 1.0, type: 3, spacing: 0.3 });
}
