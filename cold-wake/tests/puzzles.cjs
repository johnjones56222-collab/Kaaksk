// Solves the discrete console puzzles through their real controls (clicks),
// not the debug solver: breakers, memory lattice, keypad, wires, jump route.
// Also checks wrong answers are rejected.
const path = require('path');
const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await (await browser.newContext({ viewport: { width: 800, height: 450 } })).newPage();
  const errors = [];
  const problems = [];
  page.on('console', (m) => m.type() === 'error' && errors.push(m.text()));
  page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}`));
  const ev = (code) => page.evaluate(`(async () => { const g = window.__game; const d = g.debug; ${code} })()`);
  const waitFor = (code, timeout = 30000) => page.waitForFunction(`(() => { const g = window.__game; const d = g && g.debug; return ${code}; })()`, null, { timeout, polling: 100 });
  await page.goto('file://' + path.resolve(__dirname, '../public/index.html') + '?q=low');
  await page.waitForFunction(() => window.__ready || window.__error, null, { timeout: 180000 });
  await ev(`d.quickStart(['podOpen', 'lockerOpen', 'hasTool'])`);
  await page.waitForTimeout(300);
  const open = async (id) => {
    const r = await ev(`return d.use('${id}')`);
    if (r !== 'ok') problems.push(`use(${id}) -> ${r}`);
    await waitFor('d.minigameOpen()');
    await page.waitForTimeout(150);
  };
  const closed = async (flag, name) => {
    await waitFor('!d.minigameOpen()', 20000).catch(() => problems.push(`${name}: did not close after a correct solution`));
    await page.waitForTimeout(100);
    if (!(await ev(`return d.has('${flag}')`))) problems.push(`${name}: flag ${flag} not set`);
    else console.log(`${name}: solved`);
  };

  // ---- breakers: find the subset that hits the target ----
  await open('breaker');
  const amps = await page.$$eval('.breaker b', (b) => b.map((x) => Number(x.textContent)));
  const target = Number((await page.textContent('.readout')).match(/\/\s*(\d+)/)[1]);
  let pick = null;
  for (let m = 1; m < 64 && !pick; m++) {
    let s = 0;
    for (let i = 0; i < 6; i++) if (m & (1 << i)) s += amps[i];
    if (s === target) pick = m;
  }
  if (!pick) problems.push(`breakers: no subset of ${amps} sums to ${target}`);
  // first overload it on purpose
  for (let i = 0; i < 6; i++) await page.click(`.breaker:nth-child(${i + 1})`);
  if (!(await page.textContent('.panel-foot .status')).includes('OVERLOAD')) problems.push('breakers: overload not reported');
  for (let i = 0; i < 6; i++) await page.click(`.breaker:nth-child(${i + 1})`);
  for (let i = 0; i < 6; i++) if (pick & (1 << i)) await page.click(`.breaker:nth-child(${i + 1})`);
  await closed('cryoPower', 'breakers');

  // ---- memory: watch the lattice, then repeat it ----
  await open('vegaConsole');
  await page.evaluate(() => {
    window.__seq = [];
    const nodes = [...document.querySelectorAll('.node')];
    const obs = new MutationObserver((muts) => {
      for (const m of muts) {
        const i = nodes.indexOf(m.target);
        if (i >= 0 && m.target.classList.contains('lit') && !(m.oldValue || '').includes('lit')) window.__seq.push(i);
      }
    });
    nodes.forEach((n) => obs.observe(n, { attributes: true, attributeFilter: ['class'], attributeOldValue: true }));
  });
  // one deliberate mistake first
  await page.waitForFunction(() => document.querySelector('.panel-foot .status').textContent === 'REPEAT IT', null, { timeout: 15000 });
  const seq0 = await page.evaluate(() => window.__seq.slice());
  await page.click(`.node:nth-child(${((seq0[0] + 1) % 9) + 1})`);
  if (!(await page.textContent('.panel-foot .status')).includes('MISMATCH')) problems.push('memory: wrong node not rejected');
  await page.evaluate(() => (window.__seq = []));
  for (let round = 0; round < 3; round++) {
    await page.waitForFunction(() => document.querySelector('.panel-foot .status') && document.querySelector('.panel-foot .status').textContent === 'REPEAT IT', null, { timeout: 20000 });
    const seq = await page.evaluate(() => window.__seq.slice());
    await page.evaluate(() => (window.__seq = []));
    if (seq.length !== 3 + round) problems.push(`memory: round ${round + 1} showed ${seq.length} nodes`);
    for (const i of seq) {
      await page.click(`.node:nth-child(${i + 1})`);
      await page.waitForTimeout(60);
    }
    await page.waitForTimeout(300);
    if (round < 2) await page.evaluate(() => (window.__seq = []));
  }
  await closed('vegaOnline', 'memory');

  // ---- keypad: a wrong code, then the right one ----
  await open('safe');
  const code = await ev('return d.state.safeCode');
  const wrong = code === '0000' ? '1111' : '0000';
  const press = async (digits) => {
    for (const ch of digits) {
      const idx = ch === '0' ? 11 : Number(ch);
      await page.click(`.keypad button:nth-child(${idx})`);
    }
  };
  await press(wrong);
  if (!(await page.textContent('.panel-foot .status')).includes('DENIED')) problems.push('keypad: wrong code not rejected');
  await page.waitForTimeout(800);
  await press(code);
  await closed('safeOpen', 'keypad');

  // ---- wires: tap a conductor, then its terminal ----
  await ev(`d.setFlag('fireStarted'); d.setFlag('reactorOnline')`);
  await open('fuse');
  // a wrong pairing must be refused
  const rightOrder = await page.$$eval('.col.r .term', (t) => t.map((x) => Number(x.dataset.c)));
  await page.click('.wires .col:not(.r) .term:nth-child(1)');
  await page.click(`.col.r .term:nth-child(${rightOrder.indexOf(1) + 1})`);
  if (!(await page.textContent('.panel-foot .status')).includes('WRONG')) problems.push('wires: wrong terminal not rejected');
  for (let i = 0; i < 5; i++) {
    await page.click(`.wires .col:not(.r) .term:nth-child(${i + 1})`);
    await page.click(`.col.r .term:nth-child(${rightOrder.indexOf(i) + 1})`);
  }
  await closed('airlockFuse', 'wires');

  // ---- route: forbidden and unaffordable jumps are refused, a legal route wins ----
  await ev(`for (const f of ['evaStarted','stormWarning','evaDone','reachedBridge','debrisCleared']) d.setFlag(f)`);
  await open('navTable');
  const hit = (id) => page.click(`.route .hit[data-id="${id}"]`);
  await hit('I');
  if (!(await page.textContent('.panel-foot .status')).includes('No jump lane')) problems.push('route: non-adjacent jump not refused');
  await hit('C');
  await hit('D');
  await hit('G');
  if (!(await page.textContent('.panel-foot .status')).includes('PULSAR')) problems.push('route: pulsar not refused');
  await hit('F');
  await hit('I');
  if (!(await page.textContent('.panel-foot .status')).includes('Not enough fuel')) problems.push('route: over-budget jump not refused');
  await page.click('.mg .btn.small:nth-of-type(2)');
  for (const id of ['C', 'F', 'I']) await hit(id);
  await closed('routePlotted', 'route');
  const route = await ev('return d.state.flags.route');
  if (JSON.stringify(route) !== '["A","C","F","I"]') problems.push(`route: stored ${JSON.stringify(route)}`);

  await browser.close();
  console.log('console errors:', errors.length);
  for (const e of [...new Set(errors)].slice(0, 10)) console.log('  ', e.slice(0, 400));
  console.log('problems:', problems.length);
  for (const p of problems) console.log('  ', p);
  if (errors.length || problems.length) process.exit(1);
  console.log('PUZZLES OK');
})().catch((e) => {
  console.error('TEST CRASHED:', e.message.split('\n').slice(0, 3).join(' | '));
  process.exit(2);
});
