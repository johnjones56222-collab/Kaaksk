// Hourly check of Vetta's LLM endpoint. Stays silent while Vetta is broken.
// The first time a request works, it sends one ntfy notification and turns
// itself off. Open /on to start watching again.

const CHAT_URL = "https://api.vetta.sh/v1/proxy/openai/v1/chat/completions";
const MAX_LOG = 1000; // ~6 weeks of hourly entries

async function checkVetta(env) {
  try {
    const res = await fetch(CHAT_URL, {
      method: "POST",
      headers: { authorization: `Bearer ${env.VETTA_KEY}`, "content-type": "application/json" },
      body: JSON.stringify({
        model: "openai/gpt-oss-20b",
        messages: [{ role: "user", content: "say ok" }],
        max_tokens: 5,
      }),
    });
    let detail = `HTTP ${res.status}`;
    try {
      const msg = JSON.parse(await res.text())?.error?.message;
      if (msg) detail += `: ${msg}`;
    } catch {}
    return { up: res.ok, detail };
  } catch (err) {
    return { up: false, detail: `network error: ${err.message}` };
  }
}

// ntfy.sh rate-limits per IP, and Workers share IPs, so retry a few times.
// NTFY_TOKEN (optional secret) sends as your ntfy account, avoiding the shared limit.
async function notify(env, title, message) {
  const headers = { Title: title, Tags: "white_check_mark", Priority: "high" };
  if (env.NTFY_TOKEN) headers.Authorization = `Bearer ${env.NTFY_TOKEN}`;
  let res;
  for (let attempt = 0; attempt < 4; attempt++) {
    if (attempt) await new Promise((r) => setTimeout(r, 3000 * attempt));
    try {
      res = await fetch(`https://ntfy.sh/${env.NTFY_TOPIC}`, { method: "POST", headers, body: message });
      if (res.ok) return res;
    } catch (err) {
      res = new Response(err.message, { status: 599 });
    }
  }
  return res;
}

async function addLog(env, entry) {
  const log = JSON.parse((await env.KV.get("log")) || "[]");
  log.push({ at: new Date().toISOString(), ...entry });
  await env.KV.put("log", JSON.stringify(log.slice(-MAX_LOG)));
}

async function isOn(env) {
  return (await env.KV.get("off")) !== "1";
}

async function runCheck(env) {
  const result = await checkVetta(env);
  await addLog(env, { status: result.up ? "up" : "down", detail: result.detail });

  if (result.up) {
    const time = new Date().toISOString().slice(11, 16) + " UTC";
    const sent = await notify(env, "Vetta is working", `LLM request succeeded at ${time}. Monitor is now off.`);
    // Only turn off once you've actually been told; otherwise retry next hour
    if (sent.ok) await env.KV.put("off", "1");
    else await addLog(env, { status: "warn", detail: `notification failed (HTTP ${sent.status}), will retry next hour` });
    result.notified = sent.ok;
  }
  return result;
}

// Live up/down for browsers. At most one real Vetta request per LIVE_TTL_MS,
// so page loads can't flood Vetta. Doesn't touch on/off or send notifications.
const LIVE_TTL_MS = 120_000;
let liveMemo = null;

async function liveStatus(req, env) {
  const cors = { "access-control-allow-origin": "*", "content-type": "application/json" };
  if (req.method === "OPTIONS") return new Response(null, { headers: { ...cors, "access-control-allow-methods": "GET" } });

  const fresh = (s) => s && Date.now() - Date.parse(s.checked_at) < LIVE_TTL_MS;
  let status = fresh(liveMemo) ? liveMemo : JSON.parse((await env.KV.get("live")) || "null");
  if (!fresh(status)) {
    const r = await checkVetta(env);
    status = { up: r.up, detail: r.detail, checked_at: new Date().toISOString() };
    await env.KV.put("live", JSON.stringify(status));
  }
  liveMemo = status;
  return new Response(JSON.stringify(status), { headers: { ...cors, "cache-control": "no-store" } });
}

export default {
  async scheduled(_event, env, ctx) {
    if (await isOn(env)) ctx.waitUntil(runCheck(env));
  },

  // Public: /live (up/down only, for the Nexus header badge)
  // Private links, all need ?token=ADMIN_TOKEN:
  //   /on  /off  /status  /check (run one check now)  /test (send a test notification)
  async fetch(req, env) {
    const url = new URL(req.url);
    if (url.pathname === "/live") return liveStatus(req, env);
    if (!env.ADMIN_TOKEN || url.searchParams.get("token") !== env.ADMIN_TOKEN)
      return new Response("unauthorized", { status: 401 });

    switch (url.pathname) {
      case "/on":
        await env.KV.delete("off");
        await addLog(env, { status: "on", detail: "turned on" });
        return new Response("Monitor is ON. Checks Vetta every hour and notifies you once it works.\n");
      case "/off":
        await env.KV.put("off", "1");
        await addLog(env, { status: "off", detail: "turned off" });
        return new Response("Monitor is OFF.\n");
      case "/check": {
        if (!(await isOn(env))) return new Response("Monitor is OFF. Open /on first.\n");
        const r = await runCheck(env);
        if (!r.up) return new Response(`Still down: ${r.detail}\n`);
        return new Response(r.notified ? "Vetta works. Notification sent, monitor turned off.\n" : "Vetta works, but the notification failed. Monitor stays on and retries next hour.\n");
      }
      case "/test": {
        const r = await notify(env, "Vetta monitor test", "Test notification. If you see this, ntfy is set up correctly.");
        return new Response(r.ok ? "Test notification sent.\n" : `ntfy refused it: HTTP ${r.status} ${await r.text()}\n`);
      }
      case "/status": {
        const log = JSON.parse((await env.KV.get("log")) || "[]").reverse();
        const lines = log.map((e) => `${e.at.slice(0, 16).replace("T", " ")}  ${e.status.padEnd(4)}  ${e.detail}`);
        return new Response(`Monitor is ${(await isOn(env)) ? "ON" : "OFF"}\n\n${lines.join("\n") || "no checks yet"}\n`);
      }
      default:
        return new Response("Use /on, /off, /status, /check or /test\n", { status: 404 });
    }
  },
};
