// THE 1982 CRANE - level: site office, crew room / workshop, generator shed, pump house
#include "level.h"

static void mug(Builder& b, float x, float y, float z, u32 tint = 0xd8d2c0) {
    b.m(M_PLAIN, tint, 2.0f);
    b.cyl(Vec3(x, y, z), Vec3(x, y + 0.09f, z), 0.038f, 8, true);
    b.m(M_PLAIN, 0x1a120a, 2.0f);
    b.cyl(Vec3(x, y + 0.09f, z), Vec3(x, y + 0.091f, z), 0.03f, 8, true);
}
static void chair(Builder& b, float x, float y, float z, float yaw, bool tipped = false) {
    Mat4 m = Mat4::translate(Vec3(x, y, z)) * Mat4::rotY(yaw);
    if (tipped) m = m * Mat4::translate(Vec3(0, 0.22f, 0)) * Mat4::rotX(-PI * 0.5f);
    b.push(m);
    b.m(M_PLAIN, 0x30302c, 2.0f);
    for (int i = 0; i < 4; i++) {
        float lx = (i & 1) ? 0.19f : -0.19f, lz = (i & 2) ? 0.19f : -0.19f;
        b.box(Vec3(lx - 0.015f, 0, lz - 0.015f), Vec3(lx + 0.015f, 0.44f, lz + 0.015f));
    }
    b.box(Vec3(-0.2f, 0.44f, -0.215f), Vec3(-0.17f, 0.9f, -0.185f));
    b.box(Vec3(0.17f, 0.44f, -0.215f), Vec3(0.2f, 0.9f, -0.185f));
    b.m(M_TARP, 0x6a3a22, 2.0f);
    b.box(Vec3(-0.22f, 0.44f, -0.22f), Vec3(0.22f, 0.49f, 0.22f));
    b.box(Vec3(-0.21f, 0.62f, -0.22f), Vec3(0.21f, 0.92f, -0.19f));
    if (!tipped) b.collider(Vec3(-0.22f, 0, -0.22f), Vec3(0.22f, 0.5f, 0.22f), COL_SOLID);
    b.pop();
}
static void paperLitter(Builder& b, Rng& r, float x0, float x1, float z0, float z1, float y, int n) {
    for (int i = 0; i < n; i++) {
        b.push(Mat4::translate(Vec3(r.range(x0, x1), y + 0.002f + i * 0.0004f, r.range(z0, z1))) * Mat4::rotY(r.range(0.0f, TAU)));
        b.m(M_PAPER, 0xe8e8e0, 1.0f).fit(0.5f, 0.5f);
        b.box(Vec3(-0.105f, 0, -0.148f), Vec3(0.105f, 0.001f, 0.148f), F_PY);
        b.pop();
    }
}
static void wallShelf(Builder& b, const Vec3& mn, const Vec3& mx, int shelves) {
    b.m(M_PAINTSTEEL, 0x6a6e66, 1.0f).g(0.7f);
    float t = 0.03f;
    b.box(Vec3(mn.x, mn.y, mn.z), Vec3(mn.x + t, mx.y, mx.z));
    b.box(Vec3(mx.x - t, mn.y, mn.z), Vec3(mx.x, mx.y, mx.z));
    b.g(1.0f);
    for (int i = 0; i < shelves; i++) {
        float y = lerpf(mn.y + 0.12f, mx.y - 0.02f, (float)i / (float)std::max(1, shelves - 1));
        b.box(Vec3(mn.x, y - t, mn.z), Vec3(mx.x, y, mx.z));
    }
    b.collider(mn, mx, CF_FENCE, SURF_METAL); // see-through so things on the shelves can be picked up
}

// ---------------------------------------------------------------------------
// Site office (a trailer on blocks)
// ---------------------------------------------------------------------------
void build_office(Builder& b) {
    World& w = b.w;
    const float x0 = 12.0f, x1 = 16.4f, z0 = 28.0f, z1 = 40.0f, fy = 0.6f, top = 3.1f;
    // chassis skirt + floor
    b.m(M_CORRUGATED, 0x4a4c48, 0.8f).g(0.5f);
    b.box(Vec3(x0 + 0.03f, 0, z0 + 0.03f), Vec3(x1 - 0.03f, fy, z1 - 0.03f), F_SIDES);
    b.g(1.0f);
    b.collider(Vec3(x0, 0, z0), Vec3(x1, fy, z1), CF_SOLID, SURF_WOOD);
    b.m(M_LINO, 0xc8c8c0, 0.5f);
    b.box(Vec3(x0 + 0.07f, fy - 0.01f, z0 + 0.07f), Vec3(x1 - 0.07f, fy + 0.002f, z1 - 0.07f), F_PY);
    WallStyle st = {M_CORRUGATED, 0xb4b0a2, 0.8f, M_PANEL, 0xffffff, 0.6f, 0.14f, 0.62f};
    Opening oW[3] = {{30.2f, 31.3f, fy, 2.68f, false}, {32.6f, 34.4f, 1.5f, 2.5f, true}, {36.6f, 38.4f, 1.5f, 2.5f, true}};
    wallRun(b, 1, x0, z0, z1, fy, top, +1, st, oW, 3);
    Opening oE[2] = {{29.6f, 31.4f, 1.5f, 2.5f, true}, {36.6f, 38.4f, 1.5f, 2.5f, true}};
    wallRun(b, 1, x1, z0, z1, fy, top, -1, st, oE, 2);
    wallRun(b, 0, z0, x0, x1, fy, top, +1, st);
    Opening oS[1] = {{13.4f, 15.0f, 1.5f, 2.5f, true}};
    wallRun(b, 0, z1, x0, x1, fy, top, -1, st, oS, 1);
    // roof + ceiling
    b.m(M_ROOF, 0xffffff, 0.5f);
    b.solid(Vec3(x0 - 0.15f, top, z0 - 0.15f), Vec3(x1 + 0.15f, top + 0.18f, z1 + 0.15f), SURF_METAL);
    b.m(M_CEILING, 0xd0d0d0, 0.5f);
    b.box(Vec3(x0, top - 0.012f, z0), Vec3(x1, top, z1), F_NY);
    // partition with inner door
    WallStyle ps = {M_PANEL, 0xffffff, 0.6f, M_PANEL, 0xffffff, 0.6f, 0.1f, 0.78f};
    Opening oP[1] = {{13.2f, 14.3f, fy, 2.62f, false}};
    wallRun(b, 0, 35.5f, x0 + 0.07f, x1 - 0.07f, fy, top, +1, ps, oP, 1);
    makeDoor(b, D_OFFICE_INNER, Vec3(13.2f, fy, 35.5f), 1.1f, 2.02f, 0.0f, -1.5f, M_PANEL, 0xc0b8a8, -1, false);
    // steps
    b.m(M_DIAMOND, 0xb0b0b0, 1.0f).g(0.7f);
    for (int i = 0; i < 3; i++)
        b.solid(Vec3(10.5f + 0.5f * i, 0, 29.9f), Vec3(12.0f, 0.2f * (i + 1), 31.6f), SURF_METAL, CF_PROP);
    b.g(1.0f);
    b.m(M_PAINTSTEEL, 0x70746c, 1.0f);
    for (int s = 0; s < 2; s++) {
        float z = s ? 31.6f : 29.9f;
        b.beam(Vec3(10.55f, 0.2f, z), Vec3(10.55f, 1.1f, z), 0.035f);
        b.beam(Vec3(11.95f, 0.6f, z), Vec3(11.95f, 1.55f, z), 0.035f);
        b.beam(Vec3(10.55f, 1.1f, z), Vec3(11.95f, 1.55f, z), 0.035f);
    }
    makeDoor(b, D_OFFICE, Vec3(x0, fy, 30.2f), 1.1f, 2.08f, -PI * 0.5f, -1.5f, M_PAINTSTEEL, 0x8a8f86, IT_OFFICE_KEY, true);
    notePaper(b, N_OFFICE_DOOR, Vec3(x0 - 0.075f, 1.85f, 31.62f), Vec3(-1, 0, 0), 0.2f, 0.27f);
    b.sign("office", Vec3(x0 - 0.075f, 2.78f, 30.7f), Vec3(-1, 0, 0), 0.95f);
    cagedLamp(b, Vec3(x0 - 0.07f, 2.9f, 31.75f), Vec3(-1, 0, 0), LG_YARD, FL_BUZZ, 0xffc080, 1.1f, 7.0f);
    // exterior dressing: AC unit, antenna, blocks, tow hitch, rubbish
    b.m(M_MACHINE, 0x8a8c84, 1.0f);
    b.box(Vec3(13.3f, 1.7f, z0 - 0.5f), Vec3(14.3f, 2.4f, z0 - 0.07f));
    b.m(M_PAINTSTEEL, 0x5a5e58, 1.0f);
    b.cyl(Vec3(15.9f, top + 0.18f, 39.2f), Vec3(15.9f, top + 3.2f, 39.2f), 0.025f, 5);
    for (int k = 0; k < 4; k++) b.beam(Vec3(15.3f, top + 2.2f + k * 0.28f, 39.2f), Vec3(16.5f, top + 2.2f + k * 0.28f, 39.2f), 0.014f);
    b.m(M_RUST, 0x707070, 1.0f);
    b.beam(Vec3(13.2f, 0.4f, z0), Vec3(14.2f, 0.4f, z0 - 1.6f), 0.08f);
    b.beam(Vec3(15.2f, 0.4f, z0), Vec3(14.2f, 0.4f, z0 - 1.6f), 0.08f);
    b.collider(Vec3(13.0f, 0, z0 - 1.7f), Vec3(15.4f, 0.5f, z0), CF_LOW);
    b.m(M_TARP, 0x1c1c1c, 1.0f);
    b.box(Vec3(16.5f, 0, 33.0f), Vec3(17.1f, 0.5f, 33.7f));
    b.box(Vec3(16.6f, 0, 33.8f), Vec3(17.3f, 0.42f, 34.4f));
    b.box(Vec3(16.5f, 0.42f, 33.4f), Vec3(17.0f, 0.8f, 34.0f));
    b.collider(Vec3(16.5f, 0, 33.0f), Vec3(17.3f, 0.8f, 34.4f), CF_PROP);
    b.floorDecal("bootR", Vec3(10.6f, 0, 30.9f), 0.14f, 0.32f, PI * 0.5f);
    b.floorDecal("bootL", Vec3(10.0f, 0, 30.6f), 0.14f, 0.32f, PI * 0.5f);
    b.floorDecal("bootR", Vec3(9.3f, 0, 30.95f), 0.14f, 0.32f, PI * 0.52f);

    // ---- front room ---------------------------------------------------------
    float dy = fy + 0.76f;
    // reception desk under the east window
    b.m(M_WOOD, 0xa89478, 1.0f).g(0.7f);
    b.box(Vec3(15.3f, dy - 0.05f, 29.2f), Vec3(16.3f, dy, 31.5f));
    b.box(Vec3(15.35f, fy, 29.25f), Vec3(16.28f, dy - 0.05f, 29.8f));
    b.box(Vec3(15.35f, fy, 30.9f), Vec3(16.28f, dy - 0.05f, 31.45f));
    b.box(Vec3(15.32f, fy + 0.2f, 29.8f), Vec3(15.36f, dy - 0.05f, 30.9f));
    b.g(1.0f);
    b.collider(Vec3(15.3f, fy, 29.2f), Vec3(16.3f, dy, 31.5f), CF_PROP, SURF_WOOD);
    // terminal
    b.m(M_PLAIN, 0x8a8674, 2.0f);
    b.box(Vec3(15.62f, dy, 29.38f), Vec3(16.1f, dy + 0.42f, 29.9f));
    b.box(Vec3(15.4f, dy, 29.42f), Vec3(15.6f, dy + 0.03f, 29.86f)); // keyboard
    b.m(M_SCREEN, 0xffffff, 1.0f).fit();
    b.box(Vec3(15.61f, dy + 0.07f, 29.44f), Vec3(15.62f, dy + 0.37f, 29.84f), F_NX);
    for (int k = 0; k < 2; k++) {
        b.beginDyn();
        b.emissive(EG_CRT, 1.0f);
        b.sign(k ? "crt2" : "crt1", Vec3(0, 0, 0), Vec3(-1, 0, 0), 0.38f, 0.28f);
        b.noEmis();
        b.named(k ? DY_CRT_OFFICE2 : DY_CRT_OFFICE, b.endDyn(), Mat4::translate(Vec3(15.607f, dy + 0.22f, 29.64f)), false);
    }
    w.lightCrtOffice = b.light(Vec3(15.3f, dy + 0.25f, 29.64f), 0x50ff70, 0.9f, 3.5f, LG_SPECIAL, FL_BUZZ, 0.0f);
    w.lights[(size_t)w.lightCrtOffice].on = false;
    b.interact(IK_USE, U_CRT, Vec3(15.35f, dy, 29.35f), Vec3(16.1f, dy + 0.45f, 29.92f), 2.2f);
    // telephone
    b.m(M_PLAIN, 0x15130f, 2.0f);
    b.box(Vec3(15.7f, dy, 30.95f), Vec3(15.95f, dy + 0.07f, 31.2f));
    b.m(M_PLAIN, 0xc8c2b0, 2.0f);
    b.cyl(Vec3(15.825f, dy + 0.07f, 31.075f), Vec3(15.825f, dy + 0.075f, 31.075f), 0.07f, 10);
    b.beginDyn();
    b.m(M_PLAIN, 0x15130f, 2.0f);
    b.box(Vec3(-0.11f, 0, -0.022f), Vec3(0.11f, 0.03f, 0.022f));
    b.box(Vec3(-0.12f, -0.03f, -0.03f), Vec3(-0.06f, 0.01f, 0.03f));
    b.box(Vec3(0.06f, -0.03f, -0.03f), Vec3(0.12f, 0.01f, 0.03f));
    b.named(DY_PHONE, b.endDyn(), Mat4::translate(Vec3(15.825f, dy + 0.105f, 31.3f)));
    b.interact(IK_USE, U_PHONE, Vec3(15.6f, dy, 30.9f), Vec3(16.05f, dy + 0.25f, 31.42f), 2.2f);
    notePaper(b, N_INCIDENT, Vec3(15.62f, dy + 0.001f, 30.4f), Vec3(0, 1, 0), 0.24f, 0.33f, true);
    mug(b, 15.5f, dy, 30.05f);
    // desk lamp
    b.m(M_PAINTSTEEL, 0x2f4a36, 2.0f);
    b.cyl(Vec3(16.1f, dy, 30.6f), Vec3(16.1f, dy + 0.02f, 30.6f), 0.08f, 8);
    b.beam(Vec3(16.1f, dy, 30.6f), Vec3(16.05f, dy + 0.32f, 30.6f), 0.018f);
    b.cyl(Vec3(16.05f, dy + 0.3f, 30.6f), Vec3(15.92f, dy + 0.24f, 30.6f), 0.03f, 8, true, 0.08f);
    chair(b, 14.75f, fy, 30.25f, PI * 0.5f + 0.3f);
    // key cabinet on the east wall
    b.m(M_MACHINE, 0x6a7068, 1.5f);
    b.box(Vec3(16.2f, 1.6f, 32.3f), Vec3(16.33f, 2.4f, 33.1f));
    b.m(M_PLAIN, 0x101010, 2.0f);
    b.box(Vec3(16.215f, 1.63f, 32.33f), Vec3(16.3f, 2.37f, 33.07f), F_NX);
    b.beginDyn();
    b.m(M_MACHINE, 0x7a8078, 1.5f).fit();
    b.box(Vec3(0, 0, 0), Vec3(0.02f, 0.8f, 0.8f));
    b.m(M_RAIL, 0xb0b0b0, 2.0f);
    b.box(Vec3(-0.03f, 0.36f, 0.68f), Vec3(0, 0.44f, 0.72f));
    b.named(DY_KEYCAB_DOOR, b.endDyn(), Mat4::translate(Vec3(16.18f, 1.6f, 32.3f)));
    b.sign("keys", Vec3(16.33f, 2.52f, 32.7f), Vec3(-1, 0, 0), 0.36f);
    b.m(M_PLAIN, 0x202020, 2.0f);
    b.box(Vec3(16.28f, 1.7f, 33.2f), Vec3(16.33f, 2.02f, 33.42f));
    b.sign("keypad", Vec3(16.28f, 1.86f, 33.31f), Vec3(-1, 0, 0), 0.2f, 0.3f);
    b.interact(IK_USE, U_KEYPAD, Vec3(16.05f, 1.55f, 32.25f), Vec3(16.33f, 2.45f, 33.45f), 2.3f);
    // crane key + tape hang inside (revealed by the game when the cabinet opens)
    beginItem(b);
    b.m(M_RAIL, 0xc8b050, 2.0f);
    b.cyl(Vec3(0, 0.0f, 0), Vec3(0.008f, 0.0f, 0), 0.03f, 8);
    b.box(Vec3(0, -0.11f, -0.008f), Vec3(0.006f, -0.02f, 0.008f));
    b.box(Vec3(0, -0.11f, -0.008f), Vec3(0.006f, -0.09f, 0.022f));
    b.m(M_PLAIN, 0xc8a020, 2.0f);
    b.box(Vec3(-0.002f, -0.2f, -0.03f), Vec3(0.004f, -0.12f, 0.03f));
    endItem(b, IT_CRANE_KEY, Vec3(16.27f, 2.15f, 32.55f), 0.0f, 0.2f);
    itemTape(b, IT_TAPE1, Vec3(16.25f, 1.66f, 32.85f), PI * 0.5f);
    // filing cabinets
    b.m(M_MACHINE, 0x5f6a58, 1.0f).g(0.7f);
    b.box(Vec3(14.9f, fy, 28.1f), Vec3(15.4f, fy + 1.35f, 28.72f));
    b.box(Vec3(15.42f, fy, 28.1f), Vec3(15.92f, fy + 1.35f, 28.72f));
    b.g(1.0f);
    b.m(M_PLAIN, 0x20201c, 2.0f);
    for (int c = 0; c < 2; c++)
        for (int d = 0; d < 4; d++) {
            float cx = 14.9f + 0.52f * c, yy = fy + 0.05f + d * 0.33f;
            b.box(Vec3(cx + 0.03f, yy + 0.3f, 28.72f), Vec3(cx + 0.47f, yy + 0.31f, 28.725f));
            b.box(Vec3(cx + 0.18f, yy + 0.16f, 28.72f), Vec3(cx + 0.32f, yy + 0.19f, 28.745f));
        }
    // one drawer hangs open
    b.m(M_MACHINE, 0x5f6a58, 1.0f);
    b.box(Vec3(15.45f, fy + 0.72f, 28.72f), Vec3(15.89f, fy + 1.0f, 29.1f));
    b.collider(Vec3(14.9f, fy, 28.1f), Vec3(15.92f, fy + 1.35f, 28.74f), CF_PROP, SURF_METAL);
    // blueprint on the north wall
    b.m(M_CARDBOARD, 0x8a7a5a, 1.0f);
    b.box(Vec3(12.55f, 1.55f, 28.07f), Vec3(14.45f, 2.75f, 28.09f));
    b.sign("blueprint", Vec3(13.5f, 2.15f, 28.09f), Vec3(0, 0, 1), 1.75f);
    b.interact(IK_NOTE, N_BLUEPRINT, Vec3(12.6f, 1.55f, 28.07f), Vec3(14.4f, 2.75f, 28.25f), 2.4f);
    b.sign("poster", Vec3(15.45f, 2.55f, 28.075f), Vec3(0, 0, 1), 0.6f);
    // crew photo on the west wall
    b.sign("photo", Vec3(12.075f, 2.05f, 31.92f), Vec3(1, 0, 0), 0.66f);
    b.interact(IK_NOTE, N_PHOTO, Vec3(12.07f, 1.75f, 31.55f), Vec3(12.2f, 2.35f, 32.3f), 2.2f);
    b.sign("days", Vec3(15.35f, 2.2f, 35.44f), Vec3(0, 0, -1), 1.05f);
    b.sign("clock", Vec3(13.72f, 2.88f, 35.44f), Vec3(0, 0, -1), 0.3f);
    // coffee corner
    b.m(M_WOOD, 0x8a7a66, 1.0f).g(0.7f);
    b.box(Vec3(12.1f, fy, 28.3f), Vec3(12.7f, fy + 0.72f, 29.6f));
    b.g(1.0f);
    b.collider(Vec3(12.1f, fy, 28.3f), Vec3(12.7f, fy + 0.72f, 29.6f), CF_PROP, SURF_WOOD);
    b.m(M_PLAIN, 0x2a2622, 2.0f);
    b.box(Vec3(12.2f, fy + 0.72f, 28.45f), Vec3(12.5f, fy + 1.05f, 28.75f));
    b.m(M_GLASS, 0x402010, 1.0f).fit();
    b.cyl(Vec3(12.35f, fy + 0.75f, 28.6f), Vec3(12.35f, fy + 0.9f, 28.6f), 0.07f, 8);
    mug(b, 12.4f, fy + 0.72f, 29.05f, 0xb04020);
    mug(b, 12.3f, fy + 0.72f, 29.3f);
    // water cooler
    b.m(M_PLAIN, 0xb8b8b0, 2.0f);
    b.box(Vec3(12.15f, fy, 34.6f), Vec3(12.5f, fy + 1.0f, 34.95f));
    b.m(M_GLASS, 0x80b0c0, 1.0f).fit();
    b.cyl(Vec3(12.325f, fy + 1.0f, 34.775f), Vec3(12.325f, fy + 1.42f, 34.775f), 0.13f, 10);
    b.collider(Vec3(12.15f, fy, 34.6f), Vec3(12.5f, fy + 1.4f, 34.95f), CF_PROP);
    // door mat, bin, scattered paperwork
    b.m(M_RUBBER, 0x808080, 1.0f);
    b.box(Vec3(12.15f, fy, 30.25f), Vec3(12.9f, fy + 0.012f, 31.15f), F_PY | F_SIDES);
    b.m(M_PAINTSTEEL, 0x4a4e48, 2.0f);
    b.cyl(Vec3(14.3f, fy, 28.5f), Vec3(14.3f, fy + 0.32f, 28.5f), 0.14f, 8, false, 0.16f);
    Rng r(1982);
    paperLitter(b, r, 12.9f, 15.0f, 29.4f, 34.8f, fy, 14);
    fluorescent(b, Vec3(14.2f, top - 0.012f, 30.4f), false, LG_INDOOR, FL_DYING);
    fluorescent(b, Vec3(14.2f, top - 0.012f, 33.5f), false, LG_INDOOR, FL_BUZZ);
    // blinds over the windows (inside)
    b.m(M_BLINDS, 0xffffff, 1.0f);
    b.box(Vec3(x0 + 0.075f, 1.9f, 32.6f), Vec3(x0 + 0.085f, 2.5f, 34.4f), F_PX);
    b.box(Vec3(x1 - 0.085f, 1.75f, 29.6f), Vec3(x1 - 0.075f, 2.5f, 31.4f), F_NX);

    // ---- foreman's office ---------------------------------------------------
    b.m(M_WOOD, 0x6a5038, 1.0f).g(0.7f);
    b.box(Vec3(13.4f, dy - 0.05f, 38.2f), Vec3(15.7f, dy, 39.25f));
    b.box(Vec3(13.45f, fy, 38.25f), Vec3(14.0f, dy - 0.05f, 39.2f));
    b.box(Vec3(15.1f, fy, 38.25f), Vec3(15.65f, dy - 0.05f, 39.2f));
    b.box(Vec3(14.0f, fy + 0.15f, 39.15f), Vec3(15.1f, dy - 0.05f, 39.2f));
    b.g(1.0f);
    b.collider(Vec3(13.4f, fy, 38.2f), Vec3(15.7f, dy, 39.25f), CF_PROP, SURF_WOOD);
    notePaper(b, N_FOREMAN, Vec3(14.45f, dy + 0.001f, 38.62f), Vec3(0, 1, 0), 0.42f, 0.3f, true);
    // newspaper
    b.sign("news", Vec3(13.75f, dy + 0.002f - 0.012f, 38.7f), Vec3(0, 1, 0), 0.32f, 0.42f, 0.25f);
    b.interact(IK_NOTE, N_NEWSPAPER, Vec3(13.55f, dy, 38.45f), Vec3(13.95f, dy + 0.15f, 38.95f), 2.2f);
    w.notePos[N_NEWSPAPER] = Vec3(13.75f, dy, 38.7f);
    // radio set
    b.m(M_PANEL, 0x8a6a4a, 2.0f);
    b.box(Vec3(15.05f, dy, 38.75f), Vec3(15.6f, dy + 0.3f, 39.05f));
    b.sign("radio", Vec3(15.325f, dy + 0.15f, 38.75f), Vec3(0, 0, -1), 0.52f, 0.27f);
    b.interact(IK_USE, U_RADIO_SET, Vec3(15.0f, dy, 38.65f), Vec3(15.65f, dy + 0.35f, 39.1f), 2.2f);
    // bottle, glass, ashtray
    b.m(M_GLASS, 0x805020, 1.0f).fit();
    b.cyl(Vec3(14.95f, dy, 38.4f), Vec3(14.95f, dy + 0.2f, 38.4f), 0.035f, 8);
    b.cyl(Vec3(14.95f, dy + 0.2f, 38.4f), Vec3(14.95f, dy + 0.28f, 38.4f), 0.035f, 8, true, 0.012f);
    b.m(M_PLAIN, 0x30302c, 2.0f);
    b.cyl(Vec3(13.6f, dy, 39.05f), Vec3(13.6f, dy + 0.025f, 39.05f), 0.06f, 8);
    chair(b, 14.5f, fy, 37.55f, 0.15f);
    // bookshelf of binders on the east wall
    wallShelf(b, Vec3(15.95f, fy, 35.7f), Vec3(16.3f, fy + 1.9f, 36.5f), 5);
    static const u32 binders[5] = {0x7a2018, 0x1c3a5a, 0x2c4a2c, 0x5a5a52, 0x8a6a20};
    for (int s = 0; s < 4; s++)
        for (int k = 0; k < 9; k++) {
            if (hash2f(s, k) < 0.2f) continue;
            float z = 35.75f + k * 0.08f, yy = fy + 0.12f + s * 0.44f;
            b.m(M_CARDBOARD, binders[(s * 3 + k) % 5], 2.0f);
            b.box(Vec3(16.0f, yy, z), Vec3(16.27f, yy + 0.3f + 0.04f * hash2f(k, s), z + 0.065f));
        }
    itemBattery(b, IT_BATT1, Vec3(16.1f, fy + 1.44f + 0.01f, 36.42f), 0.4f);
    hideLocker(b, 4, Vec3(12.5f, fy, 36.05f), PI * 0.5f, 0x6a5a48);
    // coat stand: at a glance, somebody standing in the corner
    b.m(M_PLAIN, 0x1c1814, 2.0f);
    b.cyl(Vec3(15.95f, fy, 37.3f), Vec3(15.95f, fy + 1.85f, 37.3f), 0.022f, 6);
    b.cyl(Vec3(15.95f, fy, 37.3f), Vec3(15.95f, fy + 0.03f, 37.3f), 0.2f, 8);
    b.m(M_OILSKIN, 0xffffff, 1.0f);
    b.box(Vec3(15.72f, fy + 0.55f, 37.18f), Vec3(16.15f, fy + 1.6f, 37.42f));
    b.m(M_PAINTSTEEL, 0x8a7a22, 2.0f);
    b.cyl(Vec3(15.95f, fy + 1.82f, 37.3f), Vec3(15.95f, fy + 1.95f, 37.3f), 0.125f, 8, true, 0.09f);
    b.collider(Vec3(15.75f, fy, 37.1f), Vec3(16.15f, fy + 1.9f, 37.5f), COL_SOLID);
    fluorescent(b, Vec3(14.2f, top - 0.012f, 37.7f), false, LG_INDOOR, FL_BUZZ);
    b.m(M_BLINDS, 0xffffff, 1.0f);
    b.box(Vec3(x0 + 0.075f, 1.5f, 36.6f), Vec3(x0 + 0.085f, 2.5f, 38.4f), F_PX);
    b.box(Vec3(13.4f, 1.8f, z1 - 0.085f), Vec3(15.0f, 2.5f, z1 - 0.075f), F_NZ);
    b.sign("c_putback", Vec3(x1 - 0.075f, 2.75f, 37.5f), Vec3(-1, 0, 0), 1.7f);
    paperLitter(b, r, 12.6f, 15.8f, 35.9f, 38.0f, fy, 7);

    b.zone(Z_OFFICE, Vec3(x0, fy - 0.1f, z0), Vec3(x1, top, z1));
    b.trigger(TR_OFFICE_APPROACH, Vec3(7.0f, 0, 27.5f), Vec3(12.0f, 4, 34.0f));
    lampPost(b, 9.0f, 36.5f, PI * 0.5f, FL_NONE);
}

// ---------------------------------------------------------------------------
// Crew room + workshop
// ---------------------------------------------------------------------------
static void tallWall(Builder& b, int axis, float fixed, float a0, float a1, int inSign, const WallStyle& lo, const WallStyle& hi, const Opening* ops,
                     int nops, const Opening* opsHi = nullptr, int nopsHi = 0) {
    wallRun(b, axis, fixed, a0, a1, 0.0f, 2.6f, inSign, lo, ops, nops);
    wallRun(b, axis, fixed, a0, a1, 2.6f, 5.2f, inSign, hi, opsHi, nopsHi);
}

void forklift(Builder& b, float x, float z, float yaw, bool broken) {
    b.push(Mat4::translate(Vec3(x, 0, z)) * Mat4::rotY(yaw));
    b.m(M_PAINTSTEEL, 0x9a7a18, 0.8f).g(0.7f);
    b.box(Vec3(-0.6f, 0.3f, -1.1f), Vec3(0.6f, 1.05f, 0.7f));
    b.box(Vec3(-0.62f, 0.3f, 0.7f), Vec3(0.62f, 1.25f, 1.25f)); // counterweight
    b.g(1.0f);
    b.m(M_PLAIN, 0x202020, 1.0f);
    b.box(Vec3(-0.3f, 1.05f, -0.1f), Vec3(0.3f, 1.15f, 0.45f)); // seat
    b.box(Vec3(-0.3f, 1.15f, 0.35f), Vec3(0.3f, 1.6f, 0.45f));
    b.cyl(Vec3(0, 1.1f, -0.6f), Vec3(0, 1.45f, -0.4f), 0.02f, 6);
    b.cyl(Vec3(0, 1.45f, -0.4f), Vec3(0, 1.47f, -0.39f), 0.17f, 10);
    // overhead guard
    b.m(M_PAINTSTEEL, 0x30302c, 1.0f);
    for (int i = 0; i < 4; i++) {
        float lx = (i & 1) ? 0.55f : -0.55f, lz = (i & 2) ? 0.6f : -0.9f;
        b.beam(Vec3(lx, 1.05f, lz), Vec3(lx, 2.1f, lz), 0.05f);
    }
    b.box(Vec3(-0.6f, 2.1f, -0.95f), Vec3(0.6f, 2.15f, 0.65f));
    // mast and forks
    b.m(M_RUST, 0x909090, 1.0f);
    b.box(Vec3(-0.45f, 0.1f, -1.3f), Vec3(-0.33f, 2.4f, -1.15f));
    b.box(Vec3(0.33f, 0.1f, -1.3f), Vec3(0.45f, 2.4f, -1.15f));
    b.box(Vec3(-0.45f, 0.5f, -1.34f), Vec3(0.45f, 1.0f, -1.3f));
    b.box(Vec3(-0.4f, 0.08f, -2.3f), Vec3(-0.28f, 0.13f, -1.3f));
    b.box(Vec3(0.28f, 0.08f, -2.3f), Vec3(0.4f, 0.13f, -1.3f));
    // wheels
    b.m(M_RUBBER, 0xffffff, 1.0f);
    b.cyl(Vec3(-0.68f, 0.3f, -0.75f), Vec3(-0.5f, 0.3f, -0.75f), 0.3f, 10);
    b.cyl(Vec3(0.5f, 0.3f, -0.75f), Vec3(0.68f, 0.3f, -0.75f), 0.3f, 10);
    b.cyl(Vec3(-0.6f, 0.24f, 0.85f), Vec3(-0.44f, 0.24f, 0.85f), 0.24f, 10);
    if (!broken) b.cyl(Vec3(0.44f, 0.24f, 0.85f), Vec3(0.6f, 0.24f, 0.85f), 0.24f, 10);
    else {
        b.cyl(Vec3(1.3f, 0.0f, 1.1f), Vec3(1.3f, 0.16f, 1.1f), 0.24f, 10); // wheel lying on the floor
        b.m(M_RUST, 0x808080, 1.0f);
        b.cyl(Vec3(0.5f, 0, 0.85f), Vec3(0.5f, 0.3f, 0.85f), 0.06f, 6);     // jack stand
        b.box(Vec3(0.38f, 0, 0.73f), Vec3(0.62f, 0.03f, 0.97f));
    }
    b.collider(Vec3(-0.68f, 0, -2.3f), Vec3(0.68f, 2.15f, 1.25f), CF_PROP, SURF_METAL);
    b.pop();
}

static int chainMesh(Builder& b, float len, bool hook) {
    b.beginDyn();
    b.m(M_RUST, 0xa0a0a0, 3.0f);
    int n = (int)(len / 0.11f);
    for (int i = 0; i < n; i++) {
        float y = -i * 0.11f;
        if (i & 1) b.box(Vec3(-0.03f, y - 0.12f, -0.008f), Vec3(0.03f, y, 0.008f));
        else b.box(Vec3(-0.008f, y - 0.12f, -0.03f), Vec3(0.008f, y, 0.03f));
    }
    if (hook) {
        float y = -n * 0.11f;
        b.m(M_RUST, 0x707070, 2.0f);
        b.box(Vec3(-0.05f, y - 0.1f, -0.04f), Vec3(0.05f, y, 0.04f));
        b.beam(Vec3(0, y - 0.1f, 0), Vec3(0, y - 0.28f, 0), 0.035f);
        b.beam(Vec3(0, y - 0.28f, 0), Vec3(0.1f, y - 0.36f, 0), 0.035f);
        b.beam(Vec3(0.1f, y - 0.36f, 0), Vec3(0.17f, y - 0.26f, 0), 0.03f);
    }
    return b.endDyn();
}

void build_crewshop(Builder& b) {
    World& w = b.w;
    const float x0 = 16.0f, x1 = 40.0f, z0 = -12.0f, z1 = 6.0f, xp = 27.0f, top = 5.2f;
    WallStyle crewLo = {M_CORRUGATED, 0x76847a, 0.6f, M_BLOCK, 0xffffff, 0.385f, 0.24f, 0.55f};
    WallStyle crewHi = {M_CORRUGATED, 0x76847a, 0.6f, M_CONCWALL, 0xb0b4a8, 0.3f, 0.24f, 1.0f};
    WallStyle shopLo = {M_CORRUGATED, 0x76847a, 0.6f, M_CONCWALL, 0x9a9a94, 0.3f, 0.24f, 0.55f};
    WallStyle shopHi = {M_CORRUGATED, 0x76847a, 0.6f, M_CORRUGATED, 0x4a4e4a, 0.6f, 0.24f, 1.0f};
    // west wall (crew room): door + two windows
    Opening oW[3] = {{-9.5f, -7.5f, 1.4f, 2.4f, true}, {-3.55f, -2.45f, 0.0f, 2.12f, false}, {0.5f, 2.5f, 1.4f, 2.4f, true}};
    tallWall(b, 1, x0, z0, z1, +1, crewLo, crewHi, oW, 3);
    // north wall
    tallWall(b, 0, z0, x0, xp, +1, crewLo, crewHi, nullptr, 0);
    Opening oNs[1] = {{36.0f, 37.1f, 0.0f, 2.12f, false}};
    tallWall(b, 0, z0, xp, x1, +1, shopLo, shopHi, oNs, 1);
    // south wall
    Opening oSc[1] = {{19.0f, 21.5f, 1.4f, 2.4f, true}};
    tallWall(b, 0, z1, x0, xp, -1, crewLo, crewHi, oSc, 1);
    Opening oSs[1] = {{30.0f, 35.0f, 0.0f, 2.6f, false}}, oSsHi[1] = {{30.0f, 35.0f, 2.6f, 3.6f, false}};
    tallWall(b, 0, z1, xp, x1, -1, shopLo, shopHi, oSs, 1, oSsHi, 1);
    // roller door (closed for good)
    b.m(M_CORRUGATED, 0x4f5a54, 0.9f).g(0.6f);
    b.box(Vec3(30.0f, 0.0f, z1 - 0.06f), Vec3(35.0f, 3.6f, z1 + 0.02f));
    b.g(1.0f);
    b.m(M_HAZARD, 0xffffff, 0.8f);
    b.box(Vec3(30.0f, 0.0f, z1 + 0.02f), Vec3(35.0f, 0.4f, z1 + 0.035f), F_PZ);
    b.box(Vec3(30.0f, 0.0f, z1 - 0.075f), Vec3(35.0f, 0.4f, z1 - 0.06f), F_NZ);
    b.collider(Vec3(30.0f, 0, z1 - 0.12f), Vec3(35.0f, 3.6f, z1 + 0.12f), CF_SOLID, SURF_METAL);
    // east wall
    tallWall(b, 1, x1, z0, z1, -1, shopLo, shopHi, nullptr, 0);
    // partition
    WallStyle partLo = {M_CONCWALL, 0x9a9a94, 0.3f, M_BLOCK, 0xffffff, 0.385f, 0.2f, 0.7f};
    WallStyle partHi = {M_CORRUGATED, 0x4a4e4a, 0.6f, M_CONCWALL, 0xb0b4a8, 0.3f, 0.2f, 1.0f};
    Opening oP[1] = {{-1.0f, 0.1f, 0.0f, 2.12f, false}};
    wallRun(b, 1, xp, z0 + 0.12f, z1 - 0.12f, 0.0f, 2.6f, -1, partLo, oP, 1);
    wallRun(b, 1, xp, z0 + 0.12f, z1 - 0.12f, 2.6f, top, -1, partHi);
    // roof
    b.m(M_ROOF, 0xffffff, 0.3f);
    b.solid(Vec3(x0 - 0.3f, top, z0 - 0.3f), Vec3(x1 + 0.3f, top + 0.25f, z1 + 0.3f), SURF_METAL);
    b.m(M_CORRUGATED, 0x3a3e3a, 0.6f);
    b.box(Vec3(x0, top - 0.012f, z0), Vec3(x1, top, z1), F_NY);
    b.m(M_RUST, 0x808080, 1.0f);
    for (float tx = 19.0f; tx < 39.5f; tx += 3.5f) {
        b.beam(Vec3(tx, top - 0.2f, z0 + 0.12f), Vec3(tx, top - 0.2f, z1 - 0.12f), 0.1f, 0.22f);
        b.beam(Vec3(tx, top - 0.9f, z0 + 0.12f), Vec3(tx, top - 0.9f, z1 - 0.12f), 0.07f);
        for (float tz = z0 + 0.12f; tz < z1 - 3.0f; tz += 2.96f) {
            b.beam(Vec3(tx, top - 0.9f, tz), Vec3(tx, top - 0.25f, tz + 1.48f), 0.05f);
            b.beam(Vec3(tx, top - 0.25f, tz + 1.48f), Vec3(tx, top - 0.9f, tz + 2.96f), 0.05f);
        }
    }
    // floors
    b.m(M_LINO, 0xb8b8b0, 0.5f);
    b.box(Vec3(x0 + 0.12f, 0, z0 + 0.12f), Vec3(xp - 0.1f, 0.012f, z1 - 0.12f), F_PY);
    b.collider(Vec3(x0 + 0.12f, -0.2f, z0 + 0.12f), Vec3(xp - 0.1f, 0.012f, z1 - 0.12f), COL_SOLID, SURF_LINO);
    b.m(M_CONCRETE, 0x8a8a84, 0.25f);
    b.box(Vec3(xp + 0.1f, 0, z0 + 0.12f), Vec3(x1 - 0.12f, 0.012f, z1 - 0.12f), F_PY);
    b.collider(Vec3(xp + 0.1f, -0.2f, z0 + 0.12f), Vec3(x1 - 0.12f, 0.012f, z1 - 0.12f), COL_SOLID, SURF_CONCRETE);
    // doors
    makeDoor(b, D_CREW, Vec3(x0, 0, -3.55f), 1.1f, 2.1f, -PI * 0.5f, -1.5f, M_PAINTSTEEL, 0x5a6a5e, -1, true);
    makeDoor(b, D_CREW_SHOP, Vec3(xp, 0, -1.0f), 1.1f, 2.1f, -PI * 0.5f, 1.5f, M_PAINTSTEEL, 0x6a6a60, -1, true);
    makeDoor(b, D_SHOP_BACK, Vec3(36.0f, 0, z0), 1.1f, 2.1f, 0.0f, 1.5f, M_PAINTSTEEL, 0x5a6a5e, -1, true);
    // exterior signs + lamps
    b.sign("crew", Vec3(x0 - 0.125f, 2.5f, -3.0f), Vec3(-1, 0, 0), 1.1f);
    b.sign("days", Vec3(x0 - 0.125f, 1.85f, -0.9f), Vec3(-1, 0, 0), 1.3f);
    b.sign("workshop", Vec3(32.5f, 4.2f, z1 + 0.125f), Vec3(0, 0, 1), 2.4f);
    b.sign("hardhat", Vec3(28.6f, 1.9f, z1 + 0.125f), Vec3(0, 0, 1), 0.9f);
    cagedLamp(b, Vec3(x0 - 0.12f, 2.95f, -3.0f), Vec3(-1, 0, 0), LG_YARD, FL_BUZZ, 0xffc080, 1.1f, 7.0f);
    cagedLamp(b, Vec3(36.55f, 2.6f, z0 - 0.12f), Vec3(0, 0, -1), LG_YARD, FL_DYING, 0xffc080, 1.1f, 7.0f);
    for (int i = 0; i < 3; i++) b.sign("drip", Vec3(20.0f + i * 7.3f, 3.6f, z1 + 0.125f), Vec3(0, 0, 1), 0.7f, 3.0f);
    for (int i = 0; i < 3; i++) b.sign("drip", Vec3(x0 - 0.125f, 3.7f, -10.5f + i * 6.1f), Vec3(-1, 0, 0), 0.6f, 2.8f);

    // ---- crew room ----------------------------------------------------------
    // locker bank on the north wall
    static const char* tags[8] = {"n_ruiz", "n_moss", "n_dale", "n_terry", "n_walt", "n_anand", "n_okoro", "n_relief"};
    const float lz0 = z0 + 0.12f, lz1 = z0 + 0.62f, lx = 17.2f, lw = 0.5f, lh = 1.9f;
    b.m(M_PAINTSTEEL, 0x55685c, 1.0f).g(0.7f);
    b.box(Vec3(lx, 0, lz0), Vec3(lx + 8 * lw, 0.08f, lz1));
    b.box(Vec3(lx, lh, lz0), Vec3(lx + 8 * lw, lh + 0.03f, lz1));
    b.box(Vec3(lx - 0.02f, 0, lz0), Vec3(lx, lh + 0.03f, lz1));
    b.box(Vec3(lx + 8 * lw, 0, lz0), Vec3(lx + 8 * lw + 0.02f, lh + 0.03f, lz1));
    b.box(Vec3(lx, 0.08f, lz0), Vec3(lx + 8 * lw, lh, lz0 + 0.02f));
    b.g(1.0f);
    for (int i = 0; i < 8; i++) {
        float xa = lx + i * lw;
        b.m(M_PAINTSTEEL, 0x55685c, 1.0f);
        b.box(Vec3(xa + lw - 0.012f, 0.08f, lz0), Vec3(xa + lw + 0.012f, lh, lz1 - 0.01f)); // divider
        if (i == 2) {
            // Dale's locker stands open
            b.m(M_LOCKER, 0x6f8676, 1.0f).fit();
            b.box(Vec3(xa + 0.01f, 0.08f, lz1), Vec3(xa + 0.03f, lh, lz1 + 0.47f));
            b.m(M_PLAIN, 0x0c0c0c, 1.0f);
            b.box(Vec3(xa + 0.02f, 0.08f, lz0 + 0.02f), Vec3(xa + lw - 0.02f, lh, lz0 + 0.03f), F_PZ);
            b.m(M_PAINTSTEEL, 0x55685c, 1.0f);
            b.box(Vec3(xa + 0.02f, 1.5f, lz0 + 0.02f), Vec3(xa + lw - 0.02f, 1.52f, lz1 - 0.05f)); // shelf
            b.m(M_TARP, 0x8a6a2a, 2.0f);
            b.box(Vec3(xa + 0.08f, 0.5f, lz0 + 0.06f), Vec3(xa + 0.42f, 1.42f, lz0 + 0.2f));     // hanging hi-vis
            b.m(M_RUBBER, 0x505050, 2.0f);
            b.box(Vec3(xa + 0.08f, 0.08f, lz0 + 0.1f), Vec3(xa + 0.22f, 0.34f, lz0 + 0.4f));     // boots
            b.box(Vec3(xa + 0.27f, 0.08f, lz0 + 0.1f), Vec3(xa + 0.41f, 0.34f, lz0 + 0.4f));
            // office key on the shelf
            beginItem(b);
            b.m(M_RAIL, 0xc0c0c0, 2.0f);
            b.cyl(Vec3(0, 0.004f, 0), Vec3(0, 0.008f, 0), 0.022f, 8);
            b.box(Vec3(0.015f, 0.002f, -0.006f), Vec3(0.085f, 0.008f, 0.006f));
            b.box(Vec3(0.06f, 0.002f, -0.006f), Vec3(0.085f, 0.008f, 0.018f));
            b.m(M_PLAIN, 0x2a5a8a, 2.0f);
            b.box(Vec3(-0.08f, 0.0f, -0.02f), Vec3(-0.02f, 0.008f, 0.02f));
            endItem(b, IT_OFFICE_KEY, Vec3(xa + 0.24f, 1.523f, lz0 + 0.3f), 0.5f, 0.26f);
        } else {
            b.m(M_LOCKER, 0x6f8676, 1.0f).fit();
            b.box(Vec3(xa + 0.012f, 0.08f, lz1 - 0.02f), Vec3(xa + lw - 0.012f, lh, lz1));
        }
        b.sign(tags[i], Vec3(xa + lw * 0.5f, 1.72f, lz1 + (i == 2 ? -0.47f : 0.0f)), Vec3(0, 0, 1), 0.2f);
        if (i == 4) { // Walt's: padlocked
            b.m(M_RAIL, 0xb09a40, 2.0f);
            b.box(Vec3(xa + 0.39f, 0.92f, lz1), Vec3(xa + 0.45f, 1.0f, lz1 + 0.03f));
        }
    }
    b.collider(Vec3(lx - 0.02f, 0, lz0), Vec3(lx + 8 * lw + 0.02f, lh + 0.03f, lz1), CF_FENCE, SURF_METAL);
    b.interact(IK_NOTE, N_RELIEF_LOCKER, Vec3(lx + 7 * lw, 0.3f, lz1 - 0.05f), Vec3(lx + 8 * lw, lh, lz1 + 0.1f), 2.2f);
    w.notePos[N_RELIEF_LOCKER] = Vec3(lx + 7.5f * lw, 1.2f, lz1);
    hideLocker(b, 0, Vec3(22.3f, 0, z0 + 0.5f), 0.0f, 0x55685c);
    hideLocker(b, 1, Vec3(17.0f, 0, z1 - 0.5f), PI, 0x55685c);
    // benches
    b.m(M_WOOD, 0xb0a088, 1.0f);
    for (int k = 0; k < 2; k++) {
        float bx = 17.4f + k * 2.0f;
        b.box(Vec3(bx, 0.42f, -10.25f), Vec3(bx + 1.8f, 0.47f, -9.85f));
        b.m(M_PAINTSTEEL, 0x30302c, 1.0f);
        b.box(Vec3(bx + 0.1f, 0, -10.2f), Vec3(bx + 0.15f, 0.42f, -9.9f));
        b.box(Vec3(bx + 1.65f, 0, -10.2f), Vec3(bx + 1.7f, 0.42f, -9.9f));
        b.m(M_WOOD, 0xb0a088, 1.0f);
        b.collider(Vec3(bx, 0, -10.25f), Vec3(bx + 1.8f, 0.47f, -9.85f), CF_LOW, SURF_WOOD);
    }
    // a single wet boot, left on the bench
    b.m(M_RUBBER, 0x606060, 2.0f);
    b.box(Vec3(18.2f, 0.47f, -10.2f), Vec3(18.34f, 0.75f, -9.92f));
    b.floorDecal("puddle", Vec3(18.3f, 0.012f, -9.6f), 1.2f, 1.0f);
    // time cards + punch clock by the door
    b.m(M_WOOD, 0x6a5a48, 1.0f);
    b.box(Vec3(x0 + 0.12f, 1.2f, -2.2f), Vec3(x0 + 0.16f, 1.95f, -1.35f));
    b.m(M_PAPER, 0xd8d0b0, 3.0f);
    for (int i = 0; i < 7; i++) {
        if (i == 3) continue;
        b.box(Vec3(x0 + 0.16f, 1.25f + 0.02f * (i % 2), -2.14f + i * 0.11f), Vec3(x0 + 0.175f, 1.55f + 0.02f * (i % 2), -2.06f + i * 0.11f));
        b.box(Vec3(x0 + 0.16f, 1.6f, -2.14f + i * 0.11f), Vec3(x0 + 0.175f, 1.9f, -2.06f + i * 0.11f));
    }
    b.sign("timecards", Vec3(x0 + 0.125f, 2.08f, -1.78f), Vec3(1, 0, 0), 0.6f);
    b.interact(IK_NOTE, N_TIMECARD, Vec3(x0 + 0.12f, 1.15f, -2.25f), Vec3(x0 + 0.3f, 2.0f, -1.3f), 2.2f);
    w.notePos[N_TIMECARD] = Vec3(x0 + 0.2f, 1.6f, -1.8f);
    b.m(M_MACHINE, 0x4a5a50, 2.0f);
    b.box(Vec3(x0 + 0.12f, 1.25f, -1.15f), Vec3(x0 + 0.3f, 1.8f, -0.7f));
    b.sign("punch", Vec3(x0 + 0.3f, 1.525f, -0.925f), Vec3(1, 0, 0), 0.4f, 0.5f);
    // table, chairs, cards
    b.m(M_WOOD, 0x9a8a70, 1.0f).g(0.8f);
    b.box(Vec3(21.5f, 0.71f, 0.5f), Vec3(23.5f, 0.76f, 1.9f));
    for (int i = 0; i < 4; i++) {
        float tx = (i & 1) ? 23.4f : 21.55f, tz = (i & 2) ? 1.8f : 0.55f;
        b.box(Vec3(tx, 0, tz), Vec3(tx + 0.06f, 0.71f, tz + 0.06f));
    }
    b.g(1.0f);
    b.collider(Vec3(21.5f, 0, 0.5f), Vec3(23.5f, 0.76f, 1.9f), CF_PROP, SURF_WOOD);
    mug(b, 21.9f, 0.76f, 0.9f);
    mug(b, 22.9f, 0.76f, 1.5f, 0x3a5a8a);
    mug(b, 23.2f, 0.76f, 0.8f);
    b.m(M_PAPER, 0xe0e0d8, 6.0f);
    for (int i = 0; i < 9; i++) {
        b.push(Mat4::translate(Vec3(22.3f + hash2f(i, 1) * 0.6f, 0.761f + i * 0.0005f, 1.0f + hash2f(i, 2) * 0.5f)) * Mat4::rotY(hash2f(i, 3) * TAU));
        b.box(Vec3(-0.03f, 0, -0.045f), Vec3(0.03f, 0.001f, 0.045f), F_PY);
        b.pop();
    }
    b.m(M_PLAIN, 0x30302c, 2.0f);
    b.cyl(Vec3(22.5f, 0.76f, 0.75f), Vec3(22.5f, 0.785f, 0.75f), 0.07f, 8);
    itemBattery(b, IT_BATT2, Vec3(21.85f, 0.762f, 1.55f), 1.1f);
    chair(b, 22.0f, 0.012f, 0.0f, 0.2f);
    chair(b, 23.9f, 0.012f, 1.2f, -PI * 0.5f);
    chair(b, 22.6f, 0.012f, 2.75f, 2.4f, true);
    // vending machine
    b.m(M_MACHINE, 0x6a1c14, 1.0f).g(0.7f);
    b.box(Vec3(26.15f, 0, 2.8f), Vec3(26.9f, 1.85f, 3.7f));
    b.g(1.0f);
    b.sign("vend", Vec3(26.15f, 0.95f, 3.25f), Vec3(-1, 0, 0), 0.86f, 1.74f);
    b.collider(Vec3(26.15f, 0, 2.8f), Vec3(26.9f, 1.85f, 3.7f), CF_PROP, SURF_METAL);
    b.interact(IK_USE, U_VENDING, Vec3(26.0f, 0.3f, 2.8f), Vec3(26.3f, 1.85f, 3.7f), 2.2f);
    itemBattery(b, IT_BATT8, Vec3(26.05f, 0.2f, 3.25f), 0.0f);
    // sink + mirror
    b.m(M_PLAIN, 0xc8c8c0, 2.0f);
    b.box(Vec3(26.45f, 0.75f, -6.5f), Vec3(26.9f, 0.9f, -5.5f));
    b.m(M_PLAIN, 0x30302c, 2.0f);
    b.box(Vec3(26.5f, 0.86f, -6.4f), Vec3(26.85f, 0.905f, -5.6f), F_PY);
    b.m(M_RAIL, 0xc0c0c0, 2.0f);
    b.beam(Vec3(26.85f, 0.9f, -6.0f), Vec3(26.85f, 1.08f, -6.0f), 0.025f);
    b.beam(Vec3(26.85f, 1.08f, -6.0f), Vec3(26.68f, 1.05f, -6.0f), 0.025f);
    b.cyl(Vec3(26.75f, 0.2f, -6.0f), Vec3(26.75f, 0.75f, -6.0f), 0.04f, 6);
    b.m(M_SCREEN, 0xa0b0b0, 1.0f).fit();
    b.box(Vec3(26.885f, 1.3f, -6.35f), Vec3(26.9f, 2.0f, -5.65f), F_NX);
    b.sign("hand", Vec3(26.884f, 1.62f, -5.95f), Vec3(-1, 0, 0), 0.3f, 0.38f);
    b.collider(Vec3(26.45f, 0, -6.5f), Vec3(26.9f, 0.9f, -5.5f), CF_PROP);
    b.interact(IK_USE, U_SINK, Vec3(26.4f, 0.7f, -6.5f), Vec3(26.9f, 2.0f, -5.5f), 2.2f);
    // hard hat shelf on the south wall
    b.m(M_WOOD, 0x8a7a66, 1.0f);
    b.box(Vec3(22.5f, 1.68f, z1 - 0.5f), Vec3(25.6f, 1.72f, z1 - 0.12f));
    static const u32 hatc[4] = {0x9a8420, 0x9a8420, 0xb0b0a8, 0x9a8420};
    for (int i = 0; i < 4; i++) {
        float hx = 22.85f + i * 0.6f;
        b.m(M_PAINTSTEEL, hatc[i], 2.0f);
        b.cyl(Vec3(hx, 1.72f, z1 - 0.3f), Vec3(hx, 1.86f, z1 - 0.3f), 0.125f, 8, true, 0.09f);
        b.cyl(Vec3(hx, 1.72f, z1 - 0.3f), Vec3(hx, 1.735f, z1 - 0.3f), 0.16f, 8, true);
    }
    b.beginDyn();
    b.m(M_PAINTSTEEL, 0x9a8420, 2.0f);
    b.cyl(Vec3(0, 0, 0), Vec3(0, 0.14f, 0), 0.125f, 8, true, 0.09f);
    b.cyl(Vec3(0, 0, 0), Vec3(0, 0.015f, 0), 0.16f, 8, true);
    b.named(DY_HARDHAT, b.endDyn(), Mat4::translate(Vec3(25.3f, 1.72f, z1 - 0.3f)));
    // coats on hooks
    b.m(M_OILSKIN, 0xffffff, 1.0f);
    for (int i = 0; i < 3; i++) b.box(Vec3(23.0f + i * 0.7f, 0.6f, z1 - 0.2f), Vec3(23.45f + i * 0.7f, 1.55f, z1 - 0.12f));
    b.sign("poster", Vec3(x0 + 0.125f, 1.9f, 3.9f), Vec3(1, 0, 0), 0.62f);
    b.sign("c_monday", Vec3(xp - 0.105f, 2.35f, -3.6f), Vec3(-1, 0, 0), 2.9f);
    b.sign("nosmoke", Vec3(24.5f, 2.0f, z0 + 0.125f), Vec3(0, 0, 1), 0.42f);
    b.sign("exit", Vec3(x0 + 0.125f, 2.38f, -3.0f), Vec3(1, 0, 0), 0.45f);
    // lights (hung low on rods)
    static const float cl[3][3] = {{19.5f, -7.0f, FL_BUZZ}, {19.5f, 1.5f, FL_NONE}, {24.3f, -3.0f, FL_DYING}};
    for (auto& c : cl) {
        b.m(M_RUST, 0x808080, 2.0f);
        b.beam(Vec3(c[0] - 0.4f, 3.4f, c[1]), Vec3(c[0] - 0.4f, top, c[1]), 0.012f);
        b.beam(Vec3(c[0] + 0.4f, 3.4f, c[1]), Vec3(c[0] + 0.4f, top, c[1]), 0.012f);
        fluorescent(b, Vec3(c[0], 3.4f, c[1]), true, LG_INDOOR, (int)c[2], 1.2f);
    }
    Rng r(114);
    paperLitter(b, r, 17.0f, 26.0f, -8.5f, 4.5f, 0.012f, 10);
    b.zone(Z_CREW, Vec3(x0, -0.1f, z0), Vec3(xp, top, z1));
    b.trigger(TR_CREWROOM, Vec3(x0 + 0.3f, 0, z0 + 0.3f), Vec3(xp - 0.3f, 3, z1 - 0.3f));

    // ---- workshop -----------------------------------------------------------
    // workbench
    b.m(M_WOOD, 0x8a7a60, 1.0f).g(0.8f);
    b.box(Vec3(28.0f, 0.86f, z0 + 0.13f), Vec3(34.5f, 0.92f, z0 + 1.0f));
    b.g(1.0f);
    b.m(M_PAINTSTEEL, 0x4a4e48, 1.0f);
    for (float bx = 28.05f; bx < 34.5f; bx += 2.1f) {
        b.box(Vec3(bx, 0, z0 + 0.2f), Vec3(bx + 0.06f, 0.86f, z0 + 0.26f));
        b.box(Vec3(bx, 0, z0 + 0.9f), Vec3(bx + 0.06f, 0.86f, z0 + 0.96f));
    }
    b.box(Vec3(28.0f, 0.25f, z0 + 0.15f), Vec3(34.5f, 0.28f, z0 + 0.98f));
    b.collider(Vec3(28.0f, 0, z0 + 0.13f), Vec3(34.5f, 0.92f, z0 + 1.0f), CF_PROP, SURF_WOOD);
    // vise
    b.m(M_PAINTSTEEL, 0x2a4a6a, 2.0f);
    b.box(Vec3(28.5f, 0.92f, z0 + 0.6f), Vec3(28.8f, 1.02f, z0 + 0.95f));
    b.box(Vec3(28.55f, 1.02f, z0 + 0.62f), Vec3(28.75f, 1.16f, z0 + 0.7f));
    b.box(Vec3(28.55f, 1.02f, z0 + 0.8f), Vec3(28.75f, 1.16f, z0 + 0.88f));
    b.m(M_RAIL, 0xb0b0b0, 2.0f);
    b.cyl(Vec3(28.65f, 1.08f, z0 + 0.88f), Vec3(28.65f, 1.08f, z0 + 1.1f), 0.012f, 5);
    // pegboard with tools
    b.m(M_CARDBOARD, 0x9a8a6a, 1.0f);
    b.box(Vec3(28.2f, 1.25f, z0 + 0.125f), Vec3(34.3f, 2.3f, z0 + 0.14f));
    b.m(M_RAIL, 0x909090, 2.0f);
    Rng tr(77);
    for (int i = 0; i < 14; i++) {
        float tx = 28.5f + i * 0.41f;
        if (i == 5 || i == 9) continue; // outlines where tools are missing
        float len = tr.range(0.25f, 0.6f);
        b.box(Vec3(tx, 2.15f - len, z0 + 0.14f), Vec3(tx + 0.035f, 2.15f, z0 + 0.155f));
        b.box(Vec3(tx - 0.03f, 2.1f, z0 + 0.14f), Vec3(tx + 0.065f, 2.17f, z0 + 0.16f));
    }
    // bolt cutters on the bench
    beginItem(b);
    b.m(M_PLAIN, 0x8a1c14, 2.0f);
    b.beam(Vec3(-0.34f, 0.02f, -0.05f), Vec3(0.12f, 0.02f, -0.012f), 0.03f);
    b.beam(Vec3(-0.34f, 0.02f, 0.05f), Vec3(0.12f, 0.02f, 0.012f), 0.03f);
    b.m(M_RAIL, 0x808080, 2.0f);
    b.box(Vec3(0.1f, 0.005f, -0.035f), Vec3(0.2f, 0.035f, 0.035f));
    b.beam(Vec3(0.2f, 0.02f, -0.02f), Vec3(0.32f, 0.02f, -0.006f), 0.022f);
    b.beam(Vec3(0.2f, 0.02f, 0.02f), Vec3(0.32f, 0.02f, 0.006f), 0.022f);
    endItem(b, IT_CUTTERS, Vec3(31.0f, 0.921f, z0 + 0.62f), 0.35f, 0.4f);
    itemBattery(b, IT_BATT3, Vec3(33.6f, 0.921f, z0 + 0.5f), 2.0f);
    // coffee can of bolts, grinder, rags
    b.m(M_RUST, 0xa0a0a0, 2.0f);
    b.cyl(Vec3(29.6f, 0.92f, z0 + 0.4f), Vec3(29.6f, 1.06f, z0 + 0.4f), 0.055f, 8);
    b.m(M_MACHINE, 0x3a5a3a, 2.0f);
    b.box(Vec3(32.2f, 0.92f, z0 + 0.35f), Vec3(32.6f, 1.15f, z0 + 0.6f));
    b.cyl(Vec3(32.15f, 1.06f, z0 + 0.47f), Vec3(32.2f, 1.06f, z0 + 0.47f), 0.09f, 10);
    b.cyl(Vec3(32.6f, 1.06f, z0 + 0.47f), Vec3(32.65f, 1.06f, z0 + 0.47f), 0.09f, 10);
    b.m(M_TARP, 0x6a2a1a, 2.0f);
    b.box(Vec3(30.0f, 0.92f, z0 + 0.3f), Vec3(30.3f, 0.95f, z0 + 0.55f));
    // tool chest
    b.m(M_MACHINE, 0x8a1c14, 1.0f).g(0.8f);
    b.box(Vec3(34.8f, 0.1f, z0 + 0.15f), Vec3(35.7f, 1.05f, z0 + 0.7f));
    b.g(1.0f);
    b.collider(Vec3(34.8f, 0, z0 + 0.15f), Vec3(35.7f, 1.05f, z0 + 0.7f), CF_PROP, SURF_METAL);
    // shelving on the east wall
    wallShelf(b, Vec3(x1 - 0.8f, 0, -9.0f), Vec3(x1 - 0.13f, 2.2f, -3.0f), 5);
    Rng sr(31);
    for (int s = 0; s < 4; s++)
        for (int k = 0; k < 9; k++) {
            float z = -8.8f + k * 0.63f, yy = 0.12f + s * 0.515f;
            if ((s == 1 && k == 6) || (s == 2 && k == 2)) continue;
            float kind = sr.f();
            if (kind < 0.3f) {
                b.m(M_PAINTSTEEL, sr.chance(0.5f) ? 0x8a8a80 : 0x3a5a7a, 2.0f);
                b.cyl(Vec3(x1 - 0.45f, yy, z + 0.2f), Vec3(x1 - 0.45f, yy + 0.25f, z + 0.2f), 0.1f, 8);
            } else if (kind < 0.65f) {
                b.m(M_CARDBOARD, 0xffffff, 1.5f);
                b.box(Vec3(x1 - 0.7f, yy, z + 0.02f), Vec3(x1 - 0.25f, yy + sr.range(0.2f, 0.36f), z + 0.5f));
            } else if (kind < 0.85f) {
                b.m(M_WOOD, 0xb0a890, 1.5f);
                b.box(Vec3(x1 - 0.7f, yy, z + 0.05f), Vec3(x1 - 0.3f, yy + 0.3f, z + 0.5f));
            }
        }
    // jerry can (shelf 1) and a fuse in a parts bin (shelf 2)
    beginItem(b);
    b.m(M_PAINTSTEEL, 0x4a5a2a, 1.0f);
    b.box(Vec3(-0.17f, 0, -0.08f), Vec3(0.17f, 0.4f, 0.08f));
    b.box(Vec3(-0.1f, 0.4f, -0.02f), Vec3(0.1f, 0.46f, 0.02f));
    b.cyl(Vec3(0.13f, 0.4f, 0), Vec3(0.15f, 0.47f, 0), 0.03f, 6);
    endItem(b, IT_JERRYCAN, Vec3(x1 - 0.45f, 0.12f + 0.515f, -8.8f + 6 * 0.63f + 0.3f), PI * 0.5f, 0.4f);
    b.m(M_PLAIN, 0x2a4a7a, 2.0f);
    {
        float z = -8.8f + 2 * 0.63f, yy = 0.12f + 2 * 0.515f;
        b.box(Vec3(x1 - 0.7f, yy, z + 0.05f), Vec3(x1 - 0.3f, yy + 0.02f, z + 0.5f));
        b.box(Vec3(x1 - 0.7f, yy, z + 0.05f), Vec3(x1 - 0.68f, yy + 0.12f, z + 0.5f));
        b.box(Vec3(x1 - 0.32f, yy, z + 0.05f), Vec3(x1 - 0.3f, yy + 0.12f, z + 0.5f));
        b.box(Vec3(x1 - 0.7f, yy, z + 0.05f), Vec3(x1 - 0.3f, yy + 0.12f, z + 0.07f));
        b.box(Vec3(x1 - 0.7f, yy, z + 0.48f), Vec3(x1 - 0.3f, yy + 0.12f, z + 0.5f));
        itemFuse(b, IT_FUSE_A, Vec3(x1 - 0.5f, yy + 0.02f, z + 0.28f), 0.4f);
    }
    forklift(b, 33.0f, 1.6f, 0.5f, true);
    // engine hoist gantry + engine block
    b.m(M_PAINTSTEEL, 0x8a2a18, 1.0f);
    for (int s = -1; s <= 1; s += 2) {
        b.beam(Vec3(29.2f, 0, -4.0f + s * 0.9f), Vec3(30.0f, 3.0f, -4.0f), 0.08f);
        b.beam(Vec3(30.8f, 0, -4.0f + s * 0.9f), Vec3(30.0f, 3.0f, -4.0f), 0.08f);
    }
    b.beam(Vec3(30.0f, 3.0f, -5.2f), Vec3(30.0f, 3.0f, -2.8f), 0.1f, 0.14f);
    b.collider(Vec3(29.1f, 0, -5.0f), Vec3(29.35f, 2.2f, -3.0f), COL_SOLID);
    b.collider(Vec3(30.65f, 0, -5.0f), Vec3(30.9f, 2.2f, -3.0f), COL_SOLID);
    b.named(DY_CHAIN1, chainMesh(b, 1.3f, true), Mat4::translate(Vec3(30.0f, 2.93f, -4.0f)));
    b.m(M_MACHINE, 0x4a4a44, 2.0f).g(0.8f);
    b.box(Vec3(29.7f, 0.35f, -4.35f), Vec3(30.3f, 0.95f, -3.65f));
    b.g(1.0f);
    b.m(M_RUST, 0x808080, 1.0f);
    b.box(Vec3(29.6f, 0, -4.4f), Vec3(30.4f, 0.35f, -3.6f));
    b.collider(Vec3(29.6f, 0, -4.4f), Vec3(30.4f, 0.95f, -3.6f), CF_PROP, SURF_METAL);
    b.floorDecal("oil2", Vec3(30.2f, 0.012f, -3.4f), 2.4f, 1.9f, 0.6f);
    b.floorDecal("oil1", Vec3(33.6f, 0.012f, 2.4f), 2.0f, 2.0f, 1.9f);
    // welding cart
    b.m(M_PAINTSTEEL, 0x30302c, 1.0f);
    b.box(Vec3(38.1f, 0.1f, 2.6f), Vec3(38.9f, 0.16f, 3.3f));
    b.m(M_PAINTSTEEL, 0x2a5a8a, 1.0f);
    b.cyl(Vec3(38.3f, 0.16f, 2.95f), Vec3(38.3f, 1.4f, 2.95f), 0.12f, 8);
    b.m(M_PAINTSTEEL, 0x8a2a18, 1.0f);
    b.cyl(Vec3(38.65f, 0.16f, 2.95f), Vec3(38.65f, 1.3f, 2.95f), 0.11f, 8);
    b.collider(Vec3(38.1f, 0, 2.6f), Vec3(38.9f, 1.4f, 3.3f), CF_PROP, SURF_METAL);
    barrel(b, 29.0f, 5.2f, 0x2a4a6a);
    barrel(b, 29.7f, 5.25f, 0x7a2018);
    barrel(b, 29.3f, 4.55f, 0x2a4a6a);
    // tyres
    for (int i = 0; i < 4; i++) {
        b.m(M_RUBBER, 0xffffff, 1.0f);
        b.cyl(Vec3(38.9f + (i & 1) * 0.06f, i * 0.24f, 5.0f), Vec3(38.9f + (i & 1) * 0.06f, i * 0.24f + 0.23f, 5.0f), 0.4f, 12);
    }
    b.collider(Vec3(38.5f, 0, 4.6f), Vec3(39.3f, 0.96f, 5.4f), CF_PROP);
    hideLocker(b, 2, Vec3(xp + 0.6f, 0, z1 - 0.52f), PI, 0x4a4e48);
    // hanging chains and the swinging lamp
    b.named(DY_CHAIN2, chainMesh(b, 2.2f, true), Mat4::translate(Vec3(36.0f, top - 0.9f, 0.5f)));
    b.named(DY_CHAIN3, chainMesh(b, 1.8f, false), Mat4::translate(Vec3(31.2f, top - 0.9f, 4.0f)));
    b.beginDyn();
    b.m(M_PLAIN, 0x0c0c0c, 2.0f);
    b.beam(Vec3(0, 0, 0), Vec3(0, -1.7f, 0), 0.012f);
    b.m(M_PAINTSTEEL, 0x2f4a36, 2.0f);
    b.cyl(Vec3(0, -1.92f, 0), Vec3(0, -1.7f, 0), 0.24f, 10, false, 0.04f);
    b.m(M_PLAIN, 0xffe0a0, 2.0f).emissive(EG_INDOOR, 1.0f);
    b.boxC(Vec3(0, -1.88f, 0), Vec3(0.04f, 0.05f, 0.04f));
    b.noEmis();
    b.named(DY_SWING_LAMP, b.endDyn(), Mat4::translate(Vec3(33.5f, top - 0.2f, -5.5f)));
    w.lightSwing = b.spot(Vec3(33.5f, top - 2.2f, -5.5f), Vec3(0, -1, 0), 62.0f, 0xffd898, 2.2f, 11.0f, LG_INDOOR, FL_BUZZ, 0.5f);
    w.lights[(size_t)w.lightSwing].on = false;
    b.m(M_RUST, 0x808080, 2.0f);
    for (int k = 0; k < 2; k++) {
        float fx = k ? 36.8f : 29.5f;
        b.beam(Vec3(fx - 0.4f, 3.7f, -8.0f + k * 9.0f), Vec3(fx - 0.4f, top, -8.0f + k * 9.0f), 0.012f);
        b.beam(Vec3(fx + 0.4f, 3.7f, -8.0f + k * 9.0f), Vec3(fx + 0.4f, top, -8.0f + k * 9.0f), 0.012f);
        fluorescent(b, Vec3(fx, 3.7f, -8.0f + k * 9.0f), true, LG_INDOOR, k ? FL_DYING : FL_BUZZ, 1.2f);
    }
    b.sign("c_never", Vec3(x1 - 0.125f, 3.1f, -6.0f), Vec3(-1, 0, 0), 3.4f);
    b.sign("danger_load", Vec3(xp + 0.105f, 2.0f, -5.0f), Vec3(1, 0, 0), 1.2f);
    b.sign("exit", Vec3(36.55f, 2.4f, z0 + 0.125f), Vec3(0, 0, 1), 0.45f);
    b.m(M_HAZARD, 0xffffff, 0.8f);
    b.box(Vec3(30.0f, 0.0f, z1 - 0.9f), Vec3(35.0f, 0.014f, z1 - 0.13f), F_PY);
    b.zone(Z_SHOP, Vec3(xp, -0.1f, z0), Vec3(x1, top, z1));
    b.trigger(TR_WORKSHOP, Vec3(xp + 0.3f, 0, z0 + 0.3f), Vec3(x1 - 0.3f, 3, z1 - 0.3f));

    // outside the building
    b.m(M_PAINTSTEEL, 0x2f4a36, 0.6f).g(0.6f);
    b.box(Vec3(19.0f, 0.12f, 7.4f), Vec3(21.6f, 1.35f, 8.8f));
    b.g(1.0f);
    b.m(M_PAINTSTEEL, 0x263c2c, 0.6f);
    b.box(Vec3(18.95f, 1.35f, 7.35f), Vec3(21.65f, 1.42f, 8.85f));
    b.collider(Vec3(19.0f, 0, 7.4f), Vec3(21.6f, 1.42f, 8.8f), CF_PROP, SURF_METAL);
    // portable toilet
    b.m(M_PLAIN, 0x2a5a6a, 0.8f).g(0.6f);
    b.solid(Vec3(12.6f, 0, -9.4f), Vec3(13.8f, 2.3f, -8.2f), SURF_METAL, CF_PROP);
    b.g(1.0f);
    b.m(M_PLAIN, 0xb8b8b0, 1.0f);
    b.box(Vec3(12.55f, 2.3f, -9.45f), Vec3(13.85f, 2.4f, -8.15f));
    b.m(M_PLAIN, 0x1c3a44, 1.0f);
    b.box(Vec3(12.585f, 0.1f, -9.25f), Vec3(12.6f, 2.1f, -8.35f), F_NX);
    pallet(b, 14.2f, 0, 1.5f, 0.2f);
    pallet(b, 14.2f, 0.13f, 1.5f, 0.1f);
    pallet(b, 14.1f, 0.26f, 1.6f, 0.3f);
    b.collider(Vec3(13.5f, 0, 0.9f), Vec3(14.9f, 0.4f, 2.2f), CF_LOW, SURF_WOOD);
    lampPost(b, 13.0f, 12.0f, PI * 0.5f, FL_DYING);
    lampPost(b, 42.5f, -4.0f, -PI * 0.5f, FL_NONE);
}

// ---------------------------------------------------------------------------
// Generator shed, diesel tank, transformer cage, pump house
// ---------------------------------------------------------------------------
static int valveWheel(Builder& b) {
    b.beginDyn();
    b.m(M_PAINTSTEEL, 0x8a1c14, 2.0f);
    const float R = 0.2f;
    for (int i = 0; i < 10; i++) {
        float a0 = i / 10.0f * TAU, a1 = (i + 1) / 10.0f * TAU;
        b.beam(Vec3(std::cos(a0) * R, std::sin(a0) * R, 0), Vec3(std::cos(a1) * R, std::sin(a1) * R, 0), 0.035f);
    }
    for (int i = 0; i < 4; i++) {
        float a = i * PI * 0.5f + 0.3f;
        b.beam(Vec3(0, 0, 0), Vec3(std::cos(a) * R, std::sin(a) * R, 0), 0.025f);
    }
    b.m(M_RUST, 0x808080, 2.0f);
    b.cyl(Vec3(0, 0, -0.14f), Vec3(0, 0, 0.03f), 0.035f, 6);
    return b.endDyn();
}

void build_utility(Builder& b) {
    World& w = b.w;
    // ---- generator shed -----------------------------------------------------
    {
        const float x0 = 30.0f, x1 = 38.0f, z0 = 20.0f, z1 = 28.0f, top = 3.4f;
        WallStyle st = {M_BRICK, 0xb0a8a0, 0.4f, M_CONCWALL, 0x9a9a94, 0.3f, 0.24f, 0.55f};
        Opening oW[1] = {{23.4f, 24.5f, 0.0f, 2.12f, false}};
        wallRun(b, 1, x0, z0, z1, 0, top, +1, st, oW, 1);
        wallRun(b, 1, x1, z0, z1, 0, top, -1, st);
        wallRun(b, 0, z0, x0, x1, 0, top, +1, st);
        wallRun(b, 0, z1, x0, x1, 0, top, -1, st);
        b.m(M_ROOF, 0xffffff, 0.3f);
        b.solid(Vec3(x0 - 0.25f, top, z0 - 0.25f), Vec3(x1 + 0.25f, top + 0.2f, z1 + 0.25f), SURF_METAL);
        b.m(M_CONCWALL, 0x707070, 0.3f);
        b.box(Vec3(x0, top - 0.012f, z0), Vec3(x1, top, z1), F_NY);
        b.m(M_CONCRETE, 0x7a7a74, 0.25f);
        b.box(Vec3(x0 + 0.12f, 0, z0 + 0.12f), Vec3(x1 - 0.12f, 0.012f, z1 - 0.12f), F_PY);
        b.collider(Vec3(x0 + 0.12f, -0.2f, z0 + 0.12f), Vec3(x1 - 0.12f, 0.012f, z1 - 0.12f), COL_SOLID, SURF_CONCRETE);
        makeDoor(b, D_GEN, Vec3(x0, 0, 23.4f), 1.1f, 2.1f, -PI * 0.5f, -1.5f, M_RUST, 0xa0a0a0, -1, true);
        b.sign("gen", Vec3(x0 - 0.125f, 1.95f, 25.5f), Vec3(-1, 0, 0), 1.2f);
        b.sign("nosmoke", Vec3(x0 - 0.125f, 1.95f, 22.5f), Vec3(-1, 0, 0), 0.5f);
        cagedLamp(b, Vec3(x0 - 0.12f, 2.6f, 23.95f), Vec3(-1, 0, 0), LG_YARD, FL_NONE, 0xffc080, 1.1f, 7.0f);
        // the generator
        b.m(M_RUST, 0x707070, 1.0f);
        b.box(Vec3(32.4f, 0, 22.8f), Vec3(36.6f, 0.25f, 25.2f));
        b.m(M_MACHINE, 0xa88a1c, 1.0f).g(0.75f);
        b.box(Vec3(32.8f, 0.25f, 23.2f), Vec3(35.0f, 1.5f, 24.8f));
        b.g(1.0f);
        b.m(M_MACHINE, 0x4a4e48, 1.0f);
        b.box(Vec3(33.2f, 1.5f, 23.4f), Vec3(34.6f, 1.8f, 24.6f)); // fuel tank
        b.m(M_RAIL, 0xb0b0b0, 2.0f);
        b.cyl(Vec3(33.5f, 1.8f, 24.3f), Vec3(33.5f, 1.87f, 24.3f), 0.07f, 8);
        b.m(M_PAINTSTEEL, 0x5a5e58, 1.0f);
        b.cyl(Vec3(35.0f, 0.95f, 24.0f), Vec3(36.4f, 0.95f, 24.0f), 0.56f, 12);
        b.m(M_GRATE, 0x909090, 2.0f);
        b.box(Vec3(32.44f, 0.3f, 23.15f), Vec3(32.45f, 1.55f, 24.85f), F_NX);
        b.m(M_MACHINE, 0x30302c, 1.0f);
        b.box(Vec3(32.45f, 0.25f, 23.1f), Vec3(32.8f, 1.6f, 24.9f));
        // exhaust
        b.m(M_RUST, 0x808080, 1.0f);
        b.cyl(Vec3(34.6f, 1.5f, 23.5f), Vec3(34.6f, 2.5f, 23.5f), 0.08f, 8);
        b.cyl(Vec3(34.6f, 2.5f, 23.5f), Vec3(34.6f, 3.0f, 23.5f), 0.16f, 8);
        b.cyl(Vec3(34.6f, 3.0f, 23.5f), Vec3(34.6f, 4.4f, 23.5f), 0.08f, 8);
        // control panel at the east end
        b.m(M_MACHINE, 0x4a5a50, 1.5f);
        b.box(Vec3(36.45f, 0.6f, 23.5f), Vec3(36.72f, 1.65f, 24.5f));
        b.sign("gauge2", Vec3(36.72f, 1.4f, 23.8f), Vec3(1, 0, 0), 0.26f);
        b.sign("gauge", Vec3(36.72f, 1.4f, 24.2f), Vec3(1, 0, 0), 0.26f);
        b.beginDyn();
        b.m(M_RAIL, 0xb0b0b0, 2.0f);
        b.beam(Vec3(0, 0, 0), Vec3(0.26f, 0, 0), 0.03f);
        b.m(M_PLAIN, 0x101010, 2.0f);
        b.cyl(Vec3(0.26f, 0, -0.05f), Vec3(0.26f, 0, 0.05f), 0.026f, 8);
        b.named(DY_GEN_LEVER, b.endDyn(), Mat4::translate(Vec3(36.74f, 0.95f, 24.0f)));
        b.collider(Vec3(32.4f, 0, 22.8f), Vec3(36.72f, 1.8f, 25.2f), CF_PROP, SURF_METAL);
        b.interact(IK_USE, U_GEN_START, Vec3(36.6f, 0.7f, 23.6f), Vec3(37.1f, 1.3f, 24.4f), 2.3f);
        b.interact(IK_USE, U_GEN_FUEL, Vec3(33.1f, 1.45f, 23.9f), Vec3(33.9f, 2.0f, 25.35f), 2.6f);
        // fuse box on the north wall
        b.m(M_MACHINE, 0x3a3e3a, 1.5f);
        b.box(Vec3(31.2f, 1.1f, z0 + 0.12f), Vec3(32.2f, 1.9f, z0 + 0.24f));
        b.box(Vec3(31.16f, 1.1f, z0 + 0.24f), Vec3(31.2f, 1.9f, z0 + 0.75f)); // open door
        b.sign("fusebox", Vec3(31.7f, 1.5f, z0 + 0.24f), Vec3(0, 0, 1), 0.92f, 0.7f);
        b.sign("fuses", Vec3(31.7f, 2.05f, z0 + 0.125f), Vec3(0, 0, 1), 0.5f);
        for (int i = 0; i < 3; i++) {
            b.beginDyn();
            b.m(M_PLAIN, 0xd8d2c0, 2.0f);
            b.cyl(Vec3(0, -0.1f, 0), Vec3(0, 0.1f, 0), 0.03f, 8);
            b.m(M_PLAIN, 0xb08a30, 2.0f);
            b.cyl(Vec3(0, 0.1f, 0), Vec3(0, 0.15f, 0), 0.034f, 8);
            b.cyl(Vec3(0, -0.15f, 0), Vec3(0, -0.1f, 0), 0.034f, 8);
            b.named(DY_FUSE_A + i, b.endDyn(), Mat4::translate(Vec3(31.7f + (i - 1) * 0.276f, 1.486f, z0 + 0.285f)), false);
        }
        b.interact(IK_USE, U_FUSEBOX, Vec3(31.15f, 1.05f, z0 + 0.12f), Vec3(32.25f, 1.95f, z0 + 0.5f), 2.3f);
        notePaper(b, N_GEN_MANUAL, Vec3(x0 + 0.125f, 1.5f, 25.3f), Vec3(1, 0, 0), 0.22f, 0.3f, true);
        // cables across the floor
        b.m(M_RUBBER, 0x404040, 2.0f);
        b.beam(Vec3(36.0f, 0.03f, 23.0f), Vec3(33.0f, 0.03f, 21.0f), 0.05f);
        b.beam(Vec3(33.0f, 0.03f, 21.0f), Vec3(31.7f, 0.03f, 20.3f), 0.05f);
        b.beam(Vec3(31.7f, 0.03f, 20.3f), Vec3(31.7f, 1.1f, 20.2f), 0.05f);
        // shelf, oil cans, extinguisher
        wallShelf(b, Vec3(34.5f, 0, z1 - 0.6f), Vec3(36.6f, 1.6f, z1 - 0.13f), 4);
        b.m(M_PAINTSTEEL, 0x8a8a80, 2.0f);
        b.cyl(Vec3(34.9f, 0.59f, z1 - 0.35f), Vec3(34.9f, 0.85f, z1 - 0.35f), 0.09f, 8);
        b.cyl(Vec3(35.3f, 0.59f, z1 - 0.38f), Vec3(35.3f, 0.8f, z1 - 0.38f), 0.08f, 8);
        itemBattery(b, IT_BATT4, Vec3(36.1f, 1.08f, z1 - 0.36f), 0.7f);
        b.m(M_PAINTSTEEL, 0xa01810, 2.0f);
        b.cyl(Vec3(x0 + 0.3f, 0.5f, 22.9f), Vec3(x0 + 0.3f, 1.05f, 22.9f), 0.08f, 8);
        hideLocker(b, 3, Vec3(x1 - 0.55f, 0, z1 - 0.52f), PI, 0x4a4e48);
        w.lightGenRoom = (int)w.lights.size();
        cagedLamp(b, Vec3(34.2f, top - 0.02f, 26.4f), Vec3(0, -1, 0), LG_INDOOR, FL_BUZZ, 0xffd0a0, 1.6f, 9.0f);
        b.floorDecal("oil2", Vec3(34.2f, 0.012f, 25.9f), 2.2f, 1.6f, 0.3f);
        b.floorDecal("scorch", Vec3(36.9f, 0.012f, 22.2f), 1.4f, 1.4f, 0.0f);
        b.zone(Z_GEN, Vec3(x0, -0.1f, z0), Vec3(x1, top, z1));
        b.trigger(TR_GEN, Vec3(x0 + 0.3f, 0, z0 + 0.3f), Vec3(x1 - 0.3f, 3, z1 - 0.3f));
    }
    // ---- diesel tank --------------------------------------------------------
    {
        b.m(M_CONCRETE, 0x9a9a94, 0.5f);
        b.box(Vec3(30.6f, 0, 15.6f), Vec3(37.4f, 0.14f, 18.4f));
        b.collider(Vec3(30.6f, 0, 15.6f), Vec3(37.4f, 0.14f, 18.4f), COL_SOLID | COL_SIGHT, SURF_CONCRETE);
        b.m(M_RUST, 0x808080, 1.0f);
        b.box(Vec3(32.0f, 0.14f, 16.2f), Vec3(32.5f, 0.75f, 17.8f));
        b.box(Vec3(35.5f, 0.14f, 16.2f), Vec3(36.0f, 0.75f, 17.8f));
        b.m(M_PAINTSTEEL, 0x8a8f80, 0.5f);
        b.cyl(Vec3(31.2f, 1.6f, 17.0f), Vec3(36.8f, 1.6f, 17.0f), 0.95f, 14, true);
        b.cyl(Vec3(31.0f, 1.6f, 17.0f), Vec3(31.2f, 1.6f, 17.0f), 0.7f, 14, true, 0.95f);
        b.cyl(Vec3(36.8f, 1.6f, 17.0f), Vec3(37.0f, 1.6f, 17.0f), 0.95f, 14, true, 0.7f);
        b.m(M_RAIL, 0x909090, 2.0f);
        b.cyl(Vec3(34.0f, 2.55f, 17.0f), Vec3(34.0f, 2.75f, 17.0f), 0.15f, 8);
        b.collider(Vec3(31.0f, 0.14f, 16.05f), Vec3(37.0f, 2.6f, 17.95f), CF_SOLID, SURF_METAL);
        b.m(M_PLAIN, 0x8a1810, 1.0f);
        b.box(Vec3(33.0f, 1.3f, 16.03f), Vec3(35.0f, 2.0f, 16.05f));
        b.sign("diesel", Vec3(34.0f, 1.65f, 16.03f), Vec3(0, 0, -1), 1.9f);
        b.box(Vec3(33.0f, 1.3f, 17.95f), Vec3(35.0f, 2.0f, 17.97f));
        b.sign("diesel", Vec3(34.0f, 1.65f, 17.97f), Vec3(0, 0, 1), 1.9f);
        // hand pump at the west end
        b.m(M_PAINTSTEEL, 0x8a1c14, 1.5f);
        b.box(Vec3(30.2f, 0.14f, 16.8f), Vec3(30.5f, 1.25f, 17.2f));
        b.m(M_RAIL, 0xa0a0a0, 2.0f);
        b.beam(Vec3(30.35f, 1.25f, 17.0f), Vec3(30.1f, 1.5f, 17.0f), 0.03f);
        b.m(M_RUBBER, 0x505050, 2.0f);
        b.beam(Vec3(30.5f, 0.6f, 17.0f), Vec3(31.1f, 0.9f, 17.0f), 0.04f);
        b.beam(Vec3(30.2f, 1.0f, 16.9f), Vec3(30.05f, 0.5f, 16.7f), 0.03f);
        b.collider(Vec3(30.2f, 0, 16.8f), Vec3(30.5f, 1.3f, 17.2f), CF_PROP, SURF_METAL);
        b.interact(IK_USE, U_FUEL_TAP, Vec3(29.9f, 0.3f, 16.6f), Vec3(30.6f, 1.6f, 17.4f), 2.4f);
        b.floorDecal("oil2", Vec3(29.6f, 0, 17.2f), 2.6f, 2.0f, 0.8f);
        b.sign("nosmoke", Vec3(30.19f, 0.9f, 17.0f), Vec3(-1, 0, 0), 0.3f);
    }
    // ---- transformer cage ---------------------------------------------------
    {
        b.m(M_GRAVEL, 0xb0b0b0, 0.4f);
        b.box(Vec3(42.0f, 0, 20.0f), Vec3(48.0f, 0.014f, 26.0f), F_PY);
        for (int k = 0; k < 2; k++) {
            float tx = 43.4f + k * 2.6f;
            b.m(M_MACHINE, 0x55625a, 0.8f).g(0.7f);
            b.box(Vec3(tx, 0.15f, 22.2f), Vec3(tx + 1.5f, 1.9f, 23.8f));
            b.g(1.0f);
            b.m(M_PAINTSTEEL, 0x4a564e, 1.0f);
            for (int f = 0; f < 7; f++) {
                b.box(Vec3(tx + 0.1f + f * 0.2f, 0.3f, 21.95f), Vec3(tx + 0.14f + f * 0.2f, 1.7f, 22.2f));
                b.box(Vec3(tx + 0.1f + f * 0.2f, 0.3f, 23.8f), Vec3(tx + 0.14f + f * 0.2f, 1.7f, 24.05f));
            }
            for (int i = 0; i < 3; i++) {
                float ix = tx + 0.3f + i * 0.45f;
                b.m(M_PLAIN, 0x6a3a22, 2.0f);
                for (int s = 0; s < 4; s++) b.cyl(Vec3(ix, 1.9f + s * 0.1f, 23.0f), Vec3(ix, 1.97f + s * 0.1f, 23.0f), 0.07f - s * 0.008f, 8);
                b.m(M_RAIL, 0x909090, 2.0f);
                b.beam(Vec3(ix, 2.3f, 23.0f), Vec3(45.0f, 3.6f, 19.6f), 0.015f);
            }
            b.collider(Vec3(tx, 0, 21.95f), Vec3(tx + 1.5f, 2.3f, 24.05f), CF_PROP, SURF_METAL);
        }
        b.m(M_WOOD, 0x5a5048, 1.0f);
        b.cyl(Vec3(45.0f, 0, 19.6f), Vec3(45.0f, 6.5f, 19.6f), 0.13f, 6);
        b.box(Vec3(44.2f, 3.5f, 19.55f), Vec3(45.8f, 3.62f, 19.65f));
        // fence around it
        struct Seg { float x0, z0, x1, z1; } segs[4] = {{42, 20, 48, 20}, {48, 20, 48, 26}, {48, 26, 42, 26}, {42, 26, 42, 20}};
        for (auto& s : segs) {
            Vec3 a(s.x0, 0, s.z0), c(s.x1, 0, s.z1);
            b.m(M_PAINTSTEEL, 0x6a6e68, 1.0f);
            b.cyl(a, a + Vec3(0, 2.5f, 0), 0.045f, 6);
            b.cyl((a + c) * 0.5f, (a + c) * 0.5f + Vec3(0, 2.5f, 0), 0.04f, 6);
            b.beam(a + Vec3(0, 2.45f, 0), c + Vec3(0, 2.45f, 0), 0.04f);
            b.m(M_CHAINLINK, 0xffffff, 1.0f);
            b.quad(a + Vec3(0, 0.05f, 0), c + Vec3(0, 0.05f, 0), c + Vec3(0, 2.45f, 0), a + Vec3(0, 2.45f, 0), Vec2(0, 3.4f), Vec2(8.4f, 3.4f), Vec2(8.4f, 0), Vec2(0, 0));
            b.collider(vmin(a, c) - Vec3(0.07f, 0, 0.07f), vmax(a, c) + Vec3(0.07f, 2.5f, 0.07f), CF_FENCE, SURF_METAL);
        }
        b.sign("hv", Vec3(42.0f - 0.03f, 1.6f, 23.0f), Vec3(-1, 0, 0), 1.0f);
        b.sign("hv", Vec3(45.0f, 1.6f, 26.0f + 0.03f), Vec3(0, 0, 1), 1.0f);
        b.sign("hv", Vec3(45.0f, 1.6f, 20.0f - 0.03f), Vec3(0, 0, -1), 1.0f);
        lampPost(b, 40.5f, 31.0f, PI, FL_BUZZ);
    }
    // ---- pump house ---------------------------------------------------------
    {
        const float x0 = 44.0f, x1 = 52.0f, z0 = -40.0f, z1 = -32.0f, top = 3.3f;
        WallStyle st = {M_BRICK, 0x9a9490, 0.4f, M_CONCWALL, 0x8a9088, 0.3f, 0.25f, 0.5f};
        Opening oW[1] = {{-36.6f, -35.5f, 0.0f, 2.12f, false}};
        wallRun(b, 1, x0, z0, z1, 0, top, +1, st, oW, 1);
        wallRun(b, 1, x1, z0, z1, 0, top, -1, st);
        wallRun(b, 0, z0, x0, x1, 0, top, +1, st);
        wallRun(b, 0, z1, x0, x1, 0, top, -1, st);
        b.m(M_ROOF, 0xffffff, 0.3f);
        b.solid(Vec3(x0 - 0.25f, top, z0 - 0.25f), Vec3(x1 + 0.25f, top + 0.2f, z1 + 0.25f), SURF_METAL);
        b.m(M_CONCWALL, 0x606860, 0.3f);
        b.box(Vec3(x0, top - 0.012f, z0), Vec3(x1, top, z1), F_NY);
        b.m(M_CONCRETE, 0x6a706a, 0.25f);
        b.box(Vec3(x0 + 0.12f, 0, z0 + 0.12f), Vec3(x1 - 0.12f, 0.012f, z1 - 0.12f), F_PY);
        b.collider(Vec3(x0 + 0.12f, -0.2f, z0 + 0.12f), Vec3(x1 - 0.12f, 0.012f, z1 - 0.12f), COL_SOLID, SURF_CONCRETE);
        makeDoor(b, D_PUMP, Vec3(x0, 0, -36.6f), 1.1f, 2.1f, -PI * 0.5f, -1.5f, M_RUST, 0x90a090, -1, true);
        b.sign("pump", Vec3(x0 - 0.13f, 2.45f, -36.05f), Vec3(-1, 0, 0), 1.1f);
        cagedLamp(b, Vec3(x0 - 0.125f, 2.85f, -34.9f), Vec3(-1, 0, 0), LG_YARD, FL_DYING, 0xffc080, 1.1f, 7.0f);
        // manifold on the north wall
        float mz = z0 + 0.55f;
        b.m(M_PAINTSTEEL, 0x3a5a4a, 0.8f);
        b.cyl(Vec3(44.4f, 1.1f, mz), Vec3(51.6f, 1.1f, mz), 0.2f, 10);
        b.m(M_RUST, 0x909090, 1.0f);
        for (float fx = 44.9f; fx < 51.5f; fx += 1.0f) b.cyl(Vec3(fx, 1.1f, mz), Vec3(fx + 0.06f, 1.1f, mz), 0.26f, 10);
        static const float vx[3] = {48.0f, 46.0f, 50.0f}; // A, B, C
        static const char* vtag[3] = {"tagA", "tagB", "tagC"};
        int wheel = valveWheel(b);
        for (int i = 0; i < 3; i++) {
            b.m(M_PAINTSTEEL, 0x3a5a4a, 0.8f);
            b.cyl(Vec3(vx[i], 0, mz), Vec3(vx[i], 2.4f, mz), 0.12f, 8);
            b.cyl(Vec3(vx[i], 2.4f, mz), Vec3(vx[i], 2.4f, z0 + 0.12f), 0.12f, 8);
            b.m(M_MACHINE, 0x5a2a20, 2.0f);
            b.box(Vec3(vx[i] - 0.2f, 1.45f, mz - 0.2f), Vec3(vx[i] + 0.2f, 1.85f, mz + 0.2f));
            b.named(DY_VALVE_A + i, wheel, Mat4::translate(Vec3(vx[i], 1.65f, mz + 0.34f)));
            b.sign(vtag[i], Vec3(vx[i], 1.22f, mz + 0.21f), Vec3(0, 0, 1), 0.2f);
            b.interact(IK_USE, U_VALVE_A + i, Vec3(vx[i] - 0.3f, 1.2f, mz), Vec3(vx[i] + 0.3f, 2.0f, mz + 0.5f), 2.4f);
        }
        b.m(M_RAIL, 0x909090, 2.0f);
        b.cyl(Vec3(49.0f, 1.3f, mz), Vec3(49.0f, 1.8f, mz), 0.02f, 5);
        b.cyl(Vec3(49.0f, 1.9f, mz - 0.02f), Vec3(49.0f, 1.9f, mz + 0.04f), 0.15f, 12);
        b.sign("gauge", Vec3(49.0f, 1.9f, mz + 0.04f), Vec3(0, 0, 1), 0.28f);
        b.sign("purge", Vec3(47.0f, 2.75f, z0 + 0.13f), Vec3(0, 0, 1), 1.3f);
        b.collider(Vec3(44.4f, 0, z0 + 0.12f), Vec3(51.6f, 2.5f, mz + 0.22f), CF_PROP, SURF_METAL);
        // pump set + starter on the east wall
        b.m(M_CONCRETE, 0x8a8a84, 0.5f);
        b.box(Vec3(49.8f, 0, -37.0f), Vec3(51.2f, 0.45f, -34.6f));
        b.m(M_PAINTSTEEL, 0x2a4a6a, 0.8f);
        b.cyl(Vec3(50.5f, 0.9f, -36.8f), Vec3(50.5f, 0.9f, -35.7f), 0.42f, 12);
        b.m(M_MACHINE, 0x4a4e48, 1.0f);
        b.box(Vec3(50.1f, 0.45f, -35.7f), Vec3(50.9f, 1.3f, -34.8f));
        b.m(M_PAINTSTEEL, 0x3a5a4a, 0.8f);
        b.cyl(Vec3(50.5f, 0.9f, -36.8f), Vec3(50.5f, 0.9f, mz), 0.14f, 8);
        b.collider(Vec3(49.8f, 0, -37.0f), Vec3(51.2f, 1.35f, -34.6f), CF_PROP, SURF_METAL);
        b.m(M_MACHINE, 0x2f4a36, 1.5f);
        b.box(Vec3(x1 - 0.4f, 1.0f, -34.3f), Vec3(x1 - 0.125f, 1.95f, -33.4f));
        b.sign("feeder", Vec3(x1 - 0.13f, 2.2f, -33.85f), Vec3(-1, 0, 0), 0.75f);
        b.sign("gauge2", Vec3(x1 - 0.4f, 1.72f, -34.05f), Vec3(-1, 0, 0), 0.22f);
        b.beginDyn();
        b.m(M_RAIL, 0xb0b0b0, 2.0f);
        b.beam(Vec3(0, 0, 0), Vec3(-0.06f, 0.3f, 0), 0.035f);
        b.m(M_PLAIN, 0x101010, 2.0f);
        b.cyl(Vec3(-0.06f, 0.3f, -0.06f), Vec3(-0.06f, 0.3f, 0.06f), 0.028f, 8);
        b.named(DY_BREAKER, b.endDyn(), Mat4::translate(Vec3(x1 - 0.42f, 1.25f, -33.7f)));
        b.beginDyn();
        b.m(M_RAIL, 0xb09a40, 2.0f);
        b.box(Vec3(-0.035f, -0.06f, -0.03f), Vec3(0.0f, 0.02f, 0.03f));
        b.beam(Vec3(-0.02f, 0.02f, -0.02f), Vec3(-0.02f, 0.09f, -0.02f), 0.01f);
        b.beam(Vec3(-0.02f, 0.02f, 0.02f), Vec3(-0.02f, 0.09f, 0.02f), 0.01f);
        b.beam(Vec3(-0.02f, 0.09f, -0.02f), Vec3(-0.02f, 0.09f, 0.02f), 0.01f);
        b.m(M_PLAIN, 0xa01410, 2.0f);
        b.box(Vec3(-0.02f, -0.36f, 0.04f), Vec3(-0.015f, -0.06f, 0.26f));
        b.sign("loto", Vec3(-0.02f, -0.21f, 0.15f), Vec3(-1, 0, 0), 0.2f, 0.28f);
        b.named(DY_LOTO, b.endDyn(), Mat4::translate(Vec3(x1 - 0.46f, 1.5f, -33.7f)));
        b.interact(IK_USE, U_LOTO, Vec3(x1 - 0.7f, 1.0f, -34.0f), Vec3(x1 - 0.3f, 1.75f, -33.35f), 2.4f);
        b.interact(IK_USE, U_BREAKER, Vec3(x1 - 0.7f, 1.0f, -34.3f), Vec3(x1 - 0.3f, 1.95f, -33.35f), 2.4f);
        w.interacts[(size_t)w.useInteract[U_BREAKER]].enabled = false;
        // shelf with fittings
        wallShelf(b, Vec3(46.5f, 0.6f, z1 - 0.55f), Vec3(49.5f, 2.0f, z1 - 0.13f), 3);
        b.m(M_RUST, 0xa0a0a0, 2.0f);
        for (int i = 0; i < 6; i++) b.cyl(Vec3(46.8f + i * 0.4f, 0.72f + 0.64f * (i % 2), z1 - 0.35f), Vec3(46.8f + i * 0.4f, 0.9f + 0.64f * (i % 2), z1 - 0.35f), 0.07f, 6);
        itemBattery(b, IT_BATT5, Vec3(49.1f, 1.37f, z1 - 0.33f), 0.2f);
        hideLocker(b, 5, Vec3(x0 + 0.55f, 0, z1 - 0.6f), PI * 0.5f, 0x3a4a44);
        w.lightPump = (int)w.lights.size();
        cagedLamp(b, Vec3(48.0f, top - 0.02f, -36.0f), Vec3(0, -1, 0), LG_INDOOR, FL_DYING, 0xd0ffd8, 1.5f, 9.0f);
        // flood water (drains when the purge is done)
        b.beginDyn();
        b.m(M_GLASS, 0x1c2a22, 0.3f);
        b.box(Vec3(x0 + 0.12f, 0, z0 + 0.12f), Vec3(x1 - 0.12f, 0.001f, z1 - 0.12f), F_PY);
        b.named(DY_PUMPWATER, b.endDyn(), Mat4::translate(Vec3(0, 0.34f, 0)));
        b.sign("grime", Vec3(48.0f, 0.45f, z1 - 0.13f), Vec3(0, 0, -1), 7.6f, 0.9f);
        b.sign("grime", Vec3(x1 - 0.13f, 0.45f, -36.0f), Vec3(-1, 0, 0), 7.6f, 0.9f);
        b.sign("c_again", Vec3(x0 + 0.13f, 2.4f, -33.8f), Vec3(1, 0, 0), 1.3f);
        b.zone(Z_PUMP, Vec3(x0, -0.1f, z0), Vec3(x1, top, z1));
        b.trigger(TR_PUMP, Vec3(x0 + 0.3f, 0, z0 + 0.3f), Vec3(x1 - 0.3f, 3, z1 - 0.3f));
        // outfall pipes to the quay edge
        b.m(M_RUST, 0x808080, 0.6f);
        b.cyl(Vec3(46.0f, 0.3f, z0 - 0.12f), Vec3(46.0f, 0.3f, QUAY_Z + 0.2f), 0.28f, 10);
        b.cyl(Vec3(49.0f, 0.3f, z0 - 0.12f), Vec3(49.0f, 0.3f, QUAY_Z + 0.2f), 0.28f, 10);
        b.collider(Vec3(45.7f, 0, QUAY_Z + 0.2f), Vec3(49.3f, 0.6f, z0), CF_LOW, SURF_METAL);
        lampPost(b, 41.0f, -30.0f, -PI * 0.5f, FL_BUZZ);
    }
}
