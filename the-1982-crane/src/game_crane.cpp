// THE 1982 CRANE - the crane simulation, the two endings
#include "game.h"
#include "audio.h"

static const Vec3 PAD(12.0f, 0.0f, -36.0f);

static float ground_at(float x, float z) {
    if (z < QUAY_Z - 0.3f) {
        if (x > 13.0f && x < 41.0f && z > -63.0f && z < -49.0f) return 1.9f; // barge and its deck house: keep clear
        return -60.0f;
    }
    return 0.0f;
}

static Mat4 segment_matrix(const Vec3& a, const Vec3& b) {
    // maps the unit cable (0,0,0)-(0,-1,0) onto a-b
    Vec3 d = b - a;
    float len = length(d);
    Vec3 y = len > 1e-5f ? d * (-1.0f / len) : Vec3(0, 1, 0);
    Vec3 helper = std::fabs(y.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 x = normalize(cross(helper, y));
    Vec3 z = cross(x, y);
    Mat4 m = Mat4::identity();
    m.m[0] = x.x; m.m[1] = x.y; m.m[2] = x.z;
    m.m[4] = y.x * len; m.m[5] = y.y * len; m.m[6] = y.z * len;
    m.m[8] = z.x; m.m[9] = z.y; m.m[10] = z.z;
    m.m[12] = a.x; m.m[13] = a.y; m.m[14] = a.z;
    return m;
}

static void try_release(Game& g) {
    CraneSim& c = g.crane;
    World& w = g.w;
    if (c.ready == 2) {
        c.loadReleased = true;
        g.flags[F_RELEASED_WATER] = true;
        g.pstate = PS_SEATED;
        audio_play(S_LEVER, 0.9f);
        audio_play_at(S_SPLASH, c.loadPos, 1.0f, 0.6f, 40.0f);
        audio_play_at(S_CRANE_GROAN, CRANE_POS + Vec3(0, 30, 0), 1.0f, 0.5f, 60.0f);
        audio_play(S_SWELL, 0.9f, 0.7f);
        c.shake = 1.0f;
        g.shake = 1.0f;
        set_stage(g, ST_ESCAPE);
        schedule(g, 3.5f, TC_RELEASE_A2);
    } else if (c.ready == 4) {
        c.loadReleased = true;
        g.flags[F_RELEASED_QUAY] = true;
        g.pstate = PS_SEATED;
        audio_play(S_LEVER, 0.9f);
        audio_play_at(S_METAL_BANG, c.loadPos, 1.0f, 0.5f, 40.0f);
        audio_play(S_SWELL, 0.7f, 0.6f);
        c.shake = 0.5f;
        g.shake = 0.6f;
        w.interacts[(size_t)w.useInteract[U_LOAD_DOOR]].enabled = true;
        set_stage(g, ST_OPEN);
        schedule(g, 3.5f, TC_RELEASE_B2);
    } else {
        audio_play(S_ERROR, 0.6f);
        if (c.ready == 1) hint(g, "Lower. It has to go all the way under before it will let go.", 4.5f);
        else if (c.ready == 3) hint(g, "Lower. Set it down on the hatch marks first.", 4.5f);
        else hint(g, "Not here. Over the harbor and under the water - or square on the hatch marks on the quay, jib pointing east.", 6.5f);
    }
}

void crane_update(Game& g, const Controls& in, float dt) {
    World& w = g.w;
    CraneSim& c = g.crane;
    float t = (float)g.time;
    bool operating = g.pstate == PS_OPERATING && !c.loadReleased;
    float prevSlew = c.slew, prevTr = c.trolley;
    float wantSlew = 0, wantTr = 0, wantHo = 0;

    if (operating) {
        wantSlew = clampv(in.craneSlew, -1.0f, 1.0f) * 0.2f;
        wantTr = clampv(in.craneTrolley, -1.0f, 1.0f) * 2.7f;
        wantHo = -clampv(in.craneHoist, -1.0f, 1.0f) * 3.0f;
        if ((wantSlew != 0 || wantTr != 0 || wantHo != 0) && !g.flags[F_OPERATED]) g.flags[F_OPERATED] = true;
        if (in.craneRelease) try_release(g);
    } else if (c.loadReleased) {
        // the hook goes home on its own
        if (g.flags[F_RELEASED_QUAY]) wantHo = c.hoist > 12.0f ? -1.2f : 0.0f;
    } else if (c.locked) {
        float d = wrapAngle(0.0f - c.slew);
        wantSlew = clampv(d * 1.5f, -0.3f, 0.3f);
        wantTr = clampv((20.0f - c.trolley) * 1.0f, -2.0f, 2.0f);
        wantHo = clampv((18.9f - c.hoist) * 1.0f, -2.0f, 2.0f);
    } else {
        // it works by itself
        c.autoTimer -= dt;
        if (c.autoTimer <= 0) {
            c.autoTimer = 16.0f + g.rng.f() * 26.0f;
            c.autoTarget = -0.5f + g.rng.f() * 1.7f;
            if (g.appMode == AM_PLAY) audio_play_at(S_CRANE_GROAN, CRANE_POS + Vec3(0, 30, 0), 0.9f, 0.8f + g.rng.f() * 0.3f, 50.0f);
        }
        float d = c.autoTarget - c.slew;
        wantSlew = clampv(d * 0.6f, -0.085f, 0.085f);
        wantHo = 0.25f * std::sin(t * 0.07f);
        wantTr = 0.3f * std::sin(t * 0.045f + 1.0f);
    }
    c.slewVel = dampf(c.slewVel, wantSlew, 1.6f, dt);
    c.trolleyVel = dampf(c.trolleyVel, wantTr, 2.5f, dt);
    c.hoistVel = dampf(c.hoistVel, wantHo, 3.0f, dt);

    float nslew = clampv(c.slew + c.slewVel * dt, -1.9f, 2.7f);
    float ntr = clampv(c.trolley + c.trolleyVel * dt, CRANE_TROLLEY_MIN, CRANE_TROLLEY_MAX);
    float nho = clampv(c.hoist + c.hoistVel * dt, 5.0f, 34.6f);
    if (!c.loadReleased) {
        // the load may not pass through the quay
        auto plumb = [&](float sl, float tr) {
            return CRANE_POS + Vec3(std::cos(sl) * tr, 0, -std::sin(sl) * tr);
        };
        float bottom = CRANE_JIB_Y - c.hoist - HOOK_DROP - LOAD_BOTTOM;
        Vec3 np = plumb(nslew, ntr);
        float gnew = std::max(ground_at(np.x, np.z), std::max(ground_at(np.x + 2.6f, np.z), ground_at(np.x - 2.6f, np.z)));
        gnew = std::max(gnew, std::max(ground_at(np.x, np.z + 2.6f), ground_at(np.x, np.z - 2.6f)));
        if (bottom < gnew - 0.02f) { nslew = c.slew; ntr = c.trolley; c.slewVel = 0; c.trolleyVel = 0; np = plumb(nslew, ntr); gnew = ground_at(np.x, np.z); }
        float maxHoist = CRANE_JIB_Y - HOOK_DROP - LOAD_BOTTOM - std::max(gnew, -5.6f);
        if (nho > maxHoist) { nho = std::max(c.hoist > maxHoist ? maxHoist : c.hoist, maxHoist); c.hoistVel = std::min(c.hoistVel, 0.0f); }
    }
    c.slew = nslew; c.trolley = ntr; c.hoist = nho;
    if (nslew == prevSlew) c.slewVel *= 0.9f;
    (void)prevTr;

    // superstructure transform
    c.shake = std::max(0.0f, c.shake - dt * 0.18f);
    float jit = c.shake * 0.004f * std::sin(t * 37.0f);
    Mat4 top = Mat4::translate(CRANE_POS) * Mat4::rotY(c.slew + jit);
    w.dyn[DY_CRANE_TOP].xf = top;
    Vec3 tp = top.point(Vec3(c.trolley, CRANE_JIB_Y, 0));
    w.dyn[DY_TROLLEY].xf = top * Mat4::translate(Vec3(c.trolley, CRANE_JIB_Y, 0));

    // pendulum
    if (!c.havePrev) { c.prevT = tp; c.prevTV = Vec3(0, 0, 0); c.havePrev = true; }
    if (dt > 1e-5f) {
        Vec3 v = (tp - c.prevT) * (1.0f / dt);
        Vec3 a = (v - c.prevTV) * (1.0f / dt);
        a.y = 0;
        float al = length(a);
        if (al > 6.0f) a = a * (6.0f / al);
        float L = std::max(6.0f, c.hoist + HOOK_DROP + 3.0f);
        Vec3 wind(0.05f * std::sin(t * 0.6f) * g.wind, 0, 0.04f * std::sin(t * 0.47f + 2.0f) * g.wind);
        Vec3 acc = a * -1.0f - c.swing * (9.8f / L) - c.swingVel * 0.22f + wind;
        if (c.loadReleased) acc = c.swing * -(9.8f / 6.0f) - c.swingVel * 0.8f;
        c.swingVel += acc * dt;
        c.swing += c.swingVel * dt;
        c.swing.y = 0;
        float sl = length(c.swing), mx = L * 0.2f;
        if (sl > mx) { c.swing = c.swing * (mx / sl); c.swingVel = c.swingVel * 0.5f; }
        c.prevT = tp; c.prevTV = v;
    }
    Vec3 hookTop = tp + Vec3(0, -c.hoist, 0) + c.swing * (c.hoist / (c.hoist + HOOK_DROP + 3.0f));
    Vec3 sling = tp + Vec3(0, -(c.hoist + HOOK_DROP), 0) + c.swing;
    Vec3 jibDir = top.dir(Vec3(1, 0, 0));
    w.dyn[DY_CABLE_A].xf = segment_matrix(tp + jibDir * 0.3f + Vec3(0, -0.05f, 0), hookTop + jibDir * 0.2f + Vec3(0, -0.2f, 0));
    w.dyn[DY_CABLE_B].xf = segment_matrix(tp - jibDir * 0.3f + Vec3(0, -0.05f, 0), hookTop - jibDir * 0.2f + Vec3(0, -0.2f, 0));
    w.dyn[DY_HOOK].xf = Mat4::translate(hookTop) * Mat4::rotY(c.slew);

    // the load
    if (!c.loadReleased) {
        c.loadPos = sling;
        c.loadYaw = c.slew;
    } else if (g.flags[F_RELEASED_WATER]) {
        c.sink = std::min(40.0f, c.sink + dt * (0.6f + c.sink * 0.25f));
        c.loadPos.y = std::min(c.loadPos.y, WATER_Y - 0.5f) - dt * (0.6f + c.sink * 0.25f);
        if (c.loadPos.y < -60.0f) c.loadPos.y = -60.0f;
    } else if (g.flags[F_RELEASED_QUAY]) {
        c.loadPos = Vec3(PAD.x, LOAD_BOTTOM, PAD.z);
        c.loadYaw = 0;
    }
    float tilt = c.loadReleased ? 0.0f : 0.02f * std::sin(t * 0.8f);
    Mat4 loadXf = Mat4::translate(c.loadPos) * Mat4::rotY(c.loadYaw + 0.03f * std::sin(t * 0.21f) * (c.loadReleased ? 0.0f : 1.0f)) * Mat4::rotZ(tilt);
    w.dyn[DY_LOAD].xf = loadXf;
    w.dyn[DY_LOAD].visible = c.loadPos.y > -50.0f;
    w.dyn[DY_LOAD_DOOR].visible = w.dyn[DY_LOAD].visible;
    w.dyn[DY_LOAD_DOOR].xf = loadXf * Mat4::translate(Vec3(LOAD_L * 0.5f, -LOAD_BOTTOM + 0.05f, -LOAD_W * 0.5f + 0.04f)) * Mat4::rotY(-PI * 0.5f + c.doorOpen * 1.9f);

    // where could it be put down?
    c.ready = 0;
    if (!c.loadReleased) {
        float bottom = c.loadPos.y - LOAD_BOTTOM;
        bool overWater = c.loadPos.z < QUAY_Z - 2.2f && ground_at(c.loadPos.x, c.loadPos.z) < -10.0f;
        if (overWater) c.ready = (bottom < WATER_Y - LOAD_H * 0.75f) ? 2 : 1;
        else if (distXZ(c.loadPos, PAD) < 2.4f && std::fabs(wrapAngle(c.slew)) < 0.16f) c.ready = (bottom < 0.14f) ? 4 : 3;
    }

    // cab levers follow the inputs
    {
        float a0 = operating ? in.craneSlew : 0.0f, a1 = operating ? in.craneTrolley : 0.0f, a2 = operating ? in.craneHoist : 0.0f;
        static float s0 = 0, s1 = 0, s2 = 0;
        s0 = dampf(s0, a0, 10.0f, dt); s1 = dampf(s1, a1, 10.0f, dt); s2 = dampf(s2, a2, 10.0f, dt);
        w.dyn[DY_CAB_LEVER1].xf = top * Mat4::translate(w.cabLeverLocal[0]) * Mat4::rotX(s0 * 0.5f);
        w.dyn[DY_CAB_LEVER2].xf = top * Mat4::translate(w.cabLeverLocal[1]) * Mat4::rotZ(-s1 * 0.5f);
        w.dyn[DY_CAB_LEVER3].xf = top * Mat4::translate(w.cabLeverLocal[2]) * Mat4::rotZ(-s2 * 0.5f);
    }

    // lights riding on the crane
    {
        Light& fl = w.lights[(size_t)w.lightFlood];
        fl.pos = tp + Vec3(0, -0.35f, 0);
        fl.dir = normalize(sling + Vec3(0, -6.0f, 0) - fl.pos);
        w.lights[(size_t)w.lightCab].pos = top.point(w.cabLampLocal);
        for (int i = 0; i < 3; i++) w.lights[(size_t)w.lightBeacon[i]].pos = top.point(w.beaconLocal[i]);
    }

    // motors
    float motor = saturate(std::fabs(c.slewVel) / 0.12f + std::fabs(c.trolleyVel) / 2.5f + std::fabs(c.hoistVel) / 2.8f);
    c.motor = dampf(c.motor, motor, 4.0f, dt);
    if (g.appMode == AM_PLAY) {
        if (g.hCraneMotor < 0) g.hCraneMotor = audio_loop_at(S_CRANE_MOTOR, CRANE_POS + Vec3(0, 34, 0), 0.0f, 16.0f);
        audio_set(g.hCraneMotor, c.motor * 0.9f, 0.8f + 0.3f * c.motor);
        audio_move(g.hCraneMotor, operating || g.pstate == PS_SEATED ? g.camPos + Vec3(0, 2, 0) : CRANE_POS + Vec3(0, 34, 0));
    }

    // what is on the ladder
    if ((g.pstate == PS_OPERATING || g.pstate == PS_SEATED) && g.flags[F_OPERATED] && !c.loadReleased && g.stage == ST_LOAD) {
        float total = g.difficulty == 0 ? 230.0f : (g.difficulty == 1 ? 165.0f : 115.0f);
        c.climb += dt / total;
        c.clangT -= dt;
        if (c.clangT <= 0) {
            c.clangT = lerpf(3.6f, 0.75f, saturate(c.climb));
            audio_play_at(S_CLANG, CRANE_POS + Vec3(0, lerpf(2.0f, 30.0f, saturate(c.climb)), 0), 1.0f, 0.85f + g.rng.f() * 0.25f, 10.0f);
        }
        if (c.climbMsg == 0 && c.climb > 0.3f) { c.climbMsg = 1; say(g, "DISPATCH", "It's on the mast. First landing. Don't stop working.", 5.0f); }
        if (c.climbMsg == 1 && c.climb > 0.6f) { c.climbMsg = 2; say(g, "DISPATCH", "Third landing. Relief, whatever you're going to do, do it.", 5.0f); g.shake = std::max(g.shake, 0.3f); }
        if (c.climbMsg == 2 && c.climb > 0.86f) {
            c.climbMsg = 3;
            say(g, "", "The cab rocks. Something has put its weight on the door.", 5.0f);
            audio_play(S_SLAM, 1.0f, 0.7f);
            g.shake = std::max(g.shake, 0.7f);
        }
        if (c.climb >= 1.0f) kill_player(g, 2);
    }

    // endgame on the ground
    if (g.stage == ST_ESCAPE && g.flags[F_RELEASED_WATER]) {
        g.dawn = std::min(1.0f, g.dawn + dt * (g.events[EV_GATE_SLAM] ? 0.08f : 0.006f));
        if (!g.flags[F_FINALE] && !g.events[EV_GATE_SLAM] && g.pstate == PS_NORMAL && g.pos.y < 0.6f && g.finaleT <= 0) {
            g.finaleT = 0.01f;
            schedule(g, 1.2f, TC_FINALE_SPAWN);
        }
    }
    if (g.stage == ST_OPEN && g.rig.state == RS_DORMANT && g.pos.y < 6.0f)
        rigger_spawn(g, Vec3(PAD.x + LOAD_L * 0.5f + 3.2f, 0, PAD.z + 1.9f), RS_STATUE);
}

void crane_timed(Game& g, int code, int arg) {
    World& w = g.w;
    (void)arg;
    switch (code) {
    case TC_RELEASE_A2:
        audio_play_at(S_CRANE_GROAN, CRANE_POS + Vec3(0, 30, 0), 1.0f, 0.45f, 60.0f);
        g.blackout = 1.0f;
        say(g, "", "The wire goes slack. Far down, something takes the weight, and keeps it.", 6.0f, 0.3f);
        say(g, "", "Every lamp on the crane begins to go out.", 4.5f, 0.3f);
        w.interacts[(size_t)w.cabLeaveInteract].enabled = true;
        break;
    case TC_RELEASE_B2:
        say(g, "", "It sits on the hatch marks as if it had always been there. The knocking has stopped.", 6.5f, 0.3f);
        w.interacts[(size_t)w.cabLeaveInteract].enabled = true;
        break;
    case TC_FINALE_SPAWN: {
        if (g.stage != ST_ESCAPE || g.events[EV_GATE_SLAM]) break;
        g.flags[F_FINALE] = true;
        Vec3 sp = nav_snap(w, Vec3(g.pos.x - 6.0f, 0, std::min(g.pos.z - 13.0f, -39.0f)), 10);
        if (distXZ(sp, g.pos) < 9.0f) sp = nav_snap(w, g.pos + Vec3(-14.0f, 0, -4.0f), 10);
        rigger_spawn(g, sp, RS_FINALE);
        audio_play_at(S_SCREAM, sp + Vec3(0, 1.5f, 0), 1.0f, 0.9f, 30.0f);
        audio_play(S_STINGER, 1.0f);
        g.shake = std::max(g.shake, 0.6f);
        g.fear = 1.0f;
        g.stamina = 1.0f; g.exhausted = false;
        say(g, "DISPATCH", "It's off the ladder. It's behind you. RUN, relief. The gate. RUN.", 6.0f);
        break;
    }
    case TC_ENDING:
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Endings
// ---------------------------------------------------------------------------
void ending_update(Game& g, const Controls& in, float dt) {
    if (g.appMode == AM_PLAY) {
        // fade out of the world
        g.endingT += dt;
        g.fadeTarget = 1.0f;
        g.fadeSpeed = 0.5f;
        if (g.ending == 2) g.crane.doorOpen = std::min(1.0f, g.crane.doorOpen + dt * 0.35f);
        if (g.endingT > 3.2f) {
            g.appMode = AM_ENDING;
            g.endingT = 0;
            g.endingLine = 0;
            g.fade = 1.0f;
            g.subs.clear();
            g.hint.clear(); g.hintT = 0;
            audio_stop_all(1.5f);
            g.hGen = g.hPhone = g.hRadio = g.hRigBreath = g.hTape = g.hWaterDrain = g.hPump = g.hCraneMotor = g.hChase = g.hBuzz = g.hSlosh = -1;
            audio_loop(S_END_THEME, 0.8f);
            if (g.ending == 2) audio_play(S_KNOCK, 0.0f);
            // the shift is over: there is nothing to continue
            g.saveData.clear();
            g.hasSave = false;
            g.wantSave = true;
        }
        return;
    }
    // text cards
    g.time += dt;
    g.endingT += dt;
    int count = 0;
    const EndLine* lines = ending_lines(g.ending, &count);
    AudioEnv e;
    e.wind = g.ending == 1 ? 0.15f : 0.5f;
    e.rain = g.ending == 1 ? 0.0f : 0.5f;
    e.indoor = 1.0f;
    e.drone = g.ending == 1 ? 0.05f : 0.5f;
    audio_env(e);
    if (g.endingLine < count) {
        float hold = lines[g.endingLine].hold;
        if (g.ending == 2 && g.endingLine == 5 && g.endingT - dt < 1.2f && g.endingT >= 1.2f) audio_play(S_KNOCK, 0.9f, 0.8f);
        if (g.endingT > hold || ((in.confirm || in.interact) && g.endingT > 1.0f)) {
            g.endingLine++;
            g.endingT = 0;
        }
    } else {
        // summary card: wait for the player
        if ((in.confirm || in.interact || in.back) && g.endingT > 1.5f) {
            audio_stop_all(1.0f);
            game_to_title(g, true);
        }
    }
}
