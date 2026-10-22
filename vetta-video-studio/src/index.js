// Vetta Video Studio: a small web app for generating videos and images with Vetta.
// The browser can't call api.vetta.sh directly (no CORS), so this Worker serves
// the page and forwards its calls, using the API key the page sends in
// `x-vetta-key`. Every started video is tracked in KV and checked each minute
// by the cron trigger, which sends an ntfy notification when it completes or
// fails, even if the page is closed. Images render synchronously, so their
// notification is sent as soon as the request settles (see generateImage for
// what happens if the page is reloaded first). The image gallery is read from
// the account's Vetta Files, so it survives reloads and works on any device.

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

// Signed links last about 7 days; reuse them for 6 so a gallery load stays cheap.
async function cachedUrl(env, key, fileId) {
  const hit = await env.KV.get(`url:${fileId}`);
  if (hit) return hit;
  const url = await mintUrl(key, fileId);
  await env.KV.put(`url:${fileId}`, url, { expirationTtl: 6 * 86400 });
  return url;
}

// ntfy.sh rate-limits per IP and Workers share IPs, so messages are sent with
// the account token (NTFY_TOKEN secret) when there is one, and retried.
let NTFY_TOKEN = "";

async function notify(topic, title, message, tags, clickUrl, { attach, label = "Open video" } = {}) {
  const headers = { Title: title, Tags: tags, Priority: "high" };
  if (clickUrl) {
    headers.Click = clickUrl;
    headers.Actions = `view, ${label}, ${clickUrl}`;
  }
  if (attach) headers.Attach = attach; // ntfy shows the image in the notification
  if (NTFY_TOKEN) headers.Authorization = `Bearer ${NTFY_TOKEN}`;
  for (let attempt = 0; attempt < 3; attempt++) {
    if (attempt) await new Promise((r) => setTimeout(r, 2500 * attempt));
    // Last try is plain text, in case the attachment or button was what ntfy refused.
    const h = attempt === 2 ? { Title: title, Tags: tags, Priority: "high", ...(NTFY_TOKEN ? { Authorization: headers.Authorization } : {}) } : headers;
    const body = attempt === 2 && clickUrl ? `${message}\n${clickUrl}` : message;
    try {
      const res = await fetch(`https://ntfy.sh/${encodeURIComponent(topic)}`, { method: "POST", headers: h, body });
      if (res.ok) return true;
    } catch {}
  }
  return false;
}

// A notification ntfy refused is kept in KV and retried by checkJobs.
async function notifyOrQueue(env, n) {
  if (await notify(n.topic, n.title, n.message, n.tags, n.click, { attach: n.attach, label: n.label })) return;
  await env.KV.put(`note:${crypto.randomUUID()}`, JSON.stringify({ ...n, attempts: 1 }));
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

const DEFAULT_STYLES = ["Phone snap", "Realistic", "Everyday", "Library", "Quiet luxury", "Through the glass", "Flight mode",
  "Selfcare", "Japandi", "Foggy morning", "Rainy day", "90s editorial", "Wide-angle outfit", "Elevator mirror", "Escalator", "Tokyo street style"];

async function startVideo(env, key, b) {
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
    headers: { "idempotency-key": /^[\w-]{8,100}$/.test(b.idempotency_key || "") ? b.idempotency_key : crypto.randomUUID() },
  });
  const topic = (b.ntfy_topic || env.NTFY_TOPIC || "").trim();
  if (topic) {
    await env.KV.put(`job:${job.id}`, JSON.stringify({
      key, topic, model: job.model, prompt: b.prompt, created: Date.now(), notify_attempts: 0,
    }));
  }
  return { id: job.id, status: job.status, model: job.model, tracked: Boolean(topic) };
}

// Vetta keeps rendering when the caller hangs up, and a repeat request with the
// same idempotency key returns the finished result instead of a new image. So
// while a request runs, img:<key> is kept in KV; if this Worker gets cut off
// (e.g. the page was reloaded), checkJobs replays it later to send the ntfy.
async function generateImage(env, ctx, key, b) {
  const topic = (b.ntfy_topic || env.NTFY_TOPIC || "").trim();
  const what = `${b.model}\n"${(b.prompt || "").slice(0, 140)}"`;
  const ik = /^[\w-]{8,100}$/.test(b.idempotency_key || "") ? b.idempotency_key : crypto.randomUUID();
  const rec = `img:${ik}`;
  if (topic && !b.replay) {
    await env.KV.put(rec, JSON.stringify({ key, b: { ...b, idempotency_key: ik }, started: Date.now() }), { expirationTtl: 2 * 3600 });
  }
  try {
    const res = await vetta(key, "POST", "/media/images", {
      json: strip({
        model: b.model,
        prompt: b.prompt,
        n: b.n ? Number(b.n) : undefined,
        aspect_ratio: b.aspect_ratio,
        seed: b.seed !== "" && b.seed != null ? Number(b.seed) : undefined,
        character: b.character,
        character_strength: b.character && b.character_strength ? Number(b.character_strength) : undefined,
        style: b.style,
        image_urls: b.images?.length ? b.images.slice(0, 4) : undefined,
      }),
      headers: { "idempotency-key": ik },
    });
    const files = await Promise.all((res.files || []).map(async (f) => ({ file_id: f.id, url: await cachedUrl(env, key, f.id), width: f.width, height: f.height })));
    const cost = res.cost_micro_usd != null ? res.cost_micro_usd / 1e6 : null;
    if (topic) {
      const msg = `${what}\n${files.length} image${files.length === 1 ? "" : "s"}${cost != null ? ` · $${cost.toFixed(3)}` : ""}`;
      ctx.waitUntil(notifyOrQueue(env, { topic, title: "Image ready", message: msg, tags: "white_check_mark,frame_with_picture", click: files[0]?.url, attach: files[0]?.url, label: "Open image" }));
    }
    if (topic) await env.KV.delete(rec);
    return { model: res.model, cost_usd: cost, files };
  } catch (err) {
    // 409: the original request is still rendering; let it (or a later replay) finish.
    if (b.replay && err.status === 409) throw err;
    if (topic) {
      ctx.waitUntil(notifyOrQueue(env, { topic, title: "Image failed", message: `${what}\n${err.message}`, tags: "x" }));
      await env.KV.delete(rec);
    }
    throw err;
  }
}

async function api(req, env, ctx, path) {
  const key = (req.headers.get("x-vetta-key") || "").trim();
  if (!key.startsWith("sk_")) return json({ error: "Add your Vetta API key in Settings." }, 401);

  if (path === "/api/models") {
    const kind = new URL(req.url).searchParams.get("kind") === "image" ? "image" : "video";
    const models = await listAll(key, `/media/models?kind=${kind}`);
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

  // Vetta publishes no list of image styles, but its validation error names them,
  // so ask with an invalid style and a model that doesn't exist (nothing renders).
  if (path === "/api/styles") {
    try {
      await vetta(key, "POST", "/media/images", {
        json: { model: "none/none", prompt: "x", style: "__list__" },
        headers: { "idempotency-key": crypto.randomUUID() },
      });
    } catch (err) {
      const m = err.message.match(/use one of: (.+)$/);
      if (m) return json(m[1].split(",").map((x) => x.trim()).filter(Boolean));
    }
    return json(DEFAULT_STYLES);
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
    const work = startVideo(env, key, b);
    ctx.waitUntil(work.catch(() => {}));
    return json(await work);
  }

  if (path === "/api/generate-image" && req.method === "POST") {
    // waitUntil keeps the Vetta call (and its notification) alive even if the
    // page is reloaded while the image is still rendering.
    const b = await req.json();
    const work = generateImage(env, ctx, key, b);
    ctx.waitUntil(work.catch(() => {}));
    return json(await work);
  }

  // Images generated with this key, newest first, from Vetta Files.
  if (path === "/api/images") {
    const page = await vetta(key, "GET", "/files?kind=image&source=image_generation&limit=30");
    const files = await Promise.all(page.data.map(async (f) => ({
      file_id: f.id,
      model: f.source?.model || null,
      prompt: f.source?.prompt || "",
      created_at: f.created_at,
      url: await cachedUrl(env, key, f.id).catch(() => null),
    })));
    return json(files);
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

// Runs from the cron trigger and, as a backup, from page traffic (at most every
// 20s per isolate): follows every tracked video job and notifies once it
// settles, and retries image notifications ntfy refused.
let lastCheck = 0;

async function checkJobs(env) {
  lastCheck = Date.now();
  const notes = await env.KV.list({ prefix: "note:" });
  for (const { name } of notes.keys.slice(0, 3)) {
    const n = JSON.parse((await env.KV.get(name)) || "null");
    if (!n) continue;
    const ok = await notify(n.topic, n.title, n.message, n.tags, n.click, { attach: n.attach, label: n.label });
    if (ok || n.attempts >= MAX_NOTIFY_ATTEMPTS) await env.KV.delete(name);
    else await env.KV.put(name, JSON.stringify({ ...n, attempts: n.attempts + 1 }));
  }

  const imgs = await env.KV.list({ prefix: "img:" });
  for (const { name } of imgs.keys.slice(0, 3)) {
    const r = JSON.parse((await env.KV.get(name)) || "null");
    if (!r || Date.now() - r.started < 3 * 60_000) continue; // probably still being handled live
    const later = [];
    try {
      await generateImage(env, { waitUntil: (p) => later.push(p) }, r.key, { ...r.b, replay: true });
    } catch (err) {
      if (err.status === 409 && Date.now() - r.started < 60 * 60_000) continue;
      await env.KV.delete(name); // failure was already reported by generateImage
    }
    await Promise.all(later);
  }

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

// Lets a copy of the page saved on your device (standalone.html) call this server.
const CORS = {
  "access-control-allow-origin": "*",
  "access-control-allow-headers": "x-vetta-key, x-filename, content-type",
  "access-control-allow-methods": "GET, POST, OPTIONS",
};
const withCors = (res) => {
  const out = new Response(res.body, res);
  for (const [k, v] of Object.entries(CORS)) out.headers.set(k, v);
  return out;
};

export default {
  async fetch(req, env, ctx) {
    NTFY_TOKEN = env.NTFY_TOKEN || "";
    const path = new URL(req.url).pathname;
    if (req.method === "OPTIONS") return new Response(null, { headers: CORS });
    if (path.startsWith("/api/")) {
      if (Date.now() - lastCheck > 20_000) ctx.waitUntil(checkJobs(env));
      try {
        return withCors(await api(req, env, ctx, path));
      } catch (err) {
        return withCors(json({ error: err.message }, err.status || 500));
      }
    }
    if (path === "/api-config") return withCors(json({ ntfy_topic: env.NTFY_TOPIC || "" }));
    return new Response(PAGE, { headers: { "content-type": "text/html; charset=utf-8" } });
  },

  async scheduled(_event, env, ctx) {
    NTFY_TOKEN = env.NTFY_TOKEN || "";
    ctx.waitUntil(checkJobs(env));
  },
};
