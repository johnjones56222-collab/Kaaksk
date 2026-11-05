#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <thread>
#include <vector>

#include "core/image_io.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/random.hpp"
#include "core/thread_pool.hpp"
#include "game/input.hpp"
#include "game/player.hpp"
#include "game/settings.hpp"
#include "game/touch.hpp"
#include "test_framework.hpp"
#include "world/block.hpp"
#include "world/chunk.hpp"
#include "world/items.hpp"
#include "world/lighting.hpp"
#include "world/mesher.hpp"
#include "world/raycast.hpp"
#include "world/storage.hpp"
#include "world/world.hpp"
#include "world/worldgen.hpp"

using namespace strata;
namespace fs = std::filesystem;

namespace {

fs::path tempDir(const char* name) {
  fs::path p = fs::temp_directory_path() / (std::string("strata_test_") + name);
  std::error_code ec;
  fs::remove_all(p, ec);
  fs::create_directories(p, ec);
  return p;
}

// A rectangular block of generated chunks addressed in world coordinates.
struct Region {
  int cx0, cz0, n;
  std::vector<std::vector<uint8_t>> blocks, meta, light;
  std::vector<bool> lit;

  Region(int cx0_, int cz0_, int n_) : cx0(cx0_), cz0(cz0_), n(n_) {
    blocks.assign(static_cast<size_t>(n * n), std::vector<uint8_t>(kChunkVolume));
    meta.assign(static_cast<size_t>(n * n), std::vector<uint8_t>(kChunkVolume));
    light.assign(static_cast<size_t>(n * n), std::vector<uint8_t>(kChunkVolume));
    lit.assign(static_cast<size_t>(n * n), false);
  }
  int chunkSlot(int cx, int cz) const {
    const int ix = cx - cx0, iz = cz - cz0;
    if (ix < 0 || iz < 0 || ix >= n || iz >= n) return -1;
    return iz * n + ix;
  }
  // Slot that must exist (aborts the test run otherwise).
  size_t slotChecked(int cx, int cz) const {
    const int s = chunkSlot(cx, cz);
    if (s < 0) {
      std::fprintf(stderr, "chunk %d,%d outside the test region\n", cx, cz);
      std::abort();
    }
    return static_cast<size_t>(s);
  }
  void generate(const WorldGenerator& gen) {
    ColumnClimate climate[256];
    for (int iz = 0; iz < n; ++iz)
      for (int ix = 0; ix < n; ++ix)
        gen.generate(cx0 + ix, cz0 + iz, blocks[iz * n + ix].data(), meta[iz * n + ix].data(), climate);
  }
  int slotOf(int x, int z) const { return chunkSlot(floorDiv(x, 16), floorDiv(z, 16)); }
  BlockId get(int x, int y, int z) const {
    if (y < 0 || y >= kChunkHeight) return block::Air;
    const int s = slotOf(x, z);
    if (s < 0) return block::Air;
    return blocks[static_cast<size_t>(s)][chunkIndex(floorMod(x, 16), y, floorMod(z, 16))];
  }
  void set(int x, int y, int z, BlockId b) {
    blocks[slotChecked(floorDiv(x, 16), floorDiv(z, 16))][chunkIndex(floorMod(x, 16), y, floorMod(z, 16))] = b;
  }
  void computeLight(int cx, int cz) {
    const uint8_t* nb[9];
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) nb[(dz + 1) * 3 + (dx + 1)] = blocks[slotChecked(cx + dx, cz + dz)].data();
    computeChunkLight(nb, light[slotChecked(cx, cz)].data());
    lit[slotChecked(cx, cz)] = true;
  }
  // Light accessor interface for relightAt.
  bool valid(int x, int, int z) const {
    const int s = slotOf(x, z);
    return s >= 0 && lit[static_cast<size_t>(s)];
  }
  BlockId block(int x, int y, int z) const { return get(x, y, z); }
  uint8_t light_(int x, int y, int z) const {
    return light[slotChecked(floorDiv(x, 16), floorDiv(z, 16))][chunkIndex(floorMod(x, 16), y, floorMod(z, 16))];
  }
};

struct RegionAccess {
  Region& r;
  bool valid(int x, int y, int z) const { return r.valid(x, y, z); }
  BlockId block(int x, int y, int z) const { return r.get(x, y, z); }
  uint8_t light(int x, int y, int z) const { return r.light_(x, y, z); }
  void setLight(int x, int y, int z, uint8_t v) {
    r.light[r.slotChecked(floorDiv(x, 16), floorDiv(z, 16))][chunkIndex(floorMod(x, 16), y, floorMod(z, 16))] = v;
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// Math
// ---------------------------------------------------------------------------
TEST(math_floor_div_mod) {
  CHECK(floorDiv(-1, 16) == -1);
  CHECK(floorDiv(-16, 16) == -1);
  CHECK(floorDiv(-17, 16) == -2);
  CHECK(floorDiv(15, 16) == 0);
  CHECK(floorMod(-1, 16) == 15);
  CHECK(floorMod(-16, 16) == 0);
  CHECK(floorMod(17, 16) == 1);
  CHECK(fastFloor(-0.5f) == -1);
  CHECK(fastFloor(-1.0f) == -1);
  CHECK(fastFloor(2.999f) == 2);
  CHECK(fastFloor(-2.0) == -2);
}

TEST(math_matrix_inverse) {
  const mat4 p = perspective(radians(70.0f), 16.0f / 9.0f, 0.05f, 500.0f);
  const mat4 v = lookAt(vec3(3, 70, -4), vec3(10, 65, 20), vec3(0, 1, 0));
  const mat4 m = p * v;
  const mat4 inv = inverse(m);
  const mat4 id = m * inv;
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) CHECK_NEAR(id.at(r, c), r == c ? 1.0f : 0.0f, 1e-3f);
  const mat4 rot = rotation(vec3(0.3f, 1.0f, -0.2f), 1.1f);
  const mat4 rid = rot * inverse(rot);
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) CHECK_NEAR(rid.at(r, c), r == c ? 1.0f : 0.0f, 1e-5f);
}

TEST(math_projection_and_frustum) {
  const mat4 p = perspective(radians(90.0f), 1.0f, 0.1f, 100.0f);
  const mat4 v = lookAt(vec3(0, 0, 0), vec3(0, 0, -1), vec3(0, 1, 0));
  const mat4 vp = p * v;
  vec4 c = vp * vec4(0, 0, -0.1f, 1);
  CHECK_NEAR(c.z / c.w, -1.0f, 1e-4f);
  c = vp * vec4(0, 0, -100.0f, 1);
  CHECK_NEAR(c.z / c.w, 1.0f, 1e-4f);
  const Frustum f = Frustum::fromMatrix(vp);
  CHECK(f.intersectsAabb(vec3(-1, -1, -11), vec3(1, 1, -9)));
  CHECK(!f.intersectsAabb(vec3(-1, -1, 9), vec3(1, 1, 11)));        // behind
  CHECK(!f.intersectsAabb(vec3(50, -1, -11), vec3(52, 1, -9)));     // far to the right
  CHECK(!f.intersectsAabb(vec3(-1, -1, -300), vec3(1, 1, -200)));   // beyond far plane
  CHECK(f.intersectsAabb(vec3(-1000, -1000, -50), vec3(1000, 1000, -40)));  // huge box
}

// ---------------------------------------------------------------------------
// Random and noise
// ---------------------------------------------------------------------------
TEST(random_determinism) {
  Pcg32 a(42, 7), b(42, 7), c(43, 7);
  bool differs = false;
  for (int i = 0; i < 1000; ++i) {
    const uint32_t x = a.next();
    CHECK(x == b.next());
    if (x != c.next()) differs = true;
  }
  CHECK(differs);
  Pcg32 r(1);
  int counts[6] = {0};
  for (int i = 0; i < 60000; ++i) {
    const int v = r.range(0, 5);
    CHECK(v >= 0 && v <= 5);
    counts[v]++;
  }
  for (int i = 0; i < 6; ++i) CHECK(counts[i] > 9000 && counts[i] < 11000);
  CHECK(hash3i(1, 2, 3, 4) == hash3i(1, 2, 3, 4));
  CHECK(hash3i(1, 2, 3, 4) != hash3i(1, 2, 4, 3));
  CHECK(hash2i(5, -1, 0) != hash2i(5, 0, -1));
}

TEST(noise_range_and_determinism) {
  SimplexNoise n1(1234), n2(1234), n3(999);
  float mn = 10, mx = -10;
  double sum = 0;
  bool differs = false;
  for (int i = 0; i < 20000; ++i) {
    const float x = static_cast<float>(i) * 0.137f - 300.0f, y = static_cast<float>(i % 97) * 0.71f;
    const float v = n1.noise2(x, y);
    const float w = n1.noise3(x, y, x * 0.3f);
    CHECK(v == n2.noise2(x, y));
    CHECK(w == n2.noise3(x, y, x * 0.3f));
    if (v != n3.noise2(x, y)) differs = true;
    mn = std::min(mn, std::min(v, w));
    mx = std::max(mx, std::max(v, w));
    sum += v;
  }
  CHECK(differs);
  CHECK(mn >= -1.05f && mx <= 1.05f);
  CHECK(mn < -0.6f && mx > 0.6f);
  CHECK(std::fabs(sum / 20000.0) < 0.05);
  const float r = ridged2(n1, 3.3f, 4.4f, 5);
  CHECK(r >= 0.0f && r <= 1.0f);
}

TEST(thread_pool_runs_everything) {
  ThreadPool pool(4);
  std::atomic<int> count{0};
  for (int i = 0; i < 1000; ++i) pool.submit(static_cast<float>(i % 7), [&count] { count.fetch_add(1); });
  pool.waitIdle();
  CHECK(count.load() == 1000);
  CHECK(pool.pending() == 0);
}

TEST(thread_pool_inline_mode) {
  // Zero threads: nothing runs until runPending, which runs the most urgent first.
  ThreadPool pool(0);
  CHECK(pool.inlineMode());
  std::vector<int> order;
  for (int i = 0; i < 50; ++i) pool.submit(static_cast<float>(50 - i), [&order, i] { order.push_back(i); });
  CHECK(order.empty());
  CHECK(pool.pending() == 50);
  CHECK(pool.runPending(0.0) == 1);  // at least one task even with no time
  CHECK(order.size() == 1 && order[0] == 49);
  // Tasks may submit more work; it runs in the same call when time allows.
  pool.submit(-1.0f, [&pool, &order] { pool.submit(-2.0f, [&order] { order.push_back(1000); }); });
  pool.waitIdle();
  CHECK(pool.pending() == 0);
  CHECK(order.size() == 51);
  CHECK(order[1] == 1000 || order[2] == 1000);
  for (size_t i = 3; i < order.size(); ++i) CHECK(order[i] < order[i - 1]);
}

// ---------------------------------------------------------------------------
// Blocks, chunks, items
// ---------------------------------------------------------------------------
TEST(block_table_sane) {
  for (int i = 0; i < block::Count; ++i) {
    const BlockInfo& b = blockInfo(static_cast<BlockId>(i));
    CHECK(b.name != nullptr);
    for (uint16_t t : b.tex) CHECK(t < tex::Count);
    if (b.opaque) CHECK(b.shape == Shape::Cube && b.layer == Layer::Opaque);
    CHECK(lightOpacityTable()[i] <= 15);
    if (b.opaque) CHECK(lightOpacityTable()[i] == 15);
    if (b.drop != 0) CHECK(isValidItem(b.drop));
  }
  CHECK(blockInfo(200).shape == Shape::None);  // unused ids behave as air
  CHECK(canSurviveOn(block::Torch, block::Stone));
  CHECK(!canSurviveOn(block::Torch, block::OakLeaves));
  CHECK(!canSurviveOn(block::Torch, block::Air));
  CHECK(canSurviveOn(block::TallGrass, block::Grass));
  CHECK(!canSurviveOn(block::TallGrass, block::Stone));
  CHECK(canSurviveOn(block::Cactus, block::Sand));
}

TEST(chunk_storage) {
  Chunk c(3, -2);
  CHECK(c.block(5, 100, 5) == block::Air);
  CHECK(c.height(5, 5) == -1);
  CHECK(c.sectionEmpty(6));
  c.setBlock(5, 100, 5, block::Stone);
  CHECK(c.block(5, 100, 5) == block::Stone);
  CHECK(c.height(5, 5) == 100);
  CHECK(!c.sectionEmpty(6));
  c.setBlock(5, 120, 5, block::Dirt);
  CHECK(c.height(5, 5) == 120);
  c.setBlock(5, 120, 5, block::Air);
  CHECK(c.height(5, 5) == 100);
  c.setBlock(5, 100, 5, block::Water, 3);
  CHECK(c.meta(5, 100, 5) == 3);
  c.setBlock(5, 100, 5, block::Air, 3);
  CHECK(c.meta(5, 100, 5) == 0);  // air never keeps metadata
  CHECK(c.sectionEmpty(6));

  std::vector<uint8_t> blocks(kChunkVolume, 0), meta(kChunkVolume, 0), out(kChunkVolume);
  for (int i = 0; i < kChunkVolume; i += 7) blocks[i] = static_cast<uint8_t>(1 + (i % 5));
  for (int i = 0; i < kChunkVolume; i += 7) meta[i] = static_cast<uint8_t>(i % 3);
  c.assign(blocks.data(), meta.data());
  c.copyBlocks(out.data());
  CHECK(out == blocks);
  c.copyMeta(out.data());
  CHECK(out == meta);

  // Uniform light in an empty section is stored without allocating it.
  Chunk e(0, 0);
  std::vector<uint8_t> light(kChunkVolume, packLight(15, 0));
  for (int i = 0; i < kSectionVolume; ++i) light[static_cast<size_t>(3 * kSectionVolume + i)] = packLight(7, 2);
  e.applyLight(light.data());
  CHECK(e.section(3) == nullptr);
  CHECK(e.light(0, 50, 0) == packLight(7, 2));
  CHECK(e.light(0, 10, 0) == packLight(15, 0));
  light[static_cast<size_t>(3 * kSectionVolume + 5)] = packLight(1, 1);
  e.applyLight(light.data());
  CHECK(e.section(3) != nullptr);
  CHECK(e.light(5, 48, 0) == packLight(1, 1));
}

TEST(inventory_and_crafting) {
  Inventory inv;
  CHECK(inv.add(block::Stone, 100) == 0);
  CHECK(inv.countOf(block::Stone) == 100);
  CHECK(inv.slots[0].count == 64 && inv.slots[1].count == 36);
  CHECK(inv.add(item::Coal, 3) == 0);
  CHECK(!inv.remove(block::Stone, 101));
  CHECK(inv.countOf(block::Stone) == 100);
  CHECK(inv.remove(block::Stone, 40));
  CHECK(inv.countOf(block::Stone) == 60);
  CHECK(inv.add(block::OakLog, 1) == 0);
  const std::vector<Recipe>& rs = recipes();
  const Recipe* planks = nullptr;
  const Recipe* sticks = nullptr;
  const Recipe* torches = nullptr;
  for (const Recipe& r : rs) {
    if (r.output.id == block::Planks && r.inputs[0].id == block::OakLog) planks = &r;
    if (r.output.id == item::Stick) sticks = &r;
    if (r.output.id == block::Torch) torches = &r;
  }
  CHECK(planks && sticks && torches);
  CHECK(craft(inv, *planks));
  CHECK(inv.countOf(block::Planks) == 4 && inv.countOf(block::OakLog) == 0);
  CHECK(!craft(inv, *planks));
  CHECK(craft(inv, *sticks));
  CHECK(inv.countOf(item::Stick) == 4);
  CHECK(craft(inv, *torches));
  CHECK(inv.countOf(block::Torch) == 4 && inv.countOf(item::Coal) == 2);
  // A full inventory refuses to craft without losing ingredients...
  Inventory full;
  for (int i = 0; i < Inventory::kSlots; ++i) full.slots[i] = {block::Dirt, 64};
  full.slots[5] = {block::OakLog, 2};
  CHECK(!craft(full, *planks));
  CHECK(full.countOf(block::OakLog) == 2);
  // ...but using up an ingredient stack frees its slot for the result.
  full.slots[5] = {block::OakLog, 1};
  CHECK(craft(full, *planks));
  CHECK(full.countOf(block::Planks) == 4 && full.countOf(block::OakLog) == 0);
}

// ---------------------------------------------------------------------------
// Storage
// ---------------------------------------------------------------------------
TEST(rle_round_trip) {
  Pcg32 rng(9);
  for (int trial = 0; trial < 20; ++trial) {
    std::vector<uint8_t> data(static_cast<size_t>(rng.range(1, 300000)));
    const int mode = trial % 3;
    for (size_t i = 0; i < data.size(); ++i)
      data[i] = mode == 0 ? static_cast<uint8_t>(rng.next()) : mode == 1 ? static_cast<uint8_t>(i / 1000) : 7;
    const std::vector<uint8_t> enc = rleEncode(data.data(), data.size());
    std::vector<uint8_t> dec(data.size());
    CHECK(rleDecode(enc.data(), enc.size(), dec.data(), dec.size()));
    CHECK(dec == data);
    std::vector<uint8_t> small(data.size() - 1 + 1);
    if (data.size() > 1) CHECK(!rleDecode(enc.data(), enc.size(), small.data(), data.size() - 1));
  }
  const uint8_t garbage[] = {1, 0x80, 0x80, 0x80, 0x80, 0x80};
  uint8_t out[16];
  CHECK(!rleDecode(garbage, sizeof(garbage), out, sizeof(out)));
}

TEST(chunk_and_level_save_load) {
  const fs::path dir = tempDir("storage");
  WorldStorage storage(dir);
  CHECK(storage.ok());
  std::vector<uint8_t> blocks(kChunkVolume), meta(kChunkVolume), b2(kChunkVolume), m2(kChunkVolume);
  WorldGenerator gen(77);
  ColumnClimate climate[256];
  gen.generate(-3, 5, blocks.data(), meta.data(), climate);
  meta[1234] = 5;
  CHECK(storage.saveChunk(-3, 5, blocks.data(), meta.data()));
  CHECK(storage.loadChunk(-3, 5, b2.data(), m2.data()));
  CHECK(b2 == blocks);
  CHECK(m2 == meta);
  CHECK(!storage.loadChunk(4, 4, b2.data(), m2.data()));
  // Corrupt one byte: the checksum must reject the file.
  const fs::path file = dir / "chunks" / "c.-3.5.dat";
  {
    std::fstream f(file, std::ios::in | std::ios::out | std::ios::binary);
    f.seekp(40);
    char c = 0;
    f.read(&c, 1);
    f.seekp(40);
    c = static_cast<char>(c ^ 0x5A);
    f.write(&c, 1);
  }
  CHECK(!storage.loadChunk(-3, 5, b2.data(), m2.data()));

  LevelData level;
  level.seed = 123456789012345ull;
  level.worldTime = 98765;
  level.creative = true;
  level.hasPlayer = true;
  level.playerPos = {12.25, 70.5, -3.125};
  level.yaw = 1.5f;
  level.pitch = -0.25f;
  level.flying = true;
  level.health = 13.5f;
  level.hasSpawn = true;
  level.spawn = {4, 80, -9};
  level.selectedSlot = 4;
  level.inventory.slots[3] = {block::Torch, 17};
  level.inventory.slots[30] = {item::Coal, 2};
  CHECK(storage.saveLevel(level));
  LevelData loaded;
  CHECK(storage.loadLevel(loaded));
  CHECK(loaded.seed == level.seed);
  CHECK(loaded.worldTime == level.worldTime);
  CHECK(loaded.creative && loaded.hasPlayer && loaded.flying);
  CHECK(loaded.playerPos.x == 12.25 && loaded.playerPos.y == 70.5 && loaded.playerPos.z == -3.125);
  CHECK(loaded.yaw == 1.5f && loaded.pitch == -0.25f);
  CHECK(loaded.health == 13.5f);
  CHECK(loaded.hasSpawn && loaded.spawn == level.spawn);
  CHECK(loaded.selectedSlot == 4);
  CHECK(loaded.inventory.slots[3].id == block::Torch && loaded.inventory.slots[3].count == 17);
  CHECK(loaded.inventory.slots[30].id == item::Coal && loaded.inventory.slots[30].count == 2);
  fs::remove_all(dir);
}

// ---------------------------------------------------------------------------
// World generation
// ---------------------------------------------------------------------------
TEST(worldgen_deterministic_across_threads) {
  WorldGenerator gen(2024);
  std::vector<uint8_t> a(kChunkVolume), am(kChunkVolume), b(kChunkVolume), bm(kChunkVolume);
  ColumnClimate ca[256], cb[256];
  gen.generate(7, -11, a.data(), am.data(), ca);
  std::thread t([&] {
    WorldGenerator other(2024);
    // Generate other chunks first so any hidden state would show up.
    std::vector<uint8_t> tmp(kChunkVolume), tm(kChunkVolume);
    ColumnClimate tc[256];
    other.generate(0, 0, tmp.data(), tm.data(), tc);
    other.generate(7, -11, b.data(), bm.data(), cb);
  });
  t.join();
  CHECK(a == b);
  CHECK(am == bm);
  CHECK(std::memcmp(ca, cb, sizeof(ca)) == 0);
  WorldGenerator different(2025);
  different.generate(7, -11, b.data(), bm.data(), cb);
  CHECK(a != b);
}

namespace {
void checkWorldgenInvariants(uint64_t seed, int& totalLeaves, int& totalPlants) {
  // A 6x6 chunk area around the spawn; checks on the inner 4x4 so every neighbour exists.
  WorldGenerator gen(seed);
  const ivec3 spawn = gen.findSpawn();
  Region r(floorDiv(spawn.x, 16) - 3, floorDiv(spawn.z, 16) - 3, 6);
  r.generate(gen);
  const int x0 = (r.cx0 + 1) * 16, x1 = (r.cx0 + r.n - 1) * 16;
  const int z0 = (r.cz0 + 1) * 16, z1 = (r.cz0 + r.n - 1) * 16;
  int water = 0, leaves = 0, logs = 0, plants = 0, ores = 0, floatingWater = 0, lavaWater = 0, orphanLeaves = 0;
  int badPlants = 0;
  std::map<int, int> counts;
  for (int x = x0; x < x1; ++x)
    for (int z = z0; z < z1; ++z) {
      CHECK(r.get(x, 0, z) == block::Baserock);
      for (int y = 1; y < kChunkHeight; ++y) {
        const BlockId b = r.get(x, y, z);
        counts[b]++;
        if (b == block::Water) {
          ++water;
          const BlockId around[5] = {r.get(x + 1, y, z), r.get(x - 1, y, z), r.get(x, y, z + 1), r.get(x, y, z - 1),
                                     r.get(x, y - 1, z)};
          for (BlockId n : around) {
            if (n == block::Air) ++floatingWater;
            if (n == block::Lava) ++lavaWater;
          }
        } else if (b == block::OakLeaves || b == block::BirchLeaves || b == block::SpruceLeaves) {
          ++leaves;
          bool near = false;
          for (int dy = -6; dy <= 2 && !near; ++dy)
            for (int dz = -5; dz <= 5 && !near; ++dz)
              for (int dx = -5; dx <= 5 && !near; ++dx) {
                const BlockId n = r.get(x + dx, y + dy, z + dz);
                near = n == block::OakLog || n == block::BirchLog || n == block::SpruceLog;
              }
          if (!near) ++orphanLeaves;
        } else if (b == block::OakLog || b == block::BirchLog || b == block::SpruceLog) {
          ++logs;
        } else if (blockInfo(b).needsSupport) {
          ++plants;
          if (!canSurviveOn(b, r.get(x, y - 1, z))) ++badPlants;
        } else if (b >= block::CoalOre && b <= block::DiamondOre) {
          ++ores;
        }
      }
    }
  std::printf("  seed %llu: water %d leaves %d logs %d plants %d ores %d\n", static_cast<unsigned long long>(seed),
              water, leaves, logs, plants, ores);
  CHECK(floatingWater == 0);
  CHECK(lavaWater == 0);
  CHECK(orphanLeaves == 0);
  CHECK(badPlants == 0);
  CHECK(ores > 0);
  CHECK(counts[block::Stone] > 0);
  totalLeaves += leaves;
  totalPlants += plants;
}
}  // namespace

TEST(worldgen_invariants) {
  int leaves = 0, plants = 0;
  for (uint64_t seed : {31337ull, 1ull, 77ull, 2024ull, 9001ull, 424242ull})
    checkWorldgenInvariants(seed, leaves, plants);
  // Make sure the invariants were exercised on real vegetation.
  CHECK(leaves > 1000);
  CHECK(plants > 100);
}

TEST(worldgen_trees_cross_chunk_borders) {
  // Generate a strip twice: once chunk by chunk, once with the chunks in a
  // different order, and verify identical results; then make sure at least one
  // canopy straddles a chunk border (otherwise the test proves nothing).
  WorldGenerator gen(4242);
  int straddling = 0;
  for (int attempt = 0; attempt < 8 && straddling == 0; ++attempt) {
    Region r(attempt * 7, 3, 4);
    r.generate(gen);
    for (int z = (r.cz0 + 1) * 16; z < (r.cz0 + 3) * 16; ++z)
      for (int y = 60; y < 200; ++y) {
        const int bx = (r.cx0 + 2) * 16;  // border between chunk 1 and 2
        const BlockId a = r.get(bx - 1, y, z), b = r.get(bx, y, z);
        const bool la = a == block::OakLeaves || a == block::BirchLeaves || a == block::SpruceLeaves;
        const bool lb = b == block::OakLeaves || b == block::BirchLeaves || b == block::SpruceLeaves;
        if (la && lb) ++straddling;
      }
  }
  CHECK(straddling > 0);
}

TEST(worldgen_spawn_is_dry_land) {
  for (uint64_t seed : {1ull, 2ull, 3ull, 99ull, 123456ull}) {
    WorldGenerator gen(seed);
    const ivec3 s = gen.findSpawn();
    const ColumnSample c = gen.sampleColumn(s.x, s.z);
    CHECK(s.y == c.height + 1);
    CHECK(c.height > WorldGenerator::kSeaLevel);
  }
}

// ---------------------------------------------------------------------------
// Lighting
// ---------------------------------------------------------------------------
TEST(light_basic_rules) {
  // An open column is fully sky lit; a sealed box is dark; a torch makes a
  // diamond of decreasing light.
  Region r(0, 0, 3);
  for (auto& b : r.blocks) std::fill(b.begin(), b.end(), 0);
  for (int x = 0; x < 48; ++x)
    for (int z = 0; z < 48; ++z)
      for (int y = 0; y <= 60; ++y) r.set(x, y, z, block::Stone);
  // Sealed room in the middle chunk.
  for (int y = 61; y <= 70; ++y)
    for (int x = 18; x <= 28; ++x)
      for (int z = 18; z <= 28; ++z) {
        const bool wall = x == 18 || x == 28 || z == 18 || z == 28 || y == 61 || y == 70;
        r.set(x, y, z, wall ? block::Stone : block::Air);
      }
  r.set(23, 62, 23, block::Torch);
  r.computeLight(1, 1);
  RegionAccess a{r};
  CHECK(skyOf(a.light(16, 100, 16)) == 15);
  CHECK(skyOf(a.light(30, 61, 30)) == 15);   // open ground outside the room
  CHECK(skyOf(a.light(23, 65, 23)) == 0);    // sealed room
  CHECK(blockLightOf(a.light(23, 62, 23)) == 14);
  CHECK(blockLightOf(a.light(24, 62, 23)) == 13);
  CHECK(blockLightOf(a.light(25, 63, 23)) == 11);
  CHECK(skyOf(a.light(20, 50, 20)) == 0);    // inside stone
  // Under a leaf canopy the light drops by one per block of leaves and below.
  r.set(20, 80, 20, block::OakLeaves);
  r.computeLight(1, 1);
  CHECK(skyOf(a.light(20, 80, 20)) == 14);
  CHECK(skyOf(a.light(20, 79, 20)) == 14);  // neighbours at full light refill it sideways
}

TEST(light_incremental_matches_full) {
  WorldGenerator gen(555);
  Region r(-2, -2, 5);
  r.generate(gen);
  for (int cz = -1; cz <= 1; ++cz)
    for (int cx = -1; cx <= 1; ++cx) r.computeLight(cx, cz);
  RegionAccess access{r};
  Pcg32 rng(77);
  const BlockId palette[] = {block::Air, block::Air, block::Stone, block::Glass, block::OakLeaves,
                             block::Torch, block::Lamp, block::Water, block::Planks, block::Lava};
  int edits = 0;
  for (int i = 0; i < 400; ++i) {
    // Edits concentrate near the surface of the centre chunk where light varies most.
    const int x = rng.range(0, 15), z = rng.range(0, 15);
    int top = 0;
    for (int y = kChunkHeight - 1; y >= 0; --y)
      if (r.get(x, y, z) != block::Air) {
        top = y;
        break;
      }
    const int y = clampv(top + rng.range(-6, 3), 1, kChunkHeight - 1);
    const BlockId b = palette[rng.below(sizeof(palette) / sizeof(palette[0]))];
    if (r.get(x, y, z) == b) continue;
    r.set(x, y, z, b);
    relightAt(access, x, y, z);
    ++edits;
    if (i % 100 == 99 || i == 399) {
      // Compare all lit chunks with a from-scratch computation.
      Region fresh = r;
      for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) fresh.computeLight(cx, cz);
      int mismatches = 0;
      for (int cz = -1; cz <= 1; ++cz)
        for (int cx = -1; cx <= 1; ++cx) {
          const auto& inc = r.light[r.slotChecked(cx, cz)];
          const auto& full = fresh.light[fresh.slotChecked(cx, cz)];
          for (int k = 0; k < kChunkVolume; ++k)
            if (inc[k] != full[k]) {
              if (mismatches < 5) {
                const int ly = k >> 8, lz = (k >> 4) & 15, lx = k & 15;
                std::fprintf(stderr, "  mismatch chunk %d,%d at %d,%d,%d: inc %02x full %02x\n", cx, cz, lx, ly, lz,
                             inc[k], full[k]);
              }
              ++mismatches;
            }
        }
      CHECK_MSG(mismatches == 0, "%d mismatched cells after %d edits", mismatches, edits);
    }
  }
}

// ---------------------------------------------------------------------------
// Meshing
// ---------------------------------------------------------------------------
namespace {
std::unique_ptr<MeshInput> emptyInput() {
  auto in = std::make_unique<MeshInput>();
  std::memset(in->blocks, 0, sizeof(in->blocks));
  std::memset(in->meta, 0, sizeof(in->meta));
  std::memset(in->light, packLight(15, 0), sizeof(in->light));
  for (int i = 0; i < kPad * kPad; ++i)
    for (int c = 0; c < 3; ++c) in->grass[i][c] = in->foliage[i][c] = 128;
  return in;
}
}  // namespace

TEST(mesher_face_counts) {
  MeshData out;
  auto in = emptyInput();
  in->blocks[padIndex(5, 5, 5)] = block::Stone;
  buildSectionMesh(*in, out);
  CHECK(out.opaque.size() == 24);
  in->blocks[padIndex(6, 5, 5)] = block::Stone;
  buildSectionMesh(*in, out);
  CHECK(out.opaque.size() == 40);  // the shared face disappears
  // Neighbour in the padding (next section) also hides the face.
  in->blocks[padIndex(15, 0, 0)] = block::Stone;
  in->blocks[padIndex(16, 0, 0)] = block::Stone;
  buildSectionMesh(*in, out);
  CHECK(out.opaque.size() == 40 + 20);

  in = emptyInput();
  in->blocks[padIndex(1, 1, 1)] = block::Glass;
  in->blocks[padIndex(2, 1, 1)] = block::Glass;
  buildSectionMesh(*in, out);
  CHECK(out.glass.size() == 40);
  in->blocks[padIndex(8, 8, 8)] = block::OakLeaves;
  in->blocks[padIndex(9, 8, 8)] = block::OakLeaves;
  buildSectionMesh(*in, out);
  CHECK(out.cutout.size() == 48);  // leaves keep inner faces
  in = emptyInput();
  in->blocks[padIndex(3, 3, 3)] = block::TallGrass;
  in->blocks[padIndex(4, 3, 3)] = block::Torch;
  buildSectionMesh(*in, out);
  CHECK(out.cutout.size() == 16 + 24);
  in = emptyInput();
  in->blocks[padIndex(3, 3, 3)] = block::Water;
  buildSectionMesh(*in, out);
  CHECK(out.water.size() == 6 * 4 * 2);  // all faces plus inside-out copies
  in->blocks[padIndex(3, 4, 3)] = block::Water;
  buildSectionMesh(*in, out);
  CHECK(out.water.size() == 10 * 4 * 2);

  // Every vertex stays inside the section (plus plant jitter).
  in = emptyInput();
  Pcg32 rng(5);
  for (int i = 0; i < 2000; ++i)
    in->blocks[padIndex(rng.range(-1, 16), rng.range(-1, 16), rng.range(-1, 16))] =
        static_cast<BlockId>(rng.range(1, block::Count - 1));
  buildSectionMesh(*in, out);
  CHECK(out.vertexCount() > 0);
  for (const auto* list : {&out.opaque, &out.cutout, &out.water, &out.glass}) {
    CHECK(list->size() % 4 == 0);
    for (const TerrainVertex& v : *list) {
      CHECK(v.x >= -8 && v.x <= 16 * kPosScale + 8);
      CHECK(v.y >= 0 && v.y <= 16 * kPosScale);
      CHECK(v.z >= -8 && v.z <= 16 * kPosScale + 8);
      CHECK(v.u <= 16 && v.v <= 16);
      CHECK(v.tex < tex::Count);
      CHECK(v.normal <= 6);
    }
  }
}

TEST(mesher_smooth_light_and_ao) {
  MeshData out;
  auto in = emptyInput();
  // A floor with one block on top: floor vertices next to the block get occluded.
  for (int x = 0; x < 16; ++x)
    for (int z = 0; z < 16; ++z) in->blocks[padIndex(x, 0, z)] = block::Stone;
  in->blocks[padIndex(8, 1, 8)] = block::Stone;
  buildSectionMesh(*in, out);
  int occluded = 0;
  for (const TerrainVertex& v : out.opaque)
    if (v.normal == FacePosY && v.y == kPosScale && v.ao < 255) ++occluded;
  // The block touches 4 grid corners, each shared by 3 visible floor faces.
  CHECK(occluded == 12);
  // Light is averaged from neighbouring cells.
  in = emptyInput();
  std::memset(in->light, packLight(0, 0), sizeof(in->light));
  in->blocks[padIndex(4, 4, 4)] = block::Stone;
  in->light[padIndex(4, 5, 4)] = packLight(15, 0);
  buildSectionMesh(*in, out);
  int topVerts = 0;
  for (const TerrainVertex& v : out.opaque)
    if (v.normal == FacePosY) {
      ++topVerts;
      CHECK(v.sky == (15 * 17 + 2) / 4);
    }
  CHECK(topVerts == 4);
}

// ---------------------------------------------------------------------------
// Raycast
// ---------------------------------------------------------------------------
TEST(raycast_hits_expected_faces) {
  auto world = [](int x, int y, int z) -> BlockId {
    if (y == 5 && x >= -5 && x <= 5 && z >= -5 && z <= 5) return block::Stone;
    if (x == 3 && y == 6 && z == 0) return block::Torch;
    if (x == -3 && y == 6 && z == 0) return block::Water;
    return block::Air;
  };
  RayHit h = raycastBlocks(dvec3(0.5, 10.5, 0.5), vec3(0, -1, 0), 10.0f, world);
  CHECK(h.hit && h.block == ivec3(0, 5, 0) && h.face == FacePosY);
  CHECK_NEAR(h.distance, 4.5f, 1e-4f);
  h = raycastBlocks(dvec3(0.5, 10.5, 0.5), vec3(0, -1, 0), 4.0f, world);
  CHECK(!h.hit);
  // Torch: the ray through the middle of the cell hits the stick, the edge misses it.
  h = raycastBlocks(dvec3(-1.0, 6.3, 0.5), vec3(1, 0, 0), 10.0f, world);
  CHECK(h.hit && h.block == ivec3(3, 6, 0) && h.face == FaceNegX);
  CHECK_NEAR(h.distance, 4.0f + 6.0f / 16.0f, 1e-4f);
  h = raycastBlocks(dvec3(-1.0, 6.3, 0.05), vec3(1, 0, 0), 10.0f, world);
  CHECK(!h.hit);
  // Water is transparent to the ray.
  h = raycastBlocks(dvec3(-2.5, 8.5, 0.5), vec3(0, -1, 0), 10.0f, world);
  CHECK(h.hit && h.block == ivec3(-3, 5, 0) && h.face == FacePosY);
  // Diagonal ray from below hits a bottom face.
  h = raycastBlocks(dvec3(0.2, 2.0, 0.3), normalize(vec3(0.3f, 1.0f, 0.2f)), 10.0f, world);
  CHECK(h.hit && h.block.y == 5 && h.face == FaceNegY);
}

// ---------------------------------------------------------------------------
// PNG
// ---------------------------------------------------------------------------
TEST(png_writer_produces_valid_file) {
  const fs::path dir = tempDir("png");
  std::vector<uint8_t> px(64 * 32 * 4);
  for (size_t i = 0; i < px.size(); ++i) px[i] = static_cast<uint8_t>(i * 7);
  CHECK(writePng((dir / "a.png").string(), 64, 32, 4, px.data()));
  std::ifstream f(dir / "a.png", std::ios::binary);
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  CHECK(data.size() > 64 * 32 * 4);
  CHECK(data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G');
  // CRC of IHDR (type + data) must match the stored value.
  const uint32_t stored = (uint32_t(data[29]) << 24) | (uint32_t(data[30]) << 16) | (uint32_t(data[31]) << 8) | data[32];
  CHECK(crc32(&data[12], 17) == stored);
  CHECK(crc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xCBF43926u);
  fs::remove_all(dir);
}

// ---------------------------------------------------------------------------
// World manager
// ---------------------------------------------------------------------------
namespace {
// Streams the world around `pos` until the centre chunk is rendered.
bool streamUntilReady(World& w, const dvec3& pos, int rd, double timeoutSec = 60.0) {
  const auto start = std::chrono::steady_clock::now();
  const int cx = floorDiv(fastFloor(pos.x), 16), cz = floorDiv(fastFloor(pos.z), 16);
  for (;;) {
    w.setCenter(pos, rd);
    w.update(4.0);
    (void)w.takeMeshUpdates();
    bool ready = true;
    for (int dz = -1; dz <= 1 && ready; ++dz)
      for (int dx = -1; dx <= 1 && ready; ++dx) ready = w.isChunkRendered(cx + dx, cz + dz);
    if (ready) return true;
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() > timeoutSec) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

int topSolid(const World& w, int x, int z) {
  for (int y = kChunkHeight - 1; y > 0; --y)
    if (blockInfo(w.getBlock(x, y, z)).solid) return y;
  return 0;
}

// Compares the live light of a chunk against a from-scratch computation.
int lightMismatches(const World& w, int cx, int cz) {
  std::vector<uint8_t> blocks(static_cast<size_t>(9) * kChunkVolume), light(kChunkVolume);
  const uint8_t* nb[9];
  for (int dz = -1; dz <= 1; ++dz)
    for (int dx = -1; dx <= 1; ++dx) {
      const int i = (dz + 1) * 3 + (dx + 1);
      uint8_t* dst = blocks.data() + static_cast<size_t>(i) * kChunkVolume;
      for (int y = 0; y < kChunkHeight; ++y)
        for (int z = 0; z < 16; ++z)
          for (int x = 0; x < 16; ++x)
            dst[chunkIndex(x, y, z)] = w.getBlock((cx + dx) * 16 + x, y, (cz + dz) * 16 + z);
      nb[i] = dst;
    }
  computeChunkLight(nb, light.data());
  int bad = 0;
  for (int y = 0; y < kChunkHeight; ++y)
    for (int z = 0; z < 16; ++z)
      for (int x = 0; x < 16; ++x)
        if (w.getLight(cx * 16 + x, y, cz * 16 + z) != light[chunkIndex(x, y, z)]) ++bad;
  return bad;
}
}  // namespace

TEST(world_streaming_and_edits) {
  const fs::path dir = tempDir("world");
  const uint64_t seed = 2024;
  ivec3 spawn;
  {
    World w(seed, dir, 3);
    spawn = w.generator().findSpawn();
    const dvec3 pos(spawn.x + 0.5, spawn.y, spawn.z + 0.5);
    CHECK(streamUntilReady(w, pos, 3));
    const WorldStats st = w.stats();
    CHECK(st.chunks >= 9 && st.litChunks >= 9 && st.meshedSections > 0);
    CHECK(w.isEditable(floorDiv(spawn.x, 16), floorDiv(spawn.z, 16)));
    // Light in loaded chunks matches a full recomputation.
    const int scx = floorDiv(spawn.x, 16), scz = floorDiv(spawn.z, 16);
    CHECK(lightMismatches(w, scx, scz) == 0);

    // Build a sealed stone box underground with a torch in it.
    const int bx = spawn.x, by = spawn.y - 12, bz = spawn.z;
    for (int y = by - 1; y <= by + 3; ++y)
      for (int z = bz - 2; z <= bz + 2; ++z)
        for (int x = bx - 2; x <= bx + 2; ++x) {
          const bool wall = x == bx - 2 || x == bx + 2 || z == bz - 2 || z == bz + 2 || y == by - 1 || y == by + 3;
          CHECK(w.setBlock(x, y, z, wall ? block::Stone : block::Air, 0, World::kSyncMesh));
        }
    CHECK(w.setBlock(bx, by, bz, block::Torch, 0, World::kNotify | World::kSyncMesh | World::kByPlayer));
    CHECK(w.getBlock(bx, by, bz) == block::Torch);
    CHECK(blockLightOf(w.getLight(bx, by, bz)) == 14);
    CHECK(blockLightOf(w.getLight(bx + 1, by, bz)) == 13);
    CHECK(skyOf(w.getLight(bx, by + 1, bz)) == 0);
    // Removing the floor under the torch breaks it.
    CHECK(w.setBlock(bx, by - 1, bz, block::Air, 0, World::kNotify));
    CHECK(w.getBlock(bx, by, bz) == block::Air);
    CHECK(blockLightOf(w.getLight(bx + 1, by, bz)) == 0);
    bool torchBroke = false;
    for (const BlockChange& c : w.takeChanges())
      if (c.before == block::Torch && c.after == block::Air && !c.byPlayer) torchBroke = true;
    CHECK(torchBroke);
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) CHECK(lightMismatches(w, scx + dx, scz + dz) == 0);

    // A persistent edit at the surface.
    CHECK(w.setBlock(spawn.x + 3, spawn.y + 5, spawn.z, block::Bricks, 0, World::kNotify));
    // Edits far away (unloaded) are refused.
    CHECK(!w.setBlock(spawn.x + 5000, 70, spawn.z, block::Stone));
    CHECK(w.getBlock(spawn.x + 5000, 70, spawn.z) == block::Air);
    CHECK(w.isSolidAt(spawn.x + 5000, 70, spawn.z));  // unloaded space is solid for physics
  }
  {
    // Reopen: the edit must persist while untouched terrain regenerates identically.
    World w(seed, dir, 2);
    const dvec3 pos(spawn.x + 0.5, spawn.y, spawn.z + 0.5);
    CHECK(streamUntilReady(w, pos, 2));
    CHECK(w.getBlock(spawn.x + 3, spawn.y + 5, spawn.z) == block::Bricks);
    CHECK(w.getBlock(spawn.x, spawn.y - 12, spawn.z) == block::Air);
  }
  fs::remove_all(dir);
}

TEST(world_streams_entire_render_distance) {
  // Every chunk inside the render distance must end up meshed: the streaming
  // rings have to contain all neighbours that lighting and meshing require.
  const fs::path dir = tempDir("stream");
  {
    World w(11, dir, 3);
    auto settle = [&](const dvec3& pos, int rd) {
      const auto start = std::chrono::steady_clock::now();
      for (;;) {
        w.setCenter(pos, rd);
        w.update(4.0);
        (void)w.takeMeshUpdates();
        const WorldStats st = w.stats();
        if (st.genInFlight == 0 && st.lightInFlight == 0 && st.meshInFlight == 0) {
          // One more round so freshly integrated results can schedule follow-up work.
          w.update(4.0);
          const WorldStats st2 = w.stats();
          if (st2.genInFlight == 0 && st2.lightInFlight == 0 && st2.meshInFlight == 0) return true;
        }
        if (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() > 120.0) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    };
    for (int rd : {2, 5, 6, 7}) {
      const dvec3 pos(8.0 + rd * 1000.0, 80.0, 8.0);
      CHECK(settle(pos, rd));
      int missing = 0;
      for (int dz = -rd; dz <= rd; ++dz)
        for (int dx = -rd; dx <= rd; ++dx)
          if (dx * dx + dz * dz <= rd * rd && !w.isChunkRendered(floorDiv(fastFloor(pos.x), 16) + dx, dz)) ++missing;
      CHECK_MSG(missing == 0, "render distance %d: %d chunks never rendered", rd, missing);
    }
    // Far away chunks are unloaded again.
    const WorldStats st = w.stats();
    CHECK(st.chunks < 400);
  }
  fs::remove_all(dir);
}

TEST(world_streams_without_threads) {
  // The single-threaded web build: all jobs run inside update().
  const fs::path dir = tempDir("inline");
  {
    World w(11, dir, 0);
    const dvec3 pos(8.0, 80.0, 8.0);
    const int rd = 4;
    auto missingChunks = [&] {
      int missing = 0;
      for (int dz = -rd; dz <= rd; ++dz)
        for (int dx = -rd; dx <= rd; ++dx)
          if (dx * dx + dz * dz <= rd * rd && !w.isChunkRendered(dx, dz)) ++missing;
      return missing;
    };
    // Nothing is ever "in flight" between two updates here: every update runs
    // its share of the jobs to completion. Frames until the view is complete:
    int missing = -1, frames = 0;
    for (; frames < 5000 && missing != 0; ++frames) {
      w.setCenter(pos, rd);
      w.update(5.0);
      (void)w.takeMeshUpdates();
      missing = missingChunks();
    }
    CHECK_MSG(missing == 0, "%d chunks never rendered after %d frames", missing, frames);
    // Edits work the same way.
    const int y = w.generator().sampleColumn(3, 3).height + 1;
    w.setBlock(3, y, 3, block::Stone, 0, World::kNotify | World::kSyncMesh);
    CHECK(w.getBlock(3, y, 3) == block::Stone);
  }
  fs::remove_all(dir);
}

TEST(world_fluids_and_falling_blocks) {
  const fs::path dir = tempDir("fluids");
  {
  World w(99, dir, 3);
  const ivec3 spawn = w.generator().findSpawn();
  const dvec3 pos(spawn.x + 0.5, spawn.y, spawn.z + 0.5);
  CHECK(streamUntilReady(w, pos, 3));
  // A flat stone platform high in the air with a water source in the middle.
  const int px = spawn.x, py = 200, pz = spawn.z;
  for (int z = pz - 10; z <= pz + 10; ++z)
    for (int x = px - 10; x <= px + 10; ++x) CHECK(w.setBlock(x, py, z, block::Stone, 0, 0));
  CHECK(w.setBlock(px, py + 1, pz, block::Water, 0, World::kNotify));
  for (int i = 0; i < 400; ++i) w.tick();
  // Water spreads 7 blocks with decreasing level.
  CHECK(w.getBlock(px + 1, py + 1, pz) == block::Water && w.getMeta(px + 1, py + 1, pz) == 1);
  CHECK(w.getBlock(px + 7, py + 1, pz) == block::Water && w.getMeta(px + 7, py + 1, pz) == 7);
  CHECK(w.getBlock(px + 8, py + 1, pz) == block::Air);
  CHECK(w.getBlock(px + 3, py + 1, pz + 3) == block::Water);
  // Remove the source: the flow dries up.
  CHECK(w.setBlock(px, py + 1, pz, block::Air, 0, World::kNotify));
  for (int i = 0; i < 400; ++i) w.tick();
  int water = 0;
  for (int z = pz - 10; z <= pz + 10; ++z)
    for (int x = px - 10; x <= px + 10; ++x)
      if (w.getBlock(x, py + 1, z) == block::Water) ++water;
  CHECK(water == 0);
  // Two sources with a gap between them fill it with a new source.
  CHECK(w.setBlock(px, py + 1, pz, block::Water, 0, World::kNotify));
  CHECK(w.setBlock(px + 2, py + 1, pz, block::Water, 0, World::kNotify));
  for (int i = 0; i < 100; ++i) w.tick();
  CHECK(w.getBlock(px + 1, py + 1, pz) == block::Water && w.getMeta(px + 1, py + 1, pz) == 0);
  // Lava next to water hardens.
  CHECK(w.setBlock(px + 9, py + 1, pz + 9, block::Lava, 0, World::kNotify));
  CHECK(w.setBlock(px + 9, py + 1, pz + 8, block::Water, 0, World::kNotify));
  for (int i = 0; i < 100; ++i) w.tick();
  const BlockId hard = w.getBlock(px + 9, py + 1, pz + 9);
  CHECK(hard == block::Basalt || hard == block::Cobblestone);

  // Sand placed in the air falls onto the platform.
  CHECK(w.setBlock(px - 5, py + 20, pz - 5, block::Sand, 0, World::kNotify));
  CHECK(w.getBlock(px - 5, py + 20, pz - 5) == block::Air);
  CHECK(w.fallingBlocks().size() == 1);
  for (int i = 0; i < 240 && !w.fallingBlocks().empty(); ++i) w.updateEntities(1.0f / 60.0f);
  CHECK(w.fallingBlocks().empty());
  CHECK(w.getBlock(px - 5, py + 1, pz - 5) == block::Sand);
  // A stack of sand falls together when its support goes away.
  for (int y = py + 1; y <= py + 4; ++y) CHECK(w.setBlock(px - 3, y, pz - 3, block::Gravel, 0, 0));
  CHECK(w.setBlock(px - 3, py, pz - 3, block::Air, 0, World::kNotify));
  CHECK(w.fallingBlocks().size() == 4);
  for (int i = 0; i < 600 && !w.fallingBlocks().empty(); ++i) w.updateEntities(1.0f / 60.0f);
  CHECK(w.fallingBlocks().empty());
  int gravel = 0;
  for (int y = 1; y < py; ++y)
    if (w.getBlock(px - 3, y, pz - 3) == block::Gravel) ++gravel;
  if (gravel != 4) {
    for (int y = py + 5; y > 40; --y)
      if (w.getBlock(px - 3, y, pz - 3) != block::Air)
        std::fprintf(stderr, "  y=%d %s\n", y, blockInfo(w.getBlock(px - 3, y, pz - 3)).name);
    for (const BlockChange& c : w.takeChanges())
      std::fprintf(stderr, "  change %d,%d,%d %s -> %s\n", c.pos.x, c.pos.y, c.pos.z, blockInfo(c.before).name,
                   blockInfo(c.after).name);
  }
  CHECK(gravel == 4);
  CHECK(topSolid(w, px - 3, pz - 3) < py);
  }
  fs::remove_all(dir);
}

// ---------------------------------------------------------------------------
// Player physics
// ---------------------------------------------------------------------------
namespace {
void stepPlayer(Player& p, World& w, const PlayerControls& c, int ticks) {
  for (int i = 0; i < ticks; ++i) p.tick(w, c, 1.0 / 60.0);
}
}  // namespace

TEST(player_physics) {
  const fs::path dir = tempDir("player");
  {
    World w(7, dir, 3);
    const ivec3 spawn = w.generator().findSpawn();
    CHECK(streamUntilReady(w, dvec3(spawn.x + 0.5, spawn.y, spawn.z + 0.5), 3));
    // A 21x21 stone platform at y = 200 with a wall and a pool.
    const int px = spawn.x, py = 200, pz = spawn.z;
    for (int z = pz - 10; z <= pz + 10; ++z)
      for (int x = px - 10; x <= px + 10; ++x) CHECK(w.setBlock(x, py, z, block::Stone, 0, 0));
    for (int y = py + 1; y <= py + 3; ++y)
      for (int z = pz - 10; z <= pz + 10; ++z) CHECK(w.setBlock(px + 5, y, z, block::Stone, 0, 0));
    PlayerControls none;

    // Falling onto the platform from 10 blocks up, in survival: lands with fall damage.
    Player p;
    p.teleport(dvec3(px + 0.5, py + 11.0, pz + 0.5));
    stepPlayer(p, w, none, 180);
    CHECK(p.onGround);
    CHECK_NEAR(p.pos.y, py + 1.0, 1e-9);
    CHECK(p.health < Player::kMaxHealth && p.health >= Player::kMaxHealth - 8.0f);
    const float afterFall = p.health;

    // Walking east into the wall stops exactly at its face.
    p.yaw = kHalfPi;  // facing +X
    PlayerControls fwd;
    fwd.forward = 1.0f;
    stepPlayer(p, w, fwd, 120);
    CHECK_NEAR(p.pos.x, px + 5.0 - Player::kWidth * 0.5, 1e-6);
    CHECK(p.onGround);

    // Jump: apex between 1.2 and 1.35 blocks.
    p.teleport(dvec3(px + 0.5, py + 1.0, pz + 0.5));
    stepPlayer(p, w, none, 5);
    PlayerControls jump;
    jump.jump = true;
    double apex = p.pos.y;
    p.tick(w, jump, 1.0 / 60.0);
    for (int i = 0; i < 60; ++i) {
      p.tick(w, none, 1.0 / 60.0);
      apex = std::max(apex, p.pos.y);
    }
    CHECK(apex - (py + 1.0) > 1.2 && apex - (py + 1.0) < 1.35);
    CHECK(p.health == afterFall);  // a normal jump does not hurt

    // Sneaking towards the western edge never falls off.
    p.teleport(dvec3(px - 8.5, py + 1.0, pz + 0.5));
    stepPlayer(p, w, none, 5);
    p.yaw = -kHalfPi;  // facing -X
    PlayerControls sneak;
    sneak.forward = 1.0f;
    sneak.sneak = true;
    stepPlayer(p, w, sneak, 300);
    CHECK(p.onGround);
    CHECK_NEAR(p.pos.y, py + 1.0, 1e-9);
    CHECK(p.pos.x < px - 10.0 && p.pos.x > px - 10.0 - Player::kWidth * 0.5 - 0.05);
    // Without sneaking the player walks off and falls.
    stepPlayer(p, w, fwd, 60);
    CHECK(p.pos.y < py + 0.5);

    // Creative players take no damage and can fly.
    Player c;
    c.creative = true;
    c.teleport(dvec3(px + 0.5, py + 30.0, pz + 0.5));
    stepPlayer(c, w, none, 240);
    CHECK(c.onGround && c.health == Player::kMaxHealth);
    c.flying = true;
    PlayerControls up;
    up.jump = true;
    stepPlayer(c, w, up, 60);
    CHECK(c.pos.y > py + 4.0 && c.flying);

    // A player stuck inside blocks is moved up to free space.
    Player stuck;
    stuck.teleport(dvec3(px + 5.5, py + 1.0, pz + 0.5));
    CHECK(stuck.collides(w, stuck.box()));
    stuck.unstuck(w);
    CHECK(!stuck.collides(w, stuck.box()));
    CHECK_NEAR(stuck.pos.y, py + 4.0, 1e-9);

    // Swimming: a pool of water two blocks deep on top of the platform.
    for (int z = pz - 3; z <= pz + 3; ++z)
      for (int x = px - 3; x <= px + 3; ++x)
        for (int y = py + 1; y <= py + 2; ++y) CHECK(w.setBlock(x, y, z, block::Water, 0, 0));
    Player swim;
    swim.teleport(dvec3(px + 0.5, py + 2.5, pz + 0.5));
    stepPlayer(swim, w, none, 30);
    CHECK(swim.inWater);
    const double sinkSpeed = swim.vel.y;
    CHECK(sinkSpeed < 0.0 && sinkSpeed > -3.6);  // buoyancy slows the fall
    stepPlayer(swim, w, jump, 120);
    CHECK(swim.pos.y > py + 1.8);  // swimming up
    swim.teleport(dvec3(px + 0.5, py + 1.0, pz + 0.5));
    stepPlayer(swim, w, none, 60);
    CHECK(swim.headInWater);
    stepPlayer(swim, w, none, 60 * 17);
    CHECK(swim.air == 0.0f && swim.health < Player::kMaxHealth);  // drowning
  }
  fs::remove_all(dir);
}

TEST(settings_round_trip) {
  const fs::path dir = tempDir("settings");
  Settings a;
  a.renderDistance = 17;
  a.fov = 91.0f;
  a.shadowQuality = 3;
  a.cloudQuality = 0;
  a.bloom = false;
  a.invertMouse = true;
  a.guiScale = 3;
  CHECK(a.save((dir / "s.txt").string()));
  Settings b;
  CHECK(b.load((dir / "s.txt").string()));
  CHECK(b.renderDistance == 17 && b.fov == 91.0f && b.shadowQuality == 3 && b.cloudQuality == 0);
  CHECK(!b.bloom && b.invertMouse && b.guiScale == 3);
  // Out of range values are clamped.
  std::ofstream(dir / "bad.txt") << "render_distance = 999\nfov = -5\nshadows = 9\nnonsense\n";
  Settings c;
  CHECK(c.load((dir / "bad.txt").string()));
  CHECK(c.renderDistance == 32 && c.fov == 40.0f && c.shadowQuality == 3);
  CHECK(!c.load((dir / "missing.txt").string()));
  fs::remove_all(dir);
}

int main(int argc, char** argv) { return testing::runAll(argc, argv); }

// ---------------------------------------------------------------------------
// Touch screen controls
// ---------------------------------------------------------------------------
TEST(touch_controls) {
  using Phase = Input::TouchPhase;
  TouchControls::Layout l;
  l.width = 1600.0f;
  l.height = 800.0f;
  l.unit = 2.0f;  // 2 pixels per CSS pixel
  l.hotbarSlot = 80.0f;
  l.hotbarX = (l.width - 9.0f * l.hotbarSlot) * 0.5f;
  l.hotbarY = l.height - l.hotbarSlot - 16.0f;
  l.creative = true;
  TouchControls t;
  double now = 0.0;
  auto frame = [&](std::vector<Input::TouchEvent> ev, double dt = 1.0 / 60.0) {
    now += dt;
    t.update(ev, l, now);
    return t.output();
  };

  // Nothing touched: no input.
  TouchControls::Output o = frame({});
  CHECK(o.forward == 0.0f && o.strafe == 0.0f && !o.jump && !o.use && !o.breaking && o.hotbarSlot < 0);

  // The stick: a thumb in the lower left pushed up moves forward.
  o = frame({{1, Phase::Began, vec2(200.0f, 560.0f)}});
  CHECK(t.owns(1));
  o = frame({{1, Phase::Moved, vec2(200.0f, 560.0f - 2.0f * TouchControls::kStickRadius)}});
  CHECK_NEAR(o.forward, 1.0f, 1e-4f);
  CHECK_NEAR(o.strafe, 0.0f, 1e-4f);
  CHECK(!o.sprint);
  // Half way to the right: half speed strafing.
  o = frame({{1, Phase::Moved, vec2(200.0f + TouchControls::kStickRadius, 560.0f)}});
  CHECK_NEAR(o.strafe, 0.5f, 1e-3f);
  CHECK_NEAR(o.forward, 0.0f, 1e-3f);
  // Far past the rim straight up: sprint.
  o = frame({{1, Phase::Moved, vec2(200.0f, 560.0f - 2.0f * 1.5f * TouchControls::kStickRadius)}});
  CHECK(o.sprint && o.forward > 0.99f);
  // Released: stop.
  o = frame({{1, Phase::Ended, vec2(200.0f, 400.0f)}});
  CHECK(o.forward == 0.0f && !o.sprint && !t.owns(1));

  // A short tap on the world uses / places at the tapped point.
  o = frame({{2, Phase::Began, vec2(1000.0f, 300.0f)}});
  CHECK(!o.use && !o.breaking);
  o = frame({{2, Phase::Ended, vec2(1002.0f, 301.0f)}});
  CHECK(o.use);
  CHECK_NEAR(o.aim.x, 1002.0f, 1e-3f);
  o = frame({});
  CHECK(!o.use);  // one frame only

  // A drag looks around (in CSS pixels) and is not a tap.
  o = frame({{3, Phase::Began, vec2(1000.0f, 300.0f)}});
  o = frame({{3, Phase::Moved, vec2(1100.0f, 340.0f)}});
  CHECK_NEAR(o.look.x, 50.0f, 1e-3f);
  CHECK_NEAR(o.look.y, 20.0f, 1e-3f);
  o = frame({{3, Phase::Ended, vec2(1100.0f, 340.0f)}});
  CHECK(!o.use && o.look.x == 0.0f);

  // Resting a finger breaks under it once the hold time has passed.
  o = frame({{4, Phase::Began, vec2(900.0f, 350.0f)}});
  o = frame({}, TouchControls::kHoldTime * 0.5);
  CHECK(!o.breaking);
  o = frame({}, TouchControls::kHoldTime);
  CHECK(o.breaking && o.aiming);
  CHECK_NEAR(o.aim.x, 900.0f, 1e-3f);
  // Dragging while holding keeps breaking (at the finger) and turns the view.
  o = frame({{4, Phase::Moved, vec2(940.0f, 350.0f)}});
  CHECK(o.breaking && o.look.x > 0.0f);
  CHECK_NEAR(o.aim.x, 940.0f, 1e-3f);
  o = frame({{4, Phase::Ended, vec2(940.0f, 350.0f)}});
  CHECK(!o.breaking && !o.use);  // a hold is not also a tap

  // A cancelled touch (the system took it over) is never a tap.
  o = frame({{9, Phase::Began, vec2(1000.0f, 300.0f)}});
  o = frame({{9, Phase::Cancelled, vec2(1000.0f, 300.0f)}});
  CHECK(!o.use && !t.owns(9));

  // Hotbar slots select on touch.
  o = frame({{5, Phase::Began, vec2(l.hotbarX + l.hotbarSlot * 3.5f, l.hotbarY + l.hotbarSlot * 0.5f)}});
  CHECK(o.hotbarSlot == 3);
  o = frame({{5, Phase::Ended, vec2(l.hotbarX + l.hotbarSlot * 3.5f, l.hotbarY + l.hotbarSlot * 0.5f)}});
  CHECK(o.hotbarSlot < 0 && !o.use);

  // The pause button sits in the top right corner.
  o = frame({{6, Phase::Began, vec2(l.width - 14.0f * l.unit - 22.0f * l.unit, 14.0f * l.unit + 22.0f * l.unit)}});
  CHECK(o.pause);
  o = frame({{6, Phase::Ended, vec2(l.width - 50.0f, 50.0f)}});
  CHECK(!o.pause);

  // Two fingers at once: walk with one, look with the other.
  o = frame({{7, Phase::Began, vec2(200.0f, 600.0f)}, {8, Phase::Began, vec2(1100.0f, 300.0f)}});
  o = frame({{7, Phase::Moved, vec2(200.0f, 600.0f - 2.0f * TouchControls::kStickRadius)},
             {8, Phase::Moved, vec2(1060.0f, 300.0f)}});
  CHECK(o.forward > 0.99f);
  CHECK_NEAR(o.look.x, -20.0f, 1e-3f);
  // Opening a menu forgets every finger.
  t.releaseAll();
  CHECK(!t.owns(7) && !t.owns(8));
  o = frame({{7, Phase::Ended, vec2(0.0f, 0.0f)}});
  CHECK(o.forward == 0.0f);
}
