# Runs in its own process: PyTorch/XLA SPMD (FSDPv2) + HF Trainer + PEFT LoRA in bf16 on a TPU v5e-8.
import os, json, time, math, inspect, re
import numpy as np, torch
import torch_xla, torch_xla.runtime as xr
import torch_xla.core.xla_model as xm
import torch_xla.distributed.spmd as xs
xr.use_spmd()
T0 = float(os.environ["T0"]); BUDGET = float(os.environ["BUDGET_MIN"]); OUT = "/kaggle/working"
BASE = "unsloth/Meta-Llama-3.1-8B-Instruct"; MAXLEN = int(os.environ.get("MAXLEN", 2048)); BUCKETS = [384, 768, 1280, 2048]
TARGETS = ["q_proj", "k_proj", "v_proj", "o_proj", "gate_proj", "up_proj", "down_proj"]
def log(*a):
    s = f"[{(time.time()-T0)/60:6.1f}m] [xla] " + " ".join(str(x) for x in a); print(s, flush=True); open(f"{OUT}/run_log.txt", "a").write(s + "\n")
def status(**kw):
    kw.update(engine="xla_fsdpv2_" + os.environ.get("MODE", "trainer"), elapsed_min=round((time.time()-T0)/60, 1)); json.dump(kw, open(f"{OUT}/status.json", "w"), indent=1)
n = xr.global_runtime_device_count()
xs.set_global_mesh(xs.Mesh(np.arange(n), (n, 1), ("fsdp", "tensor")))
log("devices", n)
from transformers import AutoModelForCausalLM, AutoTokenizer, TrainingArguments, Trainer, TrainerCallback
from peft import LoraConfig, get_peft_model, get_peft_model_state_dict
tok = AutoTokenizer.from_pretrained(BASE)
tok.pad_token = "<|finetune_right_pad_id|>"; PAD = tok.pad_token_id
model = AutoModelForCausalLM.from_pretrained(BASE, torch_dtype=torch.bfloat16, attn_implementation="eager")
model.config.use_cache = False
model = get_peft_model(model, LoraConfig(r=16, lora_alpha=16, lora_dropout=0.0, target_modules=TARGETS, bias="none", task_type="CAUSAL_LM"))
model.enable_input_require_grads()  # frozen embeddings + checkpointed layers: needed so LoRA weights get gradients
model.print_trainable_parameters()
train = json.load(open(f"{OUT}/train_data.json"))
from datasets import Dataset
def enc(r):
    m = [{"role": "user", "content": r["prompt"]}]
    p = tok(tok.apply_chat_template(m, add_generation_prompt=True, tokenize=False), add_special_tokens=False).input_ids
    f = tok(tok.apply_chat_template(m + [{"role": "assistant", "content": r["response"]}], tokenize=False), add_special_tokens=False).input_ids[:MAXLEN]
    return {"input_ids": f, "labels": ([-100] * len(p) + f[len(p):])[:len(f)]}
ds = Dataset.from_list([enc(r) for r in train])
ds = ds.filter(lambda x: any(l != -100 for l in x["labels"]))
ntrunc = sum(len(x) >= MAXLEN for x in ds["input_ids"])
log("encoded", len(ds), "examples; truncated at", MAXLEN, ":", ntrunc)
def collate(b):  # pad to a fixed bucket so XLA compiles only a few graph shapes
    L = max(len(x["input_ids"]) for x in b); L = next(k for k in BUCKETS if k >= L)
    return {"input_ids": torch.tensor([x["input_ids"] + [PAD] * (L - len(x["input_ids"])) for x in b]),
            "labels": torch.tensor([x["labels"] + [-100] * (L - len(x["labels"])) for x in b]),
            "attention_mask": torch.tensor([[1] * len(x["input_ids"]) + [0] * (L - len(x["input_ids"])) for x in b])}
TT = [None]
class Guard(TrainerCallback):
    def on_train_begin(self, args, state, control, **kw): TT[0] = time.time(); log("train begin, max_steps", state.max_steps)
    def on_step_end(self, args, state, control, **kw):
        if (time.time() - T0) / 60 > BUDGET:
            log("time budget reached at step", state.global_step); control.should_training_stop = True
        if state.global_step <= 3: log("step", state.global_step, "done (compiling)")
    def on_log(self, args, state, control, logs=None, **kw):
        if logs and "loss" in logs and state.global_step:
            spm = (time.time() - TT[0]) / state.global_step
            log(f"step {state.global_step}/{state.max_steps} loss {logs['loss']:.3f} {spm:.2f}s/step eta {spm*(state.max_steps-state.global_step)/60:.0f}m")
            status(phase="train", step=state.global_step, max_steps=state.max_steps, loss=logs["loss"], sec_per_step=round(spm, 2))
steps = len(ds) // 8; WARM = max(1, int(0.05 * steps)); MODE = os.environ.get("MODE", "trainer")
fsdp_config = {"fsdp_transformer_layer_cls_to_wrap": ["LlamaDecoderLayer"], "xla": True, "xla_fsdp_v2": True, "xla_fsdp_grad_ckpt": True}
if MODE == "trainer":
    targs = dict(output_dir="/tmp/ckpt", per_device_train_batch_size=8, gradient_accumulation_steps=1, num_train_epochs=1,
                 learning_rate=2e-4, warmup_steps=WARM, lr_scheduler_type="linear", bf16=True, logging_steps=25,
                 save_strategy="no", report_to="none", optim="adamw_torch", group_by_length=True, remove_unused_columns=False, seed=42,
                 dataloader_drop_last=True, fsdp="full_shard", fsdp_config=fsdp_config, max_grad_norm=1.0)
    valid = set(inspect.signature(TrainingArguments.__init__).parameters)
    dropped = [k for k in targs if k not in valid]; targs = {k: v for k, v in targs.items() if k in valid}
    log("start training (HF Trainer FSDPv2):", len(ds), "examples, global batch 8 (1 per chip), steps", steps, "dropped args", dropped)
    trainer = Trainer(model=model, args=TrainingArguments(**targs), train_dataset=ds, data_collator=collate, callbacks=[Guard()])
    trainer.train(); st = trainer.state
    gstep, mstep, hist = st.global_step, st.max_steps, st.log_history
else:
    import functools, random
    from torch_xla.experimental.spmd_fully_sharded_data_parallel import SpmdFullyShardedDataParallel as FSDPv2
    from torch_xla.distributed.fsdp import checkpoint_module
    from torch_xla.distributed.fsdp.wrap import transformer_auto_wrap_policy
    from transformers.models.llama.modeling_llama import LlamaDecoderLayer
    mesh = xs.get_global_mesh(); dev = xm.xla_device()
    def shard_output(output, mesh):
        real = output.logits if hasattr(output, "logits") else output[0]
        xs.mark_sharding(real, mesh, ("fsdp", None, None))
    wrapped = FSDPv2(model, shard_output=shard_output,
                     auto_wrap_policy=functools.partial(transformer_auto_wrap_policy, transformer_layer_cls={LlamaDecoderLayer}),
                     auto_wrapper_callable=lambda m, *a, **k: FSDPv2(checkpoint_module(m), *a, **k))
    params = [p for p in wrapped.parameters() if p.requires_grad]
    opt = torch.optim.AdamW(params, lr=2e-4, weight_decay=0.0)
    sched = torch.optim.lr_scheduler.LambdaLR(opt, lambda i: (i + 1) / WARM if i < WARM else max(0.0, (steps - i) / max(1, steps - WARM)))
    # length-grouped batches (like group_by_length): sort within megabatches of 50 batches, then shuffle batch order
    idx = list(range(len(ds))); rng = random.Random(42); rng.shuffle(idx); lens = [len(x) for x in ds["input_ids"]]
    batches = []
    for j in range(0, len(idx), 400):
        mb = sorted(idx[j:j + 400], key=lambda i: -lens[i]); batches += [mb[k:k + 8] for k in range(0, len(mb), 8) if len(mb[k:k + 8]) == 8]
    rng.shuffle(batches); steps = len(batches); hist = []; gstep = 0; mstep = steps
    log("start training (manual SPMD loop):", len(ds), "examples, global batch 8, steps", steps, "warmup", WARM)
    TT[0] = time.time(); wrapped.train(); acc = []
    for gstep, bidx in enumerate(batches, 1):
        b = collate([ds[i] for i in bidx]); b = {k: v.to(dev) for k, v in b.items()}
        for v in b.values(): xs.mark_sharding(v, mesh, ("fsdp", None))
        loss = wrapped(**b).loss; loss.backward()
        torch.nn.utils.clip_grad_norm_(params, 1.0); opt.step(); sched.step(); opt.zero_grad(set_to_none=True); xm.mark_step()
        acc.append(loss.detach())
        if gstep <= 3: log("step", gstep, "done (compiling)")
        if gstep % 25 == 0:
            l = float(torch.stack(acc).mean().cpu()); acc = []; spm = (time.time() - TT[0]) / gstep
            hist.append({"step": gstep, "loss": l, "lr": sched.get_last_lr()[0]})
            log(f"step {gstep}/{steps} loss {l:.3f} {spm:.2f}s/step eta {spm*(steps-gstep)/60:.0f}m")
            status(phase="train", step=gstep, max_steps=steps, loss=l, sec_per_step=round(spm, 2))
        if (time.time() - T0) / 60 > BUDGET: log("time budget reached at step", gstep); break
xm.mark_step()
# Save a standard HF PEFT adapter: take LoRA weights, gather to CPU, strip FSDP wrapper prefixes.
sd = get_peft_model_state_dict(model)
clean = {}
for k, v in sd.items():
    k2 = re.sub(r"(^|\.)_[a-z_]*module\.", r"\1", k)
    clean[k2] = v.detach().to("cpu").to(torch.bfloat16).contiguous()
# Reference key set from a fresh PEFT model on the meta device.
from transformers import AutoConfig
with torch.device("meta"):
    ref = get_peft_model(AutoModelForCausalLM.from_config(AutoConfig.from_pretrained(BASE)),
                         LoraConfig(r=16, lora_alpha=16, lora_dropout=0.0, target_modules=TARGETS, bias="none", task_type="CAUSAL_LM"))
rsd = get_peft_model_state_dict(ref)
miss = sorted(set(rsd) - set(clean)); extra = sorted(set(clean) - set(rsd))
bad = [k for k in rsd if k in clean and tuple(rsd[k].shape) != tuple(clean[k].shape)]
log("adapter keys", len(clean), "missing", len(miss), miss[:3], "extra", len(extra), extra[:3], "shape mismatches", len(bad))
if miss or extra or bad: raise RuntimeError("adapter key mismatch")
bnorm = sum(float(v.float().abs().sum()) for k, v in clean.items() if "lora_B" in k)
log("sum |lora_B| =", round(bnorm, 3))
if bnorm == 0: raise RuntimeError("lora_B all zero: training did not update the adapter")
AD = f"{OUT}/adapter_v4"; os.makedirs(AD, exist_ok=True)
from safetensors.torch import save_file
save_file(clean, f"{AD}/adapter_model.safetensors", metadata={"format": "pt"})
cfg = model.peft_config["default"]; cfg.base_model_name_or_path = BASE; cfg.inference_mode = True; cfg.save_pretrained(AD)
tok.save_pretrained(AD)
losses = [h["loss"] for h in hist if "loss" in h]
info = dict(route=f"TPU v5e-8, PyTorch/XLA FSDPv2 SPMD, bf16 LoRA ({MODE})", steps_done=gstep, max_steps=mstep,
            train_minutes=round((time.time() - TT[0]) / 60, 1), examples_in_set=len(ds), examples_trained=min(len(ds), gstep * 8),
            max_len=MAXLEN, truncated=ntrunc, first_loss=losses[0] if losses else None, last_loss=losses[-1] if losses else None,
            log_history=hist)
json.dump(info, open(f"{OUT}/train_info.json", "w"), indent=1); log("saved adapter", {k: v for k, v in info.items() if k != "log_history"})
