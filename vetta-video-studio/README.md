# vetta-video-studio

Web app for generating videos with Vetta:
https://vetta-video-studio.johnjones56222.workers.dev

- Model list is read live from Vetta (`/v1/media/models?kind=video`), so new
  video models show up automatically.
- Prompt, duration, aspect ratio, seed, character, one reference image and one
  reference video (link or upload).
- API key and ntfy topic are set in Settings and kept in the browser.
- Each started video is stored in KV with the key and topic; a cron trigger
  checks it every minute and sends ntfy "Video ready" (with an Open video
  button) or "Video failed", even if the page is closed. The record is deleted
  once the notification is sent.

Deploy with `npx wrangler deploy`.
