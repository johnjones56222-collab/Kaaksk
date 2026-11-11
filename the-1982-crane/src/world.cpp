// THE 1982 CRANE - world queries, navigation and the level builder implementation
#include "builder.h"
#include <unordered_map>

// ---------------------------------------------------------------------------
// World queries
// ---------------------------------------------------------------------------
void World::query(const AABB& box, std::vector<int>& out) const {
    out.clear();
    if (grid.empty()) return;
    int x0 = clampv((int)std::floor((box.mn.x - gx0) / gcell), 0, gw - 1);
    int x1 = clampv((int)std::floor((box.mx.x - gx0) / gcell), 0, gw - 1);
    int z0 = clampv((int)std::floor((box.mn.z - gz0) / gcell), 0, gh - 1);
    int z1 = clampv((int)std::floor((box.mx.z - gz0) / gcell), 0, gh - 1);
    if (qstamp.size() != colliders.size()) qstamp.assign(colliders.size(), 0);
    u32 stamp = ++qcounter;
    for (int z = z0; z <= z1; z++)
        for (int x = x0; x <= x1; x++) {
            const std::vector<u16>& cell = grid[(size_t)z * gw + x];
            for (u16 ci : cell) {
                if (qstamp[ci] == stamp) continue;
                qstamp[ci] = stamp;
                const Collider& c = colliders[ci];
                if (c.enabled && c.box.overlaps(box)) out.push_back((int)ci);
            }
        }
}

float World::raycast(const Vec3& o, const Vec3& d, float tMax, u32 mask, int* hitCollider) const {
    if (hitCollider) *hitCollider = -1;
    if (grid.empty()) return tMax;
    float best = tMax;
    if (qstamp.size() != colliders.size()) qstamp.assign(colliders.size(), 0);
    u32 stamp = ++qcounter;
    // 2D DDA over the broadphase grid
    float fx = (o.x - gx0) / gcell, fz = (o.z - gz0) / gcell;
    int cx = (int)std::floor(fx), cz = (int)std::floor(fz);
    if (cx < 0 || cz < 0 || cx >= gw || cz >= gh) return tMax;
    int sx = d.x > 0 ? 1 : -1, sz = d.z > 0 ? 1 : -1;
    float tdx = std::fabs(d.x) > 1e-7f ? gcell / std::fabs(d.x) : 1e30f;
    float tdz = std::fabs(d.z) > 1e-7f ? gcell / std::fabs(d.z) : 1e30f;
    float tmx = std::fabs(d.x) > 1e-7f ? ((d.x > 0 ? (cx + 1) : cx) * gcell + gx0 - o.x) / d.x : 1e30f;
    float tmz = std::fabs(d.z) > 1e-7f ? ((d.z > 0 ? (cz + 1) : cz) * gcell + gz0 - o.z) / d.z : 1e30f;
    float tEnter = 0.0f;
    for (int guard = 0; guard < 512; guard++) {
        const std::vector<u16>& cell = grid[(size_t)cz * gw + cx];
        for (u16 ci : cell) {
            if (qstamp[ci] == stamp) continue;
            qstamp[ci] = stamp;
            const Collider& c = colliders[ci];
            if (!c.enabled || !(c.flags & mask)) continue;
            float t;
            if (rayAABB(o, d, c.box, best, t) && t < best) {
                best = t;
                if (hitCollider) *hitCollider = (int)ci;
            }
        }
        float tExit = std::min(tmx, tmz);
        if (best <= tExit || tExit >= tMax) break;
        if (tmx < tmz) { cx += sx; tmx += tdx; } else { cz += sz; tmz += tdz; }
        if (cx < 0 || cz < 0 || cx >= gw || cz >= gh) break;
        tEnter = tExit;
    }
    (void)tEnter;
    return best;
}

bool World::lineOfSight(const Vec3& a, const Vec3& b, u32 mask) const {
    Vec3 d = b - a;
    float len = length(d);
    if (len < 1e-4f) return true;
    d = d * (1.0f / len);
    return raycast(a, d, len, mask) >= len - 1e-3f;
}

float World::floorBelow(const Vec3& p, float maxDrop, int* surf) const {
    static thread_local std::vector<int> tmp;
    AABB col(Vec3(p.x - 0.05f, p.y - maxDrop, p.z - 0.05f), Vec3(p.x + 0.05f, p.y + 0.01f, p.z + 0.05f));
    query(col, tmp);
    float best = -1000.0f;
    for (int ci : tmp) {
        const Collider& c = colliders[(size_t)ci];
        if (!(c.flags & COL_SOLID)) continue;
        float top = c.box.mx.y;
        if (top <= p.y + 0.01f && top > best) {
            best = top;
            if (surf) *surf = c.surf;
        }
    }
    return best;
}

int World::zoneAt(const Vec3& p) const {
    for (const Zone& z : zones)
        if (z.box.contains(p)) return z.kind;
    return Z_OUTDOOR;
}

Mat4 World::doorMatrix(const Door& d) const {
    if (d.slide) {
        Vec3 dir(std::cos(d.yawClosed), 0, -std::sin(d.yawClosed));
        return Mat4::translate(d.hinge + dir * d.angle) * Mat4::rotY(d.yawClosed);
    }
    return Mat4::translate(d.hinge) * Mat4::rotY(d.yawClosed + d.angle);
}

// ---------------------------------------------------------------------------
// Navigation grid + A*
// ---------------------------------------------------------------------------
void world_build_nav(World& w) {
    NavGrid& n = w.nav;
    n.cell = 0.5f;
    n.x0 = -82.0f; n.z0 = -50.0f;
    n.w = (int)((82.0f - n.x0) / n.cell);
    n.h = (int)((82.0f - n.z0) / n.cell);
    n.blocked.assign((size_t)n.w * n.h, 1);
    n.height.assign((size_t)n.w * n.h, 0.0f);
    std::vector<int> tmp;
    for (int z = 0; z < n.h; z++)
        for (int x = 0; x < n.w; x++) {
            float px = n.x0 + (x + 0.5f) * n.cell, pz = n.z0 + (z + 0.5f) * n.cell;
            size_t i = (size_t)z * n.w + x;
            if (pz < QUAY_Z + 0.6f) continue; // never over the water / barge
            float fl = w.floorBelow(Vec3(px, 1.05f, pz), 3.0f);
            if (fl < -0.3f) continue;
            n.height[i] = fl;
            AABB body(Vec3(px - 0.22f, fl + 0.42f, pz - 0.22f), Vec3(px + 0.22f, fl + 1.95f, pz + 0.22f));
            w.query(body, tmp);
            bool blocked = false;
            for (int ci : tmp) {
                const Collider& c = w.colliders[(size_t)ci];
                if (!(c.flags & COL_SOLID) || (c.flags & COL_NAVIGNORE)) continue;
                blocked = true;
                break;
            }
            n.blocked[i] = blocked ? 1 : 0;
        }
    // Keep only what is connected to the yard: tops of furniture and sealed pockets
    // would otherwise attract nav_snap() and produce unreachable targets.
    std::vector<u8> seen((size_t)n.w * n.h, 0);
    std::vector<int> stack;
    int sx = n.cx(0.0f), sz = n.cz(30.0f);
    if (n.walkable(sx, sz)) { stack.push_back(sz * n.w + sx); seen[(size_t)(sz * n.w + sx)] = 1; }
    while (!stack.empty()) {
        int cur = stack.back();
        stack.pop_back();
        int cx = cur % n.w, cz = cur / n.w;
        for (int k = 0; k < 4; k++) {
            int nx = cx + (k == 0) - (k == 1), nz = cz + (k == 2) - (k == 3);
            if (!n.walkable(nx, nz)) continue;
            size_t ni = (size_t)nz * n.w + nx;
            if (seen[ni]) continue;
            if (std::fabs(n.height[ni] - n.height[(size_t)cur]) > 0.45f) continue;
            seen[ni] = 1;
            stack.push_back((int)ni);
        }
    }
    for (size_t i = 0; i < seen.size(); i++)
        if (!seen[i]) n.blocked[i] = 1;
}

bool nav_walkable(const World& w, const Vec3& p) { return w.nav.walkable(w.nav.cx(p.x), w.nav.cz(p.z)); }

Vec3 nav_snap(const World& w, const Vec3& p, int r) {
    const NavGrid& n = w.nav;
    int cx = n.cx(p.x), cz = n.cz(p.z);
    if (n.walkable(cx, cz)) return Vec3(p.x, n.height[(size_t)cz * n.w + cx], p.z);
    float best = 1e30f;
    Vec3 out = p;
    for (int dz = -r; dz <= r; dz++)
        for (int dx = -r; dx <= r; dx++) {
            int x = cx + dx, z = cz + dz;
            if (!n.walkable(x, z)) continue;
            Vec3 c = n.center(x, z);
            float d = (c.x - p.x) * (c.x - p.x) + (c.z - p.z) * (c.z - p.z);
            if (d < best) { best = d; out = c; }
        }
    return out;
}

static bool navStep(const NavGrid& n, int x0, int z0, int x1, int z1) {
    if (!n.walkable(x1, z1)) return false;
    float h0 = n.height[(size_t)z0 * n.w + x0], h1 = n.height[(size_t)z1 * n.w + x1];
    if (std::fabs(h1 - h0) > 0.45f) return false;
    if (x0 != x1 && z0 != z1) {
        // no corner cutting
        if (!n.walkable(x1, z0) || !n.walkable(x0, z1)) return false;
    }
    return true;
}
// straight walk test on the grid
static bool navLine(const NavGrid& n, const Vec3& a, const Vec3& b) {
    float dx = b.x - a.x, dz = b.z - a.z;
    float len = std::sqrt(dx * dx + dz * dz);
    int steps = (int)(len / (n.cell * 0.5f)) + 1;
    int px = n.cx(a.x), pz = n.cz(a.z);
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        int cx = n.cx(a.x + dx * t), cz = n.cz(a.z + dz * t);
        if (cx == px && cz == pz) continue;
        if (!navStep(n, px, pz, cx, cz)) return false;
        px = cx; pz = cz;
    }
    return true;
}

bool nav_path(const World& w, const Vec3& from, const Vec3& to, std::vector<Vec3>& path) {
    path.clear();
    const NavGrid& n = w.nav;
    if (n.w == 0) return false;
    Vec3 s = nav_snap(w, from, 8), e = nav_snap(w, to, 8);
    int sx = n.cx(s.x), sz = n.cz(s.z), ex = n.cx(e.x), ez = n.cz(e.z);
    if (!n.walkable(sx, sz) || !n.walkable(ex, ez)) return false;
    size_t total = (size_t)n.w * n.h;
    static std::vector<float> gs;
    static std::vector<int> parent;
    static std::vector<u32> stamp;
    static u32 counter = 0;
    if (gs.size() != total) { gs.assign(total, 0); parent.assign(total, -1); stamp.assign(total, 0); counter = 0; }
    counter++;
    struct Node { float f; int idx; };
    auto cmp = [](const Node& a, const Node& b) { return a.f > b.f; };
    static std::vector<Node> heap;
    heap.clear();
    auto hfn = [&](int x, int z) {
        float dx = (float)std::abs(x - ex), dz = (float)std::abs(z - ez);
        return (dx + dz) + (1.41421f - 2.0f) * std::min(dx, dz);
    };
    int sIdx = sz * n.w + sx, eIdx = ez * n.w + ex;
    gs[(size_t)sIdx] = 0; parent[(size_t)sIdx] = -1; stamp[(size_t)sIdx] = counter;
    heap.push_back({hfn(sx, sz), sIdx});
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DZ[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    bool found = false;
    int expansions = 0;
    while (!heap.empty()) {
        std::pop_heap(heap.begin(), heap.end(), cmp);
        Node cur = heap.back();
        heap.pop_back();
        if (cur.idx == eIdx) { found = true; break; }
        int cx = cur.idx % n.w, cz = cur.idx / n.w;
        float g = gs[(size_t)cur.idx];
        if (cur.f > g + hfn(cx, cz) + 0.001f) continue; // stale entry
        if (++expansions > 90000) break;
        for (int k = 0; k < 8; k++) {
            int nx = cx + DX[k], nz = cz + DZ[k];
            if (!navStep(n, cx, cz, nx, nz)) continue;
            int ni = nz * n.w + nx;
            float ng = g + (k < 4 ? 1.0f : 1.41421f);
            if (stamp[(size_t)ni] != counter || ng < gs[(size_t)ni]) {
                stamp[(size_t)ni] = counter;
                gs[(size_t)ni] = ng;
                parent[(size_t)ni] = cur.idx;
                heap.push_back({ng + hfn(nx, nz), ni});
                std::push_heap(heap.begin(), heap.end(), cmp);
            }
        }
    }
    if (!found) return false;
    std::vector<Vec3> raw;
    for (int i = eIdx; i != -1; i = parent[(size_t)i]) {
        raw.push_back(n.center(i % n.w, i / n.w));
        if (i == sIdx) break;
    }
    std::reverse(raw.begin(), raw.end());
    // string pulling
    size_t i = 0;
    while (i + 1 < raw.size()) {
        size_t j = raw.size() - 1;
        if (j > i + 24) j = i + 24;
        while (j > i + 1 && !navLine(n, raw[i], raw[j])) j--;
        path.push_back(raw[j]);
        i = j;
    }
    if (path.empty()) path.push_back(e);
    return true;
}

// ---------------------------------------------------------------------------
// Builder
// ---------------------------------------------------------------------------
void Builder::vtx(MeshData& md, const Vec3& p, const Vec3& n, const Vec2& uv, float k) {
    Vtx v;
    Vec3 wp = hasXf ? cur.point(p) : p;
    Vec3 wn = hasXf ? normalize(cur.dir(n)) : n;
    v.x = wp.x; v.y = wp.y; v.z = wp.z;
    v.nx = (i8)clampv((int)std::lround(wn.x * 127.0f), -127, 127);
    v.ny = (i8)clampv((int)std::lround(wn.y * 127.0f), -127, 127);
    v.nz = (i8)clampv((int)std::lround(wn.z * 127.0f), -127, 127);
    v.layer = (u8)mat;
    v.u = uv.x; v.v = uv.y;
    v.r = (u8)clampv((int)(tint.x * k * 255.0f + 0.5f), 0, 255);
    v.g = (u8)clampv((int)(tint.y * k * 255.0f + 0.5f), 0, 255);
    v.b = (u8)clampv((int)(tint.z * k * 255.0f + 0.5f), 0, 255);
    v.a = emisByte;
    md.v.push_back(v);
}

void Builder::quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec2& ua, const Vec2& ub, const Vec2& uc, const Vec2& ud,
                   float ka, float kb, float kc, float kd) {
    MeshData& md = target();
    Vec3 n = normalize(cross(b - a, c - a));
    u32 base = (u32)md.v.size();
    vtx(md, a, n, ua, ka); vtx(md, b, n, ub, kb); vtx(md, c, n, uc, kc); vtx(md, d, n, ud, kd);
    md.idx.push_back(base); md.idx.push_back(base + 1); md.idx.push_back(base + 2);
    md.idx.push_back(base); md.idx.push_back(base + 2); md.idx.push_back(base + 3);
}

void Builder::tri(const Vec3& a, const Vec3& b, const Vec3& c, const Vec2& ua, const Vec2& ub, const Vec2& uc) {
    MeshData& md = target();
    Vec3 n = normalize(cross(b - a, c - a));
    u32 base = (u32)md.v.size();
    vtx(md, a, n, ua, 1); vtx(md, b, n, ub, 1); vtx(md, c, n, uc, 1);
    md.idx.push_back(base); md.idx.push_back(base + 1); md.idx.push_back(base + 2);
}

void Builder::box(const Vec3& mn, const Vec3& mx, int faces) {
    float x0 = mn.x, y0 = mn.y, z0 = mn.z, x1 = mx.x, y1 = mx.y, z1 = mx.z;
    float s = uvs, gb = grad;
    Vec2 fa(0, uvRep.y), fb(uvRep.x, uvRep.y), fc(uvRep.x, 0), fd(0, 0);
    if (faces & F_PX) {
        if (uvFit) quad(Vec3(x1, y0, z1), Vec3(x1, y0, z0), Vec3(x1, y1, z0), Vec3(x1, y1, z1), fa, fb, fc, fd, gb, gb, 1, 1);
        else quad(Vec3(x1, y0, z1), Vec3(x1, y0, z0), Vec3(x1, y1, z0), Vec3(x1, y1, z1), Vec2(-z1 * s, -y0 * s), Vec2(-z0 * s, -y0 * s), Vec2(-z0 * s, -y1 * s), Vec2(-z1 * s, -y1 * s), gb, gb, 1, 1);
    }
    if (faces & F_NX) {
        if (uvFit) quad(Vec3(x0, y0, z0), Vec3(x0, y0, z1), Vec3(x0, y1, z1), Vec3(x0, y1, z0), fa, fb, fc, fd, gb, gb, 1, 1);
        else quad(Vec3(x0, y0, z0), Vec3(x0, y0, z1), Vec3(x0, y1, z1), Vec3(x0, y1, z0), Vec2(z0 * s, -y0 * s), Vec2(z1 * s, -y0 * s), Vec2(z1 * s, -y1 * s), Vec2(z0 * s, -y1 * s), gb, gb, 1, 1);
    }
    if (faces & F_PY) {
        if (uvFit) quad(Vec3(x0, y1, z1), Vec3(x1, y1, z1), Vec3(x1, y1, z0), Vec3(x0, y1, z0), fa, fb, fc, fd);
        else quad(Vec3(x0, y1, z1), Vec3(x1, y1, z1), Vec3(x1, y1, z0), Vec3(x0, y1, z0), Vec2(x0 * s, z1 * s), Vec2(x1 * s, z1 * s), Vec2(x1 * s, z0 * s), Vec2(x0 * s, z0 * s));
    }
    if (faces & F_NY) {
        if (uvFit) quad(Vec3(x0, y0, z0), Vec3(x1, y0, z0), Vec3(x1, y0, z1), Vec3(x0, y0, z1), fa, fb, fc, fd, gb, gb, gb, gb);
        else quad(Vec3(x0, y0, z0), Vec3(x1, y0, z0), Vec3(x1, y0, z1), Vec3(x0, y0, z1), Vec2(x0 * s, z0 * s), Vec2(x1 * s, z0 * s), Vec2(x1 * s, z1 * s), Vec2(x0 * s, z1 * s), gb, gb, gb, gb);
    }
    if (faces & F_PZ) {
        if (uvFit) quad(Vec3(x0, y0, z1), Vec3(x1, y0, z1), Vec3(x1, y1, z1), Vec3(x0, y1, z1), fa, fb, fc, fd, gb, gb, 1, 1);
        else quad(Vec3(x0, y0, z1), Vec3(x1, y0, z1), Vec3(x1, y1, z1), Vec3(x0, y1, z1), Vec2(x0 * s, -y0 * s), Vec2(x1 * s, -y0 * s), Vec2(x1 * s, -y1 * s), Vec2(x0 * s, -y1 * s), gb, gb, 1, 1);
    }
    if (faces & F_NZ) {
        if (uvFit) quad(Vec3(x1, y0, z0), Vec3(x0, y0, z0), Vec3(x0, y1, z0), Vec3(x1, y1, z0), fa, fb, fc, fd, gb, gb, 1, 1);
        else quad(Vec3(x1, y0, z0), Vec3(x0, y0, z0), Vec3(x0, y1, z0), Vec3(x1, y1, z0), Vec2(-x1 * s, -y0 * s), Vec2(-x0 * s, -y0 * s), Vec2(-x0 * s, -y1 * s), Vec2(-x1 * s, -y1 * s), gb, gb, 1, 1);
    }
}

void Builder::cyl(const Vec3& a, const Vec3& b, float r, int segs, bool caps, float r2) {
    if (r2 < 0) r2 = r;
    Vec3 axis = b - a;
    float len = length(axis);
    if (len < 1e-5f) return;
    axis = axis * (1.0f / len);
    Vec3 helper = std::fabs(axis.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 u = normalize(cross(helper, axis));
    Vec3 v = cross(axis, u);
    MeshData& md = target();
    float urep = std::max(1.0f, std::floor(TAU * std::max(r, r2) * uvs + 0.5f));
    u32 base = (u32)md.v.size();
    for (int i = 0; i <= segs; i++) {
        float t = (float)i / (float)segs, ang = t * TAU;
        Vec3 rad = u * std::cos(ang) + v * std::sin(ang);
        vtx(md, a + rad * r, rad, Vec2(t * urep, 0), grad);
        vtx(md, b + rad * r2, rad, Vec2(t * urep, -len * uvs), 1.0f);
    }
    for (int i = 0; i < segs; i++) {
        u32 a0 = base + (u32)i * 2, b0 = a0 + 1, a1 = a0 + 2, b1 = a0 + 3;
        md.idx.push_back(a0); md.idx.push_back(a1); md.idx.push_back(b1);
        md.idx.push_back(a0); md.idx.push_back(b1); md.idx.push_back(b0);
    }
    if (caps) {
        for (int cap = 0; cap < 2; cap++) {
            Vec3 c = cap ? b : a, n = cap ? axis : -axis;
            float rr = cap ? r2 : r;
            if (rr < 1e-4f) continue;
            u32 cb = (u32)md.v.size();
            vtx(md, c, n, Vec2(0.5f, 0.5f), cap ? 1.0f : grad);
            for (int i = 0; i <= segs; i++) {
                float ang = (float)i / (float)segs * TAU;
                Vec3 rad = u * std::cos(ang) + v * std::sin(ang);
                vtx(md, c + rad * rr, n, Vec2(0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)), cap ? 1.0f : grad);
            }
            for (int i = 0; i < segs; i++) {
                if (cap) { md.idx.push_back(cb); md.idx.push_back(cb + 1 + (u32)i); md.idx.push_back(cb + 2 + (u32)i); }
                else { md.idx.push_back(cb); md.idx.push_back(cb + 2 + (u32)i); md.idx.push_back(cb + 1 + (u32)i); }
            }
        }
    }
}

void Builder::beam(const Vec3& a, const Vec3& b, float wdt, float hgt) {
    if (hgt < 0) hgt = wdt;
    Vec3 axis = b - a;
    float len = length(axis);
    if (len < 1e-5f) return;
    axis = axis * (1.0f / len);
    Vec3 helper = std::fabs(axis.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 side = normalize(cross(axis, helper));
    Vec3 up = cross(side, axis);
    Vec3 s = side * (wdt * 0.5f), u = up * (hgt * 0.5f);
    float ul = len * uvs, vw = wdt * uvs, vh = hgt * uvs;
    auto C = [&](const Vec3& e, float si, float ui) { return e + s * si + u * ui; };
    quad(C(a, 1, -1), C(b, 1, -1), C(b, 1, 1), C(a, 1, 1), Vec2(0, vh), Vec2(ul, vh), Vec2(ul, 0), Vec2(0, 0));
    quad(C(a, -1, 1), C(b, -1, 1), C(b, -1, -1), C(a, -1, -1), Vec2(0, 0), Vec2(ul, 0), Vec2(ul, vh), Vec2(0, vh));
    quad(C(a, 1, 1), C(b, 1, 1), C(b, -1, 1), C(a, -1, 1), Vec2(0, vw), Vec2(ul, vw), Vec2(ul, 0), Vec2(0, 0));
    quad(C(a, -1, -1), C(b, -1, -1), C(b, 1, -1), C(a, 1, -1), Vec2(0, 0), Vec2(ul, 0), Vec2(ul, vw), Vec2(0, vw));
    quad(C(b, -1, -1), C(b, -1, 1), C(b, 1, 1), C(b, 1, -1), Vec2(0, vh), Vec2(0, 0), Vec2(vw, 0), Vec2(vw, vh));
    quad(C(a, 1, -1), C(a, 1, 1), C(a, -1, 1), C(a, -1, -1), Vec2(0, vh), Vec2(0, 0), Vec2(vw, 0), Vec2(vw, vh));
}

void Builder::lathe(const Vec3& c, const float* radii, const float* ys, int n, int segs) {
    for (int i = 0; i + 1 < n; i++)
        for (int j = 0; j < segs; j++) {
            float a0 = (float)j / segs * TAU, a1 = (float)(j + 1) / segs * TAU;
            Vec3 p00 = c + Vec3(std::cos(a0) * radii[i], ys[i], std::sin(a0) * radii[i]);
            Vec3 p01 = c + Vec3(std::cos(a1) * radii[i], ys[i], std::sin(a1) * radii[i]);
            Vec3 p10 = c + Vec3(std::cos(a0) * radii[i + 1], ys[i + 1], std::sin(a0) * radii[i + 1]);
            Vec3 p11 = c + Vec3(std::cos(a1) * radii[i + 1], ys[i + 1], std::sin(a1) * radii[i + 1]);
            float u0 = (float)j / segs * 2.0f, u1 = (float)(j + 1) / segs * 2.0f;
            float v0 = -ys[i] * uvs, v1 = -ys[i + 1] * uvs;
            if (radii[i] < 1e-4f) tri(p01, p10, p11, Vec2(u1, v0), Vec2(u0, v1), Vec2(u1, v1));
            else if (radii[i + 1] < 1e-4f) tri(p01, p00, p10, Vec2(u1, v0), Vec2(u0, v0), Vec2(u0, v1));
            else quad(p01, p00, p10, p11, Vec2(u1, v0), Vec2(u0, v0), Vec2(u0, v1), Vec2(u1, v1));
        }
}

void Builder::sign(const char* name, const Vec3& center, const Vec3& normal, float width, float height, float roll) {
    SignRect sr = sign_rect(name);
    if (height < 0) height = width / sr.aspect;
    Vec3 n = normalize(normal);
    Vec3 up = std::fabs(n.y) > 0.9f ? Vec3(0, 0, -1) : Vec3(0, 1, 0);
    Vec3 r = normalize(cross(up, n));
    up = cross(n, r);
    if (roll != 0.0f) {
        float cs = std::cos(roll), sn = std::sin(roll);
        Vec3 r2 = r * cs + up * sn, u2 = up * cs - r * sn;
        r = r2; up = u2;
    }
    Vec3 c = center + n * 0.012f;
    Vec3 hr = r * (width * 0.5f), hu = up * (height * 0.5f);
    int sm = mat, sp = pass;
    mat = sr.layer; pass = PASS_DECAL;
    quad(c - hr - hu, c + hr - hu, c + hr + hu, c - hr + hu, Vec2(sr.u0, sr.v1), Vec2(sr.u1, sr.v1), Vec2(sr.u1, sr.v0), Vec2(sr.u0, sr.v0));
    mat = sm; pass = sp;
}

void Builder::floorDecal(const char* name, const Vec3& center, float sx, float sz, float yaw) {
    SignRect sr = sign_rect(name);
    float cs = std::cos(yaw), sn = std::sin(yaw);
    Vec3 r(cs * sx * 0.5f, 0, -sn * sx * 0.5f), f(-sn * sz * 0.5f, 0, -cs * sz * 0.5f);
    Vec3 c = center + Vec3(0, 0.014f, 0);
    int sm = mat, sp = pass;
    mat = sr.layer; pass = PASS_DECAL;
    quad(c - r - f, c + r - f, c + r + f, c - r + f, Vec2(sr.u0, sr.v1), Vec2(sr.u1, sr.v1), Vec2(sr.u1, sr.v0), Vec2(sr.u0, sr.v0));
    mat = sm; pass = sp;
}

static AABB xformBox(const Mat4& m, const Vec3& mn, const Vec3& mx) {
    Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < 8; i++) {
        Vec3 p = m.point(Vec3((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z));
        lo = vmin(lo, p); hi = vmax(hi, p);
    }
    return AABB(lo, hi);
}

int Builder::collider(const Vec3& mn, const Vec3& mx, u16 flags, u8 surf) {
    Collider c;
    c.box = hasXf ? xformBox(cur, mn, mx) : AABB(vmin(mn, mx), vmax(mn, mx));
    c.flags = flags; c.surf = surf; c.enabled = true;
    w.colliders.push_back(c);
    return (int)w.colliders.size() - 1;
}

int Builder::light(const Vec3& pos, u32 color, float intensity, float radius, int group, int flicker, float halo) {
    Light l;
    l.pos = P(pos);
    l.color = hexc(color) * intensity;
    l.radius = radius;
    l.group = group;
    l.flicker = flicker;
    l.phase = hash2f((int)(l.pos.x * 13.0f), (int)(l.pos.z * 7.0f)) * 100.0f;
    l.halo = halo;
    w.lights.push_back(l);
    return (int)w.lights.size() - 1;
}

int Builder::spot(const Vec3& pos, const Vec3& dir, float outerDeg, u32 color, float intensity, float radius, int group, int flicker, float halo) {
    int i = light(pos, color, intensity, radius, group, flicker, halo);
    Light& l = w.lights[(size_t)i];
    l.dir = normalize(hasXf ? cur.dir(dir) : dir);
    l.cosOuter = std::cos(outerDeg * PI / 180.0f);
    l.cosInner = std::cos(outerDeg * 0.6f * PI / 180.0f);
    return i;
}

int Builder::interact(int kind, int arg, const Vec3& mn, const Vec3& mx, float range) {
    Interact it;
    it.kind = kind; it.arg = arg;
    it.box = hasXf ? xformBox(cur, mn, mx) : AABB(vmin(mn, mx), vmax(mn, mx));
    it.range = range;
    w.interacts.push_back(it);
    int idx = (int)w.interacts.size() - 1;
    Vec3 c = it.box.center();
    switch (kind) {
    case IK_PICKUP: w.itemInteract[arg] = idx; w.itemPos[arg] = c; break;
    case IK_NOTE: w.noteInteract[arg] = idx; w.notePos[arg] = c; break;
    case IK_USE: w.useInteract[arg] = idx; w.usePos[arg] = c; break;
    case IK_DOOR: w.doorPos[arg] = c; break;
    default: break;
    }
    return idx;
}

void Builder::beginDyn() {
    inDyn = true;
    for (int p = 0; p < PASS_COUNT; p++) { dynTmp[p].v.clear(); dynTmp[p].idx.clear(); }
}

int Builder::endDyn() {
    inDyn = false;
    DynMesh dm;
    Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
    for (int p = 0; p < PASS_COUNT; p++) {
        u32 vbase = (u32)w.dynMesh.v.size();
        dm.first[p] = (u32)w.dynMesh.idx.size();
        dm.count[p] = (u32)dynTmp[p].idx.size();
        for (const Vtx& v : dynTmp[p].v) {
            w.dynMesh.v.push_back(v);
            lo = vmin(lo, Vec3(v.x, v.y, v.z)); hi = vmax(hi, Vec3(v.x, v.y, v.z));
        }
        for (u32 i : dynTmp[p].idx) w.dynMesh.idx.push_back(vbase + i);
        dynTmp[p].v.clear(); dynTmp[p].idx.clear();
    }
    if (lo.x > hi.x) { lo = Vec3(0, 0, 0); hi = Vec3(0, 0, 0); }
    dm.box = AABB(lo, hi);
    w.dynMeshes.push_back(dm);
    return (int)w.dynMeshes.size() - 1;
}

int Builder::addDyn(int mesh, const Mat4& xf, bool visible) {
    DynObj d;
    d.mesh = mesh; d.xf = xf; d.visible = visible;
    w.dyn.push_back(d);
    return (int)w.dyn.size() - 1;
}

void Builder::named(int dynId, int mesh, const Mat4& xf, bool visible) {
    if ((int)w.dyn.size() < DY_NAMED_COUNT) w.dyn.resize(DY_NAMED_COUNT);
    DynObj& d = w.dyn[(size_t)dynId];
    d.mesh = mesh; d.xf = xf; d.visible = visible;
}

void Builder::finish() {
    // ---- chunk the static geometry ----
    struct Tri { u32 a, b, c; };
    std::unordered_map<u32, int> keyToChunk;
    std::vector<std::vector<Tri>> buckets[PASS_COUNT];
    w.staticMesh.v.clear();
    w.staticMesh.idx.clear();
    w.chunks.clear();
    u32 vbase[PASS_COUNT];
    for (int p = 0; p < PASS_COUNT; p++) {
        vbase[p] = (u32)w.staticMesh.v.size();
        w.staticMesh.v.insert(w.staticMesh.v.end(), stat[p].v.begin(), stat[p].v.end());
    }
    for (int p = 0; p < PASS_COUNT; p++) {
        const MeshData& md = stat[p];
        for (size_t i = 0; i + 2 < md.idx.size(); i += 3) {
            const Vtx& a = md.v[md.idx[i]];
            const Vtx& b = md.v[md.idx[i + 1]];
            const Vtx& c = md.v[md.idx[i + 2]];
            float cx = (a.x + b.x + c.x) / 3.0f, cy = (a.y + b.y + c.y) / 3.0f, cz = (a.z + b.z + c.z) / 3.0f;
            int kx = (int)std::floor(cx / 16.0f) + 64, kz = (int)std::floor(cz / 16.0f) + 64;
            int ky = cy < 10.0f ? 0 : 1 + (int)std::floor((cy - 10.0f) / 12.0f);
            kx = clampv(kx, 0, 255); kz = clampv(kz, 0, 255); ky = clampv(ky, 0, 15);
            u32 key = (u32)kx | ((u32)kz << 8) | ((u32)ky << 16);
            auto it = keyToChunk.find(key);
            int ci;
            if (it == keyToChunk.end()) {
                ci = (int)w.chunks.size();
                keyToChunk[key] = ci;
                Chunk ch;
                ch.box = AABB(Vec3(1e30f, 1e30f, 1e30f), Vec3(-1e30f, -1e30f, -1e30f));
                for (int q = 0; q < PASS_COUNT; q++) { ch.first[q] = 0; ch.count[q] = 0; }
                w.chunks.push_back(ch);
                for (int q = 0; q < PASS_COUNT; q++) buckets[q].emplace_back();
            } else ci = it->second;
            buckets[p][(size_t)ci].push_back({md.idx[i] + vbase[p], md.idx[i + 1] + vbase[p], md.idx[i + 2] + vbase[p]});
            Chunk& ch = w.chunks[(size_t)ci];
            const Vtx* vs[3] = {&a, &b, &c};
            for (int k = 0; k < 3; k++) {
                Vec3 pp(vs[k]->x, vs[k]->y, vs[k]->z);
                ch.box.mn = vmin(ch.box.mn, pp); ch.box.mx = vmax(ch.box.mx, pp);
            }
        }
    }
    for (size_t ci = 0; ci < w.chunks.size(); ci++)
        for (int p = 0; p < PASS_COUNT; p++) {
            Chunk& ch = w.chunks[ci];
            ch.first[p] = (u32)w.staticMesh.idx.size();
            ch.count[p] = (u32)buckets[p][ci].size() * 3;
            for (const Tri& t : buckets[p][ci]) {
                w.staticMesh.idx.push_back(t.a); w.staticMesh.idx.push_back(t.b); w.staticMesh.idx.push_back(t.c);
            }
        }
    for (int p = 0; p < PASS_COUNT; p++) { stat[p].v.clear(); stat[p].v.shrink_to_fit(); stat[p].idx.clear(); stat[p].idx.shrink_to_fit(); }

    // ---- light candidates per chunk ----
    for (Chunk& ch : w.chunks) {
        std::vector<std::pair<float, u16>> cand;
        for (size_t li = 0; li < w.lights.size(); li++) {
            const Light& l = w.lights[li];
            Vec3 q = vmax(ch.box.mn, vmin(l.pos, ch.box.mx));
            float d = length(q - l.pos);
            if (d > l.radius) continue;
            float lum = l.color.x + l.color.y + l.color.z;
            cand.push_back({-(lum * (1.0f - d / l.radius)), (u16)li});
        }
        std::sort(cand.begin(), cand.end());
        for (size_t i = 0; i < cand.size() && i < 14; i++) ch.lights.push_back(cand[i].second);
    }

    // ---- collision broadphase ----
    w.gcell = 4.0f;
    w.gx0 = -144.0f; w.gz0 = -224.0f;
    w.gw = 72; w.gh = 90;
    w.grid.assign((size_t)w.gw * w.gh, std::vector<u16>());
    for (size_t ci = 0; ci < w.colliders.size(); ci++) {
        const AABB& bx = w.colliders[ci].box;
        int x0 = clampv((int)std::floor((bx.mn.x - w.gx0) / w.gcell), 0, w.gw - 1);
        int x1 = clampv((int)std::floor((bx.mx.x - w.gx0) / w.gcell), 0, w.gw - 1);
        int z0 = clampv((int)std::floor((bx.mn.z - w.gz0) / w.gcell), 0, w.gh - 1);
        int z1 = clampv((int)std::floor((bx.mx.z - w.gz0) / w.gcell), 0, w.gh - 1);
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) w.grid[(size_t)z * w.gw + x].push_back((u16)ci);
    }

    // ---- rain occlusion map ----
    const int RN = World::RAIN_N;
    w.rainMap.assign((size_t)RN * RN, 0);
    for (const Collider& c : w.colliders) {
        if (!(c.flags & COL_NORAIN)) continue;
        if (c.box.mx.x - c.box.mn.x < 0.9f || c.box.mx.z - c.box.mn.z < 0.9f) continue;
        if (c.box.mx.y < 1.0f) continue;
        int x0 = clampv((int)std::ceil(c.box.mn.x - w.rainX0 - 0.5f), 0, RN - 1), x1 = clampv((int)std::floor(c.box.mx.x - w.rainX0 - 0.5f), 0, RN - 1);
        int z0 = clampv((int)std::ceil(c.box.mn.z - w.rainZ0 - 0.5f), 0, RN - 1), z1 = clampv((int)std::floor(c.box.mx.z - w.rainZ0 - 0.5f), 0, RN - 1);
        u8 hv = (u8)clampv((int)(c.box.mx.y * 4.0f), 0, 255);
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) {
                u8& m = w.rainMap[(size_t)z * RN + x];
                if (hv > m) m = hv;
            }
    }

    world_build_nav(w);
}
