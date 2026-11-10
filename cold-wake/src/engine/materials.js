// Material library. Static geometry is lit by light baked into vertex
// attributes at load time (see builder.js); the shaders here blend that baked
// light with each room's live power level, so whole decks can black out,
// flicker and come back without any per-pixel light cost.
import * as THREE from 'three';
import {
  panelTexture,
  floorTexture,
  treadTexture,
  grateTexture,
  hazardTexture,
  crateTexture,
  brushedTexture,
  fabricTexture,
  hullTexture,
  leafTexture,
  frostTexture,
  particleAtlas,
  SignAtlas,
} from './textures.js';

export const MAX_ROOMS = 20;

export const shared = {
  uRoomPower: { value: new Array(MAX_ROOMS).fill(0) },
  uRoomEmerg: { value: new Array(MAX_ROOMS).fill(0) },
  uTime: { value: 0 },
};

function injectBake(mat) {
  mat.onBeforeCompile = (shader) => {
    shader.uniforms.uRoomPower = shared.uRoomPower;
    shader.uniforms.uRoomEmerg = shared.uRoomEmerg;
    shader.vertexShader = shader.vertexShader
      .replace(
        '#include <common>',
        `#include <common>
attribute vec3 bake;
attribute vec3 bakeE;
attribute float roomIdx;
uniform float uRoomPower[${MAX_ROOMS}];
uniform float uRoomEmerg[${MAX_ROOMS}];
varying vec3 vBake;`
      )
      .replace(
        '#include <begin_vertex>',
        `#include <begin_vertex>
int bakeRoom = int(roomIdx + 0.5);
vBake = bake * uRoomPower[bakeRoom] + bakeE * uRoomEmerg[bakeRoom];`
      );
    shader.fragmentShader = shader.fragmentShader
      .replace(
        '#include <common>',
        `#include <common>
varying vec3 vBake;`
      )
      .replace(
        '#include <emissivemap_fragment>',
        `#include <emissivemap_fragment>
float bakeCavity = clamp(dot(normal, nonPerturbedNormal), 0.0, 1.0);
bakeCavity = mix(0.5, 1.0, bakeCavity * bakeCavity * bakeCavity);
totalEmissiveRadiance += vBake * diffuseColor.rgb * bakeCavity;`
      );
  };
  mat.customProgramCacheKey = () => 'bake-v1';
  return mat;
}

function baked(params) {
  const { tex, ...rest } = params;
  const p = { roughness: 0.6, metalness: 0.3, ...rest };
  if (tex) {
    p.map = tex.map;
    p.normalMap = tex.normalMap;
    if (tex.rmMap) {
      p.roughnessMap = tex.rmMap;
      p.metalnessMap = tex.rmMap;
      p.roughness = rest.roughness ?? 1;
      p.metalness = rest.metalness ?? 1;
    }
  }
  return injectBake(new THREE.MeshStandardMaterial(p));
}

const glowVertexHead = `
attribute vec3 gcol;
attribute vec4 gp;
uniform float uRoomPower[${MAX_ROOMS}];
uniform float uRoomEmerg[${MAX_ROOMS}];
uniform float uTime;
varying vec3 vCol;
#include <common>
#include <fog_pars_vertex>
float glowFactor() {
  int ri = int(gp.x + 0.5);
  float p = uRoomPower[ri];
  float f = 1.0;
  if (gp.y < 0.5) f = p;
  else if (gp.y < 1.5) f = uRoomEmerg[ri];
  else if (gp.y < 2.5) f = 1.0;
  else f = 0.22 + 0.78 * p;
  float ph = gp.w;
  float fx = gp.z;
  if (fx > 0.5 && fx < 1.5) {
    float t = floor(uTime * 14.0 + ph * 57.0);
    float r = fract(sin(t * 12.9898 + ph * 78.233) * 43758.5453);
    f *= mix(0.12, 1.0, step(0.42, r)) * (0.85 + 0.15 * sin(uTime * 61.0 + ph * 9.0));
  } else if (fx > 1.5 && fx < 2.5) {
    f *= 0.62 + 0.38 * sin(uTime * 1.7 + ph * 6.2831);
  } else if (fx > 2.5 && fx < 3.5) {
    f *= step(0.5, fract(uTime * 1.1 + ph));
  } else if (fx > 3.5 && fx < 4.5) {
    float w = 0.5 + 0.5 * sin((position.x + position.z) * 1.3 - uTime * 4.5 + ph * 6.2831);
    f *= 0.3 + 0.7 * w * w * w;
  } else if (fx > 4.5) {
    f *= 0.75 + 0.25 * sin(uTime * 7.0 + ph * 40.0) * sin(uTime * 2.3 + ph * 11.0);
  }
  return f;
}`;

function glowMaterial() {
  return new THREE.ShaderMaterial({
    uniforms: THREE.UniformsUtils.merge([THREE.UniformsLib.fog, {}]),
    vertexShader: `${glowVertexHead}
void main() {
  vCol = gcol * glowFactor();
  vec4 mvPosition = modelViewMatrix * vec4(position, 1.0);
  gl_Position = projectionMatrix * mvPosition;
  #include <fog_vertex>
}`,
    fragmentShader: `
varying vec3 vCol;
#include <common>
#include <fog_pars_fragment>
void main() {
  gl_FragColor = vec4(vCol, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
  #include <fog_fragment>
}`,
    fog: true,
  });
}

function signMaterial(atlasTexture) {
  return new THREE.ShaderMaterial({
    uniforms: THREE.UniformsUtils.merge([THREE.UniformsLib.fog, { map: { value: null } }]),
    vertexShader: `${glowVertexHead}
varying vec2 vUv;
void main() {
  vCol = gcol * glowFactor();
  vUv = uv;
  vec4 mvPosition = modelViewMatrix * vec4(position, 1.0);
  gl_Position = projectionMatrix * mvPosition;
  #include <fog_vertex>
}`,
    fragmentShader: `
uniform sampler2D map;
varying vec3 vCol;
varying vec2 vUv;
#include <common>
#include <fog_pars_fragment>
void main() {
  vec4 t = texture2D(map, vUv);
  if (t.a < 0.04) discard;
  gl_FragColor = vec4(t.rgb * vCol, t.a);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
  #include <fog_fragment>
}`,
    fog: true,
    transparent: true,
    depthWrite: false,
    polygonOffset: true,
    polygonOffsetFactor: -2,
    polygonOffsetUnits: -2,
    side: THREE.DoubleSide,
  });
}

// Additive hologram with scanlines; colour comes from the mesh's `color`.
export function holoMaterial(color = 0x58d8ff, opacity = 0.7) {
  return new THREE.ShaderMaterial({
    uniforms: {
      uTime: shared.uTime,
      uColor: { value: new THREE.Color(color) },
      uOpacity: { value: opacity },
    },
    vertexShader: `
varying vec3 vWorld;
varying vec3 vN;
varying vec3 vView;
void main() {
  vec4 w = modelMatrix * vec4(position, 1.0);
  vWorld = w.xyz;
  vN = normalize(mat3(modelMatrix) * normal);
  vView = normalize(cameraPosition - w.xyz);
  gl_Position = projectionMatrix * viewMatrix * w;
}`,
    fragmentShader: `
uniform float uTime;
uniform vec3 uColor;
uniform float uOpacity;
varying vec3 vWorld;
varying vec3 vN;
varying vec3 vView;
void main() {
  float scan = 0.65 + 0.35 * sin(vWorld.y * 90.0 - uTime * 6.0);
  float fres = pow(1.0 - abs(dot(normalize(vN), normalize(vView))), 1.6);
  float flick = 0.9 + 0.1 * sin(uTime * 23.0 + vWorld.y * 3.0);
  float a = (0.25 + fres * 0.9) * scan * flick * uOpacity;
  gl_FragColor = vec4(uColor * a * 2.2, a);
}`,
    transparent: true,
    depthWrite: false,
    blending: THREE.AdditiveBlending,
    side: THREE.DoubleSide,
  });
}

// Shimmering hex energy barrier (cargo bay atmosphere shield).
export function fieldMaterial(color = 0x4fd8ff) {
  return new THREE.ShaderMaterial({
    uniforms: { uTime: shared.uTime, uColor: { value: new THREE.Color(color) }, uPower: { value: 1 } },
    vertexShader: `
varying vec2 vUv;
void main() {
  vUv = uv;
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}`,
    fragmentShader: `
uniform float uTime;
uniform vec3 uColor;
uniform float uPower;
varying vec2 vUv;
float hexDist(vec2 p) {
  p = abs(p);
  return max(dot(p, normalize(vec2(1.0, 1.7320508))), p.x);
}
void main() {
  vec2 uv = vUv * vec2(26.0, 12.0);
  vec2 r = vec2(1.0, 1.7320508);
  vec2 h = r * 0.5;
  vec2 a = mod(uv, r) - h;
  vec2 b = mod(uv - h, r) - h;
  vec2 g = dot(a, a) < dot(b, b) ? a : b;
  float d = hexDist(g);
  float edge = smoothstep(0.42, 0.5, d);
  float wave = 0.5 + 0.5 * sin(vUv.y * 9.0 - uTime * 1.6 + sin(vUv.x * 7.0 + uTime) * 1.5);
  float rim = smoothstep(0.0, 0.06, vUv.y) * smoothstep(1.0, 0.94, vUv.y);
  float border = 1.0 - smoothstep(0.0, 0.035, min(min(vUv.x, 1.0 - vUv.x), min(vUv.y, 1.0 - vUv.y)));
  float a2 = (edge * (0.2 + 0.5 * wave) + 0.045 + border * 0.8) * uPower;
  gl_FragColor = vec4(uColor * a2 * 1.8, a2 * rim + border * 0.5 * uPower);
}`,
    transparent: true,
    depthWrite: false,
    blending: THREE.AdditiveBlending,
    side: THREE.DoubleSide,
  });
}

export function createLibrary(quality) {
  const big = quality.texSize;
  const med = Math.max(256, big / 2);
  const tex = {
    wall: panelTexture({ size: big, seed: 11, base: [98, 108, 122], depth: 3, cells: 2, accent: { chance: 0.12, color: 'rgba(40,150,190,0.85)', h: 9 } }),
    wallLight: panelTexture({ size: big, seed: 23, base: [132, 138, 147], vary: 9, depth: 2, cells: 2, rough: 0.5, metal: 0.25, grime: 0.3, streaks: 0.25, vents: 0.06, bolts: 0.4 }),
    wallDark: panelTexture({ size: big, seed: 37, base: [70, 75, 84], vary: 10, depth: 4, cells: 2, rough: 0.5, metal: 0.7, vents: 0.2, insets: 0.4 }),
    floor: floorTexture({ size: big, seed: 7 }),
    floorLight: floorTexture({ size: med, seed: 8, base: [112, 118, 126], studs: false, rough: 0.5, metal: 0.2 }),
    tread: treadTexture({ size: med }),
    grate: grateTexture({ size: med }),
    hazard: hazardTexture({ size: 256 }),
    crate: crateTexture({ size: med }),
    brushed: brushedTexture({ size: 256 }),
    fabric: fabricTexture({ size: 256 }),
    hull: hullTexture({ size: big }),
  };
  const leaf = leafTexture({ size: 256, hue: 112 });
  const leafRed = leafTexture({ size: 256, hue: 30, seed: 53 });
  const frost = frostTexture({ size: 256 });
  const atlas = new SignAtlas(quality.texSize >= 1024 ? 2048 : 1024);

  const m = {};
  // architecture
  m.wall = baked({ tex: tex.wall });
  m.wallLight = baked({ tex: tex.wallLight });
  m.wallDark = baked({ tex: tex.wallDark });
  m.ceil = baked({ tex: tex.wallDark, color: 0xb8c0cc });
  m.floor = baked({ tex: tex.floor });
  m.floorLight = baked({ tex: tex.floorLight });
  m.tread = baked({ tex: tex.tread });
  m.grate = baked({ tex: tex.grate });
  m.hazard = baked({ tex: tex.hazard });
  m.hull = baked({ tex: tex.hull });
  m.hullDark = baked({ tex: tex.hull, color: 0x5d6570 });
  // metals and plastics
  m.trim = baked({ tex: tex.brushed, color: 0xc4cad2 });
  m.trimDark = baked({ tex: tex.brushed, color: 0x4a4f58 });
  m.metal = baked({ color: 0x8a9099, roughness: 0.42, metalness: 0.85 });
  m.metalDark = baked({ color: 0x2c3038, roughness: 0.5, metalness: 0.8 });
  m.chrome = baked({ color: 0xdfe6ee, roughness: 0.12, metalness: 1 });
  m.black = baked({ color: 0x121418, roughness: 0.75, metalness: 0.1 });
  m.rubber = baked({ color: 0x1c1e22, roughness: 0.95, metalness: 0 });
  m.white = baked({ color: 0xb4bac3, roughness: 0.35, metalness: 0.05 });
  m.gray = baked({ color: 0x8b9098, roughness: 0.6, metalness: 0.2 });
  m.orange = baked({ color: 0xe8792a, roughness: 0.5, metalness: 0.2 });
  m.yellow = baked({ color: 0xe8b72a, roughness: 0.5, metalness: 0.2 });
  m.red = baked({ color: 0xc23a30, roughness: 0.5, metalness: 0.2 });
  m.blue = baked({ color: 0x2f6fc2, roughness: 0.5, metalness: 0.25 });
  m.teal = baked({ color: 0x2aa6a0, roughness: 0.5, metalness: 0.25 });
  m.green = baked({ color: 0x3f9a4a, roughness: 0.6, metalness: 0.1 });
  m.copper = baked({ color: 0xc8733a, roughness: 0.3, metalness: 1 });
  m.gold = baked({ color: 0xd9b25a, roughness: 0.25, metalness: 1 });
  m.soil = baked({ color: 0x3a2a1e, roughness: 1, metalness: 0 });
  m.skin = baked({ color: 0xb9a79a, roughness: 0.8, metalness: 0 });
  // cargo
  m.crate = baked({ tex: tex.crate });
  m.crateBlue = baked({ tex: tex.crate, color: 0x5d86c9 });
  m.crateOrange = baked({ tex: tex.crate, color: 0xe08a3c });
  m.crateGreen = baked({ tex: tex.crate, color: 0x6aa66f });
  m.crateRed = baked({ tex: tex.crate, color: 0xc65a4e });
  m.crateDark = baked({ tex: tex.crate, color: 0x5a5f68 });
  // soft goods
  m.fabric = baked({ map: tex.fabric.map, normalMap: tex.fabric.normalMap, color: 0x9aa0a8, roughness: 0.95, metalness: 0 });
  m.fabricBlue = baked({ map: tex.fabric.map, normalMap: tex.fabric.normalMap, color: 0x3c5f9a, roughness: 0.95, metalness: 0 });
  m.fabricRed = baked({ map: tex.fabric.map, normalMap: tex.fabric.normalMap, color: 0x9a4038, roughness: 0.95, metalness: 0 });
  m.fabricDark = baked({ map: tex.fabric.map, normalMap: tex.fabric.normalMap, color: 0x2c3038, roughness: 0.95, metalness: 0 });
  // plants
  m.leaf = baked({ map: leaf, alphaTest: 0.45, side: THREE.DoubleSide, roughness: 0.8, metalness: 0 });
  m.leafRed = baked({ map: leafRed, alphaTest: 0.45, side: THREE.DoubleSide, roughness: 0.8, metalness: 0 });
  // glass
  m.glass = new THREE.MeshStandardMaterial({
    color: 0x9fd0e6,
    roughness: 0.04,
    metalness: 0.9,
    transparent: true,
    opacity: 0.16,
    depthWrite: false,
    envMapIntensity: 1.6,
    side: THREE.DoubleSide,
  });
  m.glassTint = new THREE.MeshStandardMaterial({
    color: 0x66e0ff,
    roughness: 0.08,
    metalness: 0.6,
    transparent: true,
    opacity: 0.28,
    depthWrite: false,
    envMapIntensity: 1.4,
    side: THREE.DoubleSide,
    emissive: 0x0c3a4a,
    emissiveIntensity: 0.6,
  });
  m.glassFrost = new THREE.MeshStandardMaterial({
    color: 0xcfeaff,
    map: frost,
    roughness: 0.5,
    metalness: 0.2,
    transparent: true,
    opacity: 0.78,
    depthWrite: false,
    side: THREE.DoubleSide,
    emissive: 0x2c5f80,
    emissiveIntensity: 0.55,
  });
  m.glow = glowMaterial();
  m.sign = signMaterial();
  m.sign.uniforms.map.value = atlas.texture;
  for (const mat of [m.glow, m.sign]) {
    mat.uniforms.uRoomPower = shared.uRoomPower;
    mat.uniforms.uRoomEmerg = shared.uRoomEmerg;
    mat.uniforms.uTime = shared.uTime;
  }

  // Texture repeat size in metres, per material.
  const tile = {
    wall: 4,
    wallLight: 4,
    wallDark: 3.2,
    ceil: 3.2,
    floor: 4,
    floorLight: 4,
    tread: 1.6,
    grate: 1.2,
    hazard: 0.8,
    hull: 6,
    hullDark: 6,
    trim: 1,
    trimDark: 1,
    fabric: 0.5,
    fabricBlue: 0.5,
    fabricRed: 0.5,
    fabricDark: 0.5,
  };

  return { m, tile, atlas, tex, sprites: particleAtlas(), frost };
}
