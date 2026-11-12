// THE 1982 CRANE - game state and logic (platform independent)
#pragma once
#include "world.h"

// Abstract per-frame input. Produced by keyboard / mouse / touch on the web build
// and by the scripted bot in the headless test build.
struct Controls {
    float moveX = 0, moveY = 0;   // strafe (+right), forward (+ahead), each -1..1
    float lookDX = 0, lookDY = 0; // radians this frame (+right, +up)
    bool sprint = false;          // held
    bool crouch = false;          // pressed this frame (toggle)
    bool interact = false;        // pressed this frame
    bool flashlight = false;      // pressed this frame
    bool journal = false;         // pressed this frame
    bool pause = false;           // pressed this frame
    bool back = false;            // pressed this frame (close modal)
    bool confirm = false;         // pressed this frame (dismiss / advance)
    bool tap = false;             // tap-to-interact: ray through (tapX, tapY)
    float tapX = 0, tapY = 0;     // NDC -1..1
    int keypad = -1;              // 0..9 digit, 10 = clear, 11 = enter
    float craneSlew = 0, craneTrolley = 0, craneHoist = 0; // -1..1 held
    bool craneRelease = false;    // pressed
    float scroll = 0;             // note scrolling (ui units)
};

enum Stage {
    ST_FLASHLIGHT = 0, ST_BOOTH, ST_GATE, ST_OFFICE_GO, ST_OFFICE_KEY, ST_OFFICE_ENTER, ST_POWER, ST_KEYCAB, ST_BRAKE, ST_CLIMB,
    ST_CAB, ST_LOAD, ST_ESCAPE, ST_OPEN, ST_END,
    ST_COUNT
};

enum Flag {
    F_GATE_OPEN = 0, F_OFFICE_UNLOCKED, F_PHONE_ANSWERED, F_CAN_FULL, F_GEN_FUELED, F_GEN_RUNNING, F_KEYCAB_OPEN, F_VALVES_DONE,
    F_DRAINED, F_LOTO_CUT, F_PUMP_ON, F_STORM_LOCKED, F_CRANE_GATE_UNLOCKED, F_WALT_READ, F_RELEASED_WATER, F_RELEASED_QUAY,
    F_RIGGER_ACTIVE, F_VENDED, F_KNOCKED, F_HELMET_SEEN, F_DIED_ONCE, F_LIFT3_OPEN, F_RADIO_ON, F_TRIED_OFFICE, F_LOG_READ,
    F_FINALE, F_SINK_USED, F_CRT_SEEN, F_TRIED_STORM, F_TRIED_LADDER, F_GEN_TRIED, F_CAMP_SEEN, F_OPERATED,
    F_COUNT
};

enum EventId {
    EV_INTRO = 0, EV_CRANE_FIRST_MOVE, EV_FIGURE_STACK, EV_LOCKER_SLAM, EV_PHONE_RING, EV_CRT_ON, EV_KNOCK, EV_MAZE_FIGURES,
    EV_CAMP_FIGURE, EV_RIGGER_RELEASE, EV_LIFT3, EV_BARGE, EV_PUMP_BUMP, EV_CLIMB_CLANG, EV_MAST_LOOKDOWN, EV_TOP, EV_QUAY_FIRST,
    EV_CREW_ROOM, EV_WORKSHOP, EV_ROAD_MID, EV_GATE_SLAM, EV_CRANE_BASE, EV_DECK, EV_LIFTS,
    EV_COUNT
};

enum PlayerState { PS_NORMAL = 0, PS_LADDER, PS_HIDING, PS_SEATED, PS_OPERATING, PS_DEAD, PS_CUTSCENE };
enum Modal { MD_NONE = 0, MD_NOTE, MD_JOURNAL, MD_KEYPAD, MD_PAUSE };
enum AppMode { AM_TITLE = 0, AM_PLAY, AM_ENDING };
enum RiggerState { RS_DORMANT = 0, RS_PATROL, RS_INVESTIGATE, RS_CHASE, RS_SEARCH, RS_STARE, RS_FINALE, RS_STATUE };

struct Subtitle {
    std::string speaker; // "" = thought, "DISPATCH", "TAPE", "PHONE"
    std::string text;
    float time = 0;      // remaining
    float delay = 0;     // wait before showing
};

struct Timed {
    float t;
    int code;
    int arg;
};

struct Rigger {
    int state = RS_DORMANT;
    Vec3 pos;
    float yaw = 0;
    Vec3 target;            // current destination
    std::vector<Vec3> path;
    size_t pathIdx = 0;
    float repath = 0;
    float stateTime = 0;
    float lastSeen = 99;    // seconds since the player was last seen
    Vec3 lastKnown;
    float alert = 0;        // 0..1 suspicion build-up
    float walkPhase = 0;
    float speed = 0;
    float stun = 0;
    float animTime = 0;     // quantised for the stop-motion look
    float stepTimer = 0;
    float grace = 0;        // cannot detect the player while > 0
    bool sawPlayerHide = false;
    float headYaw = 0;
    float reach = 0;        // arm raise 0..1
    int patrolIdx = 0;
    float nearTime = 0;
    bool visible = false;
};

struct CraneSim {
    float slew = 0.55f, slewVel = 0;
    float trolley = 21.0f, trolleyVel = 0;
    float hoist = 18.0f, hoistVel = 0; // cable length
    Vec3 swing;                        // load offset from plumb (x,z) and velocity
    Vec3 swingVel;
    float autoTarget = 0.55f;          // autonomous slew target
    float autoTimer = 12.0f;
    bool locked = false;
    float shake = 0;
    float climb = 0;                   // 0..1 how far up the mast the Rigger is (operating phase)
    bool loadReleased = false;
    Vec3 loadPos;                      // world position of the sling point
    float loadYaw = 0;
    float sink = 0;                    // released into the harbour: metres sunk
    float doorOpen = 0;                // load door angle (ending B)
    Vec3 prevT, prevTV;                // trolley point history for the pendulum
    bool havePrev = false;
    int ready = 0;                     // 0 none, 1 over water (lower), 2 water ready, 3 over the hatch (lower), 4 hatch ready
    int climbMsg = 0;
    float clangT = 3.0f;
    float motor = 0;
};

struct Game {
    World w;
    int appMode = AM_TITLE;
    int modal = MD_NONE;
    double time = 0;       // total play time
    float dt = 0;
    u32 frame = 0;
    Rng rng;
    int difficulty = 1;    // 0 story, 1 standard, 2 nightmare

    // --- player ---
    int pstate = PS_NORMAL;
    Vec3 pos, vel;
    float yaw = 0, pitch = 0;
    bool onGround = true;
    bool crouched = false;
    float crouchAmt = 0;    // 0..1
    float stamina = 1;
    float staminaDelay = 0;
    bool exhausted = false;
    float bobPhase = 0, bobAmt = 0;
    float stepDist = 0;
    int surface = SURF_ASPHALT;
    float landKick = 0;
    float fallStart = 0;
    bool flashOn = false;
    float battery = 1.0f;   // 0..1
    int spareBatteries = 0;
    float flashFlicker = 1; // output multiplier
    float noise = 0;        // how loud the player currently is (metres of audibility)
    float fear = 0;         // 0..1 drives presentation only
    float health = 1;
    // ladders / hiding / cab
    int ladder = -1, ladderDir = 0;
    float ladderT = 0;      // 0..1 along the climb
    int ladderPhase = 0;
    Vec3 ladderFrom;
    int hideIdx = -1;
    float hideT = 0;
    float seatYaw = 0, seatPitch = 0;
    float deadT = 0;
    int deathCause = 0;     // 0 rigger, 1 water, 2 cab
    float actionLock = 0;   // seconds the player is busy (pouring fuel etc.)
    std::string actionText;

    // --- camera output ---
    Vec3 camPos;
    float camYaw = 0, camPitch = 0, camRoll = 0, camFov = 1.2f;
    float shake = 0;
    float fade = 1.0f;      // 1 = black
    float fadeTarget = 0;
    float fadeSpeed = 1.0f;
    float flash = 0;        // white flash
    float aspect = 1.78f;   // viewport aspect (set by the platform layer; used for tap rays)
    float blackout = 0;     // scripted dimming of mains lights 0..1
    float interference = 0; // VHS tearing when it is near
    float damage = 0;

    // --- progression ---
    int stage = ST_FLASHLIGHT;
    bool flags[F_COUNT];
    bool events[EV_COUNT];
    bool triggerInside[TR_COUNT];
    bool have[IT_COUNT];
    bool itemTaken[IT_COUNT];
    bool noteRead[N_COUNT];
    int fusesInstalled = 0;
    int valveStep = 0;
    float valveSpin[3];
    int keyCode = 1957;
    std::string keypadEntry;
    float keypadMsgT = 0;
    int keypadMsg = 0;      // 0 none, 1 wrong, 2 ok
    int deaths = 0;
    int ending = 0;         // 1 = put it back, 2 = opened
    float endingT = 0;
    int endingLine = 0;
    float waterLevel = 0.34f;
    float genSpin = 0;      // generator running 0..1
    float powerUp = 0;      // yard lights sequencing 0..1
    float gateT = 0;        // gate travel 0..1
    float leverAnim[8];
    float phoneRingT = -1;  // >= 0 while ringing
    float hardhatT = -1;    // falling animation
    float helmetYaw = 2.4f, helmetTarget = 2.4f;
    float lockerSlamT = -1;
    float figureT = -1;     // stack figure visible window
    float mazeFigT = -1;
    int mazeFigStage = 0;
    float knockT = -1;
    int knockCount = 0;
    float riggerReleaseT = -1;
    float finaleT = 0;
    float tapeT = -1;       // tape playing
    int tapePlaying = -1;
    float cabDoorT = 0;
    float lookedAtRigger = 0;
    float campFigT = -1;     // the figure at the camp container door
    float lookUpT = 0;       // how long the player has stared up at the cab
    float lookDownArm = -1;  // window in which looking down the mast shows something
    float titleT = 0;
    Vec3 mazeFigPos[5];
    float mazeFigYaw = 0;

    // --- environment ---
    float lightning = 0;       // current flash brightness
    float lightningTimer = 9;  // until next strike
    int lightningSeq = 0;
    float lightningSeqT = 0;
    float thunderDelay = -1;
    float thunderGain = 0;
    float rain = 1.0f;
    float wind = 0.5f;
    float indoor = 0;          // 0 outside .. 1 inside
    int zone = Z_OUTDOOR;
    float ambientTimer = 20;
    float emis[EG_COUNT];
    float dawn = 0;            // ending A

    Rigger rig;
    CraneSim crane;

    // --- presentation / text ---
    std::vector<Subtitle> subs;
    std::string toast;         // objective banner
    float toastT = 0;
    std::string hint;          // small bottom hint ("Locked.")
    float hintT = 0;
    int promptKind = -1;       // interact under the crosshair (index into w.interacts) or -1
    int promptIdx = -1;
    std::string promptText;
    int noteOpen = -1;         // NoteId, or 100 + tape index when reading a transcript
    float noteScroll = 0;
    int journalTab = 0;
    std::vector<Timed> timeline;
    std::string saveData;      // last checkpoint
    bool hasSave = false;
    bool wantSave = false;     // platform layer should persist saveData
    float checkpointT = 0;     // "saved" indicator
    int lastCheckpointStage = -1;

    // audio handles
    int hGen = -1, hPhone = -1, hRadio = -1, hRigBreath = -1, hTape = -1, hWaterDrain = -1, hPump = -1, hCraneMotor = -1, hChase = -1,
        hBuzz = -1, hSlosh = -1;
};

// --- lifecycle ---
void game_init(Game& g);                 // builds the world (slow), call once
void game_new(Game& g, int difficulty);  // start a fresh shift
bool game_load(Game& g, const std::string& data); // restore a checkpoint; false if invalid
void game_update(Game& g, const Controls& c, float dt);
void game_title_update(Game& g, float dt); // drives the title-screen camera and weather
std::string game_save_string(const Game& g);
void game_checkpoint(Game& g);
void game_to_title(Game& g, bool clearSave);

// --- queries used by the UI / renderer / bot ---
const char* stage_title(const Game& g);
void objective_lines(const Game& g, std::vector<std::string>& out, std::vector<int>& done);
std::string interact_prompt(const Game& g, int interactIdx);
Mat4 game_view_matrix(const Game& g);
Vec3 game_cam_forward(const Game& g);
bool game_player_busy(const Game& g);
Vec3 crane_trolley_world(const Game& g);
int tapes_found(const Game& g);
int notes_found(const Game& g);

// --- shared between game_*.cpp files ---
void say(Game& g, const char* speaker, const char* text, float dur = -1.0f, float delay = 0.0f);
void hint(Game& g, const char* text, float t = 3.0f);
void toast(Game& g, const char* text);
void schedule(Game& g, float delay, int code, int arg = 0);
void set_stage(Game& g, int stage);
void kill_player(Game& g, int cause);
void alert_noise(Game& g, const Vec3& pos, float radius);
void events_update(Game& g, float dt);
void events_trigger(Game& g, int triggerId);
void events_timed(Game& g, int code, int arg);
void crane_update(Game& g, const Controls& c, float dt);
void rigger_update(Game& g, float dt);
void rigger_spawn(Game& g, const Vec3& pos, int state);
void rigger_pose(Game& g);
void ending_update(Game& g, const Controls& c, float dt);
void env_update(Game& g, float dt);
void open_note(Game& g, int noteId);
void play_tape(Game& g, int tapeIdx);
void check_progress(Game& g);
void apply_world_state(Game& g);
void use_object(Game& g, int useId);

// timeline codes
enum TimedCode {
    TC_SAY_STAGE = 1, TC_CRANE_GROAN, TC_GEN_CATCH, TC_POWER_STEP, TC_RIGGER_RELEASE, TC_LOCKER_SLAM, TC_PHONE_START, TC_CRT, TC_KNOCK_REPLY,
    TC_FIGURE_HIDE, TC_LIGHTNING, TC_STINGER, TC_DRAIN_DONE, TC_PUMP_ON, TC_STORM_DONE, TC_RESPAWN, TC_HELMET, TC_BUMP, TC_CLANG,
    TC_RELEASE_A2, TC_RELEASE_B2, TC_FINALE_SPAWN, TC_ENDING, TC_VEND, TC_AMBIENT, TC_GATE_SLAM, TC_KEYCAB_OPEN, TC_FUEL_DONE, TC_POUR_DONE,
    TC_LIFT3, TC_SAVE,
};

// --- story text (story.cpp) ---
const char* note_title(int noteId);
std::string note_text(const Game& g, int noteId);
const char* tape_title(int tapeIdx);
const char* tape_text(int tapeIdx);
const char* item_name(int itemId);
struct EndLine { const char* text; float hold; };
const EndLine* ending_lines(int ending, int* count);
