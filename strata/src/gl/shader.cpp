#include "gl/shader.hpp"

#include <vector>

#include "core/log.hpp"

namespace strata {

namespace {
GLuint compile(GLenum type, const std::string& name, const std::vector<std::string>& sources) {
  std::vector<const char*> parts;
  for (const std::string& src : sources) parts.push_back(src.c_str());
  const GLuint s = glCreateShader(type);
  glShaderSource(s, static_cast<GLsizei>(parts.size()), parts.data(), nullptr);
  glCompileShader(s);
  GLint ok = GL_FALSE;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (ok != GL_TRUE) {
    GLint len = 0;
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<size_t>(len > 1 ? len : 1), '\0');
    glGetShaderInfoLog(s, len, nullptr, &log[0]);
    LOG_ERROR("%s shader '%s' failed to compile (source string:line):\n%s",
              type == GL_VERTEX_SHADER ? "vertex" : "fragment", name.c_str(), log.c_str());
    glDeleteShader(s);
    return 0;
  }
  return s;
}
}  // namespace

Shader::~Shader() { destroy(); }

void Shader::destroy() {
  if (program_) glDeleteProgram(program_);
  program_ = 0;
  cache_.clear();
}

GlslDialect nativeGlslDialect() {
#if defined(STRATA_GLES)
  return GlslDialect::Es;
#else
  return GlslDialect::Desktop;
#endif
}

std::vector<std::string> assembleShader(const shaders::Program& program, bool fragment, GlslDialect dialect) {
  // GLSL ES has no default precision for floats in fragment shaders, nor for
  // most sampler types in any stage; ES 3.0 guarantees highp everywhere.
  static const char* const kEsHeader =
      "#version 300 es\n"
      "precision highp float;\n"
      "precision highp int;\n"
      "precision highp sampler2D;\n"
      "precision highp sampler2DArray;\n"
      "precision highp sampler2DShadow;\n"
      "precision highp sampler2DArrayShadow;\n"
      "precision highp sampler3D;\n"
      "#define GLES 1\n";
  std::vector<std::string> parts;
  parts.push_back(std::string(dialect == GlslDialect::Es ? kEsHeader : "#version 330 core\n") + program.defines +
                  "\n");
  parts.push_back("#line 1 1\n");
  parts.push_back(shaders::kCommon);
  if (fragment && program.atmosphere) {
    parts.push_back("#line 1 3\n");
    parts.push_back(shaders::kAtmosphere);
  }
  parts.push_back(fragment ? "#line 1 4\n" : "#line 1 2\n");
  parts.push_back(fragment ? program.fragment : program.vertex);
  return parts;
}

bool Shader::build(const shaders::Program& program) {
  destroy();
  name_ = program.name;
  const GlslDialect dialect = nativeGlslDialect();
  const GLuint v = compile(GL_VERTEX_SHADER, name_, assembleShader(program, false, dialect));
  const GLuint f = compile(GL_FRAGMENT_SHADER, name_, assembleShader(program, true, dialect));
  if (!v || !f) {
    if (v) glDeleteShader(v);
    if (f) glDeleteShader(f);
    return false;
  }
  const GLuint p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  glDetachShader(p, v);
  glDetachShader(p, f);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok = GL_FALSE;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (ok != GL_TRUE) {
    GLint len = 0;
    glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<size_t>(len > 1 ? len : 1), '\0');
    glGetProgramInfoLog(p, len, nullptr, &log[0]);
    LOG_ERROR("program '%s' failed to link:\n%s", name_.c_str(), log.c_str());
    glDeleteProgram(p);
    return false;
  }
  program_ = p;
  return true;
}

GLint Shader::location(const char* name) {
  auto it = cache_.find(name);
  if (it != cache_.end()) return it->second;
  const GLint loc = glGetUniformLocation(program_, name);
  cache_.emplace(name, loc);
  return loc;
}

}  // namespace strata
