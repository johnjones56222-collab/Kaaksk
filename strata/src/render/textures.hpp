// Procedural block textures. Everything is generated at start-up from code:
// no image files are shipped.
#pragma once

#include <cstdint>
#include <vector>

namespace strata {

struct TextureLayer {
  std::vector<uint8_t> albedo;    // RGBA8, sRGB colour; alpha = coverage or tint mask
  std::vector<uint8_t> material;  // RGBA8: normal.x, normal.y (tangent space), smoothness, emission
  bool cutout = false;            // alpha tested (needs coverage preserving mipmaps)
};

struct TextureSet {
  int size = 0;
  std::vector<TextureLayer> layers;  // indexed by tex:: ids
};

TextureSet generateTextures(int size);

// Builds a full mip chain (level 0 included) for one RGBA8 layer.
// albedo: sRGB colour averaged in linear space; cutout layers keep their alpha
// coverage at the 0.5 threshold. material: normals renormalised.
std::vector<std::vector<uint8_t>> buildAlbedoMips(const std::vector<uint8_t>& level0, int size, bool cutout);
std::vector<std::vector<uint8_t>> buildMaterialMips(const std::vector<uint8_t>& level0, int size);

// sRGB <-> linear helpers (exact sRGB curve).
float srgbToLinear(float c);
float linearToSrgb(float c);

}  // namespace strata
