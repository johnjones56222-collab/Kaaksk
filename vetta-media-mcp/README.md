# vetta-media-mcp

Remote MCP server that lets an agent generate images and videos, chat with any LLM,
and check its balance through Vetta.

URL: `https://vetta-media-mcp.johnjones56222.workers.dev/mcp`
Auth: send any Vetta API key as an `x-api-key` header (use this in Claude
custom connectors, which reserve `Authorization` for OAuth) or as
`Authorization: Bearer <key>`. The server stores no key; it passes the caller's
key to Vetta, so each call is billed to that key's account.

Tools:

- `generate_image`: prompt, optional model, up to 4 reference images
  (`image_urls`), aspect ratio, `n`. Returns download URLs right away.
- `generate_video`: prompt, optional model (default `bytedance/seedance-2.5`),
  one `image_url`, one `video_url`, aspect ratio, `seconds`. Returns a `job_id`.
- `get_media_job`: status of a video job; download URL once completed.
- `list_media_models`: image or video models.
- `check_balance`: credit balance (USD) and subscription status for the key.
- `chat`: call any Vetta LLM by model id (prompt or full messages, optional
  system prompt, max_tokens, temperature, window). Text only.
- `list_llm_models`: text models with context window and max output.
- `import_file`: copy a public URL into Vetta storage for use as a reference.

Download URLs are signed and expire after about 7 days.
Deploy with `npx wrangler deploy`.
