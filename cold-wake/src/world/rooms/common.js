// Helpers shared by the room builders: collectibles, repairable panels,
// wall dressing.
import { G } from '../../engine/builder.js';
import { has } from '../../core/state.js';
import { FX } from '../../engine/particles.js';
import { onWall } from '../shell.js';
import { dataPad, brokenPanel, ventGrille, junctionBox } from '../props.js';

export const PI = Math.PI;

// A collectible data log lying at a world position. Anchor: log_<id>.
export function logPad(ctx, id, x, y, z, ry = 0, color = 0x5fe3ff) {
  const pad = ctx.dyn(x, y, z, ry, (b) => dataPad(b, color), { name: `log_${id}` });
  ctx.world.anchors[`log_${id}`] = pad.position.clone().setY(y + 0.1);
  ctx.sync(() => {
    pad.visible = !has(`log_${id}`);
  });
  return pad;
}

// A sparking service panel the player can patch. Anchor: panel<n>.
export function servicePanel(ctx, n, side, u, y = 1.3) {
  const { b, room } = ctx;
  let em = null;
  onWall(b, room, side, u, (bb) => {
    brokenPanel(bb, y);
    ctx.anchor(`panel${n}`, 0, y, 0.25);
    em = ctx.emitter({ rate: 9, make: FX.sparks(0.7), dz: 0 }, 0, y + 0.1, 0.12);
    const cover = ctx.dyn(0, y, 0.05, 0, (db) => {
      db.box('gray', 0, 0, 0, 0.74, 0.94, 0.03, { fine: false });
      db.box('metalDark', 0, 0, 0.018, 0.6, 0.8, 0.008, { fine: false });
      for (const [sx, sy] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) db.cyl('chrome', sx * 0.32, sy * 0.42, 0.02, 0.018, 0.012, { axis: 'z', seg: 6 });
      db.gbox(G(0x6dffb0, 2.2, 0), 0.26, 0.38, 0.024, 0.04, 0.02, 0.006);
    });
    ctx.sync(() => {
      const fixed = has(`panel${n}`);
      cover.visible = fixed;
      em.on = !fixed;
    });
  });
}

// Scatter generic wall dressing on the clear stretches of a wall.
export function dressWall(ctx, side, items) {
  const { b, room } = ctx;
  for (const it of items) {
    onWall(b, room, side, it.u, (bb) => {
      if (it.kind === 'vent') ventGrille(bb, it.y ?? 2.5, it.w ?? 1, it.h ?? 0.45);
      else if (it.kind === 'box') junctionBox(bb, it.y ?? 1.4, it);
    });
  }
}

// A keepsake belonging to a crew member. Anchor: mem<n>.
export function memento(ctx, n, x, y, z, build, ry = 0) {
  const m = ctx.dyn(x, y, z, ry, build, { name: `mem${n}` });
  ctx.world.anchors[`mem${n}`] = m.position.clone().setY(y + 0.1);
  ctx.sync(() => {
    m.visible = !has(`mem${n}`);
  });
  return m;
}
