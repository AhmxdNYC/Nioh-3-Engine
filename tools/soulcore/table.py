# Turn a survey's cores.jsonl into one markdown table of universal soul-core timings (human #134924).
# Usage: py -I table.py OUT_DIR\cores.jsonl [> table.md]
# Groups: good, flagged (form ends before its last hit, #134324), no hit, never threw.
import json, sys


def load(path):
    rows = {}
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if "idx" not in r:
            continue
        old = rows.get(r["idx"])
        if old is None or (r.get("use2") and not old.get("use2")):
            rows[r["idx"]] = r  # a kept second use wins over an earlier miss
    return list(rows.values())


def f(v):
    return "-" if v is None else "%.2f" % v


def group(r):
    if not r.get("use2") or r.get("end_s") is None:
        return "never threw"
    if r.get("early"):
        return "flagged"
    if not r.get("bursts"):
        return "no hit"
    return "good"


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: table.py cores.jsonl")
    rows = load(sys.argv[1])
    order = ["flagged", "good", "no hit", "never threw"]
    by = {g: [] for g in order}
    for r in rows:
        by[group(r)].append(r)
    print("%d rows: %s" % (len(rows), ", ".join("%d %s" % (len(by[g]), g) for g in order)))
    for g in order:
        if not by[g]:
            continue
        print("\n## %s (%d)\n" % (g, len(by[g])))
        print("| Yokai | Item | Idx | End s | Why | Bursts | First hit | Last hit | After close | Note |")
        print("|---|---|---|---|---|---|---|---|---|---|")
        for r in sorted(by[g], key=lambda r: (r.get("end_s") is None, r.get("end_s") or 0)):
            note = r.get("press", "") if g == "never threw" else ("post-exit burst" if r.get("post_exit") else "")
            print("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
                r.get("yokai") or r.get("y2") or "?", r.get("item", "?"), r["idx"], f(r.get("end_s")),
                r.get("end_why") or "-", r.get("bursts", 0), f(r.get("first_hit_s")), f(r.get("last_hit_s")),
                f(r.get("after_s")), note))


if __name__ == "__main__":
    main()
