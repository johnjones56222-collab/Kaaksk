# 1 vs 100 Gorillas

A 3D browser game: one human, one hundred gorillas. You start with nothing,
gorillas drop coins, coins buy gear, kills earn aura, aura buys powers.
Die once and every progress is restarted.

Open `index.html` in a browser (Chrome, Edge, Firefox or Safari). It's a single
file: Three.js 0.160.0 loads from cdnjs and every sound is synthesized, so there
are no other assets. It needs an internet connection the first time for Three.js
and the fonts.

## Controls

| Desktop | Touch | Action |
| --- | --- | --- |
| `W` `A` `S` `D` | left thumb | Move |
| Mouse (click the game to lock it) | right thumb | Look and aim |
| Left click or `F` | 👊 | Attack |
| `Space` | ⤒ | Jump (also dodges the boss's ground pound) |
| `Shift` | 💨 | Dash, with a moment of invincibility |
| `1`–`8` | tap the power icons | Use a power |
| `Q` | 🔁 | Swap weapon |
| `B` | 🛒 | Monke Mart (pauses the game) |
| `Enter` | button | Start the next wave |
| `Esc` / `P`, `M` | ⏸, 🔊 | Pause, mute |

If the mouse can't be locked, hold the right mouse button and drag to look,
or turn with the arrow keys.

## How a run works

- 100 gorillas arrive over 8 waves (5, 7, 9, 11, 13, 15, 17, 23). The last wave ends
  with the Silverback Supreme, a boss that ground-pounds. Jump to dodge it.
- Enemy types: Gorilla, Zoomer (fast), Silverback (tanky, can't be interrupted),
  Coconut Chucker (throws from range) and the boss.
- Between waves you heal 35% and Monke Mart opens. You can also shop mid-fight;
  the jungle waits.
- **Coins** buy weapons (fists → Big Stick → Baseball Bat → Katana → Banana
  Blaster → Ban Hammer), upgrades (HP, damage, speed, armor, regen, coin magnet)
  and med kits.
- **Aura** comes from kills, 5-kill combos, clutch kills under 25% HP, clearing
  waves and taking no damage in a wave. Aura can go negative.
- **Permadeath**: dying (or giving up) wipes coins, aura, gear, upgrades and
  powers, and you start again at wave 1 with your fists. The only things kept
  are your best score, deaths and wins.

## Powers (bought once per run with aura)

| Key | Power | Aura | What it does |
| --- | --- | --- | --- |
| 1 | ⏱️ Time Stop | 40 | ZA WARUDO: every gorilla and coconut freezes for 5s. Knockback lands when time resumes. |
| 2 | 🔴 Laser Beam | 60 | 3.5s eye laser that pierces everything along the crosshair. |
| 3 | 💘 Mega Rizz | 50 | Rizz up Bae-nana, a female gorilla who fights for you for 40s. Each hit she takes costs 12 aura; if she dies you lose 60. |
| 4 | 🗿 Mog Wave | 35 | Shockwave that damages, launches and stuns everything nearby. |
| 5 | 🍟 Fanum Tax | 70 | Takes half the HP of every gorilla on the map and turns it into coins. |
| 6 | 🤫 Mewing | 45 | 6 seconds of invincibility. |
| 7 | 📄 Mega One-Shot Job Application Nuke | 10 | Press twice to confirm. Destroys everything, including you, so the run resets. Practically useless, but it's there. |
| 8 | 💸 FREE ROBUX | 200 | The best power ever. Every gorilla (the boss too) becomes a 1 HP bacon hair beggar, and new arrivals show up broke for 20s. They say "oof" and fall apart when hit. |

## Testing hook

`window.__g100` in the console exposes the run state plus helpers used by the
automated browser tests, for example `__g100.give(1000, 1000)` for coins and aura,
or `__g100.step(seconds)` to advance game time without rendering.
