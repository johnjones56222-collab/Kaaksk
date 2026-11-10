// Central Hub - a tall atrium under a skylight, with a live hologram of the
// ship that turns from red to green as systems are restored.
import * as THREE from 'three';
import { G } from '../../engine/builder.js';
import { onWall } from '../shell.js';
import { ROOMS } from '../layout.js';
import { roomStatus } from '../screens.js';
import { holoTable, wallScreen, ventGrille } from '../props.js';
import { logPad, memento, servicePanel, PI } from './common.js';

export function buildHub(ctx) {
  const { b, room } = ctx;
  const { x0, x1, z0, z1, h } = room;
  const accent = room.color;
  ctx.setAmbient([0.045, 0.055, 0.065]);
  ctx.shell({
    wall: 'wallLight',
    skylights: [{ x0: -3.2, x1: 3.2, z0: -17.2, z1: -10.8 }],
    ceilLights: { spacing: 5.2, intensity: 1.2, color: 0xe4f2ff },
    lightRange: 10.5,
    ribs: 4,
    upperKit: { from: 4.05, max: 1.6 },
  });

  // gallery band halfway up the walls
  for (const [x, z, w, d] of [
    [0, z0 + 0.45, room.w - 0.6, 0.4],
    [0, z1 - 0.45, room.w - 0.6, 0.4],
    [x0 + 0.45, room.cz, 0.4, room.d - 0.6],
    [x1 - 0.45, room.cz, 0.4, room.d - 0.6],
  ]) {
    b.box('trimDark', x, 3.7, z, w, 0.34, d);
    b.gbox(G(accent, 1.8, 0), x, 3.515, z, Math.max(0.03, w - 0.5) * (w > d ? 1 : 0.08), 0.02, Math.max(0.03, d - 0.5) * (d > w ? 1 : 0.08));
  }
  for (let i = 0; i < 4; i++) {
    b.light(-6 + i * 4, 3.2, z0 + 1.2, accent, 0.4, 5);
    b.light(-6 + i * 4, 3.2, z1 - 1.2, accent, 0.4, 5);
  }

  // four structural columns
  for (const [cx, cz] of [[-5, -19], [5, -19], [-5, -9], [5, -9]]) {
    b.cyl('trimDark', cx, h / 2, cz, 0.42, h, { seg: 12 });
    b.cyl('metalDark', cx, 0.25, cz, 0.62, 0.5, { seg: 12, top: 0.8 });
    b.cyl('metalDark', cx, h - 0.3, cz, 0.62, 0.6, { seg: 12 });
    for (const y of [1.2, 2.6, 4.0, 5.4]) b.gtor(G(accent, 2.4, 0), cx, y, cz, 0.44, 0.02, { axis: 'y', seg: 20 });
    b.light(cx, 2.4, cz + (cz < -14 ? 1 : -1), accent, 0.45, 4.5);
    b.solidBox(cx, cz, 1.0, 1.0);
  }

  // central projector
  b.at(0, 0, -14, 0, (bb) => holoTable(bb, 1.5, { accent }));
  ctx.anchor('holo', 0, 1.2, -14);

  // the ship hologram: wireframe decks coloured by section status
  const s = 1 / 24;
  const pts = [];
  const cols = [];
  const idx = [];
  for (let i = 1; i < ROOMS.length; i++) {
    const r = ROOMS[i];
    if (r.exterior) continue;
    const hh = Math.min(r.h, 5) * s * 1.6;
    const c = [
      [r.x0, 0, r.z0], [r.x1, 0, r.z0], [r.x1, 0, r.z1], [r.x0, 0, r.z1],
      [r.x0, hh / s, r.z0], [r.x1, hh / s, r.z0], [r.x1, hh / s, r.z1], [r.x0, hh / s, r.z1],
    ];
    const edges = [[0, 1], [1, 2], [2, 3], [3, 0], [4, 5], [5, 6], [6, 7], [7, 4], [0, 4], [1, 5], [2, 6], [3, 7]];
    for (const [p, q] of edges) {
      for (const k of [p, q]) {
        pts.push(c[k][0] * s, c[k][1] * s, c[k][2] * s);
        cols.push(1, 1, 1);
        idx.push(r.id);
      }
    }
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
  const colAttr = new THREE.Float32BufferAttribute(cols, 3);
  geo.setAttribute('color', colAttr);
  const holoMat = new THREE.LineBasicMaterial({ vertexColors: true, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false });
  const holo = new THREE.LineSegments(geo, holoMat);
  const holoPivot = new THREE.Group();
  holoPivot.position.set(0, 1.75, -14);
  holo.position.set(0, 0, 0);
  holoPivot.add(holo);
  // projection cone
  const cone = new THREE.Mesh(
    new THREE.CylinderGeometry(1.25, 0.12, 0.85, 24, 1, true),
    new THREE.MeshBasicMaterial({ color: new THREE.Color(0.1, 0.5, 0.5), transparent: true, opacity: 0.12, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide })
  );
  cone.position.set(0, 1.38, -14);
  ctx.add(holoPivot);
  ctx.add(cone);
  const power = () => ctx.world.rooms[room.index].power;
  ctx.tick((dt, t) => {
    holoPivot.rotation.y += dt * 0.22;
    holoPivot.position.y = 1.9 + Math.sin(t * 0.8) * 0.05;
    const p = 0.35 + 0.65 * power();
    holoMat.opacity = (0.34 + 0.12 * p) * (0.85 + 0.15 * Math.sin(t * 17));
    cone.material.opacity = 0.035 + 0.02 * p;
  });
  const palette = { 0: [2.0, 0.2, 0.12], 1: [1.8, 1.0, 0.15], 2: [0.15, 1.7, 1.1] };
  ctx.sync(() => {
    const st = roomStatus();
    for (let i = 0; i < idx.length; i++) {
      const c = palette[st[idx[i]] ?? 1];
      colAttr.setXYZ(i, c[0], c[1], c[2]);
    }
    colAttr.needsUpdate = true;
  });

  // benches and planters in the corners
  for (const [bx, bz, ry] of [[-5.9, -16.6, PI / 2], [5.9, -16.6, -PI / 2], [-5.9, -11.4, PI / 2], [5.9, -11.4, -PI / 2]]) {
    b.at(bx, 0, bz, ry, (bb) => {
      bb.box('trimDark', 0, 0.2, 0, 2.4, 0.4, 0.6);
      bb.box('fabricBlue', 0, 0.45, 0.02, 2.3, 0.1, 0.56);
      bb.box('trimDark', 0, 0.75, -0.3, 2.4, 0.6, 0.1, { rx: -0.15 });
      bb.gbox(G(accent, 1.4, 0), 0, 0.06, 0.305, 2.2, 0.02, 0.01);
      bb.solidBox(0, -0.05, 2.4, 0.7);
    });
  }
  for (const [px, pz] of [[-6.9, -20.9], [6.9, -20.9], [-6.9, -7.1], [6.9, -7.1]]) {
    b.cyl('white', px, 0.35, pz, 0.55, 0.7, { seg: 12, top: 1.15 });
    b.cyl('soil', px, 0.69, pz, 0.56, 0.03, { seg: 12 });
    for (let i = 0; i < 5; i++) {
      const a = i * 1.26;
      b.plane('leaf', px + Math.cos(a) * 0.12, 1.5, pz + Math.sin(a) * 0.12, 1.3, 1.7, { ry: a });
    }
    b.solidBox(px, pz, 1.0, 1.0);
  }

  // information walls
  onWall(b, room, 'N', -4.6, (bb) => wallScreen(bb, ctx, 'schematic', 1.9, 1.5, 1.5, { accent }));
  onWall(b, room, 'N', 4.6, (bb) => wallScreen(bb, ctx, 'diag', 1.8, 1.5, 0.95, { accent }));
  onWall(b, room, 'S', -4.6, (bb) => wallScreen(bb, ctx, 'warn', 1.9, 1.7, 0.64, { always: true, accent: 0xff5040 }));
  onWall(b, room, 'S', 4.6, (bb) => wallScreen(bb, ctx, 'starmap', 1.8, 1.5, 0.95, { accent }));
  onWall(b, room, 'W', -9.2, (bb) => ventGrille(bb, 2.6, 1.2, 0.5));
  onWall(b, room, 'E', -18.8, (bb) => ventGrille(bb, 2.6, 1.2, 0.5));
  servicePanel(ctx, 3, 'W', -18.6);

  // ship's name high on the bow wall
  b.sign('ISV  ASTRAEA', 0, 5.0, z0 + 0.27, 0.62, { px: 128, tint: 0xdff6ff, i: 2.0, type: 0, weight: 300, spacing: 0.45 });
  b.sign('DEEP SURVEY VESSEL  /  UNITED COLONIAL SURVEY', 0, 4.45, z0 + 0.27, 0.13, { px: 56, tint: accent, i: 1.5, type: 0, spacing: 0.35 });
  b.sign('ALL DECKS: MIND THE STEP', 0, 4.7, z1 - 0.27, 0.16, { ry: PI, px: 56, tint: 0xffc060, i: 1.2, type: 0, spacing: 0.3, icon: 'warn' });
  // deck compass rose
  b.gtor(G(accent, 1.3, 0), 0, 0.012, -14, 2.6, 0.02, { axis: 'y', seg: 48 });
  b.gtor(G(accent, 0.9, 0, 2), 0, 0.012, -14, 3.4, 0.012, { axis: 'y', seg: 48 });
  b.sign('BOW', 0, 0.013, -17.9, 0.2, { rx: -PI / 2, px: 64, tint: accent, i: 0.9, type: 3, icon: 'up' });

  logPad(ctx, 'hub', 5.75, 0.51, -11.0, 0.4);
  // pilot's lucky dice on a bench
  memento(ctx, 2, -5.85, 0.5, -16.0, (db) => {
    db.box('red', -0.04, 0.03, 0, 0.06, 0.06, 0.06, { ry: 0.3, fine: false });
    db.box('white', 0.05, 0.03, 0.03, 0.06, 0.06, 0.06, { ry: -0.4, fine: false });
  });
}
