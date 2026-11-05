// Voxel ray traversal (Amanatides & Woo) with exact tests against the
// selection boxes of non-cubic blocks.
#pragma once

#include <cmath>
#include <limits>

#include "core/math.hpp"
#include "world/block.hpp"

namespace strata {

struct RayHit {
  bool hit = false;
  ivec3 block;           // the block that was hit
  int face = -1;         // face of `block` the ray entered through (-1: started inside)
  BlockId id = block::Air;
  float distance = 0.0f;
};

// Intersects a ray with an axis aligned box. On success returns the entry
// distance (may be negative when the origin is inside) and the entry face.
bool rayBox(const dvec3& origin, const vec3& dir, const dvec3& mn, const dvec3& mx, double& tEnter,
            int& face);

// `get(x, y, z)` returns the BlockId at a position. Liquids and air are passed
// through; everything else is tested against its selection box.
template <class GetBlock>
RayHit raycastBlocks(const dvec3& origin, const vec3& direction, float maxDistance, GetBlock get) {
  RayHit result;
  const vec3 dir = normalize(direction);
  if (length(dir) == 0.0f) return result;
  int ix = fastFloor(origin.x), iy = fastFloor(origin.y), iz = fastFloor(origin.z);
  const int stepX = dir.x > 0 ? 1 : (dir.x < 0 ? -1 : 0);
  const int stepY = dir.y > 0 ? 1 : (dir.y < 0 ? -1 : 0);
  const int stepZ = dir.z > 0 ? 1 : (dir.z < 0 ? -1 : 0);
  constexpr double inf = std::numeric_limits<double>::infinity();
  const double dX = stepX ? std::fabs(1.0 / dir.x) : inf;
  const double dY = stepY ? std::fabs(1.0 / dir.y) : inf;
  const double dZ = stepZ ? std::fabs(1.0 / dir.z) : inf;
  double tX = stepX > 0 ? (ix + 1 - origin.x) * dX : (stepX < 0 ? (origin.x - ix) * dX : inf);
  double tY = stepY > 0 ? (iy + 1 - origin.y) * dY : (stepY < 0 ? (origin.y - iy) * dY : inf);
  double tZ = stepZ > 0 ? (iz + 1 - origin.z) * dZ : (stepZ < 0 ? (origin.z - iz) * dZ : inf);
  double t = 0.0;
  int entered = -1;
  for (int guard = 0; guard < 1024 && t <= maxDistance; ++guard) {
    const BlockId b = get(ix, iy, iz);
    if (b != block::Air) {
      const BlockInfo& info = blockInfo(b);
      if (!info.liquid) {
        const bool fullCube = info.selMin[0] == 0.0f && info.selMin[1] == 0.0f && info.selMin[2] == 0.0f &&
                              info.selMax[0] == 1.0f && info.selMax[1] == 1.0f && info.selMax[2] == 1.0f;
        if (fullCube) {
          result.hit = true;
          result.block = {ix, iy, iz};
          result.face = entered;
          result.id = b;
          result.distance = static_cast<float>(t);
          return result;
        }
        double tEnter = 0.0;
        int face = -1;
        const dvec3 mn(ix + info.selMin[0], iy + info.selMin[1], iz + info.selMin[2]);
        const dvec3 mx(ix + info.selMax[0], iy + info.selMax[1], iz + info.selMax[2]);
        if (rayBox(origin, dir, mn, mx, tEnter, face) && tEnter <= maxDistance) {
          result.hit = true;
          result.block = {ix, iy, iz};
          result.face = tEnter < 0.0 ? -1 : face;
          result.id = b;
          result.distance = static_cast<float>(std::max(0.0, tEnter));
          return result;
        }
      }
    }
    if (tX < tY && tX < tZ) {
      ix += stepX;
      t = tX;
      tX += dX;
      entered = stepX > 0 ? FaceNegX : FacePosX;
    } else if (tY < tZ) {
      iy += stepY;
      t = tY;
      tY += dY;
      entered = stepY > 0 ? FaceNegY : FacePosY;
    } else {
      iz += stepZ;
      t = tZ;
      tZ += dZ;
      entered = stepZ > 0 ? FaceNegZ : FacePosZ;
    }
  }
  return result;
}

}  // namespace strata
