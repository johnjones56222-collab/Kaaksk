// Vetta Video Studio: a small web app for generating videos with Vetta.
// The browser can't call api.vetta.sh directly (no CORS), so this Worker serves
// the page and forwards its calls, using the API key the page sends in
// `x-vetta-key`. Every started video is tracked in KV and checked each minute
// by the cron trigger, which sends an ntfy notification when it completes or
// fails, even if the page is closed.

import PAGE from "./page.html";

const VETTA = "https://api.vetta.sh/v1";
const MAX_JOBS_PER_RUN = 6; // keeps a cron run well under the subrequest limit
const MAX_NOTIFY_ATTEMPTS = 30; // ~30 minutes of retries if ntfy keeps refusing
const GIVE_UP_AFTER_MS = 3 * 60 * 60_000;

class VettaError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

async function vetta(key, method, path, { json, form, headers = {} } = {}) {
  const res = await fetch(VETTA + path, {
    method,
    headers: { authorization: `Bearer ${key}`, ...(json ? { "content-type": "application/json" } : {}), ...headers },
    body: json ? JSON.stringify(json) : form,
  });
  const text = await res.text();
  let data;
  try {
    data = JSON.parse(text);
  } catch {
    data = { raw: text };
  }
  if (!res.ok) throw new VettaError(res.status, data?.error?.message || text.slice(0, 300));
  return data;
}

const strip = (o) => Object.fromEntries(Object.entries(o).filter(([, v]) => v !== undefined && v !== null && v !== ""));
const json = (data, status = 200) => Response.json(data, { status });

async function mintUrl(key, fileId) {
  return (await vetta(key, "POST", `/files/${fileId}/url`)).url;
}

// ntfy.sh rate-limits per IP and Workers share IPs, so retry a few times.
async function notify(topic, title, message, tags, clickUrl) {
  const headers = { Title: title, Tags: tags, Priority: "high" };
  if (clickUrl) {
    headers.Click = clickUrl;
    headers.Actions = `view, Open video, ${clickUrl}`;
  }
  for (let attempt = 0; attempt < 3; attempt++) {
    if (attempt) await new Promise((r) => setTimeout(r, 2500 * attempt));
    try {
      const res = await fetch(`https://ntfy.sh/${encodeURIComponent(topic)}`, { method: "POST", headers, body: message });
      if (res.ok) return true;
    } catch {}
  }
  return false;
}

async function listAll(key, path) {
  const out = [];
  let after;
  do {
    const sep = path.includes("?") ? "&" : "?";
    const page = await vetta(key, "GET", `${path}${sep}limit=100${after ? `&after=${after}` : ""}`);
    out.push(...page.data);
    after = page.has_more ? page.next_cursor : undefined;
  } while (after);
  return out;
}

async function api(req, env, path) {
  const key = (req.headers.get("x-vetta-key") || "").trim();
  if (!key.startsWith("sk_")) return json({ error: "Add your Vetta API key in Settings." }, 401);

  if (path === "/api/models") {
    const models = await listAll(key, "/media/models?kind=video");
    return json(models.map((m) => ({ id: m.id, name: m.name, description: m.description })));
  }

  if (path === "/api/balance") {
    const [bal, sub] = await Promise.all([
      vetta(key, "GET", "/credits/balance"),
      vetta(key, "GET", "/credits/subscription").catch(() => null),
    ]);
    return json({ balance_usd: bal.balance_micro_usd / 1e6, subscription: sub?.status ?? null });
  }

  if (path === "/api/characters") {
    const chars = await listAll(key, "/characters").catch(() => []);
    return json(chars.filter((c) => c.status === "ready").map((c) => ({ id: c.id, name: c.name, scope: c.scope })));
  }

  // The raw file is the request body; it goes to Vetta Files. Videos also get a
  // signed URL, because video_url must be an http(s) link.
  if (path === "/api/upload" && req.method === "POST") {
    const name = decodeURIComponent(req.headers.get("x-filename") || "upload");
    const type = req.headers.get("content-type") || "application/octet-stream";
    const form = new FormData();
    form.append("file", new File([await req.arrayBuffer()], name, { type }));
    form.append("name", name);
    const file = await vetta(key, "POST", "/files", { form });
    return json({ file_id: file.id, kind: file.kind, url: await mintUrl(key, file.id) });
  }

  if (path === "/api/generate" && req.method === "POST") {
    const b = await req.json();
    const job = await vetta(key, "POST", "/media/videos", {
      json: strip({
        model: b.model,
        prompt: b.prompt,
        seconds: b.seconds ? Number(b.seconds) : undefined,
        aspect_ratio: b.aspect_ratio,
        seed: b.seed !== "" && b.seed != null ? Number(b.seed) : undefined,
        character: b.character,
        image_urls: b.image ? [b.image] : undefined,
        video_url: b.video_url,
      }),
      headers: { "idempotency-key": crypto.randomUUID() },
    });
    const topic = (b.ntfy_topic || env.NTFY_TOPIC || "").trim();
    if (topic) {
      await env.KV.put(`job:${job.id}`, JSON.stringify({
        key, topic, model: job.model, prompt: b.prompt, created: Date.now(), notify_attempts: 0,
      }));
    }
    return json({ id: job.id, status: job.status, model: job.model, tracked: Boolean(topic) });
  }

  if (path === "/api/jobs") {
    const page = await vetta(key, "GET", "/media?kind=video&limit=20");
    const jobs = await Promise.all(page.data.map(async (j) => {
      const fileId = j.result?.files?.[0]?.file_id;
      return {
        id: j.id,
        status: j.status,
        model: j.model,
        prompt: j.input?.prompt,
        seconds: j.input?.seconds,
        cost_usd: j.cost_micro_usd != null ? j.cost_micro_usd / 1e6 : null,
        error: j.error?.message || null,
        created_at: j.created_at,
        url: j.status === "completed" && fileId ? await mintUrl(key, fileId).catch(() => null) : null,
      };
    }));
    return json(jobs);
  }

  return json({ error: "Not found" }, 404);
}

// Cron: follow every tracked job and notify once it settles.
async function checkJobs(env) {
  const { keys } = await env.KV.list({ prefix: "job:" });
  for (const { name } of keys.slice(0, MAX_JOBS_PER_RUN)) {
    const rec = JSON.parse((await env.KV.get(name)) || "null");
    if (!rec) continue;
    const id = name.slice(4);
    const what = `${rec.model}\n"${(rec.prompt || "").slice(0, 140)}"`;
    try {
      const job = await vetta(rec.key, "GET", `/media/${id}`);
      let sent = null;
      if (job.status === "completed") {
        const fileId = job.result?.files?.[0]?.file_id;
        const url = fileId ? await mintUrl(rec.key, fileId) : undefined;
        const cost = job.cost_micro_usd != null ? `\nCost: $${(job.cost_micro_usd / 1e6).toFixed(2)}` : "";
        sent = await notify(rec.topic, "Video ready", `${what}${cost}`, "white_check_mark,movie_camera", url);
      } else if (job.status === "failed" || job.status === "canceled" || job.status === "cancelled") {
        sent = await notify(rec.topic, "Video failed", `${what}\n${job.error?.message || job.status}`, "x");
      } else if (Date.now() - rec.created > GIVE_UP_AFTER_MS) {
        sent = await notify(rec.topic, "Video still not done", `${what}\nStill ${job.status} after 3 hours; stopped tracking ${id}.`, "hourglass");
        sent = true; // stop tracking either way
      }
      if (sent === null) continue; // still running
      rec.notify_attempts++;
      if (sent || rec.notify_attempts >= MAX_NOTIFY_ATTEMPTS) await env.KV.delete(name);
      else await env.KV.put(name, JSON.stringify(rec));
    } catch (err) {
      // A job this key can no longer see (revoked key, deleted job) is dropped.
      if (err.status === 401 || err.status === 403 || err.status === 404) await env.KV.delete(name);
    }
  }
}

export default {
  async fetch(req, env) {
    const path = new URL(req.url).pathname;
    if (path.startsWith("/api/")) {
      try {
        return await api(req, env, path);
      } catch (err) {
        return json({ error: err.message }, err.status || 500);
      }
    }
    if (path === "/api-config") return json({ ntfy_topic: env.NTFY_TOPIC || "" });
    return new Response(PAGE, { headers: { "content-type": "text/html; charset=utf-8" } });
  },

  async scheduled(_event, env, ctx) {
    ctx.waitUntil(checkJobs(env));
  },
};
