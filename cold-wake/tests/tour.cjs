// Screenshot tour of every area, for visual review.
//   node tests/tour.cjs [quality] [WxH]
const path = require('path');
const fs = require('fs');
const { chromium } = require('playwright');

const quality = process.argv[2] || 'high';
const [W, H] = (process.argv[3] || '960x540').split('x').map(Number);
const only = process.argv[4] ? process.argv[4].split(',') : null;
const LIT = ['podOpen', 'lockerOpen', 'hasTool', 'cryoPower', 'reachedHub', 'vegaOnline', 'fireStarted', 'reactorOnline', 'airlockFuse', 'hasSuit', 'evaDone'];
const SPOTS = [
  ['cryo', LIT, -9.5, 3.4, 205, -2],
  ['corridor', LIT, 0, 8.2, 0, 0],
  ['hub', LIT, 5.6, -9.2, 40, 6],
  ['aicore', LIT, -3.2, -31.2, 78, 6],
  ['quarters', LIT, 4.4, -26.6, -72, 0],
  ['medbay', LIT, -9.6, -13.6, 82, 0],
  ['lab', LIT, 10.2, -14.6, -84, 3],
  ['hydro', LIT, 4.2, 4.2, -72, 4],
  ['engineering', LIT, -6.2, 12.4, -145, 14],
  ['cargo', LIT, -9, 33.5, -150, 6],
  ['airlock', LIT, 13.2, 40, -90, 0],
  ['bridge', LIT, 0, -38.3, 0, 5],
  ['hull', LIT, 300, 16.5, 0, 3],
  ['hull-dish', LIT, 300.5, -47, 8, 10],
  ['dark-hub', ['podOpen', 'lockerOpen', 'hasTool', 'cryoPower'], 0, -8.4, 0, 6],
  ['dark-cryo', ['podOpen'], -9.5, 3.4, 205, -2],
];

(async () => {
  const out = path.resolve(__dirname, 'out');
  fs.mkdirSync(out, { recursive: true });
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await (await browser.newContext({ viewport: { width: W, height: H } })).newPage();
  const errors = [];
  page.on('console', (m) => m.type() === 'error' && errors.push(m.text()));
  page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}`));
  await page.goto('file://' + path.resolve(__dirname, '../public/index.html') + `?q=${quality}`);
  await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 240000 });
  let lastFlags = null;
  for (const [name, flags, x, z, yaw, pitch] of SPOTS) {
    if (only && !only.includes(name)) continue;
    await page.evaluate(
      ([flags, x, z, yaw, pitch, restart, name]) => {
        const g = window.__game;
        if (restart) g.debug.quickStart(flags);
        g.player.eva = name.startsWith('hull');
        g.hud.helmet(g.player.eva);
        if (name.startsWith('dark-hub')) g.player.toggleLamp(true);
        window.__look(x, z, yaw, pitch);
      },
      [flags, x, z, yaw, pitch, flags !== lastFlags, name]
    );
    lastFlags = flags;
    const f = await page.evaluate(() => window.__game.frame || 0);
    await page.waitForFunction((t) => (window.__game.frame || 0) >= t, f + 6, { timeout: 120000, polling: 100 });
    await page.screenshot({ path: path.join(out, `tour-${name}.png`) });
    const info = await page.evaluate(() => window.__game.debug.info());
    console.log(name.padEnd(12), `draws ${info.draws}`.padEnd(11), `tris ${info.tris}`.padEnd(13), `${info.frameMs} ms`);
  }
  await browser.close();
  console.log('console errors:', errors.length);
  for (const e of [...new Set(errors)].slice(0, 10)) console.log('  ', e.slice(0, 400));
})().catch((e) => {
  console.error('TOUR CRASHED:', e.message.split('\n')[0]);
  process.exit(2);
});
