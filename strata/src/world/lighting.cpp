#include "world/lighting.hpp"

#include <algorithm>

namespace strata {

namespace {
constexpr int kW = 48;  // 3 chunks wide
constexpr int kLayer = kW * kW;
constexpr int kVolume = kLayer * kChunkHeight;

inline int vidx(int x, int y, int z) { return (y * kW + z) * kW + x; }

struct Scratch {
  std::vector<uint8_t> blocks, sky, blk;
  std::vector<uint32_t> queue;
  std::vector<int16_t> top15;  // lowest y of the fully sky-lit part of each column
  Scratch() : blocks(kVolume), sky(kVolume), blk(kVolume), top15(kLayer) { queue.reserve(1 << 16); }
};

template <bool Sky>
void flood(const uint8_t* opacity, const std::vector<uint8_t>& blocks, std::vector<uint8_t>& light,
           std::vector<uint32_t>& queue) {
  for (size_t head = 0; head < queue.size(); ++head) {
    const uint32_t i = queue[head];
    const int v = light[i];
    if (v <= 0) continue;
    const int x = static_cast<int>(i % kW);
    const int z = static_cast<int>((i / kW) % kW);
    const int y = static_cast<int>(i / kLayer);
    for (int d = 0; d < 6; ++d) {
      const int nx = x + kFaceDir[d][0], ny = y + kFaceDir[d][1], nz = z + kFaceDir[d][2];
      if (nx < 0 || nx >= kW || nz < 0 || nz >= kW || ny < 0 || ny >= kChunkHeight) continue;
      const uint32_t n = static_cast<uint32_t>(vidx(nx, ny, nz));
      const int o = opacity[blocks[n]];
      if (o >= 15) continue;
      const int nv = v - lightCost(d, v, o, Sky);
      if (nv > light[n]) {
        light[n] = static_cast<uint8_t>(nv);
        queue.push_back(n);
      }
    }
  }
}
}  // namespace

void computeChunkLight(const uint8_t* const neighbourhood[9], uint8_t* outLight) {
  thread_local Scratch s;
  const uint8_t* opacity = lightOpacityTable();
  const uint8_t* emission = lightEmissionTable();

  for (int cz = 0; cz < 3; ++cz)
    for (int cx = 0; cx < 3; ++cx) {
      const uint8_t* src = neighbourhood[cz * 3 + cx];
      for (int y = 0; y < kChunkHeight; ++y)
        for (int z = 0; z < 16; ++z) {
          const uint8_t* row = src + chunkIndex(0, y, z);
          uint8_t* dst = &s.blocks[static_cast<size_t>(vidx(cx * 16, y, cz * 16 + z))];
          std::copy(row, row + 16, dst);
        }
    }

  // Sky light: straight down from the open sky, then flood fill sideways.
  for (int z = 0; z < kW; ++z)
    for (int x = 0; x < kW; ++x) {
      int v = 15;
      int top = kChunkHeight;
      for (int y = kChunkHeight - 1; y >= 0; --y) {
        const size_t i = static_cast<size_t>(vidx(x, y, z));
        const int o = opacity[s.blocks[i]];
        if (o >= 15)
          v = 0;
        else
          v = std::max(0, v - lightCost(kDirNegY, v, o, true));
        s.sky[i] = static_cast<uint8_t>(v);
        if (v == 15) top = y;
      }
      // `top` ends as the lowest y of the uninterrupted 15 run from the sky.
      s.top15[static_cast<size_t>(z * kW + x)] = static_cast<int16_t>(top);
    }
  s.queue.clear();
  for (int z = 0; z < kW; ++z)
    for (int x = 0; x < kW; ++x) {
      int limit = s.top15[static_cast<size_t>(z * kW + x)];
      if (x > 0) limit = std::max<int>(limit, s.top15[static_cast<size_t>(z * kW + x - 1)]);
      if (x < kW - 1) limit = std::max<int>(limit, s.top15[static_cast<size_t>(z * kW + x + 1)]);
      if (z > 0) limit = std::max<int>(limit, s.top15[static_cast<size_t>((z - 1) * kW + x)]);
      if (z < kW - 1) limit = std::max<int>(limit, s.top15[static_cast<size_t>((z + 1) * kW + x)]);
      limit = std::min(limit, kChunkHeight - 1);
      // Cells above `limit` sit in open sky with fully lit neighbours and can
      // not raise anything; only cells at or below it need to seed the flood.
      for (int y = 0; y <= limit; ++y) {
        const uint32_t i = static_cast<uint32_t>(vidx(x, y, z));
        if (s.sky[i] > 1) s.queue.push_back(i);
      }
    }
  flood<true>(opacity, s.blocks, s.sky, s.queue);

  // Block light from emitters.
  std::fill(s.blk.begin(), s.blk.end(), 0);
  s.queue.clear();
  for (uint32_t i = 0; i < static_cast<uint32_t>(kVolume); ++i) {
    const uint8_t e = emission[s.blocks[i]];
    if (e > 0) {
      s.blk[i] = e;
      s.queue.push_back(i);
    }
  }
  flood<false>(opacity, s.blocks, s.blk, s.queue);

  for (int y = 0; y < kChunkHeight; ++y)
    for (int z = 0; z < 16; ++z)
      for (int x = 0; x < 16; ++x) {
        const size_t i = static_cast<size_t>(vidx(16 + x, y, 16 + z));
        outLight[chunkIndex(x, y, z)] = packLight(s.sky[i], s.blk[i]);
      }
}

}  // namespace strata
