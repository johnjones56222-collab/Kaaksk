#include "world/mesher.hpp"

#include <algorithm>
#include <cmath>

#include "world/block.hpp"

namespace strata {

namespace {

// Corners of every face of the unit cube (BL, BR, TR, TL), counter-clockwise
// when seen from outside the cube.
constexpr int8_t kFaceCorners[6][4][3] = {
    {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},  // +X
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},  // -X
    {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},  // +Y
    {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},  // -Y
    {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},  // +Z
    {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},  // -Z
};
// Texture coordinates of those corners: u to the right, v downwards.
constexpr uint8_t kFaceUV[6][4][2] = {
    {{0, 1}, {1, 1}, {1, 0}, {0, 0}},  // +X
    {{0, 1}, {1, 1}, {1, 0}, {0, 0}},  // -X
    {{0, 1}, {1, 1}, {1, 0}, {0, 0}},  // +Y  (u = x, v = z)
    {{0, 0}, {1, 0}, {1, 1}, {0, 1}},  // -Y  (u = x, v = z)
    {{0, 1}, {1, 1}, {1, 0}, {0, 0}},  // +Z
    {{0, 1}, {1, 1}, {1, 0}, {0, 0}},  // -Z
};

constexpr uint8_t kAoFactor[4] = {112, 163, 212, 255};  // 0.44, 0.64, 0.83, 1.0

inline uint32_t posHash(int x, int y, int z) {
  uint32_t h = static_cast<uint32_t>(x) * 0x8DA6B343u ^ static_cast<uint32_t>(y) * 0xD8163841u ^
               static_cast<uint32_t>(z) * 0xCB1AB31Fu;
  h ^= h >> 15;
  h *= 0x2C1B3C6Du;
  h ^= h >> 12;
  return h;
}

bool textureTintable(uint16_t t) {
  switch (t) {
    case tex::GrassTop:
    case tex::GrassSide:
    case tex::OakLeaves:
    case tex::TallGrass:
    case tex::Fern:
      return true;
    default:
      return false;
  }
}

// Blocks whose top faces may be rotated randomly to hide texture tiling.
bool naturalBlock(BlockId b) {
  switch (b) {
    case block::Stone:
    case block::Dirt:
    case block::Grass:
    case block::Sand:
    case block::Gravel:
    case block::Baserock:
    case block::Snow:
    case block::SnowyGrass:
    case block::Clay:
    case block::Cobblestone:
    case block::CoalOre:
    case block::IronOre:
    case block::GoldOre:
    case block::DiamondOre:
    case block::Basalt:
    case block::OakLog:
    case block::BirchLog:
    case block::SpruceLog:
    case block::OakLeaves:
    case block::BirchLeaves:
    case block::SpruceLeaves:
      return true;
    default:
      return false;
  }
}

class Builder {
 public:
  Builder(const MeshInput& in, MeshData& out) : in_(in), out_(out) {}

  void run() {
    for (int y = 0; y < 16; ++y)
      for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
          const BlockId b = blockAt(x, y, z);
          if (b == block::Air) continue;
          const BlockInfo& info = blockInfo(b);
          switch (info.shape) {
            case Shape::Cube: addCube(x, y, z, b, info); break;
            case Shape::Cross: addCross(x, y, z, b, info); break;
            case Shape::Liquid: addLiquid(x, y, z, b); break;
            case Shape::Torch: addTorch(x, y, z, info); break;
            case Shape::None: break;
          }
        }
  }

 private:
  BlockId blockAt(int x, int y, int z) const { return in_.blocks[padIndex(x, y, z)]; }
  uint8_t metaAt(int x, int y, int z) const { return in_.meta[padIndex(x, y, z)]; }
  uint8_t lightAt(int x, int y, int z) const { return in_.light[padIndex(x, y, z)]; }
  bool occludes(int x, int y, int z) const { return blockInfo(blockAt(x, y, z)).opaque; }

  std::vector<TerrainVertex>& layerFor(const BlockInfo& info, BlockId b) {
    if (info.liquid) return b == block::Water ? out_.water : out_.opaque;
    switch (info.layer) {
      case Layer::Opaque: return out_.opaque;
      case Layer::Cutout: return out_.cutout;
      case Layer::Translucent: return out_.glass;
    }
    return out_.opaque;
  }

  void tintAt(Tint tint, int x, int z, int cx, int cz, uint8_t rgb[3]) const {
    if (tint == Tint::None) {
      rgb[0] = rgb[1] = rgb[2] = 255;
      return;
    }
    // Average the four columns that share this vertex for smooth biome blends.
    int sum[3] = {0, 0, 0};
    for (int dz = cz - 1; dz <= cz; ++dz)
      for (int dx = cx - 1; dx <= cx; ++dx) {
        const int c = padColumn(x + dx, z + dz);
        const uint8_t* src = tint == Tint::Grass ? in_.grass[c] : in_.foliage[c];
        for (int i = 0; i < 3; ++i) sum[i] += src[i];
      }
    for (int i = 0; i < 3; ++i) rgb[i] = static_cast<uint8_t>(sum[i] / 4);
  }

  // Smooth light and ambient occlusion for one face corner. (nx, ny, nz) is the
  // cell in front of the face, c the corner of the unit cube.
  void cornerLight(int face, int nx, int ny, int nz, const int8_t c[3], bool withAo, uint8_t& ao, uint8_t& sky,
                   uint8_t& blk) const {
    const int axisN = face / 2;
    const int a1 = (axisN + 1) % 3;
    const int a2 = (axisN + 2) % 3;
    int o1[3] = {0, 0, 0}, o2[3] = {0, 0, 0};
    o1[a1] = c[a1] ? 1 : -1;
    o2[a2] = c[a2] ? 1 : -1;
    const int s1x = nx + o1[0], s1y = ny + o1[1], s1z = nz + o1[2];
    const int s2x = nx + o2[0], s2y = ny + o2[1], s2z = nz + o2[2];
    const int ccx = nx + o1[0] + o2[0], ccy = ny + o1[1] + o2[1], ccz = nz + o1[2] + o2[2];
    const bool occ1 = occludes(s1x, s1y, s1z);
    const bool occ2 = occludes(s2x, s2y, s2z);
    const bool occC = occludes(ccx, ccy, ccz);
    int level = 3;
    if (withAo) level = (occ1 && occ2) ? 0 : 3 - (static_cast<int>(occ1) + static_cast<int>(occ2) + static_cast<int>(occC));
    ao = kAoFactor[level];
    int skySum = 0, blkSum = 0, n = 0;
    auto add = [&](int x, int y, int z) {
      const uint8_t l = lightAt(x, y, z);
      skySum += skyOf(l);
      blkSum += blockLightOf(l);
      ++n;
    };
    add(nx, ny, nz);
    if (!occ1) add(s1x, s1y, s1z);
    if (!occ2) add(s2x, s2y, s2z);
    if (!occC && !(occ1 && occ2)) add(ccx, ccy, ccz);
    sky = static_cast<uint8_t>((skySum * 17 + n / 2) / n);
    blk = static_cast<uint8_t>((blkSum * 17 + n / 2) / n);
  }

  static void pushQuad(std::vector<TerrainVertex>& dst, const TerrainVertex v[4]) {
    // Split along the diagonal that keeps darker corners from bleeding across the quad.
    auto bright = [](const TerrainVertex& t) { return static_cast<int>(t.ao) * 2 + t.sky + t.blk; };
    if (bright(v[0]) + bright(v[2]) < bright(v[1]) + bright(v[3])) {
      dst.push_back(v[1]);
      dst.push_back(v[2]);
      dst.push_back(v[3]);
      dst.push_back(v[0]);
    } else {
      dst.insert(dst.end(), v, v + 4);
    }
  }

  static void pushReversed(std::vector<TerrainVertex>& dst, const TerrainVertex v[4], uint8_t extraFlags) {
    TerrainVertex r[4] = {v[3], v[2], v[1], v[0]};
    for (TerrainVertex& t : r) t.flags = static_cast<uint8_t>(t.flags | extraFlags);
    dst.insert(dst.end(), r, r + 4);
  }

  static int16_t pos(int block, float offset) {
    return static_cast<int16_t>(block * kPosScale + static_cast<int>(std::lround(offset * kPosScale)));
  }

  void addCube(int x, int y, int z, BlockId b, const BlockInfo& info) {
    const bool cactus = b == block::Cactus;
    const int wx = in_.cx * 16 + x, wy = in_.sy * 16 + y, wz = in_.cz * 16 + z;
    const uint32_t h = posHash(wx, wy, wz);
    const bool natural = naturalBlock(b);
    std::vector<TerrainVertex>& dst = layerFor(info, b);
    for (int f = 0; f < 6; ++f) {
      const int nx = x + kFaceDir[f][0], ny = y + kFaceDir[f][1], nz = z + kFaceDir[f][2];
      const BlockId nb = blockAt(nx, ny, nz);
      const BlockInfo& ni = blockInfo(nb);
      const bool side = f != FacePosY && f != FaceNegY;
      if (cactus && side) {
        // Inset sides are always visible.
      } else {
        if (ni.opaque) continue;
        if (nb == b && (info.cullsSame || cactus)) continue;
      }
      const uint16_t texId = info.tex[f];
      const bool tinted = info.tint != Tint::None && textureTintable(texId);
      // The light in front of an inset or non-full face comes from this cell when the neighbour is solid.
      const bool frontOpaque = ni.opaque;
      const int lx = frontOpaque ? x : nx, ly = frontOpaque ? y : ny, lz = frontOpaque ? z : nz;
      // Random rotation / mirroring breaks up visible repetition of textures.
      int rot = 0;
      bool mirror = false;
      if (natural) {
        if (side)
          mirror = ((h >> (f * 2)) & 1u) != 0;
        else
          rot = static_cast<int>((h >> 13) & 3u);
      }
      TerrainVertex v[4];
      for (int k = 0; k < 4; ++k) {
        const int8_t* c = kFaceCorners[f][k];
        float ox = c[0], oy = c[1], oz = c[2];
        if (cactus && side) {
          const float inset = 1.0f / 16.0f;
          if (f == FacePosX) ox = 1.0f - inset;
          if (f == FaceNegX) ox = inset;
          if (f == FacePosZ) oz = 1.0f - inset;
          if (f == FaceNegZ) oz = inset;
        }
        TerrainVertex& t = v[k];
        t.x = pos(x, ox);
        t.y = pos(y, oy);
        t.z = pos(z, oz);
        t.tex = texId;
        int u = kFaceUV[f][k][0], vv = kFaceUV[f][k][1];
        if (mirror) u = 1 - u;
        for (int r = 0; r < rot; ++r) {
          const int nu = 1 - vv;
          vv = u;
          u = nu;
        }
        t.u = static_cast<uint8_t>(u * 16);
        t.v = static_cast<uint8_t>(vv * 16);
        t.normal = static_cast<uint8_t>(f);
        cornerLight(f, lx, ly, lz, c, !frontOpaque, t.ao, t.sky, t.blk);
        t.anim = info.wave == Wave::Leaves ? kAnimLeaves : kAnimNone;
        t.flags = static_cast<uint8_t>((tinted ? kFlagTint : 0) |
                                       (tinted && info.layer == Layer::Opaque ? kFlagTintMask : 0) |
                                       (info.wave == Wave::Leaves ? kFlagFoliage : 0));
        uint8_t rgb[3];
        tintAt(tinted ? info.tint : Tint::None, x, z, c[0], c[2], rgb);
        t.r = rgb[0];
        t.g = rgb[1];
        t.b = rgb[2];
        t.a = 255;
      }
      pushQuad(dst, v);
    }
  }

  void addCross(int x, int y, int z, BlockId b, const BlockInfo& info) {
    const int wx = in_.cx * 16 + x, wy = in_.sy * 16 + y, wz = in_.cz * 16 + z;
    const uint32_t h = posHash(wx, wy, wz);
    // Small random offset so fields of grass do not look like a grid.
    const float jx = (static_cast<float>(h & 7u) - 3.5f) / 32.0f;
    const float jz = (static_cast<float>((h >> 3) & 7u) - 3.5f) / 32.0f;
    const uint8_t l = lightAt(x, y, z);
    const uint8_t sky = static_cast<uint8_t>(skyOf(l) * 17);
    const uint8_t blk = static_cast<uint8_t>(blockLightOf(l) * 17);
    const bool tinted = info.tint != Tint::None;
    std::vector<TerrainVertex>& dst = layerFor(info, b);
    constexpr float lo = 1.0f / 16.0f, hi = 15.0f / 16.0f;
    const float diag[2][4] = {{lo, lo, hi, hi}, {lo, hi, hi, lo}};  // x0, z0, x1, z1
    for (const auto& d : diag) {
      const float px[4] = {d[0], d[2], d[2], d[0]};
      const float pz[4] = {d[1], d[3], d[3], d[1]};
      const float py[4] = {0.0f, 0.0f, 1.0f, 1.0f};
      const uint8_t uv[4][2] = {{0, 16}, {16, 16}, {16, 0}, {0, 0}};
      TerrainVertex v[4];
      for (int k = 0; k < 4; ++k) {
        TerrainVertex& t = v[k];
        t.x = pos(x, px[k] + jx);
        t.y = pos(y, py[k]);
        t.z = pos(z, pz[k] + jz);
        t.tex = info.tex[0];
        t.u = uv[k][0];
        t.v = uv[k][1];
        t.normal = 6;
        t.ao = 255;
        t.sky = sky;
        t.blk = blk;
        t.anim = (info.wave == Wave::Plant && py[k] > 0.5f) ? kAnimPlant : kAnimNone;
        t.flags = tinted ? kFlagTint : 0;
        uint8_t rgb[3];
        tintAt(tinted ? info.tint : Tint::None, x, z, px[k] > 0.5f ? 1 : 0, pz[k] > 0.5f ? 1 : 0, rgb);
        t.r = rgb[0];
        t.g = rgb[1];
        t.b = rgb[2];
        t.a = 255;
      }
      dst.insert(dst.end(), v, v + 4);
      pushReversed(dst, v, 0);
    }
  }

  void addTorch(int x, int y, int z, const BlockInfo& info) {
    const float mn[3] = {7.0f / 16, 0.0f, 7.0f / 16};
    const float mx[3] = {9.0f / 16, 10.0f / 16, 9.0f / 16};
    const uint8_t l = lightAt(x, y, z);
    const uint8_t sky = static_cast<uint8_t>(skyOf(l) * 17);
    const uint8_t blk = static_cast<uint8_t>(blockLightOf(l) * 17);
    for (int f = 0; f < 6; ++f) {
      TerrainVertex v[4];
      for (int k = 0; k < 4; ++k) {
        const int8_t* c = kFaceCorners[f][k];
        TerrainVertex& t = v[k];
        t.x = pos(x, c[0] ? mx[0] : mn[0]);
        t.y = pos(y, c[1] ? mx[1] : mn[1]);
        t.z = pos(z, c[2] ? mx[2] : mn[2]);
        t.tex = info.tex[0];
        const int u = kFaceUV[f][k][0];
        const int vv = kFaceUV[f][k][1];
        if (f == FacePosY) {
          t.u = static_cast<uint8_t>(7 + u * 2);
          t.v = static_cast<uint8_t>(6 + vv * 2);
        } else if (f == FaceNegY) {
          t.u = static_cast<uint8_t>(7 + u * 2);
          t.v = static_cast<uint8_t>(14 + vv * 2);
        } else {
          t.u = static_cast<uint8_t>(7 + u * 2);
          t.v = static_cast<uint8_t>(6 + vv * 10);
        }
        t.normal = static_cast<uint8_t>(f);
        t.ao = 255;
        t.sky = sky;
        t.blk = blk;
        t.anim = kAnimNone;
        t.flags = 0;
        t.r = t.g = t.b = t.a = 255;
      }
      out_.cutout.insert(out_.cutout.end(), v, v + 4);
    }
  }

  void addLiquid(int x, int y, int z, BlockId b) {
    const bool water = b == block::Water;
    auto same = [&](int ax, int ay, int az) { return blockAt(ax, ay, az) == b; };
    const bool above = same(x, y + 1, z);
    // Corner heights indexed [cz][cx] in block units.
    float corner[2][2];
    for (int cz = 0; cz < 2; ++cz)
      for (int cx = 0; cx < 2; ++cx) {
        if (above) {
          corner[cz][cx] = 1.0f;
          continue;
        }
        float sum = 0.0f;
        int count = 0;
        bool full = false;
        for (int dz = cz - 1; dz <= cz && !full; ++dz)
          for (int dx = cx - 1; dx <= cx && !full; ++dx) {
            const int ax = x + dx, az = z + dz;
            if (same(ax, y, az)) {
              if (same(ax, y + 1, az)) {
                full = true;
              } else {
                sum += liquidHeight(metaAt(ax, y, az));
                ++count;
              }
            } else if (!isSolid(blockAt(ax, y, az))) {
              ++count;  // open air pulls the surface down towards the edge
            }
          }
        corner[cz][cx] = full ? 1.0f : (count > 0 ? sum / static_cast<float>(count) : liquidHeight(metaAt(x, y, z)));
      }

    std::vector<TerrainVertex>& dst = water ? out_.water : out_.opaque;
    const uint8_t anim = water ? kAnimWater : kAnimLava;
    for (int f = 0; f < 6; ++f) {
      const int nx = x + kFaceDir[f][0], ny = y + kFaceDir[f][1], nz = z + kFaceDir[f][2];
      if (f == FacePosY) {
        if (above) continue;
      } else {
        if (same(nx, ny, nz)) continue;
        if (blockInfo(blockAt(nx, ny, nz)).opaque) continue;
      }
      TerrainVertex v[4];
      for (int k = 0; k < 4; ++k) {
        const int8_t* c = kFaceCorners[f][k];
        const float top = corner[c[2]][c[0]];
        const float oy = c[1] ? top : 0.0f;
        TerrainVertex& t = v[k];
        t.x = pos(x, c[0]);
        t.y = pos(y, oy);
        t.z = pos(z, c[2]);
        t.tex = blockInfo(b).tex[f];
        t.u = static_cast<uint8_t>(kFaceUV[f][k][0] * 16);
        if (f == FacePosY || f == FaceNegY)
          t.v = static_cast<uint8_t>(kFaceUV[f][k][1] * 16);
        else
          t.v = static_cast<uint8_t>(c[1] ? std::lround((1.0f - top) * 16.0f) : 16);
        t.normal = static_cast<uint8_t>(f);
        cornerLight(f, nx, ny, nz, c, false, t.ao, t.sky, t.blk);
        t.anim = anim;
        t.flags = 0;
        t.r = t.g = t.b = t.a = 255;
      }
      dst.insert(dst.end(), v, v + 4);
      if (water) pushReversed(dst, v, kFlagBackFace);
    }
  }

  const MeshInput& in_;
  MeshData& out_;
};

}  // namespace

float liquidHeight(uint8_t meta) {
  if (meta & kLiquidFalling) return 14.0f / 16.0f;
  const int level = meta & kLiquidLevelMask;
  return (14.0f - 1.5f * static_cast<float>(level)) / 16.0f;
}

void buildSectionMesh(const MeshInput& in, MeshData& out) {
  out.opaque.clear();
  out.cutout.clear();
  out.water.clear();
  out.glass.clear();
  Builder(in, out).run();
}

}  // namespace strata
