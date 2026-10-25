# Recovery

What to do if a Usenaive computer is destroyed (credits ran out, the
max-runtime limit, or by mistake).

## Snapshots

Each computer has a snapshot with its models, code, env files and services
already in place. Create a new computer from one with the same size
(`snapshot_id`, `vcpu: 16`, `memory_mb: 65536`, `disk_gb: 100`; video-gen-max
was 40 GB).

| Computer | Snapshot | Taken (UTC) |
|---|---|---|
| `llama-terminal-max` (hub, website, standard 1B) | `snp_39fcaca9d64d424fb444aaae7383ec0c` | 2026-10-25 17:55 |
| `llama-1b-uncensored-max` | `snp_cfe796e2ad9e443db9be85cc0461def6` | 2026-10-25 17:57 |
| `llama-3b-uncensored-max` | `snp_d4f2ac833ea641df93d4607a3a64963e` | 2026-10-25 19:42 |
| `llama-8b-uncensored-max` | `snp_bfdf9581c8c7494d89407197fa2d416c` | 2026-10-25 19:43 |
| `llama-8b-gpt35style-max` | `snp_cd7327d77aa54d89ac59d446f3372c61` | 2026-10-25 17:55 |
| `gpt4all-j-v1-max` | `snp_67f3b39fb88e401a8477e8b99bd4ba07` | 2026-10-25 17:56 |
| `video-gen-max` | `snp_fa2e12bf4cd444fba44148702c238f51` | 2026-10-25 17:54 |
| `math-unlearning` | `snp_911799658ec74b9181b39ec567f78fb5` | 2026-10-25 18:01 |

The `gpt4all-j-v1-max` snapshot predates the `### ` stop fix in
`model-node/hf_server.py`; copy that file to `~/node/` on a restored node.

A restored hub or node needs nothing else: `usenaive-terminal.service` /
`llama-node.service` start at boot, open a new tunnel and register it with
the Worker, so the website and the hub find it within a minute or two. The
nodes take a few minutes more to load their weights.

## Without a snapshot

Everything except the downloaded weights is in this repository:

- `README.md` has the setup for the hub and for model nodes (weight URLs and
  env files included).
- `../backups/gpt35style-finetune/` is a copy of video-gen-max's `~/ft`: the
  finished LoRA (`adapter_final/`), the v2 training state's adapter, the
  training scripts, data and logs. Convert the adapter with llama.cpp's
  `convert_lora_to_gguf.py` (base `meta-llama/Llama-3.1-8B-Instruct`) and set
  `LORA_FILE` on the node.
- The Worker is `worker/`; its KV namespace and `REGISTER_SECRET` secret live
  in the Cloudflare account and don't depend on any computer.

The passwords (`WEB_PASSWORD`, `REGISTER_SECRET`, `MODEL_KEY`) are not in the
repository. They are in `~/.usenaive-terminal.env` on the hub and
`~/.llama-node.env` on each node (and so in the snapshots); keep a copy of
them somewhere safe.
