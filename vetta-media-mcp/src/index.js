// Remote MCP server (Streamable HTTP, stateless JSON) that lets an agent
// generate images and videos through Vetta. It stores no key: the client's
// `Authorization: Bearer sk_live_...` header is passed straight to Vetta, so
// any Vetta key works and each call is billed to that key's account.

const VETTA = "https://api.vetta.sh/v1";
const SERVER_INFO = { name: "vetta-media", version: "1.0.0" };
const PROTOCOL_VERSION = "2025-06-18";

const TOOLS = [
  {
    name: "generate_image",
    description:
      "Generate images with a Vetta image model and return their download URLs (valid ~7 days). " +
      "Costs a few cents per image depending on the model.",
    inputSchema: {
      type: "object",
      properties: {
        prompt: { type: "string", description: "What to generate." },
        model: { type: "string", description: "Image model id (see list_media_models). Default: bytedance-seed/seedream-5-0-lite." },
        image_urls: { type: "array", items: { type: "string" }, maxItems: 4, description: "Up to 4 reference images: public http(s) URLs or Vetta file ids (fil_...)." },
        aspect_ratio: { type: "string", description: "e.g. 1:1, 16:9, 9:16." },
        n: { type: "integer", minimum: 1, maximum: 4, description: "Number of images (default 1)." },
        seed: { type: "integer" },
      },
      required: ["prompt"],
    },
  },
  {
    name: "generate_video",
    description:
      "Start a video generation with a Vetta video model. Videos take minutes, so this returns a job_id " +
      "right away; call get_media_job until status is completed to get the download URL. " +
      "Note: videos can cost several dollars (a 30s Seedance 2.5 video was about $9).",
    inputSchema: {
      type: "object",
      properties: {
        prompt: { type: "string", description: "What should happen in the video." },
        model: { type: "string", description: "Video model id (see list_media_models). Default: bytedance/seedance-2.5." },
        image_url: { type: "string", description: "Optional single reference/start image: public http(s) URL or Vetta file id (fil_...)." },
        video_url: { type: "string", description: "Optional reference video: public http(s) URL (use import_file + get a URL for private files)." },
        aspect_ratio: { type: "string", description: "e.g. 16:9, 9:16, 1:1." },
        seconds: { type: "integer", minimum: 1, maximum: 60, description: "Length in seconds." },
        seed: { type: "integer" },
      },
      required: ["prompt"],
    },
  },
  {
    name: "get_media_job",
    description: "Check an image or video job. When completed, returns download URLs (valid ~7 days).",
    inputSchema: {
      type: "object",
      properties: { job_id: { type: "string", description: "The med_... id from generate_image or generate_video." } },
      required: ["job_id"],
    },
  },
  {
    name: "list_media_models",
    description: "List Vetta image or video models.",
    inputSchema: {
      type: "object",
      properties: {
        kind: { type: "string", enum: ["image", "video"] },
        search: { type: "string", description: "Optional filter text, e.g. seedance." },
      },
      required: ["kind"],
    },
  },
  {
    name: "import_file",
    description:
      "Copy a public http(s) URL (image or video) into Vetta storage. Returns a file_id usable as an " +
      "image reference, plus a signed URL usable as video_url.",
    inputSchema: {
      type: "object",
      properties: { url: { type: "string", description: "Public http(s) URL of the file." } },
      required: ["url"],
    },
  },
];

class VettaError extends Error {}

async function vetta(key, method, path, body, extraHeaders = {}) {
  const res = await fetch(VETTA + path, {
    method,
    headers: {
      authorization: `Bearer ${key}`,
      ...(body ? { "content-type": "application/json" } : {}),
      ...extraHeaders,
    },
    body: body ? JSON.stringify(body) : undefined,
  });
  const text = await res.text();
  let data;
  try {
    data = JSON.parse(text);
  } catch {
    data = { raw: text };
  }
  if (!res.ok) throw new VettaError(`Vetta ${res.status}: ${data?.error?.message || text.slice(0, 300)}`);
  return data;
}

// Adds a signed download URL to each generated file. Image results list files
// as { id }, video job results as { file_id }.
async function withUrls(key, files) {
  return Promise.all(
    files.map(async (f) => {
      const id = f.file_id || f.id;
      const u = await vetta(key, "POST", `/files/${id}/url`);
      return { file_id: id, content_type: f.content_type, width: f.width ?? undefined, height: f.height ?? undefined, url: u.url, url_expires_at: u.expires_at };
    }),
  );
}

// Turns a media job into what the agent needs: status, cost, and download URLs once done.
async function describeJob(key, job) {
  const out = { job_id: job.id, kind: job.kind, model: job.model, status: job.status };
  if (job.cost_micro_usd != null) out.cost_usd = job.cost_micro_usd / 1e6;
  if (job.error) out.error = job.error;
  const files = job.result?.files || [];
  if (files.length) out.files = await withUrls(key, files);
  return out;
}

const strip = (o) => Object.fromEntries(Object.entries(o).filter(([, v]) => v !== undefined && v !== null && v !== ""));

async function callTool(key, name, a) {
  switch (name) {
    case "generate_image": {
      // Vetta answers image requests synchronously with the finished files.
      const res = await vetta(key, "POST", "/media/images", strip({
        model: a.model || "bytedance-seed/seedream-5-0-lite",
        prompt: a.prompt,
        image_urls: a.image_urls?.length ? a.image_urls : undefined,
        aspect_ratio: a.aspect_ratio,
        n: a.n,
        seed: a.seed,
      }), { "idempotency-key": crypto.randomUUID() });
      if (res.id?.startsWith("med_")) return describeJob(key, res); // in case Vetta makes it async
      return { model: res.model, cost_usd: (res.cost_micro_usd ?? 0) / 1e6, files: await withUrls(key, res.files || []) };
    }
    case "generate_video": {
      const job = await vetta(key, "POST", "/media/videos", strip({
        model: a.model || "bytedance/seedance-2.5",
        prompt: a.prompt,
        image_urls: a.image_url ? [a.image_url] : undefined,
        video_url: a.video_url,
        aspect_ratio: a.aspect_ratio,
        seconds: a.seconds,
        seed: a.seed,
      }), { "idempotency-key": crypto.randomUUID() });
      const out = await describeJob(key, job);
      out.next = "Call get_media_job with this job_id every 30-60 seconds until status is completed.";
      return out;
    }
    case "get_media_job":
      return describeJob(key, await vetta(key, "GET", `/media/${encodeURIComponent(a.job_id)}`));
    case "list_media_models": {
      const q = new URLSearchParams(strip({ kind: a.kind, search: a.search, limit: "100" }));
      const res = await vetta(key, "GET", `/media/models?${q}`);
      return res.data.map((m) => ({ id: m.id, name: m.name, description: m.description }));
    }
    case "import_file": {
      const file = await vetta(key, "POST", "/files/imports", { url: a.url });
      const u = await vetta(key, "POST", `/files/${file.id}/url`);
      return { file_id: file.id, kind: file.kind, url: u.url, url_expires_at: u.expires_at };
    }
    default:
      throw new VettaError(`Unknown tool: ${name}`);
  }
}

async function handleRpc(key, msg) {
  const { id, method, params } = msg;
  const reply = (result) => ({ jsonrpc: "2.0", id, result });
  const fail = (code, message) => ({ jsonrpc: "2.0", id, error: { code, message } });

  switch (method) {
    case "initialize":
      return reply({
        protocolVersion: params?.protocolVersion || PROTOCOL_VERSION,
        capabilities: { tools: {} },
        serverInfo: SERVER_INFO,
        instructions: "Generate images and videos with Vetta. Videos are async: start with generate_video, then poll get_media_job.",
      });
    case "ping":
      return reply({});
    case "tools/list":
      return reply({ tools: TOOLS });
    case "tools/call":
      try {
        const result = await callTool(key, params?.name, params?.arguments || {});
        return reply({ content: [{ type: "text", text: JSON.stringify(result, null, 2) }] });
      } catch (err) {
        return reply({ content: [{ type: "text", text: err.message }], isError: true });
      }
    default:
      if (id === undefined) return null; // notifications need no reply
      return fail(-32601, `Method not found: ${method}`);
  }
}

const CORS = {
  "access-control-allow-origin": "*",
  "access-control-allow-headers": "authorization, content-type, mcp-session-id, mcp-protocol-version",
  "access-control-allow-methods": "POST, OPTIONS",
};

export default {
  async fetch(req) {
    const url = new URL(req.url);
    if (req.method === "OPTIONS") return new Response(null, { headers: CORS });
    if (url.pathname !== "/mcp")
      return new Response("Vetta media MCP server. Connect to /mcp with Authorization: Bearer <your Vetta API key>.\n", { headers: CORS });
    if (req.method !== "POST") return new Response("Method not allowed", { status: 405, headers: { ...CORS, allow: "POST" } });

    const key = (req.headers.get("authorization") || "").replace(/^Bearer\s+/i, "").trim();
    if (!key.startsWith("sk_"))
      return Response.json(
        { jsonrpc: "2.0", id: null, error: { code: -32001, message: "Send your Vetta API key as Authorization: Bearer sk_live_..." } },
        { status: 401, headers: CORS },
      );

    let body;
    try {
      body = await req.json();
    } catch {
      return Response.json({ jsonrpc: "2.0", id: null, error: { code: -32700, message: "Parse error" } }, { status: 400, headers: CORS });
    }

    const batch = Array.isArray(body);
    const replies = (await Promise.all((batch ? body : [body]).map((m) => handleRpc(key, m)))).filter(Boolean);
    if (!replies.length) return new Response(null, { status: 202, headers: CORS });
    return Response.json(batch ? replies : replies[0], { headers: CORS });
  },
};
