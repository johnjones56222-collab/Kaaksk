// Discrete console puzzles: breaker balancing, memory lattice, keypad,
// pipe routing, wire matching and jump-route plotting.
import { audio } from '../../engine/audio.js';
import { el, shuffle } from '../../core/utils.js';
import { STAR_NODES, STAR_EDGES, JUMP_FUEL, edgeCost } from '../../game/stardata.js';

// ---- breakers: switch on the set that draws exactly the target load ----------
export function breakers(api) {
  const pool = shuffle([15, 20, 25, 30, 35, 40, 45, 55, 60, 70]).slice(0, 6);
  const solution = shuffle([0, 1, 2, 3, 4, 5]).slice(0, 3);
  const target = solution.reduce((s, i) => s + pool[i], 0);
  const on = pool.map(() => false);
  const readout = el('div', 'readout', '');
  const meter = el('div', 'meter', '<i></i><em></em>');
  const grid = el('div', 'breakers');
  const max = pool.reduce((s, v) => s + v, 0);
  meter.querySelector('em').style.left = `${(target / max) * 100}%`;
  const btns = pool.map((amps, i) => {
    const b = el('button', 'breaker', `<span class="slot"><i></i></span><b>${amps}</b><small>AMP</small>`);
    b.type = 'button';
    b.setAttribute('aria-pressed', 'false');
    b.addEventListener('click', () => {
      on[i] = !on[i];
      audio.sfx('switch');
      refresh();
    });
    grid.appendChild(b);
    return b;
  });
  function refresh() {
    const sum = on.reduce((s, v, i) => s + (v ? pool[i] : 0), 0);
    btns.forEach((b, i) => {
      b.classList.toggle('on', on[i]);
      b.setAttribute('aria-pressed', String(on[i]));
    });
    readout.innerHTML = `BUS LOAD <b>${sum}</b> / ${target} A`;
    readout.className = `readout ${sum > target ? 'over' : sum === target ? 'ok' : ''}`;
    const fill = meter.querySelector('i');
    fill.style.width = `${Math.min(100, (sum / max) * 100)}%`;
    fill.style.background = sum > target ? 'var(--bad)' : sum === target ? 'var(--good)' : 'var(--accent)';
    if (sum === target) api.win();
    else api.status(sum > target ? 'OVERLOAD - shed load' : `${target - sum} A short`, sum > target ? 'bad' : '');
  }
  api.body.append(readout, meter, grid);
  api.solve = () => {
    on.fill(false);
    solution.forEach((i) => (on[i] = true));
    refresh();
  };
  refresh();
}

// ---- memory lattice: repeat VEGA's boot sequence ------------------------------
export function memory(api) {
  const rounds = [3, 4, 5];
  let round = 0;
  let seq = [];
  let pos = 0;
  let accepting = false;
  const pips = el('div', 'pips', rounds.map(() => '<i></i>').join(''));
  const grid = el('div', 'nodes');
  const tones = [392, 440, 494, 523, 587, 659, 698, 784, 880];
  const nodes = [];
  for (let i = 0; i < 9; i++) {
    const n = el('button', 'node');
    n.type = 'button';
    n.setAttribute('aria-label', `Lattice ${i + 1}`);
    n.addEventListener('click', () => press(i));
    grid.appendChild(n);
    nodes.push(n);
  }
  const light = (i, cls = 'lit', ms = 320) => {
    nodes[i].classList.add(cls);
    audio.tone({ type: 'triangle', f0: tones[i], dur: 0.25, vol: 0.14, bus: audio.uiBus });
    api.after(ms, () => nodes[i].classList.remove(cls));
  };
  function play() {
    accepting = false;
    pos = 0;
    seq = [];
    let last = -1;
    for (let i = 0; i < rounds[round]; i++) {
      let n;
      do n = Math.floor(Math.random() * 9);
      while (n === last);
      seq.push(n);
      last = n;
    }
    api.status('WATCH THE SEQUENCE');
    seq.forEach((n, i) => api.after(500 + i * 520, () => light(n)));
    api.after(500 + seq.length * 520, () => {
      accepting = true;
      api.status('REPEAT IT');
    });
  }
  function press(i) {
    if (!accepting) return;
    if (i === seq[pos]) {
      light(i, 'good', 200);
      pos++;
      if (pos >= seq.length) {
        accepting = false;
        pips.children[round].classList.add('on');
        round++;
        if (round >= rounds.length) api.win();
        else {
          api.status('LATTICE STABLE', 'good');
          api.after(900, play);
        }
      }
    } else {
      accepting = false;
      nodes[i].classList.add('bad');
      audio.sfx('deny');
      api.status('MISMATCH - RESTARTING PASS', 'bad');
      api.after(700, () => nodes[i].classList.remove('bad'));
      api.after(1300, play);
    }
  }
  api.body.append(pips, grid);
  api.solve = () => {
    round = rounds.length;
    api.win();
  };
  api.after(400, play);
}

// ---- keypad -------------------------------------------------------------------
export function keypad(api, code) {
  let entry = '';
  const display = el('div', 'code', '----');
  const pad = el('div', 'keypad');
  const show = () => {
    display.textContent = entry.padEnd(4, '-');
  };
  const press = (k) => {
    if (k === 'C') {
      entry = '';
      audio.sfx('click');
    } else if (entry.length < 4) {
      entry += k;
      audio.sfx('key', { n: Number(k) });
    }
    display.classList.remove('bad');
    show();
    if (entry.length === 4) {
      if (entry === code) api.win();
      else {
        audio.sfx('deny');
        display.classList.add('bad');
        api.status('ACCESS DENIED', 'bad');
        api.after(600, () => {
          entry = '';
          display.classList.remove('bad');
          show();
        });
      }
    }
  };
  for (const k of ['1', '2', '3', '4', '5', '6', '7', '8', '9', 'C', '0', '<']) {
    const b = el('button', '', k === '<' ? '&larr;' : k);
    b.type = 'button';
    b.setAttribute('aria-label', k === '<' ? 'Delete' : k === 'C' ? 'Clear' : k);
    b.addEventListener('click', () => {
      if (k === '<') {
        entry = entry.slice(0, -1);
        audio.sfx('click');
        show();
      } else press(k);
    });
    pad.appendChild(b);
  }
  api.key = (e) => {
    if (/^Digit\d$|^Numpad\d$/.test(e.code)) press(e.code.slice(-1));
    else if (e.code === 'Backspace') {
      entry = entry.slice(0, -1);
      show();
    }
  };
  api.body.append(display, pad);
  api.solve = () => {
    entry = '';
    for (const ch of code) press(ch);
  };
}

// ---- pipes: rotate sections until water reaches the outlet ----------------------
const N = 1;
const E = 2;
const S = 4;
const W = 8;
const rot = (mask, r) => {
  let m = mask;
  for (let i = 0; i < r; i++) m = ((m << 1) & 15) | (m >> 3);
  return m;
};
const SHAPES = { 5: 'M50 0V100', 3: 'M50 0V50H100', 7: 'M50 0V100M50 50H100', 15: 'M50 0V100M0 50H100' };

export function pipes(api) {
  const COLS = 6;
  const ROWS = 4;
  const idx = (c, r) => r * COLS + c;
  let cells;
  let src;
  let dst;
  let solvedRot;
  // carve a random route from the left edge to the right edge
  function generate() {
    src = Math.floor(Math.random() * ROWS);
    const used = new Set();
    const path = [[0, src]];
    used.add(idx(0, src));
    let c = 0;
    let r = src;
    let guard = 0;
    while (c < COLS - 1 && guard++ < 200) {
      const opts = [];
      if (!used.has(idx(c + 1, r))) opts.push([c + 1, r], [c + 1, r]);
      if (r > 0 && !used.has(idx(c, r - 1))) opts.push([c, r - 1]);
      if (r < ROWS - 1 && !used.has(idx(c, r + 1))) opts.push([c, r + 1]);
      if (!opts.length) return false;
      [c, r] = opts[Math.floor(Math.random() * opts.length)];
      used.add(idx(c, r));
      path.push([c, r]);
    }
    dst = r;
    cells = [];
    solvedRot = [];
    const need = new Array(COLS * ROWS).fill(0);
    path.forEach(([pc, pr], i) => {
      let m = 0;
      const prev = i === 0 ? [-1, pr] : path[i - 1];
      const next = i === path.length - 1 ? [COLS, pr] : path[i + 1];
      for (const [qc, qr] of [prev, next]) {
        if (qc < pc) m |= W;
        else if (qc > pc) m |= E;
        else if (qr < pr) m |= N;
        else m |= S;
      }
      need[idx(pc, pr)] = m;
    });
    for (let i = 0; i < COLS * ROWS; i++) {
      let base;
      let sr = 0;
      if (need[i]) {
        const straight = need[i] === (N | S) || need[i] === (E | W);
        base = straight ? 5 : 3;
        if (Math.random() < 0.18) base = 7;
        while ((rot(base, sr) & need[i]) !== need[i]) sr++;
      } else {
        base = [5, 3, 3, 7, 5, 15][Math.floor(Math.random() * 6)];
        sr = -1;
      }
      solvedRot.push(sr);
      cells.push({ base, r: Math.floor(Math.random() * 4), path: !!need[i] });
    }
    return true;
  }
  function flow() {
    const wet = new Set();
    const start = idx(0, src);
    if (!(rot(cells[start].base, cells[start].r) & W)) return { wet, done: false };
    const queue = [start];
    wet.add(start);
    while (queue.length) {
      const i = queue.pop();
      const c = i % COLS;
      const r = Math.floor(i / COLS);
      const m = rot(cells[i].base, cells[i].r);
      for (const [dc, dr, a, b] of [[0, -1, N, S], [1, 0, E, W], [0, 1, S, N], [-1, 0, W, E]]) {
        if (!(m & a)) continue;
        const nc = c + dc;
        const nr = r + dr;
        if (nc < 0 || nc >= COLS || nr < 0 || nr >= ROWS) continue;
        const j = idx(nc, nr);
        if (wet.has(j)) continue;
        if (rot(cells[j].base, cells[j].r) & b) {
          wet.add(j);
          queue.push(j);
        }
      }
    }
    const end = idx(COLS - 1, dst);
    return { wet, done: wet.has(end) && !!(rot(cells[end].base, cells[end].r) & E) };
  }
  let tries = 0;
  do {
    while (!generate() && tries++ < 50);
  } while (flow().done && tries++ < 60);
  const grid = el('div', 'pipes');
  grid.style.gridTemplateColumns = `repeat(${COLS}, 1fr)`;
  grid.style.width = 'min(100%, 420px, 78vh)';
  const tiles = cells.map((cell, i) => {
    const b = el('button', 'pipe', `<svg viewBox="0 0 100 100"><path d="${SHAPES[cell.base]}"/></svg>`);
    b.type = 'button';
    b.setAttribute('aria-label', `Pipe section ${i + 1}`);
    if (i === idx(0, src)) b.classList.add('src');
    if (i === idx(COLS - 1, dst)) b.classList.add('dst');
    b.addEventListener('click', () => {
      cell.r = (cell.r + 1) % 4;
      audio.sfx('click');
      refresh();
    });
    grid.appendChild(b);
    return b;
  });
  const vis = cells.map((c) => c.r);
  function refresh(first) {
    const f = flow();
    tiles.forEach((t, i) => {
      if (!first) while (cells[i].r !== ((vis[i] % 4) + 4) % 4) vis[i] += 1;
      t.firstChild.style.transform = `rotate(${vis[i] * 90}deg)`;
      t.classList.toggle('wet', f.wet.has(i));
    });
    if (f.done) api.win();
    else api.status(`${f.wet.size} sections flooded - reach the amber outlet`);
  }
  api.body.append(grid);
  api.solve = () => {
    cells.forEach((c, i) => {
      if (solvedRot[i] >= 0) {
        while (c.r !== solvedRot[i]) {
          c.r = (c.r + 1) % 4;
        }
      }
    });
    refresh();
  };
  refresh(true);
}

// ---- wires: reconnect each conductor to its matching terminal -------------------
export function wires(api) {
  const colors = [
    ['#ff5a48', 'RED'],
    ['#ffd24a', 'YELLOW'],
    ['#5a9bff', 'BLUE'],
    ['#6dffb0', 'GREEN'],
    ['#ff9a3c', 'ORANGE'],
  ];
  const order = shuffle([0, 1, 2, 3, 4]);
  const box = el('div', 'wires');
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  const left = el('div', 'col');
  const right = el('div', 'col r');
  box.append(svg, left, el('div', ''), right);
  const done = new Set();
  let sel = null;
  let drag = null;
  const L = [];
  const R = [];
  colors.forEach(([c, name], i) => {
    const t = el('button', 'term', '<i></i>');
    t.type = 'button';
    t.style.color = c;
    t.setAttribute('aria-label', `${name} wire`);
    left.appendChild(t);
    L.push(t);
    t.addEventListener('pointerdown', (e) => {
      if (done.has(i)) return;
      e.preventDefault();
      select(i);
      drag = { i, x: e.clientX, y: e.clientY };
      redraw();
    });
  });
  order.forEach((ci) => {
    const t = el('button', 'term', '<i></i>');
    t.type = 'button';
    t.style.color = colors[ci][0];
    t.dataset.c = String(ci);
    t.setAttribute('aria-label', `${colors[ci][1]} terminal`);
    right.appendChild(t);
    R.push(t);
    t.addEventListener('click', () => {
      if (sel != null) connect(sel, ci);
    });
  });
  function select(i) {
    sel = i;
    L.forEach((t, k) => t.classList.toggle('sel', k === i));
    audio.sfx('click');
  }
  function connect(i, ci) {
    if (i === ci) {
      done.add(i);
      L[i].classList.add('done');
      R.find((t) => Number(t.dataset.c) === ci).classList.add('done');
      audio.sfx('confirm', { vol: 0.6 });
      api.status(`${done.size} / 5 conductors seated`, 'good');
    } else {
      audio.sfx('deny');
      audio.sfx('spark');
      api.status('WRONG TERMINAL', 'bad');
    }
    sel = null;
    L.forEach((t) => t.classList.remove('sel'));
    redraw();
    if (done.size === 5) api.win();
  }
  const centre = (t, side) => {
    const b = box.getBoundingClientRect();
    const r = t.getBoundingClientRect();
    return [side < 0 ? r.right - b.left : r.left - b.left, r.top + r.height / 2 - b.top];
  };
  function redraw() {
    const b = box.getBoundingClientRect();
    svg.setAttribute('viewBox', `0 0 ${b.width} ${b.height}`);
    let html = '';
    for (const i of done) {
      const [x0, y0] = centre(L[i], -1);
      const [x1, y1] = centre(R.find((t) => Number(t.dataset.c) === i), 1);
      const mx = (x0 + x1) / 2;
      html += `<path d="M${x0} ${y0} C${mx} ${y0} ${mx} ${y1} ${x1} ${y1}" stroke="${colors[i][0]}" stroke-width="6" fill="none" stroke-linecap="round"/>`;
    }
    if (drag) {
      const [x0, y0] = centre(L[drag.i], -1);
      html += `<path d="M${x0} ${y0} L${drag.x - b.left} ${drag.y - b.top}" stroke="${colors[drag.i][0]}" stroke-width="5" fill="none" stroke-linecap="round" stroke-dasharray="2 9"/>`;
    }
    svg.innerHTML = html;
  }
  const move = (e) => {
    if (!drag) return;
    drag.x = e.clientX;
    drag.y = e.clientY;
    redraw();
  };
  const up = (e) => {
    if (!drag) return;
    const d = drag;
    drag = null;
    const hit = document.elementFromPoint(e.clientX, e.clientY);
    const term = hit && hit.closest ? hit.closest('.col.r .term') : null;
    if (term) connect(d.i, Number(term.dataset.c));
    else redraw();
  };
  window.addEventListener('pointermove', move);
  window.addEventListener('pointerup', up);
  window.addEventListener('resize', redraw);
  api.onClose(() => {
    window.removeEventListener('pointermove', move);
    window.removeEventListener('pointerup', up);
    window.removeEventListener('resize', redraw);
  });
  api.body.append(box);
  api.status('0 / 5 conductors seated');
  api.solve = () => {
    for (let i = 0; i < 5; i++) if (!done.has(i)) connect(i, i);
  };
  api.after(50, redraw);
}

// ---- jump route: reach Meridian Station inside the fuel budget -------------------
export function route(api) {
  const Wd = 760;
  const Hd = 420;
  const P = (n) => [40 + n.x * (Wd - 110), 40 + n.y * (Hd - 80)];
  let path = ['A'];
  const box = el('div', 'route');
  const readout = el('div', 'readout', '');
  const row = el('div', 'mg-row');
  const undo = el('button', 'btn small', 'Undo jump');
  const reset = el('button', 'btn small', 'Clear');
  undo.type = reset.type = 'button';
  row.append(undo, reset);
  const used = () => {
    let s = 0;
    for (let i = 1; i < path.length; i++) s += edgeCost(path[i - 1], path[i]);
    return s;
  };
  function draw() {
    const last = path[path.length - 1];
    const fuel = JUMP_FUEL - used();
    let h = `<svg viewBox="0 0 ${Wd} ${Hd}" role="img" aria-label="Jump network">`;
    for (const [a, b, c] of STAR_EDGES) {
      const A = STAR_NODES.find((n) => n.id === a);
      const B = STAR_NODES.find((n) => n.id === b);
      const [x0, y0] = P(A);
      const [x1, y1] = P(B);
      let on = false;
      for (let i = 1; i < path.length; i++) if ((path[i - 1] === a && path[i] === b) || (path[i - 1] === b && path[i] === a)) on = true;
      h += `<line class="edge ${on ? 'on' : ''}" x1="${x0}" y1="${y0}" x2="${x1}" y2="${y1}"/>`;
      h += `<text class="cost" x="${(x0 + x1) / 2 + 6}" y="${(y0 + y1) / 2 - 6}">${c}</text>`;
    }
    for (const n of STAR_NODES) {
      const [x, y] = P(n);
      const inPath = path.includes(n.id);
      const c = edgeCost(last, n.id);
      const reachable = !inPath && c != null && c <= fuel && n.kind !== 'hazard';
      h += `<circle class="n ${inPath ? 'on' : ''} ${reachable ? 'next' : ''} ${n.kind || ''}" cx="${x}" cy="${y}" r="${n.kind ? 13 : 10}"/>`;
      h += `<text x="${x + (n.x > 0.8 ? -16 : 16)}" y="${y + (n.y > 0.7 ? 26 : -16)}" text-anchor="${n.x > 0.8 ? 'end' : 'start'}">${n.name}</text>`;
      h += `<circle class="hit" data-id="${n.id}" cx="${x}" cy="${y}" r="30"/>`;
    }
    h += '</svg>';
    box.innerHTML = h;
    readout.innerHTML = `JUMP FUEL <b>${fuel}</b> / ${JUMP_FUEL} units`;
    readout.className = `readout ${fuel <= 2 ? 'over' : ''}`;
    box.querySelectorAll('.hit').forEach((c) => c.addEventListener('click', () => pick(c.dataset.id)));
  }
  function pick(id) {
    const last = path[path.length - 1];
    const node = STAR_NODES.find((n) => n.id === id);
    if (id === last && path.length > 1) {
      path.pop();
      audio.sfx('click');
    } else if (path.includes(id)) {
      api.status('Already on the route', '');
      return draw();
    } else {
      const c = edgeCost(last, id);
      if (c == null) {
        audio.sfx('deny');
        api.status('No jump lane between those points', 'bad');
        return;
      }
      if (node.kind === 'hazard') {
        audio.sfx('deny');
        api.status('PULSAR XK-9: lethal radiation. The captain forbade it.', 'bad');
        return;
      }
      if (c > JUMP_FUEL - used()) {
        audio.sfx('deny');
        api.status('Not enough fuel for that jump - undo and try another lane', 'bad');
        return;
      }
      path.push(id);
      audio.sfx('uiHi');
      api.status(`${path.length - 1} jump${path.length > 2 ? 's' : ''} plotted`);
    }
    draw();
    if (id === 'I' && path[path.length - 1] === 'I') api.win(path.slice());
  }
  undo.addEventListener('click', () => {
    if (path.length > 1) path.pop();
    audio.sfx('click');
    draw();
  });
  reset.addEventListener('click', () => {
    path = ['A'];
    audio.sfx('click');
    draw();
  });
  api.body.append(readout, box, row);
  api.status('Select a neighbouring system to add a jump');
  api.solve = () => {
    path = ['A'];
    for (const id of ['B', 'E', 'H', 'I']) pick(id);
  };
  draw();
}
