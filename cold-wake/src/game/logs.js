// Recovered crew logs. Twelve data slates are scattered around the ship.
import { state } from '../core/state.js';

export const LOGS = [
  {
    id: 'cryo',
    title: 'Stasis Advisory',
    author: 'Automated thaw controller',
    where: 'Cryo Bay',
    body: () => [
      'EMERGENCY THAW - POD 07 - DESIGNATED FIRST RESPONDER.',
      'Reason: reactor scram and loss of cognitive core. Five crew remain in protective stasis and will not be woken until cabin oxygen and main power are stable.',
      'If you are reading this, you are the first responder. Your omni-tool is in locker 07. The breaker panel by the door feeds this bay. Good luck. This message will not repeat.',
    ],
  },
  {
    id: 'hub',
    title: "Captain's Log, Day 411",
    author: 'Capt. Ilse Marlow',
    where: 'Central Hub',
    body: () => [
      'Lindqvist picked up a carrier wave from inside the rings this morning. Not a pulsar, not a moon. It repeats every 7.3 seconds and it is far too clean.',
      'I have ordered a slow approach to twelve hundred kilometres. Okonkwo thinks I am being reckless. He is probably right, but nobody signs on to a survey ship to look the other way.',
      'If anything goes wrong, VEGA has standing orders: crew to stasis, ship to safe orbit, wake the first responder.',
    ],
  },
  {
    id: 'corridor',
    title: 'Dropped Slate',
    author: 'Lt. Dario Okonkwo',
    where: 'Fore Corridor',
    body: () => [
      'Note to self, since VEGA keeps "forgetting" to log it: the port-side point-defence capacitor is slow to recharge. Short bursts. Do not hold the trigger down or it cooks itself.',
      'Also: Marlow owes me twenty credits. The object in the rings is definitely artificial.',
    ],
  },
  {
    id: 'ai',
    title: 'Core Maintenance Note',
    author: 'V.E.G.A. (cached)',
    where: 'AI Core',
    body: () => [
      'If my higher functions are ever offline, the reboot is deliberately simple: the console will show a sequence across my nine memory lattices. Repeat it back. Three clean passes and I can rebuild myself from cold storage.',
      'I designed it that way because the person doing it will probably be frightened and alone. I would like them to know I am looking forward to meeting them.',
    ],
  },
  {
    id: 'mess',
    title: 'Galley Rota (Disputed)',
    author: 'Crew, collectively',
    where: 'Crew Quarters',
    body: () => [
      'MON - Raman (soup). TUE - Okonkwo (do NOT let him near the chillies again). WED - Lindqvist (whatever the garden gives us). THU - Vasquez (reheated Monday). FRI - Marlow (pancakes, non-negotiable).',
      'Scrawled underneath: "The coffee machine is not broken, it is temperamental. Press and wait. - R.V."',
    ],
  },
  {
    id: 'vasquez',
    title: 'Personal Log - R. Vasquez',
    author: 'Chief Eng. Rhea Vasquez',
    where: 'Crew Quarters',
    body: () => [
      'Three fusion cells is the minimum for a cold start and regulations say keep them in one rack. Regulations were not written by anyone who has had a rack fail.',
      'So: one in the Engineering rack, one in the lab containment locker where Lindqvist will not touch it, and one in my wall safe.',
      `Safe code is the registry of my first ship: ${state.safeCode.split('').join(' - ')}. If you are reading this and you are not me, I hope you are putting it to good use.`,
    ],
  },
  {
    id: 'med',
    title: 'Medical Note',
    author: 'Dr. Priya Raman',
    where: 'Med Bay',
    body: () => [
      'Reminder for whoever covers for me: my own stasis profile is flagged. Long sleeps give me an arrhythmia on thaw. The stabiliser is not in stock, it has to be mixed fresh.',
      'The synthesiser will show you a target compound. Match the three reagents until your sample is the same colour and it will hold the mix for you. It is easier than it sounds.',
      'Yes, I see the irony of the ship doctor being the fragile one.',
    ],
  },
  {
    id: 'lab',
    title: 'Field Notes - The Lattice',
    author: 'Dr. Tomas Lindqvist',
    where: 'Science Lab',
    body: () => [
      'We recovered a fragment from the debris around the object. It is cold, it is harder than anything in our catalogue, and it hums at the same interval as the signal. I have started calling the parent structure the Lattice.',
      'The signal is layered. There is a carrier, and under it something with structure. If I can phase-match the spectrometer to the fragment I think the lower layer will resolve.',
      'I keep wondering who it was built to warn. And about what.',
    ],
  },
  {
    id: 'hydro',
    title: 'Garden Journal',
    author: 'Dr. Tomas Lindqvist',
    where: 'Hydroponics',
    body: () => [
      'The garden gives us a fifth of our oxygen and all of our sanity. If the irrigation controller ever trips, re-seat the flow path on the console - water has to reach the outlet or the beds dry out within a day.',
      'After that, open all three scrubber valves by hand. They close themselves on a power fault. It is a safety feature and I hate it.',
      'I have tagged five specimens on the bench that have changed since we arrived at Aurelia. Scan them if you get the chance. I would like a second opinion.',
    ],
  },
  {
    id: 'eng',
    title: 'Cold Start Checklist',
    author: 'Chief Eng. Rhea Vasquez',
    where: 'Engineering',
    body: () => [
      '1. Three cells seated, one per socket. 2. Go to the ignition console. 3. Four injectors have to fire in step: watch the ring, fire when the marker crosses the band. Miss and you lose a step, so do not rush it.',
      '4. When she lights, the whole ship will take a breath. Expect surges. Expect something, somewhere, to catch fire. It always does.',
    ],
  },
  {
    id: 'cargo',
    title: 'Loadmaster Memo',
    author: 'Lt. Dario Okonkwo',
    where: 'Cargo Bay',
    body: () => [
      'The fuse box for Airlock 2 has blown twice this trip. If it goes again: five conductors, match the colours, nothing clever. I have left the cover open.',
      'EVA suits are charged. Suit oxygen is good for about four minutes of hard work, and the hatch tops you up every time you come back in. Do not be a hero out there.',
    ],
  },
  {
    id: 'bridge',
    title: "Captain's Log, Day 412",
    author: 'Capt. Ilse Marlow',
    where: 'Bridge',
    body: () => [
      'We are at twelve hundred kilometres and the object has noticed us. The signal changed the moment we crossed some line I could not see. VEGA is reporting faults faster than I can read them.',
      'I am sending the crew to stasis. If the ship is still here when someone wakes up: the jump network is on the navigation table. We carry fuel for eleven units of jump. Meridian Station is the nearest friendly port. Do not route through the pulsar, whatever the numbers say.',
      'It was worth coming to look. I want that on the record.',
    ],
  },
];

export const logById = (id) => LOGS.find((l) => l.id === id);

// Botanist's specimen notes (side task).
export const SPECIMENS = [
  ['Solanum aureliae', 'Fruit has turned faintly luminous. Sugar content up 40 percent.'],
  ['Cyan fern', 'Fronds now track the planet instead of the grow lamps.'],
  ['Amber moss', 'Spores resonate at 7.3 second intervals. That number again.'],
  ['Violet creeper', 'Root system has grown into the shape of a ring. Unsettling.'],
  ['Red clover', 'Completely ordinary. Lindqvist tagged it as a control.'],
];

export const MEMENTOS = [
  ["Captain Marlow's model ship", 'A hand-painted Astraea. The paint on the bow is worn from being held.'],
  ["Okonkwo's lucky dice", 'One of them is slightly heavier than the other. Of course it is.'],
  ["Vasquez's mug", '"I void warranties." Still half full, and still warm somehow.'],
  ["Lindqvist's bonsai", 'Trimmed with surgical care. There is a tiny label: "Do not water. I mean it."'],
  ["Dr. Raman's stethoscope", 'Engraved: "Listen first." Good advice on any ship.'],
  ['Crew photograph', 'Six people squinting into a sun that is not this one. You are on the left.'],
];
