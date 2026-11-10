// Engineering - a nine-metre hall built around the fusion core.
import * as THREE from 'three';
import { G } from '../../engine/builder.js';
import { onWall, ceilingFixture } from '../shell.js';
import { consoleDesk, fusionCell, wallScreen, barrel, shelving, table, ventGrille } from '../props.js';
import { FX } from '../../engine/particles.js';
import { shared } from '../../engine/materials.js';
import { has } from '../../core/state.js';
import { logPad, servicePanel, PI } from './common.js';

export function buildEngineering(ctx) {
  const { b, room } = ctx;
  const { x0, x1, z0, z1, h } = room;
  const accent = room.color;
  const cx = 0;
  const cz = 20;
  ctx.setAmbient([0.04, 0.04, 0.045]);
  ctx.shell({
    wall: 'wallDark',
    floor: 'tread',
    ceilLights: null,
    beams: 5,
    ribs: 4,
    upperKit: { from: 5.6, max: 2.4 },
    emergencyAt: [[-8, 12], [8, 12], [-8, 28], [8, 28]],
  });
  // high-bay lamps
  for (const [lx, lz] of [[-6, 14], [6, 14], [-6, 26], [6, 26], [-7, 20], [7, 20]]) {
    b.cyl('metalDark', lx, h - 0.9, lz, 0.03, 1.8, { seg: 5 });
    b.cyl('trimDark', lx, h - 1.9, lz, 0.5, 0.3, { seg: 14, top: 0.5 });
    b.gcyl(G(0xffe6c8, 3.4, 0), lx, h - 2.06, lz, 0.42, 0.02, { seg: 14 });
    b.light(lx, h - 3.2, lz, 0xffe6c8, 1.5, 11.5, { key: true });
  }

  // ---- reactor --------------------------------------------------------------
  b.cyl('trimDark', cx, 0.3, cz, 2.6, 0.6, { seg: 32 });
  b.cyl('metalDark', cx, 0.75, cz, 2.2, 0.3, { seg: 32 });
  b.cyl('trimDark', cx, 1.05, cz, 1.75, 0.3, { seg: 32 });
  b.gtor(G(accent, 2.4, 2, 2), cx, 0.62, cz, 2.62, 0.025, { axis: 'y', seg: 56 });
  b.gtor(G(0x66e8ff, 2.6, 0), cx, 1.22, cz, 1.62, 0.03, { axis: 'y', seg: 48 });
  b.cyl('glass', cx, 4.0, cz, 1.5, 5.6, { seg: 28, open: true });
  b.cyl('trimDark', cx, 6.95, cz, 1.75, 0.3, { seg: 32 });
  b.cyl('metalDark', cx, 7.45, cz, 2.3, 0.7, { seg: 32, top: 0.85 });
  b.cyl('trimDark', cx, (7.8 + h) / 2, cz, 1.9, h - 7.8, { seg: 32 });
  b.gtor(G(0x66e8ff, 2.6, 0), cx, 6.78, cz, 1.62, 0.03, { axis: 'y', seg: 48 });
  // inner electrode column
  b.cyl('metalDark', cx, 1.6, cz, 0.3, 0.8, { seg: 12, top: 0.5 });
  b.cyl('metalDark', cx, 6.4, cz, 0.15, 0.8, { seg: 12, top: 2.0 });
  // four pylons
  for (let i = 0; i < 4; i++) {
    const a = PI / 4 + (i * PI) / 2;
    const px = cx + Math.cos(a) * 2.35;
    const pz = cz + Math.sin(a) * 2.35;
    b.box('trimDark', px, h / 2, pz, 0.5, h, 0.5, { ry: -a });
    b.box('metalDark', px, 1.0, pz, 0.7, 2.0, 0.7, { ry: -a });
    b.gbox(G(accent, 2.2, 0), cx + Math.cos(a) * 2.62, 4.2, cz + Math.sin(a) * 2.62, 0.04, 4.4, 0.012, { ry: -a + PI / 2 });
    // coolant feed up the pylon
    b.tube('copper', [[cx + Math.cos(a) * 2.75, 0.6, cz + Math.sin(a) * 2.75], [cx + Math.cos(a) * 2.75, 7.3, cz + Math.sin(a) * 2.75], [cx + Math.cos(a) * 1.9, 7.6, cz + Math.sin(a) * 1.9]], 0.07, { seg: 8 });
  }
  // baked glow of the running core
  for (const y of [2.2, 4.0, 5.8]) {
    for (let i = 0; i < 4; i++) {
      const a = (i * PI) / 2;
      b.light(cx + Math.cos(a) * 2.9, y, cz + Math.sin(a) * 2.9, 0x7fdcff, 1.1, 9);
    }
  }
  b.light(cx, 0.9, cz - 4, accent, 0.5, 5, { emerg: false });
  // collision: an octagon out of three boxes
  b.solidBox(cx, cz, 5.2, 2.2);
  b.solidBox(cx, cz, 2.2, 5.2);
  b.solidBox(cx, cz, 4.0, 4.0);
  // deck ring
  b.tor('yellow', cx, 0.012, cz, 3.7, 0.05, { axis: 'y', seg: 56, rseg: 4 });
  b.tor('grate', cx, 0.004, cz, 3.2, 0.42, { axis: 'y', seg: 56, rseg: 4, sy: 0.05 });

  // plasma column (live shader)
  const plasmaMat = new THREE.ShaderMaterial({
    uniforms: { uTime: shared.uTime, uLevel: { value: 0 } },
    vertexShader: `varying vec2 vUv; varying vec3 vN; varying vec3 vV;
void main() { vUv = uv; vec4 w = modelMatrix * vec4(position, 1.0); vN = normalize(mat3(modelMatrix) * normal); vV = normalize(cameraPosition - w.xyz); gl_Position = projectionMatrix * viewMatrix * w; }`,
    fragmentShader: `uniform float uTime; uniform float uLevel; varying vec2 vUv; varying vec3 vN; varying vec3 vV;
float h(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float n(vec2 p) { vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f); return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y); }
void main() {
  vec2 p = vec2(vUv.x * 8.0, vUv.y * 5.0);
  float a = n(p + vec2(uTime * 0.6, -uTime * 2.2)) * 0.6 + n(p * 2.3 + vec2(-uTime * 0.9, -uTime * 3.4)) * 0.4;
  float core = pow(max(dot(normalize(vN), normalize(vV)), 0.0), 1.6);
  float ends = smoothstep(0.0, 0.12, vUv.y) * smoothstep(1.0, 0.88, vUv.y);
  float e = (0.35 + 1.4 * a * a) * core * ends;
  vec3 hot = mix(vec3(0.15, 0.5, 1.6), vec3(1.4, 1.8, 2.4), a);
  vec3 cold = vec3(0.5, 0.12, 0.04) * (0.25 + 0.3 * a);
  vec3 c = mix(cold * core * ends, hot * e * 2.2, uLevel);
  gl_FragColor = vec4(c, 1.0);
}`,
    transparent: true,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
  });
  const plasma = new THREE.Mesh(new THREE.CylinderGeometry(0.85, 0.85, 4.9, 24, 1, true), plasmaMat);
  plasma.position.set(cx, 4.0, cz);
  ctx.add(plasma);
  const core2 = new THREE.Mesh(new THREE.CylinderGeometry(0.28, 0.28, 4.9, 12, 1, true), plasmaMat);
  core2.position.copy(plasma.position);
  ctx.add(core2);
  // counter-rotating containment rings
  const rings = [2.3, 4.0, 5.7].map((y, i) => {
    const ring = ctx.dyn(cx, y, cz, 0, (db) => {
      db.tor('metalDark', 0, 0, 0, 1.25, 0.09, { axis: 'y', seg: 36 });
      db.gtor(G(0x66e8ff, 2.4, 0), 0, 0, 0, 1.34, 0.02, { axis: 'y', seg: 36 });
      for (let k = 0; k < 6; k++) {
        const a = (k / 6) * PI * 2;
        db.box('trim', Math.cos(a) * 1.25, 0, Math.sin(a) * 1.25, 0.3, 0.26, 0.2, { ry: -a + PI / 2, fine: false });
      }
    });
    ring.userData.dir = i % 2 ? -1 : 1;
    return ring;
  });
  const motes = ctx.emitter({ rate: 0, make: FX.plasma(0.7, 2.2), scale: 0 }, cx, 4.0, cz);
  const api = { level: 0 };
  ctx.ref('reactor', api);
  ctx.tick((dt, t) => {
    const target = has('reactorOnline') ? 1 : 0;
    api.level += (target - api.level) * Math.min(1, dt * 0.7);
    plasmaMat.uniforms.uLevel.value = api.level;
    for (const r of rings) {
      r.rotation.y += dt * r.userData.dir * (0.1 + api.level * 1.6);
      r.position.y += Math.sin(t * 1.3 + r.userData.dir) * 0.0006;
    }
    motes.rate = 40 * api.level;
    motes.scale = api.level;
  });

  // ---- fuel sockets around the base ----
  [[0, -1], [1, 0], [-1, 0]].forEach(([dx, dz], i) => {
    const n = i + 1;
    const ry = Math.atan2(dx, dz);
    b.at(cx + dx * 3.0, 0, cz + dz * 3.0, ry, (bb) => {
      bb.box('trimDark', 0, 0.45, 0, 0.7, 0.9, 0.6);
      bb.box('metalDark', 0, 0.93, 0.02, 0.76, 0.08, 0.66, { rx: 0.2 });
      bb.cyl('black', 0, 0.98, 0, 0.16, 0.08, { seg: 14 });
      bb.gtor(G(0xffb347, 2.4, 2, 2), 0, 1.0, 0, 0.17, 0.014, { axis: 'y', seg: 20 });
      bb.sign(`CELL ${n}`, 0, 0.6, 0.305, 0.09, { px: 56, tint: 0xffc060, i: 1.3, type: 2, icon: 'bolt' });
      bb.tube('rubber', [[0, 0.3, -0.3], [0, 0.3, -0.6], [0, 0.6, -0.9]], 0.05, { seg: 6 });
      const cell = ctx.dyn(0, 0.98, 0, 0, (db) => fusionCell(db));
      ctx.sync(() => {
        cell.visible = has(`socket${n}`);
      });
      bb.solidBox(0, 0, 0.75, 0.65);
      ctx.anchor(`socket${n}`, 0, 1.15, 0.45);
    });
  });

  // ---- ignition console, facing the door ----
  b.at(0, 0, 14.4, PI, (bb) => {
    consoleDesk(bb, ctx, { w: 2.2, screen: 'reactor', accent, always: true });
    ctx.anchor('ignite', 0, 1.2, 0.6);
  });
  b.sign('FUSION CORE', 0, 0.012, 12.6, 0.36, { rx: -PI / 2, px: 96, tint: accent, i: 0.9, type: 3, spacing: 0.35 });

  // ---- port wall: cell rack and generators ----
  onWall(b, room, 'W', 13.2, (bb) => {
    bb.box('trimDark', 0, 1.0, 0.3, 2.2, 2.0, 0.6);
    bb.box('black', 0, 1.15, 0.5, 1.9, 1.3, 0.3, { fine: false });
    for (let i = 0; i < 4; i++) {
      bb.box('metal', -0.72 + i * 0.48, 0.55, 0.52, 0.34, 0.04, 0.3, { fine: false });
      bb.gbox(G(i === 1 ? 0x6dffb0 : 0xff5040, 2.2, 2, i === 1 ? 0 : 3), -0.72 + i * 0.48, 1.74, 0.655, 0.2, 0.03, 0.006);
    }
    bb.sign('FUSION CELL STORAGE', 0, 2.15, 0.02, 0.09, { px: 56, tint: 0xffc060, i: 1.4, type: 2, icon: 'bolt' });
    const cell = ctx.dyn(-0.24, 0.57, 0.52, 0, (db) => fusionCell(db));
    ctx.sync(() => {
      cell.visible = !has('cellEng');
    });
    bb.light(0, 1.2, 1.2, 0x66e8ff, 0.5, 3.4, { emerg: false });
    bb.solid(-1.1, 0, 1.1, 0.62);
    ctx.anchor('cellEng', -0.24, 1.0, 0.8);
  });
  for (const gz of [19.5, 25.5]) {
    b.at(x0 + 0.25 + 1.3, 0, gz, PI / 2, (bb) => {
      bb.box('wallDark', 0, 1.5, 0, 3.6, 3.0, 2.4, { tile: 2.4 });
      bb.box('trimDark', 0, 3.1, 0, 3.8, 0.2, 2.6);
      bb.cyl('metalDark', -1.0, 3.9, 0, 0.36, 1.5, { seg: 12 });
      bb.cyl('metalDark', 1.0, 3.9, 0, 0.36, 1.5, { seg: 12 });
      bb.box('black', 0, 1.9, 1.21, 2.6, 0.9, 0.02, { fine: false });
      for (let i = 0; i < 8; i++) bb.box('metal', 0, 1.55 + i * 0.1, 1.23, 2.5, 0.03, 0.03, { rx: 0.5, fine: false });
      bb.gbox(G(accent, 2.2, 0), -1.3, 0.8, 1.215, 0.5, 0.05, 0.01);
      bb.gbox(G(0x6dffb0, 2.2, 0, 5), 1.3, 0.8, 1.215, 0.3, 0.05, 0.01);
      bb.box('hazard', 0, 0.2, 1.21, 3.6, 0.3, 0.02, { fine: false });
      bb.tube('orange', [[-1.0, 4.6, 0], [-1.0, 5.6, 0], [-1.0, 5.6, 2.5], [-1.0, 7.4, 4.5]], 0.12, { seg: 8 });
      bb.light(0, 1.4, 2.2, accent, 0.5, 4.5);
      bb.solidBox(0, 0, 3.7, 2.5);
    });
  }
  // ---- starboard wall: coolant tanks ----
  [15.5, 20, 24.5].forEach((tz, i) => {
    const tx = x1 - 0.25 - 1.1;
    b.cyl('white', tx, 2.6, tz, 0.95, 4.4, { seg: 20 });
    b.sph('white', tx, 4.8, tz, 0.95, { seg: 20, half: true });
    b.cyl('trimDark', tx, 0.2, tz, 1.05, 0.4, { seg: 20 });
    b.tor('metalDark', tx, 1.6, tz, 0.97, 0.05, { axis: 'y', seg: 24 });
    b.tor('metalDark', tx, 3.6, tz, 0.97, 0.05, { axis: 'y', seg: 24 });
    b.gbox(G(0x5fe3ff, 2.0, 0), tx - 0.96, 2.2, tz, 0.012, 1.6, 0.12);
    b.sign(`COOLANT ${i + 1}`, tx - 0.965, 3.3, tz, 0.12, { ry: -PI / 2, px: 56, tint: 0x9fdcff, i: 1.2, type: 3 });
    b.tube('blue', [[tx, 5.6, tz], [tx, 6.6, tz], [2.2, 6.6, tz * 0.4 + cz * 0.6], [1.6, 7.0, tz * 0.2 + cz * 0.8]], 0.11, { seg: 8 });
    b.solidBox(tx, tz, 2.0, 2.0);
    ctx.emitter({ rate: 1.2, make: FX.steam(0.2, 0.7) }, tx - 1.1, 0.3, tz + 0.6);
  });
  // ---- stern: workbench, tool wall, drums ----
  b.at(-5.5, 0, z1 - 0.25 - 0.5, PI, (bb) => {
    table(bb, 3.0, 0.8, { top: 'metal', h: 0.92 });
    bb.box('red', -1.0, 1.02, 0, 0.5, 0.2, 0.3, { fine: false });
    bb.box('metalDark', 0.2, 0.98, 0.05, 0.6, 0.06, 0.2, { ry: 0.2, fine: false });
    bb.cyl('chrome', 0.9, 0.975, 0, 0.02, 0.4, { axis: 'x', seg: 6 });
  });
  onWall(b, room, 'S', -5.5, (bb) => {
    bb.box('trimDark', 0, 1.9, 0.02, 3.0, 1.3, 0.04, { fine: false });
    for (let i = 0; i < 7; i++) bb.box(i % 3 === 0 ? 'orange' : 'chrome', -1.2 + i * 0.4, 1.9 + ((i * 7) % 3) * 0.12, 0.06, 0.06, 0.5 + ((i * 5) % 3) * 0.15, 0.04, { fine: false });
    bb.gbox(G(0xffe6c8, 2.2, 0), 0, 2.65, 0.12, 2.8, 0.03, 0.08);
    bb.light(0, 2.2, 0.9, 0xffe6c8, 0.5, 3.6);
  });
  logPad(ctx, 'eng', -4.9, 0.955, z1 - 0.8, 0.5, 0xffa040);
  barrel(b, 7.6, 28.4, 'orange');
  barrel(b, 8.4, 28.7, 'blue');
  barrel(b, 8.0, 27.7, 'orange');
  b.at(5.0, 0, z1 - 0.25 - 0.35, PI, (bb) => shelving(bb, { w: 2.6, seed: 5 }));
  // overhead trunking across the hall
  b.cyl('metal', cx, 8.2, 13.5, 0.22, 19, { axis: 'x', seg: 10 });
  b.cyl('metal', cx, 8.2, 26.5, 0.22, 19, { axis: 'x', seg: 10 });
  b.cyl('copper', -6, 8.45, cz, 0.12, 19, { axis: 'z', seg: 8 });
  b.cyl('copper', 6, 8.45, cz, 0.12, 19, { axis: 'z', seg: 8 });
  // signs, panels, screens
  b.sign('DANGER  HIGH ENERGY PLASMA', 0, 5.2, z0 + 0.27, 0.3, { px: 96, tint: 0xffb347, i: 1.6, type: 2, spacing: 0.3, icon: 'warn' });
  b.sign('AUTHORISED ENGINEERING STAFF ONLY', 0, 4.75, z0 + 0.27, 0.13, { px: 56, tint: 0xffd9a0, i: 1.2, type: 2, spacing: 0.3 });
  onWall(b, room, 'N', -5.5, (bb) => wallScreen(bb, ctx, 'reactor', 1.9, 1.6, 1.0, { always: true, accent }));
  onWall(b, room, 'N', 5.5, (bb) => wallScreen(bb, ctx, 'diag', 1.9, 1.6, 1.0, { accent }));
  onWall(b, room, 'E', 28.4, (bb) => ventGrille(bb, 3.0, 1.4, 0.6));
  servicePanel(ctx, 4, 'E', 12.2);
  servicePanel(ctx, 5, 'S', 8.2);
  ctx.emitter({ rate: 1.5, make: FX.steam(0.25, 0.8) }, -3.6, 0.1, 16.2);
  ctx.emitter({ rate: 1.5, make: FX.steam(0.25, 0.8) }, 3.6, 0.1, 23.8);
  void ceilingFixture;
}
