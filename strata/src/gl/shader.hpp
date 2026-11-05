#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "core/math.hpp"
#include "gl/gl.hpp"
#include "render/shaders.hpp"

namespace strata {

enum class GlslDialect {
  Desktop,  // "#version 330 core" (OpenGL 3.3)
  Es,       // "#version 300 es" (OpenGL ES 3.0 / WebGL 2)
};

// The dialect of the current build's GL context.
GlslDialect nativeGlslDialect();

// The source strings of one stage of `program`, in the order they are passed
// to glShaderSource. Each part restarts the line count ("#line 1 n") so a
// compiler message "n:line" points straight at the right source.
std::vector<std::string> assembleShader(const shaders::Program& program, bool fragment, GlslDialect dialect);

class Shader {
 public:
  Shader() = default;
  ~Shader();
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  bool build(const shaders::Program& program);
  void destroy();

  void use() const { glUseProgram(program_); }
  GLuint id() const { return program_; }
  bool valid() const { return program_ != 0; }

  GLint location(const char* name);
  void set(const char* name, int v) { glUniform1i(location(name), v); }
  void set(const char* name, float v) { glUniform1f(location(name), v); }
  void set(const char* name, vec2 v) { glUniform2f(location(name), v.x, v.y); }
  void set(const char* name, vec3 v) { glUniform3f(location(name), v.x, v.y, v.z); }
  void set(const char* name, vec4 v) { glUniform4f(location(name), v.x, v.y, v.z, v.w); }
  void set(const char* name, const mat4& m) { glUniformMatrix4fv(location(name), 1, GL_FALSE, m.m); }
  void setMatrices(const char* name, const mat4* m, int count) {
    glUniformMatrix4fv(location(name), count, GL_FALSE, m[0].m);
  }

 private:
  GLuint program_ = 0;
  std::string name_;
  std::unordered_map<std::string, GLint> cache_;
};

}  // namespace strata
