// One draw call for every particle in the game. Premultiplied output lets
// additive (sparks, flame) and alpha (smoke, steam) particles share the batch.
import * as THREE from 'three';

export class Particles {
  constructor(scene, atlas, max = 1400) {
    this.max = max;
    this.n = 0;
    this.pos = new Float32Array(max * 3);
    this.vel = new Float32Array(max * 3);
    this.col = new Float32Array(max * 4);
    this.aux = new Float32Array(max * 3); // size, frame, additive
    this.life = new Float32Array(max);
    this.maxLife = new Float32Array(max);
    this.size0 = new Float32Array(max);
    this.size1 = new Float32Array(max);
    this.alpha = new Float32Array(max);
    this.grav = new Float32Array(max);
    this.drag = new Float32Array(max);
    this.emitters = [];
    this.scale = 1;

    const g = new THREE.BufferGeometry();
    this.aPos = new THREE.BufferAttribute(this.pos, 3).setUsage(THREE.DynamicDrawUsage);
    this.aCol = new THREE.BufferAttribute(this.col, 4).setUsage(THREE.DynamicDrawUsage);
    this.aAux = new THREE.BufferAttribute(this.aux, 3).setUsage(THREE.DynamicDrawUsage);
    g.setAttribute('position', this.aPos);
    g.setAttribute('pcol', this.aCol);
    g.setAttribute('paux', this.aAux);
    g.setDrawRange(0, 0);
    this.material = new THREE.ShaderMaterial({
      uniforms: { map: { value: atlas }, uScale: { value: 600 } },
      vertexShader: `
attribute vec4 pcol;
attribute vec3 paux;
uniform float uScale;
varying vec4 vCol;
varying vec2 vAux;
void main() {
  vCol = pcol;
  vAux = paux.yz;
  vec4 mv = modelViewMatrix * vec4(position, 1.0);
  gl_Position = projectionMatrix * mv;
  gl_PointSize = clamp(paux.x * uScale / max(0.1, -mv.z), 1.0, 420.0);
}`,
      fragmentShader: `
uniform sampler2D map;
varying vec4 vCol;
varying vec2 vAux;
#include <common>
void main() {
  vec2 uv = vec2((vAux.x + gl_PointCoord.x) * 0.25, 1.0 - gl_PointCoord.y);
  float a = texture2D(map, uv).r * vCol.a;
  gl_FragColor = vec4(vCol.rgb * a, a * (1.0 - vAux.y));
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`,
      transparent: true,
      depthWrite: false,
      blending: THREE.CustomBlending,
      blendSrc: THREE.OneFactor,
      blendDst: THREE.OneMinusSrcAlphaFactor,
      blendSrcAlpha: THREE.OneFactor,
      blendDstAlpha: THREE.OneMinusSrcAlphaFactor,
    });
    this.points = new THREE.Points(g, this.material);
    this.points.frustumCulled = false;
    this.points.renderOrder = 20;
    scene.add(this.points);
  }

  // p: {x,y,z,vx,vy,vz,life,size,size1,r,g,b,a,frame,add,grav,drag}
  spawn(p) {
    if (this.n >= this.max) return;
    const i = this.n++;
    const i3 = i * 3;
    this.pos[i3] = p.x;
    this.pos[i3 + 1] = p.y;
    this.pos[i3 + 2] = p.z;
    this.vel[i3] = p.vx || 0;
    this.vel[i3 + 1] = p.vy || 0;
    this.vel[i3 + 2] = p.vz || 0;
    this.col[i * 4] = p.r ?? 1;
    this.col[i * 4 + 1] = p.g ?? 1;
    this.col[i * 4 + 2] = p.b ?? 1;
    this.col[i * 4 + 3] = 0;
    this.alpha[i] = p.a ?? 1;
    this.life[i] = 0;
    this.maxLife[i] = p.life || 1;
    this.size0[i] = p.size ?? 0.1;
    this.size1[i] = p.size1 ?? p.size ?? 0.1;
    this.aux[i3 + 1] = p.frame || 0;
    this.aux[i3 + 2] = p.add ? 1 : 0;
    this.grav[i] = p.grav || 0;
    this.drag[i] = p.drag || 0;
  }

  // An emitter calls make(emitter, particles) `rate` times per second while on.
  addEmitter(e) {
    const em = { rate: 10, acc: 0, on: true, room: -1, ...e };
    this.emitters.push(em);
    return em;
  }

  burst(n, make) {
    for (let i = 0; i < n; i++) make(this);
  }

  update(dt, visibleRooms, density = 1) {
    for (const e of this.emitters) {
      if (!e.on) continue;
      if (e.room >= 0 && visibleRooms && !visibleRooms.has(e.room)) continue;
      e.acc += e.rate * density * dt;
      let guard = 0;
      while (e.acc >= 1 && guard++ < 12) {
        e.acc -= 1;
        e.make(e, this);
      }
      if (e.acc > 1) e.acc = 0;
    }
    let i = 0;
    while (i < this.n) {
      const life = (this.life[i] += dt);
      const ml = this.maxLife[i];
      if (life >= ml) {
        this._remove(i);
        continue;
      }
      const i3 = i * 3;
      const k = life / ml;
      const dr = Math.max(0, 1 - this.drag[i] * dt);
      this.vel[i3] *= dr;
      this.vel[i3 + 1] = this.vel[i3 + 1] * dr - this.grav[i] * dt;
      this.vel[i3 + 2] *= dr;
      this.pos[i3] += this.vel[i3] * dt;
      this.pos[i3 + 1] += this.vel[i3 + 1] * dt;
      this.pos[i3 + 2] += this.vel[i3 + 2] * dt;
      this.aux[i3] = this.size0[i] + (this.size1[i] - this.size0[i]) * k;
      // quick fade in, smooth fade out
      const fade = Math.min(1, k * 8) * (1 - k) * (1 - k * k * 0.3);
      this.col[i * 4 + 3] = this.alpha[i] * fade;
      i++;
    }
    this.aPos.needsUpdate = true;
    this.aCol.needsUpdate = true;
    this.aAux.needsUpdate = true;
    this.points.geometry.setDrawRange(0, this.n);
  }

  _remove(i) {
    const j = --this.n;
    if (i === j) return;
    for (let k = 0; k < 3; k++) {
      this.pos[i * 3 + k] = this.pos[j * 3 + k];
      this.vel[i * 3 + k] = this.vel[j * 3 + k];
      this.aux[i * 3 + k] = this.aux[j * 3 + k];
    }
    for (let k = 0; k < 4; k++) this.col[i * 4 + k] = this.col[j * 4 + k];
    this.life[i] = this.life[j];
    this.maxLife[i] = this.maxLife[j];
    this.size0[i] = this.size0[j];
    this.size1[i] = this.size1[j];
    this.alpha[i] = this.alpha[j];
    this.grav[i] = this.grav[j];
    this.drag[i] = this.drag[j];
  }

  clear() {
    this.n = 0;
    this.points.geometry.setDrawRange(0, 0);
  }
}

const R = Math.random;
const sr = () => R() * 2 - 1;

// Ready-made particle recipes. Each returns a `make` function for an emitter.
export const FX = {
  steam: (spread = 0.15, up = 0.9) => (e, P) =>
    P.spawn({
      x: e.x + sr() * spread, y: e.y, z: e.z + sr() * spread,
      vx: (e.dx || 0) + sr() * 0.12, vy: (e.dy ?? up) * (0.7 + R() * 0.6), vz: (e.dz || 0) + sr() * 0.12,
      life: 1.6 + R() * 1.2, size: 0.12, size1: 0.7 + R() * 0.5,
      r: 0.75, g: 0.82, b: 0.9, a: 0.16, frame: 1, drag: 0.5,
    }),
  frost: (w = 1, d = 1) => (e, P) =>
    P.spawn({
      x: e.x + sr() * w, y: e.y + R() * 0.15, z: e.z + sr() * d,
      vx: sr() * 0.08, vy: 0.02 + R() * 0.05, vz: sr() * 0.08,
      life: 3 + R() * 3, size: 0.5, size1: 1.3,
      r: 0.55, g: 0.8, b: 1, a: 0.07, frame: 1, drag: 0.2,
    }),
  sparks: (power = 1) => (e, P) =>
    P.spawn({
      x: e.x, y: e.y, z: e.z,
      vx: (e.dx || 0) * 1.5 + sr() * 1.6 * power, vy: (e.dy || 0) * 1.5 + R() * 2.2 * power, vz: (e.dz || 0) * 1.5 + sr() * 1.6 * power,
      life: 0.35 + R() * 0.6, size: 0.05 + R() * 0.04, size1: 0.01,
      r: 3.2, g: 2.0 + R(), b: 0.7, a: 1, frame: 2, add: true, grav: 6, drag: 0.8,
    }),
  fire: (radius = 0.35) => (e, P) => {
    const s = e.scale ?? 1;
    if (s <= 0.02) return;
    P.spawn({
      x: e.x + sr() * radius * s, y: e.y + R() * 0.1, z: e.z + sr() * radius * s,
      vx: sr() * 0.2, vy: (1.1 + R() * 1.2) * (0.5 + 0.5 * s), vz: sr() * 0.2,
      life: 0.5 + R() * 0.5, size: (0.5 + R() * 0.3) * s, size1: 0.08,
      r: 3.4, g: 1.3 + R() * 0.9, b: 0.25, a: 0.75, frame: 3, add: true, drag: 0.4,
    });
    if (R() < 0.3) {
      P.spawn({
        x: e.x + sr() * radius * s, y: e.y + 0.8 * s, z: e.z + sr() * radius * s,
        vx: sr() * 0.15, vy: 0.7 + R() * 0.5, vz: sr() * 0.15,
        life: 1.6 + R(), size: 0.3, size1: 1.2,
        r: 0.08, g: 0.07, b: 0.07, a: 0.32, frame: 1, drag: 0.3,
      });
    }
  },
  spray: () => (e, P) =>
    P.spawn({
      x: e.x, y: e.y, z: e.z,
      vx: e.dx * 5 + sr() * 0.5, vy: e.dy * 5 + sr() * 0.5, vz: e.dz * 5 + sr() * 0.5,
      life: 0.45 + R() * 0.35, size: 0.1, size1: 0.9,
      r: 0.9, g: 0.95, b: 1, a: 0.3, frame: 1, drag: 2.2,
    }),
  vent: () => (e, P) =>
    P.spawn({
      x: e.x + sr() * 0.05, y: e.y + sr() * 0.05, z: e.z + sr() * 0.05,
      vx: (e.dx || 0) * (2.5 + R() * 2) + sr() * 0.3, vy: (e.dy ?? 1) * (2.5 + R() * 2) + sr() * 0.3, vz: (e.dz || 0) * (2.5 + R() * 2) + sr() * 0.3,
      life: 0.7 + R() * 0.8, size: 0.08, size1: 0.9 + R() * 0.6,
      r: 0.8, g: 0.9, b: 1, a: 0.22, frame: 1, drag: 0.6,
    }),
  motes: (w = 2, h = 2, d = 2, color = [0.5, 0.8, 1]) => (e, P) =>
    P.spawn({
      x: e.x + sr() * w, y: e.y + sr() * h, z: e.z + sr() * d,
      vx: sr() * 0.05, vy: sr() * 0.05 + 0.02, vz: sr() * 0.05,
      life: 4 + R() * 4, size: 0.025 + R() * 0.03,
      r: color[0] * 2, g: color[1] * 2, b: color[2] * 2, a: 0.7, frame: 0, add: true,
    }),
  drip: () => (e, P) =>
    P.spawn({
      x: e.x + sr() * 0.02, y: e.y, z: e.z + sr() * 0.02,
      vy: -0.2, life: 0.75, size: 0.035, size1: 0.02,
      r: 0.7, g: 1.1, b: 1.4, a: 0.9, frame: 0, add: true, grav: 7,
    }),
  plasma: (radius = 0.5, height = 2) => (e, P) => {
    const a = R() * Math.PI * 2;
    const s = e.scale ?? 1;
    if (s <= 0.02) return;
    P.spawn({
      x: e.x + Math.cos(a) * radius * R(), y: e.y + sr() * height, z: e.z + Math.sin(a) * radius * R(),
      vx: sr() * 0.3, vy: sr() * 1.4, vz: sr() * 0.3,
      life: 0.5 + R() * 0.7, size: 0.12 + R() * 0.2, size1: 0.02,
      r: 1.2 * s, g: 2.6 * s, b: 4 * s, a: 0.9, frame: 0, add: true,
    });
  },
};
