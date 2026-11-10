// Sliding bulkhead doors: two panels that retract into the wall, a status bar
// on each side, and a collider that blocks the opening while closed.
import * as THREE from 'three';
import { Builder, G } from '../engine/builder.js';
import { WALL_T, R } from './layout.js';
import { bus } from '../core/events.js';

const COL_LOCKED = new THREE.Color(2.6, 0.12, 0.06);
const COL_READY = new THREE.Color(0.1, 2.2, 0.9);
const COL_OPEN = new THREE.Color(0.9, 2.2, 2.6);
const COL_DEAD = new THREE.Color(0.35, 0.2, 0.05);

function buildPanel(b, side, hw, h, accent) {
  const cx = (side * hw) / 2;
  b.box('trimDark', cx, h / 2, 0, hw, h, 0.1);
  b.box('metal', cx + side * 0.03, h * 0.75, 0, hw - 0.24, h * 0.34, 0.15);
  b.box('metal', cx + side * 0.03, h * 0.3, 0, hw - 0.24, h * 0.3, 0.15);
  b.box('metalDark', cx, h * 0.525, 0, hw - 0.08, 0.09, 0.17, { fine: false });
  b.box('hazard', cx, 0.09, 0, hw - 0.02, 0.14, 0.156, { fine: false });
  b.box('metalDark', side * 0.035, h / 2, 0, 0.07, h, 0.165);
  for (let i = 0; i < 4; i++) b.box('metal', side * 0.03, h * (0.17 + 0.22 * i), 0, 0.14, 0.1, 0.185, { fine: false });
  // recessed grip
  b.box('black', cx + side * (hw * 0.18), h * 0.43, 0, 0.09, 0.26, 0.176, { fine: false });
  // light slit and seam strip, battery-backed
  b.gbox(G(accent, 1.5, 2), cx, h * 0.525, 0, hw - 0.34, 0.022, 0.178);
  b.gbox(G(accent, 2.0, 2), side * 0.13, h * 0.75, 0, 0.018, h * 0.24, 0.158);
  // upper vent slots
  for (let i = 0; i < 3; i++) b.box('black', cx + side * 0.03, h * 0.9 - i * 0.06, 0, hw - 0.5, 0.025, 0.158, { fine: false });
}

export class Door {
  constructor(def, world) {
    this.def = def;
    this.id = def.id;
    this.open = 0;
    this.target = 0;
    this.hold = 0;
    this.locked = null;
    this.forceOpen = false;
    this.forceClosed = false;
    this.roomA = R[def.a];
    this.roomB = R[def.b];
    const axisX = def.axis === 'x';
    const pos = axisX
      ? this.roomA.cx > def.x
        ? this.roomA
        : this.roomB
      : this.roomA.cz > def.z
        ? this.roomA
        : this.roomB;
    const neg = pos === this.roomA ? this.roomB : this.roomA;
    this.group = new THREE.Group();
    this.group.name = def.id;
    this.group.position.set(def.x, 0, def.z);
    this.group.rotation.y = axisX ? Math.PI / 2 : 0;
    this.group.updateMatrixWorld(true);
    const normal = axisX ? new THREE.Vector3(1, 0, 0) : new THREE.Vector3(0, 0, 1);
    const hw = def.w / 2 + 0.03;
    this.travel = def.w / 2 - 0.02;
    this.panels = [];
    for (const side of [-1, 1]) {
      const pivot = new THREE.Group();
      const b = new Builder(world.lib, pos.index);
      buildPanel(b, side, hw, def.h, 0x9fe8ff);
      pivot.add(
        b.finalize({
          name: `${def.id}:panel`,
          bakeMatrix: this.group.matrixWorld,
          split: {
            normal,
            roomPos: pos.index,
            roomNeg: neg.index,
            lightsPos: world.roomLights[pos.index] || [],
            lightsNeg: world.roomLights[neg.index] || [],
          },
        })
      );
      pivot.userData.side = side;
      this.group.add(pivot);
      this.panels.push(pivot);
    }
    // status bars
    this.barMat = new THREE.MeshBasicMaterial({ color: COL_READY.clone() });
    const barGeo = new THREE.BoxGeometry(0.7, 0.05, 0.014);
    for (const s of [-1, 1]) {
      const bar = new THREE.Mesh(barGeo, this.barMat);
      bar.position.set(0, def.h + 0.13, s * (WALL_T + 0.1 + 0.008));
      this.group.add(bar);
    }
    // collider across the opening
    const t = WALL_T;
    this.collider = axisX
      ? { x0: def.x - t, x1: def.x + t, z0: def.z - def.w / 2, z1: def.z + def.w / 2, wall: true, on: true, door: this }
      : { x0: def.x - def.w / 2, x1: def.x + def.w / 2, z0: def.z - t, z1: def.z + t, wall: true, on: true, door: this };
    world.colliders.push(this.collider);
    this._apply();
  }

  _apply() {
    for (const p of this.panels) p.position.x = p.userData.side * this.open * this.travel;
    this.collider.on = this.open < 0.72;
  }

  setLocked(reason) {
    this.locked = reason || null;
  }

  update(dt, px, pz, active) {
    const def = this.def;
    const dist = Math.hypot(px - def.x, pz - def.z);
    const near = active && dist < 3.3;
    if (this.forceClosed) {
      this.target = 0;
    } else if (this.forceOpen) {
      this.target = 1;
    } else if (!this.locked && near) {
      this.target = 1;
      this.hold = 0.8;
    } else {
      this.hold -= dt;
      if (this.hold <= 0 && !(active && dist < 1.3)) this.target = 0;
    }
    const prev = this.open;
    if (this.open < this.target) this.open = Math.min(this.target, this.open + dt * 2.6);
    else if (this.open > this.target) this.open = Math.max(this.target, this.open - dt * 1.9);
    if (prev !== this.open) {
      if (prev === 0) bus.emit('door', this, 'open');
      if (this.open === 0) bus.emit('door', this, 'shut');
      else if (prev === 1) bus.emit('door', this, 'close');
      this._apply();
    }
    const c = this.locked ? COL_LOCKED : this.open > 0.02 ? COL_OPEN : COL_READY;
    this.barMat.color.lerp(this.dead ? COL_DEAD : c, Math.min(1, dt * 10));
  }
}
