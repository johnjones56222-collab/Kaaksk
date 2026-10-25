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

    if (url.pathname === '/__register' && request.method === 'POST') {
      if (request.headers.get('authorization') !== `Bearer ${env.REGISTER_SECRET}`) return new Response('no', { status: 401 });
      const { url: tunnel } = await request.json();
      if (!/^https:\/\/[a-z0-9-]+\.trycloudflare\.com$/.test(tunnel || '')) return new Response('bad url', { status: 400 });
      if ((await env.KV.get('tunnel_url')) !== tunnel) await env.KV.put('tunnel_url', tunnel);
      cached = { url: tunnel, at: Date.now() };
      return new Response('ok');
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
