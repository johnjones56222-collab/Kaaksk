// Save / continue check: play part of the way, reload the page, continue,
// and confirm the story step, position, power and door locks come back.
const path = require('path');
const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'],
  });
  const page = await (await browser.newContext({ viewport: { width: 800, height: 450 } })).newPage();
  const errors = [];
  const problems = [];
  page.on('console', (m) => m.type() === 'error' && errors.push(m.text()));
  page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}`));
  const ev = (code) => page.evaluate(`(async () => { const g = window.__game; const d = g.debug; ${code} })()`);
  const waitFor = (code, timeout = 60000) => page.waitForFunction(`(() => { const g = window.__game; const d = g && g.debug; return ${code}; })()`, null, { timeout, polling: 200 });
  const url = 'file://' + path.resolve(__dirname, '../public/index.html') + '?q=low';
  const boot = async () => {
    await page.goto(url);
    await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 180000 });
  };
  await boot();
  await page.evaluate(() => {
    try {
      localStorage.clear();
    } catch {}
  });
  await page.reload();
  await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 180000 });
  const first = await page.$$eval('#overlay-menu .btn', (b) => b.map((x) => x.textContent));
  if (first.includes('Continue')) problems.push('Continue offered with no save');
  await page.click('#overlay-menu .btn.primary');
  await page.waitForSelector('#overlay-intro:not([hidden])');
  await page.click('#intro-skip');
  await waitFor(`g.mode === 'play'`);
  // advance the story through the reactor start
  await ev(`for (const f of ['podOpen','lockerOpen','hasTool','cryoPower','reachedHub','vegaOnline','cellEng','cellLab','safeOpen','cellQuarters','socket1','socket2','socket3','fireStarted','reactorOnline','log_hub','panel2']) d.setFlag(f); d.state.logs.push('hub'); d.teleport('hydro', 45);`);
  await page.waitForTimeout(1500);
  await ev(`d.setFlag('irrigation')`);
  await page.waitForTimeout(800);
  const before = await ev(`return { step: d.step(), pos: d.info().pos, code: d.state.safeCode, logs: d.state.logs.length, time: d.state.stats.time }`);
  // reload and continue
  await boot();
  const btns = await page.$$eval('#overlay-menu .btn', (b) => b.map((x) => x.textContent));
  if (btns[0] !== 'Continue') problems.push(`expected Continue first, got ${JSON.stringify(btns)}`);
  await page.click('#overlay-menu .btn.primary');
  await waitFor(`g.mode === 'play'`);
  await page.waitForTimeout(1500);
  const after = await ev(`return { step: d.step(), pos: d.info().pos, room: d.info().room, code: d.state.safeCode, logs: d.state.logs.length,
    power: g.world.room('hydro').power, cryoLocked: g.world.door.d_cryo.locked, bridgeLocked: !!g.world.door.d_bridge.locked, airlockLocked: !!g.world.door.d_airlock.locked,
    fire1: g.world.refs.fire1.on, cellVisible: g.world.rooms.length > 0 }`);
  console.log('before', JSON.stringify(before));
  console.log('after ', JSON.stringify(after));
  if (after.step !== before.step) problems.push(`step ${before.step} -> ${after.step}`);
  if (after.step !== 'valves') problems.push(`expected step valves, got ${after.step}`);
  if (after.code !== before.code) problems.push('safe code changed across reload');
  if (after.logs !== before.logs) problems.push('logs lost');
  if (after.room !== 'hydro') problems.push(`room is ${after.room}`);
  if (Math.hypot(after.pos[0] - before.pos[0], after.pos[1] - before.pos[1]) > 1) problems.push('position not restored');
  if (after.power < 0.9) problems.push(`hydro power ${after.power}`);
  if (after.cryoLocked) problems.push('cryo door locked after load');
  if (!after.bridgeLocked || !after.airlockLocked) problems.push('bridge/airlock should still be locked');
  if (!after.fire1) problems.push('med bay fire not burning after load');
  // starting over wipes it
  await ev(`g.pause('mission')`);
  await page.waitForSelector('#overlay-pause:not([hidden])');
  await page.click('#overlay-pause .btn.danger');
  await waitFor(`g.mode === 'menu'`);
  await page.click('#overlay-menu .btn:nth-of-type(2)');
  await page.waitForTimeout(300);
  await page.click('#overlay-menu .btn.danger');
  await page.waitForSelector('#overlay-intro:not([hidden])');
  await page.click('#intro-skip');
  await waitFor(`g.mode === 'play'`);
  // let the game draw a few frames so the current room is known
  await waitFor(`g.world.current !== null`, 30000).catch(() => problems.push('no current room after starting a new game'));
  const fresh = await ev(`return { step: d.step(), flags: Object.keys(d.state.flags).length, room: d.info().room }`);
  console.log('fresh ', JSON.stringify(fresh));
  if (fresh.step !== 'wake' || fresh.flags !== 0 || fresh.room !== 'cryo') problems.push(`new game not clean: ${JSON.stringify(fresh)}`);
  await browser.close();
  console.log('console errors:', errors.length);
  for (const e of [...new Set(errors)].slice(0, 10)) console.log('  ', e.slice(0, 500));
  console.log('problems:', problems.length);
  for (const p of problems) console.log('  ', p);
  if (errors.length || problems.length) process.exit(1);
  console.log('SAVE OK');
})().catch((e) => {
  console.error('TEST CRASHED:', e.message.split('\n')[0]);
  process.exit(2);
});
