#!/usr/bin/env python3
"""Phase 0 report for the boss-rematch design (notes/boss-rematch-design.md).

Reads Nioh3EngineFix.log (BossLog / RematchLog / REWARD lines; formats from
mod/Nioh3EngineFix/src/bosslog.cpp) and prints:
  (a) per-fight summary  (b) answers to open questions Q1-Q10
  (c) proposed boss-master.csv updates.
usage: phase0_report.py [log] [--notes DIR] [--items TSV] [--text TSV]
"""
import argparse, csv, os, re, sys
from collections import defaultdict

DEF_LOG = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3EngineFix.log"
HERE = os.path.dirname(os.path.abspath(__file__))
DEF_NOTES = os.path.normpath(os.path.join(HERE, "..", "..", "notes"))

T = re.compile(r"\bt=([\d.]+)")
BL = re.compile(r"BossLog #(\d+): (.*)")
RLFULL = re.compile(r"(RematchLog(?: \w+)?): (.*)")


def rd(p, delim=","):
    try:
        with open(p, encoding="utf-8", errors="replace", newline="") as f:
            return list(csv.DictReader(f, delimiter=delim))
    except OSError:
        return []


class Names:
    def __init__(self, notes, items, text):
        self.master = rd(os.path.join(notes, "boss-master.csv"))
        self.char_name = {}
        for r in rd(os.path.join(notes, "boss-enemy-params.csv")):
            if r.get("name"):
                self.char_name[r["char_id"]] = r["name"]
        self.by_place, self.by_char = {}, {}
        for r in self.master:
            for pair in (r.get("all_placements") or "").split(";"):
                pid = pair.split(":")[0].strip()
                if pid:
                    self.by_place[pid] = r
            if r.get("placement_id"):
                self.by_place.setdefault(r["placement_id"], r)
            for k in ("boss_char_id", "param_char_id"):
                if r.get(k):
                    self.by_char.setdefault(r[k], r)
        self.text = {}
        for r in rd(text, "\t"):
            if r.get("lang", "en") == "en":
                self.text[r["id"]] = re.sub(r"\^\d+|~\w+~", "", r["text"])
        self.items = {}
        for r in rd(items, "\t"):
            nm = r.get("name_en") or self.text.get(r.get("name_id", ""), "")
            if nm:
                for k in ("cat_code", "name_id"):
                    if r.get(k):
                        self.items.setdefault(r[k], nm)

    def char(self, cid):
        cid = str(cid)
        if cid in self.by_char:
            return self.by_char[cid]["english_name"]
        return self.char_name.get(cid, "?")

    def boss_for_place(self, pid):
        return self.by_place.get(str(pid))

    def item(self, v):
        return self.items.get(str(v))


def parse(lines):
    fights = defaultdict(lambda: dict(start=None, end=None, dur=None, places=[], flags=[], clears=[],
                                      gauges=[], rewards=[], recent=None))
    rematch = []
    for no, ln in enumerate(lines):
        m = BL.search(ln)
        if m:
            n, body = int(m.group(1)), m.group(2)
            f = fights[n]
            tm = T.search(body)
            t = float(tm.group(1)) if tm else None
            if body.startswith("BossBattleMode start"):
                f["start"] = t
            elif body.startswith("BossBattleModeEnd"):
                f["end"] = t
                d = re.search(r"fight ([\d.]+) s", body)
                f["dur"] = d.group(1) if d else None
            elif body.startswith("Character::Create placement ids"):
                f["recent"] = body.split("s:", 1)[1].strip()
            elif body.startswith("Character::Create placement"):
                p = re.match(r"Character::Create placement (\d+) \(arg2 (\d+), flag (\d+), arg4 (\d+)\)", body)
                if p:
                    f["places"].append(p.groups())
            elif body.startswith("SetMissionFlag"):
                p = re.match(r"SetMissionFlag id (\d+) value (\d+).*\((.*)\)", body)
                if p:
                    f["flags"].append(p.groups())
            elif body.startswith("SetMissionClearFlag"):
                p = re.match(r"SetMissionClearFlag mission id (\d+)", body)
                if p:
                    f["clears"].append(p.group(1))
            elif body.startswith("ShowBossGauge"):
                p = re.match(r"ShowBossGauge actor id (\d+)", body)
                if p:
                    f["gauges"].append(p.group(1))
            elif body.startswith("REWARD"):
                f["rewards"].append(body)
            continue
        m = RLFULL.search(ln)
        if m:
            tm = T.search(m.group(2))
            rematch.append((m.group(1), m.group(2), float(tm.group(1)) if tm else None, no))
    return fights, rematch


def windows(rematch):
    ws, cur = [], None
    for tag, body, t, no in rematch:
        if body.startswith("window OPEN"):
            cur = dict(open=t, close=None, missions=[])
            ws.append(cur)
        if cur is not None:
            for pat in (r"IN MISSION (\d+)", r"MissionRequestById.*?mission=(\d+)", r"request (\d+) never",
                        r"EXIT of mission (\d+)"):
                p = re.search(pat, body)
                if p and p.group(1) not in cur["missions"]:
                    cur["missions"].append(p.group(1))
        if body.startswith("window CLOSED") and cur:
            cur["close"] = t
            cur = None
    return ws


def rewards_fmt(rw, names):
    out = []
    for r in rw:
        p = re.match(r"REWARD (\S+) nargs=(-?\d+)(.*?) t=", r)
        if p and p.group(1) != "GameClearRewardGrant":
            args = re.findall(r"a(\d+)=(-?[\d.]+)", p.group(3))
            s = p.group(1) + "(" + ",".join(f"a{i}={v}" for i, v in args) + ")"
            nm = [names.item(v) for _, v in args if names.item(v)]
            out.append(s + (" -> " + "/".join(nm) if nm else ""))
        else:
            q = re.match(r"REWARD GameClearRewardGrant difficulty (\d+)", r)
            out.append("GameClearRewardGrant diff=" + (q.group(1) if q else "?"))
    return out


def report(fights, ws, names, P):
    P("=" * 78)
    P("(a) PER-FIGHT SUMMARY")
    P("=" * 78)
    if not fights:
        P("no BossLog lines found (is [Debug] BossLog = 1 and the log the right file?)")
    fmap = {}
    for n in sorted(fights):
        f = fights[n]
        win = None
        if f["start"] is not None:
            for w in ws:
                if w["open"] is not None and w["open"] <= f["start"] <= (w["close"] or 1e18):
                    win = w
        rows = [r for r in (names.boss_for_place(p[0]) for p in f["places"]) if r]
        P(f"\nFight #{n}: start t={f['start']} end t={f['end']} dur={f['dur']} s"
          f"  [{'REMATCH/replay window' if win else 'ordinary'}]")
        for p in f["places"]:
            r = names.boss_for_place(p[0])
            cid = r["boss_char_id"] if r else ""
            P(f"  placement {p[0]} (arg2 {p[1]} flag {p[2]} arg4 {p[3]}) -> boss "
              f"{r['english_name'] if r else '?'}" + (f" [char {cid} = {names.char(cid)}]" if cid else ""))
        if f["recent"]:
            P(f"  recent placements (60 s): {f['recent']}")
        if f["gauges"]:
            P("  boss gauge actor ids: " + ", ".join(f"{g} ({names.char(g)})" for g in dict.fromkeys(f["gauges"])))
        mid = rows[0].get("mission_id") if rows else ""
        P(f"  mission id: table={mid or '?'}  window={','.join(win['missions']) if win and win['missions'] else '-'}")
        infl = [x for x in f["flags"] if x[2] == "in fight"]
        aft = [x for x in f["flags"] if x[2] != "in fight"]
        P("  flags in fight: " + (", ".join(f"{a}={b}" for a, b, _ in infl) or "none")
          + ("   after fight: " + ", ".join(f"{a}={b}" for a, b, _ in aft) if aft else ""))
        P("  clear ids: " + (", ".join(dict.fromkeys(f["clears"])) or "none"))
        rw = rewards_fmt(f["rewards"], names)
        P("  rewards: " + (("\n    " + "\n    ".join(rw)) if rw else "none"))
        fmap[n] = (f, rows, win)
    return fmap


Q = [
    ("Q1", "key -> byte mapping / key alone without item",
     [r"L2: EmakiMissionRequest", r"after-MissionSetupEmaki", r"L1: EMAKIMAP"]),
    ("Q2", "Emaki record field meanings (+0x00..)",
     [r"L1: EMAKI\[", r"L3: MissionSetupEmaki", r"Character::Create placement \d+"]),
    ("Q3", "story boss needs mission script (BossBattleMode/gauge under 0xCC96)",
     [r"BossBattleMode start", r"ShowBossGauge", r"IN MISSION"]),
    ("Q4", "story flags under a scroll vs normal",
     [r"SetMissionFlag id", r"RematchLog L4 "]),
    ("Q5", "clear calls (mission-clear ids)",
     [r"SetMissionClearFlag", r"L5: ClearMapSetEntry", r"L5: emaki_unique"]),
    ("Q6", "item consumption",
     [r"L2: scroll item idx \d+ changed", r"L2: inventory count", r"invCnt="]),
    ("Q7", "where scroll use is allowed",
     [r"Q7: EmakiItemUse_MenuHandler", r"Q7: ItemUse_F1FD80", r"UI::OpenMissionReplayMenu|BattleScrollTop_Select"]),
    ("Q8", "which call grants boss drops",
     [r"REWARD ", r"L5: TrophyUnlock"]),
    ("Q9", "anything saves between menu and mission start",
     [r"Q9: MissionStartFromSave", r"Q9: BuildSaveFilePath"]),
    ("Q10", "exact replay enqueue arguments",
     [r"L7: MissionRequestById", r"L7: MissionRequestEnqueue", r"L7: MissionReplayMenu_Setup|MissionSelectListBuild"]),
]


def answers(lines, P):
    P("\n" + "=" * 78)
    P("(b) OPEN QUESTIONS Q1-Q10")
    P("=" * 78)
    res = {}
    for qid, title, pats in Q:
        hits = [[l.rstrip() for l in lines if re.search(pat, l)] for pat in pats]
        got = sum(1 for h in hits if h)
        st = "answered" if got == len(pats) else ("partial" if got else "not covered")
        res[qid] = st
        P(f"\n{qid} [{st.upper()}] {title}  ({got}/{len(pats)} evidence kinds)")
        for pat, h in zip(pats, hits):
            if h:
                P(f"   + {pat}: {len(h)} line(s); e.g.")
                for e in h[:2]:
                    P("       " + e[:230])
            else:
                P(f"   - {pat}: none")
    P("\nSummary: " + "  ".join(f"{k}={v}" for k, v in res.items()))


def q(s):
    return '"' + str(s or "").replace('"', '""') + '"'


def proposals(fmap, names, P):
    P("\n" + "=" * 78)
    P("(c) boss-master.csv UPDATES (review before applying)")
    P("=" * 78)
    P("english_name,column,current,observed")
    any_ = False
    for n, (f, rows, win) in sorted(fmap.items()):
        if not f["places"]:
            continue
        r = rows[0] if rows else None
        nm = r["english_name"] if r else f"<unknown placement {f['places'][0][0]}>"
        cur = lambda c: r.get(c) if r else ""
        fl = list(dict.fromkeys(a for a, _, w in f["flags"] if w == "in fight"))
        cl = list(dict.fromkeys(f["clears"]))
        rw = rewards_fmt(f["rewards"], names)
        obs = []
        if fl:
            obs.append(("defeat_check_value", cur("defeat_check_value"), "flags=" + "|".join(fl)))
        if cl:
            obs.append(("mission_id (clear ids)", cur("mission_id"), "|".join(cl)))
        if rw:
            obs.append(("rewards_known", cur("rewards_known"), "; ".join(rw)))
        if win and win["missions"]:
            obs.append(("refightable_in_game", cur("refightable_in_game"),
                        "yes (replayed as mission " + ",".join(win["missions"]) + ")"))
        if f["gauges"]:
            obs.append(("boss_char_id (gauge actor)", cur("boss_char_id"), "|".join(dict.fromkeys(f["gauges"]))))
        for col, c, new in obs:
            any_ = True
            P(",".join([q(nm), col, q(c), q(new)]))
    if not any_:
        P("(nothing observed yet)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log", nargs="?", default=DEF_LOG)
    ap.add_argument("--notes", default=DEF_NOTES)
    ap.add_argument("--items", default=r"D:\nioh3-re\items.tsv")
    ap.add_argument("--text", default=r"D:\nioh3-re\text_en.tsv")
    a = ap.parse_args()
    try:
        with open(a.log, encoding="utf-8", errors="replace") as f:
            lines = [l for l in f if "BossLog" in l or "RematchLog" in l]
    except OSError as e:
        sys.exit(f"cannot read log {a.log}: {e}")
    names = Names(a.notes, a.items, a.text)
    fights, rematch = parse(lines)
    ws = windows(rematch)
    out = [f"Phase 0 report: {a.log}\n{len(lines)} BossLog/RematchLog lines, {len(fights)} fight(s), {len(ws)} rematch window(s)"]
    P = out.append
    fmap = report(fights, ws, names, P)
    answers(lines, P)
    proposals(fmap, names, P)
    print("\n".join(out))


if __name__ == "__main__":
    main()
