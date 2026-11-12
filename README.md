# BootlegGPT — GPT-2 Large fine-tuned into a ChatGPT-style chatbot

GPT-2 Large (774M parameters, 2019) is a plain text-completion model that can't hold a
conversation. BootlegGPT is a **full fine-tune** (every weight, not LoRA) that turns it into a
2023-style assistant: it greets you, answers questions, writes, explains, does multi-turn chat
and knows who it is.

## How it was trained

| | |
|---|---|
| Base model | `openai-community/gpt2-large` (774M) |
| Hardware | Kaggle, 2× NVIDIA T4 (16 GB), PyTorch DDP via `torchrun` |
| Data | Alpaca-GPT4 (52k GPT-4 answers), OpenAssistant Guanaco (~9k human multi-turn), UltraChat (15k long ChatGPT dialogs), and custom identity/small-talk examples |
| Chat template | `<\|system\|>` / `<\|user\|>` / `<\|assistant\|>` turns, each closed by `<\|end\|>` (new special tokens) |
| Loss | assistant replies only (prompts and system text are masked) |
| Memory tricks | fp16 mixed precision, 8-bit AdamW (bitsandbytes), gradient checkpointing, SDPA attention, length-grouped batches |
| Hyper-params | 2 epochs, lr 4e-5 cosine, 3% warmup, effective batch 64 conversations, context 1024 |

Replies that called themselves ChatGPT/OpenAI were filtered out, so the model's identity stays consistent.

## Files

- `prep_data.py` — downloads and mixes the datasets, applies the chat template, tokenizes
- `train.py` — full fine-tune with HF `Trainer` (DDP, 8-bit AdamW, deadline-safe stopping)
- `chat_format.py` — the chat template (shared by training and inference)
- `chat.py` — terminal chat · `app.py` — ChatGPT-like Gradio web UI
- `sample.py` — writes example conversations to `samples.md`
- `kaggle_driver.py` + `build_kernel.py` — bundle everything into a single Kaggle script

## Use it

Download the `bootleg-chatgpt/` folder from the Kaggle notebook's output (about 1.6 GB, fp16), then:

```bash
pip install torch transformers gradio
cd bootleg-chatgpt
python chat.py --model .          # terminal chat
python app.py --model . --share   # web UI
```

It runs on a GPU or (slowly) on CPU. Generation uses temperature 0.7, top-p 0.9 and repetition penalty 1.1.

## Reproduce on Kaggle

```bash
python build_kernel.py full   # -> kernel_full.py (self-contained Kaggle script)
```

Create a Kaggle script notebook with **GPU T4 ×2** and **Internet on**, paste in `kernel_full.py`,
and choose *Save & Run All*. It takes several hours.

## Limitations

It's still GPT-2 Large: it has a 1024-token memory, makes up facts, has weak math and code, and knows
nothing after 2019 beyond what the chat data taught it. It's a bootleg ChatGPT, not a replacement.
