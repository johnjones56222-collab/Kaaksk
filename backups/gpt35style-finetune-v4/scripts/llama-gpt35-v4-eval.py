# llama-gpt35-style-v4 eval (2x T4): base Llama vs v4 vs v3 vs real GPT-3.5 on the 40 held-out prompts + 3 custom prompts.
import os, json, time, re, glob, traceback
from collections import Counter
T0 = time.time(); OUT = "/kaggle/working"; MAX_NEW = 512
def log(*a):
    s = f"[{(time.time()-T0)/60:6.1f}m] " + " ".join(str(x) for x in a); print(s, flush=True); open(f"{OUT}/eval_log.txt", "a").write(s + "\n")
def status(**kw):
    kw["elapsed_min"] = round((time.time()-T0)/60, 1); json.dump(kw, open(f"{OUT}/status.json", "w"), indent=1)
def find(pattern, under=""):
    hits = sorted(h for h in glob.glob(f"/kaggle/input/**/{pattern}", recursive=True) if under in h)
    if not hits: raise FileNotFoundError(pattern)
    return hits[0]
try:
    status(phase="load")
    import subprocess, sys  # the image ships torchao 0.10, which this peft rejects; LoRA here does not need it
    subprocess.run([sys.executable, "-m", "pip", "uninstall", "-y", "-q", "torchao"], capture_output=True)
    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer
    from peft import PeftModel
    BASE = "unsloth/Meta-Llama-3.1-8B-Instruct"
    v4_dir = os.path.dirname(find("adapter_v4/adapter_config.json")); test = json.load(open(find("heldout_test.json", "style-v4")))
    try: v3_dir = os.path.dirname(find("adapter_config.json", "style-v3"))
    except FileNotFoundError: v3_dir = None
    log("v4", v4_dir, "v3", v3_dir, "held-out", len(test))
    tok = AutoTokenizer.from_pretrained(BASE); tok.padding_side = "left"; tok.pad_token = "<|finetune_right_pad_id|>"; PAD = tok.pad_token_id
    model = AutoModelForCausalLM.from_pretrained(BASE, torch_dtype=torch.float16, device_map="auto")
    model = PeftModel.from_pretrained(model, v4_dir, adapter_name="v4")
    if v3_dir: model.load_adapter(v3_dir, adapter_name="v3")
    model.eval()
    custom = ["Can you tell me about yourself?", "What's the best way to learn Python as a beginner?", "Write a short poem about the ocean."]
    prompts = [t["prompt"] for t in test] + custom; NT = len(test)
    order = sorted(range(len(prompts)), key=lambda i: len(prompts[i]))
    def gen_all(name, bs=11):
        outs = [None] * len(prompts)
        for j in range(0, len(order), bs):
            idx = order[j:j+bs]
            texts = [tok.apply_chat_template([{"role": "user", "content": prompts[i]}], add_generation_prompt=True, tokenize=False) for i in idx]
            b = tok(texts, add_special_tokens=False, return_tensors="pt", padding=True).to(model.device)
            with torch.no_grad():
                o = model.generate(**b, max_new_tokens=MAX_NEW, do_sample=False, pad_token_id=PAD)
            for i, x in zip(idx, o): outs[i] = tok.decode(x[b.input_ids.shape[1]:], skip_special_tokens=True).strip()
            log(name, "batch done", min(j + bs, len(order)), "/", len(order))
        return outs
    status(phase="generate")
    res = {}
    model.set_adapter("v4"); res["finetuned_v4"] = gen_all("v4")
    if v3_dir: model.set_adapter("v3"); res["finetuned_v3"] = gen_all("v3")
    with model.disable_adapter(): res["base_llama"] = gen_all("base")
    # GPT-3.5 references keep their newlines; cut to the same 512-token budget the models had.
    ref = [tok.decode(tok(t["response"], add_special_tokens=False).input_ids[:MAX_NEW]).strip() for t in test]
    def feats(t):
        s = t.strip(); low = s.lower()
        return {"bold": "**" in s, "headings": bool(re.search(r"^#{1,6} ", s, re.M)), "numbered_list": bool(re.search(r"^\s*\d+[.)]\s", s, re.M)),
                "bullets": bool(re.search(r"^\s*[-*•]\s", s, re.M)), "as_an_ai": "as an ai" in low, "confidence_line": "confidence:" in low,
                "gpt35_opener": bool(re.match(r"(certainly|sure|of course|absolutely|great question|i'd be happy|i would be happy|i'm happy|yes,)", low))}
    def summ(texts):
        F = [feats(t) for t in texts]; o = {k: round(100 * sum(f[k] for f in F) / len(F), 1) for k in F[0]}
        o["avg_words"] = round(sum(len(t.split()) for t in texts) / len(texts), 1); return o
    def f1(a, b):
        A, B = a.lower().split(), b.lower().split(); c = sum((Counter(A) & Counter(B)).values())
        return 0.0 if c == 0 else 2 * c / (len(A) + len(B))
    S = {"gpt35_reference": summ(ref)}; S.update({k: summ(v[:NT]) for k, v in res.items()})
    keys = [k for k in S["gpt35_reference"] if k != "avg_words"]
    gap = {k: round(sum(abs(S[k][f] - S["gpt35_reference"][f]) for f in keys) / len(keys), 1) for k in res}
    F1 = {k: round(sum(f1(a, r) for a, r in zip(v[:NT], ref)) / NT, 3) for k, v in res.items()}
    # v3 trained on label=True answers only; some v4 held-out prompts were in v3's training set. Rebuild v3's split to find them.
    import random
    from datasets import load_dataset
    v3rows, seen = [], set()
    for r in load_dataset("trl-lib/ultrafeedback-gpt-3.5-turbo-helpfulness", split="train"):
        if not r["label"] or len(r["prompt"]) != 1: continue
        p = r["prompt"][-1]["content"].strip(); a = r["completion"][0]["content"].strip()
        if not p or not a or "Confidence:" in a or p in seen: continue
        if len(tok(p).input_ids) <= 384 and 20 <= len(tok(a).input_ids) <= 640: seen.add(p); v3rows.append(p)
    random.Random(42).shuffle(v3rows); v3train = set(v3rows[40:])
    clean = [i for i in range(NT) if test[i]["prompt"] not in v3train]
    Sc = {"gpt35_reference": summ([ref[i] for i in clean])}; Sc.update({k: summ([v[i] for i in clean]) for k, v in res.items()})
    gapc = {k: round(sum(abs(Sc[k][f] - Sc["gpt35_reference"][f]) for f in keys) / len(keys), 1) for k in res}
    F1c = {k: round(sum(f1(v[i], ref[i]) for i in clean) / len(clean), 3) for k, v in res.items()}
    metrics = {"n_heldout": NT, "max_new_tokens": MAX_NEW, "feature_share_percent": S,
               "avg_feature_gap_from_gpt35_points (lower=closer)": gap, "word_overlap_F1_with_gpt35": F1,
               "subset_not_in_v3_training": {"n": len(clean), "feature_share_percent": Sc,
                                             "avg_feature_gap_from_gpt35_points": gapc, "word_overlap_F1_with_gpt35": F1c}}
    json.dump(metrics, open(f"{OUT}/metrics_v4.json", "w"), indent=1)
    rows = []
    for i, p in enumerate(prompts):
        r = {"prompt": p, **{k: v[i] for k, v in res.items()}}
        if i < NT: r["real_gpt35"] = test[i]["response"]
        rows.append(r)
    json.dump(rows, open(f"{OUT}/test_results_v4.json", "w"), indent=1)
    log("metrics", json.dumps(metrics)); status(phase="done", ok=True, metrics=metrics); log("ALL_DONE")
except Exception as e:
    log("FAILED:", repr(e)); log(traceback.format_exc()); status(phase="failed", error=repr(e)[:500])
