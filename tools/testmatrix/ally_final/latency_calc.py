#!/usr/bin/env python3
"""latency_calc.py - button-to-photon latency from 240 fps slow-motion frame counts.
  python3 latency_calc.py latency.csv [--fps 240]
CSV columns: config,press,frame_press,frame_change,note   (config = STOCK | OPTIMIZED | OPTIMIZED_FG; frames are video frame numbers;
note may contain "bad" to exclude a press). ms = (frame_change - frame_press) * 1000 / fps. Prints the table per config:
n, median, mean, p90, min, max, stdev; and the delta of each config vs STOCK (median)."""
import csv, statistics, sys


def main():
    a = sys.argv[1:]
    fps = float(a[a.index("--fps") + 1]) if "--fps" in a else 240.0
    rows = [r for r in csv.DictReader(open(a[0], newline="", encoding="utf-8-sig"))
            if r.get("frame_press", "").strip() and r.get("frame_change", "").strip() and "bad" not in r.get("note", "").lower()]
    by = {}
    for r in rows:
        by.setdefault(r["config"].strip(), []).append((int(r["frame_change"]) - int(r["frame_press"])) * 1000 / fps)
    print("| config | n | median_ms | mean_ms | p90_ms | min | max | stdev | d_median_vs_STOCK |")
    print("|---|---|---|---|---|---|---|---|---|")
    base = statistics.median(by["STOCK"]) if "STOCK" in by else None
    for c in ("STOCK", "OPTIMIZED", "OPTIMIZED_FG"):
        xs = by.get(c)
        if not xs:
            continue
        xs.sort(); med = statistics.median(xs)
        p90 = xs[min(len(xs) - 1, int(len(xs) * .9))]
        d = "" if base is None or c == "STOCK" else f"{med - base:+.1f}"
        flag = "" if len(xs) >= 20 else "  (<20 presses: NOT VALID)"
        print(f"| {c} | {len(xs)} | {med:.1f} | {statistics.mean(xs):.1f} | {p90:.1f} | {xs[0]:.1f} | {xs[-1]:.1f} | "
              f"{statistics.pstdev(xs):.1f} | {d} |{flag}")
    print(f"\nresolution = {1000 / fps:.2f} ms per video frame; differences under ~4 ms (1 frame) are inside the quantisation.")


if __name__ == "__main__":
    main()
