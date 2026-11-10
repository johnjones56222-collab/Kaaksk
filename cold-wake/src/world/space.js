// Everything outside the hull: nebula sky (rendered once into a cube map),
// stars, the ringed gas giant Aurelia, moons, the sun, drifting debris, the
// alien Lattice, and the jump-tunnel effect.
import * as THREE from 'three';
import { rng } from '../core/utils.js';
import { mergeVertices } from 'three/addons/utils/BufferGeometryUtils.js';
import { ringTexture, makeCanvas } from '../engine/textures.js';

export const SUN_DIR = new THREE.Vector3(0.72, 0.42, -0.55).normalize();
export const PLANET_DIR = new THREE.Vector3(-0.58, -0.1, -0.81).normalize();
const PLANET_DIST = 9000;
const PLANET_R = 3300;

const NOISE = `
float hash13(vec3 p) { p = fract(p * 0.3183099 + 0.1); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }
float vnoise(vec3 x) {
  vec3 i = floor(x); vec3 f = fract(x); f = f * f * (3.0 - 2.0 * f);
  return mix(mix(mix(hash13(i), hash13(i + vec3(1,0,0)), f.x), mix(hash13(i + vec3(0,1,0)), hash13(i + vec3(1,1,0)), f.x), f.y),
             mix(mix(hash13(i + vec3(0,0,1)), hash13(i + vec3(1,0,1)), f.x), mix(hash13(i + vec3(0,1,1)), hash13(i + vec3(1,1,1)), f.x), f.y), f.z);
}
float fbm(vec3 p) { float a = 0.5; float s = 0.0; for (int i = 0; i < 6; i++) { s += a * vnoise(p); p = p * 2.03 + vec3(1.7, 9.2, 3.1); a *= 0.5; } return s; }
`;

function buildSky(renderer, size) {
  const scene = new THREE.Scene();
  const mat = new THREE.ShaderMaterial({
    side: THREE.BackSide,
    vertexShader: `varying vec3 vDir; void main() { vDir = position; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
    fragmentShader: `
varying vec3 vDir;
${NOISE}
float starLayer(vec3 d, float scale, float thr, float size) {
  vec3 q = d * scale;
  vec3 id = floor(q);
  float acc = 0.0;
  for (int x = -1; x <= 1; x++) for (int y = -1; y <= 1; y++) for (int z = -1; z <= 1; z++) {
    vec3 c = id + vec3(float(x), float(y), float(z));
    float h = hash13(c);
    if (h > thr) {
      vec3 sp = c + 0.5 + 0.4 * (vec3(hash13(c + 7.1), hash13(c + 3.3), hash13(c + 5.7)) - 0.5);
      float dist = length(q - sp);
      float b = (h - thr) / (1.0 - thr);
      acc += smoothstep(size * (0.5 + b), 0.0, dist) * (0.4 + 2.6 * b * b);
    }
  }
  return acc;
}
void main() {
  vec3 d = normalize(vDir);
  // galactic band tilted across the sky
  vec3 bandN = normalize(vec3(0.35, 0.25, 0.9));
  float band = exp(-pow(dot(d, bandN) * 3.2, 2.0));
  float n1 = fbm(d * 2.4 + 3.0);
  float n2 = fbm(d * 5.5 - 7.0 + n1 * 1.6);
  float n3 = fbm(d * 11.0 + n2 * 2.0);
  float cloud = smoothstep(0.38, 0.85, n2 * 0.75 + n1 * 0.45 + band * 0.22);
  float dust = smoothstep(0.45, 0.75, n3) * band;
  vec3 deep = vec3(0.004, 0.007, 0.016);
  vec3 teal = vec3(0.02, 0.16, 0.22);
  vec3 violet = vec3(0.19, 0.05, 0.26);
  vec3 rose = vec3(0.34, 0.07, 0.12);
  vec3 col = deep;
  float patchy = smoothstep(0.42, 0.7, fbm(d * 1.3 + 11.0));
  col += teal * cloud * (0.35 + 0.8 * n1) * (0.12 + 0.5 * patchy);
  col += violet * smoothstep(0.55, 0.95, n1 * 0.9 + n3 * 0.35) * (0.1 + 0.45 * patchy);
  // emission knot near the stern-side sky
  float knot = smoothstep(0.55, 1.0, dot(d, normalize(vec3(0.35, 0.3, 0.88))));
  col += rose * knot * (0.25 + 1.2 * n2 * n2) * 0.6 + vec3(0.5, 0.25, 0.12) * pow(knot, 6.0) * n3 * 0.3;
  col += vec3(0.5, 0.56, 0.7) * band * 0.055 * (0.5 + n2);
  col *= 1.0 - dust * 0.75;
  // stars: three scales, denser in the band
  float s = starLayer(d, 46.0, 0.90 - band * 0.05, 0.11) * 1.0
          + starLayer(d, 95.0, 0.86 - band * 0.10, 0.12) * 0.55
          + starLayer(d, 190.0, 0.80 - band * 0.16, 0.16) * 0.28;
  float tint = hash13(floor(d * 46.0));
  vec3 starCol = mix(vec3(1.0, 0.82, 0.62), vec3(0.66, 0.82, 1.0), tint);
  col += starCol * s * (1.0 - dust * 0.6);
  gl_FragColor = vec4(col, 1.0);
}`,
  });
  const mesh = new THREE.Mesh(new THREE.SphereGeometry(10, 48, 32), mat);
  scene.add(mesh);
  const rt = new THREE.WebGLCubeRenderTarget(size, { type: THREE.HalfFloatType, generateMipmaps: false });
  const cam = new THREE.CubeCamera(0.1, 100, rt);
  cam.update(renderer, scene);
  mesh.geometry.dispose();
  mat.dispose();
  return rt.texture;
}

function glowSprite(inner, outer) {
  const c = makeCanvas(256);
  const ctx = c.getContext('2d');
  const g = ctx.createRadialGradient(128, 128, 0, 128, 128, 128);
  g.addColorStop(0, inner);
  g.addColorStop(0.08, inner);
  g.addColorStop(0.2, outer);
  g.addColorStop(1, 'rgba(0,0,0,0)');
  ctx.fillStyle = g;
  ctx.fillRect(0, 0, 256, 256);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

export function rockGeometry(seed, detail = 2) {
  let g = new THREE.IcosahedronGeometry(1, detail);
  g.deleteAttribute('uv');
  g.deleteAttribute('normal');
  g = mergeVertices(g, 1e-4);
  const r = rng(seed);
  const f = [1.3 + r(), 1.7 + r(), 2.1 + r()];
  const ph = [r() * 6, r() * 6, r() * 6];
  const squash = [0.8 + r() * 0.5, 0.6 + r() * 0.4, 0.8 + r() * 0.5];
  const pos = g.attributes.position;
  for (let i = 0; i < pos.count; i++) {
    const x = pos.getX(i);
    const y = pos.getY(i);
    const z = pos.getZ(i);
    const k =
      0.78 +
      0.2 * Math.sin(x * f[0] * 2 + ph[0]) * Math.cos(y * f[1] * 2 + ph[1]) +
      0.14 * Math.sin(z * f[2] * 3 + ph[2] + x * 2) +
      0.08 * Math.sin((x + y + z) * 7 + ph[0]);
    pos.setXYZ(i, x * k * squash[0], y * k * squash[1], z * k * squash[2]);
  }
  g.computeVertexNormals();
  return g;
}

export class Space {
  constructor(gfx) {
    const scene = gfx.space;
    this.scene = scene;
    this.gfx = gfx;
    const q = gfx.quality;
    scene.background = buildSky(gfx.renderer, q.cube);
    scene.backgroundIntensity = 1;

    const sun = new THREE.DirectionalLight(0xfff0dc, 3.2);
    sun.position.copy(SUN_DIR);
    scene.add(sun);
    scene.add(new THREE.AmbientLight(0x35405a, 0.5));

    // ---- twinkling foreground stars ----------------------------------------
    {
      const n = q.stars;
      const r = rng(4242);
      const pos = new Float32Array(n * 3);
      const col = new Float32Array(n * 3);
      const aux = new Float32Array(n * 2);
      for (let i = 0; i < n; i++) {
        const u = r() * 2 - 1;
        const a = r() * Math.PI * 2;
        const s = Math.sqrt(1 - u * u);
        const R0 = 20000;
        pos[i * 3] = Math.cos(a) * s * R0;
        pos[i * 3 + 1] = u * R0;
        pos[i * 3 + 2] = Math.sin(a) * s * R0;
        const warm = r();
        const b = 0.6 + Math.pow(r(), 5) * 5;
        col[i * 3] = (0.75 + 0.25 * warm) * b;
        col[i * 3 + 1] = 0.82 * b;
        col[i * 3 + 2] = (1 - 0.3 * warm) * b;
        aux[i * 2] = 1.2 + Math.pow(r(), 4) * 3.2;
        aux[i * 2 + 1] = r() * 6.28;
      }
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
      g.setAttribute('scol', new THREE.BufferAttribute(col, 3));
      g.setAttribute('saux', new THREE.BufferAttribute(aux, 2));
      this.starMat = new THREE.ShaderMaterial({
        uniforms: { uTime: { value: 0 }, uPx: { value: 1 }, uStretch: { value: 0 } },
        vertexShader: `
attribute vec3 scol; attribute vec2 saux; uniform float uTime; uniform float uPx; uniform float uStretch;
varying vec3 vCol;
void main() {
  float tw = 0.75 + 0.25 * sin(uTime * (1.5 + saux.y) + saux.y * 20.0);
  vCol = scol * tw * (1.0 + uStretch * 2.0);
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  gl_PointSize = saux.x * uPx * (1.0 + uStretch * 3.0);
}`,
        fragmentShader: `
varying vec3 vCol;
void main() {
  vec2 p = gl_PointCoord - 0.5;
  float d = length(p);
  float a = smoothstep(0.5, 0.0, d);
  gl_FragColor = vec4(vCol * a * a, 1.0);
}`,
        blending: THREE.AdditiveBlending,
        depthWrite: false,
        transparent: true,
      });
      this.stars = new THREE.Points(g, this.starMat);
      this.stars.frustumCulled = false;
      scene.add(this.stars);
    }

    // ---- Aurelia ------------------------------------------------------------
    const planetPos = PLANET_DIR.clone().multiplyScalar(PLANET_DIST);
    const axis = new THREE.Vector3(0.32, 0.93, 0.18).normalize();
    this.planetMat = new THREE.ShaderMaterial({
      uniforms: { uSun: { value: SUN_DIR.clone() }, uTime: { value: 0 }, uAxis: { value: axis } },
      vertexShader: `
varying vec3 vObj; varying vec3 vN; varying vec3 vW;
void main() {
  vObj = normalize(position);
  vN = normalize(mat3(modelMatrix) * normal);
  vec4 w = modelMatrix * vec4(position, 1.0);
  vW = w.xyz;
  gl_Position = projectionMatrix * viewMatrix * w;
}`,
      fragmentShader: `
uniform vec3 uSun; uniform float uTime; uniform vec3 uAxis;
varying vec3 vObj; varying vec3 vN; varying vec3 vW;
${NOISE}
vec3 palette(float t) {
  vec3 a = vec3(0.86, 0.66, 0.38);
  vec3 b = vec3(0.60, 0.33, 0.16);
  vec3 c = vec3(0.93, 0.83, 0.62);
  vec3 d = vec3(0.36, 0.22, 0.16);
  vec3 e = vec3(0.50, 0.58, 0.56);
  t = fract(t);
  if (t < 0.25) return mix(a, b, t / 0.25);
  if (t < 0.5) return mix(b, c, (t - 0.25) / 0.25);
  if (t < 0.75) return mix(c, d, (t - 0.5) / 0.25);
  return mix(d, a, (t - 0.75) / 0.25) * mix(vec3(1.0), e * 1.6, 0.25);
}
void main() {
  vec3 n = normalize(vN);
  vec3 o = normalize(vObj);
  float lat = o.y;
  float lon = atan(o.z, o.x);
  float drift = uTime * 0.004;
  vec3 p = vec3(cos(lon + drift * (1.0 + lat * 2.0)), lat * 3.0, sin(lon + drift * (1.0 + lat * 2.0)));
  float turb = fbm(p * vec3(2.0, 9.0, 2.0) + 4.0);
  float swirl = fbm(p * vec3(6.0, 22.0, 6.0) + turb * 2.0);
  float band = lat * 5.5 + (turb - 0.5) * 0.9 + (swirl - 0.5) * 0.22;
  vec3 col = palette(band * 0.5 + 0.5);
  col *= 0.8 + 0.4 * swirl;
  // great storm
  vec2 sp = vec2((mod(lon + drift * 0.7 + 3.14159, 6.28318) - 3.9) * 1.0, (lat + 0.33) * 3.4);
  float storm = exp(-dot(sp * vec2(2.6, 5.0), sp * vec2(2.6, 5.0)));
  col = mix(col, vec3(0.80, 0.34, 0.18) * (0.7 + 0.6 * swirl), storm * 0.85);
  // polar haze
  col = mix(col, vec3(0.36, 0.52, 0.56), smoothstep(0.72, 0.98, abs(lat)) * 0.75);
  float ndl = dot(n, normalize(uSun));
  float day = smoothstep(-0.05, 0.2, ndl);
  vec3 v = normalize(cameraPosition - vW);
  float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0);
  vec3 lit = col * (0.003 + 1.5 * day * (0.2 + 0.8 * max(ndl, 0.0)));
  lit += vec3(0.95, 0.62, 0.36) * rim * day * 0.55;
  lit += vec3(0.02, 0.04, 0.08) * rim * (1.0 - day) * 0.5;
  // faint lightning on the night side
  float bolt = step(0.985, vnoise(vec3(o.xz * 14.0, floor(uTime * 6.0)))) * (1.0 - day) * step(abs(lat), 0.6);
  lit += vec3(0.5, 0.6, 1.0) * bolt * 0.6;
  gl_FragColor = vec4(lit, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`,
    });
    this.planet = new THREE.Mesh(new THREE.SphereGeometry(PLANET_R, 96, 64), this.planetMat);
    this.planet.position.copy(planetPos);
    this.planet.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), axis);
    scene.add(this.planet);

    // atmosphere shell
    this.atmoMat = new THREE.ShaderMaterial({
      uniforms: { uSun: { value: SUN_DIR.clone() } },
      vertexShader: `
varying vec3 vN; varying vec3 vW;
void main() { vN = normalize(mat3(modelMatrix) * normal); vec4 w = modelMatrix * vec4(position, 1.0); vW = w.xyz; gl_Position = projectionMatrix * viewMatrix * w; }`,
      fragmentShader: `
uniform vec3 uSun; varying vec3 vN; varying vec3 vW;
void main() {
  vec3 n = normalize(vN);
  vec3 v = normalize(cameraPosition - vW);
  float f = pow(smoothstep(0.0, 0.3, -dot(n, v)), 1.5);
  float day = smoothstep(-0.3, 0.45, dot(n, normalize(uSun)));
  vec3 c = mix(vec3(0.10, 0.2, 0.42), vec3(1.0, 0.66, 0.36), day);
  gl_FragColor = vec4(c * f * (0.25 + 1.3 * day), f);
}`,
      side: THREE.BackSide,
      transparent: true,
      blending: THREE.AdditiveBlending,
      depthWrite: false,
    });
    const atmo = new THREE.Mesh(new THREE.SphereGeometry(PLANET_R * 1.045, 64, 48), this.atmoMat);
    atmo.position.copy(planetPos);
    scene.add(atmo);

    // rings with the planet's shadow cast across them
    const inner = PLANET_R * 1.35;
    const outer = PLANET_R * 2.35;
    this.ringMat = new THREE.ShaderMaterial({
      uniforms: {
        map: { value: ringTexture() },
        uSun: { value: SUN_DIR.clone() },
        uCenter: { value: planetPos.clone() },
        uInner: { value: inner },
        uOuter: { value: outer },
        uR: { value: PLANET_R },
      },
      vertexShader: `
varying vec3 vW; varying vec3 vL;
void main() { vL = position; vec4 w = modelMatrix * vec4(position, 1.0); vW = w.xyz; gl_Position = projectionMatrix * viewMatrix * w; }`,
      fragmentShader: `
uniform sampler2D map; uniform vec3 uSun; uniform vec3 uCenter; uniform float uInner; uniform float uOuter; uniform float uR;
varying vec3 vW; varying vec3 vL;
void main() {
  float r = length(vL.xy);
  float t = (r - uInner) / (uOuter - uInner);
  vec4 c = texture2D(map, vec2(t, 0.5));
  // ray from this ring particle towards the sun: does the planet block it?
  vec3 oc = vW - uCenter;
  vec3 s = normalize(uSun);
  float b = dot(oc, s);
  float disc = b * b - (dot(oc, oc) - uR * uR);
  float sh = b < 0.0 ? smoothstep(0.0, uR * uR * 0.06, disc) : 0.0;
  float shadow = mix(1.0, 0.05, sh);
  vec3 col = c.rgb * (0.25 + 1.15 * shadow);
  gl_FragColor = vec4(col * c.a, c.a * 0.92);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`,
      transparent: true,
      side: THREE.DoubleSide,
      depthWrite: false,
      blending: THREE.CustomBlending,
      blendSrc: THREE.OneFactor,
      blendDst: THREE.OneMinusSrcAlphaFactor,
    });
    const ring = new THREE.Mesh(new THREE.RingGeometry(inner, outer, 160, 1), this.ringMat);
    ring.position.copy(planetPos);
    ring.quaternion.setFromUnitVectors(new THREE.Vector3(0, 0, 1), axis);
    ring.renderOrder = 2;
    scene.add(ring);
    this.ring = ring;

    // ---- sun ----------------------------------------------------------------
    const sunTex = glowSprite('rgba(255,255,255,1)', 'rgba(255,214,150,0.55)');
    const core = new THREE.Sprite(new THREE.SpriteMaterial({ map: sunTex, color: new THREE.Color(14, 12, 9), blending: THREE.AdditiveBlending, depthWrite: false, depthTest: false }));
    core.position.copy(SUN_DIR).multiplyScalar(22000);
    core.scale.setScalar(3600);
    scene.add(core);
    const halo = new THREE.Sprite(new THREE.SpriteMaterial({ map: sunTex, color: new THREE.Color(0.9, 0.62, 0.36), blending: THREE.AdditiveBlending, depthWrite: false, depthTest: false, opacity: 0.5 }));
    halo.position.copy(core.position);
    halo.scale.setScalar(16000);
    scene.add(halo);

    // ---- moons --------------------------------------------------------------
    const moonMat = (c1, c2, scale) =>
      new THREE.ShaderMaterial({
        uniforms: { uSun: { value: SUN_DIR.clone() }, c1: { value: new THREE.Color(c1) }, c2: { value: new THREE.Color(c2) }, uScale: { value: scale } },
        vertexShader: `varying vec3 vO; varying vec3 vN; void main() { vO = normalize(position); vN = normalize(mat3(modelMatrix) * normal); gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
        fragmentShader: `
uniform vec3 uSun; uniform vec3 c1; uniform vec3 c2; uniform float uScale; varying vec3 vO; varying vec3 vN;
${NOISE}
void main() {
  float n = fbm(vO * uScale);
  float cr = smoothstep(0.55, 0.62, vnoise(vO * uScale * 2.6)) * 0.35;
  vec3 col = mix(c1, c2, smoothstep(0.3, 0.75, n)) * (1.0 - cr);
  float ndl = max(dot(normalize(vN), normalize(uSun)), 0.0);
  gl_FragColor = vec4(col * (0.015 + 1.3 * ndl), 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`,
      });
    const moonA = new THREE.Mesh(new THREE.SphereGeometry(520, 48, 32), moonMat(0x8a8782, 0x4a4844, 3.5));
    moonA.position.set(0.32, 0.2, 0.93).normalize().multiplyScalar(7600);
    scene.add(moonA);
    const moonB = new THREE.Mesh(new THREE.SphereGeometry(210, 32, 24), moonMat(0x9fb6c4, 0x3f5866, 5));
    moonB.position.set(0.25, 0.9, -0.36).normalize().multiplyScalar(6800);
    scene.add(moonB);
    this.moons = [moonA, moonB];

    // ---- ring debris drifting past the ship -----------------------------------
    {
      const count = q.name === 'low' ? 70 : 170;
      const geo = rockGeometry(7, 2);
      const mat = new THREE.MeshStandardMaterial({ color: 0x57504a, roughness: 0.95, metalness: 0.05 });
      const inst = new THREE.InstancedMesh(geo, mat, count);
      const r = rng(991);
      const m = new THREE.Matrix4();
      const qn = new THREE.Quaternion();
      const e = new THREE.Euler();
      this.rocks = [];
      for (let i = 0; i < count; i++) {
        // a loose sheet below and to port, following the ring plane
        const p = new THREE.Vector3(-300 - r() * 2600, -260 - r() * 520 + r() * r() * 300, -1900 + r() * 3800);
        const s = 4 + Math.pow(r(), 3) * 70;
        e.set(r() * 6.28, r() * 6.28, r() * 6.28);
        qn.setFromEuler(e);
        m.compose(p, qn, new THREE.Vector3(s, s * (0.6 + r() * 0.5), s));
        inst.setMatrixAt(i, m);
        this.rocks.push({ p, e: e.clone(), s, sy: s * (0.6 + r() * 0.5), spin: (r() - 0.5) * 0.12, drift: 2 + r() * 5 });
      }
      inst.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
      inst.frustumCulled = false;
      scene.add(inst);
      this.rockMesh = inst;
    }

    // ---- the Lattice: source of the signal ------------------------------------
    {
      const g = new THREE.Group();
      const dark = new THREE.MeshStandardMaterial({ color: 0x14121c, roughness: 0.35, metalness: 0.9 });
      this.latticeGlow = new THREE.MeshBasicMaterial({ color: new THREE.Color(1.6, 0.5, 3.4) });
      const rings = [];
      for (let i = 0; i < 3; i++) {
        const R0 = 120 - i * 26;
        const arc = Math.PI * (1.25 + i * 0.22);
        const ringG = new THREE.Group();
        ringG.add(new THREE.Mesh(new THREE.TorusGeometry(R0, 5.5 - i, 6, 56, arc), dark));
        const glowRing = new THREE.Mesh(new THREE.TorusGeometry(R0 - 6.5 + i, 2.2, 5, 56, arc), this.latticeGlow);
        ringG.add(glowRing);
        for (let k = 0; k < 7; k++) {
          const a = (k / 6) * arc;
          const spike = new THREE.Mesh(new THREE.ConeGeometry(4 - i, 34 - i * 6, 4), dark);
          spike.position.set(Math.cos(a) * (R0 + 14), Math.sin(a) * (R0 + 14), 0);
          spike.rotation.z = a - Math.PI / 2;
          ringG.add(spike);
        }
        ringG.rotation.set(i * 0.9, i * 1.3, i * 0.5);
        g.add(ringG);
        rings.push(ringG);
      }
      const coreMesh = new THREE.Mesh(new THREE.OctahedronGeometry(26, 0), dark);
      g.add(coreMesh);
      const heart = new THREE.Mesh(new THREE.OctahedronGeometry(15, 0), this.latticeGlow);
      g.add(heart);
      g.position.set(0.84, 0.03, -0.54).normalize().multiplyScalar(1150);
      g.scale.setScalar(1.7);
      scene.add(g);
      this.lattice = g;
      this.latticeRings = rings;
      this.latticeCore = coreMesh;
    }

    // ---- jump tunnel ----------------------------------------------------------
    this.tunnelMat = new THREE.ShaderMaterial({
      uniforms: { uTime: { value: 0 }, uAmt: { value: 0 } },
      vertexShader: `varying vec2 vUv; void main() { vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
      fragmentShader: `
uniform float uTime; uniform float uAmt; varying vec2 vUv;
float h(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
void main() {
  float lanes = 180.0;
  float id = floor(vUv.x * lanes);
  float r = h(vec2(id, 1.0));
  float speed = 2.0 + r * 6.0;
  float y = fract(vUv.y * (1.0 + r * 2.0) - uTime * speed * 0.35 + r * 10.0);
  float streak = smoothstep(0.0, 0.05, y) * smoothstep(0.55 + 0.4 * r, 0.05, y);
  float lane = smoothstep(0.5, 0.0, abs(fract(vUv.x * lanes) - 0.5)) * step(0.55, r);
  vec3 c = mix(vec3(0.35, 0.7, 1.0), vec3(0.9, 0.6, 1.0), h(vec2(id, 7.0)));
  float a = streak * lane * uAmt;
  float fade = smoothstep(0.0, 0.25, vUv.y) * smoothstep(1.0, 0.6, vUv.y);
  gl_FragColor = vec4(c * a * 5.0 * fade, 1.0);
}`,
      side: THREE.BackSide,
      blending: THREE.AdditiveBlending,
      transparent: true,
      depthWrite: false,
      depthTest: false,
    });
    const tunnel = new THREE.Mesh(new THREE.CylinderGeometry(60, 60, 5000, 48, 1, true), this.tunnelMat);
    tunnel.rotation.x = Math.PI / 2;
    tunnel.visible = false;
    tunnel.frustumCulled = false;
    tunnel.renderOrder = 10;
    scene.add(tunnel);
    this.tunnel = tunnel;
    this.jump = 0;

    // objects that should vanish during the jump
    this.bodies = [this.planet, atmo, ring, core, halo, moonA, moonB, this.rockMesh, this.lattice];
    this._m = new THREE.Matrix4();
    this._q = new THREE.Quaternion();
    this._s = new THREE.Vector3();
  }

  // amt 0..1 ramps the tunnel in; at 1 the outside universe is gone.
  setJump(amt) {
    this.jump = amt;
    this.tunnel.visible = amt > 0.001;
    this.tunnelMat.uniforms.uAmt.value = amt;
    this.starMat.uniforms.uStretch.value = Math.min(1, amt * 1.5);
    const hide = amt > 0.85;
    for (const b of this.bodies) b.visible = !hide;
    this.scene.backgroundIntensity = 1 - Math.min(1, amt) * 0.85;
  }

  update(dt, t) {
    this.starMat.uniforms.uTime.value = t;
    this.starMat.uniforms.uPx.value = Math.max(1, this.gfx.renderer.getPixelRatio()) * (this.gfx.height / 720);
    this.planetMat.uniforms.uTime.value = t;
    this.tunnelMat.uniforms.uTime.value = t;
    this.ring.rotateZ(dt * 0.0015);
    this.moons[0].rotation.y += dt * 0.004;
    for (let i = 0; i < this.latticeRings.length; i++) {
      const r = this.latticeRings[i];
      r.rotation.x += dt * (0.05 + i * 0.03);
      r.rotation.y += dt * (0.035 - i * 0.02);
    }
    this.latticeCore.rotation.y += dt * 0.2;
    const pulse = 0.55 + 0.45 * Math.pow(0.5 + 0.5 * Math.sin(t * 1.9), 3);
    this.latticeGlow.color.setRGB(1.6 * pulse, 0.5 * pulse, 3.4 * pulse);
    // drift the debris sheet slowly aft and wrap it
    const m = this._m;
    for (let i = 0; i < this.rocks.length; i++) {
      const r = this.rocks[i];
      r.p.z += r.drift * dt;
      if (r.p.z > 1900) r.p.z -= 3800;
      r.e.x += r.spin * dt;
      r.e.y += r.spin * 0.7 * dt;
      this._q.setFromEuler(r.e);
      m.compose(r.p, this._q, this._s.set(r.s, r.sy, r.s));
      this.rockMesh.setMatrixAt(i, m);
    }
    this.rockMesh.instanceMatrix.needsUpdate = true;
  }
}
