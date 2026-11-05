#include "game/player.hpp"

#include <algorithm>
#include <cmath>

#include "world/block.hpp"
#include "world/mesher.hpp"
#include "world/world.hpp"

namespace strata {

namespace {
double comp(const dvec3& v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }
double& comp(dvec3& v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }

// Collision box of a solid block, or false for blocks without one.
bool collider(const World& w, int x, int y, int z, Aabb& out) {
  if (!w.isSolidAt(x, y, z)) return false;
  const BlockId b = w.getBlock(x, y, z);
  if (b == block::Cactus) {
    const BlockInfo& info = blockInfo(b);
    out.min = dvec3(x + info.selMin[0], y + info.selMin[1], z + info.selMin[2]);
    out.max = dvec3(x + info.selMax[0], y + info.selMax[1], z + info.selMax[2]);
  } else {
    out.min = dvec3(x, y, z);
    out.max = dvec3(x + 1, y + 1, z + 1);
  }
  return true;
}

bool overlapsOther(const Aabb& a, const Aabb& b, int axis) {
  for (int k = 0; k < 3; ++k) {
    if (k == axis) continue;
    if (!(comp(a.min, k) < comp(b.max, k) && comp(a.max, k) > comp(b.min, k))) return false;
  }
  return true;
}
}  // namespace

Aabb Player::boxAt(const dvec3& p) const {
  const double hw = kWidth * 0.5;
  return Aabb{dvec3(p.x - hw, p.y, p.z - hw), dvec3(p.x + hw, p.y + height(), p.z + hw)};
}

dvec3 Player::eyePosition(double alpha) const {
  const dvec3 p = prevPos + (pos - prevPos) * alpha;
  return dvec3(p.x, p.y + eyeHeight(), p.z);
}

vec3 Player::lookDirection() const {
  return vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
}

bool Player::collides(World& w, const Aabb& b) const {
  const int x0 = fastFloor(b.min.x), x1 = fastFloor(b.max.x - 1e-9);
  const int y0 = fastFloor(b.min.y), y1 = fastFloor(b.max.y - 1e-9);
  const int z0 = fastFloor(b.min.z), z1 = fastFloor(b.max.z - 1e-9);
  for (int y = y0; y <= y1; ++y)
    for (int z = z0; z <= z1; ++z)
      for (int x = x0; x <= x1; ++x) {
        Aabb c;
        if (collider(w, x, y, z, c) && c.intersects(b)) return true;
      }
  return false;
}

double Player::moveAxis(World& w, Aabb& b, int axis, double delta) const {
  if (delta == 0.0) return 0.0;
  Aabb swept = b;
  if (delta > 0)
    comp(swept.max, axis) += delta;
  else
    comp(swept.min, axis) += delta;
  const int x0 = fastFloor(swept.min.x), x1 = fastFloor(swept.max.x - 1e-9);
  const int y0 = fastFloor(swept.min.y), y1 = fastFloor(swept.max.y - 1e-9);
  const int z0 = fastFloor(swept.min.z), z1 = fastFloor(swept.max.z - 1e-9);
  constexpr double kEps = 1e-7;
  for (int y = y0; y <= y1; ++y)
    for (int z = z0; z <= z1; ++z)
      for (int x = x0; x <= x1; ++x) {
        Aabb c;
        if (!collider(w, x, y, z, c) || !overlapsOther(b, c, axis)) continue;
        if (delta > 0 && comp(b.max, axis) <= comp(c.min, axis) + kEps)
          delta = std::min(delta, comp(c.min, axis) - comp(b.max, axis));
        else if (delta < 0 && comp(b.min, axis) >= comp(c.max, axis) - kEps)
          delta = std::max(delta, comp(c.max, axis) - comp(b.min, axis));
      }
  comp(b.min, axis) += delta;
  comp(b.max, axis) += delta;
  return delta;
}

bool Player::supported(World& w, const Aabb& b) const {
  Aabb below = b;
  below.max.y = b.min.y;
  below.min.y = b.min.y - 0.55;
  return collides(w, below);
}

void Player::updateFluids(World& w) {
  const Aabb b = box();
  inWater = inLava = false;
  const int x0 = fastFloor(b.min.x + 0.001), x1 = fastFloor(b.max.x - 0.001);
  const int y0 = fastFloor(b.min.y), y1 = fastFloor(b.max.y - 0.001);
  const int z0 = fastFloor(b.min.z + 0.001), z1 = fastFloor(b.max.z - 0.001);
  for (int y = y0; y <= y1; ++y)
    for (int z = z0; z <= z1; ++z)
      for (int x = x0; x <= x1; ++x) {
        const BlockId id = w.getBlock(x, y, z);
        if (!isLiquid(id)) continue;
        const double top = y + (w.getBlock(x, y + 1, z) == id ? 1.0 : liquidHeight(w.getMeta(x, y, z)));
        if (top <= b.min.y) continue;
        if (id == block::Water)
          inWater = true;
        else
          inLava = true;
      }
  const double eye = pos.y + eyeHeight();
  const int ex = fastFloor(pos.x), ey = fastFloor(eye), ez = fastFloor(pos.z);
  const BlockId e = w.getBlock(ex, ey, ez);
  headInWater = false;
  if (e == block::Water) {
    const double top = ey + (w.getBlock(ex, ey + 1, ez) == e ? 1.0 : liquidHeight(w.getMeta(ex, ey, ez)));
    headInWater = eye < top;
  }
}

void Player::damage(float amount) {
  if (creative || dead || amount <= 0.0f) return;
  health -= amount;
  hurtTimer = 0.0f;
  damageFlash = 1.0f;
  if (health <= 0.0f) {
    health = 0.0f;
    dead = true;
  }
}

void Player::teleport(const dvec3& p) {
  pos = prevPos = p;
  vel = dvec3();
  fallDistance = 0.0f;
}

void Player::unstuck(World& w) {
  for (int i = 0; i < 300 && collides(w, box()); ++i) pos.y = std::floor(pos.y) + 1.0;
  prevPos = pos;
}

void Player::tick(World& w, const PlayerControls& c, double dt) {
  prevPos = pos;
  if (dead) return;
  const float fdt = static_cast<float>(dt);
  hurtTimer += fdt;
  damageFlash = std::max(0.0f, damageFlash - fdt * 2.5f);
  updateFluids(w);

  sneaking = c.sneak && !flying;
  if (!c.sprint && c.forward <= 0.0f) sprinting = false;
  if (c.sprint && c.forward > 0.0f && !sneaking) sprinting = true;
  if (sneaking) sprinting = false;

  // Horizontal acceleration towards the requested velocity.
  const dvec3 fwd(std::sin(yaw), 0.0, -std::cos(yaw));
  const dvec3 right(std::cos(yaw), 0.0, std::sin(yaw));
  double f = c.forward, s = c.strafe;
  const double len = std::sqrt(f * f + s * s);
  if (len > 1.0) {
    f /= len;
    s /= len;
  }
  double speed = 4.3;
  if (flying) speed = sprinting ? 21.0 : 10.9;
  else if (inLava) speed = 1.3;
  else if (inWater) speed = sprinting ? 3.4 : 2.3;
  else if (sneaking) speed = 1.3;
  else if (sprinting) speed = 5.6;
  const dvec3 wish = (fwd * f + right * s) * speed;
  double accel = 4.0;
  if (flying) accel = 8.0;
  else if (inWater || inLava) accel = 6.0;
  else if (onGround) accel = 22.0;
  const double k = std::min(1.0, accel * dt);
  vel.x += (wish.x - vel.x) * k;
  vel.z += (wish.z - vel.z) * k;

  // Vertical motion.
  if (flying) {
    const double target = (c.jump ? 7.5 : 0.0) - (c.sneak ? 7.5 : 0.0);
    vel.y += (target - vel.y) * std::min(1.0, 10.0 * dt);
  } else if (inWater || inLava) {
    vel.y -= (inLava ? 5.0 : 6.0) * dt;
    if (c.jump) vel.y += (inLava ? 14.0 : 19.0) * dt;
    vel.y *= 1.0 - std::min(1.0, 2.5 * dt);
    vel.y = clampv(vel.y, -3.5, 3.0);
  } else {
    if (c.jump && onGround) vel.y = 8.6;
    vel.y = std::max(vel.y - 28.0 * dt, -60.0);
  }

  Aabb b = box();
  const double dy = vel.y * dt;
  double dx = vel.x * dt, dz = vel.z * dt;
  const double my = moveAxis(w, b, 1, dy);
  const bool hitGround = dy < 0.0 && my > dy + 1e-9;
  const bool hitCeiling = dy > 0.0 && my < dy - 1e-9;
  // Sneaking keeps the player from walking off edges.
  if (sneaking && (onGround || hitGround)) {
    auto shifted = [&](double ox, double oz) {
      Aabb t = b;
      t.min.x += ox;
      t.max.x += ox;
      t.min.z += oz;
      t.max.z += oz;
      return t;
    };
    while (dx != 0.0 && !supported(w, shifted(dx, 0.0))) dx = std::fabs(dx) < 0.05 ? 0.0 : dx - std::copysign(0.05, dx);
    while (dz != 0.0 && !supported(w, shifted(dx, dz))) dz = std::fabs(dz) < 0.05 ? 0.0 : dz - std::copysign(0.05, dz);
  }
  const double mx = moveAxis(w, b, 0, dx);
  if (mx != dx) vel.x = 0.0;
  const double mz = moveAxis(w, b, 2, dz);
  if (mz != dz) vel.z = 0.0;
  pos = dvec3((b.min.x + b.max.x) * 0.5, b.min.y, (b.min.z + b.max.z) * 0.5);

  if (hitCeiling) vel.y = 0.0;
  onGround = hitGround;
  if (hitGround) {
    vel.y = 0.0;
    if (flying) flying = false;
  }
  if (flying || inWater || inLava) {
    fallDistance = 0.0f;
  } else if (!onGround && my < 0.0) {
    fallDistance += static_cast<float>(-my);
  }
  if (onGround) {
    if (fallDistance > 3.5f) damage(std::ceil(fallDistance - 3.0f));
    fallDistance = 0.0f;
    walkDistance += static_cast<float>(std::sqrt(mx * mx + mz * mz));
  }

  // Hazards and recovery.
  if (headInWater && !creative) {
    air -= fdt;
    if (air <= 0.0f) {
      air = 0.0f;
      drownTimer_ += fdt;
      if (drownTimer_ >= 1.0f) {
        damage(2.0f);
        drownTimer_ = 0.0f;
      }
    }
  } else {
    air = std::min(kMaxAir, air + fdt * 5.0f);
    drownTimer_ = 0.0f;
  }
  if (inLava && !creative) {
    lavaTimer_ += fdt;
    if (lavaTimer_ >= 0.5f) {
      damage(4.0f);
      lavaTimer_ = 0.0f;
    }
  } else {
    lavaTimer_ = 0.5f;  // damage immediately on the next contact
  }
  Aabb touch = box();
  touch.min = touch.min - dvec3(0.02, 0.02, 0.02);
  touch.max = touch.max + dvec3(0.02, 0.02, 0.02);
  bool cactus = false;
  for (int y = fastFloor(touch.min.y); y <= fastFloor(touch.max.y) && !cactus; ++y)
    for (int z = fastFloor(touch.min.z); z <= fastFloor(touch.max.z) && !cactus; ++z)
      for (int x = fastFloor(touch.min.x); x <= fastFloor(touch.max.x) && !cactus; ++x) {
        if (w.getBlock(x, y, z) != block::Cactus) continue;
        Aabb cb;
        if (collider(w, x, y, z, cb) && cb.intersects(touch)) cactus = true;
      }
  if (cactus && !creative) {
    cactusTimer_ += fdt;
    if (cactusTimer_ >= 0.5f) {
      damage(1.0f);
      cactusTimer_ = 0.0f;
    }
  } else {
    cactusTimer_ = 0.5f;
  }
  if (pos.y < -40.0) damage(4.0f);
  if (health < kMaxHealth && hurtTimer > 5.0f) {
    regenTimer_ += fdt;
    if (regenTimer_ >= 2.5f) {
      health = std::min(kMaxHealth, health + 1.0f);
      regenTimer_ = 0.0f;
    }
  } else {
    regenTimer_ = 0.0f;
  }
}

}  // namespace strata
