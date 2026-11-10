// Unified input: keyboard + mouse (pointer lock, with a drag-to-look fallback
// where lock is unavailable) and multi-touch (floating move stick, drag-look,
// on-screen buttons). The rest of the game only reads this object.
import { IS_TOUCH, settings } from '../core/state.js';
import { clamp } from '../core/utils.js';
import { bus } from '../core/events.js';

export const input = {
  moveX: 0,
  moveY: 0,
  lookX: 0,
  lookY: 0,
  sprint: false,
  held: { interact: false, fire: false, alt: false },
  aim: { x: 0, y: 0, active: false },
  mode: IS_TOUCH ? 'touch' : 'kbm',
  capture: false, // true while the player is in control (no menu / minigame)
  locked: false,
  lockFailed: false,
  _pressed: new Set(),
  pressed(a) {
    return this._pressed.has(a);
  },
  press(a) {
    this._pressed.add(a);
  },
  endFrame() {
    this._pressed.clear();
    this.lookX = 0;
    this.lookY = 0;
  },
};

const keys = new Set();
let canvas = null;
let stick = null;
let look = null;
let els = {};
let mouseDrag = false;
let lastMouse = { x: 0, y: 0 };

const KEYMAP = {
  KeyE: 'interact',
  Enter: 'interact',
  Space: 'jump',
  KeyF: 'flash',
  Escape: 'pause',
  KeyP: 'pause',
  KeyM: 'map',
  KeyL: 'logs',
  Tab: 'mission',
  KeyH: 'hint',
};

function setMode(m) {
  if (input.mode === m) return;
  input.mode = m;
  document.body.classList.toggle('touch', m === 'touch');
  bus.emit('inputmode', m);
}

function updateKeys() {
  let x = 0;
  let y = 0;
  if (keys.has('KeyW') || keys.has('ArrowUp')) y += 1;
  if (keys.has('KeyS') || keys.has('ArrowDown')) y -= 1;
  if (keys.has('KeyD') || keys.has('ArrowRight')) x += 1;
  if (keys.has('KeyA') || keys.has('ArrowLeft')) x -= 1;
  if (!stick) {
    input.moveX = x;
    input.moveY = y;
    input.sprint = keys.has('ShiftLeft') || keys.has('ShiftRight');
  }
  input.held.interact = keys.has('KeyE') || keys.has('Enter') || !!els.interactHeld;
  input.held.alt = keys.has('Space');
}

export function requestLock() {
  if (!canvas || input.mode === 'touch' || input.lockFailed) return;
  if (document.pointerLockElement === canvas) return;
  try {
    const p = canvas.requestPointerLock?.();
    if (p && typeof p.catch === 'function') p.catch(() => {});
  } catch {
    input.lockFailed = true;
  }
}

export function releaseLock() {
  if (document.pointerLockElement) {
    try {
      document.exitPointerLock();
    } catch {
      /* ignore */
    }
  }
}

export function clearInput() {
  keys.clear();
  stick = null;
  look = null;
  mouseDrag = false;
  input.moveX = 0;
  input.moveY = 0;
  input.sprint = false;
  input.held.interact = false;
  input.held.fire = false;
  input.held.alt = false;
  els.interactHeld = false;
  if (els.stickBase) els.stickBase.hidden = true;
}

function aimFrom(e) {
  const r = canvas.getBoundingClientRect();
  input.aim.x = clamp(((e.clientX - r.left) / r.width) * 2 - 1, -1, 1);
  input.aim.y = clamp(-(((e.clientY - r.top) / r.height) * 2 - 1), -1, 1);
  input.aim.active = true;
}

export function initInput(canvasEl) {
  canvas = canvasEl;
  els = {
    layer: document.getElementById('touch-layer'),
    stickBase: document.getElementById('stick-base'),
    stickKnob: document.getElementById('stick-knob'),
    interactHeld: false,
  };
  document.body.classList.toggle('touch', input.mode === 'touch');

  window.addEventListener('keydown', (e) => {
    if (e.target && /INPUT|TEXTAREA|SELECT/.test(e.target.tagName)) return;
    if (e.code === 'Tab' || e.code === 'Space' || e.code.startsWith('Arrow')) e.preventDefault();
    if (!e.repeat) {
      setMode('kbm');
      const a = KEYMAP[e.code];
      if (a) input.press(a);
      bus.emit('key', e.code);
    }
    keys.add(e.code);
    updateKeys();
  });
  window.addEventListener('keyup', (e) => {
    keys.delete(e.code);
    updateKeys();
  });
  window.addEventListener('blur', () => {
    clearInput();
  });

  document.addEventListener('pointerlockchange', () => {
    input.locked = document.pointerLockElement === canvas;
    bus.emit('lock', input.locked);
  });
  document.addEventListener('pointerlockerror', () => {
    input.lockFailed = true;
    input.locked = false;
    bus.emit('lockfailed');
  });

  // ---- mouse ---------------------------------------------------------------
  canvas.addEventListener('mousedown', (e) => {
    if (input.mode === 'touch') return;
    if (!input.capture) return;
    if (e.button === 0) {
      input.held.fire = true;
      if (input.locked) {
        input.press('click');
      } else if (input.lockFailed) {
        mouseDrag = true;
        lastMouse = { x: e.clientX, y: e.clientY };
        input.press('click');
      } else {
        requestLock();
      }
      aimFrom(e);
    }
  });
  window.addEventListener('mouseup', (e) => {
    if (e.button === 0) {
      input.held.fire = false;
      mouseDrag = false;
    }
  });
  window.addEventListener('mousemove', (e) => {
    if (input.mode === 'touch' && e.movementX === 0 && e.movementY === 0) return;
    if (input.locked) {
      // guard against the occasional huge spurious delta some browsers emit
      const mx = clamp(e.movementX || 0, -150, 150);
      const my = clamp(e.movementY || 0, -150, 150);
      input.lookX += mx;
      input.lookY += my;
      input.aim.x = clamp(input.aim.x + mx / (canvas.clientWidth * 0.5), -1, 1);
      input.aim.y = clamp(input.aim.y - my / (canvas.clientHeight * 0.5), -1, 1);
      input.aim.active = true;
    } else if (input.capture) {
      if (mouseDrag) {
        input.lookX += (e.clientX - lastMouse.x) * 1.4;
        input.lookY += (e.clientY - lastMouse.y) * 1.4;
        lastMouse = { x: e.clientX, y: e.clientY };
      }
      if (e.target === canvas || e.target === els.layer) aimFrom(e);
    }
  });
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());

  // ---- touch ---------------------------------------------------------------
  const layer = els.layer;
  const R = 58;
  const onDown = (e) => {
    if (e.pointerType === 'mouse') return;
    setMode('touch');
    if (!input.capture) return;
    e.preventDefault();
    const leftSide = e.clientX < window.innerWidth * 0.42;
    if (leftSide && !stick && !input.aimOnly) {
      stick = { id: e.pointerId, x: e.clientX, y: e.clientY };
      els.stickBase.hidden = false;
      els.stickBase.style.left = `${e.clientX}px`;
      els.stickBase.style.top = `${e.clientY}px`;
      els.stickKnob.style.transform = 'translate(-50%,-50%)';
    } else if (!look) {
      look = { id: e.pointerId, x: e.clientX, y: e.clientY, sx: e.clientX, sy: e.clientY, t: performance.now() };
      if (input.aimOnly) {
        aimFrom(e);
        input.held.fire = true;
      }
    }
    try {
      layer.setPointerCapture(e.pointerId);
    } catch {
      /* ignore */
    }
  };
  const onMove = (e) => {
    if (e.pointerType === 'mouse') return;
    if (stick && e.pointerId === stick.id) {
      let dx = e.clientX - stick.x;
      let dy = e.clientY - stick.y;
      const len = Math.hypot(dx, dy);
      if (len > R) {
        dx = (dx / len) * R;
        dy = (dy / len) * R;
      }
      els.stickKnob.style.transform = `translate(calc(-50% + ${dx}px), calc(-50% + ${dy}px))`;
      let mx = dx / R;
      let my = -dy / R;
      const mag = Math.hypot(mx, my);
      if (mag < 0.14) {
        mx = 0;
        my = 0;
      }
      input.moveX = mx;
      input.moveY = my;
      input.sprint = mag > 0.93;
    } else if (look && e.pointerId === look.id) {
      if (input.aimOnly) {
        aimFrom(e);
      } else {
        input.lookX += (e.clientX - look.x) * 2.1;
        input.lookY += (e.clientY - look.y) * 2.1;
      }
      look.x = e.clientX;
      look.y = e.clientY;
    }
  };
  const onUp = (e) => {
    if (e.pointerType === 'mouse') return;
    if (stick && e.pointerId === stick.id) {
      stick = null;
      els.stickBase.hidden = true;
      input.moveX = 0;
      input.moveY = 0;
      input.sprint = false;
    } else if (look && e.pointerId === look.id) {
      if (input.aimOnly) input.held.fire = false;
      look = null;
    }
  };
  layer.addEventListener('pointerdown', onDown);
  layer.addEventListener('pointermove', onMove);
  layer.addEventListener('pointerup', onUp);
  layer.addEventListener('pointercancel', onUp);
  layer.addEventListener('contextmenu', (e) => e.preventDefault());

  // on-screen buttons: data-action = tap action, data-hold = held flag
  for (const btn of document.querySelectorAll('[data-action]')) {
    const action = btn.dataset.action;
    const down = (e) => {
      e.preventDefault();
      e.stopPropagation();
      if (e.pointerType !== 'mouse') setMode('touch');
      btn.classList.add('down');
      input.press(action);
      if (action === 'interact') {
        els.interactHeld = true;
        input.held.interact = true;
      }
      try {
        btn.setPointerCapture(e.pointerId);
      } catch {
        /* ignore */
      }
    };
    const up = (e) => {
      e.preventDefault();
      btn.classList.remove('down');
      if (action === 'interact') {
        els.interactHeld = false;
        updateKeys();
      }
    };
    btn.addEventListener('pointerdown', down);
    btn.addEventListener('pointerup', up);
    btn.addEventListener('pointercancel', up);
    btn.addEventListener('contextmenu', (e) => e.preventDefault());
  }
}

export const lookScale = () => (input.mode === 'touch' ? 0.0023 : 0.0022) * settings.sensitivity;
