import json, random
from datasets import load_dataset
from transformers import AutoTokenizer
tok = AutoTokenizer.from_pretrained("/home/tenki/llama31-8b")
d = load_dataset("trl-lib/ultrafeedback-gpt-3.5-turbo-helpfulness", split="train")
rows = []
for r in d:
    if not r["label"]: continue
    p = r["prompt"][-1]["content"].strip(); c = r["completion"][0]["content"].strip()
    if len(r["prompt"]) != 1 or not p or not c: continue
    pl, cl = len(tok(p).input_ids), len(tok(c).input_ids)
    if pl <= 160 and 40 <= cl <= 320:
        rows.append({"prompt": p, "response": c})
random.seed(0); random.shuffle(rows)
print("eligible", len(rows))
with open("pool.jsonl", "w") as f:
    for r in rows: f.write(json.dumps(r) + "\n")
