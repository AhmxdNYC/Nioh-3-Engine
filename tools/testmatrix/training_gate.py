"""training_gate.py - release gate for the Training session fixes of 11-19 (human #122006, TASK #122016). Five live cases,
each scored from the mod logs of one in-game run. No game, no input, no bridge here: this file only checks and scores.

    py -I training_gate.py source [--root DIR]        offline: every log line a case scores on still exists in the source
    py -I training_gate.py plan [N]                   the in-game steps of each case (for the live runner, pc/1)
    py -I training_gate.py score DIR [--case N ...]   score a capture: DIR/caseN/{tr,br,ef}.txt = the log text written
                                                      during case N (training_gate_live.ps1 cuts them); exit 0 = all PASS
    py -I training_gate.py selftest                   offline fixtures: each case passes on a good log and fails on the bug log

Cases (one human bug each; ids are the bridge messages):
  1 #121835 Training 'Train here' summons a Training boss where the player stands; no native Boss Match load
  2 #121854 the arena gate's Fight again works after a won native Boss Match retired at the gate
  3 #121923 Despawn all ends the Training session; the next menu open is not forced onto the Practice page
  4 #121968 a won in-place Training session ends once the boss body is removed
  5 #121995 Training 'Train in a boss arena' goes to the boss arena as Practice; no native Boss Match, no summon in place
No boss ids or addresses: the runner picks any boss row; the rules key on log text only. A case without its log files is
NOT RUN (never PASS). 'source' fails when a scored line was renamed or removed, so a later build cannot silently drop a case.
"""
import argparse
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SRC = {"tr": "mod/Nioh3Training/src/training.c", "br": "mod/Nioh3BossRematch/src/rematch.cpp", "ef": "mod/Nioh3EngineFix/src/menu.cpp"}
NATIVE = [("tr", r"FIGHTNOW \S+: native-first mission load"), ("tr", r"FIGHTNOW \S+ mission 0x[0-9A-Fa-f]+: native load")]

# need: every (log, regex) must match, in this order (each after the previous match). any: at least one group of need-lists.
# forbid: none may match anywhere in the case window. Cases 3/4: ef.txt starts at the TRAIN stop line (the live harness cuts it
# there, the logs share no clock), so a Practice-page open in it is the bug.
# needles: literal source text (per file) the regexes depend on; 'source' checks them.
CASES = {
    1: dict(bug="121835", title="Train here = Training boss in place, no native Boss Match",
            steps=["Field, no session. Mod menu > Training > Train here > any boss row with a mission (A, confirm).",
                   "Wait 20 s. The boss stands where you are; no loading screen.", "Despawn all to clean up."],
            any=[[("br", r"Rematch: menu 'Training here' \S+ -> [1-9]"), ("tr", r"MENU training here mission 0x[0-9A-Fa-f]+ -> 1")],
                 [("tr", r"FIGHTNOW mission 0x[0-9A-Fa-f]+: Training pick: training here \(in-place summon, Practice\), no native Boss Match -> 1")]],
            forbid=NATIVE,
            needles={"br": ["Rematch: menu 'Training here' %s -> %d"], "tr": ["MENU training here mission 0x%X -> %d",
                     "FIGHTNOW %s: native-first mission load", "FIGHTNOW %s mission 0x%X: native load (%s)"]}),
    2: dict(bug="121854", title="Fight again at the gate after a native Boss Match win",
            steps=["Boss Matches > any native row > Fight now. Win the fight (the gate appears).",
                   "At the gate: Fight again (A).", "Wait until the same boss fight starts again (up to 3 min)."],
            any=[[("br", r"Rematch: (native attempt won and settled at the gate; retired|the retired win keeps the gate's Fight again for row)"),
                  ("br", r"Rematch: (Fight again direct \S+: MissionRequestById|native gate retry: home is up)")]],
            forbid=[("br", r"native boss row is no longer owned|native gate no longer owns this fight|Fight again direct \S+: request refused")],
            needles={"br": ["Rematch: native attempt won and settled at the gate; retired so a new Fight now is accepted",
                            "Rematch: the retired win keeps the gate's Fight again for row %d", "Rematch: Fight again direct %s: MissionRequestById",
                            "Rematch: native gate retry: home is up", "native boss row is no longer owned", "native gate no longer owns this fight"]}),
    3: dict(bug="121923", title="Despawn all ends the Training session, menu back on the normal page",
            steps=["Start a Training session (Train here, any row).", "Mod menu > Training > Despawn all (A). Close the menu.",
                   "Wait 5 s, open the mod menu once, close it."],
            any=[[("br", r"Rematch: menu 'Despawn all bosses' -> 1"), ("tr", r"TRAIN stop \(despawn all\)")],
                 [("tr", r"TRAIN stop \(last boss removed\)")],
                 [("tr", r"TRAIN stop \(despawn all command\)")]],  # -Drive cmd (same TrainStopIfNoBoss, not the menu row)
            forbid=[("ef", r"opened on the Practice page")],
            needles={"br": ["Rematch: menu 'Despawn all bosses' -> %d"], "tr": ['TrainStopIfNoBoss("despawn all")', 'TrainStopIfNoBoss("last boss removed")', 'TrainStopIfNoBoss("despawn all command")',
                     'Log("TRAIN stop (%s)", why)'], "ef": ['opened on the %s page (current mode, #13529)', '"Practice"']}),
    4: dict(bug="121968", title="A kill ends the Training-here session once the body is removed",
            steps=["Start a Training session (Train here, any row).", "Defeat the boss (or 'kill last' on the cmd file).",
                   "Wait up to 60 s for the body to fade. Open the mod menu once: it shows the first Training page."],
            any=[[("tr", r"TRAIN stop \(boss defeated, body removed\)")]],
            forbid=[("ef", r"opened on the Practice page")],
            needles={"tr": ['TrainStop("boss defeated, body removed")', 'Log("TRAIN stop (%s)", why)']}),
    5: dict(bug="121995", title="Train in a boss arena = the boss arena as Practice",
            steps=["Field, no session. Mod menu > Training > Train in a boss arena > any boss row (A, confirm).",
                   "Wait up to 3 min. You arrive in the boss's arena; no native Boss Match loading."],
            any=[[("tr", r"FIGHTNOW mission 0x[0-9A-Fa-f]+: Training pick: practice in the boss arena \(arena swap / world switch, no native Boss Match\) -> [1-9]"),
                  ("tr", r"TRAIN arena session \S+: round 1")]],
            forbid=NATIVE + [("tr", r"Training pick: arena route refused, training here")],
            needles={"tr": ["FIGHTNOW mission 0x%X: Training pick: practice in the boss arena (arena swap / world switch, no native Boss Match) -> %d",
                            "TRAIN arena session %s: round 1", "FIGHTNOW %s: native-first mission load"]}),
}


def read(path):
    try:
        with open(path, "rb") as f:
            return f.read().decode("utf-8", "replace").replace("\r\n", "\n")
    except FileNotFoundError:
        return None


def score_case(n, logs):
    """logs: {'tr': text, 'br': text, 'ef': text} (missing = None). Returns (verdict, reason)."""
    c = CASES[n]
    if all(logs.get(k) is None for k in ("tr", "br")):
        return "NOT RUN", "no tr/br log for this case"
    best = None
    for group in c["any"]:
        pos, ok, why = {}, True, ""
        for log, rx in group:
            m = re.compile(rx).search(logs.get(log) or "", pos.get(log, 0))
            if not m:
                ok, why = False, f"missing {log}: /{rx}/"
                break
            pos[log] = m.end()
        if ok:
            best = pos
            break
        best = best if best is not None else why
    if not isinstance(best, dict):
        return "FAIL", best
    for log, rx in c["forbid"]:
        text = logs.get(log) or ""
        m = re.compile(rx).search(text)
        if m:
            return "FAIL", f"forbidden {log}: {m.group(0)!r}"
    return "PASS", "all lines seen"


def cmd_source(root):
    bad = 0
    for n, c in CASES.items():
        for log, needles in c["needles"].items():
            text = read(os.path.join(root, SRC[log])) or ""
            for nd in needles:
                if nd not in text:
                    bad += 1
                    print(f"case {n} (#{c['bug']}) MISSING in {SRC[log]}: {nd}")
    print(f"source: {'PASS' if not bad else 'FAIL'} ({sum(len(v) for c in CASES.values() for v in c['needles'].values())} lines, {bad} missing) root {root}")
    return 1 if bad else 0


def cmd_score(capture, cases, quiet=False):
    worst = 0
    for n in cases:
        d = os.path.join(capture, f"case{n}")
        logs = {k: read(os.path.join(d, k + ".txt")) for k in ("tr", "br", "ef")}
        v, why = score_case(n, logs)
        worst = max(worst, {"PASS": 0, "NOT RUN": 2, "FAIL": 1}[v])
        if not quiet:
            print(f"case {n} #{CASES[n]['bug']} {CASES[n]['title']}: {v} ({why})")
    return worst


def selftest():
    good = {
        1: {"br": "Rematch: menu 'Training here' R1359 -> 1\n", "tr": "MENU training here mission 0x1359 -> 1\nSUMMON ...\n"},
        2: {"br": "Rematch: native attempt won and settled at the gate; retired so a new Fight now is accepted\n"
                  "Rematch: Fight again direct R04DF: MissionRequestById for mission 0x4DF from mission 0x4DF (retry point 3)\n", "tr": ""},
        3: {"br": "Rematch: menu 'Despawn all bosses' -> 1\n", "tr": "DESPAWN ALL (menu): 1 deleted\nTRAIN stop (despawn all)\n",
            "ef": "menu opened\n"},
        4: {"tr": "TRAIN round 1 won\nTRAIN stop (boss defeated, body removed)\n", "ef": "menu opened\n"},
        5: {"tr": "FIGHTNOW mission 0x1359: Training pick: practice in the boss arena (arena swap / world switch, no native Boss Match) -> 1\n"
                  "TRAIN arena session R1359: round 1 pose player 1 2 3 boss 4 5 6 (retry stays in the arena)\n"},
    }
    bad = {  # the logs of the bugs as reported
        1: {"br": "Rematch: menu 'Training here' R1359 -> 1\n", "tr": "MENU training here mission 0x1359 -> 1\nFIGHTNOW R1359: native-first mission load (all mission rows; legacy route keys ignored)\n"},
        2: {"br": "Rematch: native attempt won and settled at the gate; retired so a new Fight now is accepted\nRematch: refused: native boss row is no longer owned\n", "tr": ""},
        3: {"br": "Rematch: menu 'Despawn all bosses' -> 1\n", "tr": "DESPAWN ALL (menu): 1 deleted\n", "ef": "opened on the Practice page (current mode, #13529)\n"},
        4: {"tr": "TRAIN round 1 won\nDESPAWN #1 R1359: native delete\n", "ef": "opened on the Practice page (current mode, #13529)\n"},
        5: {"tr": "FIGHTNOW mission 0x1359: Training pick: training here (in-place summon, Practice), no native Boss Match -> 1\n"},
    }
    fails = 0
    for n in CASES:
        for name, logs, want in (("good", good[n], "PASS"), ("bug", bad[n], "FAIL")):
            v, why = score_case(n, logs)
            if v != want:
                fails += 1
                print(f"selftest case {n} {name}: got {v} want {want} ({why})")
    # case 3: the stop line alone is not enough when the next menu open still lands on the Practice page
    fails += score_case(3, {"br": "Rematch: menu 'Despawn all bosses' -> 1\n", "tr": "TRAIN stop (despawn all)\n",
                            "ef": "opened on the Practice page (current mode, #13529)\n"})[0] != "FAIL"
    if score_case(4, {})[0] != "NOT RUN":
        fails += 1
    # case 5 must not accept the old in-place route; case 1 must accept either Train here form
    fails += score_case(1, {"tr": "FIGHTNOW mission 0x4DF: Training pick: training here (in-place summon, Practice), no native Boss Match -> 1\n", "br": ""})[0] != "PASS"
    with tempfile.TemporaryDirectory() as t:  # score reads caseN/{tr,br,ef}.txt; a missing case is NOT RUN (exit 2), never PASS
        for n in (1, 4):
            os.makedirs(os.path.join(t, f"case{n}"))
            for k, v in good[n].items():
                with open(os.path.join(t, f"case{n}", k + ".txt"), "w") as f:
                    f.write(v)
        fails += cmd_score(t, [1, 4], quiet=True) != 0
        fails += cmd_score(t, [1, 2], quiet=True) != 2
    print(f"training_gate selftest: {'PASS' if not fails else 'FAIL'} ({fails} failures)")
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("source"); s.add_argument("--root", default=ROOT)
    p = sub.add_parser("plan"); p.add_argument("case", nargs="?", type=int)
    sc = sub.add_parser("score"); sc.add_argument("dir"); sc.add_argument("--case", type=int, action="append")
    sub.add_parser("selftest")
    a = ap.parse_args()
    if a.cmd == "source":
        return cmd_source(a.root)
    if a.cmd == "plan":
        for n in ([a.case] if a.case else CASES):
            c = CASES[n]
            print(f"case {n} (#{c['bug']}) {c['title']}")
            for i, st in enumerate(c["steps"], 1):
                print(f"  {i}. {st}")
        return 0
    if a.cmd == "score":
        return cmd_score(a.dir, a.case or list(CASES))
    return selftest()


if __name__ == "__main__":
    sys.exit(main())
