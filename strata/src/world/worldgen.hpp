// Procedural terrain generation. Every function is a pure function of the
// seed and world coordinates, so chunks can be generated on any thread in any
// order and still agree with each other at their borders.
#pragma once

#include <cstdint>

#include "core/math.hpp"
#include "core/noise.hpp"
#include "world/chunk.hpp"

namespace strata {

enum class Biome : uint8_t {
  Ocean,
  DeepOcean,
  FrozenOcean,
  Beach,
  River,
  FrozenRiver,
  Plains,
  Savanna,
  Forest,
  BirchForest,
  DarkForest,
  Taiga,
  SnowyTaiga,
  Tundra,
  Desert,
  Mountains,
  SnowyPeaks,
  Count
};
const char* biomeName(Biome b);

struct ColumnSample {
  int height = 0;            // y of the top terrain block
  float continent = 0.0f;    // < 0 ocean, > 0 land
  float mountain = 0.0f;     // 0..1
  float river = 0.0f;        // 1 in a river channel, 0 away from rivers
  float temperature = 0.0f;  // about -1 (polar) .. 1 (hot), includes altitude cooling
  float humidity = 0.0f;     // about -1 (dry) .. 1 (wet)
  Biome biome = Biome::Plains;
};

class WorldGenerator {
 public:
  static constexpr int kSeaLevel = 62;
  static constexpr int kLavaLevel = 10;

  explicit WorldGenerator(uint64_t seed);
  uint64_t seed() const { return seed_; }

  // Fills dense kChunkVolume arrays (chunkIndex order) and 256 column climates.
  void generate(int cx, int cz, uint8_t* blocks, uint8_t* meta, ColumnClimate* climate) const;
  // Only the 256 column climates (used for chunks loaded from disk).
  void computeClimate(int cx, int cz, ColumnClimate* climate) const;

  ColumnSample sampleColumn(int x, int z) const;
  // Block at the top of a column given its sample and steepness.
  BlockId surfaceBlock(const ColumnSample& c, int slope, int x, int z) const;
  // True when a cave removes the terrain block at (x, y, z).
  bool isCarved(int x, int y, int z, int surface, int minNeighbourSurface) const;
  // A dry, flat column near the origin suitable for spawning (y = first air block).
  ivec3 findSpawn() const;

 private:
  struct CaveValues {
    float a, b, c;
  };
  CaveValues caveCorner(int x, int y, int z) const;
  bool caveTest(const CaveValues& v, int x, int y, int z, int surface, int minNeighbourSurface,
                float entrance) const;
  float caveEntrance(int x, int z) const;
  void climateColors(const ColumnSample& c, ColumnClimate& out) const;

  uint64_t seed_;
  SimplexNoise warpX_, warpZ_, continent_, mountain_, ridge_, hills_, detail_, river_;
  SimplexNoise temperature_, humidity_, patches_, cave1_, cave2_, cave3_, entrance_;
};

}  // namespace strata
