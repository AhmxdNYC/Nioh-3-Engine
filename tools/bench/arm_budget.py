"""Wall-time model and hitch metric for the final bench.

No game and no PresentMon. The seconds below are the scripted model
(final_pc.ps1 $T and the capture-runner fixed sleeps), not a measured run.
Keep the final_pc numbers equal to FINAL_* in this file.
"""
import csv
import json
import statistics
import sys

HITCH_FACTOR = 2.0

# final_pc.ps1 before this cut: 3 runs, each relaunches.
OLD_LAUNCH_S = 210
OLD_STATS_S = 35
OLD_CLOSE_S = 25
OLD_WINDOW_PAD_S = 8
# After: 2 repeats share one launch; idle after the windows is 8s; window pad is 4s.
FINAL_LAUNCH_S = 160
FINAL_STATS_S = 8
FINAL_CLOSE_S = 8
FINAL_WINDOW_PAD_S = 4
FINAL_RUNS = 2
STAND_WINDOWS = 2
WINDOW_SEC = 20

# audit_screen_run fixed sleeps only (title-continue waits are extra).
AUDIT_OLD_WARM_S = 30
AUDIT_OLD_SETTLE_S = 8
AUDIT_NEW_WARM_S = 12
AUDIT_NEW_SETTLE_S = 4
AUDIT_MEASURE_S = 60
AUDIT_LAUNCH_FIXED_S = 9
AUDIT_CLOSE_S = 8
AUDIT_ARMS = 6
AUDIT_LAUNCHES_OLD = 6
AUDIT_LAUNCHES_NEW = 3
CHUNK_SEC = 600


def hitch(ms):
    """Count frames strictly slower than twice the median. Equal to 2x is not a hitch."""
    xs = []
    for x in ms:
        try:
            v = float(x)
        except (TypeError, ValueError):
            continue
        if v > 0:
            xs.append(v)
    if not xs:
        return {"count": 0, "worst": None}
    med = statistics.median(xs)
    spikes = [v for v in xs if v > HITCH_FACTOR * med]
    return {"count": len(spikes), "worst": (max(spikes) if spikes else None)}


def hitch_csv(path):
    ms = []
    with open(path, newline="", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f):
            ms.append(row.get("MsBetweenPresents") or row.get("msBetweenPresents") or row.get("FrameTime"))
    return hitch(ms)


def _window_s(pad):
    return WINDOW_SEC + pad


def final_pc_old_run_s():
    """One relaunched arm, stand x2 + move, no latency and no shader."""
    return (OLD_LAUNCH_S + OLD_STATS_S + OLD_CLOSE_S
            + (STAND_WINDOWS + 1) * _window_s(OLD_WINDOW_PAD_S))


def final_pc_new_session_s():
    """One launch covering FINAL_RUNS repeats, then one close."""
    per = FINAL_STATS_S + (STAND_WINDOWS + 1) * _window_s(FINAL_WINDOW_PAD_S)
    return FINAL_LAUNCH_S + FINAL_RUNS * per + FINAL_CLOSE_S


def final_pc_new_per_arm_s():
    return final_pc_new_session_s() / FINAL_RUNS


def audit_fixed_old_s():
    per = AUDIT_OLD_WARM_S + AUDIT_OLD_SETTLE_S + AUDIT_MEASURE_S + AUDIT_CLOSE_S + AUDIT_LAUNCH_FIXED_S
    return AUDIT_ARMS * per


def audit_fixed_new_s():
    per = AUDIT_NEW_WARM_S + AUDIT_NEW_SETTLE_S + AUDIT_MEASURE_S
    return AUDIT_LAUNCHES_NEW * (AUDIT_LAUNCH_FIXED_S + AUDIT_CLOSE_S) + AUDIT_ARMS * per


def plan_reuse(cells):
    """True when this cell repeats the previous cfg/preset/sweep and can skip launch."""
    out = []
    prev = None
    for c in cells:
        key = (c.get("cfg"), c.get("preset"), c.get("sweep") or "")
        out.append(prev == key)
        prev = key
    return out


def take_chunk(estimates, reuse_flags, chunk_sec=CHUNK_SEC):
    """Take a prefix whose sum stays within chunk_sec.

    The first group always starts. A launch plus its following reuse arm is
    kept together, so a chunk does not relaunch and then drop the repeat.
    """
    taken = []
    elapsed = 0.0
    i = 0
    n = len(estimates)
    while i < n:
        group = 1
        if (not reuse_flags[i]) and i + 1 < n and reuse_flags[i + 1]:
            group = 2
        need = float(sum(estimates[i:i + group]))
        if taken and elapsed + need > chunk_sec:
            break
        taken.extend(estimates[i:i + group])
        elapsed += need
        i += group
    return taken


def show():
    old = final_pc_old_run_s()
    new = final_pc_new_per_arm_s()
    return {
        "final_pc_old_s": old,
        "final_pc_new_s": new,
        "final_pc_ratio": new / old,
        "final_pc_session_s": final_pc_new_session_s(),
        "final_pc_old_pair_s": old * FINAL_RUNS,
        "audit_old_s": audit_fixed_old_s(),
        "audit_new_s": audit_fixed_new_s(),
        "audit_ratio": audit_fixed_new_s() / audit_fixed_old_s(),
        "audit_launches_old": AUDIT_LAUNCHES_OLD,
        "audit_launches_new": AUDIT_LAUNCHES_NEW,
        "chunk_sec": CHUNK_SEC,
    }


def main(argv):
    if len(argv) < 2 or argv[1] in ("-h", "--help"):
        print("usage: arm_budget.py show | hitch-csv <presentmon.csv>")
        return 2
    if argv[1] == "show":
        print(json.dumps(show()))
        return 0
    if argv[1] == "hitch-csv":
        print(json.dumps(hitch_csv(argv[2])))
        return 0
    print("unknown command", argv[1])
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
