// Saving and loading worlds. Only chunks changed by the player are stored;
// everything else is regenerated from the seed.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "world/items.hpp"

namespace strata {

std::vector<uint8_t> rleEncode(const uint8_t* data, size_t size);
// Returns false unless the stream decodes to exactly `outSize` bytes.
bool rleDecode(const uint8_t* src, size_t srcSize, uint8_t* out, size_t outSize);

struct LevelData {
  uint64_t seed = 0;
  int64_t worldTime = 0;  // ticks since creation (20 per second)
  bool creative = false;
  bool hasPlayer = false;
  dvec3 playerPos;
  float yaw = 0.0f, pitch = 0.0f;
  bool flying = false;
  float health = 20.0f;
  ivec3 spawn;
  bool hasSpawn = false;
  int selectedSlot = 0;
  Inventory inventory;
};

class WorldStorage {
 public:
  explicit WorldStorage(std::filesystem::path directory);

  const std::filesystem::path& directory() const { return dir_; }
  bool ok() const { return ok_; }

  // Safe to call from several threads for different chunks.
  bool saveChunk(int cx, int cz, const uint8_t* blocks, const uint8_t* meta) const;
  // Returns false when the chunk was never saved or the file is damaged.
  bool loadChunk(int cx, int cz, uint8_t* blocks, uint8_t* meta) const;

  bool saveLevel(const LevelData& level) const;
  bool loadLevel(LevelData& level) const;

 private:
  std::filesystem::path chunkPath(int cx, int cz) const;

  std::filesystem::path dir_;
  bool ok_ = false;
};

// Writes `data` to `path` via a temporary file and rename, so a crash never
// leaves a half written file behind.
bool writeFileAtomic(const std::filesystem::path& path, const std::string& data);

}  // namespace strata
