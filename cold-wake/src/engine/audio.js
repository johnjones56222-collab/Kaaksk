// Everything you hear is synthesised here with the Web Audio API: effects,
// room tone, the reactor, alarms, suit breathing and a slow generative score.
import { settings } from '../core/state.js';
import { clamp } from '../core/utils.js';

const midi = (n) => 440 * Math.pow(2, (n - 69) / 12);

class AudioEngine {
  constructor() {
    this.ctx = null;
    this.ready = false;
    this.loops = {};
    this.music = { mood: 'dark', next: 0, step: 0, voices: [], arp: 0, on: true, pulse: 0 };
    this.jukebox = { on: false, next: 0, step: 0, gain: null, vol: 0 };
    this.state = { power: 0, eva: false, alarm: 0, reactor: 0, reactorDist: 99, fire: 0, room: '', muffled: 0 };
    this.creak = 6;
  }

  init() {
    if (this.ctx) {
      this.resume();
      return;
    }
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    try {
      this.ctx = new AC({ latencyHint: 'interactive' });
    } catch {
      return;
    }
    const c = this.ctx;
    this.master = c.createGain();
    this.comp = c.createDynamicsCompressor();
    this.comp.threshold.value = -16;
    this.comp.ratio.value = 5;
    this.master.connect(this.comp).connect(c.destination);
    this.sfxBus = c.createGain();
    this.muffle = c.createBiquadFilter();
    this.muffle.type = 'lowpass';
    this.muffle.frequency.value = 20000;
    this.sfxBus.connect(this.muffle).connect(this.master);
    this.ambBus = c.createGain();
    this.ambBus.connect(this.muffle);
    this.musicBus = c.createGain();
    this.musicBus.connect(this.master);
    this.uiBus = c.createGain();
    this.uiBus.connect(this.master);
    // generated impulse response for a metal-room tail
    const len = Math.floor(c.sampleRate * 2.2);
    const ir = c.createBuffer(2, len, c.sampleRate);
    for (let ch = 0; ch < 2; ch++) {
      const d = ir.getChannelData(ch);
      for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 2.6);
    }
    this.reverb = c.createConvolver();
    this.reverb.buffer = ir;
    this.revSend = c.createGain();
    this.revSend.gain.value = 0.22;
    this.sfxBus.connect(this.revSend).connect(this.reverb);
    this.musicSend = c.createGain();
    this.musicSend.gain.value = 0.5;
    this.musicBus.connect(this.musicSend).connect(this.reverb);
    this.reverb.connect(this.master);
    // shared noise
    const nlen = c.sampleRate * 2;
    this.noiseBuf = c.createBuffer(1, nlen, c.sampleRate);
    const nd = this.noiseBuf.getChannelData(0);
    for (let i = 0; i < nlen; i++) nd[i] = Math.random() * 2 - 1;
    this.ready = true;
    this.applyVolumes();
    this._startLoops();
    this.resume();
  }

  resume() {
    if (this.ctx && this.ctx.state === 'suspended') this.ctx.resume().catch(() => {});
  }

  suspend() {
    if (this.ctx && this.ctx.state === 'running') this.ctx.suspend().catch(() => {});
  }

  applyVolumes() {
    if (!this.ready) return;
    const t = this.ctx.currentTime;
    this.master.gain.setTargetAtTime(settings.master, t, 0.05);
    this.sfxBus.gain.setTargetAtTime(settings.sfx, t, 0.05);
    this.ambBus.gain.setTargetAtTime(settings.sfx * 0.9, t, 0.05);
    this.uiBus.gain.setTargetAtTime(settings.sfx * 0.8, t, 0.05);
    this.musicBus.gain.setTargetAtTime(settings.music * 0.7, t, 0.05);
  }

  // ---- building blocks -------------------------------------------------------

  tone({ type = 'sine', f0 = 440, f1 = null, dur = 0.2, vol = 0.2, attack = 0.005, delay = 0, bus = null, detune = 0, pan = 0 }) {
    if (!this.ready) return;
    const c = this.ctx;
    const t = c.currentTime + delay;
    const o = c.createOscillator();
    const g = c.createGain();
    o.type = type;
    o.frequency.setValueAtTime(f0, t);
    if (f1 != null) o.frequency.exponentialRampToValueAtTime(Math.max(1, f1), t + dur);
    o.detune.value = detune;
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(Math.max(0.0002, vol), t + attack);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    let out = g;
    if (pan && c.createStereoPanner) {
      const p = c.createStereoPanner();
      p.pan.value = clamp(pan, -1, 1);
      g.connect(p);
      out = p;
    }
    o.connect(g);
    out.connect(bus || this.sfxBus);
    o.start(t);
    o.stop(t + dur + 0.05);
  }

  noise({ dur = 0.3, vol = 0.2, type = 'bandpass', f0 = 800, f1 = null, q = 1, attack = 0.005, delay = 0, bus = null, pan = 0 }) {
    if (!this.ready) return;
    const c = this.ctx;
    const t = c.currentTime + delay;
    const s = c.createBufferSource();
    s.buffer = this.noiseBuf;
    s.loop = true;
    const f = c.createBiquadFilter();
    f.type = type;
    f.Q.value = q;
    f.frequency.setValueAtTime(f0, t);
    if (f1 != null) f.frequency.exponentialRampToValueAtTime(Math.max(20, f1), t + dur);
    const g = c.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(Math.max(0.0002, vol), t + attack);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    let out = g;
    if (pan && c.createStereoPanner) {
      const p = c.createStereoPanner();
      p.pan.value = clamp(pan, -1, 1);
      g.connect(p);
      out = p;
    }
    s.connect(f).connect(g);
    out.connect(bus || this.sfxBus);
    s.start(t, Math.random() * 1.5);
    s.stop(t + dur + 0.05);
  }

  // ---- one-shots -------------------------------------------------------------

  // vol scales the recipe; pan -1..1 places it left/right.
  sfx(name, o = {}) {
    if (!this.ready) return;
    const v = o.vol ?? 1;
    const pan = o.pan || 0;
    const ui = this.uiBus;
    switch (name) {
      case 'step':
        this.noise({ dur: 0.09, vol: 0.16 * v, f0: 260 + Math.random() * 90, f1: 120, q: 1.2 });
        this.tone({ f0: 95 + Math.random() * 20, f1: 60, dur: 0.07, vol: 0.12 * v });
        break;
      case 'stepMag':
        this.tone({ f0: 150, f1: 70, dur: 0.12, vol: 0.3 * v });
        this.noise({ dur: 0.05, vol: 0.2 * v, f0: 1800, q: 2 });
        this.tone({ type: 'triangle', f0: 620, f1: 540, dur: 0.18, vol: 0.05 * v, delay: 0.02 });
        break;
      case 'jump':
        this.noise({ dur: 0.12, vol: 0.1 * v, f0: 500, f1: 900, q: 0.8 });
        break;
      case 'land':
        this.tone({ f0: 110, f1: 50, dur: 0.14, vol: 0.3 * v });
        this.noise({ dur: 0.1, vol: 0.18 * v, f0: 300, f1: 100 });
        break;
      case 'doorOpen':
        this.noise({ dur: 0.5, vol: 0.2 * v, f0: 380, f1: 1500, q: 1.4, pan });
        this.tone({ type: 'sawtooth', f0: 70, f1: 130, dur: 0.45, vol: 0.07 * v, pan });
        this.tone({ f0: 1200, dur: 0.06, vol: 0.05 * v, pan });
        break;
      case 'doorClose':
        this.noise({ dur: 0.5, vol: 0.18 * v, f0: 1400, f1: 320, q: 1.4, pan });
        this.tone({ type: 'sawtooth', f0: 120, f1: 65, dur: 0.45, vol: 0.06 * v, pan });
        break;
      case 'doorShut':
        this.tone({ f0: 85, f1: 45, dur: 0.22, vol: 0.34 * v, pan });
        this.noise({ dur: 0.12, vol: 0.18 * v, f0: 240, pan });
        break;
      case 'ui':
        this.tone({ f0: 880, dur: 0.05, vol: 0.12 * v, bus: ui });
        break;
      case 'uiHi':
        this.tone({ f0: 1320, dur: 0.05, vol: 0.1 * v, bus: ui });
        break;
      case 'click':
        this.noise({ dur: 0.03, vol: 0.2 * v, f0: 2600, q: 3, bus: ui });
        this.tone({ f0: 300, f1: 180, dur: 0.04, vol: 0.12 * v, bus: ui });
        break;
      case 'confirm':
        this.tone({ type: 'triangle', f0: 660, dur: 0.1, vol: 0.16 * v, bus: ui });
        this.tone({ type: 'triangle', f0: 990, dur: 0.16, vol: 0.16 * v, delay: 0.09, bus: ui });
        break;
      case 'deny':
        this.tone({ type: 'square', f0: 170, dur: 0.12, vol: 0.1 * v, bus: ui });
        this.tone({ type: 'square', f0: 128, dur: 0.2, vol: 0.1 * v, delay: 0.12, bus: ui });
        break;
      case 'pickup':
        [660, 880, 1320].forEach((f, i) => this.tone({ type: 'triangle', f0: f, dur: 0.12, vol: 0.14 * v, delay: i * 0.06, bus: ui }));
        break;
      case 'log':
        this.tone({ f0: 740, dur: 0.14, vol: 0.12 * v, bus: ui });
        this.tone({ f0: 1110, dur: 0.24, vol: 0.1 * v, delay: 0.1, bus: ui });
        break;
      case 'objective':
        [72, 76, 79, 84].forEach((n, i) => this.tone({ type: 'triangle', f0: midi(n), dur: 0.5, vol: 0.13 * v, delay: i * 0.09, bus: ui }));
        break;
      case 'mission':
        [60, 67, 72, 76, 79, 84].forEach((n, i) => this.tone({ type: 'triangle', f0: midi(n), dur: 1.0, vol: 0.12 * v, delay: i * 0.11, bus: ui }));
        this.tone({ f0: midi(36), dur: 1.6, vol: 0.2 * v, bus: ui });
        break;
      case 'hiss':
        this.noise({ dur: o.dur || 1.4, vol: 0.22 * v, type: 'highpass', f0: 2400, f1: 5200, attack: 0.03, pan });
        break;
      case 'powerUp':
        this.tone({ type: 'sawtooth', f0: 45, f1: 220, dur: 1.8, vol: 0.16 * v, attack: 0.3 });
        this.tone({ f0: 90, f1: 660, dur: 1.9, vol: 0.1 * v, attack: 0.4 });
        this.noise({ dur: 2.0, vol: 0.12 * v, f0: 200, f1: 3000, q: 0.6, attack: 0.5 });
        this.tone({ f0: 55, dur: 1.2, vol: 0.35 * v, delay: 1.75 });
        break;
      case 'powerDown':
        this.tone({ type: 'sawtooth', f0: 200, f1: 30, dur: 1.4, vol: 0.16 * v });
        break;
      case 'spark':
        this.noise({ dur: 0.05 + Math.random() * 0.05, vol: 0.14 * v, type: 'highpass', f0: 3000 + Math.random() * 2000, pan });
        break;
      case 'laser':
        this.tone({ type: 'sawtooth', f0: 1700, f1: 200, dur: 0.16, vol: 0.14 * v, pan });
        this.noise({ dur: 0.06, vol: 0.12 * v, type: 'highpass', f0: 4000, pan });
        break;
      case 'explosion':
        this.noise({ dur: 1.0, vol: 0.5 * v, type: 'lowpass', f0: 1400, f1: 70, attack: 0.01, pan });
        this.tone({ f0: 80, f1: 32, dur: 0.9, vol: 0.45 * v, pan });
        break;
      case 'impact':
        this.tone({ f0: 60, f1: 28, dur: 0.7, vol: 0.6 * v });
        this.noise({ dur: 0.6, vol: 0.4 * v, type: 'lowpass', f0: 500, f1: 60 });
        this.noise({ dur: 0.3, vol: 0.14 * v, f0: 2400, q: 4, delay: 0.05 });
        break;
      case 'switch':
        this.tone({ type: 'square', f0: 220, f1: 140, dur: 0.05, vol: 0.12 * v, bus: ui });
        this.noise({ dur: 0.03, vol: 0.18 * v, f0: 3200, q: 2, bus: ui });
        break;
      case 'key':
        this.tone({ f0: 1046 + (o.n || 0) * 60, dur: 0.07, vol: 0.12 * v, bus: ui });
        break;
      case 'valve':
        this.tone({ type: 'sawtooth', f0: 82, f1: 96, dur: 0.5, vol: 0.07 * v });
        this.noise({ dur: 0.5, vol: 0.07 * v, f0: 700, q: 6 });
        break;
      case 'weldDone':
        this.tone({ f0: 520, f1: 1040, dur: 0.25, vol: 0.14 * v });
        this.noise({ dur: 0.3, vol: 0.12 * v, type: 'highpass', f0: 3000 });
        break;
      case 'blip':
        this.tone({ type: 'sine', f0: (o.f || 480) + Math.random() * 120, dur: 0.035, vol: 0.05 * v, bus: ui });
        break;
      case 'thaw':
        this.noise({ dur: 2.4, vol: 0.2 * v, type: 'highpass', f0: 1500, f1: 4500, attack: 0.2 });
        this.tone({ f0: 60, f1: 110, dur: 1.5, vol: 0.2 * v, attack: 0.4 });
        break;
      case 'vegaBoot':
        [48, 55, 60, 64, 67, 72, 76].forEach((n, i) => this.tone({ type: 'sine', f0: midi(n), dur: 1.6, vol: 0.09 * v, delay: i * 0.16 }));
        this.noise({ dur: 2.2, vol: 0.06 * v, type: 'highpass', f0: 5000, attack: 0.8 });
        break;
      case 'jumpCharge':
        this.tone({ type: 'sawtooth', f0: 40, f1: 880, dur: 6.5, vol: 0.2 * v, attack: 2.5 });
        this.tone({ f0: 80, f1: 1760, dur: 6.5, vol: 0.1 * v, attack: 3 });
        this.noise({ dur: 6.5, vol: 0.22 * v, f0: 200, f1: 6000, q: 0.5, attack: 4 });
        break;
      case 'jumpBoom':
        this.tone({ f0: 70, f1: 24, dur: 2.6, vol: 0.7 * v });
        this.noise({ dur: 3, vol: 0.5 * v, type: 'lowpass', f0: 4000, f1: 60 });
        [84, 91, 96].forEach((n, i) => this.tone({ f0: midi(n), dur: 3, vol: 0.05 * v, delay: 0.2 + i * 0.1 }));
        break;
      case 'coffee':
        this.noise({ dur: 1.6, vol: 0.12 * v, f0: 900, f1: 500, q: 2, attack: 0.2 });
        this.tone({ f0: 1568, dur: 0.2, vol: 0.1 * v, delay: 1.7 });
        break;
      case 'hurt':
        this.tone({ type: 'sawtooth', f0: 180, f1: 90, dur: 0.2, vol: 0.14 * v });
        break;
      default:
        break;
    }
  }

  // ---- continuous layers -----------------------------------------------------

  _noiseLoop(filterType, freq, q) {
    const c = this.ctx;
    const s = c.createBufferSource();
    s.buffer = this.noiseBuf;
    s.loop = true;
    const f = c.createBiquadFilter();
    f.type = filterType;
    f.frequency.value = freq;
    f.Q.value = q;
    const g = c.createGain();
    g.gain.value = 0;
    s.connect(f).connect(g).connect(this.ambBus);
    s.start();
    return { g, f };
  }

  _oscLoop(type, freq, bus = this.ambBus) {
    const c = this.ctx;
    const o = c.createOscillator();
    o.type = type;
    o.frequency.value = freq;
    const g = c.createGain();
    g.gain.value = 0;
    o.connect(g).connect(bus);
    o.start();
    return { g, o };
  }

  _startLoops() {
    const c = this.ctx;
    const L = this.loops;
    L.hum1 = this._oscLoop('sine', 55);
    L.hum2 = this._oscLoop('sine', 82.6);
    L.air = this._noiseLoop('bandpass', 700, 0.4);
    L.rumble = this._noiseLoop('lowpass', 90, 0.7);
    L.eerie1 = this._oscLoop('sine', 110);
    L.eerie2 = this._oscLoop('sine', 116.5);
    // reactor: a throbbing low saw through a filter
    const rs = c.createOscillator();
    rs.type = 'sawtooth';
    rs.frequency.value = 41;
    const rf = c.createBiquadFilter();
    rf.type = 'lowpass';
    rf.frequency.value = 180;
    const rg = c.createGain();
    rg.gain.value = 0;
    const lfo = c.createOscillator();
    lfo.frequency.value = 0.8;
    const lg = c.createGain();
    lg.gain.value = 60;
    lfo.connect(lg).connect(rf.frequency);
    rs.connect(rf).connect(rg).connect(this.ambBus);
    rs.start();
    lfo.start();
    L.reactor = { g: rg };
    // alarm: two alternating tones
    const al = c.createOscillator();
    al.type = 'square';
    al.frequency.value = 520;
    const alf = c.createBiquadFilter();
    alf.type = 'lowpass';
    alf.frequency.value = 1400;
    const alg = c.createGain();
    alg.gain.value = 0;
    const alLfo = c.createOscillator();
    alLfo.type = 'square';
    alLfo.frequency.value = 1.3;
    const alLg = c.createGain();
    alLg.gain.value = 110;
    alLfo.connect(alLg).connect(al.frequency);
    al.connect(alf).connect(alg).connect(this.ambBus);
    al.start();
    alLfo.start();
    L.alarm = { g: alg };
    L.fire = this._noiseLoop('bandpass', 520, 0.6);
    L.spray = this._noiseLoop('highpass', 2600, 0.5);
    L.weld = this._noiseLoop('bandpass', 3400, 2.5);
    L.weldBuzz = this._oscLoop('sawtooth', 118);
    L.water = this._noiseLoop('highpass', 3800, 0.8);
    // suit breathing goes straight to the master so the helmet muffle skips it
    const bs = c.createBufferSource();
    bs.buffer = this.noiseBuf;
    bs.loop = true;
    const bf = c.createBiquadFilter();
    bf.type = 'bandpass';
    bf.frequency.value = 620;
    bf.Q.value = 1.1;
    const bg = c.createGain();
    bg.gain.value = 0;
    bs.connect(bf).connect(bg).connect(this.master);
    bs.start();
    L.breath = { g: bg, f: bf };
    this.jukebox.gain = c.createGain();
    this.jukebox.gain.gain.value = 0;
    this.jukebox.gain.connect(this.sfxBus);
  }

  setLoop(name, vol, time = 0.15) {
    if (!this.ready) return;
    const l = this.loops[name];
    if (l) l.g.gain.setTargetAtTime(Math.max(0, vol), this.ctx.currentTime, time);
  }

  // Called every frame with the listener's situation.
  update(dt, s) {
    if (!this.ready) return;
    Object.assign(this.state, s);
    const st = this.state;
    const t = this.ctx.currentTime;
    const p = st.power;
    const inside = st.eva ? 0 : 1;
    this.setLoop('hum1', (0.02 + 0.07 * p) * inside, 0.4);
    this.setLoop('hum2', 0.04 * p * inside, 0.4);
    this.setLoop('air', (0.008 + 0.022 * p) * inside, 0.4);
    this.setLoop('rumble', 0.1 * (0.5 + 0.5 * p), 0.4);
    const dark = (1 - p) * inside;
    this.setLoop('eerie1', 0.022 * dark, 0.8);
    this.setLoop('eerie2', 0.02 * dark, 0.8);
    const near = clamp(1 - st.reactorDist / 26, 0, 1);
    this.setLoop('reactor', st.reactor * (0.04 + 0.3 * near * near) * inside, 0.3);
    this.setLoop('alarm', st.alarm * 0.035 * (st.eva ? 0.3 : 1), 0.2);
    this.setLoop('fire', st.fire * 0.16, 0.2);
    this.setLoop('water', st.room === 'hydro' ? 0.012 + 0.012 * p : 0, 0.6);
    // helmet muffle
    this.muffle.frequency.setTargetAtTime(st.eva ? 520 : 20000, t, 0.25);
    // breathing: slow in/out swell
    if (st.eva) {
      const ph = (t % 4.2) / 4.2;
      const inh = ph < 0.42 ? Math.sin((ph / 0.42) * Math.PI) : 0;
      const exh = ph > 0.5 && ph < 0.95 ? Math.sin(((ph - 0.5) / 0.45) * Math.PI) : 0;
      this.loops.breath.g.gain.setTargetAtTime(0.05 * inh + 0.035 * exh, t, 0.05);
      this.loops.breath.f.frequency.setTargetAtTime(inh > 0 ? 760 : 480, t, 0.1);
    } else {
      this.loops.breath.g.gain.setTargetAtTime(0, t, 0.2);
    }
    // occasional hull creaks while the ship is dark
    this.creak -= dt;
    if (this.creak <= 0) {
      this.creak = 7 + Math.random() * 12;
      if (dark > 0.4 && Math.random() < 0.8) {
        const f = 70 + Math.random() * 60;
        this.tone({ type: 'sawtooth', f0: f, f1: f * (0.8 + Math.random() * 0.5), dur: 0.9 + Math.random(), vol: 0.035, attack: 0.3, pan: Math.random() * 2 - 1 });
        this.noise({ dur: 0.7, vol: 0.03, f0: 300 + Math.random() * 400, q: 8, attack: 0.2 });
      }
    }
    this._music(dt);
    this._jukebox(dt);
  }

  // ---- score -------------------------------------------------------------------

  setMood(mood) {
    if (this.music.mood === mood) return;
    this.music.mood = mood;
    this.music.next = Math.min(this.music.next, 1.5);
  }

  _music(dt) {
    const m = this.music;
    if (!m.on) return;
    m.next -= dt;
    m.arp -= dt;
    m.pulse -= dt;
    const moods = {
      dark: { chords: [[45, 52, 55, 60], [41, 48, 53, 56], [43, 50, 55, 58], [40, 47, 52, 55]], len: 11, vol: 0.05, cut: 520, arp: 0 },
      calm: { chords: [[45, 52, 57, 60, 64], [41, 48, 57, 60, 65], [48, 55, 60, 64, 67], [43, 50, 59, 62, 67]], len: 9.5, vol: 0.055, cut: 900, arp: 0.5 },
      wonder: { chords: [[50, 57, 62, 66, 69], [47, 54, 62, 66, 71], [55, 62, 67, 71, 74], [45, 52, 61, 64, 69]], len: 10, vol: 0.06, cut: 1100, arp: 0.7 },
      tense: { chords: [[40, 47, 52, 55], [41, 48, 52, 56], [40, 46, 52, 55], [39, 46, 51, 54]], len: 6, vol: 0.06, cut: 700, arp: 0.2, pulse: 0.42 },
      action: { chords: [[38, 45, 50, 53], [36, 43, 48, 52], [41, 48, 53, 57], [43, 50, 55, 58]], len: 4.8, vol: 0.06, cut: 1200, arp: 0.6, pulse: 0.3 },
      menu: { chords: [[45, 52, 57, 60, 64], [48, 55, 60, 64, 67], [41, 48, 57, 60, 65], [43, 50, 55, 59, 62]], len: 10, vol: 0.06, cut: 1000, arp: 0.6 },
    };
    const mood = moods[m.mood] || moods.calm;
    const c = this.ctx;
    const t = c.currentTime;
    if (m.next <= 0) {
      m.next = mood.len;
      const chord = mood.chords[m.step % mood.chords.length];
      m.step++;
      m.current = chord;
      const dur = mood.len * 1.35;
      chord.forEach((n, i) => {
        for (const det of [-7, 7]) {
          const o = c.createOscillator();
          o.type = i === 0 ? 'triangle' : 'sawtooth';
          o.frequency.value = midi(n);
          o.detune.value = det + (Math.random() - 0.5) * 6;
          const f = c.createBiquadFilter();
          f.type = 'lowpass';
          f.frequency.setValueAtTime(mood.cut * 0.5, t);
          f.frequency.linearRampToValueAtTime(mood.cut, t + dur * 0.45);
          f.frequency.linearRampToValueAtTime(mood.cut * 0.4, t + dur);
          const g = c.createGain();
          const v = (i === 0 ? mood.vol * 1.5 : mood.vol * 0.55) / 2;
          g.gain.setValueAtTime(0.0001, t);
          g.gain.linearRampToValueAtTime(v, t + dur * 0.32);
          g.gain.linearRampToValueAtTime(v * 0.8, t + dur * 0.7);
          g.gain.linearRampToValueAtTime(0.0001, t + dur);
          o.connect(f).connect(g).connect(this.musicBus);
          o.start(t);
          o.stop(t + dur + 0.1);
        }
      });
    }
    if (mood.arp && m.arp <= 0 && m.current) {
      m.arp = 1.2 + Math.random() * 2.6;
      if (Math.random() < mood.arp) {
        const n = m.current[1 + Math.floor(Math.random() * (m.current.length - 1))] + 12 * (1 + Math.floor(Math.random() * 2));
        this.tone({ type: 'sine', f0: midi(n), dur: 1.8, vol: 0.045, attack: 0.01, bus: this.musicBus, pan: Math.random() - 0.5 });
        this.tone({ type: 'triangle', f0: midi(n + 7), dur: 1.2, vol: 0.02, delay: 0.3, bus: this.musicBus });
      }
    }
    if (mood.pulse && m.pulse <= 0 && m.current) {
      m.pulse = mood.pulse;
      this.tone({ type: 'sawtooth', f0: midi(m.current[0] - 12), f1: midi(m.current[0] - 12) * 0.98, dur: mood.pulse * 0.8, vol: 0.07, bus: this.musicBus });
    }
  }

  // The lounge jukebox plays a small looping tune that fades with distance.
  toggleJukebox() {
    this.jukebox.on = !this.jukebox.on;
    this.jukebox.next = 0;
    return this.jukebox.on;
  }

  setJukeboxVolume(v) {
    this.jukebox.vol = v;
  }

  _jukebox(dt) {
    const j = this.jukebox;
    if (!j.gain) return;
    j.gain.gain.setTargetAtTime(j.on ? j.vol : 0, this.ctx.currentTime, 0.2);
    if (!j.on) return;
    j.next -= dt;
    if (j.next > 0) return;
    const beat = 0.26;
    j.next = beat;
    const mel = [69, 72, 76, 72, 74, 77, 81, 77, 72, 76, 79, 76, 71, 74, 79, 74];
    const bass = [45, 45, 50, 50, 48, 48, 43, 43];
    const i = j.step++;
    const n = mel[i % mel.length];
    if (i % 2 === 0 || Math.random() < 0.5) this.tone({ type: 'square', f0: midi(n), dur: beat * 1.6, vol: 0.06, bus: j.gain });
    if (i % 2 === 0) this.tone({ type: 'triangle', f0: midi(bass[(i >> 1) % bass.length]), dur: beat * 1.9, vol: 0.12, bus: j.gain });
    if (i % 4 === 2) this.noise({ dur: 0.05, vol: 0.05, type: 'highpass', f0: 6000, bus: j.gain });
    if (i % 8 === 0) this.tone({ f0: 110, f1: 50, dur: 0.12, vol: 0.14, bus: j.gain });
  }
}

export const audio = new AudioEngine();
