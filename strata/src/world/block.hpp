// Block types and their static properties.
#pragma once

#include <cstdint>

namespace strata {

using BlockId = uint8_t;

namespace block {
enum : BlockId {
  Air = 0,
  Stone,
  Dirt,
  Grass,
  Sand,
  Gravel,
  Water,
  Lava,
  Baserock,
  OakLog,
  OakLeaves,
  BirchLog,
  BirchLeaves,
  SpruceLog,
  SpruceLeaves,
  Planks,
  Cobblestone,
  Glass,
  Snow,
  SnowyGrass,
  Ice,
  Sandstone,
  Clay,
  Bricks,
  StoneBricks,
  CoalOre,
  IronOre,
  GoldOre,
  DiamondOre,
  Cactus,
  TallGrass,
  Fern,
  RedFlower,
  YellowFlower,
  DeadBush,
  Torch,
  Lamp,
  Basalt,
  Count
};
}  // namespace block

// Texture layers shared by the texture generator, the mesher and the UI.
namespace tex {
enum : uint16_t {
  Stone = 0,
  Dirt,
  GrassTop,
  GrassSide,
  Sand,
  Gravel,
  Water,
  Lava,
  Baserock,
  OakLogSide,
  OakLogTop,
  OakLeaves,
  BirchLogSide,
  BirchLogTop,
  BirchLeaves,
  SpruceLogSide,
  SpruceLogTop,
  SpruceLeaves,
  Planks,
  Cobblestone,
  Glass,
  Snow,
  SnowyGrassSide,
  Ice,
  SandstoneSide,
  SandstoneTop,
  SandstoneBottom,
  Clay,
  Bricks,
  StoneBricks,
  CoalOre,
  IronOre,
  GoldOre,
  DiamondOre,
  CactusSide,
  CactusTop,
  CactusBottom,
  TallGrass,
  Fern,
  RedFlower,
  YellowFlower,
  DeadBush,
  Torch,
  Lamp,
  BasaltSide,
  BasaltTop,
  Crack0,  // ten crack overlay stages follow
  Crack9 = Crack0 + 9,
  ItemStick,
  ItemCoal,
  Count
};
}  // namespace tex

// Face order used everywhere: +X, -X, +Y, -Y, +Z, -Z.
enum Face : uint8_t { FacePosX = 0, FaceNegX, FacePosY, FaceNegY, FacePosZ, FaceNegZ };
constexpr int kFaceDir[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
constexpr Face oppositeFace(Face f) { return static_cast<Face>(f ^ 1); }

enum class Shape : uint8_t { None, Cube, Cross, Liquid, Torch };
enum class Layer : uint8_t { Opaque, Cutout, Translucent };
enum class Tint : uint8_t { None, Grass, Foliage };
enum class Wave : uint8_t { None, Leaves, Plant };

struct BlockInfo {
  const char* name = "?";
  Shape shape = Shape::Cube;
  Layer layer = Layer::Opaque;
  bool solid = true;        // collides with entities
  bool opaque = true;       // full opaque cube: hides neighbour faces, blocks all light
  uint8_t lightOpacity = 15;
  uint8_t lightEmission = 0;
  float hardness = 1.0f;    // seconds to break by hand; negative = unbreakable in survival
  uint16_t tex[6] = {0, 0, 0, 0, 0, 0};
  Tint tint = Tint::None;
  Wave wave = Wave::None;
  bool emissive = false;    // rendered at full brightness
  bool replaceable = false; // placing a block into this cell overwrites it
  bool liquid = false;
  bool gravity = false;     // falls when unsupported
  bool needsSupport = false;
  bool cullsSame = false;   // faces between two blocks of this type are hidden (glass, ice)
  bool inCreative = true;   // listed in the creative inventory
  uint16_t drop = 0;        // item id dropped in survival (0 = nothing)
  uint8_t dropCount = 1;
  float selMin[3] = {0, 0, 0};  // selection / outline box in block units
  float selMax[3] = {1, 1, 1};
};

const BlockInfo& blockInfo(BlockId id);
inline bool isOpaque(BlockId id) { return blockInfo(id).opaque; }
inline bool isSolid(BlockId id) { return blockInfo(id).solid; }
inline bool isLiquid(BlockId id) { return blockInfo(id).liquid; }
// Can the block `plant` (which needs support) stay on top of `below`?
bool canSurviveOn(BlockId plant, BlockId below);

// Lookup tables for the light engine (index = BlockId, 256 entries).
const uint8_t* lightOpacityTable();
const uint8_t* lightEmissionTable();

// Liquid metadata: bits 0-2 = level (0 = source, 1..7 = flowing), bit 3 = falling.
constexpr uint8_t kLiquidLevelMask = 0x07;
constexpr uint8_t kLiquidFalling = 0x08;

}  // namespace strata
