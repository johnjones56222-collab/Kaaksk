// THE 1982 CRANE - procedural textures.
// Every surface, sign, poster and decal is synthesised here at start-up.
#include "texgen.h"

const MatInfo g_matInfo[M_COUNT] = {
    /* ASPHALT    */ {0.38f, 28.0f, 1.0f, 1.0f},
    /* CONCRETE   */ {0.26f, 18.0f, 0.8f, 0.8f},
    /* CONCWALL   */ {0.10f, 8.0f, 0.0f, 0.6f},
    /* BLOCK      */ {0.10f, 8.0f, 0.0f, 0.9f},
    /* CORRUGATED */ {0.34f, 22.0f, 0.0f, 1.3f},
    /* RUST       */ {0.14f, 10.0f, 0.0f, 1.0f},
    /* PAINTSTEEL */ {0.40f, 30.0f, 0.0f, 0.7f},
    /* DIAMOND    */ {0.50f, 38.0f, 0.3f, 1.3f},
    /* GRATE      */ {0.40f, 30.0f, 0.0f, 0.0f},
    /* CHAINLINK  */ {0.50f, 30.0f, 0.0f, 0.0f},
    /* WOOD       */ {0.08f, 8.0f, 0.0f, 0.9f},
    /* PANEL      */ {0.25f, 30.0f, 0.0f, 0.5f},
    /* LINO       */ {0.35f, 40.0f, 0.0f, 0.3f},
    /* CEILING    */ {0.02f, 4.0f, 0.0f, 0.5f},
    /* BRICK      */ {0.08f, 8.0f, 0.0f, 1.0f},
    /* RUBBER     */ {0.22f, 16.0f, 0.0f, 0.9f},
    /* GLASS      */ {0.90f, 80.0f, 0.0f, 0.0f},
    /* PAPER      */ {0.03f, 4.0f, 0.0f, 0.1f},
    /* TARP       */ {0.25f, 16.0f, 0.0f, 1.0f},
    /* HAZARD     */ {0.30f, 24.0f, 0.5f, 0.3f},
    /* LOCKER     */ {0.40f, 40.0f, 0.0f, 0.6f},
    /* SCREEN     */ {0.90f, 90.0f, 0.0f, 0.0f},
    /* PLAIN      */ {0.30f, 30.0f, 0.0f, 0.2f},
    /* GRAVEL     */ {0.10f, 8.0f, 0.6f, 1.0f},
    /* RAIL       */ {0.90f, 60.0f, 0.3f, 0.5f},
    /* CARDBOARD  */ {0.03f, 4.0f, 0.0f, 0.3f},
    /* MACHINE    */ {0.35f, 30.0f, 0.0f, 0.8f},
    /* CONTDOOR   */ {0.34f, 22.0f, 0.0f, 1.0f},
    /* ROOF       */ {0.20f, 16.0f, 0.5f, 0.6f},
    /* KELP       */ {0.80f, 50.0f, 0.0f, 1.0f},
    /* OILSKIN    */ {0.60f, 30.0f, 0.0f, 0.8f},
    /* BLINDS     */ {0.20f, 16.0f, 0.0f, 0.5f},
    /* NOISE      */ {0.0f, 1.0f, 0.0f, 0.0f},
    /* SIGN0..11  */ {0.12f, 12.0f, 0.0f, 0.0f}, {0.12f, 12.0f, 0.0f, 0.0f}, {0.12f, 12.0f, 0.0f, 0.0f},
                     {0.12f, 12.0f, 0.0f, 0.0f}, {0.12f, 12.0f, 0.0f, 0.0f}, {0.12f, 12.0f, 0.0f, 0.0f},
                     {0.12f, 12.0f, 0.0f, 0.0f}, {0.12f, 12.0f, 0.0f, 0.0f}, {0.12f, 12.0f, 0.0f, 0.0f},
                     {0.12f, 12.0f, 0.0f, 0.0f}, {0.12f, 12.0f, 0.0f, 0.0f}, {0.12f, 12.0f, 0.0f, 0.0f},
};

FontAtlas g_fonts[FONT_COUNT];

static const int N = TEX_SIZE;

struct Px { float r, g, b, a; };
struct Img {
    std::vector<Px> p;
    Img() : p((size_t)N * N, Px{0, 0, 0, 0}) {}
    Px& at(int x, int y) { return p[(size_t)((y) & (N - 1)) * N + ((x) & (N - 1))]; }
};

// ---------------------------------------------------------------------------
// Noise bank: tileable fields shared by all materials
// ---------------------------------------------------------------------------
enum { B_FBM2, B_FBM4, B_FBM8, B_FBM16, B_FBM32, B_WHITE, B_WOR6, B_WOR14, B_EDGE8, B_RIDGE4, B_FBM4B, B_FBM8B, B_COUNT };
static std::vector<float> g_bank[B_COUNT];
static inline float B(int f, int x, int y) { return g_bank[f][(size_t)((y) & (N - 1)) * N + ((x) & (N - 1))]; }

static float latt(int x, int y, int period, u32 seed) {
    x %= period; if (x < 0) x += period;
    y %= period; if (y < 0) y += period;
    return hash2f(x + (int)(seed * 131u), y + (int)(seed * 977u));
}
static float vnoise(float x, float y, int period, u32 seed) {
    float fxf = std::floor(x), fyf = std::floor(y);
    int xi = (int)fxf, yi = (int)fyf;
    float fx = x - fxf, fy = y - fyf;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = latt(xi, yi, period, seed), b = latt(xi + 1, yi, period, seed);
    float c = latt(xi, yi + 1, period, seed), d = latt(xi + 1, yi + 1, period, seed);
    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}
static float fbm(float u, float v, int freq, int oct, u32 seed, bool ridged = false) {
    float s = 0, a = 0.5f, n = 0;
    for (int o = 0; o < oct; o++) {
        float t = vnoise(u * freq, v * freq, freq, seed + (u32)o * 17u);
        if (ridged) t = 1.0f - std::fabs(2.0f * t - 1.0f);
        s += a * t; n += a; a *= 0.5f; freq *= 2;
    }
    return s / n;
}
static void worley(float u, float v, int cells, u32 seed, float& f1, float& f2) {
    float x = u * cells, y = v * cells;
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    f1 = f2 = 9.0f;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int cx = xi + dx, cy = yi + dy;
            int wx = ((cx % cells) + cells) % cells, wy = ((cy % cells) + cells) % cells;
            float px = cx + hash2f(wx + (int)seed * 7, wy + 13);
            float py = cy + hash2f(wx + 101, wy + (int)seed * 3);
            float d = std::sqrt((px - x) * (px - x) + (py - y) * (py - y));
            if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) f2 = d;
        }
}
static void normalizeField(std::vector<float>& f) {
    float lo = 1e9f, hi = -1e9f;
    for (float v : f) { lo = std::min(lo, v); hi = std::max(hi, v); }
    float inv = hi > lo ? 1.0f / (hi - lo) : 1.0f;
    for (float& v : f) v = (v - lo) * inv;
}
static void buildBank() {
    for (int i = 0; i < B_COUNT; i++) g_bank[i].assign((size_t)N * N, 0.0f);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float u = (x + 0.5f) / N, v = (y + 0.5f) / N;
            size_t i = (size_t)y * N + x;
            g_bank[B_FBM2][i] = fbm(u, v, 2, 5, 11);
            g_bank[B_FBM4][i] = fbm(u, v, 4, 5, 23);
            g_bank[B_FBM8][i] = fbm(u, v, 8, 4, 37);
            g_bank[B_FBM16][i] = fbm(u, v, 16, 4, 41);
            g_bank[B_FBM32][i] = fbm(u, v, 32, 3, 53);
            g_bank[B_WHITE][i] = hash2f(x * 3 + 7, y * 5 + 3);
            float f1, f2;
            worley(u, v, 6, 5, f1, f2);
            g_bank[B_WOR6][i] = f1;
            worley(u, v, 14, 9, f1, f2);
            g_bank[B_WOR14][i] = f1;
            worley(u, v, 8, 13, f1, f2);
            g_bank[B_EDGE8][i] = f2 - f1;
            g_bank[B_RIDGE4][i] = fbm(u, v, 4, 4, 67, true);
            g_bank[B_FBM4B][i] = fbm(u, v, 4, 5, 79);
            g_bank[B_FBM8B][i] = fbm(u, v, 8, 4, 97);
        }
    for (int i = 0; i < B_COUNT; i++)
        if (i != B_WHITE) normalizeField(g_bank[i]);
}

// ---------------------------------------------------------------------------
// Colour + drawing helpers
// ---------------------------------------------------------------------------
static inline Vec3 hex(u32 c) { return Vec3(((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f); }
static inline void setc(Px& p, const Vec3& c) { p.r = c.x; p.g = c.y; p.b = c.z; }
static inline Vec3 getc(const Px& p) { return Vec3(p.r, p.g, p.b); }
static inline void mulc(Px& p, float m) { p.r *= m; p.g *= m; p.b *= m; }
static inline void mixc(Px& p, const Vec3& c, float t) {
    p.r = lerpf(p.r, c.x, t); p.g = lerpf(p.g, c.y, t); p.b = lerpf(p.b, c.z, t);
}
// "over" composite used on the sign sheets, where alpha is opacity
static inline void over(Px& p, const Vec3& c, float a) {
    if (a <= 0.0f) return;
    float oa = a + p.a * (1.0f - a);
    if (oa < 1e-5f) return;
    float k = p.a * (1.0f - a);
    p.r = (c.x * a + p.r * k) / oa;
    p.g = (c.y * a + p.g * k) / oa;
    p.b = (c.z * a + p.b * k) / oa;
    p.a = oa;
}

struct Clip { int x0, y0, x1, y1; };
static Clip g_clip = {0, 0, N, N};

static void fillRect(Img& im, float x0, float y0, float x1, float y1, const Vec3& c, float a = 1.0f) {
    int ix0 = std::max(g_clip.x0, (int)std::floor(x0)), iy0 = std::max(g_clip.y0, (int)std::floor(y0));
    int ix1 = std::min(g_clip.x1, (int)std::ceil(x1)), iy1 = std::min(g_clip.y1, (int)std::ceil(y1));
    for (int y = iy0; y < iy1; y++)
        for (int x = ix0; x < ix1; x++) {
            float cx = std::min(x + 1.0f, x1) - std::max((float)x, x0);
            float cy = std::min(y + 1.0f, y1) - std::max((float)y, y0);
            float cov = saturate(cx) * saturate(cy);
            over(im.at(x, y), c, a * cov);
        }
}
static void strokeRect(Img& im, float x0, float y0, float x1, float y1, float t, const Vec3& c, float a = 1.0f) {
    fillRect(im, x0, y0, x1, y0 + t, c, a);
    fillRect(im, x0, y1 - t, x1, y1, c, a);
    fillRect(im, x0, y0 + t, x0 + t, y1 - t, c, a);
    fillRect(im, x1 - t, y0 + t, x1, y1 - t, c, a);
}
static void disc(Img& im, float cx, float cy, float r, const Vec3& c, float a = 1.0f) {
    int ix0 = std::max(g_clip.x0, (int)std::floor(cx - r - 1)), iy0 = std::max(g_clip.y0, (int)std::floor(cy - r - 1));
    int ix1 = std::min(g_clip.x1, (int)std::ceil(cx + r + 1)), iy1 = std::min(g_clip.y1, (int)std::ceil(cy + r + 1));
    for (int y = iy0; y < iy1; y++)
        for (int x = ix0; x < ix1; x++) {
            float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
            over(im.at(x, y), c, a * saturate(r - d + 0.5f));
        }
}
static void ring(Img& im, float cx, float cy, float r, float t, const Vec3& c, float a = 1.0f) {
    int ix0 = std::max(g_clip.x0, (int)std::floor(cx - r - t - 1)), iy0 = std::max(g_clip.y0, (int)std::floor(cy - r - t - 1));
    int ix1 = std::min(g_clip.x1, (int)std::ceil(cx + r + t + 1)), iy1 = std::min(g_clip.y1, (int)std::ceil(cy + r + t + 1));
    for (int y = iy0; y < iy1; y++)
        for (int x = ix0; x < ix1; x++) {
            float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
            over(im.at(x, y), c, a * saturate(t * 0.5f - std::fabs(d - r) + 0.5f));
        }
}
static void line(Img& im, float x0, float y0, float x1, float y1, float t, const Vec3& c, float a = 1.0f) {
    float mnx = std::min(x0, x1) - t - 1, mxx = std::max(x0, x1) + t + 1;
    float mny = std::min(y0, y1) - t - 1, mxy = std::max(y0, y1) + t + 1;
    int ix0 = std::max(g_clip.x0, (int)std::floor(mnx)), iy0 = std::max(g_clip.y0, (int)std::floor(mny));
    int ix1 = std::min(g_clip.x1, (int)std::ceil(mxx)), iy1 = std::min(g_clip.y1, (int)std::ceil(mxy));
    float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy;
    for (int y = iy0; y < iy1; y++)
        for (int x = ix0; x < ix1; x++) {
            float px = x + 0.5f - x0, py = y + 0.5f - y0;
            float h = l2 > 0 ? saturate((px * dx + py * dy) / l2) : 0.0f;
            float ex = px - dx * h, ey = py - dy * h;
            float d = std::sqrt(ex * ex + ey * ey);
            over(im.at(x, y), c, a * saturate(t * 0.5f - d + 0.5f));
        }
}

// --- text -------------------------------------------------------------------
static float atlasSample(const FontAtlas& F, int ch, float sx, float sy) {
    if (sx < 0 || sy < 0 || sx >= F.cell - 1 || sy >= F.cell - 1) return 0.0f;
    int cx = (ch % F.cols) * F.cell, cy = (ch / F.cols) * F.cell;
    int x0 = (int)sx, y0 = (int)sy;
    float fx = sx - x0, fy = sy - y0;
    const u8* a = &F.alpha[(size_t)(cy + y0) * F.texW + cx + x0];
    float v0 = lerpf(a[0], a[1], fx), v1 = lerpf(a[F.texW], a[F.texW + 1], fx);
    return lerpf(v0, v1, fy) * (1.0f / 255.0f);
}
static float textW(int font, const char* s, float px) {
    const FontAtlas& F = g_fonts[font];
    if (!F.valid()) return (float)std::strlen(s) * px * 0.55f;
    float w = 0;
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c < 32 || c > 127) c = '?';
        w += F.adv[c - 32];
    }
    return w * px / F.px;
}
// mode: 0 = clean, 1 = worn stencil paint, 2 = chalk scrawl
static void drawText(Img& im, int font, const char* s, float x, float baseY, float px, const Vec3& col, float alpha = 1.0f,
                     int mode = 0, u32 seed = 1) {
    const FontAtlas& F = g_fonts[font];
    if (!F.valid()) return;
    Rng rng(seed * 2654435761u + 77u);
    float pen = x;
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c < 32 || c > 127) c = '?';
        float gpx = px, gy = baseY, gx = pen;
        if (mode == 2) {
            gpx = px * rng.range(0.88f, 1.12f);
            gy += rng.range(-0.07f, 0.07f) * px;
            gx += rng.range(-0.03f, 0.03f) * px;
        }
        float scale = gpx / F.px;
        float ds = F.cell * scale;
        float dx0 = gx - 8.0f * scale, dy0 = gy - F.cell * 0.75f * scale;
        int ix0 = std::max(g_clip.x0, (int)std::floor(dx0)), iy0 = std::max(g_clip.y0, (int)std::floor(dy0));
        int ix1 = std::min(g_clip.x1, (int)std::ceil(dx0 + ds)), iy1 = std::min(g_clip.y1, (int)std::ceil(dy0 + ds));
        if (c != ' ')
            for (int y = iy0; y < iy1; y++)
                for (int xx = ix0; xx < ix1; xx++) {
                    float cov = 0;
                    for (int k = 0; k < 4; k++) {
                        float ox = (k & 1) ? 0.75f : 0.25f, oy = (k & 2) ? 0.75f : 0.25f;
                        cov += atlasSample(F, c - 32, (xx + ox - dx0) / scale, (y + oy - dy0) / scale);
                    }
                    cov *= 0.25f;
                    if (cov <= 0.003f) continue;
                    if (mode == 1) {
                        float w = B(B_FBM32, xx * 2, y * 2) * 0.6f + B(B_FBM8, xx, y) * 0.6f;
                        cov *= smoothstepf(0.34f, 0.52f, w);
                    } else if (mode == 2) {
                        float w = B(B_WHITE, xx, y) * 0.55f + B(B_FBM32, xx, y) * 0.75f;
                        cov *= smoothstepf(0.25f, 0.6f, w) * 0.95f;
                    }
                    over(im.at(xx, y), col, alpha * cov);
                }
        pen += F.adv[c - 32] * scale * (mode == 2 ? rng.range(0.95f, 1.1f) : 1.0f);
    }
}
static void drawTextC(Img& im, int font, const char* s, float cx, float baseY, float px, const Vec3& col, float alpha = 1.0f,
                      int mode = 0, u32 seed = 1) {
    drawText(im, font, s, cx - textW(font, s, px) * 0.5f, baseY, px, col, alpha, mode, seed);
}

// Lays out '\n' separated lines centred in a rect. Line prefixes: '#' big, '~' small.
static void drawLines(Img& im, int font, const char* text, float x0, float y0, float w, float h, const Vec3& col, float alpha,
                      int mode, u32 seed) {
    struct L { std::string s; float k; };
    std::vector<L> lines;
    std::string cur;
    for (const char* p = text;; p++) {
        if (*p == '\n' || *p == 0) {
            L l; l.k = 1.0f;
            if (!cur.empty() && cur[0] == '#') { l.k = 1.7f; cur.erase(0, 1); }
            else if (!cur.empty() && cur[0] == '~') { l.k = 0.55f; cur.erase(0, 1); }
            l.s = cur; lines.push_back(l); cur.clear();
            if (*p == 0) break;
        } else cur.push_back(*p);
    }
    float totalH = 0, maxW = 1;
    for (auto& l : lines) { totalH += l.k * 1.18f; maxW = std::max(maxW, textW(font, l.s.c_str(), l.k)); }
    float px = std::min(w / maxW, h / totalH);
    float y = y0 + (h - totalH * px) * 0.5f;
    for (size_t i = 0; i < lines.size(); i++) {
        float lh = lines[i].k * 1.18f * px, sz = lines[i].k * px;
        drawTextC(im, font, lines[i].s.c_str(), x0 + w * 0.5f, y + lh * 0.5f + sz * 0.36f, sz, col, alpha, mode, seed + (u32)i * 31u);
        y += lh;
    }
}

// ---------------------------------------------------------------------------
// Materials
// ---------------------------------------------------------------------------
#define FORXY for (int y = 0; y < N; y++) for (int x = 0; x < N; x++)

static void gen_asphalt(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float g = B(B_WHITE, x, y), f32 = B(B_FBM32, x, y), blot = B(B_FBM4, x, y), big = B(B_FBM2, x + 37, y + 91);
        float v = 0.085f + 0.055f * f32 + 0.035f * (g - 0.5f) + 0.05f * (blot - 0.5f);
        if (g > 0.93f) v += 0.16f * (g - 0.93f) / 0.07f;
        float h = 0.5f + 0.3f * (f32 - 0.5f) + 0.25f * (g - 0.5f);
        float e = B(B_EDGE8, x, y);
        float crack = smoothstepf(0.045f, 0.0f, e) * smoothstepf(0.42f, 0.6f, B(B_FBM4B, x, y));
        v *= 1.0f - 0.65f * crack; h -= 0.45f * crack;
        float patch = smoothstepf(0.60f, 0.64f, big);
        v *= 1.0f - 0.28f * patch;
        float seam = smoothstepf(0.03f, 0.0f, std::fabs(big - 0.62f));
        v *= 1.0f - 0.4f * seam;
        float oil = smoothstepf(0.74f, 0.9f, B(B_FBM8B, x, y));
        v *= 1.0f - 0.35f * oil;
        p.r = v; p.g = v; p.b = v * 1.06f; p.a = saturate(h);
    }
}
static void gen_concrete(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float st = B(B_FBM2, x, y), f = B(B_FBM16, x, y), g = B(B_WHITE, x, y);
        float v = 0.37f * (0.78f + 0.36f * st) + 0.05f * (f - 0.5f) + 0.025f * (g - 0.5f);
        float h = 0.6f + 0.2f * (f - 0.5f);
        if (g > 0.975f) { v *= 0.6f; h -= 0.3f; }
        int jx = x & 255, jy = y & 255;
        float j = std::min(std::min(jx, 255 - jx), std::min(jy, 255 - jy));
        float joint = smoothstepf(3.5f, 1.0f, j);
        v *= 1.0f - 0.6f * joint; h -= 0.5f * joint;
        float cr = smoothstepf(0.94f, 0.985f, B(B_RIDGE4, x, y)) * smoothstepf(0.5f, 0.7f, B(B_FBM4B, x, y));
        v *= 1.0f - 0.5f * cr; h -= 0.3f * cr;
        float rust = smoothstepf(0.72f, 0.92f, B(B_FBM8, x + 99, y + 31)) * 0.35f;
        Vec3 c(v * 1.02f, v, v * 0.95f);
        c = lerp(c, Vec3(0.30f, 0.19f, 0.11f), rust);
        setc(p, c); p.a = saturate(h);
    }
}
static void gen_concwall(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float streak = B(B_FBM4, x * 8, y), st = B(B_FBM2, x + 200, y + 100), f = B(B_FBM16, x, y);
        float v = 0.33f * (0.72f + 0.3f * streak + 0.22f * st) + 0.04f * (f - 0.5f);
        float h = 0.6f + 0.2f * (f - 0.5f);
        int ly = y % 128;
        float pour = smoothstepf(3.0f, 0.0f, (float)std::min(ly, 127 - ly));
        v *= 1.0f - 0.22f * pour; h -= 0.25f * pour;
        // form-tie holes
        int hx = (x + 64) % 128 - 64, hy = (y + 64) % 128 - 64;
        float hd = std::sqrt((float)(hx * hx + hy * hy));
        float hole = smoothstepf(6.0f, 3.5f, hd);
        v *= 1.0f - 0.55f * hole; h -= 0.5f * hole;
        float damp = smoothstepf(0.55f, 0.85f, B(B_FBM4B, x * 4, y)) * (y / (float)N);
        Vec3 c(v, v * 0.99f, v * 0.96f);
        c = lerp(c, Vec3(0.10f, 0.12f, 0.09f), damp * 0.6f);
        setc(p, c); p.a = saturate(h);
    }
}
static void gen_block(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        int row = y / 64;
        int bx = (x + ((row & 1) ? 64 : 0)) & 127, by = y & 63;
        float m = std::min(std::min(bx, 127 - bx), std::min(by, 63 - by));
        float mortar = smoothstepf(3.5f, 1.5f, m);
        float f = B(B_FBM32, x, y), w = B(B_WOR14, x, y), peel = B(B_FBM8, x, y) * 0.6f + B(B_FBM32, x + 9, y + 3) * 0.4f;
        bool upper = y < 288;
        Vec3 paint = upper ? Vec3(0.55f, 0.56f, 0.47f) : Vec3(0.24f, 0.33f, 0.29f);
        paint = paint * (0.85f + 0.3f * B(B_FBM4, x, y));
        Vec3 raw = Vec3(0.34f, 0.33f, 0.31f) * (0.8f + 0.4f * f);
        float pk = smoothstepf(0.62f, 0.68f, peel);
        Vec3 c = lerp(paint, raw, pk);
        float pores = smoothstepf(0.12f, 0.02f, w);
        c = c * (1.0f - 0.3f * pores);
        float band = smoothstepf(4.0f, 1.0f, std::fabs((float)y - 288.0f));
        c = lerp(c, Vec3(0.10f, 0.10f, 0.09f), band * 0.7f);
        c = lerp(c, Vec3(0.20f, 0.20f, 0.19f), mortar * 0.8f);
        float grime = smoothstepf(380.0f, 512.0f, (float)y) * (0.4f + 0.6f * B(B_FBM4B, x * 2, y));
        c = c * (1.0f - 0.55f * grime);
        setc(p, c);
        p.a = saturate(0.65f - 0.5f * mortar - 0.15f * pk + 0.15f * (f - 0.5f) - 0.2f * pores);
    }
}
static void gen_corrugated(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float t = (x & 31) / 32.0f;
        float h = smoothstepf(0.08f, 0.3f, t) - smoothstepf(0.58f, 0.8f, t);
        float slope = (smoothstepf(0.08f, 0.3f, t + 0.03f) - smoothstepf(0.58f, 0.8f, t + 0.03f)) - h;
        float v = 0.70f + 0.16f * h + slope * 0.9f;
        v *= 0.86f + 0.2f * B(B_FBM4, x * 8, y);
        float rustS = smoothstepf(0.58f, 0.8f, B(B_FBM4B, x * 4, y) * 0.75f + B(B_FBM16, x, y) * 0.35f);
        float spot = smoothstepf(0.10f, 0.03f, B(B_WOR14, x, y)) * smoothstepf(0.45f, 0.6f, B(B_FBM4, x + 50, y + 50));
        float rust = saturate(rustS * 0.75f + spot);
        Vec3 c(v, v, v);
        Vec3 rc = lerp(Vec3(0.36f, 0.19f, 0.09f), Vec3(0.20f, 0.10f, 0.06f), B(B_FBM32, x, y));
        c = lerp(c, rc, rust * 0.85f);
        float scr = smoothstepf(0.9f, 0.97f, B(B_FBM32, x * 3, y / 1)) * 0.25f;
        c = c * (1.0f - scr);
        setc(p, c);
        p.a = saturate(0.25f + 0.55f * h - 0.1f * rust + 0.06f * (B(B_FBM32, x, y) - 0.5f));
    }
}
static void gen_rust(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float a = B(B_FBM8, x, y), b = B(B_FBM32, x, y), w = B(B_WOR14, x, y), big = B(B_FBM2, x, y);
        Vec3 c = lerp(Vec3(0.24f, 0.12f, 0.06f), Vec3(0.44f, 0.23f, 0.09f), a);
        c = lerp(c, Vec3(0.55f, 0.33f, 0.13f), smoothstepf(0.6f, 0.9f, b) * 0.5f);
        c = c * (0.7f + 0.5f * big);
        float pit = smoothstepf(0.16f, 0.04f, w);
        c = c * (1.0f - 0.55f * pit);
        float paint = smoothstepf(0.60f, 0.66f, B(B_FBM4B, x, y) * 0.7f + b * 0.3f);
        c = lerp(c, Vec3(0.30f, 0.32f, 0.27f) * (0.8f + 0.3f * a), paint * 0.8f);
        float streak = smoothstepf(0.6f, 0.9f, B(B_FBM4, x * 8, y));
        c = c * (1.0f - 0.25f * streak);
        setc(p, c);
        p.a = saturate(0.5f + 0.35f * (b - 0.5f) - 0.4f * pit + 0.18f * paint);
    }
}
static void gen_paintsteel(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float v = 0.78f * (0.9f + 0.14f * B(B_FBM8, x, y));
        v *= 0.82f + 0.2f * B(B_FBM4, x * 8, y);
        float chipN = B(B_WOR14, x, y), reg = B(B_FBM4B, x, y), fine = B(B_FBM32, x, y);
        float chip = smoothstepf(0.24f, 0.14f, chipN + (1.0f - reg) * 0.25f) + smoothstepf(0.72f, 0.8f, reg * 0.7f + fine * 0.4f);
        chip = saturate(chip);
        Vec3 c(v, v, v);
        Vec3 rc = lerp(Vec3(0.33f, 0.17f, 0.08f), Vec3(0.17f, 0.09f, 0.05f), fine);
        c = lerp(c, rc, chip * 0.92f);
        float halo = smoothstepf(0.34f, 0.2f, chipN + (1.0f - reg) * 0.25f) * (1.0f - chip);
        c = lerp(c, Vec3(0.5f, 0.33f, 0.18f), halo * 0.35f);
        setc(p, c);
        p.a = saturate(0.62f - 0.3f * chip + 0.06f * (fine - 0.5f));
    }
}
static void gen_diamond(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        int cx = x / 32, cy = y / 32;
        float dx = (x & 31) - 15.5f, dy = (y & 31) - 15.5f;
        bool flip = ((cx + cy) & 1) != 0;
        float a = flip ? dx : dy, b = flip ? dy : dx;
        float d = std::fabs(a) / 12.0f + std::fabs(b) / 3.6f;
        float raised = smoothstepf(1.1f, 0.8f, d);
        float wear = B(B_FBM4, x, y), f = B(B_FBM32, x, y);
        float v = 0.20f + 0.08f * f + raised * (0.14f + 0.2f * wear);
        Vec3 c(v, v, v * 1.03f);
        float rust = smoothstepf(0.62f, 0.85f, B(B_FBM8B, x, y)) * (1.0f - raised * 0.7f);
        c = lerp(c, Vec3(0.30f, 0.16f, 0.08f), rust * 0.7f);
        setc(p, c);
        p.a = saturate(0.3f + 0.6f * raised + 0.05f * f);
    }
}
static void gen_grate(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        bool bar = (x & 31) < 7 || (y & 63) < 5;
        float f = B(B_FBM16, x, y);
        float v = 0.16f + 0.1f * f;
        if ((x & 31) < 2 || (y & 63) < 1) v += 0.1f;
        Vec3 c(v, v, v);
        c = lerp(c, Vec3(0.28f, 0.14f, 0.07f), smoothstepf(0.5f, 0.8f, B(B_FBM8, x, y)) * 0.7f);
        setc(p, c);
        p.a = bar ? 1.0f : 0.0f;
    }
}
static void gen_chainlink(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        const int P = 64;
        float d1 = std::fabs((float)((x + y) % P) - P * 0.5f);
        float d2 = std::fabs((float)(((x - y) % P + P) % P) - P * 0.5f);
        float d = std::min(d1, d2);
        float wire = saturate(3.4f - d);
        float v = 0.34f + 0.14f * B(B_FBM16, x, y) + 0.12f * saturate(1.5f - d);
        Vec3 c(v, v, v * 1.02f);
        c = lerp(c, Vec3(0.30f, 0.16f, 0.08f), smoothstepf(0.55f, 0.8f, B(B_FBM8, x, y)) * 0.6f);
        setc(p, c);
        p.a = wire;
    }
}
static void gen_wood(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        int row = y / 64;
        float rh = hash2f(row, 77);
        float grain = B(B_FBM4, x + row * 97, y * 8) * 0.6f + B(B_FBM16, x + row * 31, y * 4) * 0.4f;
        Vec3 base = lerp(Vec3(0.30f, 0.23f, 0.16f), Vec3(0.42f, 0.34f, 0.25f), rh);
        Vec3 c = base * (0.6f + 0.7f * grain);
        float grey = B(B_FBM2, x, y);
        c = lerp(c, Vec3(0.33f, 0.32f, 0.30f) * (0.6f + 0.7f * grain), grey * 0.6f);
        int by = y & 63;
        float gap = smoothstepf(3.0f, 0.5f, (float)std::min(by, 63 - by));
        int jx = (x + (int)(hash2f(row, 5) * 512)) & 511;
        float joint = smoothstepf(2.5f, 0.5f, (float)std::min(jx, 511 - jx));
        float g2 = std::max(gap, joint);
        c = c * (1.0f - 0.85f * g2);
        // knots
        float kx = hash2f(row, 21) * 512.0f, ky = row * 64 + 20.0f + hash2f(row, 9) * 24.0f;
        float ddx = (x - kx), ddy = (y - ky) * 2.0f;
        if (ddx > 256) ddx -= 512;
        if (ddx < -256) ddx += 512;
        float kd = std::sqrt(ddx * ddx + ddy * ddy);
        float knot = smoothstepf(14.0f, 5.0f, kd);
        c = c * (1.0f - 0.6f * knot);
        setc(p, c);
        p.a = saturate(0.55f + 0.3f * (grain - 0.5f) - 0.5f * g2 - 0.15f * knot);
    }
}
static void gen_panel(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        int col = x / 64;
        float ch = hash2f(col, 3);
        float grain = B(B_FBM4, x * 8 + col * 53, y) * 0.55f + B(B_FBM16, x * 4, y + col * 17) * 0.45f;
        Vec3 c = lerp(Vec3(0.30f, 0.17f, 0.09f), Vec3(0.44f, 0.27f, 0.14f), grain) * (0.85f + 0.25f * ch);
        int gx = x & 63;
        float groove = smoothstepf(2.5f, 0.5f, (float)std::min(gx, 63 - gx));
        c = c * (1.0f - 0.7f * groove);
        float stain = smoothstepf(0.6f, 0.9f, B(B_FBM2, x, y)) * 0.25f;
        c = c * (1.0f - stain);
        float low = smoothstepf(400.0f, 512.0f, (float)y) * 0.3f;
        c = c * (1.0f - low);
        setc(p, c);
        p.a = saturate(0.6f - 0.45f * groove + 0.1f * (grain - 0.5f));
    }
}
static void gen_lino(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        int tx = x / 128, ty = y / 128;
        bool dark = ((tx + ty) & 1) != 0;
        float sp = B(B_FBM32, x * 2, y * 2), wear = B(B_FBM2, x, y), f = B(B_FBM8, x, y);
        Vec3 c = dark ? Vec3(0.22f, 0.29f, 0.24f) : Vec3(0.58f, 0.54f, 0.42f);
        c = c * (0.82f + 0.3f * sp);
        c = lerp(c, Vec3(0.36f, 0.34f, 0.29f), smoothstepf(0.5f, 0.85f, wear) * 0.55f);
        int sx = x & 127, sy = y & 127;
        float seam = smoothstepf(2.5f, 0.5f, (float)std::min(std::min(sx, 127 - sx), std::min(sy, 127 - sy)));
        c = c * (1.0f - 0.6f * seam);
        float scuff = smoothstepf(0.93f, 0.97f, B(B_RIDGE4, x + 40, y + 80));
        c = c * (1.0f - 0.45f * scuff);
        float dirt = smoothstepf(0.6f, 0.9f, f) * 0.3f;
        c = c * (1.0f - dirt);
        setc(p, c);
        p.a = saturate(0.6f - 0.3f * seam + 0.04f * (sp - 0.5f));
    }
}
static void gen_ceiling(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float g = B(B_WHITE, x / 2, y / 2), st = B(B_FBM2, x + 77, y + 13), f = B(B_FBM16, x, y);
        float v = 0.62f + 0.05f * (f - 0.5f);
        float hole = (g > 0.88f) ? 1.0f : 0.0f;
        v *= 1.0f - 0.45f * hole;
        Vec3 c(v, v * 0.98f, v * 0.92f);
        float ring = smoothstepf(0.035f, 0.0f, std::fabs(st - 0.62f)) + smoothstepf(0.03f, 0.0f, std::fabs(st - 0.72f)) * 0.7f;
        float inside = smoothstepf(0.62f, 0.66f, st);
        c = lerp(c, Vec3(0.42f, 0.33f, 0.18f), inside * 0.4f);
        c = lerp(c, Vec3(0.26f, 0.19f, 0.09f), saturate(ring) * 0.6f);
        int sx = x & 255, sy = y & 255;
        float bar = smoothstepf(5.0f, 3.0f, (float)std::min(std::min(sx, 255 - sx), std::min(sy, 255 - sy)));
        c = lerp(c, Vec3(0.5f, 0.5f, 0.48f), bar);
        float mold = smoothstepf(0.7f, 0.9f, B(B_FBM8B, x, y)) * inside;
        c = lerp(c, Vec3(0.08f, 0.09f, 0.07f), mold * 0.6f);
        setc(p, c);
        p.a = saturate(0.6f - 0.4f * hole + 0.3f * bar);
    }
}
static void gen_brick(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        int row = y / 32;
        int xo = x + ((row & 1) ? 64 : 0);
        int bcol = (xo & 511) / 128;
        int bx = xo & 127, by = y & 31;
        float m = (float)std::min(std::min(bx, 127 - bx), std::min(by, 31 - by));
        float mortar = smoothstepf(3.0f, 1.2f, m);
        float bh = hash2f(bcol * 7 + 1, row * 13 + 5);
        float f = B(B_FBM32, x, y), soot = B(B_FBM2, x, y);
        Vec3 c = lerp(Vec3(0.33f, 0.13f, 0.09f), Vec3(0.47f, 0.22f, 0.14f), bh);
        if (bh > 0.86f) c = Vec3(0.18f, 0.12f, 0.11f);
        c = c * (0.75f + 0.5f * f);
        c = c * (1.0f - 0.5f * smoothstepf(0.5f, 0.9f, soot));
        c = lerp(c, Vec3(0.33f, 0.31f, 0.28f) * (0.8f + 0.3f * f), mortar);
        float eff = smoothstepf(0.7f, 0.9f, B(B_FBM8, x, y)) * (1.0f - mortar);
        c = lerp(c, Vec3(0.6f, 0.58f, 0.52f), eff * 0.35f);
        setc(p, c);
        p.a = saturate(0.65f - 0.5f * mortar + 0.2f * (f - 0.5f));
    }
}
static void gen_rubber(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        int yy = y & 63;
        float ch = std::fabs((float)yy - 32.0f);
        float t = fractf((x + ch * 0.8f) / 32.0f);
        float groove = smoothstepf(0.0f, 0.08f, t) * smoothstepf(0.3f, 0.22f, t);
        float mid = smoothstepf(3.0f, 1.0f, std::fabs(ch - 1.0f));
        groove = std::max(groove, mid);
        float v = 0.055f + 0.03f * B(B_FBM16, x, y);
        v *= 1.0f - 0.6f * groove;
        float dust = smoothstepf(0.5f, 0.9f, B(B_FBM4, x, y)) * 0.06f;
        setc(p, Vec3(v + dust, v + dust * 0.9f, v + dust * 0.7f));
        p.a = saturate(0.7f - 0.6f * groove);
    }
}
static void gen_glass(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float streak = B(B_FBM4, x * 8, y), blot = B(B_FBM2, x, y), f = B(B_FBM16, x, y);
        float ex = std::min(x, N - 1 - x) / (float)N, ey = std::min(y, N - 1 - y) / (float)N;
        float edge = smoothstepf(0.12f, 0.0f, std::min(ex, ey));
        float dirt = saturate(0.25f * streak + 0.45f * smoothstepf(0.45f, 0.9f, blot) + 0.6f * edge + 0.1f * f);
        Vec3 c = lerp(Vec3(0.30f, 0.38f, 0.38f), Vec3(0.30f, 0.27f, 0.20f), dirt);
        setc(p, c);
        p.a = saturate(0.16f + 0.62f * dirt);
    }
}
static void gen_paper(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float f = B(B_FBM8, x, y), g = B(B_FBM32, x, y);
        Vec3 c = Vec3(0.74f, 0.71f, 0.62f) * (0.9f + 0.14f * f + 0.05f * (g - 0.5f));
        int row = y / 20, ly = y % 20;
        if (ly >= 8 && ly < 12 && row % 13 != 0 && row % 13 != 7) {
            float len = 0.45f + 0.5f * hash2f(row, 3);
            float ux = x / (float)N;
            float word = hash2f(x / (10 + (int)(hash2f(x / 40, row) * 22)), row * 7);
            if (ux > 0.08f && ux < 0.08f + len * 0.84f && word > 0.2f) c = lerp(c, Vec3(0.12f, 0.12f, 0.14f), 0.7f);
        }
        float dx = x - 360.0f, dy = y - 150.0f;
        float rr = std::sqrt(dx * dx + dy * dy);
        float cring = smoothstepf(5.0f, 0.0f, std::fabs(rr - 52.0f)) * (0.5f + 0.5f * B(B_FBM16, x, y));
        c = lerp(c, Vec3(0.40f, 0.27f, 0.13f), cring * 0.55f);
        float fold = smoothstepf(2.0f, 0.0f, std::fabs((float)y - 256.0f));
        c = c * (1.0f - 0.15f * fold);
        float stain = smoothstepf(0.65f, 0.9f, B(B_FBM2, x, y));
        c = lerp(c, Vec3(0.5f, 0.42f, 0.27f), stain * 0.4f);
        setc(p, c);
        p.a = 0.5f + 0.05f * (g - 0.5f) - 0.1f * fold;
    }
}
static void gen_tarp(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float weave = (((x & 3) < 2) ? 0.5f : 0.0f) + (((y & 3) < 2) ? 0.5f : 0.0f);
        float fold = B(B_FBM4, x, y), fold2 = B(B_RIDGE4, x, y), st = B(B_FBM2, x, y);
        float v = 0.52f * (0.78f + 0.12f * weave) * (0.7f + 0.5f * fold);
        v *= 1.0f - 0.3f * smoothstepf(0.8f, 0.97f, fold2);
        Vec3 c(v, v, v);
        c = lerp(c, Vec3(0.2f, 0.17f, 0.12f), smoothstepf(0.55f, 0.9f, st) * 0.5f);
        setc(p, c);
        p.a = saturate(0.2f + 0.6f * fold + 0.05f * weave - 0.2f * smoothstepf(0.8f, 0.97f, fold2));
    }
}
static void gen_hazard(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        bool yel = ((x + y) & 127) < 64;
        float wear = B(B_FBM32, x * 2, y) * 0.5f + B(B_FBM8, x, y) * 0.5f, dirt = B(B_FBM2, x, y);
        Vec3 c = yel ? Vec3(0.78f, 0.58f, 0.05f) : Vec3(0.05f, 0.05f, 0.05f);
        c = c * (0.75f + 0.35f * B(B_FBM16, x, y));
        float w = smoothstepf(0.62f, 0.7f, wear);
        c = lerp(c, Vec3(0.30f, 0.29f, 0.27f), w * 0.85f);
        c = c * (1.0f - 0.45f * smoothstepf(0.4f, 0.9f, dirt));
        setc(p, c);
        p.a = saturate(0.6f - 0.15f * w);
    }
}
static void gen_locker(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float f = B(B_FBM8, x, y), fine = B(B_FBM32, x, y);
        float v = 0.74f * (0.88f + 0.16f * f);
        float h = 0.6f + 0.1f * (B(B_FBM4, x, y) - 0.5f);
        float e = (float)std::min(std::min(x, N - 1 - x), std::min(y, N - 1 - y));
        float gap = smoothstepf(5.0f, 2.0f, e);
        v *= 1.0f - 0.8f * gap; h -= 0.5f * gap;
        float bevel = smoothstepf(14.0f, 6.0f, e) * (1.0f - gap);
        v *= 1.0f + 0.1f * bevel;
        // louvers
        if (x > 120 && x < 392) {
            for (int k = 0; k < 5; k++) {
                float ly = 56.0f + k * 22.0f;
                float d = (float)y - ly;
                if (d > 0 && d < 9) { v *= 0.25f; h -= 0.4f; }
                else if (d >= 9 && d < 13) { v *= 1.15f; h += 0.15f; }
            }
            for (int k = 0; k < 3; k++) {
                float ly = 418.0f + k * 22.0f;
                float d = (float)y - ly;
                if (d > 0 && d < 9) { v *= 0.25f; h -= 0.4f; }
                else if (d >= 9 && d < 13) { v *= 1.15f; h += 0.15f; }
            }
        }
        // handle recess
        if (x > 400 && x < 460 && y > 220 && y < 300) {
            v *= 0.55f; h -= 0.3f;
            if (x > 418 && x < 442 && y > 232 && y < 288) { v *= 2.2f; h += 0.5f; }
        }
        // name plate holder
        if (x > 196 && x < 316 && y > 176 && y < 212) {
            bool rim = x < 200 || x > 312 || y < 180 || y > 208;
            v = rim ? 0.3f : 0.62f;
        }
        Vec3 c(v, v, v);
        float rust = smoothstepf(430.0f, 512.0f, (float)y) * smoothstepf(0.35f, 0.7f, B(B_FBM8B, x, y)) +
                     smoothstepf(0.8f, 0.92f, B(B_FBM4B, x, y) * 0.7f + fine * 0.3f);
        c = lerp(c, Vec3(0.30f, 0.16f, 0.08f), saturate(rust) * 0.85f);
        float scr = smoothstepf(0.95f, 0.99f, B(B_RIDGE4, x * 2, y * 2));
        c = c * (1.0f - 0.3f * scr);
        setc(p, c);
        p.a = saturate(h);
    }
}
static void gen_screen(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float u = x / (float)N - 0.5f, v = y / (float)N - 0.5f;
        float vig = 1.0f - saturate((u * u + v * v) * 2.2f);
        float scan = (y & 3) < 2 ? 1.0f : 0.72f;
        float refl = smoothstepf(0.22f, 0.0f, std::fabs(u + v * 0.6f + 0.18f)) * 0.07f;
        Vec3 c = Vec3(0.022f, 0.04f, 0.032f) * (0.5f + 0.8f * vig) * scan + Vec3(refl, refl, refl * 1.1f);
        setc(p, c);
        p.a = 0.5f;
    }
}
static void gen_plain(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float v = 0.76f * (0.94f + 0.08f * B(B_FBM8, x, y) + 0.04f * (B(B_FBM32, x, y) - 0.5f));
        v *= 1.0f - 0.12f * smoothstepf(0.6f, 0.95f, B(B_FBM2, x, y));
        setc(p, Vec3(v, v, v));
        p.a = 0.5f + 0.08f * (B(B_FBM32, x, y) - 0.5f);
    }
}
static void gen_gravel(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float w = B(B_WOR14, x * 2, y * 2), f = B(B_FBM16, x, y), big = B(B_FBM2, x, y);
        float stone = smoothstepf(0.45f, 0.1f, w);
        float tone = B(B_FBM32, x * 3 + 5, y * 3 + 9);
        Vec3 sc = lerp(Vec3(0.20f, 0.19f, 0.18f), Vec3(0.40f, 0.38f, 0.35f), tone);
        Vec3 dirt = Vec3(0.10f, 0.085f, 0.065f) * (0.7f + 0.6f * f);
        Vec3 c = lerp(dirt, sc, stone) * (0.7f + 0.5f * big);
        setc(p, c);
        p.a = saturate(0.2f + 0.7f * stone);
    }
}
static void gen_rail(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float scr = B(B_FBM8, x, y * 8) * 0.5f + B(B_FBM32, x, y * 4) * 0.5f;
        float v = 0.34f + 0.3f * scr;
        float e = std::min(y, N - 1 - y) / (float)N;
        float rust = smoothstepf(0.22f, 0.05f, e) * (0.5f + 0.5f * B(B_FBM16, x, y)) + smoothstepf(0.7f, 0.9f, B(B_FBM4, x, y)) * 0.5f;
        Vec3 c(v, v, v * 1.04f);
        c = lerp(c, Vec3(0.30f, 0.15f, 0.07f), saturate(rust) * 0.9f);
        setc(p, c);
        p.a = saturate(0.6f + 0.15f * (scr - 0.5f) - 0.2f * saturate(rust));
    }
}
static void gen_cardboard(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float f = B(B_FBM8, x, y), fine = B(B_FBM32, x, y);
        Vec3 c = Vec3(0.44f, 0.33f, 0.20f) * (0.85f + 0.2f * f + 0.08f * (fine - 0.5f));
        if ((x & 7) == 0) c = c * 0.93f;
        if (y > 226 && y < 286) {
            c = lerp(c, Vec3(0.52f, 0.44f, 0.30f), 0.75f);
            if (y < 229 || y > 283) c = c * 0.8f;
        }
        float stain = smoothstepf(0.6f, 0.85f, B(B_FBM2, x, y));
        c = c * (1.0f - 0.4f * stain);
        float e = (float)std::min(std::min(x, N - 1 - x), std::min(y, N - 1 - y));
        c = c * (1.0f - 0.35f * smoothstepf(8.0f, 0.0f, e));
        setc(p, c);
        p.a = 0.5f + 0.1f * (f - 0.5f);
    }
}
static void gen_machine(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float dim = B(B_WOR14, x * 2, y * 2), f = B(B_FBM8, x, y);
        float v = 0.66f * (0.93f + 0.09f * smoothstepf(0.0f, 0.5f, dim)) * (0.9f + 0.14f * f);
        float h = 0.45f + 0.3f * smoothstepf(0.0f, 0.5f, dim);
        int sx = x & 255, sy = y & 255;
        float e = (float)std::min(std::min(sx, 255 - sx), std::min(sy, 255 - sy));
        float seam = smoothstepf(3.0f, 1.0f, e);
        v *= 1.0f - 0.7f * seam; h -= 0.4f * seam;
        // corner bolts
        float bx = std::min(std::fabs(sx - 18.0f), std::fabs(sx - 237.0f)), by = std::min(std::fabs(sy - 18.0f), std::fabs(sy - 237.0f));
        float bd = std::sqrt(bx * bx + by * by);
        float bolt = smoothstepf(8.0f, 6.0f, bd);
        if (bolt > 0) { v = lerpf(v, 0.42f + 0.25f * smoothstepf(6.0f, 0.0f, bd), bolt); h = lerpf(h, 0.95f, bolt); }
        // louvre block in upper-left sub panel
        if (x > 56 && x < 200 && y > 60 && y < 200 && (y % 20) < 8) { v *= 0.3f; h -= 0.3f; }
        Vec3 c(v, v, v);
        float oil = smoothstepf(0.55f, 0.85f, B(B_FBM4, x * 4, y)) * smoothstepf(0.3f, 0.8f, y / (float)N);
        c = c * (1.0f - 0.5f * oil);
        float rust = smoothstepf(0.74f, 0.9f, B(B_FBM4B, x, y) * 0.7f + B(B_FBM32, x, y) * 0.3f);
        c = lerp(c, Vec3(0.30f, 0.16f, 0.08f), rust * 0.8f);
        setc(p, c);
        p.a = saturate(h);
    }
}
static void gen_contdoor(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float t = (y & 127) / 128.0f;
        float hrib = smoothstepf(0.1f, 0.2f, t) - smoothstepf(0.8f, 0.9f, t);
        float v = 0.68f + 0.1f * hrib;
        float h = 0.35f + 0.2f * hrib;
        v *= 0.86f + 0.2f * B(B_FBM4, x * 8, y);
        Vec3 c(v, v, v);
        // centre seam + edges
        float seam = smoothstepf(4.0f, 1.0f, std::fabs((float)x - 256.0f));
        float edge = smoothstepf(8.0f, 2.0f, (float)std::min(x, N - 1 - x));
        c = c * (1.0f - 0.75f * std::max(seam, edge));
        h -= 0.3f * std::max(seam, edge);
        // lock rods
        static const float rods[4] = {84.0f, 176.0f, 336.0f, 428.0f};
        for (int k = 0; k < 4; k++) {
            float d = (float)x - rods[k];
            if (std::fabs(d) < 9.0f) {
                float sh = std::sqrt(1.0f - (d / 9.0f) * (d / 9.0f));
                float rv = 0.35f + 0.5f * sh * (0.6f + 0.4f * (d < 0 ? 1.0f : 0.5f));
                c = Vec3(rv, rv, rv * 1.02f);
                h = 0.6f + 0.4f * sh;
                // brackets
                if ((y % 170) > 20 && (y % 170) < 44) { c = c * 0.6f; h = 0.95f; }
            }
            // handles
            if (y > 262 && y < 282 && d > -4 && d < 52 && (k == 1 || k == 2)) { c = Vec3(0.5f, 0.5f, 0.52f); h = 0.9f; }
            if (y > 262 && y < 282 && d > -52 && d < 4 && (k == 0 || k == 3)) { c = Vec3(0.5f, 0.5f, 0.52f); h = 0.9f; }
        }
        // placard
        if (x > 276 && x < 322 && y > 70 && y < 140) {
            c = Vec3(0.78f, 0.76f, 0.70f);
            if (((y - 70) % 10) > 5 && x > 282 && x < 282 + (int)(34 * hash2f(y / 10, 4))) c = Vec3(0.15f, 0.15f, 0.15f);
        }
        float rust = smoothstepf(380.0f, 512.0f, (float)y) * smoothstepf(0.3f, 0.7f, B(B_FBM8, x, y)) +
                     smoothstepf(0.7f, 0.86f, B(B_FBM4B, x * 2, y) * 0.7f + B(B_FBM16, x, y) * 0.3f);
        c = lerp(c, lerp(Vec3(0.36f, 0.19f, 0.09f), Vec3(0.2f, 0.1f, 0.06f), B(B_FBM32, x, y)), saturate(rust) * 0.85f);
        setc(p, c);
        p.a = saturate(h);
    }
}
static void gen_roof(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float f = B(B_FBM16, x, y), g = B(B_WHITE, x, y);
        float v = 0.05f + 0.04f * f + (g > 0.95f ? 0.1f : 0.0f);
        int sy = y & 127;
        float seam = smoothstepf(4.0f, 1.0f, (float)std::min(sy, 127 - sy));
        v += 0.03f * seam;
        float pool = smoothstepf(0.6f, 0.8f, B(B_FBM2, x, y));
        v *= 1.0f - 0.4f * pool;
        setc(p, Vec3(v, v, v * 1.05f));
        p.a = saturate(0.5f + 0.3f * (f - 0.5f) + 0.2f * seam);
    }
}
static void gen_kelp(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float f = B(B_FBM8, x, y), s = B(B_FBM4, x * 8, y), w = B(B_WOR14, x * 2, y * 2), reg = B(B_FBM4B, x, y);
        Vec3 c = lerp(Vec3(0.035f, 0.06f, 0.035f), Vec3(0.13f, 0.15f, 0.07f), f);
        c = lerp(c, Vec3(0.02f, 0.03f, 0.02f), smoothstepf(0.5f, 0.85f, s) * 0.8f);
        float barn = smoothstepf(0.3f, 0.16f, w) * smoothstepf(0.5f, 0.62f, reg);
        float core = smoothstepf(0.1f, 0.03f, w) * smoothstepf(0.5f, 0.62f, reg);
        c = lerp(c, Vec3(0.46f, 0.45f, 0.38f), barn * 0.9f);
        c = lerp(c, Vec3(0.05f, 0.05f, 0.04f), core);
        float rust = smoothstepf(0.7f, 0.9f, B(B_FBM2, x, y));
        c = lerp(c, Vec3(0.25f, 0.12f, 0.05f), rust * 0.6f * (1.0f - barn));
        setc(p, c);
        p.a = saturate(0.35f + 0.3f * (f - 0.5f) + 0.5f * barn - 0.3f * core);
    }
}
static void gen_oilskin(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        float fold = B(B_FBM4, x * 4, y), f = B(B_FBM16, x, y), r = B(B_RIDGE4, x * 2, y);
        float v = 0.035f + 0.05f * fold + 0.02f * f;
        float hi = smoothstepf(0.85f, 0.98f, r) * 0.1f;
        Vec3 c(v + hi, v * 1.04f + hi, v * 1.12f + hi);
        int sx = x & 255;
        float seam = smoothstepf(3.0f, 1.0f, (float)std::min(sx, 255 - sx));
        c = c * (1.0f - 0.5f * seam);
        float mud = smoothstepf(330.0f, 512.0f, (float)y) * B(B_FBM8, x, y);
        c = lerp(c, Vec3(0.09f, 0.07f, 0.045f), mud * 0.8f);
        setc(p, c);
        p.a = saturate(0.3f + 0.5f * fold - 0.2f * seam);
    }
}
static void gen_blinds(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        int sl = y / 32;
        float t = (y & 31) / 32.0f;
        float bend = (hash2f(sl, 9) > 0.8f) ? (B(B_FBM4, x, sl * 37) - 0.5f) * 0.4f : 0.0f;
        float tt = saturate(t + bend);
        float v = 0.62f * (1.0f - 0.45f * tt);
        float gap = smoothstepf(0.86f, 0.94f, tt);
        v *= 1.0f - 0.92f * gap;
        Vec3 c(v, v * 0.97f, v * 0.86f);
        c = c * (0.8f + 0.3f * B(B_FBM8, x, y));
        if (std::abs(x - 96) < 2 || std::abs(x - 416) < 2) c = c * 0.45f;
        setc(p, c);
        p.a = saturate(0.7f - 0.5f * tt);
    }
}
static void gen_noise(Img& im) {
    FORXY {
        Px& p = im.at(x, y);
        // stored through an sRGB texture: pre-compensate so the shader reads linear values
        p.r = std::pow(B(B_FBM4, x, y), 1.0f / 2.2f);
        p.g = std::pow(B(B_FBM16, x, y), 1.0f / 2.2f);
        p.b = std::pow(B(B_WHITE, x, y), 1.0f / 2.2f);
        p.a = B(B_FBM32, x, y);
    }
}

// ---------------------------------------------------------------------------
// Signs, posters and decals
// ---------------------------------------------------------------------------
enum { SK_BOARD, SK_STENCIL, SK_CHALK, SK_CUSTOM };
struct SignDef {
    const char* name;
    int w, h;
    int kind;
    u32 bg, fg;
    const char* text;
    void (*fn)(Img&, int, int, int, int);
};

static void weatherRect(Img& im, int x0, int y0, int w, int h, float amt) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            Px& p = im.at(x, y);
            if (p.a <= 0.0f) continue;
            float streak = B(B_FBM4, x * 8, y + x0), blot = B(B_FBM4B, x + y0, y), fine = B(B_FBM32, x, y);
            float e = (float)std::min(std::min(x - x0, x0 + w - 1 - x), std::min(y - y0, y0 + h - 1 - y));
            float rust = smoothstepf(0.66f, 0.82f, blot * 0.7f + fine * 0.3f) + smoothstepf(7.0f, 0.0f, e) * smoothstepf(0.3f, 0.7f, fine);
            mixc(p, Vec3(0.30f, 0.16f, 0.08f), saturate(rust) * 0.8f * amt);
            mulc(p, 1.0f - 0.4f * amt * streak);
        }
}
static void boltHoles(Img& im, int x0, int y0, int w, int h) {
    float m = std::max(5.0f, std::min(w, h) * 0.07f);
    float r = std::max(1.8f, std::min(w, h) * 0.022f);
    const float px[2] = {x0 + m, x0 + w - m}, py[2] = {y0 + m, y0 + h - m};
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) {
            disc(im, px[i], py[j], r + 1.0f, Vec3(0.45f, 0.45f, 0.45f));
            disc(im, px[i], py[j], r * 0.6f, Vec3(0.12f, 0.1f, 0.08f));
        }
}

// --- custom drawers ---------------------------------------------------------
static void s_oil(Img& im, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            float u = (x - x0 + 0.5f) / w * 2 - 1, v = (y - y0 + 0.5f) / h * 2 - 1;
            float r = std::sqrt(u * u + v * v) + (B(B_FBM8, x * 2, y * 2) - 0.5f) * 0.7f;
            float a = smoothstepf(0.8f, 0.35f, r);
            float sheen = B(B_FBM16, x, y);
            Px& p = im.at(x, y);
            p.r = 0.015f + 0.03f * sheen; p.g = 0.014f + 0.02f * sheen; p.b = 0.02f + 0.04f * sheen; p.a = a * 0.86f;
        }
}
static void s_puddle(Img& im, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            float u = (x - x0 + 0.5f) / w * 2 - 1, v = (y - y0 + 0.5f) / h * 2 - 1;
            float r = std::sqrt(u * u + v * v) + (B(B_FBM4, x * 3, y * 3) - 0.5f) * 0.6f;
            Px& p = im.at(x, y);
            p.r = 0.02f; p.g = 0.023f; p.b = 0.03f; p.a = smoothstepf(0.85f, 0.5f, r) * 0.7f;
        }
}
static void s_scorch(Img& im, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            float u = (x - x0 + 0.5f) / w * 2 - 1, v = (y - y0 + 0.5f) / h * 2 - 1;
            float r = std::sqrt(u * u + v * v) + (B(B_FBM16, x * 2, y * 2) - 0.5f) * 0.5f;
            Px& p = im.at(x, y);
            p.r = 0.02f; p.g = 0.018f; p.b = 0.015f; p.a = smoothstepf(0.9f, 0.1f, r) * 0.9f;
        }
}
static void s_crack(Img& im, int x0, int y0, int w, int h) {
    Rng rng(4411);
    for (int k = 0; k < 3; k++) {
        float x = x0 + 6.0f, y = y0 + h * (0.25f + 0.25f * k);
        for (int i = 0; i < 40; i++) {
            float nx = x + (w - 12) / 40.0f, ny = clampv(y + rng.range(-5.0f, 5.0f), y0 + 4.0f, y0 + h - 4.0f);
            line(im, x, y, nx, ny, rng.range(1.0f, 2.6f), Vec3(0.01f, 0.01f, 0.01f), 0.85f);
            if (rng.chance(0.12f)) line(im, x, y, x + rng.range(-10.0f, 10.0f), y + rng.range(-16.0f, 16.0f), 1.2f, Vec3(0.01f, 0.01f, 0.01f), 0.7f);
            x = nx; y = ny;
        }
    }
}
static void bootPrint(Img& im, int x0, int y0, int w, int h, bool left) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            float u = (x - x0 + 0.5f) / w * 2 - 1, v = (y - y0 + 0.5f) / h;
            if (left) u = -u;
            float a = 0;
            // sole
            float su = (u - 0.08f * std::sin(v * 3.0f)) / 0.62f, sv = (v - 0.32f) / 0.30f;
            if (su * su + sv * sv < 1.0f) a = 1.0f;
            // heel
            float hu = u / 0.5f, hv = (v - 0.82f) / 0.15f;
            if (hu * hu + hv * hv < 1.0f) a = 1.0f;
            if (a > 0) {
                int tread = (int)(v * 22.0f);
                if (tread & 1) a *= 0.35f;
                a *= 0.55f + 0.45f * B(B_FBM32, x * 2, y * 2);
            }
            Px& p = im.at(x, y);
            p.r = 0.02f; p.g = 0.024f; p.b = 0.03f; p.a = a * 0.78f;
        }
}
static void s_bootL(Img& im, int x0, int y0, int w, int h) { bootPrint(im, x0, y0, w, h, true); }
static void s_bootR(Img& im, int x0, int y0, int w, int h) { bootPrint(im, x0, y0, w, h, false); }
static void s_hand(Img& im, int x0, int y0, int w, int h) {
    Vec3 c(0.03f, 0.035f, 0.04f);
    float cx = x0 + w * 0.5f, cy = y0 + h * 0.68f;
    disc(im, cx, cy, w * 0.27f, c, 0.7f);
    disc(im, cx, cy + h * 0.1f, w * 0.22f, c, 0.7f);
    const float ang[5] = {-1.05f, -0.42f, -0.08f, 0.26f, 0.62f};
    const float len[5] = {0.30f, 0.46f, 0.52f, 0.47f, 0.36f};
    for (int i = 0; i < 5; i++) {
        float ax = std::sin(ang[i]), ay = -std::cos(ang[i]);
        float sx = cx + ax * w * 0.2f, sy = cy + ay * w * 0.2f;
        // long, too-thin fingers
        line(im, sx, sy, sx + ax * h * len[i], sy + ay * h * len[i], w * 0.085f, c, 0.7f);
    }
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            Px& p = im.at(x, y);
            p.a *= 0.45f + 0.55f * B(B_FBM32, x * 2, y * 2);
            // drips
            if (p.a > 0.2f && hash2f(x / 3, 91) > 0.82f) {
                for (int d = 1; d < 30 && y + d < y0 + h; d++) {
                    Px& q = im.at(x, y + d);
                    q.r = c.x; q.g = c.y; q.b = c.z;
                    q.a = std::max(q.a, 0.4f * (1.0f - d / 30.0f));
                }
            }
        }
}
static void s_drag(Img& im, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            float u = (x - x0 + 0.5f) / w, v = (y - y0 + 0.5f) / h;
            float a = smoothstepf(0.14f, 0.0f, std::fabs(u - 0.3f + 0.04f * std::sin(v * 9.0f))) +
                      smoothstepf(0.14f, 0.0f, std::fabs(u - 0.7f + 0.04f * std::sin(v * 7.0f + 1.0f)));
            a *= (0.3f + 0.7f * B(B_FBM4, x * 8, y)) * smoothstepf(0.0f, 0.1f, v) * smoothstepf(1.0f, 0.8f, v);
            Px& p = im.at(x, y);
            p.r = 0.02f; p.g = 0.024f; p.b = 0.028f; p.a = saturate(a) * 0.75f;
        }
}
static void laneCommon(Img& im, int x0, int y0, int w, int h, const Vec3& col) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            float u = (x - x0 + 0.5f) / w;
            float a = smoothstepf(0.0f, 0.12f, u) * smoothstepf(1.0f, 0.88f, u);
            float wear = B(B_FBM32, x * 2, y) * 0.5f + B(B_FBM8, x * 3, y) * 0.6f;
            a *= smoothstepf(0.36f, 0.56f, wear);
            Px& p = im.at(x, y);
            setc(p, col * (0.7f + 0.3f * B(B_FBM16, x, y)));
            p.a = a * 0.9f;
        }
}
static void s_lane(Img& im, int x0, int y0, int w, int h) { laneCommon(im, x0, y0, w, h, Vec3(0.62f, 0.62f, 0.58f)); }
static void s_laneY(Img& im, int x0, int y0, int w, int h) { laneCommon(im, x0, y0, w, h, Vec3(0.70f, 0.52f, 0.06f)); }
static void s_hatch(Img& im, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            int lx = x - x0, ly = y - y0;
            int e = std::min(std::min(lx, w - 1 - lx), std::min(ly, h - 1 - ly));
            bool on = e < 12 || ((lx + ly) % 48) < 12;
            float wear = B(B_FBM32, x * 2, y) * 0.5f + B(B_FBM8, x * 2, y * 2) * 0.6f;
            Px& p = im.at(x, y);
            setc(p, Vec3(0.74f, 0.56f, 0.06f) * (0.7f + 0.3f * B(B_FBM16, x, y)));
            p.a = on ? smoothstepf(0.34f, 0.54f, wear) * 0.92f : 0.0f;
        }
}
static void s_drip(Img& im, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            float u = (x - x0 + 0.5f) / w, v = (y - y0 + 0.5f) / h;
            float s = B(B_FBM16, x * 4, y0) * 0.6f + B(B_FBM32, x * 2, y0 + 7) * 0.4f;
            float a = smoothstepf(0.35f, 0.75f, s) * smoothstepf(1.0f, 0.1f, v + (1.0f - s) * 0.4f);
            a *= smoothstepf(0.0f, 0.15f, u) * smoothstepf(1.0f, 0.85f, u);
            Px& p = im.at(x, y);
            setc(p, lerp(Vec3(0.28f, 0.13f, 0.05f), Vec3(0.12f, 0.06f, 0.03f), B(B_FBM32, x, y)));
            p.a = saturate(a) * 0.85f;
        }
}
static void s_grime(Img& im, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            float v = (y - y0 + 0.5f) / h;
            float s = B(B_FBM16, x * 2, y) * 0.5f + B(B_FBM4, x * 4, y) * 0.5f;
            Px& p = im.at(x, y);
            setc(p, Vec3(0.03f, 0.03f, 0.025f));
            p.a = saturate(smoothstepf(0.15f, 1.0f, v + (s - 0.5f) * 0.7f)) * 0.8f;
        }
}
static void s_tally(Img& im, int x0, int y0, int w, int h) {
    Rng rng(771);
    Vec3 c(0.82f, 0.80f, 0.74f);
    float x = x0 + 10.0f;
    for (int g = 0; g < 9 && x < x0 + w - 44; g++) {
        float gy = y0 + 8.0f + (g % 2) * 2.0f;
        int n = (g == 8) ? 3 : 4;
        for (int i = 0; i < n; i++) {
            float lx = x + i * 7.0f;
            line(im, lx + rng.range(-1.5f, 1.5f), gy + rng.range(0.0f, 4.0f), lx + rng.range(-1.5f, 1.5f), gy + h - 20.0f + rng.range(-3.0f, 3.0f), 2.4f, c, 0.8f);
        }
        if (g != 8) line(im, x - 5.0f, gy + h - 26.0f, x + 28.0f, gy + 8.0f, 2.4f, c, 0.8f);
        x += 44.0f;
    }
    for (int y = y0; y < y0 + h; y++)
        for (int xx = x0; xx < x0 + w; xx++) im.at(xx, y).a *= 0.45f + 0.55f * B(B_WHITE, xx, y);
}
static void s_calendar(Img& im, int x0, int y0, int w, int h) {
    Vec3 paper(0.80f, 0.77f, 0.66f), ink(0.10f, 0.10f, 0.13f), red(0.62f, 0.07f, 0.05f);
    fillRect(im, x0, y0, x0 + w, y0 + h, paper);
    fillRect(im, x0, y0, x0 + w, y0 + h * 0.2f, Vec3(0.45f, 0.10f, 0.08f));
    drawTextC(im, FONT_SANS, "NOVEMBER 1982", x0 + w * 0.5f, y0 + h * 0.145f, h * 0.115f, paper);
    const char* dn[7] = {"S", "M", "T", "W", "T", "F", "S"};
    float gx = x0 + w * 0.05f, gy = y0 + h * 0.3f, cw = w * 0.9f / 7.0f, chh = h * 0.66f / 5.0f;
    for (int i = 0; i < 7; i++) drawTextC(im, FONT_SANS, dn[i], gx + cw * (i + 0.5f), y0 + h * 0.285f, h * 0.055f, ink, 0.8f);
    for (int d = 1; d <= 30; d++) {
        int idx = d;           // 1 Nov 1982 was a Monday -> column 1
        int col = idx % 7, row = idx / 7;
        float cx = gx + cw * (col + 0.5f), cy = gy + chh * (row + 0.5f);
        strokeRect(im, gx + cw * col, gy + chh * row, gx + cw * (col + 1), gy + chh * (row + 1), 1.0f, ink, 0.35f);
        char b[16];
        snprintf(b, sizeof(b), "%d", d);
        drawTextC(im, FONT_SANS, b, cx, cy + h * 0.025f, h * 0.07f, ink, 0.9f);
        if (d < 11) {
            line(im, cx - cw * 0.34f, cy - chh * 0.3f, cx + cw * 0.34f, cy + chh * 0.3f, 2.0f, ink, 0.75f);
            if (d >= 8) line(im, cx + cw * 0.34f, cy - chh * 0.3f, cx - cw * 0.34f, cy + chh * 0.3f, 2.0f, red, 0.85f);
        }
        if (d == 11) { ring(im, cx, cy, cw * 0.42f, 3.0f, red, 0.9f); ring(im, cx + 1.5f, cy - 1.0f, cw * 0.46f, 2.0f, red, 0.7f); }
    }
    weatherRect(im, x0, y0, w, h, 0.25f);
}
static void s_clock(Img& im, int x0, int y0, int w, int h) {
    float cx = x0 + w * 0.5f, cy = y0 + h * 0.5f, r = w * 0.47f;
    disc(im, cx, cy, r, Vec3(0.12f, 0.12f, 0.12f));
    disc(im, cx, cy, r * 0.9f, Vec3(0.72f, 0.70f, 0.60f));
    for (int i = 0; i < 12; i++) {
        float a = i * TAU / 12.0f;
        line(im, cx + std::sin(a) * r * 0.72f, cy - std::cos(a) * r * 0.72f, cx + std::sin(a) * r * 0.84f, cy - std::cos(a) * r * 0.84f,
             (i % 3 == 0) ? 4.0f : 2.0f, Vec3(0.08f, 0.08f, 0.08f));
    }
    // stopped at 3:07
    float ha = (3.0f + 7.0f / 60.0f) / 12.0f * TAU, ma = 7.0f / 60.0f * TAU;
    line(im, cx, cy, cx + std::sin(ha) * r * 0.45f, cy - std::cos(ha) * r * 0.45f, 5.0f, Vec3(0.05f, 0.05f, 0.05f));
    line(im, cx, cy, cx + std::sin(ma) * r * 0.7f, cy - std::cos(ma) * r * 0.7f, 3.5f, Vec3(0.05f, 0.05f, 0.05f));
    line(im, cx, cy, cx + std::sin(4.1f) * r * 0.74f, cy - std::cos(4.1f) * r * 0.74f, 1.5f, Vec3(0.6f, 0.08f, 0.05f));
    disc(im, cx, cy, 4.0f, Vec3(0.05f, 0.05f, 0.05f));
    // crack in the glass
    line(im, cx - r * 0.5f, cy - r * 0.7f, cx + r * 0.1f, cy + r * 0.05f, 1.2f, Vec3(0.9f, 0.9f, 0.9f), 0.5f);
    line(im, cx + r * 0.1f, cy + r * 0.05f, cx + r * 0.6f, cy + r * 0.5f, 1.2f, Vec3(0.9f, 0.9f, 0.9f), 0.5f);
    line(im, cx + r * 0.1f, cy + r * 0.05f, cx - r * 0.2f, cy + r * 0.7f, 1.0f, Vec3(0.9f, 0.9f, 0.9f), 0.4f);
}
static void gaugeFace(Img& im, int x0, int y0, int w, int h, float needle, const char* label) {
    float cx = x0 + w * 0.5f, cy = y0 + h * 0.5f, r = w * 0.47f;
    disc(im, cx, cy, r, Vec3(0.2f, 0.2f, 0.2f));
    disc(im, cx, cy, r * 0.88f, Vec3(0.75f, 0.73f, 0.64f));
    for (int i = 0; i <= 20; i++) {
        float a = (-0.75f + 1.5f * i / 20.0f) * PI;
        Vec3 c = i >= 15 ? Vec3(0.65f, 0.08f, 0.05f) : Vec3(0.08f, 0.08f, 0.08f);
        line(im, cx + std::sin(a) * r * 0.66f, cy - std::cos(a) * r * 0.66f, cx + std::sin(a) * r * 0.8f, cy - std::cos(a) * r * 0.8f,
             (i % 5 == 0) ? 3.0f : 1.4f, c);
    }
    float a = (-0.75f + 1.5f * needle) * PI;
    line(im, cx, cy, cx + std::sin(a) * r * 0.72f, cy - std::cos(a) * r * 0.72f, 2.6f, Vec3(0.6f, 0.06f, 0.04f));
    disc(im, cx, cy, 5.0f, Vec3(0.1f, 0.1f, 0.1f));
    drawTextC(im, FONT_SANS, label, cx, cy + r * 0.55f, h * 0.1f, Vec3(0.1f, 0.1f, 0.1f));
}
static void s_gauge(Img& im, int x0, int y0, int w, int h) { gaugeFace(im, x0, y0, w, h, 0.93f, "PSI"); }
static void s_gauge2(Img& im, int x0, int y0, int w, int h) { gaugeFace(im, x0, y0, w, h, 0.04f, "AMPS"); }
static void s_keypad(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.16f, 0.16f, 0.15f));
    strokeRect(im, x0 + 2, y0 + 2, x0 + w - 2, y0 + h - 2, 2.0f, Vec3(0.4f, 0.4f, 0.38f));
    fillRect(im, x0 + w * 0.12f, y0 + h * 0.06f, x0 + w * 0.88f, y0 + h * 0.2f, Vec3(0.05f, 0.02f, 0.02f));
    drawTextC(im, FONT_MONO, "----", x0 + w * 0.5f, y0 + h * 0.165f, h * 0.1f, Vec3(0.7f, 0.1f, 0.05f));
    const char* k[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
    for (int i = 0; i < 12; i++) {
        int c = i % 3, r = i / 3;
        float bx = x0 + w * (0.14f + 0.26f * c), by = y0 + h * (0.27f + 0.175f * r);
        fillRect(im, bx, by, bx + w * 0.2f, by + h * 0.13f, Vec3(0.55f, 0.54f, 0.48f));
        fillRect(im, bx, by + h * 0.115f, bx + w * 0.2f, by + h * 0.13f, Vec3(0.3f, 0.3f, 0.27f));
        drawTextC(im, FONT_SANS, k[i], bx + w * 0.1f, by + h * 0.098f, h * 0.085f, Vec3(0.08f, 0.08f, 0.08f));
    }
}
static void s_fusebox(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.20f, 0.21f, 0.20f));
    strokeRect(im, x0 + 3, y0 + 3, x0 + w - 3, y0 + h - 3, 3.0f, Vec3(0.09f, 0.09f, 0.09f));
    drawTextC(im, FONT_SANS, "YARD LIGHTING - 30A", x0 + w * 0.5f, y0 + h * 0.14f, h * 0.085f, Vec3(0.75f, 0.72f, 0.6f));
    const char* lab[3] = {"L1", "L2", "L3"};
    for (int i = 0; i < 3; i++) {
        float cx = x0 + w * (0.2f + 0.3f * i), cy = y0 + h * 0.52f;
        fillRect(im, cx - w * 0.09f, cy - h * 0.26f, cx + w * 0.09f, cy + h * 0.26f, Vec3(0.07f, 0.07f, 0.07f));
        fillRect(im, cx - w * 0.06f, cy - h * 0.23f, cx + w * 0.06f, cy - h * 0.15f, Vec3(0.55f, 0.42f, 0.2f));
        fillRect(im, cx - w * 0.06f, cy + h * 0.15f, cx + w * 0.06f, cy + h * 0.23f, Vec3(0.55f, 0.42f, 0.2f));
        drawTextC(im, FONT_SANS, lab[i], cx, y0 + h * 0.93f, h * 0.08f, Vec3(0.75f, 0.72f, 0.6f));
    }
    weatherRect(im, x0, y0, w, h, 0.5f);
}
static void s_photo(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.78f, 0.76f, 0.68f));
    fillRect(im, x0 + 8, y0 + 8, x0 + w - 8, y0 + h - 34, Vec3(0.22f, 0.21f, 0.17f));
    // sky band and the yard behind the crew
    fillRect(im, x0 + 8, y0 + 8, x0 + w - 8, y0 + h * 0.36f, Vec3(0.46f, 0.44f, 0.36f));
    // something tall in the background that was not supposed to be in the picture
    line(im, x0 + w * 0.82f, y0 + 10, x0 + w * 0.82f, y0 + h * 0.52f, 5.0f, Vec3(0.14f, 0.13f, 0.11f), 0.9f);
    line(im, x0 + w * 0.5f, y0 + 20, x0 + w * 0.96f, y0 + 20, 3.0f, Vec3(0.14f, 0.13f, 0.11f), 0.9f);
    line(im, x0 + w * 0.6f, y0 + 20, x0 + w * 0.6f, y0 + 50, 1.0f, Vec3(0.14f, 0.13f, 0.11f), 0.9f);
    Rng rng(5150);
    for (int i = 0; i < 7; i++) {
        float cx = x0 + w * (0.14f + 0.12f * i), cy = y0 + h * (0.4f + 0.02f * (i % 2));
        Vec3 coat = Vec3(0.30f, 0.28f, 0.22f) * rng.range(0.6f, 1.2f);
        fillRect(im, cx - w * 0.045f, cy + h * 0.06f, cx + w * 0.045f, y0 + h - 36, coat);
        disc(im, cx, cy, h * 0.055f, Vec3(0.60f, 0.52f, 0.42f));
        fillRect(im, cx - h * 0.06f, cy - h * 0.075f, cx + h * 0.06f, cy - h * 0.03f, Vec3(0.66f, 0.56f, 0.14f));
        // every face scratched out but one
        if (i != 3)
            for (int k = 0; k < 9; k++)
                line(im, cx + rng.range(-9.0f, 9.0f), cy + rng.range(-9.0f, 9.0f), cx + rng.range(-9.0f, 9.0f), cy + rng.range(-9.0f, 9.0f), 1.6f,
                     Vec3(0.88f, 0.86f, 0.8f), 0.95f);
    }
    drawTextC(im, FONT_MONO, "BERTH 9 CREW - SUMMER '82", x0 + w * 0.5f, y0 + h - 12.0f, 14.0f, Vec3(0.1f, 0.1f, 0.14f), 0.9f);
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) mulc(im.at(x, y), 0.8f + 0.3f * B(B_FBM16, x, y));
}
static void s_blueprint(Img& im, int x0, int y0, int w, int h) {
    Vec3 bg(0.10f, 0.19f, 0.36f), ln(0.72f, 0.80f, 0.88f);
    fillRect(im, x0, y0, x0 + w, y0 + h, bg);
    for (int i = 1; i < 16; i++) line(im, x0 + w * i / 16.0f, y0, x0 + w * i / 16.0f, y0 + h, 0.6f, ln, 0.12f);
    for (int i = 1; i < 10; i++) line(im, x0, y0 + h * i / 10.0f, x0 + w, y0 + h * i / 10.0f, 0.6f, ln, 0.12f);
    strokeRect(im, x0 + 8, y0 + 8, x0 + w - 8, y0 + h - 8, 1.5f, ln, 0.9f);
    // yard outline, quay, buildings, container blocks. No crane.
    strokeRect(im, x0 + w * 0.08f, y0 + h * 0.2f, x0 + w * 0.92f, y0 + h * 0.84f, 1.5f, ln, 0.9f);
    line(im, x0 + w * 0.08f, y0 + h * 0.3f, x0 + w * 0.92f, y0 + h * 0.3f, 1.0f, ln, 0.8f);
    drawText(im, FONT_SANS, "QUAY", x0 + w * 0.44f, y0 + h * 0.275f, h * 0.045f, ln, 0.9f);
    strokeRect(im, x0 + w * 0.62f, y0 + h * 0.62f, x0 + w * 0.74f, y0 + h * 0.67f, 1.0f, ln, 0.9f);
    drawText(im, FONT_SANS, "OFFICE", x0 + w * 0.62f, y0 + h * 0.61f, h * 0.035f, ln, 0.9f);
    strokeRect(im, x0 + w * 0.70f, y0 + h * 0.4f, x0 + w * 0.84f, y0 + h * 0.52f, 1.0f, ln, 0.9f);
    drawText(im, FONT_SANS, "SHOP", x0 + w * 0.72f, y0 + h * 0.39f, h * 0.035f, ln, 0.9f);
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 4; j++)
            strokeRect(im, x0 + w * (0.12f + 0.065f * i), y0 + h * (0.37f + 0.1f * j), x0 + w * (0.17f + 0.065f * i), y0 + h * (0.44f + 0.1f * j), 0.8f, ln, 0.7f);
    drawText(im, FONT_SANS, "STACKS A-E", x0 + w * 0.13f, y0 + h * 0.8f, h * 0.035f, ln, 0.9f);
    drawText(im, FONT_SANS, "BERTH 9 SITE PLAN", x0 + 16.0f, y0 + h * 0.13f, h * 0.075f, ln, 0.95f);
    drawText(im, FONT_SANS, "REV. C  AUG 1982", x0 + 16.0f, y0 + h * 0.18f, h * 0.04f, ln, 0.8f);
    // red pen: someone circled the empty quay and wrote a question
    ring(im, x0 + w * 0.34f, y0 + h * 0.25f, h * 0.07f, 2.2f, Vec3(0.8f, 0.1f, 0.06f), 0.9f);
    drawText(im, FONT_SANS, "WHERE IS IT ON THE PLAN??", x0 + w * 0.42f, y0 + h * 0.2f, h * 0.045f, Vec3(0.85f, 0.12f, 0.08f), 0.95f, 2, 99);
    weatherRect(im, x0, y0, w, h, 0.18f);
}
static void s_poster(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.78f, 0.74f, 0.60f));
    fillRect(im, x0, y0, x0 + w, y0 + h * 0.2f, Vec3(0.62f, 0.09f, 0.06f));
    drawTextC(im, FONT_SANS, "THINK!", x0 + w * 0.5f, y0 + h * 0.155f, h * 0.14f, Vec3(0.9f, 0.86f, 0.7f));
    // simple crane pictogram with a load over a figure
    Vec3 k(0.08f, 0.08f, 0.08f);
    line(im, x0 + w * 0.2f, y0 + h * 0.62f, x0 + w * 0.2f, y0 + h * 0.27f, 5.0f, k);
    line(im, x0 + w * 0.12f, y0 + h * 0.3f, x0 + w * 0.82f, y0 + h * 0.3f, 4.0f, k);
    line(im, x0 + w * 0.66f, y0 + h * 0.3f, x0 + w * 0.66f, y0 + h * 0.4f, 1.5f, k);
    fillRect(im, x0 + w * 0.56f, y0 + h * 0.4f, x0 + w * 0.76f, y0 + h * 0.47f, k);
    disc(im, x0 + w * 0.66f, y0 + h * 0.535f, h * 0.022f, k);
    line(im, x0 + w * 0.66f, y0 + h * 0.555f, x0 + w * 0.66f, y0 + h * 0.62f, 4.0f, k);
    drawTextC(im, FONT_SANS, "LOOK UP", x0 + w * 0.5f, y0 + h * 0.76f, h * 0.12f, k);
    drawTextC(im, FONT_SANS, "AND LIVE", x0 + w * 0.5f, y0 + h * 0.88f, h * 0.12f, k);
    drawTextC(im, FONT_SANS, "OVERHEAD LOADS KILL", x0 + w * 0.5f, y0 + h * 0.955f, h * 0.05f, Vec3(0.62f, 0.09f, 0.06f));
    weatherRect(im, x0, y0, w, h, 0.3f);
    // the amendment
    drawTextC(im, FONT_SANS, "DON'T", x0 + w * 0.5f, y0 + h * 0.67f, h * 0.15f, Vec3(0.06f, 0.05f, 0.05f), 0.95f, 2, 1234);
    line(im, x0 + w * 0.12f, y0 + h * 0.93f, x0 + w * 0.9f, y0 + h * 0.8f, 4.0f, Vec3(0.06f, 0.05f, 0.05f), 0.9f);
    line(im, x0 + w * 0.12f, y0 + h * 0.8f, x0 + w * 0.9f, y0 + h * 0.93f, 4.0f, Vec3(0.06f, 0.05f, 0.05f), 0.9f);
}
static void crtCommon(Img& im, int x0, int y0, int w, int h, const char* const* lines, int n, bool red) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.01f, 0.03f, 0.015f));
    Vec3 g = red ? Vec3(1.0f, 0.25f, 0.12f) : Vec3(0.35f, 1.0f, 0.45f);
    float lh = h / (float)(n + 1);
    for (int i = 0; i < n; i++) drawText(im, FONT_MONO, lines[i], x0 + w * 0.05f, y0 + lh * (i + 1.1f), lh * 0.78f, g, 0.95f);
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            Px& p = im.at(x, y);
            if ((y & 1) == 0) mulc(p, 0.6f);
            p.g += 0.012f;
        }
}
static void s_crt1(Img& im, int x0, int y0, int w, int h) {
    static const char* const L[] = {"HARROW YARD CTL  v2.1", "11/11/82  23:47:00", "", "CRANE 7 ....... NOT FOUND", "LIFT 4 ........ IN PROGRESS",
                                    "OPERATOR ...... KESSLER W", "LOAD .......... NOT SECURED", "", "> _"};
    crtCommon(im, x0, y0, w, h, L, 9, false);
}
static void s_crt2(Img& im, int x0, int y0, int w, int h) {
    static const char* const L[] = {"PUT IT BACK", "PUT IT BACK", "PUT IT BACK", "PUT IT BACK", "PUT IT BACK", "PUT IT BACK", "PUT IT BACK", "RELIEF IS HERE", "> _"};
    crtCommon(im, x0, y0, w, h, L, 9, true);
}
static void s_news(Img& im, int x0, int y0, int w, int h) {
    Vec3 paper(0.68f, 0.65f, 0.55f), ink(0.09f, 0.09f, 0.1f);
    fillRect(im, x0, y0, x0 + w, y0 + h, paper);
    drawTextC(im, FONT_SANS, "ASHGROVE HARBOR GAZETTE", x0 + w * 0.5f, y0 + h * 0.075f, h * 0.058f, ink);
    line(im, x0 + 6, y0 + h * 0.095f, x0 + w - 6, y0 + h * 0.095f, 2.0f, ink);
    drawTextC(im, FONT_MONO, "THURSDAY NOV 11 1982 - 20c", x0 + w * 0.5f, y0 + h * 0.135f, h * 0.034f, ink, 0.9f);
    line(im, x0 + 6, y0 + h * 0.15f, x0 + w - 6, y0 + h * 0.15f, 1.0f, ink);
    drawTextC(im, FONT_SANS, "BERTH 9 CREW", x0 + w * 0.5f, y0 + h * 0.235f, h * 0.085f, ink);
    drawTextC(im, FONT_SANS, "STILL MISSING", x0 + w * 0.5f, y0 + h * 0.325f, h * 0.085f, ink);
    drawTextC(im, FONT_SANS, "Harbor board: \"No crane on Berth 9\"", x0 + w * 0.5f, y0 + h * 0.385f, h * 0.036f, ink, 0.9f);
    fillRect(im, x0 + w * 0.08f, y0 + h * 0.42f, x0 + w * 0.5f, y0 + h * 0.68f, Vec3(0.2f, 0.2f, 0.19f));
    line(im, x0 + w * 0.3f, y0 + h * 0.43f, x0 + w * 0.3f, y0 + h * 0.67f, 3.0f, Vec3(0.05f, 0.05f, 0.05f));
    line(im, x0 + w * 0.14f, y0 + h * 0.46f, x0 + w * 0.47f, y0 + h * 0.46f, 2.0f, Vec3(0.05f, 0.05f, 0.05f));
    for (int c = 0; c < 2; c++)
        for (int r = 0; r < 26; r++) {
            if (c == 0 && r < 15) continue;
            float lx = x0 + w * (0.08f + 0.47f * c), ly = y0 + h * (0.43f + 0.02f * r);
            float len = w * 0.4f * (0.75f + 0.25f * hash2f(r, c + 5));
            fillRect(im, lx, ly, lx + len, ly + 2.0f, ink, 0.55f);
        }
    weatherRect(im, x0, y0, w, h, 0.2f);
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) mulc(im.at(x, y), 1.0f - 0.35f * smoothstepf(0.55f, 0.9f, B(B_FBM4, x, y)));
}
static void s_radio(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.13f, 0.10f, 0.07f));
    fillRect(im, x0 + w * 0.06f, y0 + h * 0.14f, x0 + w * 0.62f, y0 + h * 0.86f, Vec3(0.07f, 0.06f, 0.05f));
    for (int i = 0; i < 12; i++) line(im, x0 + w * (0.09f + 0.045f * i), y0 + h * 0.2f, x0 + w * (0.09f + 0.045f * i), y0 + h * 0.8f, 2.0f, Vec3(0.2f, 0.17f, 0.12f));
    fillRect(im, x0 + w * 0.66f, y0 + h * 0.16f, x0 + w * 0.95f, y0 + h * 0.46f, Vec3(0.78f, 0.66f, 0.35f));
    for (int i = 0; i < 9; i++) line(im, x0 + w * (0.68f + 0.03f * i), y0 + h * 0.2f, x0 + w * (0.68f + 0.03f * i), y0 + h * 0.34f, 1.0f, Vec3(0.1f, 0.1f, 0.1f));
    line(im, x0 + w * 0.82f, y0 + h * 0.18f, x0 + w * 0.82f, y0 + h * 0.44f, 2.0f, Vec3(0.75f, 0.08f, 0.05f));
    drawText(im, FONT_SANS, "AM", x0 + w * 0.68f, y0 + h * 0.44f, h * 0.1f, Vec3(0.1f, 0.1f, 0.1f));
    disc(im, x0 + w * 0.73f, y0 + h * 0.7f, h * 0.13f, Vec3(0.4f, 0.38f, 0.33f));
    disc(im, x0 + w * 0.89f, y0 + h * 0.7f, h * 0.13f, Vec3(0.4f, 0.38f, 0.33f));
    line(im, x0 + w * 0.73f, y0 + h * 0.7f, x0 + w * 0.73f + 5.0f, y0 + h * 0.6f, 2.0f, Vec3(0.1f, 0.1f, 0.1f));
    line(im, x0 + w * 0.89f, y0 + h * 0.7f, x0 + w * 0.89f - 7.0f, y0 + h * 0.63f, 2.0f, Vec3(0.1f, 0.1f, 0.1f));
}
static void s_punch(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.24f, 0.27f, 0.25f));
    strokeRect(im, x0 + 2, y0 + 2, x0 + w - 2, y0 + h - 2, 3.0f, Vec3(0.1f, 0.1f, 0.1f));
    float cx = x0 + w * 0.5f, cy = y0 + h * 0.34f, r = w * 0.36f;
    disc(im, cx, cy, r, Vec3(0.75f, 0.73f, 0.64f));
    for (int i = 0; i < 12; i++) {
        float a = i * TAU / 12.0f;
        line(im, cx + std::sin(a) * r * 0.78f, cy - std::cos(a) * r * 0.78f, cx + std::sin(a) * r * 0.92f, cy - std::cos(a) * r * 0.92f, 2.0f, Vec3(0.08f, 0.08f, 0.08f));
    }
    float ha = (3.0f + 7.0f / 60.0f) / 12.0f * TAU, ma = 7.0f / 60.0f * TAU;
    line(im, cx, cy, cx + std::sin(ha) * r * 0.5f, cy - std::cos(ha) * r * 0.5f, 3.5f, Vec3(0.05f, 0.05f, 0.05f));
    line(im, cx, cy, cx + std::sin(ma) * r * 0.75f, cy - std::cos(ma) * r * 0.75f, 2.5f, Vec3(0.05f, 0.05f, 0.05f));
    fillRect(im, x0 + w * 0.2f, y0 + h * 0.7f, x0 + w * 0.8f, y0 + h * 0.76f, Vec3(0.03f, 0.03f, 0.03f));
    drawTextC(im, FONT_SANS, "IN / OUT", cx, y0 + h * 0.9f, h * 0.085f, Vec3(0.8f, 0.78f, 0.66f));
}
static void s_cabpanel(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.20f, 0.22f, 0.21f));
    strokeRect(im, x0 + 3, y0 + 3, x0 + w - 3, y0 + h - 3, 2.0f, Vec3(0.08f, 0.08f, 0.08f));
    const char* lab[4] = {"SLEW", "TROLLEY", "HOIST", "RELEASE"};
    for (int i = 0; i < 4; i++) {
        float cx = x0 + w * (0.14f + 0.24f * i);
        fillRect(im, cx - w * 0.08f, y0 + h * 0.74f, cx + w * 0.08f, y0 + h * 0.9f, Vec3(0.75f, 0.72f, 0.6f));
        drawTextC(im, FONT_SANS, lab[i], cx, y0 + h * 0.865f, h * 0.085f, Vec3(0.08f, 0.08f, 0.08f));
        if (i < 3) {
            fillRect(im, cx - w * 0.012f, y0 + h * 0.2f, cx + w * 0.012f, y0 + h * 0.66f, Vec3(0.03f, 0.03f, 0.03f));
            ring(im, cx, y0 + h * 0.43f, h * 0.09f, 2.0f, Vec3(0.5f, 0.5f, 0.46f));
        } else {
            disc(im, cx, y0 + h * 0.43f, h * 0.17f, Vec3(0.12f, 0.1f, 0.1f));
            disc(im, cx, y0 + h * 0.43f, h * 0.13f, Vec3(0.55f, 0.07f, 0.05f));
        }
    }
    for (int i = 0; i < 6; i++) disc(im, x0 + w * (0.07f + 0.035f * i), y0 + h * 0.1f, 3.5f, i == 4 ? Vec3(0.8f, 0.5f, 0.05f) : Vec3(0.25f, 0.05f, 0.04f));
    weatherRect(im, x0, y0, w, h, 0.35f);
}
static void s_vend(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.42f, 0.08f, 0.06f));
    fillRect(im, x0 + w * 0.08f, y0 + h * 0.04f, x0 + w * 0.92f, y0 + h * 0.42f, Vec3(0.72f, 0.68f, 0.56f));
    drawTextC(im, FONT_SANS, "COLA", x0 + w * 0.5f, y0 + h * 0.21f, h * 0.14f, Vec3(0.5f, 0.07f, 0.05f));
    drawTextC(im, FONT_SANS, "ICE COLD  35c", x0 + w * 0.5f, y0 + h * 0.34f, h * 0.05f, Vec3(0.1f, 0.1f, 0.1f));
    for (int i = 0; i < 5; i++) {
        float by = y0 + h * (0.48f + 0.07f * i);
        fillRect(im, x0 + w * 0.6f, by, x0 + w * 0.9f, by + h * 0.05f, Vec3(0.75f, 0.72f, 0.6f));
        fillRect(im, x0 + w * 0.1f, by, x0 + w * 0.55f, by + h * 0.05f, Vec3(0.3f, 0.06f, 0.05f));
    }
    fillRect(im, x0 + w * 0.2f, y0 + h * 0.86f, x0 + w * 0.8f, y0 + h * 0.96f, Vec3(0.05f, 0.05f, 0.05f));
    weatherRect(im, x0, y0, w, h, 0.5f);
}
static void s_plate(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.50f, 0.40f, 0.18f));
    strokeRect(im, x0 + 4, y0 + 4, x0 + w - 4, y0 + h - 4, 2.0f, Vec3(0.18f, 0.13f, 0.05f));
    Vec3 k(0.13f, 0.09f, 0.03f);
    drawTextC(im, FONT_SANS, "No. 7", x0 + w * 0.5f, y0 + h * 0.28f, h * 0.22f, k);
    drawText(im, FONT_SANS, "MFR.", x0 + w * 0.08f, y0 + h * 0.5f, h * 0.11f, k);
    drawText(im, FONT_SANS, "YEAR   1982", x0 + w * 0.08f, y0 + h * 0.7f, h * 0.11f, k);
    drawText(im, FONT_SANS, "SERIAL", x0 + w * 0.08f, y0 + h * 0.9f, h * 0.11f, k);
    // the maker and serial were never stamped: the fields are smooth
    fillRect(im, x0 + w * 0.36f, y0 + h * 0.4f, x0 + w * 0.92f, y0 + h * 0.53f, Vec3(0.56f, 0.46f, 0.22f), 0.9f);
    fillRect(im, x0 + w * 0.42f, y0 + h * 0.8f, x0 + w * 0.92f, y0 + h * 0.93f, Vec3(0.56f, 0.46f, 0.22f), 0.9f);
    boltHoles(im, x0, y0, w, h);
    weatherRect(im, x0, y0, w, h, 0.3f);
}
static void s_days(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.10f, 0.30f, 0.16f));
    strokeRect(im, x0 + 5, y0 + 5, x0 + w - 5, y0 + h - 5, 3.0f, Vec3(0.85f, 0.82f, 0.7f));
    Vec3 wh(0.85f, 0.82f, 0.7f);
    drawTextC(im, FONT_SANS, "THIS SITE HAS WORKED", x0 + w * 0.5f, y0 + h * 0.2f, h * 0.1f, wh);
    fillRect(im, x0 + w * 0.3f, y0 + h * 0.27f, x0 + w * 0.7f, y0 + h * 0.62f, Vec3(0.06f, 0.06f, 0.06f));
    drawTextC(im, FONT_SANS, "211", x0 + w * 0.5f, y0 + h * 0.56f, h * 0.3f, Vec3(0.75f, 0.74f, 0.7f), 0.3f, 2, 5);
    drawTextC(im, FONT_SANS, "0", x0 + w * 0.5f, y0 + h * 0.57f, h * 0.34f, Vec3(0.9f, 0.88f, 0.82f), 0.95f, 2, 8);
    drawTextC(im, FONT_SANS, "DAYS WITHOUT A", x0 + w * 0.5f, y0 + h * 0.76f, h * 0.1f, wh);
    drawTextC(im, FONT_SANS, "LOST TIME ACCIDENT", x0 + w * 0.5f, y0 + h * 0.9f, h * 0.1f, wh);
    boltHoles(im, x0, y0, w, h);
    weatherRect(im, x0, y0, w, h, 0.4f);
}
static void s_tape(Img& im, int x0, int y0, int w, int h) {
    fillRect(im, x0, y0, x0 + w, y0 + h, Vec3(0.08f, 0.08f, 0.08f));
    fillRect(im, x0 + w * 0.06f, y0 + h * 0.1f, x0 + w * 0.94f, y0 + h * 0.62f, Vec3(0.78f, 0.74f, 0.6f));
    drawText(im, FONT_SANS, "W.K. - CRANE LOG", x0 + w * 0.1f, y0 + h * 0.32f, h * 0.14f, Vec3(0.1f, 0.1f, 0.3f), 0.9f, 2, 42);
    fillRect(im, x0 + w * 0.25f, y0 + h * 0.38f, x0 + w * 0.75f, y0 + h * 0.58f, Vec3(0.12f, 0.1f, 0.08f));
    disc(im, x0 + w * 0.36f, y0 + h * 0.48f, h * 0.075f, Vec3(0.8f, 0.8f, 0.76f));
    disc(im, x0 + w * 0.64f, y0 + h * 0.48f, h * 0.075f, Vec3(0.8f, 0.8f, 0.76f));
}
static void s_arrowp(Img& im, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            float u = (x - x0 + 0.5f) / w - 0.5f, v = (y - y0 + 0.5f) / h;
            bool on = (v > 0.4f && std::fabs(u) < 0.13f) || (v <= 0.4f && std::fabs(u) < v * 1.1f);
            float wear = B(B_FBM32, x * 2, y) * 0.5f + B(B_FBM8, x * 2, y * 2) * 0.6f;
            Px& p = im.at(x, y);
            setc(p, Vec3(0.6f, 0.6f, 0.56f));
            p.a = on ? smoothstepf(0.36f, 0.56f, wear) * 0.9f : 0.0f;
        }
}
static void s_valvetagA(Img& im, int x0, int y0, int w, int h) {
    disc(im, x0 + w * 0.5f, y0 + h * 0.5f, w * 0.47f, Vec3(0.65f, 0.5f, 0.16f));
    disc(im, x0 + w * 0.5f, y0 + h * 0.5f, w * 0.4f, Vec3(0.78f, 0.74f, 0.6f));
    drawTextC(im, FONT_SANS, "A", x0 + w * 0.5f, y0 + h * 0.73f, h * 0.62f, Vec3(0.08f, 0.08f, 0.08f));
}
static void s_valvetagB(Img& im, int x0, int y0, int w, int h) {
    disc(im, x0 + w * 0.5f, y0 + h * 0.5f, w * 0.47f, Vec3(0.65f, 0.5f, 0.16f));
    disc(im, x0 + w * 0.5f, y0 + h * 0.5f, w * 0.4f, Vec3(0.78f, 0.74f, 0.6f));
    drawTextC(im, FONT_SANS, "B", x0 + w * 0.5f, y0 + h * 0.73f, h * 0.62f, Vec3(0.08f, 0.08f, 0.08f));
}
static void s_valvetagC(Img& im, int x0, int y0, int w, int h) {
    disc(im, x0 + w * 0.5f, y0 + h * 0.5f, w * 0.47f, Vec3(0.65f, 0.5f, 0.16f));
    disc(im, x0 + w * 0.5f, y0 + h * 0.5f, w * 0.4f, Vec3(0.78f, 0.74f, 0.6f));
    drawTextC(im, FONT_SANS, "C", x0 + w * 0.5f, y0 + h * 0.73f, h * 0.62f, Vec3(0.08f, 0.08f, 0.08f));
}

#define BOARD(n, w, h, bg, fg, t) {n, w, h, SK_BOARD, bg, fg, t, nullptr}
#define STEN(n, w, h, fg, t) {n, w, h, SK_STENCIL, 0, fg, t, nullptr}
#define CHALK(n, w, h, t) {n, w, h, SK_CHALK, 0, 0xd8d4c4, t, nullptr}
#define CUST(n, w, h, f) {n, w, h, SK_CUSTOM, 0, 0, "", f}

static const SignDef g_signs[] = {
    BOARD("gate", 512, 104, 0x16243a, 0xd8d4c0, "HARROW MARINE SALVAGE\n~BERTH 9  -  PORT OF ASHGROVE  -  EST. 1931"),
    CUST("blueprint", 336, 216, s_blueprint),
    CUST("calendar", 232, 232, s_calendar),
    CUST("poster", 176, 240, s_poster),
    CUST("news", 192, 248, s_news),
    CUST("cabpanel", 352, 176, s_cabpanel),
    CUST("crt1", 240, 168, s_crt1),
    CUST("crt2", 240, 168, s_crt2),
    CUST("photo", 240, 180, s_photo),
    CUST("fusebox", 232, 176, s_fusebox),
    CUST("days", 240, 180, s_days),
    CUST("vend", 136, 224, s_vend),
    CUST("hatch", 192, 192, s_hatch),
    CUST("keypad", 112, 168, s_keypad),
    CUST("drag", 96, 224, s_drag),
    CUST("drip", 64, 224, s_drip),
    CUST("lane", 28, 224, s_lane),
    CUST("laneY", 28, 224, s_laneY),
    CUST("arrowp", 96, 208, s_arrowp),
    BOARD("danger_load", 256, 160, 0xd09a10, 0x0c0c0c, "#DANGER\nSUSPENDED LOAD\n~KEEP CLEAR - DO NOT STAND UNDER"),
    BOARD("loto", 112, 152, 0xa01410, 0xe8e2d0, "#DANGER\nLOCKED OUT\nDO NOT\nOPERATE\n~W. KESSLER 11/8"),
    STEN("num7", 112, 152, 0x0c0c0c, "7"),
    CUST("punch", 120, 152, s_punch),
    CUST("oil2", 176, 128, s_oil),
    CUST("oil1", 128, 128, s_oil),
    CUST("puddle", 128, 128, s_puddle),
    CUST("scorch", 112, 112, s_scorch),
    CUST("clock", 120, 120, s_clock),
    CUST("gauge", 112, 112, s_gauge),
    CUST("gauge2", 112, 112, s_gauge2),
    CUST("crack", 240, 112, s_crack),
    CUST("grime", 256, 112, s_grime),
    CUST("hand", 88, 112, s_hand),
    CUST("bootL", 44, 104, s_bootL),
    CUST("bootR", 44, 104, s_bootR),
    BOARD("hv", 176, 120, 0xb01812, 0xeeeade, "#DANGER\nHIGH VOLTAGE"),
    BOARD("hardhat", 176, 120, 0x163a78, 0xe6e2d2, "HARD HAT\nAREA"),
    BOARD("nosmoke", 120, 120, 0xe0dccc, 0xa81610, "NO\nSMOKING"),
    BOARD("purge", 224, 104, 0xd8d2ba, 0x101010, "PURGE SEQUENCE\n~POSTED IN FOREMAN'S BOOK\n~WRONG ORDER TRIPS THE RELIEF"),
    CUST("plate", 208, 120, s_plate),
    STEN("crane7", 368, 104, 0x0a0a0a, "CRANE 7"),
    STEN("berth9", 368, 104, 0xd6d2c2, "BERTH 9"),
    BOARD("auth", 256, 96, 0xe0dccc, 0xa81610, "AUTHORIZED\nPERSONNEL ONLY"),
    BOARD("gen", 256, 96, 0xd09a10, 0x0c0c0c, "GENERATOR\n~EAR PROTECTION REQUIRED"),
    BOARD("local114", 224, 96, 0xd8d2ba, 0x1a2c50, "LOCAL 114\n~MARINE MECHANICS & RIGGERS"),
    STEN("harrow", 256, 88, 0xd6d2c2, "HARROW"),
    CUST("tape", 144, 92, s_tape),
    CUST("radio", 176, 88, s_radio),
    CUST("tally", 400, 80, s_tally),
    CHALK("c_dontlook", 400, 80, "DON'T LOOK UP"),
    CHALK("c_never", 448, 80, "HE NEVER CAME DOWN"),
    CHALK("c_putback", 368, 80, "PUT IT BACK"),
    CHALK("c_monday", 480, 80, "IT WASN'T HERE MONDAY"),
    CHALK("c_knock", 432, 80, "DON'T KNOCK BACK"),
    CHALK("c_relief", 400, 80, "RELIEF IS COMING"),
    CHALK("c_again", 256, 80, "AGAIN"),
    BOARD("stormlock", 192, 80, 0xd09a10, 0x0c0c0c, "SLEW LOCK\n~STORM BRAKE - PULL DOWN"),
    BOARD("feeder", 176, 72, 0xd8d2ba, 0x101010, "STORM BRAKE\nPUMP"),
    BOARD("gaterel", 176, 72, 0xd09a10, 0x0c0c0c, "GATE RELEASE\n~PULL TO DISENGAGE"),
    BOARD("cab", 192, 64, 0xe0dccc, 0xa81610, "OPERATOR ONLY"),
    BOARD("maxload", 192, 64, 0xd8d2ba, 0x101010, "S.W.L. 40 TONS"),
    BOARD("office", 192, 64, 0x16243a, 0xd8d4c0, "SITE OFFICE"),
    BOARD("crew", 192, 64, 0x16243a, 0xd8d4c0, "CREW ROOM"),
    BOARD("workshop", 192, 64, 0x16243a, 0xd8d4c0, "WORKSHOP"),
    BOARD("pump", 192, 64, 0x16243a, 0xd8d4c0, "PUMP HOUSE 2"),
    BOARD("diesel", 176, 64, 0xa01410, 0xe8e2d0, "DIESEL"),
    BOARD("barge", 176, 64, 0x0c0c0c, 0xd6d2c2, "H-12"),
    STEN("cid1", 256, 60, 0xd6d2c2, "HMSU 198211 7"),
    STEN("cid2", 256, 60, 0xd6d2c2, "TKLU 430118 2"),
    STEN("cid3", 256, 60, 0xd6d2c2, "CAXU 771902 5"),
    STEN("cid4", 256, 60, 0x101010, "MSKU 082198 2"),
    BOARD("lift1", 128, 64, 0xd8d2ba, 0x101010, "LIFT 1\n~MON 11/8"),
    BOARD("lift2", 128, 64, 0xd8d2ba, 0x101010, "LIFT 2\n~TUE 11/9"),
    BOARD("lift3", 128, 64, 0xd8d2ba, 0x101010, "LIFT 3\n~WED 11/10"),
    BOARD("lift4", 128, 64, 0xd8d2ba, 0x101010, "LIFT 4\n~THU 11/11"),
    CUST("tagA", 64, 64, s_valvetagA),
    CUST("tagB", 64, 64, s_valvetagB),
    CUST("tagC", 64, 64, s_valvetagC),
    BOARD("exit", 120, 56, 0x0e4a22, 0xe6e2d2, "EXIT"),
    BOARD("timecards", 168, 48, 0xd8d2ba, 0x101010, "TIME CARDS"),
    BOARD("fuses", 128, 48, 0xd8d2ba, 0x101010, "FUSES 30A"),
    BOARD("keys", 112, 48, 0xd8d2ba, 0x101010, "KEYS"),
    BOARD("n_dale", 104, 30, 0xd8d2ba, 0x101010, "DALE"),
    BOARD("n_walt", 104, 30, 0xd8d2ba, 0x101010, "WALT"),
    BOARD("n_ruiz", 104, 30, 0xd8d2ba, 0x101010, "RUIZ"),
    BOARD("n_moss", 104, 30, 0xd8d2ba, 0x101010, "MOSS"),
    BOARD("n_terry", 104, 30, 0xd8d2ba, 0x101010, "TERRY"),
    BOARD("n_anand", 104, 30, 0xd8d2ba, 0x101010, "ANAND"),
    BOARD("n_okoro", 104, 30, 0xd8d2ba, 0x101010, "OKORO"),
    BOARD("n_relief", 104, 30, 0xd8d2ba, 0x101010, "RELIEF"),
};
static const int NSIGNS = (int)(sizeof(g_signs) / sizeof(g_signs[0]));
struct Placed { int sheet, x, y; };
static Placed g_place[sizeof(g_signs) / sizeof(g_signs[0])];
static bool g_packed = false;

// Shelf packer. The definition list above is ordered roughly by height.
static void packSigns() {
    if (g_packed) return;
    g_packed = true;
    std::vector<int> order(NSIGNS);
    for (int i = 0; i < NSIGNS; i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [](int a, int b) { return g_signs[a].h > g_signs[b].h; });
    const int pad = 3;
    int sheet = 0, x = pad, y = pad, rowH = 0;
    for (int oi = 0; oi < NSIGNS; oi++) {
        int i = order[oi];
        const SignDef& s = g_signs[i];
        if (x + s.w + pad > N) { x = pad; y += rowH + pad; rowH = 0; }
        if (y + s.h + pad > N) { sheet++; x = pad; y = pad; rowH = 0; }
        g_place[i] = {sheet, x, y};
        x += s.w + pad;
        rowH = std::max(rowH, s.h);
    }
    if (sheet >= NUM_SIGN_SHEETS) {
        fprintf(stderr, "texgen: sign sheets overflow (%d needed)\n", sheet + 1);
        for (int i = 0; i < NSIGNS; i++)
            if (g_place[i].sheet >= NUM_SIGN_SHEETS) g_place[i].sheet = NUM_SIGN_SHEETS - 1;
    }
}

SignRect sign_rect(const char* name) {
    packSigns();
    for (int i = 0; i < NSIGNS; i++)
        if (std::strcmp(g_signs[i].name, name) == 0) {
            const SignDef& s = g_signs[i];
            SignRect r;
            r.layer = M_SIGN0 + g_place[i].sheet;
            r.u0 = (g_place[i].x + 0.5f) / N;
            r.v0 = (g_place[i].y + 0.5f) / N;
            r.u1 = (g_place[i].x + s.w - 0.5f) / N;
            r.v1 = (g_place[i].y + s.h - 0.5f) / N;
            r.aspect = (float)s.w / (float)s.h;
            return r;
        }
    fprintf(stderr, "texgen: unknown sign '%s'\n", name);
    SignRect r = {M_SIGN0, 0, 0, 0.01f, 0.01f, 1.0f};
    return r;
}

static void drawSign(Img& im, int idx) {
    const SignDef& s = g_signs[idx];
    int x0 = g_place[idx].x, y0 = g_place[idx].y;
    g_clip = {x0, y0, x0 + s.w, y0 + s.h};
    Vec3 fg = hex(s.fg), bg = hex(s.bg);
    switch (s.kind) {
    case SK_BOARD: {
        fillRect(im, (float)x0, (float)y0, (float)(x0 + s.w), (float)(y0 + s.h), bg);
        float b = std::max(2.0f, std::min(s.w, s.h) * 0.035f);
        if (s.h > 40) strokeRect(im, x0 + b, y0 + b, x0 + s.w - b, y0 + s.h - b, b * 0.7f, fg, 0.9f);
        float m = s.h > 40 ? std::min(s.w, s.h) * 0.13f : 3.0f;
        drawLines(im, FONT_SANS, s.text, x0 + m, y0 + m, s.w - 2 * m, s.h - 2 * m, fg, 1.0f, 0, (u32)idx);
        if (s.h > 40) boltHoles(im, x0, y0, s.w, s.h);
        weatherRect(im, x0, y0, s.w, s.h, s.h > 40 ? 0.45f : 0.15f);
        break;
    }
    case SK_STENCIL:
        drawLines(im, FONT_SANS, s.text, x0 + 4.0f, y0 + 2.0f, s.w - 8.0f, s.h - 4.0f, fg, 0.95f, 1, (u32)idx);
        break;
    case SK_CHALK:
        drawLines(im, FONT_SANS, s.text, x0 + 10.0f, y0 + 6.0f, s.w - 20.0f, s.h - 12.0f, fg, 0.9f, 2, (u32)idx * 7u + 3u);
        break;
    default:
        if (s.fn) s.fn(im, x0, y0, s.w, s.h);
        break;
    }
    g_clip = {0, 0, N, N};
}

// ---------------------------------------------------------------------------
// Step runner
// ---------------------------------------------------------------------------
typedef void (*GenFn)(Img&);
static const GenFn g_matFns[M_SIGN0] = {
    gen_asphalt, gen_concrete, gen_concwall, gen_block, gen_corrugated, gen_rust, gen_paintsteel, gen_diamond,
    gen_grate, gen_chainlink, gen_wood, gen_panel, gen_lino, gen_ceiling, gen_brick, gen_rubber,
    gen_glass, gen_paper, gen_tarp, gen_hazard, gen_locker, gen_screen, gen_plain, gen_gravel,
    gen_rail, gen_cardboard, gen_machine, gen_contdoor, gen_roof, gen_kelp, gen_oilskin, gen_blinds, gen_noise,
};

int texgen_step_count() { return 1 + M_COUNT; }

void texgen_run_step(int step, int* layerOut, std::vector<u8>& out) {
    *layerOut = -1;
    if (step == 0) {
        buildBank();
        packSigns();
        return;
    }
    int layer = step - 1;
    if (layer < 0 || layer >= M_COUNT) return;
    if (g_bank[0].empty()) buildBank();
    Img im;
    bool sheet = layer >= M_SIGN0;
    if (!sheet) {
        g_matFns[layer](im);
    } else {
        packSigns();
        for (int i = 0; i < NSIGNS; i++)
            if (g_place[i].sheet == layer - M_SIGN0) drawSign(im, i);
    }
    out.resize((size_t)N * N * 4);
    for (size_t i = 0; i < (size_t)N * N; i++) {
        Px p = im.p[i];
        if (sheet) {
            // premultiply (compensating for the sRGB decode) so mip-maps do not fringe
            float k = std::pow(saturate(p.a), 1.0f / 2.2f);
            p.r *= k; p.g *= k; p.b *= k;
        }
        out[i * 4 + 0] = (u8)(saturate(p.r) * 255.0f + 0.5f);
        out[i * 4 + 1] = (u8)(saturate(p.g) * 255.0f + 0.5f);
        out[i * 4 + 2] = (u8)(saturate(p.b) * 255.0f + 0.5f);
        out[i * 4 + 3] = (u8)(saturate(p.a) * 255.0f + 0.5f);
    }
    *layerOut = layer;
}

void texgen_free_temp() {
    for (int i = 0; i < B_COUNT; i++) { g_bank[i].clear(); g_bank[i].shrink_to_fit(); }
}
