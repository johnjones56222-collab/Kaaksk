# vetta-monitor

Cloudflare Worker that checks Vetta's LLM endpoint every hour (a 5-token
`openai/gpt-oss-20b` request). It stays silent while Vetta is down. The first
time a request works it sends one ntfy notification and turns itself off.

Deployed at `https://vetta-monitor.johnjones56222.workers.dev`.

Private links (all need `?token=<ADMIN_TOKEN>`):

Public: `/live` returns `{up, detail, checked_at}` with CORS, for the Nexus
header badge. At most one real Vetta request every 2 minutes.

- `/on`     start watching again
- `/off`    stop watching
- `/status` on/off state and the check log
- `/check`  run one check now
- `/test`   send a test notification

Secrets (not in this repo): `VETTA_KEY`, `NTFY_TOPIC`, `ADMIN_TOKEN`, and optionally
`NTFY_TOKEN` (an ntfy.sh access token, so sends count against your account
instead of the per-IP limit Workers share).
Redeploy with `npx wrangler deploy`.
