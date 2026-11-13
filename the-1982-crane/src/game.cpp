// THE 1982 CRANE - core game logic: player, interaction, objectives, checkpoints
#include "game.h"
#include "audio.h"

// Pristine copy of the mutable parts of the world, restored on new game / load.
struct WorldSnapshot {
    std::vector<Collider> colliders;
    std::vector<Interact> interacts;
    std::vector<DynObj> dyn;
    std::vector<Light> lights;
    Door doors[D_COUNT];
    HideSpot hides[HIDE_COUNT];
    bool valid = false;
};
static WorldSnapshot g_snap;

static const float EYE_STAND = 1.62f, EYE_CROUCH = 0.98f;
static const float BODY_HALF = 0.25f, BODY_STAND = 1.75f, BODY_CROUCH = 1.15f, STEP_H = 0.36f;
static const float SPD_WALK = 2.6f, SPD_SPRINT = 5.3f, SPD_CROUCH = 1.3f;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
void say(Game& g, const char* speaker, const char* text, float dur, float delay) {
    Subtitle s;
    s.speaker = speaker ? speaker : "";
    s.text = text;
    s.time = dur > 0 ? dur : 1.7f + 0.052f * (float)std::strlen(text);
    s.delay = delay;
    if (g.subs.size() > 10) g.subs.erase(g.subs.begin());
    g.subs.push_back(s);
}
void hint(Game& g, const char* text, float t) { g.hint = text; g.hintT = t; }
void toast(Game& g, const char* text) { g.toast = text; g.toastT = 6.0f; }
void schedule(Game& g, float delay, int code, int arg) { g.timeline.push_back({delay, code, arg}); }
int tapes_found(const Game& g) {
    int n = 0;
    for (int i = IT_TAPE1; i <= IT_TAPE6; i++) n += g.itemTaken[i] ? 1 : 0;
    return n;
}
int notes_found(const Game& g) {
    int n = 0;
    for (int i = 0; i < N_COUNT; i++) n += g.noteRead[i] ? 1 : 0;
    return n;
}
bool game_player_busy(const Game& g) { return g.pstate != PS_NORMAL || g.actionLock > 0 || g.modal != MD_NONE; }

Vec3 game_cam_forward(const Game& g) {
    float cp = std::cos(g.camPitch);
    return Vec3(std::sin(g.camYaw) * cp, std::sin(g.camPitch), -std::cos(g.camYaw) * cp);
}
Mat4 game_view_matrix(const Game& g) {
    Vec3 f = game_cam_forward(g);
    Mat4 v = Mat4::lookAt(g.camPos, g.camPos + f, Vec3(0, 1, 0));
    if (g.camRoll != 0.0f) v = Mat4::rotZ(g.camRoll) * v;
    return v;
}
Vec3 crane_trolley_world(const Game& g) {
    return g.w.dyn[DY_CRANE_TOP].xf.point(Vec3(g.crane.trolley, CRANE_JIB_Y, 0));
}

const char* stage_title(const Game& g) {
    switch (g.stage) {
    case ST_FLASHLIGHT: return "Take the flashlight from the hood of your truck";
    case ST_BOOTH: return "Sign in at the guard booth";
    case ST_GATE: return "Open the yard gate";
    case ST_OFFICE_GO: return "Report to the site office";
    case ST_OFFICE_KEY: return "Find the office key";
    case ST_OFFICE_ENTER: return "Get into the site office and find out what happened";
    case ST_POWER: return "Get the yard generator running";
    case ST_KEYCAB: return "Take the crane key from the office key cabinet";
    case ST_BRAKE: return "Lock Crane 7 before you climb it";
    case ST_CLIMB: return "Climb Crane 7";
    case ST_CAB: return "The operator's cab";
    case ST_LOAD: return "Set the load down";
    case ST_ESCAPE: return "Get out of the yard";
    case ST_OPEN: return "Open the container";
    default: return "";
    }
}

static int fuses_held(const Game& g) { return (g.have[IT_FUSE_A] ? 1 : 0) + (g.have[IT_FUSE_B] ? 1 : 0) + (g.have[IT_FUSE_C] ? 1 : 0); }

void objective_lines(const Game& g, std::vector<std::string>& out, std::vector<int>& done) {
    out.clear(); done.clear();
    auto add = [&](const std::string& s, bool d) { out.push_back(s); done.push_back(d ? 1 : 0); };
    switch (g.stage) {
    case ST_FLASHLIGHT:
        add("Pick up the flashlight (on the truck's hood)", false);
        break;
    case ST_BOOTH:
        add("Read the gate log on the desk", g.flags[F_LOG_READ]);
        add("Take the radio handset", g.have[IT_RADIO]);
        break;
    case ST_GATE:
        add("Pull the manual gate release in the booth (north wall)", false);
        break;
    case ST_OFFICE_GO:
        add("The site office is the trailer up the road, on the right", false);
        break;
    case ST_OFFICE_KEY:
        add("Dale's locker - crew room, the green building further up the road", false);
        break;
    case ST_OFFICE_ENTER:
        add("Unlock the office", g.flags[F_OFFICE_UNLOCKED]);
        add("Read the report on the front desk", false);
        break;
    case ST_POWER:
        add(strf("Find the three lighting fuses (%d/3)", g.fusesInstalled + fuses_held(g)), g.fusesInstalled + fuses_held(g) >= 3);
        add(strf("Seat the fuses in the generator shed panel (%d/3)", g.fusesInstalled), g.fusesInstalled >= 3);
        add("Find a jerry can (workshop)", g.have[IT_JERRYCAN] || g.flags[F_GEN_FUELED]);
        add("Fill it at the diesel tank behind the generator shed", g.flags[F_CAN_FULL] || g.flags[F_GEN_FUELED]);
        add("Fuel the generator", g.flags[F_GEN_FUELED]);
        add("Pull the start lever", false);
        break;
    case ST_KEYCAB:
        add("Find Walt Kessler's badge number (time cards, crew room)", g.noteRead[N_TIMECARD]);
        add("Open the key cabinet in the site office", g.flags[F_KEYCAB_OPEN]);
        add("Take the crane key", false);
        break;
    case ST_BRAKE:
        add("Bolt cutters (workshop bench)", g.have[IT_CUTTERS]);
        add("Purge the flooded pump house: valves in the right order", g.flags[F_VALVES_DONE]);
        add("Cut the lockout off the brake pump starter", g.flags[F_LOTO_CUT]);
        add("Start the brake pump", g.flags[F_PUMP_ON]);
        add("Pull the slew lock on the crane's south-east leg", false);
        break;
    case ST_CLIMB:
        add("Unlock the ladder cage at the crane's foot", g.flags[F_CRANE_GATE_UNLOCKED]);
        add("Climb to the operator's cab", false);
        break;
    case ST_CAB:
        add("Read Walt's log on the right-hand console", g.flags[F_WALT_READ]);
        add("Take the controls", false);
        break;
    case ST_LOAD:
        add("Put it back: slew over the harbor, lower it under, release", false);
        add("...or land it on the hatch marks on the quay and release", false);
        break;
    case ST_ESCAPE:
        add("Climb down", g.pos.y < 2.0f);
        add("Run for the gate. Get to your truck.", false);
        break;
    case ST_OPEN:
        add("Climb down", g.pos.y < 2.0f);
        add("Open the container on the quay", false);
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Checkpoints
// ---------------------------------------------------------------------------
static std::string bits(const bool* a, int n) {
    std::string s;
    for (int i = 0; i < n; i++) s.push_back(a[i] ? '1' : '0');
    return s;
}
std::string game_save_string(const Game& g) {
    std::string s = "v2";
    s += strf(";stage=%d;diff=%d;code=%d;fuses=%d;valve=%d;deaths=%d;spare=%d", g.stage, g.difficulty, g.keyCode, g.fusesInstalled, g.valveStep,
              g.deaths, g.spareBatteries);
    s += strf(";pos=%.3f,%.3f,%.3f;yaw=%.3f;batt=%.3f;time=%.1f;seated=%d;flash=%d", g.pos.x, g.pos.y, g.pos.z, g.yaw, g.battery, g.time,
              (g.pstate == PS_SEATED || g.pstate == PS_OPERATING) ? 1 : 0, g.flashOn ? 1 : 0);
    s += ";flags=" + bits(g.flags, F_COUNT);
    s += ";events=" + bits(g.events, EV_COUNT);
    s += ";have=" + bits(g.have, IT_COUNT);
    s += ";taken=" + bits(g.itemTaken, IT_COUNT);
    s += ";notes=" + bits(g.noteRead, N_COUNT);
    return s;
}

void game_checkpoint(Game& g) {
    // never checkpoint in a state that cannot be resumed cleanly
    if (g.pstate == PS_DEAD || g.pstate == PS_LADDER || g.pstate == PS_HIDING || g.pstate == PS_CUTSCENE) {
        schedule(g, 1.0f, TC_SAVE);
        return;
    }
    g.saveData = game_save_string(g);
    g.hasSave = true;
    g.wantSave = true;
    g.checkpointT = 3.0f;
    g.lastCheckpointStage = g.stage;
}

static void reset_state(Game& g) {
    World& w = g.w;
    if (g_snap.valid) {
        w.colliders = g_snap.colliders;
        w.interacts = g_snap.interacts;
        w.dyn = g_snap.dyn;
        w.lights = g_snap.lights;
        for (int i = 0; i < D_COUNT; i++) w.doors[i] = g_snap.doors[i];
        for (int i = 0; i < HIDE_COUNT; i++) w.hides[i] = g_snap.hides[i];
    }
    audio_stop_all(0.2f);
    int diff = g.difficulty;
    int deaths = g.deaths;
    bool died = g.flags[F_DIED_ONCE];
    float aspect = g.aspect;
    float bobScale = g.bobScale;
    std::string save = g.saveData;
    bool hasSave = g.hasSave;
    World keep;
    std::swap(keep, g.w);
    g = Game();
    std::swap(keep, g.w);
    g.difficulty = diff;
    g.deaths = deaths;
    g.aspect = aspect;
    g.bobScale = bobScale;
    g.saveData = save;
    g.hasSave = hasSave;
    for (int i = 0; i < F_COUNT; i++) g.flags[i] = false;
    for (int i = 0; i < EV_COUNT; i++) g.events[i] = false;
    for (int i = 0; i < TR_COUNT; i++) g.triggerInside[i] = false;
    for (int i = 0; i < IT_COUNT; i++) { g.have[i] = false; g.itemTaken[i] = false; }
    for (int i = 0; i < N_COUNT; i++) g.noteRead[i] = false;
    for (int i = 0; i < 3; i++) g.valveSpin[i] = 0;
    for (int i = 0; i < 8; i++) g.leverAnim[i] = 0;
    for (int i = 0; i < EG_COUNT; i++) g.emis[i] = 0;
    g.flags[F_DIED_ONCE] = died;
    g.pos = g.w.playerStart;
    g.yaw = g.w.playerStartYaw;
    g.fade = 1.0f; g.fadeTarget = 0.0f; g.fadeSpeed = 0.5f;
}

void game_init(Game& g) {
    world_build(g.w);
    g_snap.colliders = g.w.colliders;
    g_snap.interacts = g.w.interacts;
    g_snap.dyn = g.w.dyn;
    g_snap.lights = g.w.lights;
    for (int i = 0; i < D_COUNT; i++) g_snap.doors[i] = g.w.doors[i];
    for (int i = 0; i < HIDE_COUNT; i++) g_snap.hides[i] = g.w.hides[i];
    g_snap.valid = true;
    reset_state(g);
    g.appMode = AM_TITLE;
    apply_world_state(g);
}

void game_to_title(Game& g, bool clearSave) {
    g.deaths = 0;
    g.flags[F_DIED_ONCE] = false;
    reset_state(g);
    if (clearSave) { g.saveData.clear(); g.hasSave = false; g.wantSave = true; }
    g.appMode = AM_TITLE;
    apply_world_state(g);
    g.fade = 1.0f; g.fadeTarget = 0.0f; g.fadeSpeed = 0.5f;
}

void game_new(Game& g, int difficulty) {
    g.deaths = 0;
    g.flags[F_DIED_ONCE] = false;
    g.difficulty = difficulty;
    reset_state(g);
    g.appMode = AM_PLAY;
    g.rng = Rng((u32)(1982u + (u32)difficulty * 7919u) ^ (u32)(g.aspect * 1000.0f));
    do { g.keyCode = 1000 + (int)(g.rng.next() % 9000u); } while (g.keyCode == 1982 || g.keyCode == 1234);
    g.stage = ST_FLASHLIGHT;
    apply_world_state(g);
    g.fade = 1.0f; g.fadeTarget = 0.0f; g.fadeSpeed = 0.35f;
    schedule(g, 2.5f, TC_SAY_STAGE, 100); // intro lines
    g.saveData = game_save_string(g);
    g.hasSave = true;
    g.wantSave = true;
    g.lastCheckpointStage = g.stage;
}

static bool parseBits(const std::string& v, bool* a, int n) {
    for (int i = 0; i < n; i++) a[i] = i < (int)v.size() && v[(size_t)i] == '1';
    return true;
}

bool game_load(Game& g, const std::string& data) {
    if (data.compare(0, 2, "v2") != 0) return false;
    int deaths = g.deaths;
    reset_state(g);
    g.deaths = deaths;
    g.appMode = AM_PLAY;
    bool seated = false;
    size_t p = 0;
    while (p < data.size()) {
        size_t e = data.find(';', p);
        if (e == std::string::npos) e = data.size();
        std::string kv = data.substr(p, e - p);
        p = e + 1;
        size_t eq = kv.find('=');
        if (eq == std::string::npos) continue;
        std::string k = kv.substr(0, eq), v = kv.substr(eq + 1);
        if (k == "stage") g.stage = clampv(atoi(v.c_str()), 0, (int)ST_END);
        else if (k == "diff") g.difficulty = clampv(atoi(v.c_str()), 0, 2);
        else if (k == "code") g.keyCode = clampv(atoi(v.c_str()), 0, 9999);
        else if (k == "fuses") g.fusesInstalled = clampv(atoi(v.c_str()), 0, 3);
        else if (k == "valve") g.valveStep = clampv(atoi(v.c_str()), 0, 3);
        else if (k == "deaths") g.deaths = std::max(g.deaths, atoi(v.c_str()));
        else if (k == "spare") g.spareBatteries = clampv(atoi(v.c_str()), 0, 20);
        else if (k == "pos") {
            float x = 0, y = 0, z = 0;
            if (sscanf(v.c_str(), "%f,%f,%f", &x, &y, &z) == 3) g.pos = Vec3(x, y, z);
        } else if (k == "yaw") g.yaw = (float)atof(v.c_str());
        else if (k == "batt") g.battery = clampv((float)atof(v.c_str()), 0.0f, 1.0f);
        else if (k == "time") g.time = atof(v.c_str());
        else if (k == "seated") seated = atoi(v.c_str()) != 0;
        else if (k == "flash") g.flashOn = atoi(v.c_str()) != 0;
        else if (k == "flags") parseBits(v, g.flags, F_COUNT);
        else if (k == "events") parseBits(v, g.events, EV_COUNT);
        else if (k == "have") parseBits(v, g.have, IT_COUNT);
        else if (k == "taken") parseBits(v, g.itemTaken, IT_COUNT);
        else if (k == "notes") parseBits(v, g.noteRead, N_COUNT);
    }
    if (g.stage >= ST_END) g.stage = ST_FLASHLIGHT;
    g.rng = Rng((u32)(g.keyCode * 31 + g.deaths * 977 + 5));
    if (g.deaths > 0) g.flags[F_DIED_ONCE] = true;
    // a give-up battery never soft-locks a reload
    if (g.battery < 0.25f && g.have[IT_FLASHLIGHT]) g.battery = 0.25f;
    g.saveData = data;
    g.hasSave = true;
    g.lastCheckpointStage = g.stage;
    apply_world_state(g);
    if (seated || g.stage == ST_CAB || g.stage == ST_LOAD) {
        g.pstate = PS_SEATED;
        g.seatYaw = 0; g.seatPitch = -0.1f;
    }
    g.fade = 1.0f; g.fadeTarget = 0.0f; g.fadeSpeed = 0.6f;
    return true;
}

// Rebuilds everything that is derived from flags / inventory.
void apply_world_state(Game& g) {
    World& w = g.w;
    // items
    for (int i = 0; i < IT_COUNT; i++) {
        bool taken = g.itemTaken[i];
        if (w.itemDyn[i] >= 0) w.dyn[(size_t)w.itemDyn[i]].visible = !taken;
        if (w.itemInteract[i] >= 0) w.interacts[(size_t)w.itemInteract[i]].enabled = !taken;
    }
    // locked away until the key cabinet opens
    if (!g.flags[F_KEYCAB_OPEN]) {
        for (int it : {IT_CRANE_KEY, IT_TAPE1}) {
            w.dyn[(size_t)w.itemDyn[it]].visible = false;
            w.interacts[(size_t)w.itemInteract[it]].enabled = false;
        }
    }
    w.dyn[DY_KEYCAB_DOOR].visible = !g.flags[F_KEYCAB_OPEN];
    // the vending machine's prize
    if (!g.flags[F_VENDED]) {
        w.dyn[(size_t)w.itemDyn[IT_BATT8]].visible = false;
        w.interacts[(size_t)w.itemInteract[IT_BATT8]].enabled = false;
    }
    // cab interactables only while seated; the cab tape rides with the crane
    bool inCab = g.pstate == PS_SEATED || g.pstate == PS_OPERATING || g.stage == ST_CAB || g.stage == ST_LOAD;
    w.interacts[(size_t)w.cabNoteInteract].enabled = inCab;
    w.interacts[(size_t)w.cabControlsInteract].enabled = inCab;
    w.interacts[(size_t)w.cabLeaveInteract].enabled = inCab;
    w.interacts[(size_t)w.itemInteract[IT_TAPE6]].enabled = inCab && !g.itemTaken[IT_TAPE6];
    // doors
    g.gateT = g.flags[F_GATE_OPEN] ? 1.0f : 0.0f;
    w.setColliderEnabled(w.gateCollider, !g.flags[F_GATE_OPEN]);
    if (g.flags[F_OFFICE_UNLOCKED]) w.doors[D_OFFICE].lockedBy = -1;
    if (g.flags[F_CRANE_GATE_UNLOCKED]) w.doors[D_CRANE_GATE].lockedBy = -1;
    if (g.flags[F_LIFT3_OPEN]) { w.doors[D_LIFT3].target = w.doors[D_LIFT3].openAngle * 0.35f; w.doors[D_LIFT3].angle = w.doors[D_LIFT3].target; }
    w.dyn[DY_BOOTPRINTS].visible = g.flags[F_LIFT3_OPEN];
    // generator / power
    for (int i = 0; i < 3; i++) w.dyn[DY_FUSE_A + i].visible = i < g.fusesInstalled;
    g.genSpin = g.flags[F_GEN_RUNNING] ? 1.0f : 0.0f;
    g.powerUp = g.flags[F_GEN_RUNNING] ? 1.0f : 0.0f;
    for (Light& l : w.lights)
        if (l.group == LG_YARD || l.group == LG_INDOOR) l.on = true; // gated by powerUp
    // pump house
    g.waterLevel = g.flags[F_DRAINED] ? -0.3f : 0.34f;
    w.dyn[DY_LOTO].visible = !g.flags[F_LOTO_CUT];
    w.interacts[(size_t)w.useInteract[U_LOTO]].enabled = !g.flags[F_LOTO_CUT];
    w.interacts[(size_t)w.useInteract[U_BREAKER]].enabled = g.flags[F_LOTO_CUT];
    if (g.flags[F_VALVES_DONE]) g.valveStep = 3;
    // crane
    CraneSim& c = g.crane;
    c = CraneSim();
    c.locked = g.flags[F_STORM_LOCKED];
    if (c.locked) { c.slew = 0.0f; c.autoTarget = 0.0f; c.trolley = 20.0f; c.hoist = 18.9f; }
    if (g.flags[F_RELEASED_WATER]) { c.loadReleased = true; c.slew = PI * 0.5f; c.trolley = 26.0f; c.hoist = 33.5f; c.sink = 30.0f; }
    if (g.flags[F_RELEASED_QUAY]) { c.loadReleased = true; c.slew = PAD_SLEW; c.trolley = PAD_R; c.hoist = 27.9f; }
    w.interacts[(size_t)w.useInteract[U_LOAD_DOOR]].enabled = g.flags[F_RELEASED_QUAY];
    w.dyn[DY_CHALK_AGAIN].visible = g.flags[F_DIED_ONCE];
    w.dyn[DY_CRT_OFFICE].visible = g.events[EV_CRT_ON];
    w.lights[(size_t)w.lightCrtOffice].on = g.events[EV_CRT_ON];
    w.dyn[DY_HARDHAT].visible = !g.events[EV_LOCKER_SLAM];
    // the Rigger
    g.rig = Rigger();
    if (g.flags[F_RIGGER_ACTIVE] && g.stage < ST_CAB) {
        // start it far from the player with a grace period
        Vec3 best = w.riggerSpawn;
        float bd = -1;
        for (const Vec3& p : w.patrolPoints) {
            float d = distXZ(p, g.pos);
            if (d > bd && nav_walkable(w, p)) { bd = d; best = p; }
        }
        rigger_spawn(g, best, RS_PATROL);
        g.rig.grace = 25.0f;
    }
    if (g.stage == ST_ESCAPE && g.flags[F_FINALE]) g.flags[F_FINALE] = false; // the chase restarts when the player reaches the ground
    if (g.stage == ST_OPEN) {
        Vec3 d = pad_dir();
        rigger_spawn(g, pad_pos() + d * (LOAD_L * 0.5f + 3.0f) + Vec3(-d.z, 0, d.x) * 1.9f, RS_STATUE);
    }
    if (g.flags[F_RADIO_ON]) g.flags[F_RADIO_ON] = false;
    g.helmetYaw = g.helmetTarget = g.flags[F_HELMET_SEEN] ? -0.9f : 2.4f;
    g.dawn = 0;
    g.triggerInside[0] = false;
}

// ---------------------------------------------------------------------------
// Stage machine
// ---------------------------------------------------------------------------
void set_stage(Game& g, int stage) {
    if (stage == g.stage) return;
    g.stage = stage;
    toast(g, stage_title(g));
    audio_play(S_UI, 0.5f, 0.8f);
    schedule(g, 1.6f, TC_SAY_STAGE, stage);
    schedule(g, 0.1f, TC_SAVE);
}

void check_progress(Game& g) {
    for (int guard = 0; guard < 12; guard++) {
        bool ok = false;
        switch (g.stage) {
        case ST_FLASHLIGHT: ok = g.have[IT_FLASHLIGHT]; break;
        case ST_BOOTH: ok = g.flags[F_LOG_READ] && g.have[IT_RADIO]; break;
        case ST_GATE: ok = g.flags[F_GATE_OPEN]; break;
        case ST_OFFICE_GO: ok = g.noteRead[N_OFFICE_DOOR] || g.flags[F_TRIED_OFFICE] || g.have[IT_OFFICE_KEY] || g.flags[F_OFFICE_UNLOCKED]; break;
        case ST_OFFICE_KEY: ok = g.have[IT_OFFICE_KEY] || g.flags[F_OFFICE_UNLOCKED]; break;
        case ST_OFFICE_ENTER: ok = g.noteRead[N_INCIDENT]; break;
        case ST_POWER: ok = g.flags[F_GEN_RUNNING]; break;
        case ST_KEYCAB: ok = g.have[IT_CRANE_KEY]; break;
        case ST_BRAKE: ok = g.flags[F_STORM_LOCKED]; break;
        default: break;
        }
        if (!ok) break;
        set_stage(g, g.stage + 1);
    }
}

void kill_player(Game& g, int cause) {
    if (g.pstate == PS_DEAD || g.appMode != AM_PLAY) return;
    g.pstate = PS_DEAD;
    g.modal = MD_NONE;
    g.deadT = 0;
    g.deathCause = cause;
    g.deaths++;
    g.flags[F_DIED_ONCE] = true;
    g.actionLock = 0;
    g.shake = 1.0f;
    g.flash = cause == 1 ? 0.0f : 0.6f;
    g.subs.clear();
    audio_play(cause == 1 ? S_SPLASH : S_SCREAM, 1.0f);
    audio_play(S_DEATH, 0.9f);
    schedule(g, 5.2f, TC_RESPAWN);
}

void open_note(Game& g, int noteId) {
    g.noteOpen = noteId;
    g.noteScroll = 0;
    g.modal = MD_NOTE;
    if (noteId >= 0 && noteId < N_COUNT) g.noteRead[noteId] = true;
    audio_play(S_PAPER, 0.7f);
}

void play_tape(Game& g, int t) {
    if (t < 0 || t > 5) return;
    audio_play(S_TAPE_CLICK, 0.8f);
    if (g.hTape >= 0) audio_stop(g.hTape, 0.2f);
    g.hTape = audio_loop(S_TAPE_HISS, 0.5f);
    g.tapePlaying = t;
    // drop any tape lines still queued from a previous tape
    for (size_t i = 0; i < g.subs.size();)
        if (g.subs[i].speaker == "TAPE") g.subs.erase(g.subs.begin() + (long)i); else i++;
    const char* txt = tape_text(t);
    std::string line;
    float total = 0.8f;
    bool first = true;
    for (const char* p = txt;; p++) {
        if (*p == '\n' || *p == 0) {
            if (!line.empty()) {
                float dur = 2.0f + 0.058f * (float)line.size();
                say(g, "TAPE", line.c_str(), dur, first ? 0.8f : 0.35f);
                total += dur + 0.35f;
                first = false;
            }
            line.clear();
            if (*p == 0) break;
        } else line.push_back(*p);
    }
    g.tapeT = total + 0.6f;
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------
std::string interact_prompt(const Game& g, int idx) {
    if (idx < 0 || idx >= (int)g.w.interacts.size()) return "";
    const Interact& it = g.w.interacts[(size_t)idx];
    switch (it.kind) {
    case IK_PICKUP: return std::string("Take ") + (it.arg == IT_FLASHLIGHT ? "flashlight" : it.arg == IT_RADIO ? "radio handset" : it.arg == IT_OFFICE_KEY ? "office key"
                                                    : (it.arg >= IT_FUSE_A && it.arg <= IT_FUSE_C) ? "fuse" : it.arg == IT_JERRYCAN ? "jerry can"
                                                    : it.arg == IT_CUTTERS ? "bolt cutters" : it.arg == IT_CRANE_KEY ? "crane key"
                                                    : (it.arg >= IT_TAPE1 && it.arg <= IT_TAPE6) ? "cassette" : "batteries");
    case IK_NOTE:
        if (it.arg == N_BLUEPRINT) return "Look at the site plan";
        if (it.arg == N_PHOTO) return "Look at the photograph";
        if (it.arg == N_PLATE) return "Read the maker's plate";
        if (it.arg == N_RELIEF_LOCKER) return "Open the locker marked RELIEF";
        if (it.arg == N_TIMECARD) return "Look through the time cards";
        if (it.arg == N_NEWSPAPER) return "Read the newspaper";
        return "Read";
    case IK_DOOR: {
        const Door& d = g.w.doors[it.arg];
        if (d.lockedBy == -2) return "Try the door";
        if (d.lockedBy >= 0) return g.have[d.lockedBy] ? "Unlock" : "Try the door";
        bool open = std::fabs(d.target) > 0.01f;
        if (it.arg == D_CRANE_GATE) return open ? "Close the cage" : "Open the cage";
        return open ? "Close door" : "Open door";
    }
    case IK_HIDE: return g.pstate == PS_HIDING ? "Step out" : "Hide inside";
    case IK_LADDER: return (it.arg & 1) ? "Climb down" : "Climb up";
    case IK_USE:
        switch (it.arg) {
        case U_GATE_LEVER: return g.flags[F_GATE_OPEN] ? "" : "Pull the gate release";
        case U_FUSEBOX: return fuses_held(g) > 0 ? "Seat the fuses" : "Look at the fuse panel";
        case U_FUEL_TAP: return "Use the diesel pump";
        case U_GEN_FUEL: return "Fuel the generator";
        case U_GEN_START: return g.flags[F_GEN_RUNNING] ? "" : "Pull the start lever";
        case U_KEYPAD: return g.flags[F_KEYCAB_OPEN] ? "" : "Use the keypad";
        case U_PHONE: return g.phoneRingT >= 0 ? "Answer the telephone" : "Lift the receiver";
        case U_RADIO_SET: return g.flags[F_RADIO_ON] ? "Switch the radio off" : "Switch the radio on";
        case U_VALVE_A: return g.flags[F_VALVES_DONE] ? "" : "Turn valve A";
        case U_VALVE_B: return g.flags[F_VALVES_DONE] ? "" : "Turn valve B";
        case U_VALVE_C: return g.flags[F_VALVES_DONE] ? "" : "Turn valve C";
        case U_LOTO: return g.have[IT_CUTTERS] ? "Cut the lockout" : "Look at the lockout";
        case U_BREAKER: return g.flags[F_PUMP_ON] ? "" : "Throw the pump starter";
        case U_STORMLOCK: return g.flags[F_STORM_LOCKED] ? "" : "Pull the slew lock";
        case U_CAB_DOOR: return "Enter the cab";
        case U_KNOCK: return "Knock";
        case U_LOAD_DOOR: return "Open the container";
        case U_TRUCK: return (g.stage == ST_ESCAPE) ? "Get in and drive" : "Your truck";
        case U_CRT: return "Read the terminal";
        case U_LIFT1: return "Look at the car";
        case U_LIFT2: return "Touch the bell";
        case U_HELMET: return "Look at the helmet";
        case U_VENDING: return g.flags[F_VENDED] ? "" : "Try the machine";
        case U_CAB_LEAVE: return "Leave the cab";
        case U_CAB_CONTROLS: return "Take the controls";
        case U_SINK: return "Turn the tap";
        default: return "Use";
        }
    default: return "";
    }
}

// Returns the interact hit by a ray, or -1.
static int pick_interact(const Game& g, const Vec3& o, const Vec3& d) {
    const World& w = g.w;
    int best = -1;
    float bestT = 1e9f;
    for (size_t i = 0; i < w.interacts.size(); i++) {
        const Interact& it = w.interacts[i];
        if (!it.enabled) continue;
        if (g.pstate == PS_HIDING && !(it.kind == IK_HIDE && it.arg == g.hideIdx)) continue;
        bool local = it.xfDyn >= 0;
        if ((g.pstate == PS_SEATED || g.pstate == PS_OPERATING) != local && !(local && it.xfDyn == DY_LOAD)) continue;
        Vec3 ro = o, rd = d;
        if (local) {
            Mat4 inv = w.dyn[(size_t)it.xfDyn].xf.inverse();
            ro = inv.point(o);
            rd = inv.dir(d);
        }
        float t;
        if (!rayAABB(ro, rd, it.box, it.range, t)) continue;
        // standing inside a big volume: rank it by its centre so things in front of it still win
        if (t <= 0.0f) t = std::min(it.range * 0.95f, length(it.box.center() - ro));
        if (t >= bestT) continue;
        if (interact_prompt(g, (int)i).empty()) continue; // nothing to do with it right now: look past it
        bestT = t; best = (int)i;
    }
    if (best < 0) return -1;
    if (g.w.interacts[(size_t)best].xfDyn < 0) {
        float block = w.raycast(o, d, bestT, COL_SIGHT);
        if (block < bestT - 0.12f) return -1;
    }
    return best;
}

static void take_item(Game& g, int item) {
    World& w = g.w;
    g.itemTaken[item] = true;
    if (w.itemDyn[item] >= 0) w.dyn[(size_t)w.itemDyn[item]].visible = false;
    if (w.itemInteract[item] >= 0) w.interacts[(size_t)w.itemInteract[item]].enabled = false;
    audio_play(item == IT_OFFICE_KEY || item == IT_CRANE_KEY ? S_KEYS : S_PICKUP, 0.8f);
    if (item >= IT_BATT1) {
        g.spareBatteries++;
        hint(g, strf("Batteries. Spare sets: %d", g.spareBatteries).c_str());
        return;
    }
    if (item >= IT_TAPE1 && item <= IT_TAPE6) {
        hint(g, strf("%s  (%d/6)", tape_title(item - IT_TAPE1), tapes_found(g)).c_str(), 4.0f);
        play_tape(g, item - IT_TAPE1);
        return;
    }
    g.have[item] = true;
    switch (item) {
    case IT_FLASHLIGHT:
        g.flashOn = true;
        audio_play(S_SWITCH, 0.7f);
        hint(g, "Flashlight. Turn it off and on with F (LAMP on a touch screen). Batteries do not last.", 6.0f);
        break;
    case IT_RADIO:
        audio_play(S_RADIO_ON, 0.8f);
        hint(g, "Radio handset. Yard channel.");
        break;
    case IT_OFFICE_KEY:
        hint(g, "Site office key.");
        schedule(g, 1.4f, TC_LOCKER_SLAM);
        break;
    case IT_FUSE_A: case IT_FUSE_B: case IT_FUSE_C:
        hint(g, strf("30 A fuse  (%d/3)", g.fusesInstalled + fuses_held(g)).c_str());
        break;
    case IT_JERRYCAN: hint(g, "Jerry can. Empty."); break;
    case IT_CUTTERS: hint(g, "Bolt cutters."); break;
    case IT_CRANE_KEY: hint(g, "Crane 7 access key. It is cold, and wet."); break;
    default: break;
    }
}

static void toggle_door(Game& g, int id) {
    World& w = g.w;
    Door& d = w.doors[id];
    Vec3 dp = w.doorPos[id];
    if (d.lockedBy == -2) {
        audio_play_at(S_DOOR_LOCKED, dp, 0.8f);
        if (id == D_LIFT3) hint(g, g.flags[F_LIFT3_OPEN] ? "It stands a hand's width open. Cold pours through the gap. There is nothing behind it but the quay."
                                                         : "Locked. From which side? Cold comes through the keyhole.", 5.0f);
        return;
    }
    if (d.lockedBy >= 0) {
        if (!g.have[d.lockedBy]) {
            audio_play_at(S_DOOR_LOCKED, dp, 0.8f);
            if (id == D_OFFICE) { g.flags[F_TRIED_OFFICE] = true; hint(g, "Locked. There is a note taped beside the door."); }
            else if (id == D_CRANE_GATE) hint(g, "Chained and padlocked. The crane key is in the office key cabinet.", 4.5f);
            else hint(g, "Locked.");
            check_progress(g);
            return;
        }
        d.lockedBy = -1;
        audio_play_at(S_KEYS, dp, 0.8f);
        if (id == D_OFFICE) g.flags[F_OFFICE_UNLOCKED] = true;
        if (id == D_CRANE_GATE) g.flags[F_CRANE_GATE_UNLOCKED] = true;
    }
    bool open = std::fabs(d.target) > 0.01f;
    d.target = open ? 0.0f : d.openAngle;
    audio_play_at(open ? S_DOOR_CLOSE : (d.metal ? S_DOOR_METAL : S_DOOR_OPEN), dp, 0.75f);
    alert_noise(g, dp, 7.0f);
    check_progress(g);
}

static void start_ladder(Game& g, int arg) {
    int lad = arg / 2, dir = arg & 1;
    if (lad == LAD_PORTAL && dir == 0 && !g.flags[F_STORM_LOCKED]) {
        g.flags[F_TRIED_LADDER] = true;
        hint(g, "The whole mast is swinging. If it slews while you are on the ladder it will shake you off. Lock it first.", 6.0f);
        return;
    }
    g.pstate = PS_LADDER;
    g.ladder = lad; g.ladderDir = dir;
    g.ladderPhase = 0; g.ladderT = 0;
    g.ladderFrom = g.pos;
    g.vel = Vec3(0, 0, 0);
    g.crouched = false;
}

static void enter_hide(Game& g, int idx) {
    g.pstate = PS_HIDING;
    g.hideIdx = idx;
    g.hideT = 0;
    g.vel = Vec3(0, 0, 0);
    g.crouched = false;
    if (g.flashOn) { g.flashOn = false; audio_play(S_SWITCH, 0.4f); }
    audio_play(S_LOCKER, 0.7f);
    g.rig.sawPlayerHide = (g.rig.state == RS_CHASE && g.rig.lastSeen < 1.6f);
    g.yaw = g.w.hides[idx].yaw;
    g.pitch = 0;
}

static void exit_hide(Game& g) {
    if (g.hideIdx < 0) return;
    const HideSpot& h = g.w.hides[g.hideIdx];
    g.pos = h.outside;
    g.pos.y = std::max(g.w.floorBelow(h.outside + Vec3(0, 0.5f, 0), 3.0f), 0.0f);
    g.pstate = PS_NORMAL;
    g.hideIdx = -1;
    audio_play(S_LOCKER, 0.6f);
}

static void enter_cab(Game& g) {
    World& w = g.w;
    g.pstate = PS_SEATED;
    g.seatYaw = 0; g.seatPitch = -0.15f;
    g.vel = Vec3(0, 0, 0);
    g.fade = 1.0f; g.fadeTarget = 0; g.fadeSpeed = 1.2f;
    audio_play(S_DOOR_METAL, 0.8f);
    w.interacts[(size_t)w.cabNoteInteract].enabled = true;
    w.interacts[(size_t)w.cabControlsInteract].enabled = true;
    w.interacts[(size_t)w.cabLeaveInteract].enabled = true;
    w.interacts[(size_t)w.itemInteract[IT_TAPE6]].enabled = !g.itemTaken[IT_TAPE6];
    if (g.stage == ST_CLIMB) set_stage(g, ST_CAB);
    // nothing follows you up here. Not yet.
    if (g.rig.state != RS_DORMANT && g.rig.state != RS_STATUE) g.rig.state = RS_DORMANT;
}

static void leave_cab(Game& g) {
    World& w = g.w;
    const Ladder& l = w.ladders[LAD_MAST4];
    g.pstate = PS_NORMAL;
    g.pos = l.top + Vec3(0.4f, 0, -0.8f);
    g.yaw = -PI * 0.5f;
    g.pitch = 0;
    g.fade = 1.0f; g.fadeTarget = 0; g.fadeSpeed = 1.2f;
    audio_play(S_DOOR_METAL, 0.8f);
    w.interacts[(size_t)w.cabNoteInteract].enabled = false;
    w.interacts[(size_t)w.cabControlsInteract].enabled = false;
    w.interacts[(size_t)w.cabLeaveInteract].enabled = false;
    w.interacts[(size_t)w.itemInteract[IT_TAPE6]].enabled = false;
}

void use_object(Game& g, int u) {
    World& w = g.w;
    Vec3 up = w.usePos[u];
    switch (u) {
    case U_GATE_LEVER:
        if (g.flags[F_GATE_OPEN]) break;
        if (g.stage < ST_GATE) { hint(g, "Sign in first: the gate log is on the desk, the radio is on its charger."); break; }
        g.flags[F_GATE_OPEN] = true;
        g.leverAnim[0] = 0.001f;
        audio_play_at(S_LEVER, up, 0.9f);
        audio_play_at(S_GATE, Vec3(0, 1.5f, 56), 1.0f, 1.0f, 12.0f);
        check_progress(g);
        break;
    case U_FUSEBOX: {
        int n = fuses_held(g);
        if (n == 0) {
            hint(g, g.fusesInstalled >= 3 ? "All three fuses are seated." : strf("Yard lighting panel. %d of 3 fuse holders are filled.", g.fusesInstalled).c_str(), 4.0f);
            break;
        }
        for (int f = IT_FUSE_A; f <= IT_FUSE_C; f++)
            if (g.have[f]) { g.have[f] = false; g.fusesInstalled++; }
        g.fusesInstalled = std::min(g.fusesInstalled, 3);
        for (int i = 0; i < 3; i++) w.dyn[DY_FUSE_A + i].visible = i < g.fusesInstalled;
        audio_play_at(S_FUSE, up, 0.9f);
        hint(g, g.fusesInstalled >= 3 ? "Three fuses seated. The panel is whole again." : strf("Fuse seated. %d of 3.", g.fusesInstalled).c_str(), 4.0f);
        schedule(g, 0.2f, TC_SAVE);
        break;
    }
    case U_FUEL_TAP:
        if (!g.have[IT_JERRYCAN]) { hint(g, "A hand pump on the bulk diesel tank. You need something to carry it in."); break; }
        if (g.flags[F_CAN_FULL]) { hint(g, "The can is already full."); break; }
        if (g.flags[F_GEN_FUELED]) { hint(g, "The generator has its fuel."); break; }
        g.actionLock = 3.6f; g.actionText = "Pumping diesel...";
        audio_play_at(S_POUR, up, 0.8f, 0.8f);
        schedule(g, 3.6f, TC_FUEL_DONE);
        break;
    case U_GEN_FUEL:
        if (g.flags[F_GEN_FUELED]) { hint(g, "The tank is full."); break; }
        if (!g.have[IT_JERRYCAN]) { hint(g, "The filler cap. The gauge beside it reads empty. You need a can of diesel."); break; }
        if (!g.flags[F_CAN_FULL]) { hint(g, "Your can is empty. The bulk tank is outside, behind the shed."); break; }
        g.actionLock = 4.2f; g.actionText = "Filling the tank...";
        audio_play_at(S_POUR, up, 0.9f);
        schedule(g, 4.2f, TC_POUR_DONE);
        break;
    case U_GEN_START:
        if (g.flags[F_GEN_RUNNING]) break;
        if (g.fusesInstalled < 3 || !g.flags[F_GEN_FUELED]) {
            g.leverAnim[1] = 0.001f;
            audio_play_at(S_LEVER, up, 0.7f);
            if (g.fusesInstalled < 3 && !g.flags[F_GEN_FUELED]) hint(g, "Nothing. No fuel in the tank, and the lighting panel is missing fuses.", 4.5f);
            else if (!g.flags[F_GEN_FUELED]) hint(g, "It turns over dry. The tank is empty.", 4.0f);
            else hint(g, strf("It would start, but the lighting panel only has %d of its 3 fuses.", g.fusesInstalled).c_str(), 4.5f);
            break;
        }
        g.leverAnim[1] = 0.001f;
        audio_play_at(S_GEN_CRANK, up, 1.0f);
        alert_noise(g, up, 30.0f);
        if (!g.flags[F_GEN_TRIED]) {
            g.flags[F_GEN_TRIED] = true;
            g.actionLock = 2.4f; g.actionText = "";
            hint(g, "It turns over, coughs, and dies. Again.", 3.5f);
        } else {
            g.actionLock = 2.6f; g.actionText = "";
            schedule(g, 2.3f, TC_GEN_CATCH);
        }
        break;
    case U_KEYPAD:
        if (g.flags[F_KEYCAB_OPEN]) break;
        if (!g.flags[F_GEN_RUNNING]) { hint(g, "An electric lock. The keypad is dead: no power."); break; }
        g.modal = MD_KEYPAD;
        g.keypadEntry.clear();
        g.keypadMsg = 0;
        break;
    case U_PHONE:
        if (g.phoneRingT >= 0) {
            g.phoneRingT = -1;
            if (g.hPhone >= 0) audio_stop(g.hPhone, 0.05f);
            g.hPhone = -1;
            g.flags[F_PHONE_ANSWERED] = true;
            audio_play(S_PHONE_PICK, 0.8f);
            audio_play(S_WHISPER, 0.6f);
            say(g, "PHONE", "(a wet hiss, like a shell held to the ear)", 3.5f, 0.6f);
            say(g, "PHONE", "...you're late, relief.", 3.5f, 0.3f);
            say(g, "PHONE", "Your name is already in the book.", 4.0f, 0.3f);
            say(g, "PHONE", "Come up and see.", 3.5f, 0.6f);
            g.fear = std::min(1.0f, g.fear + 0.3f);
        } else {
            audio_play(S_PHONE_PICK, 0.6f);
            hint(g, g.flags[F_PHONE_ANSWERED] ? "The line is open. Somebody is breathing very carefully." : "Dead. Not even a tone.");
        }
        break;
    case U_RADIO_SET:
        if (!g.flags[F_GEN_RUNNING]) { audio_play_at(S_SWITCH, up, 0.6f); hint(g, "A valve radio. Dead: no power."); break; }
        g.flags[F_RADIO_ON] = !g.flags[F_RADIO_ON];
        audio_play_at(S_SWITCH, up, 0.7f);
        if (g.flags[F_RADIO_ON]) {
            g.hRadio = audio_loop_at(S_RADIO_TUNE, up, 0.7f, 4.0f);
            hint(g, "A dance band, a long way off. You know the tune. You cannot remember ever hearing it.", 5.0f);
        } else if (g.hRadio >= 0) { audio_stop(g.hRadio, 0.3f); g.hRadio = -1; }
        break;
    case U_VALVE_A: case U_VALVE_B: case U_VALVE_C: {
        if (g.flags[F_VALVES_DONE]) break;
        static const int order[3] = {2, 0, 1}; // C, A, B
        int v = u - U_VALVE_A;
        g.valveSpin[v] += 3.0f;
        audio_play_at(S_VALVE, up, 0.9f);
        if (order[g.valveStep] == v) {
            g.valveStep++;
            if (g.valveStep >= 3) {
                g.flags[F_VALVES_DONE] = true;
                g.hWaterDrain = audio_loop_at(S_DRAIN, Vec3(48, 0, -36), 0.9f, 6.0f);
                hint(g, "Somewhere under the floor a sluice opens. The water starts to go down.", 5.0f);
                schedule(g, 13.0f, TC_DRAIN_DONE);
                schedule(g, 6.0f, TC_BUMP);
            } else hint(g, g.valveStep == 1 ? "The wheel gives. Something in the pipes shifts." : "A second groan from the manifold.");
        } else {
            g.valveStep = 0;
            audio_play_at(S_METAL_BANG, up, 1.0f);
            g.shake = std::max(g.shake, 0.4f);
            alert_noise(g, up, 28.0f);
            hint(g, "The relief valve trips with a bang you feel in your teeth. Wrong order. Start again.", 5.0f);
        }
        break;
    }
    case U_LOTO:
        if (!g.have[IT_CUTTERS]) { hint(g, "A lockout padlock and a red tag: W. KESSLER 11/8. You need something to cut it.", 5.0f); break; }
        g.flags[F_LOTO_CUT] = true;
        w.dyn[DY_LOTO].visible = false;
        w.interacts[(size_t)w.useInteract[U_LOTO]].enabled = false;
        w.interacts[(size_t)w.useInteract[U_BREAKER]].enabled = true;
        audio_play_at(S_CUT, up, 1.0f);
        hint(g, "The shackle parts. Walt's tag falls into the water, face up.", 4.5f);
        schedule(g, 0.2f, TC_SAVE);
        break;
    case U_BREAKER:
        if (g.flags[F_PUMP_ON]) break;
        if (!g.flags[F_DRAINED]) {
            hint(g, g.flags[F_VALVES_DONE] ? "Wait for the water to go down." : "You are standing in a foot of water. Not while it is flooded. Purge it first.", 5.0f);
            break;
        }
        g.flags[F_PUMP_ON] = true;
        g.leverAnim[2] = 0.001f;
        audio_play_at(S_BREAKER, up, 1.0f);
        g.hPump = audio_loop_at(S_PUMP_LOOP, Vec3(50.5f, 1, -36), 0.8f, 6.0f);
        alert_noise(g, up, 24.0f);
        hint(g, "The pump runs up. Brake pressure is building. Now the slew lock, on the crane's south-east leg.", 6.0f);
        schedule(g, 0.2f, TC_SAVE);
        break;
    case U_STORMLOCK:
        if (g.flags[F_STORM_LOCKED]) break;
        if (!g.flags[F_PUMP_ON]) {
            g.flags[F_TRIED_STORM] = true;
            audio_play_at(S_LEVER, up, 0.6f);
            hint(g, "The lever is slack. The gauge beside it reads zero. No brake pressure: the pump is in Pump House 2.", 6.0f);
            break;
        }
        g.flags[F_STORM_LOCKED] = true;
        g.leverAnim[3] = 0.001f;
        g.crane.locked = true;
        audio_play_at(S_LEVER, up, 1.0f);
        audio_play_at(S_CRANE_GROAN, CRANE_POS + Vec3(0, 20, 0), 1.0f, 0.7f, 30.0f);
        audio_play(S_STINGER, 0.8f);
        g.shake = std::max(g.shake, 0.8f);
        g.blackout = 1.0f;
        g.fear = 1.0f;
        schedule(g, 3.0f, TC_STORM_DONE);
        check_progress(g);
        break;
    case U_CAB_DOOR: enter_cab(g); break;
    case U_CAB_LEAVE:
        if (g.stage == ST_LOAD && !g.crane.loadReleased) { hint(g, "Not yet. Not with that still on the hook."); break; }
        leave_cab(g);
        break;
    case U_CAB_CONTROLS:
        if (g.crane.loadReleased) { hint(g, "The levers are dead. It has let go of them."); break; }
        if (!g.flags[F_WALT_READ]) { hint(g, "Walt left his log on the right-hand console. Read it first.", 4.0f); break; }
        g.pstate = PS_OPERATING;
        if (g.stage == ST_CAB) set_stage(g, ST_LOAD);
        hint(g, "SLEW turns the jib. TROLLEY runs the load in and out. HOIST raises and lowers. RELEASE lets go.", 8.0f);
        break;
    case U_KNOCK:
        audio_play_at(S_KNOCK, up, 0.9f);
        alert_noise(g, up, 10.0f);
        if (!g.flags[F_KNOCKED]) {
            g.flags[F_KNOCKED] = true;
            schedule(g, 2.2f, TC_KNOCK_REPLY, 0);
        } else schedule(g, 1.6f, TC_KNOCK_REPLY, 1);
        break;
    case U_LOAD_DOOR:
        if (g.stage != ST_OPEN) break;
        g.ending = 2;
        g.pstate = PS_CUTSCENE;
        g.endingT = 0; g.endingLine = -1;
        audio_play_at(S_DOOR_METAL, up, 1.0f, 0.6f);
        audio_play(S_SWELL, 0.9f);
        break;
    case U_TRUCK:
        if (g.stage == ST_ESCAPE) {
            g.ending = 1;
            g.pstate = PS_CUTSCENE;
            g.endingT = 0; g.endingLine = -1;
            audio_play(S_DOOR_CLOSE, 0.9f, 0.7f);
            if (g.rig.state != RS_DORMANT) g.rig.state = RS_STATUE;
        } else if (g.stage <= ST_GATE) hint(g, "You only just got here. Dispatch would have your card.");
        else hint(g, "The engine is still ticking. You could leave. You know you are not going to.", 5.0f);
        break;
    case U_CRT:
        g.flags[F_CRT_SEEN] = true;
        if (g.events[EV_CRT_ON]) hint(g, "CRANE 7 ... NOT FOUND.   LIFT 4 ... IN PROGRESS.   OPERATOR ... KESSLER W.   LOAD ... NOT SECURED.", 7.0f);
        else hint(g, "A yard terminal. The screen is dark. Your reflection is a little slow to move.", 5.0f);
        break;
    case U_LIFT1:
        hint(g, "LIFT 1. A sedan, thirty years out of date. The seats are wet. On the dash, the odometer turns over one more tenth while you watch.", 7.0f);
        break;
    case U_LIFT2:
        audio_play_at(S_BELL, up, 0.7f, 1.0f, 6.0f);
        hint(g, "LIFT 2. The bronze hums under your palm, like something inside is breathing on it.", 6.0f);
        break;
    case U_HELMET:
        if (!g.flags[F_HELMET_SEEN]) {
            g.flags[F_HELMET_SEEN] = true;
            hint(g, "A diver's helmet. The faceplate is fogged. From the inside.", 5.0f);
            schedule(g, 6.0f, TC_HELMET);
        } else hint(g, "It was facing the stern a moment ago.", 4.0f);
        break;
    case U_VENDING:
        if (g.flags[F_VENDED]) break;
        g.flags[F_VENDED] = true;
        audio_play_at(S_VEND, up, 0.9f);
        alert_noise(g, up, 12.0f);
        schedule(g, 1.3f, TC_VEND);
        break;
    case U_SINK:
        audio_play_at(S_TAP, up, 0.8f);
        if (!g.flags[F_SINK_USED]) {
            g.flags[F_SINK_USED] = true;
            hint(g, "The tap coughs and runs black, then clear. In the mirror, the handprint is on your side of the glass.", 6.5f);
            g.fear = std::min(1.0f, g.fear + 0.25f);
        } else hint(g, "Cold. It smells of the harbor.");
        break;
    default: break;
    }
}

void game_interact(Game& g, int idx) {
    World& w = g.w;
    if (idx < 0) return;
    Interact& it = w.interacts[(size_t)idx];
    switch (it.kind) {
    case IK_PICKUP: take_item(g, it.arg); check_progress(g); break;
    case IK_NOTE:
        open_note(g, it.arg);
        if (it.arg == N_DUTY_LOG) g.flags[F_LOG_READ] = true;
        if (it.arg == N_WALT_FINAL) g.flags[F_WALT_READ] = true;
        break;
    case IK_DOOR: toggle_door(g, it.arg); break;
    case IK_HIDE:
        if (g.pstate == PS_HIDING) exit_hide(g); else enter_hide(g, it.arg);
        break;
    case IK_LADDER: start_ladder(g, it.arg); break;
    case IK_USE: use_object(g, it.arg); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Player movement
// ---------------------------------------------------------------------------
static bool body_blocked(const Game& g, const Vec3& p, float height) {
    static std::vector<int> tmp;
    AABB body(Vec3(p.x - BODY_HALF, p.y + STEP_H, p.z - BODY_HALF), Vec3(p.x + BODY_HALF, p.y + height, p.z + BODY_HALF));
    g.w.query(body, tmp);
    for (int ci : tmp)
        if (g.w.colliders[(size_t)ci].flags & COL_SOLID) return true;
    return false;
}

static float ground_under(const Game& g, const Vec3& p, int* surf) {
    static const float off[5][2] = {{0, 0}, {0.2f, 0.2f}, {-0.2f, 0.2f}, {0.2f, -0.2f}, {-0.2f, -0.2f}};
    float best = -1000.0f;
    for (int i = 0; i < 5; i++) {
        int s = SURF_CONCRETE;
        float h = g.w.floorBelow(Vec3(p.x + off[i][0], p.y + STEP_H + 0.01f, p.z + off[i][1]), 60.0f, &s);
        if (h > best) { best = h; if (surf) *surf = s; }
    }
    return best;
}

static void footstep(Game& g, float loud) {
    int surf = g.surface;
    if (g.zone == Z_PUMP && g.waterLevel > g.pos.y + 0.03f) surf = SURF_WATER;
    audio_play(S_STEP_ASPHALT + surf, 0.35f + 0.4f * loud, 0.92f + g.rng.f() * 0.16f, (g.rng.f() - 0.5f) * 0.3f);
    float radius = g.crouched ? 3.0f : (loud > 0.8f ? 21.0f : 9.5f);
    if (surf == SURF_METAL || surf == SURF_GRATE || surf == SURF_WATER) radius *= 1.3f;
    g.noise = std::max(g.noise, radius);
    alert_noise(g, g.pos, radius);
}

static void player_move(Game& g, const Controls& c, float dt) {
    float height = lerpf(BODY_STAND, BODY_CROUCH, g.crouchAmt);
    // crouch toggle (cannot stand up under something low)
    if (c.crouch) {
        if (g.crouched) {
            if (!body_blocked(g, g.pos, BODY_STAND)) g.crouched = false;
        } else g.crouched = true;
    }
    g.crouchAmt = approachf(g.crouchAmt, g.crouched ? 1.0f : 0.0f, dt * 5.0f);

    float mx = clampv(c.moveX, -1.0f, 1.0f), my = clampv(c.moveY, -1.0f, 1.0f);
    float ml = std::sqrt(mx * mx + my * my);
    if (ml > 1.0f) { mx /= ml; my /= ml; ml = 1.0f; }
    bool wantSprint = c.sprint && my > 0.3f && !g.crouched && !g.exhausted && ml > 0.5f;
    bool adrenaline = g.flags[F_FINALE];
    if (wantSprint) {
        if (!adrenaline) g.stamina -= dt / (g.difficulty == 2 ? 5.5f : 7.5f);
        g.staminaDelay = 1.3f;
        if (g.stamina <= 0) { g.stamina = 0; g.exhausted = true; audio_play(S_BREATH, 0.8f); }
    } else {
        g.staminaDelay -= dt;
        if (g.staminaDelay <= 0) g.stamina = std::min(1.0f, g.stamina + dt * 0.24f);
        if (g.exhausted && g.stamina > 0.35f) g.exhausted = false;
    }
    float speed = g.crouched ? SPD_CROUCH : (wantSprint ? SPD_SPRINT : SPD_WALK);
    if (g.exhausted) speed *= 0.8f;
    bool wading = g.zone == Z_PUMP && g.waterLevel > g.pos.y + 0.05f;
    if (wading) speed *= 0.62f;
    Vec3 fwd(std::sin(g.yaw), 0, -std::cos(g.yaw)), right(std::cos(g.yaw), 0, std::sin(g.yaw));
    Vec3 wish = (fwd * my + right * mx) * speed;
    float accel = g.onGround ? 11.0f : 2.0f;
    g.vel.x = dampf(g.vel.x, wish.x, accel, dt);
    g.vel.z = dampf(g.vel.z, wish.z, accel, dt);

    // horizontal, one axis at a time so the player slides along walls
    Vec3 p = g.pos;
    Vec3 tryp = p;
    tryp.x += g.vel.x * dt;
    if (!body_blocked(g, tryp, height)) p.x = tryp.x; else g.vel.x = 0;
    tryp = p;
    tryp.z += g.vel.z * dt;
    if (!body_blocked(g, tryp, height)) p.z = tryp.z; else g.vel.z = 0;

    // vertical
    int surf = g.surface;
    float ground = ground_under(g, p, &surf);
    if (g.onGround) {
        if (ground >= p.y - STEP_H - 0.02f) {
            // step up / down smoothly
            if (ground > p.y + 0.001f) g.landKick = std::min(g.landKick + (ground - p.y) * 0.5f, 0.25f);
            p.y = ground;
            g.vel.y = 0;
            g.surface = surf;
        } else {
            g.onGround = false;
            g.fallStart = p.y;
        }
    }
    if (!g.onGround) {
        g.vel.y -= 19.0f * dt;
        p.y += g.vel.y * dt;
        if (p.y <= ground) {
            p.y = ground;
            float fell = g.fallStart - p.y;
            g.onGround = true;
            g.vel.y = 0;
            g.surface = surf;
            g.landKick = clampv(fell * 0.12f, 0.03f, 0.3f);
            audio_play(S_STEP_ASPHALT + surf, 0.9f, 0.8f);
            alert_noise(g, p, 14.0f);
            if (fell > 7.0f) kill_player(g, 1);
        }
    }
    float moved = distXZ(p, g.pos);
    g.pos = p;

    // footsteps + head bob
    float hs = dt > 0 ? moved / dt : 0;
    if (g.onGround && hs > 0.4f) {
        g.stepDist += moved;
        float stride = g.crouched ? 1.25f : (wantSprint ? 2.25f : 1.75f);
        if (g.stepDist > stride) { g.stepDist = 0; footstep(g, saturate(hs / SPD_SPRINT)); }
        g.bobPhase += moved * (g.crouched ? 3.4f : 2.5f);
        g.bobAmt = dampf(g.bobAmt, saturate(hs / SPD_WALK), 8.0f, dt);
    } else {
        g.bobAmt = dampf(g.bobAmt, 0.0f, 6.0f, dt);
        if (hs < 0.2f) g.stepDist = std::min(g.stepDist, 0.8f);
    }
    g.landKick = dampf(g.landKick, 0.0f, 7.0f, dt);
}

static void player_look(Game& g, const Controls& c) {
    g.yaw = wrapAngle(g.yaw + c.lookDX);
    g.pitch = clampv(g.pitch + c.lookDY, -1.45f, 1.45f);
}

static void ladder_update(Game& g, const Controls& c, float dt) {
    const Ladder& l = g.w.ladders[g.ladder];
    player_look(g, c);
    bool up = g.ladderDir == 0;
    Vec3 railBottom(l.rail.x, l.y0, l.rail.z), railTop(l.rail.x, l.y1, l.rail.z);
    Vec3 a = up ? railBottom : railTop, b = up ? railTop : railBottom, exitP = up ? l.top : l.bottom;
    if (g.ladderPhase == 0) {
        g.ladderT += dt / 0.45f;
        g.pos = lerp(g.ladderFrom, a, smooth01(g.ladderT));
        if (g.ladderT >= 1) { g.ladderPhase = 1; g.ladderT = 0; g.stepDist = 0; }
    } else if (g.ladderPhase == 1) {
        float len = std::fabs(l.y1 - l.y0);
        float spd = 1.55f;
        g.ladderT += dt * spd / std::max(len, 0.1f);
        g.pos = lerp(a, b, saturate(g.ladderT));
        g.stepDist += dt * spd;
        g.bobPhase += dt * 6.0f;
        if (g.stepDist > 0.42f) {
            g.stepDist = 0;
            audio_play(S_LADDER, 0.5f, 0.9f + g.rng.f() * 0.2f);
            g.noise = std::max(g.noise, 8.0f);
        }
        if (g.ladderT >= 1) { g.ladderPhase = 2; g.ladderT = 0; g.ladderFrom = g.pos; }
    } else {
        g.ladderT += dt / 0.5f;
        g.pos = lerp(g.ladderFrom, exitP, smooth01(g.ladderT));
        if (g.ladderT >= 1) {
            g.pos = exitP;
            g.pstate = PS_NORMAL;
            g.onGround = true;
            g.vel = Vec3(0, 0, 0);
            g.ladder = -1;
            audio_play(S_STEP_METAL, 0.6f);
        }
    }
}

// ---------------------------------------------------------------------------
// World objects driven by the player state
// ---------------------------------------------------------------------------
static void doors_update(Game& g, float dt) {
    World& w = g.w;
    for (int i = 0; i < D_COUNT; i++) {
        Door& d = w.doors[i];
        if (d.dyn < 0) continue;
        if (d.angle != d.target) {
            float sp = (i == D_LIFT3 ? 0.25f : 2.6f) * dt;
            d.angle = approachf(d.angle, d.target, sp);
            w.dyn[(size_t)d.dyn].xf = w.doorMatrix(d);
        }
        bool open = std::fabs(d.angle) > std::fabs(d.openAngle) * 0.2f;
        // never close a door collider onto the player
        if (!open && !w.colliders[(size_t)d.collider].enabled) {
            AABB pb(Vec3(g.pos.x - BODY_HALF, g.pos.y, g.pos.z - BODY_HALF), Vec3(g.pos.x + BODY_HALF, g.pos.y + 1.7f, g.pos.z + BODY_HALF));
            if (w.colliders[(size_t)d.collider].box.overlaps(pb)) continue;
        }
        w.setColliderEnabled(d.collider, !open);
    }
    // gate
    float gt = g.flags[F_GATE_OPEN] ? 1.0f : 0.0f;
    if (g.gateT != gt) {
        g.gateT = approachf(g.gateT, gt, dt / 6.5f);
        if (g.gateT > 0.25f) w.setColliderEnabled(w.gateCollider, false);
    }
    float e = smooth01(g.gateT);
    w.dyn[DY_GATE].xf = Mat4::translate(Vec3(-4.85f - 9.3f * e, 0, 56.0f));
    // hide-locker doors
    for (int i = 0; i < HIDE_COUNT; i++) {
        HideSpot& h = w.hides[i];
        if (h.door < 0) continue;
        float target = 0.0f;
        if (g.pstate == PS_HIDING && g.hideIdx == i) target = g.hideT < 0.35f ? -1.2f : -0.07f;
        h.doorAngle = approachf(h.doorAngle, target, dt * 5.0f);
        w.dyn[(size_t)h.door].xf = Mat4::translate(h.hinge) * Mat4::rotY(h.doorYaw + h.doorAngle);
    }
}

static void props_update(Game& g, float dt) {
    World& w = g.w;
    // lever throws (0 = idle; runs 0..1 once triggered)
    for (int i = 0; i < 8; i++)
        if (g.leverAnim[i] > 0 && g.leverAnim[i] < 1) g.leverAnim[i] = std::min(1.0f, g.leverAnim[i] + dt * 2.5f);
    float gl = g.flags[F_GATE_OPEN] ? smooth01(g.leverAnim[0] > 0 ? g.leverAnim[0] : 1.0f) : 0.0f;
    w.dyn[DY_GATE_LEVER].xf = Mat4::translate(Vec3(6.4f, 1.2f, 57.22f)) * Mat4::rotX(0.5f + gl * 1.4f);
    float sl = g.flags[F_GEN_RUNNING] ? 1.0f : (g.leverAnim[1] > 0 && g.leverAnim[1] < 1 ? std::sin(g.leverAnim[1] * PI) : 0.0f);
    w.dyn[DY_GEN_LEVER].xf = Mat4::translate(Vec3(36.74f, 0.95f, 24.0f)) * Mat4::rotZ(0.5f - sl * 1.3f);
    float br = g.flags[F_PUMP_ON] ? smooth01(g.leverAnim[2] > 0 ? g.leverAnim[2] : 1.0f) : 0.0f;
    w.dyn[DY_BREAKER].xf = Mat4::translate(Vec3(51.58f, 1.25f, -33.7f)) * Mat4::rotZ(-br * 1.5f);
    float st = g.flags[F_STORM_LOCKED] ? smooth01(g.leverAnim[3] > 0 ? g.leverAnim[3] : 1.0f) : 0.0f;
    {
        float x = CRANE_POS.x + 4.5f + 0.4f + 0.17f, z = CRANE_POS.z + 4.0f + 0.14f;
        w.dyn[DY_STORM_LEVER].xf = Mat4::translate(Vec3(x, 1.2f, z)) * Mat4::rotZ(0.9f - st * 1.8f);
    }
    // valve wheels
    static const float vx[3] = {48.0f, 46.0f, 50.0f};
    for (int i = 0; i < 3; i++) {
        g.valveSpin[i] = dampf(g.valveSpin[i], 0.0f, 2.0f, dt);
        float ang = (float)g.time * 0.0f + (3.0f - g.valveSpin[i]) + i * 0.7f + (g.flags[F_VALVES_DONE] ? 2.0f : 0.0f);
        w.dyn[DY_VALVE_A + i].xf = Mat4::translate(Vec3(vx[i], 1.65f, -40.0f + 0.55f + 0.34f)) * Mat4::rotZ(ang);
    }
    // pump house flood water
    float wl = g.flags[F_VALVES_DONE] ? -0.3f : 0.34f;
    g.waterLevel = approachf(g.waterLevel, wl, dt * 0.05f);
    w.dyn[DY_PUMPWATER].xf = Mat4::translate(Vec3(0, g.waterLevel + 0.012f * std::sin((float)g.time * 1.3f), 0));
    w.dyn[DY_PUMPWATER].visible = g.waterLevel > -0.25f;
    // phone handset rattles while ringing
    {
        float j = g.phoneRingT >= 0 && std::fmod(g.phoneRingT, 3.0f) < 1.4f ? std::sin((float)g.time * 70.0f) * 0.004f : 0.0f;
        bool lifted = g.flags[F_PHONE_ANSWERED];
        w.dyn[DY_PHONE].xf = Mat4::translate(Vec3(15.825f + (lifted ? -0.18f : 0.0f), 1.36f + 0.105f + j - (lifted ? 0.1f : 0.0f), 31.3f - (lifted ? 0.15f : 0.0f))) *
                             Mat4::rotY(lifted ? 0.9f : 0.0f);
    }
    // swinging lamp + chains in the workshop (the wind finds its way in)
    float t = (float)g.time;
    float sw = 0.16f * std::sin(t * 1.35f) + 0.05f * std::sin(t * 2.9f + 1.0f);
    float sw2 = 0.09f * std::sin(t * 1.1f + 2.0f);
    if (g.rig.state != RS_DORMANT && distXZ(g.rig.pos, Vec3(33.5f, 0, -5.5f)) < 9.0f) sw *= 2.2f;
    w.dyn[DY_SWING_LAMP].xf = Mat4::translate(Vec3(33.5f, 5.0f, -5.5f)) * Mat4::rotZ(sw) * Mat4::rotX(sw2);
    {
        Light& l = w.lights[(size_t)w.lightSwing];
        l.pos = w.dyn[DY_SWING_LAMP].xf.point(Vec3(0, -1.95f, 0));
        l.dir = normalize(w.dyn[DY_SWING_LAMP].xf.dir(Vec3(0, -1, 0)));
    }
    w.dyn[DY_CHAIN1].xf = Mat4::translate(Vec3(30.0f, 2.93f, -4.0f)) * Mat4::rotZ(0.05f * std::sin(t * 1.7f)) * Mat4::rotX(0.04f * std::sin(t * 1.3f + 1));
    w.dyn[DY_CHAIN2].xf = Mat4::translate(Vec3(36.0f, 4.3f, 0.5f)) * Mat4::rotZ(0.07f * std::sin(t * 1.2f + 2)) * Mat4::rotX(0.05f * std::sin(t * 1.5f));
    w.dyn[DY_CHAIN3].xf = Mat4::translate(Vec3(31.2f, 4.3f, 4.0f)) * Mat4::rotZ(0.06f * std::sin(t * 1.4f + 4)) * Mat4::rotX(0.06f * std::sin(t * 1.0f + 3));
    // helmet
    g.helmetYaw = approachf(g.helmetYaw, g.helmetTarget, dt * 0.25f);
    w.dyn[DY_HELMET].xf = Mat4::translate(Vec3(25.45f, -0.5f + 0.76f, -56.0f)) * Mat4::rotY(g.helmetYaw);
    // hard hat falling off the shelf
    if (g.hardhatT >= 0) {
        g.hardhatT += dt;
        float ft = g.hardhatT;
        float y = std::max(0.02f, 1.72f - 4.9f * ft * ft);
        float roll = std::min(ft, 2.6f);
        if (y <= 0.021f && ft - dt < 0.59f && ft >= 0.59f) audio_play_at(S_HATFALL, Vec3(25.3f, 0, 4.9f), 0.9f);
        w.dyn[DY_HARDHAT].visible = true;
        w.dyn[DY_HARDHAT].xf = Mat4::translate(Vec3(25.3f - 0.25f * roll, y, 5.7f - 0.5f * roll)) * Mat4::rotX(ft < 0.59f ? ft * 5.0f : PI) * Mat4::rotZ(roll * 0.6f);
        if (ft > 4.0f) g.hardhatT = -2.0f; // rest (stays where it stopped)
    }
    // the tape that rides in the cab
    if (!g.itemTaken[IT_TAPE6]) w.dyn[(size_t)w.itemDyn[IT_TAPE6]].xf = w.dyn[DY_CRANE_TOP].xf * Mat4::translate(w.cabTapeLocal);
    // key cabinet door swings out of the way when opened (it is simply hidden)
    (void)dt;
}

static void flashlight_update(Game& g, const Controls& c, float dt) {
    if (c.flashlight && g.have[IT_FLASHLIGHT] && g.pstate != PS_HIDING && g.pstate != PS_DEAD) {
        g.flashOn = !g.flashOn;
        audio_play(S_SWITCH, 0.6f, g.flashOn ? 1.0f : 0.85f);
    }
    if (g.flashOn) {
        float life = g.difficulty == 0 ? 600.0f : (g.difficulty == 1 ? 420.0f : 300.0f);
        g.battery -= dt / life;
        if (g.battery <= 0) {
            if (g.spareBatteries > 0) {
                g.spareBatteries--;
                g.battery = 1.0f;
                audio_play(S_FUSE, 0.5f, 1.3f);
                hint(g, strf("Fresh batteries in. Spare sets left: %d", g.spareBatteries).c_str());
            } else g.battery = 0;
        }
    }
    // output: dies down to an ember when flat; stutters when it is close
    float out = 1.0f;
    float t = (float)g.time;
    if (g.battery <= 0.0f) out = 0.13f + 0.05f * std::sin(t * 23.0f);
    else if (g.battery < 0.12f) out = 0.55f + 0.25f * (hash2f((int)(t * 9.0f), 3) > 0.25f ? 1.0f : 0.2f);
    if (g.rig.state != RS_DORMANT && g.rig.state != RS_STATUE) {
        float d = length(g.rig.pos - g.pos);
        if (d < 13.0f) {
            float k = 1.0f - d / 13.0f;
            float n = hash2f((int)(t * 17.0f), 11);
            if (n < k * 0.75f) out *= 0.12f;
        }
    }
    g.flashFlicker = dampf(g.flashFlicker, out, 30.0f, dt);
}

static void camera_update(Game& g, float dt) {
    World& w = g.w;
    float t = (float)g.time;
    Vec3 eye;
    float yaw = g.yaw, pitch = g.pitch, roll = 0;
    switch (g.pstate) {
    case PS_HIDING: {
        const HideSpot& h = w.hides[g.hideIdx];
        g.hideT += dt;
        Vec3 from = h.outside + Vec3(0, EYE_STAND, 0);
        eye = lerp(from, h.inside, smooth01(g.hideT / 0.45f));
        eye.y += 0.006f * std::sin(t * 1.9f);
        break;
    }
    case PS_SEATED:
    case PS_OPERATING: {
        eye = w.dyn[DY_CRANE_TOP].xf.point(w.cabSeat);
        yaw = PI * 0.5f - g.crane.slew + g.seatYaw;
        pitch = g.seatPitch;
        eye.y += 0.004f * std::sin(t * 1.3f);
        g.yaw = yaw; g.pitch = pitch;
        break;
    }
    case PS_LADDER:
        eye = g.pos + Vec3(0, EYE_STAND - 0.1f, 0);
        eye.y += 0.03f * std::sin(g.bobPhase);
        roll = 0.012f * std::sin(g.bobPhase * 0.5f);
        break;
    case PS_DEAD: {
        g.deadT += dt;
        float k = smooth01(g.deadT / 0.9f);
        eye = g.pos + Vec3(0, lerpf(EYE_STAND, 0.25f, k), 0);
        roll = 0.9f * k;
        pitch = lerpf(g.pitch, 0.5f, k);
        break;
    }
    default: {
        float eh = lerpf(EYE_STAND, EYE_CROUCH, g.crouchAmt);
        eye = g.pos + Vec3(0, eh - g.landKick, 0);
        float bob = g.bobAmt * g.bobScale;
        eye.y += std::sin(g.bobPhase * 2.0f) * 0.032f * bob;
        Vec3 right(std::cos(g.yaw), 0, std::sin(g.yaw));
        eye += right * (std::cos(g.bobPhase) * 0.02f * bob);
        roll = std::cos(g.bobPhase) * 0.006f * bob;
        // idle breathing
        eye.y += 0.005f * std::sin(t * 1.5f);
        break;
    }
    }
    // trauma shake
    g.shake = std::max(0.0f, g.shake - dt * 0.9f);
    float sh = g.shake * g.shake;
    float crane = g.crane.shake;
    if (sh > 0.0001f || crane > 0.001f) {
        float a = sh * 0.05f + crane * 0.012f;
        yaw += a * (hash2f((int)(t * 60.0f), 1) - 0.5f) * 2.0f;
        pitch += a * (hash2f((int)(t * 60.0f), 2) - 0.5f) * 2.0f;
        roll += a * (hash2f((int)(t * 60.0f), 3) - 0.5f);
    }
    // wind sway high on the crane
    if (eye.y > 9.0f && g.pstate != PS_DEAD) {
        float k = saturate((eye.y - 9.0f) / 22.0f) * (0.4f + 0.6f * g.wind);
        roll += 0.012f * k * std::sin(t * 0.9f);
        eye.x += 0.015f * k * std::sin(t * 0.7f + 1.0f);
    }
    g.camPos = eye;
    g.camYaw = yaw;
    g.camPitch = clampv(pitch, -1.5f, 1.5f);
    g.camRoll = roll;
    float fovT = 1.22f;
    if (g.pstate == PS_NORMAL && length(Vec3(g.vel.x, 0, g.vel.z)) > SPD_WALK + 0.8f) fovT = 1.32f;
    if (g.pstate == PS_OPERATING) fovT = 1.3f;
    g.camFov = dampf(g.camFov, fovT, 4.0f, dt);
    audio_listener(g.camPos, g.camYaw);
}

// ---------------------------------------------------------------------------
// Modal screens (the world is paused underneath)
// ---------------------------------------------------------------------------
static void close_note(Game& g) {
    int n = g.noteOpen;
    g.noteOpen = -1;
    g.modal = g.noteReturnJournal ? MD_JOURNAL : MD_NONE;
    g.noteReturnJournal = false;
    audio_play(S_PAPER, 0.5f, 1.2f);
    if (n == N_INCIDENT && !g.events[EV_CRT_ON]) schedule(g, 2.5f, TC_CRT);
    if (n == N_WALT_FINAL) say(g, "", "Put it back in the harbor. Or land it, and see who is knocking.", 5.0f, 0.5f);
    check_progress(g);
}

static void modal_update(Game& g, const Controls& c, float dt) {
    switch (g.modal) {
    case MD_NOTE:
        g.noteScroll = std::max(0.0f, g.noteScroll + c.scroll);
        if (c.back || c.confirm || c.interact || c.pause) close_note(g);
        break;
    case MD_JOURNAL:
        g.noteScroll = std::max(0.0f, g.noteScroll + c.scroll);
        if (c.back || c.journal || c.pause) { g.modal = MD_NONE; audio_play(S_PAPER, 0.4f, 1.2f); }
        break;
    case MD_KEYPAD:
        g.keypadMsgT = std::max(0.0f, g.keypadMsgT - dt);
        if (g.keypadMsg == 2 && g.keypadMsgT <= 0) { g.modal = MD_NONE; break; }
        if (c.back || c.pause) { g.modal = MD_NONE; break; }
        if (c.keypad >= 0 && g.keypadMsg != 2) {
            if (c.keypad <= 9) {
                if (g.keypadEntry.size() < 4) { g.keypadEntry.push_back((char)('0' + c.keypad)); audio_play(S_KEYPAD, 0.6f, 1.0f + c.keypad * 0.03f); }
            } else if (c.keypad == 10) { g.keypadEntry.clear(); audio_play(S_KEYPAD, 0.5f, 0.7f); }
            if (g.keypadEntry.size() == 4 && (c.keypad == 11 || c.keypad <= 9)) {
                if (atoi(g.keypadEntry.c_str()) == g.keyCode) {
                    g.keypadMsg = 2; g.keypadMsgT = 1.0f;
                    audio_play(S_OK, 0.8f);
                    events_timed(g, TC_KEYCAB_OPEN, 0); // the world is paused behind the keypad: open it now
                } else {
                    g.keypadMsg = 1; g.keypadMsgT = 1.2f;
                    g.keypadEntry.clear();
                    audio_play(S_ERROR, 0.8f);
                    alert_noise(g, g.pos, 9.0f);
                }
            }
        }
        break;
    case MD_PAUSE:
        if (c.pause || c.back) g.modal = MD_NONE;
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Frame update
// ---------------------------------------------------------------------------
static void subtitles_update(Game& g, float dt) {
    if (!g.subs.empty()) {
        Subtitle& s = g.subs.front();
        if (s.delay > 0) {
            s.delay -= dt;
            if (s.delay <= 0) {
                if (s.speaker == "DISPATCH") { audio_play(S_RADIO_ON, 0.5f); audio_play(S_RADIO_GARBLE, 0.55f, 0.9f + g.rng.f() * 0.25f); }
                else if (s.speaker == "TAPE") audio_play(S_RADIO_GARBLE, 0.3f, 0.7f + g.rng.f() * 0.15f);
            }
        } else {
            s.time -= dt;
            if (s.time <= 0) g.subs.erase(g.subs.begin());
        }
    }
    if (g.tapeT >= 0) {
        g.tapeT -= dt;
        if (g.tapeT < 0) {
            if (g.hTape >= 0) audio_stop(g.hTape, 0.4f);
            g.hTape = -1;
            g.tapePlaying = -1;
            audio_play(S_TAPE_CLICK, 0.7f, 0.9f);
        }
    }
    g.hintT = std::max(0.0f, g.hintT - dt);
    g.toastT = std::max(0.0f, g.toastT - dt);
    g.checkpointT = std::max(0.0f, g.checkpointT - dt);
}

void game_update(Game& g, const Controls& c, float dtIn) {
    float dt = clampv(dtIn, 0.0f, 0.05f);
    g.dt = dt;
    g.frame++;
    if (g.appMode == AM_TITLE) { game_title_update(g, dt); return; }
    if (g.appMode == AM_ENDING) { ending_update(g, c, dt); return; }

    // modal handling: the simulation stands still while reading
    if (g.modal != MD_NONE) {
        modal_update(g, c, dt);
        g.fade = approachf(g.fade, g.fadeTarget, dt * g.fadeSpeed);
        return;
    }
    if (c.pause && g.pstate != PS_DEAD && g.pstate != PS_CUTSCENE) { g.modal = MD_PAUSE; return; }
    if (c.journal && g.pstate != PS_DEAD && g.pstate != PS_CUTSCENE) { g.modal = MD_JOURNAL; g.noteScroll = 0; audio_play(S_PAPER, 0.5f); return; }

    g.time += dt;
    g.noise = std::max(0.0f, g.noise - dt * 14.0f);

    // timeline
    for (size_t i = 0; i < g.timeline.size();) {
        g.timeline[i].t -= dt;
        if (g.timeline[i].t <= 0) {
            Timed tm = g.timeline[i];
            g.timeline.erase(g.timeline.begin() + (long)i);
            events_timed(g, tm.code, tm.arg);
            if (g.appMode != AM_PLAY) return;
        } else i++;
    }

    // player
    bool busy = g.actionLock > 0;
    if (busy) g.actionLock = std::max(0.0f, g.actionLock - dt);
    Controls cc = c;
    if (busy) { cc.moveX = cc.moveY = 0; cc.interact = false; cc.tap = false; cc.crouch = false; }
    switch (g.pstate) {
    case PS_NORMAL:
        player_look(g, cc);
        player_move(g, cc, dt);
        break;
    case PS_LADDER:
        ladder_update(g, cc, dt);
        break;
    case PS_HIDING: {
        const HideSpot& h = g.w.hides[g.hideIdx];
        g.yaw = h.yaw + clampv(wrapAngle(g.yaw + cc.lookDX - h.yaw), -0.9f, 0.9f);
        g.pitch = clampv(g.pitch + cc.lookDY, -0.6f, 0.6f);
        if (cc.back || cc.moveY < -0.6f) { exit_hide(g); cc.interact = false; }
        break;
    }
    case PS_SEATED:
        g.seatYaw = clampv(g.seatYaw + cc.lookDX, -2.7f, 2.7f);
        g.seatPitch = clampv(g.seatPitch + cc.lookDY, -1.25f, 1.0f);
        if (cc.back && !busy) { use_object(g, U_CAB_LEAVE); cc.interact = false; }
        break;
    case PS_OPERATING:
        g.seatYaw = clampv(g.seatYaw + cc.lookDX, -1.6f, 1.6f);
        g.seatPitch = clampv(g.seatPitch + cc.lookDY, -1.35f, 0.9f);
        if (cc.back) { g.pstate = PS_SEATED; cc.interact = false; }
        break;
    default: break;
    }
    flashlight_update(g, cc, dt);
    crane_update(g, cc, dt);
    doors_update(g, dt);
    props_update(g, dt);
    camera_update(g, dt);

    // interaction target under the crosshair
    g.promptIdx = -1;
    g.promptText.clear();
    if (!busy && (g.pstate == PS_NORMAL || g.pstate == PS_HIDING || g.pstate == PS_SEATED) && g.fade < 0.6f) {
        Vec3 fwd = game_cam_forward(g);
        int idx = pick_interact(g, g.camPos, fwd);
        if (idx >= 0) { g.promptIdx = idx; g.promptText = interact_prompt(g, idx); }
        int act = -1;
        if (cc.interact) act = idx;
        if (cc.tap) {
            // ray through the tapped point
            float th = std::tan(g.camFov * 0.5f);
            Vec3 right = normalize(cross(fwd, Vec3(0, 1, 0)));
            Vec3 up = cross(right, fwd);
            Vec3 d = normalize(fwd + right * (cc.tapX * th * g.aspect) + up * (cc.tapY * th));
            int ti = pick_interact(g, g.camPos, d);
            if (ti >= 0) act = ti;
        }
        if (act >= 0) game_interact(g, act);
        else if (cc.interact && g.pstate == PS_HIDING) exit_hide(g);
    }

    events_update(g, dt);
    if (g.appMode != AM_PLAY) return;
    rigger_update(g, dt);
    env_update(g, dt);
    if (g.ending != 0) ending_update(g, c, dt);
    subtitles_update(g, dt);

    g.fade = approachf(g.fade, g.fadeTarget, dt * g.fadeSpeed);
    g.flash = std::max(0.0f, g.flash - dt * 1.8f);
    g.damage = std::max(0.0f, g.damage - dt * 0.5f);
    if (g.pstate == PS_DEAD) g.fadeTarget = g.deadT > 1.2f ? 1.0f : 0.0f, g.fadeSpeed = 0.9f;

    // safety net: anything that leaves the world goes back to the last checkpoint
    if (g.pos.y < -40.0f && g.pstate != PS_DEAD) kill_player(g, 1);
}
