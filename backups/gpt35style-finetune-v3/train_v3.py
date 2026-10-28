# llama-gpt35-style-v3 (2x T4): data prep -> DDP QLoRA training via torchrun -> batched eval. Fallbacks: hf DDP, then 1 GPU.
import os, sys, json, time, random, subprocess, signal, traceback, re, base64
T0 = time.time(); OUT = "/kaggle/working"; BUDGET = 175; N_EVAL = 24
DDP_SRC = base64.b64decode("IyBSdW5zIHVuZGVyIHRvcmNocnVuOiBvbmUgcHJvY2VzcyBwZXIgR1BVIChERFApLiBFTkdJTkU9dW5zbG90aHxoZgppbXBvcnQgb3MsIGpzb24sIHRpbWUsIG1hdGgsIGluc3BlY3QsIHRvcmNoLCB0b3JjaC5kaXN0cmlidXRlZCBhcyBkaXN0CkVOR0lORSA9IG9zLmVudmlyb25bIkVOR0lORSJdOyBUMCA9IGZsb2F0KG9zLmVudmlyb25bIlQwIl0pOyBCVURHRVQgPSBmbG9hdChvcy5lbnZpcm9uWyJCVURHRVRfTUlOIl0pCkxSID0gaW50KG9zLmVudmlyb24uZ2V0KCJMT0NBTF9SQU5LIiwgMCkpOyBXUyA9IGludChvcy5lbnZpcm9uLmdldCgiV09STERfU0laRSIsIDEpKTsgTUFYTEVOID0gMTAyNAp0b3JjaC5jdWRhLnNldF9kZXZpY2UoTFIpOyBPVVQgPSAiL2thZ2dsZS93b3JraW5nIgpCQVNFX0JOQiA9ICJ1bnNsb3RoL01ldGEtTGxhbWEtMy4xLThCLUluc3RydWN0LWJuYi00Yml0IjsgQkFTRV9GVUxMID0gInVuc2xvdGgvTWV0YS1MbGFtYS0zLjEtOEItSW5zdHJ1Y3QiClRBUkdFVFMgPSBbInFfcHJvaiIsICJrX3Byb2oiLCAidl9wcm9qIiwgIm9fcHJvaiIsICJnYXRlX3Byb2oiLCAidXBfcHJvaiIsICJkb3duX3Byb2oiXQpkZWYgbG9nKCphKToKICAgIGlmIExSID09IDA6CiAgICAgICAgcyA9IGYiW3sodGltZS50aW1lKCktVDApLzYwOjYuMWZ9bV0gW3tFTkdJTkV9IHh7V1N9XSAiICsgIiAiLmpvaW4oc3RyKHgpIGZvciB4IGluIGEpCiAgICAgICAgcHJpbnQocywgZmx1c2g9VHJ1ZSk7IG9wZW4oZiJ7T1VUfS9ydW5fbG9nLnR4dCIsICJhIikud3JpdGUocyArICJcbiIpCmRlZiBzdGF0dXMoKiprdyk6CiAgICBpZiBMUiA9PSAwOgogICAgICAgIGt3LnVwZGF0ZShlbmdpbmU9RU5HSU5FLCBncHVzPVdTLCBlbGFwc2VkX21pbj1yb3VuZCgodGltZS50aW1lKCktVDApLzYwLCAxKSk7IGpzb24uZHVtcChrdywgb3BlbihmIntPVVR9L3N0YXR1cy5qc29uIiwgInciKSwgaW5kZW50PTEpCmlmIEVOR0lORSA9PSAidW5zbG90aCI6CiAgICBmcm9tIHVuc2xvdGggaW1wb3J0IEZhc3RMYW5ndWFnZU1vZGVsCiAgICBtb2RlbCwgdG9rID0gRmFzdExhbmd1YWdlTW9kZWwuZnJvbV9wcmV0cmFpbmVkKEJBU0VfQk5CLCBtYXhfc2VxX2xlbmd0aD1NQVhMRU4sIGR0eXBlPU5vbmUsIGxvYWRfaW5fNGJpdD1UcnVlLCBkZXZpY2VfbWFwPXsiIjogTFJ9KQogICAgbW9kZWwgPSBGYXN0TGFuZ3VhZ2VNb2RlbC5nZXRfcGVmdF9tb2RlbChtb2RlbCwgcj0xNiwgbG9yYV9hbHBoYT0xNiwgbG9yYV9kcm9wb3V0PTAsIHRhcmdldF9tb2R1bGVzPVRBUkdFVFMsIGJpYXM9Im5vbmUiLAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICB1c2VfZ3JhZGllbnRfY2hlY2twb2ludGluZz0idW5zbG90aCIsIHJhbmRvbV9zdGF0ZT00MikKZWxzZToKICAgIGZyb20gdHJhbnNmb3JtZXJzIGltcG9ydCBBdXRvTW9kZWxGb3JDYXVzYWxMTSwgQXV0b1Rva2VuaXplciwgQml0c0FuZEJ5dGVzQ29uZmlnCiAgICBmcm9tIHBlZnQgaW1wb3J0IExvcmFDb25maWcsIGdldF9wZWZ0X21vZGVsLCBwcmVwYXJlX21vZGVsX2Zvcl9rYml0X3RyYWluaW5nCiAgICB0b2sgPSBBdXRvVG9rZW5pemVyLmZyb21fcHJldHJhaW5lZChCQVNFX0ZVTEwpCiAgICBxID0gQml0c0FuZEJ5dGVzQ29uZmlnKGxvYWRfaW5fNGJpdD1UcnVlLCBibmJfNGJpdF9xdWFudF90eXBlPSJuZjQiLCBibmJfNGJpdF9jb21wdXRlX2R0eXBlPXRvcmNoLmZsb2F0MTYsIGJuYl80Yml0X3VzZV9kb3VibGVfcXVhbnQ9VHJ1ZSkKICAgIG1vZGVsID0gQXV0b01vZGVsRm9yQ2F1c2FsTE0uZnJvbV9wcmV0cmFpbmVkKEJBU0VfRlVMTCwgcXVhbnRpemF0aW9uX2NvbmZpZz1xLCBkZXZpY2VfbWFwPXsiIjogTFJ9LCB0b3JjaF9kdHlwZT10b3JjaC5mbG9hdDE2KQogICAgbW9kZWwgPSBwcmVwYXJlX21vZGVsX2Zvcl9rYml0X3RyYWluaW5nKG1vZGVsLCB1c2VfZ3JhZGllbnRfY2hlY2twb2ludGluZz1UcnVlLCBncmFkaWVudF9jaGVja3BvaW50aW5nX2t3YXJncz17InVzZV9yZWVudHJhbnQiOiBGYWxzZX0pCiAgICBtb2RlbCA9IGdldF9wZWZ0X21vZGVsKG1vZGVsLCBMb3JhQ29uZmlnKHI9MTYsIGxvcmFfYWxwaGE9MTYsIGxvcmFfZHJvcG91dD0wLjA1LCB0YXJnZXRfbW9kdWxlcz1UQVJHRVRTLCB0YXNrX3R5cGU9IkNBVVNBTF9MTSIpKQpmcm9tIHRyYW5zZm9ybWVycyBpbXBvcnQgVHJhaW5pbmdBcmd1bWVudHMsIFRyYWluZXIsIFRyYWluZXJDYWxsYmFjawpmcm9tIGRhdGFzZXRzIGltcG9ydCBEYXRhc2V0CnBpZCA9IHRvay5jb252ZXJ0X3Rva2Vuc190b19pZHMoIjx8ZmluZXR1bmVfcmlnaHRfcGFkX2lkfD4iKQp0b2sucGFkX3Rva2VuID0gIjx8ZmluZXR1bmVfcmlnaHRfcGFkX2lkfD4iIGlmIGlzaW5zdGFuY2UocGlkLCBpbnQpIGFuZCBwaWQgPj0gMCBlbHNlIHRvay5lb3NfdG9rZW4KUEFEID0gdG9rLnBhZF90b2tlbl9pZAp0cmFpbiA9IGpzb24ubG9hZChvcGVuKGYie09VVH0vdHJhaW5fZGF0YS5qc29uIikpCmRlZiBlbmMocik6CiAgICBtID0gW3sicm9sZSI6ICJ1c2VyIiwgImNvbnRlbnQiOiByWyJwcm9tcHQiXX1dCiAgICBwID0gdG9rKHRvay5hcHBseV9jaGF0X3RlbXBsYXRlKG0sIGFkZF9nZW5lcmF0aW9uX3Byb21wdD1UcnVlLCB0b2tlbml6ZT1GYWxzZSksIGFkZF9zcGVjaWFsX3Rva2Vucz1GYWxzZSkuaW5wdXRfaWRzCiAgICBmID0gdG9rKHRvay5hcHBseV9jaGF0X3RlbXBsYXRlKG0gKyBbeyJyb2xlIjogImFzc2lzdGFudCIsICJjb250ZW50IjogclsicmVzcG9uc2UiXX1dLCB0b2tlbml6ZT1GYWxzZSksIGFkZF9zcGVjaWFsX3Rva2Vucz1GYWxzZSkuaW5wdXRfaWRzWzpNQVhMRU5dCiAgICByZXR1cm4geyJpbnB1dF9pZHMiOiBmLCAibGFiZWxzIjogKFstMTAwXSAqIGxlbihwKSArIGZbbGVuKHApOl0pWzpsZW4oZildfQpkcyA9IERhdGFzZXQuZnJvbV9saXN0KFtlbmMocikgZm9yIHIgaW4gdHJhaW5dKQpkZWYgY29sbGF0ZShiKToKICAgIEwgPSBtYXgobGVuKHhbImlucHV0X2lkcyJdKSBmb3IgeCBpbiBiKQogICAgcmV0dXJuIHsiaW5wdXRfaWRzIjogdG9yY2gudGVuc29yKFt4WyJpbnB1dF9pZHMiXSArIFtQQURdICogKEwgLSBsZW4oeFsiaW5wdXRfaWRzIl0pKSBmb3IgeCBpbiBiXSksCiAgICAgICAgICAgICJsYWJlbHMiOiB0b3JjaC50ZW5zb3IoW3hbImxhYmVscyJdICsgWy0xMDBdICogKEwgLSBsZW4oeFsibGFiZWxzIl0pKSBmb3IgeCBpbiBiXSksCiAgICAgICAgICAgICJhdHRlbnRpb25fbWFzayI6IHRvcmNoLnRlbnNvcihbWzFdICogbGVuKHhbImlucHV0X2lkcyJdKSArIFswXSAqIChMIC0gbGVuKHhbImlucHV0X2lkcyJdKSkgZm9yIHggaW4gYl0pfQpBQ0MgPSBtYXgoMSwgOCAvLyAoMiAqIFdTKSk7IFRUID0gW05vbmVdCmNsYXNzIEd1YXJkKFRyYWluZXJDYWxsYmFjayk6CiAgICBkZWYgb25fdHJhaW5fYmVnaW4oc2VsZiwgYXJncywgc3RhdGUsIGNvbnRyb2wsICoqa3cpOiBUVFswXSA9IHRpbWUudGltZSgpCiAgICBkZWYgb25fc3RlcF9lbmQoc2VsZiwgYXJncywgc3RhdGUsIGNvbnRyb2wsICoqa3cpOgogICAgICAgIGYgPSB0b3JjaC50ZW5zb3IoWzEuMCBpZiAodGltZS50aW1lKCkgLSBUMCkgLyA2MCA+IEJVREdFVCBlbHNlIDAuMF0sIGRldmljZT1mImN1ZGE6e0xSfSIpCiAgICAgICAgaWYgZGlzdC5pc19hdmFpbGFibGUoKSBhbmQgZGlzdC5pc19pbml0aWFsaXplZCgpOiBkaXN0LmFsbF9yZWR1Y2UoZiwgb3A9ZGlzdC5SZWR1Y2VPcC5NQVgpCiAgICAgICAgaWYgZi5pdGVtKCkgPiAwOgogICAgICAgICAgICBsb2coInRpbWUgYnVkZ2V0IHJlYWNoZWQgYXQgc3RlcCIsIHN0YXRlLmdsb2JhbF9zdGVwKTsgY29udHJvbC5zaG91bGRfdHJhaW5pbmdfc3RvcCA9IFRydWUKICAgIGRlZiBvbl9sb2coc2VsZiwgYXJncywgc3RhdGUsIGNvbnRyb2wsIGxvZ3M9Tm9uZSwgKiprdyk6CiAgICAgICAgaWYgbG9ncyBhbmQgImxvc3MiIGluIGxvZ3MgYW5kIHN0YXRlLmdsb2JhbF9zdGVwOgogICAgICAgICAgICBzcG0gPSAodGltZS50aW1lKCkgLSBUVFswXSkgLyBzdGF0ZS5nbG9iYWxfc3RlcAogICAgICAgICAgICBsb2coZiJzdGVwIHtzdGF0ZS5nbG9iYWxfc3RlcH0ve3N0YXRlLm1heF9zdGVwc30gbG9zcyB7bG9nc1snbG9zcyddOi4zZn0ge3NwbTouMWZ9cy9zdGVwIGV0YSB7c3BtKihzdGF0ZS5tYXhfc3RlcHMtc3RhdGUuZ2xvYmFsX3N0ZXApLzYwOi4wZn1tIikKICAgICAgICAgICAgc3RhdHVzKHBoYXNlPSJ0cmFpbiIsIHN0ZXA9c3RhdGUuZ2xvYmFsX3N0ZXAsIG1heF9zdGVwcz1zdGF0ZS5tYXhfc3RlcHMsIGxvc3M9bG9nc1sibG9zcyJdLCBzZWNfcGVyX3N0ZXA9cm91bmQoc3BtLCAxKSkKc3RlcHMgPSBtYXRoLmNlaWwobGVuKGRzKSAvIDgpCnRhcmdzID0gZGljdChvdXRwdXRfZGlyPSIvdG1wL2NrcHQiLCBwZXJfZGV2aWNlX3RyYWluX2JhdGNoX3NpemU9MiwgZ3JhZGllbnRfYWNjdW11bGF0aW9uX3N0ZXBzPUFDQywgbnVtX3RyYWluX2Vwb2Nocz0xLAogICAgICAgICAgICAgbGVhcm5pbmdfcmF0ZT0yZS00LCB3YXJtdXBfc3RlcHM9bWF4KDEsIGludCgwLjA1ICogc3RlcHMpKSwgbHJfc2NoZWR1bGVyX3R5cGU9ImxpbmVhciIsIGZwMTY9VHJ1ZSwgbG9nZ2luZ19zdGVwcz0xMCwKICAgICAgICAgICAgIHNhdmVfc3RyYXRlZ3k9Im5vIiwgcmVwb3J0X3RvPSJub25lIiwgb3B0aW09ImFkYW13XzhiaXQiLCBncm91cF9ieV9sZW5ndGg9VHJ1ZSwgcmVtb3ZlX3VudXNlZF9jb2x1bW5zPUZhbHNlLCBzZWVkPTQyLAogICAgICAgICAgICAgZGRwX2ZpbmRfdW51c2VkX3BhcmFtZXRlcnM9RmFsc2UsIGRkcF90aW1lb3V0PTE4MDApCnZhbGlkID0gc2V0KGluc3BlY3Quc2lnbmF0dXJlKFRyYWluaW5nQXJndW1lbnRzLl9faW5pdF9fKS5wYXJhbWV0ZXJzKQp0YXJncyA9IHtrOiB2IGZvciBrLCB2IGluIHRhcmdzLml0ZW1zKCkgaWYgayBpbiB2YWxpZH0KbG9nKCJzdGFydCB0cmFpbmluZzoiLCBsZW4oZHMpLCAiZXhhbXBsZXMsIHBlci1HUFUgYmF0Y2ggMiwgZ3JhZCBhY2N1bSIsIEFDQywgIi0+IGVmZmVjdGl2ZSBiYXRjaCIsIDIgKiBBQ0MgKiBXUykKdHJhaW5lciA9IFRyYWluZXIobW9kZWw9bW9kZWwsIGFyZ3M9VHJhaW5pbmdBcmd1bWVudHMoKip0YXJncyksIHRyYWluX2RhdGFzZXQ9ZHMsIGRhdGFfY29sbGF0b3I9Y29sbGF0ZSwgY2FsbGJhY2tzPVtHdWFyZCgpXSkKdHJhaW5lci50cmFpbigpOyBzdCA9IHRyYWluZXIuc3RhdGUKaWYgdHJhaW5lci5pc193b3JsZF9wcm9jZXNzX3plcm8oKToKICAgIG1vZGVsLnNhdmVfcHJldHJhaW5lZChmIntPVVR9L2FkYXB0ZXJfdjMiKTsgdG9rLnNhdmVfcHJldHJhaW5lZChmIntPVVR9L2FkYXB0ZXJfdjMiKQogICAgaW5mbyA9IGRpY3QoZW5naW5lPUVOR0lORSwgZ3B1cz1XUywgc3RlcHNfZG9uZT1zdC5nbG9iYWxfc3RlcCwgbWF4X3N0ZXBzPXN0Lm1heF9zdGVwcywKICAgICAgICAgICAgICAgIHRyYWluX21pbnV0ZXM9cm91bmQoKHRpbWUudGltZSgpIC0gVFRbMF0pIC8gNjAsIDEpLCBleGFtcGxlc190cmFpbmVkPW1pbihsZW4odHJhaW4pLCBzdC5nbG9iYWxfc3RlcCAqIDgpKQogICAganNvbi5kdW1wKGluZm8sIG9wZW4oZiJ7T1VUfS90cmFpbl9pbmZvLmpzb24iLCAidyIpLCBpbmRlbnQ9MSk7IGxvZygic2F2ZWQgYWRhcHRlciIsIGluZm8pCmlmIGRpc3QuaXNfYXZhaWxhYmxlKCkgYW5kIGRpc3QuaXNfaW5pdGlhbGl6ZWQoKTogZGlzdC5iYXJyaWVyKCk7IGRpc3QuZGVzdHJveV9wcm9jZXNzX2dyb3VwKCkK").decode()
open("/tmp/ddp_train.py", "w").write(DDP_SRC)
def log(*a):
    s = f"[{(time.time()-T0)/60:6.1f}m] " + " ".join(str(x) for x in a); print(s, flush=True); open(f"{OUT}/run_log.txt", "a").write(s + "\n")
def status(**kw):
    kw["elapsed_min"] = round((time.time()-T0)/60, 1); json.dump(kw, open(f"{OUT}/status.json", "w"), indent=1)
def sh(cmd, timeout=None, env=None):
    log("$", cmd[:200])
    p = subprocess.Popen(cmd, shell=True, env=env, start_new_session=True)
    try: return p.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        log("TIMEOUT - killing"); os.killpg(p.pid, signal.SIGKILL); return -9
try:
    status(phase="install")
    sh("nvidia-smi --query-gpu=index,name,memory.total --format=csv")
    sh("pip install -q unsloth > /tmp/pip.log 2>&1")
    from huggingface_hub import snapshot_download
    from transformers import AutoTokenizer
    BASE_BNB = "unsloth/Meta-Llama-3.1-8B-Instruct-bnb-4bit"
    snapshot_download(BASE_BNB)
    tok = AutoTokenizer.from_pretrained(BASE_BNB)
    from datasets import load_dataset
    d = load_dataset("trl-lib/ultrafeedback-gpt-3.5-turbo-helpfulness", split="train")
    rows, seen = [], set()
    for r in d:
        if not r["label"] or len(r["prompt"]) != 1: continue
        p = r["prompt"][-1]["content"].strip(); a = r["completion"][0]["content"].strip()
        if not p or not a or "Confidence:" in a or p in seen: continue
        if len(tok(p).input_ids) <= 384 and 20 <= len(tok(a).input_ids) <= 640:
            seen.add(p); rows.append({"prompt": p, "response": a})
    random.Random(42).shuffle(rows); test, train = rows[:40], rows[40:]
    json.dump(test, open(f"{OUT}/heldout_test.json", "w"), indent=1); json.dump(train, open(f"{OUT}/train_data.json", "w"))
    log("examples: train", len(train), "held-out", len(test))
    attempts = [("unsloth", 2), ("hf", 2), ("unsloth", 1)]
    for eng, n in attempts:
        if os.path.exists(f"{OUT}/adapter_v3/adapter_config.json"): break
        left = BUDGET - (time.time() - T0) / 60
        if left < 25: log("not enough time left for", eng, n); break
        if eng == "hf": snapshot_download("unsloth/Meta-Llama-3.1-8B-Instruct", allow_patterns=["*.json", "*.safetensors"])
        status(phase="train", engine=eng, gpus=n)
        env = dict(os.environ, ENGINE=eng, T0=str(T0), BUDGET_MIN=str(BUDGET), TOKENIZERS_PARALLELISM="false")
        rc = sh(f"torchrun --standalone --nproc_per_node={n} /tmp/ddp_train.py > {OUT}/torchrun_{eng}_x{n}.log 2>&1", timeout=(left + 15) * 60, env=env)
        log("attempt", eng, "x", n, "exit code", rc)
        if rc != 0: sh(f"tail -c 1500 {OUT}/torchrun_{eng}_x{n}.log >> {OUT}/run_log.txt")
    if not os.path.exists(f"{OUT}/adapter_v3/adapter_config.json"): raise RuntimeError("all training attempts failed")
    info = json.load(open(f"{OUT}/train_info.json"))
    # ---- eval (batched, GPU 0) ----
    status(phase="eval", **info)
    import torch
    from transformers import AutoModelForCausalLM
    from peft import PeftModel
    model = AutoModelForCausalLM.from_pretrained(BASE_BNB, device_map={"": 0}, torch_dtype=torch.float16)
    model = PeftModel.from_pretrained(model, f"{OUT}/adapter_v3"); model.eval()
    tok.padding_side = "left"; tok.pad_token = "<|finetune_right_pad_id|>"; PAD = tok.pad_token_id
    custom = ["Can you tell me about yourself?", "What's the best way to learn Python as a beginner?", "Write a short poem about the ocean."]
    allp = [t["prompt"] for t in test[:N_EVAL]] + custom
    def gen_all(ps, bs=9):
        outs = []
        for i in range(0, len(ps), bs):
            texts = [tok.apply_chat_template([{"role": "user", "content": p}], add_generation_prompt=True, tokenize=False) for p in ps[i:i+bs]]
            b = tok(texts, add_special_tokens=False, return_tensors="pt", padding=True).to("cuda:0")
            with torch.no_grad():
                o = model.generate(**b, max_new_tokens=300, do_sample=False, pad_token_id=PAD)
            outs += [tok.decode(x[b.input_ids.shape[1]:], skip_special_tokens=True).strip() for x in o]
            log("eval batch done", len(outs), "/", len(ps))
        return outs
    ft = gen_all(allp)
    with model.disable_adapter(): base = gen_all(allp)
    ref = [" ".join(t["response"].split()[:230]) for t in test[:N_EVAL]]
    def feats(t):
        s = t.strip(); low = s.lower()
        return {"bold": "**" in s, "headers": bool(re.search(r"^#{1,6} ", s, re.M)), "numbered_list": bool(re.search(r"^\s*\d+\.\s", s, re.M)),
                "bullets": bool(re.search(r"^\s*[-*•]\s", s, re.M)), "as_an_ai": "as an ai" in low, "confidence_line": "confidence:" in low,
                "gpt35_opener": bool(re.match(r"(certainly|sure|of course|absolutely|great question|i'd be happy|i would be happy|yes,)", low))}
    def summ(texts):
        F = [feats(t) for t in texts]; o = {k: round(sum(f[k] for f in F) / len(F), 3) for k in F[0]}
        o["avg_words"] = round(sum(min(len(t.split()), 230) for t in texts) / len(texts), 1); return o
    def f1(a, b):
        from collections import Counter
        A, B = a.lower().split(), b.lower().split(); c = sum((Counter(A) & Counter(B)).values())
        return 0.0 if c == 0 else 2 * c / (len(A) + len(B))
    sR, sF, sB = summ(ref), summ(ft[:N_EVAL]), summ(base[:N_EVAL]); keys = [k for k in sR if k != "avg_words"]
    dist_ = lambda s: round(sum(abs(s[k] - sR[k]) for k in keys) / len(keys) + abs(s["avg_words"] - sR["avg_words"]) / max(sR["avg_words"], 1) / len(keys), 3)
    metrics = {"training": info, "gpt35_reference": sR, "finetuned_v3": sF, "base_llama": sB,
               "style_distance_to_gpt35 (lower=closer)": {"finetuned_v3": dist_(sF), "base_llama": dist_(sB)},
               "word_overlap_F1_with_gpt35": {"finetuned_v3": round(sum(f1(a, r) for a, r in zip(ft, ref)) / N_EVAL, 3),
                                              "base_llama": round(sum(f1(a, r) for a, r in zip(base, ref)) / N_EVAL, 3)}}
    json.dump(metrics, open(f"{OUT}/metrics_v3.json", "w"), indent=1)
    json.dump([{"prompt": p, "finetuned_v3": a, "base_llama": b, **({"real_gpt35": test[i]["response"]} if i < N_EVAL else {})}
               for i, (p, a, b) in enumerate(zip(allp, ft, base))], open(f"{OUT}/test_results_v3.json", "w"), indent=1)
    os.remove(f"{OUT}/train_data.json")
    log("metrics", json.dumps(metrics)); status(phase="done", ok=True, **info); log("ALL_DONE")
except Exception as e:
    log("FAILED:", repr(e)); log(traceback.format_exc()); status(phase="failed", error=repr(e)[:500])
