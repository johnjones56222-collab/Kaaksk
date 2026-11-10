// The jump: a short in-engine cinematic from the captain's station, then the
// summary screen.
import { game } from './ctx.js';
import { hud } from '../ui/hud.js';
import { audio } from '../engine/audio.js';
import { sayAll, clearDialogue } from './dialogue.js';
import { setFlag, state, saveGame } from '../core/state.js';
import { menus } from '../ui/menus.js';
import { smooth, damp, angleDelta } from '../core/utils.js';

export function runEnding() {
  const p = game.player;
  const lever = game.world.refs.jumpLever;
  game.mode = 'cinematic';
  hud.setLayout('none');
  hud.letterbox(true);
  p.frozen = true;
  p.toggleLamp(false);
  audio.sfx('switch');
  setFlag('jumped');
  clearDialogue();
  sayAll('VEGA', ['All hands, this is VEGA. Hold on to something.', 'Jump in three. Two. One.']);
  audio.sfx('jumpCharge');
  audio.setMood('wonder');
  let t = 0;
  let boomed = false;
  let finished = false;
  const CHARGE = 6.5;
  game.cinematic = (dt) => {
    t += dt;
    if (lever) lever.rotation.x = damp(lever.rotation.x, -0.55, 6, dt);
    const k = smooth(Math.min(1, t / CHARGE));
    // drift to the captain's eye-line, looking out over the bow
    p.x = damp(p.x, 0, 1.2, dt);
    p.z = damp(p.z, -43.2, 1.2, dt);
    p.yaw += angleDelta(p.yaw, 0) * Math.min(1, dt * 1.6);
    p.pitch = damp(p.pitch, 0.06, 1.6, dt);
    p.shake = Math.max(p.shake, k * 0.55);
    game.space.setJump(boomed ? 1 : k * 0.8);
    game.gfx.bloomBoost = k * 0.9 + (boomed ? 0.4 : 0);
    game.gfx.applyFov(k * 16 + (boomed ? 10 : 0));
    p.syncCamera(dt);
    if (!boomed && t >= CHARGE) {
      boomed = true;
      hud.flash(1.3);
      audio.sfx('jumpBoom');
    }
    if (!finished && t >= CHARGE + 3.2) {
      finished = true;
      hud.fade(1, 1.6).then(() => {
        state.finished = true;
        saveGame();
        game.cinematic = null;
        game.mode = 'ended';
        hud.show(false);
        menus.showEnd();
        hud.fade(0.55, 0.4);
      });
    }
  };
}
