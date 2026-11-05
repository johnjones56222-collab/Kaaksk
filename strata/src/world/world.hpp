// The world: chunk streaming around the player, background generation /
// lighting / meshing, block edits, fluids and falling blocks.
//
// Threading model: the main thread owns every Chunk. Worker jobs receive
// copies (snapshots) of the data they need and return new data, which the main
// thread validates against version stamps before using it. Workers never touch
// live chunks, so there are no data races by construction.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/math.hpp"
#include "core/thread_pool.hpp"
#include "world/chunk.hpp"
#include "world/mesher.hpp"
#include "world/storage.hpp"
#include "world/worldgen.hpp"

namespace strata {

struct SectionMeshUpdate {
  int cx = 0, sy = 0, cz = 0;
  std::unique_ptr<MeshData> mesh;  // nullptr: remove the section's mesh
};

struct BlockChange {
  ivec3 pos;
  BlockId before = block::Air;
  BlockId after = block::Air;
  bool byPlayer = false;
};

struct FallingBlock {
  dvec3 pos;  // centre of the bottom face
  float velocity = 0.0f;
  BlockId id = block::Sand;
};

struct WorldStats {
  size_t chunks = 0, litChunks = 0, meshedSections = 0;
  size_t genInFlight = 0, lightInFlight = 0, meshInFlight = 0;
  size_t scheduledTicks = 0;
};

class World {
 public:
  // Edit flags for setBlock.
  enum : unsigned {
    kNotify = 1,     // wake fluids, falling blocks and supported blocks around the change
    kSyncMesh = 2,   // rebuild the touched sections immediately (player edits)
    kByPlayer = 4,   // recorded in the change log
  };

  World(uint64_t seed, const std::filesystem::path& directory, unsigned workerThreads);
  ~World();
  World(const World&) = delete;
  World& operator=(const World&) = delete;

  const WorldGenerator& generator() const { return generator_; }
  WorldStorage& storage() { return storage_; }
  uint64_t seed() const { return generator_.seed(); }

  // Streaming. Call once per frame.
  void setCenter(const dvec3& position, int renderDistance);
  void update(double budgetMs);

  // Block queries in world coordinates. Unloaded space reads as air.
  BlockId getBlock(int x, int y, int z) const;
  uint8_t getMeta(int x, int y, int z) const;
  uint8_t getLight(int x, int y, int z) const;  // packed sky/block light
  const ColumnClimate* climateAt(int x, int z) const;
  bool isChunkLoaded(int cx, int cz) const { return findChunk(cx, cz) != nullptr; }
  // Chunk and all eight neighbours are lit: edits and simulation are allowed.
  bool isEditable(int cx, int cz) const;
  bool isBlockEditable(int x, int z) const { return isEditable(floorDiv(x, 16), floorDiv(z, 16)); }
  // Every non-empty section of the chunk has an up to date mesh.
  bool isChunkRendered(int cx, int cz) const;
  // Solid for collision; unloaded or not yet editable space counts as solid.
  bool isSolidAt(int x, int y, int z) const;

  bool setBlock(int x, int y, int z, BlockId id, uint8_t meta = 0, unsigned flags = kNotify);

  // Simulation step (20 Hz): scheduled fluid updates.
  void tick();
  // Falling block physics, called every frame with the frame time.
  void updateEntities(float dt);
  const std::vector<FallingBlock>& fallingBlocks() const { return falling_; }

  std::vector<SectionMeshUpdate> takeMeshUpdates();
  std::vector<BlockChange> takeChanges();

  void saveAll();
  WorldStats stats() const;
  int64_t tickCount() const { return tickCount_; }

 private:
  struct GenResult {
    ChunkPos pos;
    std::unique_ptr<Chunk> chunk;
  };
  struct Stamp {
    uint64_t uid = 0;
    uint32_t version = 0;
  };
  struct LightResult {
    ChunkPos pos;
    std::array<Stamp, 9> stamps;
    std::vector<uint8_t> light;
  };
  struct MeshResult {
    ChunkPos pos;
    int sy = 0;
    uint64_t uid = 0;
    uint32_t stamp = 0;
    bool cancelled = false;  // skipped because the chunk left the view
    std::unique_ptr<MeshData> mesh;
  };
  struct ScheduledTick {
    int64_t due;
    uint64_t order;
    int x, y, z;
    bool operator>(const ScheduledTick& o) const { return due != o.due ? due > o.due : order > o.order; }
  };
  struct LightAccess;  // adapter used by the incremental light engine

  Chunk* findChunk(int cx, int cz);
  const Chunk* findChunk(int cx, int cz) const;
  bool wanted(ChunkPos p, float radius) const;
  // Thread safe: may a job for this chunk still be useful? (margin beyond the render distance)
  bool stillWanted(ChunkPos p, float margin) const;
  void recordRemoval(int x, int y, int z, BlockId before);

  void integrateResults();
  void unloadFarChunks();
  void scheduleJobs();
  void submitGeneration(ChunkPos p, float priority);
  void submitLight(ChunkPos p, float priority);
  void submitMesh(Chunk& c, int sy, float priority);
  void buildMeshInput(const Chunk& c, int sy, MeshInput& in) const;
  void remeshNow(Chunk& c, int sy);
  bool neighboursLit(int cx, int cz) const;
  void emitMesh(Chunk& c, int sy, uint32_t stamp, std::unique_ptr<MeshData> mesh);
  void removeChunkMeshes(Chunk& c);

  void markSectionDirty(int cx, int sy, int cz);
  void markAround(int x, int y, int z);  // sections whose meshes can see this cell
  void syncRemeshAround(int x, int y, int z);

  void notifyAround(int x, int y, int z);
  void scheduleFluid(int x, int y, int z, int delay);
  void fluidTick(int x, int y, int z);
  void checkSupport(int x, int y, int z);
  void checkGravity(int x, int y, int z);
  bool flowInto(int x, int y, int z, BlockId liquid, uint8_t meta);

  WorldGenerator generator_;
  WorldStorage storage_;

  std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> chunks_;
  std::unordered_set<ChunkPos, ChunkPosHash> genPending_;
  std::unordered_set<ChunkPos, ChunkPosHash> lightPending_;
  size_t meshInFlight_ = 0;
  uint64_t nextUid_ = 1;

  ChunkPos center_;
  int renderDistance_ = 8;
  // Copies of the streaming centre that worker jobs read to drop stale work.
  std::atomic<int> liveCenterX_{0}, liveCenterZ_{0}, liveRadius_{8};
  std::vector<std::pair<int, int>> spiral_;
  int spiralRadius_ = -1;

  std::mutex resultMutex_;
  std::vector<GenResult> genResults_;
  std::vector<LightResult> lightResults_;
  std::vector<MeshResult> meshResults_;

  std::vector<SectionMeshUpdate> meshUpdates_;
  std::vector<BlockChange> changes_;

  std::priority_queue<ScheduledTick, std::vector<ScheduledTick>, std::greater<ScheduledTick>> ticks_;
  std::unordered_set<uint64_t> tickSet_;
  uint64_t tickOrder_ = 0;
  int64_t tickCount_ = 0;
  std::vector<FallingBlock> falling_;

  unsigned threads_ = 1;
  std::unique_ptr<ThreadPool> pool_;  // last: destroyed first
};

}  // namespace strata
