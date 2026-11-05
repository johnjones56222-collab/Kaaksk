#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "core/log.hpp"
#include "world/lighting.hpp"

namespace strata {

namespace {

// Streaming rings (in chunks, around the player's chunk):
//   meshed   d <= R          lit      d <= R + 1.5
//   generated d <= R + 3     unloaded d >  R + 4.5
// Consecutive rings are more than sqrt(2) apart, so all eight neighbours of a
// chunk in one ring always lie inside the next ring: a meshed chunk always has
// lit neighbours, and a lit chunk always has generated neighbours.
constexpr float kLightMargin = 1.5f;
constexpr float kGenMargin = 3.0f;
constexpr float kKeepMargin = 4.5f;
inline bool withinDistance(int dx, int dz, float r) { return static_cast<float>(dx * dx + dz * dz) <= r * r; }

inline uint64_t tickKey(int x, int y, int z) {
  return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) ^
         (static_cast<uint64_t>(static_cast<uint32_t>(z) & 0xFFFFFFu) << 8) ^ static_cast<uint64_t>(y & 0xFF);
}

constexpr int kHorizontal[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
constexpr float kGravity = 32.0f;

}  // namespace

// Adapter between the incremental light engine and live chunks. Only lit
// chunks take part; every light change marks the meshes that can see it.
struct World::LightAccess {
  World& w;
  Chunk* cached = nullptr;
  int ccx = 0, ccz = 0;
  bool cachedValid = false;

  Chunk* get(int x, int z) {
    const int cx = floorDiv(x, 16), cz = floorDiv(z, 16);
    if (!cachedValid || cx != ccx || cz != ccz) {
      Chunk* c = w.findChunk(cx, cz);
      cached = (c && c->lit) ? c : nullptr;
      ccx = cx;
      ccz = cz;
      cachedValid = true;
    }
    return cached;
  }
  bool valid(int x, int, int z) { return get(x, z) != nullptr; }
  BlockId block(int x, int y, int z) {
    Chunk* c = get(x, z);
    return c ? c->block(x - ccx * 16, y, z - ccz * 16) : static_cast<BlockId>(block::Air);
  }
  uint8_t light(int x, int y, int z) {
    Chunk* c = get(x, z);
    return c ? c->light(x - ccx * 16, y, z - ccz * 16) : 0;
  }
  void setLight(int x, int y, int z, uint8_t v) {
    Chunk* c = get(x, z);
    if (!c) return;
    c->setLight(x - ccx * 16, y, z - ccz * 16, v);
    w.markAround(x, y, z);
    // markAround may look up other chunks but never changes the map.
  }
};

World::World(uint64_t seed, const std::filesystem::path& directory, unsigned workerThreads)
    : generator_(seed), storage_(directory), threads_(std::max(1u, workerThreads)) {
  // Zero worker threads: jobs run inside update() on the calling thread.
  pool_ = std::make_unique<ThreadPool>(workerThreads);
}

World::~World() {
  pool_.reset();  // joins workers; queued jobs are dropped
  saveAll();
}

Chunk* World::findChunk(int cx, int cz) {
  auto it = chunks_.find(ChunkPos{cx, cz});
  return it == chunks_.end() ? nullptr : it->second.get();
}

const Chunk* World::findChunk(int cx, int cz) const {
  auto it = chunks_.find(ChunkPos{cx, cz});
  return it == chunks_.end() ? nullptr : it->second.get();
}

bool World::wanted(ChunkPos p, float radius) const {
  return withinDistance(p.x - center_.x, p.z - center_.z, radius);
}

bool World::stillWanted(ChunkPos p, float margin) const {
  const float r = static_cast<float>(liveRadius_.load(std::memory_order_relaxed)) + margin;
  return withinDistance(p.x - liveCenterX_.load(std::memory_order_relaxed),
                        p.z - liveCenterZ_.load(std::memory_order_relaxed), r);
}

void World::setCenter(const dvec3& position, int renderDistance) {
  center_ = {floorDiv(fastFloor(position.x), 16), floorDiv(fastFloor(position.z), 16)};
  renderDistance_ = clampv(renderDistance, 2, 32);
  liveCenterX_.store(center_.x, std::memory_order_relaxed);
  liveCenterZ_.store(center_.z, std::memory_order_relaxed);
  liveRadius_.store(renderDistance_, std::memory_order_relaxed);
  if (renderDistance_ != spiralRadius_) {
    const float genRadius = static_cast<float>(renderDistance_) + kGenMargin;
    const int r = static_cast<int>(std::ceil(genRadius));
    spiral_.clear();
    for (int dz = -r; dz <= r; ++dz)
      for (int dx = -r; dx <= r; ++dx)
        if (withinDistance(dx, dz, genRadius)) spiral_.emplace_back(dx, dz);
    std::stable_sort(spiral_.begin(), spiral_.end(), [](const auto& a, const auto& b) {
      return a.first * a.first + a.second * a.second < b.first * b.first + b.second * b.second;
    });
    spiralRadius_ = renderDistance_;
  }
}

void World::update(double budgetMs) {
  integrateResults();
  unloadFarChunks();
  scheduleJobs();
  if (pool_->inlineMode()) {
    // No worker threads: spend the budget on the most urgent jobs now and
    // pick their results up straight away.
    pool_->runPending(budgetMs / 1000.0);
    integrateResults();
  }
}

// ---------------------------------------------------------------------------
// Job results
// ---------------------------------------------------------------------------
void World::integrateResults() {
  std::vector<GenResult> gens;
  std::vector<LightResult> lights;
  std::vector<MeshResult> meshes;
  {
    std::lock_guard<std::mutex> lock(resultMutex_);
    gens.swap(genResults_);
    lights.swap(lightResults_);
    meshes.swap(meshResults_);
  }
  for (GenResult& g : gens) {
    genPending_.erase(g.pos);
    if (!g.chunk || !wanted(g.pos, static_cast<float>(renderDistance_) + kKeepMargin) || chunks_.count(g.pos))
      continue;
    g.chunk->uid = nextUid_++;
    chunks_.emplace(g.pos, std::move(g.chunk));
  }
  for (LightResult& l : lights) {
    lightPending_.erase(l.pos);
    Chunk* c = findChunk(l.pos.x, l.pos.z);
    if (!c || c->lit || l.light.empty()) continue;
    bool fresh = true;
    for (int dz = -1; dz <= 1 && fresh; ++dz)
      for (int dx = -1; dx <= 1 && fresh; ++dx) {
        const Chunk* n = findChunk(l.pos.x + dx, l.pos.z + dz);
        const Stamp& s = l.stamps[static_cast<size_t>((dz + 1) * 3 + (dx + 1))];
        fresh = n && n->uid == s.uid && n->version == s.version;
      }
    if (!fresh) continue;  // a neighbour changed meanwhile; the scheduler retries
    c->applyLight(l.light.data());
    c->lit = true;
    for (int s = 0; s < kSectionCount; ++s) ++c->meshDirty[static_cast<size_t>(s)];
  }
  for (MeshResult& m : meshes) {
    --meshInFlight_;
    Chunk* c = findChunk(m.pos.x, m.pos.z);
    if (!c || c->uid != m.uid) continue;
    c->meshInFlight[static_cast<size_t>(m.sy)] = false;
    if (m.cancelled) continue;  // still dirty: rebuilt when it comes back into view
    if (m.stamp > c->meshBuilt[static_cast<size_t>(m.sy)]) emitMesh(*c, m.sy, m.stamp, std::move(m.mesh));
  }
}

void World::emitMesh(Chunk& c, int sy, uint32_t stamp, std::unique_ptr<MeshData> mesh) {
  const size_t s = static_cast<size_t>(sy);
  c.meshBuilt[s] = stamp;
  if (!mesh || mesh->empty()) {
    if (c.hasMesh[s]) {
      meshUpdates_.push_back({c.cx(), sy, c.cz(), nullptr});
      c.hasMesh[s] = false;
    }
    return;
  }
  meshUpdates_.push_back({c.cx(), sy, c.cz(), std::move(mesh)});
  c.hasMesh[s] = true;
}

void World::removeChunkMeshes(Chunk& c) {
  for (int s = 0; s < kSectionCount; ++s)
    if (c.hasMesh[static_cast<size_t>(s)]) {
      meshUpdates_.push_back({c.cx(), s, c.cz(), nullptr});
      c.hasMesh[static_cast<size_t>(s)] = false;
    }
}

void World::unloadFarChunks() {
  const float keep = static_cast<float>(renderDistance_) + kKeepMargin;
  for (auto it = chunks_.begin(); it != chunks_.end();) {
    if (wanted(it->first, keep)) {
      ++it;
      continue;
    }
    Chunk& c = *it->second;
    if (c.modified) {
      std::vector<uint8_t> blocks(kChunkVolume), meta(kChunkVolume);
      c.copyBlocks(blocks.data());
      c.copyMeta(meta.data());
      storage_.saveChunk(c.cx(), c.cz(), blocks.data(), meta.data());
    }
    removeChunkMeshes(c);
    lightPending_.erase(it->first);
    it = chunks_.erase(it);
  }
  // Falling blocks in unloaded space simply vanish.
  falling_.erase(std::remove_if(falling_.begin(), falling_.end(),
                                [this](const FallingBlock& f) {
                                  return !isChunkLoaded(floorDiv(fastFloor(f.pos.x), 16),
                                                        floorDiv(fastFloor(f.pos.z), 16));
                                }),
                 falling_.end());
}

// ---------------------------------------------------------------------------
// Scheduling
// ---------------------------------------------------------------------------
bool World::neighboursLit(int cx, int cz) const {
  for (int dz = -1; dz <= 1; ++dz)
    for (int dx = -1; dx <= 1; ++dx) {
      const Chunk* n = findChunk(cx + dx, cz + dz);
      if (!n || !n->lit) return false;
    }
  return true;
}

bool World::isEditable(int cx, int cz) const { return neighboursLit(cx, cz); }

bool World::isChunkRendered(int cx, int cz) const {
  const Chunk* c = findChunk(cx, cz);
  if (!c || !c->lit) return false;
  for (int s = 0; s < kSectionCount; ++s)
    if (c->meshBuilt[static_cast<size_t>(s)] < c->meshDirty[static_cast<size_t>(s)]) return false;
  return true;
}

void World::scheduleJobs() {
  // Enough work in flight to keep every worker busy between two frames, but
  // little enough that the queue re-prioritises quickly when the player moves.
  const size_t maxGen = threads_ * 6;
  const size_t maxLight = threads_ * 4;
  const size_t maxMesh = threads_ * 12;
  const float meshR = static_cast<float>(renderDistance_);
  const float lightR = meshR + kLightMargin;
  const float genR = meshR + kGenMargin;
  for (const auto& off : spiral_) {
    const int dx = off.first, dz = off.second;
    const ChunkPos p{center_.x + dx, center_.z + dz};
    const float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));
    Chunk* c = findChunk(p.x, p.z);
    if (!c) {
      if (genPending_.size() < maxGen && withinDistance(dx, dz, genR) && !genPending_.count(p)) submitGeneration(p, dist);
      continue;
    }
    if (!c->lit) {
      if (lightPending_.size() < maxLight && withinDistance(dx, dz, lightR) && !lightPending_.count(p)) {
        bool all = true;
        for (int nz = -1; nz <= 1 && all; ++nz)
          for (int nx = -1; nx <= 1 && all; ++nx) all = findChunk(p.x + nx, p.z + nz) != nullptr;
        if (all) submitLight(p, dist);
      }
      continue;
    }
    if (!withinDistance(dx, dz, meshR) || !neighboursLit(p.x, p.z)) continue;
    for (int s = 0; s < kSectionCount; ++s) {
      const size_t si = static_cast<size_t>(s);
      if (c->meshDirty[si] <= c->meshBuilt[si] || c->meshInFlight[si]) continue;
      if (c->sectionEmpty(s)) {
        emitMesh(*c, s, c->meshDirty[si], nullptr);  // nothing to draw
        continue;
      }
      if (meshInFlight_ >= maxMesh) break;
      submitMesh(*c, s, dist - 1.0f);
    }
  }
}

void World::submitGeneration(ChunkPos p, float priority) {
  genPending_.insert(p);
  const WorldGenerator* gen = &generator_;
  const WorldStorage* st = &storage_;
  pool_->submit(priority, [this, gen, st, p] {
    if (!stillWanted(p, kKeepMargin)) {
      std::lock_guard<std::mutex> lock(resultMutex_);
      genResults_.push_back({p, nullptr});
      return;
    }
    auto chunk = std::make_unique<Chunk>(p.x, p.z);
    std::vector<uint8_t> blocks(kChunkVolume), meta(kChunkVolume);
    if (st->loadChunk(p.x, p.z, blocks.data(), meta.data()))
      gen->computeClimate(p.x, p.z, chunk->climate);
    else
      gen->generate(p.x, p.z, blocks.data(), meta.data(), chunk->climate);
    chunk->assign(blocks.data(), meta.data());
    std::lock_guard<std::mutex> lock(resultMutex_);
    genResults_.push_back({p, std::move(chunk)});
  });
}

void World::submitLight(ChunkPos p, float priority) {
  lightPending_.insert(p);
  auto snapshot = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(9 * kChunkVolume));
  std::array<Stamp, 9> stamps;
  for (int dz = -1; dz <= 1; ++dz)
    for (int dx = -1; dx <= 1; ++dx) {
      const int i = (dz + 1) * 3 + (dx + 1);
      const Chunk* n = findChunk(p.x + dx, p.z + dz);
      n->copyBlocks(snapshot->data() + static_cast<size_t>(i) * kChunkVolume);
      stamps[static_cast<size_t>(i)] = {n->uid, n->version};
    }
  pool_->submit(priority, [this, p, snapshot, stamps] {
    if (!stillWanted(p, kKeepMargin)) {
      LightResult skipped;
      skipped.pos = p;
      std::lock_guard<std::mutex> lock(resultMutex_);
      lightResults_.push_back(std::move(skipped));
      return;
    }
    const uint8_t* nb[9];
    for (int i = 0; i < 9; ++i) nb[i] = snapshot->data() + static_cast<size_t>(i) * kChunkVolume;
    LightResult r;
    r.pos = p;
    r.stamps = stamps;
    r.light.resize(kChunkVolume);
    computeChunkLight(nb, r.light.data());
    std::lock_guard<std::mutex> lock(resultMutex_);
    lightResults_.push_back(std::move(r));
  });
}

void World::submitMesh(Chunk& c, int sy, float priority) {
  const size_t si = static_cast<size_t>(sy);
  c.meshInFlight[si] = true;
  ++meshInFlight_;
  auto input = std::make_shared<MeshInput>();
  buildMeshInput(c, sy, *input);
  const uint32_t stamp = c.meshDirty[si];
  const uint64_t uid = c.uid;
  const ChunkPos pos = c.pos();
  pool_->submit(priority, [this, input, stamp, uid, pos, sy] {
    MeshResult r;
    r.pos = pos;
    r.sy = sy;
    r.uid = uid;
    r.stamp = stamp;
    if (stillWanted(pos, kLightMargin)) {
      r.mesh = std::make_unique<MeshData>();
      buildSectionMesh(*input, *r.mesh);
    } else {
      r.cancelled = true;
    }
    std::lock_guard<std::mutex> lock(resultMutex_);
    meshResults_.push_back(std::move(r));
  });
}

void World::buildMeshInput(const Chunk& c, int sy, MeshInput& in) const {
  in.cx = c.cx();
  in.sy = sy;
  in.cz = c.cz();
  const Chunk* nb[3][3];
  for (int dz = -1; dz <= 1; ++dz)
    for (int dx = -1; dx <= 1; ++dx) nb[dz + 1][dx + 1] = findChunk(c.cx() + dx, c.cz() + dz);
  for (int z = -1; z <= 16; ++z)
    for (int x = -1; x <= 16; ++x) {
      const int ix = x < 0 ? 0 : (x > 15 ? 2 : 1);
      const int iz = z < 0 ? 0 : (z > 15 ? 2 : 1);
      const Chunk* ch = nb[iz][ix];
      const int lx = x - (ix - 1) * 16;
      const int lz = z - (iz - 1) * 16;
      const int col = padColumn(x, z);
      if (ch) {
        const ColumnClimate& cl = ch->climate[lz * 16 + lx];
        for (int i = 0; i < 3; ++i) {
          in.grass[col][i] = cl.grass[i];
          in.foliage[col][i] = cl.foliage[i];
        }
      } else {
        for (int i = 0; i < 3; ++i) in.grass[col][i] = in.foliage[col][i] = 128;
      }
      for (int y = -1; y <= 16; ++y) {
        const int wy = sy * 16 + y;
        const int pi = padIndex(x, y, z);
        if (!ch || wy < 0 || wy >= kChunkHeight) {
          in.blocks[pi] = block::Air;
          in.meta[pi] = 0;
          in.light[pi] = wy < 0 ? packLight(0, 0) : packLight(15, 0);
          continue;
        }
        in.blocks[pi] = ch->block(lx, wy, lz);
        in.meta[pi] = ch->meta(lx, wy, lz);
        in.light[pi] = ch->light(lx, wy, lz);
      }
    }
}

void World::remeshNow(Chunk& c, int sy) {
  const size_t si = static_cast<size_t>(sy);
  if (c.meshDirty[si] <= c.meshBuilt[si]) return;
  if (!neighboursLit(c.cx(), c.cz())) return;
  if (!withinDistance(c.cx() - center_.x, c.cz() - center_.z, static_cast<float>(renderDistance_))) return;
  const uint32_t stamp = c.meshDirty[si];
  if (c.sectionEmpty(sy)) {
    emitMesh(c, sy, stamp, nullptr);
    return;
  }
  auto input = std::make_unique<MeshInput>();
  buildMeshInput(c, sy, *input);
  auto mesh = std::make_unique<MeshData>();
  buildSectionMesh(*input, *mesh);
  emitMesh(c, sy, stamp, std::move(mesh));
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
BlockId World::getBlock(int x, int y, int z) const {
  if (y < 0 || y >= kChunkHeight) return block::Air;
  const int cx = floorDiv(x, 16), cz = floorDiv(z, 16);
  const Chunk* c = findChunk(cx, cz);
  return c ? c->block(x - cx * 16, y, z - cz * 16) : static_cast<BlockId>(block::Air);
}

uint8_t World::getMeta(int x, int y, int z) const {
  if (y < 0 || y >= kChunkHeight) return 0;
  const int cx = floorDiv(x, 16), cz = floorDiv(z, 16);
  const Chunk* c = findChunk(cx, cz);
  return c ? c->meta(x - cx * 16, y, z - cz * 16) : 0;
}

uint8_t World::getLight(int x, int y, int z) const {
  if (y >= kChunkHeight) return packLight(15, 0);
  if (y < 0) return packLight(0, 0);
  const int cx = floorDiv(x, 16), cz = floorDiv(z, 16);
  const Chunk* c = findChunk(cx, cz);
  if (!c || !c->lit) return packLight(15, 0);
  return c->light(x - cx * 16, y, z - cz * 16);
}

const ColumnClimate* World::climateAt(int x, int z) const {
  const int cx = floorDiv(x, 16), cz = floorDiv(z, 16);
  const Chunk* c = findChunk(cx, cz);
  return c ? &c->climate[(z - cz * 16) * 16 + (x - cx * 16)] : nullptr;
}

bool World::isSolidAt(int x, int y, int z) const {
  if (y < 0) return true;
  if (y >= kChunkHeight) return false;
  if (!isBlockEditable(x, z)) return true;
  return blockInfo(getBlock(x, y, z)).solid;
}

// ---------------------------------------------------------------------------
// Edits
// ---------------------------------------------------------------------------
void World::markSectionDirty(int cx, int sy, int cz) {
  if (sy < 0 || sy >= kSectionCount) return;
  Chunk* c = findChunk(cx, cz);
  if (c) ++c->meshDirty[static_cast<size_t>(sy)];
}

void World::markAround(int x, int y, int z) {
  const int cx0 = floorDiv(x - 1, 16), cx1 = floorDiv(x + 1, 16);
  const int cz0 = floorDiv(z - 1, 16), cz1 = floorDiv(z + 1, 16);
  const int sy0 = std::max(0, floorDiv(y - 1, 16)), sy1 = std::min(kSectionCount - 1, floorDiv(y + 1, 16));
  for (int cz = cz0; cz <= cz1; ++cz)
    for (int cx = cx0; cx <= cx1; ++cx)
      for (int sy = sy0; sy <= sy1; ++sy) markSectionDirty(cx, sy, cz);
}

void World::syncRemeshAround(int x, int y, int z) {
  const int cx0 = floorDiv(x - 1, 16), cx1 = floorDiv(x + 1, 16);
  const int cz0 = floorDiv(z - 1, 16), cz1 = floorDiv(z + 1, 16);
  const int sy0 = std::max(0, floorDiv(y - 1, 16)), sy1 = std::min(kSectionCount - 1, floorDiv(y + 1, 16));
  for (int cz = cz0; cz <= cz1; ++cz)
    for (int cx = cx0; cx <= cx1; ++cx) {
      Chunk* c = findChunk(cx, cz);
      if (!c) continue;
      for (int sy = sy0; sy <= sy1; ++sy) remeshNow(*c, sy);
    }
}

void World::recordRemoval(int x, int y, int z, BlockId before) {
  if (changes_.size() < 4096) changes_.push_back({{x, y, z}, before, block::Air, false});
}

bool World::setBlock(int x, int y, int z, BlockId id, uint8_t meta, unsigned flags) {
  if (y < 0 || y >= kChunkHeight || id >= block::Count) return false;
  const int cx = floorDiv(x, 16), cz = floorDiv(z, 16);
  Chunk* c = findChunk(cx, cz);
  if (!c || !isEditable(cx, cz)) return false;
  const int lx = x - cx * 16, lz = z - cz * 16;
  const BlockId old = c->block(lx, y, lz);
  const uint8_t oldMeta = c->meta(lx, y, lz);
  if (id == block::Air) meta = 0;
  if (old == id && oldMeta == meta) return true;
  c->setBlock(lx, y, lz, id, meta);
  ++c->version;
  c->modified = true;
  const uint8_t* opacity = lightOpacityTable();
  const uint8_t* emission = lightEmissionTable();
  if (opacity[old] != opacity[id] || emission[old] != emission[id]) {
    LightAccess access{*this};
    relightAt(access, x, y, z);
  }
  markAround(x, y, z);
  if (flags & kSyncMesh) syncRemeshAround(x, y, z);
  if (flags & kByPlayer) {
    if (changes_.size() < 4096) changes_.push_back({{x, y, z}, old, id, true});
  }
  if (flags & kNotify) notifyAround(x, y, z);
  return true;
}

void World::notifyAround(int x, int y, int z) {
  for (int d = -1; d < 6; ++d) {
    const int nx = d < 0 ? x : x + kFaceDir[d][0];
    const int ny = d < 0 ? y : y + kFaceDir[d][1];
    const int nz = d < 0 ? z : z + kFaceDir[d][2];
    const BlockId b = getBlock(nx, ny, nz);
    if (isLiquid(b)) scheduleFluid(nx, ny, nz, b == block::Water ? 5 : 30);
  }
  checkSupport(x, y + 1, z);
  checkGravity(x, y, z);
  checkGravity(x, y + 1, z);
}

void World::checkSupport(int x, int y, int z) {
  if (y <= 0 || y >= kChunkHeight) return;
  const BlockId b = getBlock(x, y, z);
  if (!blockInfo(b).needsSupport) return;
  if (canSurviveOn(b, getBlock(x, y - 1, z))) return;
  if (setBlock(x, y, z, block::Air, 0, kNotify)) recordRemoval(x, y, z, b);
}

void World::checkGravity(int x, int y, int z) {
  if (y <= 0 || y >= kChunkHeight) return;
  const BlockId b = getBlock(x, y, z);
  if (!blockInfo(b).gravity) return;
  const BlockId below = getBlock(x, y - 1, z);
  const BlockInfo& bi = blockInfo(below);
  if (bi.solid) return;
  if (!isBlockEditable(x, z)) return;
  if (setBlock(x, y, z, block::Air, 0, kNotify)) falling_.push_back({dvec3(x + 0.5, y, z + 0.5), 0.0f, b});
}

void World::updateEntities(float dt) {
  for (size_t i = 0; i < falling_.size();) {
    FallingBlock& f = falling_[i];
    const int bx = fastFloor(f.pos.x), bz = fastFloor(f.pos.z);
    if (!isBlockEditable(bx, bz)) {
      ++i;  // wait until the area is ready again
      continue;
    }
    f.velocity = std::max(f.velocity - kGravity * dt, -40.0f);
    const double newY = f.pos.y + static_cast<double>(f.velocity) * dt;
    const int cell = fastFloor(newY);
    if (cell < 1 || isSolidAt(bx, cell, bz)) {
      // Take the entity out before editing: landing may start other blocks
      // falling, which appends to falling_ and would invalidate `f`.
      const BlockId id = f.id;
      falling_[i] = falling_.back();
      falling_.pop_back();
      // Blocks of a falling stack can overlap for a frame while the ones below
      // settle; land on the first free cell above the ground instead.
      int land = std::max(1, cell + 1);
      for (int k = 0; k < 4 && land < kChunkHeight - 1 && isSolidAt(bx, land, bz); ++k) ++land;
      const BlockId there = getBlock(bx, land, bz);
      const BlockInfo& ti = blockInfo(there);
      const bool canPlace = there == block::Air || ti.liquid || ti.replaceable;
      if (!(canPlace && setBlock(bx, land, bz, id, 0, kNotify)))
        if (changes_.size() < 4096) changes_.push_back({{bx, land, bz}, id, block::Air, false});
      continue;
    }
    f.pos.y = newY;
    ++i;
  }
}

// ---------------------------------------------------------------------------
// Fluids
// ---------------------------------------------------------------------------
void World::scheduleFluid(int x, int y, int z, int delay) {
  if (y < 0 || y >= kChunkHeight) return;
  const uint64_t key = tickKey(x, y, z);
  if (!tickSet_.insert(key).second) return;
  ticks_.push({tickCount_ + delay, tickOrder_++, x, y, z});
}

void World::tick() {
  ++tickCount_;
  int budget = 4096;
  std::vector<ScheduledTick> deferred;
  while (!ticks_.empty() && ticks_.top().due <= tickCount_ && budget-- > 0) {
    const ScheduledTick t = ticks_.top();
    ticks_.pop();
    tickSet_.erase(tickKey(t.x, t.y, t.z));
    if (!isBlockEditable(t.x, t.z)) {
      if (isChunkLoaded(floorDiv(t.x, 16), floorDiv(t.z, 16))) deferred.push_back(t);
      continue;
    }
    fluidTick(t.x, t.y, t.z);
  }
  for (const ScheduledTick& t : deferred) scheduleFluid(t.x, t.y, t.z, 20);
}

bool World::flowInto(int x, int y, int z, BlockId liquid, uint8_t meta) {
  if (y < 0 || y >= kChunkHeight) return false;
  const BlockId t = getBlock(x, y, z);
  const int delay = liquid == block::Water ? 5 : 30;
  if (t == liquid) {
    const uint8_t tm = getMeta(x, y, z);
    if (tm == 0) return false;  // never overwrite a source
    const bool tFalling = (tm & kLiquidFalling) != 0;
    const bool mFalling = (meta & kLiquidFalling) != 0;
    const int tEff = tFalling ? 0 : (tm & kLiquidLevelMask);
    const int mEff = mFalling ? 0 : (meta & kLiquidLevelMask);
    if ((mFalling && !tFalling) || (!tFalling && mEff < tEff)) {
      setBlock(x, y, z, liquid, meta, kNotify);
      scheduleFluid(x, y, z, delay);
      return true;
    }
    return false;
  }
  if (isLiquid(t)) {
    // Water and lava meet.
    if (liquid == block::Water)
      setBlock(x, y, z, getMeta(x, y, z) == 0 ? block::Basalt : block::Cobblestone, 0, kNotify);
    else
      setBlock(x, y, z, block::Stone, 0, kNotify);
    return false;
  }
  const BlockInfo& ti = blockInfo(t);
  if (t == block::Air || !ti.solid) {
    if (t != block::Air) recordRemoval(x, y, z, t);  // plants and torches get washed away
    setBlock(x, y, z, liquid, meta, kNotify);
    scheduleFluid(x, y, z, delay);
    return true;
  }
  return false;
}

void World::fluidTick(int x, int y, int z) {
  const BlockId b = getBlock(x, y, z);
  if (!isLiquid(b)) return;
  const bool water = b == block::Water;
  const int step = water ? 1 : 2;
  const int maxLevel = water ? 7 : 6;
  const int delay = water ? 5 : 30;
  const uint8_t m = getMeta(x, y, z);
  const int level = m & kLiquidLevelMask;
  const bool falling = (m & kLiquidFalling) != 0;
  const bool source = m == 0;

  if (!water) {
    for (int d = 0; d < 6; ++d)
      if (getBlock(x + kFaceDir[d][0], y + kFaceDir[d][1], z + kFaceDir[d][2]) == block::Water) {
        setBlock(x, y, z, source ? block::Basalt : block::Cobblestone, 0, kNotify);
        return;
      }
  }

  if (!source) {
    int newLevel = -1;
    bool newFalling = false;
    if (getBlock(x, y + 1, z) == b) {
      newLevel = 0;
      newFalling = true;
    } else {
      int best = 100, sources = 0;
      for (const auto& h : kHorizontal) {
        const int nx = x + h[0], nz = z + h[1];
        if (getBlock(nx, y, nz) != b) continue;
        const uint8_t nm = getMeta(nx, y, nz);
        best = std::min(best, (nm & kLiquidFalling) ? 0 : (nm & kLiquidLevelMask));
        if (nm == 0) ++sources;
      }
      if (water && sources >= 2) {
        const BlockId below = getBlock(x, y - 1, z);
        if (isSolid(below) || (below == b && getMeta(x, y - 1, z) == 0)) newLevel = 0;
      }
      if (newLevel < 0 && best + step <= maxLevel) newLevel = best + step;
    }
    if (newLevel < 0) {
      setBlock(x, y, z, block::Air, 0, kNotify);
      return;
    }
    const uint8_t newMeta = static_cast<uint8_t>(newLevel | (newFalling ? kLiquidFalling : 0));
    if (newMeta != m) {
      setBlock(x, y, z, b, newMeta, kNotify);
      scheduleFluid(x, y, z, delay);
      return;
    }
  }

  // Spread: downwards first, sideways when the way down is blocked.
  bool sideways = true;
  if (y > 0) {
    const BlockId below = getBlock(x, y - 1, z);
    if (below == b) {
      if (getMeta(x, y - 1, z) != 0) sideways = false;  // part of a falling column
    } else if (flowInto(x, y - 1, z, b, kLiquidFalling)) {
      sideways = source;
    } else if (!isSolid(getBlock(x, y - 1, z)) && !isLiquid(getBlock(x, y - 1, z))) {
      sideways = false;
    }
  }
  if (!sideways) return;
  const int out = (falling ? 0 : level) + step;
  if (out > maxLevel) return;
  for (const auto& h : kHorizontal) flowInto(x + h[0], y, z + h[1], b, static_cast<uint8_t>(out));
}

// ---------------------------------------------------------------------------
std::vector<SectionMeshUpdate> World::takeMeshUpdates() {
  std::vector<SectionMeshUpdate> out;
  out.swap(meshUpdates_);
  return out;
}

std::vector<BlockChange> World::takeChanges() {
  std::vector<BlockChange> out;
  out.swap(changes_);
  return out;
}

void World::saveAll() {
  std::vector<uint8_t> blocks(kChunkVolume), meta(kChunkVolume);
  int saved = 0;
  for (auto& kv : chunks_) {
    Chunk& c = *kv.second;
    if (!c.modified) continue;
    c.copyBlocks(blocks.data());
    c.copyMeta(meta.data());
    if (storage_.saveChunk(c.cx(), c.cz(), blocks.data(), meta.data())) {
      c.modified = false;
      ++saved;
    }
  }
  if (saved > 0) LOG_INFO("saved %d chunk(s)", saved);
}

WorldStats World::stats() const {
  WorldStats s;
  s.chunks = chunks_.size();
  for (const auto& kv : chunks_) {
    if (kv.second->lit) ++s.litChunks;
    for (bool h : kv.second->hasMesh)
      if (h) ++s.meshedSections;
  }
  s.genInFlight = genPending_.size();
  s.lightInFlight = lightPending_.size();
  s.meshInFlight = meshInFlight_;
  s.scheduledTicks = ticks_.size();
  return s;
}

}  // namespace strata
