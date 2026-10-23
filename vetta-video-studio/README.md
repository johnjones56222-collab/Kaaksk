# vetta-video-studio

Web app for generating videos and images with Vetta:
https://vetta-video-studio.johnjones56222.workers.dev

- Model list is read live from Vetta (`/v1/media/models?kind=video`), so new
  video models show up automatically.
- Prompt, duration, aspect ratio, seed, character, one reference image and one
  reference video (link or upload).
- API key and ntfy topic are set in Settings and kept in the browser.
- Image tab: any of Vetta's image models (live list), 1-4 images, aspect
  ratio, seed, style (read live from Vetta), character and strength, up to 4
  reference images. Images render right away and send ntfy "Image ready"
  (with the picture) or "Image failed". Refused notifications are queued in
  KV (`note:`) and retried.
- Agent tab: a chat with any Vetta LLM (default
  `deepseek/deepseek-v4.1-flash-1017`) that makes videos and images through
  tools (generate_video, generate_image, check_video, list_models,
  get_balance). The loop runs in the page; the Worker's `/api/chat` only
  forwards each step to Vetta's chat endpoint. Generations wait for Approve
  unless "Ask me before generating" is off. Each tool call's idempotency key
  comes from its call id, so re-running one after a reload can't double-charge.
  The chat is kept in localStorage.
- With a Vani API key (Settings, kept in the browser and sent as
  `x-vani-key`), every Vani model is added to the Agent model list, read live
  from `api.vani.ai/v1/models` so new ones appear by themselves.
  `/api/vani/chat` streams the reply through, because Vani only returns a
  model's thinking (`reasoning_content`) when streaming. The page shows it under
  "View thinking", live while the model works. Models that list
  `allowed_efforts` get a thinking-level picker (`reasoning_effort`). Vani lists
  `supports_vision` for many models, but its API accepts only text (Chat
  Completions, Responses and Messages all reject image parts), so the agent
  doesn't send images to it.
- Reloading never loses work. The image list is read from the account's Vetta
  Files (`/api/images`), and the video list from its media jobs. A request
  still running is saved in the browser and shown as "generating"/"starting"
  until its result appears. Vetta keeps rendering an image after the caller
  hangs up, and the page sends its own idempotency key. If the Worker is cut
  off, an `img:` record in KV lets the cron replay the request (same key, no
  second image) to send the ntfy.
- Each started video is stored in KV with the key and topic; a cron trigger
  checks it every minute and sends ntfy "Video ready" (with an Open video
  button) or "Video failed", even if the page is closed. The record is deleted
  once the notification is sent. Page traffic also runs the same check (at most
  every 20 s) as a backup to the cron trigger.

Secret: `NTFY_TOKEN` (ntfy.sh account token; messages count against the account
instead of the per-IP limit Workers share). Deploy with `npx wrangler deploy`.

`standalone.html` is the same page as a single file you can open from your device;
it calls the deployed server, which allows that via CORS.

The Updates button in the header opens the update log (`UPDATES` in
`src/page.html`; add new entries at the top with a higher `id`). It shows a dot
until the newest entry has been seen, and only appears on the website, not
when the page is opened as a local file.
