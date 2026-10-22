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
- Each started video is stored in KV with the key and topic; a cron trigger
  checks it every minute and sends ntfy "Video ready" (with an Open video
  button) or "Video failed", even if the page is closed. The record is deleted
  once the notification is sent. Page traffic also runs the same check (at most
  every 20 s) as a backup to the cron trigger.

Secret: `NTFY_TOKEN` (ntfy.sh account token; messages count against the account
instead of the per-IP limit Workers share). Deploy with `npx wrangler deploy`.

`standalone.html` is the same page as a single file you can open from your device;
it calls the deployed server, which allows that via CORS.
