// Dev helper: open the built page headlessly, run snippets, save screenshots.
//   node tests/shot.cjs [--mobile] [--portrait] [--wait 500] out1.png "code1" out2.png "code2" ...
// Each snippet runs with `g` bound to window.__game.
const path = require('path');
const { chromium, devices } = require('playwright');

(async () => {
  const raw = process.argv.slice(2);
  const flags = { wait: 500 };
  const pairs = [];
  for (let i = 0; i < raw.length; i++) {
    if (raw[i] === '--mobile') flags.mobile = true;
    else if (raw[i] === '--portrait') flags.portrait = true;
    else if (raw[i] === '--wait') flags.wait = Number(raw[++i]);
    else if (raw[i] === '--size') flags.size = raw[++i].split('x').map(Number);
    else if (raw[i] === '--query') flags.query = raw[++i];
    else pairs.push(raw[i]);
  }
  const browser = await chromium.launch({
    args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'],
  });
  let ctxOpts = { viewport: { width: flags.size ? flags.size[0] : 1280, height: flags.size ? flags.size[1] : 720 }, deviceScaleFactor: 1 };
  if (flags.mobile) {
    const d = devices['Pixel 7'];
    ctxOpts = { ...d, viewport: flags.portrait ? d.viewport : { width: d.viewport.height, height: d.viewport.width }, deviceScaleFactor: 1 };
  }
  const ctx = await browser.newContext(ctxOpts);
  const page = await ctx.newPage();
  const errors = [];
  page.on('console', (m) => {
    if (m.type() === 'error' || m.type() === 'warning') errors.push(`[${m.type()}] ${m.text()}`);
  });
  page.on('pageerror', (e) => errors.push(`[pageerror] ${e.message}\n${e.stack || ''}`));
  await page.goto('file://' + path.resolve(__dirname, '../public/index.html') + (flags.query ? '?' + flags.query : ''));
  try {
    await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 120000 });
  } catch (e) {
    errors.push('timeout waiting for ready');
  }
  const err = await page.evaluate(() => window.__error || null);
  if (err) errors.push('BOOT ERROR: ' + err);
  for (let i = 0; i < pairs.length; i += 2) {
    const out = pairs[i];
    const code = pairs[i + 1] || '';
    if (code) {
      try {
        const r = await page.evaluate(`(async () => { const g = window.__game; ${code} })()`);
        if (r !== undefined) console.log(`${out}:`, JSON.stringify(r));
      } catch (e) {
        errors.push(`eval error (${out}): ` + e.message);
      }
    }
    await page.waitForTimeout(flags.wait);
    if (out !== '-') await page.screenshot({ path: out });
  }
  if (errors.length) console.log([...new Set(errors)].slice(0, 30).join('\n'));
  else console.log('no console errors');
  await browser.close();
})();
