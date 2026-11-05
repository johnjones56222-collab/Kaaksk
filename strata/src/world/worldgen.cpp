#include "world/worldgen.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/random.hpp"

namespace strata {

namespace {

constexpr int kSea = WorldGenerator::kSeaLevel;
constexpr uint64_t kSaltTree = 0x7A3F1C2B5E6D9081ull;
constexpr uint64_t kSaltTreeShape = 0x1D2C3B4A59687706ull;
constexpr uint64_t kSaltPlant = 0x5BD1E995A3C5F00Dull;
constexpr uint64_t kSaltOre = 0xC2B2AE3D27D4EB4Full;
constexpr uint64_t kSaltSoil = 0x165667B19E3779F9ull;
constexpr uint64_t kSaltBase = 0x27D4EB2F165667C5ull;

// Piecewise linear curve through (x, y) knots sorted by x.
float curve(float x, const float (*knots)[2], int n) {
  if (x <= knots[0][0]) return knots[0][1];
  for (int i = 1; i < n; ++i) {
    if (x <= knots[i][0]) {
      float t = (x - knots[i - 1][0]) / (knots[i][0] - knots[i - 1][0]);
      return knots[i - 1][1] + (knots[i][1] - knots[i - 1][1]) * t;
    }
  }
  return knots[n - 1][1];
}

constexpr float kBaseHeight[][2] = {
    {-1.00f, kSea - 44.0f}, {-0.42f, kSea - 34.0f}, {-0.24f, kSea - 20.0f}, {-0.12f, kSea - 6.0f},
    {-0.04f, kSea + 1.0f},  {0.04f, kSea + 3.0f},   {0.20f, kSea + 8.0f},   {0.45f, kSea + 18.0f},
    {0.75f, kSea + 30.0f},  {1.20f, kSea + 42.0f}};

// Writes clipped to the chunk being generated.
struct Writer {
  uint8_t* blocks;
  uint8_t* meta;
  int ox, oz;
  bool inside(int x, int y, int z) const {
    return x >= ox && x < ox + 16 && z >= oz && z < oz + 16 && y >= 0 && y < kChunkHeight;
  }
  BlockId get(int x, int y, int z) const { return blocks[chunkIndex(x - ox, y, z - oz)]; }
  void put(int x, int y, int z, BlockId b) {
    const int i = chunkIndex(x - ox, y, z - oz);
    blocks[i] = b;
    meta[i] = 0;
  }
  // Leaves only grow into air.
  void leaves(int x, int y, int z, BlockId b) {
    if (inside(x, y, z) && get(x, y, z) == block::Air) put(x, y, z, b);
  }
  // Logs push away air, leaves and plants but never terrain or liquids.
  void log(int x, int y, int z, BlockId b) {
    if (!inside(x, y, z)) return;
    const BlockId cur = get(x, y, z);
    if (cur == block::Air || blockInfo(cur).wave != Wave::None) put(x, y, z, b);
  }
};

// One square layer of leaves with randomly trimmed corners. The rng is
// consumed the same way whether or not cells are clipped, so every chunk
// replays an identical tree.
void blob(Writer& w, int cx, int y, int cz, int r, BlockId leaf, Pcg32& rng) {
  for (int dz = -r; dz <= r; ++dz)
    for (int dx = -r; dx <= r; ++dx) {
      const bool corner = std::abs(dx) == r && std::abs(dz) == r;
      if (corner && rng.chance(0.55f)) continue;
      w.leaves(cx + dx, y, cz + dz, leaf);
    }
}

void oakTree(Writer& w, int x, int y, int z, Pcg32& rng, BlockId logId, BlockId leafId, int minH,
             int maxH) {
  const int h = rng.range(minH, maxH);
  for (int i = 0; i < h; ++i) w.log(x, y + i, z, logId);
  const int top = y + h;
  blob(w, x, top - 3, z, 2, leafId, rng);
  blob(w, x, top - 2, z, 2, leafId, rng);
  blob(w, x, top - 1, z, 1, leafId, rng);
  // Plus-shaped crown.
  w.leaves(x, top, z, leafId);
  w.leaves(x + 1, top, z, leafId);
  w.leaves(x - 1, top, z, leafId);
  w.leaves(x, top, z + 1, leafId);
  w.leaves(x, top, z - 1, leafId);
}

void bigOakTree(Writer& w, int x, int y, int z, Pcg32& rng) {
  const int h = rng.range(7, 10);
  for (int i = 0; i < h; ++i) w.log(x, y + i, z, block::OakLog);
  // A few short branches near the top.
  static constexpr int kHorizontal[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (int b = 0; b < 3; ++b) {
    const int dir = rng.range(0, 3);
    const int by = y + h - rng.range(2, 4);
    const int dx = kHorizontal[dir][0];
    const int dz = kHorizontal[dir][1];
    w.log(x + dx, by, z + dz, block::OakLog);
    w.log(x + 2 * dx, by + 1, z + 2 * dz, block::OakLog);
  }
  const int top = y + h;
  for (int dy = -4; dy <= 1; ++dy) {
    const float r = dy <= -3 ? 3.4f : dy <= 0 ? 3.9f : 2.4f;
    const int ri = static_cast<int>(r);
    for (int dz = -ri; dz <= ri; ++dz)
      for (int dx = -ri; dx <= ri; ++dx) {
        const float fy = static_cast<float>(dy + 1);
        const float d = std::sqrt(static_cast<float>(dx * dx + dz * dz) + fy * fy * 0.5f);
        const float jitter = rng.unit() * 0.9f;
        if (d + jitter <= r) w.leaves(x + dx, top + dy, z + dz, block::OakLeaves);
      }
  }
}

void spruceTree(Writer& w, int x, int y, int z, Pcg32& rng) {
  const int h = rng.range(7, 11);
  const int bare = rng.range(1, 3);  // trunk without branches at the bottom
  for (int i = 0; i < h; ++i) w.log(x, y + i, z, block::SpruceLog);
  int radius = rng.range(0, 1);
  int maxRadius = rng.range(2, 3);
  for (int yy = y + h; yy >= y + bare; --yy) {
    for (int dz = -radius; dz <= radius; ++dz)
      for (int dx = -radius; dx <= radius; ++dx) {
        if (radius > 0 && std::abs(dx) == radius && std::abs(dz) == radius) continue;
        w.leaves(x + dx, yy, z + dz, block::SpruceLeaves);
      }
    // Classic conifer silhouette: tiers that widen downwards and reset.
    if (radius >= maxRadius) {
      radius = 1;
      maxRadius = std::min(3, maxRadius + 1);
    } else {
      ++radius;
    }
  }
  w.leaves(x, y + h, z, block::SpruceLeaves);
  w.leaves(x, y + h + 1, z, block::SpruceLeaves);
}

struct OreDef {
  BlockId block;
  int veins;
  int size;
  int minY, maxY;
};
constexpr OreDef kOres[] = {
    {block::Dirt, 7, 20, 5, 120},     {block::Gravel, 6, 18, 5, 110},
    {block::CoalOre, 16, 11, 5, 132}, {block::IronOre, 10, 8, 5, 68},
    {block::GoldOre, 3, 7, 5, 34},    {block::DiamondOre, 1, 6, 5, 16},
};

}  // namespace

const char* biomeName(Biome b) {
  switch (b) {
    case Biome::Ocean: return "Ocean";
    case Biome::DeepOcean: return "Deep Ocean";
    case Biome::FrozenOcean: return "Frozen Ocean";
    case Biome::Beach: return "Beach";
    case Biome::River: return "River";
    case Biome::FrozenRiver: return "Frozen River";
    case Biome::Plains: return "Plains";
    case Biome::Savanna: return "Savanna";
    case Biome::Forest: return "Forest";
    case Biome::BirchForest: return "Birch Forest";
    case Biome::DarkForest: return "Dark Forest";
    case Biome::Taiga: return "Taiga";
    case Biome::SnowyTaiga: return "Snowy Taiga";
    case Biome::Tundra: return "Tundra";
    case Biome::Desert: return "Desert";
    case Biome::Mountains: return "Mountains";
    case Biome::SnowyPeaks: return "Snowy Peaks";
    case Biome::Count: break;
  }
  return "Unknown";
}

WorldGenerator::WorldGenerator(uint64_t seed)
    : seed_(seed),
      warpX_(seed ^ 0x01),
      warpZ_(seed ^ 0x02),
      continent_(seed ^ 0x03),
      mountain_(seed ^ 0x04),
      ridge_(seed ^ 0x05),
      hills_(seed ^ 0x06),
      detail_(seed ^ 0x07),
      river_(seed ^ 0x08),
      temperature_(seed ^ 0x09),
      humidity_(seed ^ 0x0A),
      patches_(seed ^ 0x0B),
      cave1_(seed ^ 0x0C),
      cave2_(seed ^ 0x0D),
      cave3_(seed ^ 0x0E),
      entrance_(seed ^ 0x0F) {}

ColumnSample WorldGenerator::sampleColumn(int x, int z) const {
  const float fx = static_cast<float>(x);
  const float fz = static_cast<float>(z);
  // A large domain warp bends coastlines and mountain ranges into organic shapes.
  const float wx = fbm2(warpX_, fx * (1.0f / 1400.0f), fz * (1.0f / 1400.0f), 3) * 320.0f;
  const float wz = fbm2(warpZ_, fx * (1.0f / 1400.0f), fz * (1.0f / 1400.0f), 3) * 320.0f;
  const float px = fx + wx;
  const float pz = fz + wz;

  ColumnSample s;
  s.continent = fbm2(continent_, px * (1.0f / 2600.0f), pz * (1.0f / 2600.0f), 5) * 1.35f + 0.08f;
  float h = curve(s.continent, kBaseHeight, static_cast<int>(sizeof(kBaseHeight) / sizeof(kBaseHeight[0])));

  const float inland = smoothstepf(-0.02f, 0.30f, s.continent);
  const float mraw = fbm2(mountain_, px * (1.0f / 1500.0f), pz * (1.0f / 1500.0f), 4);
  s.mountain = smoothstepf(0.0f, 0.55f, mraw + 0.2f * s.continent) * inland;
  if (s.mountain > 0.0f) {
    const float ridge = ridged2(ridge_, px * (1.0f / 560.0f), pz * (1.0f / 560.0f), 5);
    h += s.mountain * (std::pow(ridge, 1.5f) * 120.0f + 14.0f * s.mountain);
  }
  h += fbm2(hills_, fx * (1.0f / 280.0f), fz * (1.0f / 280.0f), 4) * (5.0f + 11.0f * inland) *
       (1.0f - 0.5f * s.mountain);
  h += fbm2(detail_, fx * (1.0f / 48.0f), fz * (1.0f / 48.0f), 3) * (1.2f + 3.0f * s.mountain);

  // Rivers follow the zero line of a noise field and cut valleys down to below sea level.
  const float riverFade = 1.0f - smoothstepf(0.35f, 0.75f, s.mountain);
  if (h > kSea - 3 && riverFade > 0.0f) {
    const float rv = std::fabs(fbm2(river_, px * (1.0f / 1300.0f), pz * (1.0f / 1300.0f), 3));
    constexpr float kChannel = 0.010f;
    constexpr float kValley = 0.075f;
    const float bed = static_cast<float>(kSea) - 3.0f - 3.0f * (1.0f - smoothstepf(0.0f, kChannel, rv));
    const float t = smoothstepf(kChannel * 0.5f, kValley, rv);
    const float carved = bed + (h - bed) * t;
    h = h + (std::min(h, carved) - h) * riverFade;
    s.river = (1.0f - t) * riverFade;
  }
  h = clampv(h, 8.0f, 240.0f);
  s.height = fastFloor(h);

  s.temperature = fbm2(temperature_, fx * (1.0f / 3200.0f), fz * (1.0f / 3200.0f), 3) * 1.25f +
                  patches_.noise2(fx * (1.0f / 190.0f), fz * (1.0f / 190.0f)) * 0.05f;
  s.temperature -= std::max(0.0f, h - static_cast<float>(kSea + 8)) * 0.0045f;
  s.humidity = fbm2(humidity_, fx * (1.0f / 2600.0f), fz * (1.0f / 2600.0f), 3) * 1.25f +
               patches_.noise2(fz * (1.0f / 170.0f), fx * (1.0f / 170.0f)) * 0.05f;

  const float t = s.temperature;
  const float hu = s.humidity;
  const int H = s.height;
  const float snowLine = 168.0f + patches_.noise2(fx * (1.0f / 60.0f), fz * (1.0f / 60.0f)) * 10.0f +
                         std::min(0.0f, t) * 45.0f;
  if (H < kSea) {
    if (s.continent < -0.10f)
      s.biome = t < -0.58f ? Biome::FrozenOcean : (H < kSea - 22 ? Biome::DeepOcean : Biome::Ocean);
    else if (s.river > 0.5f)
      s.biome = t < -0.45f ? Biome::FrozenRiver : Biome::River;
    else
      s.biome = t < -0.45f ? Biome::Tundra : Biome::Plains;  // inland lake
  } else if (H <= kSea + 2 && t > -0.45f && (s.continent < 0.06f || s.river > 0.6f)) {
    s.biome = Biome::Beach;
  } else if (static_cast<float>(H) >= snowLine) {
    s.biome = Biome::SnowyPeaks;
  } else if (s.mountain > 0.5f && H > kSea + 45) {
    s.biome = Biome::Mountains;
  } else if (t > 0.30f) {
    s.biome = hu < 0.0f ? Biome::Desert : Biome::Savanna;
  } else if (t > -0.22f) {
    if (hu < -0.12f)
      s.biome = Biome::Plains;
    else if (hu < 0.18f)
      s.biome = Biome::Forest;
    else if (hu < 0.38f)
      s.biome = Biome::BirchForest;
    else
      s.biome = Biome::DarkForest;
  } else if (t > -0.45f) {
    s.biome = Biome::Taiga;
  } else {
    s.biome = hu > -0.05f ? Biome::SnowyTaiga : Biome::Tundra;
  }
  return s;
}

BlockId WorldGenerator::surfaceBlock(const ColumnSample& c, int slope, int x, int z) const {
  const float p = patches_.noise2(static_cast<float>(x) * (1.0f / 24.0f), static_cast<float>(z) * (1.0f / 24.0f));
  if (c.height < kSea) {
    if (c.biome == Biome::River || c.biome == Biome::FrozenRiver)
      return p > 0.35f ? block::Gravel : (p < -0.45f ? block::Clay : block::Sand);
    if (c.height < kSea - 16) return p > -0.1f ? block::Gravel : block::Sand;
    return p > 0.5f ? block::Clay : (p < -0.55f ? block::Gravel : block::Sand);
  }
  switch (c.biome) {
    case Biome::Beach:
      return slope >= 3 ? (p > 0.0f ? block::Stone : block::Gravel) : block::Sand;
    case Biome::Desert:
      return slope >= 4 ? block::Sandstone : block::Sand;
    case Biome::SnowyPeaks:
      return slope >= 4 ? block::Stone : block::Snow;
    case Biome::Mountains:
      if (slope >= 3) return block::Stone;
      if (p > 0.45f) return block::Gravel;
      return c.temperature < -0.3f ? block::SnowyGrass : block::Grass;
    case Biome::Tundra:
    case Biome::SnowyTaiga:
    case Biome::FrozenRiver:
    case Biome::FrozenOcean:
      return slope >= 4 ? block::Stone : block::SnowyGrass;
    default:
      if (slope >= 4 && c.height > kSea + 4) return block::Stone;
      return block::Grass;
  }
}

WorldGenerator::CaveValues WorldGenerator::caveCorner(int x, int y, int z) const {
  const float fx = static_cast<float>(x);
  const float fy = static_cast<float>(y);
  const float fz = static_cast<float>(z);
  CaveValues v;
  v.a = cave1_.noise3(fx * (1.0f / 70.0f), fy * (1.0f / 44.0f), fz * (1.0f / 70.0f));
  v.b = cave2_.noise3(fx * (1.0f / 70.0f), fy * (1.0f / 44.0f), fz * (1.0f / 70.0f));
  v.c = fbm3(cave3_, fx * (1.0f / 110.0f), fy * (1.0f / 64.0f), fz * (1.0f / 110.0f), 2);
  return v;
}

float WorldGenerator::caveEntrance(int x, int z) const {
  return entrance_.noise2(static_cast<float>(x) * (1.0f / 90.0f), static_cast<float>(z) * (1.0f / 90.0f));
}

namespace {
inline float trilerp(float c000, float c100, float c010, float c110, float c001, float c101, float c011,
                     float c111, float fx, float fy, float fz) {
  const float x00 = c000 + (c100 - c000) * fx;
  const float x10 = c010 + (c110 - c010) * fx;
  const float x01 = c001 + (c101 - c001) * fx;
  const float x11 = c011 + (c111 - c011) * fx;
  const float y0 = x00 + (x10 - x00) * fy;
  const float y1 = x01 + (x11 - x01) * fy;
  return y0 + (y1 - y0) * fz;
}
}  // namespace

bool WorldGenerator::caveTest(const CaveValues& v, int x, int y, int z, int surface,
                              int minNeighbourSurface, float entrance) const {
  (void)x;
  (void)z;
  const int depth = surface - y;
  // Never open a cave next to water: it would leave a wall of floating water.
  if (y <= kSea && y > minNeighbourSurface) return false;
  if (surface < kSea + 2 && depth < 6) return false;
  const float radius = 0.0032f + (y < 40 ? 0.0012f : 0.0f);
  bool tunnel = v.a * v.a + v.b * v.b < radius;
  if (tunnel && depth < 6 && entrance < 0.35f) tunnel = false;
  bool cavern = false;
  if (depth > 12 && y < 72) {
    const float threshold = 0.60f + 0.004f * static_cast<float>(std::max(0, y - 40));
    cavern = v.c > threshold;
  }
  return tunnel || cavern;
}

bool WorldGenerator::isCarved(int x, int y, int z, int surface, int minNeighbourSurface) const {
  if (y < 5 || y > surface) return false;
  const int x0 = floorDiv(x, 4) * 4;
  const int y0 = (y >> 2) * 4;
  const int z0 = floorDiv(z, 4) * 4;
  const CaveValues c000 = caveCorner(x0, y0, z0), c100 = caveCorner(x0 + 4, y0, z0);
  const CaveValues c010 = caveCorner(x0, y0 + 4, z0), c110 = caveCorner(x0 + 4, y0 + 4, z0);
  const CaveValues c001 = caveCorner(x0, y0, z0 + 4), c101 = caveCorner(x0 + 4, y0, z0 + 4);
  const CaveValues c011 = caveCorner(x0, y0 + 4, z0 + 4), c111 = caveCorner(x0 + 4, y0 + 4, z0 + 4);
  const float fx = static_cast<float>(x - x0) * 0.25f;
  const float fy = static_cast<float>(y - y0) * 0.25f;
  const float fz = static_cast<float>(z - z0) * 0.25f;
  CaveValues v;
  v.a = trilerp(c000.a, c100.a, c010.a, c110.a, c001.a, c101.a, c011.a, c111.a, fx, fy, fz);
  v.b = trilerp(c000.b, c100.b, c010.b, c110.b, c001.b, c101.b, c011.b, c111.b, fx, fy, fz);
  v.c = trilerp(c000.c, c100.c, c010.c, c110.c, c001.c, c101.c, c011.c, c111.c, fx, fy, fz);
  return caveTest(v, x, y, z, surface, minNeighbourSurface, caveEntrance(x, z));
}

void WorldGenerator::climateColors(const ColumnSample& c, ColumnClimate& out) const {
  const float tt = saturate((c.temperature + 0.9f) / 1.8f);
  const float hh = saturate((c.humidity + 0.9f) / 1.8f);
  const vec3 coldDry(150, 165, 125), coldWet(100, 145, 105), hotDry(196, 180, 92), hotWet(78, 168, 52);
  vec3 g = lerp(lerp(coldDry, coldWet, hh), lerp(hotDry, hotWet, hh), tt);
  vec3 f = g * vec3(0.80f, 0.88f, 0.74f);
  if (c.biome == Biome::DarkForest) f = f * 0.78f;
  if (c.biome == Biome::SnowyPeaks || c.biome == Biome::Mountains) {
    const float l = (g.x + g.y + g.z) / 3.0f;
    g = lerp(g, vec3(l), 0.25f);
  }
  out.biome = static_cast<uint8_t>(c.biome);
  for (int i = 0; i < 3; ++i) {
    out.grass[i] = static_cast<uint8_t>(clampv(g[i], 0.0f, 255.0f));
    out.foliage[i] = static_cast<uint8_t>(clampv(f[i], 0.0f, 255.0f));
  }
}

void WorldGenerator::computeClimate(int cx, int cz, ColumnClimate* climate) const {
  for (int z = 0; z < 16; ++z)
    for (int x = 0; x < 16; ++x) climateColors(sampleColumn(cx * 16 + x, cz * 16 + z), climate[z * 16 + x]);
}

void WorldGenerator::generate(int cx, int cz, uint8_t* blocks, uint8_t* meta, ColumnClimate* climate) const {
  std::memset(blocks, 0, kChunkVolume);
  std::memset(meta, 0, kChunkVolume);
  const int ox = cx * 16;
  const int oz = cz * 16;

  // Column samples with a one block border (needed for slopes).
  ColumnSample cols[18 * 18];
  for (int z = -1; z <= 16; ++z)
    for (int x = -1; x <= 16; ++x) cols[(z + 1) * 18 + (x + 1)] = sampleColumn(ox + x, oz + z);
  auto col = [&](int x, int z) -> const ColumnSample& { return cols[(z + 1) * 18 + (x + 1)]; };

  int slope[256], minNeighbour[256];
  int maxSurface = 0;
  for (int z = 0; z < 16; ++z)
    for (int x = 0; x < 16; ++x) {
      const int h = col(x, z).height;
      const int hn[4] = {col(x + 1, z).height, col(x - 1, z).height, col(x, z + 1).height, col(x, z - 1).height};
      int s = 0, mn = hn[0];
      for (int k = 0; k < 4; ++k) {
        s = std::max(s, std::abs(h - hn[k]));
        mn = std::min(mn, hn[k]);
      }
      slope[z * 16 + x] = s;
      minNeighbour[z * 16 + x] = mn;
      maxSurface = std::max(maxSurface, h);
      climateColors(col(x, z), climate[z * 16 + x]);
    }

  Writer w{blocks, meta, ox, oz};

  // Terrain layers and water.
  for (int z = 0; z < 16; ++z)
    for (int x = 0; x < 16; ++x) {
      const ColumnSample& c = col(x, z);
      const int wx = ox + x, wz = oz + z;
      const int h = c.height;
      const BlockId top = surfaceBlock(c, slope[z * 16 + x], wx, wz);
      const uint64_t hs = hash2i(seed_ ^ kSaltSoil, wx, wz);
      const int soil = 3 + static_cast<int>(hs % 2);
      for (int y = 1; y <= h; ++y) blocks[chunkIndex(x, y, z)] = block::Stone;
      auto fill = [&](int from, int to, BlockId b) {
        for (int y = std::max(1, from); y <= to; ++y) blocks[chunkIndex(x, y, z)] = b;
      };
      switch (top) {
        case block::Grass:
        case block::SnowyGrass:
          fill(h - soil, h - 1, block::Dirt);
          break;
        case block::Sand: {
          const bool desert = c.biome == Biome::Desert;
          const int sand = desert ? 5 + static_cast<int>((hs >> 8) % 2) : 3;
          fill(h - sand, h - 1, block::Sand);
          fill(h - sand - (desert ? 4 : 2), h - sand - 1, block::Sandstone);
          break;
        }
        case block::Sandstone:
          fill(h - 3, h - 1, block::Sandstone);
          break;
        case block::Gravel:
          fill(h - 2, h - 1, block::Gravel);
          break;
        case block::Clay:
          fill(h - 2, h - 1, block::Clay);
          break;
        case block::Snow:
          fill(h - 2, h - 1, block::Snow);
          break;
        default:
          break;
      }
      blocks[chunkIndex(x, h, z)] = top;
      const bool frozen = c.biome == Biome::FrozenOcean || c.biome == Biome::FrozenRiver ||
                          (c.biome == Biome::Tundra && c.height < kSea);
      for (int y = h + 1; y <= kSea; ++y) blocks[chunkIndex(x, y, z)] = block::Water;
      if (frozen && h < kSea) blocks[chunkIndex(x, kSea, z)] = block::Ice;
    }

  // Caves: noise sampled on a 4x4x4 lattice and interpolated.
  const int gyCount = std::min(64, maxSurface / 4 + 1) + 1;
  std::vector<CaveValues> grid(static_cast<size_t>(5 * 5 * gyCount));
  auto gi = [&](int gx, int gy, int gz) { return static_cast<size_t>((gy * 5 + gz) * 5 + gx); };
  for (int gy = 0; gy < gyCount; ++gy)
    for (int gz = 0; gz < 5; ++gz)
      for (int gx = 0; gx < 5; ++gx) grid[gi(gx, gy, gz)] = caveCorner(ox + gx * 4, gy * 4, oz + gz * 4);
  for (int z = 0; z < 16; ++z)
    for (int x = 0; x < 16; ++x) {
      const int h = col(x, z).height;
      const float entrance = caveEntrance(ox + x, oz + z);
      const int gx = x >> 2, gz = z >> 2;
      const float fx = static_cast<float>(x & 3) * 0.25f;
      const float fz = static_cast<float>(z & 3) * 0.25f;
      for (int y = 5; y <= h; ++y) {
        const int gy = y >> 2;
        const float fy = static_cast<float>(y & 3) * 0.25f;
        const CaveValues& c000 = grid[gi(gx, gy, gz)];
        const CaveValues& c100 = grid[gi(gx + 1, gy, gz)];
        const CaveValues& c010 = grid[gi(gx, gy + 1, gz)];
        const CaveValues& c110 = grid[gi(gx + 1, gy + 1, gz)];
        const CaveValues& c001 = grid[gi(gx, gy, gz + 1)];
        const CaveValues& c101 = grid[gi(gx + 1, gy, gz + 1)];
        const CaveValues& c011 = grid[gi(gx, gy + 1, gz + 1)];
        const CaveValues& c111 = grid[gi(gx + 1, gy + 1, gz + 1)];
        CaveValues v;
        v.a = trilerp(c000.a, c100.a, c010.a, c110.a, c001.a, c101.a, c011.a, c111.a, fx, fy, fz);
        v.b = trilerp(c000.b, c100.b, c010.b, c110.b, c001.b, c101.b, c011.b, c111.b, fx, fy, fz);
        v.c = trilerp(c000.c, c100.c, c010.c, c110.c, c001.c, c101.c, c011.c, c111.c, fx, fy, fz);
        if (caveTest(v, ox + x, y, oz + z, h, minNeighbour[z * 16 + x], entrance))
          blocks[chunkIndex(x, y, z)] = y <= kLavaLevel ? block::Lava : block::Air;
      }
    }

  // Ore veins. Veins seeded by neighbouring chunks may reach into this one.
  for (int ncz = cz - 1; ncz <= cz + 1; ++ncz)
    for (int ncx = cx - 1; ncx <= cx + 1; ++ncx) {
      Pcg32 rng(hash2i(seed_ ^ kSaltOre, ncx, ncz));
      for (const OreDef& ore : kOres)
        for (int v = 0; v < ore.veins; ++v) {
          const float x0 = static_cast<float>(ncx * 16) + rng.unit() * 16.0f;
          const float y0 = static_cast<float>(ore.minY) + rng.unit() * static_cast<float>(ore.maxY - ore.minY);
          const float z0 = static_cast<float>(ncz * 16) + rng.unit() * 16.0f;
          const float yaw = rng.unit() * kTwoPi;
          const float pitch = (rng.unit() - 0.5f) * 0.8f;
          const float len = static_cast<float>(ore.size) * 0.45f;
          const float radius = 0.55f + static_cast<float>(ore.size) / 14.0f;
          const float ext = len * 0.5f + radius + 1.0f;
          if (x0 + ext < ox || x0 - ext > ox + 16 || z0 + ext < oz || z0 - ext > oz + 16) continue;
          const vec3 dir(std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch));
          const int steps = std::max(2, ore.size / 2);
          for (int i = 0; i < steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps - 1);
            const vec3 c = vec3(x0, y0, z0) + dir * (len * (t - 0.5f));
            const float r = radius * (0.6f + 0.4f * std::sin(kPi * t));
            const int ri = static_cast<int>(std::ceil(r));
            for (int yy = fastFloor(c.y) - ri; yy <= fastFloor(c.y) + ri; ++yy)
              for (int zz = fastFloor(c.z) - ri; zz <= fastFloor(c.z) + ri; ++zz)
                for (int xx = fastFloor(c.x) - ri; xx <= fastFloor(c.x) + ri; ++xx) {
                  if (!w.inside(xx, yy, zz)) continue;
                  const vec3 d = vec3(static_cast<float>(xx) + 0.5f, static_cast<float>(yy) + 0.5f,
                                      static_cast<float>(zz) + 0.5f) - c;
                  if (dot(d, d) > r * r) continue;
                  if (w.get(xx, yy, zz) == block::Stone) w.put(xx, yy, zz, ore.block);
                }
          }
        }
    }

  // Trees on a jittered 5x5 grid. Trees rooted in neighbouring chunks are
  // replayed so canopies continue seamlessly across chunk borders.
  constexpr int kCell = 5;
  constexpr int kReach = 4;
  for (int gz = floorDiv(oz - kReach, kCell); gz <= floorDiv(oz + 15 + kReach, kCell); ++gz)
    for (int gx = floorDiv(ox - kReach, kCell); gx <= floorDiv(ox + 15 + kReach, kCell); ++gx) {
      const uint64_t th = hash2i(seed_ ^ kSaltTree, gx, gz);
      const int tx = gx * kCell + static_cast<int>(th % kCell);
      const int tz = gz * kCell + static_cast<int>((th >> 8) % kCell);
      if (tx < ox - kReach || tx > ox + 15 + kReach || tz < oz - kReach || tz > oz + 15 + kReach) continue;
      const ColumnSample c = sampleColumn(tx, tz);
      float density = 0.0f;
      switch (c.biome) {
        case Biome::Plains: density = 0.035f; break;
        case Biome::Savanna: density = 0.05f; break;
        case Biome::Forest: density = 0.72f; break;
        case Biome::BirchForest: density = 0.72f; break;
        case Biome::DarkForest: density = 0.95f; break;
        case Biome::Taiga: density = 0.62f; break;
        case Biome::SnowyTaiga: density = 0.5f; break;
        case Biome::Tundra: density = 0.025f; break;
        case Biome::Mountains: density = c.height < 150 ? 0.22f : 0.0f; break;
        default: density = 0.0f; break;
      }
      if (density <= 0.0f || c.height <= kSea || c.height > 235) continue;
      // Clearings and denser groves.
      density *= saturate(0.65f + 0.6f * patches_.noise2(static_cast<float>(tx) * (1.0f / 140.0f) + 50.0f,
                                                          static_cast<float>(tz) * (1.0f / 140.0f)));
      if (hashToUnit(splitmix64(th)) >= density) continue;
      const ColumnSample n[4] = {sampleColumn(tx + 1, tz), sampleColumn(tx - 1, tz), sampleColumn(tx, tz + 1),
                                 sampleColumn(tx, tz - 1)};
      int s = 0, mn = n[0].height;
      for (const ColumnSample& nc : n) {
        s = std::max(s, std::abs(c.height - nc.height));
        mn = std::min(mn, nc.height);
      }
      const BlockId top = surfaceBlock(c, s, tx, tz);
      if (top != block::Grass && top != block::SnowyGrass) continue;
      if (isCarved(tx, c.height, tz, c.height, mn)) continue;

      Pcg32 rng(hash2i(seed_ ^ kSaltTreeShape, tx, tz));
      const int base = c.height + 1;
      const float pick = rng.unit();
      switch (c.biome) {
        case Biome::Forest:
          if (pick < 0.7f) oakTree(w, tx, base, tz, rng, block::OakLog, block::OakLeaves, 4, 6);
          else oakTree(w, tx, base, tz, rng, block::BirchLog, block::BirchLeaves, 5, 7);
          break;
        case Biome::BirchForest:
          if (pick < 0.85f) oakTree(w, tx, base, tz, rng, block::BirchLog, block::BirchLeaves, 5, 7);
          else oakTree(w, tx, base, tz, rng, block::OakLog, block::OakLeaves, 4, 6);
          break;
        case Biome::DarkForest:
          if (pick < 0.35f) bigOakTree(w, tx, base, tz, rng);
          else oakTree(w, tx, base, tz, rng, block::OakLog, block::OakLeaves, 5, 7);
          break;
        case Biome::Taiga:
        case Biome::SnowyTaiga:
        case Biome::Tundra:
        case Biome::Mountains:
          spruceTree(w, tx, base, tz, rng);
          break;
        case Biome::Savanna:
          oakTree(w, tx, base, tz, rng, block::OakLog, block::OakLeaves, 4, 5);
          break;
        default:
          if (pick < 0.12f) bigOakTree(w, tx, base, tz, rng);
          else oakTree(w, tx, base, tz, rng, block::OakLog, block::OakLeaves, 4, 6);
          break;
      }
    }

  // Ground cover.
  for (int z = 0; z < 16; ++z)
    for (int x = 0; x < 16; ++x) {
      const ColumnSample& c = col(x, z);
      const int h = c.height;
      if (h <= kSea - 1 || h >= kChunkHeight - 4) continue;
      const BlockId top = blocks[chunkIndex(x, h, z)];
      if (blocks[chunkIndex(x, h + 1, z)] != block::Air) continue;
      const int wx = ox + x, wz = oz + z;
      const uint64_t ph = hash2i(seed_ ^ kSaltPlant, wx, wz);
      const float r = hashToUnit(ph);
      const float cluster = 0.5f + 0.5f * patches_.noise2(static_cast<float>(wx) * (1.0f / 18.0f) - 31.0f,
                                                         static_cast<float>(wz) * (1.0f / 18.0f));
      const BlockId flower = (ph >> 20) & 1 ? block::RedFlower : block::YellowFlower;
      BlockId plant = block::Air;
      if (top == block::Grass || top == block::SnowyGrass) {
        switch (c.biome) {
          case Biome::Plains:
            if (r < 0.24f * cluster + 0.04f) plant = block::TallGrass;
            else if (r < 0.30f * cluster + 0.04f && cluster > 0.6f) plant = flower;
            break;
          case Biome::Savanna:
            if (r < 0.32f) plant = block::TallGrass;
            break;
          case Biome::Forest:
          case Biome::BirchForest:
            if (r < 0.11f) plant = block::TallGrass;
            else if (r < 0.14f) plant = block::Fern;
            else if (r < 0.15f) plant = flower;
            break;
          case Biome::DarkForest:
            if (r < 0.06f) plant = block::TallGrass;
            else if (r < 0.13f) plant = block::Fern;
            break;
          case Biome::Taiga:
          case Biome::SnowyTaiga:
            if (r < 0.09f) plant = block::Fern;
            else if (r < 0.13f) plant = block::TallGrass;
            break;
          case Biome::Tundra:
            if (r < 0.02f) plant = block::TallGrass;
            break;
          case Biome::Mountains:
            if (r < 0.07f) plant = block::TallGrass;
            break;
          default:
            if (r < 0.05f) plant = block::TallGrass;
            break;
        }
      } else if (top == block::Sand && c.biome == Biome::Desert) {
        if (r < 0.006f) {
          plant = block::DeadBush;
        } else if (r < 0.011f) {
          const int height = 1 + static_cast<int>((ph >> 24) % 3);
          for (int i = 1; i <= height; ++i)
            if (blocks[chunkIndex(x, h + i, z)] == block::Air) blocks[chunkIndex(x, h + i, z)] = block::Cactus;
          continue;
        }
      }
      if (plant != block::Air) blocks[chunkIndex(x, h + 1, z)] = plant;
    }

  // Rough baserock floor.
  for (int z = 0; z < 16; ++z)
    for (int x = 0; x < 16; ++x) {
      blocks[chunkIndex(x, 0, z)] = block::Baserock;
      for (int y = 1; y <= 3; ++y) {
        const float p = hashToUnit(hash3i(seed_ ^ kSaltBase, ox + x, y, oz + z));
        if (p < 0.75f - 0.25f * static_cast<float>(y)) blocks[chunkIndex(x, y, z)] = block::Baserock;
      }
    }
}

ivec3 WorldGenerator::findSpawn() const {
  // Walk square rings around the origin until a dry, flat, uncarved column turns up.
  auto suitable = [this](int x, int z) {
    const ColumnSample c = sampleColumn(x, z);
    if (c.height <= kSea + 1 || c.height > 150) return false;
    if (c.biome == Biome::Beach || c.biome == Biome::River || c.biome == Biome::SnowyPeaks ||
        c.biome == Biome::Mountains || c.biome == Biome::Desert)
      return false;
    int mn = c.height;
    const int near[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& o : near) {
      const int h = sampleColumn(x + o[0], z + o[1]).height;
      if (std::abs(h - c.height) > 1) return false;
      mn = std::min(mn, h);
    }
    const int far[4][2] = {{3, 0}, {-3, 0}, {0, 3}, {0, -3}};
    for (const auto& o : far)
      if (std::abs(sampleColumn(x + o[0], z + o[1]).height - c.height) > 2) return false;
    return !isCarved(x, c.height, z, c.height, mn);
  };
  if (suitable(0, 0)) return {0, sampleColumn(0, 0).height + 1, 0};
  for (int ring = 16; ring <= 2048; ring += 16) {
    for (int t = -ring; t < ring; t += 16) {
      const int pts[4][2] = {{t, -ring}, {ring, t}, {-t, ring}, {-ring, -t}};
      for (const auto& p : pts)
        if (suitable(p[0], p[1])) return {p[0], sampleColumn(p[0], p[1]).height + 1, p[1]};
    }
  }
  return {0, sampleColumn(0, 0).height + 1, 0};
}

}  // namespace strata
