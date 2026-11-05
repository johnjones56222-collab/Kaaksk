// Small vector / matrix library. Matrices are column-major (OpenGL layout).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace strata {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kHalfPi = 1.57079632679489661923f;

inline float radians(float deg) { return deg * (kPi / 180.0f); }
inline float degrees(float rad) { return rad * (180.0f / kPi); }

template <class T>
constexpr T clampv(T v, T lo, T hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
inline float saturate(float v) { return clampv(v, 0.0f, 1.0f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float e0, float e1, float x) {
  float t = saturate((x - e0) / (e1 - e0));
  return t * t * (3.0f - 2.0f * t);
}

// Floor division / modulo that round toward negative infinity.
constexpr int floorDiv(int a, int b) {
  int q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return q;
}
constexpr int floorMod(int a, int b) {
  int r = a % b;
  if (r != 0 && ((r < 0) != (b < 0))) r += b;
  return r;
}
inline int fastFloor(float x) {
  int i = static_cast<int>(x);
  return (x < static_cast<float>(i)) ? i - 1 : i;
}
inline int fastFloor(double x) {
  int i = static_cast<int>(x);
  return (x < static_cast<double>(i)) ? i - 1 : i;
}

struct vec2 {
  float x = 0.0f, y = 0.0f;
  constexpr vec2() = default;
  constexpr vec2(float x_, float y_) : x(x_), y(y_) {}
  constexpr explicit vec2(float s) : x(s), y(s) {}
  constexpr vec2 operator+(vec2 o) const { return {x + o.x, y + o.y}; }
  constexpr vec2 operator-(vec2 o) const { return {x - o.x, y - o.y}; }
  constexpr vec2 operator*(float s) const { return {x * s, y * s}; }
  constexpr vec2 operator*(vec2 o) const { return {x * o.x, y * o.y}; }
  constexpr vec2 operator/(float s) const { return {x / s, y / s}; }
  vec2& operator+=(vec2 o) { x += o.x; y += o.y; return *this; }
  vec2& operator-=(vec2 o) { x -= o.x; y -= o.y; return *this; }
  vec2& operator*=(float s) { x *= s; y *= s; return *this; }
};

struct vec3 {
  float x = 0.0f, y = 0.0f, z = 0.0f;
  constexpr vec3() = default;
  constexpr vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
  constexpr explicit vec3(float s) : x(s), y(s), z(s) {}
  constexpr vec3 operator+(vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
  constexpr vec3 operator-(vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
  constexpr vec3 operator-() const { return {-x, -y, -z}; }
  constexpr vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  constexpr vec3 operator*(vec3 o) const { return {x * o.x, y * o.y, z * o.z}; }
  constexpr vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
  constexpr vec3 operator/(vec3 o) const { return {x / o.x, y / o.y, z / o.z}; }
  vec3& operator+=(vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
  vec3& operator-=(vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
  vec3& operator*=(vec3 o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
  float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
  float& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
};
constexpr vec3 operator*(float s, vec3 v) { return v * s; }

struct vec4 {
  float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
  constexpr vec4() = default;
  constexpr vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
  constexpr explicit vec4(float s) : x(s), y(s), z(s), w(s) {}
  constexpr vec4(vec3 v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
  constexpr vec3 xyz() const { return {x, y, z}; }
  constexpr vec4 operator+(vec4 o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
  constexpr vec4 operator-(vec4 o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
  constexpr vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
};

struct ivec3 {
  int x = 0, y = 0, z = 0;
  constexpr ivec3() = default;
  constexpr ivec3(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}
  constexpr ivec3 operator+(ivec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
  constexpr ivec3 operator-(ivec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
  constexpr ivec3 operator*(int s) const { return {x * s, y * s, z * s}; }
  constexpr bool operator==(ivec3 o) const { return x == o.x && y == o.y && z == o.z; }
  constexpr bool operator!=(ivec3 o) const { return !(*this == o); }
};

struct dvec3 {
  double x = 0.0, y = 0.0, z = 0.0;
  constexpr dvec3() = default;
  constexpr dvec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
  constexpr explicit dvec3(vec3 v) : x(v.x), y(v.y), z(v.z) {}
  constexpr dvec3 operator+(dvec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
  constexpr dvec3 operator-(dvec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
  constexpr dvec3 operator*(double s) const { return {x * s, y * s, z * s}; }
  dvec3& operator+=(dvec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
  constexpr vec3 toFloat() const {
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
  }
};

inline float dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }
inline float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float dot(vec4 a, vec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline vec3 cross(vec3 a, vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(vec2 v) { return std::sqrt(dot(v, v)); }
inline float length(vec3 v) { return std::sqrt(dot(v, v)); }
inline vec3 normalize(vec3 v) {
  float l = length(v);
  return l > 0.0f ? v / l : vec3(0.0f, 0.0f, 0.0f);
}
inline vec3 lerp(vec3 a, vec3 b, float t) { return a + (b - a) * t; }
inline vec3 vmin(vec3 a, vec3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline vec3 vmax(vec3 a, vec3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
inline vec3 vexp(vec3 v) { return {std::exp(v.x), std::exp(v.y), std::exp(v.z)}; }
inline double length(dvec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

struct mat4 {
  float m[16];  // column-major: element (row r, column c) is m[c * 4 + r]

  static constexpr mat4 identity() {
    return mat4{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  }
  float& at(int row, int col) { return m[col * 4 + row]; }
  float at(int row, int col) const { return m[col * 4 + row]; }
  const float* data() const { return m; }
};

inline mat4 operator*(const mat4& a, const mat4& b) {
  mat4 r{};
  for (int c = 0; c < 4; ++c)
    for (int rr = 0; rr < 4; ++rr) {
      float s = 0.0f;
      for (int k = 0; k < 4; ++k) s += a.at(rr, k) * b.at(k, c);
      r.at(rr, c) = s;
    }
  return r;
}

inline vec4 operator*(const mat4& a, vec4 v) {
  return {a.m[0] * v.x + a.m[4] * v.y + a.m[8] * v.z + a.m[12] * v.w,
          a.m[1] * v.x + a.m[5] * v.y + a.m[9] * v.z + a.m[13] * v.w,
          a.m[2] * v.x + a.m[6] * v.y + a.m[10] * v.z + a.m[14] * v.w,
          a.m[3] * v.x + a.m[7] * v.y + a.m[11] * v.z + a.m[15] * v.w};
}

inline mat4 translation(vec3 t) {
  mat4 r = mat4::identity();
  r.m[12] = t.x;
  r.m[13] = t.y;
  r.m[14] = t.z;
  return r;
}

inline mat4 scaling(vec3 s) {
  mat4 r = mat4::identity();
  r.m[0] = s.x;
  r.m[5] = s.y;
  r.m[10] = s.z;
  return r;
}

// Rotation around a unit axis (right-handed, radians).
inline mat4 rotation(vec3 axis, float angle) {
  vec3 a = normalize(axis);
  float c = std::cos(angle), s = std::sin(angle), t = 1.0f - c;
  mat4 r = mat4::identity();
  r.at(0, 0) = t * a.x * a.x + c;
  r.at(0, 1) = t * a.x * a.y - s * a.z;
  r.at(0, 2) = t * a.x * a.z + s * a.y;
  r.at(1, 0) = t * a.x * a.y + s * a.z;
  r.at(1, 1) = t * a.y * a.y + c;
  r.at(1, 2) = t * a.y * a.z - s * a.x;
  r.at(2, 0) = t * a.x * a.z - s * a.y;
  r.at(2, 1) = t * a.y * a.z + s * a.x;
  r.at(2, 2) = t * a.z * a.z + c;
  return r;
}

// OpenGL style perspective projection (clip z in [-w, w]).
inline mat4 perspective(float fovyRadians, float aspect, float zNear, float zFar) {
  float f = 1.0f / std::tan(fovyRadians * 0.5f);
  mat4 r{};
  r.at(0, 0) = f / aspect;
  r.at(1, 1) = f;
  r.at(2, 2) = (zFar + zNear) / (zNear - zFar);
  r.at(2, 3) = (2.0f * zFar * zNear) / (zNear - zFar);
  r.at(3, 2) = -1.0f;
  return r;
}

inline mat4 ortho(float l, float r, float b, float t, float n, float f) {
  mat4 o = mat4::identity();
  o.at(0, 0) = 2.0f / (r - l);
  o.at(1, 1) = 2.0f / (t - b);
  o.at(2, 2) = -2.0f / (f - n);
  o.at(0, 3) = -(r + l) / (r - l);
  o.at(1, 3) = -(t + b) / (t - b);
  o.at(2, 3) = -(f + n) / (f - n);
  return o;
}

inline mat4 lookAt(vec3 eye, vec3 center, vec3 up) {
  vec3 f = normalize(center - eye);
  vec3 s = normalize(cross(f, up));
  vec3 u = cross(s, f);
  mat4 r = mat4::identity();
  r.at(0, 0) = s.x;
  r.at(0, 1) = s.y;
  r.at(0, 2) = s.z;
  r.at(1, 0) = u.x;
  r.at(1, 1) = u.y;
  r.at(1, 2) = u.z;
  r.at(2, 0) = -f.x;
  r.at(2, 1) = -f.y;
  r.at(2, 2) = -f.z;
  r.at(0, 3) = -dot(s, eye);
  r.at(1, 3) = -dot(u, eye);
  r.at(2, 3) = dot(f, eye);
  return r;
}

// General 4x4 inverse (cofactor expansion). Returns identity when singular.
inline mat4 inverse(const mat4& mat) {
  const float* m = mat.m;
  float inv[16];
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
           m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
           m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
           m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
            m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
           m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
           m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
           m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
            m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
           m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
           m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
            m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
            m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
           m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
           m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
            m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
            m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (std::fabs(det) < 1e-30f) return mat4::identity();
  float invDet = 1.0f / det;
  mat4 r{};
  for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * invDet;
  return r;
}

struct Plane {
  vec3 n;
  float d = 0.0f;
  float distance(vec3 p) const { return dot(n, p) + d; }
};

// View frustum extracted from a view-projection matrix (Gribb/Hartmann).
struct Frustum {
  Plane planes[6];

  static Frustum fromMatrix(const mat4& vp) {
    Frustum f;
    vec4 row[4];
    for (int i = 0; i < 4; ++i) row[i] = {vp.at(i, 0), vp.at(i, 1), vp.at(i, 2), vp.at(i, 3)};
    // left, right, bottom, top, near, far
    vec4 p[6] = {row[3] + row[0], row[3] - row[0], row[3] + row[1],
                 row[3] - row[1], row[3] + row[2], row[3] - row[2]};
    for (int i = 0; i < 6; ++i) {
      vec3 n = p[i].xyz();
      float len = length(n);
      if (len > 0.0f) {
        f.planes[i].n = n / len;
        f.planes[i].d = p[i].w / len;
      }
    }
    return f;
  }

  bool intersectsAabb(vec3 mn, vec3 mx) const {
    for (const Plane& pl : planes) {
      vec3 v{pl.n.x >= 0.0f ? mx.x : mn.x, pl.n.y >= 0.0f ? mx.y : mn.y, pl.n.z >= 0.0f ? mx.z : mn.z};
      if (pl.distance(v) < 0.0f) return false;
    }
    return true;
  }
};

}  // namespace strata
