// Runs Nexus model requests server-side so a reply keeps generating after the
// page is closed. Nexus posts the exact request it would have sent (url,
// headers, body); a Durable Object per job makes that request, records the raw
// response text, and streams it back. Reopening Nexus replays the recording.

import { DurableObject } from "cloudflare:workers";

const CORS = {
  "access-control-allow-origin": "*",
  "access-control-allow-methods": "GET, POST, OPTIONS",
  "access-control-allow-headers": "content-type, x-nexus-token",
  "access-control-expose-headers": "x-nexus-job, x-nexus-state",
};
const KEEP_MS = 24 * 60 * 60 * 1000; // a finished reply can be collected for a day
const FLUSH_MS = 2000; // checkpoint to storage while streaming, in case the object is evicted
const CHUNK_CHARS = 512 * 1024; // stays under the 2 MB per-value storage limit
const DROP_HEADERS = new Set(["host", "content-length", "connection", "accept-encoding"]);

export class ReplyJob extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    this.live = null; // set while (or after) this instance runs the upstream request
  }

  async fetch(request) {
    const action = new URL(request.url).pathname.split("/").pop();
    if (action === "start") return this.start(await request.json());
    if (action === "stream") return this.stream();
    if (action === "cancel") return this.cancel();
    if (action === "status") {
      const meta = await this.ctx.storage.get("meta");
      return meta ? Response.json(meta) : new Response("unknown job", { status: 404 });
    }
    return new Response("not found", { status: 404 });
  }

  async start(job) {
    if (this.live || (await this.ctx.storage.get("meta"))) return new Response("job already started", { status: 409 });
    let readyResolve;
    const live = {
      text: "", flushed: 0, seq: 0, status: null, contentType: "", state: "running",
      controller: new AbortController(), waiters: [], flushTimer: null,
      ready: new Promise((r) => (readyResolve = r)), readyResolve,
    };
    this.live = live;
    await this.ctx.storage.put("meta", { state: "running", created: Date.now() });
    await this.ctx.storage.setAlarm(Date.now() + KEEP_MS);
    this.ctx.waitUntil(this.pump(job, live));
    return this.stream();
  }

  async pump(job, live) {
    try {
      const headers = {};
      for (const [k, v] of Object.entries(job.headers || {})) if (!DROP_HEADERS.has(k.toLowerCase())) headers[k] = v;
      const res = await fetch(job.url, {
        method: job.method || "POST", headers, body: job.body, signal: live.controller.signal,
      });
      live.status = res.status;
      live.contentType = res.headers.get("content-type") || "text/plain";
      live.readyResolve();
      await this.saveMeta(live);
      const reader = res.body.getReader();
      const decoder = new TextDecoder();
      for (;;) {
        const { done, value } = await reader.read();
        if (done) break;
        this.append(live, decoder.decode(value, { stream: true }));
      }
      this.append(live, decoder.decode());
      live.state = "done";
    } catch (err) {
      live.state = live.state === "cancelled" ? "cancelled" : "error";
      if (live.status === null) {
        live.status = live.state === "cancelled" ? 499 : 502;
        live.contentType = "text/plain";
        live.text = live.state === "cancelled" ? "Cancelled." : `Background runner could not reach the model: ${err.message}`;
      }
    }
    live.readyResolve();
    clearTimeout(live.flushTimer);
    await this.flush(live);
    await this.saveMeta(live);
    this.wake(live);
  }

  append(live, text) {
    if (!text) return;
    live.text += text;
    this.wake(live);
    if (!live.flushTimer) live.flushTimer = setTimeout(() => { live.flushTimer = null; this.flush(live); }, FLUSH_MS);
  }

  wake(live) {
    const waiters = live.waiters;
    live.waiters = [];
    waiters.forEach((w) => w());
  }

  async flush(live) {
    while (live.flushed < live.text.length) {
      const piece = live.text.slice(live.flushed, live.flushed + CHUNK_CHARS);
      live.flushed += piece.length;
      await this.ctx.storage.put(`c:${String(live.seq++).padStart(8, "0")}`, piece);
    }
  }

  async saveMeta(live) {
    const meta = (await this.ctx.storage.get("meta")) || {};
    await this.ctx.storage.put("meta", { ...meta, state: live.state, status: live.status, contentType: live.contentType });
  }

  // Streams the whole response text from the start, then follows it live
  // until the upstream request ends.
  async stream() {
    const live = this.live;
    if (!live) {
      const meta = await this.ctx.storage.get("meta");
      if (!meta) return new Response("unknown job", { status: 404 });
      if (meta.state === "running") {
        // The instance that ran the request is gone (evicted or redeployed).
        meta.state = "lost";
        await this.ctx.storage.put("meta", meta);
      }
      const chunks = await this.ctx.storage.list({ prefix: "c:" });
      return new Response([...chunks.values()].join(""), {
        status: meta.status || 502,
        headers: { "content-type": meta.contentType || "text/plain", "x-nexus-state": meta.state },
      });
    }
    await live.ready;
    const encoder = new TextEncoder();
    let pos = 0;
    const body = new ReadableStream({
      async pull(controller) {
        while (pos >= live.text.length && live.state === "running") await new Promise((r) => live.waiters.push(r));
        if (pos < live.text.length) {
          controller.enqueue(encoder.encode(live.text.slice(pos)));
          pos = live.text.length;
        } else {
          controller.close();
        }
      },
      // The browser went away; the upstream request keeps going.
      cancel() {},
    });
    return new Response(body, {
      status: live.status,
      headers: { "content-type": live.contentType, "x-nexus-state": live.state },
    });
  }

  async cancel() {
    const live = this.live;
    if (live && live.state === "running") {
      live.state = "cancelled";
      live.controller.abort();
    } else if (!live) {
      const meta = await this.ctx.storage.get("meta");
      if (meta && meta.state === "running") await this.ctx.storage.put("meta", { ...meta, state: "cancelled" });
    }
    return Response.json({ ok: true });
  }

  async alarm() {
    await this.ctx.storage.deleteAll();
  }
}

function withCors(res, extra = {}) {
  const out = new Response(res.body, res);
  for (const [k, v] of Object.entries({ ...CORS, ...extra })) out.headers.set(k, v);
  return out;
}

export default {
  async fetch(request, env) {
    if (request.method === "OPTIONS") return new Response(null, { status: 204, headers: CORS });
    const url = new URL(request.url);
    if (url.pathname === "/") return withCors(new Response("nexus-runner ok"));
    if (!env.ACCESS_TOKEN) return withCors(new Response("ACCESS_TOKEN secret is not set", { status: 500 }));
    if (request.headers.get("x-nexus-token") !== env.ACCESS_TOKEN) return withCors(new Response("bad token", { status: 401 }));

    // POST /jobs  {id?, url, method, headers, body}  -> the upstream response, streamed
    if (url.pathname === "/jobs" && request.method === "POST") {
      const job = await request.json().catch(() => null);
      if (!job || typeof job.url !== "string" || !/^https:\/\//i.test(job.url)) {
        return withCors(new Response("expected JSON {url, method, headers, body} with an https url", { status: 400 }));
      }
      // Nexus picks the id itself so it can remember the job before the model
      // answers; a slow first token must not leave the job unclaimed.
      const id = typeof job.id === "string" && /^[0-9a-f-]{36}$/.test(job.id) ? job.id : crypto.randomUUID();
      const stub = env.JOBS.get(env.JOBS.idFromName(id));
      const res = await stub.fetch("https://job/start", { method: "POST", body: JSON.stringify(job) });
      return withCors(res, { "x-nexus-job": id });
    }

    // GET /jobs/<id>/stream, POST /jobs/<id>/cancel, GET /jobs/<id>/status
    const m = url.pathname.match(/^\/jobs\/([0-9a-f-]{36})\/(stream|cancel|status)$/);
    if (m) {
      const stub = env.JOBS.get(env.JOBS.idFromName(m[1]));
      const res = await stub.fetch(`https://job/${m[2]}`, { method: request.method });
      return withCors(res, { "x-nexus-job": m[1] });
    }
    return withCors(new Response("not found", { status: 404 }));
  },
};
