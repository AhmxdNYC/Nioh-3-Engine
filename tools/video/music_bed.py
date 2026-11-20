# original music bed for the showcase: taiko drums + plucked koto-like line in the Japanese In scale, 120 BPM, synthesized here
import numpy as np, wave, sys
sr = 48000; bpm = 120; beat = 60 / bpm; dur = float(sys.argv[2]) if len(sys.argv) > 2 else 48
n = int(sr * dur); L = np.zeros(n); R = np.zeros(n); rng = np.random.default_rng(7)
def add(sig, t, pan=0.5, g=1.0):
    i = int(t * sr); j = min(n, i + len(sig))
    if i >= n: return
    L[i:j] += sig[:j - i] * g * (1 - pan) * 2 ** 0.5 / 1.4; R[i:j] += sig[:j - i] * g * pan * 2 ** 0.5 / 1.4
def taiko(f0=55, d=0.9):
    t = np.arange(int(sr * d)) / sr; f = f0 * (1 + 1.5 * np.exp(-t * 30))
    body = np.sin(2 * np.pi * np.cumsum(f) / sr) * np.exp(-t * 5)
    hit = rng.standard_normal(len(t)) * np.exp(-t * 60) * 0.35
    return np.tanh((body + hit) * 1.6)
def rim(d=0.12):
    t = np.arange(int(sr * d)) / sr; return rng.standard_normal(len(t)) * np.exp(-t * 80) * 0.5
def pluck(freq, d=1.6):
    N = int(sr / freq); buf = rng.uniform(-1, 1, N); out = np.zeros(int(sr * d))
    for k in range(len(out)):
        out[k] = buf[k % N]; buf[k % N] = 0.996 * 0.5 * (buf[k % N] + buf[(k + 1) % N])
    return out * 0.6
def drone(freq, d):
    t = np.arange(int(sr * d)) / sr
    s = sum(np.sin(2 * np.pi * freq * h * t + h) / h for h in (1, 2, 3)) * 0.12
    return s * np.minimum(1, t / 2) * np.minimum(1, (d - t) / 2)
In = [329.63, 349.23, 440.0, 493.88, 523.25, 659.26, 698.46]   # E F A B C E F
add(drone(82.41, dur), 0, 0.5, 1.0)
bars = int(dur / (4 * beat))
for b in range(bars):
    t0 = b * 4 * beat; intense = b >= 2
    add(taiko(), t0, 0.45, 1.0); add(taiko(62, 0.6), t0 + 2 * beat, 0.55, 0.8)
    if intense:
        add(taiko(70, 0.4), t0 + 1.5 * beat, 0.5, 0.5); add(taiko(), t0 + 3 * beat, 0.5, 0.7); add(taiko(70, 0.4), t0 + 3.5 * beat, 0.5, 0.6)
    for q in range(8): add(rim(), t0 + q * beat / 2, 0.7 if q % 2 else 0.3, 0.25 if intense else 0.12)
    if b >= 1:
        mel = [0, 2, 3, 2, 4, 3, 1, 0] if b % 2 else [3, 4, 5, 4, 3, 2, 3, 0]
        for q, idx in enumerate(mel):
            if q % 2 == 0 or intense: add(pluck(In[idx]), t0 + q * beat / 2, 0.35 + 0.3 * (q % 2), 0.5)
fade = np.ones(n); fl = int(sr * 2.5); fade[-fl:] = np.linspace(1, 0, fl); fade[:int(sr * .5)] = np.linspace(0, 1, int(sr * .5))
a = np.stack([L, R], 1) * fade[:, None]; a = a / np.abs(a).max() * 0.9
with wave.open(sys.argv[1], "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr); w.writeframes((a * 32767).astype("<i2").tobytes())
print("ok", dur)
