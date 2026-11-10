// First-person controller: look, walk, sprint, hop, head-bob, flashlight and
// circle-vs-box collision on the deck plan.
import * as THREE from 'three';
import { input, lookScale } from '../engine/input.js';
import { settings, state } from '../core/state.js';
import { clamp, damp } from '../core/utils.js';
import { bus } from '../core/events.js';

export class Player {
  constructor(world, gfx) {
    this.world = world;
    this.gfx = gfx;
    this.camera = gfx.camera;
    this.x = 0;
    this.z = 0;
    this.y = 0;
    this.vy = 0;
    this.vx = 0;
    this.vz = 0;
    this.yaw = 0;
    this.pitch = 0;
    this.radius = 0.34;
    this.eye = 1.66;
    this.bob = 0;
    this.bobAmp = 0;
    this.stepPhase = 0;
    this.speed = 0;
    this.grounded = true;
    this.frozen = false;
    this.eva = false;
    this.shake = 0;
    this.kick = 0;
    this.health = 100;
    this.hurtT = 0;
    this.fovKick = 0;
    this.lean = 0;
    this.boost = 0;

    // the lamp sits a little behind the eye so close surfaces do not blow out
    this.lamp = new THREE.SpotLight(0xdcecff, 0, 34, 0.5, 0.6, 1.5);
    this.lamp.position.set(0.2, -0.12, 1.3);
    this.lampTarget = new THREE.Object3D();
    this.lampTarget.position.set(0, -0.05, -6);
    this.camera.add(this.lamp);
    this.camera.add(this.lampTarget);
    this.lamp.target = this.lampTarget;
    this.lampOn = false;
    this.lampLevel = 0;
  }

  place(x, z, yaw = this.yaw, pitch = 0) {
    this.x = x;
    this.z = z;
    this.y = 0;
    this.vx = this.vz = this.vy = 0;
    this.yaw = yaw;
    this.pitch = pitch;
    this.syncCamera(0);
  }

  toggleLamp(force) {
    this.lampOn = force ?? !this.lampOn;
    bus.emit('lamp', this.lampOn);
  }

  damage(amount) {
    if (amount <= 0) return;
    this.health = Math.max(0, this.health - amount);
    this.hurtT = 1.2;
    bus.emit('hurt', amount);
  }

  update(dt) {
    const world = this.world;
    // ---- look ----
    if (input.capture && !this.frozen) {
      const s = lookScale();
      this.yaw -= input.lookX * s;
      this.pitch -= input.lookY * s * (settings.invertY ? -1 : 1);
      this.pitch = clamp(this.pitch, -1.5, 1.5);
    }
    // ---- move ----
    let mx = 0;
    let my = 0;
    if (input.capture && !this.frozen) {
      mx = input.moveX;
      my = input.moveY;
      const len = Math.hypot(mx, my);
      if (len > 1) {
        mx /= len;
        my /= len;
      }
    }
    const sin = Math.sin(this.yaw);
    const cos = Math.cos(this.yaw);
    const wishX = -sin * my + cos * mx;
    const wishZ = -cos * my - sin * mx;
    const sprinting = input.sprint && my > 0.2 && !this.eva;
    this.boost = Math.max(0, this.boost - dt);
    const max = (this.eva ? 3.1 : sprinting ? 6.8 : 4.1) * (this.boost > 0 ? 1.15 : 1);
    const accel = this.grounded ? 11 : 3;
    this.vx = damp(this.vx, wishX * max, accel, dt);
    this.vz = damp(this.vz, wishZ * max, accel, dt);
    if (input.capture && !this.frozen && input.pressed('jump') && this.grounded && !this.eva) {
      this.vy = 4.3;
      this.grounded = false;
      bus.emit('jump');
    }
    // integrate in small steps so thin colliders are never skipped
    const speed = Math.hypot(this.vx, this.vz);
    const steps = Math.max(1, Math.ceil((speed * dt) / 0.12));
    const sdt = dt / steps;
    const ox = this.x;
    const oz = this.z;
    for (let i = 0; i < steps; i++) {
      this.x += this.vx * sdt;
      this.z += this.vz * sdt;
      this._collide();
    }
    const moved = Math.hypot(this.x - ox, this.z - oz);
    this.speed = dt > 0 ? moved / dt : 0;
    state.stats.distance += moved;
    if (!this.grounded) {
      this.vy -= 12.5 * dt;
      this.y += this.vy * dt;
      if (this.y <= 0) {
        this.y = 0;
        this.vy = 0;
        this.grounded = true;
        bus.emit('land');
      }
    }
    // ---- footsteps and bob ----
    const moving = this.grounded && this.speed > 0.6;
    if (moving) {
      const stride = this.eva ? 1.25 : sprinting ? 2.5 : 1.9;
      const prev = this.stepPhase;
      this.stepPhase += (this.speed / stride) * dt;
      if (Math.floor(prev * 2) !== Math.floor(this.stepPhase * 2)) bus.emit('step', sprinting);
    }
    this.bobAmp = damp(this.bobAmp, moving ? Math.min(1, this.speed / 4) : 0, 8, dt);
    this.fovKick = damp(this.fovKick, sprinting && moving ? 6 : 0, 6, dt);
    this.lean = damp(this.lean, input.capture ? -mx * 0.012 : 0, 6, dt);
    // slow regeneration
    if (this.hurtT > 0) this.hurtT -= dt;
    else if (this.health < 100) this.health = Math.min(100, this.health + dt * 6);
    this.shake = Math.max(0, this.shake - dt * 1.8);
    // flashlight
    this.lampLevel = damp(this.lampLevel, this.lampOn ? 1 : 0, 14, dt);
    this.lamp.intensity = this.lampLevel * 22;
    this.syncCamera(dt);
    state.pos = { x: this.x, z: this.z, yaw: this.yaw };
    void world;
  }

  syncCamera() {
    const bobOn = settings.bob ? 1 : 0;
    const ph = this.stepPhase * Math.PI * 2;
    const by = Math.sin(ph * 2) * 0.032 * this.bobAmp * bobOn;
    const bx = Math.cos(ph) * 0.02 * this.bobAmp * bobOn;
    let sx = 0;
    let sy = 0;
    let sr = 0;
    if (this.shake > 0) {
      const s = this.shake * this.shake;
      sx = (Math.random() - 0.5) * 0.08 * s;
      sy = (Math.random() - 0.5) * 0.08 * s;
      sr = (Math.random() - 0.5) * 0.03 * s;
    }
    const cos = Math.cos(this.yaw);
    const sin = Math.sin(this.yaw);
    this.camera.position.set(this.x + cos * bx + sx, this.y + this.eye + by + sy, this.z - sin * bx);
    this.camera.rotation.set(this.pitch + sr, this.yaw, this.lean + sr);
    if (Math.abs(this.fovKick - (this._lastKick || 0)) > 0.05) {
      this._lastKick = this.fovKick;
      this.gfx.applyFov(this.fovKick);
    }
  }

  _collide() {
    const r = this.radius;
    const cols = this.world.colliders;
    for (let pass = 0; pass < 2; pass++) {
      for (let i = 0; i < cols.length; i++) {
        const c = cols[i];
        if (!c.on) continue;
        if (this.x < c.x0 - r || this.x > c.x1 + r || this.z < c.z0 - r || this.z > c.z1 + r) continue;
        const nx = clamp(this.x, c.x0, c.x1);
        const nz = clamp(this.z, c.z0, c.z1);
        let dx = this.x - nx;
        let dz = this.z - nz;
        const d2 = dx * dx + dz * dz;
        if (d2 >= r * r) continue;
        if (d2 > 1e-10) {
          const d = Math.sqrt(d2);
          const push = r - d;
          this.x += (dx / d) * push;
          this.z += (dz / d) * push;
        } else {
          // centre is inside the box: leave through the nearest face
          const l = this.x - c.x0;
          const rr = c.x1 - this.x;
          const t = this.z - c.z0;
          const b = c.z1 - this.z;
          const m = Math.min(l, rr, t, b);
          if (m === l) this.x = c.x0 - r;
          else if (m === rr) this.x = c.x1 + r;
          else if (m === t) this.z = c.z0 - r;
          else this.z = c.z1 + r;
        }
      }
    }
  }
}
