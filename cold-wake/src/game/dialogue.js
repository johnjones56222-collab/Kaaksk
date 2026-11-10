// Spoken lines: a queue of subtitles with a soft synthetic "voice" and an
// optional browser text-to-speech reading.
import { hud } from '../ui/hud.js';
import { audio } from '../engine/audio.js';
import { settings, state } from '../core/state.js';
import { game } from './ctx.js';

const queue = [];
let current = null;
let timer = 0;
let shown = 0;
let blip = 0;

function speak(text, sys) {
  if (!settings.voice) return;
  try {
    const synth = window.speechSynthesis;
    if (!synth) return;
    const u = new SpeechSynthesisUtterance(text);
    u.rate = sys ? 1.05 : 0.98;
    u.pitch = sys ? 0.6 : 1.15;
    u.volume = Math.min(1, settings.master * settings.sfx);
    synth.speak(u);
  } catch {
    /* speech is optional */
  }
}

// say('VEGA', 'text') - queued. opts.now flushes whatever is waiting.
export function say(speaker, text, opts = {}) {
  if (opts.now) {
    queue.length = 0;
    current = null;
    try {
      window.speechSynthesis?.cancel();
    } catch {
      /* ignore */
    }
  }
  queue.push({ speaker, text, sys: speaker !== 'VEGA' && speaker !== 'YOU' });
}

export function sayAll(speaker, lines, opts) {
  lines.forEach((l, i) => say(speaker, l, i === 0 ? opts : undefined));
}

export function clearDialogue() {
  queue.length = 0;
  current = null;
  hud.subtitle('', '');
  try {
    window.speechSynthesis?.cancel();
  } catch {
    /* ignore */
  }
}

export const dialogueBusy = () => !!current || queue.length > 0;

// Drop lines that are waiting and let the one on screen finish quickly, so
// speech never lags far behind what the player is doing.
export function catchUp() {
  queue.length = 0;
  if (current) timer = Math.min(timer, 0.9);
}

export function updateDialogue(dt) {
  if (!current) {
    if (!queue.length) return;
    current = queue.shift();
    timer = Math.max(2.4, current.text.length * 0.058) + 0.7;
    shown = 0;
    state.transcript.push([current.speaker, current.text]);
    if (state.transcript.length > 80) state.transcript.shift();
    speak(current.text, current.sys);
    if (!settings.subtitles) hud.subtitle('', '');
  }
  // typewriter reveal with voice blips
  const total = current.text.length;
  if (shown < total) {
    shown = Math.min(total, shown + dt * 46);
    if (settings.subtitles) hud.subtitle(current.speaker, current.text.slice(0, Math.floor(shown)), current.sys);
    blip -= dt;
    if (blip <= 0 && !settings.voice) {
      blip = 0.07;
      audio.sfx('blip', { f: current.sys ? 300 : 520, vol: 0.8 });
    }
    const core = game.world?.refs?.vegaCore;
    if (core && !current.sys) core.talk = 1;
  }
  timer -= dt;
  if (timer <= 0) {
    current = null;
    if (!queue.length) hud.subtitle('', '');
  }
}
