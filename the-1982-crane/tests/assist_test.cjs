// Assist features: full bright, monster / mission / item ESP. Toggles them with the number
// row and from the pause menu, checks the state, and takes screenshots for review.
//   node tests/assist_test.cjs <url> <outdir>
const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const url = process.argv[2], out = process.argv[3] || '.';
  fs.mkdirSync(out, { recursive: true });
  const W = 1280, H = 720;
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const ctx = await browser.newContext({ viewport: { width: W, height: H }, deviceScaleFactor: 1 });
  const page = await ctx.newPage();
  await page.addInitScript(() => { try { if (!localStorage.getItem('crane1982.settings')) localStorage.setItem('crane1982.settings', 'scale=0.6;q=1'); } catch (e) {} });
  const errors = [];
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
  page.on('pageerror', e => errors.push('pageerror: ' + e.message));
  let failures = 0;
  const check = (c, m) => { console.log((c ? '  ok   ' : '  FAIL ') + m); if (!c) failures++; };
  const wait = (ms) => page.waitForTimeout(ms);
  const q = (i) => page.evaluate((i) => Module._app_query(i), i);
  const dbg = (c, a = 0, b = 0, cc = 0, d = 0) => page.evaluate(([c, a, b, cc, d]) => Module._app_debug(c, a, b, cc, d), [c, a, b, cc, d]);
  const settle = async (n = 4) => { const f0 = await q(23); for (let i = 0; i < 300; i++) { await wait(30); if ((await q(23)) >= f0 + n) return; } };
  const press = async (k) => { await page.keyboard.down(k); await wait(60); await page.keyboard.up(k); await settle(); };
  const shot = (n) => page.screenshot({ path: out + '/' + n + '.png' });
  await page.goto(url);
  for (let i = 0; i < 400; i++) { await wait(250); if (await page.evaluate(() => typeof Module !== 'undefined' && Module._app_state && Module._app_state() >= 0)) break; }
  await page.mouse.click(W / 2, H / 2); await settle();
  await dbg(1, 1); await wait(3000); await settle();
  check(await q(24) === 0, 'assists start off');
  // inside the crew room, the Rigger patrolling in the yard beyond the wall
  await dbg(4, 1);
  await dbg(12, 1);
  await dbg(16, 6);                         // stage: get the generator running
  await dbg(2, 24.5, 0.012, -3.0, -1.57);  // facing west, toward the crew room door
  await dbg(3, 0.0);
  await dbg(13, 6.0, -3.0, 1);              // patrolling on the road outside
  await dbg(15, 0);
  await settle(8);
  await shot('a00-off');
  await press('Digit2');
  check(((await q(24)) & 2) === 2, 'key 2 turns monster ESP on');
  await settle(10);
  await shot('a01-monster');
  await press('Digit3');
  check(((await q(24)) & 4) === 4, 'key 3 turns mission ESP on');
  await press('Digit4');
  check(((await q(24)) & 8) === 8, 'key 4 turns item ESP on');
  await settle(6);
  await shot('a02-all-esp');
  await press('Digit1');
  check(((await q(24)) & 1) === 1, 'key 1 turns full bright on');
  await settle(6);
  await shot('a03-fullbright');
  // out in the stacks at night: full bright vs normal
  await dbg(2, -36, 0, 30.7, -1.6); await dbg(3, 0.05); await settle(8);
  await shot('a04-stacks-bright');
  await press('Digit1');
  check(((await q(24)) & 1) === 0, 'key 1 turns full bright off again');
  await settle(6);
  await shot('a05-stacks-normal');
  // the Rigger hunting, seen through a container from the quay side
  await dbg(2, -36, 0, 13.7, 0.0); await dbg(3, 0.0); await dbg(13, -36, 25, 3); await dbg(15, 0);
  await settle(10);
  await shot('a06-hunting');
  await dbg(13, -60, 40, 1); await dbg(15, 1000);
  // pause menu ASSIST page
  await press('Escape');
  check(await q(5) === 4, 'pause opens');
  await shot('a07-pause');
  const U = Math.min(Math.max(Math.min(W, H) / 400, 0.75), 1.45);
  const bw = Math.min(W - 40 * U, 320 * U), bh = 46 * U, gap = 10 * U;
  let by = Math.max(20 * U, H * 0.5 - (bh + gap) * 3.1) + 40 * U;
  await page.mouse.click(W / 2, by + 2 * (bh + gap) + bh / 2); await settle();   // RESUME, JOURNAL, ASSIST
  await shot('a08-assist-page');
  // toggle Item ESP off from the page (4th row)
  const pw = Math.min(W - 24 * U, 560 * U);
  const rh = Math.min(54 * U, (H - 140 * U) / 4.6), ph = rh * 4 + 130 * U;
  const px = W / 2 - pw / 2, py = Math.max(8, (H - ph) / 2);
  await page.mouse.click(px + pw - 40 * U, py + 46 * U + 3 * rh + rh * 0.31); await settle();
  check(((await q(24)) & 8) === 0, 'the ASSIST page toggles item ESP off');
  await shot('a09-assist-toggled');
  // settings survive a reload
  await page.reload();
  for (let i = 0; i < 400; i++) { await wait(250); if (await page.evaluate(() => typeof Module !== 'undefined' && Module._app_state && Module._app_state() >= 0)) break; }
  await wait(1500);
  check(await q(24) === (2 | 4), 'assist choices are remembered after a reload (' + (await q(24)) + ')');
  console.log('errors: ' + errors.length);
  for (const e of errors.slice(0, 10)) console.log('  ' + e);
  console.log(failures === 0 && errors.length === 0 ? 'ASSIST TEST PASSED' : 'ASSIST TEST FAILED (' + failures + ')');
  await browser.close();
  process.exit(failures === 0 && errors.length === 0 ? 0 : 1);
})();
