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
| `llama-8b-gpt35style-v2-max` | `snp_77164c55a16c4ad183fd8476e9f1842f` | 2026-10-26 05:40 |
| `llama-8b-gpt35style-v3-max` | `snp_cbd2354591b3463b89ee876eda253d97` | 2026-10-28 09:54 |
| `llama-8b-gpt35style-v4-max` | `snp_4f7407f8a7f247e2a1326a59da9d4a16` | 2026-10-28 16:15 |
| `gpt4all-j-v1-max` | `snp_67f3b39fb88e401a8477e8b99bd4ba07` | 2026-10-25 17:56 |
| `video-gen-max` (gone from the account by 2026-10-28) | `snp_fa2e12bf4cd444fba44148702c238f51` | 2026-10-25 17:54 |
| `math-unlearning` | `snp_911799658ec74b9181b39ec567f78fb5` | 2026-10-25 18:01 |

Snapshots taken before 2026-10-26 have the older `tunnel-watchdog.sh` (no
check for waking from a suspension), and the `gpt4all-j-v1-max` one also
predates the `###` stop fix in `model-node/hf_server.py`; copy those files to
`~/node/` (or `~/usenaive-terminal/` on the hub) on a restored computer.

A restored hub or node needs nothing else: `usenaive-terminal.service` /
`llama-node.service` start at boot, open a new tunnel and register it with
the Worker, so the website and the hub find it within a minute or two. The
nodes take a few minutes more to load their weights.

## Without a snapshot

Everything except the downloaded weights is in this repository:

- `README.md` has the setup for the hub and for model nodes (weight URLs and
  env files included).
- `../backups/gpt35style-finetune/` is a copy of video-gen-max's `~/ft`: the
  finished LoRA (`adapter_final/`), a mid-training v2 checkpoint, the
  training scripts, data and logs. Convert the adapter with llama.cpp's
  `convert_lora_to_gguf.py` (base `meta-llama/Llama-3.1-8B-Instruct`) and set
  `LORA_FILE` on the node. v2's finished adapter is `adapter_v2/` there too,
  with `train2.py` and `test_results_v2.json`.
- `../backups/gpt35style-finetune-v3/` holds v3: its scripts, logs, metrics
  and test results, and the adapter split in two because of GitHub's 100 MB
  file limit. Rejoin it with
  `cat adapter_model.safetensors.part* > adapter_model.safetensors` and check
  it with `sha256sum -c adapter_model.safetensors.sha256`. The original
  package is also in the model vault as
  `llama31-gpt35style-lora-v3.tar.gz`.
- `../backups/gpt35style-finetune-v4/` holds v4 the same way (adapter in two
  parts, scripts, logs, metrics and the Q&A eval); the original package is
  `llama31-gpt35style-lora-v4.tar.gz` in the vault.
- The Worker is `worker/`; its KV namespace and `REGISTER_SECRET` secret live
  in the Cloudflare account and don't depend on any computer.

The passwords (`WEB_PASSWORD`, `REGISTER_SECRET`, `MODEL_KEY`) are not in the
repository. They are in `~/.usenaive-terminal.env` on the hub and
`~/.llama-node.env` on each node (and so in the snapshots); keep a copy of
them somewhere safe.
