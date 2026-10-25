// Web terminal + Llama runner for a Usenaive computer.
// Serves the page, a WebSocket shell (tmux via node-pty), and proxies /llm/*
// to the local llama-server. Everything sits behind one password.
const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { spawn, execSync } = require('child_process');
const os = require('os');
const pty = require('node-pty');
const { WebSocketServer } = require('ws');

const PORT = Number(process.env.PORT || 7860);
const LLM_PORT = Number(process.env.LLM_PORT || 8080);
const PASSWORD = process.env.WEB_PASSWORD;
if (!PASSWORD) throw new Error('WEB_PASSWORD is not set');
const HOME = os.homedir();
const LLAMA_LOG = path.join(HOME, 'llama.log');
const PUBLIC = path.join(__dirname, 'public');

// Session cookie is an HMAC of the password, so it survives restarts.
const SESSION = crypto.createHmac('sha256', PASSWORD).update('session').digest('hex');

function authed(req) {
  const cookie = req.headers.cookie || '';
  return cookie.split(/;\s*/).includes('ut_session=' + SESSION);
}

function send(res, status, body, type = 'application/json') {
  res.writeHead(status, { 'content-type': type, 'cache-control': 'no-store' });
  res.end(typeof body === 'string' || Buffer.isBuffer(body) ? body : JSON.stringify(body));
}

function readBody(req) {
  return new Promise((resolve) => {
    let data = '';
    req.on('data', (c) => { data += c; if (data.length > 1e6) req.destroy(); });
    req.on('end', () => resolve(data));
  });
}

function llamaHealth() {
  return new Promise((resolve) => {
    const r = http.get({ host: '127.0.0.1', port: LLM_PORT, path: '/health', timeout: 1500 }, (res) => {
      let d = '';
      res.on('data', (c) => (d += c));
      res.on('end', () => resolve(res.statusCode === 200 ? 'ready' : 'loading'));
    });
    r.on('error', () => resolve('stopped'));
    r.on('timeout', () => { r.destroy(); resolve('loading'); });
  });
}

function llamaRunning() {
  try { execSync('pgrep -f "llama-serve[r].*--port ' + LLM_PORT + '"'); return true; } catch { return false; }
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

  if (!authed(req)) return send(res, 401, { error: 'login required' });

  if (url.pathname === '/api/me') return send(res, 200, { ok: true });
  if (url.pathname === '/api/system') return send(res, 200, systemInfo());

  if (url.pathname === '/api/llama/status') {
    const health = await llamaHealth();
    const status = health === 'stopped' && llamaRunning() ? 'loading' : health;
    let log = '';
    try { log = fs.readFileSync(LLAMA_LOG, 'utf8').split('\n').slice(-25).join('\n'); } catch {}
    return send(res, 200, { status, log });
  }

  if (url.pathname === '/api/llama/start' && req.method === 'POST') {
    if (llamaRunning()) return send(res, 200, { started: false, status: await llamaHealth() });
    const child = spawn('bash', [path.join(__dirname, 'llama-max.sh')], {
      detached: true,
      stdio: ['ignore', fs.openSync(LLAMA_LOG, 'w'), fs.openSync(LLAMA_LOG, 'a')],
    });
    child.unref();
    return send(res, 200, { started: true });
  }

  if (url.pathname === '/api/llama/stop' && req.method === 'POST') {
    try { execSync('pkill -f "llama-serve[r].*--port ' + LLM_PORT + '"'); } catch {}
    return send(res, 200, { stopped: true });
  }

  // Proxy /llm/* to llama-server (streams SSE straight through).
  if (url.pathname.startsWith('/llm/')) {
    const upstream = http.request({
      host: '127.0.0.1', port: LLM_PORT, method: req.method,
      path: url.pathname.slice(4) + url.search,
      headers: { 'content-type': req.headers['content-type'] || 'application/json' },
    }, (up) => {
      res.writeHead(up.statusCode, { 'content-type': up.headers['content-type'] || 'application/json', 'cache-control': 'no-store', 'x-accel-buffering': 'no' });
      up.pipe(res);
    });
    upstream.on('error', () => send(res, 503, { error: 'llama-server is not running — press "Start Llama 3.2 1B (Max)"' }));
    req.pipe(upstream);
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
