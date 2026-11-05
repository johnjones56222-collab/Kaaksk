// OpenGL access for the two targets the engine supports.
//
// Desktop: a minimal OpenGL 3.3 core loader. Only the entry points and
// constants the engine uses are declared. Function pointers are fetched at
// runtime through the windowing library, so the game never links against an
// OpenGL library directly. The wrappers are C++ functions (mangled names), so
// they cannot collide with symbols exported by a system libGL. Never include a
// system GL header in the same translation unit.
//
// Web / mobile (STRATA_GLES, set automatically for Emscripten): OpenGL ES 3.0
// as exposed by WebGL 2. The system header is used directly; the few desktop
// calls the engine makes are mapped to their ES equivalents below.
#pragma once

#include <cstddef>

#if defined(__EMSCRIPTEN__) && !defined(STRATA_GLES)
#define STRATA_GLES 1
#endif

#if defined(STRATA_GLES)

#include <GLES3/gl3.h>

#define STRATA_GLAPI GL_APIENTRY
using GLdouble = double;
using GLDEBUGPROC = void(STRATA_GLAPI*)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                        const GLchar* message, const void* userParam);

inline void glClearDepth(GLdouble depth) { glClearDepthf(static_cast<GLfloat>(depth)); }
inline void glDepthRange(GLdouble zNear, GLdouble zFar) {
  glDepthRangef(static_cast<GLfloat>(zNear), static_cast<GLfloat>(zFar));
}
// ES only has the multi-buffer form (GL_NONE or GL_BACK / GL_COLOR_ATTACHMENT0).
inline void glDrawBuffer(GLenum buffer) { glDrawBuffers(1, &buffer); }

// EXT_texture_filter_anisotropic (same values as the GL 4.6 core names).
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF

#else  // desktop OpenGL 3.3 core

#if defined(_WIN32) && !defined(__CYGWIN__)
#define STRATA_GLAPI __stdcall
#else
#define STRATA_GLAPI
#endif

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLvoid = void;
using GLbyte = signed char;
using GLubyte = unsigned char;
using GLshort = short;
using GLushort = unsigned short;
using GLint = int;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLdouble = double;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

using GLDEBUGPROC = void(STRATA_GLAPI*)(GLenum source, GLenum type, GLuint id, GLenum severity,
                                        GLsizei length, const GLchar* message, const void* userParam);

// X(return type, name, (parameters), (arguments))
#define STRATA_GL_FUNCTIONS(X)                                                                       \
  X(void, glActiveTexture, (GLenum texture), (texture))                                              \
  X(void, glAttachShader, (GLuint program, GLuint shader), (program, shader))                        \
  X(void, glBindAttribLocation, (GLuint program, GLuint index, const GLchar* name),                  \
    (program, index, name))                                                                          \
  X(void, glBindBuffer, (GLenum target, GLuint buffer), (target, buffer))                            \
  X(void, glBindFragDataLocation, (GLuint program, GLuint color, const GLchar* name),                \
    (program, color, name))                                                                          \
  X(void, glBindFramebuffer, (GLenum target, GLuint framebuffer), (target, framebuffer))             \
  X(void, glBindRenderbuffer, (GLenum target, GLuint renderbuffer), (target, renderbuffer))          \
  X(void, glBindTexture, (GLenum target, GLuint texture), (target, texture))                         \
  X(void, glBindVertexArray, (GLuint array), (array))                                                \
  X(void, glBlendEquation, (GLenum mode), (mode))                                                    \
  X(void, glBlendFunc, (GLenum sfactor, GLenum dfactor), (sfactor, dfactor))                         \
  X(void, glBlendFuncSeparate,                                                                       \
    (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha),                \
    (sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha))                                            \
  X(void, glBlitFramebuffer,                                                                         \
    (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1,      \
     GLint dstY1, GLbitfield mask, GLenum filter),                                                   \
    (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter))                          \
  X(void, glBufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage),            \
    (target, size, data, usage))                                                                     \
  X(void, glBufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, const void* data),      \
    (target, offset, size, data))                                                                    \
  X(GLenum, glCheckFramebufferStatus, (GLenum target), (target))                                     \
  X(void, glClear, (GLbitfield mask), (mask))                                                        \
  X(void, glClearBufferfv, (GLenum buffer, GLint drawbuffer, const GLfloat* value),                  \
    (buffer, drawbuffer, value))                                                                     \
  X(void, glClearColor, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha),                   \
    (red, green, blue, alpha))                                                                       \
  X(void, glClearDepth, (GLdouble depth), (depth))                                                   \
  X(void, glColorMask, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha),            \
    (red, green, blue, alpha))                                                                       \
  X(void, glCompileShader, (GLuint shader), (shader))                                                \
  X(GLuint, glCreateProgram, (void), ())                                                             \
  X(GLuint, glCreateShader, (GLenum type), (type))                                                   \
  X(void, glCullFace, (GLenum mode), (mode))                                                         \
  X(void, glDeleteBuffers, (GLsizei n, const GLuint* buffers), (n, buffers))                         \
  X(void, glDeleteFramebuffers, (GLsizei n, const GLuint* framebuffers), (n, framebuffers))          \
  X(void, glDeleteProgram, (GLuint program), (program))                                              \
  X(void, glDeleteRenderbuffers, (GLsizei n, const GLuint* renderbuffers), (n, renderbuffers))       \
  X(void, glDeleteShader, (GLuint shader), (shader))                                                 \
  X(void, glDeleteTextures, (GLsizei n, const GLuint* textures), (n, textures))                      \
  X(void, glDeleteVertexArrays, (GLsizei n, const GLuint* arrays), (n, arrays))                      \
  X(void, glDepthFunc, (GLenum func), (func))                                                        \
  X(void, glDepthMask, (GLboolean flag), (flag))                                                     \
  X(void, glDepthRange, (GLdouble n, GLdouble f), (n, f))                                            \
  X(void, glDetachShader, (GLuint program, GLuint shader), (program, shader))                        \
  X(void, glDisable, (GLenum cap), (cap))                                                            \
  X(void, glDisableVertexAttribArray, (GLuint index), (index))                                       \
  X(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count), (mode, first, count))             \
  X(void, glDrawBuffer, (GLenum buf), (buf))                                                         \
  X(void, glDrawBuffers, (GLsizei n, const GLenum* bufs), (n, bufs))                                 \
  X(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const void* indices),            \
    (mode, count, type, indices))                                                                    \
  X(void, glEnable, (GLenum cap), (cap))                                                             \
  X(void, glEnableVertexAttribArray, (GLuint index), (index))                                        \
  X(void, glFinish, (void), ())                                                                      \
  X(void, glFlush, (void), ())                                                                       \
  X(void, glFramebufferRenderbuffer,                                                                 \
    (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer),              \
    (target, attachment, renderbuffertarget, renderbuffer))                                          \
  X(void, glFramebufferTexture2D,                                                                    \
    (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level),               \
    (target, attachment, textarget, texture, level))                                                 \
  X(void, glFramebufferTextureLayer,                                                                 \
    (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer),                    \
    (target, attachment, texture, level, layer))                                                     \
  X(void, glFrontFace, (GLenum mode), (mode))                                                        \
  X(void, glGenBuffers, (GLsizei n, GLuint* buffers), (n, buffers))                                  \
  X(void, glGenFramebuffers, (GLsizei n, GLuint* framebuffers), (n, framebuffers))                   \
  X(void, glGenRenderbuffers, (GLsizei n, GLuint* renderbuffers), (n, renderbuffers))                \
  X(void, glGenTextures, (GLsizei n, GLuint* textures), (n, textures))                               \
  X(void, glGenVertexArrays, (GLsizei n, GLuint* arrays), (n, arrays))                               \
  X(void, glGenerateMipmap, (GLenum target), (target))                                               \
  X(GLenum, glGetError, (void), ())                                                                  \
  X(void, glGetFloatv, (GLenum pname, GLfloat* data), (pname, data))                                 \
  X(void, glGetIntegerv, (GLenum pname, GLint* data), (pname, data))                                 \
  X(void, glGetProgramInfoLog,                                                                       \
    (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog),                             \
    (program, bufSize, length, infoLog))                                                             \
  X(void, glGetProgramiv, (GLuint program, GLenum pname, GLint* params), (program, pname, params))   \
  X(void, glGetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog),    \
    (shader, bufSize, length, infoLog))                                                              \
  X(void, glGetShaderiv, (GLuint shader, GLenum pname, GLint* params), (shader, pname, params))      \
  X(const GLubyte*, glGetString, (GLenum name), (name))                                              \
  X(const GLubyte*, glGetStringi, (GLenum name, GLuint index), (name, index))                        \
  X(GLint, glGetUniformLocation, (GLuint program, const GLchar* name), (program, name))              \
  X(void, glHint, (GLenum target, GLenum mode), (target, mode))                                      \
  X(void, glLineWidth, (GLfloat width), (width))                                                     \
  X(void, glLinkProgram, (GLuint program), (program))                                                \
  X(void, glPixelStorei, (GLenum pname, GLint param), (pname, param))                                \
  X(void, glPolygonOffset, (GLfloat factor, GLfloat units), (factor, units))                         \
  X(void, glReadBuffer, (GLenum src), (src))                                                         \
  X(void, glReadPixels,                                                                              \
    (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels),     \
    (x, y, width, height, format, type, pixels))                                                     \
  X(void, glRenderbufferStorage,                                                                     \
    (GLenum target, GLenum internalformat, GLsizei width, GLsizei height),                           \
    (target, internalformat, width, height))                                                         \
  X(void, glScissor, (GLint x, GLint y, GLsizei width, GLsizei height), (x, y, width, height))       \
  X(void, glShaderSource,                                                                            \
    (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length),                \
    (shader, count, string, length))                                                                 \
  X(void, glTexImage2D,                                                                              \
    (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,   \
     GLenum format, GLenum type, const void* pixels),                                                \
    (target, level, internalformat, width, height, border, format, type, pixels))                    \
  X(void, glTexImage3D,                                                                              \
    (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,                \
     GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels),                   \
    (target, level, internalformat, width, height, depth, border, format, type, pixels))             \
  X(void, glTexParameterf, (GLenum target, GLenum pname, GLfloat param), (target, pname, param))     \
  X(void, glTexParameterfv, (GLenum target, GLenum pname, const GLfloat* params),                    \
    (target, pname, params))                                                                         \
  X(void, glTexParameteri, (GLenum target, GLenum pname, GLint param), (target, pname, param))       \
  X(void, glTexSubImage2D,                                                                           \
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,        \
     GLenum format, GLenum type, const void* pixels),                                                \
    (target, level, xoffset, yoffset, width, height, format, type, pixels))                          \
  X(void, glTexSubImage3D,                                                                           \
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,         \
     GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels),                 \
    (target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels))          \
  X(void, glUniform1f, (GLint location, GLfloat v0), (location, v0))                                 \
  X(void, glUniform1fv, (GLint location, GLsizei count, const GLfloat* value),                       \
    (location, count, value))                                                                        \
  X(void, glUniform1i, (GLint location, GLint v0), (location, v0))                                   \
  X(void, glUniform2f, (GLint location, GLfloat v0, GLfloat v1), (location, v0, v1))                 \
  X(void, glUniform3f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2),                         \
    (location, v0, v1, v2))                                                                          \
  X(void, glUniform3fv, (GLint location, GLsizei count, const GLfloat* value),                       \
    (location, count, value))                                                                        \
  X(void, glUniform4f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3),             \
    (location, v0, v1, v2, v3))                                                                      \
  X(void, glUniform4fv, (GLint location, GLsizei count, const GLfloat* value),                       \
    (location, count, value))                                                                        \
  X(void, glUniformMatrix4fv,                                                                        \
    (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value),                      \
    (location, count, transpose, value))                                                             \
  X(void, glUseProgram, (GLuint program), (program))                                                 \
  X(void, glVertexAttribDivisor, (GLuint index, GLuint divisor), (index, divisor))                   \
  X(void, glVertexAttribIPointer,                                                                    \
    (GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer),                    \
    (index, size, type, stride, pointer))                                                            \
  X(void, glVertexAttribPointer,                                                                     \
    (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,                    \
     const void* pointer),                                                                           \
    (index, size, type, normalized, stride, pointer))                                                \
  X(void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height), (x, y, width, height))

// Entry points that may be missing on a 3.3 context. They are loaded when
// available and left null otherwise; check before calling.
#define STRATA_GL_OPTIONAL_FUNCTIONS(X)                                                              \
  X(void, glDebugMessageCallback, (GLDEBUGPROC callback, const void* userParam),                     \
    (callback, userParam))

#define STRATA_GL_DECLARE(ret, name, params, args)                                                   \
  using PFN_##name = ret(STRATA_GLAPI*) params;                                                      \
  extern PFN_##name strata_ptr_##name;                                                               \
  inline ret name params { return strata_ptr_##name args; }
STRATA_GL_FUNCTIONS(STRATA_GL_DECLARE)
STRATA_GL_OPTIONAL_FUNCTIONS(STRATA_GL_DECLARE)
#undef STRATA_GL_DECLARE


// ---------------------------------------------------------------------------
// Constants (values from the Khronos registry; verified by tools/check_gl_header.py)
// ---------------------------------------------------------------------------
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NONE 0
#define GL_ZERO 0
#define GL_ONE 1
#define GL_NO_ERROR 0

#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_COLOR_BUFFER_BIT 0x00004000

#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006

#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207

#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_FUNC_ADD 0x8006
#define GL_MIN 0x8007
#define GL_MAX 0x8008

#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_CW 0x0900
#define GL_CCW 0x0901

#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_DEPTH_CLAMP 0x864F
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F

#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_OUT_OF_MEMORY 0x0505
#define GL_INVALID_FRAMEBUFFER_OPERATION 0x0506

#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_VIEWPORT 0x0BA2
#define GL_MAX_ARRAY_TEXTURE_LAYERS 0x88FF

#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B

#define GL_DEPTH_COMPONENT 0x1902
#define GL_RED 0x1903
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_RG 0x8227
#define GL_DEPTH_STENCIL 0x84F9

#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_NUM_EXTENSIONS 0x821D
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C

#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_TEXTURE_BORDER_COLOR 0x1004
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_MIRRORED_REPEAT 0x8370
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_LOD_BIAS 0x8501
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF

#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_TEXTURE4 0x84C4
#define GL_TEXTURE5 0x84C5
#define GL_TEXTURE6 0x84C6
#define GL_TEXTURE7 0x84C7

#define GL_R8 0x8229
#define GL_RG8 0x822B
#define GL_RGB8 0x8051
#define GL_RGBA8 0x8058
#define GL_SRGB8 0x8C41
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_R16F 0x822D
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RGB16F 0x881B
#define GL_RGBA16F 0x881A
#define GL_RGBA32F 0x8814
#define GL_R11F_G11F_B10F 0x8C3A
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH24_STENCIL8 0x88F0

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84

#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR 0x1800
#define GL_DEPTH 0x1801

#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B

#endif  // STRATA_GLES

namespace strata::gl {
using GenericProc = void (*)();
using ProcLoader = GenericProc (*)(const char* name);
// Loads every required entry point. Returns false (and fills `missing` with the
// first missing name) when the driver lacks one.
bool loadFunctions(ProcLoader loader, const char** missing);
bool hasDebugOutput();
}  // namespace strata::gl
