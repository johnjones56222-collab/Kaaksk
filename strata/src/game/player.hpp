// Player movement, collision and survival state.
#pragma once

#include "core/math.hpp"

namespace strata {

class World;

struct Aabb {
  dvec3 min, max;
  bool intersects(const Aabb& o) const {
    return min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y && min.z < o.max.z &&
           max.z > o.min.z;
  }
};

struct PlayerControls {
  float forward = 0.0f;  // -1..1
  float strafe = 0.0f;   // -1..1 (positive = right)
  bool jump = false;
  bool sneak = false;
  bool sprint = false;
};

class Player {
 public:
  static constexpr double kWidth = 0.6;
  static constexpr double kHeight = 1.8;
  static constexpr double kSneakHeight = 1.5;
  static constexpr float kMaxHealth = 20.0f;
  static constexpr float kMaxAir = 15.0f;  // seconds of breath

  dvec3 pos;      // centre of the feet
  dvec3 prevPos;  // position at the previous physics step (for interpolation)
  dvec3 vel;
  float yaw = 0.0f, pitch = 0.0f;
  bool onGround = false;
  bool flying = false;
  bool sneaking = false;
  bool sprinting = false;
  bool inWater = false, headInWater = false, inLava = false;
  bool creative = false;
  float health = kMaxHealth;
  float air = kMaxAir;
  float fallDistance = 0.0f;
  float hurtTimer = 0.0f;       // seconds since last damage
  float damageFlash = 0.0f;     // visual feedback, decays
  float walkDistance = 0.0f;    // for view bobbing
  bool dead = false;

  double height() const { return sneaking && !flying ? kSneakHeight : kHeight; }
  double eyeHeight() const { return sneaking && !flying ? 1.27 : 1.62; }
  Aabb box() const { return boxAt(pos); }
  Aabb boxAt(const dvec3& p) const;
  dvec3 eyePosition(double alpha) const;
  vec3 lookDirection() const;

  // One fixed physics step.
  void tick(World& world, const PlayerControls& c, double dt);
  void damage(float amount);
  void teleport(const dvec3& p);
  // Moves the player up until it no longer overlaps solid blocks.
  void unstuck(World& world);
  bool collides(World& world, const Aabb& b) const;

 private:
  double moveAxis(World& world, Aabb& b, int axis, double delta) const;
  bool supported(World& world, const Aabb& b) const;
  void updateFluids(World& world);
  float lavaTimer_ = 0.0f, drownTimer_ = 0.0f, cactusTimer_ = 0.0f, regenTimer_ = 0.0f;
};

}  // namespace strata
