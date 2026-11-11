// THE 1982 CRANE - procedural texture generation (CPU side, platform independent)
#pragma once
#include "core.h"

// Material layers of the world texture array. Every surface in the game is
// generated at start-up; there are no image files.
enum Mat {
    M_ASPHALT = 0,
    M_CONCRETE,      // quay slabs
    M_CONCWALL,      // poured concrete walls, streaked
    M_BLOCK,         // painted cinder block
    M_CORRUGATED,    // corrugated sheet (tintable)
    M_RUST,          // heavily rusted steel
    M_PAINTSTEEL,    // chipped painted steel (tintable)
    M_DIAMOND,       // tread plate
    M_GRATE,         // open bar grating (cutout)
    M_CHAINLINK,     // chain link fence (cutout)
    M_WOOD,          // rough planks
    M_PANEL,         // 70s faux-wood wall panelling
    M_LINO,          // worn linoleum tiles
    M_CEILING,       // stained acoustic tiles
    M_BRICK,
    M_RUBBER,
    M_GLASS,         // dirty glass (blended)
    M_PAPER,
    M_TARP,
    M_HAZARD,
    M_LOCKER,        // locker door (tintable)
    M_SCREEN,        // dead CRT glass
    M_PLAIN,         // smooth neutral surface for tinting
    M_GRAVEL,
    M_RAIL,          // bright scratched steel
    M_CARDBOARD,
    M_MACHINE,       // hammered-paint machine panel (tintable)
    M_CONTDOOR,      // container door with lock rods (tintable)
    M_ROOF,          // bitumen roofing
    M_KELP,          // sea-soaked growth
    M_OILSKIN,       // the Rigger's coat
    M_BLINDS,
    M_NOISE,         // utility noise for the shaders
    M_SIGN0,         // sign / decal sheets (alpha = opacity)
    M_SIGN1, M_SIGN2, M_SIGN3, M_SIGN4, M_SIGN5, M_SIGN6, M_SIGN7, M_SIGN8, M_SIGN9, M_SIGN10, M_SIGN11,
    M_COUNT
};
static const int NUM_SIGN_SHEETS = 12;
static const int TEX_SIZE = 512;

struct MatInfo {
    float spec;     // specular intensity
    float gloss;    // specular exponent
    float wet;      // 0..1 how much rain puddles affect this surface
    float bump;     // bump strength
};
extern const MatInfo g_matInfo[M_COUNT];

// Glyph atlas supplied by the platform layer (browser canvas text raster).
struct FontAtlas {
    int texW = 0, texH = 0, cell = 0, cols = 16;
    float px = 0;             // font pixel size used to raster the atlas
    float adv[96];            // advance width of chars 32..127 in atlas pixels
    std::vector<u8> alpha;    // texW*texH coverage
    bool valid() const { return !alpha.empty(); }
};
enum { FONT_SANS = 0, FONT_MONO = 1, FONT_COUNT = 2 };
extern FontAtlas g_fonts[FONT_COUNT];

// Signs and decals. Looked up by name when the world is built.
struct SignRect {
    int layer;          // texture array layer
    float u0, v0, u1, v1;
    float aspect;       // w / h
};
SignRect sign_rect(const char* name);

// Generation is split into steps so a loading bar can be drawn between them.
int  texgen_step_count();
// Generates layer data for step i. Returns the layer index written to *layerOut
// (or -1 if the step produced no layer) and RGBA8 pixels in out (TEX_SIZE^2*4).
void texgen_run_step(int step, int* layerOut, std::vector<u8>& out);
void texgen_free_temp();
