"""Harvest the game's own fight-start player positions from Boss Rematch logs (human #129412).

When Fight now runs, the game itself puts the player in the arena (walk-in trigger or intro cutscene teleport) and Boss Rematch
logs 'arena: BossBattleMode start - player now X Y Z; ... mission 0xM ... - Fight now row R'. That position is where a real fight
starts, so Train in arena uses it (rematch_catalog.pick_arena) instead of a guess from the trigger box. Nothing is set by hand:
every row comes from a log line the game produced.

    py -I tools/harvest_fightstart.py [DIR_OR_LOG ...]   (default: the game folder logs and D:/nioh3-re)
writes notes/arena-fightstart.csv (merged with the rows already in it): row, mission, x, y, z, samples, source.
"""
import csv, os, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "notes" / "arena-fightstart.csv"
PAT = re.compile(rb"BossBattleMode start - player now (-?\d+) (-?\d+) (-?\d+);.*?mission (0x[0-9A-Fa-f]+).*?Fight now row ([A-Z0-9]+)")
GAME = Path(os.environ.get("NIOH3_DIR", r"C:/Program Files (x86)/Steam/steamapps/common/Nioh3"))


def files(args):
    for a in args or [str(GAME), "D:/nioh3-re"]:
        p = Path(a)
        if p.is_file():
            yield p
        elif p.is_dir():
            for q in p.rglob("*"):
                n = q.name.lower()
                if q.is_file() and ("rematch" in n) and (n.endswith(".log") or ".log." in n or n.endswith(".txt")):
                    yield q


def main():
    seen = {}
    if OUT.exists():
        for r in csv.DictReader(open(OUT, encoding="utf-8")):
            seen.setdefault((r["row"], r["mission"]), []).extend([(int(r["x"]), int(r["y"]), int(r["z"]), r["source"])] * 1)
    for f in files(sys.argv[1:]):
        try:
            data = f.read_bytes()
        except OSError:
            continue
        for m in PAT.finditer(data):
            x, y, z, mid, row = int(m[1]), int(m[2]), int(m[3]), m[4].decode().upper().replace("0X", "0x"), m[5].decode()
            seen.setdefault((row, mid), []).append((x, y, z, f.name))
    rows = []
    for (row, mid), v in sorted(seen.items()):
        uniq = sorted(set(v))
        # the most common position (ties: the first); walk-in triggers give a few close samples, all inside the arena
        best = max(uniq, key=lambda s: (sum(1 for t in v if t[:3] == s[:3]), -uniq.index(s)))
        rows.append({"row": row, "mission": mid, "x": best[0], "y": best[1], "z": best[2], "samples": len({t[:3] for t in v}),
                     "source": best[3]})
    with open(OUT, "w", encoding="utf-8", newline="") as o:
        w = csv.DictWriter(o, ["row", "mission", "x", "y", "z", "samples", "source"])
        w.writeheader()
        w.writerows(rows)
    print("wrote %s: %d rows" % (OUT, len(rows)))


if __name__ == "__main__":
    main()
