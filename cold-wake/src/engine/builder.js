// Geometry builder. Rooms are assembled from thousands of primitives; this
// collects them per material, bakes the room's light fixtures into vertex
// attributes, and emits a handful of merged meshes per room.
import * as THREE from 'three';
import { hexLin } from '../core/utils.js';

const geoCache = new Map();
function cached(key, make) {
  let g = geoCache.get(key);
  if (!g) {
    g = make();
    geoCache.set(key, g);
  }
  return g;
}
const unitBox = (sx, sy, sz) => cached(`b${sx},${sy},${sz}`, () => new THREE.BoxGeometry(1, 1, 1, sx, sy, sz));
const unitCyl = (seg, top, open, hs) =>
  cached(`c${seg},${top},${open},${hs}`, () => new THREE.CylinderGeometry(top, 1, 1, seg, hs, open));
const unitSphere = (seg, half) =>
  cached(`s${seg},${half}`, () =>
    half
      ? new THREE.SphereGeometry(1, seg, Math.max(3, seg >> 2), 0, Math.PI * 2, 0, Math.PI / 2)
      : new THREE.SphereGeometry(1, seg, Math.max(4, (seg * 0.6) | 0))
  );
const unitPlane = (sx, sy) => cached(`p${sx},${sy}`, () => new THREE.PlaneGeometry(1, 1, sx, sy));

const _local = new THREE.Matrix4();
const _world = new THREE.Matrix4();
const _nm = new THREE.Matrix3();
const _v = new THREE.Vector3();
const _n = new THREE.Vector3();
const _q = new THREE.Quaternion();
const _e = new THREE.Euler();
const _p = new THREE.Vector3();
const _s = new THREE.Vector3();

// Glow descriptor: colour (sRGB hex), intensity (HDR multiplier),
// type (0 powered, 1 emergency, 2 always on, 3 painted), fx (0 steady, 1 flicker,
// 2 breathe, 3 blink, 4 chase, 5 shimmer).
export const G = (hex, i = 2, type = 0, fx = 0) => ({ hex, i, type, fx });

class Bucket {
  constructor() {
    this.pos = [];
    this.nrm = [];
    this.uv = [];
    this.idx = [];
    this.count = 0;
  }
}

class GlowBucket {
  constructor(withUv) {
    this.pos = [];
    this.col = [];
    this.gp = [];
    this.idx = [];
    this.uv = withUv ? [] : null;
    this.count = 0;
  }
}

let phaseSeed = 0.137;
const nextPhase = () => {
  phaseSeed = (phaseSeed * 7.31 + 0.173) % 1;
  return phaseSeed;
};

export class Builder {
  constructor(lib, roomIdx, colliders = null) {
    this.lib = lib;
    this.room = roomIdx;
    this.colliders = colliders;
    this.buckets = new Map();
    this.glow = new GlowBucket(false);
    this.signs = new GlowBucket(true);
    this.lights = [];
    this.stack = [new THREE.Matrix4()];
    this.segLen = 1.5;
  }

  get M() {
    return this.stack[this.stack.length - 1];
  }

  push(x = 0, y = 0, z = 0, ry = 0, rx = 0, rz = 0, s = 1) {
    _e.set(rx, ry, rz, 'YXZ');
    const m = new THREE.Matrix4().compose(_p.set(x, y, z), _q.setFromEuler(_e), _s.set(s, s, s));
    this.stack.push(this.M.clone().multiply(m));
    return this;
  }

  pop() {
    if (this.stack.length > 1) this.stack.pop();
    return this;
  }

  // Run fn with a local origin/rotation.
  at(x, y, z, ry, fn) {
    this.push(x, y, z, ry || 0);
    try {
      fn(this);
    } finally {
      this.pop();
    }
    return this;
  }

  // Local point -> world THREE.Vector3.
  wpos(x, y, z) {
    return new THREE.Vector3(x, y, z).applyMatrix4(this.M);
  }

  _bucket(key) {
    let b = this.buckets.get(key);
    if (!b) {
      if (!this.lib.m[key]) throw new Error(`Unknown material "${key}"`);
      b = new Bucket();
      this.buckets.set(key, b);
    }
    return b;
  }

  _compose(x, y, z, sx, sy, sz, o) {
    _e.set(o.rx || 0, o.ry || 0, o.rz || 0, 'YXZ');
    _local.compose(_p.set(x, y, z), _q.setFromEuler(_e), _s.set(sx, sy, sz));
    _world.multiplyMatrices(this.M, _local);
    _nm.getNormalMatrix(_world);
  }

  // uvMode: 0 project by local axis (boxes), 1 world projection, 2 native UVs scaled.
  _emit(key, geom, sx, sy, sz, uvMode, tile, su, sv, o) {
    const bk = this._bucket(key);
    const pos = geom.attributes.position;
    const nor = geom.attributes.normal;
    const uv = geom.attributes.uv;
    const base = bk.count;
    const it = 1 / tile;
    const uo = o.uo || 0;
    const vo = o.vo || 0;
    for (let i = 0; i < pos.count; i++) {
      _v.fromBufferAttribute(pos, i);
      _n.fromBufferAttribute(nor, i);
      let u = 0;
      let v = 0;
      if (uvMode === 0) {
        const ax = Math.abs(_n.x);
        const ay = Math.abs(_n.y);
        const az = Math.abs(_n.z);
        if (ay >= ax && ay >= az) {
          u = _v.x * sx * it;
          v = _v.z * sz * it;
        } else if (ax >= az) {
          u = _v.z * sz * it;
          v = _v.y * sy * it;
        } else {
          u = _v.x * sx * it;
          v = _v.y * sy * it;
        }
      } else if (uvMode === 2 && uv) {
        u = uv.getX(i) * su;
        v = uv.getY(i) * sv;
      }
      _v.applyMatrix4(_world);
      _n.applyMatrix3(_nm).normalize();
      if (uvMode === 1) {
        const ax = Math.abs(_n.x);
        const ay = Math.abs(_n.y);
        const az = Math.abs(_n.z);
        if (ay >= ax && ay >= az) {
          u = _v.x * it;
          v = _v.z * it;
        } else if (ax >= az) {
          u = _v.z * it;
          v = _v.y * it;
        } else {
          u = _v.x * it;
          v = _v.y * it;
        }
      }
      bk.pos.push(_v.x, _v.y, _v.z);
      bk.nrm.push(_n.x, _n.y, _n.z);
      bk.uv.push(u + uo, v + vo);
    }
    const index = geom.index;
    if (index) {
      for (let i = 0; i < index.count; i++) bk.idx.push(base + index.getX(i));
    } else {
      for (let i = 0; i < pos.count; i++) bk.idx.push(base + i);
    }
    bk.count += pos.count;
  }

  _emitGlow(g, geom, bucket = this.glow, uvRect = null) {
    const pos = geom.attributes.position;
    const uv = geom.attributes.uv;
    const base = bucket.count;
    const c = hexLin(g.hex, g.i);
    const ph = g.ph ?? nextPhase();
    for (let i = 0; i < pos.count; i++) {
      _v.fromBufferAttribute(pos, i).applyMatrix4(_world);
      bucket.pos.push(_v.x, _v.y, _v.z);
      bucket.col.push(c[0], c[1], c[2]);
      bucket.gp.push(this.room, g.type || 0, g.fx || 0, ph);
      if (bucket.uv) {
        const u = uv.getX(i);
        const v = uv.getY(i);
        bucket.uv.push(uvRect.u0 + (uvRect.u1 - uvRect.u0) * u, uvRect.v0 + (uvRect.v1 - uvRect.v0) * v);
      }
    }
    const index = geom.index;
    if (index) {
      for (let i = 0; i < index.count; i++) bucket.idx.push(base + index.getX(i));
    } else {
      for (let i = 0; i < pos.count; i++) bucket.idx.push(base + i);
    }
    bucket.count += pos.count;
  }

  _segs(len) {
    return len > this.segLen * 1.4 ? Math.min(24, Math.ceil(len / this.segLen)) : 1;
  }

  // ---- solid primitives ---------------------------------------------------

  box(mat, x, y, z, w, h, d, o = {}) {
    this._compose(x, y, z, w, h, d, o);
    const fine = o.fine !== false;
    const geom = unitBox(fine ? this._segs(w) : 1, fine ? this._segs(h) : 1, fine ? this._segs(d) : 1);
    const tile = o.tile || this.lib.tile[mat] || 1;
    if (o.uv === 'fit') this._emit(mat, geom, w, h, d, 2, 1, o.su || 1, o.sv || 1, o);
    else this._emit(mat, geom, w, h, d, o.uv === 'world' ? 1 : 0, tile, 1, 1, o);
    return this;
  }

  // Cylinder along Y by default; o.axis 'x' or 'z' lays it down.
  cyl(mat, x, y, z, r, h, o = {}) {
    const oo = { ...o };
    if (o.axis === 'x') oo.rz = (o.rz || 0) + Math.PI / 2;
    else if (o.axis === 'z') oo.rx = (o.rx || 0) + Math.PI / 2;
    this._compose(x, y, z, r, h, r, oo);
    const seg = o.seg || 12;
    const top = o.top ?? 1;
    const geom = unitCyl(seg, top, !!o.open, h > 2.4 ? Math.min(8, Math.ceil(h / 1.5)) : 1);
    const tile = o.tile || this.lib.tile[mat] || 1;
    this._emit(mat, geom, r, h, r, 2, 1, Math.max(1, Math.round((Math.PI * 2 * r) / tile)), h / tile, oo);
    return this;
  }

  sph(mat, x, y, z, r, o = {}) {
    this._compose(x, y, z, r * (o.sx || 1), r * (o.sy || 1), r * (o.sz || 1), o);
    this._emit(mat, unitSphere(o.seg || 12, !!o.half), 1, 1, 1, 2, 1, o.su || 1, o.sv || 1, o);
    return this;
  }

  tor(mat, x, y, z, R, r, o = {}) {
    const oo = { ...o };
    if (o.axis === 'y') oo.rx = (o.rx || 0) + Math.PI / 2;
    else if (o.axis === 'x') oo.ry = (o.ry || 0) + Math.PI / 2;
    this._compose(x, y, z, 1, 1, 1, oo);
    const geom = new THREE.TorusGeometry(R, r, o.rseg || 8, o.seg || 20, o.arc || Math.PI * 2);
    this._emit(mat, geom, 1, 1, 1, 2, 1, o.su || 4, o.sv || 1, oo);
    geom.dispose();
    return this;
  }

  // Pipe through a list of [x,y,z] points; straight runs with rounded corners.
  tube(mat, pts, r, o = {}) {
    const path = new THREE.CurvePath();
    const P = pts.map((p) => new THREE.Vector3(p[0], p[1], p[2]));
    const bend = o.bend ?? Math.max(r * 2.2, 0.12);
    let prev = P[0].clone();
    for (let i = 1; i < P.length; i++) {
      const cur = P[i];
      if (i < P.length - 1) {
        const next = P[i + 1];
        const dIn = cur.clone().sub(prev);
        const dOut = next.clone().sub(cur);
        const b = Math.min(bend, dIn.length() * 0.45, dOut.length() * 0.45);
        const a = cur.clone().addScaledVector(dIn.normalize(), -b);
        const c = cur.clone().addScaledVector(dOut.normalize(), b);
        if (a.distanceToSquared(prev) > 1e-6) path.add(new THREE.LineCurve3(prev, a));
        path.add(new THREE.QuadraticBezierCurve3(a, cur.clone(), c));
        prev = c;
      } else if (cur.distanceToSquared(prev) > 1e-6) {
        path.add(new THREE.LineCurve3(prev, cur));
      }
    }
    const len = path.getLength();
    const segs = Math.max(2, Math.min(200, Math.ceil(len / (o.step || 0.35)) + (P.length - 2) * 4));
    const geom = new THREE.TubeGeometry(path, segs, r, o.seg || 8, false);
    this._compose(0, 0, 0, 1, 1, 1, {});
    this._emit(mat, geom, 1, 1, 1, 2, 1, len / (o.tile || 1), 1, {});
    geom.dispose();
    return this;
  }

  // Any ready-made geometry (unit scale), positioned locally.
  geo(mat, geom, x = 0, y = 0, z = 0, o = {}) {
    const s = o.s || 1;
    this._compose(x, y, z, s * (o.sx || 1), s * (o.sy || 1), s * (o.sz || 1), o);
    this._emit(mat, geom, 1, 1, 1, 2, 1, o.su || 1, o.sv || 1, o);
    return this;
  }

  // Flat quad facing +Z before rotation (use rx: -PI/2 to lay it face-up).
  plane(mat, x, y, z, w, h, o = {}) {
    this._compose(x, y, z, w, h, 1, o);
    const geom = unitPlane(this._segs(w), this._segs(h));
    this._emit(mat, geom, 1, 1, 1, 2, 1, o.su || 1, o.sv || 1, o);
    return this;
  }

  // ---- glowing primitives -------------------------------------------------

  gbox(g, x, y, z, w, h, d, o = {}) {
    this._compose(x, y, z, w, h, d, o);
    this._emitGlow(g, unitBox(1, 1, 1));
    return this;
  }

  gcyl(g, x, y, z, r, h, o = {}) {
    const oo = { ...o };
    if (o.axis === 'x') oo.rz = (o.rz || 0) + Math.PI / 2;
    else if (o.axis === 'z') oo.rx = (o.rx || 0) + Math.PI / 2;
    this._compose(x, y, z, r, h, r, oo);
    this._emitGlow(g, unitCyl(o.seg || 10, o.top ?? 1, !!o.open, 1));
    return this;
  }

  gsph(g, x, y, z, r, o = {}) {
    this._compose(x, y, z, r * (o.sx || 1), r * (o.sy || 1), r * (o.sz || 1), o);
    this._emitGlow(g, unitSphere(o.seg || 10, false));
    return this;
  }

  gtor(g, x, y, z, R, r, o = {}) {
    const oo = { ...o };
    if (o.axis === 'y') oo.rx = (o.rx || 0) + Math.PI / 2;
    else if (o.axis === 'x') oo.ry = (o.ry || 0) + Math.PI / 2;
    this._compose(x, y, z, 1, 1, 1, oo);
    const geom = new THREE.TorusGeometry(R, r, 6, o.seg || 24, o.arc || Math.PI * 2);
    this._emitGlow(g, geom);
    geom.dispose();
    return this;
  }

  gtube(g, pts, r, o = {}) {
    const curve = new THREE.CatmullRomCurve3(
      pts.map((p) => new THREE.Vector3(p[0], p[1], p[2])),
      false,
      'catmullrom',
      o.tension ?? 0.1
    );
    const geom = new THREE.TubeGeometry(curve, Math.max(4, pts.length * 6), r, 6, false);
    this._compose(0, 0, 0, 1, 1, 1, {});
    this._emitGlow(g, geom);
    geom.dispose();
    return this;
  }

  // Lettering from the shared atlas. Height in metres; faces +Z before rotation.
  sign(text, x, y, z, h, o = {}) {
    const rect = this.lib.atlas.text(text, o);
    const w = h * rect.aspect;
    this._compose(x, y, z, w, h, 1, o);
    this._emitGlow(
      { hex: o.tint ?? 0xffffff, i: o.i ?? 1.6, type: o.type ?? 0, fx: o.fx ?? 0 },
      unitPlane(1, 1),
      this.signs,
      rect
    );
    return w;
  }

  // ---- lighting and collision --------------------------------------------

  // A fixture that is baked into the surrounding geometry.
  light(x, y, z, hex, intensity, range, o = {}) {
    _v.set(x, y, z).applyMatrix4(this.M);
    const c = hexLin(hex, intensity);
    this.lights.push({
      x: _v.x,
      y: _v.y,
      z: _v.z,
      r: c[0],
      g: c[1],
      b: c[2],
      range,
      emerg: !!o.emerg,
      key: !!o.key,
      hex,
      intensity,
    });
    return this;
  }

  // Blocks the player. Local rectangle on the floor plane.
  solid(x0, z0, x1, z1, o = {}) {
    if (!this.colliders) return this;
    let minX = Infinity;
    let minZ = Infinity;
    let maxX = -Infinity;
    let maxZ = -Infinity;
    for (const [x, z] of [
      [x0, z0],
      [x1, z0],
      [x0, z1],
      [x1, z1],
    ]) {
      _v.set(x, 0, z).applyMatrix4(this.M);
      minX = Math.min(minX, _v.x);
      maxX = Math.max(maxX, _v.x);
      minZ = Math.min(minZ, _v.z);
      maxZ = Math.max(maxZ, _v.z);
    }
    const c = { x0: minX, z0: minZ, x1: maxX, z1: maxZ, wall: !!o.wall, on: true, room: this.room };
    this.colliders.push(c);
    return c;
  }

  solidBox(x, z, w, d, o) {
    return this.solid(x - w / 2, z - d / 2, x + w / 2, z + d / 2, o);
  }

  // ---- output -------------------------------------------------------------

  // bakeMatrix: bake as if the geometry sat at this transform (for moving parts
  // built in local space). split: light each face from the room it faces.
  finalize({
    lights = this.lights,
    ambient = [0.05, 0.055, 0.065],
    ambientE = [0.012, 0.004, 0.004],
    name = 'room',
    bakeMatrix = null,
    split = null,
    sun = null,
  } = {}) {
    const group = new THREE.Group();
    group.name = name;
    for (const [key, bk] of this.buckets) {
      if (!bk.count) continue;
      const geom = new THREE.BufferGeometry();
      const pos = new Float32Array(bk.pos);
      const nrm = new Float32Array(bk.nrm);
      geom.setAttribute('position', new THREE.BufferAttribute(pos, 3));
      geom.setAttribute('normal', new THREE.BufferAttribute(nrm, 3));
      geom.setAttribute('uv', new THREE.BufferAttribute(new Float32Array(bk.uv), 2));
      let bp = pos;
      let bn = nrm;
      if (bakeMatrix) {
        bp = new Float32Array(pos.length);
        bn = new Float32Array(nrm.length);
        _nm.getNormalMatrix(bakeMatrix);
        for (let i = 0; i < bk.count; i++) {
          _v.fromArray(pos, i * 3).applyMatrix4(bakeMatrix).toArray(bp, i * 3);
          _n.fromArray(nrm, i * 3).applyMatrix3(_nm).normalize().toArray(bn, i * 3);
        }
      }
      const rooms = new Float32Array(bk.count).fill(this.room);
      let baked;
      if (split) {
        const A = bakeLight(bp, bn, bk.count, split.lightsPos, split.ambientPos || ambient, ambientE);
        const B = bakeLight(bp, bn, bk.count, split.lightsNeg, split.ambientNeg || ambient, ambientE);
        baked = A;
        for (let i = 0; i < bk.count; i++) {
          const i3 = i * 3;
          const side = bn[i3] * split.normal.x + bn[i3 + 1] * split.normal.y + bn[i3 + 2] * split.normal.z;
          if (side < -0.01) {
            for (let k = 0; k < 3; k++) {
              A.bake[i3 + k] = B.bake[i3 + k];
              A.bakeE[i3 + k] = B.bakeE[i3 + k];
            }
            rooms[i] = split.roomNeg;
          } else {
            rooms[i] = split.roomPos;
          }
        }
      } else {
        baked = bakeLight(bp, bn, bk.count, lights, ambient, ambientE, sun);
      }
      geom.setAttribute('bake', new THREE.BufferAttribute(baked.bake, 3));
      geom.setAttribute('bakeE', new THREE.BufferAttribute(baked.bakeE, 3));
      geom.setAttribute('roomIdx', new THREE.BufferAttribute(rooms, 1));
      geom.setIndex(bk.count > 65535 ? new THREE.BufferAttribute(new Uint32Array(bk.idx), 1) : new THREE.BufferAttribute(new Uint16Array(bk.idx), 1));
      geom.computeBoundingSphere();
      geom.computeBoundingBox();
      const mat = this.lib.m[key];
      const mesh = new THREE.Mesh(geom, mat);
      mesh.name = `${name}:${key}`;
      if (mat.transparent) mesh.renderOrder = 5;
      mesh.matrixAutoUpdate = false;
      group.add(mesh);
    }
    for (const [bucket, mat, order] of [
      [this.glow, this.lib.m.glow, 0],
      [this.signs, this.lib.m.sign, 6],
    ]) {
      if (!bucket.count) continue;
      const geom = new THREE.BufferGeometry();
      geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(bucket.pos), 3));
      geom.setAttribute('gcol', new THREE.BufferAttribute(new Float32Array(bucket.col), 3));
      geom.setAttribute('gp', new THREE.BufferAttribute(new Float32Array(bucket.gp), 4));
      if (bucket.uv) geom.setAttribute('uv', new THREE.BufferAttribute(new Float32Array(bucket.uv), 2));
      geom.setIndex(bucket.count > 65535 ? new THREE.BufferAttribute(new Uint32Array(bucket.idx), 1) : new THREE.BufferAttribute(new Uint16Array(bucket.idx), 1));
      geom.computeBoundingSphere();
      const mesh = new THREE.Mesh(geom, mat);
      mesh.name = `${name}:${order ? 'signs' : 'glow'}`;
      mesh.renderOrder = order;
      mesh.matrixAutoUpdate = false;
      group.add(mesh);
    }
    group.matrixAutoUpdate = false;
    // release build arrays
    this.buckets.clear();
    this.glow = new GlowBucket(false);
    this.signs = new GlowBucket(true);
    return group;
  }
}

// Per-vertex light accumulation. Soft wrap lighting with a smooth range
// window; emergency fixtures accumulate into their own channel.
// sun: optional { x, y, z, r, g, b } - a fixed directional light baked in as well.
export function bakeLight(pos, nrm, count, lights, ambient, ambientE, sun = null) {
  const bake = new Float32Array(count * 3);
  const bakeE = new Float32Array(count * 3);
  const L = lights.map((l) => ({ ...l, r2: l.range * l.range }));
  for (let i = 0; i < count; i++) {
    const i3 = i * 3;
    const px = pos[i3];
    const py = pos[i3 + 1];
    const pz = pos[i3 + 2];
    const nx = nrm[i3];
    const ny = nrm[i3 + 1];
    const nz = nrm[i3 + 2];
    const hemi = 0.7 + 0.3 * ny;
    let r = ambient[0] * hemi;
    let g = ambient[1] * hemi;
    let b = ambient[2] * hemi;
    if (sun) {
      const s = nx * sun.x + ny * sun.y + nz * sun.z;
      if (s > 0) {
        r += sun.r * s;
        g += sun.g * s;
        b += sun.b * s;
      }
    }
    let er = ambientE[0] * hemi;
    let eg = ambientE[1] * hemi;
    let eb = ambientE[2] * hemi;
    for (let j = 0; j < L.length; j++) {
      const l = L[j];
      const dx = l.x - px;
      const dy = l.y - py;
      const dz = l.z - pz;
      const d2 = dx * dx + dy * dy + dz * dz;
      if (d2 >= l.r2) continue;
      const d = Math.sqrt(d2) || 1e-4;
      const w = ((nx * dx + ny * dy + nz * dz) / d + 0.4) / 1.4;
      if (w <= 0) continue;
      let fall = 1 - d2 / l.r2;
      fall *= fall;
      const k = w * fall;
      if (l.emerg) {
        er += l.r * k;
        eg += l.g * k;
        eb += l.b * k;
      } else {
        r += l.r * k;
        g += l.g * k;
        b += l.b * k;
      }
    }
    // darken vertical surfaces where they meet the deck
    if (ny < 0.5 && ny > -0.5 && py < 0.5) {
      const t = Math.max(0, py) / 0.5;
      const ao = 0.72 + 0.28 * t;
      r *= ao;
      g *= ao;
      b *= ao;
    }
    bake[i3] = r;
    bake[i3 + 1] = g;
    bake[i3 + 2] = b;
    bakeE[i3] = er;
    bakeE[i3 + 1] = eg;
    bakeE[i3 + 2] = eb;
  }
  return { bake, bakeE };
}
