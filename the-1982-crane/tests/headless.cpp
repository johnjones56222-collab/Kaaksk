// THE 1982 CRANE - headless playthrough test.
// A scripted bot plays the whole game through the same Controls interface the
// player uses, with the real physics, AI, audio mixer and save system running.
// Built natively with AddressSanitizer + UBSan.
//
//   headless [a|b|chaos|all] [-v]
#include "../src/game.h"
#include "../src/audio.h"
#include <chrono>

static bool g_verbose = false;
static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

struct Bot {
    Game& g;
    Controls c;
    std::vector<Vec3> path;
    size_t pi = 0;
    float repath = 0;
    Vec3 navTarget;
    Vec3 lastPos;
    float stuckT = 0;
    float goalT = 0;
    int goalKey = -1;
    std::string goalName;
    int wantEnding = 1;
    float press = 0;      // cooldown between interact presses
    int keypadIdx = 0;
    float aimJitterT = 0;
    int aimVariant = 0;
    int standIdx = -1;
    float standAge = 0;
    Vec3 standPos;
    bool failed = false;
    explicit Bot(Game& game) : g(game) {}

    void aim(const Vec3& p) {
        Vec3 d = p - g.camPos;
        float len = length(d);
        if (len < 1e-4f) return;
        float wy = std::atan2(d.x, -d.z), wp = std::asin(clampv(d.y / len, -1.0f, 1.0f));
        c.lookDX = wrapAngle(wy - g.camYaw);
        c.lookDY = wp - g.camPitch;
    }
    void walkTo(const Vec3& p, bool sprint = false) {
        Vec3 d(p.x - g.pos.x, 0, p.z - g.pos.z);
        if (length(d) < 0.05f) return;
        float wy = std::atan2(d.x, -d.z);
        c.lookDX = wrapAngle(wy - g.yaw);
        c.lookDY = -g.pitch * 0.2f;
        c.moveY = 1.0f;
        c.sprint = sprint;
    }
    // open any closed door we are about to walk into
    bool handleDoors() {
        World& w = g.w;
        for (int i = 0; i < D_COUNT; i++) {
            Door& d = w.doors[i];
            if (d.dyn < 0 || i == D_LIFT3) continue;
            if (std::fabs(d.target) > 0.01f) continue;
            Vec3 dp = w.doorPos[i];
            if (std::fabs(dp.y - (g.pos.y + 1.0f)) > 1.5f) continue;
            float dist = distXZ(dp, g.pos);
            if (dist > 1.7f) continue;
            // only doors that lie on our way
            Vec3 heading = pi < path.size() ? normalize(Vec3(path[pi].x - g.pos.x, 0, path[pi].z - g.pos.z)) : Vec3(std::sin(g.yaw), 0, -std::cos(g.yaw));
            Vec3 toDoor = normalize(Vec3(dp.x - g.pos.x, 0, dp.z - g.pos.z));
            if (dist > 0.5f && dot(heading, toDoor) < 0.25f) continue;
            if (d.lockedBy >= 0 && !g.have[d.lockedBy]) continue;
            aim(dp);
            c.moveY = 0;
            if (g.promptIdx >= 0 && g.w.interacts[(size_t)g.promptIdx].kind == IK_DOOR && g.w.interacts[(size_t)g.promptIdx].arg == i && press <= 0) {
                c.interact = true;
                press = 0.5f;
            }
            return true;
        }
        return false;
    }
    bool navTo(const Vec3& target, bool sprint = false, float tol = 0.45f) {
        if (distXZ(g.pos, target) < tol) return true;
        if (handleDoors()) return false;
        if (path.empty() || repath <= 0 || distXZ(navTarget, target) > 0.5f) {
            if (!nav_path(g.w, g.pos, target, path, false)) { path.clear(); path.push_back(target); }
            pi = 0;
            repath = 4.0f;
            navTarget = target;
        }
        while (pi < path.size() && distXZ(g.pos, path[pi]) < 0.1f) pi++;
        if (pi >= path.size()) { walkTo(target, sprint); return distXZ(g.pos, target) < tol; }
        walkTo(path[pi], sprint);
        return false;
    }
    Vec3 interactCenter(int idx) const {
        const Interact& it = g.w.interacts[(size_t)idx];
        Vec3 c0 = it.box.center();
        if (it.xfDyn >= 0) c0 = g.w.dyn[(size_t)it.xfDyn].xf.point(c0);
        return c0;
    }
    // best ground position from which the target can be seen and reached
    Vec3 approachPoint(const Vec3& center, float range) {
        const NavGrid& n = g.w.nav;
        int cx = n.cx(center.x), cz = n.cz(center.z);
        float best = 1e9f;
        Vec3 out = nav_snap(g.w, center, 8);
        for (int dz = -6; dz <= 6; dz++)
            for (int dx = -6; dx <= 6; dx++) {
                int x = cx + dx, z = cz + dz;
                if (!n.walkable(x, z)) continue;
                Vec3 p = n.center(x, z);
                Vec3 eye = p + Vec3(0, 1.6f, 0);
                float d = length(center - eye);
                if (d > range - 0.35f) continue;
                Vec3 dir = normalize(center - eye);
                float block = g.w.raycast(eye, dir, d, COL_SIGHT);
                if (block < d - 0.3f) continue;
                float score = d + (distXZ(p, center) < 0.45f ? 2.0f : 0.0f);
                if (score < best) { best = score; out = p; }
            }
        return out;
    }
    // returns true once the interaction has been triggered
    // `seen`: where the thing actually appears on screen. The bot goes to and aims at what a
    // player can see, so an interact volume that is not where its object is drawn fails the test.
    bool interactIdx(int idx, const Vec3* standOverride = nullptr, const Vec3* seen = nullptr) {
        if (idx < 0) return false;
        const Interact& it = g.w.interacts[(size_t)idx];
        Vec3 center = seen ? *seen : interactCenter(idx);
        bool seated = g.pstate == PS_SEATED;
        if (!seated) {
            // the stand point is chosen once per target so the bot does not dither between candidates
            if (standIdx != idx || standAge > 6.0f) {
                standIdx = idx;
                standAge = 0;
                standPos = standOverride ? *standOverride : approachPoint(center, it.range);
            }
            standAge += g.dt;
            if (standOverride) standPos = *standOverride;
            bool onNav = !standOverride;
            float d = distXZ(g.pos, standPos);
            if (d > 0.3f) {
                if (onNav) navTo(standPos, false, 0.25f); else walkTo(standPos);
                return false;
            }
        }
        // try the centre first, then other points of the box
        Vec3 aimP = center;
        if (aimVariant > 0 && seen) {
            static const float o[6][3] = {{0, 0.05f, 0}, {0, -0.03f, 0}, {0.06f, 0, 0}, {-0.06f, 0, 0}, {0, 0, 0.06f}, {0, 0, -0.06f}};
            const float* q = o[(aimVariant - 1) % 6];
            aimP = center + Vec3(q[0], q[1], q[2]);
        } else if (aimVariant > 0) {
            Vec3 h = it.box.half() * 0.6f;
            static const float o[6][3] = {{0, 0.6f, 0}, {0, -0.6f, 0}, {0.7f, 0, 0}, {-0.7f, 0, 0}, {0, 0, 0.7f}, {0, 0, -0.7f}};
            const float* q = o[(aimVariant - 1) % 6];
            Vec3 lp = it.box.center() + Vec3(h.x * q[0], h.y * q[1], h.z * q[2]);
            aimP = it.xfDyn >= 0 ? g.w.dyn[(size_t)it.xfDyn].xf.point(lp) : lp;
        }
        aim(aimP);
        aimJitterT += g.dt;
        if (aimJitterT > 0.6f) { aimJitterT = 0; aimVariant++; }
        if (g.promptIdx == idx && press <= 0) {
            c.interact = true;
            press = 0.45f;
            aimVariant = 0;
            return true;
        }
        return false;
    }
    Vec3 itemSeen(int item) const {
        const DynObj& d = g.w.dyn[(size_t)g.w.itemDyn[item]];
        return d.xf.point(g.w.dynMeshes[(size_t)d.mesh].box.center());
    }
    bool pickup(int item) {
        Vec3 v = itemSeen(item);
        return interactIdx(g.w.itemInteract[item], nullptr, &v);
    }
    bool readNote(int note) {
        if (g.w.interacts[(size_t)g.w.noteInteract[note]].xfDyn >= 0) return interactIdx(g.w.noteInteract[note]);
        Vec3 v = g.w.notePos[note];
        return interactIdx(g.w.noteInteract[note], nullptr, &v);
    }
    bool useObj(int u) { return interactIdx(g.w.useInteract[u]); }
    int ladderInteract(int lad, int dir) const {
        for (size_t i = 0; i < g.w.interacts.size(); i++)
            if (g.w.interacts[i].kind == IK_LADDER && g.w.interacts[i].arg == lad * 2 + dir) return (int)i;
        return -1;
    }

    // ---- barge excursion (not on the nav grid) ----
    bool onBarge() const { return g.pos.z < QUAY_Z - 0.3f; }
    bool bargeFetch(int item) {
        static const Vec3 head(27.0f, 0, QUAY_Z + 1.4f), foot(27.0f, -0.5f, -51.4f), doorOut(31.0f, -0.5f, -55.6f), inside(33.4f, -0.5f, -55.6f);
        Vec3 ip = g.w.itemPos[item];
        Vec3 stand(ip.x - 0.2f, -0.5f, ip.z - 0.75f);
        if (item == IT_TAPE3) stand = Vec3(ip.x - 0.9f, -0.5f, ip.z);
        if (!onBarge()) {
            // once lined up with the gangway, commit to walking down it
            if (g.pos.z < QUAY_Z + 2.0f && std::fabs(g.pos.x - 27.0f) < 0.7f) walkTo(foot);
            else navTo(head, false, 0.3f);
            return false;
        }
        if (g.pos.x < 31.6f) {
            // on deck: through the cabin door
            if (handleDoors()) return false;
            if (distXZ(g.pos, doorOut) > 0.35f && g.pos.x < 30.7f) walkTo(doorOut); else walkTo(inside);
            return false;
        }
        Vec3 v = itemSeen(item);
        return interactIdx(g.w.itemInteract[item], &stand, &v);
    }
    bool bargeLeave() {
        static const Vec3 head(27.0f, 0, QUAY_Z + 1.6f), foot(27.0f, -0.5f, -51.4f), doorOut(30.9f, -0.5f, -55.6f), inside(33.4f, -0.5f, -55.6f);
        if (!onBarge()) return true;
        if (g.pos.x > 31.4f) {
            if (handleDoors()) return false;
            if (distXZ(g.pos, inside) > 0.4f && std::fabs(g.pos.z + 55.6f) > 0.3f) walkTo(inside); else walkTo(doorOut);
            return false;
        }
        if (distXZ(g.pos, foot) > 0.5f && g.pos.z < -51.0f) walkTo(foot); else walkTo(head);
        return false;
    }

    // ---- crane ----
    void operate() {
        const CraneSim& cr = g.crane;
        float ts = wantEnding == 1 ? PI * 0.5f : PAD_SLEW, tt = wantEnding == 1 ? 26.0f : PAD_R;
        float es = wrapAngle(ts - cr.slew), et = tt - cr.trolley;
        c.craneSlew = clampv(es * 5.0f, -1.0f, 1.0f);
        c.craneTrolley = clampv(et * 1.5f, -1.0f, 1.0f);
        bool placed = std::fabs(es) < 0.03f && std::fabs(et) < 0.4f;
        if (placed) c.craneHoist = -1.0f;
        if ((cr.ready == 2 && wantEnding == 1) || (cr.ready == 4 && wantEnding == 2)) {
            c.craneHoist = 0;
            if (press <= 0) { c.craneRelease = true; press = 1.0f; }
        }
    }
    bool climbTo(float targetY) {
        // returns true when standing at (or above) targetY
        World& w = g.w;
        if (g.pstate == PS_LADDER) return false;
        static const float levels[6] = {0.0f, 7.2f, 13.0f, 19.0f, 25.0f, CRANE_TOP_Y};
        int lvl = 0;
        for (int i = 0; i < 6; i++)
            if (std::fabs(g.pos.y - levels[i]) < 0.6f) lvl = i;
        if (std::fabs(g.pos.y - targetY) < 0.6f) return true;
        bool up = targetY > g.pos.y;
        int lad = up ? lvl : lvl - 1;
        if (lad < 0 || lad >= LAD_COUNT) return true;
        const Ladder& l = w.ladders[lad];
        if (lad == LAD_PORTAL && up) {
            // through the cage gate first
            Door& d = w.doors[D_CRANE_GATE];
            Vec3 front(l.bottom.x, 0, l.bottom.z + 1.5f);
            if (std::fabs(d.target) < 0.01f) {
                if (distXZ(g.pos, front) > 0.4f) { navTo(front, false, 0.3f); return false; }
                aim(w.doorPos[D_CRANE_GATE]);
                if (g.promptIdx >= 0 && w.interacts[(size_t)g.promptIdx].kind == IK_DOOR && press <= 0) { c.interact = true; press = 0.6f; }
                return false;
            }
            if (std::fabs(d.angle) < std::fabs(d.openAngle) * 0.8f) return false;
            // line up with the gateway before walking into the cage
            bool lined = std::fabs(g.pos.x - front.x) < 0.3f && g.pos.z < front.z + 0.4f;
            if (!lined) { navTo(front, false, 0.2f); return false; }
            Vec3 st = l.bottom;
            interactIdx(ladderInteract(lad, 0), &st);
            return false;
        }
        Vec3 st = up ? l.bottom : l.top;
        interactIdx(ladderInteract(lad, up ? 0 : 1), &st);
        return false;
    }

    void setGoal(int key, const char* name) {
        if (key != goalKey) {
            goalKey = key;
            goalName = name;
            goalT = 0;
            path.clear();
            aimVariant = 0;
            standIdx = -1;
            if (g_verbose) printf("    [%7.1fs] goal: %s  (pos %.1f %.1f %.1f)\n", g.time, name, g.pos.x, g.pos.y, g.pos.z);
        }
    }

    // diagnostics for a stuck bot: what is around the player?
    void dump() {
        World& w = g.w;
        printf("  --- bot dump: pos (%.2f %.2f %.2f) yaw %.2f pitch %.2f pstate %d fade %.2f onGround %d\n", g.pos.x, g.pos.y, g.pos.z, g.yaw, g.pitch, g.pstate, g.fade, g.onGround);
        if (pi < path.size()) printf("      next waypoint (%.2f %.2f %.2f), %zu left\n", path[pi].x, path[pi].y, path[pi].z, path.size() - pi);
        std::vector<int> near;
        AABB bx(g.pos - Vec3(0.45f, 0.1f, 0.45f), g.pos + Vec3(0.45f, 1.8f, 0.45f));
        w.query(bx, near);
        for (int ci : near) {
            const Collider& c0 = w.colliders[(size_t)ci];
            printf("      collider %d flags %d (%.2f %.2f %.2f)-(%.2f %.2f %.2f)\n", ci, c0.flags, c0.box.mn.x, c0.box.mn.y, c0.box.mn.z, c0.box.mx.x, c0.box.mx.y, c0.box.mx.z);
        }
        for (int i = 0; i < D_COUNT; i++)
            if (w.doors[i].dyn >= 0 && distXZ(w.doorPos[i], g.pos) < 3.0f)
                printf("      door %d angle %.2f target %.2f lockedBy %d collider enabled %d\n", i, w.doors[i].angle, w.doors[i].target, w.doors[i].lockedBy,
                       (int)w.colliders[(size_t)w.doors[i].collider].enabled);
    }

    void think() {
        c = Controls();
        press -= g.dt;
        repath -= g.dt;
        goalT += g.dt;
        World& w = g.w;
        if (g.appMode != AM_PLAY) { c.confirm = true; return; }
        if (g.modal == MD_NOTE) { if (press <= 0) { c.confirm = true; press = 0.3f; } return; }
        if (g.modal == MD_KEYPAD) {
            setGoal(900, "type the cabinet code");
            if (press <= 0) {
                std::string code = strf("%04d", g.keyCode);
                if (g.keypadMsg == 0 || g.keypadMsg == 1) {
                    size_t n = g.keypadEntry.size();
                    if (n < 4) c.keypad = code[n] - '0';
                }
                press = 0.25f;
            }
            return;
        }
        if (g.pstate == PS_DEAD || g.pstate == PS_LADDER || g.pstate == PS_CUTSCENE) return;
        if (g.actionLock > 0) return;
        if (g.pstate == PS_HIDING) { c.back = true; return; }
        if (!g.flashOn && g.have[IT_FLASHLIGHT] && press <= 0 && g.pstate == PS_NORMAL) { c.flashlight = true; press = 0.2f; }

        switch (g.stage) {
        case ST_FLASHLIGHT: setGoal(1, "take the flashlight"); pickup(IT_FLASHLIGHT); break;
        case ST_BOOTH:
            if (!g.flags[F_LOG_READ]) { setGoal(2, "read the gate log"); readNote(N_DUTY_LOG); }
            else { setGoal(3, "take the radio"); pickup(IT_RADIO); }
            break;
        case ST_GATE: setGoal(4, "pull the gate release"); useObj(U_GATE_LEVER); break;
        case ST_OFFICE_GO: setGoal(5, "read the note by the office door"); readNote(N_OFFICE_DOOR); break;
        case ST_OFFICE_KEY: setGoal(6, "office key from Dale's locker"); pickup(IT_OFFICE_KEY); break;
        case ST_OFFICE_ENTER: setGoal(7, "read the incident report"); readNote(N_INCIDENT); break;
        case ST_POWER: {
            int held = (g.have[IT_FUSE_A] ? 1 : 0) + (g.have[IT_FUSE_B] ? 1 : 0) + (g.have[IT_FUSE_C] ? 1 : 0);
            if (onBarge() && g.itemTaken[IT_FUSE_C]) { setGoal(19, "leave the barge"); bargeLeave(); break; }
            if (g.fusesInstalled + held < 3) {
                if (!g.itemTaken[IT_FUSE_A]) { setGoal(10, "fuse A (workshop shelf)"); pickup(IT_FUSE_A); }
                else if (!g.itemTaken[IT_FUSE_B]) { setGoal(11, "fuse B (stores container)"); pickup(IT_FUSE_B); }
                else { setGoal(12, "fuse C (barge)"); bargeFetch(IT_FUSE_C); }
            } else if (g.fusesInstalled < 3) { setGoal(13, "seat the fuses"); useObj(U_FUSEBOX); }
            else if (!g.have[IT_JERRYCAN] && !g.flags[F_GEN_FUELED]) { setGoal(14, "jerry can"); pickup(IT_JERRYCAN); }
            else if (!g.flags[F_CAN_FULL] && !g.flags[F_GEN_FUELED]) { setGoal(15, "fill the can"); useObj(U_FUEL_TAP); }
            else if (!g.flags[F_GEN_FUELED]) { setGoal(16, "fuel the generator"); useObj(U_GEN_FUEL); }
            else { setGoal(17, "start the generator"); useObj(U_GEN_START); }
            break;
        }
        case ST_KEYCAB:
            if (!g.noteRead[N_TIMECARD]) { setGoal(20, "Walt's time card"); readNote(N_TIMECARD); }
            else if (!g.flags[F_KEYCAB_OPEN]) { setGoal(21, "open the key cabinet"); useObj(U_KEYPAD); }
            else { setGoal(22, "take the crane key"); pickup(IT_CRANE_KEY); }
            break;
        case ST_BRAKE:
            if (!g.have[IT_CUTTERS]) { setGoal(30, "bolt cutters"); pickup(IT_CUTTERS); }
            else if (!g.flags[F_VALVES_DONE]) {
                static const int order[3] = {U_VALVE_C, U_VALVE_A, U_VALVE_B};
                setGoal(31 + g.valveStep, "purge valves");
                useObj(order[g.valveStep]);
            } else if (!g.flags[F_LOTO_CUT]) { setGoal(35, "cut the lockout"); useObj(U_LOTO); }
            else if (!g.flags[F_DRAINED]) { setGoal(36, "wait for the water"); }
            else if (!g.flags[F_PUMP_ON]) { setGoal(37, "start the brake pump"); useObj(U_BREAKER); }
            else { setGoal(38, "pull the slew lock"); useObj(U_STORMLOCK); }
            break;
        case ST_CLIMB:
            setGoal(40, "climb the crane");
            if (climbTo(CRANE_TOP_Y)) {
                Vec3 st(CRANE_POS.x + 1.3f, CRANE_TOP_Y, CRANE_POS.z);
                interactIdx(w.useInteract[U_CAB_DOOR], &st);
            }
            break;
        case ST_CAB:
            if (!g.flags[F_WALT_READ]) { setGoal(50, "read Walt's log"); interactIdx(w.cabNoteInteract); }
            else { setGoal(51, "take the controls"); interactIdx(w.cabControlsInteract); }
            break;
        case ST_LOAD:
            if (g.pstate == PS_SEATED && !g.crane.loadReleased) { setGoal(52, "take the controls (again)"); interactIdx(w.cabControlsInteract); }
            else { setGoal(53, "operate the crane"); operate(); }
            break;
        case ST_ESCAPE:
            if (g.pstate == PS_SEATED) { setGoal(60, "leave the cab"); interactIdx(w.cabLeaveInteract); }
            else if (g.pos.y > 0.6f) { setGoal(61, "climb down"); climbTo(0.0f); }
            else if (g.pos.z < 57.5f && !g.events[EV_GATE_SLAM]) { setGoal(62, "run for the gate"); navTo(Vec3(0.5f, 0, 59.0f), true, 0.6f); }
            else { setGoal(63, "the truck"); c.sprint = true; useObj(U_TRUCK); }
            break;
        case ST_OPEN:
            if (g.pstate == PS_SEATED) { setGoal(70, "leave the cab"); interactIdx(w.cabLeaveInteract); }
            else if (g.pos.y > 0.6f) { setGoal(71, "climb down"); climbTo(0.0f); }
            else { setGoal(72, "open the container"); useObj(U_LOAD_DOOR); }
            break;
        default: break;
        }
        // stuck detection
        if (length(g.pos - lastPos) > 0.02f || g.actionLock > 0 || g.stage == ST_LOAD) stuckT = 0; else stuckT += g.dt;
        lastPos = g.pos;
        if (stuckT > 1.5f && c.moveY > 0) {
            // slide sideways, then back off, and re-plan
            c.moveX = (g.frame / 50) % 2 ? 1.0f : -1.0f;
            if (stuckT > 3.0f) c.moveY = -1.0f;
            if (stuckT > 3.6f) { stuckT = 0; path.clear(); standIdx = -1; }
        }
        if (g_verbose && (g.frame % 120) == 0 && stuckT > 0.5f)
            printf("      stuck %.1fs at (%.2f %.2f %.2f) move %.1f/%.1f look %.2f wp %zu/%zu target (%.2f %.2f) prompt %d\n", stuckT, g.pos.x, g.pos.y, g.pos.z, c.moveX, c.moveY,
                   c.lookDX, pi, path.size(), pi < path.size() ? path[pi].x : navTarget.x, pi < path.size() ? path[pi].z : navTarget.z, g.promptIdx);
        if (goalT > 240.0f && goalKey != 36) {
            dump();
            printf("  BOT TIMEOUT on goal '%s' at (%.2f %.2f %.2f) stage %d prompt %d '%s'\n", goalName.c_str(), g.pos.x, g.pos.y, g.pos.z, g.stage, g.promptIdx,
                   g.promptText.c_str());
            failed = true;
        }
    }
};

// ---------------------------------------------------------------------------
static bool finite3(const Vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

static std::vector<std::string> g_collectedSaves;

struct RunResult {
    bool completed = false;
    int ending = 0;
    int deaths = 0;
    double gameTime = 0;
    int stageReached = 0;
};

static RunResult run(Game& g, int ending, int difficulty, bool riggerHarmless, double maxTime, const char* label, const std::string* fromSave = nullptr) {
    RunResult res;
    if (fromSave) {
        if (!game_load(g, *fromSave)) { printf("  FAIL: could not load save\n"); g_failures++; return res; }
        maxTime += g.time;
    } else game_new(g, difficulty);
    Bot bot(g);
    bot.wantEnding = ending;
    const float dt = 1.0f / 60.0f;
    std::vector<float> mixbuf(512 * 2);
    int lastStage = -1;
    long ticks = 0;
    float peak = 0;
    int saveChecks = 0;
    float stateTime[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int chases = 0, lastRs = RS_DORMANT, lastDeaths = 0;
    double lastT = 0;
    printf("== %s ==\n", label);
    while (g.time < maxTime) {
        bot.think();
        if (bot.failed) break;
        if (riggerHarmless && g.rig.state != RS_FINALE) g.rig.grace = 5.0f;
        game_update(g, bot.c, dt);
        ticks++;
        // audio runs alongside, exactly as it would in the browser
        if (ticks % 8 == 0) {
            audio_mix(mixbuf.data(), 512);
            for (float v : mixbuf) {
                if (!std::isfinite(v)) { CHECK(false, "audio produced a non-finite sample"); break; }
                peak = std::max(peak, std::fabs(v));
            }
        }
        if (g.appMode == AM_PLAY) {
            lastT = g.time;
            stateTime[g.rig.state & 7] += dt;
            if (g.rig.state != lastRs) {
                if (g.rig.state == RS_CHASE) chases++;
                if (g_verbose) printf("      [%7.1fs] rigger state %d -> %d  (rigger %.1f %.1f, player %.1f %.1f, dist %.1f)\n", g.time, lastRs, g.rig.state, g.rig.pos.x, g.rig.pos.z,
                                      g.pos.x, g.pos.z, distXZ(g.rig.pos, g.pos));
                lastRs = g.rig.state;
            }
            if (g.deaths != lastDeaths) {
                lastDeaths = g.deaths;
                printf("  [%7.1fs] DEATH #%d (cause %d) at (%.1f %.1f %.1f) during '%s'\n", g.time, g.deaths, g.deathCause, g.pos.x, g.pos.y, g.pos.z, bot.goalName.c_str());
            }
        }
        // invariants
        if (!finite3(g.pos) || !finite3(g.camPos) || !std::isfinite(g.camYaw) || !std::isfinite(g.camPitch)) {
            CHECK(false, "non-finite player/camera state at t=%.1f", g.time);
            break;
        }
        if (g.rig.state != RS_DORMANT && !finite3(g.rig.pos)) { CHECK(false, "non-finite rigger position"); break; }
        if (g.pos.y < -3.0f && g.pstate != PS_DEAD) { CHECK(false, "player fell out of the world at (%.1f %.1f %.1f)", g.pos.x, g.pos.y, g.pos.z); break; }
        if (g.appMode == AM_PLAY && g.stage != lastStage) {
            lastStage = g.stage;
            res.stageReached = g.stage;
            printf("  [%7.1fs] stage %2d: %s\n", g.time, g.stage, stage_title(g));
        }
        // every fresh checkpoint must survive a save/load round trip
        if (g.wantSave) {
            g.wantSave = false;
            if (!g.saveData.empty() && (g_collectedSaves.empty() || g_collectedSaves.back() != g.saveData)) g_collectedSaves.push_back(g.saveData);
            if (!g.saveData.empty()) {
                static Game* probe = nullptr;
                if (!probe) { probe = new Game(); game_init(*probe); }
                probe->deaths = 0;
                bool ok = game_load(*probe, g.saveData);
                CHECK(ok, "checkpoint failed to load: %s", g.saveData.c_str());
                if (ok) {
                    CHECK(probe->stage == g.stage, "stage mismatch after load (%d vs %d)", probe->stage, g.stage);
                    CHECK(probe->keyCode == g.keyCode, "key code mismatch after load");
                    CHECK(probe->fusesInstalled == g.fusesInstalled, "fuse count mismatch after load");
                    for (int i = 0; i < IT_COUNT; i++) CHECK(probe->itemTaken[i] == g.itemTaken[i], "item %d mismatch after load", i);
                    for (int i = 0; i < F_COUNT; i++)
                        if (i != F_FINALE && i != F_RADIO_ON) CHECK(probe->flags[i] == g.flags[i], "flag %d mismatch after load", i);
                    CHECK(game_save_string(*probe).substr(0, 20) == g.saveData.substr(0, 20), "save string not stable");
                    saveChecks++;
                }
            }
        }
        if (g.appMode == AM_ENDING) {
            res.completed = true;
            res.ending = g.ending;
            // click through the cards back to the title
            Controls c;
            int guard = 0;
            while (g.appMode == AM_ENDING && guard++ < 60 * 400) {
                c.confirm = (guard % 90) == 0;
                game_update(g, c, dt);
            }
            CHECK(g.appMode == AM_TITLE, "ending did not return to the title screen");
            break;
        }
        res.deaths = g.deaths;
    }
    res.gameTime = lastT;
    printf("  rigger: chases=%d  time patrol=%.0fs investigate=%.0fs chase=%.0fs search=%.0fs stare=%.0fs finale=%.0fs\n", chases, stateTime[RS_PATROL], stateTime[RS_INVESTIGATE],
           stateTime[RS_CHASE], stateTime[RS_SEARCH], stateTime[RS_STARE], stateTime[RS_FINALE]);
    res.deaths = std::max(res.deaths, g.deaths);
    printf("  -> %s  ending=%d  stage=%d  deaths=%d  game time=%.0fs  ticks=%ld  audio peak=%.2f  save checks=%d\n", res.completed ? "COMPLETED" : "not completed",
           res.ending, res.stageReached, res.deaths, res.gameTime, ticks, peak, saveChecks);
    CHECK(peak <= 1.0001f, "audio clipped (peak %.3f)", peak);
    CHECK(peak > 0.01f, "audio is silent");
    return res;
}

int main(int argc, char** argv) {
    std::string mode = "all";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) g_verbose = true;
        else mode = argv[i];
    }
    auto t0 = std::chrono::steady_clock::now();
    for (int s = 0; s < audio_init_steps(); s++) audio_init_step(s, 48000);
    static Game g;
    game_init(g);
    auto t1 = std::chrono::steady_clock::now();
    printf("init: %.2fs (audio + world)\n", std::chrono::duration<double>(t1 - t0).count());

    if (mode == "a" || mode == "all") {
        RunResult r = run(g, 1, 0, true, 3600.0, "ENDING A: put it back (rigger cannot see the bot)");
        CHECK(r.completed && r.ending == 1, "ending A was not reached");
        CHECK(r.deaths == 0, "the bot died %d times in the safe run", r.deaths);
    }
    if (mode == "b" || mode == "all") {
        RunResult r = run(g, 2, 1, true, 3600.0, "ENDING B: land it (rigger cannot see the bot)");
        CHECK(r.completed && r.ending == 2, "ending B was not reached");
    }
    if (mode == "resume" || mode == "all") {
        // resume from every checkpoint of a full playthrough and finish from there
        g_collectedSaves.clear();
        run(g, 2, 1, true, 3600.0, "RESUME: collecting checkpoints");
        std::vector<std::string> saves = g_collectedSaves;
        printf("  %zu checkpoints collected\n", saves.size());
        int ok = 0;
        for (size_t i = 0; i < saves.size(); i++) {
            int stage = atoi(saves[i].c_str() + saves[i].find("stage=") + 6);
            int want = (i % 2) ? 1 : 2;
            if (stage >= ST_ESCAPE) want = stage == ST_ESCAPE ? 1 : 2;
            std::string label = strf("RESUME from checkpoint %zu (stage %d), ending %d", i, stage, want);
            bool v = g_verbose;
            g_verbose = false;
            RunResult r = run(g, want, 1, true, 1500.0, label.c_str(), &saves[i]);
            g_verbose = v;
            CHECK(r.completed && r.ending == want, "could not finish from checkpoint %zu (stage %d)", i, stage);
            if (r.completed) ok++;
        }
        printf("  finished from %d of %zu checkpoints\n", ok, saves.size());
    }
    if (mode == "chaos" || mode == "all") {
        // the rigger hunts for real; the bot never hides, so deaths are expected.
        // What matters is that nothing breaks and every death resumes cleanly.
        RunResult r = run(g, 1, 1, false, 2400.0, "CHAOS: standard difficulty, live rigger, bot never hides");
        CHECK(r.stageReached >= ST_KEYCAB, "chaos run made no progress past the generator (stage %d)", r.stageReached);
        RunResult n = run(g, 2, 2, false, 1500.0, "CHAOS: nightmare difficulty");
        (void)n;
    }
    auto t2 = std::chrono::steady_clock::now();
    printf("\n%s  (%d failure%s, %.1fs wall)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", g_failures, g_failures == 1 ? "" : "s",
           std::chrono::duration<double>(t2 - t1).count());
    return g_failures == 0 ? 0 : 1;
}
