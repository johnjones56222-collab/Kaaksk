#include "world/storage.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>

#include "core/image_io.hpp"
#include "core/log.hpp"
#include "world/chunk.hpp"

namespace strata {

namespace fs = std::filesystem;

namespace {
constexpr char kChunkMagic[4] = {'S', 'T', 'R', 'C'};
constexpr uint32_t kChunkVersion = 1;

void putU32(std::string& s, uint32_t v) {
  for (int i = 0; i < 4; ++i) s.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}
uint32_t getU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

bool readFile(const fs::path& path, std::vector<uint8_t>& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  const std::streamoff size = f.tellg();
  if (size < 0) return false;
  f.seekg(0, std::ios::beg);
  out.resize(static_cast<size_t>(size));
  if (size > 0) f.read(reinterpret_cast<char*>(out.data()), size);
  return static_cast<bool>(f);
}
}  // namespace

std::vector<uint8_t> rleEncode(const uint8_t* data, size_t size) {
  // Pairs of (value, run length as LEB128 varint).
  std::vector<uint8_t> out;
  out.reserve(size / 8 + 16);
  size_t i = 0;
  while (i < size) {
    const uint8_t v = data[i];
    size_t run = 1;
    while (i + run < size && data[i + run] == v) ++run;
    out.push_back(v);
    size_t r = run;
    do {
      uint8_t byte = static_cast<uint8_t>(r & 0x7F);
      r >>= 7;
      if (r) byte |= 0x80;
      out.push_back(byte);
    } while (r);
    i += run;
  }
  return out;
}

bool rleDecode(const uint8_t* src, size_t srcSize, uint8_t* out, size_t outSize) {
  size_t i = 0, o = 0;
  while (i < srcSize) {
    const uint8_t v = src[i++];
    size_t run = 0;
    int shift = 0;
    for (;;) {
      if (i >= srcSize || shift > 28) return false;
      const uint8_t byte = src[i++];
      run |= static_cast<size_t>(byte & 0x7F) << shift;
      shift += 7;
      if (!(byte & 0x80)) break;
    }
    if (run == 0 || run > outSize - o) return false;
    std::memset(out + o, v, run);
    o += run;
  }
  return o == outSize;
}

bool writeFileAtomic(const fs::path& path, const std::string& data) {
  fs::path tmp = path;
  tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    f.flush();
    if (!f) return false;
  }
  std::error_code ec;
  fs::rename(tmp, path, ec);
  if (ec) {
    // Some platforms refuse to rename over an existing file.
    fs::remove(path, ec);
    ec.clear();
    fs::rename(tmp, path, ec);
    if (ec) {
      fs::remove(tmp, ec);
      return false;
    }
  }
  return true;
}

WorldStorage::WorldStorage(fs::path directory) : dir_(std::move(directory)) {
  std::error_code ec;
  fs::create_directories(dir_ / "chunks", ec);
  ok_ = !ec && fs::is_directory(dir_ / "chunks", ec);
  if (!ok_) LOG_ERROR("could not create world directory %s", dir_.string().c_str());
}

fs::path WorldStorage::chunkPath(int cx, int cz) const {
  return dir_ / "chunks" / ("c." + std::to_string(cx) + "." + std::to_string(cz) + ".dat");
}

bool WorldStorage::saveChunk(int cx, int cz, const uint8_t* blocks, const uint8_t* meta) const {
  if (!ok_) return false;
  std::vector<uint8_t> raw(static_cast<size_t>(kChunkVolume) * 2);
  std::memcpy(raw.data(), blocks, kChunkVolume);
  std::memcpy(raw.data() + kChunkVolume, meta, kChunkVolume);
  const std::vector<uint8_t> packed = rleEncode(raw.data(), raw.size());
  std::string file;
  file.reserve(packed.size() + 32);
  file.append(kChunkMagic, 4);
  putU32(file, kChunkVersion);
  putU32(file, static_cast<uint32_t>(cx));
  putU32(file, static_cast<uint32_t>(cz));
  putU32(file, static_cast<uint32_t>(raw.size()));
  putU32(file, static_cast<uint32_t>(packed.size()));
  putU32(file, crc32(packed.data(), packed.size()));
  file.append(reinterpret_cast<const char*>(packed.data()), packed.size());
  if (!writeFileAtomic(chunkPath(cx, cz), file)) {
    LOG_ERROR("failed to save chunk %d,%d", cx, cz);
    return false;
  }
  return true;
}

bool WorldStorage::loadChunk(int cx, int cz, uint8_t* blocks, uint8_t* meta) const {
  if (!ok_) return false;
  const fs::path path = chunkPath(cx, cz);
  std::error_code ec;
  if (!fs::exists(path, ec)) return false;
  std::vector<uint8_t> file;
  if (!readFile(path, file) || file.size() < 28 || std::memcmp(file.data(), kChunkMagic, 4) != 0) {
    LOG_WARN("chunk file %s is damaged; regenerating", path.string().c_str());
    return false;
  }
  const uint32_t version = getU32(&file[4]);
  const int32_t fcx = static_cast<int32_t>(getU32(&file[8]));
  const int32_t fcz = static_cast<int32_t>(getU32(&file[12]));
  const uint32_t rawSize = getU32(&file[16]);
  const uint32_t packedSize = getU32(&file[20]);
  const uint32_t crc = getU32(&file[24]);
  if (version != kChunkVersion || fcx != cx || fcz != cz || rawSize != static_cast<uint32_t>(kChunkVolume) * 2 ||
      packedSize != file.size() - 28 || crc32(file.data() + 28, packedSize) != crc) {
    LOG_WARN("chunk file %s failed validation; regenerating", path.string().c_str());
    return false;
  }
  std::vector<uint8_t> raw(rawSize);
  if (!rleDecode(file.data() + 28, packedSize, raw.data(), raw.size())) {
    LOG_WARN("chunk file %s could not be decoded; regenerating", path.string().c_str());
    return false;
  }
  std::memcpy(blocks, raw.data(), kChunkVolume);
  std::memcpy(meta, raw.data() + kChunkVolume, kChunkVolume);
  // Unknown block ids (from a newer version) become air rather than garbage.
  for (int i = 0; i < kChunkVolume; ++i)
    if (blocks[i] >= block::Count) {
      blocks[i] = block::Air;
      meta[i] = 0;
    }
  return true;
}

bool WorldStorage::saveLevel(const LevelData& l) const {
  if (!ok_) return false;
  std::ostringstream o;
  o.precision(17);
  o << "strata_level 1\n";
  o << "seed " << l.seed << "\n";
  o << "time " << l.worldTime << "\n";
  o << "creative " << (l.creative ? 1 : 0) << "\n";
  if (l.hasSpawn) o << "spawn " << l.spawn.x << " " << l.spawn.y << " " << l.spawn.z << "\n";
  if (l.hasPlayer) {
    o << "player " << l.playerPos.x << " " << l.playerPos.y << " " << l.playerPos.z << "\n";
    o << "look " << l.yaw << " " << l.pitch << "\n";
    o << "flying " << (l.flying ? 1 : 0) << "\n";
    o << "health " << l.health << "\n";
    o << "selected " << l.selectedSlot << "\n";
    for (int i = 0; i < Inventory::kSlots; ++i) {
      const ItemStack& s = l.inventory.slots[i];
      if (!s.empty()) o << "slot " << i << " " << s.id << " " << s.count << "\n";
    }
  }
  return writeFileAtomic(dir_ / "level.txt", o.str());
}

bool WorldStorage::loadLevel(LevelData& l) const {
  std::ifstream f(dir_ / "level.txt");
  if (!f) return false;
  std::string header;
  int version = 0;
  if (!(f >> header >> version) || header != "strata_level" || version != 1) {
    LOG_WARN("level.txt in %s is not a Strata level file", dir_.string().c_str());
    return false;
  }
  LevelData r;
  bool haveSeed = false;
  std::string line;
  std::getline(f, line);
  while (std::getline(f, line)) {
    std::istringstream in(line);
    std::string key;
    if (!(in >> key)) continue;
    if (key == "seed") {
      haveSeed = static_cast<bool>(in >> r.seed);
    } else if (key == "time") {
      in >> r.worldTime;
    } else if (key == "creative") {
      int v = 0;
      in >> v;
      r.creative = v != 0;
    } else if (key == "spawn") {
      r.hasSpawn = static_cast<bool>(in >> r.spawn.x >> r.spawn.y >> r.spawn.z);
    } else if (key == "player") {
      r.hasPlayer = static_cast<bool>(in >> r.playerPos.x >> r.playerPos.y >> r.playerPos.z);
    } else if (key == "look") {
      in >> r.yaw >> r.pitch;
    } else if (key == "flying") {
      int v = 0;
      in >> v;
      r.flying = v != 0;
    } else if (key == "health") {
      in >> r.health;
    } else if (key == "selected") {
      in >> r.selectedSlot;
      r.selectedSlot = clampv(r.selectedSlot, 0, Inventory::kHotbar - 1);
    } else if (key == "slot") {
      int slot = -1, id = 0, count = 0;
      if (in >> slot >> id >> count && slot >= 0 && slot < Inventory::kSlots && count > 0 &&
          isValidItem(static_cast<ItemId>(id))) {
        r.inventory.slots[slot].id = static_cast<ItemId>(id);
        r.inventory.slots[slot].count = static_cast<uint16_t>(std::min(count, 64));
      }
    }
  }
  if (!haveSeed) return false;
  if (!std::isfinite(r.playerPos.x) || !std::isfinite(r.playerPos.y) || !std::isfinite(r.playerPos.z))
    r.hasPlayer = false;
  if (!std::isfinite(r.health) || r.health <= 0.0f || r.health > 20.0f) r.health = 20.0f;
  l = r;
  return true;
}

}  // namespace strata
