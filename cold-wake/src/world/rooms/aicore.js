// AI Core - VEGA's home: a dark vault of compute racks around a suspended
// cognitive core that wakes up when the player reboots it.
import * as THREE from 'three';
import { G } from '../../engine/builder.js';
import { onWall } from '../shell.js';
import { serverRack, consoleDesk, crate, cableTray } from '../props.js';
import { has } from '../../core/state.js';
import { FX } from '../../engine/particles.js';
import { logPad, PI } from './common.js';

export function buildAICore(ctx) {
  const { b, room } = ctx;
  const { z0, z1, h } = room;
  const accent = room.color;
  const cx = -6.5;
  const cz = -29.5;
  ctx.setAmbient([0.02, 0.025, 0.045]);
  ctx.shell({
    wall: 'wallDark',
    ribs: 0,
    skirtGlow: true,
    ceilLights: { spacing: 4.5, intensity: 0.4, color: 0x8fa8ff },
    lightRange: 6,
  });
  // racks along the long walls
  for (let i = 0; i < 6; i++) {
    const x = -9.4 + i * 1.02;
    b.at(x, 0, z0 + 0.25 + 0.5, 0, (bb) => serverRack(bb, { color: accent, h: 2.7 }));
    if (x > -4.6) continue;
    b.at(x, 0, z1 - 0.25 - 0.5, PI, (bb) => serverRack(bb, { color: accent, h: 2.7 }));
  }
  for (let i = 0; i < 3; i++) b.at(-10.25, 0, -31.2 + i * 1.5, PI / 2, (bb) => serverRack(bb, { color: accent, h: 2.7, w: 1.2 }));
  b.at(-6.5, 0, room.cz, 0, (bb) => cableTray(bb, -4, 4, h - 0.5, -3.2));
  b.at(-6.5, 0, room.cz, 0, (bb) => cableTray(bb, -4, 4, h - 0.5, 3.2));

  // pedestal, emitter column and ceiling cradle
  b.cyl('trimDark', cx, 0.16, cz, 1.7, 0.32, { seg: 28 });
  b.cyl('metalDark', cx, 0.36, cz, 1.25, 0.1, { seg: 28 });
  b.gtor(G(accent, 2.4, 0), cx, 0.33, cz, 1.72, 0.02, { axis: 'y', seg: 44 });
  b.gtor(G(accent, 2.0, 0, 2), cx, 0.42, cz, 1.0, 0.016, { axis: 'y', seg: 36 });
  b.gcyl(G(accent, 1.4, 0, 5), cx, 0.42, cz, 0.3, 0.02, { seg: 16 });
  b.cyl('trimDark', cx, h - 0.35, cz, 1.2, 0.7, { seg: 24, top: 1.25 });
  b.cyl('metalDark', cx, h - 0.8, cz, 0.5, 0.3, { seg: 16 });
  b.gtor(G(accent, 2.2, 0), cx, h - 0.72, cz, 1.0, 0.018, { axis: 'y', seg: 36 });
  for (let i = 0; i < 6; i++) {
    const a = (i / 6) * PI * 2;
    b.tube('rubber', [[cx + Math.cos(a) * 1.1, h - 0.1, cz + Math.sin(a) * 1.1], [cx + Math.cos(a) * 2.2, h - 0.3, cz + Math.sin(a) * 2.2], [cx + Math.cos(a) * 3.3, h - 0.12, cz + Math.sin(a) * 3.0]], 0.04, { seg: 6 });
    // deck trenches
    const L = 2.0;
    b.gbox({ hex: accent, i: 1.8, type: 0, fx: 4, ph: i / 6 }, cx + Math.cos(a) * (1.7 + L / 2), 0.012, cz + Math.sin(a) * (1.7 + L / 2), L, 0.006, 0.05, { ry: -a });
  }
  b.light(cx, 1.6, cz, accent, 1.1, 7.5, { key: true });
  b.light(cx, 3.6, cz, accent, 0.6, 6);
  b.solidBox(cx, cz, 2.2, 2.2);

  // the core itself: live colours, so it is plain three.js rather than baked
  const core = new THREE.Group();
  core.position.set(cx, 2.3, cz);
  const colOff = new THREE.Color(0.5, 0.05, 0.03);
  const colOn = new THREE.Color(0.35, 1.5, 3.0);
  const mGlow = new THREE.MeshBasicMaterial({ color: colOff.clone() });
  const mWire = new THREE.LineBasicMaterial({ color: colOff.clone(), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false });
  const heart = new THREE.Mesh(new THREE.IcosahedronGeometry(0.34, 2), mGlow);
  const cage = new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.IcosahedronGeometry(0.72, 1)), mWire);
  const cage2 = new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.OctahedronGeometry(1.0, 0)), mWire);
  core.add(heart, cage, cage2);
  const rings = [];
  for (let i = 0; i < 3; i++) {
    const r = new THREE.Mesh(new THREE.TorusGeometry(0.95 + i * 0.16, 0.014, 6, 64), mGlow);
    r.rotation.set(i * 1.1, i * 0.7, 0);
    core.add(r);
    rings.push(r);
  }
  ctx.add(core);
  const api = { level: 0, talk: 0, group: core };
  ctx.ref('vegaCore', api);
  const motes = ctx.emitter({ rate: 0, make: FX.motes(1.0, 1.0, 1.0, [0.4, 0.7, 1.4]) }, cx, 2.3, cz);
  ctx.tick((dt, t) => {
    const target = has('vegaOnline') ? 1 : 0;
    api.level += (target - api.level) * Math.min(1, dt * 1.5);
    api.talk = Math.max(0, api.talk - dt * 2.5);
    const L = api.level;
    const pulse = 0.75 + 0.25 * Math.sin(t * (1.5 + L * 2)) + api.talk * 0.9;
    mGlow.color.copy(colOff).lerp(colOn, L).multiplyScalar((0.5 + L * 1.1) * pulse);
    mWire.color.copy(colOff).lerp(colOn, L).multiplyScalar(0.8 + L);
    const spin = 0.08 + L * 0.7;
    cage.rotation.y += dt * spin;
    cage.rotation.x += dt * spin * 0.6;
    cage2.rotation.y -= dt * spin * 0.5;
    rings.forEach((r, i) => {
      r.rotation.x += dt * spin * (0.9 + i * 0.4);
      r.rotation.y += dt * spin * (0.5 - i * 0.3);
    });
    heart.scale.setScalar(1 + 0.08 * Math.sin(t * 3) * L + api.talk * 0.25);
    core.position.y = 2.3 + Math.sin(t * 0.9) * 0.06 * L - (1 - L) * 0.35;
    motes.rate = 14 * L;
  });

  // reboot console between the door and the core
  b.at(-3.9, 0, -28.4, PI / 2, (bb) => {
    consoleDesk(bb, ctx, { w: 1.7, screen: 'vega', accent, always: true });
    ctx.anchor('vegaConsole', 0, 1.2, 0.5);
  });
  b.sign('V.E.G.A.  COGNITIVE CORE', -6.5, 3.5, z0 + 0.27, 0.2, { px: 72, tint: accent, i: 1.8, type: 2, spacing: 0.35 });
  b.sign('STATIC-SAFE AREA', -6.5, 0.012, -26.8, 0.18, { rx: -PI / 2, ry: PI, px: 64, tint: 0xffc060, i: 0.8, type: 3, icon: 'warn' });
  b.at(-3.1, 0, -26.1, 0.2, (bb) => crate(bb, 0, 0, 0.62, 'crateDark'));
  logPad(ctx, 'ai', -3.1, 0.63, -26.1, 0.5, 0x6f8cff);
}
