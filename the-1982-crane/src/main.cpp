// THE 1982 CRANE - browser entry point: WebGL 2 context, input events, Web Audio, storage, the main loop.
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include "game.h"
#include "audio.h"
#include "render.h"
#include "ui.h"
#include "platform.h"
#include "texgen.h"

EM_JS_DEPS(crane_deps, "$UTF8ToString,$stringToUTF8,$lengthBytesUTF8");

// ---------------------------------------------------------------------------
// JavaScript side
// ---------------------------------------------------------------------------
EM_JS(void, js_font_atlas, (int fontId, int px, int cell, int texW, int texH, unsigned char* out, float* adv), {
    const c = document.createElement('canvas');
    c.width = texW;
    c.height = texH;
    const x = c.getContext('2d', {willReadFrequently: true});
    x.clearRect(0, 0, texW, texH);
    x.fillStyle = '#ffffff';
    x.textBaseline = 'alphabetic';
    x.font = fontId == 0 ? ('bold ' + px + 'px "Arial Narrow", "Roboto Condensed", "Helvetica Neue", Helvetica, Arial, "Liberation Sans", sans-serif')
                         : ('bold ' + px + 'px "Courier New", Courier, "Liberation Mono", "DejaVu Sans Mono", monospace');
    for (let i = 0; i < 96; i++) {
        const ch = String.fromCharCode(32 + i);
        const cx = (i % 16) * cell, cy = Math.floor(i / 16) * cell;
        x.fillText(ch, cx + 8, cy + cell * 0.75);
        HEAPF32[(adv >> 2) + i] = x.measureText(ch).width;
    }
    const d = x.getImageData(0, 0, texW, texH).data;
    for (let i = 0; i < texW * texH; i++) HEAPU8[out + i] = d[i * 4 + 3];
});

EM_JS(int, js_storage_len, (const char* key), {
    try {
        const v = localStorage.getItem(UTF8ToString(key));
        return v == null ? -1 : lengthBytesUTF8(v);
    } catch (e) { return -1; }
});
EM_JS(void, js_storage_get, (const char* key, char* out, int cap), {
    try { stringToUTF8(localStorage.getItem(UTF8ToString(key)) || "", out, cap); } catch (e) {}
});
EM_JS(void, js_storage_set, (const char* key, const char* val), {
    try { localStorage.setItem(UTF8ToString(key), UTF8ToString(val)); } catch (e) {}
});
EM_JS(void, js_canvas_offset, (float* out), {
    const cv = Module.canvas || document.getElementById('canvas');
    const r = cv.getBoundingClientRect();
    HEAPF32[out >> 2] = r.left;
    HEAPF32[(out >> 2) + 1] = r.top;
});
EM_JS(int, js_is_touch, (), {
    try { return (window.matchMedia && window.matchMedia('(pointer: coarse)').matches) ? 1 : 0; } catch (e) { return 0; }
});
EM_JS(void, js_vibrate, (int ms), {
    try { if (navigator.vibrate) navigator.vibrate(ms); } catch (e) {}
});
EM_JS(void, js_exit_pointer_lock, (), {
    try { if (document.pointerLockElement) document.exitPointerLock(); } catch (e) {}
});
EM_JS(void, js_set_status, (const char* s), {
    const el = document.getElementById('status');
    if (el) {
        const t = UTF8ToString(s);
        el.textContent = t;
        el.style.display = t ? 'flex' : 'none';
    }
});

EM_JS(int, js_audio_init, (int bufSize), {
    try {
        const AC = window.AudioContext || window.webkitAudioContext;
        if (!AC) return 0;
        const ctx = new AC({latencyHint: 'playback'});
        Module.audioCtx = ctx;
        const node = ctx.createScriptProcessor(bufSize, 1, 2);
        node.onaudioprocess = function(e) {
            const L = e.outputBuffer.getChannelData(0), R = e.outputBuffer.getChannelData(1), n = L.length;
            const p = _app_audio_render(n);
            if (!p) { L.fill(0); R.fill(0); return; }
            const h = HEAPF32, b = p >> 2;
            for (let i = 0; i < n; i++) { L[i] = h[b + 2 * i]; R[i] = h[b + 2 * i + 1]; }
        };
        node.connect(ctx.destination);
        Module.audioNode = node;
        const resume = function() { if (ctx.state !== 'running' && !document.hidden) ctx.resume(); };
        ['pointerdown', 'pointerup', 'touchend', 'keydown', 'mousedown', 'click'].forEach(function(ev) {
            window.addEventListener(ev, resume, {capture: true, passive: true});
        });
        document.addEventListener('visibilitychange', function() {
            if (document.hidden) ctx.suspend(); else ctx.resume();
        });
        return ctx.sampleRate | 0;
    } catch (e) {
        console.warn('audio unavailable', e);
        return 0;
    }
});

EM_JS(void, js_install_page_hooks, (), {
    const cv = Module.canvas || document.getElementById('canvas');
    const onDown = function(e) {
        const r = cv.getBoundingClientRect();
        const touch = (e.pointerType === 'touch' || e.pointerType === 'pen') ? 1 : 0;
        const q = _app_gesture_query(e.clientX - r.left, e.clientY - r.top, touch);
        if (q & 1) {
            try {
                const p = cv.requestPointerLock({unadjustedMovement: true});
                if (p && p.catch) p.catch(function() {
                    try {
                        const p2 = cv.requestPointerLock();
                        if (p2 && p2.catch) p2.catch(function() { _app_lock_failed(); });
                    } catch (e2) { _app_lock_failed(); }
                });
            } catch (err) { try { cv.requestPointerLock(); } catch (e3) { _app_lock_failed(); } }
        }
        if (q & 2) {
            const el = document.documentElement;
            const f = el.requestFullscreen || el.webkitRequestFullscreen;
            if (f && !(document.fullscreenElement || document.webkitFullscreenElement)) {
                try {
                    const pr = f.call(el, {navigationUI: 'hide'});
                    if (pr && pr.then) pr.then(function() {
                        try { screen.orientation.lock('landscape').catch(function() {}); } catch (e4) {}
                    }).catch(function() {});
                } catch (e5) {}
            }
        }
    };
    cv.addEventListener('pointerdown', onDown);
    document.addEventListener('pointerlockerror', function() { _app_lock_failed(); });
    document.addEventListener('pointerlockchange', function() { _app_lock_changed(document.pointerLockElement === cv ? 1 : 0); });
    window.addEventListener('blur', function() { _app_focus_lost(); });
    document.addEventListener('visibilitychange', function() { if (document.hidden) _app_focus_lost(); });
    cv.addEventListener('contextmenu', function(e) { e.preventDefault(); });
    cv.addEventListener('webglcontextlost', function(e) {
        e.preventDefault();
        const el = document.getElementById('status');
        if (el) { el.textContent = 'The graphics context was lost. Reload the page to continue from your last checkpoint.'; el.style.display = 'flex'; }
    });
});

// ---------------------------------------------------------------------------
// Platform services used by the portable code
// ---------------------------------------------------------------------------
void plat_font_atlas(int fontId, int px, int cell, int texW, int texH, u8* outAlpha, float* adv) { js_font_atlas(fontId, px, cell, texW, texH, outAlpha, adv); }
bool plat_storage_get(const char* key, std::string& out) {
    int n = js_storage_len(key);
    if (n < 0) return false;
    std::vector<char> buf((size_t)n + 1, 0);
    js_storage_get(key, buf.data(), n + 1);
    out.assign(buf.data());
    return true;
}
void plat_storage_set(const char* key, const char* val) { js_storage_set(key, val); }
void plat_pointer_lock(bool) {}
bool plat_pointer_locked() { return false; }
void plat_fullscreen() {}
void plat_vibrate(int ms) { js_vibrate(ms); }
bool plat_is_touch() { return js_is_touch() != 0; }
double plat_now() { return emscripten_get_now() * 0.001; }

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static Game* G = nullptr;
static Settings S;
static InputState IN;
static LoadState LS;
static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE g_ctx = 0;
static int g_fbW = 0, g_fbH = 0;
static double g_last = 0;
static int g_loadStep = 0;
static int g_texSteps = 0, g_audioSteps = 0;
static int g_sampleRate = 48000;
static bool g_audioOk = false;
static std::vector<u8> g_texBuf;
static float g_maxAniso = 1.0f;
static bool g_selfUnlock = false;
static bool g_touchDevice = false;
static float g_autoScale = 1.0f;
static float g_frameAvg = 16.0f, g_slowT = 0, g_fastT = 0;
static float g_fps = 60.0f;
static std::string g_lastSettings;
static float g_settingsTimer = 0;
static std::vector<float> g_audioBuf;
static int g_lastDeaths = 0;
static const char* KEY_SAVE = "crane1982.save.v2";
static const char* KEY_SETTINGS = "crane1982.settings";

extern "C" {
EMSCRIPTEN_KEEPALIVE float* app_audio_render(int frames) {
    if (!audio_ready() || frames <= 0) return nullptr;
    if ((int)g_audioBuf.size() < frames * 2) g_audioBuf.resize((size_t)frames * 2);
    audio_mix(g_audioBuf.data(), frames);
    return g_audioBuf.data();
}
EMSCRIPTEN_KEEPALIVE int app_gesture_query(float x, float y, int touch) {
    if (!G || !LS.done) return 0;
    return ui_gesture_query(*G, x, y, touch != 0);
}
EMSCRIPTEN_KEEPALIVE void app_lock_changed(int locked) {
    IN.locked = locked != 0;
    if (!locked && G && LS.done) {
        if (!g_selfUnlock && G->appMode == AM_PLAY && G->modal == MD_NONE && G->pstate != PS_DEAD && G->pstate != PS_CUTSCENE) G->modal = MD_PAUSE;
        g_selfUnlock = false;
    }
}
// --- hooks for automated browser tests (harmless in normal play) ---
EMSCRIPTEN_KEEPALIVE int app_state() {
    if (!LS.done || !G) return -1;
    return G->appMode * 100 + G->modal * 10 + (G->pstate == PS_DEAD ? 1 : 0);
}
EMSCRIPTEN_KEEPALIVE int app_stage() { return G ? G->stage : -1; }
EMSCRIPTEN_KEEPALIVE void app_debug(int cmd, float a, float b, float c, float d) {
    if (!G || !LS.done) return;
    Game& g = *G;
    switch (cmd) {
    case 1: game_new(g, (int)a); break;
    case 2: g.pos = Vec3(a, b, c); g.yaw = d; g.vel = Vec3(0, 0, 0); g.onGround = true; g.pstate = PS_NORMAL; break;
    case 3: g.pitch = a; break;
    case 4: g.flashOn = a != 0; if (a != 0) { g.have[IT_FLASHLIGHT] = true; g.itemTaken[IT_FLASHLIGHT] = true; } break;
    case 5: g.lightning = a; break;
    case 6: render_set_debug((int)a, b); break;
    case 7: g.modal = (int)a; g.journalTab = (int)b; g.noteOpen = (int)c; break;
    case 8: g.pstate = PS_SEATED; g.seatYaw = a; g.seatPitch = b; break;
    case 9: g.pstate = PS_OPERATING; break;
    case 10: g.crane.slew = a; g.crane.trolley = b; g.crane.hoist = c; break;
    case 11: IN.touchMode = a != 0; break;
    case 12: g.powerUp = a; g.flags[F_GEN_RUNNING] = a > 0; break;
    case 13: rigger_spawn(g, Vec3(a, 0, b), (int)c); g.rig.grace = 1000.0f; g.flags[F_RIGGER_ACTIVE] = true; break;
    case 14: g.time = a; break;
    default: break;
    }
}
EMSCRIPTEN_KEEPALIVE int app_debug_load(const char* save) {
    if (!G || !LS.done) return 0;
    return game_load(*G, save) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int app_draw_calls() { return render_stats().drawCalls; }
EMSCRIPTEN_KEEPALIVE float app_query(int what) {
    if (!G || !LS.done) return -999.0f;
    Game& g = *G;
    switch (what) {
    case 0: return g.pos.x;
    case 1: return g.pos.y;
    case 2: return g.pos.z;
    case 3: return g.yaw;
    case 4: return (float)g.stage;
    case 5: return (float)g.modal;
    case 6: return (float)g.pstate;
    case 7: return g.flashOn ? 1.0f : 0.0f;
    case 8: return g.have[IT_FLASHLIGHT] ? 1.0f : 0.0f;
    case 9: return g.crane.hoist;
    case 10: return g.crane.slew;
    case 11: return (float)g.keypadEntry.size();
    case 12: return g.flags[F_KEYCAB_OPEN] ? 1.0f : 0.0f;
    case 13: return (float)g.appMode;
    case 14: return IN.touchMode ? 1.0f : 0.0f;
    case 15: return (float)g.promptIdx;
    case 16: return g.crouched ? 1.0f : 0.0f;
    case 17: return g.pitch;
    case 19: return audio_peak();
    case 21: return (float)g.time;
    case 22: return g_fps;
    case 23: return (float)g.frame;
    case 20: return g.noteOpen;
    case 18: return g.promptIdx >= 0 ? (float)(g.w.interacts[(size_t)g.promptIdx].kind * 100 + g.w.interacts[(size_t)g.promptIdx].arg) : -1.0f;
    default: return 0.0f;
    }
}
EMSCRIPTEN_KEEPALIVE void app_lock_failed() { IN.lockFailed = true; }
EMSCRIPTEN_KEEPALIVE void app_focus_lost() {
    for (int i = 0; i < K_COUNT; i++) IN.down[i] = false;
    IN.mdown = false;
    for (TouchPt& t : IN.t)
        if (t.active) t.ended = true;
    if (G && LS.done && G->appMode == AM_PLAY && G->modal == MD_NONE && G->pstate != PS_DEAD && G->pstate != PS_CUTSCENE) G->modal = MD_PAUSE;
}
}

// ---------------------------------------------------------------------------
// Input events
// ---------------------------------------------------------------------------
static int key_from_code(const char* code) {
    struct M { const char* c; int k; };
    static const M map[] = {
        {"KeyW", K_W}, {"KeyA", K_A}, {"KeyS", K_S}, {"KeyD", K_D}, {"ArrowUp", K_UP}, {"ArrowDown", K_DOWN}, {"ArrowLeft", K_LEFT}, {"ArrowRight", K_RIGHT},
        {"ShiftLeft", K_SHIFT}, {"ShiftRight", K_SHIFT}, {"ControlLeft", K_CTRL}, {"ControlRight", K_CTRL}, {"KeyC", K_C}, {"KeyE", K_E}, {"KeyF", K_F},
        {"KeyQ", K_Q}, {"KeyR", K_R}, {"KeyZ", K_Z}, {"KeyX", K_X}, {"KeyV", K_V}, {"Space", K_SPACE}, {"Enter", K_ENTER}, {"NumpadEnter", K_ENTER},
        {"Escape", K_ESC}, {"Tab", K_TAB}, {"KeyJ", K_J}, {"KeyP", K_P}, {"KeyM", K_M}, {"Backspace", K_BACK}, {"KeyG", K_G}, {"KeyI", K_I}, {"F3", K_F3},
        {"Digit0", K_0}, {"Digit1", K_0 + 1}, {"Digit2", K_0 + 2}, {"Digit3", K_0 + 3}, {"Digit4", K_0 + 4}, {"Digit5", K_0 + 5}, {"Digit6", K_0 + 6},
        {"Digit7", K_0 + 7}, {"Digit8", K_0 + 8}, {"Digit9", K_0 + 9}, {"Numpad0", K_0}, {"Numpad1", K_0 + 1}, {"Numpad2", K_0 + 2}, {"Numpad3", K_0 + 3},
        {"Numpad4", K_0 + 4}, {"Numpad5", K_0 + 5}, {"Numpad6", K_0 + 6}, {"Numpad7", K_0 + 7}, {"Numpad8", K_0 + 8}, {"Numpad9", K_0 + 9},
    };
    for (const M& m : map)
        if (!strcmp(m.c, code)) return m.k;
    return K_NONE;
}

static EM_BOOL on_key(int type, const EmscriptenKeyboardEvent* e, void*) {
    int k = key_from_code(e->code);
    if (k == K_NONE) return EM_FALSE;
    if (e->ctrlKey && k != K_CTRL) return EM_FALSE; // leave browser shortcuts alone
    if (type == EMSCRIPTEN_EVENT_KEYDOWN) {
        if (!e->repeat) IN.pressed[k] = true;
        IN.down[k] = true;
        IN.touchMode = false;
    } else IN.down[k] = false;
    if (k == K_F3 && type == EMSCRIPTEN_EVENT_KEYDOWN && !e->repeat) S.showFps = !S.showFps;
    return EM_TRUE;
}

static float g_canvasOfs[2] = {0, 0};
static EM_BOOL on_mouse(int type, const EmscriptenMouseEvent* e, void*) {
    if (type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
        IN.mdx += (float)e->movementX;
        IN.mdy += (float)e->movementY;
        IN.mx = (float)e->clientX - g_canvasOfs[0];
        IN.my = (float)e->clientY - g_canvasOfs[1];
        return EM_FALSE;
    }
    if (e->button != 0) return EM_FALSE;
    IN.mx = (float)e->clientX - g_canvasOfs[0];
    IN.my = (float)e->clientY - g_canvasOfs[1];
    IN.touchMode = false;
    if (type == EMSCRIPTEN_EVENT_MOUSEDOWN) { IN.mdown = true; IN.mpressed = true; }
    else if (type == EMSCRIPTEN_EVENT_MOUSEUP) { IN.mdown = false; IN.mreleased = true; }
    return EM_FALSE;
}

static EM_BOOL on_wheel(int, const EmscriptenWheelEvent* e, void*) {
    double dy = e->deltaY;
    if (e->deltaMode == DOM_DELTA_LINE) dy *= 32.0;
    else if (e->deltaMode == DOM_DELTA_PAGE) dy *= 400.0;
    IN.wheel += (float)dy;
    return EM_TRUE;
}

static EM_BOOL on_touch(int type, const EmscriptenTouchEvent* e, void*) {
    IN.touchMode = true;
    double now = plat_now();
    for (int i = 0; i < e->numTouches; i++) {
        const EmscriptenTouchPoint& tp = e->touches[i];
        if (!tp.isChanged) continue;
        float x = (float)tp.targetX, y = (float)tp.targetY;
        TouchPt* slot = nullptr;
        for (TouchPt& t : IN.t)
            if (t.active && t.id == tp.identifier && !t.ended) slot = &t;
        if (type == EMSCRIPTEN_EVENT_TOUCHSTART) {
            if (slot) continue;
            for (TouchPt& t : IN.t)
                if (!t.active) { slot = &t; break; }
            if (!slot) continue;
            *slot = TouchPt();
            slot->id = tp.identifier;
            slot->active = true;
            slot->fresh = true;
            slot->x = slot->sx = slot->lx = x;
            slot->y = slot->sy = slot->ly = y;
            slot->t0 = now;
        } else if (slot) {
            slot->x = x;
            slot->y = y;
            if (type == EMSCRIPTEN_EVENT_TOUCHEND || type == EMSCRIPTEN_EVENT_TOUCHCANCEL) slot->ended = true;
        }
    }
    return EM_TRUE;
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------
static const char* tex_label(int step) {
    static const char* L[] = {"Pouring concrete", "Laying asphalt", "Rusting steel", "Stencilling numbers", "Wetting everything", "Painting signs",
                              "Writing the duty log", "Hanging the lamps"};
    return L[(step / 6) % 8];
}

static void load_settings() {
    std::string s;
    if (plat_storage_get(KEY_SETTINGS, s)) settings_from_string(S, s);
    g_lastSettings = settings_to_string(S);
}

static bool load_step(double budgetEnd) {
    // returns true when loading is complete
    for (;;) {
        int st = g_loadStep;
        if (st == 0) {
            js_set_status("");
            for (int f = 0; f < FONT_COUNT; f++) {
                FontAtlas& F = g_fonts[f];
                F.texW = 1024; F.texH = 384; F.cell = 64; F.cols = 16; F.px = 44;
                F.alpha.assign((size_t)F.texW * F.texH, 0);
                plat_font_atlas(f, 44, 64, F.texW, F.texH, F.alpha.data(), F.adv);
            }
            ui_init();
            render_init();
            g_texSteps = texgen_step_count();
            g_audioSteps = audio_init_steps();
            g_texBuf.reserve((size_t)TEX_SIZE * TEX_SIZE * 4);
            g_loadStep++;
            LS.label = "Unlocking the gate";
        } else if (st <= g_texSteps) {
            int layer = -1;
            texgen_run_step(st - 1, &layer, g_texBuf);
            if (layer >= 0) render_upload_layer(layer, g_texBuf.data());
            LS.label = tex_label(st);
            g_loadStep++;
            if (g_loadStep > g_texSteps) {
                render_finish_textures(g_maxAniso);
                texgen_free_temp();
                g_texBuf.clear();
                g_texBuf.shrink_to_fit();
                LS.label = "Building Berth 9";
            }
        } else if (st == g_texSteps + 1) {
            G = new Game();
            game_init(*G);
            render_upload_world(G->w);
            std::string save;
            if (plat_storage_get(KEY_SAVE, save) && save.size() > 4) { G->saveData = save; G->hasSave = true; }
            g_loadStep++;
            LS.label = "Recording the storm";
        } else if (st <= g_texSteps + 1 + g_audioSteps) {
            audio_init_step(st - g_texSteps - 2, g_sampleRate);
            g_loadStep++;
            LS.label = (st - g_texSteps) % 30 < 15 ? "Recording the storm" : "Tuning the radio";
        } else {
            LS.done = true;
            LS.progress = 1.0f;
            return true;
        }
        LS.progress = (float)g_loadStep / (float)(g_texSteps + g_audioSteps + 3);
        if (plat_now() > budgetEnd) return false;
    }
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
static void update_canvas() {
    double cw = 0, ch = 0;
    emscripten_get_element_css_size("#canvas", &cw, &ch);
    if (cw < 1 || ch < 1) { cw = 800; ch = 600; }
    double dpr = emscripten_get_device_pixel_ratio();
    double cap = g_touchDevice ? 2.0 : 2.0;
    if (dpr > cap) dpr = cap;
    if (dpr < 1.0) dpr = 1.0;
    int w = (int)(cw * dpr + 0.5), h = (int)(ch * dpr + 0.5);
    if (w != g_fbW || h != g_fbH) {
        emscripten_set_canvas_element_size("#canvas", w, h);
        g_fbW = w; g_fbH = h;
    }
    IN.W = (float)cw; IN.H = (float)ch; IN.dpr = (float)dpr;
    js_canvas_offset(g_canvasOfs);
}

static void frame() {
    double now = plat_now();
    float dt = (float)(now - g_last);
    g_last = now;
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.0f) dt = 0.0f;
    update_canvas();
    if (dt > 0) {
        g_fps = g_fps * 0.95f + 0.05f * (1.0f / std::max(dt, 0.001f));
        g_frameAvg = g_frameAvg * 0.9f + 0.1f * dt * 1000.0f;
    }

    if (!LS.done) {
        if (g_loadStep > 0) {
            static Game* dummy = new Game();
            load_step(now + 0.03);
            if (!LS.done) {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, g_fbW, g_fbH);
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                ui_draw(G ? *G : *dummy, S, IN, LS, dt, g_fps, g_fbW, g_fbH);
            }
        } else load_step(now + 0.03);
        IN.endFrame();
        if (!LS.done) return;
    }
    Game& g = *G;
    Controls c;
    ui_gather(g, S, IN, c, dt);
    // The simulation steps at most 50 ms at a time. Slow devices get extra steps so the
    // world still runs in real time; one-shot inputs are only delivered on the first.
    int steps = dt > 0.05f ? 2 : 1;
    float sub = dt / (float)steps;
    game_update(g, c, sub);
    for (int i = 1; i < steps; i++) {
        Controls h;
        h.moveX = c.moveX; h.moveY = c.moveY; h.sprint = c.sprint;
        h.craneSlew = c.craneSlew; h.craneTrolley = c.craneTrolley; h.craneHoist = c.craneHoist;
        game_update(g, h, sub);
    }
    audio_set_volume(S.volume);

    // dynamic resolution when the scale is on auto
    float base = g_touchDevice ? 0.75f : 1.0f;
    if (S.renderScale < 0) {
        if (g_frameAvg > 27.0f) { g_slowT += dt; g_fastT = 0; } else if (g_frameAvg < 17.5f) { g_fastT += dt; g_slowT = 0; } else { g_slowT = g_fastT = 0; }
        if (g_slowT > 1.5f && g_autoScale > 0.5f) { g_autoScale = std::max(0.5f, g_autoScale - 0.08f); g_slowT = 0; }
        if (g_fastT > 5.0f && g_autoScale < base) { g_autoScale = std::min(base, g_autoScale + 0.05f); g_fastT = 0; }
    }
    RenderSettings rs;
    rs.quality = S.quality >= 0 ? S.quality : (g_touchDevice ? 0 : 1);
    rs.renderScale = S.renderScale > 0 ? S.renderScale : g_autoScale;
    rs.brightness = S.brightness;
    render_frame(g, g_fbW, g_fbH, rs, dt);
    ui_draw(g, S, IN, LS, dt, g_fps, g_fbW, g_fbH);

    // the cursor is needed for menus and the journal
    if (IN.locked && ui_wants_unlocked_cursor(g)) {
        g_selfUnlock = true;
        js_exit_pointer_lock();
    }
    if (g.wantSave) {
        g.wantSave = false;
        plat_storage_set(KEY_SAVE, g.hasSave ? g.saveData.c_str() : "");
    }
    if (g.deaths != g_lastDeaths) {
        if (g.deaths > g_lastDeaths && g_touchDevice) plat_vibrate(220);
        g_lastDeaths = g.deaths;
    }
    g_settingsTimer += dt;
    if (g_settingsTimer > 1.0f) {
        g_settingsTimer = 0;
        std::string s = settings_to_string(S);
        if (s != g_lastSettings) { plat_storage_set(KEY_SETTINGS, s.c_str()); g_lastSettings = s; }
    }
    IN.endFrame();
}

int main() {
    js_set_status("Starting...");
    EmscriptenWebGLContextAttributes a;
    emscripten_webgl_init_context_attributes(&a);
    a.majorVersion = 2;
    a.minorVersion = 0;
    a.alpha = EM_FALSE;
    a.depth = EM_FALSE;
    a.stencil = EM_FALSE;
    a.antialias = EM_FALSE;
    a.premultipliedAlpha = EM_TRUE;
    a.preserveDrawingBuffer = EM_FALSE;
    a.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
    g_ctx = emscripten_webgl_create_context("#canvas", &a);
    if (g_ctx <= 0) {
        js_set_status("This game needs WebGL 2, which this browser or device does not provide. Try a current version of Chrome, Edge, Firefox or Safari.");
        return 0;
    }
    emscripten_webgl_make_context_current(g_ctx);
    if (emscripten_webgl_enable_extension(g_ctx, "EXT_texture_filter_anisotropic")) {
        float mx = 1.0f;
        glGetFloatv(0x84FF /* MAX_TEXTURE_MAX_ANISOTROPY_EXT */, &mx);
        g_maxAniso = std::min(mx, 8.0f);
    }
    g_touchDevice = plat_is_touch();
    IN.touchMode = g_touchDevice;
    g_autoScale = g_touchDevice ? 0.75f : 1.0f;
    load_settings();
    g_sampleRate = js_audio_init(g_touchDevice ? 4096 : 2048);
    g_audioOk = g_sampleRate > 0;
    if (!g_audioOk) g_sampleRate = 48000;
    js_install_page_hooks();
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, on_key);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, on_key);
    emscripten_set_mousedown_callback("#canvas", nullptr, EM_TRUE, on_mouse);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, on_mouse);
    emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, on_mouse);
    emscripten_set_wheel_callback("#canvas", nullptr, EM_TRUE, on_wheel);
    emscripten_set_touchstart_callback("#canvas", nullptr, EM_TRUE, on_touch);
    emscripten_set_touchmove_callback("#canvas", nullptr, EM_TRUE, on_touch);
    emscripten_set_touchend_callback("#canvas", nullptr, EM_TRUE, on_touch);
    emscripten_set_touchcancel_callback("#canvas", nullptr, EM_TRUE, on_touch);
    g_last = plat_now();
    update_canvas();
    emscripten_set_main_loop(frame, 0, false);
    return 0;
}
