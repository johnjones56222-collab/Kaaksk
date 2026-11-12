// THE 1982 CRANE - the Rigger: perception, pursuit, and its stop-motion body
#include "game.h"
#include "audio.h"

static float chase_speed(const Game& g) { return g.difficulty == 0 ? 3.5f : (g.difficulty == 1 ? 4.05f : 4.6f); }
static float hear_mul(const Game& g) { return g.difficulty == 0 ? 0.7f : (g.difficulty == 1 ? 1.0f : 1.25f); }

void rigger_spawn(Game& g, const Vec3& pos, int state) {
    Rigger& r = g.rig;
    r = Rigger();
    r.state = state;
    r.pos = nav_snap(g.w, pos, 10);
    if (state == RS_STATUE) r.pos = pos;
    r.target = r.pos;
    r.lastKnown = g.pos;
    r.visible = true;
    Vec3 d = g.pos - r.pos;
    r.yaw = std::atan2(d.x, d.z);
    if (state == RS_STATUE && g.stage == ST_OPEN) {
        // it stands and watches the container door
        Vec3 door = pad_pos() + pad_dir() * (LOAD_L * 0.5f);
        r.yaw = std::atan2(door.x - r.pos.x, door.z - r.pos.z);
    }
}

static bool player_exposed(const Game& g) {
    return g.pstate == PS_NORMAL || g.pstate == PS_LADDER || g.pstate == PS_DEAD;
}

static bool can_see_player(const Game& g, float* distOut) {
    const Rigger& r = g.rig;
    if (!player_exposed(g)) return false;
    Vec3 eye = r.pos + Vec3(0, 2.05f, 0);
    Vec3 tgt = g.pos + Vec3(0, g.crouched ? 0.7f : 1.25f, 0);
    Vec3 d = tgt - eye;
    float dist = length(d);
    if (distOut) *distOut = dist;
    // how far it can see depends on how much light the player is carrying or standing in
    float range = g.crouched ? 6.0f : 9.5f;
    if (g.flashOn && g.have[IT_FLASHLIGHT]) range = 25.0f;
    else if (g.flags[F_GEN_RUNNING] && g.indoor < 0.5f) range += 7.0f;      // the yard lamps
    else if (g.flags[F_GEN_RUNNING]) range += 4.0f;
    if (g.noise > 12.0f) range += 4.0f;
    if (g.difficulty == 2) range *= 1.2f;
    if (g.difficulty == 0) range *= 0.85f;
    if (r.state == RS_CHASE) range = std::max(range, 16.0f) * 1.2f;
    if (dist > range) return false;
    // field of view (it sees all round at arm's length)
    Vec3 f(std::sin(r.yaw + r.headYaw), 0, std::cos(r.yaw + r.headYaw));
    Vec3 dn = normalize(Vec3(d.x, 0, d.z));
    if (dist > 2.5f && dot(f, dn) < 0.4f) return false;
    return g.w.lineOfSight(eye, tgt, COL_SIGHT);
}

void alert_noise(Game& g, const Vec3& pos, float radius) {
    Rigger& r = g.rig;
    if (r.state == RS_DORMANT || r.state == RS_STATUE || r.state == RS_FINALE || r.grace > 0) return;
    float d = length(pos - r.pos);
    if (d > radius * hear_mul(g)) return;
    if (r.state == RS_CHASE) { r.lastKnown = pos; return; }
    if (g.pstate == PS_HIDING) return;
    r.state = RS_INVESTIGATE;
    r.target = nav_snap(g.w, pos, 8);
    r.stateTime = 0;
    r.repath = 0;
    r.alert = std::max(r.alert, 0.35f);
}

static void set_path(Game& g, const Vec3& to) {
    Rigger& r = g.rig;
    if (nav_path(g.w, r.pos, to, r.path)) r.pathIdx = 0;
    else { r.path.clear(); r.pathIdx = 0; }
}

static void pick_patrol(Game& g) {
    Rigger& r = g.rig;
    const World& w = g.w;
    float lo = g.difficulty == 2 ? 10.0f : 16.0f, hi = g.difficulty == 0 ? 60.0f : 46.0f;
    int best = -1;
    for (int tries = 0; tries < 24; tries++) {
        int i = (int)(g.rng.next() % (u32)w.patrolPoints.size());
        if (i == r.patrolIdx) continue;
        float d = distXZ(w.patrolPoints[(size_t)i], g.pos);
        if (d >= lo && d <= hi) { best = i; break; }
        if (best < 0) best = i;
    }
    if (best < 0) best = 0;
    r.patrolIdx = best;
    r.target = w.patrolPoints[(size_t)best];
    set_path(g, r.target);
}

// Opens any closed door it is about to walk through.
static void open_doors(Game& g) {
    World& w = g.w;
    Rigger& r = g.rig;
    for (int i = 0; i < D_COUNT; i++) {
        Door& d = w.doors[i];
        if (d.dyn < 0 || i == D_LIFT3 || d.lockedBy == -2) continue;
        if (std::fabs(d.target) > 0.01f) continue;
        if (i == D_CRANE_GATE && d.lockedBy >= 0) continue;
        if (distXZ(w.doorPos[i], r.pos) > 1.7f || std::fabs(w.doorPos[i].y - 1.0f - r.pos.y) > 1.6f) continue;
        d.lockedBy = -1;
        d.target = d.openAngle;
        audio_play_at(S_SLAM, w.doorPos[i], 0.9f, 0.9f, 8.0f);
    }
}

static bool advance(Game& g, float speed, float dt) {
    // returns true when the end of the path has been reached
    Rigger& r = g.rig;
    const NavGrid& n = g.w.nav;
    r.speed = dampf(r.speed, speed, 5.0f, dt);
    if (r.pathIdx >= r.path.size()) { r.speed = dampf(r.speed, 0.0f, 8.0f, dt); return true; }
    Vec3 wp = r.path[r.pathIdx];
    Vec3 d(wp.x - r.pos.x, 0, wp.z - r.pos.z);
    float dist = length(d);
    if (dist < 0.3f) { r.pathIdx++; return r.pathIdx >= r.path.size(); }
    Vec3 dir = d * (1.0f / dist);
    float step = std::min(dist, r.speed * dt);
    Vec3 np = r.pos + dir * step;
    int cx = n.cx(np.x), cz = n.cz(np.z);
    if (n.walkable(cx, cz)) {
        r.pos.x = np.x; r.pos.z = np.z;
        r.pos.y = dampf(r.pos.y, n.height[(size_t)cz * n.w + cx], 12.0f, dt);
    } else {
        // slid off the grid: recover
        Vec3 s = nav_snap(g.w, r.pos, 6);
        r.pos.x = dampf(r.pos.x, s.x, 10.0f, dt);
        r.pos.z = dampf(r.pos.z, s.z, 10.0f, dt);
        r.repath = 0;
    }
    float want = std::atan2(dir.x, dir.z);
    r.yaw += clampv(wrapAngle(want - r.yaw), -7.0f * dt, 7.0f * dt);
    r.walkPhase += step * (speed > 3.0f ? 2.3f : 2.9f);
    r.stepTimer -= step;
    if (r.stepTimer <= 0) {
        r.stepTimer = speed > 3.0f ? 1.25f : 0.95f;
        audio_play_at(S_RIG_STEP, r.pos, speed > 3.0f ? 1.0f : 0.75f, 0.9f + g.rng.f() * 0.2f, 7.0f);
    }
    return false;
}

static void face(Game& g, const Vec3& p, float dt, float rate = 4.0f) {
    Rigger& r = g.rig;
    float want = std::atan2(p.x - r.pos.x, p.z - r.pos.z);
    r.yaw += clampv(wrapAngle(want - r.yaw), -rate * dt, rate * dt);
}

void rigger_update(Game& g, float dt) {
    Rigger& r = g.rig;
    World& w = g.w;
    if (r.state == RS_DORMANT) {
        r.visible = false;
        if (g.hRigBreath >= 0) { audio_stop(g.hRigBreath, 0.5f); g.hRigBreath = -1; }
        if (g.hChase >= 0) { audio_stop(g.hChase, 1.0f); g.hChase = -1; }
        rigger_pose(g);
        return;
    }
    r.visible = true;
    r.stateTime += dt;
    r.grace = std::max(0.0f, r.grace - dt);
    r.repath -= dt;
    float dist = 999;
    bool sees = r.grace <= 0 && r.state != RS_STATUE && can_see_player(g, &dist);
    float dPlayer = length(g.pos - r.pos);

    // the player has gone where it does not follow
    bool onBarge = g.pos.z < QUAY_Z - 0.5f;
    bool aloft = g.pos.y > 4.0f;
    if (aloft && g.pos.y > 6.6f && r.state != RS_STATUE && r.state != RS_FINALE) {
        r.state = RS_DORMANT;
        return;
    }

    // suspicion
    if (r.state == RS_PATROL || r.state == RS_INVESTIGATE || r.state == RS_SEARCH || r.state == RS_STARE) {
        if (sees) {
            float rate = (r.state == RS_PATROL ? 1.0f : 1.6f) * (1.4f * saturate(1.0f - dist / 26.0f) + 0.4f);
            if (dist < 4.0f) rate = 6.0f;
            r.alert += dt * rate;
            r.lastKnown = g.pos;
            if (r.alert >= 1.0f && !onBarge) {
                r.state = RS_CHASE;
                r.stateTime = 0;
                r.lastSeen = 0;
                r.repath = 0;
                audio_play_at(S_SCREAM, r.pos + Vec3(0, 2, 0), 1.0f, 1.0f, 22.0f);
                audio_play(S_STINGER, 0.9f);
                g.shake = std::max(g.shake, 0.35f);
            }
        } else r.alert = std::max(0.0f, r.alert - dt * 0.3f);
    }

    switch (r.state) {
    case RS_PATROL: {
        if (r.path.empty() || r.pathIdx >= r.path.size()) {
            // arrived: stand and listen for a while
            r.speed = dampf(r.speed, 0.0f, 8.0f, dt);
            r.nearTime += dt;
            r.headYaw = 0.9f * std::sin(r.stateTime * 0.8f);
            if (r.nearTime > 2.5f + 2.0f * hash2f(r.patrolIdx, 3)) { r.nearTime = 0; pick_patrol(g); }
        } else {
            r.headYaw = dampf(r.headYaw, 0.35f * std::sin(r.stateTime * 0.6f), 3.0f, dt);
            advance(g, 1.45f, dt);
        }
        break;
    }
    case RS_INVESTIGATE: {
        if (r.repath <= 0) { set_path(g, r.target); r.repath = 2.0f; }
        bool arrived = advance(g, 2.5f, dt);
        if (arrived) {
            r.nearTime += dt;
            r.headYaw = 1.1f * std::sin(r.nearTime * 1.4f);
            if (r.nearTime > 5.0f) { r.nearTime = 0; r.state = RS_SEARCH; r.stateTime = 0; r.lastKnown = r.pos; r.repath = 0; }
        } else r.nearTime = 0;
        break;
    }
    case RS_CHASE: {
        if (sees) { r.lastSeen = 0; r.lastKnown = g.pos; }
        else r.lastSeen += dt;
        Vec3 goal = r.lastKnown;
        // it saw the player shut themselves in
        if (g.pstate == PS_HIDING && r.sawPlayerHide) goal = w.hides[g.hideIdx].outside;
        if (onBarge) {
            // it stops at the head of the gangway and stands there
            goal = Vec3(27.0f, 0, QUAY_Z + 1.6f);
            if (distXZ(r.pos, goal) < 1.0f) { r.state = RS_STARE; r.stateTime = 0; break; }
        }
        if (aloft) {
            goal = Vec3(CRANE_POS.x - 3.2f, 0, CRANE_POS.z + 7.6f);
            if (distXZ(r.pos, goal) < 1.2f) { r.state = RS_STARE; r.stateTime = 0; break; }
        }
        if (r.repath <= 0) { set_path(g, goal); r.repath = 0.35f; }
        advance(g, chase_speed(g), dt);
        r.headYaw = dampf(r.headYaw, 0.0f, 6.0f, dt);
        float lose = g.difficulty == 0 ? 3.5f : (g.difficulty == 1 ? 5.0f : 6.5f);
        if (g.pstate == PS_HIDING && r.sawPlayerHide) {
            if (distXZ(r.pos, w.hides[g.hideIdx].outside) < 1.2f) { kill_player(g, 0); return; }
        } else if (r.lastSeen > lose) {
            r.state = RS_SEARCH;
            r.stateTime = 0;
            r.repath = 0;
            r.alert = 0.5f;
        }
        break;
    }
    case RS_SEARCH: {
        if (r.repath <= 0) {
            // prowl around the last place it knew
            float a = g.rng.f() * TAU, rad = 2.0f + g.rng.f() * 7.0f;
            Vec3 p = nav_snap(w, r.lastKnown + Vec3(std::cos(a) * rad, 0, std::sin(a) * rad), 8);
            set_path(g, r.stateTime < 0.5f ? r.lastKnown : p);
            r.repath = 3.5f + g.rng.f() * 2.0f;
        }
        bool arrived = advance(g, 2.1f, dt);
        r.headYaw = arrived ? 1.2f * std::sin(r.stateTime * 1.7f) : dampf(r.headYaw, 0.5f * std::sin(r.stateTime), 3.0f, dt);
        float total = g.difficulty == 2 ? 20.0f : 13.0f;
        if (r.stateTime > total) { r.state = RS_PATROL; r.stateTime = 0; r.path.clear(); r.alert = 0; r.nearTime = 0; }
        break;
    }
    case RS_STARE: {
        r.speed = dampf(r.speed, 0.0f, 8.0f, dt);
        face(g, g.pos, dt, 2.0f);
        r.headYaw = dampf(r.headYaw, 0.0f, 4.0f, dt);
        if (!onBarge && !aloft) { r.state = RS_CHASE; r.lastSeen = 0; r.lastKnown = g.pos; r.repath = 0; }
        else if (r.stateTime > 22.0f) { r.state = RS_PATROL; r.stateTime = 0; r.path.clear(); r.alert = 0; r.grace = 6.0f; }
        break;
    }
    case RS_FINALE: {
        if (r.repath <= 0) { set_path(g, g.pos); r.repath = 0.3f; }
        float sp = g.difficulty == 2 ? 5.0f : 4.7f;
        // it never quite falls behind, never quite catches a runner who does not stop
        if (dPlayer > 16.0f) sp += 1.4f;
        advance(g, sp, dt);
        break;
    }
    case RS_STATUE:
        r.speed = 0;
        if (g.stage == ST_OPEN) r.headYaw = dampf(r.headYaw, dPlayer < 6.0f ? clampv(wrapAngle(std::atan2(g.pos.x - r.pos.x, g.pos.z - r.pos.z) - r.yaw), -1.3f, 1.3f) : 0.0f, 1.5f, dt);
        else face(g, g.pos, dt, 1.5f);
        break;
    default: break;
    }
    if (r.state != RS_STATUE) open_doors(g);

    // contact
    if ((r.state == RS_CHASE || r.state == RS_FINALE) && g.pstate == PS_NORMAL && distXZ(r.pos, g.pos) < 1.2f && std::fabs(r.pos.y - g.pos.y) < 1.6f) {
        kill_player(g, 0);
        return;
    }
    r.reach = dampf(r.reach, (r.state == RS_CHASE || r.state == RS_FINALE) ? saturate(1.4f - dPlayer / 9.0f) : 0.0f, 5.0f, dt);

    // sound
    if (g.hRigBreath < 0) g.hRigBreath = audio_loop_at(S_RIG_BREATH, r.pos + Vec3(0, 2, 0), 0.9f, 4.0f);
    audio_move(g.hRigBreath, r.pos + Vec3(0, 2, 0));
    bool hunting = r.state == RS_CHASE || r.state == RS_FINALE;
    if (hunting && g.hChase < 0) g.hChase = audio_loop(S_CHASE_LOOP, 0.0f);
    if (g.hChase >= 0) {
        static float cg = 0;
        cg = dampf(cg, hunting ? 0.75f : 0.0f, hunting ? 3.0f : 0.5f, dt);
        audio_set(g.hChase, cg);
        if (!hunting && cg < 0.01f) { audio_stop(g.hChase, 0.3f); g.hChase = -1; cg = 0; }
    }
    rigger_pose(g);
}

// ---------------------------------------------------------------------------
// Pose. Animation time is quantised so it moves like something filmed a frame at a time.
// ---------------------------------------------------------------------------
void rigger_pose(Game& g) {
    World& w = g.w;
    Rigger& r = g.rig;
    for (int i = DY_RIG_FIRST; i <= DY_RIG_LAST; i++) w.dyn[(size_t)i].visible = r.visible && r.state != RS_DORMANT;
    if (!r.visible || r.state == RS_DORMANT) return;
    float t = (float)g.time;
    float qt = std::floor(t * 11.0f) / 11.0f;
    float ph = std::floor(r.walkPhase * 2.4f) / 2.4f;
    bool hunting = r.state == RS_CHASE || r.state == RS_FINALE;
    float moving = saturate(r.speed / 1.2f);
    float A = (hunting ? 0.8f : 0.5f) * moving;
    float lean = hunting ? 0.42f : 0.16f + 0.05f * std::sin(qt * 0.7f);
    float twitch = hash2f((int)(qt * 11.0f), 77) > 0.93f ? (hash2f((int)(qt * 11.0f), 78) - 0.5f) * 0.9f : 0.0f;
    float hip = 1.2f - 0.05f * std::fabs(std::sin(ph)) * moving;
    Mat4 root = Mat4::translate(r.pos) * Mat4::rotY(r.yaw);
    Mat4 torso = root * Mat4::translate(Vec3(0, hip, 0)) * Mat4::rotX(lean) * Mat4::rotZ(0.03f * std::sin(qt * 1.3f));
    w.dyn[DY_RIG_TORSO].xf = torso;
    w.dyn[DY_RIG_HEAD].xf = torso * Mat4::translate(Vec3(0, 0.9f, 0.03f)) * Mat4::rotY(r.headYaw + twitch) * Mat4::rotX(-lean * 0.7f + 0.12f) * Mat4::rotZ(twitch * 0.4f + 0.12f);
    // arms: the left hangs and swings, the right carries the hook; both come up when it is close
    float reach = r.reach;
    float la = -std::sin(ph) * 0.45f * moving - reach * 1.25f + 0.05f;
    float ra = std::sin(ph) * 0.35f * moving - reach * 1.5f - 0.1f;
    Mat4 ul = torso * Mat4::translate(Vec3(-0.31f, 0.84f, 0)) * Mat4::rotZ(-0.07f) * Mat4::rotX(la);
    Mat4 ur = torso * Mat4::translate(Vec3(0.31f, 0.84f, 0)) * Mat4::rotZ(0.07f) * Mat4::rotX(ra);
    w.dyn[DY_RIG_ARM_UL].xf = ul;
    w.dyn[DY_RIG_ARM_UR].xf = ur;
    Mat4 ll = ul * Mat4::translate(Vec3(0, -0.56f, 0)) * Mat4::rotX(-0.25f - reach * 0.5f + twitch * 0.5f);
    Mat4 lr = ur * Mat4::translate(Vec3(0, -0.56f, 0)) * Mat4::rotX(-0.35f - reach * 0.4f);
    w.dyn[DY_RIG_ARM_LL].xf = ll;
    w.dyn[DY_RIG_ARM_LR].xf = lr;
    w.dyn[DY_RIG_HOOK].xf = lr * Mat4::translate(Vec3(0, -0.6f, 0)) * Mat4::rotY(0.4f * std::sin(qt * 2.0f));
    // legs
    float sL = std::sin(ph) * A, sR = -std::sin(ph) * A;
    float kL = std::max(0.0f, -std::cos(ph)) * 0.9f * moving, kR = std::max(0.0f, std::cos(ph)) * 0.9f * moving;
    Mat4 pelvis = root * Mat4::translate(Vec3(0, hip, 0));
    Mat4 lul = pelvis * Mat4::translate(Vec3(-0.1f, 0, 0)) * Mat4::rotX(-sL);
    Mat4 lur = pelvis * Mat4::translate(Vec3(0.1f, 0, 0)) * Mat4::rotX(-sR);
    w.dyn[DY_RIG_LEG_UL].xf = lul;
    w.dyn[DY_RIG_LEG_UR].xf = lur;
    w.dyn[DY_RIG_LEG_LL].xf = lul * Mat4::translate(Vec3(0, -0.6f, 0)) * Mat4::rotX(kL);
    w.dyn[DY_RIG_LEG_LR].xf = lur * Mat4::translate(Vec3(0, -0.6f, 0)) * Mat4::rotX(kR);
}
