// THE 1982 CRANE - scene renderer interface (web build only)
#pragma once
#include "core.h"

struct Game;
struct World;

struct RenderSettings {
    int quality = 1;          // 0 low, 1 medium, 2 high
    float renderScale = 1.0f; // internal resolution relative to the canvas
    float brightness = 1.0f;  // exposure multiplier
    bool fullBright = false;  // assist: everything lit evenly, fog thinned
    bool espMonster = false;  // assist: the Rigger drawn through walls
};
struct RenderStats {
    int drawCalls = 0;
    int chunksDrawn = 0;
    int waterIndices = 0;
};

void render_init();
void render_upload_layer(int layer, const u8* rgba);
void render_finish_textures(float anisotropy);
void render_upload_world(const World& w);
void render_frame(Game& g, int fbW, int fbH, const RenderSettings& rs, float dt);
void render_set_debug(int layer, float fullbright);
const RenderStats& render_stats();
// Projects a world point with last frame's camera into screen space (W x H units, y down).
// Returns false when the point is behind the camera; sx/sy are then mirrored so the
// direction still points the right way for edge arrows.
bool render_project(const Vec3& p, float W, float H, float& sx, float& sy);
