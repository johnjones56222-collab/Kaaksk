// Stable front door for the sandbox. The sandbox's Cloudflare quick tunnel
// gets a new trycloudflare.com URL whenever it restarts; start.sh POSTs it to
// /__register and this Worker proxies everything (HTTP, SSE, WebSockets) there.
let cached = { url: null, at: 0 };

async function origin(env) {
  if (cached.url && Date.now() - cached.at < 30000) return cached.url;
  cached = { url: await env.KV.get('tunnel_url'), at: Date.now() };
  return cached.url;
}

const offline = () => new Response(
  '<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"><title>Usenaive Terminal</title>'
  + '<body style="background:#0b0d10;color:#e6e8eb;font:15px system-ui;display:grid;place-items:center;height:100vh;margin:0;padding:16px">'
  + '<div><h2>The sandbox is offline</h2><p style="color:#8b939e">The Usenaive computer is paused or restarting. Try again in a minute.</p></div>',
  { status: 503, headers: { 'content-type': 'text/html; charset=utf-8', 'retry-after': '30' } });

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    const isSandbox = request.headers.get('authorization') === `Bearer ${env.REGISTER_SECRET}`;

    // The hub registers without a name; model nodes register as `name` and
    // the hub reads them back from /__nodes.
    if (url.pathname === '/__register' && request.method === 'POST') {
      if (!isSandbox) return new Response('no', { status: 401 });
      const { url: tunnel, name } = await request.json();
      // Quick tunnel names are hyphenated words, which rules out api.trycloudflare.com.
      if (!/^https:\/\/[a-z0-9]+(-[a-z0-9]+)+\.trycloudflare\.com$/.test(tunnel || '')) return new Response('bad url', { status: 400 });
      if (name !== undefined && !/^[a-z0-9-]{1,40}$/.test(name)) return new Response('bad name', { status: 400 });
      const key = name ? 'node:' + name : 'tunnel_url';
      if ((await env.KV.get(key)) !== tunnel) await env.KV.put(key, tunnel);
      if (!name) cached = { url: tunnel, at: Date.now() };
      return new Response('ok');
    }

    // ?names=a,b -> { a: url, b: url }. Named lookups, not KV.list, whose
    // daily free quota is small.
    if (url.pathname === '/__nodes') {
      if (!isSandbox) return new Response('no', { status: 401 });
      const names = (url.searchParams.get('names') || '').split(',').filter((n) => /^[a-z0-9-]{1,40}$/.test(n)).slice(0, 20);
      const nodes = {};
      for (const n of names) nodes[n] = await env.KV.get('node:' + n);
      return Response.json(nodes);
    }

    const base = await origin(env);
    if (!base) return offline();
    const target = new URL(url.pathname + url.search, base);
    try {
      const res = await fetch(new Request(target, request));
      // 530/1033: tunnel is gone; drop the cache so the next request rereads KV.
      if (res.status === 530) { cached.at = 0; return offline(); }
      return res;
    } catch {
      cached.at = 0;
      return offline();
    }
  },
};
