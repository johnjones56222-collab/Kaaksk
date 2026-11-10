// Procedural textures. Everything the ship is painted with is drawn here on
// 2D canvases at load time: colour, a height field (turned into a normal map)
// and a roughness/metalness map. No image files are shipped.
import * as THREE from 'three';
import { rng, clamp } from '../core/utils.js';

export function makeCanvas(w, h = w) {
  const c = document.createElement('canvas');
  c.width = w;
  c.height = h;
  return c;
}

const rgb = (r, g, b, a = 1) => `rgba(${r | 0},${g | 0},${b | 0},${a})`;
const gray = (v, a = 1) => rgb(v, v, v, a);

// Three aligned canvases that are painted together.
class Layers {
  constructor(size) {
    this.size = size;
    this.c = makeCanvas(size);
    this.h = makeCanvas(size);
    this.m = makeCanvas(size);
    this.cc = this.c.getContext('2d');
    this.hc = this.h.getContext('2d');
    this.mc = this.m.getContext('2d');
  }
  // rough 0..1, metal 0..1 are stored in the G and B channels.
  fill(color, height, rough, metal) {
    const s = this.size;
    this.cc.fillStyle = color;
    this.cc.fillRect(0, 0, s, s);
    this.hc.fillStyle = gray(height);
    this.hc.fillRect(0, 0, s, s);
    this.mc.fillStyle = rgb(0, rough * 255, metal * 255);
    this.mc.fillRect(0, 0, s, s);
  }
  rect(x, y, w, h, color, height, rough, metal) {
    if (color) {
      this.cc.fillStyle = color;
      this.cc.fillRect(x, y, w, h);
    }
    if (height != null) {
      this.hc.fillStyle = gray(height);
      this.hc.fillRect(x, y, w, h);
    }
    if (rough != null) {
      this.mc.fillStyle = rgb(0, rough * 255, (metal ?? 0.5) * 255);
      this.mc.fillRect(x, y, w, h);
    }
  }
  circle(x, y, r, color, height, rough, metal) {
    for (const [ctx, style] of [
      [this.cc, color],
      [this.hc, height != null ? gray(height) : null],
      [this.mc, rough != null ? rgb(0, rough * 255, (metal ?? 0.5) * 255) : null],
    ]) {
      if (!style) continue;
      ctx.fillStyle = style;
      ctx.beginPath();
      ctx.arc(x, y, r, 0, Math.PI * 2);
      ctx.fill();
    }
  }
  line(x0, y0, x1, y1, w, color, height) {
    for (const [ctx, style] of [
      [this.cc, color],
      [this.hc, height != null ? gray(height) : null],
    ]) {
      if (!style) continue;
      ctx.strokeStyle = style;
      ctx.lineWidth = w;
      ctx.beginPath();
      ctx.moveTo(x0, y0);
      ctx.lineTo(x1, y1);
      ctx.stroke();
    }
  }
}

// Adds fine per-pixel grain so flat fills do not look synthetic.
function grain(ctx, size, amount, rand) {
  const img = ctx.getImageData(0, 0, size, size);
  const d = img.data;
  for (let i = 0; i < d.length; i += 4) {
    const n = (rand() - 0.5) * amount;
    d[i] = clamp(d[i] + n, 0, 255);
    d[i + 1] = clamp(d[i + 1] + n, 0, 255);
    d[i + 2] = clamp(d[i + 2] + n, 0, 255);
  }
  ctx.putImageData(img, 0, 0);
}

// Height canvas -> tangent-space normal map canvas (wrapping at the edges).
export function heightToNormal(heightCanvas, strength = 2) {
  const s = heightCanvas.width;
  const src = heightCanvas.getContext('2d').getImageData(0, 0, s, s).data;
  const out = makeCanvas(s);
  const octx = out.getContext('2d');
  const img = octx.createImageData(s, s);
  const d = img.data;
  const H = (x, y) => src[(((y + s) % s) * s + ((x + s) % s)) * 4];
  for (let y = 0; y < s; y++) {
    for (let x = 0; x < s; x++) {
      const dx = (H(x + 1, y) - H(x - 1, y)) / 255;
      const dy = (H(x, y + 1) - H(x, y - 1)) / 255;
      let nx = -dx * strength;
      let ny = dy * strength;
      let nz = 1;
      const l = Math.hypot(nx, ny, nz);
      nx /= l;
      ny /= l;
      nz /= l;
      const i = (y * s + x) * 4;
      d[i] = (nx * 0.5 + 0.5) * 255;
      d[i + 1] = (ny * 0.5 + 0.5) * 255;
      d[i + 2] = (nz * 0.5 + 0.5) * 255;
      d[i + 3] = 255;
    }
  }
  octx.putImageData(img, 0, 0);
  return out;
}

let maxAniso = 4;
export function setAnisotropy(v) {
  maxAniso = v;
}

export function canvasTexture(canvas, { srgb = false, repeat = true, mips = true } = {}) {
  const t = new THREE.CanvasTexture(canvas);
  t.colorSpace = srgb ? THREE.SRGBColorSpace : THREE.NoColorSpace;
  if (repeat) t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.anisotropy = maxAniso;
  t.generateMipmaps = mips;
  t.minFilter = mips ? THREE.LinearMipmapLinearFilter : THREE.LinearFilter;
  t.needsUpdate = true;
  return t;
}

function finish(L, { normal = 2, grainAmt = 10, seed = 1 } = {}) {
  const rand = rng(seed * 977 + 13);
  grain(L.cc, L.size, grainAmt, rand);
  grain(L.mc, L.size, grainAmt * 0.8, rand);
  return {
    map: canvasTexture(L.c, { srgb: true }),
    normalMap: canvasTexture(heightToNormal(L.h, normal)),
    rmMap: canvasTexture(L.m),
  };
}

// Recursively split a rectangle into panel rects.
function splitPanels(rand, x, y, w, h, depth, out, minSize) {
  const canSplit = depth > 0 && (w > minSize * 2 || h > minSize * 2);
  if (!canSplit || rand() < 0.22) {
    out.push([x, y, w, h]);
    return;
  }
  const horizontal = w > h ? rand() < 0.8 : rand() < 0.2;
  if (horizontal && w > minSize * 2) {
    const f = rand.pick([0.5, 0.5, 0.333, 0.667, 0.25, 0.75]);
    const a = Math.round((w * f) / 8) * 8;
    splitPanels(rand, x, y, a, h, depth - 1, out, minSize);
    splitPanels(rand, x + a, y, w - a, h, depth - 1, out, minSize);
  } else if (h > minSize * 2) {
    const f = rand.pick([0.5, 0.5, 0.333, 0.667]);
    const a = Math.round((h * f) / 8) * 8;
    splitPanels(rand, x, y, w, a, depth - 1, out, minSize);
    splitPanels(rand, x, y + a, w, h - a, depth - 1, out, minSize);
  } else {
    out.push([x, y, w, h]);
  }
}

// General sci-fi plating: seams, inset sub-panels, vents, bolts, scratches, grime.
export function panelTexture(opts = {}) {
  const {
    size = 1024,
    seed = 1,
    base = [120, 128, 138],
    vary = 14,
    depth = 3,
    cells = 2,
    seam = 3,
    rough = 0.55,
    metal = 0.55,
    bolts = 0.7,
    vents = 0.12,
    insets = 0.3,
    scratches = 60,
    grime = 0.5,
    streaks = 0.5,
    accent = null,
    normal = 2.2,
  } = opts;
  const k = size / 1024;
  const rand = rng(seed);
  const L = new Layers(size);
  L.fill(rgb(base[0] * 0.35, base[1] * 0.35, base[2] * 0.35), 70, 0.8, 0.3);
  const rects = [];
  const cell = size / cells;
  for (let cy = 0; cy < cells; cy++) {
    for (let cx = 0; cx < cells; cx++) splitPanels(rand, cx * cell, cy * cell, cell, cell, depth, rects, 96 * k);
  }
  const sm = Math.max(1, seam * k);
  for (const [x, y, w, h] of rects) {
    const v = (rand() - 0.5) * 2 * vary;
    const col = [base[0] + v, base[1] + v, base[2] + v];
    const r = clamp(rough + (rand() - 0.5) * 0.18, 0.1, 1);
    L.rect(x + sm, y + sm, w - sm * 2, h - sm * 2, rgb(...col), 200, r, metal);
    // bevel highlight and shadow lines
    L.cc.fillStyle = 'rgba(255,255,255,0.10)';
    L.cc.fillRect(x + sm, y + sm, w - sm * 2, Math.max(1, 2 * k));
    L.cc.fillRect(x + sm, y + sm, Math.max(1, 2 * k), h - sm * 2);
    L.cc.fillStyle = 'rgba(0,0,0,0.22)';
    L.cc.fillRect(x + sm, y + h - sm - Math.max(1, 2 * k), w - sm * 2, Math.max(1, 2 * k));
    L.cc.fillRect(x + w - sm - Math.max(1, 2 * k), y + sm, Math.max(1, 2 * k), h - sm * 2);
    const big = w > 150 * k && h > 150 * k;
    if (big && rand() < insets) {
      const m = (18 + rand() * 20) * k;
      const d = (rand() - 0.5) * 16;
      L.rect(x + m, y + m, w - m * 2, h - m * 2, rgb(col[0] - 12, col[1] - 12, col[2] - 12), 150, r + 0.1, metal);
      L.rect(x + m + 3 * k, y + m + 3 * k, w - m * 2 - 6 * k, h - m * 2 - 6 * k, rgb(col[0] + d, col[1] + d, col[2] + d), 178, r, metal);
    } else if (w > 110 * k && h > 70 * k && rand() < vents) {
      // louvre vent
      const m = 16 * k;
      const n = Math.max(3, Math.floor((h - m * 2) / (14 * k)));
      const sh = (h - m * 2) / n;
      for (let i = 0; i < n; i++) {
        L.rect(x + m, y + m + i * sh, w - m * 2, sh * 0.55, rgb(14, 16, 20), 40, 0.9, 0.2);
        L.rect(x + m, y + m + i * sh + sh * 0.55, w - m * 2, sh * 0.2, rgb(col[0] + 14, col[1] + 14, col[2] + 14), 230, r, metal);
      }
    }
    if (rand() < bolts && w > 60 * k && h > 60 * k) {
      const o = 11 * k;
      for (const [bx, by] of [
        [x + o, y + o],
        [x + w - o, y + o],
        [x + o, y + h - o],
        [x + w - o, y + h - o],
      ]) {
        L.circle(bx, by, 4.2 * k, rgb(col[0] - 40, col[1] - 40, col[2] - 40), 120, 0.7, metal);
        L.circle(bx - 0.5 * k, by - 0.5 * k, 2.6 * k, rgb(col[0] + 22, col[1] + 22, col[2] + 22), 245, 0.35, 0.9);
      }
    }
    if (accent && rand() < accent.chance && w > 80 * k) {
      const ah = (accent.h || 10) * k;
      L.rect(x + sm + 6 * k, y + h - sm - ah - 6 * k, w - sm * 2 - 12 * k, ah, accent.color, 205, 0.5, 0.2);
    }
  }
  // scratches
  for (let i = 0; i < scratches; i++) {
    const x = rand() * size;
    const y = rand() * size;
    const len = (20 + rand() * 120) * k;
    const a = rand() * Math.PI;
    L.cc.strokeStyle = `rgba(255,255,255,${0.03 + rand() * 0.07})`;
    L.cc.lineWidth = Math.max(0.6, k);
    L.cc.beginPath();
    L.cc.moveTo(x, y);
    L.cc.lineTo(x + Math.cos(a) * len, y + Math.sin(a) * len);
    L.cc.stroke();
    L.mc.strokeStyle = `rgba(0,255,128,${0.08 + rand() * 0.1})`;
    L.mc.lineWidth = Math.max(0.6, k);
    L.mc.beginPath();
    L.mc.moveTo(x, y);
    L.mc.lineTo(x + Math.cos(a) * len, y + Math.sin(a) * len);
    L.mc.stroke();
  }
  // grime blotches and vertical streaks
  const blot = Math.floor(26 * grime);
  for (let i = 0; i < blot; i++) {
    const x = rand() * size;
    const y = rand() * size;
    const r = (40 + rand() * 140) * k;
    for (const ox of [-size, 0, size]) {
      for (const oy of [-size, 0, size]) {
        if (x + ox + r < 0 || x + ox - r > size || y + oy + r < 0 || y + oy - r > size) continue;
        const g = L.cc.createRadialGradient(x + ox, y + oy, 0, x + ox, y + oy, r);
        g.addColorStop(0, `rgba(8,10,14,${0.1 + rand() * 0.12})`);
        g.addColorStop(1, 'rgba(8,10,14,0)');
        L.cc.fillStyle = g;
        L.cc.fillRect(x + ox - r, y + oy - r, r * 2, r * 2);
      }
    }
  }
  const st = Math.floor(30 * streaks);
  for (let i = 0; i < st; i++) {
    const x = rand() * size;
    const y = rand() * size;
    const len = (60 + rand() * 220) * k;
    const w = (2 + rand() * 7) * k;
    const g = L.cc.createLinearGradient(0, y, 0, y + len);
    g.addColorStop(0, `rgba(10,10,12,${0.1 + rand() * 0.14})`);
    g.addColorStop(1, 'rgba(10,10,12,0)');
    L.cc.fillStyle = g;
    L.cc.fillRect(x, y, w, len);
  }
  return finish(L, { normal, seed });
}

// Floor of square tiles with anti-slip studs.
export function floorTexture({ size = 1024, seed = 7, base = [58, 62, 70], tiles = 4, studs = true, rough = 0.62, metal = 0.35 } = {}) {
  const k = size / 1024;
  const rand = rng(seed);
  const L = new Layers(size);
  L.fill(rgb(12, 13, 16), 50, 0.9, 0.2);
  const ts = size / tiles;
  for (let ty = 0; ty < tiles; ty++) {
    for (let tx = 0; tx < tiles; tx++) {
      const v = (rand() - 0.5) * 12;
      const col = [base[0] + v, base[1] + v, base[2] + v];
      const g = 3 * k;
      const x = tx * ts + g;
      const y = ty * ts + g;
      const w = ts - g * 2;
      L.rect(x, y, w, w, rgb(...col), 190, rough + (rand() - 0.5) * 0.12, metal);
      L.cc.fillStyle = 'rgba(255,255,255,0.07)';
      L.cc.fillRect(x, y, w, 2 * k);
      L.cc.fillStyle = 'rgba(0,0,0,0.25)';
      L.cc.fillRect(x, y + w - 2 * k, w, 2 * k);
      if (studs) {
        const n = 6;
        const sp = w / n;
        for (let j = 0; j < n; j++) {
          for (let i = 0; i < n; i++) {
            const cx = x + sp * (i + 0.5);
            const cy = y + sp * (j + 0.5);
            L.circle(cx, cy, sp * 0.2, rgb(col[0] + 9, col[1] + 9, col[2] + 9), 235, rough - 0.12, metal);
          }
        }
      } else {
        // grooves
        const n = 8;
        for (let i = 1; i < n; i++) {
          L.rect(x + 8 * k, y + (w / n) * i, w - 16 * k, 3 * k, rgb(col[0] - 22, col[1] - 22, col[2] - 22), 120, 0.8, metal);
        }
      }
    }
  }
  // scuffs and wear
  for (let i = 0; i < 90; i++) {
    const x = rand() * size;
    const y = rand() * size;
    const len = (30 + rand() * 160) * k;
    const a = rand() * Math.PI;
    L.cc.strokeStyle = `rgba(${rand() < 0.5 ? '255,255,255' : '0,0,0'},${0.03 + rand() * 0.06})`;
    L.cc.lineWidth = (1 + rand() * 3) * k;
    L.cc.beginPath();
    L.cc.moveTo(x, y);
    L.cc.lineTo(x + Math.cos(a) * len, y + Math.sin(a) * len);
    L.cc.stroke();
  }
  return finish(L, { normal: 2.4, seed });
}

// Raised diamond tread plate for industrial decks.
export function treadTexture({ size = 512, seed = 3, base = [92, 96, 102] } = {}) {
  const rand = rng(seed);
  const L = new Layers(size);
  L.fill(rgb(...base), 110, 0.5, 0.85);
  const n = 16;
  const sp = size / n;
  for (let j = 0; j < n; j++) {
    for (let i = 0; i < n; i++) {
      const cx = (i + 0.5) * sp;
      const cy = (j + 0.5) * sp;
      const rot = (i + j) % 2 === 0 ? Math.PI / 4 : -Math.PI / 4;
      for (const [ctx, style] of [
        [L.cc, rgb(base[0] + 26, base[1] + 26, base[2] + 26)],
        [L.hc, gray(235)],
        [L.mc, rgb(0, 0.32 * 255, 0.9 * 255)],
      ]) {
        ctx.save();
        ctx.translate(cx, cy);
        ctx.rotate(rot);
        ctx.fillStyle = style;
        ctx.beginPath();
        ctx.ellipse(0, 0, sp * 0.4, sp * 0.11, 0, 0, Math.PI * 2);
        ctx.fill();
        ctx.restore();
      }
    }
  }
  for (let i = 0; i < 26; i++) {
    const x = rand() * size;
    const y = rand() * size;
    const r = 30 + rand() * 90;
    const g = L.cc.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, `rgba(10,10,12,${0.12 + rand() * 0.14})`);
    g.addColorStop(1, 'rgba(10,10,12,0)');
    L.cc.fillStyle = g;
    L.cc.fillRect(x - r, y - r, r * 2, r * 2);
  }
  return finish(L, { normal: 2.6, seed });
}

// Deck grating: bars over a dark void.
export function grateTexture({ size = 512, seed = 5 } = {}) {
  const L = new Layers(size);
  L.fill(rgb(6, 7, 9), 10, 0.95, 0.1);
  const n = 16;
  const sp = size / n;
  for (let i = 0; i < n; i++) {
    L.rect(i * sp + sp * 0.36, 0, sp * 0.28, size, rgb(70, 74, 80), 200, 0.45, 0.85);
  }
  for (let j = 0; j < n / 4; j++) {
    L.rect(0, j * sp * 4 + sp * 0.3, size, sp * 0.5, rgb(84, 88, 95), 240, 0.4, 0.85);
  }
  // frame
  const f = sp * 0.5;
  L.rect(0, 0, size, f, rgb(96, 100, 108), 250, 0.4, 0.85);
  L.rect(0, size - f, size, f, rgb(96, 100, 108), 250, 0.4, 0.85);
  L.rect(0, 0, f, size, rgb(96, 100, 108), 250, 0.4, 0.85);
  L.rect(size - f, 0, f, size, rgb(96, 100, 108), 250, 0.4, 0.85);
  return finish(L, { normal: 3, seed });
}

// Diagonal hazard stripes with wear.
export function hazardTexture({ size = 256, seed = 9 } = {}) {
  const rand = rng(seed);
  const L = new Layers(size);
  L.fill(rgb(226, 170, 30), 180, 0.6, 0.2);
  const n = 4;
  const sp = size / n;
  L.cc.fillStyle = rgb(18, 18, 20);
  for (let i = -n; i < n * 2; i++) {
    L.cc.beginPath();
    L.cc.moveTo(i * sp, 0);
    L.cc.lineTo(i * sp + sp * 0.5, 0);
    L.cc.lineTo(i * sp + sp * 0.5 + size, size);
    L.cc.lineTo(i * sp + size, size);
    L.cc.closePath();
    L.cc.fill();
  }
  for (let i = 0; i < 120; i++) {
    L.cc.fillStyle = `rgba(40,36,30,${rand() * 0.25})`;
    L.cc.fillRect(rand() * size, rand() * size, 2 + rand() * 14, 1 + rand() * 4);
  }
  return finish(L, { normal: 0.6, seed });
}

// Cargo crate face, drawn to fit a whole face (0..1 UVs).
export function crateTexture({ size = 512, seed = 11 } = {}) {
  const rand = rng(seed);
  const L = new Layers(size);
  const base = [176, 178, 182];
  L.fill(rgb(...base), 150, 0.6, 0.4);
  const f = size * 0.085;
  // recessed centre with ribs
  L.rect(f, f, size - f * 2, size - f * 2, rgb(150, 152, 157), 105, 0.65, 0.4);
  const ribs = 7;
  const rw = (size - f * 2) / ribs;
  for (let i = 0; i < ribs; i++) {
    L.rect(f + i * rw + rw * 0.25, f + 6, rw * 0.5, size - f * 2 - 12, rgb(168, 170, 175), 165, 0.55, 0.4);
  }
  // label plate
  L.rect(size * 0.3, size * 0.4, size * 0.4, size * 0.2, rgb(205, 207, 210), 175, 0.5, 0.3);
  L.cc.fillStyle = rgb(30, 32, 36);
  for (let i = 0; i < 4; i++) L.cc.fillRect(size * 0.33, size * (0.43 + i * 0.04), size * (0.14 + rand() * 0.18), size * 0.014);
  // barcode
  for (let i = 0; i < 16; i++) L.cc.fillRect(size * 0.56 + i * 4, size * 0.52, rand() < 0.5 ? 1 : 2.5, size * 0.06);
  // corner brackets
  const c = f * 1.5;
  for (const [x, y] of [
    [0, 0],
    [size - c, 0],
    [0, size - c],
    [size - c, size - c],
  ]) {
    L.rect(x, y, c, c, rgb(72, 75, 82), 235, 0.4, 0.8);
    L.circle(x + c / 2, y + c / 2, c * 0.16, rgb(140, 144, 150), 255, 0.3, 0.9);
  }
  for (let i = 0; i < 60; i++) {
    const x = rand() * size;
    const y = rand() * size;
    L.cc.strokeStyle = `rgba(0,0,0,${0.05 + rand() * 0.1})`;
    L.cc.lineWidth = 1 + rand() * 2;
    L.cc.beginPath();
    L.cc.moveTo(x, y);
    L.cc.lineTo(x + (rand() - 0.5) * 80, y + (rand() - 0.5) * 80);
    L.cc.stroke();
  }
  return finish(L, { normal: 2.4, seed });
}

// Brushed metal with fine horizontal streaks.
export function brushedTexture({ size = 256, seed = 21, base = [150, 154, 160] } = {}) {
  const rand = rng(seed);
  const L = new Layers(size);
  L.fill(rgb(...base), 128, 0.38, 0.9);
  for (let y = 0; y < size; y++) {
    const v = (rand() - 0.5) * 26;
    L.cc.fillStyle = `rgba(${v > 0 ? '255,255,255' : '0,0,0'},${Math.abs(v) / 255})`;
    L.cc.fillRect(0, y, size, 1);
    L.hc.fillStyle = gray(128 + v * 0.6);
    L.hc.fillRect(0, y, size, 1);
  }
  return finish(L, { normal: 0.5, grainAmt: 5, seed });
}

// Woven fabric for seats and bunks.
export function fabricTexture({ size = 256, seed = 31 } = {}) {
  const L = new Layers(size);
  L.fill(rgb(170, 172, 176), 128, 0.95, 0);
  const n = 64;
  const sp = size / n;
  for (let j = 0; j < n; j++) {
    for (let i = 0; i < n; i++) {
      const up = (i + j) % 2 === 0;
      L.rect(i * sp, j * sp, sp, sp, up ? rgb(182, 184, 188) : rgb(156, 158, 162), up ? 170 : 90, 0.95, 0);
    }
  }
  return finish(L, { normal: 1.2, grainAmt: 14, seed });
}

// Exterior hull plating: large pale plates, panel lines, soot and markings.
export function hullTexture({ size = 1024, seed = 41 } = {}) {
  return panelTexture({
    size,
    seed,
    base: [168, 172, 178],
    vary: 16,
    depth: 4,
    cells: 2,
    seam: 2.5,
    rough: 0.6,
    metal: 0.45,
    bolts: 0.35,
    vents: 0.06,
    insets: 0.22,
    scratches: 90,
    grime: 0.9,
    streaks: 1,
    normal: 2.6,
  });
}

// Leaves on a transparent ground for the hydroponics garden.
export function leafTexture({ size = 256, seed = 51, hue = 110 } = {}) {
  const rand = rng(seed);
  const c = makeCanvas(size);
  const ctx = c.getContext('2d');
  const leaf = (x, y, len, w, ang, light) => {
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate(ang);
    const g = ctx.createLinearGradient(0, 0, 0, -len);
    g.addColorStop(0, `hsl(${hue + rand() * 20 - 10},${50 + rand() * 20}%,${light * 0.7}%)`);
    g.addColorStop(1, `hsl(${hue + rand() * 30 - 5},${55 + rand() * 25}%,${light}%)`);
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.bezierCurveTo(w, -len * 0.3, w * 0.8, -len * 0.8, 0, -len);
    ctx.bezierCurveTo(-w * 0.8, -len * 0.8, -w, -len * 0.3, 0, 0);
    ctx.fill();
    ctx.strokeStyle = 'rgba(220,255,200,0.28)';
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(0, -len * 0.95);
    ctx.stroke();
    ctx.restore();
  };
  const cx = size / 2;
  // stem
  ctx.strokeStyle = `hsl(${hue - 10},45%,24%)`;
  ctx.lineWidth = size * 0.025;
  ctx.beginPath();
  ctx.moveTo(cx, size);
  ctx.quadraticCurveTo(cx + 6, size * 0.6, cx, size * 0.2);
  ctx.stroke();
  for (let i = 0; i < 16; i++) {
    const t = i / 15;
    const y = size * (0.95 - t * 0.75);
    const side = i % 2 === 0 ? 1 : -1;
    const len = size * (0.36 - t * 0.14) * (0.8 + rand() * 0.4);
    leaf(cx, y, len, len * 0.3, side * (0.7 + rand() * 0.5) - 0.05, 26 + rand() * 20);
  }
  leaf(cx, size * 0.22, size * 0.22, size * 0.07, 0, 44);
  const t = canvasTexture(c, { srgb: true, repeat: false });
  return t;
}

// Soft particle sprites: 0 = round glow, 1 = smoke puff, 2 = spark streak, 3 = flame lick.
export function particleAtlas() {
  const cell = 64;
  const c = makeCanvas(cell * 4, cell);
  const ctx = c.getContext('2d');
  const rand = rng(77);
  // glow
  let g = ctx.createRadialGradient(32, 32, 0, 32, 32, 30);
  g.addColorStop(0, 'rgba(255,255,255,1)');
  g.addColorStop(0.25, 'rgba(255,255,255,0.55)');
  g.addColorStop(1, 'rgba(255,255,255,0)');
  ctx.fillStyle = g;
  ctx.fillRect(0, 0, 64, 64);
  // smoke
  for (let i = 0; i < 22; i++) {
    const x = 64 + 32 + (rand() - 0.5) * 30;
    const y = 32 + (rand() - 0.5) * 30;
    const r = 8 + rand() * 14;
    g = ctx.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, 'rgba(255,255,255,0.20)');
    g.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.fillStyle = g;
    ctx.fillRect(x - r, y - r, r * 2, r * 2);
  }
  // spark
  g = ctx.createRadialGradient(128 + 32, 32, 0, 128 + 32, 32, 28);
  g.addColorStop(0, 'rgba(255,255,255,1)');
  g.addColorStop(0.12, 'rgba(255,255,255,0.9)');
  g.addColorStop(0.3, 'rgba(255,255,255,0.15)');
  g.addColorStop(1, 'rgba(255,255,255,0)');
  ctx.fillStyle = g;
  ctx.fillRect(128, 0, 64, 64);
  // flame
  ctx.save();
  ctx.beginPath();
  ctx.rect(192, 0, 64, 64);
  ctx.clip();
  for (let i = 0; i < 14; i++) {
    const x = 192 + 32 + (rand() - 0.5) * 18;
    const y = 40 + (rand() - 0.5) * 26;
    const r = 9 + rand() * 13;
    g = ctx.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, 'rgba(255,255,255,0.42)');
    g.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.fillStyle = g;
    ctx.fillRect(x - r, y - r, r * 2, r * 2);
  }
  ctx.restore();
  const t = canvasTexture(c, { srgb: false, repeat: false });
  return t;
}

// Frost crystals used on cryo glass and for the wake-up overlay.
export function frostTexture({ size = 256, seed = 61 } = {}) {
  const rand = rng(seed);
  const c = makeCanvas(size);
  const ctx = c.getContext('2d');
  ctx.fillStyle = 'rgba(190,225,245,0.34)';
  ctx.fillRect(0, 0, size, size);
  for (let i = 0; i < 240; i++) {
    const x = rand() * size;
    const y = rand() * size;
    const arms = 5 + Math.floor(rand() * 3);
    const len = 4 + rand() * 20;
    ctx.strokeStyle = `rgba(235,248,255,${0.12 + rand() * 0.4})`;
    ctx.lineWidth = 0.6 + rand() * 0.8;
    const a0 = rand() * Math.PI;
    for (let a = 0; a < arms; a++) {
      const ang = a0 + (a / arms) * Math.PI * 2;
      ctx.beginPath();
      ctx.moveTo(x, y);
      ctx.lineTo(x + Math.cos(ang) * len, y + Math.sin(ang) * len);
      ctx.stroke();
    }
  }
  for (let i = 0; i < 40; i++) {
    const x = rand() * size;
    const y = rand() * size;
    const r = 14 + rand() * 46;
    const g = ctx.createRadialGradient(x, y, 0, x, y, r);
    g.addColorStop(0, `rgba(240,250,255,${0.1 + rand() * 0.22})`);
    g.addColorStop(1, 'rgba(240,250,255,0)');
    ctx.fillStyle = g;
    ctx.fillRect(x - r, y - r, r * 2, r * 2);
  }
  return canvasTexture(c, { srgb: true });
}

// Banded alpha strip for the planet's rings (sampled radially).
export function ringTexture() {
  const w = 1024;
  const c = makeCanvas(w, 8);
  const ctx = c.getContext('2d');
  const rand = rng(303);
  for (let x = 0; x < w; x++) {
    const t = x / w;
    let a = 0.55 + 0.35 * Math.sin(t * 46) * Math.sin(t * 9.3 + 1.7);
    a *= 0.6 + 0.4 * rand();
    if (t > 0.42 && t < 0.47) a *= 0.08; // main gap
    if (t > 0.72 && t < 0.735) a *= 0.2;
    a *= Math.min(1, t * 14) * Math.min(1, (1 - t) * 9);
    const warm = 200 + 40 * Math.sin(t * 13);
    ctx.fillStyle = `rgba(${warm | 0},${(warm * 0.9) | 0},${(warm * 0.78) | 0},${clamp(a, 0, 1)})`;
    ctx.fillRect(x, 0, 1, 8);
  }
  const t = canvasTexture(c, { srgb: true, repeat: false });
  return t;
}

// ---------------------------------------------------------------------------
// Sign atlas: all lettering in the ship (door plates, labels, warnings) is
// packed into one texture so every sign in a room draws in a single call.
// ---------------------------------------------------------------------------
export class SignAtlas {
  constructor(size = 2048) {
    this.size = size;
    this.canvas = makeCanvas(size);
    this.ctx = this.canvas.getContext('2d');
    this.x = 4;
    this.y = 4;
    this.rowH = 0;
    this.cache = new Map();
    this.texture = canvasTexture(this.canvas, { srgb: true, repeat: false });
    this.full = false;
  }

  _alloc(w, h) {
    if (this.x + w + 4 > this.size) {
      this.x = 4;
      this.y += this.rowH + 6;
      this.rowH = 0;
    }
    if (this.y + h + 4 > this.size) {
      this.full = true;
      return null;
    }
    const r = { x: this.x, y: this.y, w, h };
    this.x += w + 6;
    this.rowH = Math.max(this.rowH, h);
    return r;
  }

  // Returns { u0, v0, u1, v1, aspect } for a piece of lettering.
  text(str, o = {}) {
    const {
      px = 64,
      color = '#ffffff',
      bg = null,
      border = null,
      weight = 700,
      spacing = 0.12,
      pad = 0.35,
      icon = null,
      font = '"Segoe UI","Helvetica Neue",Roboto,Arial,sans-serif',
    } = o;
    const key = JSON.stringify([str, px, color, bg, border, weight, spacing, icon]);
    if (this.cache.has(key)) return this.cache.get(key);
    const ctx = this.ctx;
    ctx.font = `${weight} ${px}px ${font}`;
    const chars = [...str];
    const sp = px * spacing;
    let tw = 0;
    for (const ch of chars) tw += ctx.measureText(ch).width + sp;
    tw -= sp;
    const padPx = px * pad;
    const iconW = icon ? px * 1.15 : 0;
    const w = Math.ceil(tw + padPx * 2 + iconW);
    const h = Math.ceil(px * 1.5);
    const r = this._alloc(w, h);
    if (!r) {
      const fallback = { u0: 0, v0: 0, u1: 0.001, v1: 0.001, aspect: w / h };
      return fallback;
    }
    if (bg) {
      ctx.fillStyle = bg;
      ctx.fillRect(r.x, r.y, w, h);
    }
    if (border) {
      ctx.strokeStyle = border;
      ctx.lineWidth = Math.max(2, px * 0.07);
      ctx.strokeRect(r.x + ctx.lineWidth / 2, r.y + ctx.lineWidth / 2, w - ctx.lineWidth, h - ctx.lineWidth);
    }
    ctx.fillStyle = color;
    ctx.textBaseline = 'middle';
    ctx.textAlign = 'left';
    let cx = r.x + padPx;
    const cy = r.y + h / 2 + px * 0.04;
    if (icon) {
      this._icon(icon, cx + px * 0.45, r.y + h / 2, px * 0.42, color);
      cx += iconW;
    }
    for (const ch of chars) {
      ctx.fillText(ch, cx, cy);
      cx += ctx.measureText(ch).width + sp;
    }
    const s = this.size;
    const out = { u0: r.x / s, v0: 1 - (r.y + h) / s, u1: (r.x + w) / s, v1: 1 - r.y / s, aspect: w / h };
    this.cache.set(key, out);
    this.texture.needsUpdate = true;
    return out;
  }

  _icon(kind, x, y, r, color) {
    const ctx = this.ctx;
    ctx.save();
    ctx.translate(x, y);
    ctx.strokeStyle = color;
    ctx.fillStyle = color;
    ctx.lineWidth = r * 0.22;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';
    ctx.beginPath();
    if (kind === 'left' || kind === 'right' || kind === 'up' || kind === 'down') {
      const rot = { right: 0, down: Math.PI / 2, left: Math.PI, up: -Math.PI / 2 }[kind];
      ctx.rotate(rot);
      ctx.moveTo(-r, 0);
      ctx.lineTo(r, 0);
      ctx.moveTo(r * 0.25, -r * 0.7);
      ctx.lineTo(r, 0);
      ctx.lineTo(r * 0.25, r * 0.7);
      ctx.stroke();
    } else if (kind === 'warn') {
      ctx.moveTo(0, -r);
      ctx.lineTo(r, r * 0.8);
      ctx.lineTo(-r, r * 0.8);
      ctx.closePath();
      ctx.stroke();
      ctx.fillRect(-r * 0.09, -r * 0.35, r * 0.18, r * 0.6);
      ctx.fillRect(-r * 0.09, r * 0.4, r * 0.18, r * 0.18);
    } else if (kind === 'bolt') {
      ctx.moveTo(r * 0.2, -r);
      ctx.lineTo(-r * 0.5, r * 0.15);
      ctx.lineTo(0, r * 0.15);
      ctx.lineTo(-r * 0.2, r);
      ctx.lineTo(r * 0.5, -r * 0.15);
      ctx.lineTo(0, -r * 0.15);
      ctx.closePath();
      ctx.fill();
    } else if (kind === 'cross') {
      ctx.fillRect(-r * 0.28, -r, r * 0.56, r * 2);
      ctx.fillRect(-r, -r * 0.28, r * 2, r * 0.56);
    } else if (kind === 'ring') {
      ctx.arc(0, 0, r * 0.8, 0, Math.PI * 2);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(0, 0, r * 0.25, 0, Math.PI * 2);
      ctx.fill();
    } else if (kind === 'leaf') {
      ctx.moveTo(0, r);
      ctx.bezierCurveTo(r * 1.1, r * 0.2, r * 0.7, -r, 0, -r);
      ctx.bezierCurveTo(-r * 0.7, -r, -r * 1.1, r * 0.2, 0, r);
      ctx.fill();
    } else if (kind === 'atom') {
      for (let i = 0; i < 3; i++) {
        ctx.beginPath();
        ctx.ellipse(0, 0, r, r * 0.38, (i * Math.PI) / 3, 0, Math.PI * 2);
        ctx.stroke();
      }
    } else {
      ctx.arc(0, 0, r * 0.6, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();
  }
}
