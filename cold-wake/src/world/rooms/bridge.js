// Bridge - panoramic viewport, helm, tactical, comms, the navigation table
// and the captain's station with the jump lever.
import * as THREE from 'three';
import { G } from '../../engine/builder.js';
import { onWall } from '../shell.js';
import { consoleDesk, chair, holoTable, wallScreen } from '../props.js';
import { makeCanvas } from '../../engine/textures.js';
import { STAR_NODES, STAR_EDGES } from '../../game/stardata.js';
import { has, state } from '../../core/state.js';
import { logPad, memento, PI } from './common.js';

export function buildBridge(ctx) {
  const { b, room } = ctx;
  const { z0, z1, h } = room;
  const accent = room.color;
  ctx.setAmbient([0.03, 0.04, 0.06]);
  ctx.shell({
    wall: 'wallDark',
    windows: [
      { side: 'N', a: -7, b: 7, y0: 0.95, y1: 4.3, pane: 2.8 },
      { side: 'W', a: -48.6, b: -42.6, y0: 1.05, y1: 3.6, pane: 3 },
      { side: 'E', a: -48.6, b: -42.6, y0: 1.05, y1: 3.6, pane: 3 },
    ],
    ceilLights: { spacing: 5.4, intensity: 0.62, color: 0xcfe0ff },
    ribSkip: (side) => side !== 'S',
    lightRange: 8,
  });
  // head-up overlay projected on the main viewport
  {
    const c = makeCanvas(1024, 256);
    const g = c.getContext('2d');
    g.strokeStyle = 'rgba(120,220,255,0.9)';
    g.fillStyle = 'rgba(120,220,255,0.9)';
    g.lineWidth = 2;
    g.font = '600 16px "SF Mono",Menlo,Consolas,monospace';
    for (let i = 0; i <= 40; i++) {
      const x = 32 + i * 24;
      g.beginPath();
      g.moveTo(x, 20);
      g.lineTo(x, i % 5 === 0 ? 40 : 30);
      g.stroke();
      if (i % 5 === 0) g.fillText(String((190 + i) % 360).padStart(3, '0'), x - 14, 58);
    }
    const br = (x, y, w, hh) => {
      const k = 14;
      g.beginPath();
      g.moveTo(x, y + k); g.lineTo(x, y); g.lineTo(x + k, y);
      g.moveTo(x + w - k, y); g.lineTo(x + w, y); g.lineTo(x + w, y + k);
      g.moveTo(x + w, y + hh - k); g.lineTo(x + w, y + hh); g.lineTo(x + w - k, y + hh);
      g.moveTo(x + k, y + hh); g.lineTo(x, y + hh); g.lineTo(x, y + hh - k);
      g.stroke();
    };
    br(150, 90, 230, 130);
    g.fillText('AURELIA  /  PERIAPSIS 412 km', 150, 240);
    br(790, 110, 90, 70);
    g.fillText('UNKNOWN OBJECT', 760, 206);
    g.fillText('SIG 1420.4 MHz', 760, 226);
    g.beginPath();
    g.moveTo(492, 128); g.lineTo(532, 128); g.moveTo(512, 108); g.lineTo(512, 148);
    g.stroke();
    const tex = new THREE.CanvasTexture(c);
    tex.colorSpace = THREE.SRGBColorSpace;
    const hud = new THREE.Mesh(new THREE.PlaneGeometry(13.4, 3.2), new THREE.MeshBasicMaterial({ map: tex, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, opacity: 0.55 }));
    hud.position.set(0, 2.65, z0 + 0.42);
    ctx.add(hud);
    ctx.tick((dt, t) => {
      hud.material.opacity = (0.35 + 0.1 * Math.sin(t * 2.1)) * ctx.world.rooms[room.index].power;
    });
  }
  // forward ledge under the viewport
  b.box('trimDark', 0, 0.45, z0 + 0.25 + 0.4, 14.6, 0.9, 0.8);
  b.box('metalDark', 0, 0.93, z0 + 0.25 + 0.5, 14.7, 0.06, 1.0, { rx: 0.12 });
  for (let i = 0; i < 7; i++) {
    b.gbox(G(i % 3 === 0 ? 0xffb347 : accent, 1.5, 0, i % 2 ? 5 : 0), -6 + i * 2, 0.985, z0 + 0.82, 1.3, 0.012, 0.36, { rx: 0.12 });
  }
  b.solid(-7.3, z0 + 0.25, 7.3, z0 + 1.2);

  // captain's dais and chair
  b.cyl('trimDark', 0, 0.015, -41.6, 2.3, 0.03, { seg: 36 });
  b.gtor(G(accent, 2.2, 0), 0, 0.034, -41.6, 2.3, 0.022, { axis: 'y', seg: 56 });
  b.gtor(G(accent, 1.2, 0, 2), 0, 0.034, -41.6, 1.5, 0.014, { axis: 'y', seg: 44 });
  b.at(0, 0, -41.5, PI, (bb) => chair(bb, { mat: 'fabricBlue', glow: accent }));
  // captain's side console with the jump lever
  b.at(1.15, 0, -42.1, PI, (bb) => {
    bb.box('trimDark', 0, 0.48, 0, 0.6, 0.96, 0.6);
    bb.box('metalDark', 0, 1.0, 0.03, 0.66, 0.07, 0.68, { rx: 0.22 });
    bb.gbox(G(0xffb347, 2.0, 0, 2), -0.18, 1.045, 0.12, 0.2, 0.01, 0.2, { rx: 0.22 });
    bb.box('black', 0.12, 1.05, 0.02, 0.2, 0.03, 0.36, { rx: 0.22, fine: false });
    bb.sign('JUMP', 0, 0.7, 0.305, 0.1, { px: 64, tint: 0xffc060, i: 1.6, type: 2, spacing: 0.4 });
    const lever = ctx.dyn(0.12, 1.04, 0.02, 0, (db) => {
      db.cyl('chrome', 0, 0.16, 0, 0.018, 0.32, { seg: 8 });
      db.cyl('red', 0, 0.34, 0, 0.045, 0.1, { seg: 10 });
      db.gtor(G(0xff5040, 2.6, 2, 2), 0, 0.3, 0, 0.05, 0.008, { axis: 'y', seg: 14 });
    });
    lever.rotation.x = 0.55;
    ctx.ref('jumpLever', lever);
    ctx.sync(() => {
      if (!has('jumped')) lever.rotation.x = 0.55;
    });
    bb.solidBox(0, 0, 0.6, 0.6);
    ctx.anchor('jump', 0, 1.15, 0.2);
  });
  b.light(0, 2.4, -41.6, accent, 0.6, 5.5);

  // helm stations
  [-2.5, 2.5].forEach((x, i) => {
    b.at(x, 0, -46.9, 0, (bb) => consoleDesk(bb, ctx, { w: 2.0, screen: i ? 'starmap' : 'nav', accent }));
    b.at(x, 0, -45.7, PI, (bb) => chair(bb, { mat: 'fabricDark', glow: accent }));
  });
  // tactical (port) and comms (starboard)
  b.at(-6.9, 0, -39.4, PI / 2, (bb) => {
    consoleDesk(bb, ctx, { w: 2.2, screen: 'tactical', accent: 0xff7a4a });
    ctx.anchor('tactical', 0, 1.2, 0.6);
  });
  b.sign('TACTICAL', -7.72, 2.5, -39.4, 0.14, { ry: PI / 2, px: 64, tint: 0xff9a70, i: 1.5, type: 0, spacing: 0.35 });
  b.at(6.9, 0, -39.4, -PI / 2, (bb) => consoleDesk(bb, ctx, { w: 2.2, screen: 'comms', accent: 0x6dffb0 }));
  b.sign('COMMS', 7.72, 2.5, -39.4, 0.14, { ry: -PI / 2, px: 64, tint: 0x9dffc8, i: 1.5, type: 0, spacing: 0.35 });
  onWall(b, room, 'S', -4.6, (bb) => wallScreen(bb, ctx, 'radar', 2.0, 1.2, 1.2, { accent: 0x6dffb0 }));
  onWall(b, room, 'S', 4.6, (bb) => wallScreen(bb, ctx, 'schematic', 2.0, 1.3, 1.3, { accent }));

  // navigation table with the jump network floating above it
  const nx = -4.3;
  const nz = -43.2;
  b.at(nx, 0, nz, 0, (bb) => holoTable(bb, 1.25, { accent }));
  ctx.anchor('navTable', nx, 1.2, nz);
  {
    const grp = new THREE.Group();
    grp.position.set(nx, 1.55, nz);
    const toP = (n) => new THREE.Vector3((n.x - 0.5) * 2.0, Math.sin(n.x * 7 + n.y * 5) * 0.16, (n.y - 0.5) * 1.5);
    const pos = [];
    for (const [p, q] of STAR_EDGES) {
      const a = toP(STAR_NODES.find((n) => n.id === p));
      const c = toP(STAR_NODES.find((n) => n.id === q));
      pos.push(a.x, a.y, a.z, c.x, c.y, c.z);
    }
    const eg = new THREE.BufferGeometry();
    eg.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    const edgeMat = new THREE.LineBasicMaterial({ color: new THREE.Color(0.1, 0.5, 0.8), transparent: true, blending: THREE.AdditiveBlending, depthWrite: false });
    grp.add(new THREE.LineSegments(eg, edgeMat));
    const nodeMats = [];
    for (const n of STAR_NODES) {
      const col = n.kind === 'hazard' ? new THREE.Color(3, 0.3, 0.2) : n.kind === 'goal' ? new THREE.Color(0.4, 3, 1.2) : n.kind === 'start' ? new THREE.Color(3, 2, 0.6) : new THREE.Color(0.6, 1.8, 3);
      const m = new THREE.MeshBasicMaterial({ color: col });
      nodeMats.push(m);
      const s = new THREE.Mesh(new THREE.SphereGeometry(n.kind ? 0.05 : 0.032, 10, 8), m);
      s.position.copy(toP(n));
      grp.add(s);
    }
    const routeGeo = new THREE.BufferGeometry();
    routeGeo.setAttribute('position', new THREE.Float32BufferAttribute(new Float32Array(3 * 12), 3));
    const routeMat = new THREE.LineBasicMaterial({ color: new THREE.Color(0.5, 3.2, 1.4) });
    const routeLine = new THREE.Line(routeGeo, routeMat);
    routeLine.visible = false;
    grp.add(routeLine);
    ctx.add(grp);
    ctx.tick((dt, t) => {
      grp.rotation.y = Math.sin(t * 0.25) * 0.5;
      const p = ctx.world.rooms[room.index].power;
      edgeMat.opacity = 0.7 * p;
      grp.visible = p > 0.05;
    });
    ctx.sync(() => {
      const route = state.flags.route;
      if (has('routePlotted') && Array.isArray(route) && route.length > 1) {
        const arr = routeGeo.attributes.position;
        route.forEach((id, i) => {
          const p = toP(STAR_NODES.find((n) => n.id === id) || STAR_NODES[0]);
          arr.setXYZ(i, p.x, p.y + 0.01, p.z);
        });
        routeGeo.setDrawRange(0, route.length);
        arr.needsUpdate = true;
        routeLine.visible = true;
      } else {
        routeLine.visible = false;
      }
    });
  }

  // ceiling ribs sweeping towards the bow, and a ring lamp over the captain
  for (let i = -3; i <= 3; i++) {
    b.box('trimDark', i * 2.2, h - 0.16, room.cz, 0.22, 0.32, room.d - 0.6, { ry: i * 0.035 });
  }
  b.tor('trimDark', 0, h - 0.5, -41.6, 1.8, 0.1, { axis: 'y', seg: 40 });
  b.gtor(G(0xcfe0ff, 3, 0), 0, h - 0.6, -41.6, 1.8, 0.03, { axis: 'y', seg: 48 });
  b.light(0, h - 1.2, -41.6, 0xcfe0ff, 0.9, 8, { key: true });
  b.sign('BRIDGE', 0, 0.014, -37.9, 0.34, { rx: -PI / 2, px: 96, tint: accent, i: 0.8, type: 3, spacing: 0.4 });
  b.sign('ISV ASTRAEA  -  UCS 7-1142', 0, 4.55, z1 - 0.27, 0.16, { ry: PI, px: 64, tint: accent, i: 1.6, type: 0, spacing: 0.35 });

  logPad(ctx, 'bridge', -0.28, 0.728, -41.5, 0.1, 0x58b8ff);
  // crew photograph propped on the helm
  memento(ctx, 6, 2.5, 0.99, -46.55, (db) => {
    db.box('black', 0, 0.09, 0, 0.26, 0.18, 0.012, { rx: -0.2, fine: false });
    db.gbox(G(0xffe2c0, 1.0, 2), 0, 0.09, 0.008, 0.22, 0.14, 0.004, { rx: -0.2 });
    for (let i = 0; i < 5; i++) db.gbox(G([0x66ccff, 0xffb060, 0x7dffc0, 0xb388ff, 0xff7a4a][i], 1.4, 2), -0.08 + i * 0.04, 0.085, 0.012, 0.022, 0.06, 0.003, { rx: -0.2 });
  }, 0.2);
  void z1;
}
