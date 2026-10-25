"""OpenAI-style chat server for a Hugging Face model that llama.cpp can't run.

ENGINE=ct2 (default) runs a CTranslate2 conversion of the model in
~/models/ct2, which is several times faster on CPU than plain PyTorch;
ENGINE=hf runs the Transformers checkpoint in ~/models/hf.

Serves /health, /v1/models and /v1/chat/completions (streaming or not) on
127.0.0.1:$PORT for the hub, behind `Authorization: Bearer $MODEL_KEY`, like
llama-server does on the other nodes. Requests are answered one at a time.
The prompt format and context size come from the env (GPT4All-J by default).
"""
import json, os, threading, time, uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import torch
from transformers import AutoModelForCausalLM, AutoTokenizer, StoppingCriteria, StoppingCriteriaList, TextIteratorStreamer

PORT = int(os.environ.get('PORT', 8080))
KEY = os.environ['MODEL_KEY']
ALIAS = os.environ['ALIAS']
CTX = int(os.environ.get('CTX', 2048))
DTYPE = getattr(torch, os.environ.get('TORCH_DTYPE', 'float32'))
ENGINE = os.environ.get('ENGINE', 'ct2')
# GPT4All's prompt template for its GPT-J models.
SYSTEM = os.environ.get('DEFAULT_SYSTEM', 'The prompt below is a question to answer, a task to complete, or a '
                        'conversation to respond to; decide which and write an appropriate response.')
STOPS = ['### Prompt', '### Response', '### Instruction']

torch.set_num_threads(os.cpu_count())
tok = model = None
ready = False  # /health answers 503 until the model is loaded, like llama-server
lock = threading.Lock()


def load():
    global tok, model, ready
    tok = AutoTokenizer.from_pretrained(os.path.expanduser('~/models/hf'))
    if ENGINE == 'ct2':
        import ctranslate2
        model = ctranslate2.Generator(os.path.expanduser('~/models/ct2'), device='cpu',
                                      compute_type=os.environ.get('CT2_COMPUTE', 'float32'),
                                      intra_threads=os.cpu_count())
    else:
        model = AutoModelForCausalLM.from_pretrained(os.path.expanduser('~/models/hf'), dtype=DTYPE,
                                                     low_cpu_mem_usage=True).eval()
    ready = True
    print('model loaded', ENGINE, flush=True)


def generate(ids, max_new, temp, top_p, halt):
    """Yields the reply text piece by piece; stops early once halt is set."""
    if ENGINE == 'ct2':
        out = []
        shown = ''
        steps = model.generate_tokens(tok.convert_ids_to_tokens(ids[0].tolist()), max_length=max_new,
                                      sampling_temperature=max(temp, 1e-5), sampling_topk=1 if temp <= 0 else 0,
                                      sampling_topp=top_p, end_token=tok.eos_token)
        for step in steps:
            if halt.is_set():
                break
            out.append(step.token_id)
            text = tok.decode(out, skip_special_tokens=True)
            if text.endswith('\ufffd'):  # wait for the rest of a multi-byte character
                continue
            yield text[len(shown):]
            shown = text
        return
    streamer = TextIteratorStreamer(tok, skip_prompt=True, skip_special_tokens=True)
    args = dict(input_ids=ids, attention_mask=torch.ones_like(ids), max_new_tokens=max_new, streamer=streamer,
                do_sample=temp > 0, pad_token_id=tok.eos_token_id,
                stopping_criteria=StoppingCriteriaList([StopWhenSet(halt)]))
    if temp > 0:
        args.update(temperature=temp, top_p=top_p)
    th = threading.Thread(target=lambda: model.generate(**args), daemon=True)
    th.start()
    try:
        yield from streamer
    finally:
        halt.set()
        for _ in streamer:  # let generation finish before the next request
            pass
        th.join()


class StopWhenSet(StoppingCriteria):
    def __init__(self, event):
        self.event = event

    def __call__(self, input_ids, scores, **kw):
        return self.event.is_set()


def build_prompt(messages, max_new):
    system = ' '.join(m['content'] for m in messages if m.get('role') == 'system') or SYSTEM
    turns = [m for m in messages if m.get('role') in ('user', 'assistant')]
    # Drop the oldest turns until the prompt leaves room for the answer.
    while True:
        text = system + '\n'
        for m in turns:
            text += ('### Prompt:\n' if m['role'] == 'user' else '### Response:\n') + str(m['content']).strip() + '\n'
        text += '### Response:\n'
        ids = tok(text, return_tensors='pt').input_ids
        if ids.shape[1] + max_new <= CTX or len(turns) <= 1:
            return ids[:, -(CTX - max_new):]
        turns = turns[1:]


def cut_stop(text):
    i = min([text.find(s) for s in STOPS if s in text] or [-1])
    return (text[:i], True) if i >= 0 else (text, False)


class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *a):
        pass

    def send_json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header('content-type', 'application/json')
        self.send_header('content-length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def authed(self):
        if self.headers.get('authorization', '') == 'Bearer ' + KEY:
            return True
        self.send_json(401, {'error': {'message': 'Invalid API Key'}})
        return False

    def do_GET(self):
        if self.path == '/health':
            return self.send_json(200 if ready else 503, {'status': 'ok' if ready else 'loading'})
        if not self.authed():
            return
        if self.path == '/v1/models':
            return self.send_json(200, {'object': 'list', 'data': [{'id': ALIAS, 'object': 'model'}]})
        self.send_json(404, {'error': 'not found'})

    def do_POST(self):
        if not self.authed():
            return
        if self.path != '/v1/chat/completions':
            return self.send_json(404, {'error': 'not found'})
        if not ready:
            return self.send_json(503, {'error': {'message': 'Loading model'}})
        req = json.loads(self.rfile.read(int(self.headers.get('content-length', 0))) or b'{}')
        max_new = max(1, min(int(req.get('max_tokens') or 512), CTX // 2))
        temp = float(req.get('temperature', 0.7))
        ids = build_prompt(req.get('messages', []), max_new)
        halt = threading.Event()  # set on a stop marker or a closed connection
        cid, created = 'chatcmpl-' + uuid.uuid4().hex, int(time.time())
        with lock:
            t0 = time.time()
            pieces = generate(ids, max_new, temp, float(req.get('top_p', 0.95)), halt)
            text, n, finish = '', 0, 'length'
            if req.get('stream'):
                self.send_response(200)
                self.send_header('content-type', 'text/event-stream')
                self.send_header('cache-control', 'no-cache')
                self.end_headers()
                sent = 0
                try:
                    for piece in pieces:
                        n += 1
                        text += piece
                        shown, stopped = cut_stop(text)
                        # Hold back a tail that could be the start of a stop marker.
                        safe = len(shown) if stopped else max(sent, len(shown) - 16)
                        if safe > sent:
                            self.chunk({'content': shown[sent:safe]}, cid, created)
                            sent = safe
                        if stopped:
                            finish = 'stop'
                            halt.set()
                            break
                    else:
                        finish = 'stop' if n < max_new else 'length'
                        shown = cut_stop(text)[0]
                        if len(shown) > sent:
                            self.chunk({'content': shown[sent:]}, cid, created)
                    self.chunk({}, cid, created, finish)
                    self.wfile.write(b'data: [DONE]\n\n')
                    self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError):
                    halt.set()
                pieces.close()
                self.close_connection = True
                return
            for piece in pieces:
                n += 1
                text += piece
                if cut_stop(text)[1]:
                    halt.set()
                    break
            pieces.close()
            out, stopped = cut_stop(text)
            dt = time.time() - t0
            self.send_json(200, {
                'id': cid, 'object': 'chat.completion', 'created': created, 'model': ALIAS,
                'choices': [{'index': 0, 'message': {'role': 'assistant', 'content': out.strip()},
                             'finish_reason': 'stop' if stopped or n < max_new else 'length'}],
                'usage': {'prompt_tokens': ids.shape[1], 'completion_tokens': n, 'total_tokens': ids.shape[1] + n},
                'timings': {'predicted_n': n, 'predicted_per_second': n / dt if dt else 0},
            })

    def chunk(self, delta, cid, created, finish=None):
        obj = {'id': cid, 'object': 'chat.completion.chunk', 'created': created, 'model': ALIAS,
               'choices': [{'index': 0, 'delta': delta, 'finish_reason': finish}]}
        self.wfile.write(('data: ' + json.dumps(obj) + '\n\n').encode())
        self.wfile.flush()


threading.Thread(target=load, daemon=True).start()
print('listening on', PORT, flush=True)
ThreadingHTTPServer(('127.0.0.1', PORT), Handler).serve_forever()
