// Keyboard + mouse interaction test for desktop browsers.
//   node tests/desktop_test.cjs <url> <outdir>
const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const url = process.argv[2], out = process.argv[3] || '.';
  fs.mkdirSync(out, { recursive: true });
  const W = 1280, H = 720;
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--autoplay-policy=no-user-gesture-required'] });
  const ctx = await browser.newContext({ viewport: { width: W, height: H }, deviceScaleFactor: 1 });
  const page = await ctx.newPage();
  // software rendering is slow: render at half resolution so the test runs at a usable frame rate
  await page.addInitScript(() => { try { localStorage.setItem('crane1982.settings', 'scale=0.5;q=1'); } catch (e) {} });
  const errors = [];
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
  page.on('pageerror', e => errors.push('pageerror: ' + e.message));
  let failures = 0;
  const check = (c, m) => { console.log((c ? '  ok   ' : '  FAIL ') + m); if (!c) failures++; };
  const wait = (ms) => page.waitForTimeout(ms);
  // wait until the game has simulated at least n more frames (software rendering is slow and uneven)
  const settle = async (n = 4) => {
    const f0 = await page.evaluate(() => Module._app_query(23));
    for (let i = 0; i < 200; i++) { await page.waitForTimeout(30); if ((await page.evaluate(() => Module._app_query(23))) >= f0 + n) return; }
  };
  const q = (i) => page.evaluate((i) => Module._app_query(i), i);
  const dbg = (c, a = 0, b = 0, cc = 0, d = 0) => page.evaluate(([c, a, b, cc, d]) => Module._app_debug(c, a, b, cc, d), [c, a, b, cc, d]);
  const press = async (k, ms = 80) => { await page.keyboard.down(k); await wait(ms); await page.keyboard.up(k); await settle(); };
  await page.goto(url);
  for (let i = 0; i < 400; i++) { await wait(250); if (await page.evaluate(() => typeof Module !== 'undefined' && Module._app_state && Module._app_state() >= 0)) break; }
  check(await q(14) === 0, 'mouse/keyboard device detected');
  await page.mouse.click(W / 2, H / 2);          // CLICK TO BEGIN
  await wait(400);
  const ctxState = await page.evaluate(() => Module.audioCtx ? Module.audioCtx.state : 'none');
  check(ctxState === 'running', 'audio context is running after the first click (' + ctxState + ')');
  await wait(1500);
  const peak = await q(19);
  check(peak > 0.005, 'the title screen is making sound (peak ' + peak.toFixed(3) + ')');
  // NEW SHIFT -> STANDARD (positions mirror draw_title for a 720px tall window)
  const U = Math.min(Math.max(Math.min(W, H) / 400, 0.75), 1.45);
  const sz = Math.min(W * 0.085, 66 * U), logoY = H * 0.13;
  const by = logoY + sz * 1.62 + 34 * U;
  const bh = Math.min(Math.max((H - by - 26 * U) / 3 - 10 * U, 30 * U), 46 * U);
  await page.mouse.click(W / 2, by + bh / 2);
  await settle();
  await page.mouse.click(W / 2, by + 22 * U + 23 * U);   // STORY (the difficulty page keeps 46U buttons)
  await settle();
  check(await q(13) === 1, 'clicking NEW SHIFT then STORY starts a shift');
  if (await q(13) !== 1) await dbg(1, 1);
  await wait(3500);
  check(await q(13) === 1, 'a shift is running');
  console.log('  (frame rate here: ' + (await q(22)).toFixed(1) + ' fps)');
  // walk forward with W (pointer lock is not available headless; movement keys do not need it)
  await dbg(2, 0, 0, 30, 0); await dbg(3, 0); await settle(4);   // open road, facing the quay
  const x0 = await q(0), z0 = await q(2), t0 = await q(21);
  await page.keyboard.down('KeyW'); await wait(1500); await page.keyboard.up('KeyW'); await settle();
  const d = Math.hypot((await q(0)) - x0, (await q(2)) - z0), gt = (await q(21)) - t0;
  check(d / gt > 1.6, 'W walks forward (' + d.toFixed(2) + ' m in ' + gt.toFixed(2) + ' s of game time)');
  await page.keyboard.down('ShiftLeft'); await page.keyboard.down('KeyW'); await wait(1000); await page.keyboard.up('KeyW'); await page.keyboard.up('ShiftLeft');
  await press('KeyC');
  check(await q(16) === 1, 'C crouches');
  await press('KeyC');
  // pick up the flashlight with E
  await dbg(2, -4.6, 0, 66.6, 0.73); await dbg(3, -0.37); await settle(6);
  await press('KeyE');
  check(await q(8) === 1, 'E picks up the flashlight');
  await press('KeyF');
  check(await q(7) === 0, 'F switches the flashlight off');
  await press('KeyF');
  // read the gate log: notes open and close
  await dbg(2, 7.6, 0.12, 58.75, Math.PI * 0.5); await dbg(3, -0.85); await settle(6);
  await press('KeyE');
  check(await q(5) === 1, 'E opens the gate log');
  await page.mouse.wheel(0, 400); await wait(200);
  await page.screenshot({ path: out + '/d01-note.png' });
  await press('KeyE');
  check(await q(5) === 0, 'E closes it again');
  check(await q(4) >= 1, 'stage advanced past the flashlight (' + await q(4) + ')');
  await press('Tab');
  check(await q(5) === 2, 'TAB opens the journal');
  await press('Digit2'); await wait(200);
  await page.screenshot({ path: out + '/d02-map.png' });
  await press('Tab');
  check(await q(5) === 0, 'TAB closes the journal');
  await press('Escape');
  check(await q(5) === 4, 'ESC pauses');
  await press('Escape');
  check(await q(5) === 0, 'ESC resumes');
  // keypad by keyboard
  await dbg(12, 1); await dbg(2, 15.55, 0.6, 32.8, Math.PI * 0.5); await dbg(3, 0.05); await settle(6);
  await press('KeyE');
  check(await q(5) === 3, 'E on the key cabinet opens the keypad');
  await press('Digit4'); await press('Digit2');
  check(await q(11) === 2, 'number keys type into the keypad (' + (await q(11)) + ' digits, modal ' + (await q(5)) + ')');
  await press('Backspace');
  check(await q(11) === 0, 'Backspace clears');
  await press('Escape');
  check(await q(5) === 0, 'ESC steps away from the keypad');
  // crane controls by keyboard
  await dbg(8, 0, -0.3); await dbg(9); await settle(6);
  const h0 = await q(9), s0 = await q(10);
  await page.keyboard.down('KeyR'); await page.keyboard.down('KeyA'); await wait(1500); await page.keyboard.up('KeyR'); await page.keyboard.up('KeyA');
  check((await q(9)) < h0 - 0.5, 'R raises the hook');
  check((await q(10)) > s0 + 0.02, 'A slews the jib');
  await press('KeyQ');
  check(await q(6) === 3, 'Q lets go of the levers');
  await page.screenshot({ path: out + '/d03-cab.png' });
  check(await q(19) > 0.005, 'game audio is playing (peak ' + (await q(19)).toFixed(3) + ')');
  console.log('errors: ' + errors.length);
  for (const e of errors.slice(0, 10)) console.log('  ' + e);
  console.log(failures === 0 && errors.length === 0 ? 'DESKTOP TEST PASSED' : 'DESKTOP TEST FAILED (' + failures + ')');
  await browser.close();
  process.exit(failures === 0 && errors.length === 0 ? 0 : 1);
})();
