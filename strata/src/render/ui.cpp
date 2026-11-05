#include "render/ui.hpp"

#include <algorithm>
#include <cmath>

#include "gl/shader.hpp"
#include "render/shaders.hpp"

namespace strata {

namespace {
constexpr float kModeColor = -2.0f;
constexpr float kModeFont = -1.0f;
// Texture modes: layer + 1000 * kind (see the UI fragment shader).
constexpr float kKindAlpha = 0.0f;       // alpha from texture, colour multiplies
constexpr float kKindOpaque = 1000.0f;   // alpha forced to 1, colour.a = shading
constexpr float kKindTintMask = 2000.0f; // tint where texture alpha is 1, colour.a = shading
constexpr float kKindCutout = 3000.0f;   // tinted everywhere, alpha from texture
}  // namespace

UiRenderer::UiRenderer() = default;
UiRenderer::~UiRenderer() { shutdown(); }

bool UiRenderer::init() {
  font_ = buildFontAtlas();
  shader_ = new Shader();
  if (!shader_->build(shaders::program("ui"))) return false;

  glGenTextures(1, &fontTex_);
  glBindTexture(GL_TEXTURE_2D, fontTex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, font_.width, font_.height, 0, GL_RED, GL_UNSIGNED_BYTE, font_.pixels.data());
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  const GLsizei stride = sizeof(Vertex);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, x)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, u)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, mode)));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<const void*>(offsetof(Vertex, r)));
  glBindVertexArray(0);
  return true;
}

void UiRenderer::shutdown() {
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (fontTex_) glDeleteTextures(1, &fontTex_);
  vbo_ = vao_ = fontTex_ = 0;
  delete shader_;
  shader_ = nullptr;
}

void UiRenderer::begin(int width, int height) {
  width_ = width > 0 ? width : 1;
  height_ = height > 0 ? height : 1;
  vertices_.clear();
}

void UiRenderer::flush(GLuint blockTextures) {
  if (vertices_.empty()) return;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width_, height_);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  shader_->use();
  shader_->set("uScreen", vec2(static_cast<float>(width_), static_cast<float>(height_)));
  shader_->set("uFont", 0);
  shader_->set("uBlocks", 1);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fontTex_);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D_ARRAY, blockTextures);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(),
               GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
  glBindVertexArray(0);
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_BLEND);
  vertices_.clear();
}

void UiRenderer::quad(const float* xy, const float* uv, float mode, uint32_t color) {
  const uint8_t a = static_cast<uint8_t>(color >> 24), r = static_cast<uint8_t>(color >> 16),
                g = static_cast<uint8_t>(color >> 8), b = static_cast<uint8_t>(color);
  const int order[6] = {0, 1, 2, 0, 2, 3};
  for (int i : order)
    vertices_.push_back({xy[i * 2], xy[i * 2 + 1], uv[i * 2], uv[i * 2 + 1], mode, r, g, b, a});
}

void UiRenderer::rect(float x, float y, float w, float h, uint32_t color) {
  const float xy[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
  const float uv[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  quad(xy, uv, kModeColor, color);
}

void UiRenderer::frame(float x, float y, float w, float h, float t, uint32_t color) {
  rect(x, y, w, t, color);
  rect(x, y + h - t, w, t, color);
  rect(x, y + t, t, h - 2 * t, color);
  rect(x + w - t, y + t, t, h - 2 * t, color);
}

float UiRenderer::textWidth(const std::string& s, float scale) const {
  float w = 0.0f;
  for (char ch : s) {
    const int c = static_cast<unsigned char>(ch);
    const int g = (c >= 32 && c <= 126) ? c - 32 : '?' - 32;
    w += static_cast<float>(font_.advance[g]) * scale;
  }
  return w;
}

float UiRenderer::text(float x, float y, const std::string& s, uint32_t color, float scale, bool shadow) {
  if (shadow) {
    const uint32_t a = (color >> 24) & 0xFF;
    text(x + scale, y + scale, s, rgba(20, 20, 24, static_cast<int>(a * 0.8f)), scale, false);
  }
  float cx = x;
  const float invW = 1.0f / static_cast<float>(font_.width);
  const float invH = 1.0f / static_cast<float>(font_.height);
  for (char ch : s) {
    const int c = static_cast<unsigned char>(ch);
    const int g = (c >= 32 && c <= 126) ? c - 32 : '?' - 32;
    const float gx = static_cast<float>((g % FontAtlas::kCols) * FontAtlas::kCellW + font_.left[g]);
    const float gy = static_cast<float>((g / FontAtlas::kCols) * FontAtlas::kCellH);
    const float gw = static_cast<float>(font_.advance[g] - 1);
    if (g != 0) {
      const float xy[8] = {cx, y, cx + gw * scale, y, cx + gw * scale, y + 8.0f * scale, cx, y + 8.0f * scale};
      const float uv[8] = {gx * invW, gy * invH, (gx + gw) * invW, gy * invH, (gx + gw) * invW, (gy + 8.0f) * invH,
                           gx * invW, (gy + 8.0f) * invH};
      quad(xy, uv, kModeFont, color);
    }
    cx += static_cast<float>(font_.advance[g]) * scale;
  }
  return cx - x;
}

void UiRenderer::crosshair(float cx, float cy, float size, float t) {
  const uint32_t c = rgba(255, 255, 255, 220);
  const uint32_t o = rgba(0, 0, 0, 140);
  rect(cx - size - 1, cy - t / 2 - 1, size * 2 + 2, t + 2, o);
  rect(cx - t / 2 - 1, cy - size - 1, t + 2, size * 2 + 2, o);
  rect(cx - size, cy - t / 2, size * 2, t, c);
  rect(cx - t / 2, cy - size, t, size * 2, c);
}

void UiRenderer::triangle(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color) {
  const uint8_t a = static_cast<uint8_t>(color >> 24), r = static_cast<uint8_t>(color >> 16),
                g = static_cast<uint8_t>(color >> 8), b = static_cast<uint8_t>(color);
  vertices_.push_back({x0, y0, 0.0f, 0.0f, kModeColor, r, g, b, a});
  vertices_.push_back({x1, y1, 0.0f, 0.0f, kModeColor, r, g, b, a});
  vertices_.push_back({x2, y2, 0.0f, 0.0f, kModeColor, r, g, b, a});
}

namespace {
// Enough segments that the outline looks round at any size used on screen.
int circleSegments(float radius) { return clampv(static_cast<int>(radius * 0.5f), 16, 64); }
}  // namespace

void UiRenderer::disc(float cx, float cy, float radius, uint32_t color) {
  const int n = circleSegments(radius);
  for (int i = 0; i < n; ++i) {
    const float a0 = kTwoPi * static_cast<float>(i) / static_cast<float>(n);
    const float a1 = kTwoPi * static_cast<float>(i + 1) / static_cast<float>(n);
    triangle(cx, cy, cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, cx + std::cos(a1) * radius,
             cy + std::sin(a1) * radius, color);
  }
}

void UiRenderer::ring(float cx, float cy, float radius, float thickness, uint32_t color) {
  const int n = circleSegments(radius);
  const float inner = std::max(0.0f, radius - thickness);
  for (int i = 0; i < n; ++i) {
    const float a0 = kTwoPi * static_cast<float>(i) / static_cast<float>(n);
    const float a1 = kTwoPi * static_cast<float>(i + 1) / static_cast<float>(n);
    const float c0 = std::cos(a0), s0 = std::sin(a0), c1 = std::cos(a1), s1 = std::sin(a1);
    const float xy[8] = {cx + c0 * inner, cy + s0 * inner, cx + c0 * radius, cy + s0 * radius,
                         cx + c1 * radius, cy + s1 * radius, cx + c1 * inner, cy + s1 * inner};
    const float uv[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    quad(xy, uv, kModeColor, color);
  }
}

void UiRenderer::icon(ItemId item, float x, float y, float s) {
  if (!isValidItem(item)) return;
  const ItemInfo info = itemInfo(item);
  if (!info.isBlock) {
    const float xy[8] = {x, y, x + s, y, x + s, y + s, x, y + s};
    const float uv[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    quad(xy, uv, static_cast<float>(info.icon) + kKindAlpha, rgba(255, 255, 255));
    return;
  }
  const BlockInfo& b = blockInfo(info.block);
  const uint32_t grass = rgba(110, 172, 72);
  const uint32_t foliage = rgba(88, 150, 58);
  if (b.shape == Shape::Torch) {
    // The torch model only uses a narrow strip of its texture.
    const float w = s * 0.18f, h = s * 0.9f;
    const float xy[8] = {x + (s - w) / 2, y + (s - h) / 2, x + (s + w) / 2, y + (s - h) / 2, x + (s + w) / 2, y + (s + h) / 2,
                         x + (s - w) / 2, y + (s + h) / 2};
    const float uv[8] = {7.0f / 16, 6.0f / 16, 9.0f / 16, 6.0f / 16, 9.0f / 16, 1.0f, 7.0f / 16, 1.0f};
    quad(xy, uv, static_cast<float>(b.tex[0]) + kKindAlpha, rgba(255, 255, 255));
    return;
  }
  if (b.shape == Shape::Cross) {
    const float xy[8] = {x, y, x + s, y, x + s, y + s, x, y + s};
    const float uv[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    const bool tinted = b.tint != Tint::None;
    quad(xy, uv, static_cast<float>(b.tex[0]) + (tinted ? kKindCutout : kKindAlpha), tinted ? grass : rgba(255, 255, 255));
    return;
  }
  // Isometric cube: top, left (+Z side) and right (+X side) faces.
  const float top[8] = {x + s * 0.5f, y, x + s, y + s * 0.25f, x + s * 0.5f, y + s * 0.5f, x, y + s * 0.25f};
  const float left[8] = {x, y + s * 0.25f, x + s * 0.5f, y + s * 0.5f, x + s * 0.5f, y + s, x, y + s * 0.75f};
  const float right[8] = {x + s * 0.5f, y + s * 0.5f, x + s, y + s * 0.25f, x + s, y + s * 0.75f, x + s * 0.5f, y + s};
  const float uvTop[8] = {0, 0, 1, 0, 1, 1, 0, 1};
  const float uvSide[8] = {0, 0, 1, 0, 1, 1, 0, 1};
  auto faceMode = [&](uint16_t texId, bool topFace) {
    const bool tintable = b.tint != Tint::None &&
                          (texId == tex::GrassTop || texId == tex::GrassSide || texId == tex::OakLeaves);
    if (b.layer == Layer::Cutout) return static_cast<float>(texId) + (tintable ? kKindCutout : kKindAlpha);
    if (b.layer == Layer::Translucent) return static_cast<float>(texId) + kKindAlpha;
    if (tintable) return static_cast<float>(texId) + (topFace ? kKindCutout : kKindTintMask);
    return static_cast<float>(texId) + kKindOpaque;
  };
  auto shadeColor = [&](uint16_t texId, int shade) {
    const bool tintable = b.tint != Tint::None &&
                          (texId == tex::GrassTop || texId == tex::GrassSide || texId == tex::OakLeaves);
    uint32_t base = tintable ? (b.tint == Tint::Foliage ? foliage : grass) : rgba(255, 255, 255);
    if (b.layer == Layer::Opaque) return (base & 0x00FFFFFFu) | (static_cast<uint32_t>(shade) << 24);
    // Cut-out and translucent faces use the colour channels for shading.
    const int r = static_cast<int>(((base >> 16) & 0xFF) * shade / 255);
    const int g = static_cast<int>(((base >> 8) & 0xFF) * shade / 255);
    const int bl = static_cast<int>((base & 0xFF) * shade / 255);
    return rgba(r, g, bl, 255);
  };
  const uint16_t tTop = b.tex[FacePosY], tLeft = b.tex[FacePosZ], tRight = b.tex[FacePosX];
  quad(top, uvTop, faceMode(tTop, true), shadeColor(tTop, 255));
  quad(left, uvSide, faceMode(tLeft, false), shadeColor(tLeft, 200));
  quad(right, uvSide, faceMode(tRight, false), shadeColor(tRight, 150));
}

}  // namespace strata
