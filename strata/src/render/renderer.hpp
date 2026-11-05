// Frame rendering: shadows, terrain, water, sky, clouds and post-processing.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/math.hpp"
#include "gl/gl.hpp"
#include "gl/gl_util.hpp"
#include "gl/shader.hpp"
#include "render/atmosphere.hpp"
#include "render/ui.hpp"
#include "world/items.hpp"
#include "world/world.hpp"

namespace strata {

struct RenderSettings {
  int shadowQuality = 2;     // 0 off, 1 low, 2 medium, 3 high
  int cloudQuality = 2;      // 0 off, 1 fast, 2 fancy
  bool reflections = true;   // screen space reflections on water
  bool bloom = true;
  bool antialiasing = true;
  float brightness = 1.0f;   // exposure bias
  bool fog = true;           // distance haze and the fade at the edge of the view (debug switch)
  float renderScale = 1.0f;  // 3D resolution relative to the window (0.5 .. 1); the UI stays sharp
};

struct ParticleDraw {
  dvec3 pos;
  float size = 0.1f;
  uint16_t tex = 0;
  float u0 = 0, v0 = 0, uvSize = 0.25f;  // sub-rectangle of the texture
  uint8_t sky = 255, blk = 0;
  float emissive = 0.0f;
};

struct EntityDraw {
  ItemId item = 0;        // block or item to draw
  dvec3 pos;              // bottom centre
  float yaw = 0.0f;
  float scale = 1.0f;
  uint8_t sky = 255, blk = 0;
};

struct FrameInput {
  dvec3 cameraPos;
  float yaw = 0.0f, pitch = 0.0f;  // radians; yaw 0 looks towards -Z
  float fovDegrees = 70.0f;
  float time = 0.0f;               // seconds, drives animation
  float timeOfDay = 0.25f;         // 0 sunrise, 0.25 noon, 0.5 sunset, 0.75 midnight
  float dt = 0.016f;
  float moonPhase = 0.5f;
  int renderDistance = 8;
  bool underwater = false;
  bool inLava = false;
  float cameraSky = 1.0f;          // sky light at the eye, 0..1
  bool hasSelection = false;
  ivec3 selection;
  BlockId selectionBlock = block::Air;
  int breakStage = -1;             // 0..9 while breaking
  ItemId heldItem = 0;
  float heldSwing = 0.0f;          // 0..1 animation phase
  float heldBob = 0.0f;
  uint8_t heldSky = 255, heldBlk = 0;
  float damageFlash = 0.0f;
  std::vector<ParticleDraw> particles;
  std::vector<EntityDraw> entities;
};

struct RenderStats {
  int sectionsVisible = 0;
  int sectionsTotal = 0;
  int drawCalls = 0;
  size_t gpuBytes = 0;
};

class Renderer {
 public:
  Renderer();
  ~Renderer();
  bool init(int width, int height, const RenderSettings& settings);
  void shutdown();
  void resize(int width, int height);  // window framebuffer size
  void setSettings(const RenderSettings& settings);
  const RenderSettings& settings() const { return settings_; }

  void uploadMeshes(std::vector<SectionMeshUpdate>& updates);
  void clearMeshes();

  // Renders the world and post-processing into the default framebuffer.
  void render(const FrameInput& in);
  UiRenderer& ui() { return ui_; }
  GLuint blockTextures() const { return albedoArray_; }
  // Reads back the default framebuffer (call before swapping buffers).
  bool saveScreenshot(const std::string& path);

  const RenderStats& stats() const { return stats_; }
  const std::string& rendererName() const { return rendererName_; }
  const DaylightState& daylight() const { return daylight_; }

 private:
  struct GpuSection {
    GLuint vao = 0, vbo = 0;
    int cx = 0, sy = 0, cz = 0;
    int opaque = 0, cutout = 0, water = 0, glass = 0;  // vertex counts
    size_t bytes = 0;
  };
  struct DrawItem {
    const GpuSection* s;
    float dist;
    vec3 offset;
  };

  bool buildShaders();
  void createTextures();
  void createTargets();
  void destroyTargets();
  void createShadowMaps();
  void ensureIndexCapacity(size_t quads);
  void setupTerrainAttributes();
  const std::vector<TerrainVertex>& blockModel(BlockId id);
  void buildFlatModel(uint16_t texLayer, std::vector<TerrainVertex>& out);

  void updateSkyLut(const FrameInput& in);
  void renderShadows(const FrameInput& in, const std::vector<DrawItem>& all);
  void drawSections(Shader& sh, const std::vector<DrawItem>& items, int layer, const mat4* lightVP);
  void drawEntities(const FrameInput& in, const mat4& viewProj);
  void drawHeldItem(const FrameInput& in, const mat4& viewProj, const vec3& fwd, const vec3& right, const vec3& up);
  void drawSelection(const FrameInput& in, const mat4& viewProj);
  void drawParticles(const FrameInput& in, const mat4& viewProj, const vec3& right, const vec3& up);
  void setTerrainUniforms(Shader& sh, const FrameInput& in, const mat4& viewProj);
  void bindTerrainTextures();
  void postProcess(const FrameInput& in, const mat4& invViewProj);
  void fullscreen();

  RenderSettings settings_;
  UiRenderer ui_;
  bool updateInternalSize();  // from outWidth_/outHeight_ and the render scale; true when it changed
  int width_ = 1, height_ = 1;        // internal (scaled) resolution of the 3D scene
  int outWidth_ = 1, outHeight_ = 1;  // window framebuffer
  std::string rendererName_;

  Shader terrain_, water_, glass_, shadow_, skyLut_, sky_, composite_, bloomDown_, bloomUp_, luminance_, adapt_,
      tonemap_, fxaa_, lines_, particles_;

  GLuint albedoArray_ = 0, materialArray_ = 0;
  GLuint indexBuffer_ = 0;
  size_t indexQuads_ = 0;
  GLuint emptyVao_ = 0;
  GLuint dynVao_ = 0, dynVbo_ = 0;
  GLuint lineVao_ = 0, lineVbo_ = 0;
  GLuint particleVao_ = 0, particleVbo_ = 0;

  gl::Target scene_, sceneCopy_, composited_, ldr_;
  gl::Target skyLutTarget_;
  gl::Target lumTarget_;
  gl::Target adaptTargets_[2];
  int adaptIndex_ = 0;
  bool adaptPrimed_ = false;
  std::vector<gl::Target> bloom_;

  GLuint shadowTex_ = 0, shadowFbo_ = 0;
  int shadowRes_ = 0, cascades_ = 0;
  mat4 shadowMats_[4];
  float cascadeEnd_[4] = {0, 0, 0, 0};
  float cascadeTexel_[4] = {0, 0, 0, 0};

  std::unordered_map<uint64_t, GpuSection> sections_;
  std::vector<std::vector<TerrainVertex>> blockModels_;
  std::vector<bool> blockModelBuilt_;

  DaylightState daylight_;
  float lutTimeOfDay_ = -1.0f;
  int lutAge_ = 0;
  RenderStats stats_;
  mat4 view_, proj_;
  float near_ = 0.05f, far_ = 500.0f;
};

}  // namespace strata
