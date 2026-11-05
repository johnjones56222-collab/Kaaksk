// GLSL sources. They are written in the common subset of GLSL 3.30 core
// (desktop) and GLSL ES 3.00 (WebGL 2 / mobile): every program is assembled as
// version header + defines + kCommon (+ kAtmosphere) + stage source.
#pragma once

namespace strata::shaders {

// One complete program: its stage sources and the defines it is built with.
struct Program {
  const char* name;
  const char* vertex;
  const char* fragment;
  const char* defines;
  bool atmosphere;  // the fragment stage also gets kAtmosphere
};

// Every program the engine builds (the tools validate all of them).
extern const Program kPrograms[];
extern const int kProgramCount;
// Looks a program up by name; unknown names are a programming error.
const Program& program(const char* name);

extern const char* const kCommon;
extern const char* const kAtmosphere;

extern const char* const kTerrainVS;
extern const char* const kTerrainFS;   // OPAQUE (alpha test via uniform), TRANSLUCENT or WATER define
extern const char* const kShadowVS;
extern const char* const kShadowFS;

extern const char* const kFullscreenVS;
extern const char* const kSkyLutFS;
extern const char* const kSkyFS;
extern const char* const kCompositeFS;
extern const char* const kBloomDownFS;
extern const char* const kBloomUpFS;
extern const char* const kLuminanceFS;
extern const char* const kAdaptFS;
extern const char* const kTonemapFS;
extern const char* const kFxaaFS;

extern const char* const kUiVS;
extern const char* const kUiFS;
extern const char* const kLineVS;
extern const char* const kLineFS;
extern const char* const kParticleVS;
extern const char* const kParticleFS;

}  // namespace strata::shaders
