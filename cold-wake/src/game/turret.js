// Point-defence gunnery: three waves of ring debris fall towards the ship and
// the player burns them down from the dorsal turret camera.
import * as THREE from 'three';
import { game } from './ctx.js';
import { input } from '../engine/input.js';
import { audio } from '../engine/audio.js';
import { hud } from '../ui/hud.js';
import { say } from './dialogue.js';
import { setFlag, state } from '../core/state.js';
import { rockGeometry } from '../world/space.js';
import { clamp, damp } from '../core/utils.js';

const WAVES = [
  { count: 8, gap: 1.5, speed: [34, 46] },
  { count: 12, gap: 1.1, speed: [40, 56] },
  { count: 16, gap: 0.85, speed: [46, 66] },
];
const TOTAL = WAVES.reduce((s, w) => s + w.count, 0);
const _ray = new THREE.Raycaster();
const _v = new THREE.Vector3();
const _ndc = new THREE.Vector2();

export class Turret {
  constructor() {
    const scene = game.gfx.scene;
    this.group = new THREE.Group();
    this.group.visible = false;
    scene.add(this.group);
    const geos = [rockGeometry(11, 2), rockGeometry(23, 2), rockGeometry(37, 2)];
    const mat = new THREE.MeshStandardMaterial({ color: 0x6b6058, roughness: 0.95, metalness: 0.05 });
    const hot = new THREE.MeshStandardMaterial({ color: 0x6b6058, roughness: 0.95, metalness: 0.05, emissive: 0xff5a20, emissiveIntensity: 0.9 });
    this.mats = { mat, hot };
    this.rocks = [];
    for (let i = 0; i < 30; i++) {
      const m = new THREE.Mesh(geos[i % 3], mat);
      m.visible = false;
      this.group.add(m);
      this.rocks.push({ mesh: m, on: false, vel: new THREE.Vector3(), r: 1, hp: 1, spin: new THREE.Vector3(), hit: 0 });
    }
    const beamMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(6, 2.2, 0.8), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false });
    const beamGeo = new THREE.CylinderGeometry(0.22, 0.22, 1, 6, 1, true);
    beamGeo.rotateX(Math.PI / 2);
    beamGeo.translate(0, 0, 0.5);
    this.beams = [new THREE.Mesh(beamGeo, beamMat), new THREE.Mesh(beamGeo, beamMat)];
    for (const b of this.beams) {
      b.visible = false;
      b.frustumCulled = false;
      this.group.add(b);
    }
    this.beamT = 0;
    this.active = false;
    this.el = {
      ui: document.getElementById('turret-ui'),
      reticle: document.getElementById('turret-reticle'),
      hull: document.getElementById('turret-hull'),
      heat: document.getElementById('turret-heat'),
      wave: document.getElementById('turret-wave'),
      count: document.getElementById('turret-count'),
      msg: document.getElementById('turret-msg'),
    };
  }

  start() {
    const p = game.player;
    const cam = game.world.anchors.turretCam;
    this.saved = { x: p.x, z: p.z, yaw: p.yaw, pitch: p.pitch, lamp: p.lampOn };
    p.toggleLamp(false);
    p.frozen = true;
    p.x = cam.x;
    p.z = cam.z;
    this.cam = cam;
    this.yaw = 0;
    this.pitch = 0.1;
    this.active = true;
    this.group.visible = true;
    this.hull = 100;
    this.heat = 0;
    this.locked = false;
    this.cool = 0;
    this.kills = 0;
    this.wave = 0;
    this.spawned = 0;
    this.spawnT = 2.5;
    this.intermission = 0;
    this.finishT = -1;
    this.shake = 0;
    for (const r of this.rocks) this._kill(r);
    input.aim.x = 0;
    input.aim.y = 0;
    input.aimOnly = true;
    game.mode = 'turret';
    hud.setLayout('turret');
    this.el.ui.hidden = false;
    this._msg(input.mode === 'touch' ? 'TOUCH A TARGET TO FIRE' : 'AIM WITH THE MOUSE - HOLD TO FIRE', 3.2);
    say('VEGA', 'Turret is yours. Short bursts, the capacitor overheats. Do not let them reach the hull.', { now: true });
  }

  _msg(text, seconds = 2) {
    this.el.msg.textContent = text;
    this.msgT = seconds;
  }

  _kill(r) {
    r.on = false;
    r.mesh.visible = false;
  }

  _spawn(w) {
    const r = this.rocks.find((k) => !k.on);
    if (!r) return;
    const c = this.cam;
    const az = (Math.random() - 0.5) * 1.15;
    const el = -0.1 + Math.random() * 0.5;
    const dist = 400 + Math.random() * 120;
    r.mesh.position.set(c.x + Math.sin(az) * Math.cos(el) * dist, c.y + Math.sin(el) * dist, c.z - Math.cos(az) * Math.cos(el) * dist);
    r.r = 3.5 + Math.random() * Math.random() * 7;
    r.hp = r.r > 6.5 ? 2 : 1;
    r.mesh.scale.setScalar(r.r);
    r.mesh.material = this.mats.mat;
    const speed = w.speed[0] + Math.random() * (w.speed[1] - w.speed[0]);
    _v.set(c.x + (Math.random() - 0.5) * 26, c.y - 2 + (Math.random() - 0.5) * 8, c.z + (Math.random() - 0.5) * 30).sub(r.mesh.position).normalize();
    r.vel.copy(_v).multiplyScalar(speed);
    r.spin.set(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).multiplyScalar(1.4);
    r.hit = 0;
    r.on = true;
    r.mesh.visible = true;
  }

  _explode(r, big) {
    const P = game.world.particles;
    const p = r.mesh.position;
    const s = r.r;
    const n = big ? 26 : 10;
    for (let i = 0; i < n; i++) {
      const a = Math.random() * Math.PI * 2;
      const b = Math.random() * Math.PI - Math.PI / 2;
      const sp = (8 + Math.random() * 30) * (big ? 1 : 0.5);
      P.spawn({
        x: p.x, y: p.y, z: p.z,
        vx: Math.cos(a) * Math.cos(b) * sp + r.vel.x * 0.4, vy: Math.sin(b) * sp + r.vel.y * 0.4, vz: Math.sin(a) * Math.cos(b) * sp + r.vel.z * 0.4,
        life: 0.5 + Math.random() * 0.9, size: s * (1.2 + Math.random() * 1.8), size1: s * 0.3,
        r: 3.5, g: 1.6 + Math.random(), b: 0.5, a: 0.9, frame: i % 2 ? 3 : 0, add: true, drag: 1.2,
      });
    }
    if (big) {
      for (let i = 0; i < 8; i++) {
        P.spawn({
          x: p.x, y: p.y, z: p.z,
          vx: (Math.random() - 0.5) * 14, vy: (Math.random() - 0.5) * 14, vz: (Math.random() - 0.5) * 14,
          life: 1.6 + Math.random(), size: s * 2, size1: s * 5,
          r: 0.25, g: 0.2, b: 0.18, a: 0.35, frame: 1, drag: 0.8,
        });
      }
    }
    const d = p.distanceTo(game.gfx.camera.position);
    audio.sfx('explosion', { vol: clamp(1.1 - d / 480, 0.2, 1) * (big ? 1 : 0.5), pan: clamp(_v.copy(p).project(game.gfx.camera).x, -1, 1) });
  }

  _fire() {
    const cam = game.gfx.camera;
    _ndc.set(input.aim.x, input.aim.y);
    _ray.setFromCamera(_ndc, cam);
    const o = _ray.ray.origin;
    const d = _ray.ray.direction;
    let best = null;
    let bestT = Infinity;
    for (const r of this.rocks) {
      if (!r.on) continue;
      _v.copy(r.mesh.position).sub(o);
      const t = _v.dot(d);
      if (t < 0) continue;
      const miss2 = _v.lengthSq() - t * t;
      const rad = r.r * 1.45 + t * 0.012;
      if (miss2 < rad * rad && t < bestT) {
        bestT = t;
        best = r;
      }
    }
    const end = best ? best.mesh.position.clone() : o.clone().addScaledVector(d, 700);
    // tracer from both barrels
    this.beams.forEach((b, i) => {
      b.position.set(this.cam.x + (i ? 0.6 : -0.6), this.cam.y - 1.6, this.cam.z - 4);
      b.lookAt(end);
      b.scale.set(1, 1, b.position.distanceTo(end));
      b.visible = true;
    });
    this.beamT = 0.07;
    audio.sfx('laser', { pan: input.aim.x * 0.6 });
    this.shake = Math.min(1, this.shake + 0.12);
    if (best) {
      best.hp--;
      best.hit = 0.15;
      if (best.hp <= 0) {
        this._explode(best, true);
        this._kill(best);
        this.kills++;
      } else {
        this._explode(best, false);
        best.mesh.material = this.mats.hot;
      }
    }
  }

  update(dt) {
    if (!this.active) return;
    const cam = game.gfx.camera;
    const W = WAVES[Math.min(this.wave, WAVES.length - 1)];
    // camera pans a little towards the reticle
    this.yaw = damp(this.yaw, -input.aim.x * 0.42, 5, dt);
    this.pitch = damp(this.pitch, 0.1 + input.aim.y * 0.26, 5, dt);
    this.shake = Math.max(0, this.shake - dt * 2.4);
    const sx = (Math.random() - 0.5) * 0.5 * this.shake * this.shake;
    const sy = (Math.random() - 0.5) * 0.5 * this.shake * this.shake;
    cam.position.set(this.cam.x + sx, this.cam.y + sy, this.cam.z);
    cam.rotation.set(this.pitch + sy * 0.02, this.yaw + sx * 0.02, 0);
    // reticle in screen space
    const w = game.gfx.width;
    const h = game.gfx.height;
    this.el.reticle.style.transform = `translate(${((input.aim.x * 0.5 + 0.5) * w).toFixed(1)}px, ${((-input.aim.y * 0.5 + 0.5) * h).toFixed(1)}px)`;
    // waves
    if (this.finishT >= 0) {
      this.finishT -= dt;
      if (this.finishT <= 0) this.end(true);
    } else if (this.intermission > 0) {
      this.intermission -= dt;
      if (this.intermission <= 0) this._msg(`WAVE ${this.wave + 1}`, 1.6);
    } else if (this.spawned < W.count) {
      this.spawnT -= dt;
      if (this.spawnT <= 0) {
        this.spawnT = W.gap * (0.7 + Math.random() * 0.6);
        this._spawn(W);
        this.spawned++;
      }
    } else if (!this.rocks.some((r) => r.on)) {
      this.wave++;
      this.spawned = 0;
      if (this.wave >= WAVES.length) {
        this.finishT = 2.2;
        this._msg('FIELD CLEAR', 3);
        audio.sfx('mission');
      } else {
        this.intermission = 2.6;
        this._msg('WAVE CLEAR', 1.6);
        audio.sfx('objective');
        this.hull = Math.min(100, this.hull + 15);
      }
    }
    // firing and heat
    this.cool -= dt;
    const trigger = (input.held.fire || input.held.alt) && this.finishT < 0;
    if (this.locked && this.heat < 0.45) this.locked = false;
    if (trigger && !this.locked && this.cool <= 0) {
      this.cool = 0.15;
      this.heat += 0.062;
      this._fire();
      if (this.heat >= 1) {
        this.locked = true;
        this._msg('CAPACITOR OVERHEAT', 1.4);
        audio.sfx('deny');
      }
    }
    this.heat = Math.max(0, this.heat - dt * (trigger && !this.locked ? 0.08 : 0.42));
    if (this.beamT > 0) {
      this.beamT -= dt;
      if (this.beamT <= 0) for (const b of this.beams) b.visible = false;
    }
    // debris
    const cx = this.cam.x;
    const cy = this.cam.y;
    const cz = this.cam.z;
    for (const r of this.rocks) {
      if (!r.on) continue;
      const m = r.mesh;
      m.position.addScaledVector(r.vel, dt);
      m.rotation.x += r.spin.x * dt;
      m.rotation.y += r.spin.y * dt;
      m.rotation.z += r.spin.z * dt;
      const d = Math.hypot(m.position.x - cx, m.position.y - cy, m.position.z - cz);
      if (d < 26 + r.r || m.position.z > cz + 10) {
        this._explode(r, true);
        this._kill(r);
        this.hull -= 9 + r.r * 1.3;
        this.shake = 1;
        audio.sfx('impact');
        hud.flash(0.25);
        game.world.flash(cx, cy, cz - 8, 0xff5a20, 60, 0.4, 60);
        if (this.hull <= 0) this._fail();
      }
    }
    // HUD
    this.el.hull.style.transform = `scaleX(${clamp(this.hull / 100, 0, 1)})`;
    this.el.hull.style.background = this.hull < 35 ? 'var(--bad)' : this.hull < 65 ? 'var(--warn)' : 'var(--good)';
    this.el.heat.style.transform = `scaleX(${clamp(this.heat, 0, 1)})`;
    this.el.heat.style.background = this.locked ? 'var(--bad)' : this.heat > 0.7 ? 'var(--warn)' : 'var(--accent)';
    this.el.wave.textContent = `WAVE ${Math.min(this.wave + 1, WAVES.length)} / ${WAVES.length}`;
    this.el.count.textContent = `${this.kills} / ${TOTAL} DESTROYED`;
    if (this.msgT > 0) {
      this.msgT -= dt;
      if (this.msgT <= 0) this.el.msg.textContent = '';
    }
  }

  _fail() {
    state.stats.retries++;
    for (const r of this.rocks) this._kill(r);
    this.hull = 100;
    this.spawned = 0;
    this.spawnT = 3;
    this.heat = 0;
    this.locked = false;
    this._msg('HULL CRITICAL - RESETTING WAVE', 2.6);
    say('VEGA', 'Too many got through. I have rerouted power to the plating. Same wave again.', { now: true });
  }

  // Test hook: clear everything instantly.
  forceWin() {
    for (const r of this.rocks) this._kill(r);
    this.wave = WAVES.length;
    this.kills = TOTAL;
    this.finishT = 0.05;
  }

  end(success) {
    if (!this.active) return;
    this.active = false;
    this.group.visible = false;
    for (const b of this.beams) b.visible = false;
    this.el.ui.hidden = true;
    input.aimOnly = false;
    input.held.fire = false;
    const p = game.player;
    p.frozen = false;
    p.place(this.saved.x, this.saved.z, this.saved.yaw, this.saved.pitch);
    game.mode = 'play';
    hud.setLayout('walk');
    if (success) {
      state.stats.kills = this.kills;
      setFlag('debrisCleared');
    }
  }
}
