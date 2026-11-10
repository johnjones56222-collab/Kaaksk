// Things the player can use. Each frame the best candidate in front of the
// camera becomes the focus; tap or hold E (or the on-screen button) to act.
import * as THREE from 'three';
import { game } from './ctx.js';
import { input } from '../engine/input.js';
import { hud } from '../ui/hud.js';

const _d = new THREE.Vector3();
const _f = new THREE.Vector3();

export class Interact {
  constructor() {
    this.items = [];
    this.focus = null;
    this.holdT = 0;
    this.marked = [];
    this.cool = 0;
  }

  // def: { id, pos (Vector3), range, label(), can(), use(), hold, holding(dt, k), locked(), mark() }
  add(def) {
    const it = {
      range: 2.5,
      hold: 0,
      can: () => true,
      locked: () => null,
      mark: () => false,
      ...def,
      x: def.pos.x,
      y: def.pos.y,
      z: def.pos.z,
    };
    this.items.push(it);
    return it;
  }

  reset() {
    this.focus = null;
    this.holdT = 0;
  }

  update(dt) {
    const cam = game.gfx.camera;
    const world = game.world;
    const px = cam.position.x;
    const py = cam.position.y;
    const pz = cam.position.z;
    cam.getWorldDirection(_f);
    let best = null;
    let bestScore = -1;
    this.marked.length = 0;
    this.cool = Math.max(0, this.cool - dt);
    for (const it of this.items) {
      _d.set(it.x - px, it.y - py, it.z - pz);
      const dist = _d.length();
      if (dist > 14) continue;
      if (!it.can()) continue;
      const wantsMark = it.mark();
      if (dist > it.range) {
        if (wantsMark && this.marked.length < 8 && !world.blocked(px, pz, it.x, it.z)) this.marked.push(it);
        continue;
      }
      const dot = _d.dot(_f) / (dist || 1);
      // generous cone that widens when very close
      const need = dist < 1.1 ? 0.45 : 0.86;
      if (dot < need) {
        if (wantsMark && this.marked.length < 8) this.marked.push(it);
        continue;
      }
      if (world.blocked(px, pz, it.x, it.z)) continue;
      const score = dot - dist * 0.03;
      if (score > bestScore) {
        bestScore = score;
        best = it;
      }
      if (wantsMark && this.marked.length < 8) this.marked.push(it);
    }
    if (best !== this.focus) {
      if (this.focus && this.focus.cancel) this.focus.cancel();
      this.focus = best;
      this.holdT = 0;
    }
    if (!best) {
      hud.setPrompt(null);
      return;
    }
    const lock = best.locked();
    const label = lock || (typeof best.label === 'function' ? best.label() : best.label);
    const pressed = input.pressed('interact') || input.pressed('click');
    if (lock) {
      hud.setPrompt({ label, locked: true, short: 'LOCKED' });
      if (pressed && this.cool <= 0) {
        this.cool = 0.5;
        game.deny(lock);
      }
      return;
    }
    if (best.hold > 0) {
      const holding = input.held.interact || (input.held.fire && input.locked);
      if (holding) {
        this.holdT += dt;
        const k = Math.min(1, this.holdT / best.hold);
        if (best.holding) best.holding(dt, k);
        if (k >= 1) {
          this.holdT = 0;
          best.use();
        }
      } else if (this.holdT > 0) {
        this.holdT = Math.max(0, this.holdT - dt * 1.5);
        if (best.cancel) best.cancel();
      }
      hud.setPrompt({ label, hold: this.holdT / best.hold, short: best.short });
    } else {
      hud.setPrompt({ label, short: best.short });
      if (pressed && this.cool <= 0) {
        this.cool = 0.25;
        best.use();
      }
    }
  }
}
