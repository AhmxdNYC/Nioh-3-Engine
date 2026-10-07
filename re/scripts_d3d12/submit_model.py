r"""submit_model.py -- submission-timing model from the RGP QueueEvent chunk (notes/gpu-scheduling.md, topic B).

    py -I re\scripts_d3d12\submit_model.py <trace.rgp> [--cb-overhead-us 8] [--split-us 250]

For every frame with GFX submits it reports, from the CPU submit times and the GPU start/end of each list:
  * first-list idle split:  'cpu_late' = GPU idle because nothing was submitted yet,
                            'gated'    = GPU idle although a list was already queued (a queue Wait / paging stall);
  * holes = gaps between consecutive GFX submits (> 300 us);
  * a no-backlog model of the same frame (the GPU starts each list as soon as it is submitted and the previous one is
    done; i.e. the CPU-bound or latency-1-serialised case, where submission timing matters):
        end_now   = GPU end with today's list boundaries
        end_fluid = GPU end if every list's work became available linearly while it was being recorded
                    (the limit of "submit more often"), plus --cb-overhead-us per extra list boundary
                    (one extra boundary per --split-us of recording time);
    gain = end_now - end_fluid  (what earlier / more frequent ExecuteCommandLists could save at most).
GPU durations are the queue-event spans (they include in-list waits); the model ignores other queues.
"""
import argparse, struct, sys, collections
sys.path.insert(0, r"C:\Users\Ahmad Hamza\nioh3-engine\patcher\gpu")
from rdf import RDF  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("trace")
ap.add_argument("--cb-overhead-us", type=float, default=8.0)
ap.add_argument("--split-us", type=float, default=250.0)
ap.add_argument("--gpu-scale", type=float, default=1.0, help="scale GPU list durations (emulate a faster/slower GPU or lighter settings)")
a = ap.parse_args()

r = RDF(a.trace)
_, ccal, gcal = struct.unpack_from("<3Q", r.data(r.find("ClockCalibration")[0]), 0)
qtype = {}
for c in r.find("QueueInfo"):
    q = struct.unpack_from("<4Q", r.data(c), 0)
    qtype[q[1]] = {1: "GFX", 2: "CMP", 3: "DMA"}.get(q[3] & 0xFF, "?") + str(c.index)
ev = []
for c in r.find("QueueEvent"):
    q = struct.unpack_from("<9Q", r.data(c), 0)
    ev.append(dict(cpu=(gcal + (q[6] - ccal) * 10) / 100.0, typ=q[2] & 0xFFFFFFFF, frame=q[3],
                   q=qtype.get(q[1], "?"), g0=q[7] / 100.0, g1=q[8] / 100.0))
ev.sort(key=lambda e: e["cpu"])
t0 = min(e["g0"] for e in ev if e["typ"] == 0 and e["g0"])
for e in ev:
    e["cpu"] -= t0; e["g0"] -= t0; e["g1"] -= t0

gfxq = collections.Counter(e["q"] for e in ev if e["typ"] == 0 and e["q"].startswith("GFX")).most_common(1)[0][0]
frames = collections.defaultdict(list)
for e in ev:
    if e["typ"] == 0 and e["q"] == gfxq and e["g0"] > -1e8:
        frames[e["frame"]].append(e)
presents = [e for e in ev if e["typ"] == 3]
prev_end = None
tot = collections.Counter()
nf = 0
print(f"GFX queue {gfxq}; queues seen: {sorted(set(e['q'] for e in ev))}")
print(f"{'frame':>6} {'lists':>5} {'rec_ms':>6} {'cpu_late':>8} {'gated':>7} {'holes>300us':>11} {'now_ms':>7} {'fluid_ms':>8} {'gain_us':>7}")
for fr in sorted(frames):
    L = sorted(frames[fr], key=lambda e: e["cpu"])
    if len(L) < 8:
        continue
    first = L[0]
    cpu_late = gated = 0.0
    pe = prev_end
    for e in L:  # every list of the frame: idle before it = waiting for the CPU (cpu_late) or with work queued (gated)
        if pe is not None and e["g0"] > pe:
            idle = e["g0"] - pe
            cl = max(0.0, min(idle, e["cpu"] - pe))
            cpu_late += cl; gated += idle - cl
        pe = max(pe or e["g1"], e["g1"])
    holes = [L[i]["cpu"] - L[i - 1]["cpu"] for i in range(1, len(L)) if L[i]["cpu"] - L[i - 1]["cpu"] > 300]
    # no-backlog model: GPU free at the first submit
    durs = [max(0.0, e["g1"] - e["g0"]) * a.gpu_scale for e in L]
    t = L[0]["cpu"]
    for e, d in zip(L, durs):
        t = max(t, e["cpu"]) + d
    end_now = t
    # fluid: list i's work arrives uniformly over [cpu_{i-1}, cpu_i]
    t = L[0]["cpu"]; extra = 0
    for i, (e, d) in enumerate(zip(L, durs)):
        start_rec = L[i - 1]["cpu"] if i else e["cpu"]
        span = max(0.0, e["cpu"] - start_rec)
        if span <= 0 or d <= 0:
            t = max(t, e["cpu"]) + d
            continue
        # work arrives at rate d/span during [start_rec, e.cpu]; GPU processes at rate 1
        t = max(t, start_rec)
        avail_end = e["cpu"]
        t = max(t + d, avail_end)  # cannot finish before the last piece arrives
        extra += int(span // a.split_us)
    end_fluid = t + extra * a.cb_overhead_us
    gain = end_now - end_fluid
    print(f"{fr:6d} {len(L):5d} {(L[-1]['cpu'] - L[0]['cpu']) / 1000:6.2f} {cpu_late:8.0f} {gated:7.0f} "
          f"{len(holes):3d} ({sum(holes)/1000:4.1f}ms) {(end_now - L[0]['cpu']) / 1000:7.2f} {(end_fluid - L[0]['cpu']) / 1000:8.2f} {gain:7.0f}")
    if prev_end is not None:
        tot["cpu_late"] += cpu_late; tot["gated"] += gated; tot["gain"] += gain; nf += 1
    prev_end = max(e["g1"] for e in L)
if nf:
    print(f"mean over {nf} frames: cpu_late {tot['cpu_late']/nf:.0f} us, gated {tot['gated']/nf:.0f} us, "
          f"fluid-submission gain (no-backlog model) {tot['gain']/nf:.0f} us")
