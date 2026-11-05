// Builds render geometry for one 16^3 section from a padded snapshot.
#pragma once

#include <cstdint>
#include <vector>

#include "world/chunk.hpp"

namespace strata {

// Positions are stored in 1/32 block units relative to the section origin.
constexpr int kPosScale = 32;

struct TerrainVertex {
  int16_t x, y, z;
  uint16_t tex;    // texture array layer
  uint8_t u, v;    // texture coordinates in 1/16 units (0..16)
  uint8_t normal;  // 0..5 cube face, 6 = plant (soft shading)
  uint8_t ao;      // ambient occlusion factor 0..255
  uint8_t sky;     // smoothed sky light 0..255
  uint8_t blk;     // smoothed block light 0..255
  uint8_t anim;    // VertexAnim
  uint8_t flags;   // VertexFlags
  uint8_t r, g, b, a;  // tint colour (sRGB)
};
static_assert(sizeof(TerrainVertex) == 20, "vertex layout must stay packed");

enum VertexAnim : uint8_t { kAnimNone = 0, kAnimLeaves = 1, kAnimPlant = 2, kAnimWater = 3, kAnimLava = 4 };
enum VertexFlags : uint8_t {
  kFlagTint = 1,       // multiply albedo by the tint colour
  kFlagTintMask = 2,   // texture alpha selects where the tint applies (opaque blocks)
  kFlagBackFace = 4,   // liquid surface seen from inside the liquid
  kFlagFoliage = 8,    // leaves: lit with some translucency
};

// Padded snapshot of one section plus a one block border on every side.
constexpr int kPad = 18;
constexpr int padIndex(int x, int y, int z) { return ((y + 1) * kPad + (z + 1)) * kPad + (x + 1); }
constexpr int padColumn(int x, int z) { return (z + 1) * kPad + (x + 1); }

struct MeshInput {
  int cx = 0, sy = 0, cz = 0;
  uint8_t blocks[kPad * kPad * kPad];
  uint8_t meta[kPad * kPad * kPad];
  uint8_t light[kPad * kPad * kPad];
  uint8_t grass[kPad * kPad][3];
  uint8_t foliage[kPad * kPad][3];
};

struct MeshData {
  std::vector<TerrainVertex> opaque;   // depth-tested, no alpha test
  std::vector<TerrainVertex> cutout;   // alpha tested (leaves, plants, torches, cactus)
  std::vector<TerrainVertex> water;    // liquid surfaces (refraction / reflection shader)
  std::vector<TerrainVertex> glass;    // alpha blended (glass, ice)
  bool empty() const { return opaque.empty() && cutout.empty() && water.empty() && glass.empty(); }
  size_t vertexCount() const { return opaque.size() + cutout.size() + water.size() + glass.size(); }
};

void buildSectionMesh(const MeshInput& in, MeshData& out);

// Surface height (0..1) of a liquid block from its metadata.
float liquidHeight(uint8_t meta);

}  // namespace strata
