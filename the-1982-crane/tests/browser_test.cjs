// Browser smoke test: loads the web build in headless Chromium (SwiftShader WebGL 2),
// waits for loading, checks for errors, and takes screenshots of scripted views.
//   node tests/browser_test.cjs <url> <outdir> [scenario]
const { chromium } = require('playwright');
const fs = require('fs');
(async () => {
  const url = process.argv[2], out = process.argv[3] || '.', scenario = process.argv[4] || 'basic';
  fs.mkdirSync(out, { recursive: true });
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', '--autoplay-policy=no-user-gesture-required'] });
  const mobile = scenario.startsWith('mobile');
  const ctx = await browser.newContext(mobile ? { viewport: { width: 844, height: 390 }, deviceScaleFactor: 1, hasTouch: true, isMobile: true } : { viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
  const page = await ctx.newPage();
  const logs = [], errors = [];
  page.on('console', m => { const t = m.type() + ': ' + m.text(); logs.push(t); if (m.type() === 'error') errors.push(t); });
  page.on('pageerror', e => errors.push('pageerror: ' + e.message));
  const t0 = Date.now();
  await page.goto(url);
  let state = -1;
  for (let i = 0; i < 600 && state < 0; i++) {
    await page.waitForTimeout(250);
    state = await page.evaluate(() => (typeof Module !== 'undefined' && Module._app_state) ? Module._app_state() : -1);
  }
  console.log('loaded: state=' + state + ' in ' + ((Date.now() - t0) / 1000).toFixed(1) + 's');
  const shot = async (name) => { await page.screenshot({ path: out + '/' + name + '.png' }); };
  const dbg = (c, a = 0, b = 0, cc = 0, d = 0) => page.evaluate(([c, a, b, cc, d]) => Module._app_debug(c, a, b, cc, d), [c, a, b, cc, d]);
  const wait = (ms) => page.waitForTimeout(ms);
  await wait(1500);
  await shot('00-gate');
  await page.mouse.click(640, 360);
  await wait(800);
  await shot('01-title');
  if (scenario === 'basic' || scenario === 'mobile') {
    await dbg(1, 1);
    await wait(3500);
    await shot('02-start');
    const views = JSON.parse(fs.readFileSync(__dirname + '/views.json', 'utf8'));
    for (const v of views) {
      if (v.save) await page.evaluate((s) => { const p = Module._malloc(s.length + 1); Module.stringToUTF8 ? 0 : 0; for (let i = 0; i < s.length; i++) HEAPU8[p + i] = s.charCodeAt(i); HEAPU8[p + s.length] = 0; Module._app_debug_load(p); Module._free(p); }, v.save);
      for (const c of (v.cmds || [])) await dbg(...c);
      await wait(v.wait || 900);
      await shot(v.name);
    }
  }
  const dc = await page.evaluate(() => Module._app_draw_calls());
  console.log('draw calls last frame: ' + dc);
  fs.writeFileSync(out + '/console.txt', logs.join('\n'));
  console.log('errors: ' + errors.length);
  for (const e of errors.slice(0, 20)) console.log('  ' + e);
  const warn = logs.filter(l => /SHADER|LINK|GL_INVALID|WebGL|error/i.test(l));
  for (const w of warn.slice(0, 20)) console.log('  log: ' + w);
  await browser.close();
})();
