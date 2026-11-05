#include "render/shaders.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace strata::shaders {

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------
const char* const kCommon = R"GLSL(
const float PI = 3.14159265359;

vec3 linearToSrgb(vec3 c) {
  c = clamp(c, 0.0, 1.0);
  return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}
vec3 srgbToLinear(vec3 c) {
  return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

uint hashU(uint x) {
  x ^= x >> 16u;
  x *= 0x7feb352du;
  x ^= x >> 15u;
  x *= 0x846ca68bu;
  x ^= x >> 16u;
  return x;
}
float hash2(ivec2 p) {
  return float(hashU(uint(p.x) * 0x8da6b343u ^ hashU(uint(p.y) + 0x9e3779b9u))) * (1.0 / 4294967295.0);
}
float hash3(ivec3 p) {
  return float(hashU(uint(p.x) * 0x8da6b343u ^ hashU(uint(p.y) * 0xd8163841u ^ hashU(uint(p.z) + 0x9e3779b9u)))) *
         (1.0 / 4294967295.0);
}

float vnoise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = p - i;
  vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  ivec2 c = ivec2(i);
  float a = hash2(c), b = hash2(c + ivec2(1, 0)), d = hash2(c + ivec2(0, 1)), e = hash2(c + ivec2(1, 1));
  return mix(mix(a, b, u.x), mix(d, e, u.x), u.y);
}
float vnoise3(vec3 p) {
  vec3 i = floor(p);
  vec3 f = p - i;
  vec3 u = f * f * (3.0 - 2.0 * f);
  ivec3 c = ivec3(i);
  float a = mix(hash3(c), hash3(c + ivec3(1, 0, 0)), u.x);
  float b = mix(hash3(c + ivec3(0, 1, 0)), hash3(c + ivec3(1, 1, 0)), u.x);
  float d = mix(hash3(c + ivec3(0, 0, 1)), hash3(c + ivec3(1, 0, 1)), u.x);
  float e = mix(hash3(c + ivec3(0, 1, 1)), hash3(c + ivec3(1, 1, 1)), u.x);
  return mix(mix(a, b, u.y), mix(d, e, u.y), u.z);
}
float fbm(vec2 p, int octaves) {
  float s = 0.0, a = 0.5, n = 0.0;
  for (int i = 0; i < octaves; ++i) {
    s += a * vnoise(p);
    n += a;
    p = p * 2.03 + vec2(17.1, 9.7);
    a *= 0.5;
  }
  return s / n;
}

// Exposure from the adapted average log luminance (shared by bloom and tone mapping).
float exposureFromLog(float avgLog, vec2 range, float bias) {
  return clamp(0.16 / exp(avgLog), range.x, range.y) * bias;
}

float linearizeDepth(float d, vec2 nearFar) {
  float z = d * 2.0 - 1.0;
  return 2.0 * nearFar.x * nearFar.y / (nearFar.y + nearFar.x - z * (nearFar.y - nearFar.x));
}

// Haze seen in a direction: the sky just above the horizon. Used both for
// terrain fog and for the "sky" below the horizon so the two always match.
vec3 fogDirection(vec3 dir) {
  return normalize(vec3(dir.x, max(dir.y, 0.03), dir.z));
}

// Direction -> sky LUT coordinates. Elevation is warped so the horizon gets
// most of the resolution.
vec2 skyLutUv(vec3 dir) {
  float el = asin(clamp(dir.y, -1.0, 1.0));
  float ax = abs(dir.x) < 1e-6 ? 1e-6 : dir.x;
  float az = atan(dir.z, ax);
  float v = 0.5 + 0.5 * sign(el) * sqrt(abs(el) / (0.5 * PI));
  return vec2(az / (2.0 * PI) + 0.5, v);
}

// Clouds: a slab of fractal noise drifting with the wind.
const float kCloudBottom = 190.0;
const float kCloudTop = 232.0;
uniform vec2 uCloudOffset;
uniform float uCloudCoverage;
float cloudCoverageAt(vec2 xz) {
  float base = fbm((xz + uCloudOffset) * 0.0021, 4);
  float cov = 1.0 - uCloudCoverage;
  return clamp((base - cov) / max(1.0 - cov, 0.05) * 2.2, 0.0, 1.0);
}
float cloudDensity(vec3 p) {
  float h = (p.y - kCloudBottom) / (kCloudTop - kCloudBottom);
  if (h <= 0.0 || h >= 1.0) return 0.0;
  float c = cloudCoverageAt(p.xz);
  if (c <= 0.0) return 0.0;
  // Flat-ish bases and billowing tops: thick parts reach higher.
  float profile = smoothstep(0.0, 0.12, h) * (1.0 - smoothstep(0.25 + 0.7 * c, 1.0, h));
  float detail = vnoise3(p * 0.045 + vec3(uCloudOffset.x * 0.02, 0.0, uCloudOffset.y * 0.02));
  return clamp(c * profile - detail * 0.32 * (1.0 - c * 0.5), 0.0, 1.0);
}
float cloudShadowAt(vec3 worldPos, vec3 lightDir) {
  if (lightDir.y < 0.05 || uCloudCoverage <= 0.0) return 1.0;
  float t = (0.5 * (kCloudBottom + kCloudTop) - worldPos.y) / lightDir.y;
  if (t < 0.0) return 1.0;
  return 1.0 - 0.75 * cloudCoverageAt(worldPos.xz + lightDir.xz * t);
}

// Gentle wind animation shared by the colour and shadow passes.
uniform float uTime;
uniform float uWind;
vec3 applyWave(vec3 rel, vec3 world, uint anim) {
  if (anim == 1u) {
    float t = uTime;
    rel.x += (sin(t * 1.3 + world.x * 0.7 + world.y * 0.4) * 0.5 + sin(t * 2.1 + world.z * 1.1) * 0.3) * 0.035 * uWind;
    rel.y += sin(t * 1.7 + world.x * 0.9 + world.z * 0.6) * 0.018 * uWind;
    rel.z += (cos(t * 1.1 + world.z * 0.8 + world.y * 0.5) * 0.5 + sin(t * 1.9 + world.x * 1.3) * 0.3) * 0.035 * uWind;
  } else if (anim == 2u) {
    float t = uTime;
    float gust = 0.6 + 0.4 * sin(t * 0.5 + world.x * 0.05 + world.z * 0.04);
    rel.x += sin(t * 2.0 + world.x * 0.6 + world.z * 0.3) * 0.10 * uWind * gust;
    rel.z += cos(t * 1.6 + world.z * 0.7 + world.x * 0.2) * 0.08 * uWind * gust;
  }
  return rel;
}
)GLSL";

const char* const kAtmosphere = R"GLSL(
const float kGroundR = 6360.0;
const float kTopR = 6460.0;
const float kViewAlt = 0.25;
const vec3 kRayleighBeta = vec3(5.802e-3, 13.558e-3, 33.1e-3);
const float kMieBetaS = 3.996e-3;
const float kMieBetaE = 4.440e-3;
const vec3 kOzoneBeta = vec3(0.650e-3, 1.881e-3, 0.085e-3);

float raySphereFar(vec3 o, vec3 d, float r) {
  float b = dot(o, d);
  float c = dot(o, o) - r * r;
  float disc = b * b - c;
  if (disc < 0.0) return -1.0;
  return -b + sqrt(disc);
}
float raySphereNear(vec3 o, vec3 d, float r) {
  float b = dot(o, d);
  float c = dot(o, o) - r * r;
  float disc = b * b - c;
  if (disc < 0.0) return -1.0;
  return -b - sqrt(disc);
}
vec3 densitiesAt(float h) {
  return vec3(exp(-h / 8.0), exp(-h / 1.2), max(0.0, 1.0 - abs(h - 25.0) / 15.0));
}
vec3 extinctionOf(vec3 od) {
  return kRayleighBeta * od.x + vec3(kMieBetaE * od.y) + kOzoneBeta * od.z;
}
vec3 opticalDepthToSpace(vec3 p, vec3 d) {
  float t = raySphereFar(p, d, kTopR);
  vec3 od = vec3(0.0);
  if (t <= 0.0) return od;
  float ds = t / 6.0;
  for (int i = 0; i < 6; ++i) {
    vec3 q = p + d * ((float(i) + 0.5) * ds);
    od += densitiesAt(length(q) - kGroundR) * ds;
  }
  return od;
}
// Single scattering plus an ambient (isotropic) term approximating light that
// scattered more than once. Samples are packed towards the viewer.
vec3 skyScatter(vec3 d, vec3 l, float intensity, vec3 ambient) {
  vec3 o = vec3(0.0, kGroundR + kViewAlt, 0.0);
  float tMax = raySphereFar(o, d, kTopR);
  float tg = raySphereNear(o, d, kGroundR);
  if (tg > 0.0) tMax = tg;
  if (tMax <= 0.0) return vec3(0.0);
  const int N = 24;
  const float n2 = float(N * N);
  vec3 view = vec3(0.0);
  vec3 sumR = vec3(0.0), sumM = vec3(0.0), sumMs = vec3(0.0);
  for (int i = 0; i < N; ++i) {
    float fi = float(i);
    float ds = tMax * (2.0 * fi + 1.0) / n2;
    vec3 p = o + d * (tMax * (fi + 0.5) * (fi + 0.5) / n2);
    vec3 den = densitiesAt(length(p) - kGroundR);
    view += den * ds;
    sumMs += exp(-extinctionOf(view)) * (kRayleighBeta * den.x + vec3(kMieBetaS * den.y)) * ds;
    if (raySphereNear(p, l, kGroundR) > 0.0) continue;
    vec3 att = exp(-extinctionOf(view + opticalDepthToSpace(p, l)));
    sumR += att * den.x * ds;
    sumM += att * den.y * ds;
  }
  float mu = dot(d, l);
  float g = 0.8;
  float g2 = g * g;
  float pR = 3.0 / (16.0 * PI) * (1.0 + mu * mu);
  float pM = 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + mu * mu)) / ((2.0 + g2) * pow(1.0 + g2 - 2.0 * g * mu, 1.5));
  return (sumR * kRayleighBeta * pR + sumM * kMieBetaS * pM) * intensity + sumMs * ambient;
}
)GLSL";

// ---------------------------------------------------------------------------
// Terrain
// ---------------------------------------------------------------------------
const char* const kTerrainVS = R"GLSL(
layout(location = 0) in ivec3 aPos;
layout(location = 1) in uint aTex;
layout(location = 2) in uvec4 aParams;  // u, v, normal, ao
layout(location = 3) in uvec4 aLight;   // sky, block, anim, flags
layout(location = 4) in vec4 aColor;

uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uCameraPos;

out vec3 vRel;
out vec3 vWorld;
out vec3 vUv;
out vec3 vNormal;
out vec3 vTint;
out vec2 vLight;
out float vAo;
flat out uint vFlags;
flat out uint vKind;

const vec3 kNormals[7] = vec3[7](vec3(1.0, 0.0, 0.0), vec3(-1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, -1.0, 0.0),
                                 vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, -1.0), vec3(0.0, 1.0, 0.0));

void main() {
  vec3 local = vec3(aPos) / 32.0;
  vec3 rel = (uModel * vec4(local, 1.0)).xyz;
  vec3 world = rel + uCameraPos;
  rel = applyWave(rel, world, aLight.z);
  vRel = rel;
  vWorld = world;
  vUv = vec3(vec2(aParams.xy) / 16.0, float(aTex));
  uint ni = min(aParams.z, 6u);
  vNormal = mat3(uModel) * kNormals[ni];
  vKind = ni;
  vAo = float(aParams.w) / 255.0;
  vLight = vec2(aLight.xy) / 255.0;
  vTint = srgbToLinear(aColor.rgb);
  vFlags = aLight.w;
  gl_Position = uViewProj * vec4(rel, 1.0);
}
)GLSL";

const char* const kTerrainFS = R"GLSL(
in vec3 vRel;
in vec3 vWorld;
in vec3 vUv;
in vec3 vNormal;
in vec3 vTint;
in vec2 vLight;
in float vAo;
flat in uint vFlags;
flat in uint vKind;

uniform sampler2DArray uAlbedo;
uniform sampler2DArray uMaterial;
uniform sampler2DArrayShadow uShadowMap;
uniform sampler2D uSkyLut;

uniform mat4 uShadowMat[4];
uniform vec4 uCascadeEnd;
uniform vec4 uCascadeTexel;
uniform int uCascades;
uniform vec3 uCamForward;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;
uniform vec3 uBounce;  // light reflected by the (sunlit) ground
uniform vec3 uBlockColor;
uniform float uFogDensity;
uniform float uFadeStart;
uniform float uFadeEnd;
uniform float uCameraSky;
uniform float uAlphaTest;
uniform float uCloudShadows;
uniform float uEntityLight;  // 1 for entities: brighten slightly so they read well

#ifdef WATER
uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform vec2 uScreenSize;
uniform vec2 uNearFar;
uniform mat4 uView;
uniform mat4 uProj;
uniform int uSsr;
#endif

out vec4 fragColor;

mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv) {
  vec3 dp1 = dFdx(p);
  vec3 dp2 = dFdy(p);
  vec2 duv1 = dFdx(uv);
  vec2 duv2 = dFdy(uv);
  vec3 dp2perp = cross(dp2, N);
  vec3 dp1perp = cross(N, dp1);
  vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
  float invmax = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-20));
  return mat3(T * invmax, B * invmax, N);
}

float shadowFactor(vec3 rel, vec3 N) {
  if (uCascades == 0) return 1.0;
  float depth = dot(rel, uCamForward);
  int c = -1;
  for (int i = 0; i < 4; ++i) {
    if (i < uCascades && depth < uCascadeEnd[i]) {
      c = i;
      break;
    }
  }
  if (c < 0) return 1.0;
  float nl = dot(N, uLightDir);
  vec3 p = rel + N * uCascadeTexel[c] * (1.2 + 1.8 * (1.0 - abs(nl)));
  vec4 sc = uShadowMat[c] * vec4(p, 1.0);
  vec3 s = sc.xyz / sc.w;
  if (s.x <= 0.0 || s.y <= 0.0 || s.x >= 1.0 || s.y >= 1.0 || s.z >= 1.0) return 1.0;
  vec2 ts = 1.0 / vec2(textureSize(uShadowMap, 0).xy);
  float sum = 0.0;
  for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x)
      sum += texture(uShadowMap, vec4(s.xy + vec2(float(x), float(y)) * ts * 1.25, float(c), s.z - 0.0004));
  float sh = sum / 9.0;
  float last = uCascadeEnd[uCascades - 1];
  return mix(sh, 1.0, smoothstep(last * 0.82, last, depth));
}

vec3 skyColor(vec3 dir) {
  return texture(uSkyLut, skyLutUv(dir)).rgb;
}

vec3 applyFog(vec3 color, vec3 rel) {
  float dist = length(rel);
  vec3 dir = rel / max(dist, 1e-4);
  float heightFalloff = exp(-max(vWorld.y - 62.0, 0.0) * 0.012);
  float haze = 1.0 - exp(-dist * uFogDensity * heightFalloff);
  float edge = smoothstep(uFadeStart, uFadeEnd, length(rel.xz));
  float f = clamp(max(haze, edge), 0.0, 1.0);
  vec3 fc = skyColor(fogDirection(dir)) * mix(0.03, 1.0, uCameraSky);
  return mix(color, fc, f);
}

void main() {
  vec4 albedoT = texture(uAlbedo, vUv);
#ifdef OPAQUE
  if (albedoT.a < uAlphaTest) discard;
#endif
  vec3 albedo = albedoT.rgb;
  if ((vFlags & 1u) != 0u) {
    float m = (vFlags & 2u) != 0u ? albedoT.a : 1.0;
    albedo = mix(albedo, albedo * vTint * 1.55, m);
  }
  vec4 mat = texture(uMaterial, vUv);
  vec3 N = normalize(vNormal);
  bool plant = vKind == 6u;
  vec3 Np = N;
  if (!plant) {
    vec2 nxy = mat.xy * 2.0 - 1.0;
    vec3 nts = vec3(nxy, sqrt(max(1.0 - dot(nxy, nxy), 0.0)));
    Np = normalize(cotangentFrame(N, vRel, vUv.xy) * nts);
  }

  float sky = vLight.x;
  float blk = vLight.y;
  float skyAmb = sky * sky;
  float directMask = smoothstep(0.30, 0.75, sky);
  float shadow = 1.0;
  if (dot(uLightColor, vec3(1.0)) > 1e-5) shadow = shadowFactor(vRel, N);
  if (uCloudShadows > 0.5) shadow *= cloudShadowAt(vWorld, uLightDir);

  float NdotL = dot(Np, uLightDir);
  float diffuseTerm;
  if (plant)
    diffuseTerm = 0.45 + 0.4 * abs(NdotL);  // thin foliage also transmits light
  else if ((vFlags & 8u) != 0u)
    diffuseTerm = max(NdotL, 0.0) * 0.85 + 0.22 * max(-NdotL, 0.0);  // leaves: some translucency
  else
    diffuseTerm = max(NdotL, 0.0) * step(-0.02, dot(N, uLightDir));

  vec3 direct = uLightColor * diffuseTerm * shadow * directMask;
  vec3 ambient = (uAmbient * (0.75 + 0.25 * Np.y) + uBounce * (0.5 - 0.5 * Np.y)) * skyAmb * vAo +
                 vec3(0.0007, 0.0008, 0.0011) * vAo;
  vec3 torch = uBlockColor * pow(blk, 2.6) * 1.6 * vAo;
  vec3 color = albedo * (direct + ambient + torch) * (1.0 + 0.15 * uEntityLight);

  vec3 V = normalize(-vRel);
  float smoothness = mat.z;
  if (!plant && NdotL > 0.0 && smoothness > 0.02) {
    vec3 H = normalize(V + uLightDir);
    float rough = clamp(1.0 - smoothness, 0.06, 1.0);
    float a = rough * rough;
    float a2 = a * a;
    float NdotH = max(dot(Np, H), 0.0);
    float NdotV = max(dot(Np, V), 1e-3);
    float dd = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * dd * dd);
    float k = a * 0.5;
    float G = (NdotL / (NdotL * (1.0 - k) + k)) * (NdotV / (NdotV * (1.0 - k) + k));
    float F = 0.04 + 0.96 * pow(1.0 - max(dot(H, V), 0.0), 5.0);
    color += uLightColor * PI * (D * G * F / (4.0 * NdotV)) * shadow * directMask;
  }
  color += albedo * mat.w * 5.0;

#ifdef TRANSLUCENT
  // Glass and ice: reflect the sky with a Fresnel term.
  float fres = 0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
  vec3 R = reflect(-V, N);
  R.y = abs(R.y);
  color += skyColor(R) * fres * (0.2 + 0.8 * skyAmb);
  fragColor = vec4(applyFog(color, vRel), clamp(albedoT.a + fres * 0.5, 0.0, 1.0));
#elif defined(WATER)
  float dist = length(vRel);
  bool fromBelow = (vFlags & 4u) != 0u;
  vec3 Nw = N;
  if (N.y > 0.5) {
    float strength = 0.30 / (1.0 + dist * 0.035);
    vec2 p = vWorld.xz;
    float e = 0.07;
    float t = uTime;
    #define WAVE(q) (vnoise(q * 0.45 + vec2(t * 0.35, t * 0.22)) * 0.55 + vnoise(q * 1.1 + vec2(-t * 0.5, t * 0.41)) * 0.30 + vnoise(q * 2.6 + vec2(t * 0.8, -t * 0.66)) * 0.15)
    float hx = WAVE((p + vec2(e, 0.0))) - WAVE((p - vec2(e, 0.0)));
    float hz = WAVE((p + vec2(0.0, e))) - WAVE((p - vec2(0.0, e)));
    #undef WAVE
    Nw = normalize(vec3(-hx / (2.0 * e) * strength, 1.0, -hz / (2.0 * e) * strength));
  }
  vec3 Nv = fromBelow ? -Nw : Nw;
  float NdotV = clamp(dot(Nv, V), 0.0, 1.0);
  vec2 suv = gl_FragCoord.xy / uScreenSize;
  float waterZ = linearizeDepth(gl_FragCoord.z, uNearFar);
  float sceneZ = linearizeDepth(texture(uSceneDepth, suv).r, uNearFar);
  vec2 nView = (uView * vec4(Nw, 0.0)).xy;
  vec2 ruv = clamp(suv + nView * 0.045 * clamp((sceneZ - waterZ) * 0.5, 0.0, 1.0), vec2(0.001), vec2(0.999));
  float rz = linearizeDepth(texture(uSceneDepth, ruv).r, uNearFar);
  if (rz < waterZ) {
    ruv = suv;
    rz = sceneZ;
  }
  vec3 behind = texture(uSceneColor, ruv).rgb;
  vec3 lightIn = uAmbient * skyAmb * 3.0 + uLightColor * max(uLightDir.y, 0.0) * 0.35 * directMask * shadow + torch;
  vec3 wcolor;
  if (!fromBelow) {
    float path = max(rz - waterZ, 0.0);
    vec3 absorb = exp(-vec3(0.39, 0.085, 0.06) * path);
    vec3 scatterCol = vec3(0.010, 0.040, 0.050) * lightIn;
    vec3 transmitted = behind * absorb + scatterCol * (1.0 - exp(-path * 0.25));
    vec3 R = reflect(-V, Nv);
    R.y = abs(R.y);
    vec3 refl = skyColor(R) * mix(0.03, 1.0, skyAmb);
    if (uSsr == 1) {
      vec3 posV = (uView * vec4(vRel, 1.0)).xyz;
      vec3 dirV = normalize((uView * vec4(R, 0.0)).xyz);
      float stepLen = 0.35;
      vec3 q = posV + dirV * 0.1;
      vec4 hit = vec4(0.0);
      for (int i = 0; i < 40; ++i) {
        q += dirV * stepLen;
        stepLen *= 1.09;
        vec4 clip = uProj * vec4(q, 1.0);
        if (clip.w <= 0.0) break;
        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if (uv.x <= 0.0 || uv.y <= 0.0 || uv.x >= 1.0 || uv.y >= 1.0) break;
        float sd = texture(uSceneDepth, uv).r;
        float sz = linearizeDepth(sd, uNearFar);
        float rayZ = -q.z;
        if (rayZ > sz + 0.03 && rayZ - sz < stepLen * 2.5 + 0.4) {
          vec3 a = q - dirV * stepLen;
          vec3 b = q;
          for (int j = 0; j < 6; ++j) {
            vec3 m = 0.5 * (a + b);
            vec4 mc = uProj * vec4(m, 1.0);
            vec2 muv = mc.xy / mc.w * 0.5 + 0.5;
            if (-m.z > linearizeDepth(texture(uSceneDepth, muv).r, uNearFar)) b = m; else a = m;
          }
          vec4 bc = uProj * vec4(b, 1.0);
          vec2 buv = bc.xy / bc.w * 0.5 + 0.5;
          if (texture(uSceneDepth, buv).r < 0.99999) {
            vec2 edge = smoothstep(vec2(0.0), vec2(0.08), buv) * (1.0 - smoothstep(vec2(0.92), vec2(1.0), buv));
            hit = vec4(texture(uSceneColor, buv).rgb, edge.x * edge.y * (1.0 - float(i) / 40.0));
          }
          break;
        }
      }
      refl = mix(refl, hit.rgb, hit.a);
    }
    float F = 0.02 + 0.98 * pow(1.0 - NdotV, 5.0);
    wcolor = mix(transmitted, refl, F);
    // Sun glint.
    float NdotLw = max(dot(Nv, uLightDir), 0.0);
    if (NdotLw > 0.0) {
      vec3 H = normalize(V + uLightDir);
      float a2 = 0.0009;
      float dd = pow(max(dot(Nv, H), 0.0), 2.0) * (a2 - 1.0) + 1.0;
      float D = a2 / (PI * dd * dd);
      wcolor += uLightColor * PI * D * F * 0.25 * shadow * directMask * NdotLw / max(NdotV, 0.1);
    }
    wcolor = applyFog(wcolor, vRel);
  } else {
    // Seen from under water: Snell's window, total internal reflection outside it.
    float tir = 1.0 - smoothstep(0.60, 0.68, NdotV);
    vec3 deep = vec3(0.010, 0.045, 0.060) * lightIn;
    wcolor = mix(behind * vec3(0.82, 0.94, 1.0), deep, tir);
  }
  fragColor = vec4(wcolor, 1.0);
#else
  fragColor = vec4(applyFog(color, vRel), 1.0);
#endif
}
)GLSL";

const char* const kShadowVS = R"GLSL(
layout(location = 0) in ivec3 aPos;
layout(location = 1) in uint aTex;
layout(location = 2) in uvec4 aParams;
layout(location = 3) in uvec4 aLight;

uniform mat4 uLightViewProj;
uniform mat4 uModel;
uniform vec3 uCameraPos;
out vec3 vUv;

void main() {
  vec3 rel = (uModel * vec4(vec3(aPos) / 32.0, 1.0)).xyz;
  rel = applyWave(rel, rel + uCameraPos, aLight.z);
  vUv = vec3(vec2(aParams.xy) / 16.0, float(aTex));
  gl_Position = uLightViewProj * vec4(rel, 1.0);
}
)GLSL";

const char* const kShadowFS = R"GLSL(
in vec3 vUv;
uniform sampler2DArray uAlbedo;
uniform float uAlphaTest;
void main() {
  if (uAlphaTest > 0.0 && texture(uAlbedo, vUv).a < uAlphaTest) discard;
}
)GLSL";

// ---------------------------------------------------------------------------
// Full screen passes
// ---------------------------------------------------------------------------
const char* const kFullscreenVS = R"GLSL(
out vec2 vUv;
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vUv = p;
#ifdef FAR_PLANE
  gl_Position = vec4(p * 2.0 - 1.0, 1.0, 1.0);
#else
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
#endif
}
)GLSL";

const char* const kSkyLutFS = R"GLSL(
in vec2 vUv;
uniform vec3 uSunDir;
uniform vec3 uMoonDir;
uniform float uMoonIntensity;
uniform vec3 uAmbientRadiance;
out vec4 fragColor;
void main() {
  float az = (vUv.x - 0.5) * 2.0 * PI;
  float t = vUv.y * 2.0 - 1.0;
  float el = sign(t) * t * t * 0.5 * PI;
  vec3 dir = vec3(cos(el) * cos(az), sin(el), cos(el) * sin(az));
  vec3 c = skyScatter(dir, uSunDir, 20.0, uAmbientRadiance);
  c += skyScatter(dir, uMoonDir, 20.0 * uMoonIntensity, vec3(0.0));
  c += vec3(0.0010, 0.0014, 0.0024) * (1.0 - 0.6 * max(dir.y, 0.0));  // airglow / light pollution
  fragColor = vec4(c, 1.0);
}
)GLSL";

const char* const kSkyFS = R"GLSL(
in vec2 vUv;
uniform mat4 uInvViewProj;
uniform sampler2D uSkyLut;
uniform vec3 uSunDir;
uniform vec3 uMoonDir;
uniform vec3 uSunDisc;
uniform float uMoonBrightness;
uniform float uMoonPhase;     // 0 new .. 0.5 full .. 1 new
uniform float uStars;         // visibility 0..1
uniform float uStarAngle;
uniform float uCameraSky;
out vec4 fragColor;

vec3 rotateY(vec3 v, float a) {
  float c = cos(a), s = sin(a);
  return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

void main() {
  vec4 p = uInvViewProj * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
  vec3 dir = normalize(p.xyz / p.w);
  // Below the horizon (beyond the loaded terrain) show the same haze the
  // terrain fades into, instead of the dark ground-facing atmosphere.
  vec3 haze = texture(uSkyLut, skyLutUv(fogDirection(dir))).rgb * mix(0.03, 1.0, uCameraSky);
  vec3 col = mix(haze, texture(uSkyLut, skyLutUv(dir)).rgb, smoothstep(-0.02, 0.03, dir.y));
  float horizon = smoothstep(-0.02, 0.06, dir.y);

  // Sun disc with limb darkening.
  const float sunRadius = 0.0105;
  float cs = dot(dir, uSunDir);
  float sunAngle = acos(clamp(cs, -1.0, 1.0));
  if (sunAngle < sunRadius * 1.2) {
    float r = sunAngle / sunRadius;
    float limb = 0.45 + 0.55 * sqrt(max(0.0, 1.0 - r * r));
    col += uSunDisc * limb * (1.0 - smoothstep(0.96, 1.02, r)) * horizon;
  }

  // Moon disc with phase shading and darker maria.
  const float moonRadius = 0.016;
  float moonAngle = acos(clamp(dot(dir, uMoonDir), -1.0, 1.0));
  if (moonAngle < moonRadius * 1.1 && uMoonBrightness > 0.0) {
    vec3 up = abs(uMoonDir.y) > 0.99 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, uMoonDir));
    vec3 mup = cross(uMoonDir, right);
    vec2 q = vec2(dot(dir - uMoonDir, right), dot(dir - uMoonDir, mup)) / moonRadius;
    float rr = dot(q, q);
    if (rr < 1.0) {
      vec3 n = vec3(q, sqrt(1.0 - rr));
      float phaseAngle = uMoonPhase * 2.0 * PI;
      vec3 lit = vec3(sin(phaseAngle), 0.0, -cos(phaseAngle));
      float light = smoothstep(-0.05, 0.1, dot(n, lit));
      float maria = vnoise(q * 3.0 + 7.0) * 0.6 + vnoise(q * 7.0 + 3.0) * 0.4;
      float albedo = 0.55 + 0.45 * smoothstep(0.35, 0.7, maria);
      float edge = 1.0 - smoothstep(0.92, 1.0, rr);
      col += vec3(0.95, 0.93, 0.88) * uMoonBrightness * light * albedo * edge * horizon;
    }
  }

  // Stars on a slowly turning celestial sphere.
  if (uStars > 0.001 && dir.y > -0.05) {
    vec3 sd = rotateY(dir, uStarAngle);
    vec3 g = sd * 260.0;
    ivec3 cell = ivec3(floor(g));
    float h = hash3(cell);
    if (h > 0.9955) {
      vec3 off = vec3(hash3(cell + ivec3(7, 0, 0)), hash3(cell + ivec3(0, 7, 0)), hash3(cell + ivec3(0, 0, 7))) * 0.7 + 0.15;
      float d = length(fract(g) - off);
      float bright = (h - 0.9955) / 0.0045;
      float twinkle = 0.75 + 0.25 * sin(uTime * (2.0 + bright * 3.0) + h * 1000.0);
      vec3 tint = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.85, 0.7), hash3(cell + ivec3(3, 3, 3)));
      col += tint * smoothstep(0.16, 0.0, d) * (0.02 + 0.25 * bright * bright) * twinkle * uStars * horizon;
    }
  }
  fragColor = vec4(min(col, vec3(30000.0)), 1.0);
}
)GLSL";

const char* const kCompositeFS = R"GLSL(
in vec2 vUv;
uniform sampler2D uScene;
uniform sampler2D uDepth;
uniform sampler2D uSkyLut;
uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;
uniform int uCloudSteps;
uniform int uUnderwater;
uniform float uCameraSky;
uniform float uLavaView;
uniform float uFadeStart;
uniform float uFadeEnd;
out vec4 fragColor;

float hgPhase(float mu, float g) {
  float g2 = g * g;
  return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * mu, 1.5));
}

vec4 marchClouds(vec3 ro, vec3 rd, float maxDist) {
  float t0, t1;
  if (abs(rd.y) < 1e-4) {
    if (ro.y < kCloudBottom || ro.y > kCloudTop) return vec4(0.0, 0.0, 0.0, 1.0);
    t0 = 0.0;
    t1 = 6000.0;
  } else {
    float ta = (kCloudBottom - ro.y) / rd.y;
    float tb = (kCloudTop - ro.y) / rd.y;
    t0 = max(min(ta, tb), 0.0);
    t1 = max(ta, tb);
  }
  t1 = min(t1, min(maxDist, 6000.0));
  if (t1 <= t0) return vec4(0.0, 0.0, 0.0, 1.0);
  int steps = uCloudSteps;
  float dt = (t1 - t0) / float(steps);
  float t = t0 + dt * hash2(ivec2(gl_FragCoord.xy));
  float T = 1.0;
  vec3 L = vec3(0.0);
  float mu = dot(rd, uLightDir);
  float phase = mix(hgPhase(mu, 0.65), hgPhase(mu, -0.15), 0.35);
  vec3 sunL = uLightColor * PI;
  const float sigma = 0.11;
  for (int i = 0; i < 64; ++i) {
    if (i >= steps) break;
    vec3 pos = ro + rd * t;
    float dens = cloudDensity(pos);
    if (dens > 0.004) {
      float od = 0.0;
      for (int j = 1; j <= 3; ++j) od += cloudDensity(pos + uLightDir * (float(j) * 10.0));
      float beer = exp(-od * 10.0 * sigma);
      float powder = 1.0 - exp(-dens * 4.0);
      float h = clamp((pos.y - kCloudBottom) / (kCloudTop - kCloudBottom), 0.0, 1.0);
      vec3 amb = uAmbient * PI * (0.45 + 0.55 * h) * 0.55;
      vec3 S = sunL * beer * phase * (0.35 + 0.65 * powder) * 3.0 + amb;
      float ext = dens * sigma;
      float Ts = exp(-ext * dt);
      L += T * S * (1.0 - Ts);
      T *= Ts;
      if (T < 0.02) break;
    }
    t += dt;
  }
  float fade = exp(-t0 * 0.00032);
  vec3 skyCol = texture(uSkyLut, skyLutUv(rd)).rgb;
  L = mix(skyCol * (1.0 - T), L, fade);
  return vec4(L, T);
}

void main() {
  vec3 color = texture(uScene, vUv).rgb;
  float d = texture(uDepth, vUv).r;
  vec4 p = uInvViewProj * vec4(vUv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
  vec3 rel = p.xyz / p.w;
  vec3 dir = normalize(rel);
  float dist = d >= 0.999999 ? 1e9 : length(rel);
  if (uUnderwater == 1) {
    float dd = min(dist, 200.0);
    vec3 absorb = exp(-vec3(0.36, 0.075, 0.055) * dd);
    vec3 fogc = vec3(0.010, 0.045, 0.062) *
                (uAmbient * 3.0 * uCameraSky * uCameraSky + uLightColor * max(uLightDir.y, 0.0) * 0.25 * uCameraSky);
    color = color * absorb + fogc * (1.0 - absorb.g);
  } else if (uLavaView > 0.5) {
    float dd = min(dist, 50.0);
    color = mix(color, vec3(1.8, 0.45, 0.05), 1.0 - exp(-dd * 1.2));
  } else if (uCloudSteps > 0) {
    vec4 cl = marchClouds(uCameraPos, dir, dist);
    // Terrain dissolved by the fade at the edge of the view must not hide the
    // clouds behind it (it would leave holes in the cloud layer).
    float edge = d >= 0.999999 ? 0.0 : smoothstep(uFadeStart, uFadeEnd, length(rel.xz));
    if (edge > 0.0) cl = mix(cl, marchClouds(uCameraPos, dir, 1e9), edge);
    color = color * cl.a + cl.rgb;
  }
  fragColor = vec4(color, 1.0);
}
)GLSL";

const char* const kBloomDownFS = R"GLSL(
in vec2 vUv;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform int uPrefilter;
uniform float uThreshold;
uniform sampler2D uAdapt;
uniform vec2 uExposureRange;
uniform float uExposureBias;
out vec4 fragColor;
vec3 tap(vec2 o) { return texture(uSrc, vUv + o * uTexel).rgb; }
void main() {
  vec3 a = tap(vec2(-2.0, -2.0)), b = tap(vec2(0.0, -2.0)), c = tap(vec2(2.0, -2.0));
  vec3 d = tap(vec2(-1.0, -1.0)), e = tap(vec2(1.0, -1.0));
  vec3 f = tap(vec2(-2.0, 0.0)), g = tap(vec2(0.0, 0.0)), h = tap(vec2(2.0, 0.0));
  vec3 i = tap(vec2(-1.0, 1.0)), j = tap(vec2(1.0, 1.0));
  vec3 k = tap(vec2(-2.0, 2.0)), l = tap(vec2(0.0, 2.0)), m = tap(vec2(2.0, 2.0));
  vec3 col = (d + e + i + j) * 0.125 + (a + b + f + g) * 0.03125 + (b + c + g + h) * 0.03125 +
             (f + g + k + l) * 0.03125 + (g + h + l + m) * 0.03125;
  if (uPrefilter == 1) {
    // Threshold in display units so bloom follows eye adaptation.
    col *= exposureFromLog(texture(uAdapt, vec2(0.5)).r, uExposureRange, uExposureBias);
    float br = max(col.r, max(col.g, col.b));
    float knee = uThreshold * 0.5;
    float soft = clamp(br - uThreshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-4);
    col *= max(soft, br - uThreshold) / max(br, 1e-4);
    col = min(col, vec3(2000.0));
  }
  fragColor = vec4(col, 1.0);
}
)GLSL";

const char* const kBloomUpFS = R"GLSL(
in vec2 vUv;
uniform sampler2D uSrc;
uniform vec2 uTexel;
out vec4 fragColor;
void main() {
  vec4 d = vec4(uTexel, -uTexel.x, 0.0);
  vec3 s = texture(uSrc, vUv - d.xy).rgb;
  s += texture(uSrc, vUv - d.wy).rgb * 2.0;
  s += texture(uSrc, vUv - d.zy).rgb;
  s += texture(uSrc, vUv + d.zw).rgb * 2.0;
  s += texture(uSrc, vUv).rgb * 4.0;
  s += texture(uSrc, vUv + d.xw).rgb * 2.0;
  s += texture(uSrc, vUv + d.zy).rgb;
  s += texture(uSrc, vUv + d.wy).rgb * 2.0;
  s += texture(uSrc, vUv + d.xy).rgb;
  fragColor = vec4(s / 16.0, 1.0);
}
)GLSL";

const char* const kLuminanceFS = R"GLSL(
in vec2 vUv;
uniform sampler2D uScene;
out vec4 fragColor;
void main() {
  vec3 c = texture(uScene, vUv).rgb;
  float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
  // Centre weighted metering: the middle of the screen matters most.
  vec2 q = vUv - 0.5;
  float w = 1.0 - 0.6 * smoothstep(0.1, 0.5, length(q));
  fragColor = vec4(log(clamp(lum, 1e-5, 60.0)) * w, w, 0.0, 1.0);
}
)GLSL";

const char* const kAdaptFS = R"GLSL(
in vec2 vUv;
uniform sampler2D uLum;
uniform sampler2D uPrev;
uniform float uLod;
uniform float uRate;
out vec4 fragColor;
void main() {
  vec2 s = textureLod(uLum, vec2(0.5), uLod).rg;
  float current = s.x / max(s.y, 1e-4);
  float prev = texture(uPrev, vec2(0.5)).r;
  fragColor = vec4(prev + (current - prev) * uRate, 0.0, 0.0, 1.0);
}
)GLSL";

const char* const kTonemapFS = R"GLSL(
in vec2 vUv;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uAdapt;
uniform float uBloomStrength;
uniform float uExposureBias;
uniform vec2 uExposureRange;
uniform float uVignette;
uniform vec4 uOverlay;  // rgb tint and amount (damage flash, under water, ...)
out vec4 fragColor;

vec3 filmic(vec3 x) {
  // Rational fit of the ACES reference rendering transform.
  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
  float exposure = exposureFromLog(texture(uAdapt, vec2(0.5)).r, uExposureRange, uExposureBias);
  vec3 col = texture(uScene, vUv).rgb * exposure + texture(uBloom, vUv).rgb * uBloomStrength;
  col = filmic(col);
  vec2 q = vUv - 0.5;
  col *= 1.0 - uVignette * dot(q, q) * 1.5;
  col = mix(col, uOverlay.rgb, uOverlay.a);
  vec3 srgb = linearToSrgb(col);
  srgb += (hash2(ivec2(gl_FragCoord.xy)) - 0.5) / 255.0;
  fragColor = vec4(srgb, dot(srgb, vec3(0.299, 0.587, 0.114)));
}
)GLSL";

const char* const kFxaaFS = R"GLSL(
in vec2 vUv;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform int uEnabled;
out vec4 fragColor;
void main() {
  vec4 m = texture(uSrc, vUv);
  if (uEnabled == 0) {
    fragColor = vec4(m.rgb, 1.0);
    return;
  }
  float lNW = texture(uSrc, vUv + vec2(-1.0, -1.0) * uTexel).a;
  float lNE = texture(uSrc, vUv + vec2(1.0, -1.0) * uTexel).a;
  float lSW = texture(uSrc, vUv + vec2(-1.0, 1.0) * uTexel).a;
  float lSE = texture(uSrc, vUv + vec2(1.0, 1.0) * uTexel).a;
  float lM = m.a;
  float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
  float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
  if (lMax - lMin < max(0.04, lMax * 0.12)) {
    fragColor = vec4(m.rgb, 1.0);
    return;
  }
  // Blur along the edge, perpendicular to the luminance gradient.
  vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), (lNW + lSW) - (lNE + lSE));
  float reduce = max((lNW + lNE + lSW + lSE) * 0.03, 1.0 / 128.0);
  float scale = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
  dir = clamp(dir * scale, vec2(-6.0), vec2(6.0)) * uTexel;
  vec3 a = 0.5 * (texture(uSrc, vUv - dir * (1.0 / 6.0)).rgb + texture(uSrc, vUv + dir * (1.0 / 6.0)).rgb);
  vec3 b = a * 0.5 + 0.25 * (texture(uSrc, vUv - dir * 0.5).rgb + texture(uSrc, vUv + dir * 0.5).rgb);
  float lb = dot(b, vec3(0.299, 0.587, 0.114));
  fragColor = vec4((lb < lMin || lb > lMax) ? a : b, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
// User interface, lines and particles
// ---------------------------------------------------------------------------
const char* const kUiVS = R"GLSL(
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in float aMode;   // < -1.5 colour, < -0.5 font, otherwise texture layer
layout(location = 3) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUv;
out float vMode;
out vec4 vColor;
void main() {
  vUv = aUv;
  vMode = aMode;
  vColor = aColor;
  gl_Position = vec4(aPos.x / uScreen.x * 2.0 - 1.0, 1.0 - aPos.y / uScreen.y * 2.0, 0.0, 1.0);
}
)GLSL";

const char* const kUiFS = R"GLSL(
in vec2 vUv;
in float vMode;
in vec4 vColor;
uniform sampler2D uFont;
uniform sampler2DArray uBlocks;
out vec4 fragColor;
void main() {
  if (vMode < -1.5) {
    fragColor = vColor;
    return;
  }
  if (vMode < -0.5) {
    fragColor = vec4(vColor.rgb, vColor.a * texture(uFont, vUv).r);
    return;
  }
  float kind = floor(vMode / 1000.0 + 0.001);
  float layer = vMode - kind * 1000.0;
  vec4 t = texture(uBlocks, vec3(vUv, layer));
  vec3 tint = srgbToLinear(vColor.rgb);
  vec3 lin;
  float a;
  if (kind < 0.5) {
    lin = t.rgb * tint;
    a = t.a * vColor.a;
  } else if (kind < 1.5) {
    lin = t.rgb * vColor.a;
    a = 1.0;
  } else if (kind < 2.5) {
    lin = t.rgb * mix(vec3(1.0), tint * 1.55, t.a) * vColor.a;
    a = 1.0;
  } else {
    lin = t.rgb * tint * 1.55;
    a = t.a;
  }
  if (a < 0.4) discard;
  fragColor = vec4(linearToSrgb(lin), a);
}
)GLSL";

const char* const kLineVS = R"GLSL(
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;
void main() { gl_Position = uViewProj * vec4(aPos, 1.0); }
)GLSL";

const char* const kLineFS = R"GLSL(
uniform vec4 uColor;
out vec4 fragColor;
void main() { fragColor = uColor; }
)GLSL";

const char* const kParticleVS = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aUv;
layout(location = 2) in vec3 aLight;  // sky, block, emissive
uniform mat4 uViewProj;
out vec3 vUv;
out vec3 vLight;
out vec3 vRel;
void main() {
  vUv = aUv;
  vLight = aLight;
  vRel = aPos;
  gl_Position = uViewProj * vec4(aPos, 1.0);
}
)GLSL";

const char* const kParticleFS = R"GLSL(
in vec3 vUv;
in vec3 vLight;
in vec3 vRel;
uniform sampler2DArray uAlbedo;
uniform vec3 uLightColor;
uniform vec3 uAmbient;
uniform vec3 uBlockColor;
out vec4 fragColor;
void main() {
  vec4 t = texture(uAlbedo, vUv);
  if (t.a < 0.5) discard;
  float sky = vLight.x;
  vec3 light = uAmbient * sky * sky + uLightColor * 0.55 * smoothstep(0.3, 0.75, sky) + uBlockColor * pow(vLight.y, 2.6) * 1.6;
  fragColor = vec4(t.rgb * light + t.rgb * vLight.z * 5.0, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
// Program table
// ---------------------------------------------------------------------------
const Program kPrograms[] = {
    {"terrain", kTerrainVS, kTerrainFS, "#define OPAQUE", false},
    {"water", kTerrainVS, kTerrainFS, "#define WATER", false},
    {"glass", kTerrainVS, kTerrainFS, "#define TRANSLUCENT", false},
    {"shadow", kShadowVS, kShadowFS, "", false},
    {"sky lut", kFullscreenVS, kSkyLutFS, "", true},
    {"sky", kFullscreenVS, kSkyFS, "#define FAR_PLANE", false},
    {"composite", kFullscreenVS, kCompositeFS, "", false},
    {"bloom down", kFullscreenVS, kBloomDownFS, "", false},
    {"bloom up", kFullscreenVS, kBloomUpFS, "", false},
    {"luminance", kFullscreenVS, kLuminanceFS, "", false},
    {"adapt", kFullscreenVS, kAdaptFS, "", false},
    {"tonemap", kFullscreenVS, kTonemapFS, "", false},
    {"fxaa", kFullscreenVS, kFxaaFS, "", false},
    {"lines", kLineVS, kLineFS, "", false},
    {"particles", kParticleVS, kParticleFS, "", false},
    {"ui", kUiVS, kUiFS, "", false},
};
const int kProgramCount = static_cast<int>(sizeof(kPrograms) / sizeof(kPrograms[0]));

const Program& program(const char* name) {
  for (const Program& p : kPrograms)
    if (std::strcmp(p.name, name) == 0) return p;
  std::fprintf(stderr, "unknown shader program '%s'\n", name);
  std::abort();
}

}  // namespace strata::shaders
