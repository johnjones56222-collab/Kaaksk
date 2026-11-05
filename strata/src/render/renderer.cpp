#include "render/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

#include "core/image_io.hpp"
#include "core/log.hpp"
#include "render/shaders.hpp"
#include "render/textures.hpp"
#include "world/mesher.hpp"

namespace strata {

namespace {

uint64_t sectionKey(int cx, int sy, int cz) {
  return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
         (static_cast<uint64_t>(static_cast<uint32_t>(cz) & 0xFFFFFFu) << 8) | static_cast<uint64_t>(sy & 0xFF);
}

struct LineVertex {
  float x, y, z;
};
struct ParticleVertex {
  float x, y, z, u, v, layer, sky, blk, emit;
};

constexpr int kTextureSize = 64;
constexpr int kSkyLutW = 256, kSkyLutH = 128;
constexpr int kLumSize = 128;

mat4 biasMatrix() {
  mat4 b = mat4::identity();
  b.at(0, 0) = b.at(1, 1) = b.at(2, 2) = 0.5f;
  b.at(0, 3) = b.at(1, 3) = b.at(2, 3) = 0.5f;
  return b;
}

bool hasExtension(const char* name) {
  GLint n = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &n);
  for (GLint i = 0; i < n; ++i) {
    const char* e = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
    if (e && std::strcmp(e, name) == 0) return true;
  }
  return false;
}

}  // namespace

Renderer::Renderer() = default;
Renderer::~Renderer() { shutdown(); }

bool Renderer::init(int width, int height, const RenderSettings& settings) {
  settings_ = settings;
  outWidth_ = std::max(1, width);
  outHeight_ = std::max(1, height);
  updateInternalSize();
  const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  rendererName_ = std::string(renderer ? renderer : "?") + " | OpenGL " + (version ? version : "?");
  LOG_INFO("renderer: %s", rendererName_.c_str());
  if (!buildShaders()) return false;
  if (!ui_.init()) return false;
  createTextures();

  glGenVertexArrays(1, &emptyVao_);
  glGenBuffers(1, &indexBuffer_);
  ensureIndexCapacity(1 << 16);

  glGenVertexArrays(1, &dynVao_);
  glGenBuffers(1, &dynVbo_);
  glBindVertexArray(dynVao_);
  glBindBuffer(GL_ARRAY_BUFFER, dynVbo_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);
  setupTerrainAttributes();

  glGenVertexArrays(1, &lineVao_);
  glGenBuffers(1, &lineVbo_);
  glBindVertexArray(lineVao_);
  glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), nullptr);

  glGenVertexArrays(1, &particleVao_);
  glGenBuffers(1, &particleVbo_);
  glBindVertexArray(particleVao_);
  glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
  const GLsizei ps = sizeof(ParticleVertex);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, ps, reinterpret_cast<const void*>(offsetof(ParticleVertex, x)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, ps, reinterpret_cast<const void*>(offsetof(ParticleVertex, u)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, ps, reinterpret_cast<const void*>(offsetof(ParticleVertex, sky)));
  glBindVertexArray(0);

  blockModels_.assign(256, {});
  blockModelBuilt_.assign(256, false);
  createTargets();
  createShadowMaps();
  return gl::checkErrors("renderer init");
}

void Renderer::shutdown() {
  clearMeshes();
  destroyTargets();
  if (shadowTex_) glDeleteTextures(1, &shadowTex_);
  if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
  shadowTex_ = shadowFbo_ = 0;
  GLuint bufs[] = {indexBuffer_, dynVbo_, lineVbo_, particleVbo_};
  for (GLuint b : bufs)
    if (b) glDeleteBuffers(1, &b);
  GLuint vaos[] = {emptyVao_, dynVao_, lineVao_, particleVao_};
  for (GLuint v : vaos)
    if (v) glDeleteVertexArrays(1, &v);
  indexBuffer_ = dynVbo_ = lineVbo_ = particleVbo_ = 0;
  emptyVao_ = dynVao_ = lineVao_ = particleVao_ = 0;
  if (albedoArray_) glDeleteTextures(1, &albedoArray_);
  if (materialArray_) glDeleteTextures(1, &materialArray_);
  albedoArray_ = materialArray_ = 0;
  ui_.shutdown();
  for (Shader* s : {&terrain_, &water_, &glass_, &shadow_, &skyLut_, &sky_, &composite_, &bloomDown_, &bloomUp_,
                    &luminance_, &adapt_, &tonemap_, &fxaa_, &lines_, &particles_})
    s->destroy();
}

bool Renderer::buildShaders() {
  bool ok = true;
  ok &= terrain_.build(shaders::program("terrain"));
  ok &= water_.build(shaders::program("water"));
  ok &= glass_.build(shaders::program("glass"));
  ok &= shadow_.build(shaders::program("shadow"));
  ok &= skyLut_.build(shaders::program("sky lut"));
  ok &= sky_.build(shaders::program("sky"));
  ok &= composite_.build(shaders::program("composite"));
  ok &= bloomDown_.build(shaders::program("bloom down"));
  ok &= bloomUp_.build(shaders::program("bloom up"));
  ok &= luminance_.build(shaders::program("luminance"));
  ok &= adapt_.build(shaders::program("adapt"));
  ok &= tonemap_.build(shaders::program("tonemap"));
  ok &= fxaa_.build(shaders::program("fxaa"));
  ok &= lines_.build(shaders::program("lines"));
  ok &= particles_.build(shaders::program("particles"));
  return ok;
}

void Renderer::createTextures() {
  const TextureSet set = generateTextures(kTextureSize);
  const int layers = static_cast<int>(set.layers.size());
  int levels = 1;
  while ((kTextureSize >> (levels - 1)) > 1) ++levels;
  const bool aniso = hasExtension("GL_EXT_texture_filter_anisotropic") || hasExtension("GL_ARB_texture_filter_anisotropic");
  float maxAniso = 1.0f;
  if (aniso) glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);

  auto makeArray = [&](GLuint& tex, GLenum internalFormat, bool albedo) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    for (int l = 0; l < levels; ++l) {
      const int s = kTextureSize >> l;
      glTexImage3D(GL_TEXTURE_2D_ARRAY, l, static_cast<GLint>(internalFormat), s, s, layers, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, nullptr);
    }
    for (int i = 0; i < layers; ++i) {
      const TextureLayer& layer = set.layers[static_cast<size_t>(i)];
      const auto mips = albedo ? buildAlbedoMips(layer.albedo, kTextureSize, layer.cutout)
                               : buildMaterialMips(layer.material, kTextureSize);
      for (int l = 0; l < levels; ++l) {
        const int s = kTextureSize >> l;
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, l, 0, 0, i, s, s, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        mips[static_cast<size_t>(l)].data());
      }
    }
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, levels - 1);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (aniso) glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY, std::min(8.0f, maxAniso));
  };
  makeArray(albedoArray_, GL_SRGB8_ALPHA8, true);
  makeArray(materialArray_, GL_RGBA8, false);
  LOG_INFO("generated %d block textures (%dx%d, %d mip levels)", layers, kTextureSize, kTextureSize, levels);
}

void Renderer::createTargets() {
  scene_ = gl::makeTarget(width_, height_, GL_RGBA16F, true);
  sceneCopy_ = gl::makeTarget(width_, height_, GL_RGBA16F, true);
  composited_ = gl::makeTarget(width_, height_, GL_RGBA16F, false);
  ldr_ = gl::makeTarget(width_, height_, GL_RGBA8, false);
  skyLutTarget_ = gl::makeTarget(kSkyLutW, kSkyLutH, GL_RGBA16F, false);
  glBindTexture(GL_TEXTURE_2D, skyLutTarget_.color);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  lumTarget_ = gl::makeTarget(kLumSize, kLumSize, GL_RG16F, false);
  glBindTexture(GL_TEXTURE_2D, lumTarget_.color);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
  // The adapted exposure lives in a 1x1 float target; fall back to half
  // floats where 32 bit float rendering is unavailable (some WebGL 2 devices).
  static const GLenum adaptFormat = gl::isColorRenderable(GL_R32F) ? GL_R32F : GL_R16F;
  for (gl::Target& t : adaptTargets_) {
    t = gl::makeTarget(1, 1, adaptFormat, false, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    const float clear[4] = {std::log(0.5f), 0.0f, 0.0f, 1.0f};
    glClearBufferfv(GL_COLOR, 0, clear);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  adaptPrimed_ = false;
  bloom_.clear();
  int bw = width_ / 2, bh = height_ / 2;
  for (int i = 0; i < 6 && bw >= 4 && bh >= 4; ++i) {
    bloom_.push_back(gl::makeTarget(bw, bh, GL_RGBA16F, false));
    bw /= 2;
    bh /= 2;
  }
  lutTimeOfDay_ = -1.0f;
}

void Renderer::destroyTargets() {
  scene_.destroy();
  sceneCopy_.destroy();
  composited_.destroy();
  ldr_.destroy();
  skyLutTarget_.destroy();
  lumTarget_.destroy();
  for (gl::Target& t : adaptTargets_) t.destroy();
  for (gl::Target& t : bloom_) t.destroy();
  bloom_.clear();
}

void Renderer::createShadowMaps() {
  if (shadowTex_) glDeleteTextures(1, &shadowTex_);
  if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
  switch (settings_.shadowQuality) {
    case 0: cascades_ = 0; shadowRes_ = 16; break;
    case 1: cascades_ = 1; shadowRes_ = 1024; break;
    case 2: cascades_ = 2; shadowRes_ = 2048; break;
    default: cascades_ = 3; shadowRes_ = 2048; break;
  }
  const int layers = std::max(1, cascades_);
  glGenTextures(1, &shadowTex_);
  glBindTexture(GL_TEXTURE_2D_ARRAY, shadowTex_);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, shadowRes_, shadowRes_, layers, 0, GL_DEPTH_COMPONENT,
               GL_UNSIGNED_INT, nullptr);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  glGenFramebuffers(1, &shadowFbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
  glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowTex_, 0, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  gl::checkFramebuffer("shadow map");
  // Clear every layer so a disabled cascade never shadows anything.
  for (int l = 0; l < layers; ++l) {
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowTex_, 0, l);
    glClear(GL_DEPTH_BUFFER_BIT);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Renderer::updateInternalSize() {
  const float scale = clampv(settings_.renderScale, 0.25f, 1.0f);
  const int w = std::max(1, static_cast<int>(std::lround(static_cast<float>(outWidth_) * scale)));
  const int h = std::max(1, static_cast<int>(std::lround(static_cast<float>(outHeight_) * scale)));
  if (w == width_ && h == height_) return false;
  width_ = w;
  height_ = h;
  return true;
}

void Renderer::resize(int width, int height) {
  outWidth_ = std::max(1, width);
  outHeight_ = std::max(1, height);
  if (!updateInternalSize()) return;
  destroyTargets();
  createTargets();
}

void Renderer::setSettings(const RenderSettings& s) {
  const bool shadowsChanged = s.shadowQuality != settings_.shadowQuality;
  const bool scaleChanged = s.renderScale != settings_.renderScale;
  settings_ = s;
  if (shadowsChanged) createShadowMaps();
  if (scaleChanged && updateInternalSize()) {
    destroyTargets();
    createTargets();
  }
}

void Renderer::ensureIndexCapacity(size_t quads) {
  if (quads <= indexQuads_) return;
  const size_t n = std::max<size_t>(quads, indexQuads_ * 2);
  std::vector<uint32_t> idx(n * 6);
  for (size_t q = 0; q < n; ++q) {
    const uint32_t b = static_cast<uint32_t>(q * 4);
    uint32_t* o = &idx[q * 6];
    o[0] = b;
    o[1] = b + 1;
    o[2] = b + 2;
    o[3] = b;
    o[4] = b + 2;
    o[5] = b + 3;
  }
  // The element buffer binding is VAO state, so bind a VAO we own first.
  glBindVertexArray(emptyVao_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(uint32_t)), idx.data(),
               GL_STATIC_DRAW);
  glBindVertexArray(0);
  indexQuads_ = n;
}

void Renderer::setupTerrainAttributes() {
  const GLsizei stride = sizeof(TerrainVertex);
  glEnableVertexAttribArray(0);
  glVertexAttribIPointer(0, 3, GL_SHORT, stride, reinterpret_cast<const void*>(offsetof(TerrainVertex, x)));
  glEnableVertexAttribArray(1);
  glVertexAttribIPointer(1, 1, GL_UNSIGNED_SHORT, stride, reinterpret_cast<const void*>(offsetof(TerrainVertex, tex)));
  glEnableVertexAttribArray(2);
  glVertexAttribIPointer(2, 4, GL_UNSIGNED_BYTE, stride, reinterpret_cast<const void*>(offsetof(TerrainVertex, u)));
  glEnableVertexAttribArray(3);
  glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, stride, reinterpret_cast<const void*>(offsetof(TerrainVertex, sky)));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<const void*>(offsetof(TerrainVertex, r)));
}

// ---------------------------------------------------------------------------
// Meshes
// ---------------------------------------------------------------------------
void Renderer::uploadMeshes(std::vector<SectionMeshUpdate>& updates) {
  for (SectionMeshUpdate& u : updates) {
    const uint64_t key = sectionKey(u.cx, u.sy, u.cz);
    if (!u.mesh || u.mesh->empty()) {
      auto it = sections_.find(key);
      if (it != sections_.end()) {
        glDeleteBuffers(1, &it->second.vbo);
        glDeleteVertexArrays(1, &it->second.vao);
        sections_.erase(it);
      }
      continue;
    }
    GpuSection& s = sections_[key];
    if (!s.vao) {
      glGenVertexArrays(1, &s.vao);
      glGenBuffers(1, &s.vbo);
      glBindVertexArray(s.vao);
      glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);
      setupTerrainAttributes();
      glBindVertexArray(0);
    }
    s.cx = u.cx;
    s.sy = u.sy;
    s.cz = u.cz;
    const MeshData& m = *u.mesh;
    s.opaque = static_cast<int>(m.opaque.size());
    s.cutout = static_cast<int>(m.cutout.size());
    s.water = static_cast<int>(m.water.size());
    s.glass = static_cast<int>(m.glass.size());
    const size_t total = m.vertexCount();
    s.bytes = total * sizeof(TerrainVertex);
    ensureIndexCapacity(total / 4 + 1);
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(s.bytes), nullptr, GL_STATIC_DRAW);
    size_t offset = 0;
    for (const std::vector<TerrainVertex>* part : {&m.opaque, &m.cutout, &m.water, &m.glass}) {
      if (!part->empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset),
                        static_cast<GLsizeiptr>(part->size() * sizeof(TerrainVertex)), part->data());
      }
      offset += part->size() * sizeof(TerrainVertex);
    }
  }
  updates.clear();
}

void Renderer::clearMeshes() {
  for (auto& kv : sections_) {
    glDeleteBuffers(1, &kv.second.vbo);
    glDeleteVertexArrays(1, &kv.second.vao);
  }
  sections_.clear();
}

const std::vector<TerrainVertex>& Renderer::blockModel(BlockId id) {
  if (blockModelBuilt_[id]) return blockModels_[id];
  auto in = std::make_unique<MeshInput>();
  std::memset(in->blocks, 0, sizeof(in->blocks));
  std::memset(in->meta, 0, sizeof(in->meta));
  std::memset(in->light, packLight(15, 0), sizeof(in->light));
  for (int i = 0; i < kPad * kPad; ++i) {
    in->grass[i][0] = 110;
    in->grass[i][1] = 172;
    in->grass[i][2] = 72;
    in->foliage[i][0] = 88;
    in->foliage[i][1] = 150;
    in->foliage[i][2] = 58;
  }
  in->blocks[padIndex(0, 0, 0)] = id;
  MeshData mesh;
  buildSectionMesh(*in, mesh);
  std::vector<TerrainVertex>& out = blockModels_[id];
  out.clear();
  for (const auto* part : {&mesh.opaque, &mesh.cutout, &mesh.glass}) out.insert(out.end(), part->begin(), part->end());
  blockModelBuilt_[id] = true;
  return out;
}

void Renderer::buildFlatModel(uint16_t layer, std::vector<TerrainVertex>& out) {
  out.clear();
  const int16_t xs[4] = {0, 32, 32, 0};
  const int16_t ys[4] = {0, 0, 32, 32};
  const uint8_t us[4] = {0, 16, 16, 0};
  const uint8_t vs[4] = {16, 16, 0, 0};
  for (int side = 0; side < 2; ++side)
    for (int k = 0; k < 4; ++k) {
      const int i = side == 0 ? k : 3 - k;
      TerrainVertex v{};
      v.x = xs[i];
      v.y = ys[i];
      v.z = 16;
      v.tex = layer;
      v.u = us[i];
      v.v = vs[i];
      v.normal = side == 0 ? FacePosZ : FaceNegZ;
      v.ao = 255;
      v.sky = 255;
      v.r = v.g = v.b = v.a = 255;
      out.push_back(v);
    }
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
void Renderer::bindTerrainTextures() {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, albedoArray_);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D_ARRAY, materialArray_);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D_ARRAY, shadowTex_);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, skyLutTarget_.color);
  glActiveTexture(GL_TEXTURE0);
}

void Renderer::setTerrainUniforms(Shader& sh, const FrameInput& in, const mat4& viewProj) {
  sh.use();
  sh.set("uViewProj", viewProj);
  sh.set("uModel", mat4::identity());
  sh.set("uCameraPos", in.cameraPos.toFloat());
  sh.set("uTime", in.time);
  sh.set("uWind", 1.0f);
  const vec3 fwd(std::sin(in.yaw) * std::cos(in.pitch), std::sin(in.pitch), -std::cos(in.yaw) * std::cos(in.pitch));
  sh.set("uCamForward", fwd);
  sh.set("uCascades", cascades_ > 0 && dot(daylight_.lightColor, vec3(1.0f)) > 1e-4f ? cascades_ : 0);
  sh.setMatrices("uShadowMat", shadowMats_, 4);
  sh.set("uCascadeEnd", vec4(cascadeEnd_[0], cascadeEnd_[1], cascadeEnd_[2], cascadeEnd_[3]));
  sh.set("uCascadeTexel", vec4(cascadeTexel_[0], cascadeTexel_[1], cascadeTexel_[2], cascadeTexel_[3]));
  sh.set("uLightDir", daylight_.lightDir);
  sh.set("uLightColor", daylight_.lightColor);
  sh.set("uAmbient", daylight_.ambientColor);
  const float flicker = 1.0f + 0.035f * std::sin(in.time * 12.7f) + 0.025f * std::sin(in.time * 7.3f + 1.3f);
  sh.set("uBlockColor", vec3(1.0f, 0.62f, 0.32f) * flicker);
  // The fade must finish before the nearest chunk that may be missing: the
  // player can stand anywhere inside its chunk, so that is about R - 0.6 chunks.
  const float fadeEnd = (static_cast<float>(in.renderDistance) - 0.6f) * 16.0f;
  sh.set("uFogDensity", in.underwater || !settings_.fog ? 0.0f : 0.0011f);
  sh.set("uFadeStart", settings_.fog ? fadeEnd * 0.82f : 1e6f);
  sh.set("uFadeEnd", settings_.fog ? fadeEnd : 2e6f);
  // Ground with ~20% albedo lit by the sun and the sky bounces light upwards.
  sh.set("uBounce", (daylight_.lightColor * std::max(daylight_.lightDir.y, 0.0f) + daylight_.ambientColor) * 0.2f);
  sh.set("uCameraSky", in.cameraSky);
  sh.set("uAlphaTest", 0.5f);
  sh.set("uCloudShadows", settings_.cloudQuality > 0 ? 1.0f : 0.0f);
  sh.set("uCloudOffset", vec2(in.time * 6.0f, in.time * 2.5f));
  sh.set("uCloudCoverage", settings_.cloudQuality > 0 ? 0.46f + 0.08f * std::sin(in.time * 0.004f) : 0.0f);
  sh.set("uEntityLight", 0.0f);
  sh.set("uAlbedo", 0);
  sh.set("uMaterial", 1);
  sh.set("uShadowMap", 2);
  sh.set("uSkyLut", 3);
}

void Renderer::drawSections(Shader& sh, const std::vector<DrawItem>& items, int layer, const mat4* lightVP) {
  (void)lightVP;
  const GLint modelLoc = sh.location("uModel");
  for (const DrawItem& item : items) {
    const GpuSection& s = *item.s;
    int first = 0, count = 0;
    switch (layer) {
      case 0: first = 0; count = s.opaque; break;
      case 1: first = s.opaque; count = s.cutout; break;
      case 2: first = s.opaque + s.cutout; count = s.water; break;
      default: first = s.opaque + s.cutout + s.water; count = s.glass; break;
    }
    if (count == 0) continue;
    const mat4 model = translation(item.offset);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
    glBindVertexArray(s.vao);
    glDrawElements(GL_TRIANGLES, count / 4 * 6, GL_UNSIGNED_INT,
                   reinterpret_cast<const void*>(static_cast<size_t>(first / 4 * 6) * sizeof(uint32_t)));
    ++stats_.drawCalls;
  }
  glBindVertexArray(0);
}

void Renderer::updateSkyLut(const FrameInput& in) {
  ++lutAge_;
  if (std::fabs(in.timeOfDay - lutTimeOfDay_) < 0.0004f && lutAge_ < 120) return;
  lutTimeOfDay_ = in.timeOfDay;
  lutAge_ = 0;
  glBindFramebuffer(GL_FRAMEBUFFER, skyLutTarget_.fbo);
  glViewport(0, 0, kSkyLutW, kSkyLutH);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  skyLut_.use();
  skyLut_.set("uSunDir", daylight_.sunDir);
  skyLut_.set("uMoonDir", daylight_.moonDir);
  // Moonlight follows the phase: a new moon barely lights the sky.
  const float phaseLight = 0.15f + 0.85f * std::sin(in.moonPhase * kPi);
  skyLut_.set("uMoonIntensity", 0.0025f * phaseLight);
  skyLut_.set("uAmbientRadiance", daylight_.ambientColor);
  fullscreen();
}

void Renderer::renderShadows(const FrameInput& in, const std::vector<DrawItem>& all) {
  for (int i = 0; i < 4; ++i) {
    cascadeEnd_[i] = 0.0f;
    cascadeTexel_[i] = 0.0f;
    shadowMats_[i] = mat4::identity();
  }
  if (cascades_ == 0 || dot(daylight_.lightColor, vec3(1.0f)) < 1e-4f) return;
  const vec3 lightDir = daylight_.lightDir;
  const float fov = radians(in.fovDegrees);
  const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
  const vec3 fwd(std::sin(in.yaw) * std::cos(in.pitch), std::sin(in.pitch), -std::cos(in.yaw) * std::cos(in.pitch));
  const vec3 right = normalize(cross(fwd, vec3(0, 1, 0)));
  const vec3 up = cross(right, fwd);
  const float maxDist = std::min(static_cast<float>(in.renderDistance * 16),
                                 settings_.shadowQuality >= 3 ? 200.0f : settings_.shadowQuality == 2 ? 128.0f : 72.0f);
  const vec3 upRef = std::fabs(lightDir.y) > 0.99f ? vec3(0, 0, 1) : vec3(0, 1, 0);
  const vec3 lf = -lightDir;
  const vec3 lr = normalize(cross(lf, upRef));
  const vec3 lu = cross(lr, lf);

  glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
  glViewport(0, 0, shadowRes_, shadowRes_);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.6f, 3.0f);
  shadow_.use();
  shadow_.set("uCameraPos", in.cameraPos.toFloat());
  shadow_.set("uTime", in.time);
  shadow_.set("uWind", 1.0f);
  shadow_.set("uAlbedo", 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, albedoArray_);
  const GLint modelLoc = shadow_.location("uModel");
  const GLint alphaLoc = shadow_.location("uAlphaTest");

  float prev = 0.3f;
  for (int c = 0; c < cascades_; ++c) {
    const float t = static_cast<float>(c + 1) / static_cast<float>(cascades_);
    const float uniform = 1.0f + (maxDist - 1.0f) * t;
    const float logSplit = std::pow(maxDist, t);
    const float split = lerpf(uniform, logSplit, 0.72f);
    // Bounding sphere of the frustum slice [prev, split].
    vec3 corners[8];
    int k = 0;
    for (float d : {prev, split}) {
      const float hh = d * std::tan(fov * 0.5f), hw = hh * aspect;
      for (int sy = -1; sy <= 1; sy += 2)
        for (int sx = -1; sx <= 1; sx += 2)
          corners[k++] = fwd * d + right * (hw * static_cast<float>(sx)) + up * (hh * static_cast<float>(sy));
    }
    vec3 centre(0.0f);
    for (const vec3& p : corners) centre += p;
    centre = centre / 8.0f;
    float radius = 0.0f;
    for (const vec3& p : corners) radius = std::max(radius, length(p - centre));
    radius = std::ceil(radius * 4.0f) / 4.0f;
    const float texel = 2.0f * radius / static_cast<float>(shadowRes_);
    // Snap the centre to the shadow texel grid in world space (no shimmering).
    const double wx = in.cameraPos.x + centre.x, wy = in.cameraPos.y + centre.y, wz = in.cameraPos.z + centre.z;
    const double px = wx * lr.x + wy * lr.y + wz * lr.z;
    const double py = wx * lu.x + wy * lu.y + wz * lu.z;
    const double sx = std::floor(px / texel) * texel - px;
    const double sy = std::floor(py / texel) * texel - py;
    centre += lr * static_cast<float>(sx) + lu * static_cast<float>(sy);
    const float back = radius + 320.0f;
    const mat4 lview = lookAt(centre + lightDir * back, centre, upRef);
    const mat4 lproj = ortho(-radius, radius, -radius, radius, 0.5f, back + radius);
    const mat4 lvp = lproj * lview;
    shadowMats_[c] = biasMatrix() * lvp;
    cascadeEnd_[c] = split;
    cascadeTexel_[c] = texel;
    prev = split;

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowTex_, 0, c);
    glClear(GL_DEPTH_BUFFER_BIT);
    shadow_.set("uLightViewProj", lvp);
    const Frustum fr = Frustum::fromMatrix(lvp);
    for (int layer = 0; layer < 2; ++layer) {
      glUniform1f(alphaLoc, layer == 0 ? 0.0f : 0.5f);
      for (const DrawItem& item : all) {
        const GpuSection& s = *item.s;
        const int count = layer == 0 ? s.opaque : s.cutout;
        if (count == 0) continue;
        if (!fr.intersectsAabb(item.offset - vec3(1.0f), item.offset + vec3(17.0f))) continue;
        const int first = layer == 0 ? 0 : s.opaque;
        const mat4 model = translation(item.offset);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
        glBindVertexArray(s.vao);
        glDrawElements(GL_TRIANGLES, count / 4 * 6, GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(static_cast<size_t>(first / 4 * 6) * sizeof(uint32_t)));
        ++stats_.drawCalls;
      }
    }
  }
  glBindVertexArray(0);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glEnable(GL_CULL_FACE);
}

void Renderer::fullscreen() {
  glBindVertexArray(emptyVao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
}

void Renderer::render(const FrameInput& in) {
  stats_ = RenderStats{};
  daylight_ = computeDaylight(in.timeOfDay);
  const vec3 fwd(std::sin(in.yaw) * std::cos(in.pitch), std::sin(in.pitch), -std::cos(in.yaw) * std::cos(in.pitch));
  const vec3 right = normalize(cross(fwd, vec3(0, 1, 0)));
  const vec3 up = cross(right, fwd);
  const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
  near_ = 0.05f;
  far_ = std::max(320.0f, static_cast<float>(in.renderDistance * 16) * 1.6f + 64.0f);
  view_ = lookAt(vec3(0.0f), fwd, vec3(0, 1, 0));
  proj_ = perspective(radians(in.fovDegrees), aspect, near_, far_);
  const mat4 viewProj = proj_ * view_;
  const mat4 invViewProj = inverse(viewProj);
  const Frustum frustum = Frustum::fromMatrix(viewProj);

  // Gather sections within range; cull against the view frustum.
  std::vector<DrawItem> all, visible;
  all.reserve(sections_.size());
  const double maxDist = static_cast<double>((in.renderDistance + 1) * 16);
  for (const auto& kv : sections_) {
    const GpuSection& s = kv.second;
    const double ox = s.cx * 16.0 - in.cameraPos.x, oy = s.sy * 16.0 - in.cameraPos.y, oz = s.cz * 16.0 - in.cameraPos.z;
    const double hx = ox + 8.0, hz = oz + 8.0;
    if (hx * hx + hz * hz > maxDist * maxDist) continue;
    const vec3 off(static_cast<float>(ox), static_cast<float>(oy), static_cast<float>(oz));
    const float dist = length(off + vec3(8.0f));
    all.push_back({&s, dist, off});
    if (frustum.intersectsAabb(off - vec3(1.0f), off + vec3(17.0f))) visible.push_back({&s, dist, off});
    stats_.gpuBytes += s.bytes;
  }
  stats_.sectionsTotal = static_cast<int>(sections_.size());
  stats_.sectionsVisible = static_cast<int>(visible.size());
  std::sort(visible.begin(), visible.end(), [](const DrawItem& a, const DrawItem& b) { return a.dist < b.dist; });

  updateSkyLut(in);
  renderShadows(in, all);

  // ---- Main HDR pass ----
  glBindFramebuffer(GL_FRAMEBUFFER, scene_.fbo);
  glViewport(0, 0, width_, height_);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  bindTerrainTextures();
  setTerrainUniforms(terrain_, in, viewProj);
  terrain_.set("uAlphaTest", 0.0f);
  drawSections(terrain_, visible, 0, nullptr);
  terrain_.set("uAlphaTest", 0.5f);
  drawSections(terrain_, visible, 1, nullptr);
  drawEntities(in, viewProj);
  drawParticles(in, viewProj, right, up);
  drawSelection(in, viewProj);

  // Sky behind everything.
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  sky_.use();
  sky_.set("uInvViewProj", invViewProj);
  sky_.set("uSkyLut", 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, skyLutTarget_.color);
  sky_.set("uSunDir", daylight_.sunDir);
  sky_.set("uMoonDir", daylight_.moonDir);
  sky_.set("uSunDisc", AtmosphereModel::transmittance(daylight_.sunDir) * 140.0f);
  sky_.set("uMoonBrightness", 0.55f * smoothstepf(-0.05f, 0.08f, daylight_.moonDir.y) *
                                  (0.35f + 0.65f * daylight_.nightFactor));
  sky_.set("uMoonPhase", in.moonPhase);
  sky_.set("uStars", saturate((-daylight_.sunDir.y - 0.02f) / 0.2f));
  sky_.set("uStarAngle", in.timeOfDay * kTwoPi);
  sky_.set("uTime", in.time);
  sky_.set("uCameraSky", in.cameraSky);
  fullscreen();
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
  glEnable(GL_CULL_FACE);

  // Copy colour and depth for refraction and screen space reflections.
  glBindFramebuffer(GL_READ_FRAMEBUFFER, scene_.fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneCopy_.fbo);
  glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                    GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, scene_.fbo);

  // Water: replaces the pixels it covers using the copy.
  bindTerrainTextures();
  setTerrainUniforms(water_, in, viewProj);
  water_.set("uSceneColor", 4);
  water_.set("uSceneDepth", 5);
  water_.set("uScreenSize", vec2(static_cast<float>(width_), static_cast<float>(height_)));
  water_.set("uNearFar", vec2(near_, far_));
  water_.set("uView", view_);
  water_.set("uProj", proj_);
  water_.set("uSsr", settings_.reflections ? 1 : 0);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, sceneCopy_.color);
  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_2D, sceneCopy_.depth);
  glActiveTexture(GL_TEXTURE0);
  drawSections(water_, visible, 2, nullptr);

  // Glass and ice, blended back to front.
  std::vector<DrawItem> backToFront = visible;
  std::reverse(backToFront.begin(), backToFront.end());
  setTerrainUniforms(glass_, in, viewProj);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  drawSections(glass_, backToFront, 3, nullptr);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);

  drawHeldItem(in, viewProj, fwd, right, up);
  postProcess(in, invViewProj);
  gl::checkErrors("frame");
}

void Renderer::drawEntities(const FrameInput& in, const mat4& viewProj) {
  (void)viewProj;
  if (in.entities.empty()) return;
  std::vector<TerrainVertex> verts;
  std::vector<std::pair<size_t, size_t>> ranges;
  std::vector<mat4> models;
  std::vector<TerrainVertex> flat;
  for (const EntityDraw& e : in.entities) {
    if (!isValidItem(e.item)) continue;
    const ItemInfo info = itemInfo(e.item);
    const std::vector<TerrainVertex>* model = nullptr;
    bool isFlat = false;
    if (info.isBlock && blockInfo(info.block).shape != Shape::Cross) {
      model = &blockModel(info.block);
    } else {
      buildFlatModel(info.isBlock ? blockInfo(info.block).tex[0] : info.icon, flat);
      model = &flat;
      isFlat = true;
    }
    const size_t start = verts.size();
    for (TerrainVertex v : *model) {
      v.sky = e.sky;
      v.blk = e.blk;
      v.anim = 0;
      verts.push_back(v);
    }
    ranges.emplace_back(start, verts.size() - start);
    const vec3 rel = (e.pos - in.cameraPos).toFloat();
    mat4 m = translation(rel) * rotation(vec3(0, 1, 0), e.yaw) * scaling(vec3(e.scale));
    m = m * translation(isFlat ? vec3(-0.5f, 0.0f, -0.5f) : vec3(-0.5f, 0.0f, -0.5f));
    models.push_back(m);
  }
  if (verts.empty()) return;
  ensureIndexCapacity(verts.size() / 4 + 1);
  glBindBuffer(GL_ARRAY_BUFFER, dynVbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(TerrainVertex)), verts.data(),
               GL_STREAM_DRAW);
  terrain_.use();
  terrain_.set("uAlphaTest", 0.5f);
  terrain_.set("uEntityLight", 1.0f);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(dynVao_);
  const GLint modelLoc = terrain_.location("uModel");
  for (size_t i = 0; i < ranges.size(); ++i) {
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, models[i].m);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(ranges[i].second / 4 * 6), GL_UNSIGNED_INT,
                   reinterpret_cast<const void*>(ranges[i].first / 4 * 6 * sizeof(uint32_t)));
    ++stats_.drawCalls;
  }
  glBindVertexArray(0);
  glEnable(GL_CULL_FACE);
  terrain_.set("uEntityLight", 0.0f);
  terrain_.set("uModel", mat4::identity());
}

void Renderer::drawHeldItem(const FrameInput& in, const mat4& viewProj, const vec3& fwd, const vec3& right,
                            const vec3& up) {
  if (!isValidItem(in.heldItem)) return;
  const ItemInfo info = itemInfo(in.heldItem);
  std::vector<TerrainVertex> verts;
  bool isFlat = false;
  if (info.isBlock && blockInfo(info.block).shape != Shape::Cross) {
    verts = blockModel(info.block);
  } else {
    buildFlatModel(info.isBlock ? blockInfo(info.block).tex[0] : info.icon, verts);
    isFlat = true;
  }
  for (TerrainVertex& v : verts) {
    v.sky = in.heldSky;
    v.blk = in.heldBlk;
    v.anim = 0;
  }
  ensureIndexCapacity(verts.size() / 4 + 1);
  glBindBuffer(GL_ARRAY_BUFFER, dynVbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(TerrainVertex)), verts.data(),
               GL_STREAM_DRAW);
  // Camera space -> camera relative world space.
  mat4 basis = mat4::identity();
  basis.at(0, 0) = right.x; basis.at(1, 0) = right.y; basis.at(2, 0) = right.z;
  basis.at(0, 1) = up.x;    basis.at(1, 1) = up.y;    basis.at(2, 1) = up.z;
  basis.at(0, 2) = -fwd.x;  basis.at(1, 2) = -fwd.y;  basis.at(2, 2) = -fwd.z;
  const float s = in.heldSwing;
  const float swing = std::sin(s * kPi);
  const float bobX = std::sin(in.heldBob) * 0.018f, bobY = std::fabs(std::cos(in.heldBob)) * 0.022f;
  mat4 local = translation(vec3(0.58f + bobX - swing * 0.18f, -0.60f + bobY + swing * 0.12f, -0.95f - swing * 0.12f)) *
               rotation(vec3(1, 0, 0), -swing * 0.9f);
  if (isFlat)
    local = local * rotation(vec3(0, 1, 0), -0.35f) * scaling(vec3(0.55f)) * translation(vec3(-0.5f, -0.2f, -0.5f));
  else
    local = local * rotation(vec3(0, 1, 0), 0.75f) * scaling(vec3(0.42f)) * translation(vec3(-0.5f, -0.5f, -0.5f));
  const mat4 model = basis * local;
  terrain_.use();
  terrain_.set("uViewProj", viewProj);
  terrain_.set("uAlphaTest", 0.5f);
  terrain_.set("uEntityLight", 1.0f);
  terrain_.set("uModel", model);
  bindTerrainTextures();
  glDepthRange(0.0, 0.02);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(dynVao_);
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(verts.size() / 4 * 6), GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);
  glEnable(GL_CULL_FACE);
  glDepthRange(0.0, 1.0);
  terrain_.set("uEntityLight", 0.0f);
  terrain_.set("uModel", mat4::identity());
}

void Renderer::drawSelection(const FrameInput& in, const mat4& viewProj) {
  if (!in.hasSelection) return;
  const BlockInfo& info = blockInfo(in.selectionBlock);
  const vec3 base = (dvec3(in.selection.x, in.selection.y, in.selection.z) - in.cameraPos).toFloat();
  const float e = 0.003f;
  const vec3 mn = base + vec3(info.selMin[0] - e, info.selMin[1] - e, info.selMin[2] - e);
  const vec3 mx = base + vec3(info.selMax[0] + e, info.selMax[1] + e, info.selMax[2] + e);
  const vec3 c[8] = {{mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z},
                     {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}};
  const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  std::vector<LineVertex> lines;
  for (const auto& ed : edges) {
    lines.push_back({c[ed[0]].x, c[ed[0]].y, c[ed[0]].z});
    lines.push_back({c[ed[1]].x, c[ed[1]].y, c[ed[1]].z});
  }
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  lines_.use();
  lines_.set("uViewProj", viewProj);
  lines_.set("uColor", vec4(0.02f, 0.02f, 0.02f, 0.65f));
  glBindVertexArray(lineVao_);
  glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(lines.size() * sizeof(LineVertex)), lines.data(),
               GL_STREAM_DRAW);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size()));
  glBindVertexArray(0);

  // Crack overlay on full blocks being mined.
  if (in.breakStage >= 0 && info.shape == Shape::Cube) {
    std::vector<TerrainVertex> verts = blockModel(block::Stone);
    const uint8_t l = 255;
    for (TerrainVertex& v : verts) {
      v.tex = static_cast<uint16_t>(tex::Crack0 + std::min(in.breakStage, 9));
      v.flags = 0;
      v.sky = l;
      v.blk = 0;
    }
    glBindBuffer(GL_ARRAY_BUFFER, dynVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(TerrainVertex)), verts.data(),
                 GL_STREAM_DRAW);
    setTerrainUniforms(glass_, in, viewProj);
    bindTerrainTextures();
    glass_.set("uModel", translation(base + vec3(0.5f)) * scaling(vec3(1.004f)) * translation(vec3(-0.5f)));
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -2.0f);
    glBindVertexArray(dynVao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(verts.size() / 4 * 6), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glDisable(GL_POLYGON_OFFSET_FILL);
    terrain_.use();
  }
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void Renderer::drawParticles(const FrameInput& in, const mat4& viewProj, const vec3& right, const vec3& up) {
  if (in.particles.empty()) return;
  std::vector<ParticleVertex> v;
  v.reserve(in.particles.size() * 6);
  for (const ParticleDraw& p : in.particles) {
    const vec3 c = (p.pos - in.cameraPos).toFloat();
    const vec3 r = right * p.size, u = up * p.size;
    const vec3 q[4] = {c - r - u, c + r - u, c + r + u, c - r + u};
    const float us[4] = {p.u0, p.u0 + p.uvSize, p.u0 + p.uvSize, p.u0};
    const float vs[4] = {p.v0 + p.uvSize, p.v0 + p.uvSize, p.v0, p.v0};
    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int i : order)
      v.push_back({q[i].x, q[i].y, q[i].z, us[i], vs[i], static_cast<float>(p.tex), p.sky / 255.0f, p.blk / 255.0f,
                   p.emissive});
  }
  particles_.use();
  particles_.set("uViewProj", viewProj);
  particles_.set("uLightColor", daylight_.lightColor);
  particles_.set("uAmbient", daylight_.ambientColor);
  particles_.set("uBlockColor", vec3(1.0f, 0.62f, 0.32f));
  particles_.set("uAlbedo", 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, albedoArray_);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(particleVao_);
  glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(ParticleVertex)), v.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(v.size()));
  glBindVertexArray(0);
  glEnable(GL_CULL_FACE);
}

void Renderer::postProcess(const FrameInput& in, const mat4& invViewProj) {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  const vec2 exposureRange(0.05f, 4.5f);

  // Clouds and under water fog.
  glBindFramebuffer(GL_FRAMEBUFFER, composited_.fbo);
  glViewport(0, 0, width_, height_);
  composite_.use();
  composite_.set("uScene", 0);
  composite_.set("uDepth", 1);
  composite_.set("uSkyLut", 2);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene_.color);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, scene_.depth);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, skyLutTarget_.color);
  glActiveTexture(GL_TEXTURE0);
  composite_.set("uInvViewProj", invViewProj);
  composite_.set("uCameraPos", in.cameraPos.toFloat());
  composite_.set("uLightDir", daylight_.lightDir);
  composite_.set("uLightColor", daylight_.lightColor);
  composite_.set("uAmbient", daylight_.ambientColor);
  composite_.set("uCloudSteps", settings_.cloudQuality == 0 ? 0 : (settings_.cloudQuality == 1 ? 10 : 20));
  composite_.set("uUnderwater", in.underwater ? 1 : 0);
  composite_.set("uLavaView", in.inLava ? 1.0f : 0.0f);
  composite_.set("uCameraSky", in.cameraSky);
  composite_.set("uTime", in.time);
  composite_.set("uCloudOffset", vec2(in.time * 6.0f, in.time * 2.5f));
  composite_.set("uCloudCoverage", settings_.cloudQuality > 0 ? 0.46f + 0.08f * std::sin(in.time * 0.004f) : 0.0f);
  const float fadeEnd = (static_cast<float>(in.renderDistance) - 0.6f) * 16.0f;
  composite_.set("uFadeStart", settings_.fog ? fadeEnd * 0.82f : 1e6f);
  composite_.set("uFadeEnd", settings_.fog ? fadeEnd : 2e6f);
  fullscreen();

  // Average log luminance -> slowly adapting exposure.
  glBindFramebuffer(GL_FRAMEBUFFER, lumTarget_.fbo);
  glViewport(0, 0, kLumSize, kLumSize);
  luminance_.use();
  luminance_.set("uScene", 0);
  glBindTexture(GL_TEXTURE_2D, composited_.color);
  fullscreen();
  glBindTexture(GL_TEXTURE_2D, lumTarget_.color);
  glGenerateMipmap(GL_TEXTURE_2D);
  const int next = 1 - adaptIndex_;
  glBindFramebuffer(GL_FRAMEBUFFER, adaptTargets_[next].fbo);
  glViewport(0, 0, 1, 1);
  adapt_.use();
  adapt_.set("uLum", 0);
  adapt_.set("uPrev", 1);
  adapt_.set("uLod", std::log2(static_cast<float>(kLumSize)));
  adapt_.set("uRate", adaptPrimed_ ? 1.0f - std::exp(-std::max(in.dt, 0.0f) * 1.4f) : 1.0f);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, lumTarget_.color);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, adaptTargets_[adaptIndex_].color);
  glActiveTexture(GL_TEXTURE0);
  fullscreen();
  adaptIndex_ = next;
  adaptPrimed_ = true;
  const GLuint adaptTex = adaptTargets_[adaptIndex_].color;

  // Bloom: thresholded downsample chain followed by additive upsampling.
  const bool bloom = settings_.bloom && !bloom_.empty();
  if (bloom) {
    bloomDown_.use();
    bloomDown_.set("uSrc", 0);
    bloomDown_.set("uAdapt", 1);
    bloomDown_.set("uExposureRange", exposureRange);
    bloomDown_.set("uExposureBias", settings_.brightness);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, adaptTex);
    glActiveTexture(GL_TEXTURE0);
    for (size_t i = 0; i < bloom_.size(); ++i) {
      glBindFramebuffer(GL_FRAMEBUFFER, bloom_[i].fbo);
      glViewport(0, 0, bloom_[i].w, bloom_[i].h);
      const bool first = i == 0;
      const int sw = first ? width_ : bloom_[i - 1].w, sh = first ? height_ : bloom_[i - 1].h;
      glBindTexture(GL_TEXTURE_2D, first ? composited_.color : bloom_[i - 1].color);
      bloomDown_.set("uTexel", vec2(1.0f / static_cast<float>(sw), 1.0f / static_cast<float>(sh)));
      bloomDown_.set("uPrefilter", first ? 1 : 0);
      bloomDown_.set("uThreshold", 0.9f);
      fullscreen();
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    bloomUp_.use();
    bloomUp_.set("uSrc", 0);
    for (int i = static_cast<int>(bloom_.size()) - 2; i >= 0; --i) {
      const gl::Target& src = bloom_[static_cast<size_t>(i + 1)];
      glBindFramebuffer(GL_FRAMEBUFFER, bloom_[static_cast<size_t>(i)].fbo);
      glViewport(0, 0, bloom_[static_cast<size_t>(i)].w, bloom_[static_cast<size_t>(i)].h);
      glBindTexture(GL_TEXTURE_2D, src.color);
      bloomUp_.set("uTexel", vec2(1.0f / static_cast<float>(src.w), 1.0f / static_cast<float>(src.h)));
      fullscreen();
    }
    glDisable(GL_BLEND);
  }

  // Tone mapping to LDR.
  glBindFramebuffer(GL_FRAMEBUFFER, ldr_.fbo);
  glViewport(0, 0, width_, height_);
  tonemap_.use();
  tonemap_.set("uScene", 0);
  tonemap_.set("uBloom", 1);
  tonemap_.set("uAdapt", 2);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, composited_.color);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, bloom ? bloom_[0].color : composited_.color);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, adaptTex);
  glActiveTexture(GL_TEXTURE0);
  tonemap_.set("uBloomStrength", bloom ? 0.07f : 0.0f);
  tonemap_.set("uExposureBias", settings_.brightness);
  tonemap_.set("uExposureRange", exposureRange);
  tonemap_.set("uVignette", 0.28f);
  vec4 overlay(0.0f);
  if (in.damageFlash > 0.0f) overlay = vec4(0.45f, 0.02f, 0.0f, std::min(0.45f, in.damageFlash * 0.45f));
  tonemap_.set("uOverlay", overlay);
  fullscreen();

  // Anti-aliasing into the window (this also scales up a reduced resolution).
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, outWidth_, outHeight_);
  fxaa_.use();
  fxaa_.set("uSrc", 0);
  fxaa_.set("uTexel", vec2(1.0f / static_cast<float>(width_), 1.0f / static_cast<float>(height_)));
  fxaa_.set("uEnabled", settings_.antialiasing ? 1 : 0);
  glBindTexture(GL_TEXTURE_2D, ldr_.color);
  fullscreen();
}

bool Renderer::saveScreenshot(const std::string& path) {
  // RGBA is the one read format every GL and GLES implementation supports.
  const size_t pixels = static_cast<size_t>(outWidth_) * static_cast<size_t>(outHeight_);
  std::vector<uint8_t> rgba(pixels * 4);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glReadBuffer(GL_BACK);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);
  glReadPixels(0, 0, outWidth_, outHeight_, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
  std::vector<uint8_t> px(pixels * 3);
  for (size_t i = 0; i < pixels; ++i)
    for (size_t c = 0; c < 3; ++c) px[i * 3 + c] = rgba[i * 4 + c];
  const bool ok = writePng(path, outWidth_, outHeight_, 3, px.data(), true);
  if (ok)
    LOG_INFO("saved screenshot %s", path.c_str());
  else
    LOG_ERROR("could not write screenshot %s", path.c_str());
  return ok;
}

}  // namespace strata
