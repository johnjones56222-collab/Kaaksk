#include "core/noise.hpp"

#include <cmath>

#include "core/math.hpp"
#include "core/random.hpp"

namespace strata {

namespace {
// Gradients: midpoints of the 12 cube edges.
constexpr float kGrad3[12][3] = {{1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0},
                                 {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
                                 {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}};

constexpr float kF2 = 0.36602540378443864676f;  // (sqrt(3) - 1) / 2
constexpr float kG2 = 0.21132486540518711775f;  // (3 - sqrt(3)) / 6
constexpr float kF3 = 1.0f / 3.0f;
constexpr float kG3 = 1.0f / 6.0f;
}  // namespace

SimplexNoise::SimplexNoise(uint64_t seed) {
  uint8_t p[256];
  for (int i = 0; i < 256; ++i) p[i] = static_cast<uint8_t>(i);
  Pcg32 rng(splitmix64(seed), 0x9E3779B97F4A7C15ull);
  for (int i = 255; i > 0; --i) {
    int j = static_cast<int>(rng.below(static_cast<uint32_t>(i + 1)));
    uint8_t t = p[i];
    p[i] = p[j];
    p[j] = t;
  }
  for (int i = 0; i < 512; ++i) {
    perm_[i] = p[i & 255];
    permMod12_[i] = static_cast<uint8_t>(perm_[i] % 12);
  }
}

float SimplexNoise::noise2(float xin, float yin) const {
  float s = (xin + yin) * kF2;
  int i = fastFloor(xin + s);
  int j = fastFloor(yin + s);
  float t = static_cast<float>(i + j) * kG2;
  float x0 = xin - (static_cast<float>(i) - t);
  float y0 = yin - (static_cast<float>(j) - t);
  int i1, j1;
  if (x0 > y0) {
    i1 = 1;
    j1 = 0;
  } else {
    i1 = 0;
    j1 = 1;
  }
  float x1 = x0 - static_cast<float>(i1) + kG2;
  float y1 = y0 - static_cast<float>(j1) + kG2;
  float x2 = x0 - 1.0f + 2.0f * kG2;
  float y2 = y0 - 1.0f + 2.0f * kG2;
  int ii = i & 255;
  int jj = j & 255;
  int gi0 = permMod12_[ii + perm_[jj]];
  int gi1 = permMod12_[ii + i1 + perm_[jj + j1]];
  int gi2 = permMod12_[ii + 1 + perm_[jj + 1]];
  float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;
  float t0 = 0.5f - x0 * x0 - y0 * y0;
  if (t0 > 0.0f) {
    t0 *= t0;
    n0 = t0 * t0 * (kGrad3[gi0][0] * x0 + kGrad3[gi0][1] * y0);
  }
  float t1 = 0.5f - x1 * x1 - y1 * y1;
  if (t1 > 0.0f) {
    t1 *= t1;
    n1 = t1 * t1 * (kGrad3[gi1][0] * x1 + kGrad3[gi1][1] * y1);
  }
  float t2 = 0.5f - x2 * x2 - y2 * y2;
  if (t2 > 0.0f) {
    t2 *= t2;
    n2 = t2 * t2 * (kGrad3[gi2][0] * x2 + kGrad3[gi2][1] * y2);
  }
  return 70.0f * (n0 + n1 + n2);
}

float SimplexNoise::noise3(float xin, float yin, float zin) const {
  float s = (xin + yin + zin) * kF3;
  int i = fastFloor(xin + s);
  int j = fastFloor(yin + s);
  int k = fastFloor(zin + s);
  float t = static_cast<float>(i + j + k) * kG3;
  float x0 = xin - (static_cast<float>(i) - t);
  float y0 = yin - (static_cast<float>(j) - t);
  float z0 = zin - (static_cast<float>(k) - t);
  int i1, j1, k1, i2, j2, k2;
  if (x0 >= y0) {
    if (y0 >= z0) {
      i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0;
    } else if (x0 >= z0) {
      i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1;
    } else {
      i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1;
    }
  } else {
    if (y0 < z0) {
      i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1;
    } else if (x0 < z0) {
      i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1;
    } else {
      i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0;
    }
  }
  float x1 = x0 - static_cast<float>(i1) + kG3;
  float y1 = y0 - static_cast<float>(j1) + kG3;
  float z1 = z0 - static_cast<float>(k1) + kG3;
  float x2 = x0 - static_cast<float>(i2) + 2.0f * kG3;
  float y2 = y0 - static_cast<float>(j2) + 2.0f * kG3;
  float z2 = z0 - static_cast<float>(k2) + 2.0f * kG3;
  float x3 = x0 - 1.0f + 3.0f * kG3;
  float y3 = y0 - 1.0f + 3.0f * kG3;
  float z3 = z0 - 1.0f + 3.0f * kG3;
  int ii = i & 255;
  int jj = j & 255;
  int kk = k & 255;
  int gi0 = permMod12_[ii + perm_[jj + perm_[kk]]];
  int gi1 = permMod12_[ii + i1 + perm_[jj + j1 + perm_[kk + k1]]];
  int gi2 = permMod12_[ii + i2 + perm_[jj + j2 + perm_[kk + k2]]];
  int gi3 = permMod12_[ii + 1 + perm_[jj + 1 + perm_[kk + 1]]];
  float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f, n3 = 0.0f;
  float t0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0;
  if (t0 > 0.0f) {
    t0 *= t0;
    n0 = t0 * t0 * (kGrad3[gi0][0] * x0 + kGrad3[gi0][1] * y0 + kGrad3[gi0][2] * z0);
  }
  float t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;
  if (t1 > 0.0f) {
    t1 *= t1;
    n1 = t1 * t1 * (kGrad3[gi1][0] * x1 + kGrad3[gi1][1] * y1 + kGrad3[gi1][2] * z1);
  }
  float t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;
  if (t2 > 0.0f) {
    t2 *= t2;
    n2 = t2 * t2 * (kGrad3[gi2][0] * x2 + kGrad3[gi2][1] * y2 + kGrad3[gi2][2] * z2);
  }
  float t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;
  if (t3 > 0.0f) {
    t3 *= t3;
    n3 = t3 * t3 * (kGrad3[gi3][0] * x3 + kGrad3[gi3][1] * y3 + kGrad3[gi3][2] * z3);
  }
  return 32.0f * (n0 + n1 + n2 + n3);
}

float fbm2(const SimplexNoise& n, float x, float y, int octaves, float lacunarity, float gain) {
  float sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f;
  for (int o = 0; o < octaves; ++o) {
    // Offset each octave so lattice artefacts do not line up at the origin.
    float off = static_cast<float>(o) * 37.17f;
    sum += amp * n.noise2(x * freq + off, y * freq - off);
    norm += amp;
    amp *= gain;
    freq *= lacunarity;
  }
  return sum / norm;
}

float fbm3(const SimplexNoise& n, float x, float y, float z, int octaves, float lacunarity,
           float gain) {
  float sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f;
  for (int o = 0; o < octaves; ++o) {
    float off = static_cast<float>(o) * 37.17f;
    sum += amp * n.noise3(x * freq + off, y * freq + off * 0.5f, z * freq - off);
    norm += amp;
    amp *= gain;
    freq *= lacunarity;
  }
  return sum / norm;
}

float ridged2(const SimplexNoise& n, float x, float y, int octaves, float lacunarity, float gain) {
  float sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f, weight = 1.0f;
  for (int o = 0; o < octaves; ++o) {
    float off = static_cast<float>(o) * 53.71f;
    float v = 1.0f - std::fabs(n.noise2(x * freq + off, y * freq + off));
    v *= v;
    v *= weight;  // crests of finer octaves only appear on top of coarse crests
    weight = saturate(v * 1.5f);
    sum += amp * v;
    norm += amp;
    amp *= gain;
    freq *= lacunarity;
  }
  return sum / norm;
}

}  // namespace strata
