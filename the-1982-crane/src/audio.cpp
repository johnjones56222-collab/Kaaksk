// THE 1982 CRANE - audio. Everything is synthesised at start-up or generated live:
// no samples, no files.
#include "audio.h"

// ---------------------------------------------------------------------------
// Synthesis toolkit
// ---------------------------------------------------------------------------
namespace {

const int SR = 32000;   // rate of the stored sounds
u32 g_ns = 0x1982ABCDu; // noise state
inline float nz() {
    g_ns = g_ns * 1664525u + 1013904223u;
    return (float)((int)(g_ns >> 8) & 0xFFFF) * (1.0f / 32768.0f) - 1.0f;
}
inline float urand() { return nz() * 0.5f + 0.5f; }

struct LP1 {
    float y = 0, a = 0.1f;
    void set(float fc, float sr) { a = 1.0f - std::exp(-TAU * fc / sr); }
    float run(float x) { y += a * (x - y); return y; }
};
struct HP1 {
    LP1 lp;
    void set(float fc, float sr) { lp.set(fc, sr); }
    float run(float x) { return x - lp.run(x); }
};
struct BQ {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void bp(float f, float q, float sr) {
        float w = TAU * clampv(f, 20.0f, sr * 0.45f) / sr, al = std::sin(w) / (2.0f * q), c = std::cos(w), a0 = 1.0f + al;
        b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2.0f * c / a0; a2 = (1.0f - al) / a0;
    }
    void lp(float f, float q, float sr) {
        float w = TAU * clampv(f, 20.0f, sr * 0.45f) / sr, al = std::sin(w) / (2.0f * q), c = std::cos(w), a0 = 1.0f + al;
        b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0; b2 = b0; a1 = -2.0f * c / a0; a2 = (1.0f - al) / a0;
    }
    float run(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

typedef std::vector<float> Buf;
inline Buf mk(float sec, int sr = SR) { return Buf((size_t)(sec * sr), 0.0f); }
inline float sat(float x) { return x / (1.0f + std::fabs(x)); }

// sum of exponentially damped sines struck at `at` seconds
void modal(Buf& b, float at, const float* f, const float* amp, const float* dec, int n, float gain, int sr = SR, float detune = 0.0f) {
    size_t s0 = (size_t)(at * sr);
    for (int k = 0; k < n; k++) {
        float fr = f[k] * (1.0f + detune * nz());
        float w = TAU * fr / sr, ph = urand() * TAU;
        float d = std::exp(-1.0f / (dec[k] * sr));
        float a = amp[k] * gain;
        for (size_t i = s0; i < b.size() && a > 1e-4f; i++) {
            b[i] += std::sin(ph) * a;
            ph += w;
            a *= d;
        }
    }
}
// filtered noise burst
void burst(Buf& b, float at, float dur, float fc, float q, float gain, float decay, int sr = SR) {
    BQ f;
    f.bp(fc, q, (float)sr);
    size_t s0 = (size_t)(at * sr), n = (size_t)(dur * sr);
    for (size_t i = 0; i < n && s0 + i < b.size(); i++) {
        float t = (float)i / sr;
        float e = std::exp(-t / decay) * std::min(1.0f, t * 800.0f);
        b[s0 + i] += f.run(nz()) * e * gain;
    }
}
// low thump (pitch-dropping sine)
void thump(Buf& b, float at, float f0, float f1, float dur, float gain, int sr = SR) {
    size_t s0 = (size_t)(at * sr), n = (size_t)(dur * sr);
    float ph = 0;
    for (size_t i = 0; i < n && s0 + i < b.size(); i++) {
        float t = (float)i / (float)n;
        float f = lerpf(f0, f1, t);
        ph += TAU * f / sr;
        b[s0 + i] += std::sin(ph) * (1.0f - t) * (1.0f - t) * gain * std::min(1.0f, (float)i / 30.0f);
    }
}
void normalize(Buf& b, float peak = 0.9f) {
    float m = 1e-6f;
    for (float v : b) m = std::max(m, std::fabs(v));
    float k = peak / m;
    for (float& v : b) v *= k;
}
void fadeEnds(Buf& b, int n = 200) {
    for (int i = 0; i < n && (size_t)i < b.size(); i++) {
        float k = (float)i / n;
        b[(size_t)i] *= k;
        b[b.size() - 1 - (size_t)i] *= k;
    }
}
// make a loop seamless by cross-fading its tail into its head
void seamless(Buf& b, int xf) {
    if ((int)b.size() < xf * 3) return;
    size_t n = b.size() - (size_t)xf;
    for (int i = 0; i < xf; i++) {
        float k = (float)i / xf;
        b[(size_t)i] = b[(size_t)i] * k + b[n + (size_t)i] * (1.0f - k);
    }
    b.resize(n);
}
void lowpass(Buf& b, float fc, int sr = SR) {
    LP1 f;
    f.set(fc, (float)sr);
    for (float& v : b) v = f.run(v);
}
void highpass(Buf& b, float fc, int sr = SR) {
    HP1 f;
    f.set(fc, (float)sr);
    for (float& v : b) v = f.run(v);
}
// cheap reverb tail baked into one-shots that need to sound far away
void bakeEcho(Buf& b, float delay, float fb, float mix, int sr = SR) {
    size_t d = (size_t)(delay * sr);
    if (d == 0) return;
    Buf w = b;
    LP1 lp;
    lp.set(1800.0f, (float)sr);
    for (size_t i = d; i < w.size(); i++) w[i] += lp.run(w[i - d]) * fb;
    for (size_t i = 0; i < b.size(); i++) b[i] = b[i] * (1.0f - mix * 0.3f) + (w[i] - b[i]) * mix;
}

struct Sound {
    std::vector<int16_t> pcm;
    int rate = SR;
};
std::vector<Sound> g_snd[S_COUNT];
float g_sndGain[S_COUNT];

void store(int id, Buf& b, float gain = 1.0f, int sr = SR, bool loop = false) {
    normalize(b, 0.92f);
    if (!loop) fadeEnds(b, 64);
    Sound s;
    s.rate = sr;
    s.pcm.resize(b.size());
    for (size_t i = 0; i < b.size(); i++) s.pcm[i] = (int16_t)clampv((int)(b[i] * 32767.0f), -32767, 32767);
    g_snd[id].push_back(std::move(s));
    g_sndGain[id] = gain;
}

// ---- individual sounds ------------------------------------------------------
void genStep(int surf) {
    for (int v = 0; v < 3; v++) {
        Buf b = mk(0.32f);
        float j = 1.0f + 0.12f * nz();
        switch (surf) {
        case 0: // asphalt (wet)
            thump(b, 0, 95 * j, 50, 0.07f, 0.7f);
            burst(b, 0.0f, 0.12f, 1100 * j, 0.8f, 0.9f, 0.03f);
            burst(b, 0.02f, 0.2f, 3600, 0.7f, 0.25f, 0.05f);
            break;
        case 1: // concrete
            thump(b, 0, 110 * j, 60, 0.06f, 0.6f);
            burst(b, 0.0f, 0.1f, 1500 * j, 0.9f, 0.9f, 0.022f);
            burst(b, 0.015f, 0.12f, 4200, 0.8f, 0.15f, 0.03f);
            break;
        case 2: { // steel plate
            const float f[5] = {150 * j, 372 * j, 690 * j, 1180 * j, 2260}, a[5] = {0.8f, 0.6f, 0.45f, 0.3f, 0.15f}, d[5] = {0.16f, 0.12f, 0.09f, 0.06f, 0.04f};
            modal(b, 0, f, a, d, 5, 0.6f);
            burst(b, 0, 0.05f, 2400, 0.7f, 0.5f, 0.012f);
            thump(b, 0, 80, 45, 0.08f, 0.5f);
            break;
        }
        case 3: { // hollow wooden floor
            const float f[3] = {105 * j, 215 * j, 430 * j}, a[3] = {1.0f, 0.5f, 0.2f}, d[3] = {0.1f, 0.07f, 0.04f};
            modal(b, 0, f, a, d, 3, 0.7f);
            burst(b, 0, 0.07f, 700, 0.8f, 0.6f, 0.02f);
            break;
        }
        case 4: { // open grating
            const float f[5] = {610 * j, 1340 * j, 2110 * j, 3280, 4700}, a[5] = {0.6f, 0.6f, 0.5f, 0.3f, 0.2f}, d[5] = {0.14f, 0.12f, 0.1f, 0.07f, 0.05f};
            modal(b, 0, f, a, d, 5, 0.5f);
            modal(b, 0.03f, f, a, d, 5, 0.2f, SR, 0.03f);
            burst(b, 0, 0.04f, 3000, 0.7f, 0.5f, 0.01f);
            thump(b, 0, 90, 50, 0.06f, 0.4f);
            break;
        }
        case 5: // lino
            thump(b, 0, 100 * j, 55, 0.07f, 0.7f);
            burst(b, 0.0f, 0.08f, 700 * j, 0.8f, 0.6f, 0.025f);
            burst(b, 0.05f, 0.05f, 2800 * j, 6.0f, 0.12f, 0.02f);
            break;
        case 6: // gravel
            for (int k = 0; k < 9; k++) burst(b, 0.012f * k + 0.01f * urand(), 0.04f, 2200 + 2600 * urand(), 1.5f, 0.5f * (1.0f - k / 10.0f), 0.01f);
            thump(b, 0, 80, 50, 0.06f, 0.4f);
            break;
        default: // water
            burst(b, 0.0f, 0.3f, 900 * j, 0.6f, 0.9f, 0.09f);
            burst(b, 0.02f, 0.25f, 2600, 0.8f, 0.4f, 0.06f);
            for (int k = 0; k < 4; k++) {
                const float f[1] = {400 + 500 * urand()}, a[1] = {0.3f}, d[1] = {0.03f};
                modal(b, 0.05f + 0.05f * k, f, a, d, 1, 0.5f);
            }
            break;
        }
        store(S_STEP_ASPHALT + surf, b, 0.9f);
    }
}

void genCreak(Buf& b, float at, float dur, float f0, float f1, float gain, float rough) {
    // stick-slip: a sawtooth whose period jitters, through two resonances
    BQ r1, r2;
    size_t s0 = (size_t)(at * SR), n = (size_t)(dur * SR);
    float ph = 0, jit = 0;
    for (size_t i = 0; i < n && s0 + i < b.size(); i++) {
        float t = (float)i / (float)n;
        float f = lerpf(f0, f1, t) * (1.0f + 0.04f * std::sin(t * 37.0f) + jit);
        ph += f / SR;
        if (ph >= 1.0f) { ph -= 1.0f; jit = rough * nz() * 0.25f; }
        float saw = ph * 2.0f - 1.0f;
        if ((i & 63) == 0) { r1.bp(f * 2.6f, 5.0f, SR); r2.bp(f * 5.3f + 300.0f, 6.0f, SR); }
        float e = std::sin(t * PI);
        e = std::pow(e, 0.6f) * (0.7f + 0.3f * std::sin(t * 53.0f + 2.0f * std::sin(t * 11.0f)));
        b[s0 + i] += (r1.run(saw) * 1.2f + r2.run(saw) * 0.7f + saw * 0.08f) * e * gain;
    }
}

void genVoiceLike(Buf& b, float at, float dur, float pitch, float gain, bool whisper) {
    BQ f1, f2, f3;
    size_t s0 = (size_t)(at * SR), n = (size_t)(dur * SR);
    float ph = 0, syl = 0, sylT = 0, a1 = 600, a2 = 1500, t1 = 600, t2 = 1500, amp = 0, ampT = 1;
    for (size_t i = 0; i < n && s0 + i < b.size(); i++) {
        float t = (float)i / SR;
        if (t >= sylT) {
            sylT = t + 0.09f + 0.16f * urand();
            t1 = 300 + 600 * urand(); t2 = 1000 + 1500 * urand();
            ampT = urand() < 0.18f ? 0.0f : 0.5f + 0.5f * urand();
            syl += 1;
        }
        a1 += (t1 - a1) * 0.002f; a2 += (t2 - a2) * 0.002f; amp += (ampT - amp) * 0.004f;
        if ((i & 31) == 0) { f1.bp(a1, 7.0f, SR); f2.bp(a2, 9.0f, SR); f3.bp(2700, 8.0f, SR); }
        float src;
        if (whisper) src = nz();
        else {
            float f = pitch * (1.0f + 0.05f * std::sin(t * 31.0f) + 0.08f * std::sin(t * 3.1f + syl));
            ph += f / SR;
            if (ph >= 1.0f) ph -= 1.0f;
            src = (ph * 2.0f - 1.0f) + 0.25f * nz();
        }
        float e = std::min(1.0f, (float)i / 300.0f) * std::min(1.0f, (float)(n - i) / 600.0f);
        b[s0 + i] += (f1.run(src) + f2.run(src) * 0.8f + f3.run(src) * 0.3f) * amp * e * gain;
    }
}

float midi(float n) { return 440.0f * std::pow(2.0f, (n - 69.0f) / 12.0f); }

// a soft pluck / struck-bar note used by the music beds
void note(Buf& b, float at, float f, float dur, float gain, int sr, float bright = 0.4f) {
    size_t s0 = (size_t)(at * sr), n = (size_t)(dur * sr);
    float p1 = 0, p2 = 0, p3 = 0;
    for (size_t i = 0; i < n && s0 + i < b.size(); i++) {
        float t = (float)i / sr;
        float wob = 1.0f + 0.004f * std::sin(t * 4.3f + at);
        p1 += TAU * f * wob / sr; p2 += TAU * f * 2.005f * wob / sr; p3 += TAU * f * 3.98f * wob / sr;
        float e = std::exp(-t / (dur * 0.35f)) * std::min(1.0f, t * 250.0f);
        b[s0 + i] += (std::sin(p1) + bright * std::sin(p2) * std::exp(-t * 5.0f) + bright * 0.5f * std::sin(p3) * std::exp(-t * 9.0f)) * e * gain;
    }
}

void genSound(int id) {
    switch (id) {
    case S_STEP_ASPHALT: case S_STEP_CONCRETE: case S_STEP_METAL: case S_STEP_WOOD: case S_STEP_GRATE: case S_STEP_LINO: case S_STEP_GRAVEL:
    case S_STEP_WATER:
        genStep(id - S_STEP_ASPHALT);
        break;
    case S_LADDER:
        for (int v = 0; v < 3; v++) {
            Buf b = mk(0.3f);
            float j = 1.0f + 0.1f * nz();
            const float f[4] = {880 * j, 1720 * j, 2930 * j, 4100}, a[4] = {0.7f, 0.5f, 0.3f, 0.15f}, d[4] = {0.09f, 0.07f, 0.05f, 0.03f};
            modal(b, 0, f, a, d, 4, 0.5f);
            thump(b, 0, 120, 70, 0.05f, 0.5f);
            burst(b, 0, 0.03f, 2000, 0.8f, 0.3f, 0.008f);
            store(id, b, 0.7f);
        }
        break;
    case S_DOOR_OPEN: {
        Buf b = mk(1.0f);
        burst(b, 0, 0.03f, 1800, 2.0f, 0.5f, 0.008f);
        thump(b, 0.0f, 140, 80, 0.05f, 0.4f);
        genCreak(b, 0.08f, 0.75f, 210, 330, 0.5f, 1.0f);
        store(id, b, 0.8f);
        break;
    }
    case S_DOOR_CLOSE: {
        Buf b = mk(0.7f);
        genCreak(b, 0.0f, 0.25f, 300, 220, 0.25f, 1.0f);
        thump(b, 0.24f, 110, 45, 0.16f, 1.0f);
        burst(b, 0.24f, 0.08f, 600, 0.8f, 0.7f, 0.03f);
        burst(b, 0.3f, 0.03f, 2500, 3.0f, 0.4f, 0.008f);
        store(id, b, 0.9f);
        break;
    }
    case S_DOOR_LOCKED: {
        Buf b = mk(0.6f);
        for (int k = 0; k < 3; k++) {
            thump(b, 0.02f + k * 0.13f, 130, 80, 0.05f, 0.7f);
            burst(b, 0.02f + k * 0.13f, 0.04f, 2100 + 300 * k, 3.0f, 0.5f, 0.01f);
        }
        store(id, b, 0.8f);
        break;
    }
    case S_DOOR_METAL: {
        Buf b = mk(1.3f);
        const float f[5] = {96, 233, 410, 870, 1530}, a[5] = {0.8f, 0.6f, 0.5f, 0.3f, 0.2f}, d[5] = {0.4f, 0.3f, 0.25f, 0.15f, 0.1f};
        modal(b, 0.0f, f, a, d, 5, 0.5f);
        burst(b, 0, 0.04f, 1500, 1.0f, 0.6f, 0.01f);
        genCreak(b, 0.1f, 1.0f, 420, 760, 0.45f, 1.4f);
        store(id, b, 0.9f);
        break;
    }
    case S_LOCKER: {
        Buf b = mk(0.6f);
        const float f[5] = {310, 640, 1270, 2140, 3350}, a[5] = {0.6f, 0.6f, 0.5f, 0.3f, 0.2f}, d[5] = {0.18f, 0.14f, 0.1f, 0.07f, 0.05f};
        modal(b, 0.0f, f, a, d, 5, 0.5f);
        modal(b, 0.09f, f, a, d, 5, 0.25f, SR, 0.04f);
        burst(b, 0, 0.05f, 2600, 1.0f, 0.5f, 0.012f);
        store(id, b, 0.8f);
        break;
    }
    case S_SLAM: {
        Buf b = mk(1.4f);
        thump(b, 0, 90, 32, 0.35f, 1.0f);
        const float f[6] = {74, 163, 287, 540, 1010, 1730}, a[6] = {0.9f, 0.7f, 0.6f, 0.4f, 0.25f, 0.15f}, d[6] = {0.6f, 0.45f, 0.35f, 0.2f, 0.12f, 0.08f};
        modal(b, 0, f, a, d, 6, 0.6f);
        burst(b, 0, 0.12f, 900, 0.6f, 0.9f, 0.03f);
        bakeEcho(b, 0.11f, 0.4f, 0.35f);
        store(id, b, 1.0f);
        break;
    }
    case S_GATE: {
        Buf b = mk(6.6f);
        genCreak(b, 0.1f, 2.4f, 380, 610, 0.5f, 1.6f);
        genCreak(b, 1.6f, 2.6f, 520, 430, 0.45f, 1.8f);
        genCreak(b, 3.6f, 2.4f, 300, 480, 0.5f, 1.5f);
        genCreak(b, 0.6f, 5.0f, 95, 80, 0.25f, 2.0f);
        {
            LP1 lp;
            lp.set(260, SR);
            for (size_t i = 0; i < b.size(); i++) {
                float t = (float)i / SR;
                float e = smoothstepf(0.0f, 0.4f, t) * smoothstepf(6.3f, 5.9f, t);
                b[i] += lp.run(nz()) * 1.6f * e * (0.7f + 0.3f * std::sin(t * 19.0f));
            }
        }
        thump(b, 6.05f, 100, 40, 0.3f, 1.2f);
        const float f[4] = {120, 310, 720, 1500}, a[4] = {0.8f, 0.5f, 0.3f, 0.2f}, d[4] = {0.3f, 0.2f, 0.12f, 0.08f};
        modal(b, 6.05f, f, a, d, 4, 0.7f);
        store(id, b, 1.0f);
        break;
    }
    case S_LEVER: {
        Buf b = mk(0.7f);
        for (int k = 0; k < 4; k++) burst(b, 0.03f + 0.05f * k, 0.02f, 2400, 3.0f, 0.4f, 0.005f);
        thump(b, 0.26f, 120, 50, 0.14f, 1.0f);
        const float f[3] = {240, 610, 1420}, a[3] = {0.6f, 0.4f, 0.25f}, d[3] = {0.18f, 0.1f, 0.06f};
        modal(b, 0.26f, f, a, d, 3, 0.6f);
        store(id, b, 0.9f);
        break;
    }
    case S_SWITCH: {
        Buf b = mk(0.12f);
        burst(b, 0, 0.012f, 3200, 2.0f, 1.0f, 0.003f);
        const float f[2] = {2900, 4700}, a[2] = {0.4f, 0.2f}, d[2] = {0.012f, 0.008f};
        modal(b, 0.002f, f, a, d, 2, 0.6f);
        thump(b, 0, 300, 150, 0.02f, 0.3f);
        store(id, b, 0.6f);
        break;
    }
    case S_BREAKER: {
        Buf b = mk(1.5f);
        thump(b, 0, 85, 38, 0.3f, 1.0f);
        burst(b, 0, 0.05f, 1200, 0.8f, 0.8f, 0.012f);
        const float f[4] = {118, 236, 472, 1180}, a[4] = {0.5f, 0.4f, 0.25f, 0.2f}, d[4] = {0.5f, 0.35f, 0.25f, 0.1f};
        modal(b, 0.01f, f, a, d, 4, 0.5f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            b[i] += sat(std::sin(TAU * 60.0f * t) * 4.0f) * 0.12f * std::exp(-t * 3.0f);
        }
        bakeEcho(b, 0.19f, 0.45f, 0.4f);
        store(id, b, 1.0f);
        break;
    }
    case S_PICKUP: {
        Buf b = mk(0.25f);
        burst(b, 0, 0.12f, 1900, 0.7f, 0.6f, 0.035f);
        burst(b, 0.05f, 0.1f, 3300, 0.9f, 0.3f, 0.03f);
        thump(b, 0.01f, 180, 110, 0.04f, 0.3f);
        store(id, b, 0.6f);
        break;
    }
    case S_PAPER: {
        Buf b = mk(0.45f);
        BQ f;
        f.bp(3200, 0.7f, SR);
        float e = 0, et = 0;
        for (size_t i = 0; i < b.size(); i++) {
            if ((i % 700) == 0) et = urand() * urand();
            e += (et - e) * 0.01f;
            float t = (float)i / (float)b.size();
            b[i] = f.run(nz()) * e * std::sin(t * PI);
        }
        store(id, b, 0.5f);
        break;
    }
    case S_KEYS: {
        Buf b = mk(0.7f);
        for (int k = 0; k < 7; k++) {
            const float f[3] = {3100 + 2800 * urand(), 5200 + 2000 * urand(), 7600}, a[3] = {0.5f, 0.35f, 0.15f}, d[3] = {0.05f, 0.035f, 0.02f};
            modal(b, 0.02f + 0.07f * k + 0.03f * urand(), f, a, d, 3, 0.5f * (1.0f - k * 0.08f));
        }
        thump(b, 0.4f, 200, 100, 0.05f, 0.35f);
        burst(b, 0.4f, 0.03f, 2200, 2.0f, 0.4f, 0.008f);
        store(id, b, 0.6f);
        break;
    }
    case S_FUSE: {
        Buf b = mk(0.35f);
        burst(b, 0, 0.02f, 2600, 2.5f, 0.7f, 0.005f);
        const float f[3] = {1450, 2380, 3900}, a[3] = {0.5f, 0.4f, 0.2f}, d[3] = {0.05f, 0.04f, 0.02f};
        modal(b, 0.0f, f, a, d, 3, 0.5f);
        thump(b, 0.08f, 190, 90, 0.06f, 0.6f);
        burst(b, 0.08f, 0.02f, 3400, 3.0f, 0.5f, 0.005f);
        store(id, b, 0.7f);
        break;
    }
    case S_TAPE_CLICK: {
        Buf b = mk(0.5f);
        thump(b, 0, 160, 70, 0.06f, 0.9f);
        burst(b, 0, 0.03f, 1700, 1.5f, 0.6f, 0.008f);
        burst(b, 0.07f, 0.02f, 2800, 3.0f, 0.4f, 0.006f);
        for (size_t i = (size_t)(0.1f * SR); i < b.size(); i++) {
            float t = (float)i / SR - 0.1f;
            b[i] += std::sin(TAU * (90.0f + 60.0f * smooth01(t * 5.0f)) * t) * 0.08f * std::exp(-t * 6.0f);
        }
        store(id, b, 0.7f);
        break;
    }
    case S_GEN_CRANK: {
        Buf b = mk(2.3f);
        float t = 0.05f;
        int k = 0;
        while (t < 2.0f) {
            float prog = t / 2.0f;
            float rate = prog < 0.6f ? lerpf(6.0f, 15.0f, prog / 0.6f) : lerpf(15.0f, 5.0f, (prog - 0.6f) / 0.4f);
            float g = 0.6f + 0.4f * std::sin(prog * PI);
            thump(b, t, 70, 38, 0.08f, g);
            burst(b, t, 0.06f, 420 + 60 * (k & 1), 0.7f, 0.7f * g, 0.02f);
            burst(b, t + 0.01f, 0.03f, 2400, 1.5f, 0.2f * g, 0.008f);
            t += 1.0f / rate;
            k++;
        }
        burst(b, 2.0f, 0.25f, 300, 0.6f, 0.6f, 0.08f); // cough
        store(id, b, 1.0f);
        break;
    }
    case S_GEN_LOOP: {
        Buf b = mk(2.0f);
        for (int k = 0; k < 48; k++) {
            float t = k / 24.0f;
            float g = 0.8f + 0.2f * urand();
            thump(b, t, 62, 40, 0.035f, g);
            burst(b, t, 0.03f, 380 + 40 * (k % 3), 0.8f, 0.5f * g, 0.012f);
            if (k % 4 == 1) burst(b, t + 0.012f, 0.02f, 2200, 2.0f, 0.18f, 0.006f);
        }
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            b[i] += sat(std::sin(TAU * 48.0f * t) * 2.0f) * 0.22f + std::sin(TAU * 96.0f * t + 1.0f) * 0.1f + nz() * 0.02f;
        }
        store(id, b, 1.0f, SR, true);
        break;
    }
    case S_VALVE: {
        Buf b = mk(0.9f);
        genCreak(b, 0.0f, 0.8f, 640, 980, 0.5f, 2.2f);
        genCreak(b, 0.05f, 0.7f, 210, 260, 0.3f, 1.5f);
        store(id, b, 0.8f);
        break;
    }
    case S_DRAIN: {
        Buf b = mk(3.2f);
        LP1 lp;
        lp.set(700, SR);
        BQ r;
        float fc = 400, ft = 400;
        for (size_t i = 0; i < b.size(); i++) {
            if ((i % 2400) == 0) ft = 220 + 700 * urand();
            fc += (ft - fc) * 0.0008f;
            if ((i & 31) == 0) r.bp(fc, 5.0f, SR);
            float n = nz();
            b[i] = lp.run(n) * 0.8f + r.run(n) * 1.4f;
        }
        for (int k = 0; k < 26; k++) {
            float f0 = 180 + 500 * urand();
            const float f[1] = {f0}, a[1] = {0.5f}, d[1] = {0.04f + 0.05f * urand()};
            modal(b, 0.1f + 2.9f * urand(), f, a, d, 1, 0.6f);
        }
        seamless(b, 6000);
        store(id, b, 0.9f, SR, true);
        break;
    }
    case S_PUMP_LOOP: {
        Buf b = mk(2.0f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float thr = 0.7f + 0.3f * std::sin(TAU * 6.0f * t);
            b[i] = (sat(std::sin(TAU * 120.0f * t) * 3.0f) * 0.4f + std::sin(TAU * 240.0f * t) * 0.2f + std::sin(TAU * 355.0f * t) * 0.12f +
                    std::sin(TAU * 720.0f * t) * 0.05f) * thr + nz() * 0.03f;
        }
        for (int k = 0; k < 12; k++) thump(b, k / 6.0f, 75, 45, 0.05f, 0.5f);
        store(id, b, 0.9f, SR, true);
        break;
    }
    case S_CRANE_GROAN:
        for (int v = 0; v < 3; v++) {
            Buf b = mk(5.5f);
            float base = 58.0f + 14.0f * v;
            genCreak(b, 0.0f, 4.6f, base * 1.15f, base * 0.86f, 0.7f, 2.4f);
            genCreak(b, 0.3f, 4.0f, base * 1.63f, base * 1.41f, 0.45f, 2.0f);
            genCreak(b, 1.4f, 2.6f, base * 5.2f, base * 6.6f, 0.2f, 3.0f);
            genCreak(b, 2.8f, 1.6f, base * 7.9f, base * 5.1f, 0.14f, 3.0f);
            thump(b, 0.1f, 46, 30, 0.9f, 0.7f);
            for (int k = 0; k < 5; k++) {
                const float f[3] = {210 + 300 * urand(), 760 + 500 * urand(), 1800}, a[3] = {0.4f, 0.25f, 0.1f}, d[3] = {0.25f, 0.15f, 0.08f};
                modal(b, 0.6f + 3.6f * urand(), f, a, d, 3, 0.35f);
            }
            bakeEcho(b, 0.31f, 0.5f, 0.45f);
            lowpass(b, 3800);
            store(id, b, 1.0f);
        }
        break;
    case S_CRANE_MOTOR: {
        Buf b = mk(2.0f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float rat = 0.6f + 0.4f * (0.5f + 0.5f * sat(std::sin(TAU * 25.0f * t) * 3.0f));
            float vib = std::sin(TAU * 0.5f * t) * 0.6f;
            b[i] = sat(std::sin(TAU * 100.0f * t) * 2.5f) * 0.35f + std::sin(TAU * 430.0f * t + vib) * 0.16f + std::sin(TAU * 861.0f * t + vib * 2) * 0.07f +
                   nz() * 0.07f * rat;
        }
        lowpass(b, 2600);
        store(id, b, 0.9f, SR, true);
        break;
    }
    case S_CHAIN:
        for (int v = 0; v < 2; v++) {
            Buf b = mk(1.5f);
            for (int k = 0; k < 14; k++) {
                float at = 0.02f + 1.2f * urand() * urand();
                const float f[3] = {1500 + 2600 * urand(), 3700 + 2200 * urand(), 6200}, a[3] = {0.5f, 0.3f, 0.12f}, d[3] = {0.06f, 0.04f, 0.02f};
                modal(b, at, f, a, d, 3, 0.4f + 0.4f * urand());
                burst(b, at, 0.02f, 2800, 1.5f, 0.2f, 0.006f);
            }
            bakeEcho(b, 0.14f, 0.35f, 0.3f);
            store(id, b, 0.8f);
        }
        break;
    case S_METAL_BANG:
        for (int v = 0; v < 3; v++) {
            Buf b = mk(3.2f);
            float j = 0.8f + 0.25f * v;
            const float f[7] = {61 * j, 137 * j, 243 * j, 418 * j, 707 * j, 1190 * j, 1930 * j};
            const float a[7] = {1.0f, 0.8f, 0.7f, 0.5f, 0.35f, 0.22f, 0.12f};
            const float d[7] = {1.4f, 1.1f, 0.9f, 0.6f, 0.4f, 0.25f, 0.15f};
            modal(b, 0, f, a, d, 7, 0.5f, SR, 0.01f);
            burst(b, 0, 0.1f, 700, 0.6f, 0.9f, 0.025f);
            thump(b, 0, 80, 30, 0.3f, 0.8f);
            bakeEcho(b, 0.27f, 0.5f, 0.5f);
            store(id, b, 1.0f);
        }
        break;
    case S_KNOCK: {
        Buf b = mk(1.7f);
        for (int k = 0; k < 3; k++) {
            float at = 0.05f + k * 0.42f;
            thump(b, at, 130, 70, 0.07f, 0.9f);
            const float f[4] = {148, 262, 541, 930}, a[4] = {0.8f, 0.7f, 0.4f, 0.2f}, d[4] = {0.22f, 0.16f, 0.1f, 0.06f};
            modal(b, at, f, a, d, 4, 0.55f);
            burst(b, at, 0.03f, 1100, 1.0f, 0.4f, 0.008f);
        }
        bakeEcho(b, 0.09f, 0.4f, 0.4f);
        store(id, b, 1.0f);
        break;
    }
    case S_THUNDER:
        for (int v = 0; v < 3; v++) {
            Buf b = mk(7.0f);
            LP1 lp, lp2;
            float e = 0, et = 1.0f;
            for (size_t i = 0; i < b.size(); i++) {
                float t = (float)i / SR;
                if ((i % 3200) == 0) et = (0.3f + 0.7f * urand()) * std::exp(-t * (0.45f + 0.1f * v));
                e += (et - e) * 0.0015f;
                float cut = 120.0f + 1900.0f * std::exp(-t * (1.2f + 0.5f * v));
                if ((i & 63) == 0) { lp.set(cut, SR); lp2.set(cut * 1.5f, SR); }
                b[i] = lp2.run(lp.run(nz())) * e * 6.0f;
            }
            if (v == 0) burst(b, 0.0f, 0.35f, 2400, 0.5f, 1.4f, 0.07f); // close crack
            thump(b, 0.05f, 55, 24, 1.6f, 0.9f);
            store(id, b, 1.0f);
        }
        break;
    case S_STINGER: {
        Buf b = mk(2.6f);
        const float fr[5] = {110.0f, 116.5f, 155.6f, 233.1f, 329.6f};
        float ph[5] = {0, 0.3f, 0.6f, 0.1f, 0.8f};
        LP1 lp;
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float s = 0;
            for (int k = 0; k < 5; k++) {
                ph[k] += fr[k] * (1.0f + 0.004f * std::sin(t * 5.0f + k)) / SR;
                if (ph[k] >= 1) ph[k] -= 1;
                s += (ph[k] * 2 - 1) * (k < 3 ? 0.3f : 0.18f);
            }
            if ((i & 63) == 0) lp.set(300.0f + 5200.0f * std::exp(-t * 3.2f), SR);
            float e = std::min(1.0f, t * 220.0f) * std::exp(-t * 1.5f);
            b[i] = lp.run(s) * e;
        }
        burst(b, 0, 0.25f, 1600, 0.5f, 0.9f, 0.05f);
        thump(b, 0, 82, 32, 0.9f, 1.0f);
        bakeEcho(b, 0.23f, 0.45f, 0.4f);
        store(id, b, 1.0f);
        break;
    }
    case S_SWELL: {
        Buf b = mk(3.6f);
        const float fr[4] = {73.4f, 77.8f, 103.8f, 146.8f};
        LP1 lp;
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float k = t / 3.1f;
            float e = k < 1.0f ? k * k * k : std::exp(-(t - 3.1f) * 12.0f);
            float s = 0;
            for (int q = 0; q < 4; q++) s += std::sin(TAU * fr[q] * t * (1.0f + 0.01f * k * q)) * 0.3f;
            if ((i & 63) == 0) lp.set(300.0f + 3000.0f * k * k, SR);
            b[i] = (s + lp.run(nz()) * 0.5f * k) * e;
        }
        store(id, b, 0.9f);
        break;
    }
    case S_SCREAM: {
        Buf b = mk(2.0f);
        genCreak(b, 0.0f, 1.5f, 620, 980, 0.6f, 3.0f);
        genCreak(b, 0.05f, 1.6f, 930, 760, 0.5f, 3.5f);
        genCreak(b, 0.0f, 1.8f, 148, 96, 0.5f, 2.5f);
        genVoiceLike(b, 0.0f, 1.6f, 310, 0.6f, false);
        for (float& v : b) v = sat(v * 2.5f);
        bakeEcho(b, 0.16f, 0.45f, 0.4f);
        store(id, b, 1.0f);
        break;
    }
    case S_CHASE_LOOP: {
        Buf b = mk(4.0f);
        for (int k = 0; k < 8; k++) {
            thump(b, k * 0.5f, 68, 34, 0.22f, 1.0f);
            if (k & 1) thump(b, k * 0.5f + 0.18f, 60, 34, 0.15f, 0.6f);
        }
        for (int k = 0; k < 32; k++) {
            const float f[2] = {2900 + 400.0f * (k % 3), 5100}, a[2] = {0.2f, 0.1f}, d[2] = {0.03f, 0.02f};
            modal(b, k * 0.125f, f, a, d, 2, (k % 4 == 2) ? 0.5f : 0.25f);
        }
        const float fr[3] = {55.0f, 58.3f, 82.4f};
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float s = 0;
            for (int q = 0; q < 3; q++) s += sat(std::sin(TAU * fr[q] * t) * 3.0f) * 0.14f;
            b[i] += s * (0.6f + 0.4f * std::sin(TAU * 4.0f * t)) + std::sin(TAU * 466.2f * t) * 0.03f * (0.5f + 0.5f * std::sin(TAU * 0.25f * t));
        }
        store(id, b, 0.9f, SR, true);
        break;
    }
    case S_RIG_STEP:
        for (int v = 0; v < 3; v++) {
            Buf b = mk(0.55f);
            thump(b, 0, 62 + 6.0f * v, 30, 0.2f, 1.0f);
            burst(b, 0, 0.14f, 420, 0.6f, 0.8f, 0.04f);
            burst(b, 0.03f, 0.18f, 1500, 0.7f, 0.25f, 0.05f); // wet
            const float f[3] = {2100 + 500 * urand(), 3600, 5400}, a[3] = {0.3f, 0.2f, 0.1f}, d[3] = {0.05f, 0.03f, 0.02f};
            modal(b, 0.16f + 0.1f * urand(), f, a, d, 3, 0.4f); // the hook touching something
            store(id, b, 1.0f);
        }
        break;
    case S_RIG_BREATH: {
        Buf b = mk(3.4f);
        BQ f1, f2;
        f1.bp(620, 2.0f, SR);
        f2.bp(1500, 2.5f, SR);
        LP1 gl;
        gl.set(260, SR);
        float ph = 0;
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float cyc = std::fmod(t, 1.7f) / 1.7f;
            float in = smoothstepf(0.0f, 0.12f, cyc) * smoothstepf(0.42f, 0.3f, cyc);
            float out = smoothstepf(0.5f, 0.58f, cyc) * smoothstepf(0.98f, 0.7f, cyc);
            float n = nz();
            float rat = 0.6f + 0.4f * sat(std::sin(TAU * 31.0f * t) * 4.0f);
            ph += (44.0f + 6.0f * std::sin(t * 9.0f)) / SR;
            if (ph >= 1) ph -= 1;
            b[i] = f2.run(n) * in * 0.5f + f1.run(n) * out * 0.9f * rat + gl.run(ph * 2 - 1) * out * 0.9f;
        }
        for (int k = 0; k < 6; k++) burst(b, 0.9f + 0.1f * k + (k > 2 ? 1.3f : 0.0f), 0.015f, 2400, 3.0f, 0.3f, 0.004f);
        seamless(b, 3000);
        store(id, b, 0.9f, SR, true);
        break;
    }
    case S_RADIO_ON: {
        Buf b = mk(0.3f);
        burst(b, 0, 0.004f, 3000, 1.0f, 1.0f, 0.002f);
        burst(b, 0.01f, 0.2f, 2300, 0.9f, 0.6f, 0.06f);
        highpass(b, 700);
        store(id, b, 0.6f);
        break;
    }
    case S_RADIO_GARBLE:
        for (int v = 0; v < 4; v++) {
            Buf b = mk(1.3f);
            genVoiceLike(b, 0.02f, 1.2f, 105.0f + 22.0f * v, 1.0f, false);
            for (float& s : b) s = sat(s * 3.0f) + nz() * 0.03f;
            BQ f;
            f.bp(1300, 0.9f, SR);
            for (float& s : b) s = f.run(s);
            highpass(b, 500);
            store(id, b, 0.6f);
        }
        break;
    case S_PHONE_RING: {
        Buf b = mk(4.0f);
        const float f[4] = {1105, 1420, 2870, 3610};
        for (size_t i = 0; i < (size_t)(1.7f * SR); i++) {
            float t = (float)i / SR;
            float ham = 0.5f + 0.5f * sat(std::sin(TAU * 21.0f * t) * 6.0f);
            float e = std::min(1.0f, t * 60.0f) * std::min(1.0f, (1.7f - t) * 5.0f);
            float s = 0;
            for (int k = 0; k < 4; k++) s += std::sin(TAU * f[k] * t + k) * (k < 2 ? 0.4f : 0.15f);
            b[i] = s * ham * e;
        }
        bakeEcho(b, 0.06f, 0.3f, 0.3f);
        store(id, b, 0.9f, SR, true);
        break;
    }
    case S_PHONE_PICK: {
        Buf b = mk(0.5f);
        burst(b, 0, 0.02f, 2400, 2.0f, 0.8f, 0.006f);
        thump(b, 0, 220, 120, 0.04f, 0.5f);
        burst(b, 0.08f, 0.25f, 1500, 0.6f, 0.2f, 0.09f);
        burst(b, 0.3f, 0.015f, 3000, 3.0f, 0.3f, 0.005f);
        store(id, b, 0.7f);
        break;
    }
    case S_BREATH: {
        Buf b = mk(1.9f);
        BQ f;
        f.bp(1100, 1.2f, SR);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float cyc = std::fmod(t, 0.95f) / 0.95f;
            float e = smoothstepf(0.0f, 0.15f, cyc) * smoothstepf(0.45f, 0.3f, cyc) * 0.6f + smoothstepf(0.5f, 0.62f, cyc) * smoothstepf(1.0f, 0.75f, cyc);
            b[i] = f.run(nz()) * e;
        }
        store(id, b, 0.5f);
        break;
    }
    case S_DEATH: {
        Buf b = mk(3.6f);
        float ph = 0;
        LP1 lp;
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float f = 120.0f * std::exp(-t * 0.8f) + 26.0f;
            ph += TAU * f / SR;
            if ((i & 63) == 0) lp.set(200.0f + 3000.0f * std::exp(-t * 2.5f), SR);
            b[i] = std::sin(ph) * std::exp(-t * 0.7f) * 0.9f + lp.run(nz()) * std::exp(-t * 3.0f) * 0.9f;
        }
        genCreak(b, 0.0f, 1.2f, 880, 310, 0.3f, 3.0f);
        store(id, b, 1.0f);
        break;
    }
    case S_BUZZ: {
        Buf b = mk(1.0f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            b[i] = sat(std::sin(TAU * 120.0f * t) * 8.0f) * 0.3f + std::sin(TAU * 2880.0f * t) * 0.02f * (0.5f + 0.5f * std::sin(TAU * 120.0f * t));
        }
        highpass(b, 900);
        store(id, b, 0.35f, SR, true);
        break;
    }
    case S_HUM: {
        Buf b = mk(2.0f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            b[i] = std::sin(TAU * 60.0f * t) * 0.5f + std::sin(TAU * 120.0f * t + 0.4f) * 0.35f + std::sin(TAU * 180.0f * t + 1.1f) * 0.18f +
                   std::sin(TAU * 300.0f * t) * 0.06f * (0.7f + 0.3f * std::sin(TAU * 1.0f * t));
        }
        store(id, b, 0.6f, SR, true);
        break;
    }
    case S_FOGHORN: {
        Buf b = mk(4.6f);
        float ph = 0, ph2 = 0;
        LP1 lp;
        lp.set(520, SR);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float f = t < 3.0f ? 174.0f : lerpf(174.0f, 87.0f, smooth01((t - 3.0f) / 0.5f));
            ph += f / SR; ph2 += f * 1.005f / SR;
            if (ph >= 1) ph -= 1;
            if (ph2 >= 1) ph2 -= 1;
            float e = smoothstepf(0.0f, 0.35f, t) * smoothstepf(4.3f, 3.7f, t);
            b[i] = lp.run((ph * 2 - 1) + (ph2 * 2 - 1)) * e;
        }
        bakeEcho(b, 0.45f, 0.5f, 0.5f);
        store(id, b, 0.8f);
        break;
    }
    case S_BELL: {
        Buf b = mk(4.0f);
        const float f[6] = {523.0f, 1046.0f * 1.19f, 1568.0f * 1.07f, 2093.0f * 1.31f, 3140.0f, 4300.0f};
        const float a[6] = {0.9f, 0.6f, 0.45f, 0.3f, 0.15f, 0.08f}, d[6] = {2.2f, 1.6f, 1.0f, 0.6f, 0.35f, 0.2f};
        modal(b, 0.0f, f, a, d, 6, 0.5f);
        modal(b, 1.3f, f, a, d, 6, 0.3f, SR, 0.002f);
        burst(b, 0, 0.02f, 3000, 1.0f, 0.3f, 0.005f);
        store(id, b, 0.7f);
        break;
    }
    case S_TAPE_HISS: {
        Buf b = mk(2.4f);
        HP1 hp;
        hp.set(1800, SR);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            b[i] = hp.run(nz()) * 0.5f + std::sin(TAU * 50.0f * t) * 0.07f * (1.0f + 0.3f * std::sin(TAU * 0.8f * t));
        }
        for (int k = 0; k < 9; k++) burst(b, 2.2f * urand(), 0.004f, 3500, 1.0f, 0.5f, 0.001f);
        seamless(b, 2000);
        store(id, b, 0.35f, SR, true);
        break;
    }
    case S_RADIO_TUNE: {
        // a slow waltz in A minor, heard through a valve radio
        const int sr = 22050;
        const float beat = 0.8f;
        Buf b = mk(beat * 24.0f, sr);
        static const int chords[8][3] = {{57, 60, 64}, {57, 60, 64}, {62, 65, 69}, {62, 65, 69}, {56, 59, 64}, {56, 62, 64}, {57, 60, 64}, {57, 60, 64}};
        static const int bass[8] = {45, 40, 38, 45, 40, 44, 45, 40};
        static const int mel[24] = {76, -1, 72, 74, 76, 77, 77, -1, 74, 72, 74, 76, 74, -1, 71, 68, 71, 74, 72, -1, 69, -1, -1, -1};
        for (int bar = 0; bar < 8; bar++) {
            float t0 = bar * 3 * beat;
            note(b, t0, midi((float)bass[bar]), beat * 1.6f, 0.5f, sr, 0.3f);
            for (int k = 1; k < 3; k++)
                for (int n = 0; n < 3; n++) note(b, t0 + k * beat, midi((float)chords[bar][n]), beat * 0.7f, 0.16f, sr, 0.3f);
        }
        // melody: a reedy lead with slow vibrato
        for (int i = 0; i < 24; i++) {
            if (mel[i] < 0) continue;
            int len = 1;
            while (i + len < 24 && mel[i + len] == -1) len++;
            float f = midi((float)mel[i]), at = i * beat, dur = len * beat * 0.96f;
            size_t s0 = (size_t)(at * sr), n = (size_t)(dur * sr);
            float ph = 0;
            for (size_t k = 0; k < n && s0 + k < b.size(); k++) {
                float t = (float)k / sr;
                float vib = 1.0f + 0.007f * std::sin(TAU * 5.2f * t) * smooth01(t * 3.0f);
                ph += TAU * f * vib / sr;
                float e = std::min(1.0f, t * 30.0f) * std::min(1.0f, (dur - t) * 12.0f);
                b[s0 + k] += (std::sin(ph) + 0.4f * std::sin(3 * ph) + 0.15f * std::sin(5 * ph)) * 0.22f * e;
            }
        }
        // tape wow by resampling, then the radio's band limit and crackle
        Buf w(b.size());
        float rp = 0;
        for (size_t i = 0; i < w.size(); i++) {
            float t = (float)i / sr;
            rp += 1.0f + 0.006f * std::sin(TAU * 0.6f * t) + 0.002f * std::sin(TAU * 6.3f * t);
            size_t i0 = (size_t)rp % b.size(), i1 = (i0 + 1) % b.size();
            float fr = rp - std::floor(rp);
            w[i] = lerpf(b[i0], b[i1], fr);
        }
        BQ bp;
        bp.bp(1100, 0.6f, (float)sr);
        for (size_t i = 0; i < w.size(); i++) w[i] = sat(bp.run(w[i]) * 2.0f) + nz() * 0.012f;
        for (int k = 0; k < 40; k++) burst(w, 19.0f * urand(), 0.003f, 2500, 1.0f, 0.25f, 0.001f, sr);
        seamless(w, 3000);
        store(id, w, 0.7f, sr, true);
        break;
    }
    case S_TITLE:
    case S_END_THEME: {
        // sparse, detuned notes over a low pedal
        const int sr = 22050;
        bool end = id == S_END_THEME;
        float len = end ? 16.0f : 20.0f;
        Buf b = mk(len, sr);
        static const int seqA[10] = {57, 64, 60, 59, 57, 52, 63, 60, 56, 57};
        static const int seqB[8] = {57, 60, 64, 67, 65, 64, 60, 57};
        const int* seq = end ? seqB : seqA;
        int n = end ? 8 : 10;
        for (int i = 0; i < n; i++) {
            float at = i * (len / n) + 0.15f * (i % 3 == 1);
            note(b, at, midi((float)seq[i]) * (1.0f + 0.004f * (i % 2)), 3.4f, 0.5f, sr, 0.5f);
            if (i % 3 == 0) note(b, at + 0.02f, midi((float)seq[i] - 12.0f), 4.2f, 0.3f, sr, 0.2f);
            if (!end && i % 4 == 2) note(b, at + 0.9f, midi((float)seq[i] + 6.0f), 2.2f, 0.12f, sr, 0.6f); // tritone ghost
        }
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / sr;
            float sw = 0.5f + 0.5f * std::sin(TAU * t / len - 1.5f);
            b[i] += (std::sin(TAU * 55.0f * t) * 0.16f + std::sin(TAU * 55.4f * t) * 0.14f + std::sin(TAU * 82.4f * t) * 0.07f * sw +
                     std::sin(TAU * (end ? 164.8f : 116.5f) * t) * 0.03f * (1.0f - sw));
        }
        bakeEcho(b, 0.37f, 0.5f, 0.55f, sr);
        seamless(b, 6000);
        store(id, b, 0.8f, sr, true);
        break;
    }
    case S_KEYPAD: {
        Buf b = mk(0.09f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            b[i] = sat(std::sin(TAU * 1180.0f * t) * 2.0f) * std::min(1.0f, t * 400.0f) * std::min(1.0f, (0.09f - t) * 200.0f);
        }
        store(id, b, 0.4f);
        break;
    }
    case S_ERROR: {
        Buf b = mk(0.45f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            b[i] = sat(std::sin(TAU * 148.0f * t) * 6.0f) * std::min(1.0f, t * 200.0f) * std::min(1.0f, (0.45f - t) * 20.0f);
        }
        lowpass(b, 1800);
        store(id, b, 0.5f);
        break;
    }
    case S_OK: {
        Buf b = mk(0.5f);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float f = t < 0.14f ? 880.0f : 1320.0f;
            float e = t < 0.14f ? std::min(1.0f, (0.14f - t) * 60.0f) : std::exp(-(t - 0.14f) * 9.0f);
            b[i] = std::sin(TAU * f * t) * e * std::min(1.0f, t * 300.0f);
        }
        thump(b, 0.28f, 140, 60, 0.12f, 0.8f);
        store(id, b, 0.5f);
        break;
    }
    case S_POUR: {
        Buf b = mk(4.0f);
        LP1 lp;
        lp.set(900, SR);
        BQ r;
        float am = 0.5f, amt = 0.5f;
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            if ((i % 1900) == 0) amt = 0.3f + 0.7f * urand();
            am += (amt - am) * 0.004f;
            if ((i & 63) == 0) r.bp(280.0f + 170.0f * t, 6.0f, SR);
            float n = nz();
            float e = smoothstepf(0.0f, 0.2f, t) * smoothstepf(4.0f, 3.5f, t);
            b[i] = (lp.run(n) * 0.6f + r.run(n) * 1.6f) * am * e;
        }
        for (int k = 0; k < 30; k++) {
            const float f[1] = {250 + 400 * urand()}, a[1] = {0.4f}, d[1] = {0.03f + 0.03f * urand()};
            modal(b, 0.2f + 3.4f * urand(), f, a, d, 1, 0.5f);
        }
        store(id, b, 0.7f);
        break;
    }
    case S_CUT: {
        Buf b = mk(1.0f);
        genCreak(b, 0.0f, 0.35f, 500, 900, 0.3f, 2.5f);
        burst(b, 0.36f, 0.05f, 2600, 0.8f, 1.2f, 0.01f);
        const float f[4] = {1900, 3300, 5200, 7100}, a[4] = {0.6f, 0.5f, 0.3f, 0.15f}, d[4] = {0.09f, 0.06f, 0.04f, 0.025f};
        modal(b, 0.36f, f, a, d, 4, 0.6f);
        thump(b, 0.36f, 170, 70, 0.08f, 0.8f);
        modal(b, 0.62f, f, a, d, 4, 0.2f, SR, 0.05f);
        modal(b, 0.74f, f, a, d, 4, 0.1f, SR, 0.05f);
        store(id, b, 0.9f);
        break;
    }
    case S_WHISPER: {
        Buf b = mk(2.8f);
        genVoiceLike(b, 0.1f, 2.5f, 0, 1.0f, true);
        highpass(b, 900);
        bakeEcho(b, 0.13f, 0.4f, 0.4f);
        store(id, b, 0.6f);
        break;
    }
    case S_VEND: {
        Buf b = mk(1.7f);
        const float f[3] = {3900, 5600, 7900}, a[3] = {0.5f, 0.4f, 0.2f}, d[3] = {0.06f, 0.04f, 0.03f};
        modal(b, 0.0f, f, a, d, 3, 0.5f);
        modal(b, 0.09f, f, a, d, 3, 0.3f, SR, 0.02f);
        thump(b, 0.3f, 120, 60, 0.1f, 0.8f);
        burst(b, 0.3f, 0.2f, 500, 0.6f, 0.5f, 0.06f);
        genCreak(b, 0.45f, 0.5f, 95, 130, 0.4f, 2.0f);
        thump(b, 1.2f, 95, 40, 0.2f, 1.0f);
        burst(b, 1.2f, 0.08f, 900, 0.7f, 0.6f, 0.02f);
        store(id, b, 0.8f);
        break;
    }
    case S_TAP: {
        Buf b = mk(1.8f);
        BQ f;
        f.bp(3200, 0.8f, SR);
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            float sput = t < 0.6f ? (hash2f((int)(t * 30.0f), 9) > 0.45f ? 1.0f : 0.1f) : 1.0f;
            float e = smoothstepf(0.0f, 0.1f, t) * smoothstepf(1.8f, 1.4f, t);
            b[i] = f.run(nz()) * sput * e * 0.8f;
        }
        thump(b, 0.05f, 140, 60, 0.15f, 0.8f);
        thump(b, 0.32f, 120, 60, 0.12f, 0.6f);
        store(id, b, 0.6f);
        break;
    }
    case S_HATFALL: {
        Buf b = mk(1.5f);
        float at = 0.0f, g = 1.0f, gap = 0.34f;
        for (int k = 0; k < 6; k++) {
            const float f[3] = {410 + 30 * nz(), 930, 1850}, a[3] = {0.7f, 0.5f, 0.2f}, d[3] = {0.05f, 0.035f, 0.02f};
            modal(b, at, f, a, d, 3, 0.6f * g);
            burst(b, at, 0.02f, 1600, 1.0f, 0.4f * g, 0.006f);
            at += gap; gap *= 0.62f; g *= 0.65f;
        }
        store(id, b, 0.8f);
        break;
    }
    case S_CLANG:
        for (int v = 0; v < 2; v++) {
            Buf b = mk(1.8f);
            float j = 1.0f + 0.12f * v;
            const float f[6] = {132 * j, 301 * j, 583 * j, 960 * j, 1710 * j, 2800};
            const float a[6] = {0.9f, 0.8f, 0.6f, 0.4f, 0.25f, 0.12f}, d[6] = {0.7f, 0.55f, 0.4f, 0.25f, 0.15f, 0.08f};
            modal(b, 0, f, a, d, 6, 0.5f, SR, 0.01f);
            burst(b, 0, 0.05f, 1400, 0.8f, 0.8f, 0.012f);
            thump(b, 0, 95, 40, 0.18f, 0.7f);
            bakeEcho(b, 0.12f, 0.45f, 0.45f);
            store(id, b, 1.0f);
        }
        break;
    case S_SPLASH: {
        Buf b = mk(2.2f);
        LP1 lp;
        for (size_t i = 0; i < b.size(); i++) {
            float t = (float)i / SR;
            if ((i & 63) == 0) lp.set(500.0f + 3500.0f * std::exp(-t * 2.5f), SR);
            b[i] = lp.run(nz()) * std::exp(-t * 2.0f) * std::min(1.0f, t * 120.0f);
        }
        thump(b, 0, 80, 30, 0.4f, 1.0f);
        for (int k = 0; k < 16; k++) {
            const float f[1] = {300 + 700 * urand()}, a[1] = {0.3f}, d[1] = {0.04f};
            modal(b, 0.2f + 1.6f * urand(), f, a, d, 1, 0.5f);
        }
        store(id, b, 1.0f);
        break;
    }
    case S_UI: {
        Buf b = mk(0.8f);
        const float f[3] = {110.0f, 220.6f, 331.0f}, a[3] = {0.9f, 0.35f, 0.12f}, d[3] = {0.35f, 0.22f, 0.12f};
        modal(b, 0, f, a, d, 3, 0.6f);
        store(id, b, 0.6f);
        break;
    }
    case S_DRIP:
        for (int v = 0; v < 3; v++) {
            Buf b = mk(0.35f);
            float ph = 0, f0 = 800.0f + 300.0f * v;
            for (size_t i = 0; i < (size_t)(0.06f * SR); i++) {
                float t = (float)i / (0.06f * SR);
                ph += TAU * lerpf(f0, f0 * 2.1f, t) / SR;
                b[i] = std::sin(ph) * (1.0f - t) * std::min(1.0f, t * 30.0f);
            }
            bakeEcho(b, 0.07f, 0.4f, 0.5f);
            store(id, b, 0.5f);
        }
        break;
    default: {
        // S_HEART is generated live; keep a silent placeholder so every id is valid
        Buf b = mk(0.05f);
        b[0] = 0.0001f;
        store(id, b, 0.0f);
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Mixer
// ---------------------------------------------------------------------------
struct Voice {
    bool active = false;
    int snd = 0, var = 0;
    double pos = 0;
    float pitch = 1;
    float gain = 1;        // requested gain
    float fade = 1;        // 0..1 envelope for stops
    float fadeRate = 0;    // per second, > 0 while stopping
    float curL = 0, curR = 0;
    bool loop = false;
    bool positional = false;
    Vec3 wpos;
    float refDist = 5;
    float pan = 0;
    float lp = 0, lpA = 1;
    float send = 0.2f;
    u32 gen = 0;
};
const int MAXV = 40;
Voice g_v[MAXV];
u32 g_gen = 1;
bool g_ready = false;
int g_outRate = 48000;
Vec3 g_lpos;
float g_lyaw = 0;
AudioEnv g_env, g_envCur;
float g_peak = 0;

// reverb
struct Comb {
    std::vector<float> buf;
    size_t idx = 0;
    float fb = 0.8f, store = 0, damp = 0.25f;
    void init(int n) { buf.assign((size_t)n, 0.0f); idx = 0; }
    float run(float x) {
        float y = buf[idx];
        store = y * (1.0f - damp) + store * damp;
        buf[idx] = x + store * fb;
        if (++idx >= buf.size()) idx = 0;
        return y;
    }
};
struct Allpass {
    std::vector<float> buf;
    size_t idx = 0;
    void init(int n) { buf.assign((size_t)n, 0.0f); idx = 0; }
    float run(float x) {
        float b = buf[idx];
        float y = -x + b;
        buf[idx] = x + b * 0.5f;
        if (++idx >= buf.size()) idx = 0;
        return y;
    }
};
Comb g_comb[2][4];
Allpass g_ap[2][2];

// procedural beds state
struct Beds {
    BQ windL1, windR1, windL2, windR2, whistle, rainMid, roof, interBP;
    HP1 rainHiL, rainHiR;
    LP1 seaL, seaR, droneLP, rumble;
    float windF1 = 300, windF2 = 900, gust = 0.5f, gustT = 0.5f;
    float t = 0;
    float dph[4] = {0, 0, 0, 0};
    float interPh = 0, interF = 2600, interFT = 2600, interGate = 0;
    float heartT = 0;
    float heartEnv1 = 0, heartEnv2 = 0, heartPh = 0;
    int ctr = 0;
    float patter = 0;
} g_b;

int alloc_voice() {
    for (int i = 0; i < MAXV; i++)
        if (!g_v[i].active) return i;
    // steal the quietest non-looping voice
    int best = -1;
    float bg = 1e9f;
    for (int i = 0; i < MAXV; i++)
        if (!g_v[i].loop) {
            float gsum = g_v[i].curL + g_v[i].curR;
            if (gsum < bg) { bg = gsum; best = i; }
        }
    return best;
}

int start_voice(int snd, float gain, float pitch, bool loop, bool positional, const Vec3& pos, float ref, float pan) {
    if (!g_ready || snd < 0 || snd >= S_COUNT || g_snd[snd].empty()) return -1;
    int i = alloc_voice();
    if (i < 0) return -1;
    Voice& v = g_v[i];
    v = Voice();
    v.active = true;
    v.snd = snd;
    v.var = (int)(g_ns >> 16) % (int)g_snd[snd].size();
    nz();
    v.gain = gain * g_sndGain[snd];
    v.pitch = pitch;
    v.loop = loop;
    v.positional = positional;
    v.wpos = pos;
    v.refDist = ref;
    v.pan = pan;
    v.gen = g_gen++;
    v.send = positional ? 0.3f : 0.12f;
    if (loop) v.pos = (double)((g_ns >> 8) % (u32)g_snd[snd][(size_t)v.var].pcm.size());
    // start silent for loops (fade in), immediate for one-shots
    v.curL = v.curR = -1.0f;
    return i | (int)((v.gen & 0xFFFFFF) << 6);
}

Voice* find_voice(int handle) {
    if (handle < 0) return nullptr;
    int i = handle & 63;
    if (i >= MAXV) return nullptr;
    Voice& v = g_v[i];
    if (!v.active || (int)(v.gen & 0xFFFFFF) != (handle >> 6)) return nullptr;
    return &v;
}

void voice_gains(const Voice& v, float& l, float& r, float& lpA) {
    float g = v.gain * v.fade;
    float pan = v.pan;
    lpA = 1.0f;
    if (v.positional) {
        Vec3 d = v.wpos - g_lpos;
        float dist = length(d);
        float att = v.refDist / (v.refDist + std::max(0.0f, dist - v.refDist));
        float far = v.refDist * 22.0f;
        att *= saturate(1.0f - dist / far) * saturate(1.0f - dist / far);
        g *= att;
        Vec3 right(std::cos(g_lyaw), 0, std::sin(g_lyaw));
        if (dist > 0.01f) {
            Vec3 dn = d * (1.0f / dist);
            pan = clampv(dot(dn, right), -1.0f, 1.0f) * 0.85f;
            Vec3 fwd(std::sin(g_lyaw), 0, -std::cos(g_lyaw));
            if (dot(dn, fwd) < 0) g *= 0.85f;
        }
        // distant sounds lose their top end
        float cut = 14000.0f / (1.0f + dist / (v.refDist * 2.5f));
        lpA = 1.0f - std::exp(-TAU * std::max(500.0f, cut) / (float)g_outRate);
    }
    float a = (pan + 1.0f) * 0.25f * PI;
    l = g * std::cos(a) * 1.4142f * 0.75f;
    r = g * std::sin(a) * 1.4142f * 0.75f;
}

void beds(float* out, int frames) {
    Beds& b = g_b;
    float sr = (float)g_outRate;
    const AudioEnv& e = g_envCur;
    float outd = 1.0f - e.indoor;
    float windLvl = e.wind * (0.2f + 0.8f * outd) * (0.55f + 0.9f * e.height) * 0.16f;
    float whistLvl = e.wind * e.height * 0.03f + e.wind * e.indoor * 0.012f;
    float rainLvl = e.rain * outd * 0.085f;
    float roofLvl = e.rain * e.indoor * 0.11f;
    float seaLvl = e.sea * 0.07f;
    float droneLvl = e.drone * 0.085f;
    float interLvl = e.interference * e.interference * 0.06f;
    float duck = 1.0f - 0.45f * e.duck;
    for (int i = 0; i < frames; i++) {
        if ((b.ctr++ & 63) == 0) {
            b.t += 64.0f / sr;
            b.gustT = 0.35f + 0.4f * (0.5f + 0.5f * std::sin(b.t * 0.37f)) + 0.25f * (0.5f + 0.5f * std::sin(b.t * 0.83f + 1.7f));
            b.gust += (b.gustT - b.gust) * 0.02f;
            b.windF1 = 230.0f + 260.0f * b.gust + 60.0f * std::sin(b.t * 0.21f);
            b.windF2 = 620.0f + 700.0f * b.gust + 150.0f * std::sin(b.t * 0.33f + 2.0f);
            b.windL1.bp(b.windF1, 1.4f, sr); b.windR1.bp(b.windF1 * 1.07f, 1.4f, sr);
            b.windL2.bp(b.windF2, 2.2f, sr); b.windR2.bp(b.windF2 * 0.93f, 2.2f, sr);
            b.whistle.bp(1250.0f + 500.0f * b.gust + 200.0f * std::sin(b.t * 1.3f), 14.0f, sr);
            b.rainMid.bp(2600.0f, 0.5f, sr);
            b.roof.bp(1500.0f, 1.2f, sr);
            b.rainHiL.set(5200.0f, sr); b.rainHiR.set(5600.0f, sr);
            b.seaL.set(420.0f, sr); b.seaR.set(480.0f, sr);
            b.droneLP.set(220.0f + 500.0f * e.drone, sr);
            b.rumble.set(90.0f, sr);
            if (urand() < 0.08f) b.interFT = 1900.0f + 1700.0f * urand();
            b.interF += (b.interFT - b.interF) * 0.2f;
            b.interBP.bp(1800.0f, 0.7f, sr);
            b.interGate = urand() < 0.3f + 0.6f * e.interference ? 1.0f : 0.15f;
        }
        float n1 = nz(), n2 = nz();
        float gust = 0.35f + 0.65f * b.gust;
        float L = 0, R = 0;
        // wind
        L += (b.windL1.run(n1) * 0.9f + b.windL2.run(n1) * 0.6f * gust) * windLvl * gust;
        R += (b.windR1.run(n2) * 0.9f + b.windR2.run(n2) * 0.6f * gust) * windLvl * gust;
        float wh = b.whistle.run(n1) * whistLvl * gust * gust;
        L += wh; R += wh * 0.8f;
        // rain
        float rm = b.rainMid.run(n2);
        L += (b.rainHiL.run(n1) * 0.7f + rm * 0.5f) * rainLvl;
        R += (b.rainHiR.run(n2) * 0.7f + rm * 0.45f) * rainLvl;
        // rain on the roof
        if (roofLvl > 0.0005f) {
            if (urand() < 900.0f / sr) b.patter = 0.5f + 0.5f * urand();
            float p = b.roof.run(n1 * b.patter);
            b.patter *= 0.994f;
            float rb = b.rumble.run(n2);
            L += (p * 0.9f + rb * 1.6f) * roofLvl; R += (p * 0.8f + rb * 1.6f) * roofLvl;
        }
        // sea
        if (seaLvl > 0.0005f) {
            float wash = 0.45f + 0.35f * std::sin(b.t * 0.7f) + 0.2f * std::sin(b.t * 0.43f + 1.0f);
            L += b.seaL.run(n1) * seaLvl * wash * 2.4f;
            R += b.seaR.run(n2) * seaLvl * wash * 2.4f;
        }
        // drone
        {
            static const float df[4] = {41.2f, 41.55f, 61.7f, 87.3f};
            float d = 0;
            for (int k = 0; k < 4; k++) {
                b.dph[k] += df[k] / sr;
                if (b.dph[k] >= 1.0f) b.dph[k] -= 1.0f;
                float saw = b.dph[k] * 2.0f - 1.0f;
                d += saw * (k < 2 ? 0.5f : (k == 2 ? 0.25f * e.drone : 0.18f * e.drone * e.drone));
            }
            d = b.droneLP.run(d) * droneLvl * (0.8f + 0.2f * std::sin(b.t * 0.5f));
            L += d; R += d;
        }
        // interference
        if (interLvl > 0.0002f) {
            b.interPh += b.interF / sr;
            if (b.interPh >= 1.0f) b.interPh -= 1.0f;
            float sq = std::sin(TAU * b.interPh + 2.0f * n1);
            float st = b.interBP.run(n2) * b.interGate;
            float s = (sq * 0.35f + st * 1.3f) * interLvl;
            L += s; R += s * 0.9f;
        }
        // heartbeat
        if (e.heart > 0.02f) {
            float bpm = 62.0f + 78.0f * saturate(e.heart);
            b.heartT += bpm / 60.0f / sr;
            if (b.heartT >= 1.0f) { b.heartT -= 1.0f; b.heartEnv1 = 1.0f; }
            if (b.heartT >= 0.27f && b.heartT < 0.27f + 2.0f * bpm / 60.0f / sr) b.heartEnv2 = 0.75f;
            b.heartPh += 52.0f / sr;
            if (b.heartPh >= 1.0f) b.heartPh -= 1.0f;
            float s = std::sin(TAU * b.heartPh) * (b.heartEnv1 + b.heartEnv2) * 0.34f * saturate(e.heart * 1.2f);
            b.heartEnv1 *= std::exp(-22.0f / sr);
            b.heartEnv2 *= std::exp(-26.0f / sr);
            L += s; R += s;
        }
        out[i * 2] += L * duck;
        out[i * 2 + 1] += R * duck;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
int audio_init_steps() { return S_COUNT + 1; }

void audio_init_step(int step, int sampleRate) {
    if (step == 0) {
        g_outRate = sampleRate > 8000 ? sampleRate : 48000;
        for (int i = 0; i < S_COUNT; i++) { g_snd[i].clear(); g_sndGain[i] = 1.0f; }
        static const int cd[2][4] = {{1557, 1617, 1491, 1422}, {1580, 1640, 1514, 1445}};
        static const int ad[2][2] = {{225, 556}, {248, 579}};
        float k = (float)g_outRate / 44100.0f;
        for (int c = 0; c < 2; c++) {
            for (int i = 0; i < 4; i++) g_comb[c][i].init((int)(cd[c][i] * k * 1.6f));
            for (int i = 0; i < 2; i++) g_ap[c][i].init((int)(ad[c][i] * k));
        }
        g_ns = 0x1982ABCDu;
        return;
    }
    int id = step - 1;
    if (id >= 0 && id < S_COUNT) genSound(id);
    if (id == S_COUNT - 1) g_ready = true;
}

bool audio_ready() { return g_ready; }

void audio_mix(float* out, int frames) {
    for (int i = 0; i < frames * 2; i++) out[i] = 0.0f;
    if (!g_ready) return;
    // smooth the environment
    {
        float k = 1.0f - std::exp(-(float)frames / (float)g_outRate * 4.0f);
        AudioEnv& c = g_envCur;
        const AudioEnv& t = g_env;
        c.wind += (t.wind - c.wind) * k; c.rain += (t.rain - c.rain) * k; c.indoor += (t.indoor - c.indoor) * k;
        c.sea += (t.sea - c.sea) * k; c.drone += (t.drone - c.drone) * k; c.interference += (t.interference - c.interference) * k;
        c.heart += (t.heart - c.heart) * k; c.master += (t.master - c.master) * k; c.duck += (t.duck - c.duck) * k;
        c.height += (t.height - c.height) * k;
    }
    static std::vector<float> send;
    send.assign((size_t)frames, 0.0f);
    float invRate = 1.0f / (float)g_outRate;
    for (int vi = 0; vi < MAXV; vi++) {
        Voice& v = g_v[vi];
        if (!v.active) continue;
        const Sound& s = g_snd[v.snd][(size_t)v.var];
        size_t len = s.pcm.size();
        if (len < 4) { v.active = false; continue; }
        float tl, tr, lpA;
        voice_gains(v, tl, tr, lpA);
        if (v.curL < 0) { v.curL = v.loop ? 0.0f : tl; v.curR = v.loop ? 0.0f : tr; }
        double step = (double)v.pitch * (double)s.rate * (double)invRate;
        float gk = 1.0f - std::exp(-invRate * 60.0f);
        const int16_t* pcm = s.pcm.data();
        for (int i = 0; i < frames; i++) {
            size_t i0 = (size_t)v.pos;
            if (i0 >= len) {
                if (v.loop) { v.pos -= (double)len * std::floor(v.pos / (double)len); i0 = (size_t)v.pos; if (i0 >= len) i0 = 0; }
                else { v.active = false; break; }
            }
            size_t i1 = i0 + 1;
            if (i1 >= len) i1 = v.loop ? 0 : i0;
            float fr = (float)(v.pos - (double)i0);
            float x = ((float)pcm[i0] + ((float)pcm[i1] - (float)pcm[i0]) * fr) * (1.0f / 32768.0f);
            v.lp += lpA * (x - v.lp);
            x = v.lp;
            v.curL += (tl - v.curL) * gk;
            v.curR += (tr - v.curR) * gk;
            out[i * 2] += x * v.curL;
            out[i * 2 + 1] += x * v.curR;
            send[(size_t)i] += x * (v.curL + v.curR) * v.send;
            v.pos += step;
            if (v.fadeRate > 0) {
                v.fade -= v.fadeRate * invRate;
                if (v.fade <= 0) { v.active = false; break; }
                if ((i & 15) == 0) voice_gains(v, tl, tr, lpA);
            }
        }
    }
    // beds
    beds(out, frames);
    // reverb (bigger and wetter outdoors)
    float fb = lerpf(0.84f, 0.7f, g_envCur.indoor);
    float wet = lerpf(0.5f, 0.3f, g_envCur.indoor);
    for (int c = 0; c < 2; c++)
        for (int k = 0; k < 4; k++) g_comb[c][k].fb = fb;
    float peak = 0;
    float master = clampv(g_envCur.master, 0.0f, 1.5f);
    for (int i = 0; i < frames; i++) {
        float in = send[(size_t)i];
        for (int c = 0; c < 2; c++) {
            float y = 0;
            for (int k = 0; k < 4; k++) y += g_comb[c][k].run(in);
            y = g_ap[c][0].run(y * 0.25f);
            y = g_ap[c][1].run(y);
            float o = (out[i * 2 + c] + y * wet) * master;
            // soft limiter: asymptotically approaches +-1, never clips
            o *= 1.5f;
            o = o / (1.0f + std::fabs(o));
            if (!(o == o)) o = 0.0f;
            o = clampv(o, -1.0f, 1.0f);
            out[i * 2 + c] = o;
            peak = std::max(peak, std::fabs(o));
        }
    }
    g_peak = std::max(peak, g_peak * 0.95f);
}

int audio_play(int snd, float gain, float pitch, float pan) { return start_voice(snd, gain, pitch, false, false, Vec3(), 1.0f, pan); }
int audio_play_at(int snd, const Vec3& pos, float gain, float pitch, float refDist) { return start_voice(snd, gain, pitch, false, true, pos, refDist, 0.0f); }
int audio_loop(int snd, float gain, float pitch) { return start_voice(snd, gain, pitch, true, false, Vec3(), 1.0f, 0.0f); }
int audio_loop_at(int snd, const Vec3& pos, float gain, float refDist, float pitch) { return start_voice(snd, gain, pitch, true, true, pos, refDist, 0.0f); }

void audio_stop(int handle, float fade) {
    Voice* v = find_voice(handle);
    if (!v) return;
    v->fadeRate = 1.0f / std::max(0.01f, fade);
}
void audio_set(int handle, float gain, float pitch) {
    Voice* v = find_voice(handle);
    if (!v) return;
    v->gain = gain * g_sndGain[v->snd];
    if (pitch > 0) v->pitch = pitch;
}
void audio_move(int handle, const Vec3& pos) {
    Voice* v = find_voice(handle);
    if (v) v->wpos = pos;
}
bool audio_active(int handle) { return find_voice(handle) != nullptr; }
void audio_listener(const Vec3& pos, float yaw) { g_lpos = pos; g_lyaw = yaw; }
void audio_env(const AudioEnv& e) { g_env = e; }
void audio_stop_all(float fade) {
    for (int i = 0; i < MAXV; i++)
        if (g_v[i].active) g_v[i].fadeRate = 1.0f / std::max(0.01f, fade);
}
float audio_peak() { return g_peak; }
