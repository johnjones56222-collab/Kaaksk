#include "gl/gl_util.hpp"

#include "core/log.hpp"

namespace strata::gl {

namespace {
const char* errorName(GLenum e) {
  switch (e) {
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    default: return "unknown GL error";
  }
}
}  // namespace

GLuint makeTexture2D(int w, int h, GLenum internalFormat, GLenum format, GLenum type, GLenum filter, GLenum wrap,
                     const void* data) {
  GLuint t = 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), w, h, 0, format, type, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(filter));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(filter == GL_NEAREST ? GL_NEAREST : GL_LINEAR));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrap));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrap));
  return t;
}

void Target::destroy() {
  if (fbo) glDeleteFramebuffers(1, &fbo);
  if (color) glDeleteTextures(1, &color);
  if (depth) glDeleteTextures(1, &depth);
  fbo = color = depth = 0;
  w = h = 0;
}

Target makeTarget(int w, int h, GLenum colorFormat, bool withDepth, GLenum filter) {
  Target t;
  t.w = w;
  t.h = h;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  if (colorFormat == GL_RGBA16F || colorFormat == GL_RGB16F || colorFormat == GL_R11F_G11F_B10F ||
      colorFormat == GL_RGBA32F) {
    type = GL_FLOAT;
  } else if (colorFormat == GL_R16F || colorFormat == GL_R32F) {
    format = GL_RED;
    type = GL_FLOAT;
  } else if (colorFormat == GL_RG16F) {
    format = GL_RG;
    type = GL_FLOAT;
  }
  t.color = makeTexture2D(w, h, colorFormat, format, type, filter, GL_CLAMP_TO_EDGE);
  if (withDepth) {
    glGenTextures(1, &t.depth);
    glBindTexture(GL_TEXTURE_2D, t.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  glGenFramebuffers(1, &t.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.color, 0);
  if (withDepth) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, t.depth, 0);
  checkFramebuffer("render target");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return t;
}

bool isColorRenderable(GLenum internalFormat) {
  Target t;
  GLenum format = GL_RGBA;
  if (internalFormat == GL_R16F || internalFormat == GL_R32F || internalFormat == GL_R8) format = GL_RED;
  else if (internalFormat == GL_RG16F) format = GL_RG;
  const GLenum type = internalFormat == GL_RGBA8 || internalFormat == GL_R8 ? GL_UNSIGNED_BYTE : GL_FLOAT;
  t.color = makeTexture2D(1, 1, internalFormat, format, type, GL_NEAREST, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &t.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.color, 0);
  const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  t.destroy();
  while (glGetError() != GL_NO_ERROR) {
  }
  return ok;
}

bool checkFramebuffer(const char* name) {
  const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    LOG_ERROR("framebuffer '%s' incomplete (0x%x)", name, status);
    return false;
  }
  return true;
}

bool checkErrors(const char* where) {
  bool ok = true;
  for (int i = 0; i < 16; ++i) {
    const GLenum e = glGetError();
    if (e == GL_NO_ERROR) break;
    LOG_ERROR("OpenGL error %s (0x%x) at %s", errorName(e), e, where);
    ok = false;
  }
  return ok;
}

}  // namespace strata::gl
