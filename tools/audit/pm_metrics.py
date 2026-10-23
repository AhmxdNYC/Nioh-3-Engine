"""PresentMon CSV -> one JSON line {ok, avg_fps, low1_fps (1000/p99 frame time), frametime_ms (mean), frames, min_fps}.
Rendered presents (MsBetweenPresents); FG is OFF in config.xml for the audit."""
import csv
import json
import sys


def main(p):
    try:
        rows = list(csv.DictReader(open(p, encoding="utf-8-sig")))
        col = "MsBetweenPresents" if rows and "MsBetweenPresents" in rows[0] else "FrameTime"
        ft = sorted(float(r[col]) for r in rows if r.get(col) not in (None, "", "NA"))
        ft = [x for x in ft if x > 0]
        if len(ft) < 600:
            return {"ok": False, "err": "only %d frames" % len(ft)}
        mean = sum(ft) / len(ft)
        p99 = ft[min(len(ft) - 1, int(0.99 * len(ft)))]
        return {"ok": True, "avg_fps": round(1000 / mean, 3), "low1_fps": round(1000 / p99, 3),
                "frametime_ms": round(mean, 4), "frames": len(ft), "min_fps": round(1000 / ft[-1], 3), "col": col}
    except Exception as e:
        return {"ok": False, "err": str(e)}


print(json.dumps(main(sys.argv[1])))
