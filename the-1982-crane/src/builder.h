// THE 1982 CRANE - level construction helper (internal to the level code)
#pragma once
#include "world.h"

static inline Vec3 hexc(u32 c) { return Vec3(((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f); }

enum Face { F_PX = 1, F_NX = 2, F_PY = 4, F_NY = 8, F_PZ = 16, F_NZ = 32, F_ALL = 63, F_SIDES = 51, F_NOBOTTOM = 55 };

struct Builder {
    World& w;
    MeshData stat[PASS_COUNT];
    MeshData dynTmp[PASS_COUNT];
    bool inDyn = false;

    // --- current state ---
    int mat = M_PLAIN;
    int pass = PASS_OPAQUE;
    Vec3 tint = Vec3(1, 1, 1);
    u8 emisByte = 0;
    float uvs = 0.5f;        // texture repeats per metre
    bool uvFit = false;      // stretch 0..1 over each box face instead of metric mapping
    Vec2 uvRep = Vec2(1, 1); // repeats used with uvFit
    float grad = 1.0f;       // brightness multiplier at the bottom of boxes (fake contact shadow)
    Mat4 cur = Mat4::identity();
    bool hasXf = false;
    std::vector<Mat4> stack;

    explicit Builder(World& world) : w(world) {}

    // --- state setters ---
    Builder& m(int material, u32 hexTint = 0xffffff, float uvScale = 0.5f) {
        mat = material; tint = hexc(hexTint); uvs = uvScale; uvFit = false;
        pass = (material == M_GRATE || material == M_CHAINLINK) ? PASS_CUTOUT : (material == M_GLASS ? PASS_GLASS : (material >= M_SIGN0 ? PASS_DECAL : PASS_OPAQUE));
        return *this;
    }
    Builder& fit(float ru = 1.0f, float rv = 1.0f) { uvFit = true; uvRep = Vec2(ru, rv); return *this; }
    Builder& emissive(int group, float strength) {
        emisByte = (u8)(group * 32 + (int)(saturate(strength) * 31.0f + 0.5f));
        return *this;
    }
    Builder& noEmis() { emisByte = 0; return *this; }
    Builder& g(float gradBottom) { grad = gradBottom; return *this; }
    void push(const Mat4& mtx) { stack.push_back(cur); cur = cur * mtx; hasXf = true; }
    void pop() {
        if (!stack.empty()) { cur = stack.back(); stack.pop_back(); }
        hasXf = !stack.empty();
        if (!hasXf) cur = Mat4::identity();
    }
    Vec3 P(const Vec3& p) const { return hasXf ? cur.point(p) : p; }

    // --- primitives ---
    void quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec2& ua, const Vec2& ub, const Vec2& uc, const Vec2& ud,
              float ka = 1, float kb = 1, float kc = 1, float kd = 1);
    void tri(const Vec3& a, const Vec3& b, const Vec3& c, const Vec2& ua, const Vec2& ub, const Vec2& uc);
    void box(const Vec3& mn, const Vec3& mx, int faces = F_ALL);
    void boxC(const Vec3& c, const Vec3& half, int faces = F_ALL) { box(c - half, c + half, faces); }
    // cylinder / cone between two points
    void cyl(const Vec3& a, const Vec3& b, float r, int segs = 8, bool caps = true, float r2 = -1.0f);
    // box-section member between two points (w wide, h high in its cross-section)
    void beam(const Vec3& a, const Vec3& b, float wdt, float hgt = -1.0f);
    // surface of revolution around +Y at `c`: n rings of (radius, y)
    void lathe(const Vec3& c, const float* radii, const float* ys, int n, int segs = 12);
    // textured sign / decal quad. `normal` must be axis aligned; width in metres, height from aspect unless given.
    void sign(const char* name, const Vec3& center, const Vec3& normal, float width, float height = -1.0f, float roll = 0.0f);
    // decal lying on the floor (facing +Y), rotated by yaw
    void floorDecal(const char* name, const Vec3& center, float sx, float sz, float yaw = 0.0f);

    // --- collision / gameplay ---
    int collider(const Vec3& mn, const Vec3& mx, u16 flags = COL_SOLID | COL_SIGHT | COL_NORAIN, u8 surf = SURF_CONCRETE);
    // box + collider in one call (collider is the transformed bounds)
    int solid(const Vec3& mn, const Vec3& mx, u8 surf = SURF_CONCRETE, u16 flags = COL_SOLID | COL_SIGHT | COL_NORAIN, int faces = F_ALL) {
        box(mn, mx, faces);
        return collider(mn, mx, flags, surf);
    }
    int light(const Vec3& pos, u32 color, float intensity, float radius, int group, int flicker = FL_NONE, float halo = 0.0f);
    int spot(const Vec3& pos, const Vec3& dir, float outerDeg, u32 color, float intensity, float radius, int group, int flicker = FL_NONE, float halo = 0.0f);
    int interact(int kind, int arg, const Vec3& mn, const Vec3& mx, float range = 2.3f);
    // trigger volumes and zones follow the current transform, like colliders
    void trigger(int id, const Vec3& mn, const Vec3& mx);
    void zone(int kind, const Vec3& mn, const Vec3& mx);

    // Pickups and documents are placed in world coordinates whatever transform is active.
    struct XfState { Mat4 cur; bool hasXf; std::vector<Mat4> stack; };
    XfState saveXf() const { return XfState{cur, hasXf, stack}; }
    void identityXf() { cur = Mat4::identity(); hasXf = false; stack.clear(); }
    void restoreXf(const XfState& st) { cur = st.cur; hasXf = st.hasXf; stack = st.stack; }

    // --- dynamic meshes ---
    void beginDyn();
    int endDyn();                                   // returns dyn mesh index
    int addDyn(int mesh, const Mat4& xf, bool visible = true);
    void named(int dynId, int mesh, const Mat4& xf = Mat4::identity(), bool visible = true);

    // --- finish ---
    void finish();

private:
    MeshData& target() { return inDyn ? dynTmp[pass] : stat[pass]; }
    void vtx(MeshData& md, const Vec3& p, const Vec3& n, const Vec2& uv, float k);
};
