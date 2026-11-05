#include "render/textures.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

#include "core/math.hpp"
#include "core/random.hpp"
#include "world/block.hpp"

namespace strata {

float srgbToLinear(float c) {
  return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
float linearToSrgb(float c) {
  c = saturate(c);
  return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

namespace {

// ---------------------------------------------------------------------------
// Image and tileable noise helpers
// ---------------------------------------------------------------------------
struct Img {
  int n;
  std::vector<vec3> col;  // sRGB 0..1
  std::vector<float> a, h, smooth, emit;
  float bump = 2.0f;
  bool cutout = false;
  explicit Img(int size)
      : n(size),
        col(static_cast<size_t>(size * size), vec3(0.5f)),
        a(static_cast<size_t>(size * size), 1.0f),
        h(static_cast<size_t>(size * size), 0.5f),
        smooth(static_cast<size_t>(size * size), 0.1f),
        emit(static_cast<size_t>(size * size), 0.0f) {}
  size_t idx(int x, int y) const {
    x = ((x % n) + n) % n;
    y = ((y % n) + n) % n;
    return static_cast<size_t>(y * n + x);
  }
  template <class F>
  void each(F f) {
    for (int y = 0; y < n; ++y)
      for (int x = 0; x < n; ++x) {
        const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(n);
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(n);
        f(x, y, u, v, static_cast<size_t>(y * n + x));
      }
  }
};

inline uint32_t ihash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}
inline uint32_t hash3u(int x, int y, uint32_t seed) {
  return ihash(static_cast<uint32_t>(x) * 0x8DA6B343u ^ ihash(static_cast<uint32_t>(y) * 0xD8163841u ^ seed));
}
inline float unitHash(int x, int y, uint32_t seed) { return static_cast<float>(hash3u(x, y, seed) >> 8) * (1.0f / 16777216.0f); }
inline int wrapi(int v, int p) { return ((v % p) + p) % p; }
inline float fract(float x) { return x - std::floor(x); }

// Tileable value noise, period `p` lattice cells across the texture, result 0..1.
float vnoise(float u, float v, int p, uint32_t seed) {
  const float x = u * static_cast<float>(p), y = v * static_cast<float>(p);
  const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
  const float xf = x - static_cast<float>(xi), yf = y - static_cast<float>(yi);
  const float sx = xf * xf * xf * (xf * (xf * 6.0f - 15.0f) + 10.0f);
  const float sy = yf * yf * yf * (yf * (yf * 6.0f - 15.0f) + 10.0f);
  const float a = unitHash(wrapi(xi, p), wrapi(yi, p), seed);
  const float b = unitHash(wrapi(xi + 1, p), wrapi(yi, p), seed);
  const float c = unitHash(wrapi(xi, p), wrapi(yi + 1, p), seed);
  const float d = unitHash(wrapi(xi + 1, p), wrapi(yi + 1, p), seed);
  return lerpf(lerpf(a, b, sx), lerpf(c, d, sx), sy);
}

float fbm(float u, float v, int p, int octaves, uint32_t seed, float gain = 0.5f) {
  float sum = 0.0f, amp = 1.0f, norm = 0.0f;
  for (int o = 0; o < octaves; ++o) {
    sum += vnoise(u, v, p << o, seed + static_cast<uint32_t>(o) * 1013u) * amp;
    norm += amp;
    amp *= gain;
  }
  return sum / norm;
}

// Anisotropic noise: separate periods along u and v (both must divide evenly).
float vnoise2(float u, float v, int pu, int pv, uint32_t seed) {
  const float x = u * static_cast<float>(pu), y = v * static_cast<float>(pv);
  const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
  const float xf = x - static_cast<float>(xi), yf = y - static_cast<float>(yi);
  const float sx = xf * xf * (3.0f - 2.0f * xf);
  const float sy = yf * yf * (3.0f - 2.0f * yf);
  const float a = unitHash(wrapi(xi, pu), wrapi(yi, pv), seed);
  const float b = unitHash(wrapi(xi + 1, pu), wrapi(yi, pv), seed);
  const float c = unitHash(wrapi(xi, pu), wrapi(yi + 1, pv), seed);
  const float d = unitHash(wrapi(xi + 1, pu), wrapi(yi + 1, pv), seed);
  return lerpf(lerpf(a, b, sx), lerpf(c, d, sx), sy);
}

struct Cell {
  float f1 = 1e9f, f2 = 1e9f;
  uint32_t id = 0;
  float cx = 0, cy = 0;  // feature point relative to the sample (cell units)
};

// Tileable Voronoi with `cells` cells across the texture.
Cell voronoi(float u, float v, int cells, uint32_t seed, float jitter = 0.9f, float stretchY = 1.0f) {
  const float x = u * static_cast<float>(cells), y = v * static_cast<float>(cells);
  const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
  Cell r;
  for (int dy = -2; dy <= 2; ++dy)
    for (int dx = -2; dx <= 2; ++dx) {
      const int cx = xi + dx, cy = yi + dy;
      const int wx = wrapi(cx, cells), wy = wrapi(cy, cells);
      const uint32_t h = hash3u(wx, wy, seed);
      const float px = static_cast<float>(cx) + 0.5f + (static_cast<float>(h & 0xFFFF) / 65535.0f - 0.5f) * jitter;
      const float py = static_cast<float>(cy) + 0.5f + (static_cast<float>(h >> 16) / 65535.0f - 0.5f) * jitter;
      const float ddx = px - x, ddy = (py - y) * stretchY;
      const float d = std::sqrt(ddx * ddx + ddy * ddy);
      if (d < r.f1) {
        r.f2 = r.f1;
        r.f1 = d;
        r.id = h;
        r.cx = ddx;
        r.cy = ddy;
      } else if (d < r.f2) {
        r.f2 = d;
      }
    }
  return r;
}

inline float idUnit(uint32_t id, uint32_t salt) { return static_cast<float>(ihash(id ^ salt) >> 8) * (1.0f / 16777216.0f); }

vec3 rgb(int r, int g, int b) { return vec3(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)) / 255.0f; }

void normaliseHeight(Img& m) {
  float lo = 1e9f, hi = -1e9f;
  for (float v : m.h) {
    lo = std::min(lo, v);
    hi = std::max(hi, v);
  }
  const float range = hi - lo > 1e-6f ? hi - lo : 1.0f;
  for (float& v : m.h) v = (v - lo) / range;
}

// Soft disc stamp for sprite style textures (plants, leaves). No wrapping.
void stamp(Img& m, float cx, float cy, float radius, vec3 colour, float height, bool wrap) {
  const int r = static_cast<int>(std::ceil(radius + 1.0f));
  for (int y = static_cast<int>(cy) - r; y <= static_cast<int>(cy) + r; ++y)
    for (int x = static_cast<int>(cx) - r; x <= static_cast<int>(cx) + r; ++x) {
      if (!wrap && (x < 0 || y < 0 || x >= m.n || y >= m.n)) continue;
      const float dx = static_cast<float>(x) + 0.5f - cx, dy = static_cast<float>(y) + 0.5f - cy;
      if (dx * dx + dy * dy > radius * radius) continue;
      const size_t i = m.idx(x, y);
      m.col[i] = colour;
      m.a[i] = 1.0f;
      m.h[i] = height;
    }
}

void clearTransparent(Img& m) {
  std::fill(m.a.begin(), m.a.end(), 0.0f);
  std::fill(m.h.begin(), m.h.end(), 0.0f);
  m.cutout = true;
}

// ---------------------------------------------------------------------------
// Natural materials
// ---------------------------------------------------------------------------
void stone(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float base = fbm(u, v, 4, 5, s);
    const float grain = vnoise(u, v, 32, s + 7);
    const float ridge = 1.0f - std::fabs(fbm(u, v, 3, 4, s + 11) * 2.0f - 1.0f);
    const float crack = smoothstepf(0.955f, 0.995f, ridge);  // thin, sparse fractures
    const float g = 0.47f + (base - 0.5f) * 0.26f + (grain - 0.5f) * 0.10f - crack * 0.10f;
    const float t = fbm(u, v, 2, 2, s + 3);
    m.col[i] = vec3(g) * lerp(vec3(0.97f, 0.99f, 1.04f), vec3(1.04f, 1.0f, 0.95f), t);
    m.h[i] = base * 0.6f + grain * 0.2f - crack * 0.5f;
    m.smooth[i] = 0.16f + grain * 0.06f;
  });
  m.bump = 3.0f;
}

void dirt(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float base = fbm(u, v, 4, 5, s);
    const float fine = vnoise(u, v, 32, s + 5);
    const Cell c = voronoi(u, v, 9, s + 9);
    const float pebble = (idUnit(c.id, 1) > 0.72f) ? 1.0f - smoothstepf(0.18f, 0.32f, c.f1) : 0.0f;
    const vec3 soil = lerp(rgb(86, 64, 46), rgb(122, 94, 68), base) * (0.9f + 0.2f * fine);
    const vec3 stoneCol = lerp(rgb(120, 110, 100), rgb(150, 138, 124), idUnit(c.id, 2));
    m.col[i] = lerp(soil, stoneCol, pebble);
    m.h[i] = base * 0.5f + fine * 0.2f + pebble * 0.6f;
    m.smooth[i] = 0.05f + pebble * 0.1f;
  });
  m.bump = 2.5f;
}

void grassTop(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float base = fbm(u, v, 4, 4, s);
    const float blades = vnoise(u, v, 32, s + 1) * 0.6f + vnoise(u, v, 64, s + 2) * 0.4f;
    const float clump = fbm(u, v, 8, 2, s + 3);
    const float g = 0.66f + (base - 0.5f) * 0.18f + (blades - 0.5f) * 0.30f + (clump - 0.5f) * 0.1f;
    // Grayscale with a hint of yellow so the biome tint stays natural.
    m.col[i] = vec3(g * 1.02f, g, g * 0.92f);
    m.h[i] = blades * 0.8f + base * 0.2f;
    m.smooth[i] = 0.12f;
  });
  m.bump = 2.0f;
}

void grassSide(Img& m, uint32_t s, bool snow) {
  dirt(m, s);
  const int n = m.n;
  for (int x = 0; x < n; ++x) {
    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(n);
    // Ragged overhang: 3..7 texels deep with occasional longer strands.
    const float depth = (snow ? 0.20f : 0.13f) + 0.07f * vnoise(u, 0.1f, 8, s + 40) +
                        (vnoise(u, 0.3f, 32, s + 41) > 0.78f ? 0.09f : 0.0f);
    for (int y = 0; y < n; ++y) {
      const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(n);
      const size_t i = m.idx(x, y);
      if (v < depth) {
        if (snow) {
          const float g = 0.93f + 0.05f * vnoise(u, v, 16, s + 42);
          m.col[i] = vec3(g, g * 1.01f, g * 1.04f);
          m.smooth[i] = 0.35f;
          m.a[i] = 0.0f;  // no tint
        } else {
          const float g = 0.60f + 0.25f * vnoise(u, v, 32, s + 43);
          m.col[i] = vec3(g * 1.02f, g, g * 0.9f);
          m.a[i] = 1.0f;  // tint mask
          m.smooth[i] = 0.12f;
        }
        m.h[i] = 0.9f;
      } else {
        // Thin shadow below the overhang.
        const float sh = 1.0f - 0.35f * (1.0f - smoothstepf(depth, depth + 0.06f, v));
        m.col[i] = m.col[i] * sh;
        m.a[i] = 0.0f;
      }
    }
  }
}

void sand(Img& m, uint32_t s, vec3 c0, vec3 c1) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float base = fbm(u, v, 4, 3, s);
    const float grain = vnoise(u, v, 64, s + 1);
    const float ripple = 0.5f + 0.5f * std::sin((v * 6.0f + fbm(u, v, 2, 2, s + 2) * 1.6f) * kTwoPi);
    const float speck = unitHash(static_cast<int>(u * 64.0f), static_cast<int>(v * 64.0f), s + 3) > 0.93f ? 1.0f : 0.0f;
    vec3 c = lerp(c0, c1, base * 0.7f + ripple * 0.3f) * (0.94f + 0.12f * grain);
    c = lerp(c, c * 0.8f, speck * 0.5f);
    m.col[i] = c;
    m.h[i] = ripple * 0.3f + grain * 0.5f;
    m.smooth[i] = 0.08f;
  });
  m.bump = 1.2f;
}

void gravel(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const Cell c = voronoi(u, v, 7, s, 0.95f);
    const float edge = smoothstepf(0.02f, 0.16f, c.f2 - c.f1);
    const float dome = std::sqrt(std::max(0.0f, 1.0f - c.f1 * c.f1 * 1.6f));
    const float tone = idUnit(c.id, 7);
    const vec3 rock = lerp(rgb(96, 92, 88), rgb(160, 152, 144), tone) * lerp(vec3(1.0f), rgb(255, 238, 220), idUnit(c.id, 8) * 0.6f);
    const float grain = vnoise(u, v, 32, s + 2);
    m.col[i] = lerp(rgb(52, 48, 44), rock * (0.85f + 0.25f * dome) * (0.92f + 0.12f * grain), edge);
    m.h[i] = dome * edge;
    m.smooth[i] = 0.18f;
  });
  m.bump = 4.0f;
}

void baserock(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float a = fbm(u, v, 4, 5, s);
    const float r = 1.0f - std::fabs(fbm(u, v, 4, 3, s + 3) * 2.0f - 1.0f);
    const float g = 0.12f + a * 0.22f + std::pow(r, 6.0f) * 0.2f;
    m.col[i] = vec3(g, g * 0.98f, g * 0.97f);
    m.h[i] = a + std::pow(r, 6.0f);
    m.smooth[i] = 0.2f;
  });
  m.bump = 4.0f;
}

void snowTex(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float base = fbm(u, v, 4, 4, s);
    const float sparkle = unitHash(static_cast<int>(u * 64.0f), static_cast<int>(v * 64.0f), s + 1) > 0.985f ? 1.0f : 0.0f;
    const float g = 0.90f + (base - 0.5f) * 0.08f;
    m.col[i] = vec3(g * 0.97f, g * 0.99f, g * 1.03f);
    m.h[i] = base;
    m.smooth[i] = 0.3f + sparkle * 0.6f;
  });
  m.bump = 1.0f;
}

void iceTex(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float base = fbm(u, v, 3, 4, s);
    const float r1 = 1.0f - std::fabs(fbm(u, v, 3, 3, s + 1) * 2.0f - 1.0f);
    const float crack = smoothstepf(0.94f, 0.99f, r1);
    m.col[i] = lerp(rgb(150, 182, 228), rgb(196, 220, 245), base) + vec3(crack * 0.25f);
    m.a[i] = 0.72f + crack * 0.2f;
    m.h[i] = base * 0.3f - crack * 0.4f;
    m.smooth[i] = 0.92f;
  });
  m.bump = 1.0f;
}

void clayTex(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float base = fbm(u, v, 4, 4, s);
    const float g = 0.58f + (base - 0.5f) * 0.10f;
    m.col[i] = vec3(g * 0.96f, g * 0.98f, g * 1.06f);
    m.h[i] = base;
    m.smooth[i] = 0.25f;
  });
  m.bump = 1.0f;
}

void lava(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    // Domain warped noise gives slow, molten swirls; low values cool into crust.
    const float wu = u + (fbm(u, v, 2, 3, s + 5) - 0.5f) * 0.35f;
    const float wv = v + (fbm(u, v, 2, 3, s + 6) - 0.5f) * 0.35f;
    const float heat = fbm(wu, wv, 3, 5, s);
    const float veins = 1.0f - std::fabs(fbm(u, v, 4, 3, s + 7) * 2.0f - 1.0f);
    const float glow = saturate(smoothstepf(0.38f, 0.62f, heat) + std::pow(veins, 6.0f) * 0.6f);
    vec3 col = lerp(rgb(52, 14, 6), rgb(236, 88, 12), smoothstepf(0.0f, 0.55f, glow));
    col = lerp(col, rgb(255, 214, 110), smoothstepf(0.6f, 1.0f, glow));
    m.col[i] = col;
    m.emit[i] = 0.2f + 0.8f * glow;
    m.h[i] = 1.0f - glow;
    m.smooth[i] = 0.35f + 0.3f * glow;
  });
  m.bump = 3.0f;
}

void waterTex(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float base = fbm(u, v, 4, 3, s);
    m.col[i] = lerp(rgb(30, 80, 130), rgb(50, 110, 160), base);
    m.a[i] = 0.75f;
    m.h[i] = base;
    m.smooth[i] = 0.95f;
  });
}

// ---------------------------------------------------------------------------
// Wood
// ---------------------------------------------------------------------------
void bark(Img& m, uint32_t s, vec3 dark, vec3 light, float grooveSharpness) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float streak = vnoise2(u, v, 12, 2, s) * 0.6f + vnoise2(u, v, 24, 4, s + 1) * 0.4f;
    const float groove = std::pow(1.0f - std::fabs(vnoise2(u, v, 8, 1, s + 2) * 2.0f - 1.0f), grooveSharpness);
    const float fine = vnoise(u, v, 32, s + 3);
    const float t = saturate(streak * 0.8f + fine * 0.2f - groove * 0.45f);
    m.col[i] = lerp(dark, light, t);
    m.h[i] = streak * 0.5f - groove * 0.6f + fine * 0.1f;
    m.smooth[i] = 0.08f;
  });
  m.bump = 4.0f;
}

void birchBark(Img& m, uint32_t s) {
  m.each([&](int x, int y, float u, float v, size_t i) {
    const float base = fbm(u, v, 4, 3, s);
    float g = 0.82f + (base - 0.5f) * 0.1f;
    vec3 c = vec3(g, g * 0.985f, g * 0.94f);
    // Dark horizontal lenticels.
    const int row = y / 4;
    const float rowRand = unitHash(row, 0, s + 1);
    const float dashStart = unitHash(row, 1, s + 2);
    const float dashLen = 0.15f + 0.25f * unitHash(row, 2, s + 3);
    float pos = fract(u - dashStart);
    const bool dash = rowRand > 0.45f && pos < dashLen && (y % 4) < (rowRand > 0.8f ? 2 : 1);
    const float speck = unitHash(x, y, s + 4) > 0.97f ? 1.0f : 0.0f;
    if (dash) c = rgb(40, 36, 32) * (0.8f + 0.4f * base);
    c = lerp(c, c * 0.75f, speck);
    m.col[i] = c;
    m.h[i] = dash ? 0.0f : 0.6f + base * 0.3f;
    m.smooth[i] = dash ? 0.05f : 0.2f;
  });
  m.bump = 2.0f;
}

void logTop(Img& m, uint32_t s, vec3 inner, vec3 outer, vec3 barkCol) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float dx = u - 0.5f, dy = v - 0.5f;
    const float warp = (fbm(u, v, 4, 3, s) - 0.5f) * 0.05f;
    const float r = std::sqrt(dx * dx + dy * dy) + warp;
    if (r > 0.44f) {
      const float n = vnoise(u, v, 32, s + 1);
      m.col[i] = barkCol * (0.8f + 0.3f * n);
      m.h[i] = 0.3f + n * 0.3f;
    } else {
      const float ring = 0.5f + 0.5f * std::sin(r * 70.0f);
      const float fine = vnoise(u, v, 32, s + 2);
      m.col[i] = lerp(inner, outer, ring * 0.55f + r) * (0.93f + 0.1f * fine);
      m.h[i] = 0.7f - ring * 0.15f;
    }
    m.smooth[i] = 0.1f;
  });
  m.bump = 2.0f;
}

void planks(Img& m, uint32_t s) {
  const int n = m.n;
  const int boards = 4;
  const int bh = n / boards;
  m.each([&](int x, int y, float u, float v, size_t i) {
    const int b = y / bh;
    const int seamX = static_cast<int>(unitHash(b, 0, s) * static_cast<float>(n));
    const bool hSeam = (y % bh) == bh - 1;
    const bool vSeam = x == seamX;
    const float boardTone = unitHash(b, 1, s) * 0.5f + unitHash(b + (x > seamX ? 7 : 0), 2, s) * 0.5f;
    const float grain = vnoise2(u + static_cast<float>(b) * 0.37f, v, 2, 32, s + 3) * 0.6f +
                        vnoise2(u, v + static_cast<float>(b) * 0.21f, 4, 64, s + 4) * 0.4f;
    const float knot = std::max(0.0f, 1.0f - voronoi(u, v, 3, s + 5).f1 * 4.5f);
    vec3 c = lerp(rgb(150, 110, 66), rgb(184, 140, 88), boardTone);
    c = c * (0.86f + 0.22f * grain) * (1.0f - knot * 0.35f);
    if (hSeam || vSeam) c = c * 0.45f;
    m.col[i] = c;
    m.h[i] = (hSeam || vSeam) ? 0.0f : 0.7f + grain * 0.2f;
    m.smooth[i] = 0.22f;
  });
  m.bump = 2.5f;
}

// ---------------------------------------------------------------------------
// Masonry
// ---------------------------------------------------------------------------
void cobble(Img& m, uint32_t s, bool mossy) {
  m.each([&](int, int, float u, float v, size_t i) {
    const Cell c = voronoi(u, v, 5, s, 0.85f);
    const float gap = c.f2 - c.f1;
    const float mortar = 1.0f - smoothstepf(0.04f, 0.12f, gap);
    const float dome = std::sqrt(std::max(0.0f, 1.0f - std::pow(c.f1 * 1.15f, 2.0f)));
    const float tone = idUnit(c.id, 3);
    const float grain = fbm(u, v, 16, 2, s + 1);
    vec3 st = lerp(rgb(92, 92, 92), rgb(150, 148, 144), tone) * (0.8f + 0.3f * dome) * (0.9f + 0.18f * grain);
    if (mossy && fbm(u, v, 4, 3, s + 2) > 0.58f) st = lerp(st, rgb(70, 100, 50), 0.6f);
    m.col[i] = lerp(st, rgb(58, 56, 52), mortar);
    m.h[i] = dome * (1.0f - mortar);
    m.smooth[i] = 0.15f;
  });
  m.bump = 4.0f;
}

void bricks(Img& m, uint32_t s) {
  const int n = m.n;
  const int rows = 4, bh = n / rows, bw = n / 2;
  m.each([&](int x, int y, float u, float v, size_t i) {
    const int row = y / bh;
    const int off = (row % 2) * (bw / 2);
    const int bx = (x + off) % n;
    const int col = bx / bw;
    const int ly = y % bh, lx = bx % bw;
    const bool mortar = ly < 2 || lx < 2;
    const float tone = unitHash(col, row, s);
    const float grain = vnoise(u, v, 32, s + 1);
    const float speck = unitHash(x, y, s + 2) > 0.9f ? 1.0f : 0.0f;
    vec3 brick = lerp(rgb(140, 62, 44), rgb(176, 88, 62), tone) * (0.88f + 0.2f * grain);
    brick = lerp(brick, brick * 0.7f, speck * 0.6f);
    const vec3 mor = rgb(178, 172, 160) * (0.9f + 0.15f * grain);
    m.col[i] = mortar ? mor : brick;
    const float edge = std::min({static_cast<float>(lx - 2), static_cast<float>(ly - 2), static_cast<float>(bw - 1 - lx),
                                 static_cast<float>(bh - 1 - ly)});
    m.h[i] = mortar ? 0.0f : 0.6f + 0.4f * saturate(edge / 2.0f) + grain * 0.1f;
    m.smooth[i] = mortar ? 0.05f : 0.15f;
  });
  m.bump = 3.0f;
}

void stoneBricks(Img& m, uint32_t s) {
  const int n = m.n;
  const int bh = n / 2, bw = n / 2;
  m.each([&](int x, int y, float u, float v, size_t i) {
    const int row = y / bh;
    const int off = (row % 2) * (bw / 2);
    const int bx = (x + off) % n;
    const int ly = y % bh, lx = bx % bw;
    const bool groove = ly < 2 || lx < 2;
    const float base = fbm(u, v, 4, 4, s);
    const float crack = smoothstepf(0.93f, 0.99f, 1.0f - std::fabs(fbm(u, v, 3, 3, s + 9) * 2.0f - 1.0f));
    const float tone = unitHash(bx / bw, row, s + 1);
    const float g = 0.48f + (base - 0.5f) * 0.18f + tone * 0.06f - crack * 0.12f;
    const float bevel = std::min({static_cast<float>(lx - 2), static_cast<float>(ly - 2), static_cast<float>(bw - 1 - lx),
                                  static_cast<float>(bh - 1 - ly)});
    const float shade = groove ? 0.55f : 0.9f + 0.1f * saturate(bevel / 3.0f);
    m.col[i] = vec3(g, g, g * 1.02f) * shade;
    m.h[i] = groove ? 0.0f : 0.7f + 0.3f * saturate(bevel / 3.0f) - crack * 0.3f;
    m.smooth[i] = 0.18f;
  });
  m.bump = 3.0f;
}

void sandstoneSide(Img& m, uint32_t s) {
  m.each([&](int, int y, float u, float v, size_t i) {
    const float band = 0.5f + 0.5f * std::sin((v * 7.0f + fbm(u, v, 2, 3, s) * 0.8f) * kTwoPi);
    const float grain = vnoise(u, v, 64, s + 1);
    vec3 c = lerp(rgb(206, 186, 132), rgb(226, 208, 156), band) * (0.95f + 0.08f * grain);
    if (y < 3) c = c * 0.86f;  // weathered top edge
    m.col[i] = c;
    m.h[i] = band * 0.4f + grain * 0.3f;
    m.smooth[i] = 0.1f;
  });
  m.bump = 2.0f;
}

void sandstoneTop(Img& m, uint32_t s, bool rough) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float base = fbm(u, v, 4, 4, s);
    const float grain = vnoise(u, v, 64, s + 1);
    const float crack = rough ? smoothstepf(0.92f, 0.99f, 1.0f - std::fabs(fbm(u, v, 3, 3, s + 2) * 2.0f - 1.0f)) : 0.0f;
    m.col[i] = lerp(rgb(208, 190, 138), rgb(228, 212, 162), base) * (0.95f + 0.07f * grain) * (1.0f - crack * 0.25f);
    m.h[i] = base * 0.5f + grain * 0.3f - crack * 0.4f;
    m.smooth[i] = 0.1f;
  });
  m.bump = 1.5f;
}

void basaltSide(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float col = vnoise2(u, v, 6, 1, s);
    const float fine = vnoise2(u, v, 32, 4, s + 1);
    const float seam = std::pow(1.0f - std::fabs(vnoise2(u, v, 6, 1, s + 2) * 2.0f - 1.0f), 10.0f);
    const float g = 0.20f + col * 0.08f + fine * 0.05f - seam * 0.08f;
    m.col[i] = vec3(g, g * 1.0f, g * 1.04f);
    m.h[i] = col * 0.5f + fine * 0.2f - seam * 0.5f;
    m.smooth[i] = 0.25f;
  });
  m.bump = 3.0f;
}

void basaltTop(Img& m, uint32_t s) {
  m.each([&](int, int, float u, float v, size_t i) {
    const Cell c = voronoi(u, v, 4, s, 0.6f);
    const float edge = smoothstepf(0.03f, 0.1f, c.f2 - c.f1);
    const float g = 0.18f + idUnit(c.id, 1) * 0.08f;
    m.col[i] = vec3(g, g, g * 1.04f) * (0.6f + 0.4f * edge);
    m.h[i] = edge;
    m.smooth[i] = 0.3f;
  });
  m.bump = 3.0f;
}

void ore(Img& m, uint32_t s, vec3 c0, vec3 c1, float shine, float density) {
  stone(m, s);
  m.each([&](int, int, float u, float v, size_t i) {
    const Cell cluster = voronoi(u, v, 4, s + 31, 0.9f);
    if (idUnit(cluster.id, 5) > density) return;
    const Cell fleck = voronoi(u, v, 16, s + 32, 0.9f);
    const float inCluster = 1.0f - smoothstepf(0.25f, 0.55f, cluster.f1);
    const float flake = 1.0f - smoothstepf(0.22f, 0.42f, fleck.f1);
    const float k = inCluster * flake;
    if (k <= 0.05f) return;
    const vec3 mineral = lerp(c0, c1, idUnit(fleck.id, 6));
    m.col[i] = lerp(m.col[i], mineral, saturate(k * 1.6f));
    m.h[i] += k * 0.6f;
    m.smooth[i] = lerpf(m.smooth[i], shine, saturate(k * 1.6f));
  });
}

// ---------------------------------------------------------------------------
// Foliage and plants (sprite based)
// ---------------------------------------------------------------------------
void leaves(Img& m, uint32_t s, vec3 dark, vec3 light, int count, bool needles) {
  clearTransparent(m);
  Pcg32 rng(s);
  const float n = static_cast<float>(m.n);
  for (int k = 0; k < count; ++k) {
    const float cx = rng.unit() * n, cy = rng.unit() * n;
    const float ang = rng.unit() * kPi;
    const float len = needles ? n * (0.06f + 0.05f * rng.unit()) : n * (0.045f + 0.035f * rng.unit());
    const float wid = needles ? n * 0.012f : len * (0.45f + 0.2f * rng.unit());
    const float tone = rng.unit();
    const vec3 base = lerp(dark, light, tone);
    const float ca = std::cos(ang), sa = std::sin(ang);
    const int r = static_cast<int>(std::ceil(len + 2.0f));
    for (int y = static_cast<int>(cy) - r; y <= static_cast<int>(cy) + r; ++y)
      for (int x = static_cast<int>(cx) - r; x <= static_cast<int>(cx) + r; ++x) {
        const float dx = static_cast<float>(x) + 0.5f - cx, dy = static_cast<float>(y) + 0.5f - cy;
        const float along = dx * ca + dy * sa, across = -dx * sa + dy * ca;
        const float e = (along * along) / (len * len) + (across * across) / (wid * wid);
        if (e > 1.0f) continue;
        const size_t i = m.idx(x, y);
        const float vein = needles ? 1.0f : 0.85f + 0.15f * smoothstepf(0.0f, 0.35f, std::fabs(across) / wid);
        const float shade = 0.8f + 0.2f * (along / len);
        m.col[i] = base * vein * shade;
        m.a[i] = 1.0f;
        m.h[i] = 0.4f + 0.6f * (1.0f - e) + tone * 0.2f;
        m.smooth[i] = 0.25f;
      }
  }
  m.bump = 2.0f;
}

void tallGrassTex(Img& m, uint32_t s, int blades) {
  clearTransparent(m);
  Pcg32 rng(s);
  const float n = static_cast<float>(m.n);
  for (int b = 0; b < blades; ++b) {
    const float x0 = n * (0.12f + 0.76f * rng.unit());
    const float height = n * (0.45f + 0.5f * rng.unit());
    const float bend = n * (rng.unit() - 0.5f) * 0.35f;
    const float w0 = n * (0.022f + 0.018f * rng.unit());
    const float tone = 0.55f + 0.35f * rng.unit();
    for (float t = 0.0f; t <= 1.0f; t += 0.5f / height) {
      const float x = x0 + bend * t * t;
      const float y = n - t * height;
      const float w = w0 * (1.0f - 0.85f * t) + 0.35f;
      const float g = tone * (0.75f + 0.35f * t);
      stamp(m, x, y, w, vec3(g * 1.02f, g, g * 0.88f), t, false);
    }
  }
  m.bump = 1.0f;
}

void fernTex(Img& m, uint32_t s) {
  clearTransparent(m);
  Pcg32 rng(s);
  const float n = static_cast<float>(m.n);
  for (int f = 0; f < 6; ++f) {
    const float baseX = n * (0.35f + 0.3f * rng.unit());
    const float ang = (rng.unit() - 0.5f) * 1.6f;
    const float len = n * (0.55f + 0.35f * rng.unit());
    const float tone = 0.55f + 0.3f * rng.unit();
    for (float t = 0.0f; t <= 1.0f; t += 0.6f / len) {
      // Stem arching outwards.
      const float a = ang * (0.4f + t);
      const float x = baseX + std::sin(a) * t * len * 0.7f;
      const float y = n - std::cos(a * 0.8f) * t * len;
      stamp(m, x, y, 0.7f, vec3(tone * 0.9f), t, false);
      // Leaflets on both sides, shrinking towards the tip.
      if (static_cast<int>(t * len) % 3 == 0) {
        const float leaf = (1.0f - t) * n * 0.11f + 1.0f;
        for (int side = -1; side <= 1; side += 2)
          for (float k = 0.0f; k < 1.0f; k += 0.2f) {
            const float lx = x + std::cos(a) * static_cast<float>(side) * leaf * k;
            const float ly = y + std::sin(a) * static_cast<float>(side) * leaf * k - k * 1.5f;
            stamp(m, lx, ly, 0.9f * (1.0f - k * 0.6f), vec3(tone), t * 0.8f + 0.2f, false);
          }
      }
    }
  }
  m.bump = 1.0f;
}

void flowerTex(Img& m, uint32_t s, vec3 petal, vec3 centre, float size) {
  clearTransparent(m);
  const float n = static_cast<float>(m.n);
  const vec3 stem = rgb(62, 118, 46);
  const float headY = n * 0.36f;
  for (float y = headY; y < n; y += 0.5f) stamp(m, n * 0.5f + std::sin(y * 0.08f) * 1.2f, y, n * 0.018f, stem, 0.4f, false);
  // Two leaves.
  for (int side = -1; side <= 1; side += 2)
    for (float t = 0.0f; t < 1.0f; t += 0.04f) {
      const float x = n * 0.5f + static_cast<float>(side) * t * n * 0.2f;
      const float y = n * 0.78f - std::sin(t * kPi) * n * 0.08f - t * n * 0.08f;
      stamp(m, x, y, n * 0.03f * std::sin(t * kPi) + 0.5f, stem * 1.1f, 0.5f, false);
    }
  Pcg32 rng(s);
  const int petals = 6;
  for (int p = 0; p < petals; ++p) {
    const float a = static_cast<float>(p) / static_cast<float>(petals) * kTwoPi + rng.unit() * 0.3f;
    for (float t = 0.0f; t < 1.0f; t += 0.08f) {
      const float r = n * size * t;
      stamp(m, n * 0.5f + std::cos(a) * r, headY + std::sin(a) * r * 0.75f, n * size * 0.42f * std::sin(t * kPi + 0.3f) + 0.6f,
            petal * (0.85f + 0.15f * t), 0.8f, false);
    }
  }
  stamp(m, n * 0.5f, headY, n * size * 0.35f, centre, 1.0f, false);
  m.bump = 1.0f;
}

void deadBushTex(Img& m, uint32_t s) {
  clearTransparent(m);
  Pcg32 rng(s);
  const float n = static_cast<float>(m.n);
  std::function<void(float, float, float, float, int)> branch = [&](float x, float y, float ang, float len, int depth) {
    const float steps = len * 2.0f;
    float cx = x, cy = y;
    for (int i = 0; i < static_cast<int>(steps); ++i) {
      cx += std::sin(ang) * 0.5f;
      cy -= std::cos(ang) * 0.5f;
      stamp(m, cx, cy, 0.45f + 0.25f * static_cast<float>(depth), rgb(118, 82, 46) * (0.8f + 0.1f * static_cast<float>(depth)),
            0.5f, false);
    }
    if (depth <= 0) return;
    const int kids = 2;
    for (int k = 0; k < kids; ++k) branch(cx, cy, ang + (rng.unit() - 0.5f) * 1.6f, len * 0.6f, depth - 1);
  };
  for (int b = 0; b < 4; ++b) branch(n * (0.4f + 0.2f * rng.unit()), n, (rng.unit() - 0.5f) * 1.4f, n * 0.28f, 2);
  m.bump = 1.0f;
}

void cactusSide(Img& m, uint32_t s) {
  m.each([&](int x, int y, float u, float v, size_t i) {
    const float ridge = 0.5f + 0.5f * std::cos(u * kTwoPi * 4.0f);
    const float fine = vnoise(u, v, 32, s);
    vec3 c = lerp(rgb(38, 88, 34), rgb(86, 146, 60), ridge) * (0.9f + 0.15f * fine);
    // Spines: small light dots along the ridges.
    const bool spine = (x % 16 == 8) && (y % 10 == 3);
    if (spine) c = rgb(230, 225, 190);
    m.col[i] = c;
    m.h[i] = ridge * 0.8f + (spine ? 0.4f : 0.0f);
    m.smooth[i] = 0.3f;
  });
  m.bump = 2.5f;
}

void cactusTop(Img& m, uint32_t s, bool top) {
  m.each([&](int, int, float u, float v, size_t i) {
    const float dx = u - 0.5f, dy = v - 0.5f;
    const float r = std::sqrt(dx * dx + dy * dy);
    const float a = std::atan2(dy, dx);
    const float star = 0.5f + 0.5f * std::cos(a * 8.0f);
    const float fine = vnoise(u, v, 32, s);
    vec3 c = lerp(rgb(58, 112, 44), rgb(98, 158, 72), top ? star * (1.0f - r) + 0.3f : 0.5f) * (0.9f + 0.15f * fine);
    if (r > 0.45f) c = c * 0.8f;
    m.col[i] = c;
    m.h[i] = top ? star * 0.5f : fine;
    m.smooth[i] = 0.3f;
  });
}

void torchTex(Img& m, uint32_t s) {
  const int n = m.n;
  m.each([&](int, int y, float u, float v, size_t i) {
    const float fine = vnoise(u, v, 32, s);
    if (y < n * 8 / 16) {
      // Flame and ember head (rows above 8/16; the model shows 6/16 .. 8/16).
      const float t = saturate((static_cast<float>(y) - static_cast<float>(n) * 6.0f / 16.0f) / (static_cast<float>(n) * 2.0f / 16.0f));
      m.col[i] = lerp(rgb(255, 236, 160), rgb(255, 150, 40), t) * (0.95f + 0.1f * fine);
      m.emit[i] = 1.0f;
      m.smooth[i] = 0.0f;
    } else {
      m.col[i] = lerp(rgb(92, 64, 36), rgb(132, 96, 58), fine);
      m.emit[i] = 0.0f;
      m.smooth[i] = 0.1f;
    }
    m.h[i] = fine;
  });
  m.bump = 1.0f;
}

void lampTex(Img& m, uint32_t s) {
  const int n = m.n;
  m.each([&](int x, int y, float u, float v, size_t i) {
    const int border = n / 12;
    const bool frame = x < border || y < border || x >= n - border || y >= n - border || std::abs(x - n / 2) < border / 2 ||
                       std::abs(y - n / 2) < border / 2;
    const float fine = fbm(u, v, 8, 3, s);
    if (frame) {
      m.col[i] = rgb(60, 52, 44) * (0.8f + 0.3f * fine);
      m.emit[i] = 0.0f;
      m.h[i] = 1.0f;
      m.smooth[i] = 0.5f;
    } else {
      m.col[i] = lerp(rgb(255, 196, 110), rgb(255, 236, 190), fine);
      m.emit[i] = 0.85f + 0.15f * fine;
      m.h[i] = 0.3f;
      m.smooth[i] = 0.8f;
    }
  });
  m.bump = 2.0f;
}

void glassTex(Img& m, uint32_t s) {
  const int n = m.n;
  m.each([&](int x, int y, float u, float v, size_t i) {
    const int border = std::max(1, n / 32);
    const bool frame = x < border || y < border || x >= n - border || y >= n - border;
    const float streak = smoothstepf(0.8f, 1.0f, 0.5f + 0.5f * std::sin((u + v) * 9.0f + fbm(u, v, 2, 2, s) * 2.0f));
    m.col[i] = frame ? rgb(220, 230, 232) : lerp(rgb(200, 226, 235), rgb(255, 255, 255), streak);
    m.a[i] = frame ? 0.75f : 0.10f + streak * 0.18f;
    m.h[i] = frame ? 1.0f : 0.5f;
    m.smooth[i] = 0.97f;
  });
  m.bump = 1.0f;
}

void crackTex(Img& m, uint32_t s, int stage) {
  clearTransparent(m);
  m.cutout = false;  // blended overlay
  // Random walk cracks revealed progressively with the stage.
  Pcg32 rng(s);
  const float n = static_cast<float>(m.n);
  const float reveal = static_cast<float>(stage + 1) / 10.0f;
  for (int c = 0; c < 9; ++c) {
    float x = n * 0.5f + (rng.unit() - 0.5f) * n * 0.25f;
    float y = n * 0.5f + (rng.unit() - 0.5f) * n * 0.25f;
    float ang = rng.unit() * kTwoPi;
    const int steps = static_cast<int>(n * 0.9f);
    for (int k = 0; k < steps; ++k) {
      const float order = (static_cast<float>(c) / 9.0f) * 0.35f + static_cast<float>(k) / static_cast<float>(steps) * 0.65f;
      ang += (rng.unit() - 0.5f) * 0.7f;
      x += std::cos(ang) * 0.7f;
      y += std::sin(ang) * 0.7f;
      if (order > reveal) continue;
      const size_t i = m.idx(static_cast<int>(x), static_cast<int>(y));
      m.col[i] = rgb(20, 18, 16);
      m.a[i] = 0.85f;
    }
  }
}

void stickIcon(Img& m, uint32_t s) {
  clearTransparent(m);
  const float n = static_cast<float>(m.n);
  for (float t = 0.0f; t <= 1.0f; t += 0.004f) {
    const float x = n * (0.22f + 0.56f * t), y = n * (0.78f - 0.56f * t);
    stamp(m, x, y, n * 0.065f, rgb(60, 40, 20), 0.5f, false);
  }
  for (float t = 0.0f; t <= 1.0f; t += 0.004f) {
    const float x = n * (0.22f + 0.56f * t), y = n * (0.78f - 0.56f * t);
    const float g = 0.85f + 0.3f * vnoise(t, 0.5f, 16, s);
    stamp(m, x, y, n * 0.04f, rgb(140, 100, 58) * g, 1.0f, false);
  }
}

void coalIcon(Img& m, uint32_t s) {
  clearTransparent(m);
  const float n = static_cast<float>(m.n);
  m.each([&](int, int, float u, float v, size_t i) {
    const float dx = u - 0.5f, dy = v - 0.52f;
    const float a = std::atan2(dy, dx);
    const float radius = 0.30f + 0.06f * std::sin(a * 3.0f + 1.0f) + 0.04f * std::sin(a * 7.0f);
    const float r = std::sqrt(dx * dx + dy * dy);
    if (r > radius) return;
    const Cell c = voronoi(u, v, 6, s, 0.9f);
    const float facet = 0.5f + 0.5f * idUnit(c.id, 2);
    const float light = saturate(0.6f - (dx + dy) * 1.2f);
    const float g = 0.10f + 0.12f * facet * light;
    m.col[i] = r > radius - 2.0f / n ? vec3(0.04f) : vec3(g);
    m.a[i] = 1.0f;
    m.h[i] = facet;
  });
}

// ---------------------------------------------------------------------------
TextureLayer finish(const Img& m) {
  const int n = m.n;
  TextureLayer t;
  t.cutout = m.cutout;
  t.albedo.resize(static_cast<size_t>(n * n * 4));
  t.material.resize(static_cast<size_t>(n * n * 4));
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      const size_t i = static_cast<size_t>(y * n + x);
      const vec3 c = m.col[i];
      t.albedo[i * 4 + 0] = static_cast<uint8_t>(std::lround(saturate(c.x) * 255.0f));
      t.albedo[i * 4 + 1] = static_cast<uint8_t>(std::lround(saturate(c.y) * 255.0f));
      t.albedo[i * 4 + 2] = static_cast<uint8_t>(std::lround(saturate(c.z) * 255.0f));
      t.albedo[i * 4 + 3] = static_cast<uint8_t>(std::lround(saturate(m.a[i]) * 255.0f));
      // Tangent-space normal from the height field (x right, y down the image).
      const float hx = m.h[m.idx(x + 1, y)] - m.h[m.idx(x - 1, y)];
      const float hy = m.h[m.idx(x, y + 1)] - m.h[m.idx(x, y - 1)];
      vec3 nrm = normalize(vec3(-hx * m.bump, -hy * m.bump, 1.0f));
      if (m.cutout && m.a[i] < 0.5f) nrm = vec3(0, 0, 1);
      t.material[i * 4 + 0] = static_cast<uint8_t>(std::lround((nrm.x * 0.5f + 0.5f) * 255.0f));
      t.material[i * 4 + 1] = static_cast<uint8_t>(std::lround((nrm.y * 0.5f + 0.5f) * 255.0f));
      t.material[i * 4 + 2] = static_cast<uint8_t>(std::lround(saturate(m.smooth[i]) * 255.0f));
      t.material[i * 4 + 3] = static_cast<uint8_t>(std::lround(saturate(m.emit[i]) * 255.0f));
    }
  return t;
}

}  // namespace

TextureSet generateTextures(int size) {
  TextureSet set;
  set.size = size;
  set.layers.resize(tex::Count);
  auto make = [&](uint16_t id, const std::function<void(Img&)>& f) {
    Img m(size);
    f(m);
    normaliseHeight(m);
    set.layers[id] = finish(m);
  };
  make(tex::Stone, [](Img& m) { stone(m, 101); });
  make(tex::Dirt, [](Img& m) { dirt(m, 202); });
  make(tex::GrassTop, [](Img& m) { grassTop(m, 303); });
  make(tex::GrassSide, [](Img& m) { grassSide(m, 202, false); });
  make(tex::Sand, [](Img& m) { sand(m, 404, rgb(206, 190, 142), rgb(232, 220, 176)); });
  make(tex::Gravel, [](Img& m) { gravel(m, 505); });
  make(tex::Water, [](Img& m) { waterTex(m, 606); });
  make(tex::Lava, [](Img& m) { lava(m, 707); });
  make(tex::Baserock, [](Img& m) { baserock(m, 808); });
  make(tex::OakLogSide, [](Img& m) { bark(m, 909, rgb(58, 42, 26), rgb(112, 84, 54), 6.0f); });
  make(tex::OakLogTop, [](Img& m) { logTop(m, 910, rgb(176, 136, 84), rgb(136, 98, 58), rgb(78, 58, 36)); });
  make(tex::OakLeaves, [](Img& m) { leaves(m, 911, vec3(0.40f), vec3(0.78f), 150, false); });
  make(tex::BirchLogSide, [](Img& m) { birchBark(m, 1001); });
  make(tex::BirchLogTop, [](Img& m) { logTop(m, 1002, rgb(214, 190, 140), rgb(180, 152, 104), rgb(208, 204, 194)); });
  make(tex::BirchLeaves, [](Img& m) { leaves(m, 1003, rgb(70, 108, 40), rgb(140, 176, 78), 150, false); });
  make(tex::SpruceLogSide, [](Img& m) { bark(m, 1101, rgb(46, 32, 22), rgb(112, 80, 52), 3.0f); });
  make(tex::SpruceLogTop, [](Img& m) { logTop(m, 1102, rgb(150, 112, 70), rgb(112, 80, 48), rgb(56, 40, 26)); });
  make(tex::SpruceLeaves, [](Img& m) { leaves(m, 1103, rgb(26, 52, 34), rgb(58, 92, 60), 520, true); });
  make(tex::Planks, [](Img& m) { planks(m, 1201); });
  make(tex::Cobblestone, [](Img& m) { cobble(m, 1301, false); });
  make(tex::Glass, [](Img& m) { glassTex(m, 1401); });
  make(tex::Snow, [](Img& m) { snowTex(m, 1501); });
  make(tex::SnowyGrassSide, [](Img& m) { grassSide(m, 202, true); });
  make(tex::Ice, [](Img& m) { iceTex(m, 1601); });
  make(tex::SandstoneSide, [](Img& m) { sandstoneSide(m, 1701); });
  make(tex::SandstoneTop, [](Img& m) { sandstoneTop(m, 1702, false); });
  make(tex::SandstoneBottom, [](Img& m) { sandstoneTop(m, 1703, true); });
  make(tex::Clay, [](Img& m) { clayTex(m, 1801); });
  make(tex::Bricks, [](Img& m) { bricks(m, 1901); });
  make(tex::StoneBricks, [](Img& m) { stoneBricks(m, 2001); });
  make(tex::CoalOre, [](Img& m) { ore(m, 101, rgb(18, 18, 18), rgb(40, 38, 36), 0.25f, 0.55f); });
  make(tex::IronOre, [](Img& m) { ore(m, 101, rgb(196, 140, 104), rgb(224, 186, 150), 0.45f, 0.5f); });
  make(tex::GoldOre, [](Img& m) { ore(m, 101, rgb(232, 182, 40), rgb(255, 228, 110), 0.85f, 0.45f); });
  make(tex::DiamondOre, [](Img& m) { ore(m, 101, rgb(90, 220, 230), rgb(190, 250, 255), 0.95f, 0.4f); });
  make(tex::CactusSide, [](Img& m) { cactusSide(m, 2101); });
  make(tex::CactusTop, [](Img& m) { cactusTop(m, 2102, true); });
  make(tex::CactusBottom, [](Img& m) { cactusTop(m, 2103, false); });
  make(tex::TallGrass, [](Img& m) { tallGrassTex(m, 2201, 16); });
  make(tex::Fern, [](Img& m) { fernTex(m, 2301); });
  make(tex::RedFlower, [](Img& m) { flowerTex(m, 2401, rgb(206, 34, 30), rgb(60, 30, 20), 0.15f); });
  make(tex::YellowFlower, [](Img& m) { flowerTex(m, 2402, rgb(246, 210, 50), rgb(200, 120, 30), 0.12f); });
  make(tex::DeadBush, [](Img& m) { deadBushTex(m, 2501); });
  make(tex::Torch, [](Img& m) { torchTex(m, 2601); });
  make(tex::Lamp, [](Img& m) { lampTex(m, 2701); });
  make(tex::BasaltSide, [](Img& m) { basaltSide(m, 2801); });
  make(tex::BasaltTop, [](Img& m) { basaltTop(m, 2802); });
  for (int k = 0; k < 10; ++k)
    make(static_cast<uint16_t>(tex::Crack0 + k), [k](Img& m) { crackTex(m, 2901, k); });
  make(tex::ItemStick, [](Img& m) { stickIcon(m, 3001); });
  make(tex::ItemCoal, [](Img& m) { coalIcon(m, 3002); });
  return set;
}

// ---------------------------------------------------------------------------
// Mipmaps
// ---------------------------------------------------------------------------
namespace {
float coverage(const std::vector<uint8_t>& img, int size, float scale) {
  int above = 0;
  const int count = size * size;
  for (int i = 0; i < count; ++i)
    if (static_cast<float>(img[static_cast<size_t>(i) * 4 + 3]) * scale >= 127.5f) ++above;
  return static_cast<float>(above) / static_cast<float>(count);
}
}  // namespace

std::vector<std::vector<uint8_t>> buildAlbedoMips(const std::vector<uint8_t>& level0, int size, bool cutout) {
  std::vector<std::vector<uint8_t>> levels;
  levels.push_back(level0);
  const float targetCoverage = cutout ? coverage(level0, size, 1.0f) : 0.0f;
  int s = size;
  while (s > 1) {
    const int ns = s / 2;
    const std::vector<uint8_t>& src = levels.back();
    std::vector<uint8_t> dst(static_cast<size_t>(ns * ns * 4));
    for (int y = 0; y < ns; ++y)
      for (int x = 0; x < ns; ++x) {
        float rgbSum[3] = {0, 0, 0};
        float wSum = 0.0f, aSum = 0.0f;
        for (int dy = 0; dy < 2; ++dy)
          for (int dx = 0; dx < 2; ++dx) {
            const size_t i = static_cast<size_t>(((y * 2 + dy) * s + (x * 2 + dx)) * 4);
            const float a = static_cast<float>(src[i + 3]) / 255.0f;
            // Weight colours by alpha so transparent texels do not darken edges.
            const float w = cutout ? a + 1e-4f : 1.0f;
            for (int c = 0; c < 3; ++c) rgbSum[c] += srgbToLinear(static_cast<float>(src[i + static_cast<size_t>(c)]) / 255.0f) * w;
            wSum += w;
            aSum += a;
          }
        const size_t o = static_cast<size_t>((y * ns + x) * 4);
        for (int c = 0; c < 3; ++c)
          dst[o + static_cast<size_t>(c)] = static_cast<uint8_t>(std::lround(linearToSrgb(rgbSum[c] / wSum) * 255.0f));
        dst[o + 3] = static_cast<uint8_t>(std::lround(aSum / 4.0f * 255.0f));
      }
    if (cutout && targetCoverage > 0.0f) {
      // Scale alpha so the fraction of texels passing the 0.5 test stays the same.
      float lo = 0.0f, hi = 8.0f;
      for (int it = 0; it < 20; ++it) {
        const float mid = 0.5f * (lo + hi);
        if (coverage(dst, ns, mid) < targetCoverage)
          lo = mid;
        else
          hi = mid;
      }
      const float scale = hi;
      for (int i = 0; i < ns * ns; ++i) {
        const size_t k = static_cast<size_t>(i) * 4 + 3;
        dst[k] = static_cast<uint8_t>(std::min(255.0f, std::round(static_cast<float>(dst[k]) * scale)));
      }
    }
    levels.push_back(std::move(dst));
    s = ns;
  }
  return levels;
}

std::vector<std::vector<uint8_t>> buildMaterialMips(const std::vector<uint8_t>& level0, int size) {
  std::vector<std::vector<uint8_t>> levels;
  levels.push_back(level0);
  int s = size;
  while (s > 1) {
    const int ns = s / 2;
    const std::vector<uint8_t>& src = levels.back();
    std::vector<uint8_t> dst(static_cast<size_t>(ns * ns * 4));
    for (int y = 0; y < ns; ++y)
      for (int x = 0; x < ns; ++x) {
        vec3 nsum(0.0f);
        float sm = 0.0f, em = 0.0f;
        for (int dy = 0; dy < 2; ++dy)
          for (int dx = 0; dx < 2; ++dx) {
            const size_t i = static_cast<size_t>(((y * 2 + dy) * s + (x * 2 + dx)) * 4);
            const float nx = static_cast<float>(src[i]) / 127.5f - 1.0f;
            const float ny = static_cast<float>(src[i + 1]) / 127.5f - 1.0f;
            const float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
            nsum += vec3(nx, ny, nz);
            sm += static_cast<float>(src[i + 2]);
            em += static_cast<float>(src[i + 3]);
          }
        const vec3 n = normalize(nsum);
        const size_t o = static_cast<size_t>((y * ns + x) * 4);
        dst[o + 0] = static_cast<uint8_t>(std::lround((n.x * 0.5f + 0.5f) * 255.0f));
        dst[o + 1] = static_cast<uint8_t>(std::lround((n.y * 0.5f + 0.5f) * 255.0f));
        dst[o + 2] = static_cast<uint8_t>(std::lround(sm / 4.0f));
        dst[o + 3] = static_cast<uint8_t>(std::lround(em / 4.0f));
      }
    levels.push_back(std::move(dst));
    s = ns;
  }
  return levels;
}

}  // namespace strata
