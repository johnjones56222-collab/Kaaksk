// Immediate mode 2D drawing: rectangles, text and item icons, batched into a
// single draw call per flush. Coordinates are in window pixels (top left 0,0).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gl/gl.hpp"
#include "render/font.hpp"
#include "world/items.hpp"

namespace strata {

class Shader;

inline uint32_t rgba(int r, int g, int b, int a = 255) {
  return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) |
         static_cast<uint32_t>(b);
}

class UiRenderer {
 public:
  UiRenderer();
  ~UiRenderer();
  bool init();
  void shutdown();

  void begin(int width, int height);
  // Draws everything queued since begin() on top of the current framebuffer.
  void flush(GLuint blockTextures);

  void rect(float x, float y, float w, float h, uint32_t color);
  void frame(float x, float y, float w, float h, float thickness, uint32_t color);
  // Returns the width of the drawn text.
  float text(float x, float y, const std::string& s, uint32_t color, float scale, bool shadow = true);
  float textWidth(const std::string& s, float scale) const;
  float lineHeight(float scale) const { return 10.0f * scale; }
  void icon(ItemId item, float x, float y, float size);
  void crosshair(float cx, float cy, float size, float thickness);
  // Shapes for the touch screen controls.
  void triangle(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color);
  void disc(float cx, float cy, float radius, uint32_t color);
  void ring(float cx, float cy, float radius, float thickness, uint32_t color);

  int width() const { return width_; }
  int height() const { return height_; }

 private:
  struct Vertex {
    float x, y, u, v, mode;
    uint8_t r, g, b, a;
  };
  void quad(const float* xy, const float* uv, float mode, uint32_t color);

  std::vector<Vertex> vertices_;
  FontAtlas font_;
  Shader* shader_ = nullptr;
  GLuint vao_ = 0, vbo_ = 0, fontTex_ = 0;
  int width_ = 1, height_ = 1;
};

}  // namespace strata
