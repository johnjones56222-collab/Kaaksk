// Real-input check: drives the game with genuine mouse, keyboard and touch
// events (no debug shortcuts for the actions under test).
const path = require('path');
const { chromium, devices } = require('playwright');

const ARGS = ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'];
const URL = 'file://' + path.resolve(__dirname, '../public/index.html') + '?q=low';
const LIT = ['podOpen', 'lockerOpen', 'hasTool', 'cryoPower', 'reachedHub', 'vegaOnline', 'fireStarted', 'reactorOnline', 'firesDone', 'airlockFuse', 'hasSuit', 'evaStarted', 'stormWarning', 'evaDone', 'reachedBridge'];

async function session(browser, opts) {
  const ctx = await browser.newContext(opts);
  const page = await ctx.newPage();
  const errors = [];
  page.on('console', (m) => m.type() === 'error' && errors.push(m.text()));
  page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}`));
  await page.goto(URL);
  await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 180000 });
  const ev = (code) => page.evaluate(`(async () => { const g = window.__game; const d = g.debug; ${code} })()`);
  const waitFor = (code, timeout = 30000) => page.waitForFunction(`(() => { const g = window.__game; const d = g && g.debug; return ${code}; })()`, null, { timeout, polling: 100 });
  const frames = async (n = 3) => {
    const f = await page.evaluate(() => window.__game.frame || 0);
    await page.waitForFunction((t) => (window.__game.frame || 0) >= t, f + n, { timeout: 30000, polling: 50 });
  };
  return { ctx, page, errors, ev, waitFor, frames };
}

(async () => {
  const browser = await chromium.launch({ args: ARGS });
  const problems = [];
  const note = (m) => {
    problems.push(m);
    console.log('  PROBLEM:', m);
  };
  let allErrors = [];

  // ================= desktop: mouse + keyboard =================
  {
    const { ctx, page, errors, ev, waitFor, frames } = await session(browser, { viewport: { width: 900, height: 506 } });
    await ev(`d.quickStart(${JSON.stringify(LIT)}); d.teleport('hub', 0); g.player.place(0, -9, 0, 0);`);
    await frames(4);
    // 1. clicking the view captures the pointer (or falls back to drag-look)
    await page.mouse.move(450, 250);
    await page.mouse.down();
    await page.waitForTimeout(250);
    await page.mouse.up();
    await page.waitForTimeout(300);
    const locked = await ev('return g.input.locked');
    console.log('pointer lock after clicking the view:', locked);
    if (locked) {
      // 2. locked mouse motion feeds the look controls. (Headless Chromium
      // reports +x then -x for scripted moves while locked, so send well-formed
      // locked motion events and read what the game accumulated.)
      const { look, yawA } = await page.evaluate(() => {
        const g = window.__game;
        const c = document.getElementById('game');
        const before = g.input.lookX;
        const yawA = g.player.yaw;
        for (let i = 0; i < 10; i++) c.dispatchEvent(new MouseEvent('mousemove', { movementX: 14, movementY: 3, bubbles: true }));
        return { look: g.input.lookX - before, yawA };
      });
      if (look !== 140) note(`locked mouse motion accumulated ${look}px of look, expected 140`);
      await frames(3);
      const yawB = await ev('return g.player.yaw');
      if (!(yawB < yawA - 0.2)) note(`locked mouse-look did not turn the view right (${yawA.toFixed(3)} -> ${yawB.toFixed(3)})`);
      await page.evaluate(() => document.exitPointerLock());
      await waitFor(`g.mode === 'paused'`, 10000).catch(() => note('losing the pointer lock did not pause'));
      await page.click('#overlay-pause .btn.primary');
      await waitFor(`g.mode === 'play'`);
      await page.evaluate(() => document.exitPointerLock && document.exitPointerLock());
      await page.waitForTimeout(300);
      if ((await ev('return g.mode')) === 'paused') {
        // resume re-locked and we just unlocked again: close the pad without locking
        await ev(`g.input.lockFailed = true; g.resume();`);
      }
    }
    // 3. drag-to-look where pointer lock is unavailable
    await ev(`g.input.lockFailed = true; if (document.pointerLockElement) document.exitPointerLock();`);
    await page.waitForTimeout(300);
    if ((await ev('return g.mode')) === 'paused') await ev('g.resume()');
    await frames(3);
    const a0 = await ev('return [g.player.yaw, g.player.pitch]');
    await page.mouse.move(400, 250);
    await page.mouse.down();
    for (let i = 1; i <= 10; i++) await page.mouse.move(400 + i * 14, 250 + i * 4);
    await page.mouse.up();
    await frames(4);
    const a1 = await ev('return [g.player.yaw, g.player.pitch]');
    if (Math.abs(a1[0] - a0[0]) < 0.1) note(`drag-look did not turn the view (${a0[0].toFixed(3)} -> ${a1[0].toFixed(3)})`);
    if (Math.abs(a1[1] - a0[1]) < 0.02) note('drag-look did not pitch the view');
    // 4. WASD walks, Shift runs
    await ev(`g.player.place(0, -9, Math.PI, 0)`);
    await frames(2);
    const p0 = await ev('return [g.player.x, g.player.z]');
    await page.keyboard.down('KeyW');
    await frames(12);
    await page.keyboard.up('KeyW');
    await frames(2);
    const p1 = await ev('return [g.player.x, g.player.z]');
    if (p1[1] - p0[1] < 0.5) note(`W did not walk forward (${p0} -> ${p1})`);
    await page.keyboard.down('KeyD');
    await frames(10);
    await page.keyboard.up('KeyD');
    await frames(2);
    const p2 = await ev('return [g.player.x, g.player.z]');
    if (Math.abs(p2[0] - p1[0]) < 0.3) note('D did not strafe');
    const stopped = await ev('return [g.input.moveX, g.input.moveY]');
    if (stopped[0] || stopped[1]) note('keys released but still moving');
    // 5. Space jumps, F toggles the lamp
    await page.keyboard.press('Space');
    await frames(2);
    if (await ev('return g.player.grounded')) note('Space did not jump');
    const lamp0 = await ev('return g.player.lampOn');
    await page.keyboard.press('KeyF');
    await frames(2);
    if ((await ev('return g.player.lampOn')) === lamp0) note('F did not toggle the flashlight');
    // 6. E uses the focused object
    await ev(`d.approach('holo', 2.2)`);
    await frames(4);
    if ((await ev('return d.focus()')) !== 'holo') note(`expected to focus the hub projector, got ${await ev('return d.focus()')}`);
    const t0 = await ev('return d.state.transcript.length');
    await page.keyboard.press('KeyE');
    await frames(6);
    // 7. Tab / M / Escape and the datapad
    await page.keyboard.press('Tab');
    await waitFor(`g.mode === 'paused'`, 10000).catch(() => note('Tab did not open the datapad'));
    await page.keyboard.press('Escape');
    await waitFor(`g.mode === 'play'`, 10000).catch(() => note('Escape did not close the datapad'));
    await page.keyboard.press('KeyM');
    await waitFor(`g.mode === 'paused'`, 10000).catch(() => note('M did not open the map'));
    const mapTab = await page.textContent('#overlay-pause .tab.on').catch(() => '');
    if (mapTab !== 'Map') note(`M opened "${mapTab}" instead of the map`);
    await page.keyboard.press('Escape');
    await waitFor(`g.mode === 'play'`, 10000);
    await page.waitForTimeout(600);
    const t1 = await ev('return d.state.transcript.length');
    if (t1 <= t0) note('E on the hub projector produced no status line');
    // 8. gunnery with a real mouse (no pointer lock): move to aim, hold to fire
    await ev(`g.player.place(0, -40, 0, 0);`);
    await frames(3);
    await ev(`g.startTurret()`);
    await waitFor(`g.mode === 'turret'`);
    await frames(3);
    await page.mouse.move(650, 150);
    await frames(3);
    const aim = await ev('return [g.input.aim.x, g.input.aim.y]');
    if (Math.abs(aim[0] - (650 / 450 - 1)) > 0.08) note(`turret aim did not follow the mouse: ${aim}`);
    await page.mouse.down();
    await frames(8);
    const heat = await ev('return g.turret.heat');
    await page.mouse.up();
    if (!(heat > 0.05)) note(`holding the mouse did not fire the turret (heat ${heat})`);
    await frames(3);
    const heatAfter = await ev('return g.turret.heat');
    await frames(6);
    if ((await ev('return g.turret.heat')) > heatAfter + 0.01) note('turret kept firing after the button was released');
    // spawn a rock dead ahead and shoot it for real
    await ev(`const t = g.turret; const r = t.rocks[0]; t._spawn({ speed: [1, 1] }); r.mesh.position.set(t.cam.x, t.cam.y + 30, t.cam.z - 200); r.vel.set(0, 0, 0); r.hp = 1; r.r = 6; r.mesh.scale.setScalar(6); t.spawnT = 99; t.yaw = 0; g.input.aim.x = 0; g.input.aim.y = 0;`);
    await frames(2);
    // find where that rock is on screen and click on it
    const scr = await ev(`const v = g.turret.rocks[0].mesh.position.clone().project(g.gfx.camera); return [(v.x * 0.5 + 0.5) * innerWidth, (-v.y * 0.5 + 0.5) * innerHeight];`);
    const k0 = await ev('return g.turret.kills');
    await page.mouse.move(scr[0], scr[1]);
    await frames(4);
    const scr2 = await ev(`const v = g.turret.rocks[0].mesh.position.clone().project(g.gfx.camera); return [(v.x * 0.5 + 0.5) * innerWidth, (-v.y * 0.5 + 0.5) * innerHeight];`);
    await page.mouse.move(scr2[0], scr2[1]);
    await frames(3);
    await page.mouse.down();
    await frames(8);
    await page.mouse.up();
    const k1 = await ev('return g.turret.kills');
    if (k1 <= k0) note('shooting a rock under the reticle did not destroy it');
    await ev('g.turret.forceWin()');
    await waitFor(`g.mode === 'play'`, 20000).catch(() => note('turret did not hand control back'));
    if (!(await ev(`return d.has('debrisCleared')`))) note('debrisCleared not set after gunnery');
    console.log('desktop checks done');
    allErrors = allErrors.concat(errors);
    await ctx.close();
  }

  // ================= phone: touch =================
  {
    const d = devices['Pixel 7'];
    const land = { width: d.viewport.height, height: d.viewport.width };
    const { ctx, page, errors, ev, waitFor, frames } = await session(browser, { ...d, viewport: land, deviceScaleFactor: 1 });
    const touch = (type, id, x, y) =>
      page.evaluate(([type, id, x, y]) => document.getElementById('touch-layer').dispatchEvent(new PointerEvent(type, { pointerId: id, pointerType: 'touch', clientX: x, clientY: y, bubbles: true, cancelable: true })), [type, id, x, y]);
    await ev(`d.quickStart(${JSON.stringify(LIT)}); g.player.place(0, -40, 0, 0);`);
    await frames(4);
    // hold the USE button to work: patch a service panel
    await ev(`d.approach('panel2')`);
    await frames(4);
    if ((await ev('return d.focus()')) !== 'panel2') note(`touch: expected focus panel2, got ${await ev('return d.focus()')}`);
    const box = await page.locator('#btn-interact').boundingBox();
    await page.evaluate(([x, y]) => document.getElementById('btn-interact').dispatchEvent(new PointerEvent('pointerdown', { pointerId: 31, pointerType: 'touch', clientX: x, clientY: y, bubbles: true, cancelable: true })), [box.x + 40, box.y + 40]);
    await waitFor(`d.has('panel2')`, 40000).catch(() => note('touch: holding USE did not patch the panel'));
    await page.evaluate(([x, y]) => document.getElementById('btn-interact').dispatchEvent(new PointerEvent('pointerup', { pointerId: 31, pointerType: 'touch', clientX: x, clientY: y, bubbles: true, cancelable: true })), [box.x + 40, box.y + 40]);
    if (await ev('return g.input.held.interact')) note('touch: USE still held after release');
    // jump and lamp buttons
    await page.tap('#btn-jump');
    await frames(2);
    if (await ev('return g.player.grounded')) note('touch: jump button did not jump');
    const l0 = await ev('return g.player.lampOn');
    await page.tap('#btn-flash');
    await frames(2);
    if ((await ev('return g.player.lampOn')) === l0) note('touch: lamp button did not toggle');
    await page.tap('#btn-map');
    await waitFor(`g.mode === 'paused'`, 10000).catch(() => note('touch: map button did not open the datapad'));
    await page.tap('#overlay-pause .btn.primary');
    await waitFor(`g.mode === 'play'`);
    // gunnery by touch: touching a target aims and fires
    await ev(`g.player.place(0, -40, 0, 0)`);
    await frames(3);
    await ev('g.startTurret()');
    await waitFor(`g.mode === 'turret'`);
    await frames(3);
    await touch('pointerdown', 21, land.width * 0.3, land.height * 0.35);
    await frames(8);
    const aim = await ev('return [g.input.aim.x, g.input.aim.y]');
    const heat = await ev('return g.turret.heat');
    if (Math.abs(aim[0] - -0.4) > 0.08) note(`touch: turret aim did not follow the finger: ${aim}`);
    if (!(heat > 0.05)) note(`touch: touching did not fire (heat ${heat})`);
    await touch('pointermove', 21, land.width * 0.6, land.height * 0.5);
    await frames(3);
    const aim2 = await ev('return g.input.aim.x');
    if (Math.abs(aim2 - 0.2) > 0.08) note(`touch: aim did not track the drag: ${aim2}`);
    await touch('pointerup', 21, land.width * 0.6, land.height * 0.5);
    await frames(2);
    if (await ev('return g.input.held.fire')) note('touch: still firing after lifting the finger');
    if (await page.isVisible('#stick-base')) note('touch: move stick shown during gunnery');
    await page.screenshot({ path: path.resolve(__dirname, 'out/controls-turret-touch.png') });
    await ev('g.turret.forceWin()');
    await waitFor(`g.mode === 'play'`, 20000);
    await frames(3);
    // back on foot the stick works again
    const q0 = await ev('return [g.player.x, g.player.z]');
    await touch('pointerdown', 22, land.width * 0.2, land.height * 0.6);
    await touch('pointermove', 22, land.width * 0.2, land.height * 0.6 - 56);
    await frames(12);
    await touch('pointerup', 22, land.width * 0.2, land.height * 0.6 - 56);
    const q1 = await ev('return [g.player.x, g.player.z]');
    if (Math.hypot(q1[0] - q0[0], q1[1] - q0[1]) < 0.4) note('touch: stick did not move the player after gunnery');
    console.log('touch checks done');
    allErrors = allErrors.concat(errors);
    await ctx.close();
  }

  await browser.close();
  console.log('console errors:', allErrors.length);
  for (const e of [...new Set(allErrors)].slice(0, 10)) console.log('  ', e.slice(0, 400));
  console.log('problems:', problems.length);
  if (allErrors.length || problems.length) process.exit(1);
  console.log('CONTROLS OK');
})().catch((e) => {
  console.error('TEST CRASHED:', e.message.split('\n').slice(0, 3).join(' | '));
  process.exit(2);
});
