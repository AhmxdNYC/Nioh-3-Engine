"""Cost-vs-looks report for the round 8 settings sweep.
    py sweep_report.py <round8 task output>
Per setting: avg fps, GPU busy ms, CPU busy ms, cores (from the task log) vs the mean of the baseline launches, and
image difference of its load-in screenshot vs the baselines (SSIM on 8x8 windows of a 1280x720 grayscale image;
1.0 = identical) next to the baseline-vs-baseline noise floor.
Writes captures\sweep\report.csv and prints a table.
"""
import csv, os, re, sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SW = os.path.join(HERE, "sweep")


def img(name):
    p = os.path.join(SW, name + ".png")
    if not os.path.exists(p):
        return None
    return np.asarray(Image.open(p).convert("L").resize((1280, 720), Image.BILINEAR), dtype=np.float64)


def box(a, k=8):
    h, w = (a.shape[0] // k) * k, (a.shape[1] // k) * k
    return a[:h, :w].reshape(h // k, k, w // k, k).mean(axis=(1, 3))


def ssim(a, b):
    c1, c2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    ma, mb = box(a), box(b)
    va, vb = box(a * a) - ma ** 2, box(b * b) - mb ** 2
    cov = box(a * b) - ma * mb
    s = ((2 * ma * mb + c1) * (2 * cov + c2)) / ((ma ** 2 + mb ** 2 + c1) * (va + vb + c2))
    return float(s.mean())


def mad(a, b):
    return float(np.abs(a - b).mean())


runs, cur = [], None
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    m = re.match(r"=== (\S+)\s+(.*?)\s+\d\d:\d\d:\d\d", line.strip())
    if m:
        cur = {"run": m.group(1), "change": m.group(2)}
        runs.append(cur)
        continue
    if not cur:
        continue
    m = re.search(r"displayed interval: avg\s+[\d.]+ ms \(\s*([\d.]+) fps\).*1%low\s+([\d.]+) fps", line)
    if m:
        cur["fps"], cur["low1"] = float(m.group(1)), float(m.group(2))
    m = re.search(r"GPU busy \(app frames\): avg\s+([\d.]+)", line)
    if m:
        cur["gpu"] = float(m.group(1))
    m = re.search(r"CPU busy \(app frames\): avg\s+([\d.]+)", line)
    if m:
        cur["cpu"] = float(m.group(1))
    m = re.search(r"Nioh3 CPU: ([\d.]+) cores", line)
    if m:
        cur["cores"] = float(m.group(1))

bases = [r for r in runs if r["run"].startswith("base") and "fps" in r]
def bmean(k):
    v = [r[k] for r in bases if k in r]
    return sum(v) / len(v) if v else float("nan")
B = {k: bmean(k) for k in ("fps", "gpu", "cpu", "cores", "low1")}
bimgs = [(r["run"], img(r["run"])) for r in bases]
bimgs = [(n, a) for n, a in bimgs if a is not None]
noise = [ssim(bimgs[i][1], bimgs[j][1]) for i in range(len(bimgs)) for j in range(i + 1, len(bimgs))]
noise_mad = [mad(bimgs[i][1], bimgs[j][1]) for i in range(len(bimgs)) for j in range(i + 1, len(bimgs))]
print("baseline mean: fps %.1f  gpu %.2f ms  cpu %.2f ms  cores %.2f   (n=%d)" % (B["fps"], B["gpu"], B["cpu"], B["cores"], len(bases)))
print("noise floor (baseline vs baseline): SSIM %s  MAD %s" % (
    ", ".join("%.4f" % x for x in noise), ", ".join("%.2f" % x for x in noise_mad)))
rows = []
for r in runs:
    a = img(r["run"])
    s = [ssim(a, b) for _, b in bimgs if b is not a] if a is not None else []
    md = [mad(a, b) for _, b in bimgs if b is not a] if a is not None else []
    row = dict(run=r["run"], change=r["change"], fps=r.get("fps"), low1=r.get("low1"), gpu=r.get("gpu"), cpu=r.get("cpu"),
               cores=r.get("cores"),
               dfps=(r["fps"] / B["fps"] - 1) * 100 if "fps" in r else None,
               dgpu=r["gpu"] - B["gpu"] if "gpu" in r else None,
               dcpu=r["cpu"] - B["cpu"] if "cpu" in r else None,
               ssim=max(s) if s else None, mad=min(md) if md else None)
    rows.append(row)
with open(os.path.join(SW, "report.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, list(rows[0].keys()))
    w.writeheader()
    w.writerows(rows)
print("%-11s %-40s %6s %6s %7s %7s %7s %7s" % ("run", "change", "fps", "dfps%", "dGPUms", "dCPUms", "SSIM", "MAD"))
f = lambda v, p: ("%" + p) % v if v is not None else "-"
for r in rows:
    print("%-11s %-40s %6s %6s %7s %7s %7s %7s" % (r["run"], r["change"][:40], f(r["fps"], ".1f"), f(r["dfps"], "+.1f"),
          f(r["dgpu"], "+.2f"), f(r["dcpu"], "+.2f"), f(r["ssim"], ".4f"), f(r["mad"], ".2f")))
