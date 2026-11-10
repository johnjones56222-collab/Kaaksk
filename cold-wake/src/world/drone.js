// BIT - a palm-sized maintenance drone VEGA sends to keep the player company
// once she is back online. It hovers at the edge of view and turns its eye
// towards the current objective.
import * as THREE from 'three';
import { damp, angleDelta } from '../core/utils.js';

export class Drone {
  constructor(scene) {
    const g = new THREE.Group();
    const shell = new THREE.MeshStandardMaterial({ color: 0xd8dde4, roughness: 0.35, metalness: 0.4 });
    const dark = new THREE.MeshStandardMaterial({ color: 0x20242b, roughness: 0.5, metalness: 0.7 });
    this.eyeMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(0.4, 2.2, 3.2) });
    this.jetMat = new THREE.MeshBasicMaterial({ color: new THREE.Color(0.3, 1.2, 2.4), transparent: true, opacity: 0.8, blending: THREE.AdditiveBlending, depthWrite: false });
    const body = new THREE.Mesh(new THREE.SphereGeometry(0.11, 18, 14), shell);
    body.scale.set(1, 0.82, 1.08);
    const visor = new THREE.Mesh(new THREE.SphereGeometry(0.085, 16, 12, 0, Math.PI * 2, 0, Math.PI * 0.55), dark);
    visor.rotation.x = Math.PI / 2;
    visor.position.z = 0.045;
    const eye = new THREE.Mesh(new THREE.CircleGeometry(0.03, 16), this.eyeMat);
    eye.position.z = 0.131;
    const ring = new THREE.Mesh(new THREE.TorusGeometry(0.15, 0.012, 6, 28), dark);
    ring.rotation.x = Math.PI / 2;
    this.ring = ring;
    const jet = new THREE.Mesh(new THREE.ConeGeometry(0.045, 0.12, 10, 1, true), this.jetMat);
    jet.position.y = -0.14;
    jet.rotation.x = Math.PI;
    this.jet = jet;
    for (const s of [-1, 1]) {
      const fin = new THREE.Mesh(new THREE.BoxGeometry(0.02, 0.05, 0.1), shell);
      fin.position.set(s * 0.125, 0.02, -0.02);
      g.add(fin);
      const tip = new THREE.Mesh(new THREE.SphereGeometry(0.012, 8, 6), this.eyeMat);
      tip.position.set(s * 0.15, 0, 0);
      ring.add(tip);
    }
    g.add(body, visor, eye, ring, jet);
    g.visible = false;
    scene.add(g);
    this.group = g;
    this.yaw = 0;
    this.placed = false;
  }

  // target: optional world point to look at (the objective).
  update(dt, t, player, world, visible, target, talk) {
    const g = this.group;
    if (!visible) {
      g.visible = false;
      this.placed = false;
      return;
    }
    const sin = Math.sin(player.yaw);
    const cos = Math.cos(player.yaw);
    const at = (f, r) => [player.x - sin * f + cos * r, player.z - cos * f - sin * r];
    // prefer a spot ahead and to the right; pull in when a wall is in the way
    let [tx, tz] = at(1.55, 0.8);
    if (world.blocked(player.x, player.z, tx, tz)) {
      [tx, tz] = at(0.7, 0.5);
      if (world.blocked(player.x, player.z, tx, tz)) [tx, tz] = at(0.1, 0.25);
    }
    const ty = (player.eva ? 1.3 : 1.22) + Math.sin(t * 1.7) * 0.05;
    if (!this.placed || Math.hypot(g.position.x - player.x, g.position.z - player.z) > 9) {
      g.position.set(tx, ty, tz);
      this.placed = true;
    }
    g.visible = true;
    g.position.x = damp(g.position.x, tx, 3.2, dt);
    g.position.z = damp(g.position.z, tz, 3.2, dt);
    g.position.y = damp(g.position.y, ty, 4, dt);
    // face the objective, or the way the player is heading
    let want = player.yaw;
    if (target) want = Math.atan2(target.x - g.position.x, target.z - g.position.z);
    else want = Math.atan2(-sin, -cos);
    this.yaw += angleDelta(this.yaw, want) * Math.min(1, dt * 4);
    g.rotation.y = this.yaw;
    g.rotation.z = Math.sin(t * 1.3) * 0.06;
    this.ring.rotation.z += dt * 2.2;
    const pulse = 1 + (talk || 0) * 1.6 + Math.sin(t * 5) * 0.08;
    this.eyeMat.color.setRGB(0.4 * pulse, 2.2 * pulse, 3.2 * pulse);
    this.jet.scale.y = 0.8 + Math.sin(t * 31) * 0.2;
    this.jetMat.opacity = 0.55 + Math.sin(t * 23) * 0.2;
  }
}
