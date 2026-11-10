// Walkability check: steps the player controller along a route through every
// door and room (and the hull catwalk) at 60 Hz without rendering, and reports
// anywhere the walker gets stuck on geometry.
const path = require('path');
const { chromium } = require('playwright');

const INTERIOR = [
  ['leave pod', -7.6, 5.5], ['cryo door', -4.5, 1], ['mid corridor', 0, 1], ['hub door', 0, -5], ['hub', 0, -9],
  ['round the table', 3.2, -11], ['hub north', 3.2, -17], ['fore door', 0, -20.5], ['fore corridor', 0, -24],
  ['ai door', 0, -31], ['ai core', -4.6, -31], ['back', 0, -31], ['quarters door', 0, -27], ['quarters', 5, -27],
  ['mess', 5.5, -30.5], ['back', 4, -27], ['corridor', 0, -27], ['bridge door', 0, -35], ['bridge', 0, -38.4],
  ['nav table side', -2.4, -41], ['helm', 0, -44.6], ['captain side', 2.6, -41], ['bridge exit', 0, -38],
  ['corridor', 0, -24], ['hub', 0, -20.5], ['hub west', -3.2, -17], ['med door', -6, -14], ['med bay', -10, -14],
  ['med centre', -14.5, -13.2], ['med back', -10, -14], ['hub', -6, -14], ['south arc', -3.2, -11], ['south arc', 3.2, -11],
  ['lab door', 6, -14], ['lab', 10.5, -14], ['lab console', 14.8, -10.6], ['lab back', 10.5, -14], ['hub', 6, -14],
  ['hub south', 3.2, -11], ['hub exit', 0, -9], ['corridor', 0, -5], ['hydro door', 0, 5], ['hydro', 4.2, 5],
  ['hydro aisle', 5, 3.4], ['hydro north', 5, -3.2], ['scrubbers', 14, -3.2], ['hydro east', 17, 0.2], ['hydro south', 16.8, 6.9],
  ['hydro aisle', 5, 6.9], ['hydro door', 4.2, 5], ['corridor', 0, 5], ['engine door', 0, 9], ['engineering', 0, 12],
  ['console side', -3, 13.4], ['port walkway', -4.6, 16], ['reactor west', -4.6, 20], ['reactor south-west', -4.6, 25],
  ['aft', 0, 28.6], ['cargo door', 0, 32], ['cargo', 2.6, 34.5], ['past crates', 8.6, 38.6], ['airlock door', 10, 40],
  ['airlock', 14.2, 40], ['suit bay', 15, 38.7], ['console', 14.6, 41.2], ['airlock exit', 10.5, 40], ['shuttle bow', 1, 46.5],
  ['shield', -4, 47.6], ['port side', -8.6, 41], ['cargo north', -5, 36.4], ['cargo door', 0, 32], ['engineering', 0, 28.6],
  ['starboard walkway', 4.6, 25], ['reactor east', 4.6, 16], ['exit', 0, 12],
];
const HULL = [
  ['catwalk', 300, 12], ['breach 1', 297.4, 10], ['catwalk', 300, 10], ['catwalk', 300, -10], ['breach 2', 302.6, -10],
  ['catwalk', 300, -10], ['catwalk', 300, -28], ['breach 3', 297.4, -28], ['catwalk', 300, -28], ['dish gate', 300, -49.5],
  ['dish console', 302.2, -53.2], ['platform', 297.6, -52], ['back', 300, -49.5], ['hatch', 300, 19.6],
];

(async () => {
  const browser = await chromium.launch({
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'],
  });
  const page = await (await browser.newContext({ viewport: { width: 640, height: 360 } })).newPage();
  const errors = [];
  page.on('console', (m) => m.type() === 'error' && errors.push(m.text()));
  page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}`));
  await page.goto('file://' + path.resolve(__dirname, '../public/index.html') + '?q=low');
  await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 180000 });
  const run = (route, start, eva) =>
    page.evaluate(
      ([route, start, eva]) => {
        const g = window.__game;
        const p = g.player;
        const out = [];
        if (start) p.place(start[0], start[1], 0, 0);
        p.eva = eva;
        g.input.capture = true;
        const dt = 1 / 60;
        let total = 0;
        for (const [name, tx, tz] of route) {
          const d0 = Math.hypot(tx - p.x, tz - p.z);
          const limit = d0 / (eva ? 2 : 2.6) + 3.5;
          let t = 0;
          let ok = false;
          while (t < limit) {
            const dx = tx - p.x;
            const dz = tz - p.z;
            if (Math.hypot(dx, dz) < 0.45) {
              ok = true;
              break;
            }
            p.yaw = Math.atan2(-dx, -dz);
            g.input.moveY = 1;
            g.input.moveX = 0;
            p.update(dt);
            g.world.update(dt, p.x, p.z, g.gfx.camera.position);
            t += dt;
          }
          total += t;
          if (!ok) {
            out.push(`stuck heading to "${name}" (${tx}, ${tz}) at (${p.x.toFixed(2)}, ${p.z.toFixed(2)}) in ${g.world.current ? g.world.current.id : '?'}`);
            p.place(tx, tz, p.yaw, 0);
          }
        }
        g.input.moveY = 0;
        return { out, total: Number(total.toFixed(1)), end: [Number(p.x.toFixed(2)), Number(p.z.toFixed(2))], visited: Object.keys(g.debug.state.visited) };
      },
      [route, start, eva]
    );
  await page.evaluate(() => window.__game.debug.quickStart(['podOpen', 'lockerOpen', 'hasTool', 'cryoPower', 'reachedHub', 'vegaOnline', 'fireStarted', 'reactorOnline', 'firesDone', 'airlockFuse', 'hasSuit', 'evaStarted', 'evaDone']));
  await page.waitForTimeout(500);
  const a = await run(INTERIOR, null, false);
  console.log(`interior: ${INTERIOR.length} legs, ${a.total}s of walking, rooms visited: ${a.visited.length}`);
  for (const s of a.out) console.log('  ', s);
  const b = await run(HULL, [300, 19], true);
  console.log(`hull: ${HULL.length} legs, ${b.total}s of walking`);
  for (const s of b.out) console.log('  ', s);
  await browser.close();
  const missing = ['cryo', 'spineB', 'hub', 'spineA', 'aicore', 'quarters', 'bridge', 'medbay', 'lab', 'hydro', 'engineering', 'cargo', 'airlock'].filter((r) => !a.visited.includes(r));
  if (missing.length) console.log('rooms never entered:', missing.join(', '));
  console.log('console errors:', errors.length);
  for (const e of [...new Set(errors)].slice(0, 10)) console.log('  ', e.slice(0, 400));
  const bad = a.out.length + b.out.length + errors.length + missing.length;
  console.log(bad ? 'WALK FAILED' : 'WALK OK');
  process.exit(bad ? 1 : 0);
})().catch((e) => {
  console.error('TEST CRASHED:', e.message.split('\n')[0]);
  process.exit(2);
});
