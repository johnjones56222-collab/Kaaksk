// Animated display channels. Each channel is a small canvas redrawn a few
// times a second and shared by every monitor that tunes to it.
import * as THREE from 'three';
import { makeCanvas } from '../engine/textures.js';
import { has, count, state } from '../core/state.js';
import { ROOMS, DOORS } from './layout.js';
import { rng, fmtTime } from '../core/utils.js';

const FONT = '"SF Mono","Roboto Mono",Menlo,Consolas,monospace';
const C = { bg: '#03101a', cyan: '#5fe3ff', dim: '#1d5a70', green: '#6dffb0', amber: '#ffb347', red: '#ff5040', white: '#e6f7ff', violet: '#b79bff' };

function frame(ctx, w, h, title, color = C.cyan, right = '') {
  ctx.fillStyle = C.bg;
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = color;
  ctx.globalAlpha = 0.35;
  ctx.lineWidth = 1;
  ctx.strokeRect(2.5, 2.5, w - 5, h - 5);
  ctx.globalAlpha = 1;
  ctx.fillStyle = color;
  ctx.fillRect(3, 3, w - 6, 15);
  ctx.fillStyle = C.bg;
  ctx.font = `700 10px ${FONT}`;
  ctx.textBaseline = 'middle';
  ctx.textAlign = 'left';
  ctx.fillText(title, 8, 11);
  if (right) {
    ctx.textAlign = 'right';
    ctx.fillText(right, w - 8, 11);
    ctx.textAlign = 'left';
  }
}

function text(ctx, s, x, y, color = C.cyan, size = 10, weight = 400, align = 'left') {
  ctx.fillStyle = color;
  ctx.font = `${weight} ${size}px ${FONT}`;
  ctx.textAlign = align;
  ctx.textBaseline = 'middle';
  ctx.fillText(s, x, y);
  ctx.textAlign = 'left';
}

function bar(ctx, x, y, w, h, v, color) {
  ctx.fillStyle = 'rgba(95,227,255,0.12)';
  ctx.fillRect(x, y, w, h);
  ctx.fillStyle = color;
  ctx.fillRect(x, y, w * Math.max(0, Math.min(1, v)), h);
}

const hex = (n) => n.toString(16).toUpperCase().padStart(2, '0');
const LOG_LINES = [
  'BUS-A NOMINAL', 'THERMAL LOOP 2 .. OK', 'CRC CHECK PASS', 'SYNC GYRO 0.0021', 'HULL STRESS 12%', 'COOLANT 284K',
  'PKT 0x3F41 ACK', 'RCS IDLE', 'O2 PARTIAL 20.9', 'MEM SCRUB 88%', 'LIDAR SWEEP DONE', 'STARTRACK LOCK', 'N2 RESERVE 71%',
  'VALVE 14 CLOSED', 'RAD DOSE 0.4 mSv', 'INERTIAL DRIFT 3e-6', 'GRAVPLATE 0.98g', 'FILTER 7 CHANGE', 'UPLINK NO CARRIER',
];

export class Screens {
  constructor() {
    this.channels = new Map();
    this.order = [];
    this.mats = [];
    this.cursor = 0;
    this.alert = 0;
    this._define();
  }

  define(name, w, h, draw) {
    const canvas = makeCanvas(w, h);
    const ctx = canvas.getContext('2d');
    const tex = new THREE.CanvasTexture(canvas);
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.generateMipmaps = false;
    tex.minFilter = THREE.LinearFilter;
    const ch = { name, w, h, canvas, ctx, tex, draw, used: false, rand: rng(name.length * 131 + w) };
    this.channels.set(name, ch);
    this.order.push(ch);
    draw(ctx, w, h, 0, ch);
    tex.needsUpdate = true;
    return ch;
  }

  // Material for a monitor in a given room. `always` screens run on battery.
  material(name, roomIdx, always = false, gain = 1.35) {
    const ch = this.channels.get(name) || this.channels.get('diag');
    ch.used = true;
    const key = `${ch.name}|${roomIdx}|${always}|${gain}`;
    let rec = this.mats.find((m) => m.key === key);
    if (!rec) {
      const mat = new THREE.MeshBasicMaterial({ map: ch.tex });
      rec = { key, mat, roomIdx, always, gain };
      this.mats.push(rec);
    }
    return rec.mat;
  }

  update(t, roomPower, budget = 2) {
    let n = 0;
    for (let i = 0; i < this.order.length && n < budget; i++) {
      this.cursor = (this.cursor + 1) % this.order.length;
      const ch = this.order[this.cursor];
      if (!ch.used) continue;
      ch.draw(ch.ctx, ch.w, ch.h, t, ch);
      ch.tex.needsUpdate = true;
      n++;
    }
    for (const rec of this.mats) {
      const p = rec.always ? 1 : roomPower[rec.roomIdx];
      const v = rec.gain * (rec.always ? 0.9 : p);
      rec.mat.color.setScalar(v);
    }
  }

  _define() {
    // --- general diagnostics -------------------------------------------------
    this.define('diag', 256, 160, (ctx, w, h, t, ch) => {
      frame(ctx, w, h, 'SYS DIAGNOSTIC', C.cyan, `T+${fmtTime(state.stats.time)}`);
      const names = ['PWR', 'THM', 'ATM', 'NAV', 'COM', 'HUL'];
      const vals = [
        has('reactorOnline') ? 0.92 : has('cryoPower') ? 0.18 : 0.06,
        0.5 + 0.2 * Math.sin(t * 0.7),
        has('valvesDone') ? 0.95 : 0.34 + 0.05 * Math.sin(t),
        has('vegaOnline') ? 0.88 : 0.1,
        has('antennaAligned') ? 0.9 : 0.12,
        has('breachesDone') ? 0.97 : 0.61,
      ];
      for (let i = 0; i < 6; i++) {
        const y = 28 + i * 14;
        text(ctx, names[i], 8, y + 4, C.cyan, 9, 700);
        const v = vals[i] + 0.02 * Math.sin(t * 3 + i);
        bar(ctx, 34, y, 120, 8, v, v < 0.3 ? C.red : v < 0.65 ? C.amber : C.green);
        text(ctx, `${Math.round(v * 100)}%`, 160, y + 4, C.white, 9);
      }
      for (let i = 0; i < 9; i++) {
        let s = '';
        for (let j = 0; j < 3; j++) s += hex(Math.floor(ch.rand() * 256)) + ' ';
        text(ctx, s, 196, 28 + i * 10, C.dim, 8);
      }
      const k = Math.floor(t * 0.8) % LOG_LINES.length;
      text(ctx, `> ${LOG_LINES[k]}`, 8, 122, C.green, 9);
      text(ctx, `> ${LOG_LINES[(k + 7) % LOG_LINES.length]}`, 8, 134, C.dim, 9);
      if (Math.floor(t * 2) % 2) ctx.fillRect(8, 142, 6, 9);
    });

    // --- vitals / ECG --------------------------------------------------------
    this.define('vitals', 256, 160, (ctx, w, h, t) => {
      const ok = has('compound');
      frame(ctx, w, h, 'STASIS VITALS', ok ? C.green : C.cyan, 'POD LINK');
      const rows = [
        ['MARLOW', 46, C.green],
        ['OKONKWO', 44, C.green],
        ['RAMAN', ok ? 45 : 71, ok ? C.green : C.amber],
        ['LINDQVIST', 43, C.green],
        ['VASQUEZ', 47, C.green],
      ];
      rows.forEach(([name, bpm, col], i) => {
        const y = 30 + i * 25;
        text(ctx, name, 8, y, C.white, 9, 700);
        text(ctx, `${bpm + Math.round(Math.sin(t * 0.9 + i) * 1.5)} BPM`, 8, y + 11, col, 8);
        ctx.strokeStyle = col;
        ctx.lineWidth = 1.2;
        ctx.beginPath();
        for (let x = 0; x < 170; x += 2) {
          const ph = ((x * 0.03 + t * (bpm / 46) * 0.9 + i * 0.37) % 1 + 1) % 1;
          let v = 0;
          if (ph > 0.42 && ph < 0.46) v = -3;
          else if (ph >= 0.46 && ph < 0.5) v = 9;
          else if (ph >= 0.5 && ph < 0.54) v = -5;
          else if (ph > 0.66 && ph < 0.76) v = 2.4 * Math.sin(((ph - 0.66) / 0.1) * Math.PI);
          if (!ok && i === 2) v *= 0.6 + 0.8 * Math.abs(Math.sin(x * 0.4 + t * 9));
          const yy = y + 5 - v;
          if (x === 0) ctx.moveTo(78 + x, yy);
          else ctx.lineTo(78 + x, yy);
        }
        ctx.stroke();
      });
    });

    // --- radar sweep ---------------------------------------------------------
    this.define('radar', 192, 192, (ctx, w, h, t, ch) => {
      ctx.fillStyle = C.bg;
      ctx.fillRect(0, 0, w, h);
      const cx = w / 2;
      const cy = h / 2;
      const alert = has('stormWarning') && !has('debrisCleared');
      const col = alert ? C.red : C.green;
      ctx.strokeStyle = col;
      ctx.globalAlpha = 0.4;
      for (let r = 22; r <= 88; r += 22) {
        ctx.beginPath();
        ctx.arc(cx, cy, r, 0, Math.PI * 2);
        ctx.stroke();
      }
      ctx.beginPath();
      ctx.moveTo(cx - 88, cy);
      ctx.lineTo(cx + 88, cy);
      ctx.moveTo(cx, cy - 88);
      ctx.lineTo(cx, cy + 88);
      ctx.stroke();
      ctx.globalAlpha = 1;
      const a = t * 1.6;
      const g = ctx.createConicGradient ? ctx.createConicGradient(a, cx, cy) : null;
      if (g) {
        g.addColorStop(0, 'rgba(0,0,0,0)');
        g.addColorStop(0.86, 'rgba(0,0,0,0)');
        g.addColorStop(1, alert ? 'rgba(255,80,64,0.55)' : 'rgba(109,255,176,0.5)');
        ctx.fillStyle = g;
        ctx.beginPath();
        ctx.arc(cx, cy, 88, 0, Math.PI * 2);
        ctx.fill();
      }
      const r2 = rng(alert ? 99 : 42);
      const n = alert ? 22 : 5;
      for (let i = 0; i < n; i++) {
        const ang = r2() * Math.PI * 2;
        let rad = 20 + r2() * 66;
        if (alert) rad = ((rad - t * 4 - i * 3) % 70 + 70) % 70 + 14;
        const fade = 1 - ((((a - ang) % (Math.PI * 2)) + Math.PI * 2) % (Math.PI * 2)) / (Math.PI * 2);
        ctx.fillStyle = col;
        ctx.globalAlpha = 0.25 + 0.75 * fade;
        ctx.fillRect(cx + Math.cos(ang) * rad - 1.5, cy + Math.sin(ang) * rad - 1.5, 3, 3);
      }
      ctx.globalAlpha = 1;
      text(ctx, alert ? 'DEBRIS INBOUND' : 'PROXIMITY', 6, 10, col, 9, 700);
      text(ctx, alert ? `${n} TRK` : 'CLEAR', w - 6, 10, col, 9, 700, 'right');
    });

    // --- scrolling console log ----------------------------------------------
    this.define('log', 256, 160, (ctx, w, h, t) => {
      frame(ctx, w, h, 'EVENT LOG', C.cyan, 'LIVE');
      const k = Math.floor(t * 1.5);
      for (let i = 0; i < 11; i++) {
        const j = (k + i) % LOG_LINES.length;
        const warn = (k + i) % 7 === 3;
        text(ctx, `${String((k + i) % 1000).padStart(3, '0')} ${LOG_LINES[j]}`, 8, 28 + i * 12, warn ? C.amber : i === 10 ? C.white : C.dim, 9);
      }
    });

    // --- orbital plot --------------------------------------------------------
    this.define('starmap', 256, 160, (ctx, w, h, t) => {
      frame(ctx, w, h, 'ORBITAL PLOT / AURELIA', C.cyan, has('routePlotted') ? 'JUMP SET' : 'DECAYING');
      const cx = 96;
      const cy = 92;
      ctx.fillStyle = '#e0a860';
      ctx.beginPath();
      ctx.arc(cx, cy, 20, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = 'rgba(224,200,160,0.5)';
      ctx.beginPath();
      ctx.ellipse(cx, cy, 40, 10, -0.3, 0, Math.PI * 2);
      ctx.stroke();
      ctx.strokeStyle = C.dim;
      ctx.setLineDash([3, 3]);
      ctx.beginPath();
      ctx.ellipse(cx, cy, 70, 44, -0.3, 0, Math.PI * 2);
      ctx.stroke();
      ctx.setLineDash([]);
      const a = t * 0.25;
      const sx = cx + Math.cos(a) * 70 * Math.cos(-0.3) - Math.sin(a) * 44 * Math.sin(-0.3);
      const sy = cy + Math.cos(a) * 70 * Math.sin(-0.3) + Math.sin(a) * 44 * Math.cos(-0.3);
      ctx.fillStyle = C.white;
      ctx.fillRect(sx - 2, sy - 2, 4, 4);
      text(ctx, 'ASTRAEA', sx + 6, sy - 5, C.white, 8);
      text(ctx, 'PERIAPSIS', 180, 40, C.dim, 8);
      text(ctx, '412 km', 180, 51, C.amber, 11, 700);
      text(ctx, 'DECAY', 180, 70, C.dim, 8);
      text(ctx, has('routePlotted') ? 'N/A' : '-3.1 km/h', 180, 81, has('routePlotted') ? C.green : C.red, 11, 700);
      text(ctx, 'ECC 0.214', 180, 100, C.dim, 8);
      text(ctx, 'INC 11.6', 180, 111, C.dim, 8);
    });

    // --- reactor status ------------------------------------------------------
    this.define('reactor', 256, 160, (ctx, w, h, t) => {
      const on = has('reactorOnline');
      const cells = count('socket', 3);
      frame(ctx, w, h, 'FUSION CORE', on ? C.green : C.amber, on ? 'ONLINE' : 'SCRAMMED');
      const cx = 70;
      const cy = 92;
      ctx.lineWidth = 8;
      ctx.strokeStyle = 'rgba(95,227,255,0.14)';
      ctx.beginPath();
      ctx.arc(cx, cy, 42, Math.PI * 0.75, Math.PI * 2.25);
      ctx.stroke();
      const v = on ? 0.82 + 0.03 * Math.sin(t * 2) : cells * 0.06;
      ctx.strokeStyle = on ? C.green : C.amber;
      ctx.beginPath();
      ctx.arc(cx, cy, 42, Math.PI * 0.75, Math.PI * (0.75 + 1.5 * v));
      ctx.stroke();
      ctx.lineWidth = 1;
      text(ctx, on ? `${(v * 12.4).toFixed(1)}` : '0.0', cx, cy - 4, C.white, 20, 700, 'center');
      text(ctx, 'GW', cx, cy + 14, C.dim, 9, 400, 'center');
      text(ctx, 'FUEL CELLS', 136, 34, C.dim, 8);
      for (let i = 0; i < 3; i++) {
        ctx.fillStyle = i < cells ? C.green : 'rgba(255,80,64,0.5)';
        ctx.fillRect(136 + i * 36, 42, 30, 12);
      }
      text(ctx, 'CONTAINMENT', 136, 70, C.dim, 8);
      bar(ctx, 136, 77, 106, 7, on ? 0.94 : 0.4, on ? C.green : C.amber);
      text(ctx, 'PLASMA TEMP', 136, 96, C.dim, 8);
      text(ctx, on ? `${(148 + Math.sin(t) * 2).toFixed(0)} MK` : '0.3 MK', 136, 108, C.white, 12, 700);
      text(ctx, on ? 'ALL LOOPS NOMINAL' : cells < 3 ? `INSERT CELLS ${cells}/3` : 'READY TO IGNITE', 136, 134, on ? C.green : Math.floor(t * 2) % 2 ? C.amber : C.red, 9, 700);
    });

    // --- emergency banner ----------------------------------------------------
    this.define('warn', 256, 96, (ctx, w, h, t) => {
      const on = Math.floor(t * 2) % 2 === 0;
      ctx.fillStyle = on ? '#3a0604' : '#160202';
      ctx.fillRect(0, 0, w, h);
      ctx.fillStyle = '#ff4030';
      for (let i = -2; i < 14; i++) {
        ctx.beginPath();
        const x = i * 24 + ((t * 12) % 24);
        ctx.moveTo(x, 0);
        ctx.lineTo(x + 12, 0);
        ctx.lineTo(x, 12);
        ctx.lineTo(x - 12, 12);
        ctx.fill();
        ctx.beginPath();
        ctx.moveTo(x, h - 12);
        ctx.lineTo(x + 12, h - 12);
        ctx.lineTo(x, h);
        ctx.lineTo(x - 12, h);
        ctx.fill();
      }
      const stormy = has('stormWarning') && !has('debrisCleared');
      text(ctx, stormy ? 'COLLISION ALERT' : has('reactorOnline') ? 'SYSTEMS RECOVERING' : 'EMERGENCY POWER', w / 2, 38, on ? '#ffd0c8' : '#ff5040', 19, 700, 'center');
      text(ctx, stormy ? 'ALL HANDS TO STATIONS' : has('reactorOnline') ? 'FOLLOW VEGA GUIDANCE' : 'REACTOR OFFLINE', w / 2, 62, '#ff8070', 10, 700, 'center');
    });

    // --- ship schematic with section status ---------------------------------
    this.define('schematic', 256, 256, (ctx, w, h, t) => {
      frame(ctx, w, h, 'ISV ASTRAEA / DECK 1', C.cyan, 'STATUS');
      const sx = 2.2;
      const ox = w / 2;
      const oy = 140;
      const status = roomStatus();
      for (let i = 1; i < ROOMS.length; i++) {
        const r = ROOMS[i];
        if (r.exterior) continue;
        const st = status[r.id] || 0;
        const col = st === 2 ? C.green : st === 1 ? C.amber : C.red;
        ctx.fillStyle = col;
        ctx.globalAlpha = 0.16 + (st === 0 ? 0.1 * (0.5 + 0.5 * Math.sin(t * 4)) : 0.06);
        ctx.fillRect(ox + r.x0 * sx, oy + r.z0 * sx, r.w * sx, r.d * sx);
        ctx.globalAlpha = 0.9;
        ctx.strokeStyle = col;
        ctx.lineWidth = 1;
        ctx.strokeRect(ox + r.x0 * sx + 0.5, oy + r.z0 * sx + 0.5, r.w * sx - 1, r.d * sx - 1);
      }
      ctx.globalAlpha = 1;
      ctx.fillStyle = C.white;
      for (const d of DOORS) ctx.fillRect(ox + d.x * sx - 1.5, oy + d.z * sx - 1.5, 3, 3);
      text(ctx, 'BOW', ox, 26, C.dim, 8, 400, 'center');
      text(ctx, 'STERN', ox, h - 8, C.dim, 8, 400, 'center');
    });

    // --- hydroponics ---------------------------------------------------------
    this.define('hydro', 256, 160, (ctx, w, h, t) => {
      const ok = has('irrigation');
      const o2 = has('valvesDone');
      frame(ctx, w, h, 'HYDROPONICS / LIFE SUPPORT', ok ? C.green : C.amber, ok ? 'FLOW OK' : 'NO FLOW');
      text(ctx, 'CABIN O2', 8, 32, C.dim, 8);
      text(ctx, o2 ? '20.8%' : `${(17.4 - 0.2 * Math.sin(t * 0.3)).toFixed(1)}%`, 8, 48, o2 ? C.green : C.red, 18, 700);
      text(ctx, 'CO2', 96, 32, C.dim, 8);
      text(ctx, o2 ? '0.04%' : '1.9%', 96, 48, o2 ? C.green : C.amber, 18, 700);
      text(ctx, 'H2O', 170, 32, C.dim, 8);
      text(ctx, ok ? '4.2 L/m' : '0.0 L/m', 170, 48, ok ? C.green : C.red, 14, 700);
      ctx.strokeStyle = ok ? C.green : C.dim;
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      for (let x = 0; x < 240; x += 3) {
        const y = 100 - (ok ? 14 : 2) * Math.sin(x * 0.05 + t * 2) - (ok ? 6 : 1) * Math.sin(x * 0.13 - t * 3);
        if (x === 0) ctx.moveTo(8 + x, y);
        else ctx.lineTo(8 + x, y);
      }
      ctx.stroke();
      for (let i = 0; i < 8; i++) {
        const g = ok ? 0.5 + 0.4 * Math.sin(i * 1.7 + t * 0.2) : 0.2;
        bar(ctx, 8 + i * 30, 132, 24, 8, g, ok ? C.green : C.amber);
      }
      text(ctx, 'BED MOISTURE', 8, 124, C.dim, 8);
    });

    // --- signal waveform -----------------------------------------------------
    this.define('wave', 256, 160, (ctx, w, h, t) => {
      const ok = has('signalDecoded');
      frame(ctx, w, h, 'SPECTRAL ANALYSIS', C.violet, ok ? 'DECODED' : 'UNKNOWN SRC');
      ctx.strokeStyle = C.violet;
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      for (let x = 0; x < 240; x += 2) {
        const env = Math.sin((x / 240) * Math.PI);
        const y = 78 - env * (22 * Math.sin(x * 0.11 + t * 4) + 10 * Math.sin(x * 0.31 - t * 6) + (ok ? 0 : 8 * Math.sin(x * 1.7 + t * 30)));
        if (x === 0) ctx.moveTo(8 + x, y);
        else ctx.lineTo(8 + x, y);
      }
      ctx.stroke();
      for (let i = 0; i < 30; i++) {
        const v = Math.abs(Math.sin(i * 0.9 + t * 2) * Math.cos(i * 0.37 - t));
        ctx.fillStyle = i === 11 || i === 12 ? C.white : 'rgba(183,155,255,0.6)';
        ctx.fillRect(10 + i * 8, 150 - v * 30, 5, v * 30);
      }
      text(ctx, ok ? '"DEBRIS TIDE - LEAVE"' : '1420.4 MHz / REPEATING', 8, 30, ok ? C.white : C.dim, 9, 700);
    });

    // --- bridge navigation ---------------------------------------------------
    this.define('nav', 256, 160, (ctx, w, h, t) => {
      frame(ctx, w, h, 'HELM', C.cyan, has('routePlotted') ? 'COURSE LAID IN' : 'NO COURSE');
      const cx = 78;
      const cy = 92;
      ctx.save();
      ctx.beginPath();
      ctx.arc(cx, cy, 50, 0, Math.PI * 2);
      ctx.clip();
      ctx.translate(cx, cy);
      ctx.rotate(Math.sin(t * 0.4) * 0.12);
      ctx.fillStyle = '#0b3550';
      ctx.fillRect(-60, -60, 120, 60 + Math.sin(t * 0.6) * 6);
      ctx.fillStyle = '#4a2c12';
      ctx.fillRect(-60, Math.sin(t * 0.6) * 6, 120, 70);
      ctx.strokeStyle = C.white;
      ctx.beginPath();
      ctx.moveTo(-60, Math.sin(t * 0.6) * 6);
      ctx.lineTo(60, Math.sin(t * 0.6) * 6);
      ctx.stroke();
      ctx.restore();
      ctx.strokeStyle = C.cyan;
      ctx.beginPath();
      ctx.arc(cx, cy, 50, 0, Math.PI * 2);
      ctx.stroke();
      ctx.strokeStyle = C.amber;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(cx - 22, cy);
      ctx.lineTo(cx - 7, cy);
      ctx.lineTo(cx, cy + 6);
      ctx.lineTo(cx + 7, cy);
      ctx.lineTo(cx + 22, cy);
      ctx.stroke();
      ctx.lineWidth = 1;
      text(ctx, 'HDG', 150, 36, C.dim, 8);
      text(ctx, `${(214 + Math.sin(t * 0.2) * 2).toFixed(1)}`, 150, 50, C.white, 15, 700);
      text(ctx, 'VEL', 150, 72, C.dim, 8);
      text(ctx, '7.42 km/s', 150, 85, C.white, 12, 700);
      text(ctx, 'JUMP DRIVE', 150, 108, C.dim, 8);
      const charge = has('routePlotted') ? 1 : has('debrisCleared') ? 0.7 : has('reactorOnline') ? 0.45 : 0;
      bar(ctx, 150, 116, 92, 8, charge, charge >= 1 ? C.green : C.amber);
      text(ctx, charge >= 1 ? 'READY' : 'CHARGING', 150, 136, charge >= 1 ? C.green : C.amber, 9, 700);
    });

    // --- cargo manifest ------------------------------------------------------
    this.define('cargo', 256, 160, (ctx, w, h, t) => {
      frame(ctx, w, h, 'CARGO MANIFEST', C.amber, 'BAY 1');
      const rows = [
        ['C-0412', 'SURVEY DRONES', '6'],
        ['C-0418', 'CORE SAMPLES', '31'],
        ['C-0433', 'RATION PACKS', '880'],
        ['C-0440', 'HULL PLATE', '14'],
        ['C-0457', 'COOLANT GEL', '22'],
        ['C-0461', 'SEED VAULT', '1'],
        ['C-0470', 'EVA CONSUMABLES', '40'],
      ];
      rows.forEach((r, i) => {
        const y = 30 + i * 15;
        const hl = Math.floor(t * 0.6) % rows.length === i;
        if (hl) {
          ctx.fillStyle = 'rgba(255,179,71,0.16)';
          ctx.fillRect(5, y - 7, w - 10, 14);
        }
        text(ctx, r[0], 8, y, C.amber, 9, 700);
        text(ctx, r[1], 62, y, C.white, 9);
        text(ctx, r[2], w - 10, y, C.amber, 9, 700, 'right');
      });
      text(ctx, 'MASS 41.2 t   BAL OK', 8, 148, C.dim, 8);
    });

    // --- VEGA presence -------------------------------------------------------
    this.define('vega', 256, 160, (ctx, w, h, t) => {
      const on = has('vegaOnline');
      ctx.fillStyle = on ? '#02101e' : '#0a0406';
      ctx.fillRect(0, 0, w, h);
      if (!on) {
        text(ctx, 'V.E.G.A.', w / 2, 56, '#6a2a26', 24, 700, 'center');
        text(ctx, 'COGNITIVE CORE OFFLINE', w / 2, 86, Math.floor(t * 1.5) % 2 ? '#ff5040' : '#7a2018', 10, 700, 'center');
        text(ctx, 'MANUAL REBOOT REQUIRED', w / 2, 102, '#7a2018', 9, 400, 'center');
        return;
      }
      const cx = w / 2;
      const cy = 78;
      for (let r = 0; r < 4; r++) {
        ctx.strokeStyle = `rgba(95,227,255,${0.8 - r * 0.17})`;
        ctx.lineWidth = 2 - r * 0.3;
        ctx.beginPath();
        for (let a = 0; a <= 64; a++) {
          const ang = (a / 64) * Math.PI * 2;
          const rad = 22 + r * 11 + 4 * Math.sin(ang * (3 + r) + t * (1.5 + r * 0.6)) * (0.4 + 0.6 * Math.abs(Math.sin(t * 2.2)));
          const x = cx + Math.cos(ang) * rad;
          const y = cy + Math.sin(ang) * rad * 0.82;
          if (a === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.stroke();
      }
      ctx.fillStyle = C.white;
      ctx.beginPath();
      ctx.arc(cx, cy, 7 + 2 * Math.sin(t * 3), 0, Math.PI * 2);
      ctx.fill();
      text(ctx, 'V.E.G.A.  ONLINE', cx, 146, C.cyan, 10, 700, 'center');
    });

    // --- static --------------------------------------------------------------
    this.define('noise', 128, 80, (ctx, w, h, t, ch) => {
      const img = ctx.createImageData(w, h);
      const d = img.data;
      for (let i = 0; i < d.length; i += 4) {
        const v = ch.rand() * 90;
        d[i] = v * 0.7;
        d[i + 1] = v * 0.9;
        d[i + 2] = v;
        d[i + 3] = 255;
      }
      ctx.putImageData(img, 0, 0);
      text(ctx, 'NO SIGNAL', w / 2, h / 2, '#cfe8f5', 11, 700, 'center');
    });

    // --- airlock -------------------------------------------------------------
    this.define('airlock', 256, 160, (ctx, w, h, t) => {
      const ready = has('airlockFuse');
      frame(ctx, w, h, 'AIRLOCK 2 / DORSAL', ready ? C.green : C.red, ready ? 'READY' : 'FAULT');
      text(ctx, 'CHAMBER', 8, 34, C.dim, 8);
      text(ctx, '101.3 kPa', 8, 50, C.white, 16, 700);
      text(ctx, 'OUTER HATCH', 8, 74, C.dim, 8);
      text(ctx, 'SEALED', 8, 88, C.green, 12, 700);
      text(ctx, 'SUIT CHECK', 140, 34, C.dim, 8);
      text(ctx, has('hasSuit') ? 'WORN' : 'NOT WORN', 140, 50, has('hasSuit') ? C.green : C.amber, 12, 700);
      text(ctx, 'EXT TEMP', 140, 74, C.dim, 8);
      text(ctx, '-171 C', 140, 88, C.white, 12, 700);
      text(ctx, ready ? (has('hasSuit') ? 'CYCLE WHEN READY' : 'DON EVA SUIT FIRST') : 'FUSE BOX FAULT - SEE PANEL', 8, 130, ready ? C.green : Math.floor(t * 2) % 2 ? C.red : C.amber, 10, 700);
    });

    // --- breaker / power bus -------------------------------------------------
    this.define('power', 256, 160, (ctx, w, h, t) => {
      const on = has('cryoPower');
      frame(ctx, w, h, 'AUX POWER BUS C', on ? C.green : C.red, on ? 'STABLE' : 'TRIPPED');
      for (let i = 0; i < 6; i++) {
        const x = 14 + i * 40;
        ctx.strokeStyle = C.dim;
        ctx.strokeRect(x, 34, 28, 64);
        const up = on ? i % 2 === 0 || i === 3 : false;
        ctx.fillStyle = up ? C.green : C.red;
        ctx.fillRect(x + 6, up ? 40 : 70, 16, 22);
        text(ctx, `B${i + 1}`, x + 14, 108, C.dim, 8, 400, 'center');
      }
      text(ctx, on ? 'LOAD BALANCED - DOOR POWER RESTORED' : 'BALANCE LOAD TO RESET BUS', 8, 136, on ? C.green : Math.floor(t * 2) % 2 ? C.red : C.amber, 9, 700);
    });

    // --- tactical ------------------------------------------------------------
    this.define('tactical', 256, 160, (ctx, w, h, t) => {
      const alert = has('stormWarning') && !has('debrisCleared');
      const done = has('debrisCleared');
      frame(ctx, w, h, 'POINT DEFENCE', alert ? C.red : done ? C.green : C.cyan, alert ? 'ARMED' : 'STANDBY');
      ctx.strokeStyle = alert ? C.red : C.cyan;
      ctx.lineWidth = 1;
      const cx = 70;
      const cy = 92;
      ctx.beginPath();
      ctx.arc(cx, cy, 34, 0, Math.PI * 2);
      ctx.moveTo(cx - 46, cy);
      ctx.lineTo(cx - 12, cy);
      ctx.moveTo(cx + 12, cy);
      ctx.lineTo(cx + 46, cy);
      ctx.moveTo(cx, cy - 46);
      ctx.lineTo(cx, cy - 12);
      ctx.moveTo(cx, cy + 12);
      ctx.lineTo(cx, cy + 46);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(cx + Math.sin(t * 1.3) * 20, cy + Math.cos(t * 0.9) * 16, 5, 0, Math.PI * 2);
      ctx.stroke();
      text(ctx, 'TURRET A', 140, 36, C.dim, 8);
      text(ctx, 'READY', 140, 49, C.green, 11, 700);
      text(ctx, 'CAPACITOR', 140, 70, C.dim, 8);
      bar(ctx, 140, 77, 100, 7, 0.9 + 0.08 * Math.sin(t * 5), C.green);
      text(ctx, alert ? 'TAKE THE STATION' : done ? 'FIELD CLEAR' : 'NO CONTACTS', 140, 108, alert ? (Math.floor(t * 3) % 2 ? C.red : C.white) : C.green, 10, 700);
    });

    // --- medical synthesiser ---------------------------------------------------
    this.define('med', 256, 160, (ctx, w, h, t) => {
      const ok = has('compound');
      frame(ctx, w, h, 'PHARMA SYNTH', ok ? C.green : C.cyan, ok ? 'DISPENSED' : 'IDLE');
      for (let i = 0; i < 3; i++) {
        const x = 20 + i * 44;
        ctx.strokeStyle = C.dim;
        ctx.strokeRect(x, 34, 26, 78);
        const lvl = 0.35 + 0.3 * Math.sin(t * 0.6 + i * 2) + (ok ? 0.2 : 0);
        ctx.fillStyle = ['#ff5a7a', '#5affc0', '#5a9bff'][i];
        ctx.fillRect(x + 2, 34 + 78 * (1 - lvl), 22, 78 * lvl - 2);
        text(ctx, ['A', 'B', 'C'][i], x + 13, 122, C.white, 9, 700, 'center');
      }
      text(ctx, 'ORDER', 160, 40, C.dim, 8);
      text(ctx, 'CARDIO-', 160, 54, C.white, 10, 700);
      text(ctx, 'STABILISER', 160, 66, C.white, 10, 700);
      text(ctx, 'PATIENT', 160, 88, C.dim, 8);
      text(ctx, 'RAMAN, P.', 160, 100, C.white, 10, 700);
      text(ctx, ok ? 'DOSE DELIVERED' : 'AWAITING MIX', 160, 136, ok ? C.green : C.amber, 9, 700);
    });

    // --- comms ---------------------------------------------------------------
    this.define('comms', 256, 160, (ctx, w, h, t) => {
      const ok = has('antennaAligned');
      frame(ctx, w, h, 'LONG RANGE COMMS', ok ? C.green : C.amber, ok ? 'CARRIER LOCK' : 'NO CARRIER');
      for (let i = 0; i < 5; i++) {
        ctx.strokeStyle = ok ? C.green : C.dim;
        ctx.globalAlpha = ok ? 0.3 + 0.7 * (((t * 1.5 - i * 0.3) % 1) + 1) % 1 : 0.4;
        ctx.beginPath();
        ctx.arc(52, 100, 12 + i * 11, -Math.PI * 0.8, -Math.PI * 0.2);
        ctx.stroke();
      }
      ctx.globalAlpha = 1;
      ctx.fillStyle = C.white;
      ctx.fillRect(49, 100, 6, 30);
      text(ctx, 'DISH AZ', 130, 36, C.dim, 8);
      text(ctx, ok ? '214.0' : '197.3', 130, 50, ok ? C.green : C.red, 14, 700);
      text(ctx, 'DISH EL', 196, 36, C.dim, 8);
      text(ctx, ok ? '+11.6' : '-04.2', 196, 50, ok ? C.green : C.red, 14, 700);
      text(ctx, 'MERIDIAN STN', 130, 78, C.dim, 8);
      text(ctx, ok ? 'HANDSHAKE OK' : 'UNREACHABLE', 130, 91, ok ? C.green : C.amber, 10, 700);
      text(ctx, ok ? 'RELAY ACTIVE' : 'ALIGN DISH (EVA)', 130, 132, ok ? C.green : Math.floor(t * 2) % 2 ? C.red : C.amber, 9, 700);
    });
  }
}

// 0 = fault, 1 = degraded, 2 = nominal - drives the schematic and the hub hologram.
export function roomStatus() {
  const pw = has('reactorOnline');
  return {
    cryo: has('cryoPower') ? 2 : 0,
    spineB: pw ? 2 : 1,
    hub: pw ? 2 : 1,
    spineA: pw ? 2 : 1,
    aicore: has('vegaOnline') ? 2 : 0,
    quarters: pw ? 2 : 1,
    medbay: has('compound') && has('firesDone') ? 2 : pw ? 1 : 0,
    lab: has('signalDecoded') ? 2 : pw ? 1 : 0,
    hydro: has('valvesDone') ? 2 : has('irrigation') ? 1 : 0,
    engineering: pw ? 2 : 0,
    cargo: has('breachesDone') ? 2 : pw ? 1 : 0,
    airlock: has('airlockFuse') ? 2 : 0,
    bridge: has('debrisCleared') ? 2 : has('breachesDone') ? 1 : 0,
  };
}
