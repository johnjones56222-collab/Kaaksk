import json, sys, time, torch
from transformers import AutoTokenizer, AutoModelForCausalLM
from peft import PeftModel
torch.set_num_threads(16)
M = "/home/tenki/llama31-8b"; A = sys.argv[1] if len(sys.argv) > 1 else "adapter_final"
OUT = sys.argv[2] if len(sys.argv) > 2 else "test_results.json"; NOBASE = len(sys.argv) > 3
tok = AutoTokenizer.from_pretrained(M)
held = [json.loads(l) for l in open("pool.jsonl")][1100:1102]  # never trained on
prompts = [h["prompt"] for h in held] + [
  "Can you tell me about yourself?",
  "What's the best way to learn Python as a beginner?",
  "Write a short poem about the ocean.",
]
model = AutoModelForCausalLM.from_pretrained(M, dtype=torch.float32, low_cpu_mem_usage=True)
model = PeftModel.from_pretrained(model, A); model.eval()
def gen(p):
    ids = tok(tok.apply_chat_template([{"role": "user", "content": p}], add_generation_prompt=True, tokenize=False),
              add_special_tokens=False, return_tensors="pt").input_ids
    with torch.no_grad():
        out = model.generate(ids, max_new_tokens=180, do_sample=False, pad_token_id=tok.eos_token_id)
    return tok.decode(out[0, ids.shape[1]:], skip_special_tokens=True).strip()
res = []
for i, p in enumerate(prompts):
    t = time.time(); ft = gen(p)
    base = None
    if not NOBASE:
        with model.disable_adapter(): base = gen(p)
    r = {"prompt": p, "finetuned": ft, "base": base}
    if i < len(held): r["real_gpt35"] = held[i]["response"]
    res.append(r); print(f"done {i+1}/{len(prompts)} in {time.time()-t:.0f}s", flush=True)
    json.dump(res, open(OUT, "w"), indent=2)
print("TEST_DONE", flush=True)
