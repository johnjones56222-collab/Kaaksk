// Chunk column storage: 16 x 256 x 16 blocks split into 16 sections of 16^3.
// Sections that are entirely air with uniform light are not allocated.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "world/block.hpp"

namespace strata {

constexpr int kChunkSize = 16;
constexpr int kChunkHeight = 256;
constexpr int kSectionCount = 16;
constexpr int kSectionVolume = 16 * 16 * 16;
constexpr int kChunkVolume = kChunkSize * kChunkSize * kChunkHeight;

// Index inside a dense chunk array (y major, then z, then x).
constexpr int chunkIndex(int x, int y, int z) { return (y << 8) | (z << 4) | x; }

inline uint8_t packLight(int sky, int blk) { return static_cast<uint8_t>((sky << 4) | blk); }
inline int skyOf(uint8_t light) { return light >> 4; }
inline int blockLightOf(uint8_t light) { return light & 15; }

struct ChunkPos {
  int x = 0, z = 0;
  bool operator==(const ChunkPos& o) const { return x == o.x && z == o.z; }
  bool operator!=(const ChunkPos& o) const { return !(*this == o); }
};

struct ChunkPosHash {
  size_t operator()(const ChunkPos& p) const {
    uint64_t v = (static_cast<uint64_t>(static_cast<uint32_t>(p.x)) << 32) ^
                 static_cast<uint64_t>(static_cast<uint32_t>(p.z));
    v ^= v >> 33;
    v *= 0xFF51AFD7ED558CCDull;
    v ^= v >> 33;
    return static_cast<size_t>(v);
  }
};

struct Section {
  uint8_t blocks[kSectionVolume];
  uint8_t meta[kSectionVolume];
  uint8_t light[kSectionVolume];
  uint16_t nonAir = 0;
};

// Per-column climate produced by world generation (used for biome tinting).
struct ColumnClimate {
  uint8_t biome = 0;
  uint8_t grass[3] = {120, 180, 80};    // sRGB tint
  uint8_t foliage[3] = {90, 150, 60};   // sRGB tint
};

class Chunk {
 public:
  Chunk(int cx, int cz);

  int cx() const { return pos_.x; }
  int cz() const { return pos_.z; }
  ChunkPos pos() const { return pos_; }

  // Local coordinates: x, z in [0, 16), y in [0, 256). No bounds checks.
  BlockId block(int x, int y, int z) const {
    const Section* s = sections_[y >> 4].get();
    return s ? s->blocks[sectionIndex(x, y, z)] : static_cast<BlockId>(block::Air);
  }
  uint8_t meta(int x, int y, int z) const {
    const Section* s = sections_[y >> 4].get();
    return s ? s->meta[sectionIndex(x, y, z)] : 0;
  }
  uint8_t light(int x, int y, int z) const {
    const Section* s = sections_[y >> 4].get();
    return s ? s->light[sectionIndex(x, y, z)] : uniformLight_[y >> 4];
  }

  void setBlock(int x, int y, int z, BlockId b, uint8_t meta = 0);
  void setMeta(int x, int y, int z, uint8_t meta);
  void setLight(int x, int y, int z, uint8_t value);

  // Highest non-air y in a column, or -1 when the column is empty.
  int height(int x, int z) const { return heightmap_[z * 16 + x]; }

  bool sectionEmpty(int s) const { return !sections_[s] || sections_[s]->nonAir == 0; }
  const Section* section(int s) const { return sections_[s].get(); }
  uint8_t uniformLight(int s) const { return uniformLight_[s]; }

  // Dense copies in chunkIndex order (kChunkVolume bytes).
  void copyBlocks(uint8_t* out) const;
  void copyMeta(uint8_t* out) const;
  // Replaces all blocks and metadata. Light is reset to full sky light.
  void assign(const uint8_t* blocks, const uint8_t* meta);
  // Replaces all light values (output of the light engine).
  void applyLight(const uint8_t* light);

  ColumnClimate climate[256];

  // Bookkeeping owned by the world (main thread only).
  uint64_t uid = 0;               // unique per loaded instance; detects stale job results
  bool lit = false;
  bool modified = false;          // needs saving
  uint32_t version = 0;           // bumped on every block change
  std::array<uint32_t, kSectionCount> meshDirty{};    // bumped when a section mesh is stale
  std::array<uint32_t, kSectionCount> meshBuilt{};    // meshDirty value the GPU mesh reflects
  std::array<bool, kSectionCount> meshInFlight{};
  std::array<bool, kSectionCount> hasMesh{};

  static constexpr int sectionIndex(int x, int y, int z) { return ((y & 15) << 8) | (z << 4) | x; }

 private:
  Section* ensureSection(int s);
  void recomputeColumn(int x, int z);
  void trimSection(int s);

  ChunkPos pos_;
  std::array<std::unique_ptr<Section>, kSectionCount> sections_;
  std::array<uint8_t, kSectionCount> uniformLight_;
  std::array<int16_t, 256> heightmap_;
};

}  // namespace strata
