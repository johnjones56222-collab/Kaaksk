// Title screen, datapad (pause) with its tabs, log reader, intro crawl and
// the end-of-game summary.
import { game } from '../game/ctx.js';
import { settings, saveSettings, state, hasSave, has } from '../core/state.js';
import { audio } from '../engine/audio.js';
import { el, fmtTime } from '../core/utils.js';
import { STEPS, SIDE, MISSION_NAMES, missions } from '../game/missions.js';
import { LOGS, logById } from '../game/logs.js';
import { drawPlan } from './hud.js';
import { ROOMS } from '../world/layout.js';

const $ = (id) => document.getElementById(id);
const click = () => audio.sfx('click');

function button(label, cls, fn) {
  const b = el('button', `btn ${cls || ''}`, label);
  b.type = 'button';
  b.addEventListener('click', () => {
    click();
    fn();
  });
  return b;
}

// ---- settings (shared by the title screen and the datapad) ---------------------
function settingsPanel() {
  const box = el('div', 'settings');
  const range = (label, key, min, max, step, fmt, onChange) => {
    const row = el('div', 'set-row');
    const id = `set-${key}`;
    const lab = el('label', '', label);
    lab.htmlFor = id;
    const inp = el('input');
    inp.type = 'range';
    inp.id = id;
    inp.min = String(min);
    inp.max = String(max);
    inp.step = String(step);
    inp.value = String(settings[key]);
    const out = el('output', '', fmt(settings[key]));
    inp.addEventListener('input', () => {
      settings[key] = Number(inp.value);
      out.textContent = fmt(settings[key]);
      saveSettings();
      if (onChange) onChange();
    });
    row.append(lab, inp, out);
    box.appendChild(row);
  };
  const seg = (label, key, options, onChange) => {
    const row = el('div', 'set-row');
    row.appendChild(el('label', '', label));
    const s = el('div', 'seg');
    s.setAttribute('role', 'group');
    s.setAttribute('aria-label', label);
    const btns = options.map(([value, text]) => {
      const b = el('button', settings[key] === value ? 'on' : '', text);
      b.type = 'button';
      b.addEventListener('click', () => {
        settings[key] = value;
        btns.forEach((x, i) => x.classList.toggle('on', options[i][0] === value));
        saveSettings();
        click();
        if (onChange) onChange();
      });
      s.appendChild(b);
      return b;
    });
    row.append(s, el('span', ''));
    box.appendChild(row);
  };
  const pct = (v) => `${Math.round(v * 100)}%`;
  const onOff = [[true, 'On'], [false, 'Off']];
  seg('Graphics', 'quality', [['low', 'Low'], ['medium', 'Medium'], ['high', 'High']], () => game.gfx.setQuality(settings.quality));
  seg('Dynamic resolution', 'dynamicRes', onOff);
  range('Brightness', 'brightness', 0.6, 1.6, 0.05, pct);
  range('Field of view', 'fov', 60, 100, 1, (v) => `${v}°`, () => game.gfx.applyFov());
  range('Look sensitivity', 'sensitivity', 0.3, 2.5, 0.05, (v) => `${v.toFixed(2)}x`);
  seg('Invert look Y', 'invertY', onOff);
  seg('Head bob', 'bob', onOff);
  range('Master volume', 'master', 0, 1, 0.05, pct, () => audio.applyVolumes());
  range('Music', 'music', 0, 1, 0.05, pct, () => audio.applyVolumes());
  range('Effects', 'sfx', 0, 1, 0.05, pct, () => audio.applyVolumes());
  seg('Subtitles', 'subtitles', onOff);
  seg('Spoken lines', 'voice', [[true, 'Device voice'], [false, 'Off']]);
  const fsOk = document.fullscreenEnabled || document.webkitFullscreenEnabled;
  if (fsOk) {
    const row = el('div', 'set-row');
    row.appendChild(el('label', '', 'Display'));
    row.appendChild(
      button('Toggle fullscreen', 'small', () => {
        try {
          if (document.fullscreenElement) document.exitFullscreen();
          else {
            const p = document.documentElement.requestFullscreen?.();
            if (p && p.catch) p.catch(() => {});
          }
        } catch {
          /* optional */
        }
      })
    );
    row.appendChild(el('span', ''));
    box.appendChild(row);
  }
  return box;
}

function controlsPanel() {
  const k = el('div', 'keys');
  k.innerHTML = `
    <h3>KEYBOARD AND MOUSE</h3>
    <kbd>W A S D</kbd><span>Move</span>
    <kbd>Mouse</kbd><span>Look (click the view to capture the pointer)</span>
    <kbd>Shift</kbd><span>Run</span>
    <kbd>Space</kbd><span>Jump</span>
    <kbd>E</kbd><span>Use / hold to work</span>
    <kbd>F</kbd><span>Flashlight</span>
    <kbd>Tab</kbd><span>Datapad: mission, map, logs</span>
    <kbd>M</kbd><span>Map</span>
    <kbd>Esc</kbd><span>Pause</span>
    <h3>TOUCH</h3>
    <kbd>Left side</kbd><span>Drag to move. Push to the edge to run.</span>
    <kbd>Right side</kbd><span>Drag to look around</span>
    <kbd>USE</kbd><span>Tap to use, hold to weld, spray or turn valves</span>
    <kbd>Gunnery</kbd><span>Touch a target to aim and fire</span>
    <h3>TIPS</h3>
    <kbd>Amber diamond</kbd><span>Points to your objective, through doors</span>
    <kbd>Blue diamonds</kbd><span>Mark things you can use nearby</span>`;
  return k;
}

export const menus = {
  // ---- title ----------------------------------------------------------------
  showTitle() {
    const root = $('overlay-menu');
    root.innerHTML = '';
    const m = el('div', 'menu');
    const main = () => {
      m.innerHTML = `<h1>COLD WAKE</h1><div class="sub">ISV Astraea &middot; Aurelia system &middot; Day 412</div>
        <p class="blurb">You are the first responder. The reactor is dark, the ship's mind is silent, and five crewmates are asleep in the ice. Bring the Astraea back to life and take her home.</p>`;
      if (hasSave()) m.appendChild(button('Continue', 'primary', () => game.startGame(true)));
      m.appendChild(
        button('New game', hasSave() ? '' : 'primary', () => {
          if (hasSave()) confirmNew();
          else game.startGame(false);
        })
      );
      m.appendChild(button('Settings', '', () => sub('Settings', settingsPanel())));
      m.appendChild(button('How to play', '', () => sub('How to play', controlsPanel())));
      m.appendChild(el('div', 'foot', 'Sound on is recommended.<br>Progress saves automatically after every objective.'));
    };
    const sub = (title, content) => {
      m.innerHTML = '';
      const wrap = el('div', 'menu-sub');
      wrap.appendChild(el('div', 'sub', title));
      const scroll = el('div', '');
      scroll.style.cssText = 'max-height:62vh;overflow:auto;padding-right:8px;touch-action:pan-y';
      scroll.appendChild(content);
      wrap.appendChild(scroll);
      wrap.appendChild(button('Back', 'small', main));
      m.appendChild(wrap);
    };
    const confirmNew = () => {
      m.innerHTML = '<h1>COLD WAKE</h1><div class="sub">Start over?</div><p class="blurb">A new game replaces your saved progress.</p>';
      m.appendChild(button('Start a new game', 'danger', () => game.startGame(false)));
      m.appendChild(button('Keep my save', 'primary', main));
    };
    main();
    root.appendChild(m);
    root.hidden = false;
  },

  hideTitle() {
    $('overlay-menu').hidden = true;
  },

  // ---- intro crawl ----------------------------------------------------------
  playIntro() {
    const root = $('overlay-intro');
    const box = $('intro-lines');
    const lines = [
      ['ISV ASTRAEA &nbsp;/&nbsp; DEEP SURVEY VESSEL', ''],
      ['AURELIA SYSTEM &nbsp;/&nbsp; MISSION DAY 412', ''],
      ['REACTOR SCRAM &nbsp;&middot;&nbsp; COGNITIVE CORE OFFLINE', 'warn'],
      ['CREW IN PROTECTIVE STASIS', ''],
      ['EMERGENCY THAW: POD 07', 'warn'],
    ];
    box.innerHTML = lines.map(([t, c]) => `<div class="${c}">${t}</div>`).join('');
    root.hidden = false;
    return new Promise((resolve) => {
      const timers = [];
      let done = false;
      const finish = () => {
        if (done) return;
        done = true;
        timers.forEach(clearTimeout);
        root.hidden = true;
        $('intro-skip').removeEventListener('click', finish);
        window.removeEventListener('keydown', onKey);
        resolve();
      };
      const onKey = (e) => {
        if (e.code === 'Escape' || e.code === 'Enter' || e.code === 'Space') finish();
      };
      [...box.children].forEach((d, i) =>
        timers.push(
          setTimeout(() => {
            d.classList.add('on');
            audio.sfx('blip', { f: 300 });
          }, 500 + i * 1300)
        )
      );
      timers.push(setTimeout(finish, 500 + lines.length * 1300 + 1600));
      $('intro-skip').addEventListener('click', finish);
      window.addEventListener('keydown', onKey);
    });
  },

  // ---- datapad --------------------------------------------------------------
  openPause(tab = 'mission') {
    const root = $('overlay-pause');
    root.innerHTML = '';
    const panel = el('div', 'panel datapad');
    panel.setAttribute('role', 'dialog');
    panel.setAttribute('aria-label', 'Datapad');
    const head = el('div', 'panel-head', `<h2>Datapad</h2><p>ISV ASTRAEA &middot; T+${fmtTime(state.stats.time)}</p>`);
    const tabs = el('div', 'tabs');
    tabs.setAttribute('role', 'tablist');
    const body = el('div', 'panel-body');
    const foot = el('div', 'panel-foot');
    const names = [['mission', 'Mission'], ['map', 'Map'], ['logs', `Logs ${state.logs.length}/12`], ['comms', 'Comms'], ['settings', 'Settings'], ['help', 'Controls']];
    const tabBtns = {};
    const show = (id) => {
      for (const k of Object.keys(tabBtns)) tabBtns[k].classList.toggle('on', k === id);
      body.innerHTML = '';
      if (id === 'mission') body.appendChild(this._missionTab());
      else if (id === 'map') this._mapTab(body);
      else if (id === 'logs') body.appendChild(this._logsTab());
      else if (id === 'comms') body.appendChild(this._commsTab());
      else if (id === 'settings') body.appendChild(settingsPanel());
      else body.appendChild(controlsPanel());
      body.scrollTop = 0;
    };
    for (const [id, label] of names) {
      const t = el('button', 'tab', label);
      t.type = 'button';
      t.setAttribute('role', 'tab');
      t.addEventListener('click', () => {
        click();
        show(id);
      });
      tabs.appendChild(t);
      tabBtns[id] = t;
    }
    foot.appendChild(button('Quit to title', 'small danger', () => game.quitToTitle()));
    foot.appendChild(el('div', 'status', 'Progress is saved.'));
    foot.appendChild(button('Resume', 'primary', () => game.resume()));
    panel.append(head, tabs, body, foot);
    root.appendChild(panel);
    root.hidden = false;
    show(tab);
  },

  closePause() {
    const root = $('overlay-pause');
    root.hidden = true;
    root.innerHTML = '';
  },

  _missionTab() {
    const box = el('div', '');
    const cur = missions.index;
    let lastM = 0;
    let group = null;
    STEPS.forEach((s, i) => {
      // only show missions that have been reached
      if (s.m > (STEPS[Math.min(cur, STEPS.length - 1)].m)) return;
      if (s.m !== lastM) {
        lastM = s.m;
        group = el('div', 'obj-group', `<h3>${String(s.m).padStart(2, '0')} &middot; ${MISSION_NAMES[s.m]}</h3>`);
        box.appendChild(group);
      }
      if (i > cur) return;
      const done = i < cur;
      const row = el('div', `obj ${done ? 'done' : 'now'}`, `<span>${s.text().replace(/<[^>]+>/g, '')}</span>`);
      group.appendChild(row);
    });
    const side = el('div', 'obj-group', '<h3>Optional</h3>');
    for (const s of SIDE) {
      const n = s.n();
      side.appendChild(el('div', `obj ${n >= s.of ? 'done' : ''}`, `<span>${s.text}</span><em>${n}/${s.of}</em>`));
    }
    box.appendChild(side);
    return box;
  },

  _mapTab(body) {
    const wrap = el('div', 'map-wrap');
    const c = el('canvas');
    c.id = 'bigmap';
    wrap.appendChild(c);
    body.appendChild(wrap);
    const draw = () => {
      const r = wrap.getBoundingClientRect();
      const dpr = Math.min(2, window.devicePixelRatio || 1);
      const W = Math.max(200, r.width);
      const H = Math.max(200, r.height);
      // the deck is ~44 m wide and ~102 m long: lay it on its side when the pad is wide
      const sideways = W > H;
      const spanX = sideways ? 104 : 46;
      const spanZ = sideways ? 46 : 104;
      const k = Math.min(W / spanX, H / spanZ);
      c.width = W * dpr;
      c.height = H * dpr;
      c.style.width = `${W}px`;
      c.style.height = `${H}px`;
      const ctx = c.getContext('2d');
      ctx.scale(dpr, dpr);
      ctx.translate(W / 2, H / 2);
      if (sideways) ctx.rotate(Math.PI / 2);
      ctx.translate(0, -0.5 * k);
      const world = game.world;
      if (!has('vegaOnline')) {
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        ctx.fillStyle = '#ff5a48';
        ctx.font = '700 16px ui-monospace,Menlo,Consolas,monospace';
        ctx.textAlign = 'center';
        ctx.fillText('NAVIGATION OFFLINE', W / 2, H / 2 - 6);
        ctx.fillStyle = '#86afc2';
        ctx.font = '12px ui-monospace,Menlo,Consolas,monospace';
        ctx.fillText('Reboot VEGA in the AI Core to restore the deck plan.', W / 2, H / 2 + 16);
        return;
      }
      // labels are drawn upright afterwards, so draw the plan without them first
      drawPlan(ctx, k, world, true, false);
      const p = game.player;
      const interior = !(world.current && world.current.exterior);
      const wp = game.hud.waypoint;
      const mark = (x, z, color, size) => {
        ctx.fillStyle = color;
        ctx.strokeStyle = '#03101a';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(x * k, z * k - size);
        ctx.lineTo(x * k + size, z * k);
        ctx.lineTo(x * k, z * k + size);
        ctx.lineTo(x * k - size, z * k);
        ctx.closePath();
        ctx.stroke();
        ctx.fill();
      };
      if (wp && wp.x < 100) mark(wp.x, wp.z, '#ffb347', 8);
      if (interior) {
        ctx.save();
        ctx.translate(p.x * k, p.z * k);
        ctx.rotate(-p.yaw);
        ctx.fillStyle = '#fff';
        ctx.strokeStyle = '#03101a';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(0, -11);
        ctx.lineTo(7, 8);
        ctx.lineTo(0, 4);
        ctx.lineTo(-7, 8);
        ctx.closePath();
        ctx.stroke();
        ctx.fill();
        ctx.restore();
      }
      // upright room labels
      ctx.font = '600 11px ui-monospace,Menlo,Consolas,monospace';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      for (let i = 1; i < ROOMS.length; i++) {
        const r0 = ROOMS[i];
        if (r0.exterior) continue;
        const seen = state.visited[r0.id];
        ctx.save();
        ctx.translate(r0.cx * k, r0.cz * k);
        if (sideways) ctx.rotate(-Math.PI / 2);
        ctx.fillStyle = seen ? '#d8f4ff' : 'rgba(134,175,194,0.45)';
        const words = (seen ? r0.name : '?').toUpperCase().split(' ');
        const narrow = (sideways ? r0.d : r0.w) * k < 60;
        if (narrow) ctx.font = '600 9px ui-monospace,Menlo,Consolas,monospace';
        words.forEach((wd, j) => ctx.fillText(wd, 0, (j - (words.length - 1) / 2) * 12));
        ctx.restore();
      }
    };
    requestAnimationFrame(draw);
  },

  _logsTab() {
    const lay = el('div', 'log-layout');
    const list = el('div', 'log-list');
    const text = el('div', 'log-text');
    const open = (id, btn) => {
      const log = logById(id);
      text.innerHTML = `<h3>${log.title}</h3><div class="meta">${log.author.toUpperCase()} &middot; ${log.where.toUpperCase()}</div>${log.body().map((p) => `<p>${p}</p>`).join('')}`;
      list.querySelectorAll('.log-item').forEach((b) => b.classList.remove('on'));
      if (btn) btn.classList.add('on');
      text.scrollTop = 0;
    };
    let first = null;
    for (const log of LOGS) {
      const got = state.logs.includes(log.id);
      const b = el('button', `log-item ${got ? '' : 'missing'}`, got ? `${log.title}<small>${log.where.toUpperCase()}</small>` : `Not recovered<small>${log.where.toUpperCase()}</small>`);
      b.type = 'button';
      if (got) {
        b.addEventListener('click', () => {
          click();
          open(log.id, b);
        });
        if (!first) first = [log.id, b];
      } else b.disabled = true;
      list.appendChild(b);
    }
    if (first) open(first[0], first[1]);
    else text.innerHTML = '<p class="hint-line">No logs recovered yet. Look for glowing data slates around the ship.</p>';
    lay.append(list, text);
    return lay;
  },

  _commsTab() {
    const box = el('div', 'transcript');
    if (!state.transcript.length) box.innerHTML = '<p class="hint-line">Nothing on the channel yet.</p>';
    for (const [who, line] of state.transcript.slice(-40)) box.appendChild(el('div', '', `<b>${who}</b>${line}`));
    requestAnimationFrame(() => {
      const body = box.parentElement;
      if (body) body.scrollTop = body.scrollHeight;
    });
    return box;
  },

  // ---- log reader -----------------------------------------------------------
  openReader(id) {
    const log = logById(id);
    const root = $('overlay-reader');
    root.innerHTML = '';
    const panel = el('div', 'panel reader');
    panel.setAttribute('role', 'dialog');
    panel.innerHTML = `<div class="panel-head"><h2>${log.title}</h2><p>${log.where.toUpperCase()}</p></div>
      <div class="panel-body"><div class="log-text"><div class="meta">${log.author.toUpperCase()}</div>${log.body().map((p) => `<p>${p}</p>`).join('')}</div></div>`;
    const foot = el('div', 'panel-foot');
    foot.appendChild(el('div', 'status', `Saved to your datapad (${state.logs.length}/12)`));
    foot.appendChild(button('Close', 'primary', () => game.closeReader()));
    panel.appendChild(foot);
    root.appendChild(panel);
    root.hidden = false;
  },

  closeReader() {
    const root = $('overlay-reader');
    root.hidden = true;
    root.innerHTML = '';
  },

  // ---- ending ---------------------------------------------------------------
  showEnd() {
    const root = $('overlay-end');
    root.innerHTML = '';
    const s = state.stats;
    const side = SIDE.reduce((a, x) => a + Math.min(x.n(), x.of), 0);
    const sideMax = SIDE.reduce((a, x) => a + x.of, 0);
    const pct = side / sideMax;
    const rank = pct >= 1 ? 'CHIEF OF THE BOAT' : pct >= 0.6 ? 'SENIOR SPECIALIST' : pct >= 0.25 ? 'FIRST RESPONDER' : 'SURVIVOR';
    const panel = el('div', 'panel end');
    panel.innerHTML = `<div class="panel-body">
      <div class="tag">MERIDIAN STATION &middot; ARRIVAL</div>
      <h1>HOME</h1>
      <div class="rank">${rank}</div>
      <p>The Astraea drops out of jump into the traffic lanes of Meridian Station with six living crew aboard. Behind you, the tide tears through the rings of Aurelia, and the Lattice keeps repeating its warning to whoever comes next.</p>
      <div class="stats">
        <div class="stat"><b>${fmtTime(s.time)}</b><span>Mission time</span></div>
        <div class="stat"><b>${Math.round(s.distance)} m</b><span>Walked</span></div>
        <div class="stat"><b>${state.logs.length}/12</b><span>Logs</span></div>
        <div class="stat"><b>${side}/${sideMax}</b><span>Optional tasks</span></div>
        <div class="stat"><b>${s.kills || 0}</b><span>Debris destroyed</span></div>
      </div>
      <p>VEGA: "They are waking up now. I told them who brought the ship home. Thank you, Specialist."</p></div>`;
    const foot = el('div', 'panel-foot');
    foot.appendChild(el('div', 'status', 'Thank you for playing Cold Wake.'));
    foot.appendChild(button('Back to title', 'primary', () => game.quitToTitle()));
    panel.appendChild(foot);
    root.appendChild(panel);
    root.hidden = false;
  },

  hideEnd() {
    const root = $('overlay-end');
    root.hidden = true;
    root.innerHTML = '';
  },
};
