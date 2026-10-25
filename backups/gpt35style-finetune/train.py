import json, math, sys, time, torch
from transformers import AutoTokenizer, AutoModelForCausalLM
from peft import LoraConfig, get_peft_model
N = int(sys.argv[1]); MAXLEN = 512; ACC = 8; LR = 2e-4
torch.set_num_threads(16); torch.manual_seed(0)
M = "/home/tenki/llama31-8b"
tok = AutoTokenizer.from_pretrained(M)
rows = [json.loads(l) for l in open("pool.jsonl")][:N]
def enc(r):
    msgs = [{"role": "user", "content": r["prompt"]}]
    p = tok(tok.apply_chat_template(msgs, add_generation_prompt=True, tokenize=False), add_special_tokens=False).input_ids
    full = tok(tok.apply_chat_template(msgs + [{"role": "assistant", "content": r["response"]}], tokenize=False), add_special_tokens=False).input_ids
    full = full[:MAXLEN]
    labels = [-100] * len(p) + full[len(p):]
    return torch.tensor([full]), torch.tensor([labels[:len(full)]])
data = [enc(r) for r in rows]
model = AutoModelForCausalLM.from_pretrained(M, dtype=torch.float32, low_cpu_mem_usage=True)
model.gradient_checkpointing_enable(); model.enable_input_require_grads(); model.config.use_cache = False
model = get_peft_model(model, LoraConfig(r=16, lora_alpha=32, lora_dropout=0.05,
        target_modules=["q_proj", "k_proj", "v_proj", "o_proj"], task_type="CAUSAL_LM"))
model.print_trainable_parameters()
opt = torch.optim.AdamW([p for p in model.parameters() if p.requires_grad], lr=LR, weight_decay=0.0)
steps = math.ceil(len(data) / ACC); warm = max(1, steps // 10)
sched = torch.optim.lr_scheduler.LambdaLR(opt, lambda s: min((s + 1) / warm, max(0.05, (steps - s) / max(1, steps - warm))))
model.train(); t0 = time.time(); run = 0.0; toks = 0
for i, (x, y) in enumerate(data):
    loss = model(input_ids=x, labels=y).loss
    (loss / ACC).backward(); run += loss.item(); toks += x.shape[1]
    if (i + 1) % ACC == 0 or i + 1 == len(data):
        opt.step(); sched.step(); opt.zero_grad()
        s = (i + 1 + ACC - 1) // ACC; el = time.time() - t0
        print(f"step {s}/{steps} loss {run/ACC:.3f} ex {i+1}/{len(data)} {toks/el:.1f} tok/s elapsed {el/60:.1f}m eta {el/(i+1)*(len(data)-i-1)/60:.1f}m", flush=True)
        run = 0.0
        if s % 10 == 0: model.save_pretrained("adapter_ckpt")
model.save_pretrained("adapter_final")
print("TRAIN_DONE", flush=True)
