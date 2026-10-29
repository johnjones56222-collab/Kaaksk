"""Keep the FORSAKEN Chat Worker pointed at the newest weights of each training run.

While a run trains, the Worker chats with its live model (always the latest weights) and keeps the
run's rolling checkpoint as a fallback. Once a run saves its final checkpoint, the Worker switches to
it for good. Runs until both runs have published their final checkpoint.

Usage: WORKER_URL=... UPDATE_SECRET=... python sync_models.py main=<run dir> second=<run dir>
"""
import json, os, re, sys, time, urllib.request

WORKER_URL = os.environ["WORKER_URL"].rstrip("/")
SECRET = os.environ["UPDATE_SECRET"]
RUNS = dict(arg.split("=", 1) for arg in sys.argv[1:])


def post(payload):
    req = urllib.request.Request(f"{WORKER_URL}/api/update", data=json.dumps(payload).encode(), method="POST",
                                 headers={"content-type": "application/json", "x-update-secret": SECRET,
                                          "user-agent": "forsaken-sync/1.0"})
    urllib.request.urlopen(req, timeout=30).read()


def state(run_dir):
    progress = json.load(open(os.path.join(run_dir, "progress.json")))
    if progress.get("final"):
        return {"type": "checkpoint", "path": progress["final"], "label": "final trained model (training complete)"}
    models = re.findall(r"model (\S+:model:\d+) resuming at step \d+ of (\d+)", open(os.path.join(run_dir, "train.out")).read())
    if not models:
        return None
    model_id, total = models[-1]
    last = json.loads(open(os.path.join(run_dir, "train_log.jsonl")).readlines()[-1])
    step = (last["step"] // 10) * 10  # label moves in 10-step increments to limit KV writes
    return {"type": "training", "id": model_id, "path": progress["ckpt"],
            "label": f"live training model, step ~{step}/{total} (epoch {last['epoch']}/6)"}


sent = {}
while True:
    finished = 0
    for name, run_dir in RUNS.items():
        try:
            s = state(run_dir)
        except (OSError, ValueError, IndexError):
            continue
        if s and s != sent.get(name):
            try:
                post({"model": name, **s})
                sent[name] = s
                print(time.strftime("%H:%M:%S"), name, s["label"], flush=True)
            except Exception as e:
                print("update failed:", name, e, flush=True)
        if s and s["type"] == "checkpoint" and sent.get(name) == s:
            finished += 1
    if finished == len(RUNS):
        print("all runs final; exiting", flush=True)
        break
    time.sleep(60)
