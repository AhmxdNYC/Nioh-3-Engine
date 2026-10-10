#!/usr/bin/env python3
"""extraperf_table.py - one markdown row per item from fastab -Shots sessions (results.json + pairs\\pairs.json from shotpairs.py).

  py -I tools\\testmatrix\\extraperf_table.py <session dir> [...]

Columns: item, fps A -> B, fps gain % +- SE (mod frame clock, paired ABBA), GPU busy delta ms +- SE (PresentMon, sampled),
look change (mean abs diff / changed pixels vs the session's A-vs-A noise floor), screenshot pair path.
"""
import json
import pathlib
import sys

print("| item | fps A -> B | fps gain % (SE) | GPU busy ms (SE) | look: mean abs diff / px changed (noise) | pair |")
print("|---|---|---|---|---|---|")
for s in sys.argv[1:]:
    s = pathlib.Path(s)
    res = json.loads((s / "results.json").read_text(encoding="utf-8"))["items"]
    pp = s / "pairs" / "pairs.json"
    pairs = json.loads(pp.read_text(encoding="utf-8")) if pp.exists() else {}
    nz = pairs.get("_noise_A_vs_A_median", {})
    for it, m in res.items():
        f, g = m.get("fps", {}), m.get("gpu_ms", {})
        se_pct = 100.0 * f.get("se", 0) / f["a"] if f.get("a") else 0
        p = pairs.get(it, {})
        look = ("%.1f / %.0f %% (%.1f / %.0f %%)" % (p["mean_abs_diff"], p["changed_pct"], nz.get("mean_abs_diff", 0), nz.get("changed_pct", 0))
                if p else "-")
        gpu = "%+.3f (%.3f)" % (g["delta"], g["se"]) if g else "-"
        print("| %s | %.1f -> %.1f | %+.2f (%.2f) | %s | %s | `%s` |" % (
            it, f.get("a", 0), f.get("b", 0), f.get("pct", 0), se_pct, gpu, look,
            str(pathlib.Path(p["pair"]).relative_to(s.parent.parent.parent.parent)) if p else "-"))
