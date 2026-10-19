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

async function notify(env, title, message) {
  await fetch(`https://ntfy.sh/${env.NTFY_TOPIC}`, {
    method: "POST",
    headers: { Title: title, Tags: "white_check_mark", Priority: "high" },
    body: message,
  });
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
    await env.KV.put("off", "1");
    await notify(env, "Vetta is working", `LLM request succeeded at ${time}. Monitor is now off.`);
  }
  return result;
}

export default {
  async scheduled(_event, env, ctx) {
    if (await isOn(env)) ctx.waitUntil(runCheck(env));
  },

  // Private links, all need ?token=ADMIN_TOKEN:
  //   /on  /off  /status  /check (run one check now)
  async fetch(req, env) {
    const url = new URL(req.url);
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
        return new Response(r.up ? "Vetta works. Notification sent, monitor turned off.\n" : `Still down: ${r.detail}\n`);
      }
      case "/status": {
        const log = JSON.parse((await env.KV.get("log")) || "[]").reverse();
        const lines = log.map((e) => `${e.at.slice(0, 16).replace("T", " ")}  ${e.status.padEnd(4)}  ${e.detail}`);
        return new Response(`Monitor is ${(await isOn(env)) ? "ON" : "OFF"}\n\n${lines.join("\n") || "no checks yet"}\n`);
      }
      default:
        return new Response("Use /on, /off, /status or /check\n", { status: 404 });
    }
  },
};
