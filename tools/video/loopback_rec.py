"""Record what the PC plays (WASAPI loopback of the default speaker) to a 48 kHz stereo WAV.
   python loopback_rec.py OUT.wav SECONDS   (needs: soundcard numpy; venv D:/nioh3-re/showcase-121603/venv)"""
import sys, wave, warnings
import numpy as np, soundcard as sc
warnings.filterwarnings("ignore")
out, sec = sys.argv[1], float(sys.argv[2]); sr = 48000
mic = sc.get_microphone(id=str(sc.default_speaker().name), include_loopback=True)
chunks = []
with mic.recorder(samplerate=sr, channels=2) as r:
    for _ in range(int(sec * 10)):
        chunks.append(r.record(numframes=sr // 10))
a = np.clip(np.concatenate(chunks), -1, 1)
with wave.open(out, "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr); w.writeframes((a * 32767).astype("<i2").tobytes())
print("ok", out, a.shape[0] / sr, "s peak", float(np.abs(a).max()))
