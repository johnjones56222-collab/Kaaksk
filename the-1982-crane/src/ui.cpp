// THE 1982 CRANE - input mapping, HUD, touch controls, menus, journal, map, endings
#include "ui.h"
#include "gfx.h"
#include "game.h"
#include "audio.h"
#include "platform.h"

void InputState::endFrame() {
    for (int i = 0; i < K_COUNT; i++) pressed[i] = false;
    mdx = mdy = 0;
    mpressed = mreleased = false;
    wheel = 0;
    for (TouchPt& p : t) {
        if (!p.active) continue;
        p.fresh = false;
        p.lx = p.x; p.ly = p.y;
        if (p.ended) p = TouchPt();
    }
}

namespace {

// ---------------------------------------------------------------------------
// Batch renderer
// ---------------------------------------------------------------------------
const int ATLAS = 1024, CELL = 64, FONT_PX = 44;
struct UV { float x, y, u, v; u32 c; };
std::vector<UV> g_v;
Program g_prog;
GLuint g_vao = 0, g_vbo = 0, g_tex = 0;
float g_W = 800, g_H = 600, g_dpr = 1;
int g_fbH = 600;
bool g_clipOn = false;
float g_clip[4];

const char* VS = R"(
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aCol;
uniform vec2 uScreen;
out vec2 vUV;
out vec4 vCol;
void main() {
    vUV = aUV;
    vCol = aCol;
    gl_Position = vec4(aPos.x / uScreen.x * 2.0 - 1.0, 1.0 - aPos.y / uScreen.y * 2.0, 0.0, 1.0);
}
)";
const char* FS = R"(
uniform sampler2D uTex;
in vec2 vUV;
in vec4 vCol;
out vec4 frag;
void main() {
    float a = texture(uTex, vUV).r;
    frag = vec4(vCol.rgb, vCol.a * a);
}
)";

inline u32 col(u32 rgb, float a = 1.0f) {
    u32 r = (rgb >> 16) & 255, gg = (rgb >> 8) & 255, b = rgb & 255;
    u32 al = (u32)clampv((int)(a * 255.0f + 0.5f), 0, 255);
    return r | (gg << 8) | (b << 16) | (al << 24);
}
inline u32 fade(u32 c, float k) {
    u32 a = (u32)clampv((int)(((c >> 24) & 255) * k + 0.5f), 0, 255);
    return (c & 0x00FFFFFFu) | (a << 24);
}

void flush() {
    if (g_v.empty()) return;
    g_prog.use();
    g_prog.f2("uScreen", g_W, g_H);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(g_v.size() * sizeof(UV)), g_v.data(), GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    if (g_clipOn) {
        glEnable(GL_SCISSOR_TEST);
        int x0 = (int)(g_clip[0] * g_dpr), y0 = (int)(g_clip[1] * g_dpr), x1 = (int)((g_clip[0] + g_clip[2]) * g_dpr), y1 = (int)((g_clip[1] + g_clip[3]) * g_dpr);
        glScissor(x0, g_fbH - y1, std::max(0, x1 - x0), std::max(0, y1 - y0));
    } else glDisable(GL_SCISSOR_TEST);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)g_v.size());
    g_v.clear();
}
void clip(float x, float y, float w, float h) {
    flush();
    g_clipOn = true;
    g_clip[0] = x; g_clip[1] = y; g_clip[2] = w; g_clip[3] = h;
}
void noclip() {
    flush();
    g_clipOn = false;
}

const float WU = (ATLAS - 8 + 0.5f) / ATLAS, WV = (ATLAS - 8 + 0.5f) / ATLAS; // a solid white texel
void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, u32 c) {
    g_v.push_back({x0, y0, u0, v0, c}); g_v.push_back({x1, y0, u1, v0, c}); g_v.push_back({x1, y1, u1, v1, c});
    g_v.push_back({x0, y0, u0, v0, c}); g_v.push_back({x1, y1, u1, v1, c}); g_v.push_back({x0, y1, u0, v1, c});
}
void rect(float x, float y, float w, float h, u32 c) { quad(x, y, x + w, y + h, WU, WV, WU, WV, c); }
void frame(float x, float y, float w, float h, float t, u32 c) {
    rect(x, y, w, t, c); rect(x, y + h - t, w, t, c);
    rect(x, y + t, t, h - 2 * t, c); rect(x + w - t, y + t, t, h - 2 * t, c);
}
void tri(float ax, float ay, float bx, float by, float cx, float cy, u32 c) {
    g_v.push_back({ax, ay, WU, WV, c}); g_v.push_back({bx, by, WU, WV, c}); g_v.push_back({cx, cy, WU, WV, c});
}
void disc(float cx, float cy, float r, u32 c, int segs = 28) {
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * TAU, a1 = (float)(i + 1) / segs * TAU;
        tri(cx, cy, cx + std::cos(a0) * r, cy + std::sin(a0) * r, cx + std::cos(a1) * r, cy + std::sin(a1) * r, c);
    }
}
void ring(float cx, float cy, float r, float t, u32 c, int segs = 32) {
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / segs * TAU, a1 = (float)(i + 1) / segs * TAU;
        float c0 = std::cos(a0), s0 = std::sin(a0), c1 = std::cos(a1), s1 = std::sin(a1);
        float ri = r - t * 0.5f, ro = r + t * 0.5f;
        tri(cx + c0 * ri, cy + s0 * ri, cx + c0 * ro, cy + s0 * ro, cx + c1 * ro, cy + s1 * ro, c);
        tri(cx + c0 * ri, cy + s0 * ri, cx + c1 * ro, cy + s1 * ro, cx + c1 * ri, cy + s1 * ri, c);
    }
}
void line(float ax, float ay, float bx, float by, float t, u32 c) {
    float dx = bx - ax, dy = by - ay, l = std::sqrt(dx * dx + dy * dy);
    if (l < 1e-4f) return;
    float nx = -dy / l * t * 0.5f, ny = dx / l * t * 0.5f;
    tri(ax + nx, ay + ny, bx + nx, by + ny, bx - nx, by - ny, c);
    tri(ax + nx, ay + ny, bx - nx, by - ny, ax - nx, ay - ny, c);
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------
float adv(int font, int ch) {
    const FontAtlas& F = g_fonts[font];
    if (ch < 32 || ch > 127) ch = '?';
    return F.valid() ? F.adv[ch - 32] : FONT_PX * 0.55f;
}
float textW(int font, const char* s, float size) {
    float w = 0;
    for (; *s; s++) w += adv(font, (unsigned char)*s);
    return w * size / FONT_PX;
}
float textW(int font, const std::string& s, float size) { return textW(font, s.c_str(), size); }
// align: 0 left, 1 centre, 2 right. y is the baseline.
void text(int font, const char* s, float x, float y, float size, u32 c, int align = 0, float spacing = 0.0f) {
    float k = size / FONT_PX;
    float w = textW(font, s, size) + spacing * (float)std::max(0, (int)std::strlen(s) - 1);
    if (align == 1) x -= w * 0.5f;
    else if (align == 2) x -= w;
    float rowOff = font == FONT_MONO ? 384.0f : 0.0f;
    for (; *s; s++) {
        int ch = (unsigned char)*s;
        if (ch < 32 || ch > 127) ch = '?';
        if (ch != ' ') {
            int i = ch - 32;
            float cx = (float)((i % 16) * CELL), cy = (float)((i / 16) * CELL) + rowOff;
            float x0 = x - 8.0f * k, y0 = y - CELL * 0.75f * k;
            quad(x0, y0, x0 + CELL * k, y0 + CELL * k, cx / ATLAS, cy / ATLAS, (cx + CELL) / ATLAS, (cy + CELL) / ATLAS, c);
        }
        x += adv(font, ch) * k + spacing;
    }
}
void text(int font, const std::string& s, float x, float y, float size, u32 c, int align = 0, float spacing = 0.0f) {
    text(font, s.c_str(), x, y, size, c, align, spacing);
}
void textShadow(int font, const std::string& s, float x, float y, float size, u32 c, int align = 0) {
    text(font, s, x + std::max(1.0f, size * 0.06f), y + std::max(1.0f, size * 0.06f), size, col(0, ((c >> 24) & 255) / 255.0f * 0.8f), align);
    text(font, s, x, y, size, c, align);
}
void wrap(int font, const std::string& s, float size, float maxW, std::vector<std::string>& out) {
    out.clear();
    size_t p = 0;
    while (p <= s.size()) {
        size_t e = s.find('\n', p);
        if (e == std::string::npos) e = s.size();
        std::string para = s.substr(p, e - p);
        p = e + 1;
        if (para.empty()) { out.push_back(""); if (e >= s.size()) break; continue; }
        std::string cur;
        size_t q = 0;
        // preserve leading indentation
        while (q < para.size() && para[q] == ' ') { cur.push_back(' '); q++; }
        std::string lead = cur;
        while (q < para.size()) {
            size_t sp = para.find(' ', q);
            if (sp == std::string::npos) sp = para.size();
            std::string word = para.substr(q, sp - q);
            q = sp + 1;
            std::string trial = cur.size() > lead.size() ? cur + " " + word : cur + word;
            if (textW(font, trial, size) > maxW && cur.size() > lead.size()) {
                out.push_back(cur);
                cur = word;
            } else cur = trial;
        }
        out.push_back(cur);
        if (e >= s.size()) break;
    }
}

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
const u32 C_INK = 0xd8d2bf, C_DIM = 0x8b877a, C_AMBER = 0xd9a441, C_RED = 0xc0352a, C_PAPER = 0xcdc4aa, C_PINK = 0x2b2620, C_PANEL = 0x0c0d0e,
          C_GREEN = 0x7fc48a, C_BLUEPRINT = 0x14284a, C_BPINK = 0xb9cde4;

// ---------------------------------------------------------------------------
// Pointer for menus
// ---------------------------------------------------------------------------
struct Ptr {
    bool has = false;
    float x = -1, y = -1;
    bool pressed = false, released = false, down = false;
    float dy = 0;
};
Ptr g_ptr;
u32 g_pressId = 0, g_dragId = 0;
float g_dragLast = 0;

u32 hid(const char* s, float x, float y) {
    u32 h = 2166136261u;
    for (; *s; s++) h = (h ^ (u8)*s) * 16777619u;
    return hash32(h ^ (u32)(int)(x * 7.0f) ^ ((u32)(int)(y * 13.0f) << 16));
}
bool inside(float px, float py, float x, float y, float w, float h) { return px >= x && py >= y && px < x + w && py < y + h; }

void ptr_update(InputState& in) {
    g_ptr = Ptr();
    for (TouchPt& t : in.t) {
        if (!t.active || t.role != TR_UI) continue;
        g_ptr.has = true;
        g_ptr.x = t.x; g_ptr.y = t.y;
        g_ptr.pressed = t.fresh;
        g_ptr.released = t.ended;
        g_ptr.down = !t.ended;
        g_ptr.dy = t.y - t.ly;
        return;
    }
    if (!in.locked && in.mx >= 0) {
        g_ptr.has = true;
        g_ptr.x = in.mx; g_ptr.y = in.my;
        g_ptr.pressed = in.mpressed;
        g_ptr.released = in.mreleased;
        g_ptr.down = in.mdown;
        g_ptr.dy = 0;
    }
}

float U = 1.0f;   // layout unit
float TK = 1.0f;  // text scale

bool button(const char* label, float x, float y, float w, float h, bool enabled = true, bool primary = false, float alpha = 1.0f) {
    u32 id = hid(label, x, y);
    bool hover = g_ptr.has && inside(g_ptr.x, g_ptr.y, x, y, w, h);
    if (enabled && g_ptr.pressed && hover) g_pressId = id;
    bool held = g_pressId == id && g_ptr.down && hover;
    bool clicked = enabled && g_ptr.released && hover && g_pressId == id;
    u32 bg = held ? col(0x2a2620, 0.95f * alpha) : (hover && enabled ? col(0x1d1b18, 0.92f * alpha) : col(C_PANEL, 0.82f * alpha));
    rect(x, y, w, h, bg);
    frame(x, y, w, h, std::max(1.0f, U), col(primary ? C_AMBER : (enabled ? 0x5a564c : 0x2e2c28), alpha));
    float ts = std::min(h * 0.42f, 19.0f * TK);
    u32 tc = enabled ? col(primary ? C_AMBER : C_INK, alpha) : col(0x56534a, alpha);
    text(FONT_SANS, label, x + w * 0.5f, y + h * 0.5f + ts * 0.36f, ts, tc, 1, ts * 0.06f);
    if (clicked) audio_play(S_UI, 0.35f, 1.6f);
    return clicked;
}
float slider(const char* label, const char* valueText, float x, float y, float w, float h, float v) {
    u32 id = hid(label, x, y);
    float trackX = x + w * 0.42f, trackW = w * 0.5f;
    bool hover = g_ptr.has && inside(g_ptr.x, g_ptr.y, trackX - 12 * U, y, trackW + 24 * U, h);
    if (g_ptr.pressed && hover) g_dragId = id;
    if (g_dragId == id && g_ptr.down) v = saturate((g_ptr.x - trackX) / trackW);
    float ts = 16.0f * TK;
    text(FONT_SANS, label, x, y + h * 0.5f + ts * 0.36f, ts, col(C_INK));
    rect(trackX, y + h * 0.5f - 1.5f * U, trackW, 3.0f * U, col(0x4a463e));
    rect(trackX, y + h * 0.5f - 1.5f * U, trackW * v, 3.0f * U, col(C_AMBER));
    disc(trackX + trackW * v, y + h * 0.5f, 8.0f * U, col(g_dragId == id ? C_AMBER : C_INK));
    text(FONT_MONO, valueText, x + w, y + h * 0.5f + ts * 0.36f, ts * 0.9f, col(C_DIM), 2);
    return v;
}
bool toggleRow(const char* label, const char* value, float x, float y, float w, float h) {
    u32 id = hid(label, x, y);
    bool hover = g_ptr.has && inside(g_ptr.x, g_ptr.y, x, y, w, h);
    if (g_ptr.pressed && hover) g_pressId = id;
    bool clicked = g_ptr.released && hover && g_pressId == id;
    if (hover) rect(x - 6 * U, y, w + 12 * U, h, col(0xffffff, 0.04f));
    float ts = 16.0f * TK;
    text(FONT_SANS, label, x, y + h * 0.5f + ts * 0.36f, ts, col(C_INK));
    text(FONT_SANS, std::string("< ") + value + " >", x + w, y + h * 0.5f + ts * 0.36f, ts, col(C_AMBER), 2);
    if (clicked) audio_play(S_UI, 0.3f, 1.8f);
    return clicked;
}

// ---------------------------------------------------------------------------
// Touch HUD layout (shared by input gathering and drawing)
// ---------------------------------------------------------------------------
enum HudBtn { HB_USE = 0, HB_LAMP, HB_CROUCH, HB_RUN, HB_JOURNAL, HB_PAUSE, HB_CAB_LEVERS, HB_CAB_LOG, HB_CAB_LEAVE, HB_RELEASE, HB_STAND, HB_STEPOUT, HB_COUNT };
enum HudSlider { HS_SLEW = 0, HS_TROLLEY, HS_HOIST, HS_COUNT };
struct Circle { float x, y, r; bool show; };
struct Bar { float x, y, w, h; bool vertical; bool show; };
struct HudLayout {
    Circle b[HB_COUNT];
    Bar s[HS_COUNT];
};
float g_slider[HS_COUNT] = {0, 0, 0};
bool g_runToggle = false;

HudLayout hud_layout(const Game& g, float W, float H) {
    HudLayout L;
    for (auto& c : L.b) c = {0, 0, 0, false};
    for (auto& s : L.s) s = {0, 0, 0, 0, false, false};
    float u = U;
    bool play = g.appMode == AM_PLAY && g.modal == MD_NONE && g.pstate != PS_DEAD && g.pstate != PS_CUTSCENE;
    if (!play) return L;
    L.b[HB_PAUSE] = {W - 34 * u, 34 * u, 22 * u, true};
    L.b[HB_JOURNAL] = {W - 90 * u, 34 * u, 22 * u, true};
    if (g.pstate == PS_NORMAL || g.pstate == PS_LADDER) {
        bool normal = g.pstate == PS_NORMAL;
        L.b[HB_USE] = {W - 76 * u, H - 196 * u, 40 * u, normal};
        L.b[HB_RUN] = {W - 76 * u, H - 92 * u, 32 * u, normal};
        L.b[HB_CROUCH] = {W - 168 * u, H - 64 * u, 28 * u, normal};
        L.b[HB_LAMP] = {W - 176 * u, H - 154 * u, 28 * u, g.have[IT_FLASHLIGHT]};
    } else if (g.pstate == PS_HIDING) {
        L.b[HB_STEPOUT] = {W - 80 * u, H - 90 * u, 40 * u, true};
    } else if (g.pstate == PS_SEATED) {
        L.b[HB_USE] = {W - 76 * u, H - 196 * u, 40 * u, true};
        L.b[HB_CAB_LEVERS] = {W - 168 * u, H - 64 * u, 34 * u, !g.crane.loadReleased};
        L.b[HB_CAB_LOG] = {W - 168 * u, H - 154 * u, 30 * u, true};
        L.b[HB_CAB_LEAVE] = {W - 76 * u, H - 92 * u, 32 * u, true};
    } else if (g.pstate == PS_OPERATING) {
        L.s[HS_SLEW] = {28 * u, H - 92 * u, std::min(260 * u, W * 0.36f), 54 * u, false, true};
        L.s[HS_TROLLEY] = {W - 200 * u, H - 250 * u, 58 * u, 200 * u, true, true};
        L.s[HS_HOIST] = {W - 112 * u, H - 250 * u, 58 * u, 200 * u, true, true};
        L.b[HB_RELEASE] = {W * 0.5f, H - 64 * u, 40 * u, true};
        L.b[HB_STAND] = {W - 150 * u, 34 * u, 22 * u, true};
    }
    return L;
}
bool in_circle(float x, float y, const Circle& c) { return c.show && (x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= (c.r * 1.15f) * (c.r * 1.15f); }
bool in_bar(float x, float y, const Bar& b) { return b.show && x >= b.x - 10 * U && x <= b.x + b.w + 10 * U && y >= b.y - 10 * U && y <= b.y + b.h + 10 * U; }

// ---------------------------------------------------------------------------
// Menu state
// ---------------------------------------------------------------------------
enum TitlePage { TP_GATE = 0, TP_MAIN, TP_NEW, TP_CONFIRM, TP_SETTINGS, TP_HELP };
enum PausePage { PP_MAIN = 0, PP_SETTINGS, PP_HELP, PP_QUIT };
int g_titlePage = TP_GATE;
int g_pausePage = PP_MAIN;
float g_titleT = 0;
int g_pendingDifficulty = 1;
struct LockRect { float x, y, w, h; };
std::vector<LockRect> g_lockRects, g_lockRectsPrev; // buttons whose click should also capture the mouse
float g_fsRect[4] = {-1, -1, 0, 0};
float g_gateHint = 0;
float g_controlsCard = 0;
int g_lastStage = -1;
int g_lastAppMode = -1;
std::vector<std::string> g_lines;
// pending one-frame actions produced by UI clicks, delivered with the next Controls
Controls g_pending;
bool g_hasPending = false;

const char* DIFF_NAME[3] = {"STORY", "STANDARD", "NIGHTMARE"};
const char* DIFF_DESC[3] = {"More battery life. It is slower, hears less, and gives up sooner. For the story.",
                            "The shift as intended. Keep to the dark. Hide when it hunts.",
                            "Weak batteries. It sees further, hears more, and searches longer. It climbs faster."};

} // namespace

// ---------------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------------
std::string settings_to_string(const Settings& s) {
    return strf("sens=%.3f;tsens=%.3f;inv=%d;q=%d;scale=%.3f;bri=%.3f;vol=%.3f;txt=%d;bob=%d;fps=%d", s.lookSens, s.touchSens, s.invertY ? 1 : 0, s.quality,
                s.renderScale, s.brightness, s.volume, s.textSize, s.headBob ? 1 : 0, s.showFps ? 1 : 0);
}
void settings_from_string(Settings& s, const std::string& str) {
    size_t p = 0;
    while (p < str.size()) {
        size_t e = str.find(';', p);
        if (e == std::string::npos) e = str.size();
        std::string kv = str.substr(p, e - p);
        p = e + 1;
        size_t eq = kv.find('=');
        if (eq == std::string::npos) continue;
        std::string k = kv.substr(0, eq);
        float v = (float)atof(kv.c_str() + eq + 1);
        if (k == "sens") s.lookSens = clampv(v, 0.2f, 3.0f);
        else if (k == "tsens") s.touchSens = clampv(v, 0.2f, 3.0f);
        else if (k == "inv") s.invertY = v != 0;
        else if (k == "q") s.quality = clampv((int)v, -1, 2);
        else if (k == "scale") s.renderScale = (v < 0) ? -1.0f : clampv(v, 0.4f, 1.0f);
        else if (k == "bri") s.brightness = clampv(v, 0.6f, 1.8f);
        else if (k == "vol") s.volume = clampv(v, 0.0f, 1.0f);
        else if (k == "txt") s.textSize = clampv((int)v, 0, 2);
        else if (k == "bob") s.headBob = v != 0;
        else if (k == "fps") s.showFps = v != 0;
    }
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void ui_init() {
    g_prog = gfx_program(VS, FS, "", "ui");
    g_prog.use();
    g_prog.i1("uTex", 0);
    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UV), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UV), (void*)8);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UV), (void*)16);
    glBindVertexArray(0);
    std::vector<u8> atlas((size_t)ATLAS * ATLAS, 0);
    for (int f = 0; f < FONT_COUNT; f++) {
        const FontAtlas& F = g_fonts[f];
        if (!F.valid()) continue;
        int yo = f == FONT_MONO ? 384 : 0;
        for (int y = 0; y < std::min(F.texH, 384); y++)
            for (int x = 0; x < std::min(F.texW, ATLAS); x++) atlas[(size_t)(y + yo) * ATLAS + x] = F.alpha[(size_t)y * F.texW + x];
    }
    for (int y = ATLAS - 16; y < ATLAS; y++)
        for (int x = ATLAS - 16; x < ATLAS; x++) atlas[(size_t)y * ATLAS + x] = 255;
    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS, ATLAS, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool ui_wants_unlocked_cursor(const Game& g) {
    if (g.appMode != AM_PLAY) return true;
    return g.modal == MD_JOURNAL || g.modal == MD_KEYPAD || g.modal == MD_PAUSE;
}

int ui_gesture_query(const Game& g, float x, float y, bool touch) {
    int r = 0;
    if (touch) {
        if (g.appMode == AM_TITLE && g_titlePage == TP_GATE) r |= 2;
        if (g_fsRect[0] >= 0 && inside(x, y, g_fsRect[0], g_fsRect[1], g_fsRect[2], g_fsRect[3])) r |= 2;
        return r;
    }
    if (g_fsRect[0] >= 0 && inside(x, y, g_fsRect[0], g_fsRect[1], g_fsRect[2], g_fsRect[3])) r |= 2;
    for (const LockRect& lr : g_lockRectsPrev)
        if (inside(x, y, lr.x, lr.y, lr.w, lr.h)) r |= 1;
    if (g.appMode != AM_PLAY) return r;
    if (g.modal == MD_NONE && g.pstate != PS_DEAD) r |= 1;
    return r;
}

// ---------------------------------------------------------------------------
// Input -> Controls
// ---------------------------------------------------------------------------
void ui_gather(Game& g, Settings& s, InputState& in, Controls& c, float dt) {
    (void)dt;
    c = Controls();
    g_W = in.W; g_H = in.H;
    U = clampv(std::min(in.W, in.H) / 400.0f, 0.75f, 1.45f);
    g.bobScale = s.headBob ? 1.0f : 0.25f;
    bool play = g.appMode == AM_PLAY;
    bool modal = g.modal != MD_NONE;
    bool operating = g.pstate == PS_OPERATING;
    HudLayout L = hud_layout(g, in.W, in.H);

    // ---- keyboard ----
    auto k = [&](int key) { return in.down[key]; };
    auto kp = [&](int key) { return in.pressed[key]; };
    if (play) {
        if (!modal) {
            if (operating) {
                c.craneSlew = (k(K_A) || k(K_LEFT) ? 1.0f : 0.0f) - (k(K_D) || k(K_RIGHT) ? 1.0f : 0.0f);
                c.craneTrolley = (k(K_W) || k(K_UP) ? 1.0f : 0.0f) - (k(K_S) || k(K_DOWN) ? 1.0f : 0.0f);
                c.craneHoist = (k(K_R) ? 1.0f : 0.0f) - (k(K_F) || k(K_V) ? 1.0f : 0.0f);
                c.craneRelease = kp(K_SPACE) || kp(K_ENTER);
                c.back = kp(K_Q) || kp(K_BACK) || kp(K_X);
            } else {
                c.moveY = (k(K_W) || k(K_UP) ? 1.0f : 0.0f) - (k(K_S) || k(K_DOWN) ? 1.0f : 0.0f);
                c.moveX = (k(K_D) || k(K_RIGHT) ? 1.0f : 0.0f) - (k(K_A) || k(K_LEFT) ? 1.0f : 0.0f);
                c.sprint = k(K_SHIFT);
                c.crouch = kp(K_C) || kp(K_CTRL);
                c.interact = kp(K_E) || kp(K_ENTER) || (kp(K_SPACE) && g.pstate != PS_NORMAL);
                c.flashlight = kp(K_F);
                c.back = kp(K_Q) || kp(K_BACK);
                if (g.pstate == PS_HIDING && (kp(K_S) || kp(K_DOWN))) c.back = true;
            }
            c.journal = kp(K_TAB) || kp(K_J) || kp(K_I) || kp(K_M);
            c.pause = kp(K_ESC) || kp(K_P);
            // mouse look (pointer locked) and click-to-use
            if (in.locked) {
                c.lookDX = in.mdx * 0.0022f * s.lookSens;
                c.lookDY = -in.mdy * 0.0022f * s.lookSens * (s.invertY ? -1.0f : 1.0f);
                if (in.mpressed && g.promptIdx >= 0) c.interact = true;
            }
        } else {
            c.pause = kp(K_ESC) || kp(K_P);
            c.back = kp(K_BACK) || kp(K_Q);
            c.confirm = kp(K_E) || kp(K_SPACE) || kp(K_ENTER);
            c.journal = kp(K_TAB) || kp(K_J) || kp(K_I) || kp(K_M);
            c.scroll = (k(K_DOWN) || k(K_S) ? 1.0f : 0.0f) - (k(K_UP) || k(K_W) ? 1.0f : 0.0f);
            if (g.modal == MD_NOTE && in.locked && in.mpressed) c.confirm = true;
            if (g.modal == MD_KEYPAD) {
                c.confirm = false;
                for (int d = 0; d <= 9; d++)
                    if (kp(K_0 + d)) c.keypad = d;
                if (kp(K_BACK)) { c.keypad = 10; c.back = false; }
                if (kp(K_ENTER)) c.keypad = 11;
            }
        }
    } else if (g.appMode == AM_ENDING) {
        c.confirm = kp(K_SPACE) || kp(K_ENTER) || kp(K_E) || kp(K_ESC) || in.mpressed;
    }

    // ---- touch ----
    for (TouchPt& t : in.t) {
        if (!t.active) continue;
        if (t.fresh) {
            t.role = TR_UI;
            t.arg = -1;
            if (play && !modal && g.pstate != PS_DEAD && g.pstate != PS_CUTSCENE) {
                t.role = TR_NONE;
                for (int i = 0; i < HB_COUNT; i++)
                    if (in_circle(t.x, t.y, L.b[i])) { t.role = TR_BTN; t.arg = i; break; }
                if (t.role == TR_NONE)
                    for (int i = 0; i < HS_COUNT; i++)
                        if (in_bar(t.x, t.y, L.s[i])) { t.role = TR_SLIDER; t.arg = i; break; }
                if (t.role == TR_NONE) {
                    bool joyZone = t.x < in.W * 0.45f && t.y > in.H * 0.28f && (g.pstate == PS_NORMAL);
                    t.role = joyZone ? TR_JOY : TR_LOOK;
                }
                if (t.role == TR_BTN) {
                    switch (t.arg) {
                    case HB_USE: c.interact = true; break;
                    case HB_LAMP: c.flashlight = true; break;
                    case HB_CROUCH: c.crouch = true; break;
                    case HB_RUN: g_runToggle = !g_runToggle; break;
                    case HB_JOURNAL: c.journal = true; break;
                    case HB_PAUSE: c.pause = true; break;
                    case HB_CAB_LEVERS: use_object(g, U_CAB_CONTROLS); break;
                    case HB_CAB_LOG: game_interact(g, g.w.cabNoteInteract); break;
                    case HB_CAB_LEAVE: use_object(g, U_CAB_LEAVE); break;
                    case HB_RELEASE: c.craneRelease = true; break;
                    case HB_STAND: c.back = true; break;
                    case HB_STEPOUT: c.back = true; break;
                    default: break;
                    }
                }
            } else if (g.appMode == AM_ENDING) {
                c.confirm = true;
            }
        }
        if (t.role == TR_JOY) {
            float dx = t.x - t.sx, dy = t.y - t.sy;
            float R = 52.0f * U;
            float l = std::sqrt(dx * dx + dy * dy);
            if (l > R) {
                // the origin follows the thumb so it never runs out of travel
                t.sx += dx * (1.0f - R / l);
                t.sy += dy * (1.0f - R / l);
                dx = t.x - t.sx; dy = t.y - t.sy;
                l = R;
            }
            float m = l / R;
            if (m > 0.12f && !t.ended) {
                float k2 = (m - 0.12f) / 0.88f / m;
                c.moveX = dx / R * k2 * (1.0f / 1.0f);
                c.moveY = -dy / R * k2;
                c.moveX = clampv(c.moveX * 1.15f, -1.0f, 1.0f);
                c.moveY = clampv(c.moveY * 1.15f, -1.0f, 1.0f);
            }
            if (t.ended) g_runToggle = false;
            if (g_runToggle) c.sprint = true;
        } else if (t.role == TR_LOOK) {
            float dx = t.x - t.lx, dy = t.y - t.ly;
            if (std::fabs(t.x - t.sx) + std::fabs(t.y - t.sy) > 10.0f) t.moved = true;
            float ks = 0.0062f * s.touchSens;
            c.lookDX += dx * ks;
            c.lookDY += -dy * ks * (s.invertY ? -1.0f : 1.0f);
            if (t.ended && !t.moved && plat_now() - t.t0 < 0.35) {
                c.tap = true;
                c.tapX = t.x / in.W * 2.0f - 1.0f;
                c.tapY = 1.0f - t.y / in.H * 2.0f;
            }
        } else if (t.role == TR_SLIDER) {
            const Bar& b = L.s[t.arg];
            float v;
            if (b.vertical) v = clampv(((b.y + b.h * 0.5f) - t.y) / (b.h * 0.5f), -1.0f, 1.0f);
            else v = clampv((t.x - (b.x + b.w * 0.5f)) / (b.w * 0.5f), -1.0f, 1.0f);
            if (std::fabs(v) < 0.1f) v = 0;
            g_slider[t.arg] = t.ended ? 0.0f : v;
        }
    }
    // sliders spring back when no finger holds them
    for (int i = 0; i < HS_COUNT; i++) {
        bool held = false;
        for (TouchPt& t : in.t)
            if (t.active && !t.ended && t.role == TR_SLIDER && t.arg == i) held = true;
        if (!held) g_slider[i] = 0;
    }
    if (operating) {
        if (g_slider[HS_SLEW] != 0) c.craneSlew = -g_slider[HS_SLEW];
        if (g_slider[HS_TROLLEY] != 0) c.craneTrolley = g_slider[HS_TROLLEY];
        if (g_slider[HS_HOIST] != 0) c.craneHoist = g_slider[HS_HOIST];
    }
    if (g.pstate != PS_NORMAL) g_runToggle = false;
    // keyboard sprint never toggles the touch run button
    if (g_hasPending) {
        if (g_pending.keypad >= 0) c.keypad = g_pending.keypad;
        c.back |= g_pending.back;
        c.confirm |= g_pending.confirm;
        c.interact |= g_pending.interact;
        c.journal |= g_pending.journal;
        c.pause |= g_pending.pause;
        g_pending = Controls();
        g_hasPending = false;
    }
    // scrolling of notes and the journal is handled by the UI itself
    if (modal) c.scroll = 0;
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------
namespace {

void panel(float x, float y, float w, float h, float a = 0.9f) {
    rect(x, y, w, h, col(C_PANEL, a));
    frame(x, y, w, h, std::max(1.0f, U), col(0x3c3a34, a));
}

void draw_title_logo(float cx, float y, float size, float a) {
    float t = g_titleT;
    float flick = (hash2f((int)(t * 9.0f), 4) > 0.94f) ? 0.55f : 1.0f;
    text(FONT_SANS, "THE 1982", cx, y, size * 0.42f, col(C_DIM, a), 1, size * 0.12f);
    text(FONT_SANS, "CRANE", cx + size * 0.03f, y + size * 1.02f + size * 0.03f, size * 1.25f, col(0x000000, 0.6f * a), 1, size * 0.18f);
    text(FONT_SANS, "CRANE", cx, y + size * 1.02f, size * 1.25f, col(C_INK, a * flick), 1, size * 0.18f);
    float lw = textW(FONT_SANS, "CRANE", size * 1.25f) + size * 0.18f * 4;
    rect(cx - lw * 0.5f, y + size * 1.25f, lw, std::max(2.0f, size * 0.05f), col(C_RED, a));
}

std::string fmt_time(double secs) {
    int s = (int)secs;
    return strf("%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
}

void help_text(std::string& out, bool touch) {
    if (touch)
        out = "CONTROLS (TOUCH)\n"
              "Left thumb: drag anywhere on the left to walk.\n"
              "Right thumb: drag to look around. Tap an object to use it.\n"
              "USE: interact with what is in the middle of the screen.\n"
              "LAMP: flashlight.  RUN: toggle running.  LOW: crouch.\n"
              "Top right: JOURNAL (objectives, map, documents) and PAUSE.\n"
              "In the crane cab: SLEW, TROLLEY and HOIST levers spring back when you let go. RELEASE lets go of the load.\n\n";
    else
        out = "CONTROLS\n"
              "W A S D  walk     MOUSE  look     SHIFT  run     C  crouch\n"
              "E or CLICK  use / pick up / open     F  flashlight\n"
              "TAB  journal (objectives, map, documents, tapes)     ESC  pause\n"
              "In the crane cab:  A D  slew   W S  trolley   R F  hoist   SPACE  release   Q  let go of the levers\n\n";
    out += "HOW TO SURVIVE\n"
           "It hunts by light and sound. Your flashlight and the yard lamps make you visible from far away. Walk, do not run. Crouch to be quiet.\n\n"
           "If it sees you, break line of sight and get into a locker. It will not open a door it did not see you close.\n\n"
           "It will not follow you onto the barge, or far up the crane.\n\n"
           "Batteries run down. Spare packs are hidden around the yard.\n\n"
           "Read everything. The yard tells you what it wants.\n\n"
           "The game saves at every new objective.";
}

void draw_scroll_text(int font, const std::string& body, float x, float y, float w, float h, float size, u32 color, float& scroll, InputState& in) {
    wrap(font, body, size, w, g_lines);
    float lh = size * 1.42f;
    float total = g_lines.size() * lh + size;
    float maxS = std::max(0.0f, total - h);
    // drag / wheel
    u32 id = hid("scroll", x, y);
    if (g_ptr.has && g_ptr.pressed && inside(g_ptr.x, g_ptr.y, x, y, w, h)) { g_dragId = id; g_dragLast = g_ptr.y; }
    if (g_dragId == id && g_ptr.down) { scroll -= g_ptr.y - g_dragLast; g_dragLast = g_ptr.y; }
    scroll += in.wheel * 0.6f;
    float kb = (in.down[K_DOWN] || in.down[K_S] ? 1.0f : 0.0f) - (in.down[K_UP] || in.down[K_W] ? 1.0f : 0.0f);
    scroll += kb * 9.0f;
    scroll = clampv(scroll, 0.0f, maxS);
    clip(x - 4, y, w + 8, h);
    float yy = y + size - scroll;
    for (const std::string& l : g_lines) {
        if (yy > y - lh && yy < y + h + lh) text(font, l, x, yy, size, color);
        yy += lh;
    }
    noclip();
    if (maxS > 1.0f) {
        float th = std::max(20.0f * U, h * h / total);
        float ty = y + (h - th) * (scroll / maxS);
        rect(x + w + 8 * U, y, 3 * U, h, col(0x000000, 0.15f));
        rect(x + w + 8 * U, ty, 3 * U, th, col(0x000000, 0.45f));
        if (scroll < maxS - 2) text(FONT_SANS, "v", x + w * 0.5f, y + h + size * 0.9f, size * 0.8f, fade(color, 0.6f), 1);
    }
}

void draw_settings(Settings& s, float x, float y, float w, InputState& in, bool inGame) {
    (void)in;
    float rh = 40.0f * U;
    float yy = y;
    s.lookSens = 0.2f + 2.8f * slider("Mouse sensitivity", strf("%.2f", s.lookSens).c_str(), x, yy, w, rh, (s.lookSens - 0.2f) / 2.8f);
    yy += rh;
    s.touchSens = 0.2f + 2.8f * slider("Touch look speed", strf("%.2f", s.touchSens).c_str(), x, yy, w, rh, (s.touchSens - 0.2f) / 2.8f);
    yy += rh;
    if (toggleRow("Invert look", s.invertY ? "ON" : "OFF", x, yy, w, rh)) s.invertY = !s.invertY;
    yy += rh;
    static const char* qn[4] = {"AUTO", "LOW", "MEDIUM", "HIGH"};
    if (toggleRow("Graphics", qn[s.quality + 1], x, yy, w, rh)) s.quality = s.quality >= 2 ? -1 : s.quality + 1;
    yy += rh;
    {
        float v = s.renderScale < 0 ? 0.0f : (s.renderScale - 0.4f) / 0.6f;
        std::string vt = s.renderScale < 0 ? "AUTO" : strf("%d%%", (int)(s.renderScale * 100.0f + 0.5f));
        v = slider("Resolution", vt.c_str(), x, yy, w, rh, v);
        if (g_dragId == hid("Resolution", x, yy) && g_ptr.down) s.renderScale = v < 0.02f ? -1.0f : 0.4f + 0.6f * v;
    }
    yy += rh;
    s.brightness = 0.6f + 1.2f * slider("Brightness", strf("%d%%", (int)(s.brightness * 100.0f + 0.5f)).c_str(), x, yy, w, rh, (s.brightness - 0.6f) / 1.2f);
    yy += rh;
    s.volume = slider("Volume", strf("%d%%", (int)(s.volume * 100.0f + 0.5f)).c_str(), x, yy, w, rh, s.volume);
    yy += rh;
    static const char* tn[3] = {"SMALL", "MEDIUM", "LARGE"};
    if (toggleRow("Text size", tn[s.textSize], x, yy, w, rh)) s.textSize = (s.textSize + 1) % 3;
    yy += rh;
    if (toggleRow("Head bob", s.headBob ? "ON" : "REDUCED", x, yy, w, rh)) s.headBob = !s.headBob;
    yy += rh;
    if (toggleRow("Show frame rate", s.showFps ? "ON" : "OFF", x, yy, w, rh)) s.showFps = !s.showFps;
    yy += rh;
    (void)inGame;
}

// --------------------------------------------------------------------------- loading
void draw_loading(const LoadState& ls) {
    rect(0, 0, g_W, g_H, col(0x050505));
    float cx = g_W * 0.5f, cy = g_H * 0.42f;
    float sz = std::min(g_W * 0.11f, 70.0f * U);
    draw_title_logo(cx, cy - sz * 0.9f, sz, 1.0f);
    float bw = std::min(g_W * 0.6f, 420.0f * U), bh = 4.0f * U;
    float by = cy + sz * 1.3f;
    rect(cx - bw * 0.5f, by, bw, bh, col(0x2a2824));
    rect(cx - bw * 0.5f, by, bw * saturate(ls.progress), bh, col(ls.error ? C_RED : C_AMBER));
    text(FONT_MONO, ls.label, cx, by + 26.0f * U, 14.0f * TK, col(C_DIM), 1);
    text(FONT_SANS, "Best played with headphones, in the dark.", cx, g_H - 28.0f * U, 13.0f * TK, col(0x5a574e), 1);
}

// --------------------------------------------------------------------------- title
void draw_title(Game& g, Settings& s, InputState& in, float dt) {
    g_titleT += dt;
    float cx = g_W * 0.5f;
    bool portrait = g_H > g_W * 1.1f;
    // darken so the menu reads over the yard
    rect(0, 0, g_W, g_H, col(0x000000, 0.35f));
    float sz = std::min(g_W * (portrait ? 0.16f : 0.085f), 66.0f * U);
    float logoY = portrait ? g_H * 0.16f : g_H * 0.13f;
    if (g_titlePage == TP_GATE) {
        draw_title_logo(cx, g_H * 0.3f, sz * 1.15f, 1.0f);
        g_gateHint += dt;
        float a = 0.45f + 0.45f * std::sin(g_gateHint * 2.4f);
        text(FONT_SANS, in.touchMode ? "TAP TO BEGIN" : "CLICK TO BEGIN", cx, g_H * 0.68f, 20.0f * TK, col(C_INK, a), 1, 3.0f * U);
        text(FONT_SANS, "Best played with headphones, in the dark.", cx, g_H * 0.68f + 34.0f * U, 13.0f * TK, col(C_DIM, 0.8f), 1);
        if (in.touchMode && g_H > g_W) text(FONT_SANS, "Turn your phone sideways for the best view.", cx, g_H * 0.68f + 56.0f * U, 13.0f * TK, col(C_DIM, 0.8f), 1);
        if (g_ptr.released || in.pressed[K_SPACE] || in.pressed[K_ENTER] || in.pressed[K_E]) {
            g_titlePage = TP_MAIN;
            audio_play(S_UI, 0.6f);
        }
        return;
    }
    draw_title_logo(cx, logoY, sz, 1.0f);
    text(FONT_MONO, "BERTH 9 - PORT OF ASHGROVE - NOVEMBER 11, 1982", cx, logoY + sz * 1.62f, 12.0f * TK, col(C_DIM), 1);
    float bw = std::min(g_W - 40.0f * U, 340.0f * U), bh = 46.0f * U, gap = 10.0f * U;
    float by = logoY + sz * 1.62f + 34.0f * U;
    float bx = cx - bw * 0.5f;
    if (g_titlePage == TP_MAIN) {
        if (g.hasSave && !g.saveData.empty()) {
            g_lockRects.push_back({bx, by, bw, bh});
            if (button("CONTINUE SHIFT", bx, by, bw, bh, true, true)) {
                std::string sv = g.saveData;
                if (!game_load(g, sv)) game_new(g, g.difficulty);
            }
            by += bh + gap;
        }
        if (button("NEW SHIFT", bx, by, bw, bh, true, !g.hasSave)) g_titlePage = TP_NEW;
        by += bh + gap;
        if (button("SETTINGS", bx, by, bw, bh)) g_titlePage = TP_SETTINGS;
        by += bh + gap;
        if (button("HOW TO PLAY", bx, by, bw, bh)) g_titlePage = TP_HELP;
        by += bh + gap;
        if (in.touchMode) {
            g_fsRect[0] = bx; g_fsRect[1] = by; g_fsRect[2] = bw; g_fsRect[3] = bh;
            button("FULLSCREEN", bx, by, bw, bh);
            by += bh + gap;
        } else g_fsRect[0] = -1;
        text(FONT_SANS, "An original horror game. All textures, models and sounds are generated in C++ at load.", cx, g_H - 16.0f * U, 11.0f * TK,
             col(0x5a574e), 1);
    } else if (g_titlePage == TP_NEW || g_titlePage == TP_CONFIRM) {
        g_fsRect[0] = -1;
        if (g_titlePage == TP_NEW) {
            text(FONT_SANS, "CHOOSE YOUR SHIFT", cx, by + 4 * U, 16.0f * TK, col(C_DIM), 1, 2.0f);
            by += 22.0f * U;
            for (int d = 0; d < 3; d++) {
                if (!(g.hasSave && !g.saveData.empty())) g_lockRects.push_back({bx, by, bw, bh});
                if (button(DIFF_NAME[d], bx, by, bw, bh, true, d == 1)) {
                    g_pendingDifficulty = d;
                    if (g.hasSave && !g.saveData.empty()) g_titlePage = TP_CONFIRM;
                    else { game_new(g, d); g_titlePage = TP_MAIN; }
                }
                wrap(FONT_SANS, DIFF_DESC[d], 12.0f * TK, bw, g_lines);
                float ty = by + bh + 15.0f * U;
                for (auto& l : g_lines) { text(FONT_SANS, l, cx, ty, 12.0f * TK, col(C_DIM), 1); ty += 15.0f * TK; }
                by = ty + 4.0f * U;
            }
            if (button("BACK", bx, by, bw, bh * 0.85f) || in.pressed[K_ESC]) g_titlePage = TP_MAIN;
        } else {
            text(FONT_SANS, "Start over? Your shift in progress will be lost.", cx, by + 10 * U, 16.0f * TK, col(C_INK), 1);
            by += 34.0f * U;
            g_lockRects.push_back({bx, by, bw, bh});
            if (button("START A NEW SHIFT", bx, by, bw, bh, true, true)) { game_new(g, g_pendingDifficulty); g_titlePage = TP_MAIN; }
            by += bh + gap;
            if (button("KEEP MY SHIFT", bx, by, bw, bh) || in.pressed[K_ESC]) g_titlePage = TP_MAIN;
        }
    } else if (g_titlePage == TP_SETTINGS) {
        g_fsRect[0] = -1;
        float pw = std::min(g_W - 24.0f * U, 520.0f * U);
        float ph = std::min(g_H - by - 10.0f * U, 40.0f * U * 10 + 90.0f * U);
        float py = std::max(10.0f * U, std::min(by - 10.0f * U, g_H - ph - 10.0f * U));
        if (ph < 300.0f * U) { py = 10.0f * U; ph = g_H - 20.0f * U; }
        panel(cx - pw * 0.5f, py, pw, ph);
        static float scroll = 0;
        float innerH = ph - 76.0f * U;
        float contentH = 40.0f * U * 10;
        u32 id = hid("settings-scroll", 0, 0);
        if (g_ptr.pressed && inside(g_ptr.x, g_ptr.y, cx - pw * 0.5f, py, pw * 0.36f, ph)) { g_dragId = id; g_dragLast = g_ptr.y; }
        if (g_dragId == id && g_ptr.down) { scroll -= g_ptr.y - g_dragLast; g_dragLast = g_ptr.y; }
        scroll = clampv(scroll + in.wheel * 0.5f, 0.0f, std::max(0.0f, contentH - innerH));
        clip(cx - pw * 0.5f, py + 12 * U, pw, innerH);
        draw_settings(s, cx - pw * 0.5f + 22 * U, py + 16 * U - scroll, pw - 44 * U, in, false);
        noclip();
        if (button("DONE", cx - 80 * U, py + ph - 56 * U, 160 * U, 42 * U, true, true) || in.pressed[K_ESC]) g_titlePage = TP_MAIN;
    } else if (g_titlePage == TP_HELP) {
        g_fsRect[0] = -1;
        float pw = std::min(g_W - 24.0f * U, 640.0f * U);
        float py = 12.0f * U, ph = g_H - 24.0f * U;
        panel(cx - pw * 0.5f, py, pw, ph);
        std::string h;
        help_text(h, in.touchMode);
        static float scroll = 0;
        draw_scroll_text(FONT_SANS, h, cx - pw * 0.5f + 22 * U, py + 18 * U, pw - 56 * U, ph - 90 * U, 14.5f * TK, col(C_INK), scroll, in);
        if (button("BACK", cx - 80 * U, py + ph - 56 * U, 160 * U, 42 * U, true, true) || in.pressed[K_ESC]) g_titlePage = TP_MAIN;
    }
}

// --------------------------------------------------------------------------- note reader
void draw_note(Game& g, InputState& in) {
    rect(0, 0, g_W, g_H, col(0x000000, 0.72f));
    bool tape = g.noteOpen >= 100;
    int id = g.noteOpen;
    std::string title = tape ? tape_title(id - 100) : note_title(id);
    std::string body = tape ? std::string("(transcript)\n\n") + tape_text(id - 100) : note_text(g, id);
    float pw = std::min(g_W - 20.0f * U, 620.0f * U), ph = std::min(g_H - 20.0f * U, 640.0f * U);
    float px = (g_W - pw) * 0.5f, py = (g_H - ph) * 0.5f;
    // paper with a little shadow and a torn feel
    rect(px + 6 * U, py + 8 * U, pw, ph, col(0x000000, 0.5f));
    rect(px, py, pw, ph, col(tape ? 0xb8b3a2 : C_PAPER));
    for (int i = 0; i < 9; i++) {
        float yy = py + ph * (0.08f + 0.1f * i) + 7.0f * hash2f(i, 3);
        rect(px, yy, pw, 1.0f, col(0x6a5a40, 0.08f));
    }
    rect(px + pw - 70 * U, py + ph * 0.6f, 50 * U, 50 * U, col(0x7a5a30, 0.06f));
    float ts = 16.0f * TK;
    text(FONT_SANS, title, px + 22 * U, py + 34 * U, ts * 1.2f, col(0x4a1e18));
    rect(px + 22 * U, py + 44 * U, pw - 44 * U, 1.5f * U, col(0x4a1e18, 0.5f));
    static float lastScroll = 0;
    float sc = g.noteScroll;
    draw_scroll_text(FONT_MONO, body, px + 22 * U, py + 56 * U, pw - 50 * U, ph - 110 * U, ts, col(C_PINK), sc, in);
    g.noteScroll = sc;
    (void)lastScroll;
    const char* closeLbl = in.touchMode ? "CLOSE" : "CLOSE  [E]";
    if (button(closeLbl, px + pw * 0.5f - 85 * U, py + ph - 48 * U, 170 * U, 38 * U, true, true)) { g_pending.confirm = true; g_hasPending = true; }
}

// --------------------------------------------------------------------------- keypad
void draw_keypad(Game& g, InputState& in) {
    (void)in;
    rect(0, 0, g_W, g_H, col(0x000000, 0.6f));
    float kw = std::min(g_W - 30.0f * U, 300.0f * U), bh = std::min(58.0f * U, (g_H - 170.0f * U) / 4.5f);
    float kh = 110.0f * U + bh * 4 + 70.0f * U;
    float kx = (g_W - kw) * 0.5f, ky = std::max(8.0f, (g_H - kh) * 0.5f);
    rect(kx, ky, kw, kh, col(0x1c1d1b, 0.98f));
    frame(kx, ky, kw, kh, 2 * U, col(0x4a4a44));
    text(FONT_SANS, "KEY CABINET - ENTER CODE", kx + kw * 0.5f, ky + 26 * U, 13.0f * TK, col(C_DIM), 1);
    rect(kx + 20 * U, ky + 40 * U, kw - 40 * U, 52 * U, col(0x0a0302));
    std::string disp;
    for (int i = 0; i < 4; i++) disp += i < (int)g.keypadEntry.size() ? g.keypadEntry[(size_t)i] : '-';
    u32 dc = g.keypadMsg == 1 ? col(0xff3010) : (g.keypadMsg == 2 ? col(0x40ff60) : col(0xd03a18));
    if (g.keypadMsg == 1 && g.keypadMsgT > 0) disp = "ERR ";
    if (g.keypadMsg == 2) disp = "OPEN";
    text(FONT_MONO, disp, kx + kw * 0.5f, ky + 78 * U, 34.0f * U, dc, 1, 10.0f * U);
    static const char* labels[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "CLR", "0", "OK"};
    static const int codes[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 11};
    float gx = kx + 20 * U, gy = ky + 104 * U, bw = (kw - 40 * U - 16 * U) / 3.0f;
    for (int i = 0; i < 12; i++) {
        float bx = gx + (i % 3) * (bw + 8 * U), byy = gy + (i / 3) * (bh + 8 * U);
        if (button(labels[i], bx, byy, bw, bh, g.keypadMsg != 2)) { g_pending.keypad = codes[i]; g_hasPending = true; }
    }
    if (button("STEP AWAY", kx + 20 * U, ky + kh - 52 * U, kw - 40 * U, 40 * U)) { g_pending.back = true; g_hasPending = true; }
}

// --------------------------------------------------------------------------- journal
struct MapLabel { const char* name; float x, z; };
const MapLabel MAP_LABELS[] = {
    {"GATE", 0, 54.0f}, {"BOOTH", 15.0f, 59.5f}, {"YOUR TRUCK", -3.3f, 71.0f}, {"SITE OFFICE", 14.2f, 44.0f}, {"CREW ROOM", 21.5f, -14.5f},
    {"WORKSHOP", 33.5f, 9.5f}, {"GENERATOR", 34.0f, 31.0f}, {"DIESEL", 34.0f, 14.6f}, {"TRANSFORMER", 57.0f, 23.5f}, {"PUMP HOUSE 2", 61.0f, -36.0f},
    {"CRANE 7", CRANE_POS.x, CRANE_POS.z + 7.6f}, {"BARGE H-12", 27.0f, -64.0f}, {"THE LIFTS", -41.0f, -38.6f}, {"CONTAINER STACKS", -48.5f, 48.6f},
    {"STORES", -54.9f, 8.2f},
};
enum { ML_GATE = 0, ML_BOOTH, ML_TRUCK, ML_OFFICE, ML_CREW, ML_SHOP, ML_GEN, ML_DIESEL, ML_TRANS, ML_PUMP, ML_CRANE, ML_BARGE, ML_LIFTS, ML_STACKS, ML_STORES };

void objective_marks(const Game& g, std::vector<Vec3>& out) {
    out.clear();
    auto add = [&](int ml) {
        if (ml == ML_CRANE) out.push_back(CRANE_POS);
        else if (ml == ML_STORES) out.push_back(Vec3(-54.9f, 0, 11.0f));
        else if (ml == ML_BARGE) out.push_back(Vec3(34.0f, 0, -56.0f));
        else if (ml == ML_OFFICE) out.push_back(Vec3(14.2f, 0, 34.0f));
        else if (ml == ML_CREW) out.push_back(Vec3(21.5f, 0, -3.0f));
        else if (ml == ML_SHOP) out.push_back(Vec3(33.5f, 0, -3.0f));
        else if (ml == ML_GEN) out.push_back(Vec3(34.0f, 0, 24.0f));
        else if (ml == ML_DIESEL) out.push_back(Vec3(30.4f, 0, 17.0f));
        else if (ml == ML_PUMP) out.push_back(Vec3(48.0f, 0, -36.0f));
        else if (ml == ML_BOOTH) out.push_back(Vec3(7.2f, 0, 58.6f));
        else if (ml == ML_TRUCK) out.push_back(Vec3(-3.3f, 0, 67.4f));
        else if (ml == ML_GATE) out.push_back(Vec3(0, 0, 56.0f));
    };
    switch (g.stage) {
    case ST_FLASHLIGHT: add(ML_TRUCK); break;
    case ST_BOOTH: case ST_GATE: add(ML_BOOTH); break;
    case ST_OFFICE_GO: case ST_OFFICE_ENTER: add(ML_OFFICE); break;
    case ST_OFFICE_KEY: add(ML_CREW); break;
    case ST_POWER:
        if (!g.itemTaken[IT_FUSE_A] || (!g.have[IT_JERRYCAN] && !g.flags[F_GEN_FUELED])) add(ML_SHOP);
        if (!g.itemTaken[IT_FUSE_B]) add(ML_STORES);
        if (!g.itemTaken[IT_FUSE_C]) add(ML_BARGE);
        if (g.have[IT_JERRYCAN] && !g.flags[F_CAN_FULL] && !g.flags[F_GEN_FUELED]) add(ML_DIESEL);
        add(ML_GEN);
        break;
    case ST_KEYCAB:
        if (!g.noteRead[N_TIMECARD]) add(ML_CREW);
        add(ML_OFFICE);
        break;
    case ST_BRAKE:
        if (!g.have[IT_CUTTERS]) add(ML_SHOP);
        if (!g.flags[F_PUMP_ON]) add(ML_PUMP);
        add(ML_CRANE);
        break;
    case ST_CLIMB: add(ML_CRANE); break;
    case ST_ESCAPE: add(ML_GATE); add(ML_TRUCK); break;
    case ST_OPEN: out.push_back(pad_pos()); break;
    default: break;
    }
}

void draw_map(Game& g, float x, float y, float w, float h) {
    const World& wd = g.w;
    rect(x, y, w, h, col(C_BLUEPRINT));
    const float wx0 = -86, wx1 = 86, wz0 = -66, wz1 = 74;
    float sc = std::min(w / (wx1 - wx0), h / (wz1 - wz0));
    float ox = x + (w - (wx1 - wx0) * sc) * 0.5f, oy = y + (h - (wz1 - wz0) * sc) * 0.5f;
    auto P = [&](float wx, float wz) { return Vec2(ox + (wx - wx0) * sc, oy + (wz - wz0) * sc); };
    clip(x, y, w, h);
    // grid
    for (float gx = -80; gx <= 80; gx += 20) { Vec2 a = P(gx, wz0), b = P(gx, wz1); line(a.x, a.y, b.x, b.y, 1.0f, col(C_BPINK, 0.08f)); }
    for (float gz = -60; gz <= 70; gz += 20) { Vec2 a = P(wx0, gz), b = P(wx1, gz); line(a.x, a.y, b.x, b.y, 1.0f, col(C_BPINK, 0.08f)); }
    // water
    {
        Vec2 a = P(wx0, wz0), b = P(wx1, QUAY_Z);
        rect(a.x, a.y, b.x - a.x, b.y - a.y, col(0x0a1a34, 0.9f));
        for (int i = 0; i < 6; i++) {
            float wz = QUAY_Z - 3.0f - i * 3.0f;
            Vec2 c = P(wx0, wz), d = P(wx1, wz);
            line(c.x, c.y, d.x, d.y, 1.0f, col(C_BPINK, 0.06f));
        }
    }
    // everything solid and tall, from the collision data itself
    for (const Collider& c : wd.colliders) {
        if (!(c.flags & COL_SOLID)) continue;
        float ht = c.box.mx.y - c.box.mn.y;
        if (ht < 1.0f || c.box.mx.y < 1.0f) continue;
        float ex = c.box.mx.x - c.box.mn.x, ez = c.box.mx.z - c.box.mn.z;
        if (ex > 100 || ez > 100) continue;
        Vec2 a = P(c.box.mn.x, c.box.mn.z), b = P(c.box.mx.x, c.box.mx.z);
        bool fence = !(c.flags & COL_SIGHT);
        rect(a.x, a.y, std::max(1.0f, b.x - a.x), std::max(1.0f, b.y - a.y), col(C_BPINK, fence ? 0.18f : 0.42f));
    }
    // quay edge and rails
    { Vec2 a = P(wx0, QUAY_Z), b = P(wx1, QUAY_Z); line(a.x, a.y, b.x, b.y, 2.0f, col(C_BPINK, 0.8f)); }
    for (float rz : {-32.0f, -40.0f}) { Vec2 a = P(-74, rz), b = P(64, rz); line(a.x, a.y, b.x, b.y, 1.0f, col(C_BPINK, 0.35f)); }
    { Vec2 a = P(14, -62), b = P(40, -50); frame(a.x, a.y, b.x - a.x, b.y - a.y, 1.5f, col(C_BPINK, 0.7f)); }
    // the crane
    {
        Vec2 c = P(CRANE_POS.x, CRANE_POS.z);
        frame(c.x - 5 * sc, c.y - 5 * sc, 10 * sc, 10 * sc, 1.5f, col(C_BPINK, 0.9f));
        float sl = g.crane.slew;
        Vec2 j = P(CRANE_POS.x + std::cos(sl) * 44.0f, CRANE_POS.z - std::sin(sl) * 44.0f);
        Vec2 cj = P(CRANE_POS.x - std::cos(sl) * 15.0f, CRANE_POS.z + std::sin(sl) * 15.0f);
        line(cj.x, cj.y, j.x, j.y, 2.0f, col(C_BPINK, 0.55f));
    }
    // labels
    float ls = std::max(8.5f, 10.0f * TK * std::min(1.0f, w / 520.0f));
    for (const MapLabel& m : MAP_LABELS) {
        Vec2 p = P(m.x, m.z);
        text(FONT_SANS, m.name, p.x, p.y + ls * 0.35f, ls, col(C_BPINK, 0.95f), 1);
    }
    // the red pen
    static std::vector<Vec3> marks;
    objective_marks(g, marks);
    for (const Vec3& mk : marks) {
        Vec2 p = P(mk.x, mk.z);
        float r = 7.0f * sc + 6.0f;
        ring(p.x, p.y, r, 2.0f, col(0xe0402a, 0.9f), 24);
        ring(p.x + 1.2f, p.y - 0.8f, r * 1.08f, 1.2f, col(0xe0402a, 0.6f), 24);
    }
    // you are here
    if (g.pos.z < wz1 + 5) {
        Vec2 p = P(g.pos.x, g.pos.z);
        float yaw = g.yaw;
        float fx = std::sin(yaw), fz = -std::cos(yaw);
        float r = 7.0f;
        Vec2 tip(p.x + fx * r * 1.6f, p.y + fz * r * 1.6f);
        Vec2 l(p.x + (-fz) * r * 0.8f - fx * r * 0.6f, p.y + fx * r * 0.8f - fz * r * 0.6f);
        Vec2 rr(p.x - (-fz) * r * 0.8f - fx * r * 0.6f, p.y - fx * r * 0.8f - fz * r * 0.6f);
        tri(tip.x, tip.y, l.x, l.y, rr.x, rr.y, col(C_AMBER));
        if (std::fmod((float)g.time, 1.0f) < 0.6f) ring(p.x, p.y, 10.0f, 1.5f, col(C_AMBER, 0.6f), 20);
    }
    noclip();
    text(FONT_SANS, "N", x + w - 16 * U, y + 20 * U, 14 * TK, col(C_BPINK, 0.8f), 1);
    line(x + w - 16 * U, y + 24 * U, x + w - 16 * U, y + 40 * U, 1.5f, col(C_BPINK, 0.8f));
}

void draw_journal(Game& g, InputState& in) {
    rect(0, 0, g_W, g_H, col(0x000000, 0.78f));
    float pw = std::min(g_W - 16.0f * U, 760.0f * U), ph = std::min(g_H - 16.0f * U, 620.0f * U);
    float px = (g_W - pw) * 0.5f, py = (g_H - ph) * 0.5f;
    panel(px, py, pw, ph, 0.96f);
    static const char* tabs[5] = {"OBJECTIVE", "MAP", "DOCUMENTS", "TAPES", "ITEMS"};
    float tw = (pw - 24 * U) / 5.0f, th = 38.0f * U;
    for (int i = 0; i < 5; i++) {
        float tx = px + 12 * U + i * tw;
        bool sel = g.journalTab == i;
        if (button(tabs[i], tx + 2 * U, py + 10 * U, tw - 4 * U, th, true, sel)) { g.journalTab = i; g.noteScroll = 0; }
    }
    for (int d = 1; d <= 5; d++)
        if (in.pressed[K_0 + d]) { g.journalTab = d - 1; g.noteScroll = 0; }
    if (in.pressed[K_RIGHT] || in.pressed[K_D]) g.journalTab = (g.journalTab + 1) % 5;
    if (in.pressed[K_LEFT] || in.pressed[K_A]) g.journalTab = (g.journalTab + 4) % 5;
    float cx = px + 22 * U, cy = py + 10 * U + th + 18 * U, cw = pw - 44 * U, ch = ph - (th + 28 * U) - 70 * U;
    float ts = 15.0f * TK;
    switch (g.journalTab) {
    case 0: {
        float yy = cy + ts;
        text(FONT_SANS, strf("NOVEMBER 11, 1982   -   %s", g.difficulty == 0 ? "STORY" : g.difficulty == 1 ? "STANDARD" : "NIGHTMARE"), cx, yy, ts * 0.8f, col(C_DIM));
        yy += ts * 2.0f;
        wrap(FONT_SANS, stage_title(g), ts * 1.35f, cw, g_lines);
        for (auto& l : g_lines) { text(FONT_SANS, l, cx, yy, ts * 1.35f, col(C_AMBER)); yy += ts * 1.8f; }
        yy += ts * 0.4f;
        std::vector<std::string> lines;
        std::vector<int> done;
        objective_lines(g, lines, done);
        for (size_t i = 0; i < lines.size(); i++) {
            float bx = cx, by = yy - ts * 0.8f;
            frame(bx, by, ts * 0.9f, ts * 0.9f, 1.5f, col(C_INK, done[i] ? 0.5f : 0.9f));
            if (done[i]) { line(bx + 2, by + ts * 0.45f, bx + ts * 0.35f, by + ts * 0.8f, 2.0f, col(C_GREEN)); line(bx + ts * 0.35f, by + ts * 0.8f, bx + ts * 0.85f, by + 1, 2.0f, col(C_GREEN)); }
            wrap(FONT_SANS, lines[i], ts, cw - ts * 1.6f, g_lines);
            for (auto& l : g_lines) { text(FONT_SANS, l, cx + ts * 1.5f, yy, ts, col(done[i] ? C_DIM : C_INK)); yy += ts * 1.45f; }
            yy += ts * 0.3f;
        }
        yy += ts * 0.6f;
        std::string st = strf("Documents %d/%d    Tapes %d/6    Time on shift %s", notes_found(g), (int)N_COUNT, tapes_found(g), fmt_time(g.time).c_str());
        text(FONT_SANS, st, cx, std::min(yy, cy + ch), ts * 0.8f, col(C_DIM));
        break;
    }
    case 1:
        draw_map(g, cx, cy, cw, ch);
        text(FONT_SANS, "Red circles: where the foreman's notes point you next.", cx, cy + ch + ts * 1.2f, ts * 0.78f, col(C_DIM));
        break;
    case 2:
    case 3: {
        bool tapes = g.journalTab == 3;
        static float scroll = 0;
        float rowH = 40.0f * U;
        int n = tapes ? 6 : N_COUNT;
        int shown = 0;
        u32 sid = hid(tapes ? "tapes" : "docs", 0, 0);
        if (g_ptr.pressed && inside(g_ptr.x, g_ptr.y, cx, cy, cw, ch)) { g_dragId = sid; g_dragLast = g_ptr.y; }
        float dragged = 0;
        if (g_dragId == sid && g_ptr.down) { dragged = g_ptr.y - g_dragLast; scroll -= dragged; g_dragLast = g_ptr.y; }
        int total = 0;
        for (int i = 0; i < n; i++) total += tapes ? (g.itemTaken[IT_TAPE1 + i] ? 1 : 0) : (g.noteRead[i] ? 1 : 0);
        scroll = clampv(scroll + in.wheel * 0.5f, 0.0f, std::max(0.0f, total * rowH - ch));
        clip(cx - 4, cy, cw + 8, ch);
        for (int i = 0; i < n; i++) {
            bool have = tapes ? g.itemTaken[IT_TAPE1 + i] : g.noteRead[i];
            if (!have) continue;
            float ry = cy + shown * rowH - scroll;
            shown++;
            if (ry + rowH < cy || ry > cy + ch) continue;
            const char* name = tapes ? tape_title(i) : note_title(i);
            if (button(name, cx, ry + 2 * U, cw, rowH - 6 * U)) {
                if (std::fabs(dragged) < 4.0f) {
                    g.noteOpen = tapes ? 100 + i : i;
                    g.noteScroll = 0;
                    g.modal = MD_NOTE;
                    g.noteReturnJournal = true;
                    audio_play(S_PAPER, 0.6f);
                }
            }
        }
        noclip();
        if (shown == 0) text(FONT_SANS, tapes ? "No tapes found yet. Kessler recorded a log every night." : "Nothing read yet.", cx, cy + ts * 1.5f, ts, col(C_DIM));
        else text(FONT_SANS, strf("%d of %d found", shown, n), cx, cy + ch + ts * 1.2f, ts * 0.78f, col(C_DIM));
        break;
    }
    case 4: {
        float yy = cy + ts;
        auto item = [&](const std::string& s, u32 c = C_INK) {
            text(FONT_SANS, s, cx, yy, ts, col(c));
            yy += ts * 1.6f;
        };
        if (g.have[IT_FLASHLIGHT]) item(strf("Flashlight - battery %d%%", (int)(g.battery * 100.0f + 0.5f)), g.battery < 0.15f ? C_RED : C_INK);
        item(strf("Spare batteries: %d", g.spareBatteries), C_INK);
        if (g.have[IT_RADIO]) item("Radio handset - yard channel");
        if (g.have[IT_OFFICE_KEY]) item("Site office key");
        int fuses = (g.have[IT_FUSE_A] ? 1 : 0) + (g.have[IT_FUSE_B] ? 1 : 0) + (g.have[IT_FUSE_C] ? 1 : 0);
        if (fuses) item(strf("30 A lighting fuses: %d", fuses));
        if (g.fusesInstalled > 0 && g.fusesInstalled < 3) item(strf("Fuses seated in the panel: %d of 3", g.fusesInstalled), C_DIM);
        if (g.have[IT_JERRYCAN] && !g.flags[F_GEN_FUELED]) item(g.flags[F_CAN_FULL] ? "Jerry can - full of diesel" : "Jerry can - empty");
        if (g.have[IT_CUTTERS]) item("Bolt cutters");
        if (g.have[IT_CRANE_KEY]) item("Crane 7 access key");
        if (g.noteRead[N_TIMECARD]) item(strf("Walt Kessler's badge number: %04d", g.keyCode), C_AMBER);
        if (g.noteRead[N_FOREMAN] || g.flags[F_VALVES_DONE]) item("Purge order (foreman's book): C - A - B", C_AMBER);
        break;
    }
    default: break;
    }
    const char* lbl = in.touchMode ? "CLOSE" : "CLOSE  [TAB]";
    if (button(lbl, px + pw * 0.5f - 85 * U, py + ph - 52 * U, 170 * U, 40 * U, true, true)) { g_pending.journal = true; g_hasPending = true; }
}

// --------------------------------------------------------------------------- pause
void draw_pause(Game& g, Settings& s, InputState& in) {
    rect(0, 0, g_W, g_H, col(0x000000, 0.7f));
    float cx = g_W * 0.5f;
    float bw = std::min(g_W - 40.0f * U, 320.0f * U), bh = 46.0f * U, gap = 10.0f * U;
    float bx = cx - bw * 0.5f;
    if (g_pausePage == PP_MAIN) {
        float by = std::max(20.0f * U, g_H * 0.5f - (bh + gap) * 2.6f);
        text(FONT_SANS, "PAUSED", cx, by - 4 * U, 26.0f * TK, col(C_INK), 1, 6.0f * U);
        text(FONT_MONO, stage_title(g), cx, by + 22 * U, 12.0f * TK, col(C_DIM), 1);
        by += 40 * U;
        g_lockRects.push_back({bx, by, bw, bh});
        if (button("RESUME", bx, by, bw, bh, true, true)) g.modal = MD_NONE;
        by += bh + gap;
        if (button("JOURNAL", bx, by, bw, bh)) { g.modal = MD_JOURNAL; g.noteScroll = 0; }
        by += bh + gap;
        if (button("SETTINGS", bx, by, bw, bh)) g_pausePage = PP_SETTINGS;
        by += bh + gap;
        if (button("HOW TO PLAY", bx, by, bw, bh)) g_pausePage = PP_HELP;
        by += bh + gap;
        if (button("QUIT TO TITLE", bx, by, bw, bh)) g_pausePage = PP_QUIT;
    } else if (g_pausePage == PP_SETTINGS) {
        float pw = std::min(g_W - 24.0f * U, 520.0f * U);
        float ph = std::min(g_H - 20.0f * U, 40.0f * U * 10 + 90.0f * U);
        float py = (g_H - ph) * 0.5f;
        panel(cx - pw * 0.5f, py, pw, ph);
        static float scroll = 0;
        float innerH = ph - 76.0f * U, contentH = 40.0f * U * 10;
        u32 id = hid("pause-settings-scroll", 0, 0);
        if (g_ptr.pressed && inside(g_ptr.x, g_ptr.y, cx - pw * 0.5f, py, pw * 0.36f, ph)) { g_dragId = id; g_dragLast = g_ptr.y; }
        if (g_dragId == id && g_ptr.down) { scroll -= g_ptr.y - g_dragLast; g_dragLast = g_ptr.y; }
        scroll = clampv(scroll + in.wheel * 0.5f, 0.0f, std::max(0.0f, contentH - innerH));
        clip(cx - pw * 0.5f, py + 12 * U, pw, innerH);
        draw_settings(s, cx - pw * 0.5f + 22 * U, py + 16 * U - scroll, pw - 44 * U, in, true);
        noclip();
        if (button("DONE", cx - 80 * U, py + ph - 56 * U, 160 * U, 42 * U, true, true)) g_pausePage = PP_MAIN;
    } else if (g_pausePage == PP_HELP) {
        float pw = std::min(g_W - 24.0f * U, 640.0f * U);
        float py = 12.0f * U, ph = g_H - 24.0f * U;
        panel(cx - pw * 0.5f, py, pw, ph);
        std::string h;
        help_text(h, in.touchMode);
        static float scroll = 0;
        draw_scroll_text(FONT_SANS, h, cx - pw * 0.5f + 22 * U, py + 18 * U, pw - 56 * U, ph - 90 * U, 14.5f * TK, col(C_INK), scroll, in);
        if (button("BACK", cx - 80 * U, py + ph - 56 * U, 160 * U, 42 * U, true, true)) g_pausePage = PP_MAIN;
    } else if (g_pausePage == PP_QUIT) {
        float by = g_H * 0.5f - 70 * U;
        text(FONT_SANS, "Quit to the title screen?", cx, by, 18.0f * TK, col(C_INK), 1);
        text(FONT_SANS, "You will continue from the last objective.", cx, by + 24 * U, 13.0f * TK, col(C_DIM), 1);
        by += 46 * U;
        if (button("QUIT", bx, by, bw, bh, true, true)) {
            g_pausePage = PP_MAIN;
            game_to_title(g, false);
            g_titlePage = TP_MAIN;
        }
        by += bh + gap;
        if (button("KEEP PLAYING", bx, by, bw, bh)) g_pausePage = PP_MAIN;
    }
    if (in.pressed[K_BACK] && g_pausePage != PP_MAIN) g_pausePage = PP_MAIN;
}

// --------------------------------------------------------------------------- HUD
void hud_button(const Circle& c, const char* label, bool active, bool highlight, float a = 1.0f) {
    if (!c.show) return;
    disc(c.x, c.y, c.r, col(active ? 0x2a2418 : 0x000000, (active ? 0.55f : 0.32f) * a));
    ring(c.x, c.y, c.r, std::max(1.5f, 2.0f * U), col(highlight ? C_AMBER : C_INK, (highlight ? 0.95f : 0.45f) * a));
    float ts = std::min(c.r * 0.5f, 13.0f * U);
    text(FONT_SANS, label, c.x, c.y + ts * 0.36f, ts, col(highlight ? C_AMBER : C_INK, (highlight ? 1.0f : 0.75f) * a), 1);
}

void draw_hud(Game& g, Settings& s, InputState& in, float dt, float fps) {
    float ts = 17.0f * TK;
    bool touch = in.touchMode;
    // stage change: show the controls card on desktop at the very start
    if (g.stage != g_lastStage) {
        if (g_lastStage == -1 && g.stage == ST_FLASHLIGHT) g_controlsCard = 14.0f;
        g_lastStage = g.stage;
    }
    g_controlsCard = std::max(0.0f, g_controlsCard - dt);
    bool alive = g.pstate != PS_DEAD && g.pstate != PS_CUTSCENE;

    // crosshair + prompt
    if (alive && (g.pstate == PS_NORMAL || g.pstate == PS_SEATED || g.pstate == PS_LADDER) && g.actionLock <= 0) {
        float cx = g_W * 0.5f, cy = g_H * 0.5f;
        if (g.promptIdx >= 0) {
            ring(cx, cy, 7.0f * U, 1.5f * U, col(C_INK, 0.85f), 20);
            std::string p = g.promptText;
            if (!touch) p = "[E]  " + p;
            textShadow(FONT_SANS, p, cx, cy + 32.0f * U, ts * 0.95f, col(C_INK, 0.95f), 1);
        } else disc(cx, cy, 1.6f * U, col(C_INK, 0.45f), 10);
    }
    // action progress
    if (g.actionLock > 0 && !g.actionText.empty()) {
        float cx = g_W * 0.5f, cy = g_H * 0.5f + 30.0f * U;
        textShadow(FONT_SANS, g.actionText, cx, cy, ts, col(C_INK), 1);
        static float total = 1.0f;
        if (g.actionLock > total - 0.01f || total < g.actionLock) total = g.actionLock;
        float bw = 160.0f * U;
        rect(cx - bw * 0.5f, cy + 10 * U, bw, 3 * U, col(0x000000, 0.6f));
        rect(cx - bw * 0.5f, cy + 10 * U, bw * (1.0f - g.actionLock / std::max(0.01f, total)), 3 * U, col(C_AMBER));
    }
    // objective banner
    if (g.toastT > 0 && !g.toast.empty()) {
        float a = std::min(1.0f, g.toastT / 0.8f) * std::min(1.0f, (6.0f - g.toastT) / 0.4f);
        float x = 18.0f * U, y = 26.0f * U + (touch ? 0.0f : 0.0f);
        text(FONT_SANS, "NEW OBJECTIVE", x, y, ts * 0.7f, col(C_AMBER, a), 0, 2.0f * U);
        wrap(FONT_SANS, g.toast, ts * 1.05f, std::min(g_W * 0.6f, 460.0f * U), g_lines);
        float yy = y + ts * 1.35f;
        for (auto& l : g_lines) { textShadow(FONT_SANS, l, x, yy, ts * 1.05f, col(C_INK, a)); yy += ts * 1.35f; }
    }
    // checkpoint indicator
    if (g.checkpointT > 0) {
        float a = std::min(1.0f, g.checkpointT) * (0.6f + 0.4f * std::sin((float)g.time * 6.0f));
        text(FONT_MONO, "PROGRESS SAVED", g_W - 18.0f * U, g_H - (touch ? g_H * 0.5f : 18.0f * U), ts * 0.7f, col(C_DIM, a), 2);
    }
    // subtitles
    {
        float y = g_H - (touch ? 128.0f : 64.0f) * U;
        const Subtitle* sub = nullptr;
        if (!g.subs.empty() && g.subs.front().delay <= 0) sub = &g.subs.front();
        if (sub) {
            float a = std::min(1.0f, sub->time / 0.35f);
            float maxW = std::min(g_W * (touch ? 0.6f : 0.72f), 760.0f * U);
            std::string sp = sub->speaker;
            u32 scol = sp == "DISPATCH" ? C_AMBER : (sp == "TAPE" ? 0x9fb8a0 : (sp == "PHONE" ? 0xd04a3a : C_INK));
            std::string body = sub->text;
            wrap(FONT_SANS, body, ts * 1.08f, maxW, g_lines);
            float lh = ts * 1.42f;
            float bh = lh * g_lines.size() + (sp.empty() ? 0.0f : lh * 0.8f) + 12 * U;
            float bw = 0;
            for (auto& l : g_lines) bw = std::max(bw, textW(FONT_SANS, l, ts * 1.08f));
            float by = y - bh;
            rect(g_W * 0.5f - bw * 0.5f - 14 * U, by, bw + 28 * U, bh + 6 * U, col(0x000000, 0.5f * a));
            float yy = by + lh * 0.95f;
            if (!sp.empty()) {
                text(FONT_SANS, sp, g_W * 0.5f, yy - lh * 0.08f, ts * 0.72f, col(scol, a), 1, 2.0f * U);
                yy += lh * 0.8f;
            }
            u32 tc = sp.empty() ? col(0xe8e2d0, a) : col(sp == "TAPE" ? 0xc8d8c0 : 0xf0e8d0, a);
            for (auto& l : g_lines) { text(FONT_SANS, l, g_W * 0.5f, yy, ts * 1.08f, tc, 1); yy += lh; }
        }
        // short hints
        if (g.hintT > 0 && !g.hint.empty()) {
            float a = std::min(1.0f, g.hintT / 0.5f);
            float hy = sub ? y - 150.0f * U : y - 10 * U;
            if (touch) hy = std::min(hy, g_H * 0.5f + 80 * U);
            wrap(FONT_SANS, g.hint, ts * 0.92f, std::min(g_W * 0.62f, 640.0f * U), g_lines);
            float yy = hy - (g_lines.size() - 1) * ts * 1.3f;
            for (auto& l : g_lines) { textShadow(FONT_SANS, l, g_W * 0.5f, yy, ts * 0.92f, col(0xc8c2b0, a), 1); yy += ts * 1.3f; }
        }
    }
    // battery + stamina (bottom left on desktop, top left on touch)
    if (alive && g.have[IT_FLASHLIGHT] && (g.pstate == PS_NORMAL || g.pstate == PS_LADDER || g.pstate == PS_HIDING)) {
        float x = 18.0f * U, y = touch ? 70.0f * U : g_H - 34.0f * U;
        if (touch && g.toastT > 0) y = 140.0f * U;
        float bw = 34.0f * U, bh = 14.0f * U;
        frame(x, y, bw, bh, 1.5f * U, col(C_INK, g.flashOn ? 0.8f : 0.4f));
        rect(x + bw, y + bh * 0.3f, 3.0f * U, bh * 0.4f, col(C_INK, g.flashOn ? 0.8f : 0.4f));
        int bars = (int)std::ceil(g.battery * 4.0f - 0.001f);
        bool low = g.battery < 0.15f;
        for (int i = 0; i < 4; i++) {
            if (i >= bars) break;
            float flick = low && std::fmod((float)g.time, 0.8f) < 0.4f ? 0.3f : 1.0f;
            rect(x + 3 * U + i * (bw - 6 * U) / 4.0f, y + 3 * U, (bw - 6 * U) / 4.0f - 2 * U, bh - 6 * U, col(low ? C_RED : C_INK, 0.85f * flick));
        }
        if (g.spareBatteries > 0) text(FONT_MONO, strf("+%d", g.spareBatteries), x + bw + 10 * U, y + bh * 0.85f, ts * 0.75f, col(C_DIM));
        if (g.stamina < 0.99f && g.pstate == PS_NORMAL) {
            float sw = 90.0f * U;
            float sy = y + bh + 8 * U;
            rect(x, sy, sw, 3 * U, col(0x000000, 0.5f));
            rect(x, sy, sw * g.stamina, 3 * U, col(g.exhausted ? C_RED : 0xa8a290, 0.8f));
        }
    }
    // desktop key legend
    if (!touch && alive && g_controlsCard > 0 && g.pstate == PS_NORMAL) {
        float a = std::min(1.0f, g_controlsCard / 1.5f);
        const char* l1 = "WASD move    MOUSE look    SHIFT run    C crouch";
        const char* l2 = "E use    F flashlight    TAB journal & map    ESC pause";
        textShadow(FONT_SANS, l1, g_W - 18 * U, g_H - 40 * U, ts * 0.78f, col(C_DIM, a), 2);
        textShadow(FONT_SANS, l2, g_W - 18 * U, g_H - 20 * U, ts * 0.78f, col(C_DIM, a), 2);
    }
    if (!touch && alive && (g.pstate == PS_OPERATING || g.pstate == PS_SEATED)) {
        const char* l = g.pstate == PS_OPERATING ? "A D  slew     W S  trolley     R F  hoist     SPACE  release     Q  let go"
                                                 : "E  use what you are looking at     Q / BACKSPACE  leave the cab";
        textShadow(FONT_SANS, l, g_W * 0.5f, g_H - 18 * U, ts * 0.8f, col(C_DIM, 0.9f), 1);
    }
    if (!touch && g.pstate == PS_HIDING) textShadow(FONT_SANS, "E or S  step out", g_W * 0.5f, g_H - 18 * U, ts * 0.8f, col(C_DIM, 0.9f), 1);
    // crane gauges while operating
    if (g.pstate == PS_OPERATING) {
        const CraneSim& c = g.crane;
        float x = 16.0f * U, y = 20.0f * U;
        float bs = ts * 0.8f;
        panel(x, y, 210 * U, 108 * U, 0.6f);
        float slewDeg = c.slew * 180.0f / PI;
        float hookY = c.loadPos.y - LOAD_BOTTOM;
        text(FONT_MONO, strf("SLEW    %+6.1f", slewDeg), x + 12 * U, y + 22 * U, bs, col(C_INK));
        text(FONT_MONO, strf("TROLLEY %6.1f m", c.trolley), x + 12 * U, y + 42 * U, bs, col(C_INK));
        text(FONT_MONO, strf("LOAD    %+6.1f m", hookY), x + 12 * U, y + 62 * U, bs, col(hookY < 0.6f ? C_AMBER : C_INK));
        const char* st = c.ready == 2 ? "IN THE WATER - RELEASE" : c.ready == 4 ? "ON THE HATCH - RELEASE" : c.ready == 1 ? "OVER WATER - LOWER" : c.ready == 3 ? "OVER HATCH - LOWER" : "";
        u32 stc = (c.ready == 2 || c.ready == 4) ? (std::fmod((float)g.time, 0.6f) < 0.35f ? C_AMBER : C_INK) : C_DIM;
        text(FONT_SANS, st, x + 12 * U, y + 88 * U, bs, col(stc));
        // the ladder below
        float cl = saturate(c.climb);
        if (cl > 0.01f) {
            float gx = g_W - 26 * U, gy = touch ? 70 * U : 30 * U, gh = 120 * U;
            rect(gx, gy, 4 * U, gh, col(0x000000, 0.5f));
            rect(gx, gy + gh * (1.0f - cl), 4 * U, gh * cl, col(C_RED, 0.5f + 0.5f * cl));
            text(FONT_SANS, "MAST", gx + 2 * U, gy + gh + 14 * U, bs * 0.7f, col(C_DIM), 1);
        }
    }
    // touch controls
    if (touch && alive && g.modal == MD_NONE) {
        HudLayout L = hud_layout(g, g_W, g_H);
        bool usable = g.promptIdx >= 0;
        hud_button(L.b[HB_PAUSE], "II", false, false);
        hud_button(L.b[HB_JOURNAL], "LOG", false, false);
        hud_button(L.b[HB_USE], "USE", usable, usable, usable ? 1.0f : 0.6f);
        hud_button(L.b[HB_LAMP], "LAMP", g.flashOn, g.flashOn);
        hud_button(L.b[HB_CROUCH], "LOW", g.crouched, g.crouched);
        hud_button(L.b[HB_RUN], "RUN", g_runToggle, g_runToggle);
        hud_button(L.b[HB_STEPOUT], "OUT", false, true);
        hud_button(L.b[HB_CAB_LEVERS], "LEVERS", false, true);
        hud_button(L.b[HB_CAB_LOG], "LOG", false, !g.flags[F_WALT_READ]);
        hud_button(L.b[HB_CAB_LEAVE], "LEAVE", false, false);
        bool ready = g.crane.ready == 2 || g.crane.ready == 4;
        hud_button(L.b[HB_RELEASE], "RELEASE", ready, ready);
        hud_button(L.b[HB_STAND], "X", false, false);
        static const char* sl[HS_COUNT] = {"SLEW", "TROLLEY", "HOIST"};
        static const char* sa[HS_COUNT][2] = {{"<", ">"}, {"IN", "OUT"}, {"DOWN", "UP"}};
        for (int i = 0; i < HS_COUNT; i++) {
            const Bar& b = L.s[i];
            if (!b.show) continue;
            rect(b.x, b.y, b.w, b.h, col(0x000000, 0.4f));
            frame(b.x, b.y, b.w, b.h, 1.5f * U, col(C_INK, 0.4f));
            float v = g_slider[i];
            float kx, ky;
            if (b.vertical) {
                line(b.x + b.w * 0.5f, b.y + 10 * U, b.x + b.w * 0.5f, b.y + b.h - 10 * U, 2 * U, col(C_INK, 0.3f));
                kx = b.x + b.w * 0.5f; ky = b.y + b.h * 0.5f - v * b.h * 0.42f;
                text(FONT_SANS, sa[i][1], kx, b.y - 6 * U, ts * 0.65f, col(C_DIM), 1);
                text(FONT_SANS, sa[i][0], kx, b.y + b.h + 14 * U, ts * 0.65f, col(C_DIM), 1);
                text(FONT_SANS, sl[i], kx, b.y + b.h + 30 * U, ts * 0.72f, col(C_INK, 0.8f), 1);
            } else {
                line(b.x + 10 * U, b.y + b.h * 0.5f, b.x + b.w - 10 * U, b.y + b.h * 0.5f, 2 * U, col(C_INK, 0.3f));
                kx = b.x + b.w * 0.5f + v * b.w * 0.42f; ky = b.y + b.h * 0.5f;
                text(FONT_SANS, sa[i][0], b.x + 12 * U, ky + ts * 0.25f, ts * 0.8f, col(C_DIM), 1);
                text(FONT_SANS, sa[i][1], b.x + b.w - 12 * U, ky + ts * 0.25f, ts * 0.8f, col(C_DIM), 1);
                text(FONT_SANS, sl[i], b.x + b.w * 0.5f, b.y - 8 * U, ts * 0.72f, col(C_INK, 0.8f), 1);
            }
            disc(kx, ky, 15 * U, col(v != 0 ? C_AMBER : C_INK, 0.85f));
        }
        // joystick
        for (const TouchPt& t : in.t) {
            if (!t.active || t.role != TR_JOY || t.ended) continue;
            ring(t.sx, t.sy, 52.0f * U, 2.0f * U, col(C_INK, 0.3f));
            disc(t.x, t.y, 22.0f * U, col(C_INK, 0.35f));
        }
        if (g.pstate == PS_NORMAL && g.time < 40.0 && !g.events[EV_CRANE_FIRST_MOVE]) {
            bool joy = false;
            for (const TouchPt& t : in.t) joy |= t.active && t.role == TR_JOY;
            if (!joy) {
                float a = 0.25f + 0.15f * std::sin((float)g.time * 3.0f);
                ring(g_W * 0.2f, g_H * 0.72f, 52.0f * U, 2.0f * U, col(C_INK, a));
                text(FONT_SANS, "MOVE", g_W * 0.2f, g_H * 0.72f + 5 * U, ts * 0.8f, col(C_INK, a), 1);
                text(FONT_SANS, "DRAG TO LOOK", g_W * 0.62f, g_H * 0.72f, ts * 0.8f, col(C_INK, a), 1);
            }
        }
    }
    // pointer lock reminder
    if (!touch && g.modal == MD_NONE && alive && !in.locked && g.appMode == AM_PLAY) {
        float a = 0.7f + 0.3f * std::sin((float)g.time * 3.0f);
        float by = g_H * 0.66f;
        rect(g_W * 0.5f - 130 * U, by - 26 * U, 260 * U, 36 * U, col(0x000000, 0.45f));
        text(FONT_SANS, "CLICK TO CONTINUE", g_W * 0.5f, by - 2 * U, ts * 0.85f, col(C_INK, a), 1, 2.0f * U);
    }
    if (s.showFps) text(FONT_MONO, strf("%.0f fps", fps), g_W - 10 * U, g_H - 10 * U, 11.0f * U, col(0x80ff80, 0.8f), 2);
}

// --------------------------------------------------------------------------- ending
void draw_ending(Game& g, InputState& in) {
    (void)in;
    rect(0, 0, g_W, g_H, col(0x000000));
    int count = 0;
    const EndLine* lines = ending_lines(g.ending, &count);
    float ts = 20.0f * TK;
    if (g.endingLine < count) {
        const EndLine& l = lines[g.endingLine];
        float t = g.endingT;
        float a = smooth01(t / 1.2f) * smooth01((l.hold - t) / 1.0f);
        bool last = g.endingLine == count - 1;
        std::string s = l.text;
        float maxW = std::min(g_W * 0.8f, 720.0f * U);
        if (last) {
            size_t nl = s.find("\n\n");
            std::string a1 = s.substr(0, nl), a2 = nl == std::string::npos ? "" : s.substr(nl + 2);
            float sz = std::min(g_W * 0.075f, 52.0f * U);
            text(FONT_SANS, a1, g_W * 0.5f, g_H * 0.45f, sz, col(C_INK, a), 1, sz * 0.12f);
            text(FONT_SANS, a2, g_W * 0.5f, g_H * 0.45f + sz * 1.2f, ts, col(C_AMBER, a), 1);
        } else {
            wrap(FONT_SANS, s, ts, maxW, g_lines);
            float lh = ts * 1.5f;
            float yy = g_H * 0.5f - (g_lines.size() - 1) * lh * 0.5f;
            for (auto& ln : g_lines) { text(FONT_SANS, ln, g_W * 0.5f, yy, ts, col(0xe0dac8, a), 1); yy += lh; }
        }
        if (g.endingT > 1.0f) text(FONT_SANS, in.touchMode ? "tap to continue" : "click to continue", g_W * 0.5f, g_H - 24 * U, 12.0f * TK, col(0x4a473f), 1);
    } else {
        float a = smooth01(g.endingT / 1.0f);
        float cx = g_W * 0.5f, y = g_H * 0.28f;
        text(FONT_SANS, g.ending == 1 ? "ENDING: PUT IT BACK" : "ENDING: LANDED", cx, y, ts * 1.2f, col(C_INK, a), 1, 3.0f * U);
        y += ts * 2.4f;
        std::string st[5] = {strf("Shift: %s", g.difficulty == 0 ? "Story" : g.difficulty == 1 ? "Standard" : "Nightmare"),
                             strf("Time on shift: %s", fmt_time(g.time).c_str()), strf("Times it caught you: %d", g.deaths),
                             strf("Documents read: %d of %d", notes_found(g), (int)N_COUNT), strf("Kessler's tapes: %d of 6", tapes_found(g))};
        for (auto& s : st) { text(FONT_SANS, s, cx, y, ts * 0.85f, col(C_DIM, a), 1); y += ts * 1.5f; }
        y += ts;
        text(FONT_SANS, g.ending == 1 ? "There is another way the shift can end." : "There is another way the shift can end.", cx, y, ts * 0.8f, col(0x6a665a, a), 1);
        if (g.endingT > 1.5f) text(FONT_SANS, in.touchMode ? "TAP TO RETURN TO THE TITLE" : "CLICK TO RETURN TO THE TITLE", cx, g_H - 40 * U, 14.0f * TK,
                                   col(C_INK, 0.5f + 0.4f * std::sin(g.endingT * 3.0f)), 1);
    }
}

} // namespace

// ---------------------------------------------------------------------------
void ui_draw(Game& g, Settings& s, InputState& in, const LoadState& ls, float dt, float fps, int fbW, int fbH) {
    g_W = in.W; g_H = in.H; g_dpr = in.dpr;
    g_fbH = fbH;
    (void)fbW;
    U = clampv(std::min(in.W, in.H) / 400.0f, 0.75f, 1.45f);
    static const float tsz[3] = {0.88f, 1.0f, 1.18f};
    TK = U * tsz[s.textSize];
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbW, fbH);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ptr_update(in);
    g_lockRects.clear();
    if (g_ptr.released || !g_ptr.down) {
        if (g_ptr.released || !g_ptr.has) { g_pressId = 0; }
        if (!g_ptr.down) g_dragId = 0;
    }
    if (!ls.done) {
        draw_loading(ls);
    } else if (g.appMode == AM_TITLE) {
        if (g_lastAppMode != AM_TITLE) { g_titlePage = g_lastAppMode == -1 ? TP_GATE : TP_MAIN; g_lastStage = -1; }
        draw_title(g, s, in, dt);
    } else if (g.appMode == AM_PLAY) {
        if (g.modal != MD_PAUSE) g_pausePage = PP_MAIN;
        if (g.modal == MD_NONE || g.modal == MD_PAUSE) draw_hud(g, s, in, dt, fps);
        if (g.modal == MD_NOTE) draw_note(g, in);
        else if (g.modal == MD_JOURNAL) draw_journal(g, in);
        else if (g.modal == MD_KEYPAD) draw_keypad(g, in);
        else if (g.modal == MD_PAUSE) draw_pause(g, s, in);
    } else if (g.appMode == AM_ENDING) {
        draw_ending(g, in);
    }
    g_lastAppMode = ls.done ? g.appMode : -1;
    g_lockRectsPrev = g_lockRects;
    if (g_ptr.released) g_pressId = 0;
    flush();
    noclip();
    glDisable(GL_BLEND);
}
