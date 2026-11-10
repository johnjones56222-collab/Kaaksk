// Touch-control check on an emulated phone: the floating move stick, drag
// look, the USE button and the HUD layout in landscape and portrait.
//   node tests/touch.cjs
const path = require('path');
const fs = require('fs');
const { chromium, devices } = require('playwright');

const outDir = path.resolve(__dirname, 'out');
fs.mkdirSync(outDir, { recursive: true });

(async () => {
  const browser = await chromium.launch({
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'],
  });
  const d = devices['Pixel 7'];
  const land = { width: d.viewport.height, height: d.viewport.width };
  const ctx = await browser.newContext({ ...d, viewport: land, deviceScaleFactor: 1 });
  const page = await ctx.newPage();
  const errors = [];
  const problems = [];
  page.on('console', (m) => m.type() === 'error' && errors.push(m.text()));
  page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}`));
  const ev = (code) => page.evaluate(`(async () => { const g = window.__game; const d = g.debug; ${code} })()`);
  const waitFor = (code, timeout = 60000) => page.waitForFunction(`(() => { const g = window.__game; const d = g && g.debug; return ${code}; })()`, null, { timeout, polling: 200 });
  // synthetic touch pointer on the play layer
  const touch = (type, id, x, y) =>
    page.evaluate(
      ([type, id, x, y]) => {
        const el = document.getElementById('touch-layer');
        el.dispatchEvent(new PointerEvent(type, { pointerId: id, pointerType: 'touch', clientX: x, clientY: y, bubbles: true, cancelable: true, isPrimary: id === 11 }));
      },
      [type, id, x, y]
    );

  await page.goto('file://' + path.resolve(__dirname, '../public/index.html') + '?q=low');
  await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 180000 });
  await page.evaluate(() => {
    try {
      localStorage.clear();
    } catch {}
  });
  await page.screenshot({ path: path.join(outDir, 'touch-00-title.png') });
  await page.tap('#overlay-menu .btn.primary');
  await page.waitForSelector('#overlay-intro:not([hidden])', { timeout: 20000 });
  await page.tap('#intro-skip');
  await waitFor(`g.mode === 'play'`);
  await page.waitForTimeout(800);
  const mode = await ev('return g.input.mode');
  if (mode !== 'touch') problems.push(`input mode is ${mode}, expected touch`);
  if (!(await page.isVisible('#btn-interact'))) problems.push('USE button not visible in touch mode');
  await page.screenshot({ path: path.join(outDir, 'touch-01-pod.png') });

  // USE button opens the pod
  await ev(`d.approach('podLatch', 0.5)`);
  await page.waitForTimeout(500);
  await page.tap('#btn-interact');
  await waitFor(`d.has('podOpen')`, 15000).catch(() => problems.push('USE button did not release the pod latch'));
  await page.waitForTimeout(1500);

  // look: drag on the right half
  const yaw0 = await ev('return g.player.yaw');
  await touch('pointerdown', 12, land.width * 0.7, land.height * 0.5);
  for (let i = 1; i <= 8; i++) {
    await touch('pointermove', 12, land.width * 0.7 + i * 14, land.height * 0.5);
    await page.waitForTimeout(60);
  }
  await touch('pointerup', 12, land.width * 0.7 + 112, land.height * 0.5);
  await page.waitForTimeout(400);
  const yaw1 = await ev('return g.player.yaw');
  if (Math.abs(yaw1 - yaw0) < 0.1) problems.push(`drag-look did not turn the view (${yaw0} -> ${yaw1})`);

  // move: floating stick on the left half, pushed forward
  await ev(`g.player.place(-9, 0.5, Math.PI, 0)`);
  await page.waitForTimeout(300);
  const p0 = await ev('return [g.player.x, g.player.z]');
  await touch('pointerdown', 11, land.width * 0.2, land.height * 0.6);
  await touch('pointermove', 11, land.width * 0.2, land.height * 0.6 - 50);
  await page.waitForTimeout(500);
  const stickShown = await page.isVisible('#stick-base');
  if (!stickShown) problems.push('move stick did not appear');
  await page.screenshot({ path: path.join(outDir, 'touch-02-moving.png') });
  // two fingers at once: keep moving while looking
  await touch('pointerdown', 12, land.width * 0.75, land.height * 0.4);
  await touch('pointermove', 12, land.width * 0.75 - 40, land.height * 0.4);
  await page.waitForTimeout(2500);
  await touch('pointerup', 12, land.width * 0.75 - 40, land.height * 0.4);
  await touch('pointerup', 11, land.width * 0.2, land.height * 0.6 - 50);
  await page.waitForTimeout(300);
  const p1 = await ev('return [g.player.x, g.player.z]');
  const moved = Math.hypot(p1[0] - p0[0], p1[1] - p0[1]);
  if (moved < 0.5) problems.push(`stick did not move the player (${moved.toFixed(2)} m)`);
  if (await page.isVisible('#stick-base')) problems.push('move stick still visible after release');
  const mv = await ev('return [g.input.moveX, g.input.moveY]');
  if (mv[0] !== 0 || mv[1] !== 0) problems.push('movement did not stop on release');

  // pause button and datapad on a small screen
  await page.tap('#btn-pause');
  await page.waitForSelector('#overlay-pause:not([hidden])', { timeout: 10000 }).catch(() => problems.push('pause button did not open the datapad'));
  await page.screenshot({ path: path.join(outDir, 'touch-03-datapad.png') });
  for (const i of [2, 5, 6]) {
    await page.tap(`#overlay-pause .tab:nth-child(${i})`);
    await page.waitForTimeout(300);
    await page.screenshot({ path: path.join(outDir, `touch-04-tab${i}.png`) });
  }
  await page.tap('#overlay-pause .btn.primary');
  await waitFor(`g.mode === 'play'`);

  // a console puzzle on a small screen
  await ev(`d.setFlag('lockerOpen'); d.setFlag('hasTool'); d.approach('breaker'); `);
  await page.waitForTimeout(500);
  await page.tap('#btn-interact');
  await waitFor('d.minigameOpen()', 15000).catch(() => problems.push('USE did not open the breaker puzzle'));
  await page.waitForTimeout(300);
  await page.screenshot({ path: path.join(outDir, 'touch-05-puzzle.png') });
  const fits = await page.evaluate(() => {
    const p = document.querySelector('#overlay-minigame .panel').getBoundingClientRect();
    return p.left >= 0 && p.right <= window.innerWidth + 1 && p.top >= -1 && p.bottom <= window.innerHeight + 1;
  });
  if (!fits) problems.push('puzzle panel overflows the phone screen');
  await ev('d.solve()');
  await waitFor('!d.minigameOpen()', 15000);
  await page.waitForTimeout(2000);
  await page.screenshot({ path: path.join(outDir, 'touch-06-lit.png') });

  // portrait
  await page.setViewportSize({ width: d.viewport.width, height: d.viewport.height });
  await page.waitForTimeout(1200);
  await page.screenshot({ path: path.join(outDir, 'touch-07-portrait.png') });
  const overflow = await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth + 1);
  if (overflow) problems.push('page scrolls sideways in portrait');
  await page.tap('#btn-pause');
  await page.waitForSelector('#overlay-pause:not([hidden])', { timeout: 10000 });
  await page.waitForTimeout(300);
  await page.screenshot({ path: path.join(outDir, 'touch-08-portrait-pad.png') });

  await browser.close();
  console.log('console errors:', errors.length);
  for (const e of [...new Set(errors)].slice(0, 10)) console.log('  ', e.slice(0, 500));
  console.log('problems:', problems.length);
  for (const p of problems) console.log('  ', p);
  if (errors.length || problems.length) process.exit(1);
  console.log('TOUCH OK');
})().catch((e) => {
  console.error('TEST CRASHED:', e.message.split('\n')[0]);
  process.exit(2);
});
