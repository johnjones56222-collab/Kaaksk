// Built-in 5x8 pixel font covering printable ASCII (32..126).
#pragma once

#include <cstdint>
#include <vector>

namespace strata {

struct FontAtlas {
  static constexpr int kCellW = 6;  // glyph cell incl. spacing
  static constexpr int kCellH = 9;
  static constexpr int kCols = 16;
  static constexpr int kRows = 6;
  int width = kCellW * kCols;
  int height = kCellH * kRows;
  std::vector<uint8_t> pixels;  // R8 coverage, row 0 at the top
  int left[95] = {};            // first lit column of each glyph
  int advance[95] = {};         // horizontal advance in pixels
};

FontAtlas buildFontAtlas();

}  // namespace strata
