// Developer tool: builds the level and writes a top-down map (PPM) of the nav grid,
// colliders, items and notes, plus reachability statistics.
#include "../src/world.h"
#include <chrono>

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "map.ppm";
    auto t0 = std::chrono::steady_clock::now();
    World w;
    world_build(w);
    auto t1 = std::chrono::steady_clock::now();
    printf("world: %.2fs  static verts=%zu tris=%zu chunks=%zu | dyn verts=%zu meshes=%zu objs=%zu | colliders=%zu lights=%zu interacts=%zu\n",
           std::chrono::duration<double>(t1 - t0).count(), w.staticMesh.v.size(), w.staticMesh.idx.size() / 3, w.chunks.size(), w.dynMesh.v.size(),
           w.dynMeshes.size(), w.dyn.size(), w.colliders.size(), w.lights.size(), w.interacts.size());
    size_t maxl = 0;
    for (auto& c : w.chunks) maxl = std::max(maxl, c.lights.size());
    printf("max lights/chunk=%zu\n", maxl);
    const NavGrid& n = w.nav;
    const int S = 4; // pixels per nav cell... keep 1:1 scaled x4
    int W = n.w * S / 2, H = n.h * S / 2;
    std::vector<u8> img((size_t)W * H * 3, 0);
    auto px = [&](int x, int y, u8 r, u8 g, u8 b) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        u8* p = &img[((size_t)y * W + x) * 3];
        p[0] = r; p[1] = g; p[2] = b;
    };
    for (int z = 0; z < n.h; z++)
        for (int x = 0; x < n.w; x++) {
            bool bl = n.blocked[(size_t)z * n.w + x] != 0;
            float h = n.height[(size_t)z * n.w + x];
            u8 v = bl ? 20 : (u8)(90 + clampv(h, 0.0f, 1.0f) * 120);
            for (int a = 0; a < S / 2; a++)
                for (int c = 0; c < S / 2; c++) px(x * S / 2 + a, z * S / 2 + c, bl ? 20 : v / 2, v, bl ? 30 : v / 2);
        }
    auto mark = [&](const Vec3& p, u8 r, u8 g, u8 b, int rad) {
        int cx = (int)((p.x - n.x0) / n.cell * S / 2), cz = (int)((p.z - n.z0) / n.cell * S / 2);
        for (int dz = -rad; dz <= rad; dz++)
            for (int dx = -rad; dx <= rad; dx++) px(cx + dx, cz + dz, r, g, b);
    };
    // reachability from the player start
    int bad = 0;
    std::vector<Vec3> path;
    auto check = [&](const char* kind, int id, const Vec3& p, float maxY) {
        if (p.y > maxY) return; // up the crane
        if (p.z < QUAY_Z) return; // barge
        Vec3 s = nav_snap(w, p, 6);
        float d = distXZ(s, p);
        bool ok = nav_path(w, w.playerStart, p, path) && d < 2.2f;
        if (!ok) { printf("  UNREACHABLE %s %d at (%.1f %.1f %.1f) snap dist %.2f\n", kind, id, p.x, p.y, p.z, d); bad++; }
    };
    for (int i = 0; i < IT_COUNT; i++) { mark(w.itemPos[i], 255, 220, 0, 2); check("item", i, w.itemPos[i], 3.0f); }
    for (int i = 0; i < N_COUNT; i++) { mark(w.notePos[i], 0, 200, 255, 2); check("note", i, w.notePos[i], 3.0f); }
    for (int i = 0; i < U_COUNT; i++) { mark(w.usePos[i], 255, 60, 255, 2); check("use", i, w.usePos[i], 3.0f); }
    for (auto& p : w.patrolPoints) { mark(p, 255, 0, 0, 1); check("patrol", 0, p, 3.0f); }
    for (int i = 0; i < HIDE_COUNT; i++) { mark(w.hides[i].outside, 255, 140, 0, 2); check("hide", i, w.hides[i].outside, 3.0f); }
    mark(w.playerStart, 255, 255, 255, 3);
    printf("unreachable: %d\n", bad);
    FILE* f = fopen(out, "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    fwrite(img.data(), 1, img.size(), f);
    fclose(f);
    return 0;
}
