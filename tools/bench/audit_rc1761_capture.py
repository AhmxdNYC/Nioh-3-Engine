"""Read-only external-capture audit; no extraction, launch or report mutation."""
import csv
import io
import json
import math
import statistics
import zipfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def stamp(value):
    main, _, fraction = value.strip().replace("T", " ").removesuffix("Z").partition(".")
    return datetime.strptime(main, "%Y-%m-%d %H:%M:%S").replace(tzinfo=timezone.utc).timestamp() + (float("0." + fraction) if fraction else 0)


with zipfile.ZipFile(ROOT / "captures/pc2/rc1761/pc2_rc1761.zip") as archive:
    rows = list(csv.DictReader(io.StringIO(archive.read("ab/presentmon_pc2.csv").decode("utf-8-sig"))))
    assert {(r["Application"], r["ProcessID"], r["SwapChainAddress"]) for r in rows} == {("Nioh3.exe", "21868", "0x16F31DCB020")}
    partial = [r for r in rows if None in r.values()]
    assert len(partial) == 1 and partial[0] is rows[-1]
    rows = [r for r in rows if None not in r.values()]
    manifest = [json.loads(s) for s in archive.read("ab/manifest.jsonl").decode().splitlines() if s.strip()]
    windows = [r for r in manifest if r.get("kind") == "window"]
    spans = [(stamp(r["t0_utc"]), stamp(r["t1_utc"])) for r in windows]
    times = [stamp(r["CPUStartDateTime"]) for r in rows]
    scores = {h: sum(a <= t + h * 3600 <= b for t in times for a, b in spans) for h in range(-14, 15)}
    best = max(scores, key=scores.get)
    assert best == 14 and sum(n == scores[best] for n in scores.values()) == 1
    assert scores[0] == 0
    print(f"pc2: sole whole-hour overlap candidate +{best}h, {scores[best]} rows; unshifted 0; one truncated terminal row excluded")
    print("Offset inferred from overlap, not an independently archived UTC clock calibration.")
    reports = json.loads(archive.read("ab/results.json"))["items"]
    grouped = {}
    for window, (a, b) in zip(windows, spans):
        selected = [r for r, t in zip(rows, times) if a <= t + best * 3600 <= b]
        assert len(selected) >= 200
        assert all(r["GPUBusy"] not in ("NA", "") for r in selected)
        mean = statistics.mean(float(r["GPUBusy"]) for r in selected)
        grouped.setdefault(window["item"], {}).setdefault(window["pair"], {})[window["arm"]] = mean
        coverage = len(selected) / window["clock"]["mod_frames"]
        gaps = [stamp(y["CPUStartDateTime"]) - stamp(x["CPUStartDateTime"]) for x, y in zip(selected, selected[1:])]
        print(f"  {window['tag']}: {len(selected)} samples; count/mod-clock {coverage:.3f}; max timestamp gap {max(gaps):.3f}s")
    for item, pairs in grouped.items():
        a = statistics.mean(p["A"] for p in pairs.values())
        b = statistics.mean(p["B"] for p in pairs.values())
        se = statistics.stdev(p["B"] - p["A"] for p in pairs.values()) / math.sqrt(len(pairs))
        calculated = {"a": a, "b": b, "delta": b - a, "se": se, "ci": se * 4.303, "pct": (b - a) / a * 100}
        assert all(math.isclose(value, reports[item]["gpu_ms"][key], rel_tol=1e-9, abs_tol=1e-9) for key, value in calculated.items())
        print(f"  {item}: sampled GPUBusy {a:.6f}->{b:.6f}ms; delta {b-a:+.6f} +/-{se:.6f} SE; archived arithmetic PASS")
    print("FrameTime, GPUTime, GPUBusy and DisplayedTime are separate columns; archived fps/frame_ms use the mod clock, not DisplayedTime.")

with zipfile.ZipFile(ROOT / "captures/ally/rc1761/ally_rc1761.zip") as archive:
    lines = archive.read("up/mangohud.csv").decode().splitlines()
    rows = list(csv.DictReader(lines[2:]))
    elapsed = [int(r["elapsed"]) for r in rows]
    differences = [b - a for a, b in zip(elapsed, elapsed[1:])]
    assert all(d > 0 for d in differences)
    assert "GPUBusy" not in rows[0] and "gpu_load" in rows[0]
    reports = json.loads(archive.read("up/results.json"))["items"]
    assert all(report["gpu_ms"] is None for report in reports.values())
    print(f"Ally: {len(rows)} relative elapsed samples; range {elapsed[0]}..{elapsed[-1]}; median step {statistics.median(differences):.0f}")
    print("Ally alignment unresolved: CSV has no UTC origin/unit declaration; mod startup is not proof of capture origin. gpu_load is not GPU busy ms; archived no-data verdict retained.")

print("PASS: capture schema and sampled pc2 GPU arithmetic; inferred pc2 alignment only, Ally alignment unresolved; no display/latency/whole-mod verdict")
