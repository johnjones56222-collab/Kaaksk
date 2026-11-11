// THE 1982 CRANE - world description: geometry, colliders, lights, interactables.
// Platform independent; the renderer uploads what is built here.
#pragma once
#include "core.h"
#include "texgen.h"

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
struct Vtx {
    float x, y, z;
    i8 nx, ny, nz;
    u8 layer;
    float u, v;
    u8 r, g, b, a; // a: emissive (group*32 + strength 0..31)
};

struct MeshData {
    std::vector<Vtx> v;
    std::vector<u32> idx;
};

enum Pass { PASS_OPAQUE = 0, PASS_CUTOUT, PASS_DECAL, PASS_GLASS, PASS_COUNT };

// Emissive groups let whole families of glowing surfaces be switched together.
enum EmisGroup {
    EG_NONE = 0,
    EG_ALWAYS,   // battery powered things, truck lights
    EG_YARD,     // yard lamps (generator circuit)
    EG_INDOOR,   // building lamps (generator circuit)
    EG_CRANE,    // crane cab / floodlight
    EG_CRT,      // screens
    EG_BEACON,   // blinking red aircraft beacons
    EG_SPECIAL,  // scripted
    EG_COUNT
};

struct Chunk {
    AABB box;
    u32 first[PASS_COUNT];
    u32 count[PASS_COUNT];
    std::vector<u16> lights; // candidate lights touching this chunk
};

struct DynMesh {
    u32 first[PASS_COUNT];
    u32 count[PASS_COUNT];
    AABB box; // local bounds
};

// ---------------------------------------------------------------------------
// Collision
// ---------------------------------------------------------------------------
enum ColFlags {
    COL_SOLID = 1,     // blocks movement
    COL_SIGHT = 2,     // blocks line of sight / interaction rays
    COL_NAVIGNORE = 4, // ignored by the nav grid (doors)
    COL_NORAIN = 8,    // contributes to the rain occlusion map
    COL_LOW = 16,      // low obstacle (kerb): blocks movement only
};
enum Surface { SURF_ASPHALT = 0, SURF_CONCRETE, SURF_METAL, SURF_WOOD, SURF_GRATE, SURF_LINO, SURF_GRAVEL, SURF_WATER, SURF_COUNT };

struct Collider {
    AABB box;
    u16 flags;
    u8 surf;
    bool enabled;
};

// ---------------------------------------------------------------------------
// Lights
// ---------------------------------------------------------------------------
enum LightGroup { LG_ALWAYS = 0, LG_YARD, LG_INDOOR, LG_CRANE, LG_SPECIAL, LG_COUNT };
enum Flicker { FL_NONE = 0, FL_BUZZ, FL_DYING, FL_PULSE, FL_BEACON };

struct Light {
    Vec3 pos;
    Vec3 color;
    float radius = 10;
    Vec3 dir = Vec3(0, -1, 0);
    float cosOuter = -2.0f; // <= -1 means omni
    float cosInner = -2.0f;
    int group = LG_YARD;
    int flicker = FL_NONE;
    float phase = 0;
    float halo = 0;     // glow billboard size in metres (0 = none)
    bool on = true;     // gameplay switch
    float cur = 0;      // current intensity multiplier (updated every frame)
    float boost = 1.0f; // scripted multiplier
};

// ---------------------------------------------------------------------------
// Gameplay object ids (placed by the level, driven by the game)
// ---------------------------------------------------------------------------
enum ItemId {
    IT_FLASHLIGHT = 0, IT_RADIO, IT_OFFICE_KEY, IT_FUSE_A, IT_FUSE_B, IT_FUSE_C, IT_JERRYCAN, IT_CUTTERS, IT_CRANE_KEY,
    IT_TAPE1, IT_TAPE2, IT_TAPE3, IT_TAPE4, IT_TAPE5, IT_TAPE6,
    IT_BATT1, IT_BATT2, IT_BATT3, IT_BATT4, IT_BATT5, IT_BATT6, IT_BATT7, IT_BATT8,
    IT_COUNT
};
enum NoteId {
    N_DUTY_LOG = 0, N_OFFICE_DOOR, N_INCIDENT, N_TIMECARD, N_FOREMAN, N_BLUEPRINT, N_NEWSPAPER, N_CAMP, N_DIVER,
    N_RELIEF_LOCKER, N_WALT_FINAL, N_GEN_MANUAL, N_LIFTS, N_PLATE, N_PHOTO, N_MAST,
    N_COUNT
};
enum DoorId {
    D_BOOTH = 0, D_OFFICE, D_OFFICE_INNER, D_CREW, D_CREW_SHOP, D_SHOP_BACK, D_GEN, D_PUMP, D_CRANE_GATE, D_LIFT3, D_BARGE,
    D_COUNT
};
enum UseId {
    U_GATE_LEVER = 0, U_FUSEBOX, U_FUEL_TAP, U_GEN_FUEL, U_GEN_START, U_KEYPAD, U_PHONE, U_RADIO_SET,
    U_VALVE_A, U_VALVE_B, U_VALVE_C, U_LOTO, U_BREAKER, U_STORMLOCK, U_CAB_DOOR, U_KNOCK, U_LOAD_DOOR, U_TRUCK,
    U_CRT, U_LIFT1, U_LIFT2, U_HELMET, U_VENDING, U_CAB_LEAVE, U_CAB_CONTROLS, U_SINK,
    U_COUNT
};
enum LadderId { LAD_PORTAL = 0, LAD_MAST1, LAD_MAST2, LAD_MAST3, LAD_MAST4, LAD_COUNT };
static const int HIDE_COUNT = 6;

enum InteractKind { IK_PICKUP = 0, IK_NOTE, IK_DOOR, IK_HIDE, IK_LADDER, IK_USE };

struct Interact {
    int kind = 0;
    int arg = 0;      // ItemId / NoteId / DoorId / hide index / ladder*2+dir / UseId
    AABB box;         // world space, or local to dyn[xfDyn] when xfDyn >= 0
    int xfDyn = -1;
    bool enabled = true;
    float range = 2.3f;
};

struct Door {
    Vec3 hinge;         // hinge position (bottom)
    float width = 1, height = 2.1f, thick = 0.06f;
    float yawClosed = 0; // direction of the closed leaf from the hinge (radians, 0 = +X, towards -Z positive)
    float openAngle = 1.7f;
    float angle = 0;     // current
    float target = 0;
    bool slide = false;  // sliding gate: angle is metres of travel along yawClosed
    int collider = -1;
    int dyn = -1;
    int lockedBy = -1;   // ItemId needed, -2 = scripted lock, -1 = free
    bool metal = true;
};

struct Ladder {
    Vec3 bottom;  // where the player stands at the foot
    Vec3 top;     // where the player stands after climbing
    Vec3 rail;    // x,z of the ladder itself (y ignored)
    float yaw;    // facing the rungs
    float y0, y1; // rung extent
};

struct HideSpot {
    Vec3 inside;   // eye position while hidden
    Vec3 outside;  // where the player is put back
    float yaw;     // looking out
    int door;      // dyn object index of the locker door (or -1)
    Vec3 hinge;    // door hinge (world)
    float doorYaw; // closed yaw of the door leaf
    float doorAngle = 0;
};

struct Trigger {
    AABB box;
    int id;
};
enum TriggerId {
    TR_YARD_ENTER = 0, TR_ROAD_MID, TR_OFFICE_APPROACH, TR_MAZE_ENTER, TR_MAZE_DEEP, TR_KNOCK, TR_QUAY, TR_LIFTS, TR_BARGE,
    TR_CRANE_BASE, TR_PORTAL_DECK, TR_MAST_MID, TR_MAST_TOP, TR_CREWROOM, TR_WORKSHOP, TR_PUMP, TR_GEN, TR_GATE_EXIT, TR_OUTSIDE,
    TR_CAMP, TR_WATER, TR_LIFT3_DOOR,
    TR_COUNT
};

struct Zone {
    AABB box;
    int kind; // ZoneKind
};
enum ZoneKind { Z_OUTDOOR = 0, Z_BOOTH, Z_OFFICE, Z_CREW, Z_SHOP, Z_GEN, Z_PUMP, Z_CONTAINER, Z_CAB, Z_BARGE_CABIN, Z_UNDER_PORTAL };

struct DynObj {
    int mesh = -1;
    Mat4 xf = Mat4::identity();
    bool visible = true;
    float emis = 1.0f;
};

// Named dynamic objects the game animates.
enum DynId {
    DY_GATE = 0,
    DY_CRANE_TOP,   // rotating superstructure (jib, counter jib, cab, machinery)
    DY_TROLLEY,
    DY_HOOK,
    DY_CABLE_A, DY_CABLE_B,
    DY_LOAD,        // the container on the hook
    DY_LOAD_DOOR,
    DY_VALVE_A, DY_VALVE_B, DY_VALVE_C,
    DY_GATE_LEVER, DY_STORM_LEVER, DY_BREAKER, DY_GEN_LEVER,
    DY_LOTO,
    DY_FUSE_A, DY_FUSE_B, DY_FUSE_C, // fuses seated in the fuse box
    DY_HELMET,
    DY_PHONE,
    DY_HARDHAT,     // the one that falls off the shelf
    DY_PUMPWATER,   // flood water in the pump house
    DY_CRT_OFFICE,  // overlay quad for the terminal
    DY_CRT_OFFICE2,
    DY_SWING_LAMP,
    DY_CHAIN1, DY_CHAIN2, DY_CHAIN3,
    DY_CAMP_FIGURE,
    DY_FIG0, DY_FIG1, DY_FIG2, DY_FIG3, DY_FIG4, DY_FIG5, // apparition silhouettes
    DY_CHALK_AGAIN,
    DY_BOOTPRINTS,
    DY_CAB_LEVER1, DY_CAB_LEVER2, DY_CAB_LEVER3,
    DY_KEYCAB_DOOR,
    DY_RIG_FIRST,   // the Rigger's body parts start here
    DY_RIG_TORSO = DY_RIG_FIRST, DY_RIG_HEAD, DY_RIG_ARM_UL, DY_RIG_ARM_LL, DY_RIG_ARM_UR, DY_RIG_ARM_LR,
    DY_RIG_LEG_UL, DY_RIG_LEG_LL, DY_RIG_LEG_UR, DY_RIG_LEG_LR, DY_RIG_HOOK,
    DY_RIG_LAST = DY_RIG_HOOK,
    DY_NAMED_COUNT
};

// Crane geometry constants (crane-local frame: origin on the mast axis at ground level,
// +X along the jib when slew = 0).
static const Vec3 CRANE_POS(-8.0f, 0.0f, -36.0f);
static const float CRANE_JIB_Y = 34.5f;      // underside of the jib where the trolley runs
static const float CRANE_TROLLEY_MIN = 7.0f;
static const float CRANE_TROLLEY_MAX = 40.0f;
static const float CRANE_TOP_Y = 31.0f;      // top platform of the fixed mast
static const float LOAD_H = 2.6f;            // size of the hanging container
static const float LOAD_L = 6.1f;
static const float LOAD_W = 2.44f;
static const float HOOK_DROP = 1.6f;         // trolley cable end -> sling point under the hook
static const float LOAD_BOTTOM = 4.6f;       // sling point -> underside of the container
static const float QUAY_Z = -48.0f;          // water begins north of this
static const float WATER_Y = -2.4f;

static const int MAX_CHUNK_LIGHTS = 6;

struct NavGrid {
    float x0 = 0, z0 = 0, cell = 0.5f;
    int w = 0, h = 0;
    std::vector<u8> blocked;   // 1 = not walkable
    std::vector<float> height; // floor height per cell
    bool inside(int x, int z) const { return x >= 0 && z >= 0 && x < w && z < h; }
    bool walkable(int x, int z) const { return inside(x, z) && !blocked[(size_t)z * w + x]; }
    int cx(float x) const { return (int)std::floor((x - x0) / cell); }
    int cz(float z) const { return (int)std::floor((z - z0) / cell); }
    Vec3 center(int x, int z) const { return Vec3(x0 + (x + 0.5f) * cell, height[(size_t)z * w + x], z0 + (z + 0.5f) * cell); }
};

struct World {
    // static geometry, one index range per pass per chunk
    MeshData staticMesh;
    std::vector<Chunk> chunks;
    // dynamic mesh library
    MeshData dynMesh;
    std::vector<DynMesh> dynMeshes;
    std::vector<DynObj> dyn; // first DY_NAMED_COUNT entries are the named ones

    std::vector<Collider> colliders;
    std::vector<Light> lights;
    std::vector<Interact> interacts;
    std::vector<Trigger> triggers;
    std::vector<Zone> zones;
    Door doors[D_COUNT];
    Ladder ladders[LAD_COUNT];
    HideSpot hides[HIDE_COUNT];
    int itemDyn[IT_COUNT];      // dyn object index of each pickup
    int itemInteract[IT_COUNT];
    int useInteract[U_COUNT];
    int noteInteract[N_COUNT];
    Vec3 usePos[U_COUNT];       // rough world position of each usable (for the test bot / audio)
    Vec3 itemPos[IT_COUNT];
    Vec3 notePos[N_COUNT];
    Vec3 doorPos[D_COUNT];

    // named lights the game drives directly
    int lightFlood = -1;     // crane floodlight (moves with the trolley)
    int lightCab = -1;
    int lightTruckL = -1, lightTruckR = -1;
    int lightLantern = -1;
    int lightSwing = -1;
    int lightCrtOffice = -1;
    int lightBeacon[3] = {-1, -1, -1};
    int lightGenRoom = -1;
    int lightPump = -1;
    int gateCollider = -1;
    // crane-local positions of things that ride on the rotating superstructure
    Vec3 cabSeat, cabLampLocal, cabTapeLocal, beaconLocal[3], cabLeverLocal[3];
    int cabNoteInteract = -1, cabControlsInteract = -1, cabLeaveInteract = -1;

    // collision broadphase (uniform grid on XZ)
    float gx0 = 0, gz0 = 0, gcell = 4.0f;
    int gw = 0, gh = 0;
    std::vector<std::vector<u16>> grid;

    NavGrid nav;

    // rain occlusion: height of the highest roof per metre
    static const int RAIN_N = 256;
    float rainX0 = -128.0f, rainZ0 = -140.0f;
    std::vector<u8> rainMap; // height / 0.25 m, clamped

    mutable std::vector<u32> qstamp; // de-duplication stamps for grid queries
    mutable u32 qcounter = 0;

    Vec3 playerStart;
    float playerStartYaw = 0;
    Vec3 truckPos;
    std::vector<Vec3> patrolPoints; // places the Rigger wanders between
    Vec3 riggerSpawn;

    // --- queries -------------------------------------------------------------
    // Returns distance to the first collider with any of `mask` flags, or tMax.
    float raycast(const Vec3& o, const Vec3& d, float tMax, u32 mask, int* hitCollider = nullptr) const;
    bool lineOfSight(const Vec3& a, const Vec3& b, u32 mask = COL_SIGHT) const;
    // Collects colliders overlapping the box.
    void query(const AABB& box, std::vector<int>& out) const;
    // Height of the walkable surface under a point (searches downward from y).
    float floorBelow(const Vec3& p, float maxDrop, int* surf = nullptr) const;
    int zoneAt(const Vec3& p) const;
    void setColliderEnabled(int idx, bool en) { if (idx >= 0 && idx < (int)colliders.size()) colliders[(size_t)idx].enabled = en; }
    Mat4 doorMatrix(const Door& d) const;
};

// Builds the whole level. Deterministic.
void world_build(World& w);
// (Re)builds the navigation grid from the current colliders.
void world_build_nav(World& w);

// A* path on the nav grid. Returns false if unreachable. Path excludes the start.
bool nav_path(const World& w, const Vec3& from, const Vec3& to, std::vector<Vec3>& path);
// Nearest walkable cell centre to p (within r cells), or p itself.
Vec3 nav_snap(const World& w, const Vec3& p, int r = 6);
bool nav_walkable(const World& w, const Vec3& p);
