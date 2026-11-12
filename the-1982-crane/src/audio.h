// THE 1982 CRANE - procedural audio: every sound is synthesised, nothing is sampled.
#pragma once
#include "core.h"

enum Snd {
    S_STEP_ASPHALT = 0, S_STEP_CONCRETE, S_STEP_METAL, S_STEP_WOOD, S_STEP_GRATE, S_STEP_LINO, S_STEP_GRAVEL, S_STEP_WATER, // order = Surface
    S_LADDER, S_DOOR_OPEN, S_DOOR_CLOSE, S_DOOR_LOCKED, S_DOOR_METAL, S_LOCKER, S_SLAM, S_GATE, S_LEVER, S_SWITCH, S_BREAKER,
    S_PICKUP, S_PAPER, S_KEYS, S_FUSE, S_TAPE_CLICK, S_GEN_CRANK, S_GEN_LOOP, S_VALVE, S_DRAIN, S_PUMP_LOOP, S_CRANE_GROAN,
    S_CRANE_MOTOR, S_CHAIN, S_METAL_BANG, S_KNOCK, S_THUNDER, S_STINGER, S_SWELL, S_SCREAM, S_CHASE_LOOP, S_RIG_STEP, S_RIG_BREATH,
    S_RADIO_ON, S_RADIO_GARBLE, S_PHONE_RING, S_PHONE_PICK, S_HEART, S_BREATH, S_DEATH, S_BUZZ, S_HUM, S_FOGHORN, S_BELL,
    S_TAPE_HISS, S_RADIO_TUNE, S_TITLE, S_KEYPAD, S_ERROR, S_OK, S_POUR, S_CUT, S_WHISPER, S_VEND, S_TAP, S_HATFALL, S_CLANG,
    S_SPLASH, S_UI, S_DRIP, S_END_THEME,
    S_COUNT
};

// Continuous, procedurally generated beds. All values 0..1.
struct AudioEnv {
    float wind = 0.5f;
    float rain = 1.0f;
    float indoor = 0.0f;       // muffles weather, adds roof patter
    float sea = 0.0f;          // proximity to the quay edge
    float drone = 0.15f;       // low dread bed
    float interference = 0.0f; // radio squeal when it is near
    float heart = 0.0f;        // heartbeat rate/level
    float master = 1.0f;
    float duck = 0.0f;         // lowers the beds under dialogue / tapes
    float height = 0.0f;       // up the crane: more wind, less ground
};

int audio_init_steps();
void audio_init_step(int step, int sampleRate); // call for step = 0 .. steps-1
bool audio_ready();
// Fills `frames` stereo frames, interleaved L R.
void audio_mix(float* out, int frames);

int audio_play(int snd, float gain = 1.0f, float pitch = 1.0f, float pan = 0.0f);
int audio_play_at(int snd, const Vec3& pos, float gain = 1.0f, float pitch = 1.0f, float refDist = 5.0f);
int audio_loop(int snd, float gain, float pitch = 1.0f);
int audio_loop_at(int snd, const Vec3& pos, float gain, float refDist = 5.0f, float pitch = 1.0f);
void audio_stop(int handle, float fade = 0.25f);
void audio_set(int handle, float gain, float pitch = -1.0f);
void audio_move(int handle, const Vec3& pos);
bool audio_active(int handle);
void audio_listener(const Vec3& pos, float yaw);
void audio_env(const AudioEnv& e);
void audio_stop_all(float fade = 0.3f);
float audio_peak(); // recent output peak (diagnostics)
void audio_set_volume(float v); // user volume 0..1
