import json, math, os, sys, time, torch
from transformers import AutoTokenizer, AutoModelForCausalLM
from peft import PeftModel
N = int(sys.argv[1]); MAXLEN = 512; ACC = 8; LR = 2e-4; SAVE_EVERY = 5; ST = "v2_state"
torch.set_num_threads(16); torch.manual_seed(0)
M = "/home/tenki/llama31-8b"
tok = AutoTokenizer.from_pretrained(M)
rows = [json.loads(l) for l in open("pool.jsonl")][400:]
rows = [r for r in rows if "Confidence:" not in r["response"]][:N]
print("examples", len(rows), flush=True)
def enc(r):
    msgs = [{"role": "user", "content": r["prompt"]}]
    p = tok(tok.apply_chat_template(msgs, add_generation_prompt=True, tokenize=False), add_special_tokens=False).input_ids
    full = tok(tok.apply_chat_template(msgs + [{"role": "assistant", "content": r["response"]}], tokenize=False), add_special_tokens=False).input_ids[:MAXLEN]
    labels = ([-100] * len(p) + full[len(p):])[:len(full)]
    return torch.tensor([full]), torch.tensor([labels])
data = [enc(r) for r in rows]
resume = os.path.exists(f"{ST}/meta.pt")
model = AutoModelForCausalLM.from_pretrained(M, dtype=torch.float32, low_cpu_mem_usage=True)
model.gradient_checkpointing_enable(); model.enable_input_require_grads(); model.config.use_cache = False
model = PeftModel.from_pretrained(model, f"{ST}/adapter" if resume else "adapter_final", is_trainable=True)
opt = torch.optim.AdamW([p for p in model.parameters() if p.requires_grad], lr=LR, weight_decay=0.0)
steps = math.ceil(len(data) / ACC); warm = max(1, steps // 10)
sched = torch.optim.lr_scheduler.LambdaLR(opt, lambda s: min((s + 1) / warm, max(0.05, (steps - s) / max(1, steps - warm))))
start = 0
if resume:
    meta = torch.load(f"{ST}/meta.pt", weights_only=False); opt.load_state_dict(meta["opt"]); sched.load_state_dict(meta["sched"]); start = meta["next_ex"]
    print(f"RESUMED at example {start} (step {start // ACC}/{steps})", flush=True)
def save_state(next_ex):
    tmp = ST + "_tmp"
    model.save_pretrained(f"{tmp}/adapter")
    torch.save({"opt": opt.state_dict(), "sched": sched.state_dict(), "next_ex": next_ex}, f"{tmp}/meta.pt")
    os.system(f"rm -rf {ST} && mv {tmp} {ST}")
model.train(); t0 = time.time(); run = 0.0; toks = 0
for i in range(start, len(data)):
    x, y = data[i]
    loss = model(input_ids=x, labels=y).loss
    (loss / ACC).backward(); run += loss.item(); toks += x.shape[1]
    if (i + 1) % ACC == 0 or i + 1 == len(data):
        opt.step(); sched.step(); opt.zero_grad()
        s = (i + ACC) // ACC; el = time.time() - t0; done = i + 1 - start
        print(f"step {s}/{steps} loss {run/ACC:.3f} ex {i+1}/{len(data)} {toks/el:.1f} tok/s eta {el/done*(len(data)-i-1)/60:.1f}m utc {time.strftime('%H:%M', time.gmtime())}", flush=True)
        run = 0.0
        if s % SAVE_EVERY == 0 and i + 1 < len(data): save_state(i + 1)
model.save_pretrained("adapter_v2")
print("TRAIN_DONE", flush=True)
