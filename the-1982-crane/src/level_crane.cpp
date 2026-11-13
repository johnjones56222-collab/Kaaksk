// THE 1982 CRANE - level: Crane No. 7, its load, the Rigger and the apparitions
#include "level.h"

static const u32 CRANE_YEL = 0xa88a24;
static const u32 CRANE_DARK = 0x5a5236;

// Vertical ladder with a safety cage. `faceZ` = +1 if the climber stands on the +Z side.
static void ladderMesh(Builder& b, float x, float z, float y0, float y1, int faceZ, bool cage) {
    b.m(M_RUST, 0xa0a098, 1.0f);
    b.beam(Vec3(x - 0.25f, y0, z), Vec3(x - 0.25f, y1 + 1.1f, z), 0.045f);
    b.beam(Vec3(x + 0.25f, y0, z), Vec3(x + 0.25f, y1 + 1.1f, z), 0.045f);
    for (float y = y0 + 0.3f; y < y1 + 0.05f; y += 0.3f) b.beam(Vec3(x - 0.25f, y, z), Vec3(x + 0.25f, y, z), 0.028f);
    if (cage) {
        float zo = z + faceZ * 0.75f;
        for (float y = y0 + 2.4f; y < y1 - 0.2f; y += 1.3f) {
            b.beam(Vec3(x - 0.38f, y, z), Vec3(x - 0.38f, y, zo), 0.025f);
            b.beam(Vec3(x + 0.38f, y, z), Vec3(x + 0.38f, y, zo), 0.025f);
            b.beam(Vec3(x - 0.38f, y, zo), Vec3(x + 0.38f, y, zo), 0.025f);
        }
        for (int k = -1; k <= 1; k++) b.beam(Vec3(x + k * 0.38f, y0 + 2.4f, zo), Vec3(x + k * 0.38f, y1 - 0.2f, zo), 0.02f);
    }
}

static void setLadder(World& w, int id, const Vec3& bottom, const Vec3& top, float railX, float railZ, float yaw, float y0, float y1) {
    Ladder& l = w.ladders[id];
    l.bottom = bottom; l.top = top; l.rail = Vec3(railX, 0, railZ); l.yaw = yaw; l.y0 = y0; l.y1 = y1;
}

// Truss between two parallel chords (zig-zag lacing)
static void lacing(Builder& b, const Vec3& a0, const Vec3& a1, const Vec3& b0, const Vec3& b1, int bays, float t) {
    for (int i = 0; i < bays; i++) {
        float t0 = (float)i / bays, t1 = (float)(i + 1) / bays;
        Vec3 pa0 = lerp(a0, a1, t0), pa1 = lerp(a0, a1, t1), pb0 = lerp(b0, b1, t0), pb1 = lerp(b0, b1, t1);
        if (i & 1) b.beam(pa0, pb1, t); else b.beam(pb0, pa1, t);
        b.beam(pa1, pb1, t);
    }
}

void build_crane(Builder& b) {
    World& w = b.w;
    const float cx = CRANE_POS.x, cz = CRANE_POS.z;
    const float lx[2] = {cx - 4.5f, cx + 4.5f}, lz[2] = {cz - 4.0f, cz + 4.0f};
    // ---- portal ---------------------------------------------------------------
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) {
            float x = lx[i], z = lz[j];
            b.m(M_PAINTSTEEL, CRANE_DARK, 0.8f);
            b.box(Vec3(x - 1.1f, 0.12f, z - 0.32f), Vec3(x + 1.1f, 0.55f, z + 0.32f));
            b.m(M_RAIL, 0x808080, 1.0f);
            for (int k = -1; k <= 1; k += 2) b.cyl(Vec3(x + k * 0.65f, 0.26f, z - 0.2f), Vec3(x + k * 0.65f, 0.26f, z + 0.2f), 0.2f, 10);
            b.m(M_PAINTSTEEL, CRANE_YEL, 0.5f).g(0.55f);
            b.box(Vec3(x - 0.4f, 0.55f, z - 0.4f), Vec3(x + 0.4f, 6.4f, z + 0.4f));
            b.g(1.0f);
            b.m(M_KELP, 0xffffff, 0.5f);
            b.box(Vec3(x - 0.42f, 0.55f, z - 0.42f), Vec3(x + 0.42f, 1.25f + 0.3f * ((i + j) & 1), z + 0.42f), F_SIDES);
            b.collider(Vec3(x - 1.1f, 0, z - 0.4f), Vec3(x + 1.1f, 0.55f, z + 0.4f), CF_PROP, SURF_METAL);
            b.collider(Vec3(x - 0.4f, 0, z - 0.4f), Vec3(x + 0.4f, 7.0f, z + 0.4f), CF_SOLID, SURF_METAL);
            // knee braces
            b.m(M_PAINTSTEEL, CRANE_YEL, 0.5f);
            float sx = i ? -1.0f : 1.0f, sz = j ? -1.0f : 1.0f;
            b.beam(Vec3(x, 4.2f, z), Vec3(x + sx * 2.0f, 6.4f, z), 0.22f);
            b.beam(Vec3(x, 4.2f, z), Vec3(x, 6.4f, z + sz * 2.0f), 0.22f);
        }
    b.m(M_PAINTSTEEL, CRANE_YEL, 0.5f);
    for (int j = 0; j < 2; j++) b.box(Vec3(lx[0] - 0.5f, 6.2f, lz[j] - 0.4f), Vec3(lx[1] + 0.5f, 7.0f, lz[j] + 0.4f));
    for (int i = 0; i < 2; i++) b.box(Vec3(lx[i] - 0.4f, 6.2f, lz[0] + 0.4f), Vec3(lx[i] + 0.4f, 7.0f, lz[1] - 0.4f));
    b.box(Vec3(cx - 2.2f, 6.3f, lz[0] + 0.4f), Vec3(cx - 1.8f, 7.0f, lz[1] - 0.4f));
    b.box(Vec3(cx + 1.8f, 6.3f, lz[0] + 0.4f), Vec3(cx + 2.2f, 7.0f, lz[1] - 0.4f));
    // deck
    const float dx0 = cx - 5.0f, dx1 = cx + 5.0f, dz0 = cz - 5.0f, dz1 = cz + 5.0f, DY = 7.2f;
    b.m(M_DIAMOND, 0xa0a098, 0.5f);
    b.box(Vec3(dx0, 7.0f, dz0), Vec3(dx1, DY, dz1));
    b.collider(Vec3(dx0, 7.0f, dz0), Vec3(dx1, DY, dz1), CF_SOLID, SURF_METAL);
    // deck railing
    b.m(M_RUST, 0xa0a098, 1.0f);
    struct Run { float x0, z0, x1, z1; } runs[4] = {{dx0, dz0, dx1, dz0}, {dx1, dz0, dx1, dz1}, {dx1, dz1, dx0, dz1}, {dx0, dz1, dx0, dz0}};
    for (auto& r : runs) {
        Vec3 a(r.x0, DY, r.z0), c(r.x1, DY, r.z1);
        for (int k = 0; k <= 5; k++) {
            Vec3 p = lerp(a, c, k / 5.0f);
            b.beam(p, p + Vec3(0, 1.1f, 0), 0.04f);
        }
        b.beam(a + Vec3(0, 1.1f, 0), c + Vec3(0, 1.1f, 0), 0.045f);
        b.beam(a + Vec3(0, 0.55f, 0), c + Vec3(0, 0.55f, 0), 0.03f);
        b.collider(vmin(a, c) - Vec3(0.06f, 0, 0.06f), vmax(a, c) + Vec3(0.06f, 1.3f, 0.06f), COL_SOLID, SURF_METAL);
    }
    b.sign("crane7", Vec3(cx, 6.6f, lz[1] + 0.405f), Vec3(0, 0, 1), 5.6f);
    b.sign("crane7", Vec3(cx, 6.6f, lz[0] - 0.405f), Vec3(0, 0, -1), 5.6f);
    b.sign("maxload", Vec3(lx[1] + 0.405f, 6.6f, cz), Vec3(1, 0, 0), 2.4f);
    b.sign("danger_load", Vec3(lx[0], 2.0f, lz[1] + 0.405f), Vec3(0, 0, 1), 0.76f);
    b.sign("plate", Vec3(lx[1], 1.6f, lz[1] + 0.405f), Vec3(0, 0, 1), 0.62f);
    b.interact(IK_NOTE, N_PLATE, Vec3(lx[1] - 0.35f, 1.35f, lz[1] + 0.38f), Vec3(lx[1] + 0.35f, 1.85f, lz[1] + 0.5f), 2.3f);
    w.notePos[N_PLATE] = Vec3(lx[1], 1.6f, lz[1] + 0.5f);
    b.sign("c_dontlook", Vec3(cx + 0.5f, 0.006f, cz + 1.5f), Vec3(0, 1, 0), 5.0f);
    b.sign("tally", Vec3(lx[0] + 0.405f, 1.5f, lz[1]), Vec3(1, 0, 0), 0.78f);
    // storm lock on the south-east leg
    {
        float x = lx[1] + 0.4f, z = lz[1];
        b.m(M_MACHINE, 0x4a5a50, 1.5f);
        b.box(Vec3(x, 0.9f, z - 0.3f), Vec3(x + 0.16f, 1.6f, z + 0.3f));
        b.sign("stormlock", Vec3(x + 0.005f, 1.85f, z), Vec3(1, 0, 0), 0.7f);
        b.sign("gauge", Vec3(x + 0.16f, 1.38f, z - 0.14f), Vec3(1, 0, 0), 0.22f);
        b.beginDyn();
        b.m(M_RAIL, 0xb0b0b0, 2.0f);
        b.beam(Vec3(0, 0, 0), Vec3(0.3f, 0.0f, 0), 0.035f);
        b.m(M_PLAIN, 0xa01410, 2.0f);
        b.cyl(Vec3(0.3f, 0, -0.06f), Vec3(0.3f, 0, 0.06f), 0.03f, 8);
        b.named(DY_STORM_LEVER, b.endDyn(), Mat4::translate(Vec3(x + 0.17f, 1.2f, z + 0.14f)) * Mat4::rotZ(0.9f));
        b.interact(IK_USE, U_STORMLOCK, Vec3(x, 0.85f, z - 0.35f), Vec3(x + 0.55f, 1.75f, z + 0.35f), 2.4f);
    }
    // access ladder with a locked cage at its foot
    {
        const float ax = cx - 3.2f, az = dz1 + 0.15f; // ladder plane just south of the deck edge
        ladderMesh(b, ax, az, 0.0f, DY, +1, true);
        b.m(M_RUST, 0xa0a098, 1.0f);
        const float gx0 = ax - 0.6f, gx1 = ax + 0.6f, gz1 = az + 0.95f;
        for (int k = 0; k < 2; k++) {
            float px = k ? gx1 : gx0;
            b.beam(Vec3(px, 0, gz1), Vec3(px, 2.35f, gz1), 0.05f);
            b.beam(Vec3(px, 2.3f, az), Vec3(px, 2.3f, gz1), 0.04f);
            b.m(M_CHAINLINK, 0xffffff, 1.0f);
            b.quad(Vec3(px, 0.05f, az), Vec3(px, 0.05f, gz1), Vec3(px, 2.3f, gz1), Vec3(px, 2.3f, az), Vec2(0, 3.2f), Vec2(1.3f, 3.2f), Vec2(1.3f, 0), Vec2(0, 0));
            b.m(M_RUST, 0xa0a098, 1.0f);
            b.collider(Vec3(px - 0.05f, 0, az - 0.1f), Vec3(px + 0.05f, 2.35f, gz1), CF_FENCE, SURF_METAL);
        }
        b.beam(Vec3(gx0, 2.3f, gz1), Vec3(gx1, 2.3f, gz1), 0.04f);
        b.collider(Vec3(gx0, 0, az - 0.12f), Vec3(gx1, 2.35f, az - 0.06f), COL_SOLID); // nothing gets behind the ladder
        makeDoor(b, D_CRANE_GATE, Vec3(gx0, 0.02f, gz1), 1.2f, 2.25f, 0.0f, -1.5f, M_GRATE, 0xc0c0b8, IT_CRANE_KEY, true);
        b.sign("auth", Vec3(ax, 2.6f, gz1 + 0.03f), Vec3(0, 0, 1), 1.1f);
        b.interact(IK_LADDER, LAD_PORTAL * 2, Vec3(ax - 0.3f, 0.3f, az - 0.1f), Vec3(ax + 0.3f, 2.2f, az + 0.12f), 2.0f);
        b.interact(IK_LADDER, LAD_PORTAL * 2 + 1, Vec3(ax - 0.4f, DY, az - 0.4f), Vec3(ax + 0.4f, DY + 1.3f, az + 0.15f), 2.2f);
        setLadder(w, LAD_PORTAL, Vec3(ax, 0, az + 0.45f), Vec3(ax, DY, az - 0.85f), ax, az + 0.38f, 0.0f, 0.0f, DY);
    }
    // ---- mast -----------------------------------------------------------------
    const float mx0 = cx - 2.0f, mx1 = cx + 2.0f, mz0 = cz - 2.0f, mz1 = cz + 2.0f, MTOP = 34.0f;
    b.m(M_PAINTSTEEL, CRANE_YEL, 0.5f);
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? mx1 : mx0, z = (i & 2) ? mz1 : mz0;
        b.box(Vec3(x - 0.15f, DY, z - 0.15f), Vec3(x + 0.15f, MTOP, z + 0.15f));
        b.collider(Vec3(x - 0.15f, DY, z - 0.15f), Vec3(x + 0.15f, MTOP, z + 0.15f), CF_PROP, SURF_METAL);
    }
    {
        const int bays = 9;
        const float bh = (MTOP - DY) / bays;
        for (int k = 0; k < bays; k++) {
            float y0 = DY + k * bh, y1 = y0 + bh;
            b.m(M_PAINTSTEEL, CRANE_YEL, 0.5f);
            b.beam(Vec3(mx0, y1, mz0), Vec3(mx1, y1, mz0), 0.13f);
            b.beam(Vec3(mx0, y1, mz1), Vec3(mx1, y1, mz1), 0.13f);
            b.beam(Vec3(mx0, y1, mz0), Vec3(mx0, y1, mz1), 0.13f);
            b.beam(Vec3(mx1, y1, mz0), Vec3(mx1, y1, mz1), 0.13f);
            b.m(M_RUST, 0xb0a070, 0.6f);
            bool f = (k & 1) != 0;
            b.beam(Vec3(f ? mx0 : mx1, y0, mz0), Vec3(f ? mx1 : mx0, y1, mz0), 0.1f);
            b.beam(Vec3(f ? mx1 : mx0, y0, mz1), Vec3(f ? mx0 : mx1, y1, mz1), 0.1f);
            b.beam(Vec3(mx0, y0, f ? mz1 : mz0), Vec3(mx0, y1, f ? mz0 : mz1), 0.1f);
            // the east face stays open at cab level so the cab door is reachable
            if (y1 < CRANE_TOP_Y - 0.5f) b.beam(Vec3(mx1, y0, f ? mz0 : mz1), Vec3(mx1, y1, f ? mz1 : mz0), 0.1f);
        }
    }
    // platforms and internal ladders
    static const float PY[4] = {13.0f, 19.0f, 25.0f, CRANE_TOP_Y};
    const float px0 = mx0 + 0.15f, px1 = mx1 - 0.15f, pz0 = mz0 + 0.15f, pz1 = mz1 - 0.15f;
    for (int p = 0; p < 4; p++) {
        float y = PY[p];
        bool nw = (p % 2) == 0; // hatch corner alternates NW / SE
        b.m(M_GRATE, 0xb0b0a8, 0.8f);
        if (nw) {
            b.box(Vec3(px0 + 0.9f, y - 0.05f, pz0), Vec3(px1, y, pz1), F_PY | F_NY);
            b.box(Vec3(px0, y - 0.05f, pz0 + 0.9f), Vec3(px0 + 0.9f, y, pz1), F_PY | F_NY);
            b.collider(Vec3(px0 + 0.9f, y - 0.12f, pz0), Vec3(px1, y, pz1), COL_SOLID, SURF_GRATE);
            b.collider(Vec3(px0, y - 0.12f, pz0 + 0.9f), Vec3(px0 + 0.9f, y, pz1), COL_SOLID, SURF_GRATE);
            b.collider(Vec3(px0, y - 0.12f, pz0), Vec3(px0 + 0.9f, y + 1.25f, pz0 + 0.9f), COL_SOLID);
            b.m(M_RUST, 0xa0a098, 1.0f);
            b.beam(Vec3(px0 + 0.9f, y, pz0), Vec3(px0 + 0.9f, y + 1.0f, pz0), 0.035f);
            b.beam(Vec3(px0 + 0.9f, y, pz0 + 0.9f), Vec3(px0 + 0.9f, y + 1.0f, pz0 + 0.9f), 0.035f);
            b.beam(Vec3(px0 + 0.9f, y + 1.0f, pz0), Vec3(px0 + 0.9f, y + 1.0f, pz0 + 0.9f), 0.035f);
        } else {
            b.box(Vec3(px0, y - 0.05f, pz0), Vec3(px1 - 0.9f, y, pz1), F_PY | F_NY);
            b.box(Vec3(px1 - 0.9f, y - 0.05f, pz0), Vec3(px1, y, pz1 - 0.9f), F_PY | F_NY);
            b.collider(Vec3(px0, y - 0.12f, pz0), Vec3(px1 - 0.9f, y, pz1), COL_SOLID, SURF_GRATE);
            b.collider(Vec3(px1 - 0.9f, y - 0.12f, pz0), Vec3(px1, y, pz1 - 0.9f), COL_SOLID, SURF_GRATE);
            b.collider(Vec3(px1 - 0.9f, y - 0.12f, pz1 - 0.9f), Vec3(px1, y + 1.25f, pz1), COL_SOLID);
            b.m(M_RUST, 0xa0a098, 1.0f);
            b.beam(Vec3(px1 - 0.9f, y, pz1), Vec3(px1 - 0.9f, y + 1.0f, pz1), 0.035f);
            b.beam(Vec3(px1 - 0.9f, y, pz1 - 0.9f), Vec3(px1 - 0.9f, y + 1.0f, pz1 - 0.9f), 0.035f);
            b.beam(Vec3(px1 - 0.9f, y + 1.0f, pz1), Vec3(px1 - 0.9f, y + 1.0f, pz1 - 0.9f), 0.035f);
        }
        // handrails between the chords + invisible guard walls
        b.m(M_RUST, 0xa0a098, 1.0f);
        for (int h = 0; h < 2; h++) {
            float ry = y + 0.55f + h * 0.55f;
            b.beam(Vec3(mx0, ry, mz0), Vec3(mx1, ry, mz0), 0.035f);
            b.beam(Vec3(mx0, ry, mz1), Vec3(mx1, ry, mz1), 0.035f);
            b.beam(Vec3(mx0, ry, mz0), Vec3(mx0, ry, mz1), 0.035f);
            if (p != 3) b.beam(Vec3(mx1, ry, mz0), Vec3(mx1, ry, mz1), 0.035f);
            else {
                b.beam(Vec3(mx1, ry, mz0), Vec3(mx1, ry, cz - 0.6f), 0.035f);
                b.beam(Vec3(mx1, ry, cz + 0.6f), Vec3(mx1, ry, mz1), 0.035f);
            }
        }
        b.collider(Vec3(mx0 - 0.1f, y, mz0 - 0.1f), Vec3(mx1 + 0.1f, y + 1.4f, mz0 + 0.12f), COL_SOLID);
        b.collider(Vec3(mx0 - 0.1f, y, mz1 - 0.12f), Vec3(mx1 + 0.1f, y + 1.4f, mz1 + 0.1f), COL_SOLID);
        b.collider(Vec3(mx0 - 0.1f, y, mz0), Vec3(mx0 + 0.12f, y + 1.4f, mz1), COL_SOLID);
        b.collider(Vec3(mx1 - 0.12f, y, mz0), Vec3(mx1 + 0.1f, y + 1.4f, mz1), COL_SOLID);
        // ladder arriving at this platform
        float yb = p == 0 ? DY : PY[p - 1];
        int lad = LAD_MAST1 + p;
        if (nw) {
            float lxx = px0 + 0.45f, lzz = pz0 + 0.1f;
            ladderMesh(b, lxx, lzz, yb, y, +1, true);
            b.interact(IK_LADDER, lad * 2, Vec3(lxx - 0.3f, yb + 0.3f, lzz - 0.1f), Vec3(lxx + 0.3f, yb + 2.2f, lzz + 0.12f), 2.0f);
            b.interact(IK_LADDER, lad * 2 + 1, Vec3(px0, y, pz0), Vec3(px0 + 0.9f, y + 1.3f, pz0 + 0.9f), 2.2f);
            setLadder(w, lad, Vec3(lxx, yb, lzz + 0.55f), Vec3(px0 + 1.5f, y, pz0 + 0.6f), lxx, lzz + 0.38f, 0.0f, yb, y);
        } else {
            float lxx = px1 - 0.45f, lzz = pz1 - 0.1f;
            ladderMesh(b, lxx, lzz, yb, y, -1, true);
            b.interact(IK_LADDER, lad * 2, Vec3(lxx - 0.3f, yb + 0.3f, lzz - 0.12f), Vec3(lxx + 0.3f, yb + 2.2f, lzz + 0.1f), 2.0f);
            b.interact(IK_LADDER, lad * 2 + 1, Vec3(px1 - 0.9f, y, pz1 - 0.9f), Vec3(px1, y + 1.3f, pz1), 2.2f);
            setLadder(w, lad, Vec3(lxx, yb, lzz - 0.55f), Vec3(px1 - 1.5f, y, pz1 - 0.6f), lxx, lzz - 0.38f, PI, yb, y);
        }
        // a caged work light on each landing (crane circuit: lit whether or not the yard has power)
        cagedLamp(b, Vec3(nw ? px1 - 0.3f : px0 + 0.3f, y + 2.3f, nw ? pz1 - 0.05f : pz0 + 0.05f), Vec3(0, 0, nw ? -1.0f : 1.0f), LG_CRANE, p == 1 ? FL_DYING : FL_BUZZ,
                  0xffd0a0, 1.0f, 7.5f);
    }
    // things left on the landings
    {
        // Walt's lunch pail and a note on the second landing
        b.m(M_PAINTSTEEL, 0x2f4a36, 2.0f);
        b.box(Vec3(px0 + 0.3f, 19.0f, pz1 - 0.6f), Vec3(px0 + 0.65f, 19.22f, pz1 - 0.4f));
        b.cyl(Vec3(px0 + 0.3f, 19.27f, pz1 - 0.5f), Vec3(px0 + 0.65f, 19.27f, pz1 - 0.5f), 0.1f, 8);
        notePaper(b, N_MAST, Vec3(px0 + 0.95f, 19.004f, pz1 - 0.5f), Vec3(0, 1, 0), 0.2f, 0.27f);
        itemBattery(b, IT_BATT7, Vec3(px0 + 0.45f, 19.002f, pz1 - 0.95f), 0.9f);
        itemTape(b, IT_TAPE5, Vec3(px1 - 0.5f, 25.002f, pz0 + 0.5f), 0.4f);
        // one boot. Just the one.
        b.m(M_RUBBER, 0x606060, 2.0f);
        b.box(Vec3(px1 - 0.9f, 25.0f, pz1 - 1.8f), Vec3(px1 - 0.76f, 25.28f, pz1 - 1.5f));
        b.sign("c_never", Vec3(cx, 13.004f, cz + 0.6f), Vec3(0, 1, 0), 2.6f);
        b.sign("hand", Vec3(mx0 + 0.16f, 26.2f, mz1 - 0.02f), Vec3(1, 0, 0), 0.26f, 0.33f);
    }
    itemTape(b, IT_TAPE4, Vec3(49.3f, 1.372f, -32.47f), 0.3f); // pump house shelf
    // slew ring
    b.m(M_RUST, 0x6a6a64, 0.6f);
    b.cyl(Vec3(cx, MTOP, cz), Vec3(cx, MTOP + 0.35f, cz), 2.5f, 20, true);
    b.m(M_PAINTSTEEL, CRANE_YEL, 0.5f);
    b.box(Vec3(mx0 - 0.2f, MTOP - 0.25f, mz0 - 0.2f), Vec3(mx1 + 0.2f, MTOP, mz1 + 0.2f));
    b.interact(IK_USE, U_CAB_DOOR, Vec3(mx1 - 0.2f, CRANE_TOP_Y + 0.1f, cz - 0.7f), Vec3(mx1 + 0.7f, CRANE_TOP_Y + 2.2f, cz + 0.7f), 2.4f);
    b.trigger(TR_CRANE_BASE, Vec3(cx - 10, 0, cz - 9), Vec3(cx + 10, 4, cz + 9));
    b.trigger(TR_PORTAL_DECK, Vec3(dx0, DY - 0.1f, dz0), Vec3(dx1, DY + 3, dz1));
    b.trigger(TR_MAST_MID, Vec3(mx0, 18.9f, mz0), Vec3(mx1, 21.5f, mz1));
    b.trigger(TR_MAST_TOP, Vec3(mx0, CRANE_TOP_Y - 0.1f, mz0), Vec3(mx1, CRANE_TOP_Y + 3, mz1));
    b.zone(Z_UNDER_PORTAL, Vec3(dx0, 0, dz0), Vec3(dx1, 7.0f, dz1));

    // ---- rotating superstructure (crane-local, origin on the mast axis at ground level) ----
    b.beginDyn();
    const float JY = 34.9f, JT = 37.1f;
    b.m(M_DIAMOND, 0x8a8a80, 0.5f);
    b.box(Vec3(-3.0f, 34.35f, -2.0f), Vec3(3.2f, 34.6f, 2.0f));
    // cathead
    b.m(M_PAINTSTEEL, CRANE_YEL, 0.5f);
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? 1.3f : -1.3f, z = (i & 2) ? 1.3f : -1.3f;
        b.beam(Vec3(x, 34.6f, z), Vec3(x * 0.15f, 44.5f, z * 0.2f), 0.24f);
    }
    b.m(M_RUST, 0xb0a070, 0.6f);
    for (int k = 0; k < 3; k++) {
        float t0 = k / 3.0f, t1 = (k + 1) / 3.0f;
        float y0 = lerpf(34.6f, 44.5f, t0), y1 = lerpf(34.6f, 44.5f, t1);
        float s0 = lerpf(1.3f, 0.2f, t0), s1 = lerpf(1.3f, 0.2f, t1);
        float q0 = lerpf(1.3f, 0.26f, t0), q1 = lerpf(1.3f, 0.26f, t1);
        b.beam(Vec3(-s0, y0, q0), Vec3(s1, y1, q1), 0.1f);
        b.beam(Vec3(s0, y0, -q0), Vec3(-s1, y1, -q1), 0.1f);
        b.beam(Vec3(s0, y0, -q0), Vec3(s1, y1, q1), 0.1f);
        b.beam(Vec3(-s0, y0, q0), Vec3(-s1, y1, -q1), 0.1f);
    }
    // jib: two lower chords the trolley runs on, one upper chord
    b.m(M_PAINTSTEEL, CRANE_YEL, 0.5f);
    b.beam(Vec3(1.5f, JY, -0.9f), Vec3(44.0f, JY, -0.9f), 0.22f);
    b.beam(Vec3(1.5f, JY, 0.9f), Vec3(44.0f, JY, 0.9f), 0.22f);
    b.beam(Vec3(1.5f, JT, 0), Vec3(38.0f, JT, 0), 0.22f);
    b.beam(Vec3(38.0f, JT, 0), Vec3(44.0f, JY + 0.4f, 0), 0.22f);
    b.m(M_RUST, 0xb0a070, 0.6f);
    lacing(b, Vec3(2.0f, JY, -0.9f), Vec3(38.0f, JY, -0.9f), Vec3(2.0f, JT, 0), Vec3(38.0f, JT, 0), 14, 0.09f);
    lacing(b, Vec3(2.0f, JY, 0.9f), Vec3(38.0f, JY, 0.9f), Vec3(2.0f, JT, 0), Vec3(38.0f, JT, 0), 14, 0.09f);
    for (int i = 0; i <= 16; i++) {
        float x = lerpf(2.0f, 44.0f, i / 16.0f);
        b.beam(Vec3(x, JY + 0.12f, -0.9f), Vec3(x, JY + 0.12f, 0.9f), 0.07f);
    }
    b.beam(Vec3(38.0f, JY, -0.9f), Vec3(41.0f, JY + 1.3f, 0), 0.09f);
    b.beam(Vec3(38.0f, JY, 0.9f), Vec3(41.0f, JY + 1.3f, 0), 0.09f);
    // counter-jib with machinery house and ballast
    b.m(M_PAINTSTEEL, CRANE_YEL, 0.5f);
    b.beam(Vec3(-1.5f, JY, -1.0f), Vec3(-15.0f, JY, -1.0f), 0.22f);
    b.beam(Vec3(-1.5f, JY, 1.0f), Vec3(-15.0f, JY, 1.0f), 0.22f);
    b.m(M_GRATE, 0xb0b0a8, 0.8f);
    b.box(Vec3(-15.0f, JY + 0.1f, -1.0f), Vec3(-1.5f, JY + 0.13f, 1.0f), F_PY | F_NY);
    b.m(M_RUST, 0xa0a098, 1.0f);
    for (int s = -1; s <= 1; s += 2) {
        b.beam(Vec3(-15.0f, JY + 1.2f, s * 1.0f), Vec3(-1.5f, JY + 1.2f, s * 1.0f), 0.04f);
        for (int k = 0; k <= 6; k++) b.beam(Vec3(-15.0f + k * 2.25f, JY, s * 1.0f), Vec3(-15.0f + k * 2.25f, JY + 1.2f, s * 1.0f), 0.035f);
    }
    b.m(M_CORRUGATED, 0x6a6230, 0.7f).g(0.7f);
    b.box(Vec3(-9.5f, JY + 0.13f, -0.95f), Vec3(-4.2f, JY + 2.4f, 0.95f));
    b.g(1.0f);
    b.sign("crane7", Vec3(-6.85f, JY + 1.3f, 0.955f), Vec3(0, 0, 1), 4.2f);
    b.sign("crane7", Vec3(-6.85f, JY + 1.3f, -0.955f), Vec3(0, 0, -1), 4.2f);
    b.m(M_CONCWALL, 0x8a8a84, 0.5f);
    for (int k = 0; k < 4; k++) b.box(Vec3(-14.9f + k * 0.95f, JY - 2.0f, -1.15f), Vec3(-14.1f + k * 0.95f, JY + 1.6f, 1.15f));
    // pendants
    b.m(M_RUST, 0x808078, 1.0f);
    for (int s = -1; s <= 1; s += 2) {
        b.beam(Vec3(0, 44.5f, s * 0.2f), Vec3(20.0f, JT, 0), 0.07f);
        b.beam(Vec3(0, 44.5f, s * 0.2f), Vec3(36.0f, JT, 0), 0.07f);
        b.beam(Vec3(0, 44.5f, s * 0.2f), Vec3(-14.5f, JY + 0.1f, s * 1.0f), 0.07f);
    }
    // sea growth hanging from the jib: it came up out of the water like this
    b.m(M_KELP, 0xffffff, 0.7f);
    {
        Rng kr(1957);
        for (int i = 0; i < 26; i++) {
            float x = kr.range(4.0f, 43.0f), z = kr.chance(0.5f) ? -0.9f : 0.9f, len = kr.range(0.4f, 2.2f);
            b.box(Vec3(x - 0.05f, JY - 0.11f - len, z - 0.03f), Vec3(x + 0.05f, JY - 0.11f, z + 0.03f));
        }
        for (int i = 0; i < 10; i++) {
            float x = kr.range(-14.0f, -2.0f), len = kr.range(0.5f, 1.8f);
            b.box(Vec3(x - 0.05f, JY - 0.11f - len, 0.97f), Vec3(x + 0.05f, JY - 0.11f, 1.03f));
        }
    }
    // beacons
    w.beaconLocal[0] = Vec3(0, 44.9f, 0);
    w.beaconLocal[1] = Vec3(44.0f, JY + 0.8f, 0);
    w.beaconLocal[2] = Vec3(-15.0f, JY + 1.9f, 0);
    b.m(M_PLAIN, 0xff2010, 2.0f).emissive(EG_BEACON, 1.0f);
    for (int i = 0; i < 3; i++) b.boxC(w.beaconLocal[i], Vec3(0.14f, 0.14f, 0.14f));
    b.noEmis();
    // ---- operator's cab (hangs beside the mast head, on the jib side) ----
    {
        const float x0 = 2.5f, x1 = 5.1f, y0 = 31.4f, y1 = 33.9f, z0 = -1.3f, z1 = 1.3f, t = 0.06f;
        b.m(M_PAINTSTEEL, CRANE_YEL, 0.6f);
        for (int i = 0; i < 4; i++) {
            float x = (i & 1) ? x1 - 0.1f : x0 + 0.1f, z = (i & 2) ? z1 - 0.1f : z0 + 0.1f;
            b.beam(Vec3(x, y1, z), Vec3(x * 0.6f, 34.35f, z), 0.12f);
        }
        b.box(Vec3(x0, y0 - 0.1f, z0), Vec3(x1, y0, z1));                 // floor
        b.box(Vec3(x0, y1, z0), Vec3(x1, y1 + 0.1f, z1));                 // roof
        b.box(Vec3(x0, y0, z0), Vec3(x0 + t, y1, z1), F_ALL);             // west wall (door wall)
        b.box(Vec3(x0, y0, z0), Vec3(x1, y0 + 0.75f, z0 + t));            // lower side walls
        b.box(Vec3(x0, y0, z1 - t), Vec3(x1, y0 + 0.75f, z1));
        b.box(Vec3(x0, y1 - 0.25f, z0), Vec3(x1, y1, z0 + t));            // window headers
        b.box(Vec3(x0, y1 - 0.25f, z1 - t), Vec3(x1, y1, z1));
        b.box(Vec3(x1 - t, y1 - 0.25f, z0), Vec3(x1, y1, z1));
        b.box(Vec3(x1 - t, y0 + 0.5f, z0), Vec3(x1, y0 + 0.58f, z1));     // front sill bar
        for (int i = 0; i < 4; i++) {
            float x = (i & 1) ? x1 - 0.08f : x0, z = (i & 2) ? z1 - 0.08f : z0;
            b.box(Vec3(x, y0, z), Vec3(x + 0.08f, y1, z + 0.08f));
        }
        b.m(M_PANEL, 0x808080, 0.6f);
        b.box(Vec3(x0 + t, y0 + 0.001f, z0 + t), Vec3(x1 - t, y0 + 0.012f, z1 - t), F_PY); // floor lining
        // door (closed leaf with a window)
        b.m(M_PAINTSTEEL, 0x6a6230, 1.0f);
        b.box(Vec3(x0 - 0.02f, y0 + 0.05f, -0.45f), Vec3(x0, y0 + 2.0f, 0.45f), F_NX | F_SIDES);
        b.sign("cab", Vec3(x0 - 0.021f, y0 + 1.72f, 0), Vec3(-1, 0, 0), 0.7f);
        b.m(M_RAIL, 0xb0b0b0, 2.0f);
        b.box(Vec3(x0 - 0.07f, y0 + 1.0f, 0.3f), Vec3(x0 - 0.02f, y0 + 1.04f, 0.4f));
        // glass: front (down to the floor so the operator can see the hook), sides
        b.m(M_GLASS, 0xffffff, 1.0f).fit();
        b.box(Vec3(x1 - 0.01f, y0 + 0.05f, z0 + 0.08f), Vec3(x1, y1 - 0.25f, z1 - 0.08f), F_PX);
        b.box(Vec3(x0 + 0.08f, y0 + 0.75f, z0), Vec3(x1 - 0.08f, y1 - 0.25f, z0 + 0.01f), F_NZ);
        b.box(Vec3(x0 + 0.08f, y0 + 0.75f, z1 - 0.01f), Vec3(x1 - 0.08f, y1 - 0.25f, z1), F_PZ);
        // seat
        b.m(M_TARP, 0x4a2a1a, 2.0f);
        b.box(Vec3(3.25f, y0 + 0.4f, -0.25f), Vec3(3.8f, y0 + 0.5f, 0.25f));
        b.box(Vec3(3.2f, y0 + 0.5f, -0.25f), Vec3(3.3f, y0 + 1.15f, 0.25f));
        b.m(M_PLAIN, 0x202020, 2.0f);
        b.cyl(Vec3(3.5f, y0, 0), Vec3(3.5f, y0 + 0.4f, 0), 0.05f, 6);
        // a jacket over the seat back, a thermos, a lunchbox, a photograph on the glass
        b.m(M_OILSKIN, 0xffffff, 1.5f);
        b.box(Vec3(3.16f, y0 + 0.6f, -0.3f), Vec3(3.2f, y0 + 1.2f, 0.3f));
        b.m(M_PAINTSTEEL, 0x2f4a36, 2.0f);
        b.cyl(Vec3(4.5f, y0 + 0.012f, 0.95f), Vec3(4.5f, y0 + 0.3f, 0.95f), 0.045f, 8);
        b.m(M_RAIL, 0xb0b0b0, 2.0f);
        b.cyl(Vec3(4.5f, y0 + 0.3f, 0.95f), Vec3(4.5f, y0 + 0.36f, 0.95f), 0.04f, 8);
        b.m(M_PAINTSTEEL, 0x7a2018, 2.0f);
        b.box(Vec3(4.1f, y0 + 0.012f, 0.8f), Vec3(4.35f, y0 + 0.2f, 1.1f));
        // side consoles with the levers
        b.m(M_MACHINE, 0x4a5a50, 1.5f);
        b.box(Vec3(3.4f, y0, -1.2f), Vec3(4.3f, y0 + 0.72f, -0.75f));
        b.box(Vec3(3.4f, y0, 0.75f), Vec3(4.0f, y0 + 0.72f, 1.2f));
        b.box(Vec3(4.55f, y0, -0.9f), Vec3(5.0f, y0 + 0.5f, 0.9f)); // front console (low, under the glass line)
        b.sign("cabpanel", Vec3(4.55f, y0 + 0.28f, 0), Vec3(-1, 0, 0), 1.5f, 0.42f);
        b.sign("gauge2", Vec3(3.85f, y0 + 0.722f, -0.98f), Vec3(0, 1, 0), 0.2f);
        b.sign("photo", Vec3(4.6f, y0 + 1.55f, z0 + 0.012f), Vec3(0, 0, 1), 0.3f);
        b.sign("c_putback", Vec3(4.0f, y1 - 0.005f, 0), Vec3(0, -1, 0), 1.6f);
        // cab lamp fitting
        w.cabLampLocal = Vec3(3.8f, y1 - 0.15f, 0);
        b.m(M_PLAIN, 0xffe0b0, 2.0f).emissive(EG_CRANE, 1.0f);
        b.boxC(Vec3(3.8f, y1 - 0.05f, 0), Vec3(0.12f, 0.04f, 0.12f));
        b.noEmis();
        // Walt's last log on the right-hand console
        b.m(M_PAPER, 0xffffff, 1.0f).fit(0.6f, 0.6f);
        b.box(Vec3(3.55f, y0 + 0.72f, 0.82f), Vec3(3.85f, y0 + 0.724f, 1.05f), F_PY);
        w.cabSeat = Vec3(3.62f, y0 + 1.18f, 0);
        w.cabTapeLocal = Vec3(4.75f, y0 + 0.502f, -0.6f);
        w.cabLeverLocal[0] = Vec3(3.7f, y0 + 0.72f, -0.95f);
        w.cabLeverLocal[1] = Vec3(4.05f, y0 + 0.72f, -0.95f);
        w.cabLeverLocal[2] = Vec3(3.72f, y0 + 0.72f, 0.92f);
    }
    // under-jib floods (decorative fittings)
    b.m(M_PLAIN, 0xfff0d0, 2.0f).emissive(EG_CRANE, 0.8f);
    b.box(Vec3(6.0f, JY - 0.3f, -0.25f), Vec3(6.5f, JY - 0.12f, 0.25f));
    b.noEmis();
    b.sign("maxload", Vec3(9.0f, JY + 0.9f, 0.92f), Vec3(0, 0, 1), 2.6f);
    int topMesh = b.endDyn();
    b.named(DY_CRANE_TOP, topMesh, Mat4::translate(CRANE_POS));
    // cab interactables (local to the superstructure)
    {
        Interact it;
        it.kind = IK_NOTE; it.arg = N_WALT_FINAL; it.xfDyn = DY_CRANE_TOP; it.range = 2.4f;
        it.box = AABB(Vec3(3.45f, 32.05f, 0.75f), Vec3(3.95f, 32.3f, 1.15f));
        w.interacts.push_back(it);
        w.cabNoteInteract = (int)w.interacts.size() - 1;
        w.noteInteract[N_WALT_FINAL] = w.cabNoteInteract;
        it.kind = IK_USE; it.arg = U_CAB_CONTROLS;
        it.box = AABB(Vec3(4.45f, 31.4f, -0.9f), Vec3(5.05f, 32.1f, 0.9f));
        w.interacts.push_back(it);
        w.cabControlsInteract = (int)w.interacts.size() - 1;
        w.useInteract[U_CAB_CONTROLS] = w.cabControlsInteract;
        it.arg = U_CAB_LEAVE;
        it.box = AABB(Vec3(2.45f, 31.5f, -1.25f), Vec3(2.8f, 33.5f, 1.25f)); // the whole rear wall, so it can be reached from the seat
        w.interacts.push_back(it);
        w.cabLeaveInteract = (int)w.interacts.size() - 1;
        w.useInteract[U_CAB_LEAVE] = w.cabLeaveInteract;
        for (int idx : {w.cabNoteInteract, w.cabControlsInteract, w.cabLeaveInteract}) w.interacts[(size_t)idx].enabled = false;
        w.notePos[N_WALT_FINAL] = CRANE_POS + Vec3(3.7f, 32.15f, 0.95f);
        w.usePos[U_CAB_CONTROLS] = CRANE_POS + Vec3(4.7f, 31.8f, 0);
        w.usePos[U_CAB_LEAVE] = CRANE_POS + Vec3(2.6f, 32.4f, 0);
    }
    // cab levers
    for (int i = 0; i < 3; i++) {
        b.beginDyn();
        b.m(M_RAIL, 0xb0b0b0, 2.0f);
        b.beam(Vec3(0, 0, 0), Vec3(0, 0.26f, 0), 0.025f);
        b.m(M_PLAIN, 0x101010, 2.0f);
        b.boxC(Vec3(0, 0.29f, 0), Vec3(0.03f, 0.035f, 0.03f));
        b.named(DY_CAB_LEVER1 + i, b.endDyn());
    }
    // the sixth tape sits on the cab's front console; it rides with the cab
    beginItem(b);
    b.m(M_PLAIN, 0x161616, 2.0f);
    b.box(Vec3(-0.052f, 0, -0.033f), Vec3(0.052f, 0.013f, 0.033f));
    b.sign("tape", Vec3(0, 0.0011f, 0), Vec3(0, 1, 0), 0.1f, 0.064f);
    {
        int mesh = b.endDyn();
        w.itemDyn[IT_TAPE6] = b.addDyn(mesh, Mat4::translate(CRANE_POS + w.cabTapeLocal));
        Interact it;
        it.kind = IK_PICKUP; it.arg = IT_TAPE6; it.xfDyn = DY_CRANE_TOP; it.range = 2.4f;
        it.box = AABB(w.cabTapeLocal - Vec3(0.25f, 0.1f, 0.25f), w.cabTapeLocal + Vec3(0.25f, 0.25f, 0.25f));
        it.enabled = false;
        w.interacts.push_back(it);
        w.itemInteract[IT_TAPE6] = (int)w.interacts.size() - 1;
        w.itemPos[IT_TAPE6] = CRANE_POS + w.cabTapeLocal;
    }

    // ---- trolley, hook, cables, load ------------------------------------------
    b.beginDyn();
    b.m(M_PAINTSTEEL, CRANE_DARK, 0.8f);
    b.box(Vec3(-0.9f, 0.0f, -1.05f), Vec3(0.9f, 0.3f, 1.05f));
    b.m(M_RAIL, 0x909090, 1.0f);
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? 0.6f : -0.6f, z = (i & 2) ? 0.9f : -0.9f;
        b.cyl(Vec3(x, 0.42f, z - 0.08f), Vec3(x, 0.42f, z + 0.08f), 0.16f, 8);
    }
    b.cyl(Vec3(-0.3f, -0.05f, -0.2f), Vec3(-0.3f, -0.05f, 0.2f), 0.22f, 10);
    b.cyl(Vec3(0.3f, -0.05f, -0.2f), Vec3(0.3f, -0.05f, 0.2f), 0.22f, 10);
    b.m(M_PLAIN, 0xfff0d0, 2.0f).emissive(EG_CRANE, 1.0f);
    b.box(Vec3(-0.25f, -0.12f, 0.5f), Vec3(0.25f, 0.0f, 0.9f));
    b.box(Vec3(-0.25f, -0.12f, -0.9f), Vec3(0.25f, 0.0f, -0.5f));
    b.noEmis();
    b.named(DY_TROLLEY, b.endDyn());
    {
        b.beginDyn();
        b.m(M_RAIL, 0x707070, 4.0f);
        b.cyl(Vec3(0, 0, 0), Vec3(0, -1.0f, 0), 0.022f, 5, false);
        int cable = b.endDyn();
        b.named(DY_CABLE_A, cable);
        b.named(DY_CABLE_B, cable);
    }
    b.beginDyn();
    b.m(M_HAZARD, 0xffffff, 1.5f);
    b.box(Vec3(-0.34f, -0.75f, -0.16f), Vec3(0.34f, 0.0f, 0.16f));
    b.m(M_RAIL, 0x808080, 1.0f);
    b.cyl(Vec3(-0.2f, -0.25f, -0.19f), Vec3(-0.2f, -0.25f, 0.19f), 0.2f, 10);
    b.cyl(Vec3(0.2f, -0.25f, -0.19f), Vec3(0.2f, -0.25f, 0.19f), 0.2f, 10);
    b.m(M_RUST, 0x707068, 1.0f);
    b.beam(Vec3(0, -0.75f, 0), Vec3(0, -1.15f, 0), 0.12f);
    b.beam(Vec3(0, -1.15f, 0), Vec3(0.25f, -1.5f, 0), 0.11f);
    b.beam(Vec3(0.25f, -1.5f, 0), Vec3(0.02f, -1.68f, 0), 0.11f);
    b.beam(Vec3(0.02f, -1.68f, 0), Vec3(-0.24f, -1.5f, 0), 0.1f);
    b.beam(Vec3(-0.24f, -1.5f, 0), Vec3(-0.2f, -1.3f, 0), 0.08f);
    b.m(M_KELP, 0xffffff, 1.0f);
    b.box(Vec3(-0.36f, -0.5f, -0.18f), Vec3(0.36f, -0.2f, 0.18f), F_SIDES);
    b.named(DY_HOOK, b.endDyn());
    // the load: a 20 ft container, drowned for a long time
    b.beginDyn();
    {
        const float hl = LOAD_L * 0.5f, hw = LOAD_W * 0.5f, top = -2.0f, bot = -LOAD_BOTTOM;
        b.m(M_RUST, 0x707068, 1.0f);
        for (int i = 0; i < 4; i++) {
            float x = (i & 1) ? hl - 0.1f : -hl + 0.1f, z = (i & 2) ? hw - 0.1f : -hw + 0.1f;
            b.beam(Vec3(0, 0, 0), Vec3(x, top, z), 0.05f);
        }
        b.m(M_CORRUGATED, 0x3a4640, 0.8f).g(0.7f);
        b.box(Vec3(-hl, bot, -hw), Vec3(hl, top, hw), F_ALL & ~F_PX);
        b.g(1.0f);
        b.m(M_KELP, 0xffffff, 0.5f);
        b.box(Vec3(-hl - 0.012f, bot - 0.01f, -hw - 0.012f), Vec3(hl - 0.3f, bot + 1.2f, hw + 0.012f), F_SIDES | F_NY);
        b.box(Vec3(-hl - 0.012f, top - 0.5f, -hw - 0.012f), Vec3(-hl + 2.2f, top + 0.012f, hw + 0.012f), F_SIDES | F_PY);
        Rng kr(7);
        for (int i = 0; i < 22; i++) {
            float x = kr.range(-hl, hl - 0.2f), z = kr.chance(0.5f) ? -hw : hw, len = kr.range(0.3f, 1.6f);
            b.box(Vec3(x - 0.04f, bot - len, z - 0.03f), Vec3(x + 0.04f, bot, z + 0.03f));
        }
        b.m(M_PAINTSTEEL, 0x2a302c, 1.0f);
        for (int i = 0; i < 4; i++) {
            float x = (i & 1) ? hl - 0.12f : -hl, z = (i & 2) ? hw - 0.12f : -hw;
            b.box(Vec3(x, bot, z), Vec3(x + 0.12f, top, z + 0.12f));
        }
        b.sign("lift4", Vec3(1.2f, bot + 1.5f, hw + 0.014f), Vec3(0, 0, 1), 0.9f);
        b.sign("cid1", Vec3(-0.6f, top - 0.5f, -hw - 0.014f), Vec3(0, 0, -1), 2.4f);
        b.sign("hand", Vec3(hl - 0.9f, bot + 1.4f, hw + 0.014f), Vec3(0, 0, 1), 0.3f, 0.38f);
        // dark interior visible once the door is open
        b.m(M_PLAIN, 0x030303, 1.0f);
        b.box(Vec3(hl - 0.5f, bot + 0.1f, -hw + 0.1f), Vec3(hl - 0.45f, top - 0.1f, hw - 0.1f), F_PX);
    }
    b.named(DY_LOAD, b.endDyn());
    b.beginDyn();
    b.m(M_CONTDOOR, 0x3a4640, 1.0f).fit();
    b.box(Vec3(0, 0, -0.03f), Vec3(LOAD_W - 0.08f, LOAD_H - 0.1f, 0.03f));
    b.named(DY_LOAD_DOOR, b.endDyn());
    {
        Interact it;
        it.kind = IK_USE; it.arg = U_LOAD_DOOR; it.xfDyn = DY_LOAD; it.range = 2.8f; it.enabled = false;
        it.box = AABB(Vec3(LOAD_L * 0.5f - 0.2f, -LOAD_BOTTOM, -LOAD_W * 0.5f), Vec3(LOAD_L * 0.5f + 0.3f, -2.0f, LOAD_W * 0.5f));
        w.interacts.push_back(it);
        w.useInteract[U_LOAD_DOOR] = (int)w.interacts.size() - 1;
        w.usePos[U_LOAD_DOOR] = pad_pos() + pad_dir() * (LOAD_L * 0.5f + 0.6f) + Vec3(0, 1.2f, 0);
    }
    // lights that ride on the crane
    w.lightFlood = b.spot(CRANE_POS + Vec3(20, 34, 0), Vec3(0, -1, 0), 34.0f, 0xfff0d8, 3.4f, 50.0f, LG_CRANE, FL_BUZZ, 2.2f);
    w.lightCab = b.light(CRANE_POS + w.cabLampLocal, 0xffd8a0, 1.3f, 9.0f, LG_CRANE, FL_DYING, 0.8f);
    for (int i = 0; i < 3; i++) w.lightBeacon[i] = b.light(CRANE_POS + w.beaconLocal[i], 0xff2810, 1.6f, 10.0f, LG_CRANE, FL_BEACON, 1.3f);
    // "AGAIN", chalked inside the gate. It is not there the first time.
    b.beginDyn();
    b.sign("c_again", Vec3(0, 0, 0), Vec3(0, 1, 0), 3.4f);
    b.named(DY_CHALK_AGAIN, b.endDyn(), Mat4::translate(Vec3(0.3f, 0.008f, 51.0f)), false);
}

// ---------------------------------------------------------------------------
// The Rigger and the standing figures
// ---------------------------------------------------------------------------
void build_rigger(Builder& b) {
    World& w = b.w;
    // torso (origin at the pelvis)
    b.beginDyn();
    b.m(M_OILSKIN, 0xffffff, 1.0f);
    b.box(Vec3(-0.2f, 0.0f, -0.12f), Vec3(0.2f, 0.84f, 0.13f));
    b.box(Vec3(-0.29f, 0.7f, -0.14f), Vec3(0.29f, 0.92f, 0.12f));       // shoulders, hunched
    b.box(Vec3(-0.24f, -0.62f, -0.14f), Vec3(0.24f, 0.08f, 0.15f), F_ALL & ~F_NY); // coat skirt
    b.m(M_KELP, 0xffffff, 1.5f);
    b.box(Vec3(-0.245f, -0.64f, -0.145f), Vec3(0.245f, -0.3f, 0.155f), F_SIDES);
    b.m(M_RUST, 0x808078, 2.0f);
    for (int i = 0; i < 4; i++) b.box(Vec3(-0.02f, 0.15f + i * 0.17f, 0.13f), Vec3(0.02f, 0.19f + i * 0.17f, 0.145f)); // buckles
    b.box(Vec3(-0.21f, 0.02f, -0.13f), Vec3(0.21f, 0.08f, 0.14f));        // belt
    // hunched back, collar turned up
    b.m(M_OILSKIN, 0xffffff, 1.0f);
    b.box(Vec3(-0.25f, 0.55f, -0.21f), Vec3(0.25f, 0.92f, -0.1f));
    b.box(Vec3(-0.17f, 0.88f, -0.16f), Vec3(0.17f, 1.0f, 0.06f));
    // a mooring line wound round it, the way you would tie off a load
    b.m(M_TARP, 0x6a5a3a, 2.0f);
    b.beam(Vec3(-0.23f, 0.18f, 0.15f), Vec3(0.23f, 0.62f, 0.15f), 0.035f);
    b.beam(Vec3(0.23f, 0.62f, 0.15f), Vec3(0.23f, 0.7f, -0.14f), 0.035f);
    b.beam(Vec3(-0.23f, 0.3f, 0.15f), Vec3(0.2f, 0.82f, 0.13f), 0.03f);
    // torn hem: strips of coat and weed of uneven length
    {
        Rng hr(1103);
        for (int i = 0; i < 12; i++) {
            float a0 = (float)i / 12.0f * TAU;
            float px = std::cos(a0) * 0.235f, pz = std::sin(a0) * 0.145f + 0.005f;
            float len = hr.range(0.08f, 0.42f);
            if (i & 1) b.m(M_KELP, 0xffffff, 2.0f); else b.m(M_OILSKIN, 0xffffff, 2.0f);
            b.box(Vec3(px - 0.035f, -0.62f - len, pz - 0.012f), Vec3(px + 0.035f, -0.6f, pz + 0.012f));
        }
    }
    // barnacles on the shoulders, and a red lockout tag on the belt
    b.m(M_KELP, 0xc8c8b8, 3.0f);
    for (int i = 0; i < 9; i++) {
        float bx = (i % 2 ? 1.0f : -1.0f) * (0.12f + 0.03f * (i % 4)), bz = -0.12f + 0.05f * (i % 5);
        b.box(Vec3(bx - 0.025f, 0.9f, bz - 0.025f), Vec3(bx + 0.025f, 0.94f + 0.01f * (i % 3), bz + 0.025f));
    }
    b.m(M_PLAIN, 0xa01410, 2.0f);
    b.box(Vec3(0.1f, -0.2f, 0.145f), Vec3(0.17f, 0.04f, 0.15f));
    b.m(M_RUST, 0xb09a40, 2.0f);
    b.box(Vec3(0.11f, 0.03f, 0.14f), Vec3(0.16f, 0.09f, 0.16f));
    b.named(DY_RIG_TORSO, b.endDyn(), Mat4::identity(), false);
    // head (origin at the neck)
    b.beginDyn();
    b.m(M_OILSKIN, 0xffffff, 2.0f);
    b.box(Vec3(-0.06f, -0.02f, -0.06f), Vec3(0.06f, 0.1f, 0.06f));
    b.m(M_PLAIN, 0x0e0d0c, 2.0f);
    b.box(Vec3(-0.105f, 0.06f, -0.12f), Vec3(0.105f, 0.34f, 0.1f));
    b.m(M_RUST, 0x4a4a46, 1.5f);                                           // welding mask
    b.box(Vec3(-0.125f, -0.04f, 0.1f), Vec3(0.125f, 0.38f, 0.135f));
    b.box(Vec3(-0.125f, -0.04f, 0.02f), Vec3(-0.105f, 0.36f, 0.1f));
    b.box(Vec3(0.105f, -0.04f, 0.02f), Vec3(0.125f, 0.36f, 0.1f));
    b.m(M_PLAIN, 0x050505, 2.0f);
    b.box(Vec3(-0.075f, 0.2f, 0.135f), Vec3(0.075f, 0.265f, 0.14f));
    b.m(M_PLAIN, 0xfff2c0, 2.0f).emissive(EG_SPECIAL, 0.55f);                // something behind the glass
    b.box(Vec3(-0.06f, 0.222f, 0.14f), Vec3(-0.022f, 0.24f, 0.142f));
    b.box(Vec3(0.022f, 0.222f, 0.14f), Vec3(0.06f, 0.24f, 0.142f));
    b.noEmis();
    b.m(M_PAINTSTEEL, 0x7a6a20, 2.0f);                                       // hard hat
    b.cyl(Vec3(0, 0.33f, -0.01f), Vec3(0, 0.46f, -0.01f), 0.14f, 8, true, 0.1f);
    b.cyl(Vec3(0, 0.33f, 0.0f), Vec3(0, 0.345f, 0.0f), 0.185f, 8, true);
    // weed hanging off the brim
    b.m(M_KELP, 0xffffff, 3.0f);
    {
        Rng kr(77);
        for (int i = 0; i < 7; i++) {
            float a0 = 0.6f + (float)i / 7.0f * (TAU - 1.2f);
            float px = std::cos(a0) * 0.17f, pz = -std::fabs(std::sin(a0)) * 0.17f;
            float len = kr.range(0.12f, 0.4f);
            b.box(Vec3(px - 0.012f, 0.33f - len, pz - 0.012f), Vec3(px + 0.012f, 0.335f, pz + 0.012f));
        }
    }
    b.named(DY_RIG_HEAD, b.endDyn(), Mat4::identity(), false);
    // arms
    for (int side = 0; side < 2; side++) {
        b.beginDyn();
        b.m(M_OILSKIN, 0xffffff, 1.5f);
        b.box(Vec3(-0.055f, -0.56f, -0.055f), Vec3(0.055f, 0.03f, 0.055f));
        b.named(side ? DY_RIG_ARM_UR : DY_RIG_ARM_UL, b.endDyn(), Mat4::identity(), false);
        b.beginDyn();
        b.m(M_OILSKIN, 0xffffff, 1.5f);
        b.box(Vec3(-0.045f, -0.6f, -0.045f), Vec3(0.045f, 0.02f, 0.045f));
        if (side == 0) {
            // left hand: fingers far too long
            b.m(M_PLAIN, 0x1c1a16, 2.0f);
            for (int f = 0; f < 4; f++) {
                float fx = -0.036f + f * 0.024f;
                b.beam(Vec3(fx, -0.6f, 0.0f), Vec3(fx * 1.6f, -0.86f - 0.03f * (f == 1 || f == 2), 0.03f), 0.014f);
            }
        }
        b.named(side ? DY_RIG_ARM_LR : DY_RIG_ARM_LL, b.endDyn(), Mat4::identity(), false);
    }
    // the hook where the right hand should be (origin at the wrist)
    b.beginDyn();
    b.m(M_RUST, 0x6a6a64, 2.0f);
    b.beam(Vec3(0, 0.02f, 0), Vec3(0, -0.2f, 0), 0.05f);
    b.beam(Vec3(0, -0.2f, 0), Vec3(0.0f, -0.36f, 0.12f), 0.045f);
    b.beam(Vec3(0.0f, -0.36f, 0.12f), Vec3(0.0f, -0.5f, 0.02f), 0.04f);
    b.beam(Vec3(0.0f, -0.5f, 0.02f), Vec3(0.0f, -0.44f, -0.1f), 0.035f);
    b.beam(Vec3(0.0f, -0.44f, -0.1f), Vec3(0.0f, -0.34f, -0.12f), 0.025f);
    b.named(DY_RIG_HOOK, b.endDyn(), Mat4::identity(), false);
    // legs
    for (int side = 0; side < 2; side++) {
        b.beginDyn();
        b.m(M_OILSKIN, 0xffffff, 1.5f);
        b.box(Vec3(-0.07f, -0.6f, -0.07f), Vec3(0.07f, 0.02f, 0.07f));
        b.named(side ? DY_RIG_LEG_UR : DY_RIG_LEG_UL, b.endDyn(), Mat4::identity(), false);
        b.beginDyn();
        b.m(M_OILSKIN, 0xffffff, 1.5f);
        b.box(Vec3(-0.055f, -0.56f, -0.055f), Vec3(0.055f, 0.02f, 0.055f));
        b.m(M_RUBBER, 0x707070, 2.0f);
        b.box(Vec3(-0.07f, -0.64f, -0.08f), Vec3(0.07f, -0.5f, 0.2f));
        b.named(side ? DY_RIG_LEG_LR : DY_RIG_LEG_LL, b.endDyn(), Mat4::identity(), false);
    }
    // standing figures
    b.beginDyn();
    humanFigure(b, true);
    int fig = b.endDyn();
    for (int i = 0; i < 6; i++) b.named(DY_FIG0 + i, fig, Mat4::identity(), false);
    b.named(DY_CAMP_FIGURE, fig, Mat4::identity(), false);

    w.riggerSpawn = Vec3(-8.0f, 0, -27.0f);
    static const float pts[][2] = {
        {0, 40}, {0, 20}, {0, 0}, {0, -18}, {-14, 30}, {-14, 8}, {-16, -16}, {9, 30}, {10, 12}, {12, -6}, {24, 12}, {28, 16}, {42, 14},
        {40, -20}, {42, -30}, {30, -28}, {10, -28}, {-10, -27}, {-30, -28}, {-50, -30}, {-70, -26}, {-76, 0}, {-76, 30}, {-60, 48}, {-30, 48},
        {-48, 19}, {-36, 2.7f}, {-61, 8.2f}, {21, -3}, {33, -8}, {-48, 35.7f}, {5, 50}};
    for (auto& p : pts) w.patrolPoints.push_back(Vec3(p[0], 0, p[1]));
}
