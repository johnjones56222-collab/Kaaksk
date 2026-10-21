# 1 vs 100 Gorillas

A 3D browser game with three ways to settle the oldest debate on the internet:

| Mode | You are | Against | Progression |
| --- | --- | --- | --- |
| **1 Human vs 100 Gorillas** | a human | 100 gorillas over 8 waves | coins buy weapons and upgrades, aura buys 12 powers |
| **1 Gorilla vs 100 Humans** | a gorilla | 100 humans over 8 waves | bananas buy moves and upgrades, aura buys 10 gorilla powers |
| **100 Humans vs 1 Gorilla** | one of 100 humans | one huge gorilla | none: no powers, no weapons, no shop |

In the two wave modes you start with nothing, and dying once restarts every
progress: gear, upgrades, powers, coins and aura are all wiped.

Open `index.html` in a browser (Chrome, Edge, Firefox or Safari). It's a single
file: Three.js 0.160.0 loads from cdnjs and every sound and the music are
synthesized, so there are no other assets. It needs an internet connection the
first time, for Three.js and the fonts.

## Controls

| Desktop | Touch | Action |
| --- | --- | --- |
| `W` `A` `S` `D` | left thumb | Move |
| Mouse (click the game to lock it) | right thumb | Look and aim |
| Left click or `F` | 👊 / 🦍 | Attack |
| `Space` | ⤒ | Jump (double jump with the upgrade; also dodges ground pounds) |
| `Shift` | 💨 | Dash, with a moment of invincibility |
| `1`–`0`, `-`, `=` | tap the power icons | Use a power (12 keys for the human, 10 for the gorilla) |
| `Q` | 🔁 | Swap weapon or move |
| `B` | 🛒 | Open the shop (the enemies wait) |
| `Enter` | button | Start the next wave; on the title, play the last mode again |
| `Esc` / `P`, `M` | ⏸, 🔊 | Pause, mute (music has its own toggle in the pause menu) |

If the mouse can't be locked, hold the right mouse button and drag to look, or
turn with the arrow keys. Off-screen enemies are marked with arrows at the edge
of the screen.

## 1 Human vs 100 Gorillas

100 gorillas arrive over 8 waves (5, 7, 9, 11, 13, 15, 17, 23): Gorillas, Zoomers
(fast), Silverbacks (tanky, can't be interrupted), Coconut Chuckers (throw from
range) and finally the Silverback Supreme, a boss that ground-pounds. Between
waves you heal 35% and **Monke Mart** opens.

**Weapons (coins):** Bare Fists → Big Stick (22) → Frying Pan (40, every hit
stuns: BONK) → Baseball Bat (65) → Katana (170) → Banana Blaster (320, ranged) →
Ban Hammer (650) → Banana Bazooka (900, explosive splash).

| Key | Power | Aura | What it does |
| --- | --- | --- | --- |
| 1 | ⏱️ Time Stop | 40 | ZA WARUDO: every gorilla and coconut freezes for 5s. You keep moving. |
| 2 | 🔴 Laser Beam | 60 | 3.5s eye laser that pierces everything along the crosshair. |
| 3 | 💘 Mega Rizz | 50 | Rizz up Bae-nana, a female gorilla who fights for you for 40s. If she gets jumped (killed), you lose 75 aura. Hits on her cost nothing. |
| 4 | 🗿 Mog Wave | 35 | Shockwave that launches and stuns everything nearby. |
| 5 | 🍟 Fanum Tax | 70 | Takes half the HP of every gorilla on the map. You keep the coins. |
| 6 | 🤫 Mewing | 45 | 6 seconds of invincibility. |
| 7 | 🌱 Touch Grass | 30 | Grass sprouts around you and you heal 60% of your max HP over 4s. |
| 8 | 🐺 Sigma Grindset | 55 | 8s of double damage, faster attacks and faster legs. |
| 9 | 🍌 Banana Peels | 45 | Scatters 16 peels. Gorillas that step on one slip, flip and stay down. |
| 0 | 🌀 Delulu | 80 | Every gorilla goes delulu for 7s and fights the nearest gorilla instead of you. |
| - | 📄 Mega One-Shot Job Application Nuke | 10 | Press twice to confirm. Destroys everything, including you, so the run resets. Practically useless, but it's there. |
| = | 💸 FREE ROBUX | 200 | The best power ever. Every gorilla (the boss too) becomes a 1 HP Roblox bacon hair beggar, and new arrivals show up broke for 20s. |

## 1 Gorilla vs 100 Humans

The reverse mode: you're a silverback and 100 humans come for you over 8 waves:
Gym Bros, Influencers (their selfie flash blinds you), Zookeepers (tranquilizer
darts slow you down), Mall Cops (tanky, can't be interrupted) and finally
GIGACHAD. Humans drop bananas, and **The Banana Stand** sells moves instead of
weapons.

**Moves (bananas):** Knuckle Punch → Double Slam (30, hits four) → Haymaker (80)
→ Tree Trunk (180, hits everyone in front) → Boulder Toss (340, ranged splash) →
Silverback Smash (700).

| Key | Power | Aura | What it does |
| --- | --- | --- | --- |
| 1 | 🥁 Chest Beat | 35 | Every human nearby panics, takes damage and runs for 4s. |
| 2 | 🍌 Banana Barrage | 45 | A fan of 12 exploding bananas. |
| 3 | 💥 Ground Pound | 55 | Leap toward the crosshair and slam the ground. Huge shockwave. |
| 4 | 🦍 Call the Troop | 70 | Big Steve, Lil Tony and Grandpa Gus fight with you for 25s. |
| 5 | 🛢️ Barrel Roll | 50 | An exploding barrel that bowls straight through humans. |
| 6 | 🌿 Jungle Camo | 45 | Nearly invisible for 6s. Humans lose you, and your next hit deals triple damage. |
| 7 | 📱 Go Viral | 65 | Every human stops to film you for 5s. Free hits. |
| 8 | 🐒 Return to Monke | 80 | Grow huge for 8s: double damage, longer reach, heal on hit. |
| 9 | 🧬 Evolve | 10 | Press twice to confirm. You evolve into a human with a 9-to-5 and your run is over. The gorilla's useless button. |
| 0 | 📶 FREE WI-FI | 200 | The best gorilla power. Every human becomes a 1 HP doomscroller who forgets you exist, and new arrivals show up scrolling for 20s. |

## Upgrades (both wave modes)

Thirteen upgrades, each with a human and a gorilla name: max HP, damage, move
speed, armor, regen, pickup magnet, crit chance, attack speed, lifesteal, power
cooldowns, extra aura from kills, extra coins/bananas from drops, and a double
jump. The shop also sells a heal (Med Kit / Fruit Basket).

**Aura** comes from kills, 5-kill combos, clutch kills under 25% HP, clearing
waves and taking no damage in a wave. It can go negative.

## 100 Humans vs 1 Gorilla

You and 99 other humans against THE GORILLA (1600 HP). No powers, no weapons,
no shop: just your fists. After a countdown everyone charges.

- The gorilla swipes the closest humans away, grabs and throws people (thrown
  humans bowl over whoever they land on), spins when it's surrounded, and leaps
  onto the densest part of the crowd for a ground pound (red ring: jump or run).
  It roars at 66% and 33% HP, which makes nearby humans panic, and enrages at 33%,
  when it starts charging through the crowd.
- Your punches **rally** the humans around you, so they punch faster while you
  fight. If you only watch, the crowd wears the gorilla down but loses.
- Out of the fight for 5 seconds, you catch your breath and heal.
- If everyone else falls, you're the **last human standing** and your punches
  hit twice as hard.
- The kill feed shows who got yeeted. You win when the gorilla falls; you lose
  when you do.

## Records

Each mode keeps its own best, deaths and wins on this device (for the swarm
mode, the most survivors in a win). They're the only things that survive a death.

## Testing hook

`window.__g100` in the console exposes the run state plus helpers used by the
automated browser tests, for example `__g100.give(1000, 1000)` for coins and
aura, `__g100.startRun('reverse')`, or `__g100.step(seconds)` to advance game
time without rendering.
