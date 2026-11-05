#pragma once

#include "gl/gl.hpp"

namespace strata::gl {

GLuint makeTexture2D(int w, int h, GLenum internalFormat, GLenum format, GLenum type, GLenum filter, GLenum wrap,
                     const void* data = nullptr);

// Off-screen colour (+ optional depth texture) target.
struct Target {
  GLuint fbo = 0;
  GLuint color = 0;
  GLuint depth = 0;
  int w = 0, h = 0;
  void destroy();
};

Target makeTarget(int w, int h, GLenum colorFormat, bool withDepth, GLenum filter = GL_LINEAR);

// Reports incomplete framebuffers; returns false on failure.
bool checkFramebuffer(const char* name);
// Whether the context can render into a texture of this format (WebGL 2
// without EXT_color_buffer_float cannot render to 32 bit float formats).
bool isColorRenderable(GLenum internalFormat);
// Logs and clears pending GL errors; returns true when there were none.
bool checkErrors(const char* where);

}  // namespace strata::gl
