"""arena_walk_test.py - does 'Train in arena' really put the player INSIDE the arena? (human #129412)

For each Training arena row: start the arena session exactly as the menu does (Training command 'fnow <mission>', the in-place
arena swap with the row's pose), wait for 'ARENA ... CONTROL: player released', lock on to the boss (RS), then for up to 30 s walk
at the boss (left stick forward) and attack (RB). PASS = the Training log shows a landed hit on the boss ('STYLE MOVE ... | HIT |'
or 'landed N>0'). FAIL = 30 s with no hit (fence, gate, wall or wrong floor between player and boss). The closest BOSSMON distance
and the player's start/end position are written too, so a FAIL shows where the player got stuck.

No OS input: VirtualPad pipe only (tools/testmatrix/vpad.py; [Debug] VirtualPad = 1 in Nioh3EngineFix.ini). Needs the style log
(Nioh3Training.ini [StyleMeter] AlwaysOn = 1) for HIT lines. Infinite health / NoDeath stay on in Training sessions.

SAFETY (docs/dev/AGENT-RULES.md): run only on a game YOU own: human not playing (cursor still, pad all-zero), the shared game lock
held, saves backed up (restore afterwards). The script refuses without --i-own-the-game and stops when --owner-pid dies.

    py -I tools/rematch/arena_walk_test.py --i-own-the-game --owner-pid <pid> [--rows R1CBC,R15FF] [--walk-sec 30] [--dry-run]
Output: captures/rematch/arenawalk-<stamp>.csv (row, mission, verdict, first_hit_s, hits, min_dist_m, start, end).

--learn (human #129421, universal placement): first, for every Boss Rematch row with no spot in notes/arena-fightstart.csv, run
Fight now (Boss Rematch pipe 'fight <id>'), walk forward until the game starts the fight ('BossBattleMode start - player now' in
Nioh3BossRematch.log, up to --learn-sec), then 'exit'. Afterwards run tools/harvest_fightstart.py and tools/auto_arena.py, install
the new Nioh3Training_arenas.tsv (game restart) and run this script again without --learn for the hit check.
"""
import argparse, csv, os, re, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME = Path(os.environ.get("NIOH3_DIR", r"C:/Program Files (x86)/Steam/steamapps/common/Nioh3"))
VPAD = ROOT / "tools" / "testmatrix" / "vpad.py"
TLOG, TCMD, TSV = GAME / "Nioh3Training.log", GAME / "Nioh3Training.cmd", GAME / "Nioh3Training_arenas.tsv"
HIT = re.compile(rb"STYLE MOVE: .*?\| (HIT|MISS) \|.*?landed (\d+)")
CTRL = re.compile(rb"ARENA \+\d+ ms CONTROL: player released at (-?\d+) (-?\d+) (-?\d+)")
MON = re.compile(rb"BOSSMON \+\d+ ms boss .*? player (-?\d+) (-?\d+) (-?\d+) dist ([\d.]+) m")
SESS = re.compile(rb"TRAIN arena session (\S+):")


BRLOG = GAME / "Nioh3BossRematch.log"
PIPE = r"\\.\pipe\nioh3bossrematch"
BBM = re.compile(rb"BossBattleMode start - player now (-?\d+) (-?\d+) (-?\d+);")


def br_pipe(line, timeout=5.0):
    """one command on the Boss Rematch pipe (nioh3bossrematch) -> reply line ('error ...' when the pipe is gone)."""
    try:
        with open(PIPE, "r+b", buffering=0) as p:
            p.write((line + "\n").encode("ascii"))
            b, t0 = b"", time.time()
            while not b.endswith(b"\n") and time.time() - t0 < timeout:
                c = p.read(1)
                if not c:
                    break
                b += c
            return b.decode("ascii", "replace").strip()
    except OSError as e:
        return "error %s" % e


def seen_rows():
    p = ROOT / "notes" / "arena-fightstart.csv"
    return {r["row"] for r in csv.DictReader(open(p, encoding="utf-8"))} if p.exists() else set()


def rows_from_tsv(want):
    out = []
    for l in TSV.read_text(encoding="utf-8").splitlines():
        f = l.split("\t")
        if l.startswith("#") or len(f) < 14 or f[12] != "arena":
            continue
        if want and f[13] not in want:
            continue
        out.append({"id": f[13], "mission": f[0], "pose": f[5:9]})
    return out


class Tail:
    def __init__(self, p):
        self.p, self.pos = p, p.stat().st_size if p.exists() else 0

    def new(self):
        try:
            with open(self.p, "rb") as f:
                f.seek(self.pos)
                b = f.read()
                self.pos += len(b)
                return b
        except OSError:
            return b""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rows", default="")
    ap.add_argument("--walk-sec", type=int, default=30)
    ap.add_argument("--load-sec", type=int, default=90)
    ap.add_argument("--owner-pid", type=int, default=0)
    ap.add_argument("--i-own-the-game", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--learn", action="store_true")
    ap.add_argument("--learn-sec", type=int, default=60)
    a = ap.parse_args()
    rows = rows_from_tsv(set(x for x in a.rows.split(",") if x))
    print("%d arena rows%s" % (len(rows), " (dry run)" if a.dry_run else ""))
    if a.dry_run:
        for r in rows:
            print("  %s mission %s pose %s" % (r["id"], r["mission"], " ".join(r["pose"])))
        return 0
    if not a.i_own_the_game or not a.owner_pid:
        sys.exit("refused: needs --i-own-the-game --owner-pid (lock held, human not playing, saves backed up)")

    def vp(*cmd):
        r = subprocess.run([sys.executable, "-I", str(VPAD), "--owner-pid", str(a.owner_pid), "--quiet", *map(str, cmd)],
                           capture_output=True, text=True)
        if r.returncode in (2, 3):
            sys.exit("abort: vpad %s (exit %d)" % (" ".join(map(str, cmd)), r.returncode))
        return r.returncode == 0

    def tcmd(line):
        with open(TCMD, "a", encoding="ascii") as f:
            f.write(line + "\r\n")

    if a.learn:
        have = seen_rows()
        todo = [r for r in rows if r["id"] not in have and r["id"][0] in "RDH"]
        print("learn: %d rows without a Fight now spot" % len(todo))
        for r in todo:
            bl = Tail(BRLOG)
            q = br_pipe("fight %s" % r["id"])
            print("[%s] %s: fight -> %s" % (time.strftime("%H:%M:%S"), r["id"], q))
            if not q.startswith("queued"):
                continue
            got, t0 = None, time.time()
            while time.time() - t0 < a.load_sec + a.learn_sec and not got:
                time.sleep(1)
                m = BBM.search(bl.new())
                if m:
                    got = m.groups()
                elif time.time() - t0 > 20:
                    vp("stick", "L", 0, 1, 900)   # walk in through the trigger when the game does not warp/teleport
            print("   -> %s" % ("seen %s" % b" ".join(got).decode() if got else "no fight start"))
            time.sleep(3)
            print("   exit -> %s" % br_pipe("exit"))
            time.sleep(15)
        print("now: py -I tools/harvest_fightstart.py && py -I tools/auto_arena.py, install the Training TSV, restart, rerun without --learn")
        return 0

    stamp = time.strftime("%Y%m%d-%H%M%S")
    out = ROOT / "captures" / "rematch" / ("arenawalk-%s.csv" % stamp)
    out.parent.mkdir(parents=True, exist_ok=True)
    res = []
    for r in rows:
        tl = Tail(TLOG)
        print("[%s] %s: fnow %s" % (time.strftime("%H:%M:%S"), r["id"], r["mission"]))
        tcmd("fnow %s" % r["mission"])
        start = None
        t0 = time.time()
        while time.time() - t0 < a.load_sec and start is None:
            time.sleep(0.5)
            m = CTRL.search(tl.new())
            if m:
                start = tuple(int(x) for x in m.groups())
        rec = {"row": r["id"], "mission": r["mission"], "verdict": "NO-SESSION", "first_hit_s": "", "hits": 0, "min_dist_m": "",
               "start": "%d %d %d" % start if start else "", "end": ""}
        if start:
            time.sleep(2.5)                      # boss AI grace + gauge
            vp("press", "RS", 120)              # lock on
            hits, mind, end, tw = 0, 1e9, None, time.time()
            while time.time() - tw < a.walk_sec:
                vp("stick", "L", 0, 1, 900)
                vp("press", "RB", 120)
                b = tl.new()
                for m in MON.finditer(b):
                    mind, end = min(mind, float(m[4])), (m[1].decode(), m[2].decode(), m[3].decode())
                for m in HIT.finditer(b):
                    if m[1] == b"HIT" or int(m[2]) > 0:
                        hits += 1
                        if not rec["first_hit_s"]:
                            rec["first_hit_s"] = "%.1f" % (time.time() - tw)
                if hits >= 2:
                    break
            rec.update(verdict="PASS" if hits else "FAIL", hits=hits, min_dist_m="%.1f" % mind if mind < 1e9 else "",
                       end=" ".join(end) if end else "")
        print("   -> %s hits %s min %s m start %s end %s" % (rec["verdict"], rec["hits"], rec["min_dist_m"], rec["start"], rec["end"]))
        res.append(rec)
        tcmd("trainstop")
        time.sleep(3)
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, list(res[0].keys()) if res else ["row"])
        w.writeheader()
        w.writerows(res)
    print("wrote %s: %d PASS / %d rows" % (out, sum(r["verdict"] == "PASS" for r in res), len(res)))
    return 0 if all(r["verdict"] == "PASS" for r in res) else 1


if __name__ == "__main__":
    sys.exit(main())
