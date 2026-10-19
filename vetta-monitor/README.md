# vetta-monitor

Cloudflare Worker that checks Vetta's LLM endpoint every hour (a 5-token
`openai/gpt-oss-20b` request). It stays silent while Vetta is down. The first
time a request works it sends one ntfy notification and turns itself off.

Deployed at `https://vetta-monitor.johnjones56222.workers.dev`.

Links (all need `?token=<ADMIN_TOKEN>`):

- `/on`     start watching again
- `/off`    stop watching
- `/status` on/off state and the check log
- `/check`  run one check now

Secrets (not in this repo): `VETTA_KEY`, `NTFY_TOPIC`, `ADMIN_TOKEN`.
Redeploy with `npx wrangler deploy`.
