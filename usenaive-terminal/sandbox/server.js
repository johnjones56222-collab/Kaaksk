// Web terminal + Llama runner for a Usenaive computer.
// Serves the page, a WebSocket shell (tmux via node-pty), and proxies /llm/*
// to the local llama-server. Everything sits behind one password.
const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { spawn, execSync } = require('child_process');
const { Readable } = require('stream');
const os = require('os');
const pty = require('node-pty');
const { WebSocketServer } = require('ws');

const PORT = Number(process.env.PORT || 7860);
// Requests pick a model by the `model` field (chat) or ?model= (status/start/
// stop); anything else gets `standard`. The standard model runs here; each
// other one (uncensored, GPT-3.5 style) runs on its own Usenaive computer (a "node", see model-node/),
// found through the Worker and called with MODEL_KEY.
const MODELS = {
  standard: { id: 'llama-3.2-1b-instruct-f16', port: 8080, script: 'llama-max.sh', log: 'llama.log' },
  'uncensored-1b': { id: 'llama-3.2-1b-instruct-abliterated-f16', node: 'uncensored-1b' },
  'uncensored-3b': { id: 'llama-3.2-3b-instruct-abliterated-f16', node: 'uncensored-3b' },
  'uncensored-8b': { id: 'llama-3.1-8b-instruct-abliterated-bf16', node: 'uncensored-8b' },
  'gpt35style-8b': { id: 'llama-3.1-8b-instruct-gpt35style-bf16', node: 'gpt35style-8b' },
  'gpt4all-j': { id: 'gpt4all-j-v1.0', node: 'gpt4all-j' },
};
MODELS.uncensored = MODELS['uncensored-1b']; // the page's old key
const pickModel = (name) => {
  const n = String(name || '').toLowerCase();
  return Object.values(MODELS).find((m) => n === m.id || n.endsWith('/' + m.id)) || MODELS[n]
    || (/gpt4all/.test(n) ? MODELS['gpt4all-j'] : null)
    || (/gpt-?3\.?5|gpt35/.test(n) ? MODELS['gpt35style-8b'] : null)
    || (/abliterat|uncensor/.test(n) ? MODELS[/8b/.test(n) ? 'uncensored-8b' : /3b/.test(n) ? 'uncensored-3b' : 'uncensored-1b'] : MODELS.standard);
};

let nodeCache = { at: 0, urls: {} };
async function nodeUrl(m) {
  if (Date.now() - nodeCache.at > 30000) {
    const names = [...new Set(Object.values(MODELS).filter((x) => x.node).map((x) => x.node))].join(',');
    try {
      const r = await fetch(process.env.WORKER_URL + '/__nodes?names=' + names, {
        headers: { authorization: 'Bearer ' + process.env.REGISTER_SECRET }, signal: AbortSignal.timeout(5000) });
      if (r.ok) nodeCache = { at: Date.now(), urls: await r.json() };
    } catch {}
  }
  return nodeCache.urls[m.node] || null;
}
const PASSWORD = process.env.WEB_PASSWORD;
if (!PASSWORD) throw new Error('WEB_PASSWORD is not set');
const HOME = os.homedir();
const PUBLIC = path.join(__dirname, 'public');

// Session cookie is an HMAC of the password, so it survives restarts.
const SESSION = crypto.createHmac('sha256', PASSWORD).update('session').digest('hex');

function authed(req) {
  const cookie = req.headers.cookie || '';
  return cookie.split(/;\s*/).includes('ut_session=' + SESSION);
}

// Other apps (e.g. Nexus) call /llm/* and /api/llama/* from their own origin
// with `Authorization: Bearer <WEB_PASSWORD>` instead of the cookie.
const isApiPath = (p) => p.startsWith('/llm/') || p.startsWith('/api/llama/');
function bearerOk(req) {
  const got = Buffer.from(String(req.headers.authorization || '').replace(/^Bearer\s+/i, ''));
  const want = Buffer.from(PASSWORD);
  return got.length === want.length && crypto.timingSafeEqual(got, want);
}
const CORS = {
  'access-control-allow-origin': '*',
  'access-control-allow-headers': 'authorization, content-type',
  'access-control-allow-methods': 'GET, POST, OPTIONS',
  'access-control-max-age': '86400',
};

function send(res, status, body, type = 'application/json') {
  res.writeHead(status, { 'content-type': type, 'cache-control': 'no-store', ...(res.cors ? CORS : {}) });
  res.end(typeof body === 'string' || Buffer.isBuffer(body) ? body : JSON.stringify(body));
}

function readBody(req, limit = 1e6) {
  return new Promise((resolve) => {
    let data = '';
    req.on('data', (c) => { data += c; if (data.length > limit) req.destroy(); });
    req.on('end', () => resolve(data));
  });
}

async function llamaHealth(m) {
  if (m.node) {
    const base = await nodeUrl(m);
    if (!base) return 'stopped';
    try {
      const r = await fetch(base + '/health', { signal: AbortSignal.timeout(4000) });
      if (r.status === 530 || r.status === 502) { nodeCache.at = 0; return 'stopped'; }
      return r.status === 200 ? 'ready' : 'loading';
    } catch { nodeCache.at = 0; return 'stopped'; }
  }
  return new Promise((resolve) => {
    const r = http.get({ host: '127.0.0.1', port: m.port, path: '/health', timeout: 1500 }, (res) => {
      let d = '';
      res.on('data', (c) => (d += c));
      res.on('end', () => resolve(res.statusCode === 200 ? 'ready' : 'loading'));
    });
    r.on('error', () => resolve('stopped'));
    r.on('timeout', () => { r.destroy(); resolve('loading'); });
  });
}

function llamaRunning(m) {
  if (m.node) return false;
  try { execSync('pgrep -f "llama-serve[r].*--port ' + m.port + '"'); return true; } catch { return false; }
}

function startLlama(m) {
  if (m.node || llamaRunning(m)) return false; // nodes keep their own server running
  const log = path.join(HOME, m.log);
  const child = spawn('bash', [path.join(__dirname, m.script)], {
    detached: true,
    stdio: ['ignore', fs.openSync(log, 'w'), fs.openSync(log, 'a')],
  });
  child.unref();
  return true;
}

// Chat requests start the model themselves, so callers never have to press
// Start first. Loading takes a few seconds; give up after 90.
async function ensureLlama(m) {
  if ((await llamaHealth(m)) === 'ready') return true;
  startLlama(m);
  for (let i = 0; i < 90; i++) {
    await new Promise((r) => setTimeout(r, 1000));
    if ((await llamaHealth(m)) === 'ready') return true;
  }
  return false;
}

function systemInfo() {
  const mem = os.totalmem(), free = os.freemem();
  let disk = '';
  try { disk = execSync("df -h / | tail -1 | awk '{print $3\" / \"$2}'").toString().trim(); } catch {}
  return {
    hostname: os.hostname(),
    cpus: os.cpus().length,
    cpu_model: os.cpus()[0] && os.cpus()[0].model,
    mem_total_gb: +(mem / 2 ** 30).toFixed(1),
    mem_used_gb: +((mem - free) / 2 ** 30).toFixed(1),
    load: os.loadavg().map((n) => +n.toFixed(2)),
    disk,
    uptime_s: Math.round(os.uptime()),
  };
}

const MIME = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml' };

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x');

  if (url.pathname === '/api/login' && req.method === 'POST') {
    const body = await readBody(req);
    let pw = '';
    try { pw = JSON.parse(body).password || ''; } catch {}
    const ok = pw.length === PASSWORD.length && crypto.timingSafeEqual(Buffer.from(pw), Buffer.from(PASSWORD));
    if (!ok) return send(res, 401, { error: 'wrong password' });
    res.writeHead(200, {
      'content-type': 'application/json',
      'set-cookie': `ut_session=${SESSION}; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=2592000`,
    });
    return res.end('{"ok":true}');
  }

  if (url.pathname === '/' || url.pathname === '/index.html') {
    return send(res, 200, fs.readFileSync(path.join(PUBLIC, 'index.html')), MIME['.html']);
  }

  if (isApiPath(url.pathname)) {
    res.cors = true;
    if (req.method === 'OPTIONS') { res.writeHead(204, CORS); return res.end(); }
  }
  if (!authed(req) && !(res.cors && bearerOk(req))) return send(res, 401, { error: 'login required' });

  if (url.pathname === '/api/me') return send(res, 200, { ok: true });
  if (url.pathname === '/api/system') return send(res, 200, systemInfo());

  const model = pickModel(url.searchParams.get('model'));

  if (url.pathname === '/api/llama/status') {
    const health = await llamaHealth(model);
    const status = health === 'stopped' && llamaRunning(model) ? 'loading' : health;
    let log = '';
    if (model.node) return send(res, 200, { status, log, node: model.node });
    try { log = fs.readFileSync(path.join(HOME, model.log), 'utf8').split('\n').slice(-25).join('\n'); } catch {}
    return send(res, 200, { status, log });
  }

  if (url.pathname === '/api/llama/start' && req.method === 'POST') {
    if (!startLlama(model)) return send(res, 200, { started: false, status: await llamaHealth(model) });
    return send(res, 200, { started: true });
  }

  if (url.pathname === '/api/llama/stop' && req.method === 'POST') {
    if (model.node) return send(res, 200, { stopped: false, node: model.node });
    try { execSync('pkill -f "llama-serve[r].*--port ' + model.port + '"'); } catch {}
    return send(res, 200, { stopped: true });
  }

  // Proxy /llm/* to llama-server (streams SSE straight through).
  if (url.pathname.startsWith('/llm/')) {
    if (url.pathname === '/llm/v1/models') {
      return send(res, 200, { object: 'list', data: [...new Set(Object.values(MODELS))].map((m) => ({ id: m.id, object: 'model', owned_by: m.node ? 'node:' + m.node : 'sandbox' })) });
    }
    // Read the body to route on its `model`, then replay it upstream.
    const body = req.method === 'POST' ? await readBody(req, 50e6) : ''; // a full 128K-token chat is a few MB
    let target = model;
    if (body) { try { target = pickModel(JSON.parse(body).model); } catch {} }
    if (req.method === 'POST' && !(await ensureLlama(target))) {
      return send(res, 503, { error: target.node ? `the ${target.node} computer is offline or still loading` : 'llama-server did not come up; see ~/' + target.log + ' on the sandbox' });
    }
    if (target.node) {
      const ac = new AbortController();
      res.on('close', () => ac.abort());
      try {
        const up = await fetch((await nodeUrl(target)) + url.pathname.slice(4) + url.search, {
          method: req.method, body: body || undefined, signal: ac.signal,
          headers: { 'content-type': 'application/json', authorization: 'Bearer ' + process.env.MODEL_KEY },
        });
        res.writeHead(up.status, { 'content-type': up.headers.get('content-type') || 'application/json', 'cache-control': 'no-store', 'x-accel-buffering': 'no', ...CORS });
        if (up.body) Readable.fromWeb(up.body).on('error', () => res.end()).pipe(res); else res.end();
      } catch { if (!res.headersSent) send(res, 502, { error: `could not reach the ${target.node} computer` }); }
      return;
    }
    const upstream = http.request({
      host: '127.0.0.1', port: target.port, method: req.method,
      path: url.pathname.slice(4) + url.search,
      headers: { 'content-type': req.headers['content-type'] || 'application/json', 'content-length': Buffer.byteLength(body) },
    }, (up) => {
      res.writeHead(up.statusCode, { 'content-type': up.headers['content-type'] || 'application/json', 'cache-control': 'no-store', 'x-accel-buffering': 'no', ...CORS });
      up.pipe(res);
    });
    upstream.on('error', () => send(res, 503, { error: 'llama-server is not running — press "Start Llama 3.2 1B (Max)"' }));
    upstream.end(body);
    res.on('close', () => upstream.destroy()); // client went away (req 'close' fires once the body is read)
    return;
  }

  const file = path.join(PUBLIC, path.normalize(url.pathname));
  if (file.startsWith(PUBLIC) && fs.existsSync(file) && fs.statSync(file).isFile()) {
    return send(res, 200, fs.readFileSync(file), MIME[path.extname(file)] || 'application/octet-stream');
  }
  send(res, 404, { error: 'not found' });
});

// Terminal: every browser tab attaches to a tmux session, so shells and
// running programs survive page reloads.
const wss = new WebSocketServer({ noServer: true });
server.on('upgrade', (req, socket, head) => {
  const url = new URL(req.url, 'http://x');
  if (url.pathname !== '/ws/term' || !authed(req)) { socket.destroy(); return; }
  wss.handleUpgrade(req, socket, head, (ws) => {
    const session = (url.searchParams.get('session') || 'main').replace(/[^\w-]/g, '').slice(0, 32) || 'main';
    const term = pty.spawn('tmux', ['new-session', '-A', '-s', session], {
      name: 'xterm-256color', cols: 120, rows: 32, cwd: HOME,
      env: { ...process.env, TERM: 'xterm-256color', WEB_PASSWORD: '' },
    });
    term.onData((d) => { if (ws.readyState === 1) ws.send(d); });
    term.onExit(() => ws.close());
    ws.on('message', (msg) => {
      const s = msg.toString();
      if (s.startsWith('\x00resize:')) {
        const [cols, rows] = s.slice(8).split('x').map(Number);
        if (cols > 0 && rows > 0) term.resize(cols, rows);
      } else if (!s.startsWith('\x00')) term.write(s); // \x00ping keeps proxies from idling the socket out
    });
    ws.on('close', () => term.kill());
  });
});

server.listen(PORT, '127.0.0.1', () => console.log('listening on', PORT));
