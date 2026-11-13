// Touch-screen interaction test: drives the game the way a phone player would
// (real touch events through the Chrome DevTools protocol) and checks the results.
//   node tests/mobile_test.cjs <url> <outdir>
const { chromium } = require('playwright');
const fs = require('fs');

(async () => {
  const url = process.argv[2], out = process.argv[3] || '.';
  fs.mkdirSync(out, { recursive: true });
  const W = 844, H = 390;
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const ctx = await browser.newContext({ viewport: { width: W, height: H }, deviceScaleFactor: 1, hasTouch: true, isMobile: true });
  const page = await ctx.newPage();
  const errors = [];
  page.on('console', m => { if (m.type() === 'error' && !/cancel a touchstart event with cancelable=false/.test(m.text())) errors.push(m.text()); });
  page.on('pageerror', e => errors.push('pageerror: ' + e.message));
  const cdp = await ctx.newCDPSession(page);
  let failures = 0;
  const check = (cond, msg) => { console.log((cond ? '  ok   ' : '  FAIL ') + msg); if (!cond) failures++; };
  const wait = (ms) => page.waitForTimeout(ms);
  // wait until the game has simulated at least n more frames (software rendering is slow and uneven)
  const settle = async (n = 4) => {
    const f0 = await page.evaluate(() => Module._app_query(23));
    for (let i = 0; i < 200; i++) { await page.waitForTimeout(30); if ((await page.evaluate(() => Module._app_query(23))) >= f0 + n) return; }
  };
  const q = (i) => page.evaluate((i) => Module._app_query(i), i);
  const dbg = (c, a = 0, b = 0, cc = 0, d = 0) => page.evaluate(([c, a, b, cc, d]) => Module._app_debug(c, a, b, cc, d), [c, a, b, cc, d]);
  const touch = (type, pts) => cdp.send('Input.dispatchTouchEvent', { type, touchPoints: pts.map((p, i) => ({ x: p[0], y: p[1], id: p[2] !== undefined ? p[2] : i })) });
  const tap = async (x, y) => { await touch('touchStart', [[x, y, 5]]); await wait(60); await touch('touchEnd', []); await settle(); };
  const drag = async (x0, y0, x1, y1, ms, id = 7) => {
    await touch('touchStart', [[x0, y0, id]]);
    const steps = Math.max(2, Math.floor(ms / 33));
    for (let i = 1; i <= steps; i++) { await touch('touchMove', [[x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps, id]]); await wait(33); }
    return async () => { await touch('touchEnd', []); await wait(80); };
  };
  const shot = (n) => page.screenshot({ path: out + '/' + n + '.png' });

  await page.goto(url);
  for (let i = 0; i < 400; i++) { await wait(250); if (await page.evaluate(() => typeof Module !== 'undefined' && Module._app_state && Module._app_state() >= 0)) break; }
  check(await q(14) === 1, 'touch screen detected (pointer: coarse)');
  // the UI layout unit on this screen
  const U = Math.min(Math.max(Math.min(W, H) / 400, 0.75), 1.45);
  await tap(W / 2, H / 2);           // TAP TO BEGIN
  await wait(300);
  await shot('m00-title');
  // NEW SHIFT is the first menu button (no save yet)
  // mirrors draw_title() for short landscape screens
  const sz = Math.min(W * 0.085, 66 * U, H * 0.085), logoY = H * 0.08;
  const by = logoY + sz * 1.62 + 22 * U;
  const bh = Math.min(Math.max((H - by - 26 * U) / 4 - 7 * U, 30 * U), 46 * U);
  await tap(W / 2, by + bh / 2);
  await wait(300);
  await shot('m01-difficulty');
  check(await q(13) === 0, 'NEW SHIFT opens the shift choice (still on the title)');
  // STORY is the first of three columns
  const colW = Math.min((W - 48 * U) / 3, 250 * U), cbh = Math.min(42 * U, bh);
  await tap(W / 2 - colW * 1.5 - 8 * U + colW / 2, by + 18 * U + cbh / 2);
  await wait(500);
  let mode = await q(13);
  check(mode === 1, 'tapping STORY starts the shift');
  if (mode !== 1) await dbg(1, 0);
  await wait(3000);
  check(await q(13) === 1, 'a new shift has started');
  await shot('m02-hud');

  // walk forward with the left thumb
  const z0 = await q(2), x0 = await q(0);
  const release = await drag(W * 0.2, H * 0.72, W * 0.2, H * 0.72 - 60, 400);
  await wait(1500);
  await release();
  const moved = Math.hypot((await q(0)) - x0, (await q(2)) - z0);
  check(moved > 1.0, 'left-thumb joystick walks the player (' + moved.toFixed(2) + ' m)');

  // look with the right thumb
  const yaw0 = await q(3);
  const rel2 = await drag(W * 0.65, H * 0.5, W * 0.65 + 120, H * 0.5, 300, 8);
  await rel2();
  const dyaw = Math.abs((await q(3)) - yaw0);
  check(dyaw > 0.2, 'right-thumb drag turns the view (' + dyaw.toFixed(2) + ' rad)');

  // pick up the flashlight with the USE button
  await dbg(2, -4.6, 0, 66.6, 0.73);
  await dbg(3, -0.37);
  await wait(500);
  const prompt = await q(18);
  check(prompt === 0, 'the flashlight on the hood is targeted (prompt ' + prompt + ')');
  await tap(W - 76 * U, H - 196 * U);   // USE
  await wait(400);
  check(await q(8) === 1, 'USE button picks up the flashlight');
  check(await q(7) === 1, 'flashlight switches on when taken');
  await tap(W - 176 * U, H - 154 * U);  // LAMP
  await wait(200);
  check(await q(7) === 0, 'LAMP button switches it off');
  await tap(W - 176 * U, H - 154 * U);
  await wait(200);
  check(await q(7) === 1, 'LAMP button switches it back on');
  await tap(W - 168 * U, H - 64 * U);   // LOW
  await wait(300);
  check(await q(16) === 1, 'LOW button crouches');
  await tap(W - 168 * U, H - 64 * U);
  await wait(300);

  // tap-to-use: tap directly on the radio handset in the booth
  await dbg(2, 7.4, 0.12, 59.5, Math.PI * 0.5);
  await dbg(3, -0.5);
  await wait(400);
  await shot('m03-booth');

  // journal
  await tap(W - 90 * U, 34 * U);
  await wait(400);
  check(await q(5) === 2, 'LOG button opens the journal');
  await shot('m04-journal');
  // MAP tab (second tab)
  const pw = Math.min(W - 16 * U, 760 * U), ph = Math.min(H - 16 * U, 620 * U);
  const px = (W - pw) / 2, py = (H - ph) / 2, tw = (pw - 24 * U) / 5;
  await tap(px + 12 * U + tw * 1.5, py + 10 * U + 19 * U);
  await wait(300);
  await shot('m05-map');
  await tap(px + pw / 2, py + ph - 52 * U + 20 * U); // CLOSE
  await wait(400);
  check(await q(5) === 0, 'CLOSE leaves the journal');

  // pause and resume
  await tap(W - 34 * U, 34 * U);
  await wait(300);
  check(await q(5) === 4, 'pause button pauses');
  await shot('m06-pause');
  const bw = Math.min(W - 40 * U, 320 * U), bh2 = 46 * U, gap = 10 * U;
  const pby = Math.max(20 * U, H * 0.5 - (bh2 + gap) * 3.1) + 40 * U;
  await tap(W / 2, pby + bh2 / 2);
  await wait(300);
  check(await q(5) === 0, 'RESUME returns to the game');

  // keypad with on-screen buttons
  await dbg(12, 1);
  await dbg(2, 15.55, 0.6, 32.8, Math.PI * 0.5);
  await dbg(3, 0.05);
  await wait(400);
  await tap(W - 76 * U, H - 196 * U);
  await wait(400);
  check(await q(5) === 3, 'USE on the key cabinet opens the keypad');
  await shot('m07-keypad');
  const kw = Math.min(W - 30 * U, 300 * U), kbh = Math.min(58 * U, (H - 170 * U) / 4.5);
  const kh = 110 * U + kbh * 4 + 70 * U;
  const kx = (W - kw) / 2, ky = Math.max(8, (H - kh) / 2);
  const kbw = (kw - 40 * U - 16 * U) / 3;
  await tap(kx + 20 * U + kbw / 2, ky + 104 * U + kbh / 2);                       // "1"
  await tap(kx + 20 * U + (kbw + 8 * U) + kbw / 2, ky + 104 * U + kbh / 2);       // "2"
  await wait(200);
  check(await q(11) === 2, 'keypad buttons type digits (' + (await q(11)) + ')');
  await tap(W / 2, ky + kh - 52 * U + 20 * U); // STEP AWAY
  await wait(300);
  check(await q(5) === 0, 'STEP AWAY closes the keypad');

  // crane levers by touch
  await dbg(8, 0, -0.3);
  await dbg(9);
  await wait(400);
  await shot('m08-levers');
  const h0 = await q(9);
  const hx = W - 112 * U + 29 * U, hy = H - 250 * U + 100 * U;
  const rel3 = await drag(hx, hy, hx, hy - 80 * U, 300, 9);
  await wait(1500);
  await shot('m09-hoisting');
  await rel3();
  const h1 = await q(9);
  check(h1 < h0 - 0.5, 'HOIST lever raises the load (cable ' + h0.toFixed(2) + ' -> ' + h1.toFixed(2) + ')');
  await tap(W - 150 * U, 34 * U); // X: let go of the levers
  await wait(300);
  check(await q(6) === 3, 'X lets go of the levers (seated again)');

  // assists from the pause menu on a phone: ASSIST is the third button
  await tap(W - 34 * U, 34 * U);
  await wait(200);
  {
    const bw3 = Math.min(W - 40 * U, 320 * U), bh3 = 46 * U, gap3 = 10 * U;
    const by3 = Math.max(20 * U, H * 0.5 - (bh3 + gap3) * 3.1) + 40 * U;
    await tap(W / 2, by3 + 2 * (bh3 + gap3) + bh3 / 2);
    await wait(200);
    await shot('m10-assist-page');
    const pw3 = Math.min(W - 24 * U, 560 * U), rh3 = Math.min(54 * U, (H - 140 * U) / 4.6), ph3 = rh3 * 4 + 130 * U;
    const px3 = W / 2 - pw3 / 2, py3 = Math.max(8, (H - ph3) / 2);
    for (let i = 0; i < 4; i++) await tap(px3 + pw3 - 40 * U, py3 + 46 * U + i * rh3 + rh3 * 0.31);
    check((await q(24)) === 15, 'all four assists switched on from the ASSIST page (' + (await q(24)) + ')');
    await tap(W / 2, py3 + ph3 - 54 * U + 21 * U);   // DONE
    await tap(W / 2, by3 + bh3 / 2);                 // RESUME
    check(await q(5) === 0, 'back in the game');
    await dbg(2, 24.5, 0.012, -3.0, -1.57); await dbg(3, 0.0); await dbg(13, 6.0, -3.0, 1); await dbg(15, 0);
    await settle(10);
    await shot('m11-assist-on');
  }
  console.log('errors: ' + errors.length);
  for (const e of errors.slice(0, 10)) console.log('  ' + e);
  console.log(failures === 0 && errors.length === 0 ? 'MOBILE TEST PASSED' : 'MOBILE TEST FAILED (' + failures + ' checks)');
  await browser.close();
  process.exit(failures === 0 && errors.length === 0 ? 0 : 1);
})();
