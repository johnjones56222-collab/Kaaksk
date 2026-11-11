// THE 1982 CRANE - level construction (internal header shared by the level_*.cpp files)
#pragma once
#include "builder.h"

static const u16 CF_SOLID = COL_SOLID | COL_SIGHT | COL_NORAIN; // walls, big objects
static const u16 CF_PROP = COL_SOLID | COL_SIGHT;                // furniture
static const u16 CF_FENCE = COL_SOLID;                           // see-through
static const u16 CF_LOW = COL_SOLID | COL_LOW;

// A rectangular hole in a wall. Windows get glass and a see-through collider.
struct Opening {
    float a0, a1; // along the wall
    float y0, y1;
    bool window;
};
struct WallStyle {
    int matOut; u32 tintOut; float uvsOut;
    int matIn; u32 tintIn; float uvsIn;
    float thick;
    float gradOut;
};

// Wall along X (axis = 0) at z = fixed, or along Z (axis = 1) at x = fixed.
// `inSign` is the direction (+1 / -1 on the other axis) that faces the interior.
void wallRun(Builder& b, int axis, float fixed, float a0, float a1, float y0, float y1, int inSign, const WallStyle& st,
             const Opening* ops = nullptr, int nops = 0);

void lampPost(Builder& b, float x, float z, float armYaw, int flicker = FL_NONE, bool dead = false);
// Hinged door leaf as a dynamic object + collider + interact. Returns nothing; fills w.doors[id].
void makeDoor(Builder& b, int id, const Vec3& hinge, float width, float height, float yawClosed, float openAngle, int mat, u32 tint,
              int lockedBy = -1, bool metal = true);
// Pickup: small dynamic mesh that disappears when taken.
void beginItem(Builder& b);
void endItem(Builder& b, int itemId, const Vec3& pos, float yaw = 0.0f, float grabSize = 0.3f);
void itemBattery(Builder& b, int itemId, const Vec3& pos, float yaw = 0.0f);
void itemTape(Builder& b, int itemId, const Vec3& pos, float yaw = 0.0f);
void itemFuse(Builder& b, int itemId, const Vec3& pos, float yaw = 0.0f);
// A sheet of paper / clipboard that opens a note.
void notePaper(Builder& b, int noteId, const Vec3& pos, const Vec3& normal, float w = 0.22f, float h = 0.3f, bool clipboard = false);
// Tall cabinet the player can hide in. `yaw` is the direction the door faces.
void hideLocker(Builder& b, int hideIdx, const Vec3& base, float yaw, u32 tint);
// Small generic props
void barrel(Builder& b, float x, float z, u32 tint, float y = 0.0f, bool collide = true);
void pallet(Builder& b, float x, float y, float z, float yaw = 0.0f);
void crate(Builder& b, const Vec3& mn, const Vec3& mx, bool collide = true);
void cone(Builder& b, float x, float z);
void fluorescent(Builder& b, const Vec3& pos, bool alongX, int group, int flicker, float intensity = 1.0f);
void cagedLamp(Builder& b, const Vec3& pos, const Vec3& normal, int group, int flicker, u32 color, float intensity, float radius);
void humanFigure(Builder& b, bool hardhat);
void forklift(Builder& b, float x, float z, float yaw, bool broken = false);

// Sections
void build_ground(Builder& b);
void build_entrance(Builder& b);
void build_office(Builder& b);
void build_crewshop(Builder& b);
void build_utility(Builder& b);   // generator shed, diesel tank, transformer cage, pump house
void build_containers(Builder& b);
void build_quay(Builder& b);      // quay furniture, lifts, barge
void build_crane(Builder& b);
void build_rigger(Builder& b);
void build_backdrop(Builder& b);
