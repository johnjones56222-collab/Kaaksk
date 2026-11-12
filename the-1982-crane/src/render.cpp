// THE 1982 CRANE - scene renderer (WebGL 2)
// Forward lit, one spot shadow (the flashlight), per-chunk light lists, fog,
// rain, halos and a film / tape post pass.
#include "render.h"
#include "gfx.h"
#include "game.h"

namespace {

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------
const char* VS_WORLD = R"(
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in float aLayer;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aColor;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform mat4 uShadowMat;
uniform mat3 uNormalMat;
uniform float uEmis[8];
out vec3 vWorld;
out vec3 vNormal;
out vec3 vUVL;
out vec3 vColor;
out float vEmis;
out vec4 vShadow;
void main() {
    vec4 w = uModel * vec4(aPos, 1.0);
    vWorld = w.xyz;
    vNormal = uNormalMat * aNormal;
    vUVL = vec3(aUV, aLayer);
    vColor = aColor.rgb;
    float e = aColor.a * 255.0;
    float grp = floor(e / 32.0 + 0.001);
    vEmis = (e - grp * 32.0) / 31.0 * uEmis[int(grp)];
    vShadow = uShadowMat * w;
    gl_Position = uViewProj * w;
}
)";

const char* FS_WORLD = R"(
uniform sampler2DArray uTex;
uniform sampler2DShadow uShadowMap;
uniform vec4 uMat[48];
uniform vec3 uCamPos;
uniform vec4 uLPos[NL];
uniform vec4 uLCol[NL];
uniform vec4 uLDir[NL];
uniform vec3 uFlashPos;
uniform vec3 uFlashDir;
uniform vec4 uFlashP;      // intensity, cosOuter, cosInner, range
uniform vec4 uGSPos;       // crane floodlight: xyz, radius
uniform vec4 uGSCol;       // rgb, cosOuter
uniform vec4 uGSDir;       // xyz, cosInner
uniform vec3 uAmbSky;
uniform vec3 uAmbGnd;
uniform vec4 uLightning;   // direction to light, intensity
uniform vec4 uFog;         // rgb, density
uniform vec4 uMisc;        // time, wetness, noise layer, exposure
uniform vec4 uMisc2;       // shadow texel, fullbright, 0, 0
in vec3 vWorld;
in vec3 vNormal;
in vec3 vUVL;
in vec3 vColor;
in float vEmis;
in vec4 vShadow;
out vec4 frag;

vec3 lightTerm(vec3 N, vec3 V, vec3 L, vec3 albedo, float spec, float gloss) {
    float nl = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float sp = pow(max(dot(N, H), 0.0), gloss) * spec * step(0.0001, nl);
    return albedo * nl + vec3(sp);
}

void main() {
    vec4 tex = texture(uTex, vUVL);
    vec4 mp = uMat[int(vUVL.z + 0.5)];
    vec3 N = normalize(vNormal);
    vec3 toCam = uCamPos - vWorld;
    float dist = length(toCam);
    vec3 V = toCam / max(dist, 0.0001);
    float alpha = 1.0;
    vec3 albedo = tex.rgb * vColor;
    float spec = mp.x, gloss = mp.y;
#ifdef CUTOUT
    float dth = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    if (tex.a < 0.25 + 0.5 * dth) discard;
    if (!gl_FrontFacing) N = -N;
#endif
#ifdef DECAL
    alpha = tex.a;
    if (alpha < 0.012) discard;
    spec *= alpha;
#endif
#ifdef GLASS
    if (!gl_FrontFacing) N = -N;
    float fres = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    alpha = clamp(tex.a + fres * 0.35, 0.0, 0.96);
#endif
#ifdef WATER
    {
        float nl = uMisc.z;
        vec2 wp = vWorld.xz * 0.06;
        vec3 n1 = texture(uTex, vec3(wp + vec2(uMisc.x * 0.011, uMisc.x * 0.007), nl)).rgb;
        vec3 n2 = texture(uTex, vec3(wp * 2.3 - vec2(uMisc.x * 0.017, -uMisc.x * 0.012), nl)).rgb;
        vec2 dn = (n1.rg + n2.gr - 1.0) * 0.55;
        N = normalize(vec3(dn.x, 1.0, dn.y));
        albedo = vec3(0.004, 0.009, 0.011);
        spec = 1.4; gloss = 70.0;
    }
#endif
#ifdef BUMP
#if !defined(CUTOUT) && !defined(DECAL) && !defined(GLASS) && !defined(WATER)
    {
        // height is stored in alpha: bump without tangents (Mikkelsen)
        vec3 dpx = dFdx(vWorld), dpy = dFdy(vWorld);
        float dhx = dFdx(tex.a), dhy = dFdy(tex.a);
        vec3 r1 = cross(dpy, N), r2 = cross(N, dpx);
        float det = dot(dpx, r1);
        vec3 grad = sign(det) * (dhx * r1 + dhy * r2);
        N = normalize(abs(det) * N - 0.03 * mp.w * grad);
    }
#endif
#endif
    // standing water on anything flat and outdoors
    float wetK = mp.z * uMisc.y * smoothstep(0.75, 0.95, N.y);
#ifndef WATER
    if (wetK > 0.01) {
        float nl = uMisc.z;
        float pud = smoothstep(0.50, 0.66, texture(uTex, vec3(vWorld.xz * 0.043, nl)).r);
        vec2 rip = texture(uTex, vec3(vWorld.xz * 0.7 + vec2(0.0, uMisc.x * 0.5), nl)).gb - 0.5;
        float k = pud * wetK;
        albedo *= mix(0.78, 0.42, k);
        spec = mix(spec * 1.6, 1.1, k);
        gloss = mix(gloss, 110.0, k);
        N = normalize(mix(N, vec3(rip.x * 0.16, 1.0, rip.y * 0.16), k * 0.9));
    }
#endif
    // ambient
    vec3 lit = albedo * mix(uAmbGnd, uAmbSky, N.y * 0.5 + 0.5);
    // local lights
    for (int i = 0; i < NL; i++) {
        vec3 L = uLPos[i].xyz - vWorld;
        float d2 = dot(L, L);
        float r = uLPos[i].w;
        float att = max(0.0, 1.0 - d2 / (r * r));
        if (att <= 0.0) continue;
        att *= att;
        L *= inversesqrt(max(d2, 0.0001));
        float spot = uLCol[i].w <= -1.0 ? 1.0 : smoothstep(uLCol[i].w, uLDir[i].w, dot(-L, uLDir[i].xyz));
        lit += uLCol[i].rgb * (att * spot) * lightTerm(N, V, L, albedo, spec, gloss);
    }
    // the crane's floodlight
    {
        vec3 L = uGSPos.xyz - vWorld;
        float d2 = dot(L, L);
        float att = max(0.0, 1.0 - d2 / (uGSPos.w * uGSPos.w));
        att *= att;
        L *= inversesqrt(max(d2, 0.0001));
        float spot = smoothstep(uGSCol.w, uGSDir.w, dot(-L, uGSDir.xyz));
        lit += uGSCol.rgb * (att * spot) * lightTerm(N, V, L, albedo, spec, gloss);
    }
    // flashlight, with its shadow
    float beamFog = 0.0;
    if (uFlashP.x > 0.001) {
        vec3 L = uFlashPos - vWorld;
        float df = length(L);
        L /= max(df, 0.0001);
        float cone = dot(-L, uFlashDir);
        float spot = smoothstep(uFlashP.y, uFlashP.z, cone);
        if (spot > 0.0) {
            float hot = 0.5 + 0.5 * smoothstep(0.955, 0.995, cone);
            float ring = 0.92 + 0.08 * sin(acos(clamp(cone, -1.0, 1.0)) * 90.0);
            float att = 1.0 / (1.0 + 0.09 * df + 0.02 * df * df) * clamp(1.0 - df / uFlashP.w, 0.0, 1.0);
            float sh = 1.0;
#ifdef SHADOWS
            vec3 sc = vShadow.xyz / vShadow.w * 0.5 + 0.5;
            if (vShadow.w > 0.0 && sc.x > 0.0 && sc.x < 1.0 && sc.y > 0.0 && sc.y < 1.0) {
                float bias = 0.0009 + 0.0014 * (1.0 - max(dot(N, L), 0.0));
                float tx = uMisc2.x;
                sh = 0.25 * (texture(uShadowMap, vec3(sc.xy + vec2(-tx, -tx), sc.z - bias)) + texture(uShadowMap, vec3(sc.xy + vec2(tx, -tx), sc.z - bias)) +
                             texture(uShadowMap, vec3(sc.xy + vec2(-tx, tx), sc.z - bias)) + texture(uShadowMap, vec3(sc.xy + vec2(tx, tx), sc.z - bias)));
            }
#endif
            lit += vec3(1.0, 0.93, 0.78) * (uFlashP.x * spot * hot * ring * att * sh) * lightTerm(N, V, L, albedo, spec, gloss);
        }
        // the beam in the wet air
        float vc = smoothstep(0.80, 0.985, dot(-V, uFlashDir));
        beamFog = vc * uFlashP.x * 0.022 * (1.0 - exp(-dist * 0.16));
    }
    // lightning
    if (uLightning.w > 0.001) {
        float nl = max(dot(N, uLightning.xyz), 0.0) * 0.85 + 0.15;
        lit += vec3(0.62, 0.70, 0.95) * uLightning.w * nl * albedo;
    }
#ifdef WATER
    {
        float fres = pow(1.0 - max(dot(N, V), 0.0), 4.0);
        lit += (uFog.rgb * 0.8 + vec3(0.25, 0.30, 0.42) * uLightning.w) * fres;
    }
#endif
    lit = mix(lit, albedo, uMisc2.y);
    // fog
    float f = 1.0 - exp(-dist * uFog.w);
    vec3 fogc = uFog.rgb + vec3(0.20, 0.24, 0.34) * uLightning.w + vec3(1.0, 0.93, 0.78) * beamFog;
    vec3 col = mix(lit, fogc * alpha, f);
    // things that glow cut through the fog further than things that are merely lit
    col += albedo * vEmis * 2.4 * exp(-dist * uFog.w * 0.3);
    // tone map + gamma
#ifdef DECAL
    vec3 c = col / max(alpha, 0.001);
    c = 1.0 - exp(-c * uMisc.w);
    frag = vec4(pow(c, vec3(0.4545)) * alpha, alpha);
#else
    col = 1.0 - exp(-col * uMisc.w);
    frag = vec4(pow(col, vec3(0.4545)), alpha);
#endif
}
)";

const char* VS_SHADOW = R"(
layout(location=0) in vec3 aPos;
layout(location=2) in float aLayer;
layout(location=3) in vec2 aUV;
uniform mat4 uViewProj;
uniform mat4 uModel;
out vec3 vUVL;
void main() {
    vUVL = vec3(aUV, aLayer);
    gl_Position = uViewProj * uModel * vec4(aPos, 1.0);
}
)";
const char* FS_SHADOW = R"(
uniform sampler2DArray uTex;
in vec3 vUVL;
void main() {
#ifdef CUTOUT
    if (texture(uTex, vUVL).a < 0.5) discard;
#endif
}
)";

const char* VS_FULL = R"(
out vec2 vUV;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 1.0, 1.0);
}
)";

const char* FS_SKY = R"(
uniform sampler2DArray uTex;
uniform vec3 uRight, uUp, uFwd;
uniform vec4 uParams;      // tanX, tanY, time, noise layer
uniform vec4 uFog;         // rgb, exposure
uniform vec4 uLightning;   // dir, intensity
uniform vec4 uDawn;        // rgb amount
in vec2 vUV;
out vec4 frag;
void main() {
    vec2 ndc = vUV * 2.0 - 1.0;
    vec3 d = normalize(uFwd + uRight * ndc.x * uParams.x + uUp * ndc.y * uParams.y);
    float up = max(d.y, 0.0);
    // cloud deck
    vec2 cp = d.xz / (d.y + 0.22) * 0.09;
    float t = uParams.z;
    float c1 = texture(uTex, vec3(cp + vec2(t * 0.004, t * 0.0015), uParams.w)).r;
    float c2 = texture(uTex, vec3(cp * 2.7 - vec2(t * 0.006, 0.0), uParams.w)).g;
    float cloud = smoothstep(0.35, 0.8, c1 * 0.7 + c2 * 0.3);
    vec3 zen = vec3(0.004, 0.006, 0.011);
    vec3 sky = mix(uFog.rgb, zen + vec3(0.012, 0.014, 0.02) * cloud, smoothstep(0.0, 0.55, up));
    // lightning lights the clouds from inside
    float lg = pow(max(dot(d, uLightning.xyz), 0.0), 3.0);
    sky += vec3(0.42, 0.5, 0.75) * uLightning.w * (0.25 + cloud * (0.35 + 1.4 * lg)) * smoothstep(-0.05, 0.3, d.y + 0.1);
    sky += uDawn.rgb * (0.35 + 0.65 * pow(max(dot(d, normalize(vec3(0.9, 0.12, 0.3))), 0.0), 2.0)) * (0.5 + 0.5 * cloud);
    if (d.y < 0.0) sky = uFog.rgb + vec3(0.20, 0.24, 0.34) * uLightning.w + uDawn.rgb * 0.35;
    sky = 1.0 - exp(-sky * uFog.w);
    frag = vec4(pow(sky, vec3(0.4545)), 1.0);
}
)";

const char* VS_RAIN = R"(
layout(location=0) in vec3 aSeed;
layout(location=1) in vec2 aCorner;
uniform mat4 uViewProj;
uniform vec3 uCamPos;
uniform vec4 uParams;      // time, wind x, wind z, intensity
uniform sampler2D uRainMap;
uniform vec4 uRainOrg;     // x0, z0, 1/size, 0
uniform vec3 uFlashPos, uFlashDir;
uniform vec4 uFlashP;
uniform vec4 uLPos[4];
uniform vec4 uLCol[4];
uniform vec4 uGSPos, uGSCol, uGSDir;
uniform vec3 uAmb;
uniform vec4 uRainHole;    // no rain inside this sphere (the crane cab, which is not in the roof map)
out vec4 vCol;
void main() {
    vec3 box = vec3(26.0, 16.0, 26.0);
    float speed = 11.0 + aSeed.x * 5.0;
    vec3 p = aSeed * box;
    p.y -= uParams.x * speed;
    p.xz += uParams.yz * uParams.x * 2.2;
    p = mod(p - uCamPos, box) - box * 0.5 + uCamPos;
    // under a roof? then this drop does not exist
    float roof = texture(uRainMap, (p.xz - uRainOrg.xy) * uRainOrg.z).r * 255.0 * 0.25;
    vec3 fall = normalize(vec3(uParams.y * 0.22, -1.0, uParams.z * 0.22));
    float len = 0.42 + aSeed.z * 0.3;
    vec3 toCam = normalize(uCamPos - p);
    vec3 side = normalize(cross(fall, toCam));
    vec3 wp = p + fall * (aCorner.y * len) + side * (aCorner.x * 0.0055);
    // light picked up on the way down
    vec3 l = uAmb;
    for (int i = 0; i < 4; i++) {
        vec3 L = uLPos[i].xyz - p;
        float att = max(0.0, 1.0 - dot(L, L) / (uLPos[i].w * uLPos[i].w));
        l += uLCol[i].rgb * att * att * 0.5;
    }
    {
        vec3 L = uGSPos.xyz - p;
        float d2 = dot(L, L);
        float att = max(0.0, 1.0 - d2 / (uGSPos.w * uGSPos.w));
        float spot = smoothstep(uGSCol.w, uGSDir.w, dot(-normalize(L), uGSDir.xyz));
        l += uGSCol.rgb * att * att * spot * 0.6;
    }
    if (uFlashP.x > 0.001) {
        vec3 L = uFlashPos - p;
        float df = length(L);
        float spot = smoothstep(uFlashP.y, uFlashP.z, dot(-L / df, uFlashDir));
        l += vec3(1.0, 0.93, 0.78) * uFlashP.x * spot / (1.0 + 0.1 * df + 0.03 * df * df) * 0.9;
    }
    float d = length(p - uCamPos);
    float a = uParams.w * (1.0 - smoothstep(9.0, 13.0, d)) * smoothstep(0.9, 2.6, d);
    if (p.y < roof) a = 0.0;
    if (uRainHole.w > 0.0 && distance(p, uRainHole.xyz) < uRainHole.w) a = 0.0;
    vCol = vec4(l * a, a);
    gl_Position = a > 0.0 ? uViewProj * vec4(wp, 1.0) : vec4(2.0, 2.0, 2.0, 1.0);
}
)";
const char* FS_RAIN = R"(
in vec4 vCol;
out vec4 frag;
void main() { frag = vec4(vCol.rgb, 0.0); }
)";

const char* VS_HALO = R"(
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aCorner;
layout(location=2) in vec4 aColSize;
uniform mat4 uViewProj;
uniform vec3 uRight, uUp;
out vec2 vC;
out vec3 vCol;
void main() {
    vC = aCorner;
    vCol = aColSize.rgb;
    vec3 wp = aPos + (uRight * aCorner.x + uUp * aCorner.y) * aColSize.w;
    gl_Position = uViewProj * vec4(wp, 1.0);
}
)";
const char* FS_HALO = R"(
in vec2 vC;
in vec3 vCol;
out vec4 frag;
void main() {
    float r = length(vC);
    float a = max(0.0, 1.0 - r);
    a = a * a * a + 0.6 * pow(a, 12.0);
    frag = vec4(vCol * a, 0.0);
}
)";

const char* FS_POST = R"(
uniform sampler2D uScene;
uniform vec4 uP0;   // time, fear, interference, fade
uniform vec4 uP1;   // flash, damage, grain, 1/height
uniform vec4 uP2;   // dawn, hide, dead, aspect
in vec2 vUV;
out vec4 frag;
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
    vec2 uv = vUV;
    float t = uP0.x, fear = uP0.y, inter = uP0.z;
    // tape tracking errors when it is close
    if (inter > 0.01) {
        float band = floor(uv.y * 36.0 + t * 7.0);
        float n = hash(vec2(band, floor(t * 13.0)));
        float tear = step(1.0 - inter * 0.55, n) * (hash(vec2(band, 3.0)) - 0.5) * 0.09 * inter;
        uv.x += tear + sin(uv.y * 310.0 + t * 40.0) * 0.0015 * inter;
        float roll = step(0.992 - inter * 0.02, hash(vec2(floor(t * 5.0), 9.0)));
        uv.y = fract(uv.y + roll * 0.08 * inter);
    }
    vec2 c = uv - 0.5;
    float r2 = dot(c, c);
    // lens: slight barrel + colour fringing that grows with fear
    vec2 duv = uv + c * r2 * 0.045;
    float ca = 0.0016 + fear * 0.0035 + inter * 0.006;
    vec3 col;
    col.r = texture(uScene, duv + c * ca).r;
    col.g = texture(uScene, duv).g;
    col.b = texture(uScene, duv - c * ca).b;
    // cheap glow: brighten around hot pixels
    vec3 bl = texture(uScene, duv + vec2(0.004, 0.0)).rgb + texture(uScene, duv - vec2(0.004, 0.0)).rgb + texture(uScene, duv + vec2(0.0, 0.006)).rgb +
              texture(uScene, duv - vec2(0.0, 0.006)).rgb;
    col += max(bl * 0.25 - 0.55, 0.0) * 0.5;
    // grade: cold shadows, slightly crushed
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(lum), col, 0.82 - fear * 0.25);
    col *= vec3(0.96, 1.0, 1.04);
    col = mix(col, col * vec3(1.12, 1.03, 0.9), uP2.x);
    col = pow(col, vec3(1.06));
    // scanlines + grain
    col *= 0.965 + 0.035 * sin(uv.y / uP1.w * 3.14159);
    float g = hash(uv * vec2(917.0, 613.0) + fract(t) * 37.0) - 0.5;
    col += g * (uP1.z + inter * 0.2) * (1.0 - lum * 0.5);
    if (inter > 0.01) col = mix(col, vec3(hash(uv * 311.0 + t)), inter * inter * 0.35 * step(0.5, hash(vec2(floor(uv.y * 90.0), floor(t * 24.0)))));
    // vignette, pulsing a little with the heart
    float vig = 1.0 - r2 * (1.35 + fear * 1.3 + 0.25 * fear * sin(t * 7.0));
    col *= clamp(vig, 0.0, 1.0);
    // hiding in a locker: looking out through the louvres
    if (uP2.y > 0.001) {
        float slat = smoothstep(0.30, 0.42, abs(fract(uv.y * 7.0) - 0.5));
        float open = smoothstep(0.28, 0.72, uv.y) * smoothstep(0.98, 0.9, uv.y);
        col *= mix(1.0, slat * open * 0.9 + 0.02, uP2.y);
    }
    col = mix(col, vec3(0.5, 0.02, 0.0) * (lum + 0.2), clamp(uP1.y, 0.0, 1.0) * clamp(r2 * 3.0, 0.0, 1.0));
    col += uP1.x;
    col *= 1.0 - uP0.w;
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
enum { V_OPAQUE = 0, V_CUTOUT, V_DECAL, V_GLASS, V_WATER, V_COUNT };
Program g_world[V_COUNT];
Program g_shadow[2];
Program g_sky, g_rain, g_halo, g_post;
GLuint g_texArray = 0, g_rainTex = 0;
GLuint g_vaoStatic = 0, g_vboStatic = 0, g_iboStatic = 0;
GLuint g_vaoDyn = 0, g_vboDyn = 0, g_iboDyn = 0;
GLuint g_vaoWater = 0, g_vboWater = 0, g_iboWater = 0;
GLuint g_vaoRain = 0, g_vboRain = 0, g_iboRain = 0;
GLuint g_vaoHalo = 0, g_vboHalo = 0;
GLuint g_vaoEmpty = 0;
GLuint g_sceneFbo = 0, g_sceneTex = 0, g_sceneDepth = 0;
GLuint g_shadowFbo = 0, g_shadowTex = 0;
int g_sceneW = 0, g_sceneH = 0, g_shadowSize = 0;
int g_quality = -1;
int g_debugLayer = -1;
float g_fullbright = 0;
const int RAIN_N = 2200;
Vec3 g_flashDir(0, 0, -1);
bool g_flashDirInit = false;
RenderStats g_stats;
float g_matParams[48 * 4];

struct Plane { Vec3 n; float d; };
struct Frustum {
    Plane p[6];
    bool visible(const AABB& b) const {
        for (int i = 0; i < 6; i++) {
            Vec3 v(p[i].n.x > 0 ? b.mx.x : b.mn.x, p[i].n.y > 0 ? b.mx.y : b.mn.y, p[i].n.z > 0 ? b.mx.z : b.mn.z);
            if (dot(p[i].n, v) + p[i].d < 0) return false;
        }
        return true;
    }
};
Frustum make_frustum(const Mat4& m) {
    Frustum f;
    auto row = [&](int r) { return Vec4(m.m[r], m.m[4 + r], m.m[8 + r], m.m[12 + r]); };
    Vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
    Vec4 pl[6] = {Vec4(r3.x + r0.x, r3.y + r0.y, r3.z + r0.z, r3.w + r0.w), Vec4(r3.x - r0.x, r3.y - r0.y, r3.z - r0.z, r3.w - r0.w),
                  Vec4(r3.x + r1.x, r3.y + r1.y, r3.z + r1.z, r3.w + r1.w), Vec4(r3.x - r1.x, r3.y - r1.y, r3.z - r1.z, r3.w - r1.w),
                  Vec4(r3.x + r2.x, r3.y + r2.y, r3.z + r2.z, r3.w + r2.w), Vec4(r3.x - r2.x, r3.y - r2.y, r3.z - r2.z, r3.w - r2.w)};
    for (int i = 0; i < 6; i++) {
        float l = std::sqrt(pl[i].x * pl[i].x + pl[i].y * pl[i].y + pl[i].z * pl[i].z);
        f.p[i].n = Vec3(pl[i].x / l, pl[i].y / l, pl[i].z / l);
        f.p[i].d = pl[i].w / l;
    }
    return f;
}

void setup_vtx_attribs() {
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_BYTE, GL_TRUE, sizeof(Vtx), (void*)12);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(Vtx), (void*)15);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)16);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), (void*)24);
}

void build_programs(int quality) {
    for (int i = 0; i < V_COUNT; i++)
        if (g_world[i].id) { glDeleteProgram(g_world[i].id); g_world[i] = Program(); }
    std::string base = quality == 0 ? "#define NL 4\n" : "#define NL 6\n#define BUMP\n";
    if (quality >= 1) base += "#define SHADOWS\n";
    static const char* var[V_COUNT] = {"", "#define CUTOUT\n", "#define DECAL\n", "#define GLASS\n", "#define WATER\n"};
    static const char* names[V_COUNT] = {"world", "cutout", "decal", "glass", "water"};
    for (int i = 0; i < V_COUNT; i++) {
        g_world[i] = gfx_program(VS_WORLD, FS_WORLD, (base + var[i]).c_str(), names[i]);
        g_world[i].use();
        g_world[i].i1("uTex", 0);
        g_world[i].i1("uShadowMap", 1);
    }
}

void make_scene_target(int w, int h) {
    if (w == g_sceneW && h == g_sceneH && g_sceneFbo) return;
    g_sceneW = w; g_sceneH = h;
    if (!g_sceneFbo) { glGenFramebuffers(1, &g_sceneFbo); glGenTextures(1, &g_sceneTex); glGenRenderbuffers(1, &g_sceneDepth); }
    glBindTexture(GL_TEXTURE_2D, g_sceneTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindRenderbuffer(GL_RENDERBUFFER, g_sceneDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, g_sceneFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_sceneTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_sceneDepth);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void make_shadow_target(int size) {
    if (size == g_shadowSize && g_shadowFbo) return;
    g_shadowSize = size;
    if (!g_shadowFbo) { glGenFramebuffers(1, &g_shadowFbo); glGenTextures(1, &g_shadowTex); }
    glBindTexture(GL_TEXTURE_2D, g_shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindFramebuffer(GL_FRAMEBUFFER, g_shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_shadowTex, 0);
    GLenum none = GL_NONE;
    glDrawBuffers(1, &none);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---- light selection ----------------------------------------------------------
struct LightSet { float pos[24], col[24], dir[24]; };

void write_light(LightSet& ls, int slot, const Light* l) {
    float* p = &ls.pos[slot * 4];
    float* c = &ls.col[slot * 4];
    float* d = &ls.dir[slot * 4];
    if (!l) {
        p[0] = p[1] = p[2] = 0; p[3] = 1.0f;
        c[0] = c[1] = c[2] = 0; c[3] = -2.0f;
        d[0] = 0; d[1] = -1; d[2] = 0; d[3] = -2.0f;
        return;
    }
    p[0] = l->pos.x; p[1] = l->pos.y; p[2] = l->pos.z; p[3] = l->radius;
    c[0] = l->color.x * l->cur; c[1] = l->color.y * l->cur; c[2] = l->color.z * l->cur; c[3] = l->cosOuter;
    d[0] = l->dir.x; d[1] = l->dir.y; d[2] = l->dir.z; d[3] = l->cosInner;
}

struct Pick { float score; int idx; };
int g_moving[5];
int g_movingN = 0;
bool is_moving(int li) {
    for (int i = 0; i < g_movingN; i++)
        if (g_moving[i] == li) return true;
    return false;
}

void pick_lights(const World& w, const u16* cand, int ncand, const AABB& box, int flood, int nl, LightSet& out) {
    Pick best[6];
    int n = 0;
    auto consider = [&](int li) {
        if (li == flood) return;
        const Light& l = w.lights[(size_t)li];
        if (l.cur <= 0.004f) return;
        Vec3 q = vmax(box.mn, vmin(l.pos, box.mx));
        float d = length(q - l.pos);
        if (d >= l.radius) return;
        float s = (l.color.x + l.color.y + l.color.z) * l.cur * (1.0f - d / l.radius);
        if (n < nl) { best[n++] = {s, li}; }
        else {
            int lo = 0;
            for (int i = 1; i < nl; i++)
                if (best[i].score < best[lo].score) lo = i;
            if (s > best[lo].score) best[lo] = {s, li};
        }
    };
    if (cand) {
        for (int i = 0; i < ncand; i++)
            if (!is_moving(cand[i])) consider(cand[i]);
    } else {
        for (int i = 0; i < (int)w.lights.size(); i++)
            if (!is_moving(i)) consider(i);
    }
    for (int i = 0; i < g_movingN; i++) consider(g_moving[i]);
    for (int i = 0; i < 6; i++) write_light(out, i, i < n ? &w.lights[(size_t)best[i].idx] : nullptr);
}

void set_lights(Program& p, const LightSet& ls, int nl) {
    glUniform4fv(p.loc("uLPos"), nl, ls.pos);
    glUniform4fv(p.loc("uLCol"), nl, ls.col);
    glUniform4fv(p.loc("uLDir"), nl, ls.dir);
}

} // namespace

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void render_init() {
    glGenVertexArrays(1, &g_vaoEmpty);
    g_shadow[0] = gfx_program(VS_SHADOW, FS_SHADOW, "", "shadow");
    g_shadow[1] = gfx_program(VS_SHADOW, FS_SHADOW, "#define CUTOUT\n", "shadow-cutout");
    g_sky = gfx_program(VS_FULL, FS_SKY, "", "sky");
    g_rain = gfx_program(VS_RAIN, FS_RAIN, "", "rain");
    g_halo = gfx_program(VS_HALO, FS_HALO, "", "halo");
    g_post = gfx_program(VS_FULL, FS_POST, "", "post");
    // texture array
    glGenTextures(1, &g_texArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_texArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 10, GL_SRGB8_ALPHA8, TEX_SIZE, TEX_SIZE, M_COUNT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    for (int i = 0; i < 48; i++) {
        const MatInfo& m = g_matInfo[i < M_COUNT ? i : 0];
        g_matParams[i * 4] = m.spec; g_matParams[i * 4 + 1] = m.gloss; g_matParams[i * 4 + 2] = m.wet; g_matParams[i * 4 + 3] = m.bump;
    }
    // rain particles
    {
        std::vector<float> v;
        std::vector<u16> idx;
        Rng r(777);
        for (int i = 0; i < RAIN_N; i++) {
            float sx = r.f(), sy = r.f(), sz = r.f();
            static const float c[4][2] = {{-1, 0}, {1, 0}, {1, 1}, {-1, 1}};
            for (int k = 0; k < 4; k++) { v.push_back(sx); v.push_back(sy); v.push_back(sz); v.push_back(c[k][0]); v.push_back(c[k][1]); }
            u16 b = (u16)(i * 4);
            idx.push_back(b); idx.push_back((u16)(b + 1)); idx.push_back((u16)(b + 2)); idx.push_back(b); idx.push_back((u16)(b + 2)); idx.push_back((u16)(b + 3));
        }
        glGenVertexArrays(1, &g_vaoRain);
        glBindVertexArray(g_vaoRain);
        glGenBuffers(1, &g_vboRain);
        glBindBuffer(GL_ARRAY_BUFFER, g_vboRain);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(float)), v.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &g_iboRain);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_iboRain);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * sizeof(u16)), idx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 20, (void*)12);
    }
    // halos (dynamic)
    glGenVertexArrays(1, &g_vaoHalo);
    glBindVertexArray(g_vaoHalo);
    glGenBuffers(1, &g_vboHalo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vboHalo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 36, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 36, (void*)12);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 36, (void*)20);
    // water sheet (tessellated so the fog and lights have vertices to work with)
    {
        std::vector<Vtx> v;
        std::vector<u32> idx;
        const float x0 = -420, x1 = 420, z0 = -420, z1 = QUAY_Z + 0.05f;
        const int nx = 28, nz = 16;
        for (int j = 0; j <= nz; j++)
            for (int i = 0; i <= nx; i++) {
                Vtx t;
                float fz = (float)j / nz;
                fz = fz * fz; // denser near the quay
                t.x = lerpf(x0, x1, (float)i / nx); t.y = WATER_Y; t.z = lerpf(z1, z0, fz);
                t.nx = 0; t.ny = 127; t.nz = 0; t.layer = (u8)M_NOISE;
                t.u = t.x * 0.05f; t.v = t.z * 0.05f;
                t.r = t.g = t.b = 255; t.a = 0;
                v.push_back(t);
            }
        for (int j = 0; j < nz; j++)
            for (int i = 0; i < nx; i++) {
                u32 a = (u32)(j * (nx + 1) + i), b = a + 1, c = a + (u32)(nx + 1), d = c + 1;
                idx.push_back(a); idx.push_back(b); idx.push_back(d); idx.push_back(a); idx.push_back(d); idx.push_back(c);
            }
        glGenVertexArrays(1, &g_vaoWater);
        glBindVertexArray(g_vaoWater);
        glGenBuffers(1, &g_vboWater);
        glBindBuffer(GL_ARRAY_BUFFER, g_vboWater);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(Vtx)), v.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &g_iboWater);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_iboWater);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * sizeof(u32)), idx.data(), GL_STATIC_DRAW);
        setup_vtx_attribs();
        g_stats.waterIndices = (int)idx.size();
    }
    glBindVertexArray(0);
}

void render_upload_layer(int layer, const u8* rgba) {
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_texArray);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, TEX_SIZE, TEX_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

void render_finish_textures(float anisotropy) {
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_texArray);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    if (anisotropy > 1.0f) glTexParameterf(GL_TEXTURE_2D_ARRAY, 0x84FE /* TEXTURE_MAX_ANISOTROPY_EXT */, anisotropy);
}

void render_upload_world(const World& w) {
    glGenVertexArrays(1, &g_vaoStatic);
    glBindVertexArray(g_vaoStatic);
    glGenBuffers(1, &g_vboStatic);
    glBindBuffer(GL_ARRAY_BUFFER, g_vboStatic);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(w.staticMesh.v.size() * sizeof(Vtx)), w.staticMesh.v.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &g_iboStatic);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_iboStatic);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(w.staticMesh.idx.size() * sizeof(u32)), w.staticMesh.idx.data(), GL_STATIC_DRAW);
    setup_vtx_attribs();
    glGenVertexArrays(1, &g_vaoDyn);
    glBindVertexArray(g_vaoDyn);
    glGenBuffers(1, &g_vboDyn);
    glBindBuffer(GL_ARRAY_BUFFER, g_vboDyn);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(w.dynMesh.v.size() * sizeof(Vtx)), w.dynMesh.v.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &g_iboDyn);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_iboDyn);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(w.dynMesh.idx.size() * sizeof(u32)), w.dynMesh.idx.data(), GL_STATIC_DRAW);
    setup_vtx_attribs();
    glBindVertexArray(0);
    // rain occlusion map
    glGenTextures(1, &g_rainTex);
    glBindTexture(GL_TEXTURE_2D, g_rainTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, World::RAIN_N, World::RAIN_N, 0, GL_RED, GL_UNSIGNED_BYTE, w.rainMap.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_moving[0] = w.lightCab;
    g_moving[1] = w.lightBeacon[0];
    g_moving[2] = w.lightBeacon[1];
    g_moving[3] = w.lightBeacon[2];
    g_moving[4] = w.lightSwing;
    g_movingN = 5;
}

void render_set_debug(int layer, float fullbright) { g_debugLayer = layer; g_fullbright = fullbright; }
const RenderStats& render_stats() { return g_stats; }

void render_frame(Game& g, int fbW, int fbH, const RenderSettings& rs, float dt) {
    World& w = g.w;
    g_stats.drawCalls = 0;
    g_stats.chunksDrawn = 0;
    if (rs.quality != g_quality) { g_quality = rs.quality; build_programs(g_quality); }
    int nl = g_quality == 0 ? 4 : 6;
    bool shadowsOn = g_quality >= 1;

    if (g.appMode == AM_ENDING || !g_vaoStatic) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }
    // --- targets ---
    float scale = clampv(rs.renderScale, 0.35f, 1.0f);
    int sw = std::max(64, (int)(fbW * scale)), sh = std::max(64, (int)(fbH * scale));
    int maxDim = g_quality == 2 ? 2200 : (g_quality == 1 ? 1600 : 1100);
    if (std::max(sw, sh) > maxDim) {
        float k = (float)maxDim / (float)std::max(sw, sh);
        sw = (int)(sw * k); sh = (int)(sh * k);
    }
    make_scene_target(sw, sh);
    if (shadowsOn) make_shadow_target(g_quality == 2 ? 1536 : 1024);

    // --- camera ---
    float aspect = (float)fbW / (float)fbH;
    g.aspect = aspect;
    // keep the horizontal field of view sensible on tall (portrait) screens
    float fovy = g.camFov;
    if (aspect < 1.2f) fovy = 2.0f * std::atan(std::tan(g.camFov * 0.5f) * 1.2f / aspect);
    fovy = std::min(fovy, 2.2f);
    Mat4 proj = Mat4::perspective(fovy, aspect, 0.06f, 520.0f);
    Mat4 view = game_view_matrix(g);
    Mat4 vp = proj * view;
    Frustum fr = make_frustum(vp);
    Vec3 fwd = game_cam_forward(g);
    Vec3 right = normalize(cross(fwd, Vec3(0, 1, 0)));
    Vec3 up = cross(right, fwd);
    if (g.camRoll != 0.0f) {
        float cs = std::cos(g.camRoll), sn = std::sin(g.camRoll);
        Vec3 r2 = right * cs - up * sn, u2 = up * cs + right * sn;
        right = r2; up = u2;
    }
    float t = (float)g.time;

    // --- flashlight ---
    bool flashOn = g.flashOn && g.have[IT_FLASHLIGHT] && g.pstate != PS_HIDING && g.appMode == AM_PLAY;
    float flashI = flashOn ? 3.4f * g.flashFlicker : 0.0f;
    if (!g_flashDirInit) { g_flashDir = fwd; g_flashDirInit = true; }
    g_flashDir = normalize(lerp(g_flashDir, fwd, 1.0f - std::exp(-dt * 16.0f)));
    Vec3 flashPos = g.camPos + right * 0.2f - up * 0.16f + fwd * 0.08f;
    {
        Vec3 d = flashPos - g.camPos;
        float len = length(d);
        float hit = w.raycast(g.camPos, d * (1.0f / len), len + 0.08f, COL_SIGHT);
        if (hit < len + 0.08f) flashPos = g.camPos + d * (std::max(0.0f, hit - 0.08f) / len);
    }
    const float flashOuter = 0.52f, flashRange = 34.0f;
    Mat4 shadowVP = Mat4::identity();
    if (flashI > 0.001f && shadowsOn) {
        Vec3 fu = std::fabs(g_flashDir.y) > 0.98f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
        Mat4 lv = Mat4::lookAt(flashPos, flashPos + g_flashDir, fu);
        Mat4 lp = Mat4::perspective(flashOuter * 2.0f + 0.1f, 1.0f, 0.12f, flashRange);
        shadowVP = lp * lv;
    }

    // --- environment colours ---
    float dawn = g.dawn;
    Vec3 fogCol = lerp(Vec3(0.019f, 0.026f, 0.036f), Vec3(0.30f, 0.30f, 0.31f), dawn);
    float fogDen = lerpf(0.0205f, 0.006f, g.indoor) * (1.0f - 0.25f * dawn);
    if (g.appMode == AM_TITLE) fogDen = 0.016f;
    Vec3 ambSky = lerp(Vec3(0.016f, 0.021f, 0.030f), Vec3(0.26f, 0.26f, 0.27f), dawn) * (1.0f - 0.45f * g.indoor);
    Vec3 ambGnd = lerp(Vec3(0.007f, 0.007f, 0.008f), Vec3(0.10f, 0.095f, 0.09f), dawn) * (1.0f - 0.3f * g.indoor);
    Vec3 lightDir = normalize(Vec3(0.3f, 0.82f, -0.5f));
    float lightning = g.lightning * (1.0f - 0.6f * g.indoor);
    const float exposure = 1.7f * clampv(rs.brightness, 0.5f, 2.0f);
    const Light& flood = w.lights[(size_t)w.lightFlood];
    float floodCur = flood.cur;

    auto set_common = [&](Program& p, const Mat4& viewProj) {
        p.use();
        p.m4("uViewProj", viewProj);
        p.m4("uShadowMat", shadowVP);
        p.v3("uCamPos", g.camPos);
        glUniform4fv(p.loc("uMat"), 48, g_matParams);
        glUniform1fv(p.loc("uEmis"), 8, g.emis);
        p.v3("uFlashPos", flashPos);
        p.v3("uFlashDir", g_flashDir);
        p.f4("uFlashP", flashI, std::cos(flashOuter), std::cos(flashOuter * 0.55f), flashRange);
        p.f4("uGSPos", flood.pos.x, flood.pos.y, flood.pos.z, flood.radius);
        p.f4("uGSCol", flood.color.x * floodCur, flood.color.y * floodCur, flood.color.z * floodCur, flood.cosOuter);
        p.f4("uGSDir", flood.dir.x, flood.dir.y, flood.dir.z, flood.cosInner);
        p.v3("uAmbSky", ambSky);
        p.v3("uAmbGnd", ambGnd);
        p.f4("uLightning", lightDir.x, lightDir.y, lightDir.z, lightning);
        p.f4("uFog", fogCol.x, fogCol.y, fogCol.z, fogDen);
        p.f4("uMisc", t, g.rain, (float)M_NOISE, exposure);
        p.f4("uMisc2", 0.7f / (float)std::max(1, g_shadowSize), g_fullbright, 0, 0);
    };

    // --- visibility + light lists ---
    static std::vector<int> visChunks;
    static std::vector<LightSet> chunkLights;
    visChunks.clear();
    for (size_t i = 0; i < w.chunks.size(); i++)
        if (fr.visible(w.chunks[i].box)) visChunks.push_back((int)i);
    chunkLights.resize(visChunks.size());
    for (size_t i = 0; i < visChunks.size(); i++) {
        const Chunk& ch = w.chunks[(size_t)visChunks[i]];
        pick_lights(w, ch.lights.data(), (int)ch.lights.size(), ch.box, w.lightFlood, nl, chunkLights[i]);
    }
    struct DynVis { int idx; AABB box; LightSet ls; float nm[9]; };
    static std::vector<DynVis> visDyn;
    visDyn.clear();
    for (size_t i = 0; i < w.dyn.size(); i++) {
        const DynObj& d = w.dyn[i];
        if (!d.visible || d.mesh < 0) continue;
        const DynMesh& dm = w.dynMeshes[(size_t)d.mesh];
        Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
        for (int k = 0; k < 8; k++) {
            Vec3 p = d.xf.point(Vec3((k & 1) ? dm.box.mx.x : dm.box.mn.x, (k & 2) ? dm.box.mx.y : dm.box.mn.y, (k & 4) ? dm.box.mx.z : dm.box.mn.z));
            lo = vmin(lo, p); hi = vmax(hi, p);
        }
        AABB wb(lo, hi);
        if (!fr.visible(wb)) continue;
        visDyn.emplace_back();
        DynVis& dv = visDyn.back();
        dv.idx = (int)i;
        dv.box = wb;
        // big objects (the crane's superstructure) would otherwise collect every light in the yard
        AABB lb = wb;
        Vec3 ext = wb.mx - wb.mn;
        if (ext.x + ext.z > 30.0f) lb = AABB(g.camPos - Vec3(6, 6, 6), g.camPos + Vec3(6, 6, 6));
        pick_lights(w, nullptr, 0, lb, w.lightFlood, nl, dv.ls);
        gfx_normal_matrix(d.xf, dv.nm);
    }
    static const float IDENT3[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    Mat4 ident = Mat4::identity();

    // --- shadow pass ---
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (flashI > 0.001f && shadowsOn) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_shadowFbo);
        glViewport(0, 0, g_shadowSize, g_shadowSize);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        Frustum lf = make_frustum(shadowVP);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, g_texArray);
        for (int pass = 0; pass < 2; pass++) {
            Program& sp = g_shadow[pass];
            sp.use();
            sp.m4("uViewProj", shadowVP);
            sp.i1("uTex", 0);
            if (pass == 1) glDisable(GL_CULL_FACE);
            sp.m4("uModel", ident);
            glBindVertexArray(g_vaoStatic);
            for (size_t i = 0; i < w.chunks.size(); i++) {
                const Chunk& ch = w.chunks[i];
                if (!ch.count[pass] || !lf.visible(ch.box)) continue;
                glDrawElements(GL_TRIANGLES, (GLsizei)ch.count[pass], GL_UNSIGNED_INT, (void*)(size_t)(ch.first[pass] * 4));
                g_stats.drawCalls++;
            }
            glBindVertexArray(g_vaoDyn);
            for (size_t i = 0; i < w.dyn.size(); i++) {
                const DynObj& d = w.dyn[i];
                if (!d.visible || d.mesh < 0) continue;
                const DynMesh& dm = w.dynMeshes[(size_t)d.mesh];
                if (!dm.count[pass]) continue;
                Vec3 c = d.xf.point(dm.box.center());
                float rad = length(dm.box.half()) * 1.2f;
                if (length(c - flashPos) > flashRange + rad) continue;
                sp.m4("uModel", d.xf);
                glDrawElements(GL_TRIANGLES, (GLsizei)dm.count[pass], GL_UNSIGNED_INT, (void*)(size_t)(dm.first[pass] * 4));
                g_stats.drawCalls++;
            }
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // --- main pass ---
    glBindFramebuffer(GL_FRAMEBUFFER, g_sceneFbo);
    glViewport(0, 0, g_sceneW, g_sceneH);
    glClearColor(fogCol.x, fogCol.y, fogCol.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_shadowTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_texArray);

    auto draw_pass = [&](int pass, int variant) {
        Program& p = g_world[variant];
        set_common(p, vp);
        p.m4("uModel", ident);
        p.m3("uNormalMat", IDENT3);
        glBindVertexArray(g_vaoStatic);
        for (size_t i = 0; i < visChunks.size(); i++) {
            const Chunk& ch = w.chunks[(size_t)visChunks[i]];
            if (!ch.count[pass]) continue;
            set_lights(p, chunkLights[i], nl);
            glDrawElements(GL_TRIANGLES, (GLsizei)ch.count[pass], GL_UNSIGNED_INT, (void*)(size_t)(ch.first[pass] * 4));
            g_stats.drawCalls++;
            if (pass == PASS_OPAQUE) g_stats.chunksDrawn++;
        }
        glBindVertexArray(g_vaoDyn);
        for (const DynVis& dv : visDyn) {
            const DynObj& d = w.dyn[(size_t)dv.idx];
            const DynMesh& dm = w.dynMeshes[(size_t)d.mesh];
            if (!dm.count[pass]) continue;
            p.m4("uModel", d.xf);
            p.m3("uNormalMat", dv.nm);
            set_lights(p, dv.ls, nl);
            glDrawElements(GL_TRIANGLES, (GLsizei)dm.count[pass], GL_UNSIGNED_INT, (void*)(size_t)(dm.first[pass] * 4));
            g_stats.drawCalls++;
        }
    };
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    draw_pass(PASS_OPAQUE, V_OPAQUE);
    glDisable(GL_CULL_FACE);
    draw_pass(PASS_CUTOUT, V_CUTOUT);

    // water
    {
        Program& p = g_world[V_WATER];
        set_common(p, vp);
        p.m4("uModel", ident);
        p.m3("uNormalMat", IDENT3);
        LightSet ls;
        AABB wb(Vec3(g.camPos.x - 30, WATER_Y, QUAY_Z - 40), Vec3(g.camPos.x + 30, WATER_Y, QUAY_Z));
        pick_lights(w, nullptr, 0, wb, w.lightFlood, nl, ls);
        set_lights(p, ls, nl);
        glBindVertexArray(g_vaoWater);
        glDrawElements(GL_TRIANGLES, g_stats.waterIndices, GL_UNSIGNED_INT, nullptr);
        g_stats.drawCalls++;
    }
    // sky (drawn at the far plane, only where nothing else was)
    {
        g_sky.use();
        g_sky.i1("uTex", 0);
        g_sky.v3("uRight", right);
        g_sky.v3("uUp", up);
        g_sky.v3("uFwd", fwd);
        float ty = std::tan(fovy * 0.5f);
        g_sky.f4("uParams", ty * aspect, ty, t, (float)M_NOISE);
        g_sky.f4("uFog", fogCol.x, fogCol.y, fogCol.z, exposure);
        g_sky.f4("uLightning", lightDir.x, lightDir.y, lightDir.z, g.lightning);
        g_sky.f4("uDawn", 0.55f * dawn, 0.42f * dawn, 0.34f * dawn, dawn);
        glBindVertexArray(g_vaoEmpty);
        glDepthMask(GL_FALSE);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        g_stats.drawCalls++;
    }
    // decals
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -2.0f);
    draw_pass(PASS_DECAL, V_DECAL);
    glDisable(GL_POLYGON_OFFSET_FILL);
    // glass
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_pass(PASS_GLASS, V_GLASS);

    // halos + pickup glints
    {
        static std::vector<float> hv;
        hv.clear();
        auto add = [&](const Vec3& p, const Vec3& col, float size) {
            static const float c[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
            for (int k = 0; k < 6; k++) {
                hv.push_back(p.x); hv.push_back(p.y); hv.push_back(p.z);
                hv.push_back(c[k][0]); hv.push_back(c[k][1]);
                hv.push_back(col.x); hv.push_back(col.y); hv.push_back(col.z); hv.push_back(size);
            }
        };
        for (const Light& l : w.lights) {
            if (l.halo <= 0 || l.cur <= 0.01f) continue;
            float d = length(l.pos - g.camPos);
            if (d > 260.0f) continue;
            // fog makes a lamp's glow bigger and softer with distance
            float grow = 1.0f + saturate(d / 60.0f) * 1.6f;
            float k = l.cur * 0.3f * std::exp(-d * fogDen * 0.45f);
            add(l.pos, l.color * k, l.halo * grow);
        }
        if (flood.cur > 0.01f) add(flood.pos, flood.color * (0.2f * flood.cur), 2.6f);
        if (g.appMode == AM_PLAY) {
            // things you can pick up catch the light for a moment
            for (int i = 0; i < IT_COUNT; i++) {
                if (g.itemTaken[i] || w.itemDyn[i] < 0) continue;
                const DynObj& d = w.dyn[(size_t)w.itemDyn[i]];
                if (!d.visible) continue;
                Vec3 p = d.xf.point(Vec3(0, 0.03f, 0));
                float dist = length(p - g.camPos);
                if (dist > 11.0f) continue;
                float pulse = std::pow(0.5f + 0.5f * std::sin(t * 2.2f + i * 1.7f), 6.0f);
                float k = (0.05f + 0.3f * pulse) * saturate(1.0f - dist / 11.0f);
                add(p, Vec3(0.9f, 0.85f, 0.7f) * k, 0.1f + 0.07f * pulse);
            }
        }
        if (!hv.empty()) {
            g_halo.use();
            g_halo.m4("uViewProj", vp);
            g_halo.v3("uRight", right);
            g_halo.v3("uUp", up);
            glBlendFunc(GL_ONE, GL_ONE);
            glBindVertexArray(g_vaoHalo);
            glBindBuffer(GL_ARRAY_BUFFER, g_vboHalo);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(hv.size() * sizeof(float)), hv.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(hv.size() / 9));
            g_stats.drawCalls++;
        }
    }
    // rain
    if (g.rain > 0.02f && g_rainTex) {
        g_rain.use();
        g_rain.m4("uViewProj", vp);
        g_rain.v3("uCamPos", g.camPos);
        g_rain.f4("uParams", t, 1.2f * g.wind, 0.5f * g.wind, g.rain * 0.3f);
        if (g.pstate == PS_SEATED || g.pstate == PS_OPERATING) {
            Vec3 cab = w.dyn[DY_CRANE_TOP].xf.point(Vec3(3.8f, 32.6f, 0));
            g_rain.f4("uRainHole", cab.x, cab.y, cab.z, 2.2f);
        } else g_rain.f4("uRainHole", 0, 0, 0, 0);
        g_rain.i1("uRainMap", 2);
        g_rain.f4("uRainOrg", w.rainX0, w.rainZ0, 1.0f / (float)World::RAIN_N, 0);
        g_rain.v3("uFlashPos", flashPos);
        g_rain.v3("uFlashDir", g_flashDir);
        g_rain.f4("uFlashP", flashI, std::cos(flashOuter), std::cos(flashOuter * 0.55f), flashRange);
        g_rain.f4("uGSPos", flood.pos.x, flood.pos.y, flood.pos.z, flood.radius);
        g_rain.f4("uGSCol", flood.color.x * floodCur, flood.color.y * floodCur, flood.color.z * floodCur, flood.cosOuter);
        g_rain.f4("uGSDir", flood.dir.x, flood.dir.y, flood.dir.z, flood.cosInner);
        Vec3 amb = ambSky * 1.6f + Vec3(0.5f, 0.56f, 0.75f) * lightning;
        g_rain.v3("uAmb", amb);
        LightSet ls;
        AABB cb(g.camPos - Vec3(8, 8, 8), g.camPos + Vec3(8, 8, 8));
        pick_lights(w, nullptr, 0, cb, w.lightFlood, 4, ls);
        glUniform4fv(g_rain.loc("uLPos"), 4, ls.pos);
        glUniform4fv(g_rain.loc("uLCol"), 4, ls.col);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, g_rainTex);
        glActiveTexture(GL_TEXTURE0);
        glBlendFunc(GL_ONE, GL_ONE);
        glBindVertexArray(g_vaoRain);
        glDrawElements(GL_TRIANGLES, RAIN_N * 6, GL_UNSIGNED_SHORT, nullptr);
        g_stats.drawCalls++;
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // --- post ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbW, fbH);
    glDisable(GL_DEPTH_TEST);
    g_post.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_sceneTex);
    g_post.i1("uScene", 0);
    float hide = g.pstate == PS_HIDING ? smooth01(g.hideT / 0.5f) : 0.0f;
    g_post.f4("uP0", std::fmod(t, 600.0f), g.fear, g.interference, saturate(g.fade));
    g_post.f4("uP1", g.flash, g.damage + (g.pstate == PS_DEAD ? saturate(g.deadT) : 0.0f), 0.055f, 1.0f / (float)std::max(1, fbH / 3));
    g_post.f4("uP2", dawn, hide, g.pstate == PS_DEAD ? 1.0f : 0.0f, aspect);
    glBindVertexArray(g_vaoEmpty);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    g_stats.drawCalls++;
    glBindVertexArray(0);

    // debug: look at one texture layer
    if (g_debugLayer >= 0) {
        static Program dbg;
        if (!dbg.id)
            dbg = gfx_program(VS_FULL,
                              "uniform sampler2DArray uTex; uniform float uLayer; in vec2 vUV; out vec4 frag;\n"
                              "void main(){ vec4 t = texture(uTex, vec3(vUV.x, 1.0 - vUV.y, uLayer)); vec3 bg = vec3(0.25,0.25,0.3);\n"
                              "frag = vec4(pow(mix(bg, t.rgb / max(t.a, 0.001), uLayer >= 33.0 ? t.a : 1.0), vec3(0.4545)), 1.0); }",
                              "", "debug");
        dbg.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, g_texArray);
        dbg.i1("uTex", 0);
        dbg.f1("uLayer", (float)g_debugLayer);
        int s = std::min(fbW, fbH);
        glViewport((fbW - s) / 2, (fbH - s) / 2, s, s);
        glBindVertexArray(g_vaoEmpty);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glViewport(0, 0, fbW, fbH);
    }
}
