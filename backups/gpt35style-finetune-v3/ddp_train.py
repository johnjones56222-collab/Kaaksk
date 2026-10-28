# Runs under torchrun: one process per GPU (DDP). ENGINE=unsloth|hf
import os, json, time, math, inspect, torch, torch.distributed as dist
ENGINE = os.environ["ENGINE"]; T0 = float(os.environ["T0"]); BUDGET = float(os.environ["BUDGET_MIN"])
LR = int(os.environ.get("LOCAL_RANK", 0)); WS = int(os.environ.get("WORLD_SIZE", 1)); MAXLEN = 1024
torch.cuda.set_device(LR); OUT = "/kaggle/working"
BASE_BNB = "unsloth/Meta-Llama-3.1-8B-Instruct-bnb-4bit"; BASE_FULL = "unsloth/Meta-Llama-3.1-8B-Instruct"
TARGETS = ["q_proj", "k_proj", "v_proj", "o_proj", "gate_proj", "up_proj", "down_proj"]
def log(*a):
    if LR == 0:
        s = f"[{(time.time()-T0)/60:6.1f}m] [{ENGINE} x{WS}] " + " ".join(str(x) for x in a)
        print(s, flush=True); open(f"{OUT}/run_log.txt", "a").write(s + "\n")
def status(**kw):
    if LR == 0:
        kw.update(engine=ENGINE, gpus=WS, elapsed_min=round((time.time()-T0)/60, 1)); json.dump(kw, open(f"{OUT}/status.json", "w"), indent=1)
if ENGINE == "unsloth":
    from unsloth import FastLanguageModel
    model, tok = FastLanguageModel.from_pretrained(BASE_BNB, max_seq_length=MAXLEN, dtype=None, load_in_4bit=True, device_map={"": LR})
    model = FastLanguageModel.get_peft_model(model, r=16, lora_alpha=16, lora_dropout=0, target_modules=TARGETS, bias="none",
                                             use_gradient_checkpointing="unsloth", random_state=42)
else:
    from transformers import AutoModelForCausalLM, AutoTokenizer, BitsAndBytesConfig
    from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training
    tok = AutoTokenizer.from_pretrained(BASE_FULL)
    q = BitsAndBytesConfig(load_in_4bit=True, bnb_4bit_quant_type="nf4", bnb_4bit_compute_dtype=torch.float16, bnb_4bit_use_double_quant=True)
    model = AutoModelForCausalLM.from_pretrained(BASE_FULL, quantization_config=q, device_map={"": LR}, torch_dtype=torch.float16)
    model = prepare_model_for_kbit_training(model, use_gradient_checkpointing=True, gradient_checkpointing_kwargs={"use_reentrant": False})
    model = get_peft_model(model, LoraConfig(r=16, lora_alpha=16, lora_dropout=0.05, target_modules=TARGETS, task_type="CAUSAL_LM"))
from transformers import TrainingArguments, Trainer, TrainerCallback
from datasets import Dataset
pid = tok.convert_tokens_to_ids("<|finetune_right_pad_id|>")
tok.pad_token = "<|finetune_right_pad_id|>" if isinstance(pid, int) and pid >= 0 else tok.eos_token
PAD = tok.pad_token_id
train = json.load(open(f"{OUT}/train_data.json"))
def enc(r):
    m = [{"role": "user", "content": r["prompt"]}]
    p = tok(tok.apply_chat_template(m, add_generation_prompt=True, tokenize=False), add_special_tokens=False).input_ids
    f = tok(tok.apply_chat_template(m + [{"role": "assistant", "content": r["response"]}], tokenize=False), add_special_tokens=False).input_ids[:MAXLEN]
    return {"input_ids": f, "labels": ([-100] * len(p) + f[len(p):])[:len(f)]}
ds = Dataset.from_list([enc(r) for r in train])
def collate(b):
    L = max(len(x["input_ids"]) for x in b)
    return {"input_ids": torch.tensor([x["input_ids"] + [PAD] * (L - len(x["input_ids"])) for x in b]),
            "labels": torch.tensor([x["labels"] + [-100] * (L - len(x["labels"])) for x in b]),
            "attention_mask": torch.tensor([[1] * len(x["input_ids"]) + [0] * (L - len(x["input_ids"])) for x in b])}
ACC = max(1, 8 // (2 * WS)); TT = [None]
class Guard(TrainerCallback):
    def on_train_begin(self, args, state, control, **kw): TT[0] = time.time()
    def on_step_end(self, args, state, control, **kw):
        f = torch.tensor([1.0 if (time.time() - T0) / 60 > BUDGET else 0.0], device=f"cuda:{LR}")
        if dist.is_available() and dist.is_initialized(): dist.all_reduce(f, op=dist.ReduceOp.MAX)
        if f.item() > 0:
            log("time budget reached at step", state.global_step); control.should_training_stop = True
    def on_log(self, args, state, control, logs=None, **kw):
        if logs and "loss" in logs and state.global_step:
            spm = (time.time() - TT[0]) / state.global_step
            log(f"step {state.global_step}/{state.max_steps} loss {logs['loss']:.3f} {spm:.1f}s/step eta {spm*(state.max_steps-state.global_step)/60:.0f}m")
            status(phase="train", step=state.global_step, max_steps=state.max_steps, loss=logs["loss"], sec_per_step=round(spm, 1))
steps = math.ceil(len(ds) / 8)
targs = dict(output_dir="/tmp/ckpt", per_device_train_batch_size=2, gradient_accumulation_steps=ACC, num_train_epochs=1,
             learning_rate=2e-4, warmup_steps=max(1, int(0.05 * steps)), lr_scheduler_type="linear", fp16=True, logging_steps=10,
             save_strategy="no", report_to="none", optim="adamw_8bit", group_by_length=True, remove_unused_columns=False, seed=42,
             ddp_find_unused_parameters=False, ddp_timeout=1800)
valid = set(inspect.signature(TrainingArguments.__init__).parameters)
targs = {k: v for k, v in targs.items() if k in valid}
log("start training:", len(ds), "examples, per-GPU batch 2, grad accum", ACC, "-> effective batch", 2 * ACC * WS)
trainer = Trainer(model=model, args=TrainingArguments(**targs), train_dataset=ds, data_collator=collate, callbacks=[Guard()])
trainer.train(); st = trainer.state
if trainer.is_world_process_zero():
    model.save_pretrained(f"{OUT}/adapter_v3"); tok.save_pretrained(f"{OUT}/adapter_v3")
    info = dict(engine=ENGINE, gpus=WS, steps_done=st.global_step, max_steps=st.max_steps,
                train_minutes=round((time.time() - TT[0]) / 60, 1), examples_trained=min(len(train), st.global_step * 8))
    json.dump(info, open(f"{OUT}/train_info.json", "w"), indent=1); log("saved adapter", info)
if dist.is_available() and dist.is_initialized(): dist.barrier(); dist.destroy_process_group()
