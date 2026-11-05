// Deterministic hashing and pseudo random numbers. Nothing here depends on the
// standard library distributions, whose output differs between platforms.
#pragma once

#include <cstdint>

namespace strata {

inline uint64_t splitmix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ull;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
  return x ^ (x >> 31);
}

inline uint64_t mixInt(int v) { return static_cast<uint64_t>(static_cast<uint32_t>(v)); }

inline uint64_t hash2i(uint64_t seed, int x, int z) {
  uint64_t h = splitmix64(seed ^ 0x51ED270B27C3A4F1ull);
  h = splitmix64(h ^ mixInt(x));
  h = splitmix64(h ^ (mixInt(z) << 1));
  return h;
}

inline uint64_t hash3i(uint64_t seed, int x, int y, int z) {
  uint64_t h = splitmix64(seed ^ 0x2545F4914F6CDD1Dull);
  h = splitmix64(h ^ mixInt(x));
  h = splitmix64(h ^ (mixInt(y) << 1));
  h = splitmix64(h ^ (mixInt(z) << 2));
  return h;
}

// Uniform float in [0, 1) from the top 24 bits of a hash.
inline float hashToUnit(uint64_t h) { return static_cast<float>(h >> 40) * (1.0f / 16777216.0f); }

// PCG32 (O'Neill), small fast generator with good statistical quality.
class Pcg32 {
 public:
  explicit Pcg32(uint64_t seed = 0x853C49E6748FEA9Bull, uint64_t stream = 0xDA3E39CB94B95BDBull) {
    state_ = 0;
    inc_ = (stream << 1u) | 1u;
    next();
    state_ += seed;
    next();
  }

  uint32_t next() {
    uint64_t old = state_;
    state_ = old * 6364136223846793005ull + inc_;
    uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = static_cast<uint32_t>(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
  }

  // Uniform integer in [0, bound). bound must be > 0.
  uint32_t below(uint32_t bound) {
    uint32_t threshold = (0u - bound) % bound;
    for (;;) {
      uint32_t r = next();
      if (r >= threshold) return r % bound;
    }
  }

  // Uniform integer in [lo, hi] (inclusive).
  int range(int lo, int hi) {
    return lo + static_cast<int>(below(static_cast<uint32_t>(hi - lo + 1)));
  }

  float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
  float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
  bool chance(float p) { return unit() < p; }

 private:
  uint64_t state_;
  uint64_t inc_;
};

}  // namespace strata
