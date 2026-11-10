// Analogue console puzzles: injector timing, compound mixing, signal phase
// matching and dish alignment.
import { audio } from '../../engine/audio.js';
import { el, clamp, TAU, angleDelta } from '../../core/utils.js';
import { fitCanvas } from './base.js';

const size = () => Math.round(Math.min(300, window.innerWidth * 0.7, window.innerHeight * 0.48));

// ---- reactor ignition: fire four injectors in step ------------------------------
export function sync(api) {
  const S = size();
  const { canvas, ctx } = fitCanvas(S, S);
  const stages = [
    { arc: 1.15, speed: 1.7 },
    { arc: 0.9, speed: 2.2 },
    { arc: 0.72, speed: 2.7 },
    { arc: 0.58, speed: 3.2 },
  ];
  let stage = 0;
  let ang = 0;
  let dir = 1;
  let target = 1.2;
  let flash = 0;
  let flashCol = '#6dffb0';
  const fire = el('button', 'btn primary', 'Fire injector');
  fire.type = 'button';
  const pips = el('div', 'pips', stages.map(() => '<i></i>').join(''));
  function attempt() {
    const st = stages[Math.min(stage, stages.length - 1)];
    const off = Math.abs(angleDelta(target, ((ang % TAU) + TAU) % TAU));
    if (off <= st.arc / 2) {
      pips.children[stage].classList.add('on');
      stage++;
      flash = 1;
      flashCol = '#6dffb0';
      audio.sfx('confirm', { vol: 0.7 });
      audio.tone({ type: 'sawtooth', f0: 60 + stage * 25, f1: 120 + stage * 40, dur: 0.5, vol: 0.14 });
      if (stage >= stages.length) {
        api.win();
        return;
      }
      dir = -dir;
      target = Math.random() * TAU;
      api.status(`INJECTOR ${stage} IN STEP`, 'good');
    } else {
      flash = 1;
      flashCol = '#ff5a48';
      audio.sfx('deny');
      if (stage > 0) {
        stage--;
        pips.children[stage].classList.remove('on');
      }
      api.status('OUT OF STEP - lost an injector', 'bad');
    }
  }
  fire.addEventListener('click', attempt);
  canvas.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    attempt();
  });
  api.key = (e) => {
    if (e.code === 'Space' || e.code === 'Enter' || e.code === 'KeyE') {
      e.preventDefault();
      if (!e.repeat) attempt();
    }
  };
  api.tick = (dt) => {
    const st = stages[Math.min(stage, stages.length - 1)];
    ang += dir * st.speed * dt;
    flash = Math.max(0, flash - dt * 3);
    const c = S / 2;
    const R = S * 0.38;
    ctx.clearRect(0, 0, S, S);
    ctx.lineCap = 'butt';
    ctx.strokeStyle = 'rgba(95,227,255,0.16)';
    ctx.lineWidth = S * 0.075;
    ctx.beginPath();
    ctx.arc(c, c, R, 0, TAU);
    ctx.stroke();
    ctx.strokeStyle = '#ffb347';
    ctx.beginPath();
    ctx.arc(c, c, R, target - st.arc / 2, target + st.arc / 2);
    ctx.stroke();
    for (let i = 0; i < 36; i++) {
      const a = (i / 36) * TAU;
      ctx.strokeStyle = 'rgba(95,227,255,0.35)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(c + Math.cos(a) * (R + S * 0.055), c + Math.sin(a) * (R + S * 0.055));
      ctx.lineTo(c + Math.cos(a) * (R + S * 0.075), c + Math.sin(a) * (R + S * 0.075));
      ctx.stroke();
    }
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 4;
    ctx.beginPath();
    ctx.moveTo(c + Math.cos(ang) * (R - S * 0.06), c + Math.sin(ang) * (R - S * 0.06));
    ctx.lineTo(c + Math.cos(ang) * (R + S * 0.06), c + Math.sin(ang) * (R + S * 0.06));
    ctx.stroke();
    const g = ctx.createRadialGradient(c, c, 0, c, c, R * 0.7);
    const heat = stage / stages.length;
    g.addColorStop(0, `rgba(${160 + 95 * heat},${220},255,${0.25 + 0.6 * heat})`);
    g.addColorStop(1, 'rgba(40,120,255,0)');
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.arc(c, c, R * 0.7 * (0.8 + 0.05 * Math.sin(ang * 3)), 0, TAU);
    ctx.fill();
    if (flash > 0) {
      ctx.strokeStyle = flashCol;
      ctx.globalAlpha = flash;
      ctx.lineWidth = 6;
      ctx.beginPath();
      ctx.arc(c, c, R + S * 0.085, 0, TAU);
      ctx.stroke();
      ctx.globalAlpha = 1;
    }
    ctx.fillStyle = '#d8f4ff';
    ctx.font = `600 ${Math.round(S * 0.11)}px ui-monospace,Menlo,Consolas,monospace`;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(`${stage}/4`, c, c);
  };
  api.body.append(pips, canvas, fire);
  api.status('Fire when the white marker crosses the amber band');
  api.solve = () => {
    stage = stages.length;
    api.win();
  };
}

// ---- compound synthesis: match the target colour with three reagents -----------
export function mix(api) {
  const base = [
    [255, 70, 110],
    [70, 240, 170],
    [80, 140, 255],
  ];
  const target = [0, 0, 0].map(() => 20 + Math.round(Math.random() * 13) * 5);
  const val = [50, 50, 50];
  while (Math.max(...target.map((t, i) => Math.abs(t - val[i]))) < 15) target[0] = target[0] > 50 ? 20 : 85;
  const colour = (v) => {
    const sum = v[0] + v[1] + v[2] || 1;
    const lum = clamp(sum / 190, 0.25, 1.15);
    const c = [0, 1, 2].map((k) => clamp(((v[0] * base[0][k] + v[1] * base[1][k] + v[2] * base[2][k]) / sum) * lum, 0, 255));
    return `rgb(${c[0] | 0},${c[1] | 0},${c[2] | 0})`;
  };
  const sw = el('div', 'swatches');
  const a = el('div', '', '<div class="swatch"></div><div class="swatch-label">TARGET</div>');
  const b = el('div', '', '<div class="swatch"></div><div class="swatch-label">YOUR MIX</div>');
  sw.append(a, b);
  a.firstChild.style.background = colour(target);
  const readout = el('div', 'readout', '');
  const meter = el('div', 'meter', '<i></i>');
  const sl = el('div', 'sliders');
  const inputs = [];
  const outs = [];
  ['REAGENT A', 'REAGENT B', 'REAGENT C'].forEach((name, i) => {
    const lab = el('label', '', name);
    lab.htmlFor = `mix-${i}`;
    const inp = el('input');
    inp.type = 'range';
    inp.min = '0';
    inp.max = '100';
    inp.step = '1';
    inp.value = '50';
    inp.id = `mix-${i}`;
    inp.style.accentColor = `rgb(${base[i].join(',')})`;
    const out = el('output', '', '50');
    inp.addEventListener('input', () => {
      val[i] = Number(inp.value);
      refresh();
    });
    sl.append(lab, inp, out);
    inputs.push(inp);
    outs.push(out);
  });
  let hold = 0;
  let match = 0;
  function refresh() {
    b.firstChild.style.background = colour(val);
    const diffs = val.map((v, i) => v - target[i]);
    const worst = Math.max(...diffs.map(Math.abs));
    const avg = diffs.reduce((s, d) => s + Math.abs(d), 0) / 3;
    match = clamp(100 - avg * 2.2, 0, 100);
    readout.innerHTML = `MATCH <b>${match.toFixed(0)}%</b>`;
    readout.className = `readout ${worst <= 5 ? 'ok' : ''}`;
    meter.firstChild.style.width = `${match}%`;
    meter.firstChild.style.background = worst <= 5 ? 'var(--good)' : 'var(--accent)';
    // once close, the synthesiser tells you which way each reagent is off
    outs.forEach((o, i) => {
      const d = diffs[i];
      const hintOn = match >= 62;
      o.textContent = `${val[i]}${hintOn ? (Math.abs(d) <= 5 ? ' =' : d > 0 ? ' -' : ' +') : ''}`;
    });
    api.locked = worst <= 5;
    api.status(api.locked ? 'Compound stable - holding' : match >= 62 ? 'Close: + means add, - means reduce' : 'Adjust the reagents until the colours match');
  }
  api.tick = (dt) => {
    if (api.locked) {
      hold += dt;
      if (hold > 0.9) api.win();
    } else hold = 0;
  };
  api.body.append(sw, readout, meter, sl);
  api.solve = () => {
    target.forEach((t, i) => {
      inputs[i].value = String(t);
      val[i] = t;
    });
    refresh();
    hold = 1;
  };
  refresh();
}

// ---- signal analysis: phase-match the spectrometer to the carrier ---------------
export function wave(api) {
  const W = Math.round(Math.min(560, window.innerWidth - 80));
  const H = Math.round(Math.min(190, window.innerHeight * 0.3));
  const { canvas, ctx } = fitCanvas(W, H);
  // The sliders move in clear steps and the carrier sits exactly on one, so
  // the match is findable by eye on a phone as well as with a mouse.
  const PH = TAU / 24;
  const pick = (n) => Math.floor(Math.random() * n);
  const target = { amp: 0.5 + pick(9) * 0.05, freq: 1.5 + pick(15) * 0.25, phase: (2 + pick(21)) * PH };
  const cur = { amp: 0.3, freq: 1, phase: 0 };
  const defs = [
    ['AMPLITUDE', 'amp', 0.2, 1, 0.05],
    ['FREQUENCY', 'freq', 1, 6, 0.25],
    ['PHASE', 'phase', 0, TAU - PH, PH],
  ];
  const sl = el('div', 'sliders');
  const inputs = {};
  defs.forEach(([name, key, min, max, step]) => {
    const lab = el('label', '', name);
    lab.htmlFor = `wave-${key}`;
    const inp = el('input');
    inp.type = 'range';
    inp.id = `wave-${key}`;
    inp.min = String(min);
    inp.max = String(max);
    inp.step = String(step);
    inp.value = String(cur[key]);
    const out = el('output', '', cur[key].toFixed(2));
    inp.addEventListener('input', () => {
      cur[key] = Number(inp.value);
      out.textContent = cur[key].toFixed(2);
    });
    sl.append(lab, inp, out);
    inputs[key] = inp;
  });
  const readout = el('div', 'readout', '');
  const meter = el('div', 'meter', '<i></i><em style="left:94%"></em>');
  let t = 0;
  let hold = 0;
  const f = (p, x) => p.amp * Math.sin(x * p.freq * TAU + p.phase);
  api.tick = (dt) => {
    t += dt;
    // score: mean squared difference between the two traces
    let err = 0;
    const n = 80;
    for (let i = 0; i < n; i++) {
      const x = i / n;
      const d = f(target, x) - f(cur, x);
      err += d * d;
    }
    err /= n;
    const match = clamp(100 * (1 - Math.sqrt(err) / 0.75), 0, 100);
    readout.innerHTML = `PHASE LOCK <b>${match.toFixed(0)}%</b>`;
    readout.className = `readout ${match >= 94 ? 'ok' : ''}`;
    meter.firstChild.style.width = `${match}%`;
    meter.firstChild.style.background = match >= 94 ? 'var(--good)' : 'var(--violet)';
    if (match >= 94) {
      hold += dt;
      api.status('Carrier locked - resolving lower layer...', 'good');
      if (hold > 1.1) api.win();
    } else {
      hold = 0;
      api.status('Overlay your trace on the dashed carrier');
    }
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = '#030c14';
    ctx.fillRect(0, 0, W, H);
    ctx.strokeStyle = 'rgba(183,155,255,0.14)';
    ctx.lineWidth = 1;
    for (let i = 1; i < 8; i++) {
      ctx.beginPath();
      ctx.moveTo((W / 8) * i, 0);
      ctx.lineTo((W / 8) * i, H);
      ctx.stroke();
    }
    ctx.beginPath();
    ctx.moveTo(0, H / 2);
    ctx.lineTo(W, H / 2);
    ctx.stroke();
    const trace = (p, color, dash, jitter) => {
      ctx.strokeStyle = color;
      ctx.lineWidth = 2.2;
      ctx.setLineDash(dash);
      ctx.beginPath();
      for (let x = 0; x <= W; x += 3) {
        const y = H / 2 - f(p, x / W) * H * 0.42 + (jitter ? Math.sin(x * 0.7 + t * 30) * 1.2 : 0);
        if (x === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
      ctx.setLineDash([]);
    };
    trace(target, '#b79bff', [7, 6], true);
    trace(cur, match >= 94 ? '#6dffb0' : '#5fe3ff', [], false);
  };
  api.body.append(canvas, readout, meter, sl);
  api.solve = () => {
    for (const k of ['amp', 'freq', 'phase']) {
      cur[k] = target[k];
      inputs[k].value = String(target[k]);
    }
    hold = 2;
  };
}

// ---- dish alignment: hold the servo reticle on a drifting relay ------------------
export function align(api) {
  const W = Math.round(Math.min(480, window.innerWidth - 80));
  const H = Math.round(Math.min(280, window.innerHeight * 0.44));
  const { canvas, ctx } = fitCanvas(W, H);
  const ret = { x: W * 0.2, y: H * 0.7 };
  const want = { x: W * 0.2, y: H * 0.7 };
  let t = Math.random() * 10;
  let lock = 0;
  const keys = new Set();
  const meter = el('div', 'meter', '<i></i>');
  const readout = el('div', 'readout', '');
  const setWant = (e) => {
    const r = canvas.getBoundingClientRect();
    want.x = clamp(((e.clientX - r.left) / r.width) * W, 0, W);
    want.y = clamp(((e.clientY - r.top) / r.height) * H, 0, H);
  };
  let down = false;
  canvas.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    down = true;
    try {
      canvas.setPointerCapture(e.pointerId);
    } catch {
      /* ignore */
    }
    setWant(e);
  });
  canvas.addEventListener('pointermove', (e) => {
    if (down || e.pointerType === 'mouse') setWant(e);
  });
  canvas.addEventListener('pointerup', () => (down = false));
  api.key = (e) => {
    if (e.code.startsWith('Arrow') || /^Key[WASD]$/.test(e.code)) {
      keys.add(e.code);
      e.preventDefault();
    }
  };
  const keyUp = (e) => keys.delete(e.code);
  window.addEventListener('keyup', keyUp);
  api.onClose(() => window.removeEventListener('keyup', keyUp));
  api.tick = (dt) => {
    t += dt;
    const k = 150 * dt;
    if (keys.has('ArrowLeft') || keys.has('KeyA')) want.x -= k;
    if (keys.has('ArrowRight') || keys.has('KeyD')) want.x += k;
    if (keys.has('ArrowUp') || keys.has('KeyW')) want.y -= k;
    if (keys.has('ArrowDown') || keys.has('KeyS')) want.y += k;
    want.x = clamp(want.x, 0, W);
    want.y = clamp(want.y, 0, H);
    // the relay drifts on a slow Lissajous path
    const tx = W / 2 + Math.sin(t * 0.55) * W * 0.3 + Math.sin(t * 1.3) * W * 0.06;
    const ty = H / 2 + Math.cos(t * 0.4) * H * 0.26 + Math.sin(t * 1.7) * H * 0.07;
    // servo lag: the reticle chases your input
    const lag = 1 - Math.exp(-3.2 * dt);
    ret.x += (want.x - ret.x) * lag;
    ret.y += (want.y - ret.y) * lag;
    const d = Math.hypot(ret.x - tx, ret.y - ty);
    const rad = Math.min(W, H) * 0.13;
    const on = d < rad;
    lock = clamp(lock + (on ? dt * 0.3 : -dt * 0.12), 0, 1);
    meter.firstChild.style.width = `${lock * 100}%`;
    meter.firstChild.style.background = on ? 'var(--good)' : 'var(--accent)';
    readout.innerHTML = `CARRIER LOCK <b>${Math.round(lock * 100)}%</b>`;
    readout.className = `readout ${on ? 'ok' : ''}`;
    api.status(on ? 'On target - hold it there' : 'Drag (or use arrow keys) to steer the dish onto the relay', on ? 'good' : '');
    if (lock >= 1) api.win();
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = '#020a12';
    ctx.fillRect(0, 0, W, H);
    for (let i = 0; i < 40; i++) {
      const sx = (Math.sin(i * 91.7) * 0.5 + 0.5) * W;
      const sy = (Math.sin(i * 47.3 + 2) * 0.5 + 0.5) * H;
      ctx.fillStyle = `rgba(200,230,255,${0.2 + 0.3 * Math.abs(Math.sin(t + i))})`;
      ctx.fillRect(sx, sy, 1.5, 1.5);
    }
    ctx.strokeStyle = 'rgba(95,227,255,0.12)';
    ctx.lineWidth = 1;
    for (let x = 0; x < W; x += 40) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, H);
      ctx.stroke();
    }
    for (let y = 0; y < H; y += 40) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(W, y);
      ctx.stroke();
    }
    // relay beacon
    const pulse = 0.5 + 0.5 * Math.sin(t * 5);
    ctx.strokeStyle = `rgba(109,255,176,${0.3 + 0.5 * pulse})`;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(tx, ty, rad * (0.5 + 0.5 * pulse), 0, TAU);
    ctx.stroke();
    ctx.fillStyle = '#6dffb0';
    ctx.beginPath();
    ctx.arc(tx, ty, 4, 0, TAU);
    ctx.fill();
    ctx.font = '11px ui-monospace,Menlo,Consolas,monospace';
    ctx.fillText('MERIDIAN RELAY', tx + 10, ty - 10);
    // reticle
    ctx.strokeStyle = on ? '#6dffb0' : '#5fe3ff';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(ret.x, ret.y, rad, 0, TAU);
    ctx.moveTo(ret.x - rad - 8, ret.y);
    ctx.lineTo(ret.x - rad * 0.4, ret.y);
    ctx.moveTo(ret.x + rad * 0.4, ret.y);
    ctx.lineTo(ret.x + rad + 8, ret.y);
    ctx.moveTo(ret.x, ret.y - rad - 8);
    ctx.lineTo(ret.x, ret.y - rad * 0.4);
    ctx.moveTo(ret.x, ret.y + rad * 0.4);
    ctx.lineTo(ret.x, ret.y + rad + 8);
    ctx.stroke();
  };
  api.body.append(canvas, readout, meter);
  api.solve = () => {
    lock = 1;
    api.win();
  };
}
