// Crew Quarters and mess - bunks, galley, a lounge under the viewport, and
// Chief Vasquez's wall safe.
import { G } from '../../engine/builder.js';
import { onWall } from '../shell.js';
import { bunk, locker, table, stool, fusionCell, wallScreen, ventGrille } from '../props.js';
import { has } from '../../core/state.js';
import { logPad, memento, PI } from './common.js';

export function buildQuarters(ctx) {
  const { b, room } = ctx;
  const { z0, z1, x1 } = room;
  const accent = room.color;
  ctx.setAmbient([0.055, 0.05, 0.045]);
  ctx.shell({
    windows: [{ side: 'E', a: -33, b: -28, y0: 1.0, y1: 2.7 }],
    ceilLights: { spacing: 4.4, intensity: 0.8, color: 0xffe2c0 },
    ribSkip: (side, u) => side === 'N' || side === 'S' || (side === 'W' && u < -29),
  });
  // bunks along the bow wall, with dividers
  [3.42, 5.72, 8.02].forEach((x, i) => {
    b.at(x, 0, z0 + 0.25 + 0.5, 0, (bb) => bunk(bb, { sheet: i === 1 ? 'fabricRed' : 'fabricBlue' }));
    b.box('trimDark', x + 1.14, 1.3, z0 + 0.8, 0.08, 2.6, 1.1);
  });
  // wardrobe lockers
  ['MARLOW', 'OKONKWO', 'RAMAN', 'LINDQVIST'].forEach((n, i) => {
    b.at(9.85 + i * 0.68, 0, z0 + 0.25 + 0.27, 0, (bb) => locker(bb, { label: n, accent }));
  });
  b.solid(9.5, z0 + 0.25, 12.25, z0 + 0.82);

  // lounge under the viewport
  onWall(b, room, 'E', -30.5, (bb) => {
    bb.box('trimDark', 0, 0.2, 0.36, 4.4, 0.4, 0.7);
    bb.box('fabricRed', 0, 0.46, 0.38, 4.3, 0.12, 0.66);
    bb.box('fabricRed', 0, 0.74, 0.08, 4.3, 0.46, 0.14, { rx: 0.12 });
    bb.solid(-2.2, 0, 2.2, 0.75);
    bb.at(0, 0, 1.6, 0, (tb) => table(tb, 1.5, 0.7, { h: 0.45, top: 'trimDark' }));
  });
  // Lindqvist's bonsai on the coffee table
  memento(ctx, 4, x1 - 0.25 - 1.6, 0.48, -30.5, (db) => {
    db.box('white', 0, 0.04, 0, 0.22, 0.08, 0.14, { fine: false });
    db.cyl('soil', 0, 0.085, 0, 0.06, 0.02, { seg: 8 });
    db.tube('copper', [[0, 0.08, 0], [0.03, 0.2, 0.01], [-0.03, 0.3, 0]], 0.012, { seg: 5 });
    db.plane('leaf', -0.02, 0.33, 0, 0.26, 0.22, {});
    db.plane('leaf', -0.02, 0.33, 0, 0.26, 0.22, { ry: PI / 2 });
  });

  // mess table
  b.at(7.3, 0, -28.4, 0, (bb) => {
    table(bb, 2.2, 1.0, {});
    for (const [sx, sz] of [[-0.7, 0.8], [0.7, 0.8], [-0.7, -0.8], [0.7, -0.8]]) stool(bb, sx, sz);
    bb.cyl('white', 0.5, 0.86, 0.1, 0.05, 0.1, { seg: 10 });
    bb.box('fabric', -0.4, 0.815, -0.1, 0.34, 0.012, 0.24, { ry: 0.2, fine: false });
  });
  logPad(ctx, 'mess', 6.9, 0.815, -28.6, -0.4, 0xffb060);
  // Vasquez's mug
  memento(ctx, 3, 8.2, 0.81, -28.1, (db) => {
    db.cyl('orange', 0, 0.055, 0, 0.045, 0.11, { seg: 10 });
    db.tor('orange', 0.055, 0.055, 0, 0.03, 0.009, { seg: 10 });
    db.cyl('black', 0, 0.108, 0, 0.038, 0.006, { seg: 10 });
  });

  // galley along the stern wall
  const gz = z1 - 0.25 - 0.32;
  b.box('white', 9.2, 0.44, gz, 6.0, 0.88, 0.62);
  b.box('metal', 9.2, 0.9, gz, 6.1, 0.05, 0.68);
  b.box('black', 9.2, 0.08, gz + 0.02, 6.0, 0.16, 0.6, { fine: false });
  b.box('white', 9.2, 2.25, gz + 0.08, 6.0, 0.7, 0.46);
  b.gbox(G(0xffe2c0, 2.2, 0), 9.2, 1.89, gz - 0.02, 5.6, 0.02, 0.06);
  for (let i = 0; i < 5; i++) {
    b.box('trimDark', 6.8 + i * 1.2, 0.5, gz - 0.315, 0.02, 0.7, 0.01, { fine: false });
    b.box('chrome', 6.6 + i * 1.2, 0.72, gz - 0.33, 0.2, 0.02, 0.02, { fine: false });
    b.light(6.8 + i * 1.2, 1.5, gz - 0.7, 0xffe2c0, 0.3, 2.6);
  }
  // sink
  b.box('metalDark', 8.0, 0.92, gz, 0.7, 0.03, 0.44, { fine: false });
  b.tube('chrome', [[8.0, 0.92, gz + 0.2], [8.0, 1.25, gz + 0.2], [8.0, 1.25, gz + 0.02]], 0.02, { seg: 6 });
  // coffee machine
  b.at(10.6, 0.925, gz + 0.04, PI, (bb) => {
    bb.box('black', 0, 0.26, 0, 0.42, 0.52, 0.4);
    bb.box('chrome', 0, 0.5, 0.05, 0.44, 0.06, 0.34, { fine: false });
    bb.box('metalDark', 0, 0.03, 0.13, 0.36, 0.04, 0.26, { fine: false });
    bb.cyl('chrome', 0, 0.3, 0.16, 0.03, 0.08, { seg: 8 });
    bb.gbox(G(0xffb060, 2.4, 0, 2), -0.12, 0.42, 0.203, 0.08, 0.04, 0.006);
    bb.gbox(G(0x6dffb0, 2.4, 0), 0.1, 0.42, 0.203, 0.04, 0.04, 0.006);
    bb.cyl('white', 0, 0.1, 0.15, 0.045, 0.1, { seg: 10 });
    ctx.anchor('coffee', 0, 0.35, 0.35);
  });
  b.sign('GALLEY', 9.2, 2.72, z1 - 0.27, 0.14, { ry: PI, px: 64, tint: accent, i: 1.5, type: 0, spacing: 0.4 });
  // fridge
  b.box('gray', 12.35, 1.05, gz, 0.72, 2.1, 0.66);
  b.box('chrome', 12.08, 1.2, gz - 0.345, 0.03, 0.6, 0.03, { fine: false });
  b.gbox(G(0x9fdcff, 1.6, 0), 12.5, 1.8, gz - 0.335, 0.12, 0.05, 0.008);
  b.solid(6.2, z1 - 0.95, 12.75, z1 - 0.25);

  // jukebox
  b.at(4.3, 0, z1 - 0.25 - 0.36, PI, (bb) => {
    bb.box('trimDark', 0, 0.75, 0, 0.9, 1.5, 0.62);
    bb.cyl('trimDark', 0, 1.5, 0, 0.45, 0.62, { axis: 'z', seg: 18 });
    bb.gtor(G(0xff58d8, 2.6, 0, 2), 0, 1.5, 0.315, 0.38, 0.022, { seg: 28 });
    bb.gtor(G(0x58e0ff, 2.4, 0, 2), 0, 1.5, 0.315, 0.29, 0.018, { seg: 24 });
    bb.gtor(G(0xffc060, 2.2, 0, 2), 0, 1.5, 0.315, 0.2, 0.016, { seg: 20 });
    bb.box('black', 0, 0.8, 0.315, 0.66, 0.5, 0.01, { fine: false });
    for (let i = 0; i < 6; i++) bb.gbox(G([0xff58d8, 0x58e0ff, 0xffc060][i % 3], 2, 0, 3), -0.25 + i * 0.1, 0.9 - (i % 2) * 0.12, 0.322, 0.06, 0.06, 0.006);
    bb.box('fabricDark', 0, 0.3, 0.315, 0.7, 0.36, 0.01, { fine: false });
    bb.light(0, 1.4, 0.9, 0xff58d8, 0.5, 3.6);
    bb.solidBox(0, 0, 0.95, 0.66);
    ctx.anchor('jukebox', 0, 1.2, 0.5);
  });

  // ---- port wall: Vasquez's safe on a cabinet, posters ----
  onWall(b, room, 'W', -31.6, (bb) => {
    bb.box('gray', 0, 0.42, 0.3, 1.1, 0.84, 0.58);
    bb.box('trimDark', 0, 0.86, 0.3, 1.14, 0.04, 0.62, { fine: false });
    // safe body
    bb.box('metalDark', 0, 1.27, 0.07, 0.78, 0.78, 0.1, { fine: false });
    bb.box('metalDark', -0.36, 1.27, 0.28, 0.06, 0.78, 0.5, { fine: false });
    bb.box('metalDark', 0.36, 1.27, 0.28, 0.06, 0.78, 0.5, { fine: false });
    bb.box('metalDark', 0, 1.63, 0.28, 0.78, 0.06, 0.5, { fine: false });
    bb.box('metalDark', 0, 0.91, 0.28, 0.78, 0.06, 0.5, { fine: false });
    bb.gbox(G(0x66e8ff, 1.2, 2), 0, 1.585, 0.25, 0.5, 0.012, 0.3);
    const door = ctx.dyn(-0.33, 1.27, 0.53, 0, (db) => {
      db.push(0.33, 0, 0);
      db.box('trim', 0, 0, 0, 0.66, 0.66, 0.05, { fine: false });
      db.box('black', 0.14, 0.06, 0.03, 0.22, 0.3, 0.012, { fine: false });
      for (let r = 0; r < 4; r++) for (let c = 0; c < 3; c++) db.gbox(G(0xffb347, 1.8, 2), 0.07 + c * 0.07, 0.16 - r * 0.07, 0.038, 0.045, 0.045, 0.004);
      db.cyl('chrome', -0.14, 0, 0.05, 0.07, 0.04, { axis: 'z', seg: 12 });
      db.box('chrome', -0.14, 0, 0.08, 0.16, 0.025, 0.02, { fine: false });
      db.pop();
    });
    ctx.tick((dt) => {
      const target = has('safeOpen') ? -1.9 : 0;
      door.rotation.y += (target - door.rotation.y) * Math.min(1, dt * 4);
    });
    const cell = ctx.dyn(0, 0.95, 0.28, 0, (db) => fusionCell(db));
    ctx.sync(() => {
      cell.visible = !has('cellQuarters');
    });
    bb.sign('R. VASQUEZ / CHIEF ENG.', 0, 1.82, 0.02, 0.07, { px: 48, tint: 0xffc890, i: 1.0, type: 3 });
    bb.solid(-0.6, 0, 0.6, 0.62);
    ctx.anchor('safe', 0, 1.27, 0.7);
  });
  onWall(b, room, 'W', -33.9, (bb) => {
    bb.box('black', 0, 1.7, 0.012, 0.9, 1.25, 0.02, { fine: false });
    bb.sign('VISIT AURELIA', 0, 2.12, 0.026, 0.14, { px: 72, tint: 0xffc060, i: 1.3, type: 0, spacing: 0.2 });
    bb.gcyl(G(0xe8a850, 1.1, 0), 0, 1.62, 0.02, 0.26, 0.012, { axis: 'z', seg: 24 });
    bb.gtor(G(0xd8c8a0, 1.0, 0), 0, 1.62, 0.03, 0.4, 0.012, { seg: 32, rx: 1.2 });
    bb.sign('RINGS YOU CAN ALMOST TOUCH', 0, 1.2, 0.026, 0.055, { px: 40, tint: 0xdfefff, i: 1.0, type: 0 });
  });
  onWall(b, room, 'W', -24.9, (bb) => ventGrille(bb, 2.5, 0.9, 0.4));
  onWall(b, room, 'N', 12.6, (bb) => wallScreen(bb, ctx, 'log', 2.6, 0.5, 0.36, { noLight: true }));

  // Vasquez's log, left on her bunk
  logPad(ctx, 'vasquez', 8.3, 0.66, z0 + 0.8, 0.3, 0xffb060);
  b.sign('CREW QUARTERS', 7.5, 0.012, -25.9, 0.26, { rx: -PI / 2, ry: PI, px: 96, tint: accent, i: 0.7, type: 3, spacing: 0.3 });
}
