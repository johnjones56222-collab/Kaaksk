// THE 1982 CRANE - thin OpenGL ES 3 / WebGL 2 helpers (web build only)
#pragma once
#include "core.h"
#include <GLES3/gl3.h>
#include <map>

struct Program {
    GLuint id = 0;
    std::map<std::string, GLint> locs;
    GLint loc(const char* name) {
        auto it = locs.find(name);
        if (it != locs.end()) return it->second;
        GLint l = glGetUniformLocation(id, name);
        locs[name] = l;
        return l;
    }
    void use() const { glUseProgram(id); }
    void i1(const char* n, int v) { glUniform1i(loc(n), v); }
    void f1(const char* n, float v) { glUniform1f(loc(n), v); }
    void f2(const char* n, float a, float b) { glUniform2f(loc(n), a, b); }
    void v3(const char* n, const Vec3& v) { glUniform3f(loc(n), v.x, v.y, v.z); }
    void f4(const char* n, float a, float b, float c, float d) { glUniform4f(loc(n), a, b, c, d); }
    void m4(const char* n, const Mat4& m) { glUniformMatrix4fv(loc(n), 1, GL_FALSE, m.m); }
    void m3(const char* n, const float* m9) { glUniformMatrix3fv(loc(n), 1, GL_FALSE, m9); }
    void f1v(const char* n, int count, const float* v) { glUniform1fv(loc(n), count, v); }
    void f4v(const char* n, int count, const float* v) { glUniform4fv(loc(n), count, v); }
};

static inline GLuint gfx_compile(GLenum type, const std::string& src, const char* label) {
    GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        printf("SHADER ERROR (%s %s):\n%s\n", label, type == GL_VERTEX_SHADER ? "vs" : "fs", log);
    }
    return s;
}

// `defines` is a block of "#define X\n" lines inserted after the version header.
static inline Program gfx_program(const char* vs, const char* fs, const char* defines, const char* label) {
    std::string head = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2DArray;\nprecision highp sampler2DShadow;\n";
    head += defines ? defines : "";
    GLuint v = gfx_compile(GL_VERTEX_SHADER, head + vs, label);
    GLuint f = gfx_compile(GL_FRAGMENT_SHADER, head + fs, label);
    Program p;
    p.id = glCreateProgram();
    glAttachShader(p.id, v);
    glAttachShader(p.id, f);
    glLinkProgram(p.id);
    GLint ok = 0;
    glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p.id, sizeof(log), nullptr, log);
        printf("LINK ERROR (%s):\n%s\n", label, log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

// upper-left 3x3 inverse-transpose of a model matrix, column major
static inline void gfx_normal_matrix(const Mat4& m, float* out9) {
    float a = m.m[0], b = m.m[4], c = m.m[8];
    float d = m.m[1], e = m.m[5], f = m.m[9];
    float g = m.m[2], h = m.m[6], i = m.m[10];
    float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    float det = a * A + b * B + c * C;
    if (std::fabs(det) < 1e-12f) det = 1.0f;
    float inv = 1.0f / det;
    // inverse = adj / det ; inverse transpose = cofactor matrix / det
    out9[0] = A * inv;                  out9[1] = -(b * i - c * h) * inv;   out9[2] = (b * f - c * e) * inv;
    out9[3] = B * inv;                  out9[4] = (a * i - c * g) * inv;    out9[5] = -(a * f - c * d) * inv;
    out9[6] = C * inv;                  out9[7] = -(a * h - b * g) * inv;   out9[8] = (a * e - b * d) * inv;
}
