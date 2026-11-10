// Modal frame shared by every console puzzle. A puzzle receives `api` and
// calls api.win() when solved; the promise resolves with { ok, data }.
import { audio } from '../../engine/audio.js';
import { el } from '../../core/utils.js';

let active = null;

export function minigameOpen() {
  return !!active;
}

export function openMinigame({ title, sub = '', hint = '', build, wide = false }) {
  const root = document.getElementById('overlay-minigame');
  root.innerHTML = '';
  const panel = el('div', 'panel mg');
  if (wide) panel.style.width = 'min(760px, 100%)';
  panel.setAttribute('role', 'dialog');
  panel.setAttribute('aria-label', title);
  panel.innerHTML = `
    <div class="panel-head"><h2>${title}</h2><p>${sub}</p></div>
    <div class="panel-body"></div>
    <div class="panel-foot"><div class="status"></div><button type="button" class="btn small" data-x>Step away</button></div>`;
  root.appendChild(panel);
  root.hidden = false;
  const body = panel.querySelector('.panel-body');
  if (hint) body.appendChild(el('p', 'hint-line', hint));
  const statusEl = panel.querySelector('.status');
  return new Promise((resolve) => {
    const timers = new Set();
    const cleanups = [];
    let done = false;
    const api = {
      body,
      panel,
      tick: null,
      solve: null,
      status(text, kind = '') {
        statusEl.textContent = text;
        statusEl.className = `status ${kind}`;
      },
      after(ms, fn) {
        const id = setTimeout(() => {
          timers.delete(id);
          if (!done) fn();
        }, ms);
        timers.add(id);
        return id;
      },
      onClose(fn) {
        cleanups.push(fn);
      },
      win(data) {
        if (done) return;
        done = true;
        audio.sfx('confirm');
        api.status('COMPLETE', 'good');
        panel.style.borderColor = 'var(--good)';
        setTimeout(() => finish(true, data), 650);
      },
      close() {
        if (done) return;
        done = true;
        finish(false);
      },
    };
    function finish(ok, data) {
      for (const id of timers) clearTimeout(id);
      for (const fn of cleanups) {
        try {
          fn();
        } catch {
          /* ignore */
        }
      }
      window.removeEventListener('keydown', onKey, true);
      root.hidden = true;
      root.innerHTML = '';
      active = null;
      resolve({ ok, data });
    }
    const onKey = (e) => {
      if (e.code === 'Escape') {
        e.preventDefault();
        e.stopImmediatePropagation();
        api.close();
      } else if (api.key) {
        api.key(e);
      }
    };
    window.addEventListener('keydown', onKey, true);
    panel.querySelector('[data-x]').addEventListener('click', () => {
      audio.sfx('click');
      api.close();
    });
    active = api;
    try {
      build(api);
    } catch (err) {
      console.error('[minigame]', err);
      api.close();
    }
  });
}

// Per-frame hook for canvas puzzles.
export function tickMinigame(dt) {
  if (active && active.tick) active.tick(dt);
}

// Used by the automated playthrough test.
export function solveMinigame() {
  if (active && active.solve) {
    active.solve();
    return true;
  }
  return false;
}

export function abortMinigame() {
  if (active) active.close();
}

// Canvas sized for the device, with drawing done in CSS pixels.
export function fitCanvas(w, h) {
  const c = document.createElement('canvas');
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  c.width = Math.round(w * dpr);
  c.height = Math.round(h * dpr);
  c.style.width = `${w}px`;
  c.style.aspectRatio = `${w} / ${h}`;
  const ctx = c.getContext('2d');
  ctx.scale(dpr, dpr);
  return { canvas: c, ctx, w, h };
}
