// Voxel light engine: sky light and block light, 0..15 each.
//
// Propagation rule (shared by the full and incremental algorithms):
//   light(n) = max over neighbours c of  light(c) - cost(c -> n)
//   cost = 0 for sky light 15 travelling straight down through a fully
//          transparent block, otherwise max(1, opacity(n)).
// Opaque blocks never receive light; emitters start at their emission level.
//
// A chunk's light is fully determined by the blocks within 15 cells of it,
// i.e. by its 3x3 chunk neighbourhood, which is what computeChunkLight uses.
#pragma once

#include <cstdint>
#include <vector>

#include "world/block.hpp"
#include "world/chunk.hpp"

namespace strata {

enum LightDir : int { kDirPosX = 0, kDirNegX, kDirPosY, kDirNegY, kDirPosZ, kDirNegZ };

inline int lightCost(int dir, int srcLight, int opacity, bool sky) {
  if (sky && dir == kDirNegY && srcLight == 15 && opacity == 0) return 0;
  return opacity > 1 ? opacity : 1;
}

// neighbourhood[(dz + 1) * 3 + (dx + 1)] are dense block arrays (kChunkVolume)
// of the chunk and its eight neighbours. Writes kChunkVolume packed light
// values for the centre chunk.
void computeChunkLight(const uint8_t* const neighbourhood[9], uint8_t* outLight);

// Incremental update after the block at (x, y, z) changed. `A` provides:
//   bool    valid(int x, int y, int z)      inside lit, loaded chunks (y range checked by caller)
//   BlockId block(int x, int y, int z)
//   uint8_t light(int x, int y, int z)      packed sky/block light
//   void    setLight(int x, int y, int z, uint8_t packed)
template <class A>
void relightAt(A& access, int x, int y, int z);

// ---------------------------------------------------------------------------
namespace detail {

struct LightNode {
  int x, y, z, v;
};

template <bool Sky, class A>
int getChannel(A& a, int x, int y, int z) {
  const uint8_t l = a.light(x, y, z);
  return Sky ? skyOf(l) : blockLightOf(l);
}

template <bool Sky, class A>
void setChannel(A& a, int x, int y, int z, int v) {
  const uint8_t l = a.light(x, y, z);
  const uint8_t n = Sky ? packLight(v, blockLightOf(l)) : packLight(skyOf(l), v);
  if (n != l) a.setLight(x, y, z, n);
}

template <bool Sky, class A>
void relightChannel(A& a, int x, int y, int z) {
  const uint8_t* opacity = lightOpacityTable();
  const uint8_t* emission = lightEmissionTable();
  std::vector<LightNode> removal;
  std::vector<LightNode> prop;
  std::vector<LightNode> reseed;
  removal.reserve(256);
  prop.reserve(256);

  const int old = getChannel<Sky>(a, x, y, z);
  setChannel<Sky>(a, x, y, z, 0);
  removal.push_back({x, y, z, old});

  for (size_t head = 0; head < removal.size(); ++head) {
    const LightNode c = removal[head];
    if (Sky ? c.y == kChunkHeight - 1 : emission[a.block(c.x, c.y, c.z)] > 0) reseed.push_back(c);
    for (int d = 0; d < 6; ++d) {
      const int nx = c.x + kFaceDir[d][0], ny = c.y + kFaceDir[d][1], nz = c.z + kFaceDir[d][2];
      if (ny < 0 || ny >= kChunkHeight || !a.valid(nx, ny, nz)) continue;
      const int nl = getChannel<Sky>(a, nx, ny, nz);
      if (nl == 0) continue;
      const int expected = c.v - lightCost(d, c.v, opacity[a.block(nx, ny, nz)], Sky);
      if (nl <= expected) {
        setChannel<Sky>(a, nx, ny, nz, 0);
        removal.push_back({nx, ny, nz, nl});
      } else {
        prop.push_back({nx, ny, nz, 0});
      }
    }
  }

  // Restore cells that are their own source: emitters, and the top layer for sky light.
  for (const LightNode& c : reseed) {
    const BlockId b = a.block(c.x, c.y, c.z);
    int v;
    if (Sky) {
      const int o = opacity[b];
      v = o >= 15 ? 0 : 15 - lightCost(kDirNegY, 15, o, true);
    } else {
      v = emission[b];
    }
    if (v > getChannel<Sky>(a, c.x, c.y, c.z)) {
      setChannel<Sky>(a, c.x, c.y, c.z, v);
      prop.push_back({c.x, c.y, c.z, 0});
    }
  }

  for (size_t head = 0; head < prop.size(); ++head) {
    const LightNode c = prop[head];
    const int v = getChannel<Sky>(a, c.x, c.y, c.z);
    if (v <= 0) continue;
    for (int d = 0; d < 6; ++d) {
      const int nx = c.x + kFaceDir[d][0], ny = c.y + kFaceDir[d][1], nz = c.z + kFaceDir[d][2];
      if (ny < 0 || ny >= kChunkHeight || !a.valid(nx, ny, nz)) continue;
      const int o = opacity[a.block(nx, ny, nz)];
      if (o >= 15) continue;
      const int nv = v - lightCost(d, v, o, Sky);
      if (nv > getChannel<Sky>(a, nx, ny, nz)) {
        setChannel<Sky>(a, nx, ny, nz, nv);
        prop.push_back({nx, ny, nz, 0});
      }
    }
  }
}

}  // namespace detail

template <class A>
void relightAt(A& access, int x, int y, int z) {
  detail::relightChannel<true>(access, x, y, z);
  detail::relightChannel<false>(access, x, y, z);
}

}  // namespace strata
