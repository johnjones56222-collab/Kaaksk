// THE 1982 CRANE - level: shared prop helpers, ground, the entrance, and world_build()
#include "level.h"

// ---------------------------------------------------------------------------
// Walls
// ---------------------------------------------------------------------------
void wallRun(Builder& b, int axis, float fixed, float a0, float a1, float y0, float y1, int inSign, const WallStyle& st,
             const Opening* ops, int nops) {
    float ht = st.thick * 0.5f;
    float fo0 = inSign > 0 ? fixed - ht : fixed, fo1 = inSign > 0 ? fixed : fixed + ht; // outer layer
    float fi0 = inSign > 0 ? fixed : fixed - ht, fi1 = inSign > 0 ? fixed + ht : fixed; // inner layer
    auto piece = [&](float pa0, float pa1, float py0, float py1) {
        if (pa1 - pa0 < 0.01f || py1 - py0 < 0.01f) return;
        b.m(st.matOut, st.tintOut, st.uvsOut).g(st.gradOut);
        if (axis == 0) b.box(Vec3(pa0, py0, fo0), Vec3(pa1, py1, fo1), F_ALL & ~(inSign > 0 ? F_PZ : F_NZ));
        else b.box(Vec3(fo0, py0, pa0), Vec3(fo1, py1, pa1), F_ALL & ~(inSign > 0 ? F_PX : F_NX));
        b.m(st.matIn, st.tintIn, st.uvsIn).g(0.78f);
        if (axis == 0) b.box(Vec3(pa0, py0, fi0), Vec3(pa1, py1, fi1), F_ALL & ~(inSign > 0 ? F_NZ : F_PZ));
        else b.box(Vec3(fi0, py0, pa0), Vec3(fi1, py1, pa1), F_ALL & ~(inSign > 0 ? F_NX : F_PX));
        b.g(1.0f);
        if (axis == 0) b.collider(Vec3(pa0, py0, fixed - ht), Vec3(pa1, py1, fixed + ht), CF_SOLID);
        else b.collider(Vec3(fixed - ht, py0, pa0), Vec3(fixed + ht, py1, pa1), CF_SOLID);
    };
    float cursor = a0;
    for (int i = 0; i < nops; i++) {
        const Opening& o = ops[i];
        piece(cursor, o.a0, y0, y1);
        piece(o.a0, o.a1, y0, o.y0);
        piece(o.a0, o.a1, o.y1, y1);
        if (o.window) {
            // glass
            b.m(M_GLASS, 0xffffff, 1.0f).fit();
            if (axis == 0) b.box(Vec3(o.a0, o.y0, fixed), Vec3(o.a1, o.y1, fixed + 0.004f), F_PZ);
            else b.box(Vec3(fixed, o.y0, o.a0), Vec3(fixed + 0.004f, o.y1, o.a1), F_PX);
            // frame + mullion
            b.m(M_PLAIN, 0x2a2c2a, 1.0f);
            float f = 0.045f, e = ht + 0.02f, mid = (o.a0 + o.a1) * 0.5f;
            if (axis == 0) {
                b.box(Vec3(o.a0, o.y0, fixed - e), Vec3(o.a1, o.y0 + f, fixed + e));
                b.box(Vec3(o.a0, o.y1 - f, fixed - e), Vec3(o.a1, o.y1, fixed + e));
                b.box(Vec3(o.a0, o.y0, fixed - e), Vec3(o.a0 + f, o.y1, fixed + e));
                b.box(Vec3(o.a1 - f, o.y0, fixed - e), Vec3(o.a1, o.y1, fixed + e));
                b.box(Vec3(mid - f * 0.5f, o.y0, fixed - e * 0.7f), Vec3(mid + f * 0.5f, o.y1, fixed + e * 0.7f));
                b.collider(Vec3(o.a0, o.y0, fixed - ht), Vec3(o.a1, o.y1, fixed + ht), CF_FENCE);
            } else {
                b.box(Vec3(fixed - e, o.y0, o.a0), Vec3(fixed + e, o.y0 + f, o.a1));
                b.box(Vec3(fixed - e, o.y1 - f, o.a0), Vec3(fixed + e, o.y1, o.a1));
                b.box(Vec3(fixed - e, o.y0, o.a0), Vec3(fixed + e, o.y1, o.a0 + f));
                b.box(Vec3(fixed - e, o.y0, o.a1 - f), Vec3(fixed + e, o.y1, o.a1));
                b.box(Vec3(fixed - e * 0.7f, o.y0, mid - f * 0.5f), Vec3(fixed + e * 0.7f, o.y1, mid + f * 0.5f));
                b.collider(Vec3(fixed - ht, o.y0, o.a0), Vec3(fixed + ht, o.y1, o.a1), CF_FENCE);
            }
        } else {
            // door frame
            b.m(M_PLAIN, 0x30322e, 1.0f);
            float f = 0.05f, e = ht + 0.025f;
            if (axis == 0) {
                b.box(Vec3(o.a0 - f, o.y0, fixed - e), Vec3(o.a0, o.y1 + f, fixed + e));
                b.box(Vec3(o.a1, o.y0, fixed - e), Vec3(o.a1 + f, o.y1 + f, fixed + e));
                b.box(Vec3(o.a0, o.y1, fixed - e), Vec3(o.a1, o.y1 + f, fixed + e));
            } else {
                b.box(Vec3(fixed - e, o.y0, o.a0 - f), Vec3(fixed + e, o.y1 + f, o.a0));
                b.box(Vec3(fixed - e, o.y0, o.a1), Vec3(fixed + e, o.y1 + f, o.a1 + f));
                b.box(Vec3(fixed - e, o.y1, o.a0), Vec3(fixed + e, o.y1 + f, o.a1));
            }
        }
        cursor = o.a1;
    }
    piece(cursor, a1, y0, y1);
}

// ---------------------------------------------------------------------------
// Props
// ---------------------------------------------------------------------------
void lampPost(Builder& b, float x, float z, float armYaw, int flicker, bool dead) {
    Vec3 d(std::sin(armYaw), 0, std::cos(armYaw));
    b.m(M_PAINTSTEEL, 0x59625a, 1.0f).g(0.6f);
    b.cyl(Vec3(x, 0, z), Vec3(x, 8.0f, z), 0.1f, 8, false, 0.07f);
    b.g(1.0f);
    b.m(M_RUST, 0x8a8a8a, 1.0f);
    b.box(Vec3(x - 0.22f, 0, z - 0.22f), Vec3(x + 0.22f, 0.06f, z + 0.22f));
    b.box(Vec3(x - 0.13f, 0.06f, z - 0.13f), Vec3(x + 0.13f, 0.5f, z + 0.13f));
    b.m(M_PAINTSTEEL, 0x59625a, 1.0f);
    Vec3 tip = Vec3(x, 8.25f, z) + d * 1.9f;
    b.beam(Vec3(x, 7.9f, z), tip, 0.07f);
    Vec3 fc = tip + d * 0.25f;
    Vec3 side(d.z, 0, -d.x);
    // fixture housing (axis aligned approximation)
    b.m(M_PLAIN, 0x3a3d3a, 1.0f);
    float hx = std::fabs(d.x) * 0.42f + std::fabs(side.x) * 0.17f, hz = std::fabs(d.z) * 0.42f + std::fabs(side.z) * 0.17f;
    b.box(Vec3(fc.x - hx, fc.y - 0.08f, fc.z - hz), Vec3(fc.x + hx, fc.y + 0.1f, fc.z + hz));
    b.m(M_PLAIN, 0xffc88a, 1.0f).emissive(EG_YARD, dead ? 0.0f : 1.0f);
    b.box(Vec3(fc.x - hx * 0.8f, fc.y - 0.11f, fc.z - hz * 0.8f), Vec3(fc.x + hx * 0.8f, fc.y - 0.08f, fc.z + hz * 0.8f));
    b.noEmis();
    b.collider(Vec3(x - 0.14f, 0, z - 0.14f), Vec3(x + 0.14f, 8.0f, z + 0.14f), CF_PROP);
    if (!dead) {
        int li = b.spot(fc + Vec3(0, -0.15f, 0), Vec3(0, -1, 0), 66.0f, 0xffb066, 2.4f, 26.0f, LG_YARD, flicker, 1.5f);
        b.w.lights[(size_t)li].on = false; // yard circuit is dead until the generator runs
    }
}

void makeDoor(Builder& b, int id, const Vec3& hinge, float width, float height, float yawClosed, float openAngle, int mat, u32 tint,
              int lockedBy, bool metal) {
    World& w = b.w;
    Door& d = w.doors[id];
    d.hinge = hinge; d.width = width; d.height = height; d.thick = 0.055f;
    d.yawClosed = yawClosed; d.openAngle = openAngle; d.angle = 0; d.target = 0;
    d.slide = false; d.lockedBy = lockedBy; d.metal = metal;
    b.beginDyn();
    b.m(mat, tint, 1.0f).fit();
    b.box(Vec3(0.01f, 0.01f, -d.thick * 0.5f), Vec3(width - 0.01f, height - 0.01f, d.thick * 0.5f));
    // handle + plate both sides
    b.m(M_RAIL, 0xb0b0b0, 2.0f);
    for (int s = -1; s <= 1; s += 2) {
        float z0 = s * (d.thick * 0.5f), z1 = s * (d.thick * 0.5f + 0.06f);
        b.box(Vec3(width - 0.16f, height * 0.48f, std::min(z0, z1)), Vec3(width - 0.07f, height * 0.5f, std::max(z0, z1)));
        b.box(Vec3(width - 0.17f, height * 0.44f, std::min(z0, s * (d.thick * 0.5f + 0.008f))), Vec3(width - 0.06f, height * 0.54f, std::max(z0, s * (d.thick * 0.5f + 0.008f))));
    }
    // hinges
    b.m(M_RUST, 0x808080, 2.0f);
    for (int k = 0; k < 3; k++) {
        float hy = height * (0.12f + 0.38f * k);
        b.box(Vec3(-0.015f, hy, -0.045f), Vec3(0.03f, hy + 0.1f, 0.045f));
    }
    int mesh = b.endDyn();
    d.dyn = b.addDyn(mesh, w.doorMatrix(d));
    // closed-position collider and interact volume
    Mat4 mclosed = w.doorMatrix(d);
    b.push(mclosed);
    d.collider = b.collider(Vec3(0, 0, -0.07f), Vec3(width, height, 0.07f), COL_SOLID | COL_SIGHT | COL_NAVIGNORE, SURF_METAL);
    b.interact(IK_DOOR, id, Vec3(0.02f, 0.2f, -0.16f), Vec3(width - 0.02f, height - 0.1f, 0.16f), 2.4f);
    b.pop();
}

void beginItem(Builder& b) { b.beginDyn(); }
void endItem(Builder& b, int itemId, const Vec3& pos, float yaw, float grab) {
    int mesh = b.endDyn();
    World& w = b.w;
    w.itemDyn[itemId] = b.addDyn(mesh, Mat4::translate(pos) * Mat4::rotY(yaw));
    b.interact(IK_PICKUP, itemId, pos - Vec3(grab, grab * 0.5f, grab), pos + Vec3(grab, grab, grab), 2.3f);
    w.itemPos[itemId] = pos;
}
void itemBattery(Builder& b, int itemId, const Vec3& pos, float yaw) {
    beginItem(b);
    b.m(M_CARDBOARD, 0xc8b070, 2.0f);
    b.box(Vec3(-0.05f, 0, -0.035f), Vec3(0.05f, 0.012f, 0.035f));
    for (int i = -1; i <= 1; i += 2) {
        b.m(M_PLAIN, 0xb06a18, 2.0f);
        b.cyl(Vec3(i * 0.02f, 0.03f, -0.03f), Vec3(i * 0.02f, 0.03f, 0.022f), 0.017f, 8);
        b.m(M_RAIL, 0xd0d0d0, 2.0f);
        b.cyl(Vec3(i * 0.02f, 0.03f, 0.022f), Vec3(i * 0.02f, 0.03f, 0.03f), 0.008f, 6);
        b.m(M_PLAIN, 0x101010, 2.0f);
        b.cyl(Vec3(i * 0.02f, 0.03f, -0.034f), Vec3(i * 0.02f, 0.03f, -0.03f), 0.0172f, 8);
    }
    endItem(b, itemId, pos, yaw, 0.28f);
}
void itemTape(Builder& b, int itemId, const Vec3& pos, float yaw) {
    beginItem(b);
    b.m(M_PLAIN, 0x161616, 2.0f);
    b.box(Vec3(-0.052f, 0, -0.033f), Vec3(0.052f, 0.013f, 0.033f));
    b.sign("tape", Vec3(0, 0.0131f - 0.012f, 0), Vec3(0, 1, 0), 0.1f, 0.064f);
    endItem(b, itemId, pos, yaw, 0.28f);
}
void itemFuse(Builder& b, int itemId, const Vec3& pos, float yaw) {
    beginItem(b);
    b.m(M_PLAIN, 0xd8d2c0, 2.0f);
    b.cyl(Vec3(-0.035f, 0.02f, 0), Vec3(0.035f, 0.02f, 0), 0.018f, 8);
    b.m(M_PLAIN, 0xb08a30, 2.0f);
    b.cyl(Vec3(-0.05f, 0.02f, 0), Vec3(-0.035f, 0.02f, 0), 0.02f, 8);
    b.cyl(Vec3(0.035f, 0.02f, 0), Vec3(0.05f, 0.02f, 0), 0.02f, 8);
    endItem(b, itemId, pos, yaw, 0.28f);
}

void notePaper(Builder& b, int noteId, const Vec3& pos, const Vec3& normal, float wdt, float hgt, bool clipboard) {
    Vec3 n = normalize(normal);
    Vec3 up = std::fabs(n.y) > 0.9f ? Vec3(0, 0, -1) : Vec3(0, 1, 0);
    Vec3 r = normalize(cross(up, n));
    up = cross(n, r);
    auto slab = [&](float ww, float hh, float d0, float d1) {
        Vec3 a = pos - r * (ww * 0.5f) - up * (hh * 0.5f) + n * d0;
        Vec3 c = pos + r * (ww * 0.5f) + up * (hh * 0.5f) + n * d1;
        b.box(vmin(a, c), vmax(a, c));
    };
    if (clipboard) {
        b.m(M_CARDBOARD, 0x8a6a40, 2.0f);
        slab(wdt + 0.04f, hgt + 0.06f, 0.0f, 0.008f);
        b.m(M_RAIL, 0xc0c0c0, 2.0f);
        Vec3 cp = pos + up * (hgt * 0.5f + 0.01f);
        Vec3 a = cp - r * 0.05f - up * 0.015f + n * 0.008f, c = cp + r * 0.05f + up * 0.015f + n * 0.02f;
        b.box(vmin(a, c), vmax(a, c));
    }
    b.m(M_PAPER, 0xffffff, 1.0f).fit(0.6f, 0.6f);
    slab(wdt, hgt, clipboard ? 0.008f : 0.0f, clipboard ? 0.011f : 0.004f);
    Vec3 e = r * (wdt * 0.5f + 0.08f) + up * (hgt * 0.5f + 0.08f) + n * 0.1f;
    Vec3 ea(std::fabs(e.x), std::fabs(e.y), std::fabs(e.z));
    ea = vmax(ea, Vec3(0.08f, 0.08f, 0.08f));
    b.interact(IK_NOTE, noteId, pos - ea, pos + ea, 2.2f);
    b.w.notePos[noteId] = pos;
}

void hideLocker(Builder& b, int hideIdx, const Vec3& base, float yaw, u32 tint) {
    World& w = b.w;
    const float W = 0.8f, D = 0.72f, H = 2.05f, T = 0.03f;
    Mat4 m = Mat4::translate(base) * Mat4::rotY(yaw);
    b.push(m);
    b.m(M_PAINTSTEEL, tint, 0.8f).g(0.7f);
    b.box(Vec3(-W / 2, 0, -D / 2), Vec3(-W / 2 + T, H, D / 2));       // sides
    b.box(Vec3(W / 2 - T, 0, -D / 2), Vec3(W / 2, H, D / 2));
    b.box(Vec3(-W / 2, 0, -D / 2), Vec3(W / 2, H, -D / 2 + T));       // back
    b.box(Vec3(-W / 2, H - T, -D / 2), Vec3(W / 2, H, D / 2));        // top
    b.box(Vec3(-W / 2, 0, -D / 2), Vec3(W / 2, 0.06f, D / 2));        // plinth
    b.g(1.0f);
    // interior: a coat hook and a hanging oilskin so it reads as a tight, occupied space
    b.m(M_OILSKIN, 0xffffff, 1.0f);
    b.box(Vec3(-0.3f, 0.5f, -D / 2 + T), Vec3(-0.05f, 1.75f, -D / 2 + T + 0.06f));
    b.collider(Vec3(-W / 2, 0, -D / 2), Vec3(W / 2, H, D / 2), CF_PROP, SURF_METAL);
    b.interact(IK_HIDE, hideIdx, Vec3(-W / 2, 0.3f, D / 2 - 0.1f), Vec3(W / 2, H - 0.1f, D / 2 + 0.12f), 2.2f);
    b.pop();
    // door (dynamic)
    b.beginDyn();
    b.m(M_LOCKER, tint, 1.0f).fit();
    b.box(Vec3(0, 0.06f, -0.015f), Vec3(W, H - 0.02f, 0.015f));
    int mesh = b.endDyn();
    HideSpot& h = w.hides[hideIdx];
    h.hinge = m.point(Vec3(-W / 2, 0, D / 2));
    h.doorYaw = yaw;
    h.doorAngle = 0;
    h.door = b.addDyn(mesh, Mat4::translate(h.hinge) * Mat4::rotY(yaw));
    h.inside = m.point(Vec3(0, 1.5f, -0.02f));
    h.outside = m.point(Vec3(0, 0, D / 2 + 0.55f));
    Vec3 f = m.dir(Vec3(0, 0, 1));
    h.yaw = std::atan2(f.x, -f.z);
}

void barrel(Builder& b, float x, float z, u32 tint, float y, bool collide) {
    b.m(M_PAINTSTEEL, tint, 0.8f).g(0.6f);
    b.cyl(Vec3(x, y, z), Vec3(x, y + 0.88f, z), 0.29f, 10, true);
    b.g(1.0f);
    b.m(M_RUST, 0x9a9a9a, 1.0f);
    for (int k = 0; k < 3; k++) {
        float ry = y + 0.02f + k * 0.42f;
        b.cyl(Vec3(x, ry, z), Vec3(x, ry + 0.03f, z), 0.302f, 10, false);
    }
    if (collide) b.collider(Vec3(x - 0.27f, y, z - 0.27f), Vec3(x + 0.27f, y + 0.88f, z + 0.27f), CF_PROP, SURF_METAL);
}

void pallet(Builder& b, float x, float y, float z, float yaw) {
    b.push(Mat4::translate(Vec3(x, y, z)) * Mat4::rotY(yaw));
    b.m(M_WOOD, 0xd0c8b8, 1.0f);
    for (int i = -1; i <= 1; i++) b.box(Vec3(-0.6f, 0, i * 0.44f - 0.05f), Vec3(0.6f, 0.1f, i * 0.44f + 0.05f));
    for (int i = 0; i < 6; i++) {
        float bx = -0.6f + i * 0.222f;
        b.box(Vec3(bx, 0.1f, -0.5f), Vec3(bx + 0.14f, 0.125f, 0.5f));
    }
    b.pop();
}

void crate(Builder& b, const Vec3& mn, const Vec3& mx, bool collide) {
    b.m(M_WOOD, 0xc8bca4, 1.2f).g(0.7f);
    b.box(mn, mx);
    b.g(1.0f);
    b.m(M_WOOD, 0x8a7c66, 1.2f);
    float t = 0.035f, e = 0.012f;
    // edge battens on the vertical corners and top rim
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? mx.x - t : mn.x, z = (i & 2) ? mx.z - t : mn.z;
        b.box(Vec3(x - e, mn.y, z - e), Vec3(x + t + e, mx.y + e, z + t + e));
    }
    b.box(Vec3(mn.x - e, mx.y - t, mn.z - e), Vec3(mx.x + e, mx.y + e, mn.z + t));
    b.box(Vec3(mn.x - e, mx.y - t, mx.z - t), Vec3(mx.x + e, mx.y + e, mx.z + e));
    if (collide) b.collider(mn, mx, CF_PROP, SURF_WOOD);
}

void cone(Builder& b, float x, float z) {
    b.m(M_RUBBER, 0x404040, 2.0f);
    b.box(Vec3(x - 0.17f, 0, z - 0.17f), Vec3(x + 0.17f, 0.03f, z + 0.17f));
    b.m(M_PLAIN, 0xc85a14, 2.0f);
    b.cyl(Vec3(x, 0.03f, z), Vec3(x, 0.62f, z), 0.13f, 8, true, 0.025f);
    b.m(M_PLAIN, 0xd8d8d0, 2.0f);
    b.cyl(Vec3(x, 0.3f, z), Vec3(x, 0.4f, z), 0.086f, 8, false, 0.069f);
}

void fluorescent(Builder& b, const Vec3& pos, bool alongX, int group, int flicker, float intensity) {
    float lx = alongX ? 0.62f : 0.09f, lz = alongX ? 0.09f : 0.62f;
    b.m(M_PLAIN, 0x8a8c86, 2.0f);
    b.box(Vec3(pos.x - lx, pos.y - 0.05f, pos.z - lz), Vec3(pos.x + lx, pos.y, pos.z + lz));
    b.m(M_PLAIN, 0xe8f0ff, 2.0f).emissive(group == LG_INDOOR ? EG_INDOOR : EG_ALWAYS, 1.0f);
    b.box(Vec3(pos.x - lx * 0.92f, pos.y - 0.075f, pos.z - lz * 0.6f), Vec3(pos.x + lx * 0.92f, pos.y - 0.05f, pos.z + lz * 0.6f));
    b.noEmis();
    int li = b.light(pos + Vec3(0, -0.25f, 0), 0xdfe8ff, 1.5f * intensity, 8.5f, group, flicker, 0.0f);
    if (group == LG_INDOOR) b.w.lights[(size_t)li].on = false;
}

void cagedLamp(Builder& b, const Vec3& pos, const Vec3& normal, int group, int flicker, u32 color, float intensity, float radius) {
    Vec3 n = normalize(normal);
    Vec3 c = pos + n * 0.1f;
    b.m(M_RUST, 0x707070, 2.0f);
    b.box(pos - Vec3(0.07f, 0.07f, 0.07f) + n * 0.02f, pos + Vec3(0.07f, 0.07f, 0.07f) + n * 0.02f);
    int eg = group == LG_INDOOR ? EG_INDOOR : (group == LG_YARD ? EG_YARD : (group == LG_CRANE ? EG_CRANE : EG_ALWAYS));
    b.m(M_PLAIN, color, 2.0f).emissive(eg, 1.0f);
    b.box(c - Vec3(0.045f, 0.045f, 0.045f), c + Vec3(0.045f, 0.045f, 0.045f));
    b.noEmis();
    b.m(M_RAIL, 0x909090, 2.0f);
    Vec3 u = std::fabs(n.y) > 0.9f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
    Vec3 r = normalize(cross(u, n));
    u = cross(n, r);
    for (int k = 0; k < 4; k++) {
        float a = k * PI * 0.5f + 0.4f;
        Vec3 o = (r * std::cos(a) + u * std::sin(a)) * 0.075f;
        b.beam(pos + o, c + n * 0.09f + o * 0.6f, 0.008f);
    }
    int li = b.light(c + n * 0.08f, color, intensity, radius, group, flicker, 0.5f);
    if (group == LG_INDOOR || group == LG_YARD) b.w.lights[(size_t)li].on = false;
}

// A standing worker silhouette, origin at the feet, facing +Z.
void humanFigure(Builder& b, bool hardhat) {
    b.m(M_OILSKIN, 0xffffff, 1.0f);
    b.box(Vec3(-0.17f, 0.0f, -0.09f), Vec3(-0.03f, 0.86f, 0.09f));   // legs
    b.box(Vec3(0.03f, 0.0f, -0.09f), Vec3(0.17f, 0.86f, 0.09f));
    b.box(Vec3(-0.23f, 0.80f, -0.13f), Vec3(0.23f, 1.48f, 0.13f));   // coat
    b.box(Vec3(-0.25f, 0.5f, -0.14f), Vec3(0.25f, 0.9f, 0.14f));     // coat skirt
    b.box(Vec3(-0.33f, 0.78f, -0.08f), Vec3(-0.23f, 1.44f, 0.08f));  // arms hang straight
    b.box(Vec3(0.23f, 0.78f, -0.08f), Vec3(0.33f, 1.44f, 0.08f));
    b.m(M_PLAIN, 0x1a1816, 2.0f);
    b.box(Vec3(-0.1f, 1.48f, -0.1f), Vec3(0.1f, 1.74f, 0.11f));      // head, featureless
    b.m(M_RUBBER, 0x505050, 2.0f);
    b.box(Vec3(-0.18f, 0.0f, -0.1f), Vec3(-0.02f, 0.08f, 0.2f));     // boots
    b.box(Vec3(0.02f, 0.0f, -0.1f), Vec3(0.18f, 0.08f, 0.2f));
    if (hardhat) {
        b.m(M_PAINTSTEEL, 0x8a7a22, 2.0f);
        b.cyl(Vec3(0, 1.72f, 0), Vec3(0, 1.86f, 0), 0.125f, 8, true, 0.09f);
        b.box(Vec3(-0.14f, 1.71f, -0.14f), Vec3(0.14f, 1.73f, 0.19f));
    }
}

// ---------------------------------------------------------------------------
// Ground
// ---------------------------------------------------------------------------
static void groundTiles(Builder& b, int mat, u32 tint, float uvs, float x0, float x1, float z0, float z1, float y, float step = 8.0f) {
    b.m(mat, tint, uvs);
    for (float z = z0; z < z1 - 0.01f; z += step)
        for (float x = x0; x < x1 - 0.01f; x += step)
            b.box(Vec3(x, y - 0.2f, z), Vec3(std::min(x + step, x1), y, std::min(z + step, z1)), F_PY);
}

void build_ground(Builder& b) {
    // visual ground
    groundTiles(b, M_ASPHALT, 0xffffff, 0.25f, -84, 84, -22, 58, 0.0f);
    groundTiles(b, M_CONCRETE, 0xffffff, 0.25f, -84, 84, QUAY_Z, -22, 0.0f);
    groundTiles(b, M_ASPHALT, 0xe8e8e8, 0.25f, -12, 12, 58, 100, 0.0f);
    groundTiles(b, M_GRAVEL, 0xffffff, 0.3f, -148, -12, 58, 100, 0.0f, 16.0f);
    groundTiles(b, M_GRAVEL, 0xffffff, 0.3f, 12, 148, 58, 100, 0.0f, 16.0f);
    groundTiles(b, M_GRAVEL, 0xd0d0d0, 0.3f, -148, -84, QUAY_Z, 58, 0.0f, 16.0f);
    groundTiles(b, M_GRAVEL, 0xd0d0d0, 0.3f, 84, 148, QUAY_Z, 58, 0.0f, 16.0f);
    // physical ground
    b.collider(Vec3(-150, -4, -22), Vec3(150, 0, 100), COL_SOLID | COL_SIGHT, SURF_ASPHALT);
    b.collider(Vec3(-150, -4, QUAY_Z), Vec3(150, 0, -22), COL_SOLID | COL_SIGHT, SURF_CONCRETE);
    // the world ends just beyond the road outside the gate
    b.collider(Vec3(-20, 0, 84), Vec3(20, 6, 85), COL_SOLID);
    b.collider(Vec3(-17, 0, 56.4f), Vec3(-16, 6, 85), COL_SOLID);
    b.collider(Vec3(16, 0, 56.4f), Vec3(17, 6, 85), COL_SOLID);
    b.collider(Vec3(-17, 0, 56.2f), Vec3(-4.9f, 6, 56.6f), COL_SOLID);
    b.collider(Vec3(9.0f, 0, 56.2f), Vec3(17, 6, 56.6f), COL_SOLID);
    b.collider(Vec3(-83, 0, QUAY_Z - 1), Vec3(-82, 6, -20), COL_SOLID);
    b.collider(Vec3(82, 0, QUAY_Z - 1), Vec3(83, 6, -20), COL_SOLID);

    // quay wall down to the water, with growth below the tide line
    b.m(M_CONCWALL, 0xd0d0d0, 0.25f);
    for (float x = -148; x < 148; x += 16) b.box(Vec3(x, -1.5f, QUAY_Z - 0.5f), Vec3(x + 16, 0.0f, QUAY_Z), F_NZ | F_PY);
    b.m(M_KELP, 0xffffff, 0.3f);
    for (float x = -148; x < 148; x += 16) b.box(Vec3(x, -5.0f, QUAY_Z - 0.56f), Vec3(x + 16, -1.5f, QUAY_Z), F_NZ);
    // kerb along the quay edge (gap for the gangway)
    b.m(M_HAZARD, 0xd0d0d0, 0.5f);
    b.box(Vec3(-82, 0, QUAY_Z + 0.25f), Vec3(25.6f, 0.38f, QUAY_Z + 0.65f));
    b.box(Vec3(28.4f, 0, QUAY_Z + 0.25f), Vec3(82, 0.38f, QUAY_Z + 0.65f));
    b.collider(Vec3(-82, 0, QUAY_Z - 0.2f), Vec3(25.6f, 1.3f, QUAY_Z + 0.65f), CF_LOW);
    b.collider(Vec3(28.4f, 0, QUAY_Z - 0.2f), Vec3(82, 1.3f, QUAY_Z + 0.65f), CF_LOW);

    // road paint
    for (float z = 50; z > -18; z -= 7.0f) b.floorDecal("lane", Vec3(0, 0, z), 0.22f, 3.2f);
    for (float z = 54; z > -20; z -= 3.4f) {
        b.floorDecal("laneY", Vec3(-4.9f, 0, z), 0.16f, 3.4f);
        b.floorDecal("laneY", Vec3(4.9f, 0, z), 0.16f, 3.4f);
    }
    b.floorDecal("arrowp", Vec3(2.4f, 0, 47), 1.2f, 3.2f);
    b.floorDecal("arrowp", Vec3(-2.4f, 0, 12), 1.2f, 3.2f, PI);
    b.floorDecal("berth9", Vec3(0, 0, -19.0f), 8.5f, 2.4f);
    // grime: oil, cracks, standing water
    Rng r(8211);
    static const char* stains[4] = {"oil1", "oil2", "puddle", "crack"};
    for (int i = 0; i < 150; i++) {
        float x, z;
        int kind = r.irange(0, 3);
        if (i < 50) { x = r.range(-9.0f, 9.0f); z = r.range(-20.0f, 78.0f); }           // road
        else if (i < 100) { x = r.range(-80.0f, 80.0f); z = r.range(QUAY_Z + 2.0f, -23.0f); } // quay
        else { x = r.range(-22.0f, 80.0f); z = r.range(-20.0f, 54.0f); }               // east yard
        float s = r.range(1.0f, 3.4f);
        if (kind == 3) b.floorDecal("crack", Vec3(x, 0, z), s * 1.6f, s * 0.8f, r.range(0.0f, TAU));
        else b.floorDecal(stains[kind], Vec3(x, 0, z), s, s * r.range(0.7f, 1.2f), r.range(0.0f, TAU));
    }
    // drains and manhole covers
    static const float drains[][2] = {{3.6f, 30}, {-3.6f, 2}, {22, -27}, {-30, -27}, {48, 12}, {-14, 40}, {60, -27}, {-58, -27}};
    for (auto& d : drains) {
        b.m(M_GRATE, 0x909090, 1.0f);
        b.box(Vec3(d[0] - 0.4f, 0.0f, d[1] - 0.3f), Vec3(d[0] + 0.4f, 0.012f, d[1] + 0.3f), F_PY);
        b.m(M_RUST, 0x606060, 1.0f);
        b.box(Vec3(d[0] - 0.46f, 0.0f, d[1] - 0.36f), Vec3(d[0] + 0.46f, 0.008f, d[1] + 0.36f), F_PY);
    }
    b.m(M_DIAMOND, 0xa0a0a0, 1.0f);
    b.cyl(Vec3(-2.2f, 0, 38), Vec3(-2.2f, 0.015f, 38), 0.42f, 12, true);
    b.cyl(Vec3(30, 0, -25), Vec3(30, 0.015f, -25), 0.42f, 12, true);
    b.cyl(Vec3(-20, 0, 6), Vec3(-20, 0.015f, 6), 0.42f, 12, true);

    b.trigger(TR_WATER, Vec3(-200, -30, -300), Vec3(200, -1.4f, 200));
}

// ---------------------------------------------------------------------------
// Fences / entrance
// ---------------------------------------------------------------------------
static void fenceRun(Builder& b, float x0, float z0, float x1, float z1, float h = 3.0f) {
    Vec3 a(x0, 0, z0), c(x1, 0, z1);
    float len = length(c - a);
    Vec3 d = (c - a) * (1.0f / len);
    int n = std::max(1, (int)std::ceil(len / 4.0f));
    b.m(M_PAINTSTEEL, 0x6a6e68, 1.0f).g(0.6f);
    for (int i = 0; i <= n; i++) {
        Vec3 p = a + d * (len * i / n);
        b.cyl(p, p + Vec3(0, h + 0.1f, 0), 0.045f, 6, true);
        // outward-leaning barbed wire arm
        b.beam(p + Vec3(0, h, 0), p + Vec3(d.z * 0.3f, h + 0.42f, -d.x * 0.3f), 0.03f);
    }
    b.g(1.0f);
    b.beam(a + Vec3(0, h, 0), c + Vec3(0, h, 0), 0.04f);
    b.beam(a + Vec3(0, 0.08f, 0), c + Vec3(0, 0.08f, 0), 0.03f);
    b.m(M_RUST, 0x808080, 2.0f);
    for (int k = 0; k < 3; k++) {
        float t = (k + 1) / 3.0f;
        Vec3 o(d.z * 0.3f * t, h + 0.42f * t, -d.x * 0.3f * t);
        b.beam(a + o, c + o, 0.012f);
    }
    // mesh
    b.m(M_CHAINLINK, 0xffffff, 1.0f);
    for (int i = 0; i < n; i++) {
        Vec3 p0 = a + d * (len * i / n), p1 = a + d * (len * (i + 1) / n);
        float u0 = len * i / n * 1.4f, u1 = len * (i + 1) / n * 1.4f;
        b.quad(p0 + Vec3(0, 0.08f, 0), p1 + Vec3(0, 0.08f, 0), p1 + Vec3(0, h, 0), p0 + Vec3(0, h, 0), Vec2(u0, h * 1.4f), Vec2(u1, h * 1.4f),
               Vec2(u1, 0), Vec2(u0, 0), 0.75f, 0.75f, 1, 1);
    }
    Vec3 mn = vmin(a, c) - Vec3(0.08f, 0, 0.08f), mx = vmax(a, c) + Vec3(0.08f, h + 0.4f, 0.08f);
    b.collider(mn, mx, CF_FENCE, SURF_METAL);
}

static void buildTruck(Builder& b, const Vec3& pos) {
    World& w = b.w;
    w.truckPos = pos;
    b.push(Mat4::translate(pos));
    const u32 body = 0x3b5656;
    // wheels
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? 0.86f : -0.86f, z = (i & 2) ? 1.62f : -1.72f;
        b.m(M_RUBBER, 0xffffff, 1.0f);
        b.cyl(Vec3(x - 0.13f, 0.38f, z), Vec3(x + 0.13f, 0.38f, z), 0.38f, 12, true);
        b.m(M_RAIL, 0x9a9a9a, 1.0f);
        b.cyl(Vec3(x - 0.14f, 0.38f, z), Vec3(x + 0.14f, 0.38f, z), 0.2f, 8, true);
    }
    b.m(M_RUST, 0x555555, 1.0f);
    b.box(Vec3(-0.8f, 0.34f, -2.45f), Vec3(0.8f, 0.52f, 2.55f)); // frame
    b.m(M_PAINTSTEEL, body, 0.7f).g(0.75f);
    b.box(Vec3(-0.93f, 0.5f, -2.5f), Vec3(0.93f, 1.12f, -1.2f));  // hood
    b.box(Vec3(-0.95f, 0.5f, -1.2f), Vec3(0.95f, 1.18f, 0.32f));  // cab lower
    b.box(Vec3(-0.9f, 1.76f, -0.95f), Vec3(0.9f, 1.84f, 0.3f));   // roof
    b.box(Vec3(-0.93f, 0.52f, 0.36f), Vec3(0.93f, 0.62f, 2.56f)); // bed floor
    b.box(Vec3(-0.93f, 0.62f, 0.36f), Vec3(-0.87f, 1.12f, 2.56f));
    b.box(Vec3(0.87f, 0.62f, 0.36f), Vec3(0.93f, 1.12f, 2.56f));
    b.box(Vec3(-0.87f, 0.62f, 0.36f), Vec3(0.87f, 1.12f, 0.42f));
    b.box(Vec3(-0.87f, 0.56f, 2.56f), Vec3(0.87f, 0.62f, 3.05f)); // dropped tailgate
    b.g(1.0f);
    // cab pillars and back wall
    b.box(Vec3(-0.9f, 1.18f, 0.22f), Vec3(0.9f, 1.76f, 0.3f));
    for (int s = -1; s <= 1; s += 2) {
        b.box(Vec3(s * 0.9f - 0.04f, 1.18f, -0.98f), Vec3(s * 0.9f + 0.04f, 1.76f, -0.9f));
        b.box(Vec3(s * 0.9f - 0.04f, 1.18f, -0.3f), Vec3(s * 0.9f + 0.04f, 1.76f, -0.22f));
    }
    // glass
    b.m(M_GLASS, 0xffffff, 1.0f).fit();
    b.box(Vec3(-0.86f, 1.18f, -0.96f), Vec3(0.86f, 1.76f, -0.955f), F_NZ);
    b.box(Vec3(-0.9f, 1.18f, -0.9f), Vec3(-0.895f, 1.76f, 0.22f), F_NX);
    b.box(Vec3(0.895f, 1.18f, -0.9f), Vec3(0.9f, 1.76f, 0.22f), F_PX);
    // dark interior so the cab does not look hollow
    b.m(M_PLAIN, 0x0c0c0c, 1.0f);
    b.box(Vec3(-0.84f, 1.18f, -0.9f), Vec3(0.84f, 1.5f, 0.2f));
    // grille, bumper, lights
    b.m(M_PLAIN, 0x141414, 2.0f);
    b.box(Vec3(-0.6f, 0.62f, -2.53f), Vec3(0.6f, 1.02f, -2.5f));
    b.m(M_RAIL, 0xc0c0c0, 1.0f);
    b.box(Vec3(-0.98f, 0.42f, -2.66f), Vec3(0.98f, 0.6f, -2.5f));
    b.box(Vec3(-0.98f, 0.42f, 2.56f), Vec3(0.98f, 0.58f, 2.68f));
    b.m(M_PLAIN, 0xfff2c8, 2.0f).emissive(EG_ALWAYS, 1.0f);
    b.box(Vec3(-0.84f, 0.74f, -2.545f), Vec3(-0.62f, 0.96f, -2.5f));
    b.box(Vec3(0.62f, 0.74f, -2.545f), Vec3(0.84f, 0.96f, -2.5f));
    b.m(M_PLAIN, 0xb01008, 2.0f).emissive(EG_ALWAYS, 0.5f);
    b.box(Vec3(-0.92f, 0.7f, 2.56f), Vec3(-0.78f, 0.9f, 2.585f));
    b.box(Vec3(0.78f, 0.7f, 2.56f), Vec3(0.92f, 0.9f, 2.585f));
    b.noEmis();
    // side mirrors, toolbox in the bed, a coil of rope
    b.m(M_PLAIN, 0x202020, 2.0f);
    b.box(Vec3(-1.12f, 1.3f, -0.92f), Vec3(-0.95f, 1.5f, -0.86f));
    b.box(Vec3(0.95f, 1.3f, -0.92f), Vec3(1.12f, 1.5f, -0.86f));
    b.m(M_MACHINE, 0x8a2018, 1.0f);
    b.box(Vec3(-0.8f, 0.62f, 0.46f), Vec3(0.8f, 1.0f, 0.9f));
    b.m(M_TARP, 0x4a5a3a, 1.0f);
    b.box(Vec3(-0.7f, 0.62f, 1.2f), Vec3(0.5f, 0.82f, 2.3f));
    b.sign("local114", Vec3(-0.951f, 0.86f, -0.45f), Vec3(-1, 0, 0), 0.9f);
    b.sign("local114", Vec3(0.951f, 0.86f, -0.45f), Vec3(1, 0, 0), 0.9f);
    b.collider(Vec3(-0.98f, 0, -2.66f), Vec3(0.98f, 1.2f, 2.68f), CF_PROP, SURF_METAL);
    b.collider(Vec3(-0.95f, 1.2f, -1.0f), Vec3(0.95f, 1.84f, 0.32f), CF_PROP, SURF_METAL);
    // headlights
    w.lightTruckL = b.spot(Vec3(-0.73f, 0.85f, -2.62f), Vec3(-0.03f, -0.1f, -1), 30.0f, 0xfff0c8, 2.6f, 36.0f, LG_ALWAYS, FL_NONE, 0.55f);
    w.lightTruckR = b.spot(Vec3(0.73f, 0.85f, -2.62f), Vec3(0.03f, -0.1f, -1), 30.0f, 0xfff0c8, 2.6f, 36.0f, LG_ALWAYS, FL_NONE, 0.55f);
    b.light(Vec3(0, 0.9f, 3.0f), 0xff2010, 0.5f, 4.0f, LG_ALWAYS, FL_NONE, 0.0f);
    b.interact(IK_USE, U_TRUCK, Vec3(-1.3f, 0.3f, -1.4f), Vec3(1.3f, 1.9f, 0.5f), 2.6f);
    b.pop();
    // flashlight on the hood
    beginItem(b);
    b.m(M_PLAIN, 0x8a1c14, 2.0f);
    b.cyl(Vec3(-0.09f, 0.024f, 0), Vec3(0.07f, 0.024f, 0), 0.022f, 8);
    b.m(M_RAIL, 0xc8c8c8, 2.0f);
    b.cyl(Vec3(0.07f, 0.024f, 0), Vec3(0.12f, 0.024f, 0), 0.022f, 8, true, 0.036f);
    b.m(M_PLAIN, 0xfff0c0, 2.0f).emissive(EG_ALWAYS, 0.35f);
    b.cyl(Vec3(0.12f, 0.024f, 0), Vec3(0.123f, 0.024f, 0), 0.032f, 8);
    b.noEmis();
    endItem(b, IT_FLASHLIGHT, pos + Vec3(-0.45f, 1.125f, -1.75f), 0.6f, 0.35f);
}

void build_entrance(Builder& b) {
    World& w = b.w;
    // perimeter
    fenceRun(b, -82, 56, -5.0f, 56);
    fenceRun(b, 5.0f, 56, 82, 56);
    fenceRun(b, -82, 56, -82, QUAY_Z + 0.6f);
    fenceRun(b, 82, 56, 82, QUAY_Z + 0.6f);

    // gate posts, arch and sign
    b.m(M_PAINTSTEEL, 0x5a5f58, 0.8f).g(0.6f);
    b.solid(Vec3(-5.25f, 0, 55.8f), Vec3(-4.85f, 4.8f, 56.2f), SURF_METAL, CF_PROP);
    b.solid(Vec3(4.85f, 0, 55.8f), Vec3(5.25f, 4.8f, 56.2f), SURF_METAL, CF_PROP);
    b.g(1.0f);
    b.box(Vec3(-5.25f, 4.5f, 55.85f), Vec3(5.25f, 4.8f, 56.15f));
    b.m(M_RUST, 0x707070, 1.0f);
    b.box(Vec3(-4.3f, 4.8f, 55.95f), Vec3(4.3f, 6.5f, 56.05f));
    b.sign("gate", Vec3(0, 5.65f, 56.05f), Vec3(0, 0, 1), 8.4f);
    b.sign("gate", Vec3(0, 5.65f, 55.95f), Vec3(0, 0, -1), 8.4f);
    b.sign("auth", Vec3(-6.6f, 1.7f, 56.1f), Vec3(0, 0, 1), 1.5f);
    b.sign("hardhat", Vec3(-8.4f, 1.7f, 56.1f), Vec3(0, 0, 1), 1.1f);
    b.sign("danger_load", Vec3(10.6f, 1.75f, 56.1f), Vec3(0, 0, 1), 1.6f);

    // sliding gate leaf (dynamic)
    {
        b.beginDyn();
        b.m(M_PAINTSTEEL, 0x5a5f58, 1.0f);
        b.beam(Vec3(0, 0.12f, 0), Vec3(9.7f, 0.12f, 0), 0.08f);
        b.beam(Vec3(0, 2.9f, 0), Vec3(9.7f, 2.9f, 0), 0.08f);
        for (int i = 0; i <= 4; i++) b.beam(Vec3(9.7f * i / 4.0f, 0.12f, 0), Vec3(9.7f * i / 4.0f, 2.9f, 0), 0.07f);
        b.beam(Vec3(0, 0.12f, 0), Vec3(9.7f, 2.9f, 0), 0.04f);
        b.m(M_RUBBER, 0x808080, 2.0f);
        b.cyl(Vec3(0.6f, 0.09f, -0.04f), Vec3(0.6f, 0.09f, 0.04f), 0.09f, 8);
        b.cyl(Vec3(9.1f, 0.09f, -0.04f), Vec3(9.1f, 0.09f, 0.04f), 0.09f, 8);
        b.m(M_CHAINLINK, 0xffffff, 1.0f);
        b.quad(Vec3(0, 0.12f, 0), Vec3(9.7f, 0.12f, 0), Vec3(9.7f, 2.9f, 0), Vec3(0, 2.9f, 0), Vec2(0, 4), Vec2(13.6f, 4), Vec2(13.6f, 0), Vec2(0, 0));
        // chain and padlock that were cut long ago still hang from the frame
        b.m(M_RUST, 0x909090, 2.0f);
        b.beam(Vec3(9.4f, 1.3f, 0.05f), Vec3(9.5f, 0.75f, 0.06f), 0.025f);
        int mesh = b.endDyn();
        b.named(DY_GATE, mesh, Mat4::translate(Vec3(-4.85f, 0, 56.0f)));
    }
    b.m(M_RAIL, 0x808080, 1.0f);
    b.box(Vec3(-15.0f, 0.0f, 55.96f), Vec3(5.0f, 0.03f, 56.04f)); // ground track
    w.gateCollider = b.collider(Vec3(-4.85f, 0, 55.9f), Vec3(4.85f, 3.2f, 56.1f), COL_SOLID | COL_NAVIGNORE, SURF_METAL);

    // guard booth ------------------------------------------------------------
    const float bx0 = 5.6f, bx1 = 8.8f, bz0 = 57.0f, bz1 = 60.2f, fy = 0.12f, top = 2.7f;
    b.m(M_CONCRETE, 0xb0b0b0, 0.5f);
    b.solid(Vec3(bx0 - 0.15f, 0, bz0 - 0.15f), Vec3(bx1 + 0.15f, fy, bz1 + 0.15f), SURF_CONCRETE, COL_SOLID | COL_SIGHT);
    b.m(M_LINO, 0xb0b0b0, 0.5f);
    b.box(Vec3(bx0, fy, bz0), Vec3(bx1, fy + 0.005f, bz1), F_PY);
    WallStyle bs = {M_PAINTSTEEL, 0x8f9a90, 0.6f, M_PANEL, 0xffffff, 0.6f, 0.14f, 0.6f};
    Opening wW[2] = {{58.0f, 59.1f, fy, 2.15f, false}, {59.35f, 60.0f, 1.1f, 2.05f, true}};
    wallRun(b, 1, bx0, bz0, bz1, fy, top, +1, bs, wW, 2);
    Opening wS[1] = {{6.2f, 8.2f, 1.1f, 2.05f, true}};
    wallRun(b, 0, bz1, bx0, bx1, fy, top, -1, bs, wS, 1);
    Opening wN[1] = {{7.2f, 8.4f, 1.1f, 2.05f, true}};
    wallRun(b, 0, bz0, bx0, bx1, fy, top, +1, bs, wN, 1);
    wallRun(b, 1, bx1, bz0, bz1, fy, top, -1, bs);
    b.m(M_ROOF, 0xffffff, 0.5f);
    b.solid(Vec3(bx0 - 0.35f, top, bz0 - 0.35f), Vec3(bx1 + 0.35f, top + 0.14f, bz1 + 0.35f), SURF_METAL);
    b.m(M_CEILING, 0xc0c0c0, 0.5f);
    b.box(Vec3(bx0, top - 0.01f, bz0), Vec3(bx1, top, bz1), F_NY);
    makeDoor(b, D_BOOTH, Vec3(bx0, fy, 58.0f), 1.1f, 2.03f, -PI * 0.5f, -1.45f, M_PAINTSTEEL, 0x7a857c, -1, true);
    // desk along the east wall
    b.m(M_WOOD, 0xb09a80, 1.0f).g(0.7f);
    b.box(Vec3(8.02f, fy + 0.74f, 57.25f), Vec3(8.72f, fy + 0.79f, 59.95f));
    b.box(Vec3(8.06f, fy, 57.3f), Vec3(8.7f, fy + 0.74f, 57.8f));
    b.box(Vec3(8.06f, fy, 59.4f), Vec3(8.7f, fy + 0.74f, 59.9f));
    b.g(1.0f);
    b.collider(Vec3(8.02f, fy, 57.25f), Vec3(8.72f, fy + 0.8f, 59.95f), CF_PROP, SURF_WOOD);
    float dy = fy + 0.79f;
    notePaper(b, N_DUTY_LOG, Vec3(8.3f, dy + 0.001f, 58.75f), Vec3(0, 1, 0), 0.3f, 0.4f, true);
    // battery lantern
    b.m(M_PAINTSTEEL, 0x8a2018, 2.0f);
    b.cyl(Vec3(8.38f, dy, 58.05f), Vec3(8.38f, dy + 0.06f, 58.05f), 0.06f, 8);
    b.cyl(Vec3(8.38f, dy + 0.2f, 58.05f), Vec3(8.38f, dy + 0.24f, 58.05f), 0.06f, 8);
    b.m(M_PLAIN, 0xffd8a0, 2.0f).emissive(EG_ALWAYS, 0.9f);
    b.cyl(Vec3(8.38f, dy + 0.06f, 58.05f), Vec3(8.38f, dy + 0.2f, 58.05f), 0.045f, 8, false);
    b.noEmis();
    b.light(Vec3(8.3f, dy + 0.3f, 58.05f), 0xffc890, 1.25f, 6.5f, LG_ALWAYS, FL_BUZZ, 0.35f);
    // dead security monitor
    b.m(M_PLAIN, 0x4a4a44, 2.0f);
    b.box(Vec3(8.2f, dy, 57.42f), Vec3(8.66f, dy + 0.36f, 57.86f));
    b.m(M_SCREEN, 0xffffff, 1.0f).fit();
    b.box(Vec3(8.19f, dy + 0.05f, 57.47f), Vec3(8.2f, dy + 0.32f, 57.81f), F_NX);
    // radio handset on its charger
    b.m(M_PLAIN, 0x202020, 2.0f);
    b.box(Vec3(8.3f, dy, 59.42f), Vec3(8.46f, dy + 0.03f, 59.56f));
    beginItem(b);
    b.m(M_PLAIN, 0x23261f, 2.0f);
    b.box(Vec3(-0.035f, 0, -0.02f), Vec3(0.035f, 0.17f, 0.02f));
    b.cyl(Vec3(0.02f, 0.17f, 0), Vec3(0.02f, 0.3f, 0), 0.006f, 5);
    b.m(M_PLAIN, 0x101010, 2.0f);
    b.box(Vec3(-0.028f, 0.1f, -0.024f), Vec3(0.028f, 0.15f, -0.02f));
    b.m(M_PLAIN, 0xff4a20, 2.0f).emissive(EG_ALWAYS, 0.8f);
    b.box(Vec3(-0.025f, 0.155f, -0.022f), Vec3(-0.015f, 0.165f, -0.02f));
    b.noEmis();
    endItem(b, IT_RADIO, Vec3(8.38f, dy + 0.03f, 59.49f), 0.3f, 0.3f);
    // gate release lever on the north wall
    b.m(M_MACHINE, 0x8a8a80, 1.5f);
    b.box(Vec3(6.2f, 1.0f, 57.07f), Vec3(6.6f, 1.55f, 57.2f));
    b.sign("gaterel", Vec3(6.4f, 1.78f, 57.075f), Vec3(0, 0, 1), 0.44f);
    b.beginDyn();
    b.m(M_RAIL, 0xb0b0b0, 2.0f);
    b.beam(Vec3(0, 0, 0), Vec3(0, 0.34f, 0), 0.03f);
    b.m(M_PLAIN, 0xb01810, 2.0f);
    b.cyl(Vec3(-0.05f, 0.34f, 0), Vec3(0.05f, 0.34f, 0), 0.024f, 8);
    b.named(DY_GATE_LEVER, b.endDyn(), Mat4::translate(Vec3(6.4f, 1.2f, 57.22f)));
    b.interact(IK_USE, U_GATE_LEVER, Vec3(6.15f, 0.95f, 57.07f), Vec3(6.65f, 1.7f, 57.4f), 2.3f);
    // wall dressing
    b.sign("calendar", Vec3(8.725f, 1.75f, 59.1f), Vec3(-1, 0, 0), 0.48f);
    b.sign("keys", Vec3(6.0f, 1.9f, 60.125f), Vec3(0, 0, -1), 0.34f);
    b.m(M_WOOD, 0x70604a, 2.0f);
    b.box(Vec3(5.8f, 1.5f, 60.1f), Vec3(6.2f, 1.75f, 60.13f));
    b.m(M_RAIL, 0xc0b060, 2.0f);
    for (int k = 0; k < 4; k++) b.box(Vec3(5.85f + k * 0.09f, 1.52f, 60.07f), Vec3(5.87f + k * 0.09f, 1.6f, 60.1f));
    // stool and heater
    b.m(M_PLAIN, 0x2a2a2a, 2.0f);
    b.cyl(Vec3(7.3f, fy, 58.6f), Vec3(7.3f, fy + 0.55f, 58.6f), 0.03f, 6);
    b.cyl(Vec3(7.3f, fy + 0.55f, 58.6f), Vec3(7.3f, fy + 0.6f, 58.6f), 0.17f, 10);
    b.collider(Vec3(7.15f, fy, 58.45f), Vec3(7.45f, fy + 0.6f, 58.75f), CF_PROP);
    b.m(M_MACHINE, 0x6a6a60, 2.0f);
    b.box(Vec3(5.8f, fy, 57.2f), Vec3(6.0f, fy + 0.45f, 57.75f));
    b.zone(Z_BOOTH, Vec3(bx0, 0, bz0), Vec3(bx1, top, bz1));
    // outside the booth: a clipboard sign and a dead exterior lamp
    b.sign("nosmoke", Vec3(bx0 - 0.075f, 1.9f, 57.55f), Vec3(-1, 0, 0), 0.4f);
    cagedLamp(b, Vec3(bx0 - 0.07f, 2.35f, 58.55f), Vec3(-1, 0, 0), LG_YARD, FL_BUZZ, 0xffc080, 1.0f, 6.0f);

    // truck and the road outside
    buildTruck(b, Vec3(-3.3f, 0, 67.4f));
    cone(b, 3.4f, 62.0f);
    cone(b, 3.9f, 63.5f);
    cone(b, -9.0f, 58.2f);
    // concrete barriers flanking the approach
    b.m(M_CONCWALL, 0xb8b8b8, 0.5f).g(0.6f);
    for (int i = 0; i < 4; i++) {
        float z = 60.5f + i * 3.6f;
        b.solid(Vec3(-12.4f, 0, z), Vec3(-11.8f, 0.85f, z + 3.2f), SURF_CONCRETE, CF_PROP);
        b.solid(Vec3(11.8f, 0, z), Vec3(12.4f, 0.85f, z + 3.2f), SURF_CONCRETE, CF_PROP);
    }
    b.g(1.0f);
    // mailbox post with a rain-soaked newspaper
    b.m(M_WOOD, 0x8a7a66, 2.0f);
    b.box(Vec3(9.6f, 0, 61.0f), Vec3(9.7f, 1.1f, 61.1f));
    b.m(M_PAINTSTEEL, 0x505850, 2.0f);
    b.box(Vec3(9.45f, 1.1f, 60.85f), Vec3(9.85f, 1.35f, 61.25f));
    b.collider(Vec3(9.45f, 0, 60.85f), Vec3(9.85f, 1.35f, 61.25f), CF_PROP);

    lampPost(b, -9.5f, 52.5f, PI * 0.5f, FL_BUZZ);

    b.trigger(TR_YARD_ENTER, Vec3(-5, 0, 44), Vec3(5, 4, 54));
    b.trigger(TR_ROAD_MID, Vec3(-6, 0, 16), Vec3(6, 4, 26));
    b.trigger(TR_GATE_EXIT, Vec3(-5, 0, 56.3f), Vec3(5, 4, 60));
    b.trigger(TR_OUTSIDE, Vec3(-16, 0, 60), Vec3(16, 4, 84));
    w.playerStart = Vec3(-5.6f, 0, 66.2f);
    w.playerStartYaw = 0.35f;
}

// ---------------------------------------------------------------------------
// Far backdrop: things seen only as silhouettes and lights
// ---------------------------------------------------------------------------
void build_backdrop(Builder& b) {
    // warehouses beyond the east and west fences
    b.m(M_CORRUGATED, 0x3a3e40, 0.2f).g(0.5f);
    b.box(Vec3(98, 0, -30), Vec3(140, 14, 40), F_ALL & ~F_NY);
    b.box(Vec3(-146, 0, -10), Vec3(-100, 11, 50), F_ALL & ~F_NY);
    b.box(Vec3(-140, 0, -46), Vec3(-108, 18, -22), F_ALL & ~F_NY);
    b.g(1.0f);
    // far shore across the water: a low dark line with a few lights
    b.m(M_PLAIN, 0x050608, 0.1f);
    b.box(Vec3(-400, WATER_Y, -420), Vec3(400, 6.0f, -400), F_PZ | F_PY);
    b.m(M_PLAIN, 0xffc890, 1.0f).emissive(EG_ALWAYS, 1.0f);
    Rng r(4242);
    for (int i = 0; i < 46; i++) {
        float x = r.range(-380.0f, 380.0f), y = r.range(2.0f, 9.0f);
        float s = r.range(0.35f, 0.8f);
        b.box(Vec3(x - s, y - s, -399.6f), Vec3(x + s, y + s, -399.5f), F_PZ);
    }
    b.m(M_PLAIN, 0xff3020, 1.0f).emissive(EG_BEACON, 1.0f);
    b.box(Vec3(120, 30, -399.6f), Vec3(122, 32, -399.5f), F_PZ);
    b.box(Vec3(-210, 44, -399.6f), Vec3(-208, 46, -399.5f), F_PZ);
    b.noEmis();
    // channel buoys
    for (int i = 0; i < 3; i++) {
        float x = -60.0f + i * 55.0f, z = -105.0f - i * 14.0f;
        b.m(M_PAINTSTEEL, i == 1 ? 0x2a5a30 : 0x7a1c14, 1.0f);
        b.cyl(Vec3(x, WATER_Y - 0.3f, z), Vec3(x, WATER_Y + 1.2f, z), 0.7f, 8, true, 0.45f);
        b.beam(Vec3(x, WATER_Y + 1.2f, z), Vec3(x, WATER_Y + 3.0f, z), 0.08f);
        b.m(M_PLAIN, i == 1 ? 0x40ff60 : 0xff3020, 1.0f).emissive(EG_BEACON, 1.0f);
        b.boxC(Vec3(x, WATER_Y + 3.1f, z), Vec3(0.12f, 0.12f, 0.12f));
        b.noEmis();
    }
    // telegraph poles along the road south of the gate
    b.m(M_WOOD, 0x5a5048, 1.0f);
    for (int i = 0; i < 4; i++) {
        float z = 62.0f + i * 11.0f;
        b.cyl(Vec3(-14.2f, 0, z), Vec3(-14.2f, 8.5f, z), 0.12f, 6);
        b.box(Vec3(-15.0f, 7.8f, z - 0.05f), Vec3(-13.4f, 7.9f, z + 0.05f));
    }
    b.m(M_PLAIN, 0x0a0a0a, 1.0f);
    for (int i = 0; i < 3; i++) {
        float z0 = 62.0f + i * 11.0f, z1 = z0 + 11.0f;
        for (int k = -1; k <= 1; k += 2)
            for (int sgm = 0; sgm < 4; sgm++) {
                float t0 = sgm / 4.0f, t1 = (sgm + 1) / 4.0f;
                float s0 = 0.5f * (4 * t0 * (1 - t0)), s1 = 0.5f * (4 * t1 * (1 - t1));
                b.beam(Vec3(-14.2f + k * 0.7f, 7.9f - s0, lerpf(z0, z1, t0)), Vec3(-14.2f + k * 0.7f, 7.9f - s1, lerpf(z0, z1, t1)), 0.02f);
            }
    }
}

// ---------------------------------------------------------------------------
void world_build(World& w) {
    w = World();
    w.dyn.resize(DY_NAMED_COUNT);
    for (int i = 0; i < IT_COUNT; i++) { w.itemDyn[i] = -1; w.itemInteract[i] = -1; }
    for (int i = 0; i < U_COUNT; i++) w.useInteract[i] = -1;
    for (int i = 0; i < N_COUNT; i++) w.noteInteract[i] = -1;
    for (int i = 0; i < HIDE_COUNT; i++) w.hides[i].door = -1;
    Builder b(w);
    build_ground(b);
    build_entrance(b);
    build_office(b);
    build_crewshop(b);
    build_utility(b);
    build_containers(b);
    build_quay(b);
    build_crane(b);
    build_rigger(b);
    build_backdrop(b);
    b.finish();
}
