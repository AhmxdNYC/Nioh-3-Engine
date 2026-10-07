"""Build Nioh3_Performance.xlsx from results.csv (raw measurements) plus summary sheets with formulas."""
import csv
from openpyxl import Workbook
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter

HERE = r"C:\Users\Ahmad Hamza\nioh3-engine\captures"
rows = list(csv.DictReader(open(fr"{HERE}\results.csv", newline="", encoding="utf-8")))
by_step = {int(r["step"]): r for r in rows}

F = "Arial"
H_FILL = PatternFill("solid", fgColor="1F3864")
H_FONT = Font(name=F, bold=True, color="FFFFFF")
T_FONT = Font(name=F, bold=True, size=14)
B_FONT = Font(name=F)
BOLD = Font(name=F, bold=True)
INPUT = Font(name=F, color="0000FF")          # measured values (hardcoded inputs)
GOOD = PatternFill("solid", fgColor="E2EFDA")
thin = Side(style="thin", color="BFBFBF")
BOX = Border(left=thin, right=thin, top=thin, bottom=thin)


def num(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def header(ws, r, cols):
    for c, name in enumerate(cols, 1):
        cell = ws.cell(r, c, name)
        cell.font, cell.fill, cell.border = H_FONT, H_FILL, BOX
        cell.alignment = Alignment(wrap_text=True, vertical="center")


def widths(ws, ws_widths):
    for i, w in enumerate(ws_widths, 1):
        ws.column_dimensions[get_column_letter(i)].width = w


wb = Workbook()

# ---------------- Summary: default vs modified vs modified+FG ----------------
s = wb.active
s.title = "Summary"
s["A1"] = "Nioh 3 (v2.02): default vs modified vs modified + frame generation"
s["A1"].font = T_FONT
s["A2"] = "Ryzen 5 7600X, RX 9070, 2560x1440 @ 240 Hz. Same spot, camera sweeping, 20 s PresentMon capture each. Blue = measured; game speed 0.60 for the Dynamic FPS row is an estimate (60 tier)."
s["A2"].font = Font(name=F, italic=True)
cols = ["Setup", "FPS on screen", "Real frames (rendered by game)", "1% low (on screen)", "CPU ms / frame",
        "GPU ms / frame", "Game speed (1.00 = real time)", "What changed"]
header(s, 4, cols)
STD = "Standard preset, 65% dynamic res, FSR3."
HI = "High/Highest settings, 75% fixed res, FSR4 via OptiScaler."
final = [  # (step, name, real frames rendered or None = same as on screen, game speed, what)
    (19, "Default, no frame gen (original 60 cap)", None, 1.0, f"{STD} No patches."),
    (22, "Default, no frame gen, 120 cap (stock Dynamic FPS on)", None, 0.6,
     f"{STD} No patches. Dynamic FPS kept the game in its 60 fps tier."),
    (23, "Default settings + engine patches, no frame gen", None, 1.0,
     f"{STD} All engine patches (they switch Dynamic FPS off)."),
    (21, "Modified, no frame gen", None, 1.0, f"{HI} All engine patches."),
    (16, "Default with frame gen (original setup)", 60, 1.0, f"{STD} 60 cap + FSR3 frame gen. No patches."),
    (18, "Modified + frame gen", 85, 1.0, f"{HI} All engine patches + FSR frame gen."),
]
for i, (step, name, rendered, speed, what) in enumerate(final):
    r = 5 + i
    d = by_step[step]
    s.cell(r, 1, name).font = BOLD
    s.cell(r, 1).alignment = Alignment(wrap_text=True)
    s.cell(r, 2, num(d["avg_fps_displayed"]))
    s.cell(r, 3, rendered if rendered is not None else f"=B{r}")
    for c, v in ((4, num(d["low1_fps_displayed"])), (5, num(d["cpu_busy_ms"])), (6, num(d["gpu_busy_ms"]))):
        s.cell(r, c, v)
    for c in range(2, 7):
        s.cell(r, c).number_format = "0.0"
        s.cell(r, c).font = B_FONT if c == 3 and rendered is None else INPUT
    s.cell(r, 7, speed).font = INPUT
    s.cell(r, 7).number_format = "0.00"
    s.cell(r, 8, what).font = B_FONT
    s.cell(r, 8).alignment = Alignment(wrap_text=True)
    for c in range(1, 9):
        s.cell(r, c).border = BOX
    s.row_dimensions[r].height = 30
last = 4 + len(final)

s[f"A{last + 2}"] = "Change in real frames vs stock at the 120 cap (row 6)"
s[f"A{last + 2}"].font = BOLD
header(s, last + 3, ["Comparison", "FPS on screen", "Real frames", "Graphics"])
cmp_rows = [("Default settings + engine patches", 7, "same settings"),
            ("Modified, no frame gen", 8, "much higher settings + FSR4"),
            ("Modified + frame gen", 10, "much higher settings + FSR4")]
for i, (name, src, gfx) in enumerate(cmp_rows):
    r = last + 4 + i
    s.cell(r, 1, name).font = BOLD
    s.cell(r, 2, f"=IFERROR(B{src}/$B$6-1,0)").number_format = "+0%;-0%;0%"
    s.cell(r, 3, f"=IFERROR(C{src}/$C$6-1,0)").number_format = "+0%;-0%;0%"
    s.cell(r, 4, gfx).font = B_FONT
    s.cell(r, 2).font = s.cell(r, 3).font = B_FONT
    for c in range(1, 5):
        s.cell(r, c).border = BOX
nr = last + 4 + len(cmp_rows) + 1
s[f"A{nr}"] = ("Notes: 'Real frames' = frames the game renders; frame gen roughly doubles what is on screen but only real frames "
               "lower input lag. Stock at the 120 cap varies by luck: in one session it stayed in the 120 tier (110 fps), "
               "in the back-to-back A/B it fell to the 60 tier (72-73 fps) because of Dynamic FPS. Patched never drops. "
               "The two frame-gen rows ran with the AMD overlay open (pacing looks worse).")
s[f"A{nr}"].font = Font(name=F, italic=True)
s[f"A{nr}"].alignment = Alignment(wrap_text=True, vertical="top")
s.merge_cells(f"A{nr}:H{nr}")
s.row_dimensions[nr].height = 60
widths(s, [44, 14, 14, 14, 12, 12, 16, 70])

# ---------------- Original setup -> modified ----------------
o = wb.create_sheet("Original vs Now")
o["A1"] = "Your original setup (60 Hz desktop) vs now"
o["A1"].font = T_FONT
header(o, 3, ["Metric", "Original (60 Hz desktop, your settings)", "Now: modified, no frame gen (clean session)", "Change"])
orig, now = by_step[0], by_step[11]
metrics = [("FPS on screen", "avg_fps_displayed"), ("1% low", "low1_fps_displayed"),
           ("CPU ms / frame", "cpu_busy_ms"), ("GPU ms / frame", "gpu_busy_ms")]
for i, (label, key) in enumerate(metrics):
    r = 4 + i
    o.cell(r, 1, label).font = BOLD
    o.cell(r, 2, num(orig[key])).font = INPUT
    o.cell(r, 3, num(now[key])).font = INPUT
    o.cell(r, 4, f"=IFERROR(C{r}/B{r}-1,0)").number_format = "+0%;-0%;0%"
    for c in range(1, 5):
        o.cell(r, c).border = BOX
o["A9"] = ("Original was capped at 60 by the 60 Hz desktop (frame-gen frames could never be shown). "
           "Most of this gain is setup (240 Hz, FSR4, settings); engine patches add correct game speed, fewer spikes and less CPU.")
o["A9"].font = Font(name=F, italic=True)
o["A9"].alignment = Alignment(wrap_text=True)
o.merge_cells("A9:D9")
o.row_dimensions[9].height = 36
widths(o, [20, 34, 40, 12])

# ---------------- Engine patch A/B (same session, back-to-back) ----------------
e = wb.create_sheet("Engine patches A-B")
e["A1"] = "Engine patches only: stock vs patched, back-to-back in the same session"
e["A1"].font = T_FONT
header(e, 3, ["Test", "Stock", "Patched", "Change", "Metric"])
pairs = [
    ("Clean pair 1 (120 cap)", 10, 11, "avg_fps_displayed", "FPS"),
    ("Clean pair 1 (120 cap)", 10, 11, "spikes_2x", "Stutter spikes (20 s)"),
    ("Clean pair 2 (120 cap)", 12, 13, "avg_fps_displayed", "FPS"),
    ("Clean pair 2 (120 cap)", 12, 13, "spikes_2x", "Stutter spikes (20 s)"),
    ("Profiler session (120 cap)", 2, 4, "low1_fps_displayed", "1% low"),
    ("Profiler session (120 cap)", 2, 4, "spikes_2x", "Stutter spikes (20 s)"),
    ("60 fps cap (from config)", 14, 15, "low1_fps_displayed", "1% low"),
]
for i, (name, a, b, key, label) in enumerate(pairs):
    r = 4 + i
    e.cell(r, 1, name).font = BOLD
    e.cell(r, 2, num(by_step[a][key])).font = INPUT
    e.cell(r, 3, num(by_step[b][key])).font = INPUT
    e.cell(r, 4, f"=IFERROR(C{r}/B{r}-1,0)").number_format = "+0%;-0%;0%"
    e.cell(r, 5, label).font = B_FONT
    for c in range(1, 6):
        e.cell(r, c).border = BOX
r = 4 + len(pairs)
e.cell(r, 1, "60 fps cap (from config)").font = BOLD
e.cell(r, 2, 6.32).font = INPUT
e.cell(r, 3, 4.69).font = INPUT
e.cell(r, 4, f"=IFERROR(C{r}/B{r}-1,0)").number_format = "+0%;-0%;0%"
e.cell(r, 5, "CPU cores busy (Nioh3 process)").font = B_FONT
for c in range(1, 6):
    e.cell(r, c).border = BOX
    e.cell(r, c).fill = GOOD
r += 1
e.cell(r, 1, "Game speed when missing 120").font = BOLD
e.cell(r, 2, 0.90).font = INPUT
e.cell(r, 3, 1.00).font = INPUT
e.cell(r, 4, f"=IFERROR(C{r}/B{r}-1,0)").number_format = "+0%;-0%;0%"
e.cell(r, 5, "1.00 = real time (stock runs in slow motion off-target)").font = B_FONT
for c in range(1, 6):
    e.cell(r, c).border = BOX
    e.cell(r, c).fill = GOOD
e.cell(r + 2, 1, "CPU cores: measured with patcher\\cpuuse.ps1 (15 s). Game speed: patcher\\ft_vrr_patch.fsx verify.").font = Font(name=F, italic=True)
widths(e, [30, 10, 10, 10, 50])

# ---------------- Second round: shader, frame-gen presenter, worker pools, CPU_SPEC_CLASS, physics ----------------
r2 = list(csv.DictReader(open(fr"{HERE}\rounds2-4.csv", newline="", encoding="utf-8")))
s2 = wb.create_sheet("Round 2 optimizations", 1)
s2["A1"] = "Second round of optimizations: verdicts"
s2["A1"].font = T_FONT
header(s2, 3, ["Optimization", "Without", "With", "Change", "Measured as", "Verdict"])


def mean(rows, key):
    v = [num(r[key]) for r in rows if num(r[key]) is not None]
    return sum(v) / len(v) if v else None


def pick(rnd, runs, f1=None):
    return [r for r in r2 if r["round"] == str(rnd) and r["run"] in runs and r["valid"] == "yes"
            and (f1 is None or r["f1"] == f1)]


verdicts = [
    ("Fast shadow shader (no frame gen)", mean(pick(3, {"G0a", "G0b"}), "avg_fps"), mean(pick(3, {"G1a", "G1b"}), "avg_fps"),
     "avg fps, modified preset, 2 vs 2 launches", "KEEP (on by default)"),
    ("Fast shadow shader (no frame gen)", mean(pick(3, {"G0a", "G0b"}), "gpu_busy_ms"), mean(pick(3, {"G1a", "G1b"}), "gpu_busy_ms"),
     "GPU ms per frame", "KEEP"),
    ("Fast shadow shader (frame gen on)", mean(pick(2, {"A1", "A2", "B1", "B2"}), "avg_fps"), mean(pick(2, {"C1", "C2"}), "avg_fps"),
     "on-screen fps, 8 vs 4 runs", "KEEP"),
    ("Frame-gen presenter fix (F1)", mean(pick(2, {"A1", "A2", "B1", "B2", "C1", "C2"}, "F1 off"), "cores"),
     mean(pick(2, {"A1", "A2", "B1", "B2", "C1", "C2"}, "F1 on"), "cores"), "CPU cores busy, 6 launches", "KEEP (power)"),
    ("Worker-pool fix (P12)", mean(pick(3, {"P0b"}), "avg_fps"), mean(pick(3, {"P1b"}), "avg_fps"),
     "avg fps, default settings, no FG", "OFF (no gain)"),
    ("CPU_SPEC_CLASS = HIGH", mean(pick(3, {"P0b"}), "avg_fps"), mean(pick(3, {"S1a", "S1b"}), "avg_fps"),
     "avg fps, default settings, no FG", "NO EFFECT"),
    ("Physics on 4 threads", mean(pick(4, {"phys2"}), "avg_fps"), mean(pick(4, {"phys4"}), "avg_fps"),
     "avg fps at a physics-free spot", "OPTIONAL (stable, unproven)"),
    ("Parallel object update (0 -> 32 units)", mean(pick(6, {"A1", "A2"}), "avg_fps"), mean(pick(6, {"B1", "B2"}), "avg_fps"),
     "avg fps, default settings, no FG, 2 vs 2", "OFF (no gain; main thread unchanged)"),
    ("Far foliage/prop distance x0.6", mean(pick(7, {"A1", "A2"}), "avg_fps"), mean(pick(7, {"B1", "B2"}), "avg_fps"),
     "avg fps, default settings, no FG, 2 vs 2", "OPTIONAL (small gain, slight far pop-in)"),
    ("Far foliage/prop distance x0.6", mean(pick(7, {"A1", "A2"}), "low1_fps"), mean(pick(7, {"B1", "B2"}), "low1_fps"),
     "1% low fps", "OPTIONAL"),
]
for i, (name, a, b, how, verdict) in enumerate(verdicts):
    r = 4 + i
    s2.cell(r, 1, name).font = BOLD
    for c, v in ((2, a), (3, b)):
        cell = s2.cell(r, c, round(v, 2) if v is not None else None)
        cell.font, cell.number_format = INPUT, "0.00"
    s2.cell(r, 4, f"=IFERROR(C{r}/B{r}-1,0)").number_format = "+0.0%;-0.0%;0.0%"
    s2.cell(r, 4).font = B_FONT
    s2.cell(r, 5, how).font = B_FONT
    s2.cell(r, 6, verdict).font = BOLD
    for c in range(1, 7):
        s2.cell(r, c).border = BOX
        if verdict.startswith("KEEP"):
            s2.cell(r, c).fill = GOOD
nr = 4 + len(verdicts) + 1
s2.cell(nr, 1, "Blue = mean of the measured runs (raw runs below). For GPU ms and CPU cores, a negative change is better.").font = Font(name=F, italic=True)
header(s2, nr + 2, ["Round", "Run", "Setup", "F1", "Valid", "Avg fps", "1% low", "Spikes", "GPU ms", "CPU ms", "Cores"])
for i, rr in enumerate(r2):
    for c, k in enumerate(["round", "run", "setup", "f1", "valid", "avg_fps", "low1_fps", "spikes", "gpu_busy_ms", "cpu_busy_ms", "cores"], 1):
        v = rr[k]
        n_ = num(v)
        cell = s2.cell(nr + 3 + i, c, n_ if n_ is not None and k not in ("run", "setup", "f1", "valid") else v)
        cell.font = B_FONT
widths(s2, [36, 12, 12, 10, 44, 28, 9, 9, 9, 9, 9])

# ---------------- All runs (raw) ----------------
a = wb.create_sheet("All runs (raw)")
keys = list(rows[0].keys())
header(a, 1, keys)
for i, rrow in enumerate(rows, 2):
    for c, k in enumerate(keys, 1):
        v = rrow[k]
        n = num(v)
        cell = a.cell(i, c, n if n is not None and k not in ("step",) else (int(v) if k == "step" and v else v))
        cell.font = B_FONT
widths(a, [6, 48, 17, 40, 28, 10, 10, 10, 9, 10, 9, 9, 14, 30, 60])
a.freeze_panes = "C2"

# ---------------- Notes ----------------
n = wb.create_sheet("Notes")
notes = [
    ("Hardware", "Ryzen 5 7600X (6c/12t), 32 GB DDR5-4800, Radeon RX 9070, 2560x1440 240 Hz, NVMe, Windows 11"),
    ("Game", "Nioh 3 v2.02 (Steam build 25297068)"),
    ("Tool", "Intel PresentMon 2.6; 20 s capture while the camera sweeps left/right at the same spot"),
    ("Colours", "Blue numbers = measured values; black = formulas"),
    ("Fair comparisons", "Only back-to-back pairs in the same session; in-game time of day changes GPU load over a session"),
    ("Profiler rows", "Rows marked devmode_overhead=yes ran with AMD's GPU profiler attached (small overhead)"),
    ("Raw data", "captures\\results.csv and captures\\bench-*.csv"),
]
header(n, 1, ["Item", "Detail"])
for i, (k, v) in enumerate(notes, 2):
    n.cell(i, 1, k).font = BOLD
    n.cell(i, 2, v).font = B_FONT
widths(n, [20, 110])

wb.calculation.fullCalcOnLoad = True   # Excel computes every formula on open
out = fr"{HERE}\Nioh3_Performance.xlsx"
wb.save(out)
print("saved", out)
