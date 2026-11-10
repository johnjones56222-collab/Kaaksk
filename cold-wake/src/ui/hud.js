// Heads-up display: mission panel, compass, minimap, prompts, subtitles,
// toasts, objective waypoint and the full-screen effects layers.
import * as THREE from 'three';
import { game } from '../game/ctx.js';
import { input } from '../engine/input.js';
import { ROOMS, DOORS, roomAt, routeDoors, HULL_ORIGIN } from '../world/layout.js';
import { state, has } from '../core/state.js';
import { clamp, $ } from '../core/utils.js';

const _v = new THREE.Vector3();
const RING = 138.2;

export const hud = {
  el: {},
  waypoint: null,
  marks: [],
  mapTimer: 0,
  hintTimer: 0,
  flashV: 0,

  init() {
    const ids = ['hud', 'hud-mission', 'mission-tag', 'mission-title', 'mission-text', 'mission-list', 'compass-tape', 'minimap', 'room-name', 'vital-hp', 'vital-o2', 'chips', 'crosshair', 'waypoint', 'markers', 'subtitle', 'sub-speaker', 'sub-text', 'prompt', 'prompt-key', 'prompt-text', 'hint', 'toasts', 'btn-interact', 'btn-interact-label', 'btn-flash', 'fx-fade', 'fx-flash', 'fx-damage', 'fx-frost', 'fx-helmet', 'letterbox', 'touch-buttons', 'hud-nav', 'hud-vitals', 'hud-compass', 'hud-buttons'];
    for (const id of ids) this.el[id] = document.getElementById(id);
    // compass tape: three turns of labelled ticks
    const names = ['N', '·', 'NE', '·', 'E', '·', 'SE', '·', 'S', '·', 'SW', '·', 'W', '·', 'NW', '·'];
    let html = '';
    for (let k = 0; k < 3; k++) for (const n of names) html += `<span class="${n.length <= 2 && n !== '·' && n.length === 1 ? 'c' : ''}">${n}</span>`;
    this.el['compass-tape'].innerHTML = html;
    this.mapCtx = this.el.minimap.getContext('2d');
    for (let i = 0; i < 8; i++) {
      const m = document.createElement('i');
      m.hidden = true;
      this.el.markers.appendChild(m);
      this.marks.push(m);
    }
    this.ring = $('#crosshair .ring');
  },

  show(on) {
    this.el.hud.hidden = !on;
  },

  // Hide the walking HUD pieces during gunnery or cinematics.
  setLayout(mode) {
    const walk = mode === 'walk';
    for (const id of ['hud-mission', 'hud-nav', 'hud-vitals', 'hud-compass', 'crosshair', 'touch-buttons', 'hud-buttons']) {
      this.el[id].style.visibility = walk || (mode === 'turret' && id === 'hud-buttons') ? '' : 'hidden';
    }
    if (!walk) {
      this.setPrompt(null);
      this.el.waypoint.hidden = true;
      for (const m of this.marks) m.hidden = true;
    }
  },

  setMission(tag, title, text, list) {
    this.el['mission-tag'].textContent = tag;
    this.el['mission-title'].textContent = title;
    this.el['mission-text'].innerHTML = text;
    this.el['mission-list'].innerHTML = (list || []).map((l) => `<li class="${l.done ? 'done' : ''}">${l.text}</li>`).join('');
  },

  pulseMission() {
    const e = this.el['hud-mission'];
    e.classList.remove('pulse');
    void e.offsetWidth;
    e.classList.add('pulse');
  },

  // p: null | { label, locked, hold (0..1 or undefined) }
  setPrompt(p) {
    const e = this.el.prompt;
    const btn = this.el['btn-interact'];
    const cross = this.el.crosshair;
    if (!p) {
      if (!e.hidden) e.hidden = true;
      btn.classList.remove('ready', 'locked');
      if (this._btnLabel !== 'USE') {
        this._btnLabel = 'USE';
        this.el['btn-interact-label'].textContent = 'USE';
      }
      cross.classList.remove('focus', 'hold');
      this.ring.style.strokeDashoffset = RING;
      this._prompt = null;
      return;
    }
    if (this._prompt !== p.label) {
      this._prompt = p.label;
      this.el['prompt-text'].textContent = p.label;
      const short = p.short || p.label.split(' ').slice(0, 2).join(' ');
      this._btnLabel = short;
      this.el['btn-interact-label'].textContent = short.toUpperCase();
    }
    e.hidden = false;
    e.classList.toggle('locked', !!p.locked);
    btn.classList.toggle('ready', !p.locked);
    btn.classList.toggle('locked', !!p.locked);
    cross.classList.toggle('focus', !p.locked);
    const holding = p.hold != null;
    cross.classList.toggle('hold', holding);
    this.ring.style.strokeDashoffset = holding ? RING * (1 - clamp(p.hold, 0, 1)) : RING;
    this.el['prompt-key'].textContent = holding ? 'HOLD E' : 'E';
  },

  toast(text, kind = 'info', small = '') {
    const t = document.createElement('div');
    t.className = `toast ${kind}`;
    t.innerHTML = (small ? `<small>${small}</small>` : '') + text;
    const box = this.el.toasts;
    box.appendChild(t);
    while (box.children.length > 4) box.firstChild.remove();
    setTimeout(() => t.classList.add('out'), 4200);
    setTimeout(() => t.remove(), 4700);
  },

  subtitle(speaker, text, sys = false) {
    const e = this.el.subtitle;
    if (!text) {
      e.hidden = true;
      return;
    }
    e.hidden = false;
    e.classList.toggle('sys', sys);
    this.el['sub-speaker'].textContent = speaker;
    this.el['sub-text'].textContent = text;
  },

  hint(html, seconds = 6) {
    this.el.hint.innerHTML = html;
    this.el.hint.hidden = false;
    this.hintTimer = seconds;
  },

  setRoom(name) {
    this.el['room-name'].textContent = name;
  },

  setChips(list) {
    const key = list.map((c) => c.label + (c.on ? 1 : 0)).join('|');
    if (key === this._chips) return;
    this._chips = key;
    this.el.chips.innerHTML = list.map((c) => `<span class="chip ${c.on ? 'on' : ''}">${c.label}</span>`).join('');
  },

  fade(opacity, seconds = 0.6) {
    const e = this.el['fx-fade'];
    e.style.transitionDuration = `${seconds}s`;
    void e.offsetWidth;
    e.style.opacity = String(opacity);
    return new Promise((res) => setTimeout(res, seconds * 1000 + 30));
  },

  flash(v = 1) {
    this.flashV = Math.max(this.flashV, v);
  },

  frost(v, seconds = 2.4) {
    const e = this.el['fx-frost'];
    clearTimeout(this._frostT);
    if (v > 0) e.hidden = false;
    e.style.transitionDuration = `${seconds}s`;
    void e.offsetWidth;
    e.style.opacity = String(v);
    // take the blurred layer out of the page once it has faded
    if (v <= 0) this._frostT = setTimeout(() => (e.hidden = true), seconds * 1000 + 200);
  },

  helmet(on) {
    this.el['fx-helmet'].hidden = !on;
  },

  letterbox(on) {
    this.el.letterbox.classList.toggle('on', on);
  },

  setWaypoint(target, label = '') {
    this.waypoint = target ? { x: target.x, y: target.y ?? 1.2, z: target.z, label } : null;
  },

  // Through-the-doors guidance: aim at the next doorway when the goal is in another room.
  _resolve(wp) {
    const world = game.world;
    const cur = world.current;
    const tr = roomAt(wp.x, wp.z);
    if (!cur || !tr || cur === tr || cur.exterior || tr.exterior) return { x: wp.x, y: wp.y, z: wp.z, via: null };
    let doors = routeDoors(cur.id, tr.id, (d) => !world.door[d.id].locked);
    if (!doors) doors = routeDoors(cur.id, tr.id);
    if (!doors || !doors.length) return { x: wp.x, y: wp.y, z: wp.z, via: null };
    const d = doors[0];
    return { x: d.x, y: 1.5, z: d.z, via: tr, doors };
  },

  update(dt) {
    const p = game.player;
    const cam = game.gfx.camera;
    const el = this.el;
    // compass: one "span" is 22.5 degrees and 45px wide
    const deg = ((((-p.yaw * 180) / Math.PI) % 360) + 360) % 360;
    el['compass-tape'].style.transform = `translateX(${-(720 + (deg / 22.5) * 45 + 22.5)}px)`;
    // vitals
    const hp = clamp(p.health / 100, 0, 1);
    const hpBar = el['vital-hp'].querySelector('i');
    hpBar.style.transform = `scaleX(${hp})`;
    el['vital-hp'].classList.toggle('low', hp < 0.35);
    el['fx-damage'].style.opacity = String(clamp((1 - hp) * 0.9 + (p.hurtT > 0.9 ? 0.4 : 0), 0, 1));
    const eva = p.eva;
    el['vital-o2'].hidden = !eva;
    if (eva) {
      const o2 = clamp(game.o2 / game.o2Max, 0, 1);
      el['vital-o2'].querySelector('i').style.transform = `scaleX(${o2})`;
      el['vital-o2'].querySelector('em').textContent = `${Math.ceil(game.o2)}s`;
      el['vital-o2'].classList.toggle('low', o2 < 0.25);
    }
    el['btn-flash'].classList.toggle('on', p.lampOn);
    // flash decay
    if (this.flashV > 0) {
      this.flashV = Math.max(0, this.flashV - dt * 1.4);
      el['fx-flash'].style.opacity = String(Math.min(1, this.flashV));
    }
    if (this.hintTimer > 0) {
      this.hintTimer -= dt;
      if (this.hintTimer <= 0) el.hint.hidden = true;
    }
    if (game.mode !== 'play') return;
    // waypoint
    const wEl = el.waypoint;
    const w = game.gfx.width;
    const h = game.gfx.height;
    if (this.waypoint && has('hasTool')) {
      const r = this._resolve(this.waypoint);
      _v.set(r.x, r.y, r.z).project(cam);
      let x = _v.x;
      let y = _v.y;
      const behind = _v.z > 1;
      if (behind) {
        x = -x;
        y = -y;
      }
      const edge = behind || Math.abs(x) > 0.9 || Math.abs(y) > 0.82;
      if (edge) {
        const m = Math.max(Math.abs(x) / 0.9, Math.abs(y) / 0.8, 0.0001);
        if (behind) {
          const len = Math.hypot(x, y) || 1;
          x = (x / len) * 0.9;
          y = (y / len) * 0.8;
          if (Math.abs(y) < 0.3) y = -0.6;
        } else {
          x /= m;
          y /= m;
        }
      }
      const sx = (x * 0.5 + 0.5) * w;
      const sy = (-y * 0.5 + 0.5) * h;
      wEl.hidden = false;
      wEl.classList.toggle('edge', edge);
      wEl.style.transform = `translate(${sx.toFixed(1)}px, ${sy.toFixed(1)}px)`;
      const dist = Math.hypot(this.waypoint.x - p.x, this.waypoint.z - p.z);
      const txt = r.via ? `${r.via.name.toUpperCase()}  ${Math.round(dist)} m` : `${Math.round(dist)} m`;
      const lab = wEl.lastElementChild;
      if (lab.textContent !== txt) lab.textContent = txt;
      this._wpWorld = r;
    } else {
      wEl.hidden = true;
      this._wpWorld = null;
    }
    // interactable markers
    const list = game.interact ? game.interact.marked : [];
    for (let i = 0; i < this.marks.length; i++) {
      const m = this.marks[i];
      const it = list[i];
      if (!it) {
        m.hidden = true;
        continue;
      }
      _v.set(it.x, it.y + 0.35, it.z).project(cam);
      if (_v.z > 1 || Math.abs(_v.x) > 1 || Math.abs(_v.y) > 1) {
        m.hidden = true;
        continue;
      }
      m.hidden = false;
      m.style.transform = `translate(${((_v.x * 0.5 + 0.5) * w).toFixed(1)}px, ${((-_v.y * 0.5 + 0.5) * h).toFixed(1)}px) rotate(45deg)`;
    }
    // minimap at ~12 Hz
    this.mapTimer -= dt;
    if (this.mapTimer <= 0) {
      this.mapTimer = 0.08;
      this.drawMinimap();
    }
  },

  drawMinimap() {
    const ctx = this.mapCtx;
    const S = 264;
    const p = game.player;
    const world = game.world;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, S, S);
    ctx.save();
    ctx.beginPath();
    ctx.arc(S / 2, S / 2, S / 2 - 2, 0, Math.PI * 2);
    ctx.clip();
    ctx.fillStyle = 'rgba(3,12,20,0.85)';
    ctx.fillRect(0, 0, S, S);
    if (!has('vegaOnline')) {
      // navigation is offline until VEGA is rebooted
      for (let i = 0; i < 90; i++) {
        ctx.fillStyle = `rgba(95,227,255,${Math.random() * 0.25})`;
        ctx.fillRect(Math.random() * S, Math.random() * S, 2 + Math.random() * 30, 2);
      }
      ctx.fillStyle = '#ff5a48';
      ctx.font = '700 20px ui-monospace,Menlo,Consolas,monospace';
      ctx.textAlign = 'center';
      ctx.fillText('NAV OFFLINE', S / 2, S / 2 - 4);
      ctx.fillStyle = '#86afc2';
      ctx.font = '14px ui-monospace,Menlo,Consolas,monospace';
      ctx.fillText('VEGA REQUIRED', S / 2, S / 2 + 18);
      ctx.restore();
      return;
    }
    const k = 5.2;
    ctx.translate(S / 2, S / 2);
    ctx.rotate(p.yaw);
    ctx.translate(-p.x * k, -p.z * k);
    drawPlan(ctx, k, world, true);
    // objective
    const wp = this.waypoint;
    if (wp) {
      const dx = wp.x - p.x;
      const dz = wp.z - p.z;
      const d = Math.hypot(dx, dz);
      const maxD = (S / 2 - 16) / k;
      const f = d > maxD ? maxD / d : 1;
      const ox = (p.x + dx * f) * k;
      const oz = (p.z + dz * f) * k;
      ctx.fillStyle = '#ffb347';
      ctx.strokeStyle = '#000';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(ox, oz - 9);
      ctx.lineTo(ox + 9, oz);
      ctx.lineTo(ox, oz + 9);
      ctx.lineTo(ox - 9, oz);
      ctx.closePath();
      ctx.stroke();
      ctx.fill();
    }
    ctx.restore();
    // player arrow (always pointing up)
    ctx.save();
    ctx.translate(S / 2, S / 2);
    ctx.fillStyle = '#ffffff';
    ctx.strokeStyle = '#03101a';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(0, -12);
    ctx.lineTo(8, 9);
    ctx.lineTo(0, 4);
    ctx.lineTo(-8, 9);
    ctx.closePath();
    ctx.stroke();
    ctx.fill();
    ctx.restore();
  },
};

const hexCss = (h, a = 1) => `rgba(${(h >> 16) & 255},${(h >> 8) & 255},${h & 255},${a})`;

// Draws the deck plan in world units scaled by k. Shared by minimap and full map.
export function drawPlan(ctx, k, world, fogOfWar, labels = false) {
  const cur = world.current;
  for (let i = 1; i < ROOMS.length; i++) {
    const r = ROOMS[i];
    if (r.exterior) continue;
    const seen = !fogOfWar || state.visited[r.id];
    ctx.fillStyle = hexCss(r.color, cur === r ? 0.42 : seen ? 0.17 : 0.03);
    ctx.fillRect(r.x0 * k, r.z0 * k, r.w * k, r.d * k);
    ctx.strokeStyle = hexCss(r.color, seen ? 0.95 : 0.25);
    ctx.lineWidth = cur === r ? 3 : 1.5;
    ctx.strokeRect(r.x0 * k, r.z0 * k, r.w * k, r.d * k);
    if (labels) {
      ctx.fillStyle = seen ? '#d8f4ff' : 'rgba(134,175,194,0.5)';
      ctx.font = `600 ${Math.max(9, Math.min(13, r.w * k * 0.11))}px ui-monospace,Menlo,Consolas,monospace`;
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      const words = (seen ? r.name : '???').toUpperCase().split(' ');
      words.forEach((wd, j) => ctx.fillText(wd, r.cx * k, r.cz * k + (j - (words.length - 1) / 2) * 14));
    }
  }
  for (const d of DOORS) {
    const door = world.door[d.id];
    ctx.fillStyle = door && door.locked ? '#ff5a48' : '#6dffb0';
    const w = d.axis === 'x' ? 5 : d.w * k;
    const h = d.axis === 'x' ? d.w * k : 5;
    ctx.fillRect(d.x * k - w / 2, d.z * k - h / 2, w, h);
  }
  if (cur && cur.exterior) {
    // the catwalk outside
    const X = HULL_ORIGIN.x;
    ctx.strokeStyle = '#d8f4ff';
    ctx.lineWidth = 2;
    ctx.strokeRect((X - 1.3) * k, -50 * k, 2.6 * k, 70 * k);
    ctx.strokeRect((X - 3.6) * k, -57 * k, 7.2 * k, 7 * k);
    for (const [s, z] of [[-1, 10], [1, -10], [-1, -28]]) ctx.strokeRect((X + s * 2.9 - 1.6) * k, (z - 2) * k, 3.2 * k, 4 * k);
  }
}

export function hintKeys(kbm, touch) {
  return input.mode === 'touch' ? touch : kbm;
}
