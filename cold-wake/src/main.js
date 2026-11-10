// Cold Wake - entry point. Boots the renderer, builds the ship, and runs the
// game state machine: title -> intro -> play (with pause, consoles, gunnery,
// spacewalk and the closing cinematic).
import * as THREE from 'three';
import { Gfx } from './engine/gfx.js';
import { createLibrary } from './engine/materials.js';
import { initInput, input, requestLock, releaseLock, clearInput } from './engine/input.js';
import { audio } from './engine/audio.js';
import { World } from './world/world.js';
import { Space, SUN_DIR } from './world/space.js';
import { ROOM_BUILDERS } from './world/rooms/index.js';
import { ROOMS, R, roomAt, HULL_ORIGIN } from './world/layout.js';
import { Player } from './game/player.js';
import { Drone } from './world/drone.js';
import { Interact } from './game/interact.js';
import { Turret } from './game/turret.js';
import { game } from './game/ctx.js';
import { missions } from './game/missions.js';
import { setupContent, applyRules, updateContent, onRoomEnter } from './game/content.js';
import { updateDialogue, clearDialogue, say, dialogueBusy } from './game/dialogue.js';
import { runEnding } from './game/ending.js';
import { hud } from './ui/hud.js';
import { menus } from './ui/menus.js';
import { openMinigame, tickMinigame, solveMinigame, minigameOpen, abortMinigame } from './ui/minigames/base.js';
import { state, settings, loadSettings, loadGame, saveGame, resetState, clearSave, has, setFlag } from './core/state.js';
import { bus } from './core/events.js';
import { nextFrame, wait, clamp } from './core/utils.js';

const $ = (id) => document.getElementById(id);

function fatal(title, detail) {
  const f = $('fatal');
  f.innerHTML = `<div><h2>${title}</h2><p>${detail}</p></div>`;
  f.hidden = false;
  $('overlay-loading').hidden = true;
}

async function boot() {
  loadSettings();
  const qp = new URLSearchParams(location.search);
  if (qp.get('q')) settings.quality = qp.get('q');
  const canvas = $('game');
  let gfx;
  try {
    gfx = new Gfx(canvas);
  } catch (err) {
    console.error(err);
    fatal('3D graphics unavailable', 'Cold Wake needs WebGL 2. Try a current version of Chrome, Edge, Firefox or Safari, and make sure hardware acceleration is turned on.');
    return;
  }
  const loadFill = $('load-fill');
  const loadNote = $('load-note');
  const progress = (k, note) => {
    loadFill.style.width = `${Math.round(k * 100)}%`;
    if (note) loadNote.textContent = note;
  };
  progress(0.04, 'Fabricating hull plating');
  await nextFrame();
  await nextFrame();
  const lib = createLibrary(gfx.quality);
  progress(0.16, 'Charting the Aurelia system');
  await nextFrame();
  const space = new Space(gfx);
  progress(0.24, 'Laying the keel');
  await nextFrame();
  const world = new World(gfx, lib);
  await world.build(ROOM_BUILDERS, (k, name) => progress(0.24 + k * 0.7, `Fitting out: ${name}`));
  const player = new Player(world, gfx);
  Object.assign(game, { gfx, world, space, player, hud, audio, missions, menus, input });
  game.interact = new Interact();
  game.o2Max = 240;
  game.o2 = game.o2Max;
  game.coffeeT = 0;
  game.fx = { spray: 0, weld: 0 };
  game.cinematic = null;
  game.prevMode = 'play';
  game.dialogueBusy = dialogueBusy;
  hud.init();
  initInput(canvas);
  setupContent();
  game.turret = new Turret();
  game.drone = new Drone(gfx.scene);
  world.sun.position.copy(SUN_DIR);
  defineGameApi();
  wireEvents();
  progress(1, 'Ready');
  await nextFrame();
  // warm the shader cache with every room visible once, so play starts smoothly
  for (let i = 1; i < world.rooms.length; i++) world.rooms[i].group.visible = true;
  gfx.renderer.compile(gfx.scene, gfx.camera);
  $('overlay-loading').hidden = true;
  game.toTitle();
  let last = performance.now();
  let errors = 0;
  let pausedT = 0;
  const frame = (now) => {
    requestAnimationFrame(frame);
    const raw = now - last;
    last = now;
    const dt = clamp(raw / 1000, 0.0005, 0.05);
    game.frame = (game.frame || 0) + 1;
    try {
      if (game.mode === 'paused' || game.mode === 'ended' || game.mode === 'intro') {
        // keep the last frame; redraw occasionally so resizes still show
        pausedT += dt;
        handleMenuKeys();
        if (pausedT > 0.25) {
          pausedT = 0;
          if (game.mode !== 'intro') gfx.render();
        }
        updateAudioIdle(dt);
        input.endFrame();
        return;
      }
      gfx.adapt(raw);
      step(dt);
      gfx.render();
      input.endFrame();
      errors = 0;
    } catch (err) {
      console.error(err);
      input.endFrame();
      if (++errors > 60) fatal('Something went wrong', 'The game hit an error it could not recover from. Reload the page to continue from your last save.');
    }
  };
  requestAnimationFrame(frame);
  window.__ready = true;
}

function updateAudioIdle(dt) {
  audio.update(dt, {});
}

function handleMenuKeys() {
  if (game.mode !== 'paused') return;
  if (input.pressed('pause') || input.pressed('mission') || input.pressed('map')) game.resume();
}

const menuCam = { t: 0 };

function step(dt) {
  const { gfx, world, space, player } = game;
  const mode = game.mode;
  input.capture = mode === 'play';
  if (mode === 'menu') {
    // slow drift over the dorsal hull with the planet beyond the bow
    menuCam.t += dt;
    const t = menuCam.t;
    const X = HULL_ORIGIN.x;
    gfx.camera.position.set(X + 15 + Math.sin(t * 0.05) * 4, 7.5 + Math.sin(t * 0.07) * 1.2, 21 - Math.sin(t * 0.04) * 3);
    gfx.camera.lookAt(X - 16, 2.5, -60);
    world.update(dt, gfx.camera.position.x, gfx.camera.position.z, gfx.camera.position);
    space.update(dt, world.time);
    audio.setMood('menu');
    audio.update(dt, { power: 0.4, eva: false, alarm: 0, reactor: 0, reactorDist: 99, fire: 0, room: '' });
    return;
  }
  state.stats.time += dt;
  if (mode === 'turret') {
    game.turret.update(dt);
  } else if (mode === 'cinematic' && game.cinematic) {
    game.cinematic(dt);
  } else {
    player.update(dt);
  }
  world.update(dt, player.x, player.z, gfx.camera.position);
  space.update(dt, world.time);
  if (mode === 'play') {
    if (input.pressed('flash') && has('hasTool')) player.toggleLamp();
    game.interact.update(dt);
    missions.update(dt);
    if (input.pressed('pause')) game.pause('mission');
    else if (input.pressed('mission')) game.pause('mission');
    else if (input.pressed('map')) game.pause('map');
    else if (input.pressed('logs')) game.pause('logs');
    // nudge desktop players who have not captured the pointer
    const needClick = input.mode === 'kbm' && !input.locked;
    if (needClick && !game.clickHint) {
      game.clickHint = true;
      hud.hint(input.lockFailed ? '<b>Hold the mouse button and drag</b> to look around.' : '<b>Click the view</b> to look around with the mouse.', 6);
    } else if (!needClick) game.clickHint = false;
  } else if (mode === 'turret' && input.pressed('pause')) {
    game.pause('help');
  }
  const core = world.refs.vegaCore;
  game.drone.update(
    dt,
    world.time,
    player,
    world,
    has('vegaOnline') && (mode === 'play' || mode === 'minigame' || mode === 'reader') && !player.frozen,
    hud._wpWorld,
    core ? core.talk : 0
  );
  if (mode === 'minigame') tickMinigame(dt);
  updateContent(dt);
  updateDialogue(dt);
  hud.update(dt);
}

function wireEvents() {
  const { world, player, gfx } = game;
  bus.on('flag', () => {
    applyRules();
    missions.check();
  });
  bus.on('room', (room) => {
    if (game.mode === 'menu') return;
    state.visited[room.id] = true;
    hud.setRoom(room.name);
    if (room.id === 'hub' && has('cryoPower')) setFlag('reachedHub');
    if (room.id === 'bridge' && has('stormWarning')) setFlag('reachedBridge');
    if (game.mode === 'play') onRoomEnter(room);
  });
  bus.on('door', (door, what) => {
    if (game.mode === 'menu') return;
    const d = Math.hypot(door.def.x - player.x, door.def.z - player.z);
    if (d > 26) return;
    const cam = gfx.camera;
    const dx = door.def.x - player.x;
    const dz = door.def.z - player.z;
    const pan = clamp((dx * Math.cos(player.yaw) - dz * Math.sin(player.yaw)) / 8, -1, 1);
    void cam;
    audio.sfx(what === 'open' ? 'doorOpen' : what === 'shut' ? 'doorShut' : 'doorClose', { vol: clamp(1.1 - d / 22, 0.1, 1), pan });
  });
  bus.on('step', (running) => audio.sfx(player.eva ? 'stepMag' : 'step', { vol: running ? 1.25 : 0.9 }));
  bus.on('jump', () => audio.sfx('jump'));
  bus.on('land', () => audio.sfx('land'));
  bus.on('lamp', () => audio.sfx('click'));
  bus.on('hurt', () => {
    if (!game.hurtCool || performance.now() - game.hurtCool > 500) {
      game.hurtCool = performance.now();
      audio.sfx('hurt');
    }
  });
  bus.on('lock', (locked) => {
    if (!locked && game.mode === 'play' && input.mode === 'kbm' && !input.lockFailed) game.pause('mission');
  });
  bus.on('lockfailed', () => {
    if (game.mode === 'play') hud.hint('<b>Hold the mouse button and drag</b> to look around.', 7);
  });
  window.addEventListener('resize', () => gfx.resize());
  window.addEventListener('orientationchange', () => setTimeout(() => gfx.resize(), 250));
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
      if (game.mode === 'play' || game.mode === 'turret') game.pause('mission');
      saveIfPlaying();
      audio.suspend();
    } else {
      audio.resume();
    }
  });
  window.addEventListener('pagehide', saveIfPlaying);
  // any tap or key wakes the audio context (browsers require a gesture)
  const wake = () => audio.init();
  window.addEventListener('pointerdown', wake, { passive: true });
  window.addEventListener('keydown', wake);
  void world;
}

function saveIfPlaying() {
  if (state.started && !state.finished && game.mode !== 'menu' && game.mode !== 'loading') saveGame();
}

function defineGameApi() {
  const { world, player, gfx } = game;
  const A = world.anchors;

  game.touch = () => input.mode === 'touch';

  game.deny = (msg) => {
    audio.sfx('deny');
    hud.toast(msg, 'bad');
  };

  game.toTitle = () => {
    game.mode = 'menu';
    clearInput();
    releaseLock();
    hud.show(false);
    hud.helmet(false);
    hud.letterbox(false);
    hud.frost(0, 0);
    hud.fade(0, 0.8);
    world.alarmTarget = 0;
    for (let i = 1; i < ROOMS.length; i++) world.setPower(ROOMS[i].id, ROOMS[i].id === 'hull', true);
    game.space.setJump(0);
    gfx.bloomBoost = 0;
    gfx.applyFov(0);
    if (game.drone) game.drone.group.visible = false;
    menus.hideEnd();
    menus.closePause();
    menus.showTitle();
  };

  game.startGame = async (resume) => {
    audio.init();
    menus.hideTitle();
    await hud.fade(1, 0.5);
    clearDialogue();
    const loaded = resume && loadGame();
    if (!loaded) {
      resetState();
      clearSave();
      state.started = true;
    }
    // reset transient state
    player.eva = false;
    player.frozen = false;
    player.health = 100;
    player.boost = 0;
    player.toggleLamp(false);
    game.o2 = game.o2Max;
    game.o2Warned = false;
    game.coffeeT = 0;
    game.cinematic = null;
    game.clickHint = false;
    world.particles.clear();
    for (const d of world.doors) {
      d.forceClosed = false;
      d.open = 0;
      d.target = 0;
      d._apply();
    }
    applyRules(true);
    if (!loaded) {
      game.mode = 'intro';
      await menus.playIntro();
    }
    // placement
    const pos = state.pos;
    const room = pos ? roomAt(pos.x, pos.z) : null;
    if (!has('podOpen')) {
      player.place(A.spawn.x, A.spawn.z, 0, 0.05);
    } else if (room && !room.exterior) {
      player.place(pos.x, pos.z, pos.yaw || 0, 0);
    } else if (has('evaStarted') && !has('evaDone')) {
      player.place(15, 40.4, Math.PI / 2, 0);
    } else {
      player.place(R.cryo.cx, R.cryo.cz, 0, 0);
    }
    world.current = null;
    hud.show(true);
    hud.setLayout('walk');
    hud.frost(has('podOpen') ? 0 : 1, 0);
    game.mode = 'play';
    missions.begin(!!loaded);
    saveGame();
    requestLock();
    await hud.fade(0, loaded ? 0.8 : 2.2);
    if (!has('podOpen')) hud.hint(game.touch() ? 'Drag on the right side to look. Tap <b>USE</b> at the latch.' : 'Move the mouse to look. Press <b>E</b> at the latch.', 8);
  };

  game.pause = (tab = 'mission') => {
    if (game.mode !== 'play' && game.mode !== 'turret') return;
    game.prevMode = game.mode;
    game.mode = 'paused';
    clearInput();
    releaseLock();
    saveIfPlaying();
    menus.openPause(tab);
  };

  game.resume = () => {
    if (game.mode !== 'paused') return;
    menus.closePause();
    game.mode = game.prevMode || 'play';
    input.endFrame();
    requestLock();
  };

  game.quitToTitle = () => {
    saveIfPlaying();
    abortMinigame();
    if (game.turret.active) game.turret.end(false);
    clearDialogue();
    if (audio.jukebox.on) audio.toggleJukebox();
    player.eva = false;
    player.frozen = false;
    game.cinematic = null;
    game.toTitle();
  };

  game.runMinigame = async (def) => {
    if (game.mode !== 'play') return { ok: false };
    game.mode = 'minigame';
    clearInput();
    releaseLock();
    hud.setPrompt(null);
    const r = await openMinigame(def);
    if (r.ok) state.stats.minigames++;
    input.endFrame();
    if (game.mode === 'minigame') {
      game.mode = 'play';
      requestLock();
    }
    return r;
  };

  game.openReader = (id) => {
    if (game.mode !== 'play') return;
    game.mode = 'reader';
    clearInput();
    releaseLock();
    hud.setPrompt(null);
    menus.openReader(id);
    saveGame();
  };

  game.closeReader = () => {
    menus.closeReader();
    if (game.mode === 'reader') {
      game.mode = 'play';
      input.endFrame();
      requestLock();
    }
  };

  game.igniteReactor = () => {
    audio.sfx('powerUp');
    hud.flash(0.6);
    player.shake = 1;
    setFlag('fireStarted');
    setFlag('reactorOnline');
    hud.toast('Main power restored', 'good', 'FUSION CORE ONLINE');
  };

  game.startEVA = async () => {
    if (game.mode !== 'play') return;
    game.mode = 'cinematic';
    clearInput();
    hud.setPrompt(null);
    world.door.d_airlock.forceClosed = true;
    say('VEGA', 'Depressurising. Mag boots will engage on the hull.', { now: true });
    audio.sfx('hiss', { dur: 2.6 });
    const vents = world.refs.airlockVents || [];
    for (const v of vents) v.rate = 30;
    await wait(1900);
    await hud.fade(1, 0.8);
    for (const v of vents) v.rate = 0;
    player.place(A.evaSpawn.x, A.evaSpawn.z, 0, 0);
    player.eva = true;
    player.toggleLamp(false);
    game.o2 = game.o2Max;
    game.o2Warned = false;
    hud.helmet(true);
    audio.sfx('doorOpen');
    setFlag('evaStarted');
    game.mode = 'play';
    await hud.fade(0, 1.2);
  };

  game.endEVA = async (forced) => {
    if (game.mode !== 'play') return;
    game.mode = 'cinematic';
    clearInput();
    hud.setPrompt(null);
    if (minigameOpen()) abortMinigame();
    await hud.fade(1, 0.7);
    player.place(15, 40.4, Math.PI / 2, 0);
    player.eva = false;
    hud.helmet(false);
    world.door.d_airlock.forceClosed = false;
    audio.sfx('hiss', { dur: 1.6 });
    if (forced) {
      player.health = Math.max(25, player.health - 40);
      state.stats.retries++;
      hud.toast('Suit oxygen exhausted', 'bad', 'EMERGENCY RETRIEVAL');
      say('VEGA', 'I reeled your tether in. You were out of air. Catch your breath and go again.', { now: true });
    } else if (has('breachesDone') && has('antennaAligned') && !has('evaDone')) {
      setFlag('stormWarning');
      setFlag('evaDone');
    } else {
      hud.toast('Suit oxygen topped up', 'good', 'AIRLOCK');
    }
    game.mode = 'play';
    await hud.fade(0, 0.9);
  };

  game.respawn = async () => {
    if (game.mode !== 'play') return;
    game.mode = 'cinematic';
    clearInput();
    state.stats.retries++;
    await hud.fade(1, 0.5);
    if (player.eva) {
      player.eva = false;
      hud.helmet(false);
      player.place(15, 40.4, Math.PI / 2, 0);
    } else {
      player.place(-5.5, -14, Math.PI / 2, 0);
    }
    player.health = 100;
    for (let n = 1; n <= 4; n++) {
      const em = world.refs[`fire${n}`];
      if (em && !has(`fire${n}`)) em.scale = 1;
    }
    say('VEGA', 'Easy. I had a drone drag you clear. Take a moment.', { now: true });
    game.mode = 'play';
    await hud.fade(0, 1.2);
  };

  game.startTurret = () => {
    if (game.mode !== 'play') return;
    hud.setPrompt(null);
    releaseLockForAim();
    game.turret.start();
  };

  game.startJump = () => {
    if (game.mode !== 'play') return;
    hud.setPrompt(null);
    runEnding();
  };

  // ---- hooks for the automated playthrough test ----
  game.debug = {
    state,
    has,
    setFlag,
    solve: solveMinigame,
    minigameOpen,
    teleport(name, yawDeg) {
      const a = A[name];
      const room = a ? null : R[name];
      const x = a ? a.x : room.cx;
      const z = a ? a.z : room.cz;
      player.place(x, z, yawDeg != null ? (yawDeg * Math.PI) / 180 : player.yaw, 0);
    },
    // stand next to an anchor, facing it, somewhere clear with a line of sight
    approach(name, dist = 1.35) {
      const a = A[name];
      if (!a) return false;
      const cur = roomAt(a.x, a.z);
      const toC = Math.atan2((cur ? cur.cz : a.z + 1) - a.z, (cur ? cur.cx : a.x) - a.x);
      let best = null;
      for (const d of [dist, dist + 0.5, dist - 0.4, dist + 1.1]) {
        for (let i = 0; i < 16 && !best; i++) {
          const ang = toC + (i % 2 ? 1 : -1) * Math.ceil(i / 2) * (Math.PI / 8);
          const x = a.x + Math.cos(ang) * d;
          const z = a.z + Math.sin(ang) * d;
          const r = 0.4;
          if (roomAt(x, z) !== cur) continue;
          if (world.colliders.some((c) => c.on && x > c.x0 - r && x < c.x1 + r && z > c.z0 - r && z < c.z1 + r)) continue;
          if (world.blocked(x, z, a.x, a.z)) continue;
          best = { x, z, d };
        }
        if (best) break;
      }
      if (!best) best = { x: a.x, z: a.z + dist, d: dist };
      const yaw = Math.atan2(-(a.x - best.x), -(a.z - best.z));
      const eye = player.eye;
      player.place(best.x, best.z, yaw, Math.atan2(a.y - eye, best.d));
      return true;
    },
    use(id) {
      const it = game.interact.items.find((i) => i.id === id);
      if (!it) return 'missing';
      if (!it.can()) return 'unavailable';
      const lock = it.locked();
      if (lock) return `locked: ${lock}`;
      it.use();
      return 'ok';
    },
    step: () => (missions.step ? missions.step.id : 'done'),
    focus: () => (game.interact.focus ? game.interact.focus.id : null),
    info: () => ({
      mode: game.mode,
      step: missions.step ? missions.step.id : 'done',
      room: world.current ? world.current.id : null,
      pos: [Number(player.x.toFixed(2)), Number(player.z.toFixed(2))],
      draws: gfx.renderer.info.render.calls,
      tris: gfx.renderer.info.render.triangles,
      particles: world.particles.n,
      frameMs: Number(gfx.frameAvg.toFixed(1)),
    }),
  };
  window.__look = (x, z, yawDeg, pitchDeg = 0) => player.place(x, z, (yawDeg * Math.PI) / 180, (pitchDeg * Math.PI) / 180);
  window.__game = game;
  window.THREE_REV = THREE.REVISION;
}

// Gunnery uses the pointer position on touch screens and locked deltas on desktop.
function releaseLockForAim() {
  if (input.mode === 'touch') clearInput();
}

boot().catch((err) => {
  console.error(err);
  window.__error = String((err && err.stack) || err);
  fatal('Cold Wake could not start', `<pre>${String((err && err.message) || err)}</pre>`);
});
