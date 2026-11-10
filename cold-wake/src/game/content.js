// Gameplay wiring: every usable object on the ship, the rules that follow
// from story flags (power, locks, alarms), hazards and the spacewalk.
import { game } from './ctx.js';
import { state, has, count, setFlag } from '../core/state.js';
import { hud } from '../ui/hud.js';
import { audio } from '../engine/audio.js';
import { say, sayAll } from './dialogue.js';
import { missions, cellsCarried, cellsFound } from './missions.js';
import { LOGS, logById, SPECIMENS, MEMENTOS } from './logs.js';
import { ROOMS, R } from '../world/layout.js';
import { FX } from '../engine/particles.js';
import { breakers, memory, keypad, pipes, wires, route } from '../ui/minigames/puzzles.js';
import { sync, mix, wave, align } from '../ui/minigames/tuning.js';
import { clamp } from '../core/utils.js';

const V = 'VEGA';

export function setupContent() {
  const { world, player } = game;
  const I = game.interact;
  const A = world.anchors;
  const step = () => (missions.step ? missions.step.id : '');
  const add = (anchor, def) => {
    if (!A[anchor]) {
      console.warn(`[content] missing anchor "${anchor}"`);
      return null;
    }
    return I.add({ id: anchor, pos: A[anchor], ...def });
  };
  const mg = (def) => game.runMinigame(def);

  // ---- Mission 1: Cold Wake ----
  add('podLatch', {
    label: 'Release pod latch',
    short: 'Release',
    range: 2,
    can: () => !has('podOpen'),
    mark: () => true,
    use() {
      audio.sfx('thaw');
      audio.sfx('hiss', { dur: 2 });
      hud.frost(0, 5);
      world.particles.burst(40, (P) => FX.steam(0.4, 0.5)({ x: A.podLatch.x, y: 0.4 + Math.random() * 1.6, z: A.podLatch.z }, P));
      setFlag('podOpen');
      hud.hint(game.touch() ? 'Left side of the screen: <b>move</b>. Right side: <b>look</b>.' : '<b>W A S D</b> move &nbsp; <b>Mouse</b> look &nbsp; <b>Shift</b> run &nbsp; <b>E</b> use', 9);
    },
  });
  add('locker', {
    label: () => (has('lockerOpen') ? 'Take Omni-Tool' : 'Open locker 07'),
    short: 'Locker',
    can: () => has('podOpen') && !has('hasTool'),
    mark: () => step() === 'tool',
    use() {
      if (!has('lockerOpen')) {
        audio.sfx('doorShut', { vol: 0.5 });
        setFlag('lockerOpen');
      } else {
        audio.sfx('pickup');
        setFlag('hasTool');
        hud.toast('Omni-Tool acquired', 'good', 'EQUIPMENT');
        hud.hint(game.touch() ? 'Tap the <b>lamp</b> button for your flashlight.' : 'Press <b>F</b> for your flashlight. <b>Tab</b> opens the datapad.', 8);
        player.toggleLamp(true);
      }
    },
  });
  add('breaker', {
    label: 'Reset breakers',
    short: 'Breakers',
    can: () => !has('cryoPower'),
    locked: () => (has('hasTool') ? null : 'Panel is sealed - you need your Omni-Tool'),
    mark: () => step() === 'breaker',
    async use() {
      const r = await mg({ title: 'Auxiliary Bus C', sub: 'BREAKER PANEL', hint: 'The bus trips on any load but the rated one. <b>Switch on breakers that add up to exactly the target.</b>', build: breakers });
      if (r.ok) {
        audio.sfx('powerUp', { vol: 0.7 });
        setFlag('cryoPower');
        hud.toast('Cryo Bay power restored', 'good');
      }
    },
  });

  // ---- Mission 2: VEGA ----
  add('vegaConsole', {
    label: 'Reboot VEGA',
    short: 'Reboot',
    can: () => !has('vegaOnline'),
    mark: () => step() === 'vega',
    async use() {
      const r = await mg({ title: 'Cognitive Core', sub: 'COLD REBOOT', hint: 'VEGA rebuilds from nine memory lattices. <b>Watch each sequence, then repeat it.</b> Three clean passes.', build: memory });
      if (r.ok) {
        audio.sfx('vegaBoot');
        setFlag('vegaOnline');
        hud.toast('VEGA online - navigation restored', 'good', 'SYSTEMS');
      }
    },
  });

  // ---- Mission 3: fusion cells and the reactor ----
  const takeCell = (flag, where) => () => {
    audio.sfx('pickup');
    setFlag(flag);
    hud.toast(`Fusion cell recovered (${cellsFound()}/3)`, 'good', where);
    const n = cellsFound();
    if (n === 1) say(V, 'That is one.');
    else if (n === 2) say(V, 'Two. One more.');
  };
  add('cellEng', { label: 'Take fusion cell', short: 'Take cell', can: () => !has('cellEng'), mark: () => step() === 'cells', use: takeCell('cellEng', 'ENGINEERING') });
  add('cellLab', { label: 'Take fusion cell', short: 'Take cell', can: () => !has('cellLab'), mark: () => step() === 'cells', use: takeCell('cellLab', 'SCIENCE LAB') });
  add('safe', {
    label: () => (has('safeOpen') ? 'Take fusion cell' : 'Enter safe code'),
    short: 'Safe',
    can: () => !has('cellQuarters'),
    mark: () => step() === 'cells',
    async use() {
      if (has('safeOpen')) return takeCell('cellQuarters', 'CREW QUARTERS')();
      const known = state.logs.includes('vasquez');
      const r = await mg({
        title: "Vasquez's Safe",
        sub: 'FOUR-DIGIT LOCK',
        hint: known ? `Her log gave the code: <b>${state.safeCode.split('').join(' ')}</b>.` : 'Four digits. <b>The Chief kept a personal log somewhere in these quarters.</b>',
        build: (api) => keypad(api, state.safeCode),
      });
      if (r.ok) {
        audio.sfx('doorShut', { vol: 0.6 });
        setFlag('safeOpen');
      }
    },
  });
  for (let n = 1; n <= 3; n++) {
    add(`socket${n}`, {
      label: 'Seat fusion cell',
      short: 'Seat cell',
      can: () => !has(`socket${n}`) && has('vegaOnline'),
      locked: () => (cellsCarried() > 0 ? null : 'Empty socket - bring a fusion cell'),
      mark: () => step() === 'install' || (step() === 'cells' && cellsCarried() > 0),
      use() {
        audio.sfx('doorShut', { vol: 0.5 });
        audio.sfx('confirm');
        setFlag(`socket${n}`);
        hud.toast(`Cell seated (${count('socket', 3)}/3)`, 'good', 'REACTOR');
      },
    });
  }
  add('ignite', {
    label: 'Begin ignition',
    short: 'Ignite',
    can: () => !has('reactorOnline') && has('vegaOnline'),
    locked: () => (count('socket', 3) >= 3 ? null : `Reactor needs three cells (${count('socket', 3)}/3 seated)`),
    mark: () => step() === 'ignite',
    async use() {
      const r = await mg({ title: 'Fusion Core Ignition', sub: 'INJECTOR TIMING', hint: 'Four injectors must fire in step. <b>Fire when the white marker crosses the amber band.</b> A miss loses a step.', build: sync });
      if (r.ok) game.igniteReactor();
    },
  });

  // ---- Mission 4: life support ----
  add('irrigation', {
    label: 'Repair irrigation flow',
    short: 'Irrigation',
    can: () => has('reactorOnline') && !has('irrigation'),
    mark: () => step() === 'irrigation',
    async use() {
      const r = await mg({ title: 'Irrigation Control', sub: 'FLOW PATH', hint: 'Water enters at the green section. <b>Tap pipe sections to turn them until the flow reaches the amber outlet.</b>', build: pipes });
      if (r.ok) {
        setFlag('irrigation');
        hud.toast('Irrigation restored', 'good', 'HYDROPONICS');
      }
    },
  });
  for (let n = 1; n <= 3; n++) {
    const wheel = world.refs[`valveWheel${n}`];
    let creak = 0;
    add(`valve${n}`, {
      label: 'Open scrubber valve',
      short: 'Valve',
      hold: 1.6,
      can: () => has('reactorOnline') && !has(`valve${n}`),
      locked: () => (has('irrigation') ? null : 'No water pressure - repair irrigation first'),
      mark: () => step() === 'valves',
      holding(dt, k) {
        if (wheel) wheel.rotation.z = k * 2.4;
        creak -= dt;
        if (creak <= 0) {
          creak = 0.45;
          audio.sfx('valve');
        }
      },
      use() {
        audio.sfx('hiss', { dur: 1.2 });
        setFlag(`valve${n}`);
        hud.toast(`Scrubber ${n} venting oxygen (${count('valve', 3)}/3)`, 'good');
        if (count('valve', 3) >= 3) setFlag('valvesDone');
      },
    });
  }

  // ---- Mission 5: fire and medicine ----
  add('extinguisher', {
    label: 'Take fire extinguisher',
    short: 'Take',
    can: () => has('reactorOnline') && !has('hasExt'),
    mark: () => step() === 'ext',
    use() {
      audio.sfx('pickup');
      setFlag('hasExt');
      hud.toast('Fire extinguisher', 'good', 'EQUIPMENT');
    },
  });
  for (let n = 1; n <= 4; n++) {
    const em = world.refs[`fire${n}`];
    add(`fire${n}`, {
      label: 'Hold to spray',
      short: 'Spray',
      hold: 2.3,
      range: 3.4,
      can: () => has('fireStarted') && !has(`fire${n}`),
      locked: () => (has('hasExt') ? null : 'Too hot to approach - find an extinguisher'),
      mark: () => step() === 'fires',
      holding(dt, k) {
        if (em) em.scale = Math.max(0.05, 1 - k);
        game.fx.spray = 1;
        const cam = game.gfx.camera;
        const dx = em.x - cam.position.x;
        const dy = em.y + 0.3 - (cam.position.y - 0.3);
        const dz = em.z - cam.position.z;
        const len = Math.hypot(dx, dy, dz) || 1;
        for (let i = 0; i < 3; i++) FX.spray()({ x: cam.position.x + (dx / len) * 0.5, y: cam.position.y - 0.3, z: cam.position.z + (dz / len) * 0.5, dx: dx / len, dy: dy / len, dz: dz / len }, world.particles);
      },
      cancel() {
        if (em && !has(`fire${n}`)) em.scale = Math.min(1, em.scale + 0.3);
      },
      use() {
        if (em) em.scale = 0;
        audio.sfx('hiss', { dur: 1 });
        world.particles.burst(24, (P) => FX.steam(0.6, 1.2)({ x: em.x, y: 0.4, z: em.z }, P));
        setFlag(`fire${n}`);
        hud.toast(`Fire out (${count('fire', 4)}/4)`, 'good');
        if (count('fire', 4) >= 4) setFlag('firesDone');
      },
    });
  }
  add('synth', {
    label: 'Mix stabiliser',
    short: 'Synthesise',
    can: () => has('reactorOnline') && !has('compound'),
    locked: () => (has('firesDone') ? null : 'Synthesiser locked out while the fire alarm is active'),
    mark: () => step() === 'compound',
    async use() {
      const r = await mg({ title: 'Pharma Synthesiser', sub: 'CARDIAC STABILISER', hint: 'Dr. Raman needs a fresh mix. <b>Adjust the three reagents until your sample matches the target colour.</b>', build: mix });
      if (r.ok) {
        setFlag('compound');
        hud.toast('Stabiliser delivered to pod 03', 'good', 'MED BAY');
      }
    },
  });

  // ---- Mission 6: the signal ----
  add('spectro', {
    label: 'Run phase match',
    short: 'Analyse',
    can: () => has('reactorOnline') && !has('signalDecoded'),
    locked: () => (has('compound') || step() === 'signal' ? null : 'VEGA: "One emergency at a time. The crew comes first."'),
    mark: () => step() === 'signal',
    async use() {
      const r = await mg({ title: 'Spectrometer', sub: 'UNKNOWN SIGNAL 1420.4 MHz', hint: "Lindqvist's notes: <b>shape your trace until it lies on top of the dashed carrier.</b>", build: wave, wide: true });
      if (r.ok) {
        audio.sfx('vegaBoot', { vol: 0.6 });
        setFlag('signalDecoded');
        hud.toast('Signal decoded: "DEBRIS TIDE - LEAVE"', 'warn', 'SCIENCE LAB');
      }
    },
  });

  // ---- Mission 7: spacewalk ----
  add('fuse', {
    label: 'Repair fuse box',
    short: 'Fuse box',
    can: () => has('reactorOnline') && !has('airlockFuse'),
    mark: () => step() === 'fuse',
    async use() {
      const r = await mg({ title: 'Airlock 2 Fuse Box', sub: 'FIVE CONDUCTORS', hint: '<b>Join each conductor to the terminal of the same colour.</b> Drag across, or tap a wire and then its terminal.', build: wires });
      if (r.ok) {
        setFlag('airlockFuse');
        hud.toast('Airlock power restored', 'good', 'CARGO BAY');
      }
    },
  });
  add('suit', {
    label: 'Put on EVA suit',
    short: 'Suit up',
    can: () => has('airlockFuse') && !has('hasSuit'),
    mark: () => step() === 'suit',
    use() {
      audio.sfx('hiss', { dur: 0.8 });
      audio.sfx('pickup');
      setFlag('hasSuit');
      hud.toast('EVA suit sealed', 'good', 'EQUIPMENT');
    },
  });
  add('cycle', {
    label: 'Cycle airlock',
    short: 'Cycle',
    can: () => has('airlockFuse') && !has('evaDone'),
    locked: () => (has('hasSuit') ? (has('signalDecoded') ? null : 'VEGA: "Nothing to do out there yet."') : 'Put on an EVA suit first'),
    mark: () => step() === 'cycle',
    use: () => game.startEVA(),
  });
  add('hatch', {
    label: 'Enter airlock',
    short: 'Go inside',
    range: 2.8,
    can: () => player.eva,
    mark: () => step() === 'return' || game.o2 < 60,
    use: () => game.endEVA(false),
  });
  for (let n = 1; n <= 3; n++) {
    const a = A[`breach${n}`];
    let buzz = 0;
    add(`breach${n}`, {
      label: 'Hold to weld patch',
      short: 'Weld',
      hold: 3.0,
      range: 4.6,
      can: () => player.eva && !has(`breach${n}`),
      mark: () => true,
      holding(dt) {
        game.fx.weld = 1;
        buzz -= dt;
        if (buzz <= 0) {
          buzz = 0.07;
          const ox = (Math.random() - 0.5) * 1.6;
          const oz = (Math.random() - 0.5) * 1.6;
          world.flash(a.x + ox, 0.6, a.z + oz, 0x9fd8ff, 26, 0.12, 7);
          for (let i = 0; i < 4; i++) FX.sparks(1.4)({ x: a.x + ox, y: 0.3, z: a.z + oz }, world.particles);
        }
      },
      use() {
        audio.sfx('weldDone');
        setFlag(`breach${n}`);
        hud.toast(`Breach sealed (${count('breach', 3)}/3)`, 'good', 'HULL');
        if (count('breach', 3) >= 3) setFlag('breachesDone');
      },
    });
  }
  add('dish', {
    label: 'Align dish',
    short: 'Align',
    can: () => player.eva && !has('antennaAligned'),
    mark: () => step() === 'antenna',
    async use() {
      const r = await mg({ title: 'Long Range Array', sub: 'MANUAL ALIGNMENT', hint: 'The servo lags behind you. <b>Steer the reticle onto the drifting relay and hold it there</b> until the carrier locks.', build: align });
      if (r.ok) {
        setFlag('antennaAligned');
        hud.toast('Carrier lock with Meridian Station', 'good', 'COMMS');
      }
    },
  });

  // ---- Missions 8 and 9: the bridge ----
  add('tactical', {
    label: 'Take the tactical station',
    short: 'Tactical',
    can: () => has('stormWarning') && !has('debrisCleared'),
    mark: () => step() === 'turret',
    use: () => game.startTurret(),
  });
  add('navTable', {
    label: 'Plot jump course',
    short: 'Navigate',
    can: () => has('evaDone') && !has('routePlotted'),
    locked: () => (has('debrisCleared') ? null : 'Navigation locked out during the collision alert'),
    mark: () => step() === 'route',
    async use() {
      const r = await mg({ title: 'Navigation Table', sub: 'JUMP NETWORK', hint: 'Start at <b>Aurelia</b>, finish at <b>Meridian Station</b>. Each lane costs fuel and you carry eleven units. Tap a system to jump to it.', build: route, wide: true });
      if (r.ok) {
        setFlag('route', r.data);
        setFlag('routePlotted');
        hud.toast('Course laid in for Meridian Station', 'good', 'HELM');
      }
    },
  });
  add('jump', {
    label: 'Pull the jump lever',
    short: 'JUMP',
    can: () => has('evaDone') && !has('jumped'),
    locked: () => (has('routePlotted') ? null : 'No course laid in'),
    mark: () => step() === 'jump',
    use: () => game.startJump(),
  });

  // ---- side content ----
  for (const log of LOGS) {
    add(`log_${log.id}`, {
      label: 'Read data slate',
      short: 'Read',
      range: 2.3,
      can: () => !state.logs.includes(log.id),
      mark: () => true,
      use() {
        audio.sfx('log');
        state.logs.push(log.id);
        setFlag(`log_${log.id}`);
        hud.toast(`${log.title}`, 'info', `LOG ${state.logs.length}/12 SAVED`);
        game.openReader(log.id);
      },
    });
  }
  for (let n = 1; n <= 6; n++) {
    let z = 0;
    add(`panel${n}`, {
      label: 'Hold to patch panel',
      short: 'Patch',
      hold: 1.4,
      can: () => !has(`panel${n}`) && has('hasTool'),
      mark: () => false,
      holding(dt) {
        z -= dt;
        if (z <= 0) {
          z = 0.12;
          audio.sfx('spark', { vol: 0.6 });
        }
      },
      use() {
        audio.sfx('confirm');
        setFlag(`panel${n}`);
        hud.toast(`Service panel patched (${count('panel', 6)}/6)`, 'good', 'MAINTENANCE');
        if (count('panel', 6) >= 6) say(has('vegaOnline') ? V : 'SHIP', 'Every service panel is green. Vasquez will think she is dreaming.');
      },
    });
  }
  for (let n = 1; n <= 5; n++) {
    add(`plant${n}`, {
      label: 'Scan specimen',
      short: 'Scan',
      range: 2.2,
      can: () => !has(`plant${n}`) && has('hasTool'),
      mark: () => false,
      use() {
        audio.sfx('uiHi');
        audio.sfx('log');
        setFlag(`plant${n}`);
        const [name, note] = SPECIMENS[n - 1];
        hud.toast(note, 'info', `${name.toUpperCase()}  (${count('plant', 5)}/5)`);
        if (count('plant', 5) >= 5) say(V, 'All five specimens catalogued. Lindqvist will be insufferable, and delighted.');
      },
    });
  }
  for (let n = 1; n <= 6; n++) {
    add(`mem${n}`, {
      label: `Pick up ${MEMENTOS[n - 1][0]}`,
      short: 'Pick up',
      range: 2.1,
      can: () => !has(`mem${n}`),
      mark: () => false,
      use() {
        audio.sfx('pickup');
        setFlag(`mem${n}`);
        hud.toast(MEMENTOS[n - 1][1], 'info', `KEEPSAKE ${count('mem', 6)}/6`);
      },
    });
  }
  add('coffee', {
    label: 'Brew coffee',
    short: 'Coffee',
    can: () => has('reactorOnline') && game.coffeeT <= 0,
    use() {
      audio.sfx('coffee');
      game.coffeeT = 90;
      player.boost = 90;
      hud.toast('You feel faster for a while', 'good', 'CAFFEINATED');
      if (!has('coffeeOnce')) {
        setFlag('coffeeOnce');
        say(V, 'The coffee machine works on the first try for you. Vasquez has been fighting it for a year.');
      }
    },
  });
  add('jukebox', {
    label: () => (audio.jukebox.on ? 'Stop the jukebox' : 'Play the jukebox'),
    short: 'Jukebox',
    can: () => has('reactorOnline'),
    use() {
      audio.sfx('click');
      const on = audio.toggleJukebox();
      hud.toast(on ? 'Now playing: "Slow Orbit"' : 'Jukebox stopped', 'info', 'CREW LOUNGE');
    },
  });
  add('holo', {
    label: 'Check ship status',
    short: 'Status',
    range: 3,
    can: () => has('vegaOnline'),
    use() {
      audio.sfx('ui');
      const lines = [];
      lines.push(has('reactorOnline') ? 'Reactor: online.' : `Reactor: scrammed, ${count('socket', 3)} of 3 cells seated.`);
      lines.push(has('valvesDone') ? 'Life support: nominal.' : 'Life support: failing.');
      lines.push(has('breachesDone') ? 'Hull: sealed.' : 'Hull: three dorsal breaches.');
      lines.push(has('antennaAligned') ? 'Comms: carrier lock.' : 'Comms: no carrier.');
      say(V, lines.join(' '), { now: true });
    },
  });
}

// World state that follows from story flags. Safe to call at any time.
export function applyRules(instant = false) {
  const { world } = game;
  const pw = has('reactorOnline');
  for (let i = 1; i < ROOMS.length; i++) {
    const r = ROOMS[i];
    let on = pw;
    if (r.id === 'cryo') on = pw || has('cryoPower');
    if (r.id === 'aicore') on = pw || has('vegaOnline');
    if (r.id === 'hull') on = true;
    const delay = Math.min(2.2, Math.hypot(r.cx - R.engineering.cx, r.cz - R.engineering.cz) / 28);
    world.setPower(r.id, on, instant || r.id === 'hull', r.id === 'cryo' || r.id === 'aicore' ? 0 : delay);
  }
  const lock = (id, reason) => world.door[id].setLocked(reason);
  lock('d_cryo', has('cryoPower') || pw ? null : 'Door unpowered - reset the breaker panel');
  lock('d_eng', has('vegaOnline') ? null : 'Security lockdown - VEGA authorisation required');
  lock('d_hydro', pw ? null : 'No main power');
  lock('d_cargo', pw ? null : 'No main power');
  lock('d_med', pw ? null : 'Sealed - no main power');
  lock('d_airlock', has('airlockFuse') ? null : 'Airlock fault - check the fuse box');
  lock('d_bridge', has('evaDone') ? null : 'Bridge sealed - hull breach, pressure loss');
  const storm = has('stormWarning') && !has('debrisCleared');
  const fire = has('fireStarted') && !has('firesDone');
  world.alarmTarget = storm ? 1 : fire ? 0.3 : 0;
  world.sync();
}

// Per-frame gameplay: hazards, oxygen, ambience, music mood.
export function updateContent(dt) {
  const { world, player } = game;
  const cur = world.current;
  // fires hurt up close and throw flickering light
  let fireNear = 0;
  if (has('fireStarted') && !has('firesDone') && cur && cur.id === 'medbay') {
    for (let n = 1; n <= 4; n++) {
      if (has(`fire${n}`)) continue;
      const em = world.refs[`fire${n}`];
      const d = Math.hypot(em.x - player.x, em.z - player.z);
      fireNear = Math.max(fireNear, clamp(1 - d / 9, 0, 1) * (em.scale ?? 1));
      if (d < 1.25 && (em.scale ?? 1) > 0.3) player.damage(14 * dt);
      if (Math.random() < dt * 9) world.flash(em.x, 0.9, em.z, 0xff7a2a, 10 + Math.random() * 10 * (em.scale ?? 1), 0.16, 8);
    }
  }
  // suit oxygen
  if (player.eva && game.mode === 'play') {
    game.o2 -= dt;
    if (game.o2 < 45 && !game.o2Warned) {
      game.o2Warned = true;
      say(V, 'Suit oxygen is under a minute. Head for the hatch.', { now: true });
      audio.sfx('deny');
    }
    if (game.o2 <= 0) game.endEVA(true);
  }
  if (player.health <= 0 && game.mode === 'play') game.respawn();
  game.coffeeT = Math.max(0, game.coffeeT - dt);
  // jukebox carries through the lounge
  const jb = world.anchors.jukebox;
  if (jb) {
    const d = Math.hypot(jb.x - player.x, jb.z - player.z);
    audio.setJukeboxVolume(player.eva ? 0 : Math.pow(clamp(1 - d / 16, 0, 1), 2) * (cur && cur.id === 'quarters' ? 1 : 0.45));
  }
  // audio scene
  const reactor = world.refs.reactor;
  const eng = R.engineering;
  audio.update(dt, {
    power: cur ? world.rooms[cur.index].power : 0,
    eva: player.eva,
    alarm: world.alarm,
    reactor: reactor ? reactor.level : 0,
    reactorDist: Math.hypot(player.x - eng.cx, player.z - eng.cz),
    fire: fireNear,
    room: cur ? cur.id : '',
  });
  audio.setLoop('spray', game.fx.spray ? 0.2 : 0, 0.05);
  audio.setLoop('weld', game.fx.weld ? 0.12 : 0, 0.03);
  audio.setLoop('weldBuzz', game.fx.weld ? 0.05 : 0, 0.03);
  game.fx.spray = 0;
  game.fx.weld = 0;
  let mood = 'calm';
  if (game.mode === 'turret') mood = 'action';
  else if (has('stormWarning') && !has('debrisCleared')) mood = 'tense';
  else if (player.eva) mood = 'wonder';
  else if (!has('reactorOnline')) mood = 'dark';
  audio.setMood(mood);
  // status chips
  hud.setChips([
    ...(has('hasTool') ? [{ label: 'LAMP', on: player.lampOn }] : []),
    ...(cellsCarried() > 0 ? [{ label: `CELLS ${cellsCarried()}`, on: true }] : []),
    ...(has('hasExt') && !has('firesDone') ? [{ label: 'EXTINGUISHER', on: true }] : []),
    ...(has('hasSuit') ? [{ label: 'EVA SUIT', on: player.eva }] : []),
    ...(game.coffeeT > 0 ? [{ label: 'COFFEE', on: true }] : []),
  ]);
  void sayAll;
  void logById;
}
