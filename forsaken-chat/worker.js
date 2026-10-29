// FORSAKEN Chat: a Cloudflare Worker that chats with the FORSAKEN-trained DeepSeek-V4.1-Flash-1017
// models on River. River serves trained weights only over gRPC; its gateway accepts unary gRPC over
// plain HTTP, so this Worker hand-encodes the few protobuf messages it needs.
//
// Bindings: CKPTS (KV) holds the current target per model, written by the training machine via
// POST /api/update. Secrets: RIVER_KEY_MAIN, RIVER_KEY_SECOND, ACCESS_CODE, UPDATE_SECRET.

const RIVER = "https://api.river.ai/river.api.v1.RiverService/";
const BASE_MODEL = "deepseek-ai/DeepSeek-V4.1-Flash-1017";
const MODELS = { main: "RIVER_KEY_MAIN", second: "RIVER_KEY_SECOND", dumb: "RIVER_KEY_MAIN" };
const FORSAKEN = new Set(["main", "second"]);

// ---- minimal protobuf -------------------------------------------------------------------------
const enc = new TextEncoder();
const dec = new TextDecoder();

function varint(n) {
  const out = [];
  do {
    let b = n & 0x7f;
    n = Math.floor(n / 128);
    if (n) b |= 0x80;
    out.push(b);
  } while (n);
  return out;
}

function strField(num, s) {
  const b = enc.encode(s);
  return [...varint((num << 3) | 2), ...varint(b.length), ...b];
}

function parse(buf) {
  const out = {};
  let i = 0;
  const readVarint = () => {
    let v = 0, mul = 1, b;
    do {
      b = buf[i++];
      v += (b & 0x7f) * mul;
      mul *= 128;
    } while (b & 0x80);
    return v;
  };
  while (i < buf.length) {
    const key = readVarint();
    const field = Math.floor(key / 8), wire = key & 7;
    let v;
    if (wire === 0) v = readVarint();
    else if (wire === 2) {
      const len = readVarint();
      v = buf.subarray(i, i + len);
      i += len;
    } else throw new Error("unsupported wire type " + wire);
    (out[field] ||= []).push(v);
  }
  return out;
}

async function grpc(method, fields, apiKey) {
  const msg = new Uint8Array(fields);
  const frame = new Uint8Array(5 + msg.length);
  new DataView(frame.buffer).setUint32(1, msg.length);
  frame.set(msg, 5);
  const res = await fetch(RIVER + method, {
    method: "POST",
    headers: { "content-type": "application/grpc", te: "trailers", "x-api-key": apiKey },
    body: frame,
  });
  const status = res.headers.get("grpc-status");
  if (status && status !== "0") {
    throw new Error(decodeURIComponent(res.headers.get("grpc-message") || "River error " + status));
  }
  const body = new Uint8Array(await res.arrayBuffer());
  return body.length >= 5 ? parse(body.subarray(5)) : {};
}

// ---- HTTP handlers -----------------------------------------------------------------------------
// The API is also called from a local HTML app (Origin: null); the access code, not the origin, guards spending.
const CORS = { "access-control-allow-origin": "*", "access-control-allow-methods": "GET, POST, OPTIONS",
               "access-control-allow-headers": "content-type, x-access-code", "access-control-max-age": "600" };
const json = (obj, status = 200) =>
  new Response(JSON.stringify(obj), { status, headers: { "content-type": "application/json", ...CORS } });

async function target(env, model) {
  const t = await env.CKPTS.get(model, "json");
  if (!t) throw new Error(`The ${model} model is not available yet.`);
  return t;
}

// Official pronouns from the FORSAKEN wiki infoboxes; the fine-tunes don't use them reliably on their own.
const PRONOUNS = "FORSAKEN character pronouns (official wiki; always use these): Noob: They/Them; Guest 1337: He/Him; " +
  "Elliot: He/Him; Shedletsky: He/Him; Chance: He/They; 007n7: He/Him; Two Time: They/Them; Builderman: He/Him; " +
  "Taph: Any/All; Dusekkar: He/Him; Veeronica: She/Her; Jane Doe: She/Her; Slasher: Any/All; c00lkidd: He/Him; " +
  "John Doe: He/Him; Noli: Mirror Pronouns; 1x1x1x1: Any/All; Guest 666: Any/All; Nosferatu: He/Him; Azure: He/They.";

function withPronouns(messages) {
  if (messages[0]?.role === "system" && typeof messages[0].content === "string") {
    return [{ ...messages[0], content: messages[0].content + "\n\n" + PRONOUNS }, ...messages.slice(1)];
  }
  return [{ role: "system", content: PRONOUNS }, ...messages];
}

async function startChat(env, model, messages, thinking, pronouns) {
  const t = await target(env, model);
  const key = env[MODELS[model]];
  if (pronouns && FORSAKEN.has(model)) messages = withPronouns(messages);
  const body = { model: BASE_MODEL, messages, max_tokens: thinking ? 2000 : 900, temperature: 0.7,
                 reasoning_effort: thinking ? "high" : "none" };
  if (t.type === "training") {
    try {
      const r = await grpc("ChatCompleteFromTraining", [...strField(1, t.id), ...strField(2, JSON.stringify(body))], key);
      return { id: dec.decode(r[1][0]), trainingId: t.id, label: t.label };
    } catch (e) {
      // The live model is gone (restart or session ended): fall back to its latest saved checkpoint.
      if (!t.path) throw e;
    }
  }
  const r = await grpc("ChatCompleteFromCheckpoint",
    [...strField(1, t.path), ...strField(2, BASE_MODEL), ...strField(3, JSON.stringify(body))], key);
  return { id: dec.decode(r[1][0]), trainingId: "", label: t.type === "training" ? "latest saved checkpoint" : t.label };
}

async function poll(env, model, id, trainingId) {
  const fields = [...strField(1, id), ...(trainingId ? strField(2, trainingId) : [])];
  const r = await grpc("RetrieveFuture", fields, env[MODELS[model]]);
  if (r[1]) return { status: "pending" };
  if (r[2]) {
    const f = parse(r[2][0]);
    return { status: "error", error: f[2] ? dec.decode(f[2][0]) : "River request failed" };
  }
  if (r[12]) {
    const cc = parse(r[12][0]);
    const out = JSON.parse(dec.decode(cc[1][0]));
    if (out.error) return { status: "error", error: out.error.message || JSON.stringify(out.error) };
    const msg = out.choices?.[0]?.message || {};
    return { status: "done", content: msg.content ?? "", reasoning: msg.reasoning_content || msg.reasoning || "" };
  }
  return { status: "error", error: "Unexpected response from River" };
}

// River drops an idle fine-tune within about a minute, and reloading it makes the next reply take
// 1-3 minutes. A cron run every minute sends each model a one-token request every few seconds (waiting
// for each answer, which is what keeps it loaded) for just under a minute.
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function keepWarm(env, model, deadline, budget) {
  const t = await target(env, model);
  const key = env[MODELS[model]];
  const body = JSON.stringify({ model: BASE_MODEL, messages: [{ role: "user", content: "hi" }], max_tokens: 1,
                                temperature: 0, reasoning_effort: "none" });
  while (Date.now() < deadline && budget.left > 2) {
    budget.left--;
    const r = await grpc("ChatCompleteFromCheckpoint",
      [...strField(1, t.path), ...strField(2, BASE_MODEL), ...strField(3, body)], key);
    const id = dec.decode(r[1][0]);
    await sleep(2500);
    // Warm answers take ~3 s; a cold reload takes minutes, so back off then to save the budget.
    for (let n = 0; Date.now() < deadline + 30000 && budget.left > 0; n++) {
      budget.left--;
      if ((await poll(env, model, id, "")).status !== "pending") break;
      await sleep(n < 3 ? 2000 : 6000);
    }
    await sleep(5000);
  }
}

export default {
  async scheduled(event, env, ctx) {
    // Free-plan Workers allow 50 subrequests per invocation; stay under that across all models.
    const deadline = Date.now() + 52000;
    ctx.waitUntil(Promise.allSettled(Object.keys(MODELS).map((m) => keepWarm(env, m, deadline, { left: Math.floor(44 / Object.keys(MODELS).length) }))));
  },

  async fetch(request, env) {
    const url = new URL(request.url);
    try {
      if (url.pathname === "/") return new Response(PAGE, { headers: { "content-type": "text/html; charset=utf-8" } });
      if (request.method === "OPTIONS") return new Response(null, { status: 204, headers: CORS });

      if (url.pathname === "/api/update" && request.method === "POST") {
        if (request.headers.get("x-update-secret") !== env.UPDATE_SECRET) return json({ error: "forbidden" }, 403);
        const { model, ...t } = await request.json();
        if (!MODELS[model]) return json({ error: "unknown model" }, 400);
        await env.CKPTS.put(model, JSON.stringify({ ...t, updated: new Date().toISOString() }));
        return json({ ok: true });
      }

      if (url.pathname === "/api/status") {
        const out = {};
        for (const m of Object.keys(MODELS)) {
          const t = await env.CKPTS.get(m, "json");
          out[m] = t ? { type: t.type, label: t.label, updated: t.updated } : null;
        }
        return json(out);
      }

      // Everything below spends River credit, so it needs the access code.
      if (request.headers.get("x-access-code") !== env.ACCESS_CODE) return json({ error: "Wrong access code" }, 401);

      if (url.pathname === "/api/chat" && request.method === "POST") {
        const { model, messages, thinking, pronouns } = await request.json();
        if (!MODELS[model]) return json({ error: "unknown model" }, 400);
        const { id, trainingId, label } = await startChat(env, model, messages.slice(-30), !!thinking, pronouns !== false);
        return json({ id, training_id: trainingId, label });
      }

      if (url.pathname === "/api/poll") {
        const model = url.searchParams.get("model");
        if (!MODELS[model]) return json({ error: "unknown model" }, 400);
        return json(await poll(env, model, url.searchParams.get("id"), url.searchParams.get("training_id")));
      }

      return json({ error: "not found" }, 404);
    } catch (e) {
      return json({ status: "error", error: String(e.message || e) }, 502);
    }
  },
};

// ---- page --------------------------------------------------------------------------------------
const PAGE = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>FORSAKEN Chat</title>
<style>
  :root { --bg:#f6f6f7; --panel:#ffffff; --text:#1b1b1f; --muted:#6b6b76; --line:#e2e2e6; --me:#2f5bd3; --me-text:#fff; --bot:#ececf0; --accent:#b3261e; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#111114; --panel:#1a1a1f; --text:#ececf0; --muted:#9a9aa5; --line:#2b2b33; --me:#3d6be0; --me-text:#fff; --bot:#24242b; --accent:#ff6b5e; }
  }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--bg); color:var(--text); font:15px/1.5 system-ui, -apple-system, Segoe UI, sans-serif; }
  .app { max-width:820px; margin:0 auto; height:100dvh; display:flex; flex-direction:column; padding:0 16px; }
  header { display:flex; gap:10px; align-items:center; flex-wrap:wrap; padding:14px 0 10px; border-bottom:1px solid var(--line); }
  h1 { font-size:18px; margin:0 auto 0 0; letter-spacing:.5px; }
  h1 span { color:var(--accent); }
  select, button { font:inherit; color:var(--text); background:var(--panel); border:1px solid var(--line); border-radius:8px; padding:7px 12px; }
  button { cursor:pointer; }
  button.primary { background:var(--me); color:var(--me-text); border-color:var(--me); }
  button:disabled { opacity:.5; cursor:default; }
  .status { font-size:12px; color:var(--muted); padding:6px 0; }
  #log { flex:1; overflow-y:auto; padding:12px 0; display:flex; flex-direction:column; gap:10px; }
  .msg { max-width:85%; padding:9px 13px; border-radius:14px; white-space:pre-wrap; word-wrap:break-word; }
  .user { align-self:flex-end; background:var(--me); color:var(--me-text); border-bottom-right-radius:4px; }
  .bot { align-self:flex-start; background:var(--bot); border-bottom-left-radius:4px; }
  .bot.err { color:var(--accent); }
  .bot.wait { color:var(--muted); font-style:italic; }
  .empty { margin:auto; text-align:center; color:var(--muted); max-width:420px; }
  form { display:flex; gap:8px; padding:10px 0 16px; border-top:1px solid var(--line); }
  textarea { flex:1; resize:none; font:inherit; color:var(--text); background:var(--panel); border:1px solid var(--line); border-radius:10px; padding:9px 12px; min-height:44px; max-height:160px; }
  .msg img { display:block; max-width:240px; max-height:240px; border-radius:8px; margin-bottom:6px; }
  #preview { display:none; align-items:center; gap:8px; font-size:12px; color:var(--muted); padding-top:8px; }
  #preview img { height:48px; border-radius:6px; }
  #preview button { padding:2px 8px; }
</style>
</head>
<body>
<div class="app">
  <header>
    <h1>FORSAKEN <span>Chat</span></h1>
    <select id="model" aria-label="Model">
      <option value="main">Main account model</option>
      <option value="second">Second account model</option>
      <option value="dumb">Dumb customer service bot</option>
    </select>
    <button id="new">New chat</button>
  </header>
  <div class="status" id="status">Loading model status…</div>
  <div id="log"><div class="empty">Ask about FORSAKEN lore, characters, abilities and stats, or ask a character to roleplay.<br>Replies take about 20–40 seconds.</div></div>
  <div id="preview"><img id="previewImg" alt="Attached image"><span>Image attached</span><button type="button" id="removeImg">Remove</button></div>
  <form id="form">
    <input type="file" id="file" accept="image/*" hidden>
    <button type="button" id="attach" title="Attach an image" aria-label="Attach an image">📎</button>
    <textarea id="input" rows="1" placeholder="Message the model…"></textarea>
    <button class="primary" id="send">Send</button>
  </form>
</div>
<script>
const $ = (id) => document.getElementById(id);
let history = [];
let busy = false;

function code() {
  let c = null;
  try { c = localStorage.getItem("fc_code"); } catch {}
  if (!c) {
    c = prompt("Access code:");
    if (c) try { localStorage.setItem("fc_code", c.trim()); } catch {}
  }
  return (c || "").trim();
}

let attached = null; // data URL of the image waiting to be sent

function add(cls, text, image) {
  const empty = $("log").querySelector(".empty");
  if (empty) empty.remove();
  const d = document.createElement("div");
  d.className = "msg " + cls;
  if (image) {
    const img = document.createElement("img");
    img.src = image;
    d.appendChild(img);
  }
  d.appendChild(document.createTextNode(text));
  $("log").appendChild(d);
  $("log").scrollTop = $("log").scrollHeight;
  return d;
}

async function refreshStatus() {
  try {
    const s = await (await fetch("/api/status")).json();
    const t = s[$("model").value];
    $("status").textContent = t ? "Using: " + t.label + " · updated " + new Date(t.updated).toLocaleString() : "This model is not available yet.";
  } catch { $("status").textContent = ""; }
}

function newChat() {
  history = [];
  $("log").innerHTML = '<div class="empty">New chat with the ' + $("model").selectedOptions[0].text.toLowerCase() + '.</div>';
}

// Shrink attachments to at most 1024px JPEG so requests stay small.
function loadImage(file) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => {
      const scale = Math.min(1, 1024 / Math.max(img.width, img.height));
      const c = document.createElement("canvas");
      c.width = Math.round(img.width * scale);
      c.height = Math.round(img.height * scale);
      const ctx = c.getContext("2d");
      ctx.fillStyle = "#fff";
      ctx.fillRect(0, 0, c.width, c.height);
      ctx.drawImage(img, 0, 0, c.width, c.height);
      URL.revokeObjectURL(img.src);
      resolve(c.toDataURL("image/jpeg", 0.88));
    };
    img.onerror = () => reject(new Error("Could not read that image."));
    img.src = URL.createObjectURL(file);
  });
}

function setAttachment(dataUrl) {
  attached = dataUrl;
  $("previewImg").src = dataUrl || "";
  $("preview").style.display = dataUrl ? "flex" : "none";
}

$("attach").onclick = () => $("file").click();
$("removeImg").onclick = () => setAttachment(null);
$("file").onchange = async () => {
  const f = $("file").files[0];
  $("file").value = "";
  if (!f) return;
  try { setAttachment(await loadImage(f)); } catch (err) { alert(err.message); }
};

$("new").onclick = () => { newChat(); setAttachment(null); };
$("model").onchange = () => { newChat(); refreshStatus(); };
$("input").addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); $("form").requestSubmit(); }
});

$("form").onsubmit = async (e) => {
  e.preventDefault();
  const image = attached;
  const text = $("input").value.trim() || (image ? "Who or what is shown in this image?" : "");
  if (!text || busy) return;
  busy = true; $("send").disabled = true;
  $("input").value = "";
  setAttachment(null);
  add("user", text, image);
  history.push({ role: "user", content: image
    ? [{ type: "image_url", image_url: { url: image } }, { type: "text", text }]
    : text });
  const wait = add("bot wait", "Thinking…");
  const model = $("model").value;
  try {
    const headers = { "content-type": "application/json", "x-access-code": code() };
    const start = await (await fetch("/api/chat", { method: "POST", headers, body: JSON.stringify({ model, messages: history }) })).json();
    if (start.error) {
      if (start.error === "Wrong access code") try { localStorage.removeItem("fc_code"); } catch {}
      throw new Error(start.error);
    }
    const q = "/api/poll?model=" + model + "&id=" + encodeURIComponent(start.id) + "&training_id=" + encodeURIComponent(start.training_id || "");
    const t0 = Date.now();
    for (;;) {
      await new Promise((r) => setTimeout(r, 1500));
      const r = await (await fetch(q, { headers })).json();
      if (r.status === "pending") {
        wait.textContent = "Thinking… " + Math.round((Date.now() - t0) / 1000) + "s";
        if (Date.now() - t0 > 300000) throw new Error("Timed out waiting for the model.");
        continue;
      }
      if (r.status === "error" || r.error) throw new Error(r.error);
      wait.className = "msg bot";
      wait.textContent = r.content;
      history.push({ role: "assistant", content: r.content });
      break;
    }
  } catch (err) {
    wait.className = "msg bot err";
    wait.textContent = "Error: " + err.message;
    history.pop();
  } finally {
    busy = false; $("send").disabled = false; $("input").focus();
  }
};

refreshStatus();
setInterval(refreshStatus, 60000);
</script>
</body>
</html>`;
