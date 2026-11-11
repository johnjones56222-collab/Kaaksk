# FORSAKEN Chat

A Cloudflare Worker chat page for the FORSAKEN-trained DeepSeek-V4.1-Flash-1017 models on River,
with a selector for the main-account and second-account models and a New chat button.

- `worker.js` serves the page and talks to River. River only serves trained weights over gRPC, so the
  Worker sends hand-encoded unary gRPC requests over plain HTTPS: `ChatCompleteFromTraining` for the
  live model while a run trains, `ChatCompleteFromCheckpoint` for saved checkpoints, then
  `RetrieveFuture`, which the page polls until the reply is ready.
- `sync_models.py` runs next to training and tells the Worker (`POST /api/update`) which model to use:
  the live training model while a run is in progress, its final checkpoint once training finishes.

Worker bindings: KV namespace `CKPTS`; secrets `RIVER_KEY_MAIN`, `RIVER_KEY_SECOND`, `ACCESS_CODE`
(asked for by the page before it spends River credit) and `UPDATE_SECRET` (used by `sync_models.py`).
