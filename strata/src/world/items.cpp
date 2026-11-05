#include "world/items.hpp"

#include <algorithm>

namespace strata {

bool isValidItem(ItemId id) {
  if (id == item::None) return false;
  if (id < 256) return id < block::Count && id != block::Air;
  return id < item::End;
}

ItemInfo itemInfo(ItemId id) {
  if (id > 0 && id < 256) {
    const BlockInfo& b = blockInfo(static_cast<BlockId>(id));
    return ItemInfo{b.name, b.tex[FacePosZ], true, static_cast<BlockId>(id), 64};
  }
  switch (id) {
    case item::Stick: return ItemInfo{"Stick", tex::ItemStick, false, block::Air, 64};
    case item::Coal: return ItemInfo{"Coal", tex::ItemCoal, false, block::Air, 64};
    default: return ItemInfo{"Nothing", 0, false, block::Air, 0};
  }
}

int Inventory::add(ItemId id, int count) {
  if (!isValidItem(id) || count <= 0) return count;
  const int maxStack = itemInfo(id).maxStack;
  for (ItemStack& s : slots) {
    if (count == 0) break;
    if (s.id == id && s.count > 0 && s.count < maxStack) {
      int moved = std::min(count, maxStack - s.count);
      s.count = static_cast<uint16_t>(s.count + moved);
      count -= moved;
    }
  }
  for (ItemStack& s : slots) {
    if (count == 0) break;
    if (s.empty()) {
      int moved = std::min(count, maxStack);
      s.id = id;
      s.count = static_cast<uint16_t>(moved);
      count -= moved;
    }
  }
  return count;
}

int Inventory::countOf(ItemId id) const {
  int n = 0;
  for (const ItemStack& s : slots)
    if (s.id == id) n += s.count;
  return n;
}

bool Inventory::remove(ItemId id, int count) {
  if (count <= 0) return true;
  if (countOf(id) < count) return false;
  // Take from the back so hotbar stacks are used last.
  for (int i = kSlots - 1; i >= 0 && count > 0; --i) {
    ItemStack& s = slots[i];
    if (s.id != id) continue;
    int taken = std::min<int>(count, s.count);
    s.count = static_cast<uint16_t>(s.count - taken);
    count -= taken;
    if (s.count == 0) s.id = item::None;
  }
  return true;
}

void Inventory::takeOne(int slot) {
  if (slot < 0 || slot >= kSlots) return;
  ItemStack& s = slots[slot];
  if (s.empty()) return;
  s.count = static_cast<uint16_t>(s.count - 1);
  if (s.count == 0) s.id = item::None;
}

void Inventory::clear() {
  for (ItemStack& s : slots) s = ItemStack{};
}

const std::vector<Recipe>& recipes() {
  static const std::vector<Recipe> list = {
      {{block::Planks, 4}, {{block::OakLog, 1}}},
      {{block::Planks, 4}, {{block::BirchLog, 1}}},
      {{block::Planks, 4}, {{block::SpruceLog, 1}}},
      {{item::Stick, 4}, {{block::Planks, 2}}},
      {{block::Torch, 4}, {{item::Stick, 1}, {item::Coal, 1}}},
      {{block::Stone, 4}, {{block::Cobblestone, 4}, {item::Coal, 1}}},
      {{block::Glass, 4}, {{block::Sand, 4}, {item::Coal, 1}}},
      {{block::Bricks, 4}, {{block::Clay, 4}, {item::Coal, 1}}},
      {{block::Sandstone, 1}, {{block::Sand, 4}}},
      {{block::StoneBricks, 4}, {{block::Stone, 4}}},
      {{block::Lamp, 1}, {{block::Glass, 1}, {block::Torch, 1}}},
  };
  return list;
}

bool canCraft(const Inventory& inv, const Recipe& r) {
  for (const ItemStack& in : r.inputs)
    if (inv.countOf(in.id) < in.count) return false;
  return true;
}

bool craft(Inventory& inv, const Recipe& r) {
  if (!canCraft(inv, r)) return false;
  Inventory trial = inv;
  for (const ItemStack& in : r.inputs) trial.remove(in.id, in.count);
  if (trial.add(r.output.id, r.output.count) != 0) return false;
  inv = trial;
  return true;
}

}  // namespace strata
