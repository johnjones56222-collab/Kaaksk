// THE 1982 CRANE - services provided by the host (browser)
#pragma once
#include "core.h"

// Rasterises ASCII 32..127 into a grid of `cell` px cells, 16 columns, and reports advances.
void plat_font_atlas(int fontId, int px, int cell, int texW, int texH, u8* outAlpha, float* adv);
bool plat_storage_get(const char* key, std::string& out);
void plat_storage_set(const char* key, const char* val);
void plat_pointer_lock(bool on);
bool plat_pointer_locked();
void plat_fullscreen();
void plat_vibrate(int ms);
bool plat_is_touch();
double plat_now(); // seconds
