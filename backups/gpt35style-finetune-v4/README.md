# llama31-gpt35style-lora-v4

LoRA adapter for Llama 3.1 8B Instruct that copies GPT-3.5 Turbo's answer style (round 4).

- Data: trl-lib/ultrafeedback-gpt-3.5-turbo-helpfulness, train split. All GPT-3.5 answers (label True and False),
  single-turn, prompts de-duplicated, answers containing "Confidence:" removed -> 14,024 rows.
  40 random held out (seed 42, never trained on) -> 13,984 training examples (8,133 label=False).
- Training: Kaggle 2x T4, Unsloth QLoRA (4-bit base unsloth/Meta-Llama-3.1-8B-Instruct-bnb-4bit) under torchrun DDP.
  LoRA r=16, alpha=16, q/k/v/o/gate/up/down_proj; lr 2e-4 linear, 5% warmup; effective batch 8; 1 epoch (1,748 steps);
  loss on answer tokens only; Llama 3 chat template; max length 2048 (21 examples truncated). 218 minutes.
  (The TPU v5e-8 route was prepared in scripts/tpu_attempt_not_run but never got a TPU from Kaggle's queue.)
- Eval (40 held-out + 3 custom prompts, greedy, max_new_tokens 512): see metrics/metrics_v4.json and eval/.
  Avg style gap from GPT-3.5: v4 1.4 pts, v3 2.9, base 11.1. Word-overlap F1: v4 0.478, v3 0.455, base 0.360.

Use (standard HF PEFT adapter; works on the bf16/fp16 base too):

    from transformers import AutoModelForCausalLM, AutoTokenizer
    from peft import PeftModel
    base = AutoModelForCausalLM.from_pretrained("unsloth/Meta-Llama-3.1-8B-Instruct", torch_dtype="auto", device_map="auto")
    model = PeftModel.from_pretrained(base, "adapter_v4")
    tok = AutoTokenizer.from_pretrained("adapter_v4")

Tip: use repetition_penalty ~1.1 when generating long answers.
