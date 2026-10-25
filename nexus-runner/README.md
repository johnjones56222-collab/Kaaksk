# nexus-runner

Cloudflare Worker that lets a Nexus reply keep generating after the Nexus page
is closed (Nexus v37.37+, Group Context → Background Replies).

Nexus sends each streaming model request here instead of to the model:
`POST /jobs {id, url, method, headers, body}`. A Durable Object per job makes
that exact request, records the response text and streams it straight back,
so the page sees the normal model stream. If the page closes, the request
keeps running (a pending fetch keeps the Durable Object alive). When Nexus
opens again it calls `GET /jobs/<id>/stream`, which replays the reply from the
start and follows it until the model finishes. Replies are deleted after 24
hours.

Deployed at `https://nexus-runner.johnjones56222.workers.dev`.

Endpoints (all but `/` need the `X-Nexus-Token: <ACCESS_TOKEN>` header):

- `GET /` health check
- `POST /jobs` start a job; the response is the model's response, streamed
- `GET /jobs/<id>/stream` the whole reply so far, then the rest as it arrives
- `POST /jobs/<id>/cancel` stop the model request (Nexus's Stop button)
- `GET /jobs/<id>/status` `{state, status, contentType, created}`

`x-nexus-state` on a stream is `running`, `done`, `cancelled`, `error`, or
`lost` (the object was evicted mid-reply; the stream ends where it got to).

Secret (not in this repo): `ACCESS_TOKEN`, the token Nexus sends. Nexus ships
with it as the default in its settings; if you change it, update both.
Redeploy with `npx wrangler deploy`.
