// THE 1982 CRANE - core math / utility layer
// Pure C++17, no platform dependencies. Shared by the web build and the
// native headless test build.
#pragma once
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef int8_t i8;

static const float PI = 3.14159265358979f;
static const float TAU = 6.28318530717959f;

template <class T> static inline T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float saturate(float v) { return clampv(v, 0.0f, 1.0f); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smooth01(float t) { t = saturate(t); return t * t * (3.0f - 2.0f * t); }
static inline float smoothstepf(float a, float b, float x) { return smooth01((x - a) / (b - a)); }
static inline float signf(float v) { return v < 0 ? -1.0f : 1.0f; }
static inline float approachf(float cur, float target, float maxDelta) {
    if (cur < target) return std::min(cur + maxDelta, target);
    return std::max(cur - maxDelta, target);
}
// frame-rate independent exponential smoothing
static inline float dampf(float cur, float target, float rate, float dt) {
    return lerpf(target, cur, std::exp(-rate * dt));
}
static inline float wrapAngle(float a) {
    while (a > PI) a -= TAU;
    while (a < -PI) a += TAU;
    return a;
}
static inline float fractf(float v) { return v - std::floor(v); }

struct Vec2 {
    float x = 0, y = 0;
    Vec2() {}
    Vec2(float X, float Y) : x(X), y(Y) {}
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
};
static inline float length(const Vec2& a) { return std::sqrt(a.x * a.x + a.y * a.y); }

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() {}
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator*(const Vec3& o) const { return Vec3(x * o.x, y * o.y, z * o.z); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};
static inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
static inline float length2(const Vec3& a) { return dot(a, a); }
static inline Vec3 normalize(const Vec3& a) {
    float l = length(a);
    return l > 1e-8f ? a * (1.0f / l) : Vec3(0, 1, 0);
}
static inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
static inline Vec3 vmin(const Vec3& a, const Vec3& b) { return Vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)); }
static inline Vec3 vmax(const Vec3& a, const Vec3& b) { return Vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)); }
static inline float distXZ(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    Vec4() {}
    Vec4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
    Vec4(const Vec3& v, float W) : x(v.x), y(v.y), z(v.z), w(W) {}
};

// Column-major 4x4 matrix (OpenGL convention): m[col*4+row]
struct Mat4 {
    float m[16];
    static Mat4 identity() {
        Mat4 r;
        std::memset(r.m, 0, sizeof(r.m));
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
    static Mat4 translate(const Vec3& t) {
        Mat4 r = identity();
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }
    static Mat4 scale(const Vec3& s) {
        Mat4 r = identity();
        r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
        return r;
    }
    static Mat4 rotX(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
        return r;
    }
    static Mat4 rotY(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
        return r;
    }
    static Mat4 rotZ(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
        return r;
    }
    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int c = 0; c < 4; c++)
            for (int rr = 0; rr < 4; rr++) {
                float s = 0;
                for (int k = 0; k < 4; k++) s += m[k * 4 + rr] * b.m[c * 4 + k];
                r.m[c * 4 + rr] = s;
            }
        return r;
    }
    Vec3 point(const Vec3& p) const {
        return Vec3(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                    m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                    m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]);
    }
    Vec3 dir(const Vec3& p) const {
        return Vec3(m[0] * p.x + m[4] * p.y + m[8] * p.z,
                    m[1] * p.x + m[5] * p.y + m[9] * p.z,
                    m[2] * p.x + m[6] * p.y + m[10] * p.z);
    }
    Vec4 mul4(const Vec4& p) const {
        return Vec4(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12] * p.w,
                    m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13] * p.w,
                    m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14] * p.w,
                    m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15] * p.w);
    }
    static Mat4 perspective(float fovyRad, float aspect, float zn, float zf) {
        Mat4 r;
        std::memset(r.m, 0, sizeof(r.m));
        float f = 1.0f / std::tan(fovyRad * 0.5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zf + zn) / (zn - zf);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zf * zn) / (zn - zf);
        return r;
    }
    static Mat4 ortho(float l, float r_, float b, float t, float n, float f) {
        Mat4 r = identity();
        r.m[0] = 2.0f / (r_ - l);
        r.m[5] = 2.0f / (t - b);
        r.m[10] = -2.0f / (f - n);
        r.m[12] = -(r_ + l) / (r_ - l);
        r.m[13] = -(t + b) / (t - b);
        r.m[14] = -(f + n) / (f - n);
        return r;
    }
    static Mat4 lookAt(const Vec3& eye, const Vec3& at, const Vec3& up) {
        Vec3 f = normalize(at - eye);
        Vec3 s = normalize(cross(f, up));
        Vec3 u = cross(s, f);
        Mat4 r = identity();
        r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
        r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] = dot(f, eye);
        return r;
    }
    // Inverse for rigid transforms + (possibly non-uniform) scale: general 4x4 inverse.
    Mat4 inverse() const {
        Mat4 inv;
        const float* a = m;
        float* o = inv.m;
        o[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
        o[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
        o[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
        o[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
        o[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
        o[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
        o[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
        o[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
        o[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
        o[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
        o[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
        o[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
        o[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
        o[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
        o[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
        o[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
        float det = a[0] * o[0] + a[1] * o[4] + a[2] * o[8] + a[3] * o[12];
        if (std::fabs(det) < 1e-20f) return identity();
        det = 1.0f / det;
        for (int i = 0; i < 16; i++) o[i] *= det;
        return inv;
    }
};

// Axis-aligned box
struct AABB {
    Vec3 mn, mx;
    AABB() {}
    AABB(const Vec3& a, const Vec3& b) : mn(a), mx(b) {}
    static AABB fromCenter(const Vec3& c, const Vec3& half) { return AABB(c - half, c + half); }
    Vec3 center() const { return (mn + mx) * 0.5f; }
    Vec3 half() const { return (mx - mn) * 0.5f; }
    bool contains(const Vec3& p) const {
        return p.x >= mn.x && p.x <= mx.x && p.y >= mn.y && p.y <= mx.y && p.z >= mn.z && p.z <= mx.z;
    }
    bool overlaps(const AABB& o) const {
        return mn.x < o.mx.x && mx.x > o.mn.x && mn.y < o.mx.y && mx.y > o.mn.y && mn.z < o.mx.z && mx.z > o.mn.z;
    }
    AABB grown(float r) const { return AABB(mn - Vec3(r, r, r), mx + Vec3(r, r, r)); }
};

// Ray vs AABB slab test. Returns true on hit with tOut = entry distance (>= 0).
static inline bool rayAABB(const Vec3& o, const Vec3& d, const AABB& b, float tMax, float& tOut) {
    float t0 = 0.0f, t1 = tMax;
    for (int i = 0; i < 3; i++) {
        float oi = o[i], di = d[i], lo = b.mn[i], hi = b.mx[i];
        if (std::fabs(di) < 1e-9f) {
            if (oi < lo || oi > hi) return false;
        } else {
            float inv = 1.0f / di;
            float ta = (lo - oi) * inv, tb = (hi - oi) * inv;
            if (ta > tb) std::swap(ta, tb);
            if (ta > t0) t0 = ta;
            if (tb < t1) t1 = tb;
            if (t0 > t1) return false;
        }
    }
    tOut = t0;
    return true;
}

// ---------------------------------------------------------------------------
// Deterministic RNG (xorshift32) and hashing
// ---------------------------------------------------------------------------
struct Rng {
    u32 s;
    explicit Rng(u32 seed = 0x1982C0DEu) : s(seed ? seed : 1u) {}
    u32 next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float f() { return (next() >> 8) * (1.0f / 16777216.0f); }        // [0,1)
    float range(float a, float b) { return a + (b - a) * f(); }
    int irange(int a, int b) { return a + (int)(next() % (u32)(b - a + 1)); } // inclusive
    bool chance(float p) { return f() < p; }
};

static inline u32 hash32(u32 x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
static inline u32 hash2i(int x, int y) { return hash32((u32)x * 0x9E3779B1u ^ hash32((u32)y + 0x7F4A7C15u)); }
static inline float hash2f(int x, int y) { return (hash2i(x, y) >> 8) * (1.0f / 16777216.0f); }

// printf-style std::string
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
static inline std::string strf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return std::string(buf);
}
