#include "world/block.hpp"

#include <array>

#include "world/items.hpp"

namespace strata {

namespace {

void setAll(BlockInfo& b, uint16_t t) {
  for (uint16_t& x : b.tex) x = t;
}
void setColumn(BlockInfo& b, uint16_t side, uint16_t top, uint16_t bottom) {
  b.tex[FacePosX] = b.tex[FaceNegX] = b.tex[FacePosZ] = b.tex[FaceNegZ] = side;
  b.tex[FacePosY] = top;
  b.tex[FaceNegY] = bottom;
}
void setSel(BlockInfo& b, float x0, float y0, float z0, float x1, float y1, float z1) {
  b.selMin[0] = x0;
  b.selMin[1] = y0;
  b.selMin[2] = z0;
  b.selMax[0] = x1;
  b.selMax[1] = y1;
  b.selMax[2] = z1;
}

BlockInfo solidCube(const char* name, uint16_t t, float hardness) {
  BlockInfo b;
  b.name = name;
  setAll(b, t);
  b.hardness = hardness;
  return b;
}

BlockInfo plant(const char* name, uint16_t t) {
  BlockInfo b;
  b.name = name;
  b.shape = Shape::Cross;
  b.layer = Layer::Cutout;
  b.solid = false;
  b.opaque = false;
  b.lightOpacity = 0;
  b.hardness = 0.0f;
  setAll(b, t);
  b.wave = Wave::Plant;
  b.replaceable = false;
  b.needsSupport = true;
  setSel(b, 0.15f, 0.0f, 0.15f, 0.85f, 0.8f, 0.85f);
  return b;
}

BlockInfo leaves(const char* name, uint16_t t, Tint tint) {
  BlockInfo b;
  b.name = name;
  b.layer = Layer::Cutout;
  b.opaque = false;
  b.lightOpacity = 1;
  b.hardness = 0.3f;
  setAll(b, t);
  b.tint = tint;
  b.wave = Wave::Leaves;
  b.drop = 0;
  return b;
}

std::array<BlockInfo, 256> buildTable() {
  std::array<BlockInfo, 256> t{};
  for (int i = 0; i < 256; ++i) t[i].drop = static_cast<uint16_t>(i);

  BlockInfo& air = t[block::Air];
  air.name = "Air";
  air.shape = Shape::None;
  air.solid = false;
  air.opaque = false;
  air.lightOpacity = 0;
  air.hardness = 0.0f;
  air.replaceable = true;
  air.inCreative = false;
  air.drop = 0;

  t[block::Stone] = solidCube("Stone", tex::Stone, 1.8f);
  t[block::Stone].drop = block::Cobblestone;
  t[block::Dirt] = solidCube("Dirt", tex::Dirt, 0.6f);

  t[block::Grass] = solidCube("Grass Block", tex::GrassSide, 0.7f);
  setColumn(t[block::Grass], tex::GrassSide, tex::GrassTop, tex::Dirt);
  t[block::Grass].tint = Tint::Grass;
  t[block::Grass].drop = block::Dirt;

  t[block::Sand] = solidCube("Sand", tex::Sand, 0.6f);
  t[block::Sand].gravity = true;
  t[block::Gravel] = solidCube("Gravel", tex::Gravel, 0.7f);
  t[block::Gravel].gravity = true;

  BlockInfo& water = t[block::Water];
  water.name = "Water";
  water.shape = Shape::Liquid;
  water.layer = Layer::Translucent;
  water.solid = false;
  water.opaque = false;
  water.lightOpacity = 1;
  water.hardness = -1.0f;
  setAll(water, tex::Water);
  water.liquid = true;
  water.replaceable = true;
  water.inCreative = false;
  water.drop = 0;

  BlockInfo& lava = t[block::Lava];
  lava.name = "Lava";
  lava.shape = Shape::Liquid;
  lava.layer = Layer::Opaque;
  lava.solid = false;
  lava.opaque = false;
  lava.lightOpacity = 1;
  lava.lightEmission = 15;
  lava.hardness = -1.0f;
  setAll(lava, tex::Lava);
  lava.emissive = true;
  lava.liquid = true;
  lava.replaceable = true;
  lava.inCreative = false;
  lava.drop = 0;

  t[block::Baserock] = solidCube("Baserock", tex::Baserock, -1.0f);
  t[block::Baserock].drop = 0;
  t[block::Baserock].inCreative = false;

  t[block::OakLog] = solidCube("Oak Log", tex::OakLogSide, 1.8f);
  setColumn(t[block::OakLog], tex::OakLogSide, tex::OakLogTop, tex::OakLogTop);
  t[block::OakLeaves] = leaves("Oak Leaves", tex::OakLeaves, Tint::Foliage);
  t[block::BirchLog] = solidCube("Birch Log", tex::BirchLogSide, 1.8f);
  setColumn(t[block::BirchLog], tex::BirchLogSide, tex::BirchLogTop, tex::BirchLogTop);
  t[block::BirchLeaves] = leaves("Birch Leaves", tex::BirchLeaves, Tint::None);
  t[block::SpruceLog] = solidCube("Spruce Log", tex::SpruceLogSide, 1.8f);
  setColumn(t[block::SpruceLog], tex::SpruceLogSide, tex::SpruceLogTop, tex::SpruceLogTop);
  t[block::SpruceLeaves] = leaves("Spruce Leaves", tex::SpruceLeaves, Tint::None);

  t[block::Planks] = solidCube("Planks", tex::Planks, 1.5f);
  t[block::Cobblestone] = solidCube("Cobblestone", tex::Cobblestone, 2.0f);

  BlockInfo& glass = t[block::Glass];
  glass = solidCube("Glass", tex::Glass, 0.4f);
  glass.layer = Layer::Translucent;
  glass.opaque = false;
  glass.lightOpacity = 0;
  glass.cullsSame = true;
  glass.drop = 0;

  t[block::Snow] = solidCube("Snow", tex::Snow, 0.4f);
  t[block::SnowyGrass] = solidCube("Snowy Grass", tex::SnowyGrassSide, 0.7f);
  setColumn(t[block::SnowyGrass], tex::SnowyGrassSide, tex::Snow, tex::Dirt);
  t[block::SnowyGrass].drop = block::Dirt;

  BlockInfo& ice = t[block::Ice];
  ice = solidCube("Ice", tex::Ice, 0.6f);
  ice.layer = Layer::Translucent;
  ice.opaque = false;
  ice.lightOpacity = 2;
  ice.cullsSame = true;
  ice.drop = 0;

  t[block::Sandstone] = solidCube("Sandstone", tex::SandstoneSide, 1.2f);
  setColumn(t[block::Sandstone], tex::SandstoneSide, tex::SandstoneTop, tex::SandstoneBottom);
  t[block::Clay] = solidCube("Clay", tex::Clay, 0.7f);
  t[block::Bricks] = solidCube("Bricks", tex::Bricks, 2.2f);
  t[block::StoneBricks] = solidCube("Stone Bricks", tex::StoneBricks, 2.0f);

  t[block::CoalOre] = solidCube("Coal Ore", tex::CoalOre, 2.4f);
  t[block::CoalOre].drop = item::Coal;
  t[block::IronOre] = solidCube("Iron Ore", tex::IronOre, 2.8f);
  t[block::GoldOre] = solidCube("Gold Ore", tex::GoldOre, 3.0f);
  t[block::DiamondOre] = solidCube("Diamond Ore", tex::DiamondOre, 3.5f);

  BlockInfo& cactus = t[block::Cactus];
  cactus = solidCube("Cactus", tex::CactusSide, 0.5f);
  setColumn(cactus, tex::CactusSide, tex::CactusTop, tex::CactusBottom);
  cactus.layer = Layer::Cutout;
  cactus.opaque = false;
  cactus.lightOpacity = 0;
  cactus.needsSupport = true;
  setSel(cactus, 1.0f / 16, 0.0f, 1.0f / 16, 15.0f / 16, 1.0f, 15.0f / 16);

  t[block::TallGrass] = plant("Tall Grass", tex::TallGrass);
  t[block::TallGrass].tint = Tint::Grass;
  t[block::TallGrass].replaceable = true;
  t[block::TallGrass].drop = 0;
  t[block::Fern] = plant("Fern", tex::Fern);
  t[block::Fern].tint = Tint::Grass;
  t[block::Fern].replaceable = true;
  t[block::Fern].drop = 0;
  t[block::RedFlower] = plant("Red Flower", tex::RedFlower);
  setSel(t[block::RedFlower], 0.3f, 0.0f, 0.3f, 0.7f, 0.65f, 0.7f);
  t[block::YellowFlower] = plant("Yellow Flower", tex::YellowFlower);
  setSel(t[block::YellowFlower], 0.3f, 0.0f, 0.3f, 0.7f, 0.6f, 0.7f);
  t[block::DeadBush] = plant("Dead Bush", tex::DeadBush);
  t[block::DeadBush].wave = Wave::None;
  t[block::DeadBush].replaceable = true;
  t[block::DeadBush].drop = item::Stick;

  BlockInfo& torch = t[block::Torch];
  torch.name = "Torch";
  torch.shape = Shape::Torch;
  torch.layer = Layer::Cutout;
  torch.solid = false;
  torch.opaque = false;
  torch.lightOpacity = 0;
  torch.lightEmission = 14;
  torch.hardness = 0.0f;
  setAll(torch, tex::Torch);
  torch.needsSupport = true;
  setSel(torch, 6.0f / 16, 0.0f, 6.0f / 16, 10.0f / 16, 10.0f / 16, 10.0f / 16);

  t[block::Lamp] = solidCube("Lamp", tex::Lamp, 0.5f);
  t[block::Lamp].lightEmission = 15;
  t[block::Lamp].emissive = true;

  t[block::Basalt] = solidCube("Basalt", tex::BasaltSide, 2.5f);
  setColumn(t[block::Basalt], tex::BasaltSide, tex::BasaltTop, tex::BasaltTop);

  // Unused ids behave like air so corrupted data can never crash the engine.
  for (int i = block::Count; i < 256; ++i) {
    t[i] = air;
    t[i].name = "Unknown";
  }
  return t;
}

const std::array<BlockInfo, 256>& table() {
  static const std::array<BlockInfo, 256> t = buildTable();
  return t;
}

struct LightTables {
  uint8_t opacity[256];
  uint8_t emission[256];
};

const LightTables& lightTables() {
  static const LightTables lt = [] {
    LightTables l{};
    for (int i = 0; i < 256; ++i) {
      const BlockInfo& b = table()[i];
      l.opacity[i] = b.opaque ? 15 : b.lightOpacity;
      l.emission[i] = b.lightEmission;
    }
    return l;
  }();
  return lt;
}

}  // namespace

const BlockInfo& blockInfo(BlockId id) { return table()[id]; }

bool canSurviveOn(BlockId plantId, BlockId below) {
  switch (plantId) {
    case block::TallGrass:
    case block::Fern:
    case block::RedFlower:
    case block::YellowFlower:
      return below == block::Grass || below == block::Dirt || below == block::SnowyGrass;
    case block::DeadBush:
      return below == block::Sand || below == block::Dirt || below == block::Grass;
    case block::Cactus:
      return below == block::Sand || below == block::Cactus;
    case block::Torch: {
      // Any full solid cube except foliage-like cut-out blocks (leaves, cactus).
      const BlockInfo& b = blockInfo(below);
      return b.solid && b.shape == Shape::Cube && b.layer != Layer::Cutout;
    }
    default:
      return true;
  }
}

const uint8_t* lightOpacityTable() { return lightTables().opacity; }
const uint8_t* lightEmissionTable() { return lightTables().emission; }

}  // namespace strata
