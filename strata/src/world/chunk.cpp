#include "world/chunk.hpp"

#include <cstring>

namespace strata {

Chunk::Chunk(int cx, int cz) : pos_{cx, cz} {
  uniformLight_.fill(packLight(15, 0));
  heightmap_.fill(-1);
}

Section* Chunk::ensureSection(int s) {
  if (!sections_[s]) {
    auto sec = std::make_unique<Section>();
    std::memset(sec->blocks, 0, sizeof(sec->blocks));
    std::memset(sec->meta, 0, sizeof(sec->meta));
    std::memset(sec->light, uniformLight_[s], sizeof(sec->light));
    sec->nonAir = 0;
    sections_[s] = std::move(sec);
  }
  return sections_[s].get();
}

void Chunk::setBlock(int x, int y, int z, BlockId b, uint8_t m) {
  const int s = y >> 4;
  if (b == block::Air) m = 0;
  Section* sec = sections_[s].get();
  if (!sec) {
    if (b == block::Air) return;
    sec = ensureSection(s);
  }
  const int i = sectionIndex(x, y, z);
  const BlockId old = sec->blocks[i];
  if (old == block::Air && b != block::Air) ++sec->nonAir;
  if (old != block::Air && b == block::Air) --sec->nonAir;
  sec->blocks[i] = b;
  sec->meta[i] = m;
  int16_t& h = heightmap_[z * 16 + x];
  if (b != block::Air) {
    if (y > h) h = static_cast<int16_t>(y);
  } else if (y == h) {
    recomputeColumn(x, z);
  }
}

void Chunk::setMeta(int x, int y, int z, uint8_t m) {
  Section* sec = sections_[y >> 4].get();
  if (!sec) return;  // air has no metadata
  const int i = sectionIndex(x, y, z);
  if (sec->blocks[i] != block::Air) sec->meta[i] = m;
}

void Chunk::setLight(int x, int y, int z, uint8_t value) {
  const int s = y >> 4;
  Section* sec = sections_[s].get();
  if (!sec) {
    if (value == uniformLight_[s]) return;
    sec = ensureSection(s);
  }
  sec->light[sectionIndex(x, y, z)] = value;
}

void Chunk::recomputeColumn(int x, int z) {
  for (int y = kChunkHeight - 1; y >= 0; --y) {
    if (block(x, y, z) != block::Air) {
      heightmap_[z * 16 + x] = static_cast<int16_t>(y);
      return;
    }
  }
  heightmap_[z * 16 + x] = -1;
}

void Chunk::copyBlocks(uint8_t* out) const {
  for (int s = 0; s < kSectionCount; ++s) {
    uint8_t* dst = out + s * kSectionVolume;
    if (sections_[s])
      std::memcpy(dst, sections_[s]->blocks, kSectionVolume);
    else
      std::memset(dst, 0, kSectionVolume);
  }
}

void Chunk::copyMeta(uint8_t* out) const {
  for (int s = 0; s < kSectionCount; ++s) {
    uint8_t* dst = out + s * kSectionVolume;
    if (sections_[s])
      std::memcpy(dst, sections_[s]->meta, kSectionVolume);
    else
      std::memset(dst, 0, kSectionVolume);
  }
}

void Chunk::assign(const uint8_t* blocks, const uint8_t* meta) {
  for (int s = 0; s < kSectionCount; ++s) {
    const uint8_t* src = blocks + s * kSectionVolume;
    uint16_t nonAir = 0;
    for (int i = 0; i < kSectionVolume; ++i)
      if (src[i] != block::Air) ++nonAir;
    uniformLight_[s] = packLight(15, 0);
    if (nonAir == 0) {
      sections_[s].reset();
      continue;
    }
    Section* sec = ensureSection(s);
    std::memcpy(sec->blocks, src, kSectionVolume);
    if (meta) {
      const uint8_t* msrc = meta + s * kSectionVolume;
      for (int i = 0; i < kSectionVolume; ++i) sec->meta[i] = src[i] == block::Air ? 0 : msrc[i];
    } else {
      std::memset(sec->meta, 0, kSectionVolume);
    }
    std::memset(sec->light, packLight(15, 0), kSectionVolume);
    sec->nonAir = nonAir;
  }
  for (int z = 0; z < 16; ++z)
    for (int x = 0; x < 16; ++x) recomputeColumn(x, z);
}

void Chunk::trimSection(int s) {
  Section* sec = sections_[s].get();
  if (!sec || sec->nonAir != 0) return;
  const uint8_t first = sec->light[0];
  for (int i = 1; i < kSectionVolume; ++i)
    if (sec->light[i] != first) return;
  uniformLight_[s] = first;
  sections_[s].reset();
}

void Chunk::applyLight(const uint8_t* light) {
  for (int s = 0; s < kSectionCount; ++s) {
    const uint8_t* src = light + s * kSectionVolume;
    Section* sec = sections_[s].get();
    if (!sec) {
      bool uniform = true;
      for (int i = 1; i < kSectionVolume && uniform; ++i) uniform = src[i] == src[0];
      if (uniform) {
        uniformLight_[s] = src[0];
        continue;
      }
      sec = ensureSection(s);
    }
    std::memcpy(sec->light, src, kSectionVolume);
    trimSection(s);
  }
}

}  // namespace strata
