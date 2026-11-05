#include "world/raycast.hpp"

#include <algorithm>

namespace strata {

bool rayBox(const dvec3& origin, const vec3& dir, const dvec3& mn, const dvec3& mx, double& tEnter, int& face) {
  const double o[3] = {origin.x, origin.y, origin.z};
  const double d[3] = {dir.x, dir.y, dir.z};
  const double lo[3] = {mn.x, mn.y, mn.z};
  const double hi[3] = {mx.x, mx.y, mx.z};
  double tmin = -std::numeric_limits<double>::infinity();
  double tmax = std::numeric_limits<double>::infinity();
  int enterFace = -1;
  for (int a = 0; a < 3; ++a) {
    if (d[a] == 0.0) {
      if (o[a] < lo[a] || o[a] > hi[a]) return false;
      continue;
    }
    double t1 = (lo[a] - o[a]) / d[a];
    double t2 = (hi[a] - o[a]) / d[a];
    // Entering through the low side means hitting the negative face.
    int f1 = a * 2 + 1;  // FaceNegX / FaceNegY / FaceNegZ
    int f2 = a * 2;      // FacePosX / ...
    if (t1 > t2) {
      std::swap(t1, t2);
      std::swap(f1, f2);
    }
    if (t1 > tmin) {
      tmin = t1;
      enterFace = f1;
    }
    tmax = std::min(tmax, t2);
    if (tmin > tmax) return false;
  }
  if (tmax < 0.0) return false;
  tEnter = tmin;
  face = enterFace;
  return true;
}

}  // namespace strata
