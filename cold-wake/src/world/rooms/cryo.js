// Cryo Bay - where the player wakes. Eight stasis capsules in two rows.
import { G } from '../../engine/builder.js';
import { onWall } from '../shell.js';
import { cryoPod, locker, lockerDoor, consoleDesk, wallScreen, pipeBundle, ventGrille } from '../props.js';
import { FX } from '../../engine/particles.js';
import { has } from '../../core/state.js';
import { logPad, memento, PI } from './common.js';

export function buildCryo(ctx) {
  const { b, room } = ctx;
  ctx.setAmbient([0.03, 0.045, 0.07]);
  ctx.shell({
    windows: [{ side: 'W', a: -1, b: 5, y0: 1.05, y1: 3.0 }],
    ceilLights: { spacing: 4.6, intensity: 0.8, color: 0xcfe8ff },
    ducts: { size: 0.46 },
    ribSkip: (side) => side !== 'W',
  });
  const xs = [-13.6, -10.6, -7.6, -4.6];
  const crew = ['MARLOW', 'OKONKWO', 'RAMAN', 'LINDQVIST', 'VASQUEZ'];
  // north row: all occupied
  xs.forEach((x, i) => {
    b.at(x, 0, room.z0 + 0.85, 0, (bb) => {
      cryoPod(bb, ctx, { occupied: true, label: `0${i + 1}` });
      bb.sign(crew[i], 0, 0.012, 1.25, 0.16, { rx: -PI / 2, px: 56, tint: 0x9fdcff, i: 0.9, type: 3, spacing: 0.25 });
      ctx.emitter({ rate: 2.2, make: FX.frost(0.5, 0.35) }, 0, 0.08, 0.7);
    });
  });
  // south row: Vasquez, the player's (open) pod, two empties
  xs.forEach((x, i) => {
    b.at(x, 0, room.z1 - 0.85, PI, (bb) => {
      if (i === 2) {
        cryoPod(bb, ctx, { occupied: false, open: true, label: '07' });
        ctx.anchor('spawn', 0, 0, 0.06);
        ctx.anchor('podLatch', 0.12, 1.42, 0.4);
        const canopy = ctx.dyn(0, 0, 0, 0, (db) => {
          db.cyl('glassFrost', 0, 1.36, -0.02, 0.5, 2.1, { seg: 14, open: true });
          db.box('trimDark', 0, 0.36, 0.49, 0.6, 0.07, 0.05, { fine: false });
          db.box('trimDark', 0, 2.34, 0.49, 0.6, 0.07, 0.05, { fine: false });
          db.box('metal', 0.2, 1.25, 0.5, 0.05, 0.22, 0.05, { fine: false });
          db.gbox(G(0x6dffb0, 2.4, 2, 2), 0.2, 1.42, 0.51, 0.04, 0.04, 0.02);
        });
        // blocks the way out, but not the player's reach for the latch
        const col = bb.solid(-0.5, 0.34, 0.5, 0.5);
        ctx.tick((dt) => {
          if (has('podOpen') && canopy.visible) {
            canopy.position.y += dt * 1.5;
            if (canopy.position.y > 2.6) canopy.visible = false;
          }
        });
        ctx.sync(() => {
          col.on = !has('podOpen');
          if (!has('podOpen')) {
            canopy.visible = true;
            canopy.position.y = 0;
          }
        });
        bb.sign('YOU', 0, 0.012, 1.25, 0.16, { rx: -PI / 2, px: 56, tint: 0x6dffb0, i: 1.0, type: 3, spacing: 0.25 });
      } else {
        cryoPod(bb, ctx, { occupied: i === 0, label: `0${i + 5}` });
        if (i === 0) bb.sign(crew[4], 0, 0.012, 1.25, 0.16, { rx: -PI / 2, px: 56, tint: 0x9fdcff, i: 0.9, type: 3, spacing: 0.25 });
      }
      ctx.emitter({ rate: 2.2, make: FX.frost(0.5, 0.35) }, 0, 0.08, 0.7);
    });
  });

  // coolant manifold above each row
  onWall(b, room, 'N', room.cx, (bb) => pipeBundle(bb, -6.6, 6.6, 3.5, { mats: ['metal', 'blue', 'metal'] }));
  onWall(b, room, 'S', room.cx, (bb) => pipeBundle(bb, -6.6, 6.6, 3.5, { mats: ['metal', 'blue', 'metal'] }));

  // ---- east wall: lockers, door, breaker panel ----
  onWall(b, room, 'E', -2.3, (bb) => {
    // three lockers; the middle one holds the omni-tool
    for (let i = -1; i <= 1; i++) {
      bb.at(i * 0.66, 0, 0.27, 0, (lb) => {
        if (i === 0) {
          locker(lb, { open: true, label: 'LOCKER 07' });
          lb.box('metal', 0, 1.1, -0.02, 0.56, 0.03, 0.44, { fine: false });
          lb.gbox(G(0x9fdcff, 1.2, 2), 0, 1.98, 0.0, 0.4, 0.015, 0.2);
          const door = ctx.dyn(-0.31 + 0.015, 0, 0.25, 0, (db) => {
            db.push(0.31 - 0.015, 0, -0.25);
            lockerDoor(db, 0.62, 2.05, 0.5);
            db.pop();
          });
          ctx.tick((dt) => {
            const target = has('lockerOpen') ? -1.95 : 0;
            door.rotation.y += (target - door.rotation.y) * Math.min(1, dt * 5);
          });
          // the omni-tool on its shelf
          const tool = ctx.dyn(0, 1.13, 0, 0.5, (db) => {
            db.box('black', 0, 0.04, 0, 0.26, 0.07, 0.12, { fine: false });
            db.box('orange', -0.1, 0.04, 0, 0.05, 0.075, 0.125, { fine: false });
            db.cyl('chrome', 0.15, 0.04, 0, 0.035, 0.08, { axis: 'x', seg: 8 });
            db.gcyl(G(0x9fe8ff, 3, 2), 0.19, 0.04, 0, 0.026, 0.012, { axis: 'x', seg: 8 });
            db.gbox(G(0x6dffb0, 2.2, 2, 2), -0.02, 0.078, 0, 0.1, 0.004, 0.05);
          });
          ctx.sync(() => {
            tool.visible = !has('hasTool');
          });
          ctx.anchor('locker', 0, 1.25, 0.35);
        } else {
          locker(lb, { label: i < 0 ? 'LOCKER 06' : 'LOCKER 08' });
        }
      });
    }
    bb.solid(-1.0, 0, 1.0, 0.56);
  });
  onWall(b, room, 'E', 4.4, (bb) => {
    // breaker panel
    bb.box('gray', 0, 1.45, 0.08, 1.0, 1.3, 0.16, { fine: false });
    bb.box('metalDark', 0, 1.45, 0.165, 0.9, 1.2, 0.012, { fine: false });
    for (let i = 0; i < 6; i++) {
      const x = -0.3 + (i % 3) * 0.3;
      const y = 1.25 + Math.floor(i / 3) * 0.36;
      bb.box('black', x, y, 0.18, 0.14, 0.24, 0.02, { fine: false });
      bb.box('red', x, y - 0.04, 0.2, 0.07, 0.1, 0.04, { fine: false });
      bb.gbox(G(0xff3a20, 2.6, 1, 3), x, y + 0.15, 0.176, 0.04, 0.025, 0.008);
      bb.gbox(G(0x6dffb0, 2.4, 0), x, y + 0.15, 0.177, 0.04, 0.025, 0.008);
    }
    bb.sign('AUX POWER BUS C', 0, 2.2, 0.02, 0.09, { px: 56, tint: 0xffc060, i: 1.4, type: 2, icon: 'bolt' });
    bb.box('hazard', 0, 0.008, 0.6, 1.2, 0.016, 0.9, { fine: false });
    bb.tube('rubber', [[0.3, 2.1, 0.08], [0.3, 3.2, 0.08], [0.9, 3.5, 0.08]], 0.035, { seg: 6 });
    bb.tube('orange', [[-0.3, 2.1, 0.08], [-0.3, 3.4, 0.08]], 0.03, { seg: 6 });
    ctx.anchor('breaker', 0, 1.45, 0.3);
  });
  onWall(b, room, 'E', 6.3, (bb) => wallScreen(bb, ctx, 'power', 1.75, 1.1, 0.7, { always: true, accent: 0xffb347 }));

  // ---- centre: thaw-control console and a med cart ----
  b.at(-9, 0, 2, PI / 2, (bb) => consoleDesk(bb, ctx, { w: 1.8, screen: 'vitals', accent: 0x66ccff, always: true }));
  b.at(-5.2, 0, 3.4, 0.3, (bb) => {
    bb.box('white', 0, 0.5, 0, 0.7, 0.7, 0.45);
    bb.box('trimDark', 0, 0.87, 0, 0.74, 0.04, 0.5, { fine: false });
    for (const [sx, sz] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) bb.cyl('rubber', sx * 0.28, 0.07, sz * 0.16, 0.07, 0.05, { axis: 'x', seg: 8 });
    bb.cyl('white', -0.2, 0.96, 0.05, 0.05, 0.14, { seg: 8 });
    bb.cyl('teal', -0.08, 0.94, -0.08, 0.035, 0.1, { seg: 8 });
    bb.solidBox(0, 0, 0.75, 0.5);
  });
  logPad(ctx, 'cryo', -5.1, 0.895, 3.45, 0.6);

  // bench under the viewport, with a forgotten keepsake
  onWall(b, room, 'W', 2, (bb) => {
    bb.box('trimDark', 0, 0.22, 0.3, 4.2, 0.44, 0.5);
    bb.box('fabricBlue', 0, 0.48, 0.3, 4.1, 0.08, 0.46);
    bb.solid(-2.1, 0, 2.1, 0.56);
  });
  onWall(b, room, 'W', -2.6, (bb) => ventGrille(bb, 2.6, 1.1, 0.5));
  onWall(b, room, 'W', 6.5, (bb) => wallScreen(bb, ctx, 'diag', 1.75, 1.2, 0.75, { always: true }));

  // deck lettering
  b.sign('CRYO BAY 01', -9, 0.012, -0.6, 0.34, { rx: -PI / 2, px: 96, tint: 0x9fdcff, i: 0.7, type: 3, spacing: 0.3 });
  b.sign('STASIS IN PROGRESS', -9, 3.2, room.z0 + 0.27, 0.16, { px: 64, tint: 0x66ccff, i: 1.6, type: 2, spacing: 0.3 });

  // captain's keepsake on the bench: a tiny model of the ship
  memento(ctx, 1, -15.45, 0.54, 3.2, (db) => {
    db.box('white', 0, 0.05, 0, 0.3, 0.05, 0.08, { fine: false });
    db.box('gray', 0.05, 0.085, 0, 0.1, 0.03, 0.05, { fine: false });
    db.box('orange', -0.13, 0.05, 0, 0.04, 0.07, 0.1, { fine: false });
    db.cyl('black', 0, 0.012, 0, 0.1, 0.024, { seg: 10 });
  });
}
