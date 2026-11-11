// THE 1982 CRANE - level: container yard, quay furniture, the lifts, the barge
#include "level.h"

static const float CL = 12.19f, CW = 2.44f, CH = 2.59f;
static const u32 CONT_COL[7] = {0x7a2e22, 0x27466e, 0x2f5a44, 0x9a5a20, 0x6c6f6a, 0xa8a49a, 0x8a7a22};

static u32 darker(u32 c, float k) {
    Vec3 v = hexc(c) * k;
    return ((u32)(v.x * 255) << 16) | ((u32)(v.y * 255) << 8) | (u32)(v.z * 255);
}

// Solid container shell in the current transform: long axis X, doors at +X.
static void containerShell(Builder& b, float len, float y, u32 tint, bool roofFace) {
    float hl = len * 0.5f, hw = CW * 0.5f;
    b.m(M_CORRUGATED, tint, 0.8f).g(0.62f);
    b.box(Vec3(-hl + 0.02f, y + 0.14f, -hw + 0.02f), Vec3(hl - 0.02f, y + CH - 0.06f, hw - 0.02f), F_PZ | F_NZ | F_NX | (roofFace ? F_PY : 0));
    b.m(M_CONTDOOR, tint, 1.0f).fit().g(0.62f);
    b.box(Vec3(hl - 0.04f, y + 0.14f, -hw + 0.06f), Vec3(hl - 0.02f, y + CH - 0.06f, hw - 0.06f), F_PX);
    b.g(1.0f);
    // frame
    b.m(M_PAINTSTEEL, darker(tint, 0.62f), 1.0f);
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? hl - 0.12f : -hl, z = (i & 2) ? hw - 0.12f : -hw;
        b.box(Vec3(x, y, z), Vec3(x + 0.12f, y + CH, z + 0.12f));
    }
    for (int s = -1; s <= 1; s += 2) {
        float z = s > 0 ? hw - 0.07f : -hw;
        b.box(Vec3(-hl + 0.12f, y, z), Vec3(hl - 0.12f, y + 0.15f, z + 0.07f));
        b.box(Vec3(-hl + 0.12f, y + CH - 0.09f, z), Vec3(hl - 0.12f, y + CH, z + 0.07f));
        float x = s > 0 ? hl - 0.07f : -hl;
        b.box(Vec3(x, y, -hw + 0.12f), Vec3(x + 0.07f, y + 0.15f, hw - 0.12f));
        b.box(Vec3(x, y + CH - 0.09f, -hw + 0.12f), Vec3(x + 0.07f, y + CH, hw - 0.12f));
    }
}

static void containerStack(Builder& b, float cx, float cz, bool alongZ, float len, int height, u32 seed, bool doorsEast) {
    float yaw = alongZ ? PI * 0.5f : 0.0f;
    if (!doorsEast) yaw += PI;
    b.push(Mat4::translate(Vec3(cx, 0, cz)) * Mat4::rotY(yaw));
    for (int h = 0; h < height; h++) {
        u32 tint = CONT_COL[hash32(seed * 31u + (u32)h * 7u) % 7u];
        containerShell(b, len, h * CH, tint, h == height - 1);
        float r = hash2f((int)seed, h);
        if (r < 0.55f) {
            static const char* ids[4] = {"cid1", "cid2", "cid3", "cid4"};
            const char* id = ids[hash32(seed + (u32)h) % 3u];
            b.sign(id, Vec3(len * 0.5f - 2.2f, h * CH + 2.05f, CW * 0.5f - 0.005f), Vec3(0, 0, 1), 2.6f);
            b.sign(id, Vec3(-len * 0.5f + 2.2f, h * CH + 2.05f, -CW * 0.5f + 0.005f), Vec3(0, 0, -1), 2.6f);
        }
        if (r > 0.4f && r < 0.75f && len > 8.0f) {
            b.sign("harrow", Vec3(-1.0f, h * CH + 1.35f, CW * 0.5f - 0.005f), Vec3(0, 0, 1), 3.4f);
            b.sign("harrow", Vec3(1.0f, h * CH + 1.35f, -CW * 0.5f + 0.005f), Vec3(0, 0, -1), 3.4f);
        }
        if (hash2f((int)seed + 9, h) < 0.5f) {
            b.sign("drip", Vec3(hash2f((int)seed, h + 20) * (len - 2.0f) - len * 0.5f + 1.0f, h * CH + 1.5f, CW * 0.5f - 0.004f), Vec3(0, 0, 1), 0.9f, 2.0f);
            b.sign("drip", Vec3(hash2f((int)seed, h + 40) * (len - 2.0f) - len * 0.5f + 1.0f, h * CH + 1.5f, -CW * 0.5f + 0.004f), Vec3(0, 0, -1), 0.9f, 2.0f);
        }
    }
    b.collider(Vec3(-len * 0.5f, 0, -CW * 0.5f), Vec3(len * 0.5f, height * CH, CW * 0.5f), CF_SOLID, SURF_METAL);
    b.pop();
}

// Hollow ground-level container with its doors standing open. Leaves the transform pushed
// (local frame: long axis X, doors at +X, floor at y = 0.16) so the caller can furnish it.
static void openContainerBegin(Builder& b, float cx, float cz, float yaw, u32 tint) {
    float hl = CL * 0.5f, hw = CW * 0.5f, t = 0.05f;
    b.push(Mat4::translate(Vec3(cx, 0, cz)) * Mat4::rotY(yaw));
    // outer skin
    b.m(M_CORRUGATED, tint, 0.8f).g(0.62f);
    b.box(Vec3(-hl + 0.02f, 0.14f, -hw + 0.02f), Vec3(hl - 0.02f, CH - 0.06f, -hw + t), F_NZ);
    b.box(Vec3(-hl + 0.02f, 0.14f, hw - t), Vec3(hl - 0.02f, CH - 0.06f, hw - 0.02f), F_PZ);
    b.box(Vec3(-hl + 0.02f, 0.14f, -hw + 0.02f), Vec3(-hl + t, CH - 0.06f, hw - 0.02f), F_NX);
    b.box(Vec3(-hl + 0.02f, CH - 0.1f, -hw + 0.02f), Vec3(hl - 0.02f, CH - 0.06f, hw - 0.02f), F_PY);
    b.g(1.0f);
    // inner skin (dark, rust streaked)
    b.m(M_CORRUGATED, darker(tint, 0.45f), 0.8f).g(0.7f);
    b.box(Vec3(-hl + t, 0.16f, -hw + t), Vec3(hl - 0.02f, CH - 0.1f, -hw + t + 0.005f), F_PZ);
    b.box(Vec3(-hl + t, 0.16f, hw - t - 0.005f), Vec3(hl - 0.02f, CH - 0.1f, hw - t), F_NZ);
    b.box(Vec3(-hl + t, 0.16f, -hw + t), Vec3(-hl + t + 0.005f, CH - 0.1f, hw - t), F_PX);
    b.box(Vec3(-hl + t, CH - 0.105f, -hw + t), Vec3(hl - 0.02f, CH - 0.1f, hw - t), F_NY);
    b.g(1.0f);
    b.m(M_WOOD, 0x6a5a48, 0.8f);
    b.box(Vec3(-hl + 0.02f, 0.0f, -hw + 0.02f), Vec3(hl - 0.02f, 0.16f, hw - 0.02f), F_PY | F_PX);
    // frame
    b.m(M_PAINTSTEEL, darker(tint, 0.62f), 1.0f);
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? hl - 0.12f : -hl, z = (i & 2) ? hw - 0.12f : -hw;
        b.box(Vec3(x, 0, z), Vec3(x + 0.12f, CH, z + 0.12f));
    }
    b.box(Vec3(hl - 0.1f, CH - 0.12f, -hw + 0.12f), Vec3(hl, CH, hw - 0.12f));
    for (int s = -1; s <= 1; s += 2) {
        float z = s > 0 ? hw - 0.07f : -hw;
        b.box(Vec3(-hl + 0.12f, 0, z), Vec3(hl - 0.12f, 0.15f, z + 0.07f));
        b.box(Vec3(-hl + 0.12f, CH - 0.09f, z), Vec3(hl - 0.12f, CH, z + 0.07f));
    }
    // open door leaves, folded back along the sides
    b.m(M_CONTDOOR, tint, 1.0f).fit(0.5f, 1.0f).g(0.62f);
    b.push(Mat4::translate(Vec3(hl, 0, hw)) * Mat4::rotY(-2.6f));
    b.box(Vec3(0, 0.14f, -0.02f), Vec3(1.16f, CH - 0.06f, 0.02f));
    b.pop();
    b.push(Mat4::translate(Vec3(hl, 0, -hw)) * Mat4::rotY(2.75f));
    b.box(Vec3(0, 0.14f, -0.02f), Vec3(1.16f, CH - 0.06f, 0.02f));
    b.pop();
    b.g(1.0f);
    // colliders
    b.collider(Vec3(-hl, 0, -hw), Vec3(hl, 0.16f, hw), CF_SOLID, SURF_WOOD);
    b.collider(Vec3(-hl, 0, -hw), Vec3(hl, CH, -hw + 0.08f), CF_SOLID, SURF_METAL);
    b.collider(Vec3(-hl, 0, hw - 0.08f), Vec3(hl, CH, hw), CF_SOLID, SURF_METAL);
    b.collider(Vec3(-hl, 0, -hw), Vec3(-hl + 0.08f, CH, hw), CF_SOLID, SURF_METAL);
    b.collider(Vec3(-hl, CH - 0.1f, -hw), Vec3(hl, CH, hw), CF_SOLID, SURF_METAL);
    b.zone(Z_CONTAINER, Vec3(-hl, 0, -hw), Vec3(hl, CH, hw));
}

static void sleepingBag(Builder& b, float x, float z, u32 tint, float yaw) {
    b.push(Mat4::translate(Vec3(x, 0.16f, z)) * Mat4::rotY(yaw));
    b.m(M_TARP, tint, 1.5f);
    b.box(Vec3(-0.95f, 0, -0.3f), Vec3(0.95f, 0.1f, 0.3f));
    b.box(Vec3(-0.95f, 0.1f, -0.26f), Vec3(0.2f, 0.16f, 0.26f));
    b.m(M_TARP, 0xb0a890, 1.5f);
    b.box(Vec3(0.6f, 0.1f, -0.2f), Vec3(0.92f, 0.18f, 0.2f));
    b.pop();
}

void build_containers(Builder& b) {
    World& w = b.w;
    static const float SX[4] = {-67.6f, -54.9f, -42.2f, -29.5f};
    static const float RZ[11] = {-11.0f, -5.5f, 0.0f, 5.5f, 11.0f, 16.5f, 22.0f, 27.5f, 33.0f, 38.5f, 44.0f};
    static const char* MAP[11] = {"2312", "3022", "1203", "2320", "0132", "2021", "3202", "1230", "2012", "0321", "2102"};
    for (int r = 0; r < 11; r++)
        for (int s = 0; s < 4; s++) {
            int h = MAP[r][s] - '0';
            if (h == 0) continue;
            if ((r == 4 && s == 1) || (r == 8 && s == 2)) continue; // the two open containers
            containerStack(b, SX[s], RZ[r], false, CL, h, (u32)(r * 4 + s + 1), ((r + s) & 1) != 0);
        }
    // --- container A: abandoned stores, a fuse on a crate at the back ---
    openContainerBegin(b, SX[1], RZ[4], PI, 0x2f5a44);
    crate(b, Vec3(-5.6f, 0.16f, -0.9f), Vec3(-4.7f, 0.9f, -0.1f));
    crate(b, Vec3(-5.5f, 0.16f, 0.2f), Vec3(-4.5f, 0.7f, 1.05f));
    crate(b, Vec3(-3.9f, 0.16f, -1.05f), Vec3(-3.1f, 1.0f, -0.3f));
    crate(b, Vec3(-1.2f, 0.16f, 0.3f), Vec3(-0.3f, 0.8f, 1.05f));
    b.m(M_TARP, 0x3a4a5a, 1.0f);
    b.box(Vec3(1.5f, 0.16f, -1.05f), Vec3(3.4f, 0.75f, -0.2f));
    b.collider(Vec3(1.5f, 0.16f, -1.05f), Vec3(3.4f, 0.75f, -0.2f), CF_PROP);
    b.m(M_CARDBOARD, 0xffffff, 1.5f);
    b.box(Vec3(-2.6f, 0.16f, -1.0f), Vec3(-2.0f, 0.6f, -0.5f));
    b.box(Vec3(-2.5f, 0.6f, -0.95f), Vec3(-2.05f, 0.95f, -0.55f));
    b.sign("hand", Vec3(-2.0f, 1.45f, 1.165f), Vec3(0, 0, -1), 0.3f, 0.38f);
    b.sign("hand", Vec3(-2.5f, 1.3f, 1.165f), Vec3(0, 0, -1), 0.3f, 0.38f, 0.4f);
    b.sign("hand", Vec3(-3.3f, 1.6f, 1.165f), Vec3(0, 0, -1), 0.3f, 0.38f, -0.3f);
    b.sign("c_dontlook", Vec3(-6.04f, 1.6f, 0.0f), Vec3(1, 0, 0), 2.0f);
    itemFuse(b, IT_FUSE_B, b.cur.point(Vec3(-5.0f, 0.705f, 0.6f)), 0.7f);
    itemBattery(b, IT_BATT6, b.cur.point(Vec3(-3.5f, 1.005f, -0.7f)), 0.3f);
    b.pop();
    // --- container B: where the crew hid ---
    openContainerBegin(b, SX[2], RZ[8], PI, 0x7a2e22);
    sleepingBag(b, -4.6f, -0.82f, 0x3a4a3a, 0.0f);
    sleepingBag(b, -4.4f, 0.8f, 0x4a3a2a, 0.05f);
    sleepingBag(b, -2.0f, -0.84f, 0x2a3a4a, -0.04f);
    sleepingBag(b, -1.6f, 0.82f, 0x3a4a3a, 0.02f);
    crate(b, Vec3(-3.4f, 0.16f, -0.3f), Vec3(-2.8f, 0.62f, 0.3f));
    // lantern on the crate
    b.m(M_PAINTSTEEL, 0x2f4a36, 2.0f);
    b.cyl(Vec3(-3.1f, 0.62f, 0), Vec3(-3.1f, 0.68f, 0), 0.06f, 8);
    b.cyl(Vec3(-3.1f, 0.82f, 0), Vec3(-3.1f, 0.86f, 0), 0.06f, 8);
    b.m(M_PLAIN, 0xffd090, 2.0f).emissive(EG_ALWAYS, 0.8f);
    b.cyl(Vec3(-3.1f, 0.68f, 0), Vec3(-3.1f, 0.82f, 0), 0.045f, 8, false);
    b.noEmis();
    w.lightLantern = b.light(Vec3(-3.1f, 0.95f, 0), 0xffb878, 1.2f, 7.0f, LG_ALWAYS, FL_PULSE, 0.3f);
    // tins, stove, a deck of cards mid-hand
    b.m(M_RAIL, 0xa0a0a0, 2.0f);
    for (int i = 0; i < 7; i++) b.cyl(Vec3(-0.6f + hash2f(i, 3) * 1.4f, 0.16f, -0.9f + hash2f(i, 5) * 0.5f), Vec3(-0.6f + hash2f(i, 3) * 1.4f, 0.27f, -0.9f + hash2f(i, 5) * 0.5f), 0.04f, 6);
    b.m(M_PAINTSTEEL, 0x2a4a6a, 2.0f);
    b.cyl(Vec3(0.2f, 0.16f, 0.7f), Vec3(0.2f, 0.3f, 0.7f), 0.1f, 8);
    b.m(M_PAPER, 0xe0e0d8, 6.0f);
    for (int i = 0; i < 5; i++) {
        b.push(Mat4::translate(Vec3(-2.6f + i * 0.09f, 0.161f + i * 0.0005f, 0.05f)) * Mat4::rotY(i * 0.4f));
        b.box(Vec3(-0.03f, 0, -0.045f), Vec3(0.03f, 0.001f, 0.045f), F_PY);
        b.pop();
    }
    b.sign("tally", Vec3(-3.0f, 1.7f, -1.165f), Vec3(0, 0, 1), 2.4f);
    b.sign("c_relief", Vec3(-3.0f, 1.6f, 1.165f), Vec3(0, 0, -1), 2.6f);
    b.sign("c_dontlook", Vec3(2.5f, 1.8f, -1.165f), Vec3(0, 0, 1), 2.2f);
    notePaper(b, N_CAMP, b.cur.point(Vec3(-3.25f, 0.622f, 0.14f)), Vec3(0, 1, 0), 0.18f, 0.24f);
    itemTape(b, IT_TAPE2, b.cur.point(Vec3(-2.95f, 0.622f, -0.16f)), 0.5f);
    b.trigger(TR_CAMP, Vec3(-6.0f, 0, -1.2f), Vec3(2.0f, CH, 1.2f));
    b.pop();
    // --- the container that knocks ---
    {
        float cx = SX[0], cz = RZ[2];
        b.sign("c_knock", Vec3(cx + 1.0f, 1.55f, cz + CW * 0.5f + 0.004f), Vec3(0, 0, 1), 4.2f);
        b.interact(IK_USE, U_KNOCK, Vec3(cx - 5.5f, 0.6f, cz + CW * 0.5f - 0.05f), Vec3(cx + 5.5f, 2.2f, cz + CW * 0.5f + 0.12f), 2.2f);
        b.trigger(TR_KNOCK, Vec3(cx - 6.0f, 0, cz + CW * 0.5f), Vec3(cx + 6.0f, 3, cz + CW * 0.5f + 3.0f));
        b.floorDecal("puddle", Vec3(cx - 2.0f, 0, cz + 2.0f), 3.0f, 2.2f, 0.4f);
        b.floorDecal("drag", Vec3(cx + 3.0f, 0, cz + 2.4f), 1.2f, 4.0f, PI * 0.5f);
    }
    // loose stacks in the east lane
    containerStack(b, -17.0f, 39.0f, true, 6.06f, 2, 91, true);
    containerStack(b, -13.5f, -6.0f, true, 6.06f, 1, 92, false);
    containerStack(b, -18.5f, 14.0f, true, 6.06f, 1, 93, true);
    // clutter
    Rng r(555);
    for (int i = 0; i < 9; i++) barrel(b, -21.5f + r.range(-0.8f, 0.8f) + (i % 3) * 0.7f, 26.0f + (i / 3) * 0.7f + r.range(-0.1f, 0.1f), CONT_COL[i % 7]);
    pallet(b, -15.0f, 0, 31.0f, 0.3f);
    pallet(b, -15.1f, 0.13f, 31.1f, 0.1f);
    b.collider(Vec3(-15.8f, 0, 30.4f), Vec3(-14.3f, 0.3f, 31.8f), CF_LOW, SURF_WOOD);
    crate(b, Vec3(-74.9f, 0, 20.0f), Vec3(-73.9f, 1.0f, 21.2f));
    crate(b, Vec3(-76.5f, 0, 20.4f), Vec3(-75.3f, 0.8f, 21.4f));
    b.m(M_TARP, 0x3a4a3a, 0.8f);
    b.solid(Vec3(-80.0f, 0, -2.0f), Vec3(-76.5f, 1.5f, 2.5f), SURF_WOOD, CF_PROP);
    b.m(M_TARP, 0x4a4438, 0.8f);
    b.solid(Vec3(-80.5f, 0, 30.0f), Vec3(-77.0f, 1.2f, 33.0f), SURF_WOOD, CF_PROP);
    // jersey barriers closing the west lane's south end
    b.m(M_CONCWALL, 0xb8b8b8, 0.5f).g(0.6f);
    for (int i = 0; i < 3; i++) b.solid(Vec3(-81.5f + i * 2.6f, 0, 50.0f), Vec3(-79.2f + i * 2.6f, 0.85f, 50.6f), SURF_CONCRETE, CF_PROP);
    b.g(1.0f);
    forklift(b, -11.5f, 33.0f, -0.4f);
    // lamps
    lampPost(b, -42.2f, 22.0f, 0.0f, FL_DYING);
    lampPost(b, -67.6f, 38.5f, PI * 0.5f, FL_BUZZ);
    lampPost(b, -12.0f, 20.0f, -PI * 0.5f, FL_NONE);
    lampPost(b, -12.0f, -12.0f, -PI * 0.5f, FL_BUZZ);
    lampPost(b, -78.0f, -14.0f, PI * 0.5f, FL_DYING);
    lampPost(b, -78.0f, 12.0f, PI * 0.5f, FL_NONE, true);
    lampPost(b, -6.0f, 30.0f, PI * 0.5f, FL_NONE);
    lampPost(b, 6.0f, 6.0f, -PI * 0.5f, FL_BUZZ);
    // chalk on container sides inside the maze
    b.sign("c_never", Vec3(SX[3], 1.6f, RZ[6] - CW * 0.5f - 0.004f), Vec3(0, 0, -1), 4.6f);
    b.sign("tally", Vec3(SX[1] + 2.0f, 1.5f, RZ[9] + CW * 0.5f + 0.004f), Vec3(0, 0, 1), 3.0f);
    b.sign("c_putback", Vec3(SX[2], 1.7f, RZ[3] + CW * 0.5f + 0.004f), Vec3(0, 0, 1), 4.0f);
    // wet prints leading into the stacks
    for (int i = 0; i < 12; i++) {
        float t = (float)i;
        b.floorDecal(i & 1 ? "bootL" : "bootR", Vec3(-23.0f - t * 0.75f, 0, 19.3f + ((i & 1) ? 0.16f : -0.16f)), 0.14f, 0.32f, PI * 0.5f);
    }
    b.trigger(TR_MAZE_ENTER, Vec3(-74.0f, 0, -14.0f), Vec3(-24.0f, 4, 46.0f));
    b.trigger(TR_MAZE_DEEP, Vec3(-74.0f, 0, 6.0f), Vec3(-49.0f, 4, 20.0f));
    (void)w;
}

// ---------------------------------------------------------------------------
// Quay, lifts, barge
// ---------------------------------------------------------------------------
static void bollard(Builder& b, float x, float z) {
    b.m(M_PAINTSTEEL, 0x30332f, 1.0f).g(0.6f);
    b.cyl(Vec3(x, 0, z), Vec3(x, 0.55f, z), 0.26f, 10, false);
    b.g(1.0f);
    b.cyl(Vec3(x, 0.55f, z), Vec3(x, 0.68f, z), 0.38f, 10, true);
    b.collider(Vec3(x - 0.3f, 0, z - 0.3f), Vec3(x + 0.3f, 0.68f, z + 0.3f), CF_PROP, SURF_METAL);
}
static void cableReel(Builder& b, float x, float z, float yaw) {
    b.push(Mat4::translate(Vec3(x, 0, z)) * Mat4::rotY(yaw));
    b.m(M_WOOD, 0xb0a080, 1.0f);
    b.cyl(Vec3(0, 0.95f, -0.5f), Vec3(0, 0.95f, -0.42f), 0.95f, 14);
    b.cyl(Vec3(0, 0.95f, 0.42f), Vec3(0, 0.95f, 0.5f), 0.95f, 14);
    b.m(M_RUBBER, 0x606060, 2.0f);
    b.cyl(Vec3(0, 0.95f, -0.42f), Vec3(0, 0.95f, 0.42f), 0.6f, 12, false);
    b.collider(Vec3(-0.95f, 0, -0.5f), Vec3(0.95f, 1.9f, 0.5f), CF_PROP, SURF_WOOD);
    b.pop();
}
static void liftTag(Builder& b, const char* name, float x, float z) {
    b.m(M_WOOD, 0x8a7a66, 2.0f);
    b.box(Vec3(x - 0.03f, 0, z - 0.03f), Vec3(x + 0.03f, 1.25f, z + 0.03f));
    b.box(Vec3(x - 0.27f, 0.95f, z + 0.03f), Vec3(x + 0.27f, 1.25f, z + 0.045f));
    b.sign(name, Vec3(x, 1.1f, z + 0.045f), Vec3(0, 0, 1), 0.5f);
}

void build_quay(Builder& b) {
    World& w = b.w;
    for (float x = -78.0f; x <= 78.0f; x += 12.0f) {
        if (x > 22.0f && x < 32.0f) continue;
        bollard(b, x, QUAY_Z + 1.4f);
    }
    bollard(b, 22.5f, QUAY_Z + 1.4f);
    bollard(b, 31.5f, QUAY_Z + 1.4f);
    // crane rails
    b.m(M_RAIL, 0xffffff, 0.5f);
    for (int k = 0; k < 2; k++) {
        float z = k ? -40.0f : -32.0f;
        for (float x = -74.0f; x < 64.0f; x += 6.0f) b.box(Vec3(x, 0, z - 0.05f), Vec3(x + 6.0f, 0.07f, z + 0.05f), F_PY | F_PZ | F_NZ);
        b.m(M_RUST, 0x707070, 1.0f);
        for (float x = -74.0f; x < 64.0f; x += 1.5f) b.box(Vec3(x, 0, z - 0.14f), Vec3(x + 0.12f, 0.03f, z + 0.14f), F_PY | F_SIDES);
        b.m(M_HAZARD, 0xffffff, 1.0f);
        b.solid(Vec3(-74.6f, 0, z - 0.35f), Vec3(-74.0f, 0.9f, z + 0.35f), SURF_METAL, CF_PROP);
        b.solid(Vec3(64.0f, 0, z - 0.35f), Vec3(64.6f, 0.9f, z + 0.35f), SURF_METAL, CF_PROP);
        b.m(M_RAIL, 0xffffff, 0.5f);
    }
    // landing zone
    b.floorDecal("hatch", Vec3(12.0f, 0, -36.0f), 8.0f, 4.2f);
    b.floorDecal("num7", Vec3(-8.0f, 0, -27.0f), 2.2f, 3.0f);
    // loose stores on the apron
    cableReel(b, 20.0f, -26.5f, 0.3f);
    cableReel(b, 23.0f, -27.2f, 1.2f);
    b.m(M_RUST, 0x8a8a8a, 0.5f);
    for (int i = 0; i < 5; i++) b.box(Vec3(-24.0f + i * 0.05f, i * 0.06f, -27.5f + i * 0.04f), Vec3(-20.0f + i * 0.05f, i * 0.06f + 0.05f, -25.5f + i * 0.04f));
    b.collider(Vec3(-24.0f, 0, -27.5f), Vec3(-19.8f, 0.3f, -25.3f), CF_LOW, SURF_METAL);
    pallet(b, 33.0f, 0, -27.0f, 0.1f);
    b.m(M_TARP, 0xa09880, 1.5f);
    for (int i = 0; i < 5; i++) b.box(Vec3(32.5f + (i % 2) * 0.5f, 0.13f + (i / 2) * 0.2f, -27.4f), Vec3(33.0f + (i % 2) * 0.5f, 0.33f + (i / 2) * 0.2f, -26.6f));
    b.collider(Vec3(32.4f, 0, -27.5f), Vec3(33.6f, 0.75f, -26.5f), CF_PROP);
    forklift(b, 15.0f, -26.0f, -0.6f);
    barrel(b, 36.0f, -26.0f, 0x2a4a6a);
    barrel(b, 36.7f, -26.3f, 0x7a2018);
    // danger boards on stands
    for (int k = 0; k < 2; k++) {
        float sx = k ? 2.5f : -19.0f;
        b.m(M_PAINTSTEEL, 0x5a5e58, 1.0f);
        b.cyl(Vec3(sx, 0, -29.0f), Vec3(sx, 2.3f, -29.0f), 0.04f, 6);
        b.m(M_RUST, 0x707070, 1.0f);
        b.box(Vec3(sx - 0.8f, 1.3f, -29.03f), Vec3(sx + 0.8f, 2.3f, -28.98f));
        b.sign("danger_load", Vec3(sx, 1.8f, -28.98f), Vec3(0, 0, 1), 1.56f);
        b.collider(Vec3(sx - 0.1f, 0, -29.1f), Vec3(sx + 0.1f, 2.3f, -28.9f), COL_SOLID);
    }
    // the crane's supply cable: it runs to the edge and simply goes down into the water
    b.m(M_RUBBER, 0x505050, 1.0f);
    b.beam(Vec3(-3.5f, 0.06f, -40.6f), Vec3(-1.2f, 0.06f, -44.5f), 0.12f);
    b.beam(Vec3(-1.2f, 0.06f, -44.5f), Vec3(-1.0f, 0.42f, -47.5f), 0.12f);
    b.beam(Vec3(-1.0f, 0.42f, -47.5f), Vec3(-1.0f, 0.3f, -48.6f), 0.12f);
    b.beam(Vec3(-1.0f, 0.3f, -48.6f), Vec3(-1.0f, -4.5f, -48.75f), 0.12f);
    // quay ladder and tyre fenders
    b.m(M_RUST, 0x808080, 1.0f);
    b.beam(Vec3(-20.3f, 0.6f, QUAY_Z - 0.62f), Vec3(-20.3f, -4.0f, QUAY_Z - 0.62f), 0.05f);
    b.beam(Vec3(-19.7f, 0.6f, QUAY_Z - 0.62f), Vec3(-19.7f, -4.0f, QUAY_Z - 0.62f), 0.05f);
    for (int i = 0; i < 14; i++) b.beam(Vec3(-20.3f, 0.3f - i * 0.3f, QUAY_Z - 0.62f), Vec3(-19.7f, 0.3f - i * 0.3f, QUAY_Z - 0.62f), 0.03f);
    b.m(M_RUBBER, 0xffffff, 1.0f);
    for (float x = -70.0f; x < 76.0f; x += 9.0f) b.cyl(Vec3(x, -0.7f, QUAY_Z - 0.58f), Vec3(x, -0.7f, QUAY_Z - 0.82f), 0.45f, 10);
    lampPost(b, -44.0f, -24.5f, PI, FL_BUZZ);
    lampPost(b, -18.0f, -24.5f, PI, FL_DYING);
    lampPost(b, 24.0f, -24.5f, PI, FL_NONE);
    lampPost(b, 58.0f, -24.5f, PI, FL_BUZZ);

    // ---- the lifts ----------------------------------------------------------
    // LIFT 1: a sedan, twenty years under water
    {
        b.push(Mat4::translate(Vec3(-30.0f, 0, -43.0f)) * Mat4::rotY(0.12f));
        b.m(M_KELP, 0xffffff, 0.6f).g(0.7f);
        b.box(Vec3(-2.3f, 0.32f, -0.88f), Vec3(2.3f, 0.92f, 0.88f));
        b.box(Vec3(-1.0f, 0.92f, -0.8f), Vec3(1.15f, 1.42f, 0.8f));
        b.box(Vec3(-2.36f, 0.4f, -0.8f), Vec3(-2.3f, 0.8f, 0.8f));
        b.g(1.0f);
        b.m(M_RAIL, 0x808880, 1.0f);
        b.box(Vec3(-2.45f, 0.3f, -0.92f), Vec3(-2.3f, 0.46f, 0.92f));
        b.box(Vec3(2.3f, 0.3f, -0.92f), Vec3(2.45f, 0.46f, 0.92f));
        b.m(M_SCREEN, 0x607068, 1.0f).fit();
        b.box(Vec3(-1.02f, 0.98f, -0.72f), Vec3(-1.0f, 1.36f, 0.72f), F_NX);
        b.box(Vec3(1.15f, 0.98f, -0.72f), Vec3(1.17f, 1.36f, 0.72f), F_PX);
        b.box(Vec3(-0.9f, 0.98f, 0.8f), Vec3(1.05f, 1.36f, 0.82f), F_PZ);
        b.box(Vec3(-0.9f, 0.98f, -0.82f), Vec3(1.05f, 1.36f, -0.8f), F_NZ);
        b.m(M_RUBBER, 0x808080, 1.0f);
        for (int i = 0; i < 4; i++) {
            float x = (i & 1) ? 1.45f : -1.5f, z = (i & 2) ? 0.82f : -0.82f;
            b.cyl(Vec3(x, 0.28f, z - 0.1f), Vec3(x, 0.28f, z + 0.1f), 0.3f, 10);
        }
        b.sign("hand", Vec3(0.2f, 1.18f, 0.825f), Vec3(0, 0, 1), 0.3f, 0.36f);
        b.collider(Vec3(-2.45f, 0, -0.92f), Vec3(2.45f, 1.42f, 0.92f), CF_PROP, SURF_METAL);
        b.interact(IK_USE, U_LIFT1, Vec3(-2.3f, 0.3f, -1.05f), Vec3(2.3f, 1.5f, 1.05f), 2.4f);
        b.pop();
        b.floorDecal("puddle", Vec3(-30.0f, 0, -43.0f), 7.0f, 4.2f, 0.1f);
        b.floorDecal("drag", Vec3(-30.0f, 0, -45.6f), 2.2f, 3.6f, 0.0f);
        liftTag(b, "lift1", -27.0f, -40.6f);
    }
    // LIFT 2: a church bell
    {
        Vec3 c(-41.0f, 0.22f, -43.2f);
        b.m(M_WOOD, 0x6a5a48, 1.0f);
        b.box(Vec3(-42.0f, 0, -43.6f), Vec3(-40.0f, 0.22f, -43.4f));
        b.box(Vec3(-42.0f, 0, -43.0f), Vec3(-40.0f, 0.22f, -42.8f));
        static const float rad[7] = {0.86f, 0.74f, 0.6f, 0.5f, 0.42f, 0.26f, 0.0f};
        static const float ys[7] = {0.0f, 0.12f, 0.5f, 0.95f, 1.25f, 1.45f, 1.5f};
        b.m(M_KELP, 0xb0d0b0, 0.6f).g(0.8f);
        b.lathe(c, rad, ys, 7, 14);
        b.g(1.0f);
        b.m(M_RUST, 0x6a8a70, 1.0f);
        b.box(c + Vec3(-0.12f, 1.5f, -0.2f), c + Vec3(0.12f, 1.75f, 0.2f));
        b.collider(c + Vec3(-0.86f, -0.22f, -0.86f), c + Vec3(0.86f, 1.75f, 0.86f), CF_PROP, SURF_METAL);
        b.interact(IK_USE, U_LIFT2, c + Vec3(-0.95f, 0, -0.95f), c + Vec3(0.95f, 1.8f, 0.95f), 2.4f);
        b.floorDecal("puddle", Vec3(-41.0f, 0, -43.2f), 4.2f, 3.6f, 1.0f);
        b.floorDecal("drag", Vec3(-41.0f, 0, -45.6f), 1.6f, 3.4f, 0.0f);
        liftTag(b, "lift2", -39.0f, -40.6f);
    }
    // LIFT 3: a door, standing in its frame
    {
        float dx = -52.0f, dz = -41.6f;
        b.m(M_WOOD, 0x4a4034, 1.0f);
        b.box(Vec3(dx - 1.0f, 0, dz - 0.5f), Vec3(dx + 1.0f, 0.08f, dz + 0.5f));
        b.m(M_KELP, 0xc0c0b0, 1.0f);
        b.box(Vec3(dx - 0.56f, 0.08f, dz - 0.07f), Vec3(dx - 0.45f, 2.22f, dz + 0.07f));
        b.box(Vec3(dx + 0.45f, 0.08f, dz - 0.07f), Vec3(dx + 0.56f, 2.22f, dz + 0.07f));
        b.box(Vec3(dx - 0.56f, 2.12f, dz - 0.07f), Vec3(dx + 0.56f, 2.24f, dz + 0.07f));
        b.collider(Vec3(dx - 0.56f, 0, dz - 0.07f), Vec3(dx - 0.45f, 2.24f, dz + 0.07f), CF_PROP);
        b.collider(Vec3(dx + 0.45f, 0, dz - 0.07f), Vec3(dx + 0.56f, 2.24f, dz + 0.07f), CF_PROP);
        b.collider(Vec3(dx - 1.0f, 0, dz - 0.5f), Vec3(dx + 1.0f, 0.08f, dz + 0.5f), COL_SOLID, SURF_WOOD);
        makeDoor(b, D_LIFT3, Vec3(dx - 0.45f, 0.08f, dz), 0.9f, 2.04f, 0.0f, 1.25f, M_PANEL, 0x7a8a80, -2, false);
        b.floorDecal("puddle", Vec3(dx, 0, dz), 3.6f, 3.0f, 0.5f);
        b.floorDecal("drag", Vec3(dx, 0, -45.3f), 1.4f, 4.4f, 0.0f);
        liftTag(b, "lift3", dx + 2.2f, -40.6f);
        b.trigger(TR_LIFT3_DOOR, Vec3(dx - 2.5f, 0, dz - 2.5f), Vec3(dx + 2.5f, 3, dz + 2.5f));
        // the bootprints that appear later, walking out of it
        b.beginDyn();
        for (int i = 0; i < 9; i++)
            b.floorDecal(i & 1 ? "bootL" : "bootR", Vec3(((i & 1) ? 0.17f : -0.17f), 0, 0.5f + i * 0.72f), 0.15f, 0.34f, PI);
        b.named(DY_BOOTPRINTS, b.endDyn(), Mat4::translate(Vec3(dx, 0.002f, dz)), false);
    }
    // foreman's lift log on a post
    b.m(M_WOOD, 0x8a7a66, 2.0f);
    b.box(Vec3(-35.03f, 0, -39.53f), Vec3(-34.97f, 1.3f, -39.47f));
    b.box(Vec3(-35.2f, 1.0f, -39.47f), Vec3(-34.8f, 1.5f, -39.455f));
    notePaper(b, N_LIFTS, Vec3(-35.0f, 1.25f, -39.455f), Vec3(0, 0, 1), 0.24f, 0.33f, true);
    b.sign("c_putback", Vec3(-46.0f, 0.02f, -37.0f), Vec3(0, 1, 0), 5.0f);
    b.trigger(TR_QUAY, Vec3(-80, 0, QUAY_Z), Vec3(80, 4, -24));
    b.trigger(TR_LIFTS, Vec3(-58, 0, QUAY_Z), Vec3(-24, 4, -37));

    // ---- barge H-12 ---------------------------------------------------------
    {
        const float x0 = 14.0f, x1 = 40.0f, z0 = -62.0f, z1 = -50.0f, dk = -0.5f;
        b.m(M_RUST, 0x6a6a6a, 0.3f).g(0.6f);
        b.box(Vec3(x0, -3.4f, z0), Vec3(x1, dk, z1), F_SIDES);
        b.g(1.0f);
        b.m(M_KELP, 0xffffff, 0.3f);
        b.box(Vec3(x0 - 0.02f, -3.4f, z0 - 0.02f), Vec3(x1 + 0.02f, -1.9f, z1 + 0.02f), F_SIDES);
        b.m(M_DIAMOND, 0x9a9a94, 0.5f);
        for (float x = x0; x < x1; x += 6.5f) b.box(Vec3(x, dk - 0.1f, z0), Vec3(std::min(x + 6.5f, x1), dk, z1), F_PY);
        b.collider(Vec3(x0, -3.4f, z0), Vec3(x1, dk, z1), COL_SOLID | COL_SIGHT, SURF_METAL);
        // bulwarks
        b.m(M_RUST, 0x7a7a74, 0.5f);
        b.solid(Vec3(x0, dk, z0), Vec3(x1, dk + 0.6f, z0 + 0.12f), SURF_METAL, CF_LOW | COL_SIGHT);
        b.solid(Vec3(x0, dk, z0), Vec3(x0 + 0.12f, dk + 0.6f, z1), SURF_METAL, CF_LOW);
        b.solid(Vec3(x1 - 0.12f, dk, z0), Vec3(x1, dk + 0.6f, z1), SURF_METAL, CF_LOW);
        b.solid(Vec3(x0, dk, z1 - 0.12f), Vec3(25.6f, dk + 0.6f, z1), SURF_METAL, CF_LOW);
        b.solid(Vec3(28.4f, dk, z1 - 0.12f), Vec3(x1, dk + 0.6f, z1), SURF_METAL, CF_LOW);
        // invisible rails so nobody walks into the harbour
        b.collider(Vec3(x0 - 0.3f, dk, z0 - 0.3f), Vec3(x1 + 0.3f, dk + 2.2f, z0), COL_SOLID);
        b.collider(Vec3(x0 - 0.3f, dk, z0), Vec3(x0, dk + 2.2f, z1), COL_SOLID);
        b.collider(Vec3(x1, dk, z0), Vec3(x1 + 0.3f, dk + 2.2f, z1), COL_SOLID);
        b.collider(Vec3(x0, dk, z1 - 0.12f), Vec3(25.6f, dk + 2.2f, z1 + 0.1f), COL_SOLID);
        b.collider(Vec3(28.4f, dk, z1 - 0.12f), Vec3(x1, dk + 2.2f, z1 + 0.1f), COL_SOLID);
        // gangway: timber ramp with cleats (stepped collider)
        b.m(M_WOOD, 0xa09078, 1.0f);
        b.beam(Vec3(27.0f, 0.0f, QUAY_Z + 0.5f), Vec3(27.0f, dk + 0.04f, z1 - 0.9f), 2.2f, 0.08f);
        b.m(M_WOOD, 0x6a5a48, 1.0f);
        for (int i = 0; i < 8; i++) {
            float t = (i + 0.5f) / 8.0f;
            float z = lerpf(QUAY_Z + 0.5f, z1 - 0.9f, t), y = lerpf(0.0f, dk + 0.04f, t);
            b.box(Vec3(25.95f, y + 0.03f, z - 0.03f), Vec3(28.05f, y + 0.07f, z + 0.03f));
        }
        for (int i = 0; i < 6; i++) {
            float zA = lerpf(QUAY_Z + 0.5f, z1 - 0.9f, i / 6.0f), zB = lerpf(QUAY_Z + 0.5f, z1 - 0.9f, (i + 1) / 6.0f);
            float y = lerpf(0.0f, dk, (i + 0.5f) / 6.0f);
            b.collider(Vec3(25.9f, y - 0.4f, zB), Vec3(28.1f, y, zA), COL_SOLID, SURF_WOOD);
        }
        b.m(M_RUST, 0x808080, 1.0f);
        for (int s = 0; s < 2; s++) {
            float gx = s ? 28.1f : 25.9f;
            b.beam(Vec3(gx, 1.0f, QUAY_Z + 0.5f), Vec3(gx, dk + 1.0f, z1 - 0.9f), 0.04f);
            b.beam(Vec3(gx, 0.0f, QUAY_Z + 0.5f), Vec3(gx, 1.0f, QUAY_Z + 0.5f), 0.04f);
            b.beam(Vec3(gx, dk, z1 - 0.9f), Vec3(gx, dk + 1.0f, z1 - 0.9f), 0.04f);
            b.collider(Vec3(gx - (s ? 0.0f : 0.3f), -1.0f, z1 - 1.0f), Vec3(gx + (s ? 0.3f : 0.0f), 2.0f, QUAY_Z + 0.7f), COL_SOLID);
        }
        // mooring lines
        b.m(M_TARP, 0x8a7a5a, 2.0f);
        b.beam(Vec3(22.5f, 0.55f, QUAY_Z + 1.4f), Vec3(17.0f, dk + 0.5f, z1 - 0.1f), 0.06f);
        b.beam(Vec3(31.5f, 0.55f, QUAY_Z + 1.4f), Vec3(37.5f, dk + 0.5f, z1 - 0.1f), 0.06f);
        // deck house
        const float hx0 = 32.0f, hx1 = 38.2f, hz0 = -60.2f, hz1 = -53.6f, ht = dk + 2.45f;
        WallStyle st = {M_PAINTSTEEL, 0x8a8f86, 0.6f, M_PANEL, 0xc0c0c0, 0.6f, 0.12f, 0.6f};
        Opening oW[2] = {{-58.4f, -57.3f, dk + 1.1f, dk + 1.9f, true}, {-56.1f, -55.1f, dk, dk + 2.0f, false}};
        wallRun(b, 1, hx0, hz0, hz1, dk, ht, +1, st, oW, 2);
        Opening oE[1] = {{-58.0f, -56.0f, dk + 1.1f, dk + 1.9f, true}};
        wallRun(b, 1, hx1, hz0, hz1, dk, ht, -1, st, oE, 1);
        wallRun(b, 0, hz0, hx0, hx1, dk, ht, +1, st);
        Opening oS[1] = {{34.0f, 36.2f, dk + 1.1f, dk + 1.9f, true}};
        wallRun(b, 0, hz1, hx0, hx1, dk, ht, -1, st, oS, 1);
        b.m(M_ROOF, 0xffffff, 0.4f);
        b.solid(Vec3(hx0 - 0.2f, ht, hz0 - 0.2f), Vec3(hx1 + 0.2f, ht + 0.12f, hz1 + 0.2f), SURF_METAL);
        b.m(M_PANEL, 0x808080, 0.6f);
        b.box(Vec3(hx0, ht - 0.01f, hz0), Vec3(hx1, ht, hz1), F_NY);
        b.m(M_WOOD, 0x8a7a66, 0.8f);
        b.box(Vec3(hx0 + 0.06f, dk, hz0 + 0.06f), Vec3(hx1 - 0.06f, dk + 0.012f, hz1 - 0.06f), F_PY);
        b.collider(Vec3(hx0 + 0.06f, dk - 0.2f, hz0 + 0.06f), Vec3(hx1 - 0.06f, dk + 0.012f, hz1 - 0.06f), COL_SOLID, SURF_WOOD);
        makeDoor(b, D_BARGE, Vec3(hx0, dk, -56.1f), 1.0f, 1.98f, -PI * 0.5f, -1.4f, M_PAINTSTEEL, 0x7a7f76, -1, true);
        b.sign("barge", Vec3(hx0 - 0.065f, dk + 2.1f, -57.8f), Vec3(-1, 0, 0), 1.1f);
        // interior: bunk, table, diver's log, the third fuse
        float fy = dk + 0.012f;
        b.m(M_WOOD, 0x7a6a56, 1.0f);
        b.box(Vec3(hx1 - 0.95f, fy, hz0 + 0.1f), Vec3(hx1 - 0.08f, fy + 0.45f, hz0 + 2.1f));
        b.m(M_TARP, 0x4a4a5a, 1.5f);
        b.box(Vec3(hx1 - 0.93f, fy + 0.45f, hz0 + 0.12f), Vec3(hx1 - 0.1f, fy + 0.57f, hz0 + 2.08f));
        b.collider(Vec3(hx1 - 0.95f, fy, hz0 + 0.1f), Vec3(hx1 - 0.08f, fy + 0.57f, hz0 + 2.1f), CF_PROP);
        b.m(M_WOOD, 0x8a7a60, 1.0f);
        b.box(Vec3(33.6f, fy + 0.7f, hz1 - 1.0f), Vec3(35.6f, fy + 0.75f, hz1 - 0.1f));
        b.box(Vec3(33.65f, fy, hz1 - 0.95f), Vec3(33.72f, fy + 0.7f, hz1 - 0.15f));
        b.box(Vec3(35.48f, fy, hz1 - 0.95f), Vec3(35.55f, fy + 0.7f, hz1 - 0.15f));
        b.collider(Vec3(33.6f, fy, hz1 - 1.0f), Vec3(35.6f, fy + 0.75f, hz1 - 0.1f), CF_PROP, SURF_WOOD);
        notePaper(b, N_DIVER, Vec3(34.3f, fy + 0.751f, hz1 - 0.5f), Vec3(0, 1, 0), 0.24f, 0.32f, true);
        itemFuse(b, IT_FUSE_C, Vec3(35.2f, fy + 0.752f, hz1 - 0.45f), 1.2f);
        itemTape(b, IT_TAPE3, Vec3(hx1 - 0.5f, fy + 0.572f, hz0 + 1.0f), 0.8f);
        // chart on the wall, oilskins, a hurricane lamp that is somehow still lit
        b.sign("blueprint", Vec3(35.0f, dk + 1.5f, hz0 + 0.065f), Vec3(0, 0, 1), 1.3f);
        b.m(M_OILSKIN, 0xffffff, 1.0f);
        b.box(Vec3(hx0 + 0.07f, dk + 0.5f, hz0 + 0.4f), Vec3(hx0 + 0.16f, dk + 1.6f, hz0 + 0.9f));
        b.m(M_PAINTSTEEL, 0x2f4a36, 2.0f);
        b.cyl(Vec3(33.9f, fy + 0.75f, hz1 - 0.4f), Vec3(33.9f, fy + 0.8f, hz1 - 0.4f), 0.055f, 8);
        b.m(M_PLAIN, 0xffd090, 2.0f).emissive(EG_ALWAYS, 0.7f);
        b.cyl(Vec3(33.9f, fy + 0.8f, hz1 - 0.4f), Vec3(33.9f, fy + 0.93f, hz1 - 0.4f), 0.04f, 8, false);
        b.noEmis();
        b.light(Vec3(33.9f, fy + 1.05f, hz1 - 0.45f), 0xffb878, 1.0f, 6.0f, LG_ALWAYS, FL_PULSE, 0.3f);
        b.zone(Z_BARGE_CABIN, Vec3(hx0, dk - 0.1f, hz0), Vec3(hx1, ht, hz1));
        // deck gear: winch, compressor, the helmet
        b.m(M_MACHINE, 0x4a5a50, 1.0f).g(0.7f);
        b.box(Vec3(17.0f, dk, -57.5f), Vec3(19.0f, dk + 0.9f, -55.5f));
        b.g(1.0f);
        b.m(M_RUST, 0x808080, 1.0f);
        b.cyl(Vec3(17.2f, dk + 1.1f, -56.5f), Vec3(18.8f, dk + 1.1f, -56.5f), 0.45f, 12);
        b.collider(Vec3(17.0f, dk, -57.5f), Vec3(19.0f, dk + 1.55f, -55.5f), CF_PROP, SURF_METAL);
        // A-frame over the stern
        b.m(M_RUST, 0x8a8a8a, 0.6f);
        b.beam(Vec3(15.0f, dk, -59.5f), Vec3(14.2f, dk + 5.0f, -56.5f), 0.16f);
        b.beam(Vec3(15.0f, dk, -53.5f), Vec3(14.2f, dk + 5.0f, -56.5f), 0.16f);
        b.beam(Vec3(14.2f, dk + 5.0f, -56.5f), Vec3(13.2f, dk + 1.0f, -56.5f), 0.03f);
        b.beam(Vec3(18.0f, dk + 1.4f, -56.5f), Vec3(14.2f, dk + 5.0f, -56.5f), 0.03f);
        // compressor
        b.m(M_PAINTSTEEL, 0x8a2a18, 1.0f);
        b.cyl(Vec3(22.0f, dk + 0.5f, -58.6f), Vec3(23.4f, dk + 0.5f, -58.6f), 0.3f, 10);
        b.m(M_MACHINE, 0x30302c, 1.5f);
        b.box(Vec3(22.3f, dk + 0.8f, -58.85f), Vec3(23.1f, dk + 1.25f, -58.35f));
        b.m(M_RUBBER, 0x505050, 2.0f);
        b.beam(Vec3(23.4f, dk + 0.5f, -58.6f), Vec3(24.6f, dk + 0.04f, -57.6f), 0.04f);
        b.beam(Vec3(24.6f, dk + 0.04f, -57.6f), Vec3(25.4f, dk + 0.04f, -56.2f), 0.04f);
        b.collider(Vec3(22.0f, dk, -58.9f), Vec3(23.4f, dk + 1.25f, -58.3f), CF_PROP, SURF_METAL);
        crate(b, Vec3(25.0f, dk, -56.4f), Vec3(25.9f, dk + 0.7f, -55.6f));
        b.beginDyn();
        b.m(M_RUST, 0xb09a50, 2.0f);
        static const float hr[6] = {0.2f, 0.24f, 0.24f, 0.2f, 0.1f, 0.0f};
        static const float hy[6] = {0.0f, 0.08f, 0.26f, 0.38f, 0.44f, 0.46f};
        b.lathe(Vec3(0, 0, 0), hr, hy, 6, 10);
        b.cyl(Vec3(0, 0, 0), Vec3(0, -0.06f, 0), 0.3f, 10, true, 0.26f);
        b.m(M_SCREEN, 0xa0b0a8, 1.0f).fit();
        b.cyl(Vec3(0, 0.2f, 0.2f), Vec3(0, 0.2f, 0.27f), 0.1f, 8);
        b.cyl(Vec3(0.2f, 0.22f, 0.0f), Vec3(0.25f, 0.22f, 0.0f), 0.06f, 8);
        b.cyl(Vec3(-0.25f, 0.22f, 0.0f), Vec3(-0.2f, 0.22f, 0.0f), 0.06f, 8);
        b.named(DY_HELMET, b.endDyn(), Mat4::translate(Vec3(25.45f, dk + 0.76f, -56.0f)) * Mat4::rotY(2.4f));
        b.interact(IK_USE, U_HELMET, Vec3(25.1f, dk + 0.7f, -56.35f), Vec3(25.8f, dk + 1.3f, -55.65f), 2.3f);
        // wetsuit laid out like a body, coiled hose
        b.m(M_RUBBER, 0x707070, 2.0f);
        b.box(Vec3(20.0f, dk, -53.0f), Vec3(20.5f, dk + 0.08f, -51.3f));
        b.box(Vec3(19.6f, dk, -52.6f), Vec3(20.9f, dk + 0.07f, -52.3f));
        for (int i = 0; i < 3; i++) b.cyl(Vec3(29.5f, dk + i * 0.07f, -59.0f), Vec3(29.5f, dk + 0.07f + i * 0.07f, -59.0f), 0.5f - i * 0.03f, 12, false);
        b.floorDecal("puddle", Vec3(25.0f, dk, -55.0f), 3.6f, 3.0f, 0.3f);
        b.floorDecal("bootR", Vec3(26.6f, dk, -53.2f), 0.14f, 0.32f, 0.0f);
        b.floorDecal("bootL", Vec3(26.3f, dk, -53.9f), 0.14f, 0.32f, 0.0f);
        b.trigger(TR_BARGE, Vec3(x0, dk - 0.1f, z0), Vec3(x1, dk + 3, z1));
    }
    (void)w;
}
