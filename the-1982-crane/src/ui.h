// THE 1982 CRANE - input mapping, HUD, touch controls, menus (web build)
#pragma once
#include "core.h"

struct Game;
struct Controls;

enum Key {
    K_NONE = 0, K_W, K_A, K_S, K_D, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_SHIFT, K_CTRL, K_C, K_E, K_F, K_Q, K_R, K_Z, K_X, K_V,
    K_SPACE, K_ENTER, K_ESC, K_TAB, K_J, K_P, K_M, K_BACK, K_G, K_I, K_F3,
    K_0, K_9 = K_0 + 9,
    K_COUNT
};

enum TouchRole { TR_NONE = 0, TR_UI, TR_JOY, TR_LOOK, TR_BTN, TR_SLIDER };

struct TouchPt {
    int id = -1;
    bool active = false; // slot in use
    bool fresh = false;  // began this frame
    bool ended = false;  // ended this frame (slot is freed after the frame)
    float x = 0, y = 0, sx = 0, sy = 0, lx = 0, ly = 0; // current, start, last frame
    double t0 = 0;
    int role = TR_NONE, arg = 0;
    bool moved = false;
};

struct InputState {
    bool down[K_COUNT] = {};
    bool pressed[K_COUNT] = {};
    float mdx = 0, mdy = 0;      // mouse movement this frame (css px)
    float mx = -1, my = -1;      // cursor (css px)
    bool mdown = false, mpressed = false, mreleased = false;
    float wheel = 0;
    bool locked = false;         // pointer lock active
    TouchPt t[10];
    bool touchMode = false;      // last input came from a touch screen
    float W = 800, H = 600, dpr = 1;
    void endFrame();
};

struct Settings {
    float lookSens = 1.0f;
    float touchSens = 1.0f;
    bool invertY = false;
    int quality = -1;            // -1 = auto
    float renderScale = -1.0f;   // -1 = auto
    float brightness = 1.0f;
    float volume = 0.85f;
    int textSize = 1;            // 0 small, 1 medium, 2 large
    bool headBob = true;
    bool showFps = false;
};

struct LoadState {
    bool done = false;
    float progress = 0;
    const char* label = "";
    bool error = false;
};

void ui_init();
// Before game_update: turns raw input into Controls (keyboard, mouse, virtual sticks and buttons).
void ui_gather(Game& g, Settings& s, InputState& in, Controls& c, float dt);
// After the 3D frame: draws everything 2D and handles menu clicks.
void ui_draw(Game& g, Settings& s, InputState& in, const LoadState& ls, float dt, float fps, int fbW, int fbH);
// Called from inside a browser input event: what privileged action does this press ask for?
// bit 0 = pointer lock, bit 1 = fullscreen
int ui_gesture_query(const Game& g, float x, float y, bool touch);
bool ui_wants_unlocked_cursor(const Game& g);

std::string settings_to_string(const Settings& s);
void settings_from_string(Settings& s, const std::string& str);
