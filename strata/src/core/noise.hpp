// Seeded 2D/3D simplex noise and fractal helpers.
#pragma once

#include <cstdint>

namespace strata {

class SimplexNoise {
 public:
  explicit SimplexNoise(uint64_t seed = 0);

  // Both return values in roughly [-1, 1].
  float noise2(float x, float y) const;
  float noise3(float x, float y, float z) const;

 private:
  uint8_t perm_[512];
  uint8_t permMod12_[512];
};

// Fractal Brownian motion, normalised so the result stays in about [-1, 1].
float fbm2(const SimplexNoise& n, float x, float y, int octaves, float lacunarity = 2.0f,
           float gain = 0.5f);
float fbm3(const SimplexNoise& n, float x, float y, float z, int octaves, float lacunarity = 2.0f,
           float gain = 0.5f);
// Ridged multifractal in [0, 1]: sharp crests where the base noise crosses zero.
float ridged2(const SimplexNoise& n, float x, float y, int octaves, float lacunarity = 2.0f,
              float gain = 0.5f);

}  // namespace strata
