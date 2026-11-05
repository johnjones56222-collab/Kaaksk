// Items (blocks plus a few non-placeable materials), inventories and recipes.
#pragma once

#include <cstdint>
#include <vector>

#include "world/block.hpp"

namespace strata {

using ItemId = uint16_t;

// Ids 1..255 are the block with the same id; non-block items start at 256.
namespace item {
enum : ItemId { None = 0, Stick = 256, Coal, End };
}

struct ItemInfo {
  const char* name;
  uint16_t icon;      // texture layer used for flat icons
  bool isBlock;
  BlockId block;      // valid when isBlock
  uint16_t maxStack;
};

bool isValidItem(ItemId id);
ItemInfo itemInfo(ItemId id);

struct ItemStack {
  ItemId id = item::None;
  uint16_t count = 0;
  bool empty() const { return id == item::None || count == 0; }
};

class Inventory {
 public:
  static constexpr int kSlots = 36;  // slots 0..8 form the hotbar
  static constexpr int kHotbar = 9;

  ItemStack slots[kSlots];

  // Adds items, filling existing stacks first. Returns the amount that did not fit.
  int add(ItemId id, int count);
  int countOf(ItemId id) const;
  // Removes `count` items of a type from anywhere. Fails (and changes nothing)
  // when there are not enough.
  bool remove(ItemId id, int count);
  // Removes one item from a specific slot.
  void takeOne(int slot);
  void clear();
};

struct Recipe {
  ItemStack output;
  std::vector<ItemStack> inputs;
};

const std::vector<Recipe>& recipes();
bool canCraft(const Inventory& inv, const Recipe& r);
// Crafts once. Returns false when ingredients are missing or the output does
// not fit (in which case the inventory is unchanged).
bool craft(Inventory& inv, const Recipe& r);

}  // namespace strata
