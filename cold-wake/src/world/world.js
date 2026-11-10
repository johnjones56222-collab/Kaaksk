// Builds the ship and runs everything that lives in it: room power states,
// doors, visibility, live screens, particles and the small real-time light rig.
import * as THREE from 'three';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { Builder } from '../engine/builder.js';
import { shared, MAX_ROOMS } from '../engine/materials.js';
import { Particles } from '../engine/particles.js';
import { Screens } from './screens.js';
import { Door } from './doors.js';
import { buildShell } from './shell.js';
import { ROOMS, DOORS, R, roomAt } from './layout.js';
import { clamp, damp, nextFrame } from '../core/utils.js';
import { bus } from '../core/events.js';

// Fluorescent-style strike: a few stutters, then steady.
function strike(t) {
  if (t <= 0) return 0;
  if (t >= 1.5) return 1;
  const steps = [0.0, 0.7, 0.08, 0.5, 0.05, 0.85, 0.3, 1.0];
  const i = Math.min(steps.length - 1, Math.floor((t / 1.5) * steps.length));
  return steps[i];
}

export class World {
  constructor(gfx, lib) {
    this.gfx = gfx;
    this.lib = lib;
    this.scene = gfx.scene;
    this.colliders = [];
    this.roomLights = [];
    this.rooms = [];
    this.doors = [];
    this.door = {};
    this.anchors = {};
    this.refs = {};
    this.syncers = [];
    this.globalTickers = [];
    this.screens = new Screens();
    this.particles = new Particles(this.scene, lib.sprites, Math.round(1500 * gfx.quality.particles));
    this.visible = new Set();
    this.current = null;
    this.time = 0;
    this.alarm = 0;
    this.alarmTarget = 0;
    this.powerScale = new Float32Array(MAX_ROOMS).fill(0);

    // real-time rig: hemisphere fill, sun (exterior only), flashlight, 3 pooled points
    this.hemi = new THREE.HemisphereLight(0x9db4d6, 0x1c2026, 0.22);
    this.scene.add(this.hemi);
    this.sun = new THREE.DirectionalLight(0xfff1dc, 0);
    this.sun.position.set(0.72, 0.42, -0.55).normalize();
    this.scene.add(this.sun);
    this.scene.add(this.sun.target);
    this.pool = [];
    for (let i = 0; i < 3; i++) {
      const l = new THREE.PointLight(0xffffff, 0, 11, 2);
      l.userData = { slot: null, fade: 0, fx: null };
      this.scene.add(l);
      this.pool.push(l);
    }
    this.fxLights = [];
  }

  async build(builders, progress) {
    const n = ROOMS.length - 1;
    for (let i = 1; i < ROOMS.length; i++) {
      this._buildRoom(ROOMS[i], builders[ROOMS[i].id]);
      progress?.(i / (n + 1), ROOMS[i].name);
      await nextFrame();
    }
    for (const def of DOORS) {
      const door = new Door(def, this);
      this.doors.push(door);
      this.door[def.id] = door;
      this.scene.add(door.group);
    }
    this.lib.atlas.texture.needsUpdate = true;
    progress?.(1, 'Doors');
  }

  _buildRoom(def, decorate) {
    const world = this;
    const b = new Builder(this.lib, def.index, this.colliders);
    const extras = new THREE.Group();
    const rec = {
      def,
      index: def.index,
      group: null,
      tickers: [],
      power: 0,
      target: 0,
      strikeT: 0,
      striking: false,
      flicker: 0,
      emitters: [],
    };
    const deferred = [];
    const screenParts = new Map();
    const ctx = {
      world,
      lib: this.lib,
      room: def,
      b,
      extras,
      info: null,
      shell(opts) {
        ctx.info = buildShell(b, def, opts);
        return ctx.info;
      },
      // named world-space point, given in the builder's current local frame
      anchor(name, x, y, z) {
        world.anchors[name] = b.wpos(x, y, z);
        return world.anchors[name];
      },
      ref(name, obj) {
        world.refs[name] = obj;
        return obj;
      },
      sync(fn) {
        world.syncers.push(fn);
      },
      tick(fn) {
        rec.tickers.push(fn);
      },
      // A moving part: built in its own local space, baked at its rest pose.
      dyn(x, y, z, ry, buildFn, o = {}) {
        const pivot = new THREE.Group();
        const m = b.M.clone().multiply(
          new THREE.Matrix4().compose(
            new THREE.Vector3(x, y, z),
            new THREE.Quaternion().setFromEuler(new THREE.Euler(o.rx || 0, ry || 0, o.rz || 0, 'YXZ')),
            new THREE.Vector3(1, 1, 1)
          )
        );
        m.decompose(pivot.position, pivot.quaternion, pivot.scale);
        extras.add(pivot);
        const db = new Builder(world.lib, def.index, null);
        buildFn(db);
        deferred.push(() => {
          pivot.updateMatrixWorld(true);
          pivot.add(db.finalize({ lights: world.roomLights[def.index], bakeMatrix: pivot.matrixWorld.clone(), name: o.name || 'dyn', ambient: rec.ambient }));
        });
        return pivot;
      },
      // A monitor tuned to a display channel.
      screen(channel, x, y, z, w, h, o = {}) {
        const mat = world.screens.material(channel, def.index, !!o.always, o.gain ?? 1.35);
        const g = new THREE.PlaneGeometry(w, h);
        const m = b.M.clone().multiply(
          new THREE.Matrix4().compose(
            new THREE.Vector3(x, y, z),
            new THREE.Quaternion().setFromEuler(new THREE.Euler(o.rx || 0, o.ry || 0, o.rz || 0, 'YXZ')),
            new THREE.Vector3(1, 1, 1)
          )
        );
        g.applyMatrix4(m);
        if (!screenParts.has(mat)) screenParts.set(mat, []);
        screenParts.get(mat).push(g);
      },
      emitter(e, x, y, z) {
        const p = b.wpos(x, y, z);
        const em = world.particles.addEmitter({ ...e, x: p.x, y: p.y, z: p.z, room: def.index });
        rec.emitters.push(em);
        return em;
      },
      add(obj) {
        extras.add(obj);
        return obj;
      },
    };
    rec.ambient = [0.05, 0.055, 0.065];
    ctx.setAmbient = (a) => {
      rec.ambient = a;
    };
    if (decorate) decorate(ctx);
    else ctx.shell({});
    this.roomLights[def.index] = b.lights;
    const group = b.finalize({ name: def.id, ambient: rec.ambient });
    for (const fn of deferred) fn();
    for (const [mat, parts] of screenParts) {
      const merged = mergeGeometries(parts, false);
      for (const p of parts) p.dispose();
      const mesh = new THREE.Mesh(merged, mat);
      mesh.matrixAutoUpdate = false;
      mesh.name = `${def.id}:screens`;
      extras.add(mesh);
    }
    group.add(extras);
    // static meshes never move: freeze their matrices
    group.updateMatrixWorld(true);
    rec.group = group;
    rec.extras = extras;
    group.visible = false;
    this.scene.add(group);
    this.rooms[def.index] = rec;
  }

  room(id) {
    return this.rooms[R[id].index];
  }

  // delay staggers the strike so a deck lights up in a wave from the reactor
  setPower(id, on, instant = false, delay = 0) {
    const r = this.room(id);
    const t = on ? 1 : 0;
    if (r.target === t && !instant) return;
    r.target = t;
    r.striking = false;
    if (instant) {
      r.power = t;
    } else if (on) {
      r.striking = true;
      r.strikeT = -delay;
    }
  }

  sync() {
    for (const fn of this.syncers) {
      try {
        fn();
      } catch (err) {
        console.error('[world.sync]', err);
      }
    }
  }

  // Temporary real-time light (weld arc, explosion, muzzle flash).
  flash(x, y, z, color, intensity, duration = 0.25, distance = 9) {
    this.fxLights.push({ x, y, z, color, intensity, t: 0, duration, distance });
    if (this.fxLights.length > 4) this.fxLights.shift();
  }

  // 2D sight test against walls and closed doors.
  blocked(ax, az, bx, bz) {
    const dx = bx - ax;
    const dz = bz - az;
    for (const c of this.colliders) {
      if (!c.on || !c.wall) continue;
      let t0 = 0;
      let t1 = 1;
      let hit = true;
      for (const [p, d, lo, hi] of [
        [ax, dx, c.x0, c.x1],
        [az, dz, c.z0, c.z1],
      ]) {
        if (Math.abs(d) < 1e-9) {
          if (p < lo || p > hi) {
            hit = false;
            break;
          }
        } else {
          let a = (lo - p) / d;
          let b2 = (hi - p) / d;
          if (a > b2) [a, b2] = [b2, a];
          t0 = Math.max(t0, a);
          t1 = Math.min(t1, b2);
          if (t0 > t1) {
            hit = false;
            break;
          }
        }
      }
      if (hit) return true;
    }
    return false;
  }

  update(dt, px, pz, camPos) {
    this.time += dt;
    const t = this.time;
    shared.uTime.value = t;

    const here = roomAt(px, pz);
    if (here && here !== this.current) {
      const prev = this.current;
      this.current = here;
      bus.emit('room', here, prev);
    }
    const cur = this.current;
    const exterior = !!(cur && cur.exterior);

    for (const d of this.doors) d.update(dt, px, pz, !exterior);

    // flood visibility through open doors
    const vis = this.visible;
    vis.clear();
    if (cur) {
      vis.add(cur.index);
      if (!exterior) {
        const queue = [cur.id];
        while (queue.length) {
          const id = queue.pop();
          for (const d of this.doors) {
            if (d.open <= 0.01) continue;
            const other = d.def.a === id ? d.def.b : d.def.b === id ? d.def.a : null;
            if (!other) continue;
            const idx = R[other].index;
            if (!vis.has(idx)) {
              vis.add(idx);
              queue.push(other);
            }
          }
        }
      }
    }
    for (let i = 1; i < this.rooms.length; i++) this.rooms[i].group.visible = vis.has(i);
    for (const d of this.doors) d.group.visible = vis.has(d.roomA.index) || vis.has(d.roomB.index);

    // power levels -> shader uniforms
    this.alarm = damp(this.alarm, this.alarmTarget, 3, dt);
    const pw = shared.uRoomPower.value;
    const em = shared.uRoomEmerg.value;
    for (let i = 1; i < this.rooms.length; i++) {
      const r = this.rooms[i];
      if (r.striking) {
        r.strikeT += dt;
        r.power = strike(r.strikeT);
        if (r.strikeT >= 1.5) r.striking = false;
      } else {
        r.power = damp(r.power, r.target, 5, dt);
      }
      let p = r.power;
      if (r.flicker > 0) {
        r.flicker -= dt;
        p *= 0.55 + 0.45 * Math.abs(Math.sin(t * 37 + i) * Math.sin(t * 11.3));
      }
      const pulse = 0.5 + 0.5 * Math.sin(t * 2.6 + i * 1.3);
      const alarmPulse = Math.pow(0.5 + 0.5 * Math.sin(t * 5.2), 2);
      pw[i] = p;
      em[i] = Math.max((1 - p) * (0.45 + 0.55 * pulse * pulse), this.alarm * (0.25 + 0.75 * alarmPulse));
      this.powerScale[i] = p;
    }
    pw[0] = 1;
    em[0] = 0;

    for (const i of vis) {
      const r = this.rooms[i];
      for (const fn of r.tickers) fn(dt, t);
    }
    for (const fn of this.globalTickers) fn(dt, t);

    this.screens.update(t, this.powerScale, this.gfx.quality.name === 'low' ? 1 : 2);
    this.particles.update(dt, vis, this.gfx.quality.particles);
    this.particles.material.uniforms.uScale.value =
      (this.gfx.renderer.domElement.height * 0.5) / Math.tan((this.gfx.camera.fov * Math.PI) / 360);

    // ambient rig follows the room the player is in
    const curPower = cur ? this.rooms[cur.index].power : 0;
    const scene = this.scene;
    scene.environmentIntensity = damp(scene.environmentIntensity, exterior ? 0.12 : 0.07 + 0.3 * curPower, 4, dt);
    this.hemi.intensity = damp(this.hemi.intensity, exterior ? 0.1 : 0.07 + 0.2 * curPower, 4, dt);
    this.sun.intensity = damp(this.sun.intensity, exterior ? 3.4 : 0, 6, dt);
    scene.fog.density = damp(scene.fog.density, exterior ? 0.0006 : 0.011, 4, dt);

    this._updatePool(dt, camPos, cur, curPower);
  }

  _updatePool(dt, camPos, cur, curPower) {
    // age effect lights
    for (let i = this.fxLights.length - 1; i >= 0; i--) {
      const f = this.fxLights[i];
      f.t += dt;
      if (f.t >= f.duration) this.fxLights.splice(i, 1);
    }
    const fx = this.fxLights;
    // nearest key fixtures to the camera, re-picked a few times a second
    this._poolT = (this._poolT || 0) - dt;
    if (this._poolT <= 0 || this._poolRoom !== cur) {
      this._poolT = 0.2;
      this._poolRoom = cur;
      const keys = cur && this.roomLights[cur.index] ? this.roomLights[cur.index].filter((l) => l.key) : [];
      this._cands = keys;
      if (keys.length > 3) {
        this._cands = keys
          .map((l) => ({ l, d: (l.x - camPos.x) ** 2 + (l.z - camPos.z) ** 2 }))
          .sort((p, q) => p.d - q.d)
          .slice(0, 3)
          .map((o) => o.l);
      }
    }
    const cands = this._cands || [];
    const nFx = Math.min(fx.length, 2);
    // keep lights on the fixture they already have; hand the rest to free lights
    const free = cands.slice();
    for (let i = nFx; i < this.pool.length; i++) {
      const u = this.pool[i].userData;
      const k = u.slot ? free.indexOf(u.slot) : -1;
      if (k >= 0) {
        free.splice(k, 1);
        u.want = u.slot;
      } else {
        u.want = undefined;
      }
    }
    for (let i = nFx; i < this.pool.length; i++) {
      const u = this.pool[i].userData;
      if (u.want === undefined) u.want = free.shift() || null;
    }
    for (let i = 0; i < this.pool.length; i++) {
      const L = this.pool[i];
      const u = L.userData;
      if (i < nFx) {
        const f = fx[fx.length - 1 - i];
        const k = 1 - f.t / f.duration;
        L.position.set(f.x, f.y, f.z);
        L.color.setHex(f.color);
        L.intensity = f.intensity * k * k;
        L.distance = f.distance;
        u.slot = null;
        u.fade = 0;
        continue;
      }
      const want = u.want;
      if (u.slot !== want) {
        u.fade -= dt * 4;
        if (u.fade <= 0) {
          u.fade = 0;
          u.slot = want;
          if (want) {
            L.position.set(want.x, want.y, want.z);
            L.color.setHex(want.hex);
            L.distance = Math.max(8, want.range * 1.2);
          }
        }
      } else if (want) {
        u.fade = Math.min(1, u.fade + dt * 3);
      }
      L.intensity = u.slot ? u.fade * curPower * clamp(u.slot.intensity, 0.4, 1.6) * 9 : 0;
    }
  }
}
