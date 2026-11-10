// Cargo Bay with its atmosphere shield, the survey shuttle, and the airlock.
import * as THREE from 'three';
import { G } from '../../engine/builder.js';
import { onWall } from '../shell.js';
import { crate, barrel, shelving, wallScreen, consoleDesk, railing, ventGrille } from '../props.js';
import { fieldMaterial } from '../../engine/materials.js';
import { FX } from '../../engine/particles.js';
import { has } from '../../core/state.js';
import { logPad, servicePanel, PI } from './common.js';

export function buildCargo(ctx) {
  const { b, room } = ctx;
  const { x0, x1, z0, z1, h } = room;
  const accent = room.color;
  ctx.setAmbient([0.045, 0.045, 0.04]);
  ctx.shell({
    floor: 'tread',
    windows: [{ side: 'S', a: -8, b: 8, y0: 0.4, y1: 5.8, pane: 4, noGlass: true }],
    ceilLights: { spacing: 6.6, intensity: 1.25, color: 0xfff2d8 },
    lightRange: 11,
    ribs: 4.5,
    ribSkip: (side) => side === 'S',
  });
  // atmosphere shield across the hangar mouth
  const field = new THREE.Mesh(new THREE.PlaneGeometry(16, 5.4), fieldMaterial(0x4fd8ff));
  field.position.set(0, 3.1, z1 - 0.125);
  field.rotation.y = PI;
  ctx.add(field);
  ctx.tick(() => {
    field.material.uniforms.uPower.value = 0.25 + 0.75 * ctx.world.rooms[room.index].power;
  });
  for (let i = 0; i < 5; i++) {
    b.gbox(G(0x4fd8ff, 2.4, 0, 2), -8 + i * 4, 3.1, z1 - 0.3, 0.05, 5.2, 0.05);
    b.light(-6 + i * 3, 2.4, z1 - 1.6, 0x4fd8ff, 0.55, 6.5);
  }
  b.box('hazard', 0, 0.01, z1 - 1.3, 16, 0.02, 0.5, { fine: true });
  b.sign('ATMOSPHERE SHIELD  -  DO NOT CROSS', 0, 6.3, z1 - 0.27, 0.2, { ry: PI, px: 72, tint: 0xffc060, i: 1.5, type: 2, spacing: 0.3, icon: 'warn' });

  // ---- survey shuttle on its pad ----
  b.at(-4.2, 0, 41.5, 0, (bb) => {
    bb.box('hazard', 0, 0.012, 0, 5.4, 0.024, 9.2, { fine: true });
    bb.box('floor', 0, 0.016, 0, 4.9, 0.024, 8.7, { fine: true, uv: 'world' });
    // hull
    bb.sph('hull', 0, 1.55, 0.3, 1.25, { seg: 20, sz: 2.9, sy: 0.82, su: 3, sv: 2 });
    bb.box('hullDark', 0, 1.0, 0.2, 2.3, 0.5, 5.6);
    bb.sph('glassTint', 0, 1.95, 2.3, 0.86, { seg: 16, sz: 1.5, sy: 0.6 });
    // wings and tail
    for (const s of [-1, 1]) {
      bb.box('hull', s * 1.9, 1.2, -0.9, 2.2, 0.12, 2.6, { rz: s * -0.12, ry: s * -0.2 });
      bb.box('orange', s * 2.9, 1.34, -1.2, 0.2, 0.14, 2.2, { rz: s * -0.12, ry: s * -0.2, fine: false });
      bb.cyl('metalDark', s * 1.15, 1.3, -3.3, 0.42, 1.5, { axis: 'z', seg: 14 });
      bb.gcyl(G(0x66c8ff, 1.6, 0, 2), s * 1.15, 1.3, -4.06, 0.3, 0.03, { axis: 'z', seg: 14 });
      // skids
      bb.box('metalDark', s * 0.95, 0.22, 0.3, 0.14, 0.12, 4.6, { fine: false });
      bb.box('metalDark', s * 0.95, 0.5, 1.6, 0.1, 0.6, 0.1, { fine: false });
      bb.box('metalDark', s * 0.95, 0.5, -1.4, 0.1, 0.6, 0.1, { fine: false });
      bb.gbox(G(s < 0 ? 0xff4030 : 0x40ff70, 3, 0, 3), s * 3.0, 1.36, -0.2, 0.06, 0.06, 0.06);
    }
    bb.box('hull', 0, 2.5, -2.6, 0.12, 1.3, 1.6, { rx: -0.35 });
    bb.box('orange', 0, 3.0, -3.0, 0.14, 0.24, 1.0, { rx: -0.35, fine: false });
    bb.sign('ASTRAEA-2', 1.27, 1.55, 0.2, 0.28, { ry: PI / 2, px: 96, tint: 0x20262e, i: 1.0, type: 3, spacing: 0.2 });
    bb.sign('ASTRAEA-2', -1.27, 1.55, 0.2, 0.28, { ry: -PI / 2, px: 96, tint: 0x20262e, i: 1.0, type: 3, spacing: 0.2 });
    bb.gbox(G(0xffffff, 3.2, 0), 0, 1.15, 3.6, 0.5, 0.08, 0.06);
    bb.light(0, 1.2, 4.6, 0xdfefff, 0.6, 5);
    bb.solidBox(0, 0, 3.0, 7.6);
  });

  // ---- crates ----
  const stack = (x, z, ry, list) => {
    b.at(x, 0, z, ry, (bb) => {
      for (const [dx, dz, s, m, y] of list) crate(bb, dx, dz, s, m, { y: y || 0, noSolid: !!y });
    });
  };
  stack(-9.2, 33.2, 0.1, [[0, 0, 1.5, 'crateBlue'], [1.7, 0.1, 1.5, 'crate'], [0.2, 0.1, 1.2, 'crateOrange', 1.5], [0.1, 1.7, 1.2, 'crateDark'], [1.8, 0.2, 1.0, 'crateGreen', 1.5]]);
  stack(8.4, 33.4, -0.15, [[0, 0, 1.6, 'crateOrange'], [-1.8, 0.2, 1.4, 'crateDark'], [0, 0.1, 1.3, 'crate', 1.6], [0.2, 1.8, 1.2, 'crateRed'], [-1.7, 1.7, 1.0, 'crateBlue']]);
  stack(5.0, 46.6, 0.3, [[0, 0, 1.3, 'crateGreen'], [1.5, 0.2, 1.3, 'crate'], [0.6, 0.1, 1.0, 'crateOrange', 1.3]]);
  stack(9.6, 46.4, -0.2, [[0, 0, 1.2, 'crateDark']]);
  barrel(b, 10.2, 44.3, 'blue');
  barrel(b, 10.9, 44.8, 'orange');
  barrel(b, 10.5, 45.3, 'blue');
  onWall(b, room, 'W', 36.6, (bb) => bb.at(0, 0, 0.36, 0, (sb) => shelving(sb, { w: 3.2, h: 3.0, seed: 11 })));
  logPad(ctx, 'cargo', 5.05, 1.31, 46.55, 0.4, 0xffd24a);

  // ---- overhead crane ----
  b.box('trimDark', 0, 6.9, 36.5, 23.4, 0.4, 0.5);
  b.box('yellow', 0, 6.66, 36.5, 23.4, 0.08, 0.6);
  b.box('yellow', 4.0, 6.4, 36.5, 1.4, 0.5, 1.2);
  b.cyl('rubber', 4.0, 5.3, 36.5, 0.025, 1.8, { seg: 5 });
  b.box('metalDark', 4.0, 4.3, 36.5, 0.3, 0.3, 0.3, { fine: false });
  b.tor('metal', 4.0, 4.0, 36.5, 0.14, 0.035, { seg: 12, arc: PI * 1.5 });
  b.gbox(G(0xffb347, 3, 0, 3), 4.0, 6.1, 37.12, 0.1, 0.1, 0.02);
  // ---- loader bot ----
  b.at(4.2, 0, 41.2, -0.6, (bb) => {
    bb.box('yellow', 0, 0.7, 0, 1.1, 0.8, 1.5);
    bb.box('rubber', -0.62, 0.3, 0, 0.26, 0.6, 1.7);
    bb.box('rubber', 0.62, 0.3, 0, 0.26, 0.6, 1.7);
    bb.box('trimDark', 0, 1.3, -0.3, 0.8, 0.5, 0.7);
    bb.gbox(G(0x5fe3ff, 2.6, 0), 0, 1.36, 0.06, 0.5, 0.1, 0.01);
    bb.box('metalDark', -0.35, 0.9, 1.1, 0.1, 0.1, 1.0, { rx: -0.25, fine: false });
    bb.box('metalDark', 0.35, 0.9, 1.1, 0.1, 0.1, 1.0, { rx: -0.25, fine: false });
    bb.box('metal', 0, 0.76, 1.62, 1.0, 0.06, 0.5, { fine: false });
    bb.cyl('metalDark', 0.3, 1.66, -0.3, 0.05, 0.24, { seg: 8 });
    bb.gcyl(G(0xffb347, 3.2, 0, 3), 0.3, 1.82, -0.3, 0.07, 0.1, { seg: 8 });
    bb.solidBox(0, 0.1, 1.5, 2.0);
  });

  // ---- airlock approach: fuse box by the door ----
  onWall(b, room, 'E', 42.4, (bb) => {
    bb.box('gray', 0, 1.4, 0.08, 0.9, 1.1, 0.16, { fine: false });
    bb.box('black', 0, 1.4, 0.165, 0.78, 0.98, 0.01, { fine: false });
    bb.box('gray', -0.62, 1.4, 0.4, 0.04, 1.1, 0.7, { ry: 0.5, fine: false });
    const cols = ['red', 'yellow', 'blue', 'green', 'orange'];
    cols.forEach((c, i) => {
      bb.tube(c, [[-0.3, 1.75 - i * 0.16, 0.17], [-0.1 + (i % 2) * 0.1, 1.7 - i * 0.16, 0.26], [0.02, 1.62 - i * 0.14, 0.2]], 0.012, { seg: 5 });
      bb.cyl('chrome', 0.3, 1.75 - i * 0.16, 0.18, 0.02, 0.03, { axis: 'z', seg: 6 });
    });
    bb.gbox(G(0xff5040, 2.6, 2, 3), 0.3, 0.98, 0.172, 0.06, 0.04, 0.008);
    bb.sign('AIRLOCK 2 FUSE BOX', 0, 2.1, 0.02, 0.08, { px: 48, tint: 0xffc060, i: 1.3, type: 2, icon: 'bolt' });
    const em = ctx.emitter({ rate: 7, make: FX.sparks(0.6) }, 0, 1.5, 0.25);
    ctx.sync(() => {
      em.on = !has('airlockFuse');
    });
    ctx.anchor('fuse', 0, 1.4, 0.3);
  });
  onWall(b, room, 'E', 37.4, (bb) => wallScreen(bb, ctx, 'airlock', 1.8, 1.3, 0.82, { always: true, accent: 0xff7a4a }));
  onWall(b, room, 'N', 5.0, (bb) => wallScreen(bb, ctx, 'cargo', 1.9, 1.6, 1.0, { accent }));
  onWall(b, room, 'N', -5.0, (bb) => ventGrille(bb, 3.2, 1.6, 0.7));
  servicePanel(ctx, 6, 'W', 31.8);
  // deck markings
  b.sign('CARGO BAY 1', 0, 0.014, 32.4, 0.4, { rx: -PI / 2, px: 96, tint: accent, i: 0.9, type: 3, spacing: 0.35 });
  b.box('yellow', 2.4, 0.008, 39.5, 0.12, 0.016, 15, { fine: true });
  b.box('yellow', 11.0, 0.008, 40, 1.6, 0.016, 0.12, { fine: false });
  b.sign('AIRLOCK', 10.0, 0.014, 40.9, 0.22, { rx: -PI / 2, ry: -PI / 2, px: 72, tint: 0xff7a4a, i: 1.0, type: 3, icon: 'up' });
  void railing;
  void consoleDesk;
  void x0;
  void x1;
  void z0;
  void h;
}

export function buildAirlock(ctx) {
  const { b, room } = ctx;
  const { x1, z0, z1, h } = room;
  const accent = room.color;
  ctx.setAmbient([0.04, 0.04, 0.04]);
  ctx.shell({
    wall: 'wallDark',
    floor: 'tread',
    ribs: 0,
    beams: 0,
    windows: [{ side: 'E', a: 39.35, b: 40.65, y0: 1.15, y1: 2.25, pane: 9 }],
    ceilLights: { spacing: 3, intensity: 0.7, color: 0xfff0e0 },
    lightRange: 5.5,
    emergencyAt: [[15, 40]],
  });
  // outer hatch dressing around the porthole
  const hx = x1 - 0.25;
  b.tor('trimDark', hx - 0.06, 1.7, 40, 1.35, 0.14, { axis: 'x', seg: 28 });
  b.tor('hazard', hx - 0.03, 1.7, 40, 1.6, 0.07, { axis: 'x', seg: 28, su: 12 });
  for (let i = 0; i < 8; i++) {
    const a = (i / 8) * PI * 2;
    b.box('metalDark', hx - 0.12, 1.7 + Math.sin(a) * 1.35, 40 + Math.cos(a) * 1.35, 0.22, 0.22, 0.34, { rx: -a, fine: false });
  }
  b.gtor(G(0xff7a4a, 2.4, 2, 2), hx - 0.2, 1.7, 40, 1.15, 0.02, { axis: 'x', seg: 36 });
  b.sign('EXTERIOR  /  VACUUM', hx - 0.02, 3.0, 40, 0.14, { ry: -PI / 2, px: 64, tint: 0xff9a70, i: 1.5, type: 2, spacing: 0.3, icon: 'warn' });
  // suit alcoves on the bow wall
  [13.5, 15.0, 16.5].forEach((x, i) => {
    b.at(x, 0, z0 + 0.25 + 0.3, 0, (bb) => {
      bb.box('trimDark', -0.6, 1.15, 0, 0.08, 2.3, 0.6);
      bb.box('trimDark', 0.6, 1.15, 0, 0.08, 2.3, 0.6);
      bb.box('trimDark', 0, 2.34, 0, 1.28, 0.08, 0.6);
      bb.gbox(G(0xfff0e0, 2.0, 0), 0, 2.28, 0.1, 0.9, 0.02, 0.12);
      if (i < 2) {
        const suit = ctx.dyn(0, 0, 0.02, 0, (db) => {
          db.box('white', 0, 1.25, 0, 0.56, 0.66, 0.32);
          db.box('orange', 0, 1.42, 0.165, 0.3, 0.14, 0.01, { fine: false });
          db.box('gray', 0, 1.25, -0.2, 0.44, 0.6, 0.16, { fine: false });
          db.sph('white', 0, 1.78, 0, 0.21, { seg: 14 });
          db.sph('gold', 0, 1.78, 0.07, 0.17, { seg: 12, sz: 0.9 });
          db.box('white', -0.37, 1.2, 0, 0.16, 0.62, 0.18, { fine: false });
          db.box('white', 0.37, 1.2, 0, 0.16, 0.62, 0.18, { fine: false });
          db.box('white', -0.15, 0.5, 0, 0.21, 0.86, 0.22, { fine: false });
          db.box('white', 0.15, 0.5, 0, 0.21, 0.86, 0.22, { fine: false });
          db.box('gray', -0.15, 0.06, 0.04, 0.22, 0.12, 0.32, { fine: false });
          db.box('gray', 0.15, 0.06, 0.04, 0.22, 0.12, 0.32, { fine: false });
          db.gbox(G(0x6dffb0, 2.4, 2), 0.16, 1.44, 0.172, 0.05, 0.03, 0.004);
        });
        if (i === 0) {
          ctx.sync(() => {
            suit.visible = !has('hasSuit');
          });
          ctx.anchor('suit', 0, 1.3, 0.4);
        }
      }
    });
  });
  b.solid(12.8, z0 + 0.25, 17.2, z0 + 0.9);
  b.sign('EVA SUITS', 15, 2.75, z0 + 0.27, 0.12, { px: 56, tint: accent, i: 1.5, type: 2, spacing: 0.35 });
  // cycle console on the stern wall
  b.at(14.6, 0, z1 - 0.25 - 0.55, PI, (bb) => {
    consoleDesk(bb, ctx, { w: 1.5, screen: 'airlock', accent, always: true });
    ctx.anchor('cycle', 0, 1.2, 0.6);
  });
  // O2 recharge post
  b.at(16.9, 0, z1 - 0.6, PI, (bb) => {
    bb.cyl('white', 0, 0.7, 0, 0.2, 1.4, { seg: 12 });
    bb.cyl('teal', 0, 1.45, 0, 0.21, 0.1, { seg: 12 });
    bb.gbox(G(0x5fe3ff, 2.2, 2), 0, 1.0, 0.205, 0.1, 0.3, 0.008);
    bb.sign('O2', 0, 1.3, 0.21, 0.09, { px: 56, tint: 0x9fdcff, i: 1.4, type: 2 });
    bb.solidBox(0, 0, 0.45, 0.45);
  });
  // deck grating and vent emitters used during depressurisation
  b.box('grate', 15, 0.006, 40, 3.0, 0.012, 3.6, { fine: true });
  const vents = [
    ctx.emitter({ rate: 0, make: FX.vent(), dx: 0, dy: 1, dz: 0 }, 13.4, 0.1, 38.6),
    ctx.emitter({ rate: 0, make: FX.vent(), dx: 0, dy: 1, dz: 0 }, 16.6, 0.1, 41.4),
    ctx.emitter({ rate: 0, make: FX.vent(), dx: 0, dy: -1, dz: 0 }, 15, h - 0.1, 40),
  ];
  ctx.ref('airlockVents', vents);
  for (const [bx, bz] of [[12.6, 37.6], [17.4, 42.4]]) {
    b.cyl('metalDark', bx, h - 0.06, bz, 0.1, 0.12, { seg: 8 });
    b.gcyl(G(0xffb347, 4, 2, 3), bx, h - 0.2, bz, 0.07, 0.14, { seg: 8 });
  }
}
