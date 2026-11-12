// THE 1982 CRANE - scripted events, weather, lights and the ambient soundscape
#include "game.h"
#include "audio.h"

static bool looking_at(const Game& g, const Vec3& target, float cosCone) {
    Vec3 d = normalize(target - g.camPos);
    return dot(d, game_cam_forward(g)) > cosCone;
}

static void force_lightning(Game& g, float strength) {
    g.lightningSeq = 3;
    g.lightningSeqT = 0;
    g.lightning = std::max(g.lightning, strength);
    g.thunderDelay = 0.5f + g.rng.f() * 0.8f;
    g.thunderGain = strength;
}

static void show_figure(Game& g, int dynId, const Vec3& pos, float yaw) {
    DynObj& d = g.w.dyn[(size_t)dynId];
    d.visible = true;
    d.xf = Mat4::translate(pos) * Mat4::rotY(yaw);
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------
static void stage_lines(Game& g, int stage) {
    switch (stage) {
    case -1:
        say(g, "", "Somebody was standing on top of the container stack.", 4.5f);
        say(g, "", "Nobody is standing on top of the container stack.", 4.5f, 0.5f);
        break;
    case 100:
        say(g, "", "Thursday, November 11, 1982.  11:47 p.m.", 4.5f);
        say(g, "", "Harrow Marine Salvage, Berth 9. The hall said: relief mechanic, one night, a crane that won't shut down.", 7.0f, 0.4f);
        say(g, "", "The gate is shut and the yard is dark. Your flashlight is on the hood.", 5.5f, 0.4f);
        break;
    case ST_BOOTH:
        say(g, "", "The guard booth is by the gate. Somebody should have left a log.", 4.5f);
        break;
    case ST_GATE:
        say(g, "DISPATCH", "Relief, this is dispatch. You're on the yard channel now.", 4.5f);
        say(g, "DISPATCH", "Gate has no power. There's a manual release in the booth. North wall.", 5.5f, 0.5f);
        break;
    case ST_OFFICE_GO:
        say(g, "DISPATCH", "Site office is the trailer up the road, on your right. The foreman will have left paperwork.", 6.5f);
        break;
    case ST_OFFICE_KEY:
        say(g, "", "Dale's locker. The crew room is the green building further up the road, on the right.", 5.5f);
        break;
    case ST_OFFICE_ENTER:
        say(g, "DISPATCH", "You find anybody yet?", 3.0f);
        say(g, "DISPATCH", "Day shift signed in seven on Monday. I have nobody signing out.", 5.5f, 0.6f);
        break;
    case ST_POWER:
        say(g, "DISPATCH", "Dale pulled the fuses? ...Then put them back. Three fuses and a can of diesel.", 6.5f);
        say(g, "DISPATCH", "Generator shed is east of the office. And relief - when the lamps come on, keep moving.", 7.0f, 0.6f);
        break;
    case ST_KEYCAB:
        say(g, "DISPATCH", "Lights are up. I can see the whole yard from h- ...never mind where from.", 6.0f, 2.0f);
        say(g, "DISPATCH", "Key cabinet is in the office. Kessler's badge number. His time card will have it.", 6.5f, 0.6f);
        break;
    case ST_BRAKE:
        say(g, "DISPATCH", "You have the key. Don't use it yet. That crane is still slewing.", 5.5f);
        say(g, "DISPATCH", "Lock it first. The brake pump is in Pump House Two, east end of the quay. The foreman's book has the rest.", 8.0f, 0.6f);
        break;
    case ST_CLIMB:
        say(g, "DISPATCH", "She's locked. ...It didn't like that.", 4.5f, 2.5f);
        say(g, "DISPATCH", "Go on up, relief. The ladder is on the south side. Don't look down.", 6.0f, 0.6f);
        break;
    case ST_CAB:
        say(g, "", "Walt's chair is still warm.", 4.0f, 1.0f);
        break;
    case ST_LOAD:
        say(g, "DISPATCH", "You're on the levers. Harbor or quay, relief. Your call. It's always your call.", 7.0f);
        break;
    case ST_ESCAPE:
        say(g, "DISPATCH", "It let go. Get down. Get to your truck.", 5.0f, 3.0f);
        say(g, "DISPATCH", "Don't stop for anything that calls your name.", 5.0f, 0.5f);
        break;
    case ST_OPEN:
        say(g, "DISPATCH", "...You landed it.", 4.0f, 3.0f);
        say(g, "DISPATCH", "Then go down and see. Everybody does.", 5.0f, 0.8f);
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Timed actions
// ---------------------------------------------------------------------------
void events_timed(Game& g, int code, int arg) {
    World& w = g.w;
    switch (code) {
    case TC_SAY_STAGE: stage_lines(g, arg); break;
    case TC_SAVE: game_checkpoint(g); break;
    case TC_CRANE_GROAN:
        audio_play_at(S_CRANE_GROAN, CRANE_POS + Vec3(0, 30, 0), 1.0f, 0.8f + g.rng.f() * 0.3f, 40.0f);
        break;
    case TC_FUEL_DONE:
        g.flags[F_CAN_FULL] = true;
        hint(g, "The can is full. It is heavier than diesel should be.", 4.0f);
        break;
    case TC_POUR_DONE:
        g.flags[F_GEN_FUELED] = true;
        g.flags[F_CAN_FULL] = false;
        hint(g, "The generator's tank is full.", 3.5f);
        schedule(g, 0.2f, TC_SAVE);
        break;
    case TC_GEN_CATCH:
        g.flags[F_GEN_RUNNING] = true;
        g.hGen = audio_loop_at(S_GEN_LOOP, Vec3(34.2f, 1.0f, 24.0f), 1.0f, 7.0f);
        g.shake = std::max(g.shake, 0.3f);
        for (int i = 0; i < 6; i++) schedule(g, 1.0f + i * 0.85f, TC_POWER_STEP, i);
        schedule(g, 26.0f, TC_RIGGER_RELEASE);
        say(g, "", "It catches. Across the yard the lamps come on, one bank at a time.", 5.5f, 1.2f);
        check_progress(g);
        break;
    case TC_POWER_STEP: {
        g.powerUp = std::min(1.0f, (arg + 1) / 6.0f);
        static const Vec3 at[6] = {Vec3(34, 4, 24), Vec3(13, 6, 12), Vec3(0, 7, 30), Vec3(-12, 7, 0), Vec3(-40, 7, -24), Vec3(40, 7, -28)};
        audio_play_at(S_BREAKER, at[arg % 6], 1.0f, 0.8f, 30.0f);
        if (arg == 5) {
            // the phone rings again; the crane notices the light
            audio_play_at(S_CRANE_GROAN, CRANE_POS + Vec3(0, 30, 0), 1.0f, 0.7f, 50.0f);
            g.crane.autoTimer = 0.5f;
        }
        break;
    }
    case TC_RIGGER_RELEASE: {
        if (g.flags[F_RIGGER_ACTIVE] || g.stage >= ST_CAB) break;
        g.flags[F_RIGGER_ACTIVE] = true;
        g.events[EV_RIGGER_RELEASE] = true;
        Vec3 sp = w.riggerSpawn;
        if (distXZ(sp, g.pos) < 26.0f) {
            float bd = -1;
            for (const Vec3& p : w.patrolPoints) {
                float d = distXZ(p, g.pos);
                if (d > 30.0f && d < 70.0f && d > bd) { bd = d; sp = p; }
            }
        }
        rigger_spawn(g, sp, RS_PATROL);
        g.rig.grace = 9.0f;
        g.blackout = 1.0f;
        audio_play(S_SWELL, 0.9f);
        audio_play_at(S_CLANG, CRANE_POS + Vec3(0, 12, 0), 1.0f, 0.8f, 40.0f);
        say(g, "DISPATCH", "Relief. The cab door just opened.", 4.0f, 1.0f);
        say(g, "DISPATCH", "Something is on the ladder. It isn't using the rungs.", 5.5f, 0.5f);
        say(g, "DISPATCH", "Stay out of the light. If it comes for you, get in a locker and shut the door.", 7.0f, 0.5f);
        schedule(g, 0.3f, TC_SAVE);
        break;
    }
    case TC_LOCKER_SLAM:
        if (g.events[EV_LOCKER_SLAM]) break;
        g.events[EV_LOCKER_SLAM] = true;
        audio_play_at(S_SLAM, w.hides[1].hinge + Vec3(0, 1, 0), 1.0f, 0.9f, 8.0f);
        g.hardhatT = 0.0f;
        g.shake = std::max(g.shake, 0.35f);
        g.fear = std::min(1.0f, g.fear + 0.4f);
        say(g, "", "A locker at the far end bangs shut. The hard hat on the shelf is rocking.", 5.5f, 1.2f);
        break;
    case TC_PHONE_START:
        if (g.flags[F_PHONE_ANSWERED] || g.phoneRingT >= 0) break;
        g.phoneRingT = 0;
        g.hPhone = audio_loop_at(S_PHONE_RING, w.usePos[U_PHONE], 1.0f, 5.0f);
        break;
    case TC_CRT:
        if (g.events[EV_CRT_ON]) break;
        g.events[EV_CRT_ON] = true;
        w.dyn[DY_CRT_OFFICE].visible = true;
        w.lights[(size_t)w.lightCrtOffice].on = true;
        audio_play_at(S_SWITCH, w.usePos[U_CRT], 0.9f, 0.6f);
        audio_play_at(S_BUZZ, w.usePos[U_CRT], 0.6f);
        say(g, "", "Behind you, the dead terminal switches itself on.", 4.5f, 0.5f);
        break;
    case TC_KNOCK_REPLY: {
        Vec3 c = w.usePos[U_KNOCK];
        if (arg == 0) {
            audio_play_at(S_KNOCK, c + Vec3(0, 0, -1.2f), 1.0f, 0.8f, 5.0f);
            g.fear = std::min(1.0f, g.fear + 0.35f);
            say(g, "", "Three knocks back. From inside. Polite.", 4.5f, 1.4f);
        } else {
            // every container answers
            for (int i = 0; i < 7; i++) {
                Vec3 p = g.pos + Vec3((g.rng.f() - 0.5f) * 40.0f, 1.0f, (g.rng.f() - 0.5f) * 40.0f);
                audio_play_at(S_KNOCK, p, 1.0f, 0.6f + g.rng.f() * 0.5f, 9.0f);
            }
            audio_play(S_STINGER, 0.6f, 0.8f);
            g.fear = 1.0f;
            g.shake = std::max(g.shake, 0.4f);
            alert_noise(g, g.pos, 60.0f);
            say(g, "", "All of them. Every box in the stacks. The rule said don't knock back.", 6.0f, 1.2f);
        }
        break;
    }
    case TC_FIGURE_HIDE:
        w.dyn[(size_t)arg].visible = false;
        break;
    case TC_LIGHTNING: force_lightning(g, arg / 100.0f); break;
    case TC_STINGER: audio_play(S_STINGER, arg / 100.0f); break;
    case TC_DRAIN_DONE:
        g.flags[F_DRAINED] = true;
        if (g.hWaterDrain >= 0) { audio_stop(g.hWaterDrain, 1.5f); g.hWaterDrain = -1; }
        hint(g, "The pump house is dry. The lockout on the starter is the only thing left.", 5.0f);
        schedule(g, 0.2f, TC_SAVE);
        break;
    case TC_BUMP:
        audio_play_at(S_METAL_BANG, Vec3(48, -1, -36), 1.0f, 0.5f, 10.0f);
        g.shake = std::max(g.shake, 0.5f);
        g.fear = std::min(1.0f, g.fear + 0.3f);
        if (g.zone == Z_PUMP) say(g, "", "Something large bumps the underside of the floor, once, on its way out with the water.", 6.0f, 0.6f);
        break;
    case TC_STORM_DONE:
        g.blackout = 0.0f;
        g.crane.shake = 0.0f;
        if (g.flags[F_RIGGER_ACTIVE] && g.rig.state != RS_DORMANT) {
            g.rig.state = RS_INVESTIGATE;
            g.rig.target = nav_snap(w, g.pos, 8);
            g.rig.stateTime = 0;
            g.rig.repath = 0;
        }
        break;
    case TC_RESPAWN: {
        static const char* lines[5] = {"11:47 p.m.   Again.", "You went up.", "It does not use the rungs.", "The lamps were on.", "Relief mech. signed in."};
        std::string save = g.saveData;
        int deaths = g.deaths;
        if (!game_load(g, save)) game_new(g, g.difficulty);
        g.deaths = deaths;
        g.flags[F_DIED_ONCE] = true;
        g.w.dyn[DY_CHALK_AGAIN].visible = true;
        say(g, "", lines[(deaths - 1 + 5) % 5], 4.0f, 0.6f);
        break;
    }
    case TC_HELMET: {
        Vec3 hp(25.45f, 0, -56.0f);
        g.helmetTarget = std::atan2(g.pos.x - hp.x, g.pos.z - hp.z);
        audio_play_at(S_VALVE, hp, 0.4f, 0.6f);
        break;
    }
    case TC_CLANG:
        audio_play_at(S_CLANG, CRANE_POS + Vec3(0, (float)arg, 0), 1.0f, 0.9f + g.rng.f() * 0.2f, 14.0f);
        break;
    case TC_VEND:
        w.dyn[(size_t)w.itemDyn[IT_BATT8]].visible = true;
        w.interacts[(size_t)w.itemInteract[IT_BATT8]].enabled = true;
        hint(g, "It takes nothing and gives something. A pack of batteries drops into the tray. They are dry.", 5.5f);
        break;
    case TC_KEYCAB_OPEN:
        g.flags[F_KEYCAB_OPEN] = true;
        w.dyn[DY_KEYCAB_DOOR].visible = false;
        for (int it : {IT_CRANE_KEY, IT_TAPE1})
            if (!g.itemTaken[it]) {
                w.dyn[(size_t)w.itemDyn[it]].visible = true;
                w.interacts[(size_t)w.itemInteract[it]].enabled = true;
            }
        audio_play_at(S_LOCKER, w.usePos[U_KEYPAD], 0.8f);
        hint(g, "The lock lets go. One key on the hooks, and a cassette.", 4.5f);
        schedule(g, 0.2f, TC_SAVE);
        break;
    case TC_GATE_SLAM:
        w.setColliderEnabled(w.gateCollider, true);
        break;
    default:
        // crane / ending codes live in game_crane.cpp
        extern void crane_timed(Game& g, int code, int arg);
        crane_timed(g, code, arg);
        break;
    }
}

// ---------------------------------------------------------------------------
// Trigger volumes
// ---------------------------------------------------------------------------
static void maze_figures(Game& g) {
    // five of them, standing in the aisle the player is not looking down
    World& w = g.w;
    Vec3 f = game_cam_forward(g);
    static const float dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int order[4] = {0, 1, 2, 3};
    std::sort(order, order + 4, [&](int a, int b) { return dirs[a][0] * f.x + dirs[a][1] * f.z < dirs[b][0] * f.x + dirs[b][1] * f.z; });
    for (int oi = 0; oi < 4; oi++) {
        Vec3 d(dirs[order[oi]][0], 0, dirs[order[oi]][1]);
        Vec3 side(d.z, 0, -d.x);
        Vec3 c = g.pos + d * 11.0f;
        bool ok = nav_walkable(w, c) && nav_walkable(w, c + side * 0.9f) && nav_walkable(w, c - side * 0.9f) &&
                  w.lineOfSight(g.pos + Vec3(0, 1.5f, 0), c + Vec3(0, 1.5f, 0));
        if (!ok) continue;
        for (int i = 0; i < 5; i++) {
            Vec3 p = c + side * ((i - 2) * 0.55f) + d * ((i & 1) ? 0.7f : 0.0f);
            g.mazeFigPos[i] = p;
        }
        g.mazeFigYaw = std::atan2(-d.x, -d.z);
        g.mazeFigT = 0;
        g.mazeFigStage = 0;
        force_lightning(g, 1.0f);
        audio_play(S_STINGER, 0.7f, 0.9f);
        g.fear = 1.0f;
        return;
    }
    g.events[EV_MAZE_FIGURES] = false; // no room here: try again at the next opportunity
}

void events_trigger(Game& g, int id) {
    World& w = g.w;
    auto once = [&](int ev) {
        if (g.events[ev]) return false;
        g.events[ev] = true;
        return true;
    };
    switch (id) {
    case TR_WATER: kill_player(g, 1); break;
    case TR_YARD_ENTER:
        if (g.flags[F_GATE_OPEN] && once(EV_CRANE_FIRST_MOVE)) {
            audio_play_at(S_CRANE_GROAN, CRANE_POS + Vec3(0, 30, 0), 1.0f, 0.85f, 60.0f);
            audio_play(S_SWELL, 0.5f);
            g.crane.autoTarget = g.crane.slew - 0.9f;
            g.crane.autoTimer = 14.0f;
            g.fear = std::min(1.0f, g.fear + 0.4f);
            say(g, "DISPATCH", "...Relief. Did it just move?", 3.5f, 2.5f);
            say(g, "DISPATCH", "There's nobody in that cab. There is nobody supposed to be in that cab.", 6.0f, 0.5f);
        }
        break;
    case TR_ROAD_MID:
        if (once(EV_ROAD_MID)) {
            audio_play_at(S_METAL_BANG, Vec3(-40, 3, 15), 1.0f, 0.6f, 25.0f);
            say(g, "", "Something heavy shifts, deep in the container stacks.", 4.5f, 1.2f);
        }
        break;
    case TR_OFFICE_APPROACH:
        if (g.stage >= ST_OFFICE_GO && once(EV_FIGURE_STACK)) {
            show_figure(g, DY_FIG0, Vec3(-17.0f, 5.18f, 37.2f), PI * 0.5f);
            g.figureT = 0;
            force_lightning(g, 1.0f);
            schedule(g, 5.0f, TC_SAY_STAGE, -1);
        }
        break;
    case TR_CREWROOM:
        if (once(EV_CREW_ROOM)) {
            audio_play_at(S_DRIP, Vec3(18.3f, 0.5f, -9.6f), 0.9f);
            say(g, "", "Seven lockers with names. An eighth with fresh tape on it.", 5.0f, 1.0f);
        }
        break;
    case TR_WORKSHOP:
        if (once(EV_WORKSHOP)) audio_play_at(S_CHAIN, Vec3(36, 3, 0.5f), 0.9f);
        break;
    case TR_MAZE_ENTER:
        if (!g.triggerInside[TR_MAZE_DEEP]) audio_play_at(S_METAL_BANG, g.pos + Vec3(-18, 4, (g.rng.f() - 0.5f) * 20), 0.7f, 0.7f, 20.0f);
        break;
    case TR_MAZE_DEEP:
        if (!g.flags[F_RIGGER_ACTIVE] && once(EV_MAZE_FIGURES)) maze_figures(g);
        break;
    case TR_KNOCK:
        if (once(EV_KNOCK)) {
            audio_play_at(S_KNOCK, w.usePos[U_KNOCK] + Vec3(0, 0, -1.2f), 1.0f, 0.8f, 5.0f);
            say(g, "", "Three knocks. From inside the container.", 4.5f, 1.6f);
            g.fear = std::min(1.0f, g.fear + 0.3f);
        }
        break;
    case TR_CAMP:
        if (once(EV_CAMP_FIGURE)) say(g, "", "Bedrolls. Tins. A lantern still burning. They were living in here.", 5.5f, 0.6f);
        break;
    case TR_QUAY:
        if (once(EV_QUAY_FIRST)) {
            audio_play_at(S_FOGHORN, Vec3(60, 5, -300), 1.0f, 1.0f, 200.0f);
            say(g, "", "The load turns slowly on its wire. Water is still running out of it.", 5.5f, 1.0f);
        }
        break;
    case TR_LIFTS:
        if (once(EV_LIFTS)) say(g, "", "A row of things that came up out of the harbor, tagged like evidence.", 5.5f, 0.8f);
        break;
    case TR_LIFT3_DOOR:
        if (g.flags[F_GEN_RUNNING] && !g.flags[F_LIFT3_OPEN] && once(EV_LIFT3)) {
            g.flags[F_LIFT3_OPEN] = true;
            Door& d = w.doors[D_LIFT3];
            d.target = d.openAngle * 0.35f;
            w.dyn[DY_BOOTPRINTS].visible = true;
            audio_play_at(S_DOOR_OPEN, w.doorPos[D_LIFT3], 0.9f, 0.5f);
            g.fear = std::min(1.0f, g.fear + 0.4f);
            say(g, "", "The door is open. Wet bootprints lead out of it, onto the quay.", 5.5f, 1.5f);
            say(g, "", "There is nothing behind a door that stands in the open air.", 5.0f, 0.4f);
        }
        break;
    case TR_BARGE:
        if (once(EV_BARGE)) say(g, "", "The barge rides low and quiet. Out here, over the water, it feels like nothing can follow.", 6.0f, 1.0f);
        break;
    case TR_CRANE_BASE:
        if (once(EV_CRANE_BASE)) {
            audio_play_at(S_CRANE_GROAN, CRANE_POS + Vec3(0, 25, 0), 0.9f, 1.0f, 40.0f);
            say(g, "", "Crane No. 7. The steel is wet higher than any tide comes.", 5.0f, 0.8f);
        }
        break;
    case TR_PORTAL_DECK:
        if (once(EV_DECK)) say(g, "", "Weed on the handrails. It smells like low tide, seven metres up.", 5.0f, 0.6f);
        break;
    case TR_MAST_MID:
        if (once(EV_CLIMB_CLANG)) {
            for (int i = 0; i < 5; i++) schedule(g, 1.0f + i * 0.55f, TC_CLANG, 4 + i * 2);
            say(g, "", "Something is on the ladder below you.", 4.0f, 3.2f);
            g.lookDownArm = 25.0f;
            g.fear = std::min(1.0f, g.fear + 0.4f);
        }
        break;
    case TR_MAST_TOP:
        if (once(EV_TOP)) say(g, "", "The cab door is on the east side. There is a light on inside, and the shape of a chair.", 6.0f, 0.6f);
        break;
    case TR_PUMP:
        if (!g.flags[F_DRAINED] && once(EV_PUMP_BUMP)) say(g, "", "Flooded. Shin deep, black, and colder than the harbor.", 5.0f, 0.5f);
        break;
    case TR_GATE_EXIT:
        if (g.stage == ST_ESCAPE && g.flags[F_FINALE] && g.pos.z > 56.5f && once(EV_GATE_SLAM)) {
            g.flags[F_GATE_OPEN] = false;
            g.gateT = 0.0f;
            w.setColliderEnabled(w.gateCollider, true);
            audio_play_at(S_SLAM, Vec3(0, 1.5f, 56), 1.0f, 0.7f, 12.0f);
            audio_play_at(S_GATE, Vec3(0, 1.5f, 56), 0.9f, 1.6f, 12.0f);
            g.shake = std::max(g.shake, 0.6f);
            g.flags[F_FINALE] = false;
            g.rig.state = RS_STATUE;
            g.rig.pos = Vec3(clampv(g.rig.pos.x, -3.0f, 3.0f), 0, 54.6f);
            g.rig.yaw = 0;
            say(g, "", "The gate slams across behind you. It stops at the rail and stands there with its hand on the mesh.", 7.0f, 0.5f);
            say(g, "", "Your truck. Go.", 3.5f, 0.3f);
        }
        break;
    default: break;
    }
}

void events_update(Game& g, float dt) {
    World& w = g.w;
    Vec3 pp = g.pos + Vec3(0, 0.6f, 0);
    for (const Trigger& t : w.triggers) {
        bool in = t.box.contains(pp);
        if (in && !g.triggerInside[t.id]) {
            g.triggerInside[t.id] = true;
            events_trigger(g, t.id);
            if (g.pstate == PS_DEAD) return;
        } else if (!in) g.triggerInside[t.id] = false;
    }
    // the figure on the stack: only there while the lightning lasts
    if (g.figureT >= 0) {
        g.figureT += dt;
        if (g.figureT > 1.1f && g.lightning < 0.1f) {
            w.dyn[DY_FIG0].visible = false;
            g.figureT = -1;
        }
    }
    // figures in the stacks: three flashes, each one closer
    if (g.mazeFigT >= 0) {
        g.mazeFigT += dt;
        bool lit = g.lightning > 0.12f;
        float adv = g.mazeFigStage * 3.2f;
        Vec3 toP = normalize(Vec3(g.pos.x - g.mazeFigPos[2].x, 0, g.pos.z - g.mazeFigPos[2].z));
        for (int i = 0; i < 5; i++) {
            DynObj& d = w.dyn[DY_FIG1 + i];
            d.visible = lit && g.mazeFigStage < 3;
            d.xf = Mat4::translate(g.mazeFigPos[i] + toP * adv) * Mat4::rotY(g.mazeFigYaw);
        }
        if (g.mazeFigStage == 0 && g.mazeFigT > 2.6f) { g.mazeFigStage = 1; force_lightning(g, 0.9f); }
        if (g.mazeFigStage == 1 && g.mazeFigT > 5.0f) { g.mazeFigStage = 2; force_lightning(g, 1.0f); audio_play(S_STINGER, 0.5f, 1.1f); }
        if (g.mazeFigStage == 2 && g.mazeFigT > 7.2f) {
            g.mazeFigStage = 3;
            for (int i = 0; i < 5; i++) w.dyn[DY_FIG1 + i].visible = false;
            g.mazeFigT = -1;
            say(g, "", "Five men in hard hats, standing in the aisle. Then four metres nearer. Then nobody.", 6.5f, 0.8f);
        }
    }
    // the figure at the camp container door
    if (g.campFigT < 0 && !g.flags[F_CAMP_SEEN] && (g.itemTaken[IT_TAPE2] || g.noteRead[N_CAMP]) && g.triggerInside[TR_CAMP]) {
        g.flags[F_CAMP_SEEN] = true;
        g.campFigT = 0;
        show_figure(g, DY_CAMP_FIGURE, Vec3(-49.9f, 0, 33.0f), PI * 0.5f);
        audio_play(S_SWELL, 0.5f, 1.2f);
    }
    if (g.campFigT >= 0) {
        g.campFigT += dt;
        Vec3 fp(-49.9f, 1.4f, 33.0f);
        float d = length(fp - g.camPos);
        bool seen = looking_at(g, fp, 0.8f);
        if (seen && g.campFigT > 0.3f && g.campFigT < 50.0f) { g.campFigT = 50.0f; g.fear = 1.0f; audio_play(S_STINGER, 0.55f, 0.85f); }
        if (d < 4.2f || (g.campFigT > 50.0f && !seen) || g.campFigT > 58.0f || (g.campFigT > 20.0f && g.campFigT < 50.0f)) {
            w.dyn[DY_CAMP_FIGURE].visible = false;
            g.campFigT = -1;
        }
    }
    // looking up at the cab from the ground. He waves.
    if (!g.events[EV_TOP] && g.pos.y < 3.0f && g.stage >= ST_GATE && g.stage < ST_CLIMB && g.flags[F_GATE_OPEN]) {
        Vec3 cab = w.dyn[DY_CRANE_TOP].xf.point(Vec3(3.8f, 32.6f, 0));
        if (looking_at(g, cab, 0.985f) && distXZ(g.pos, CRANE_POS) < 60.0f && w.lineOfSight(g.camPos, cab, COL_SIGHT)) {
            g.lookUpT += dt;
            if (g.lookUpT > 1.6f && g.lookedAtRigger <= 0) {
                g.lookedAtRigger = 90.0f;
                g.fear = std::min(1.0f, g.fear + 0.5f);
                audio_play(S_SWELL, 0.6f, 0.9f);
                w.lights[(size_t)w.lightCab].boost = 2.5f;
                say(g, "", "Somebody in the cab lifts an arm. Slowly. Like a man signalling a lift.", 6.0f, 0.2f);
                say(g, "", "Dale's note said not to look up.", 4.0f, 0.4f);
                if (g.flags[F_RIGGER_ACTIVE] && g.rig.state == RS_PATROL) {
                    g.rig.state = RS_INVESTIGATE;
                    g.rig.target = nav_snap(w, g.pos, 8);
                    g.rig.repath = 0;
                    g.rig.stateTime = 0;
                }
            }
        } else g.lookUpT = std::max(0.0f, g.lookUpT - dt * 2.0f);
    }
    if (g.lookedAtRigger > 0) {
        g.lookedAtRigger -= dt;
        Light& l = w.lights[(size_t)w.lightCab];
        l.boost = dampf(l.boost, 1.0f, 0.6f, dt);
    }
    // looking down the mast after the clanging
    if (g.lookDownArm > 0) {
        g.lookDownArm -= dt;
        if (g.pos.y > 12.0f && g.camPitch < -1.05f && !g.events[EV_MAST_LOOKDOWN] && g.pstate == PS_NORMAL) {
            g.events[EV_MAST_LOOKDOWN] = true;
            float fy = g.pos.y > 24.0f ? 19.0f : (g.pos.y > 18.0f ? 13.0f : 7.2f);
            show_figure(g, DY_FIG0, Vec3(CRANE_POS.x + 0.3f, fy, CRANE_POS.z + 0.2f), 0.0f);
            g.figureT = 0;
            force_lightning(g, 1.0f);
            audio_play(S_STINGER, 0.8f);
            g.fear = 1.0f;
            say(g, "", "He is standing on the landing below, face turned up. He was not climbing. He was waiting for you to look.", 7.5f, 1.4f);
        }
    }
    // office phone: starts once the player is inside for the first time
    if (g.zone == Z_OFFICE && g.stage >= ST_OFFICE_ENTER && !g.events[EV_PHONE_RING]) {
        g.events[EV_PHONE_RING] = true;
        schedule(g, 4.0f, TC_PHONE_START);
    }
    if (g.phoneRingT >= 0) {
        g.phoneRingT += dt;
        if (g.phoneRingT > 75.0f) { // it gives up eventually
            g.phoneRingT = -1;
            if (g.hPhone >= 0) audio_stop(g.hPhone, 0.1f);
            g.hPhone = -1;
        }
    }
}

// ---------------------------------------------------------------------------
// Weather, lights, soundscape
// ---------------------------------------------------------------------------
static bool zone_is_indoor(int z) {
    return z == Z_BOOTH || z == Z_OFFICE || z == Z_CREW || z == Z_SHOP || z == Z_GEN || z == Z_PUMP || z == Z_CONTAINER || z == Z_BARGE_CABIN || z == Z_CAB;
}

void env_update(Game& g, float dt) {
    World& w = g.w;
    float t = (float)g.time;
    // --- where are we ---
    int z = w.zoneAt(g.camPos);
    if (g.pstate == PS_SEATED || g.pstate == PS_OPERATING) z = Z_CAB;
    g.zone = z;
    float inT = zone_is_indoor(z) ? 1.0f : (z == Z_UNDER_PORTAL ? 0.35f : 0.0f);
    if (g.pstate == PS_HIDING) inT = 1.0f;
    g.indoor = dampf(g.indoor, inT, 3.0f, dt);

    // --- lightning ---
    g.lightning = std::max(0.0f, g.lightning - dt * (g.lightning > 0.5f ? 5.0f : 2.2f));
    g.lightningTimer -= dt;
    if (g.lightningTimer <= 0) {
        g.lightningTimer = 9.0f + g.rng.f() * 26.0f;
        g.lightningSeq = 2 + (int)(g.rng.next() % 3u);
        g.lightningSeqT = 0;
        float close = g.rng.f();
        g.thunderDelay = 0.4f + (1.0f - close) * 3.2f;
        g.thunderGain = 0.45f + 0.55f * close;
    }
    if (g.lightningSeq > 0) {
        g.lightningSeqT -= dt;
        if (g.lightningSeqT <= 0) {
            g.lightning = std::max(g.lightning, 0.45f + 0.55f * g.rng.f());
            g.lightningSeqT = 0.06f + g.rng.f() * 0.22f;
            g.lightningSeq--;
        }
    }
    if (g.thunderDelay >= 0) {
        g.thunderDelay -= dt;
        if (g.thunderDelay < 0) {
            audio_play(S_THUNDER, g.thunderGain * (1.0f - 0.45f * g.indoor), 0.8f + g.rng.f() * 0.4f, (g.rng.f() - 0.5f) * 0.8f);
            g.shake = std::max(g.shake, 0.12f * g.thunderGain);
        }
    }
    g.wind = 0.55f + 0.25f * std::sin(t * 0.13f) + 0.2f * std::sin(t * 0.31f + 1.3f);
    g.rain = g.dawn > 0 ? std::max(0.0f, 1.0f - g.dawn * 1.3f) : 1.0f;

    // --- lights ---
    g.blackout = std::max(0.0f, g.blackout - dt * 0.22f);
    bool rigNear = g.rig.state != RS_DORMANT && g.rig.state != RS_STATUE;
    for (size_t i = 0; i < w.lights.size(); i++) {
        Light& l = w.lights[i];
        float v = l.on ? 1.0f : 0.0f;
        float thr = 0.08f + 0.86f * fractf(l.phase * 0.137f);
        if (l.group == LG_YARD || l.group == LG_INDOOR) {
            v *= g.powerUp >= thr ? 1.0f : 0.0f;
            v *= 1.0f - 0.85f * saturate(g.blackout * 1.4f) * (0.5f + 0.5f * hash2f((int)(t * 14.0f), (int)i));
        } else if (l.group == LG_CRANE) {
            if (g.crane.loadReleased && g.flags[F_RELEASED_WATER]) v *= std::max(0.0f, 1.0f - g.crane.sink * 0.12f);
        }
        float fl = 1.0f;
        switch (l.flicker) {
        case FL_BUZZ: fl = 0.93f + 0.07f * hash2f((int)(t * 31.0f), (int)i * 7); break;
        case FL_DYING: {
            float n = hash2f((int)(t * 11.0f + l.phase), (int)i * 13);
            float slow = hash2f((int)(t * 0.35f + l.phase), (int)i * 5);
            fl = (n < 0.14f) ? 0.12f : 1.0f;
            if (slow < 0.12f) fl *= 0.05f;
            break;
        }
        case FL_PULSE: fl = 0.82f + 0.1f * std::sin(t * 7.3f + l.phase) + 0.08f * std::sin(t * 12.7f + l.phase * 2.0f); break;
        case FL_BEACON: {
            float ph = fractf(t / 1.7f + l.phase * 0.01f);
            fl = smoothstepf(0.0f, 0.08f, ph) * smoothstepf(0.34f, 0.2f, ph);
            break;
        }
        default: break;
        }
        // it eats the light
        if (rigNear && l.group != LG_CRANE) {
            float d = length(l.pos - (g.rig.pos + Vec3(0, 1.5f, 0)));
            if (d < 15.0f && hash2f((int)(t * 19.0f), (int)i * 3) < (1.0f - d / 15.0f) * 0.85f) fl *= 0.08f;
        }
        l.cur = v * fl * l.boost;
    }
    // --- emissive groups ---
    float yard = g.powerUp * (1.0f - 0.8f * saturate(g.blackout));
    g.emis[EG_NONE] = 0;
    g.emis[EG_ALWAYS] = 1.0f;
    g.emis[EG_YARD] = yard * (0.94f + 0.06f * hash2f((int)(t * 30.0f), 1));
    g.emis[EG_INDOOR] = yard * (0.92f + 0.08f * hash2f((int)(t * 27.0f), 2));
    float craneLit = 1.0f;
    if (g.crane.loadReleased && g.flags[F_RELEASED_WATER]) craneLit = std::max(0.0f, 1.0f - g.crane.sink * 0.12f);
    g.emis[EG_CRANE] = craneLit * (0.9f + 0.1f * hash2f((int)(t * 23.0f), 3));
    g.emis[EG_CRT] = g.events[EV_CRT_ON] ? 0.85f + 0.15f * hash2f((int)(t * 50.0f), 4) : 0.0f;
    {
        float ph = fractf(t / 1.7f);
        g.emis[EG_BEACON] = craneLit * smoothstepf(0.0f, 0.08f, ph) * smoothstepf(0.34f, 0.2f, ph);
    }
    g.emis[EG_SPECIAL] = 0.6f + 0.4f * hash2f((int)(t * 9.0f), 5);

    // --- fear / presentation ---
    float rd = rigNear ? length(g.rig.pos - g.pos) : 999.0f;
    float near = rigNear ? saturate(1.0f - rd / 20.0f) : 0.0f;
    float fearT = std::max(near, g.rig.state == RS_CHASE || g.rig.state == RS_FINALE ? 0.85f : 0.0f);
    if (g.pstate == PS_OPERATING) fearT = std::max(fearT, g.crane.climb * 0.9f);
    if (fearT > g.fear) g.fear = dampf(g.fear, fearT, 2.5f, dt);
    else g.fear = std::max(fearT, g.fear - dt * 0.035f);
    float interT = rigNear ? saturate(1.0f - rd / 14.0f) : 0.0f;
    if (g.pstate == PS_OPERATING) interT = std::max(interT, saturate((g.crane.climb - 0.6f) * 2.5f));
    if (g.pstate == PS_DEAD) interT = 1.0f;
    g.interference = dampf(g.interference, interT, 5.0f, dt);

    // --- ambient one-shots ---
    g.ambientTimer -= dt;
    if (g.ambientTimer <= 0 && g.appMode == AM_PLAY) {
        g.ambientTimer = 14.0f + g.rng.f() * 26.0f;
        float a = g.rng.f() * TAU, r = 28.0f + g.rng.f() * 40.0f;
        Vec3 p = g.pos + Vec3(std::cos(a) * r, 2.0f + g.rng.f() * 8.0f, std::sin(a) * r);
        int k = (int)(g.rng.next() % 6u);
        float od = 1.0f - 0.5f * g.indoor;
        switch (k) {
        case 0: audio_play_at(S_METAL_BANG, p, 0.8f * od, 0.6f + g.rng.f() * 0.5f, 22.0f); break;
        case 1: audio_play_at(S_CHAIN, p, 0.8f * od, 0.8f + g.rng.f() * 0.3f, 16.0f); break;
        case 2: audio_play_at(S_CRANE_GROAN, CRANE_POS + Vec3(0, 30, 0), 0.8f * od, 0.8f + g.rng.f() * 0.4f, 45.0f); break;
        case 3: audio_play_at(S_FOGHORN, Vec3(g.rng.f() * 200 - 100, 5, -320), 0.9f * od, 0.95f + g.rng.f() * 0.1f, 220.0f); break;
        case 4: audio_play_at(S_BELL, Vec3(-5, 0, -110), 0.9f * od, 1.0f, 60.0f); break;
        default: audio_play_at(S_DRIP, g.pos + Vec3(std::cos(a) * 4.0f, 2.0f, std::sin(a) * 4.0f), 0.6f); break;
        }
    }

    // --- continuous beds ---
    AudioEnv e;
    e.wind = g.wind;
    e.rain = g.rain;
    e.indoor = g.indoor;
    e.sea = saturate(1.0f - (g.camPos.z - QUAY_Z) / 34.0f) * (1.0f - 0.6f * g.indoor);
    e.drone = 0.16f + 0.5f * g.fear + (g.appMode == AM_TITLE ? 0.1f : 0.0f);
    e.interference = g.interference;
    e.heart = saturate(g.fear * 1.15f - 0.25f) + (g.exhausted ? 0.4f : 0.0f);
    e.duck = (!g.subs.empty() && g.subs.front().delay <= 0 && g.subs.front().speaker != "") ? 0.5f : 0.0f;
    e.height = saturate((g.camPos.y - 6.0f) / 26.0f);
    e.master = (g.pstate == PS_DEAD) ? saturate(1.0f - (g.deadT - 2.0f) * 0.5f) : 1.0f;
    audio_env(e);
    // looping machinery follows its state
    if (g.flags[F_GEN_RUNNING] && g.hGen < 0) g.hGen = audio_loop_at(S_GEN_LOOP, Vec3(34.2f, 1.0f, 24.0f), 1.0f, 7.0f);
    if (g.flags[F_PUMP_ON] && g.hPump < 0) g.hPump = audio_loop_at(S_PUMP_LOOP, Vec3(50.5f, 1, -36), 0.8f, 6.0f);
    if (g.hBuzz < 0 && g.flags[F_GEN_RUNNING]) g.hBuzz = audio_loop_at(S_HUM, Vec3(45, 1.5f, 23), 0.8f, 5.0f);
    if (g.hSlosh < 0) g.hSlosh = audio_loop_at(S_HUM, CRANE_POS + Vec3(0, 3, 0), 0.5f, 6.0f, 0.5f);
}

// ---------------------------------------------------------------------------
// Title screen: the yard seen from the road, in the storm
// ---------------------------------------------------------------------------
void game_title_update(Game& g, float dt) {
    g.time += dt;
    g.titleT += dt;
    float t = g.titleT;
    Controls none;
    crane_update(g, none, dt);
    g.pos = Vec3(2.6f, 0, 76.5f);
    g.camPos = Vec3(2.6f + 0.5f * std::sin(t * 0.07f), 2.0f + 0.12f * std::sin(t * 0.11f), 76.5f);
    g.camYaw = -0.035f + 0.03f * std::sin(t * 0.05f);
    g.camPitch = 0.2f + 0.02f * std::sin(t * 0.08f);
    g.camRoll = 0.004f * std::sin(t * 0.3f);
    g.camFov = 1.05f;
    g.flashOn = false;
    audio_listener(g.camPos, g.camYaw);
    env_update(g, dt);
    g.fade = approachf(g.fade, g.fadeTarget, dt * g.fadeSpeed);
    g.flash = 0;
}
