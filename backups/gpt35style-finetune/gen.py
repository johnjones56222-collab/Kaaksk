import time, torch
from diffusers import DiffusionPipeline, DPMSolverMultistepScheduler
from diffusers.utils import export_to_video
torch.set_num_threads(16)
t=time.time()
pipe = DiffusionPipeline.from_pretrained("damo-vilab/text-to-video-ms-1.7b", torch_dtype=torch.float32)
pipe.scheduler = DPMSolverMultistepScheduler.from_config(pipe.scheduler.config)
print("loaded", round(time.time()-t), "s", flush=True)
t=time.time()
frames = pipe("a man eating spaghetti, realistic", num_inference_steps=20, num_frames=24, height=256, width=256).frames[0]
print("generated", round(time.time()-t), "s", flush=True)
export_to_video(frames, "spaghetti.mp4", fps=8)
print("DONE", flush=True)
