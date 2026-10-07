r"""queue_events.py -- decode the RGP QueueInfo / QueueEvent / ClockCalibration chunks (CPU submit timestamps).

    py -I queue_events.py <trace.rgp> [frame_t0_gpu_hex]

Layout found empirically in this driver's RDF (72-byte QueueEvent, all qwords):
  q0 hdr | q1 queue context (matches QueueInfo q1) | q2 lo32 = type (0 submit, 3 present), hi32 = sqtt cb id |
  q3 frame index | q4 ? | q5 api object (command list / swapchain) | q6 CPU QPC (10 MHz) | q7 GPU start | q8 GPU end (100 MHz)
ClockCalibration: q1 CPU QPC, q2 GPU ticks (same instant). CPU times are mapped to the GPU base with it.
Prints, per event: CPU submit time, GPU start/end (all in us relative to the first GPU start of the frame
with the most events), and 'lag' = GPU start - CPU submit (small lag after an idle GPU = the GPU was waiting on the CPU).
"""
import sys, struct, collections
sys.path.insert(0, r"C:\Users\Ahmad Hamza\nioh3-engine\patcher\gpu")
from rdf import RDF  # noqa: E402

r = RDF(sys.argv[1])
cal = r.find("ClockCalibration")[0]
_, ccal, gcal = struct.unpack_from("<3Q", r.data(cal), 0)
queues = {}
for c in r.find("QueueInfo"):
    q = struct.unpack_from("<4Q", r.data(c), 0)
    queues[q[1]] = (c.index, "GFX" if (q[3] & 0xFF) == 1 else f"CMP{c.index}")
ev = []
for c in r.find("QueueEvent"):
    q = struct.unpack_from("<9Q", r.data(c), 0)
    typ, cb = q[2] & 0xFFFFFFFF, q[2] >> 32
    cpu_g = gcal + (q[6] - ccal) * 10          # CPU time in GPU ticks
    ev.append(dict(cpu=cpu_g, typ=typ, cb=cb, frame=q[3], q=queues.get(q[1], (-1, hex(q[1])))[1], api=q[5], g0=q[7], g1=q[8]))
ev.sort(key=lambda e: e["cpu"])
frames = collections.Counter(e["frame"] for e in ev)
print("frames:", dict(frames))
starts = [e["g0"] for e in ev if e["g0"] and e["typ"] == 0]
t0 = int(sys.argv[2], 16) if len(sys.argv) > 2 else min(starts)
names = {0: "SUBMIT", 1: "SIGNAL", 2: "WAIT", 3: "PRESENT"}
us = lambda t: (t - t0) / 100.0  # noqa: E731  (100 MHz ticks -> us)
print(f"{'cpu_us':>9} {'type':7} {'queue':6} {'frame':>5} {'cb':>7} {'gpu_start':>9} {'gpu_end':>9} {'dur':>7} {'lag':>7}  api")
last_end = {}
for e in ev:
    g0 = us(e["g0"]) if e["g0"] else float("nan")
    g1 = us(e["g1"]) if e["g1"] else float("nan")
    idle = ""
    le = last_end.get(e["q"])
    if e["g0"] and le is not None and e["g0"] > le:
        idle = f"  idle-before {(e['g0'] - le) / 100.0:.1f}"
    if e["g1"]:
        last_end[e["q"]] = max(last_end.get(e["q"], 0), e["g1"])
    print(f"{us(e['cpu']):9.1f} {names.get(e['typ'], e['typ']):7} {e['q']:6} {e['frame']:5d} {e['cb']:7x} {g0:9.1f} {g1:9.1f} "
          f"{(g1 - g0):7.1f} {(g0 - us(e['cpu'])):7.1f}  {e['api']:#x}{idle}")
