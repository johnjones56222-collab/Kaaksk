// The main story: nine missions made of ordered steps. A step is finished
// when its `done` test passes, so steps completed early are skipped cleanly
// and a loaded save lands on the right one.
import { state, has, count, saveGame } from '../core/state.js';
import { game } from './ctx.js';
import { hud } from '../ui/hud.js';
import { say, sayAll } from './dialogue.js';
import { audio } from '../engine/audio.js';
import { R } from '../world/layout.js';

const A = (name) => game.world.anchors[name];
const SYS = 'SHIP';
const V = 'VEGA';

export const MISSION_NAMES = ['', 'Cold Wake', 'Ghost in the Machine', 'Heart of the Ship', 'Breathe', 'Trial by Fire', 'The Signal', 'Spacewalk', 'Hard Rain', 'Way Home'];

export const STEPS = [
  {
    id: 'wake', m: 1,
    text: () => 'Release the cryo-pod latch',
    done: () => has('podOpen'),
    target: () => A('podLatch'),
    start: () => sayAll(SYS, ['Emergency thaw complete. Pod seven: occupant conscious.', 'Release the latch to exit.']),
  },
  {
    id: 'tool', m: 1,
    text: () => 'Take your Omni-Tool from locker 07',
    done: () => has('hasTool'),
    target: () => A('locker'),
    start: () => sayAll(SYS, ['Cognitive core offline. Main power offline. First-responder protocol active.', 'Retrieve your omni-tool. Locker zero-seven, by the door.']),
  },
  {
    id: 'breaker', m: 1,
    text: () => 'Restore power at the breaker panel',
    done: () => has('cryoPower'),
    target: () => A('breaker'),
    start: () => say(SYS, 'Bay door is unpowered. Reset the auxiliary bus at the breaker panel.'),
  },
  {
    id: 'hub', m: 2,
    text: () => 'Make your way to the Central Hub',
    done: () => has('reachedHub'),
    target: () => ({ x: R.hub.cx, y: 1.2, z: R.hub.cz + 3 }),
    start: () => sayAll(SYS, ['Auxiliary bus stable. Door power restored.', 'Proceed to the Central Hub. Guidance is limited until the cognitive core is rebooted.']),
  },
  {
    id: 'vega', m: 2,
    text: () => 'Reboot VEGA in the AI Core',
    done: () => has('vegaOnline'),
    target: () => A('vegaConsole'),
    start: () => say(SYS, 'AI Core: fore corridor, port side. Manual reboot required.'),
  },
  {
    id: 'cells', m: 3,
    text: () => `Collect three fusion cells <b>${cellsFound()}/3</b>`,
    list: () => [
      { text: 'Engineering storage rack', done: has('cellEng') },
      { text: 'Science Lab energy locker', done: has('cellLab') },
      { text: "Vasquez's safe, Crew Quarters", done: has('cellQuarters') },
    ],
    done: () => cellsFound() >= 3,
    target: () => [!has('cellEng') && A('cellEng'), !has('cellLab') && A('cellLab'), !has('cellQuarters') && A('safe')].filter(Boolean),
    start: () =>
      sayAll(V, [
        '...Oh. Hello. There you are.',
        "I'm VEGA. I have been asleep for nineteen hours, which is embarrassing for a ship's mind.",
        'Navigation is back on your display. The reactor scrammed, and I need three fusion cells before I can relight it.',
        'Chief Vasquez hid them in three different places. Of course she did.',
      ]),
  },
  {
    id: 'install', m: 3,
    text: () => `Seat the cells in the reactor sockets <b>${count('socket', 3)}/3</b>`,
    done: () => count('socket', 3) >= 3,
    target: () => [1, 2, 3].filter((n) => !has(`socket${n}`)).map((n) => A(`socket${n}`)),
    start: () => say(V, 'That is all three. Take them to the sockets around the reactor base in Engineering.'),
  },
  {
    id: 'ignite', m: 3,
    text: () => 'Start the reactor at the ignition console',
    done: () => has('reactorOnline'),
    target: () => A('ignite'),
    start: () => say(V, 'Cells are seated. The ignition console is yours: four injectors, fired in step.'),
  },
  {
    id: 'irrigation', m: 4,
    text: () => 'Repair irrigation control in Hydroponics',
    done: () => has('irrigation'),
    target: () => A('irrigation'),
    start: () =>
      sayAll(V, [
        'She is lit! Main power is... hold on.',
        'Surge on the port bus. There is a fire in the Med Bay, and cabin oxygen is at seventeen percent and falling.',
        'Oxygen first. Hydroponics, starboard side of the mid corridor. The irrigation controller has tripped.',
      ]),
  },
  {
    id: 'valves', m: 4,
    text: () => `Open the oxygen scrubber valves <b>${count('valve', 3)}/3</b>`,
    done: () => has('valvesDone'),
    target: () => [1, 2, 3].filter((n) => !has(`valve${n}`)).map((n) => A(`valve${n}`)),
    start: () => say(V, 'Water is flowing. Now the three scrubber valves, by hand. They fail closed on a power fault.'),
  },
  {
    id: 'ext', m: 5,
    text: () => 'Take the extinguisher from the Mid Corridor fire point',
    done: () => has('hasExt'),
    target: () => A('extinguisher'),
    start: () => sayAll(V, ['Oxygen is climbing. You can breathe properly again, and so can they.', 'Now the fire. There is an extinguisher at the fire point in the mid corridor.']),
  },
  {
    id: 'fires', m: 5,
    text: () => `Put out the Med Bay fires <b>${count('fire', 4)}/4</b>`,
    done: () => has('firesDone'),
    target: () => [1, 2, 3, 4].filter((n) => !has(`fire${n}`)).map((n) => A(`fire${n}`)),
    start: () => say(V, 'Med Bay is off the hub, port side. Hold the spray on each seat of the fire, and mind the heat.'),
  },
  {
    id: 'compound', m: 5,
    text: () => 'Synthesise a cardiac stabiliser for Dr. Raman',
    done: () => has('compound'),
    target: () => A('synth'),
    start: () => sayAll(V, ['Fire is out. Suppression would have handled it if I had been awake. Sorry.', "One more thing here: Dr. Raman's heart is struggling in stasis. The synthesiser can mix a stabiliser."]),
  },
  {
    id: 'signal', m: 6,
    text: () => 'Analyse the signal at the Science Lab spectrometer',
    done: () => has('signalDecoded'),
    target: () => A('spectro'),
    start: () => sayAll(V, ['Dose delivered. Her rhythm is steadying.', 'Now, why we are in this state. Lindqvist was studying the signal in the lab. Finish his phase match and I can read it.']),
  },
  {
    id: 'fuse', m: 7,
    text: () => 'Repair the airlock fuse box in the Cargo Bay',
    done: () => has('airlockFuse'),
    target: () => A('fuse'),
    start: () =>
      sayAll(V, [
        'It is resolving... It is a warning. "Debris tide. Leave." Over and over.',
        'The Lattice is not a weapon. It is a buoy. Something shears these rings on a cycle, and we parked in its path.',
        'I need the long-range dish to reach Meridian Station, and there are three breaches on the dorsal hull. That means a walk outside.',
        'The airlock is in the Cargo Bay, aft of Engineering. Its fuse box has blown again.',
      ]),
  },
  {
    id: 'suit', m: 7,
    text: () => 'Put on an EVA suit in the airlock',
    done: () => has('hasSuit'),
    target: () => A('suit'),
    start: () => say(V, 'Airlock has power. The suits are inside.'),
  },
  {
    id: 'cycle', m: 7,
    text: () => 'Cycle the airlock',
    done: () => has('evaStarted'),
    target: () => A('cycle'),
    start: () => say(V, 'Suit seals are green. You have about four minutes of oxygen per excursion. The hatch tops you up.'),
  },
  {
    id: 'breaches', m: 7,
    text: () => `Weld the hull breaches <b>${count('breach', 3)}/3</b>`,
    done: () => has('breachesDone'),
    target: () => [1, 2, 3].filter((n) => !has(`breach${n}`)).map((n) => A(`breachStand${n}`)),
    start: () => sayAll(V, ['Mag boots engaged. Follow the catwalk: three breaches, then the dish at the bow end.', 'And take a second to look up. I never get tired of it.']),
  },
  {
    id: 'antenna', m: 7,
    text: () => 'Realign the long-range dish',
    done: () => has('antennaAligned'),
    target: () => A('dish'),
    start: () => say(V, 'Hull is holding pressure. Now the dish, at the bow end of the catwalk.'),
  },
  {
    id: 'return', m: 7,
    text: () => 'Return through the airlock hatch',
    done: () => has('evaDone'),
    target: () => A('hatch'),
    start: () => sayAll(V, ['Carrier lock! Meridian Station can hear us.', 'That is everything out here. Come back in.']),
  },
  {
    id: 'bridge', m: 8,
    text: () => 'Get to the Bridge',
    done: () => has('reachedBridge'),
    target: () => ({ x: 0, y: 1.2, z: -40 }),
    start: () => sayAll(V, ['You are in. And just in time: the tide is turning. Debris inbound, a lot of it.', 'Get to the Bridge. The point-defence turret is slaved to the tactical station.'], { now: true }),
  },
  {
    id: 'turret', m: 8,
    text: () => 'Take the tactical station and clear the debris',
    done: () => has('debrisCleared'),
    target: () => A('tactical'),
    start: () => say(V, 'Tactical station, port side. I will feed you targets.'),
  },
  {
    id: 'route', m: 9,
    text: () => 'Plot a jump course at the navigation table',
    done: () => has('routePlotted'),
    target: () => A('navTable'),
    start: () => sayAll(V, ['Field is clear. I did not think that would work.', 'The jump drive is charged. Plot a course on the navigation table: Meridian Station, eleven units of fuel, and stay away from the pulsar.']),
  },
  {
    id: 'jump', m: 9,
    text: () => "Pull the jump lever at the captain's station",
    done: () => has('jumped'),
    target: () => A('jump'),
    start: () => say(V, "Course laid in. The captain's lever is yours. She would want you to pull it."),
  },
];

export function cellsFound() {
  return (has('cellEng') ? 1 : 0) + (has('cellLab') ? 1 : 0) + (has('cellQuarters') ? 1 : 0);
}
export const cellsCarried = () => cellsFound() - count('socket', 3);

export const SIDE = [
  { id: 'logs', text: 'Recover crew logs', n: () => state.logs.length, of: 12 },
  { id: 'panels', text: 'Patch sparking service panels', n: () => count('panel', 6), of: 6 },
  { id: 'plants', text: "Scan Lindqvist's tagged specimens", n: () => count('plant', 5), of: 5 },
  { id: 'mems', text: 'Find crew keepsakes', n: () => count('mem', 6), of: 6 },
];

export const missions = {
  index: 0,
  timer: 0,

  get step() {
    return STEPS[this.index] || null;
  },

  // Start (or resume) at the first unfinished step.
  begin(resumed) {
    this.index = 0;
    while (this.index < STEPS.length && STEPS[this.index].done()) this.index++;
    const s = this.step;
    if (s && s.start) {
      if (resumed) say(has('vegaOnline') ? V : SYS, has('vegaOnline') ? 'Welcome back. Picking up where we left off.' : 'Session restored.');
      else s.start();
    }
    this.refresh();
  },

  // Called whenever a flag changes.
  check() {
    let advanced = false;
    while (this.step && this.step.done()) {
      const prev = this.step;
      this.index++;
      advanced = true;
      const next = this.step;
      if (next && next.m !== prev.m) {
        audio.sfx('mission');
        hud.toast(MISSION_NAMES[next.m], 'good', `MISSION ${String(next.m).padStart(2, '0')}`);
      } else {
        audio.sfx('objective');
      }
      if (next && next.start && !next.done()) next.start();
    }
    if (advanced) {
      hud.pulseMission();
      saveGame();
    }
    this.refresh();
    return advanced;
  },

  refresh() {
    const s = this.step;
    if (!s) {
      hud.setMission('ALL MISSIONS COMPLETE', 'Way Home', 'The Astraea is under way.', []);
      hud.setWaypoint(null);
      return;
    }
    hud.setMission(`MISSION ${String(s.m).padStart(2, '0')}`, MISSION_NAMES[s.m], s.text(), s.list ? s.list() : []);
    this.retarget();
  },

  retarget() {
    const s = this.step;
    if (!s || !s.target) return hud.setWaypoint(null);
    let t = s.target();
    if (Array.isArray(t)) {
      const p = game.player;
      let best = null;
      let bd = Infinity;
      for (const c of t) {
        if (!c) continue;
        const d = (c.x - p.x) ** 2 + (c.z - p.z) ** 2;
        if (d < bd) {
          bd = d;
          best = c;
        }
      }
      t = best;
    }
    hud.setWaypoint(t || null);
  },

  update(dt) {
    this.timer -= dt;
    if (this.timer <= 0) {
      this.timer = 0.5;
      this.retarget();
    }
  },
};
