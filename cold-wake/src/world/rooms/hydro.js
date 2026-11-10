// Hydroponics - the ship's lungs: grow beds under violet lamps, oxygen
// scrubbers, water tanks and a botanist's specimen bench.
import * as THREE from 'three';
import { G } from '../../engine/builder.js';
import { onWall } from '../shell.js';
import { planter, consoleDesk, valveWheel, wallScreen } from '../props.js';
import { FX } from '../../engine/particles.js';
import { has } from '../../core/state.js';
import { logPad, PI } from './common.js';

export function buildHydro(ctx) {
  const { b, room } = ctx;
  const { z0, z1, h } = room;
  const accent = room.color;
  ctx.setAmbient([0.045, 0.06, 0.05]);
  ctx.shell({
    skylights: [
      { x0: 5.6, x1: 9.4, z0: -2.6, z1: 6.4 },
      { x0: 11.6, x1: 15.4, z0: -2.6, z1: 6.4 },
    ],
    windows: [{ side: 'E', a: -2, b: 6, y0: 1.15, y1: 3.4 }],
    ceilLights: { spacing: 5, intensity: 0.6, color: 0xf0ffe8 },
    ribSkip: (side) => side === 'N' || side === 'S',
    lightRange: 8,
  });
  // grow beds
  [-1.4, 1.8, 5.0].forEach((z, i) => {
    b.at(10.5, 0, z, 0, (bb) => planter(bb, 9, { kind: i, seed: i + 2 }));
    ctx.emitter({ rate: 3, make: FX.steam(2.5, 0.25) }, 10.5, 0.9, z);
    b.sign(['SOLANUM 7', 'ZEA STELLARIS', 'LACTUCA / BETA'][i], 6.4, 0.5, z + 0.505, 0.07, { px: 44, tint: 0xdfffe0, i: 0.9, type: 3 });
  });

  // oxygen scrubbers on the bow wall, each with a hand valve
  [6, 10.5, 15].forEach((x, i) => {
    const n = i + 1;
    b.at(x, 0, z0 + 0.25 + 0.75, 0, (bb) => {
      bb.cyl('white', 0, 1.5, 0, 0.62, 2.6, { seg: 18 });
      bb.cyl('trimDark', 0, 0.12, 0, 0.72, 0.24, { seg: 18 });
      bb.sph('white', 0, 2.8, 0, 0.62, { seg: 18, half: true });
      bb.cyl('metalDark', 0, 2.2, 0, 0.64, 0.14, { seg: 18 });
      bb.cyl('metalDark', 0, 0.9, 0, 0.64, 0.14, { seg: 18 });
      bb.cyl('metal', 0, (3.4 + h) / 2, 0, 0.16, h - 3.4, { seg: 10 });
      bb.tube('teal', [[0.5, 0.5, 0.3], [1.0, 0.5, 0.3], [1.0, 3.6, 0.1], [1.0, h - 0.05, 0.1]], 0.07, { seg: 8 });
      bb.box('black', 0, 1.75, 0.6, 0.5, 0.34, 0.08, { fine: false });
      bb.gbox(G(0xff5040, 2.4, 1, 3), -0.12, 1.75, 0.645, 0.1, 0.1, 0.006);
      bb.gbox(G(0x6dffb0, 2.2, 0), 0.12, 1.75, 0.645, 0.1, 0.1, 0.006);
      bb.sign(`O2 SCRUBBER ${n}`, 0, 2.05, 0.635, 0.07, { px: 44, tint: 0xdfffe0, i: 1.2, type: 2 });
      bb.cyl('metalDark', 0, 1.15, 0.6, 0.08, 0.2, { axis: 'z', seg: 10 });
      const wheel = ctx.dyn(0, 1.15, 0.66, 0, (db) => valveWheel(db, 'red'));
      ctx.ref(`valveWheel${n}`, wheel);
      ctx.sync(() => {
        if (has(`valve${n}`)) wheel.rotation.z = 2.4;
      });
      const vent = ctx.emitter({ rate: 0, make: FX.vent(), dx: 0, dy: 1, dz: 0 }, 0, 3.4, 0);
      ctx.sync(() => {
        vent.rate = has(`valve${n}`) ? 5 : 0;
      });
      bb.solidBox(0, 0, 1.4, 1.4);
      ctx.anchor(`valve${n}`, 0, 1.15, 0.85);
    });
  });

  // irrigation control by the door
  b.at(3.3, 0, -1.6, PI / 2, (bb) => {
    consoleDesk(bb, ctx, { w: 1.8, screen: 'hydro', accent });
    ctx.anchor('irrigation', 0, 1.2, 0.6);
  });
  // feed pipes from the console up and across to the beds
  b.tube('teal', [[2.6, 0.3, -2.9], [2.6, h - 0.5, -2.9], [10.5, h - 0.5, -2.9]], 0.09, { seg: 8 });
  b.tube('metal', [[2.75, 0.3, -3.3], [2.75, h - 0.75, -3.3], [16.5, h - 0.75, -3.3], [16.5, h - 0.75, 7.5]], 0.06, { seg: 8 });
  ctx.emitter({ rate: 0.9, make: FX.drip() }, 6.5, h - 0.82, -3.3);
  ctx.emitter({ rate: 0.7, make: FX.drip() }, 13.2, h - 0.82, -3.3);

  // water tanks on the stern wall
  [6.5, 12.5].forEach((x) => {
    b.cyl('white', x, 1.25, z1 - 0.25 - 0.85, 0.8, 4.2, { axis: 'x', seg: 18 });
    b.sph('white', x - 2.1, 1.25, z1 - 1.1, 0.8, { seg: 14, sx: 0.4 });
    b.sph('white', x + 2.1, 1.25, z1 - 1.1, 0.8, { seg: 14, sx: 0.4 });
    for (const s of [-1.2, 1.2]) {
      b.box('trimDark', x + s, 0.3, z1 - 1.1, 0.3, 0.6, 1.5);
      b.tor('metalDark', x + s, 1.25, z1 - 1.1, 0.82, 0.04, { axis: 'x', seg: 20 });
    }
    b.gbox(G(0x5fe3ff, 1.8, 0), x, 1.25, z1 - 1.92, 1.6, 0.06, 0.01);
    b.sign('H2O  4000 L', x, 1.55, z1 - 1.905, 0.1, { ry: PI, px: 56, tint: 0x9fdcff, i: 1.2, type: 3 });
    b.solid(x - 2.4, z1 - 1.95, x + 2.4, z1 - 0.25);
  });
  b.tube('teal', [[9.5, 1.25, z1 - 1.1], [9.5, h - 0.4, z1 - 1.1], [9.5, h - 0.4, 5.0]], 0.08, { seg: 8 });

  // botanist's bench under the viewport with five tagged specimens
  onWall(b, room, 'E', 2, (bb) => {
    bb.box('trimDark', 0, 0.42, 0.34, 8.4, 0.84, 0.64);
    bb.box('white', 0, 0.87, 0.34, 8.5, 0.05, 0.7);
    bb.gbox(G(accent, 1.4, 0), 0, 0.5, 0.665, 8.0, 0.02, 0.01);
    bb.solid(-4.25, 0, 4.25, 0.7);
    const tints = ['leaf', 'leafRed', 'leaf', 'leafRed', 'leaf'];
    const berry = [0xff7ad8, 0x5fe3ff, 0xffd24a, 0xb388ff, 0xff5040];
    for (let i = 0; i < 5; i++) {
      const x = 3.2 - i * 1.6;
      bb.cyl('white', x, 1.02, 0.34, 0.16, 0.26, { seg: 12, top: 1.2 });
      bb.cyl('soil', x, 1.15, 0.34, 0.18, 0.01, { seg: 12 });
      const hgt = 0.5 + (i % 3) * 0.16;
      bb.plane(tints[i], x, 1.16 + hgt / 2, 0.34, hgt * 0.9, hgt, { ry: i });
      bb.plane(tints[i], x, 1.16 + hgt / 2, 0.34, hgt * 0.9, hgt, { ry: i + PI / 2 });
      bb.gsph(G(berry[i], 2.6, 2, 2), x + 0.06, 1.16 + hgt * 0.72, 0.38, 0.03);
      bb.gsph(G(berry[i], 2.6, 2, 2), x - 0.07, 1.16 + hgt * 0.5, 0.3, 0.025);
      bb.box('black', x, 0.91, 0.6, 0.2, 0.012, 0.08, { fine: false });
      bb.gbox(G(0xdfffe0, 1.0, 2), x, 0.918, 0.6, 0.16, 0.004, 0.04);
      ctx.anchor(`plant${i + 1}`, x, 1.3, 0.5);
    }
    bb.light(0, 1.6, 0.9, accent, 0.4, 5);
  });
  logPad(ctx, 'hydro', 18.2, 0.9, -1.9, 0.4, 0x8dff6a);

  onWall(b, room, 'W', 1.6, (bb) => wallScreen(bb, ctx, 'hydro', 2.0, 1.4, 0.9, { accent }));
  b.sign('HYDROPONICS', 4.2, 0.012, 3.0, 0.28, { rx: -PI / 2, ry: -PI / 2, px: 96, tint: accent, i: 0.8, type: 3, spacing: 0.3, icon: 'leaf' });
  b.sign('LIFE SUPPORT  /  O2 GARDEN', 10.5, 3.9, z0 + 0.27, 0.16, { px: 64, tint: accent, i: 1.6, type: 0, spacing: 0.3, icon: 'leaf' });
  // fireflies of pollen in the lamp light
  ctx.emitter({ rate: 5, make: FX.motes(4.5, 0.8, 4, [0.6, 1.2, 0.5]) }, 10.5, 1.8, 1.8);
  void THREE;
}
