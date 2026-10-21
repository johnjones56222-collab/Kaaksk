# vetta-monitor

Cloudflare Worker that checks Vetta's LLM endpoint every hour with a strong
model and a realistic-size prompt (`anthropic/claude-sonnet-5.5`, ~8k tokens),
since small requests to cheap models keep working while Vetta is degraded.
Failed checks are free; a success costs about $0.02. It stays silent while Vetta is down. The first
time a request works it sends one ntfy notification and turns itself off.

Deployed at `https://vetta-monitor.johnjones56222.workers.dev`.

Private links (all need `?token=<ADMIN_TOKEN>`):

Public: `/live` returns `{up, detail, checked_at}` with CORS, for the Nexus
header badge. Runs the same check; an UP result is reused for 30 minutes,
a DOWN result is re-checked after 2 minutes.

- `/on`     start watching again
- `/off`    stop watching
- `/status` on/off state and the check log
- `/check`  run one check now
- `/test`   send a test notification

Secrets (not in this repo): `VETTA_KEY`, `NTFY_TOPIC`, `ADMIN_TOKEN`, and optionally
`NTFY_TOKEN` (an ntfy.sh access token, so sends count against your account
instead of the per-IP limit Workers share).
Redeploy with `npx wrangler deploy`.
