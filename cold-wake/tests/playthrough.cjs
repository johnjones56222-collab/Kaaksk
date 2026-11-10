// Automated playthrough: boots the built page in headless Chromium and drives
// the whole story through the game's debug hooks, solving every console
// puzzle, then checks that it reached the ending without console errors.
//   node tests/playthrough.cjs [--shots] [--mobile]
const path = require('path');
const fs = require('fs');
const { chromium, devices } = require('playwright');

const SHOTS = process.argv.includes('--shots');
const MOBILE = process.argv.includes('--mobile');
const outDir = path.resolve(__dirname, 'out');
fs.mkdirSync(outDir, { recursive: true });

(async () => {
  const browser = await chromium.launch({
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--autoplay-policy=no-user-gesture-required'],
  });
  let ctxOpts = { viewport: { width: 800, height: 450 }, deviceScaleFactor: 1 };
  if (MOBILE) {
    const d = devices['Pixel 7'];
    ctxOpts = { ...d, viewport: { width: d.viewport.height, height: d.viewport.width }, deviceScaleFactor: 1 };
  }
  const ctx = await browser.newContext(ctxOpts);
  const page = await ctx.newPage();
  const errors = [];
  const warnings = [];
  page.on('console', (m) => {
    if (m.type() === 'error') errors.push(m.text());
    else if (m.type() === 'warning') warnings.push(m.text());
  });
  page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}\n${e.stack || ''}`));
  const t0 = Date.now();
  const log = (...a) => console.log(`[${((Date.now() - t0) / 1000).toFixed(0)}s]`, ...a);
  const ev = (code) => page.evaluate(`(async () => { const g = window.__game; const d = g.debug; ${code} })()`);
  const shot = async (name) => {
    if (SHOTS) await page.screenshot({ path: path.join(outDir, `pt-${name}.png`) });
  };
  const waitFor = (code, timeout = 60000) => page.waitForFunction(`(() => { const g = window.__game; const d = g && g.debug; return ${code}; })()`, null, { timeout, polling: 200 });
  const problems = [];
  // wait for the game to draw n more frames (the software renderer is slow)
  const frames = async (n = 3) => {
    const f = await page.evaluate(() => window.__game.frame || 0);
    await page.waitForFunction((t) => (window.__game.frame || 0) >= t, f + n, { timeout: 30000, polling: 50 });
  };
  const note = (m) => {
    problems.push(m);
    console.log('  PROBLEM:', m);
  };

  await page.goto('file://' + path.resolve(__dirname, '../public/index.html') + '?q=low');
  await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 180000 });
  if (await page.evaluate(() => window.__error)) throw new Error('boot failed: ' + (await page.evaluate(() => window.__error)));
  log('booted');
  await shot('00-title');
  // clear any old save so the run is deterministic
  await page.evaluate(() => {
    try {
      localStorage.clear();
    } catch {}
  });
  await page.click('#overlay-menu .btn.primary');
  await page.waitForSelector('#overlay-intro:not([hidden])', { timeout: 20000 });
  await page.click('#intro-skip');
  await waitFor(`g.mode === 'play'`);
  await page.waitForTimeout(600);
  log('in game', JSON.stringify(await ev('return d.info()')));
  await shot('01-pod');

  // Use an interactable the way a player would: walk up, look at it, press.
  async function use(id, { expectFocus = true, solve = false, waitMode = 'play', anchor = id } = {}) {
    await ev(`d.approach('${anchor}')`);
    await frames(3);
    const focus = await ev('return d.focus()');
    if (expectFocus && focus !== id) note(`focus: wanted ${id}, got ${focus} @ ${JSON.stringify(await ev('return d.info().pos'))}`);
    const r = await ev(`return d.use('${id}')`);
    if (r !== 'ok') note(`use(${id}) -> ${r}`);
    if (solve) {
      await waitFor('d.minigameOpen()', 15000);
      await page.waitForTimeout(250);
      if (SHOTS) await shot(`mg-${id}`);
      await ev('d.solve()');
      await waitFor('!d.minigameOpen()', 20000);
    }
    if (waitMode) await waitFor(`g.mode === '${waitMode}'`, 30000);
    await page.waitForTimeout(120);
    return r;
  }
  const expectStep = async (want) => {
    const s = await ev('return d.step()');
    if (s !== want) note(`step: wanted ${want}, got ${s}`);
    log('step ->', s);
  };

  // ---- Mission 1 ----
  await use('podLatch');
  await expectStep('tool');
  await use('locker');
  await use('locker');
  await expectStep('breaker');
  await shot('02-cryo-dark');
  await use('breaker', { solve: true });
  await expectStep('hub');
  await page.waitForTimeout(1800);
  await shot('03-cryo-lit');
  // a held interaction through the real keyboard path
  await ev(`d.approach('panel1')`);
  await page.waitForTimeout(300);
  await page.keyboard.down('KeyE');
  await waitFor(`d.has('panel1')`, 30000).catch(() => problems.push('hold-to-patch on panel1 never completed'));
  await page.keyboard.up('KeyE');
  // ---- Mission 2 ----
  await ev(`d.teleport('hub', 0)`);
  await page.waitForTimeout(500);
  await expectStep('vega');
  await shot('04-hub-dark');
  await use('vegaConsole', { solve: true });
  await expectStep('cells');
  await page.waitForTimeout(800);
  await shot('05-aicore');
  // ---- Mission 3 ----
  await use('cellEng');
  await use('cellLab');
  await ev(`d.approach('log_vasquez')`);
  await page.waitForTimeout(300);
  await ev(`d.use('log_vasquez')`);
  await waitFor(`g.mode === 'reader'`);
  await shot('06-reader');
  await page.click('#overlay-reader .btn.primary');
  await waitFor(`g.mode === 'play'`);
  await use('safe', { solve: true });
  await use('safe');
  await expectStep('install');
  for (const n of [1, 2, 3]) await use(`socket${n}`);
  await expectStep('ignite');
  await shot('07-engineering-dark');
  await use('ignite', { solve: true });
  await expectStep('irrigation');
  await page.waitForTimeout(2500);
  await shot('08-engineering-lit');
  // ---- Mission 4 ----
  await use('irrigation', { solve: true });
  for (const n of [1, 2, 3]) await use(`valve${n}`);
  await expectStep('ext');
  await shot('09-hydro');
  // ---- Mission 5 ----
  await use('extinguisher');
  await ev(`d.teleport('medbay', 90)`);
  await page.waitForTimeout(700);
  await shot('10-medbay-fire');
  // hazard: standing in the flames hurts, and running out of health respawns you in the hub
  {
    await ev(`const f = g.world.refs.fire1; g.player.place(f.x, f.z + 0.2, 0, 0); g.player.health = 30;`);
    await waitFor(`g.player.health < 30 || g.mode !== 'play'`, 30000).catch(() => note('fire did not hurt the player'));
    await ev(`g.player.health = 0.5`);
    await waitFor(`g.mode === 'play' && g.player.health > 90 && d.info().room === 'hub'`, 40000).catch(() => note('no respawn after health ran out'));
    log('fire hazard and respawn ok');
  }
  // quit to the title mid-mission, then continue from the save
  {
    const before = await ev('return d.step()');
    await ev(`g.pause('mission')`);
    await page.waitForSelector('#overlay-pause:not([hidden])');
    await page.click('#overlay-pause .btn.danger');
    await waitFor(`g.mode === 'menu'`);
    const first = await page.textContent('#overlay-menu .btn.primary');
    if (first !== 'Continue') note(`title offers "${first}" instead of Continue`);
    await page.click('#overlay-menu .btn.primary');
    await waitFor(`g.mode === 'play'`, 30000);
    await frames(3);
    const after = await ev('return d.step()');
    if (after !== before) note(`continue landed on ${after}, expected ${before}`);
    if (!(await ev(`return g.world.refs.fire1.on`))) note('fires not burning after continue');
    log('quit and continue ok');
  }
  for (const n of [1, 2, 3, 4]) await use(`fire${n}`, { expectFocus: false });
  await expectStep('compound');
  await use('synth', { solve: true });
  await expectStep('signal');
  // ---- Mission 6 ----
  await use('spectro', { solve: true });
  await expectStep('fuse');
  await shot('11-lab');
  // ---- Mission 7 ----
  await use('fuse', { solve: true });
  await use('suit');
  await expectStep('cycle');
  await use('cycle', { waitMode: null });
  await waitFor(`g.mode === 'play' && g.player.eva`, 40000);
  await page.waitForTimeout(1500);
  await expectStep('breaches');
  await shot('12-eva');
  // hazard: suit oxygen runs out -> pulled back inside, then go out again
  {
    await ev(`g.o2 = 0.4`);
    await waitFor(`g.mode === 'play' && !g.player.eva`, 40000).catch(() => note('no retrieval when suit oxygen ran out'));
    await frames(3);
    const room = await ev('return d.info().room');
    if (room !== 'airlock') note(`after retrieval the player is in ${room}`);
    if (await ev(`return d.has('evaDone')`)) note('EVA marked done after a forced retrieval');
    await frames(3);
    await use('cycle', { waitMode: null });
    await waitFor(`g.mode === 'play' && g.player.eva`, 40000);
    await page.waitForTimeout(1200);
    log('oxygen hazard and second excursion ok');
  }
  // welding through the real hold-to-work path
  {
    await ev(`d.approach('breachStand1')`);
    await frames(3);
    await ev(`const a = g.world.anchors.breach1; const p = g.player; p.yaw = Math.atan2(-(a.x - p.x), -(a.z - p.z)); p.pitch = -0.25;`);
    await frames(3);
    const f = await ev('return d.focus()');
    if (f !== 'breach1') note(`weld focus: wanted breach1, got ${f}`);
    await page.keyboard.down('KeyE');
    await waitFor(`d.has('breach1')`, 60000).catch(() => note('hold-to-weld on breach 1 never completed'));
    await page.keyboard.up('KeyE');
  }
  for (const n of [2, 3]) await use(`breach${n}`, { anchor: `breachStand${n}`, expectFocus: false });
  await expectStep('antenna');
  await use('dish', { solve: true });
  await expectStep('return');
  await shot('13-dish');
  await use('hatch', { waitMode: null });
  await waitFor(`g.mode === 'play' && !g.player.eva`, 40000);
  await page.waitForTimeout(800);
  await expectStep('bridge');
  // ---- Mission 8 ----
  await ev(`d.teleport('bridge', 0)`);
  await page.waitForTimeout(600);
  await expectStep('turret');
  await shot('14-bridge-alert');
  await use('tactical', { waitMode: 'turret' });
  await page.waitForTimeout(6000);
  await shot('15-turret');
  // fire a few real shots, then clear the field
  await page.mouse.move(400, 200);
  await page.mouse.down();
  await page.waitForTimeout(1200);
  await page.mouse.up();
  // pause and resume inside gunnery, and survive a failed wave
  await ev(`g.pause('help')`);
  await page.waitForSelector('#overlay-pause:not([hidden])');
  await page.click('#overlay-pause .btn.primary');
  await waitFor(`g.mode === 'turret'`, 10000).catch(() => note('resume did not return to the turret'));
  await ev('g.turret._fail()');
  await page.waitForTimeout(800);
  if ((await ev('return g.turret.hull')) !== 100) note('turret hull not reset after a failed wave');
  await ev('g.turret.forceWin()');
  await waitFor(`g.mode === 'play'`, 30000);
  await expectStep('route');
  // ---- Mission 9 ----
  await use('navTable', { solve: true });
  await expectStep('jump');
  // ---- optional content ----
  const logs = ['cryo', 'hub', 'corridor', 'ai', 'mess', 'med', 'lab', 'hydro', 'eng', 'cargo', 'bridge'];
  for (const id of logs) {
    await ev(`d.approach('log_${id}')`);
    await frames(3);
    const f = await ev('return d.focus()');
    if (f !== `log_${id}`) note(`focus: wanted log_${id}, got ${f}`);
    await ev(`d.use('log_${id}')`);
    await waitFor(`g.mode === 'reader'`, 10000);
    await page.click('#overlay-reader .btn.primary');
    await waitFor(`g.mode === 'play'`, 10000);
  }
  for (const n of [2, 3, 4, 5, 6]) await use(`panel${n}`);
  for (const n of [1, 2, 3, 4, 5]) await use(`plant${n}`);
  for (const n of [1, 2, 3, 4, 5, 6]) await use(`mem${n}`);
  await use('coffee');
  await use('jukebox');
  await use('holo');
  // datapad tabs
  await ev(`g.pause('mission')`);
  await page.waitForSelector('#overlay-pause:not([hidden])');
  for (const i of [1, 2, 3, 4, 5, 0]) {
    await page.click(`#overlay-pause .tab:nth-child(${i + 1})`);
    await page.waitForTimeout(250);
    if (SHOTS) await shot(`pad-${i}`);
  }
  await page.click('#overlay-pause .btn.primary');
  await waitFor(`g.mode === 'play'`);
  const side = await ev(`return { logs: d.state.logs.length, flags: Object.keys(d.state.flags).length }`);
  if (side.logs !== 12) problems.push(`logs collected ${side.logs}/12`);
  log('optional content done', JSON.stringify(side));
  // ---- the jump ----
  await use('jump', { waitMode: null });
  await page.waitForTimeout(5000);
  await shot('16-jump');
  await waitFor(`g.mode === 'ended'`, 90000);
  await page.waitForTimeout(800);
  await shot('17-end');
  const info = await ev('return d.info()');
  log('finished', JSON.stringify(info));
  // back to title, and the save should not offer Continue after finishing
  await page.click('#overlay-end .btn.primary');
  await waitFor(`g.mode === 'menu'`);
  const btns = await page.$$eval('#overlay-menu .btn', (b) => b.map((x) => x.textContent));
  if (btns.includes('Continue')) problems.push('Continue offered after finishing');

  await browser.close();
  console.log('\n==== RESULT ====');
  console.log('console errors:', errors.length);
  for (const e of [...new Set(errors)].slice(0, 20)) console.log('  ', e.slice(0, 600));
  console.log('warnings:', [...new Set(warnings)].length);
  for (const w of [...new Set(warnings)].slice(0, 10)) console.log('  ', w.slice(0, 300));
  console.log('problems:', problems.length);
  for (const p of problems) console.log('  ', p);
  if (errors.length || problems.length) process.exit(1);
  console.log('PLAYTHROUGH OK');
})().catch((e) => {
  console.error('TEST CRASHED:', e.message);
  process.exit(2);
});
