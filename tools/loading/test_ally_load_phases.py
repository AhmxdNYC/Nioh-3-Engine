"""Fixture test for ally_load_phases.py. usage: py -I tools/loading/test_ally_load_phases.py (prints ALL OK)"""
import io, json, os, sys, tempfile, contextlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ally_load_phases as alp  # noqa: E402

FX = os.path.join(HERE, "fixtures")


def run(*args):
    out = os.path.join(tempfile.mkdtemp(), "o.json")
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        alp.main(["alp", *args, "--json", out])
    with open(out, encoding="utf-8") as f:
        return json.load(f), buf.getvalue()


def near(a, b, tol=0.051):
    assert a is not None and abs(a - b) <= tol, (a, b)


# 1. Tip-page wait credited to G9 by the probe: the tail is split off, the active holder is G8, the tip wait stays unknown.
j, txt = run(os.path.join(FX, "tip_wait_g9_stale.log"))
L = j["loads"][0]
assert j["auto_continue"] == 0
near(L["pre_gate"]["s"], 2.0); near(L["active"]["s"], 4.0); near(L["post_eval"]["s"], 24.0)
assert L["stale_gate"] == "G9"
near(L["gates"]["G9"]["stale_tail"], 24.0); near(L["gates"]["G9"]["active_held"], 0.10)
near(L["gates"]["G8"]["active_held"], 2.0)
assert L["holder_active"].startswith("G8 ")
near(L["uncredited"]["s"], 0.9)
assert L["ticks_no_failing_gate"] == [6]
assert L["evals_stalled_from_tick"] == 7
assert L["post_eval_split"]["tip_confirm_wait"]["s"] is None and L["post_eval_split"]["tip_confirm_wait"]["src"] == "unknown"
assert L["tail_reads_probe"]["MB"] == 20
near(L["first_g9_zero"]["s"], 5.9); assert "pass1" not in L and "no_eval_gap_after_pass1" not in L
assert "unknown" in txt

# 1b. Same log with an external press marker: used, and tagged as external.
j, _ = run(os.path.join(FX, "tip_wait_g9_stale.log"), "--confirm-marker", "press=10:00:30.000")
sp = j["loads"][0]["post_eval_split"]
near(sp["tip_confirm_wait"]["s"], 14.0); near(sp["after_confirm_to_clear"]["s"], 10.0)
assert "external" in sp["tip_confirm_wait"]["src"]

# 1c. Mod cover still up after the work end (it hides the tip page), then a harness press marker from seq/index.txt.
j, txt = run(os.path.join(FX, "tip_wait_g9_stale.log"), "--rematch", os.path.join(FX, "cover_hides_tip.rematch.log"),
             "--markers", os.path.join(FX, "cover_hides_tip.index.txt"), "--confirm-name", "TIP-A")
L = j["loads"][0]
near(L["cover_ack_to_terminal_tail"]["s"], 9.0)
assert [(s["from"].split(" (")[0], round(s["s"], 1)) for s in L["tail_segments"]] == [
    ("last eval", 9.0), ("mod cover end", 13.0), ("marker TIP-A", 2.0)], L["tail_segments"]
near(L["post_eval_split"]["tip_confirm_wait"]["s"], 22.0)
assert "overlaps tail by 9.0 s" in txt

# 2. AutoContinue answer inside the tail, across midnight.
j, _ = run(os.path.join(FX, "autocontinue_midnight.log"))
L = j["loads"][0]
assert j["auto_continue"] == 1
near(L["post_eval"]["s"], 2.5)
near(L["post_eval_split"]["tip_confirm_wait"]["s"], 1.0); near(L["post_eval_split"]["after_confirm_to_clear"]["s"], 1.5)
assert L["post_eval_split"]["tip_confirm_wait"]["src"].startswith("log")
near(L["gates"]["G9"]["active_held"], 1.0)

# 3. Chained loads (map load, then the short script-owned boss-intro load) + rematch events + an open window.
j, txt = run(os.path.join(FX, "chained_boss_entry.log"), "--rematch", os.path.join(FX, "chained_boss_entry.rematch.log"))
a, b = j["loads"]
near(a["post_eval"]["s"], 11.0); near(a["gates"]["G9"]["active_held"], 0.5)
near(b["post_eval"]["s"], 0.0); near(b["gates"]["G9"]["active_held"], 0.70)
assert j["open_windows"] == [3]
ev = {e["event"]: e for e in j["rematch_events"]}
assert ev["boss_battle_start"]["where"] == "inside #2 at +3.0 s", ev["boss_battle_start"]["where"]
assert ev["request"]["where"] == "outside any 58A window"
assert ev["first_frame_after_load"]["where"].startswith("after #1 clear")
assert "gap #1 clear -> #2 set: 1.5 s" in txt

# 4. LoadGates off: phases are unknown, not zero.
src = open(os.path.join(FX, "tip_wait_g9_stale.log"), encoding="utf-8").read().splitlines(True)
tmp = os.path.join(tempfile.mkdtemp(), "nogates.log")
with open(tmp, "w", encoding="utf-8") as f:
    f.writelines(x for x in src if "LoadGates" not in x)
j, _ = run(tmp)
L = j["loads"][0]
assert L["pre_gate"]["s"] is None and L["active"]["s"] is None and L["post_eval"]["s"] is None

# 5. Generic timestamps are chronology only, not input/confirmation evidence.
j, _ = run(os.path.join(FX, "tip_wait_g9_stale.log"), "--marker", "screenshot=10:00:30")
assert j["loads"][0]["post_eval_split"]["tip_confirm_wait"]["s"] is None
assert any(x["to"] == "marker screenshot" for x in j["loads"][0]["tail_segments"])

# 6. Exercise actual event parsing and analyzer with missing/mismatched/duplicate
# lifecycle receipts. The chronological terminal event remains visible but never
# establishes a cover interval without a same-token ACK and native request.
original = open(os.path.join(FX, "cover_hides_tip.rematch.log"), encoding="utf-8").read()
for name, mutated in [
    ("missing_ack", "\n".join(x for x in original.splitlines() if "present_ack" not in x)),
    ("wrong_ack_token", original.replace("token=1 phase=2 attempt=0", "token=2 phase=2 attempt=0")),
    ("wrong_end_attempt", original.replace("phase=5 attempt=1", "phase=5 attempt=2")),
    ("wrong_end_token", original.replace("token=1 phase=5", "token=2 phase=5")),
    ("reused_token", original + original),
    ("new_begin", original.replace("[10:00:25.000]", "[10:00:24.000] Rematch: cover token=2 phase=1 attempt=0 event=begin\n[10:00:25.000]")),
]:
    path = os.path.join(tempfile.mkdtemp(), name + ".log")
    with open(path, "w", encoding="utf-8") as f: f.write(mutated)
    j, _ = run(os.path.join(FX, "tip_wait_g9_stale.log"), "--rematch", path)
    assert "cover_ack_to_terminal_tail" not in j["loads"][0], name
    assert any(e["event"] == "cover_end" for e in j["rematch_events"]), name

# 7. Three failing G9 evaluations (as in real ev253 loads 1 and 3) must
# not become two proven native passes or a claimed no-evaluation interval.
path = os.path.join(tempfile.mkdtemp(), "three_g9.log")
original = open(os.path.join(FX, "tip_wait_g9_stale.log"), encoding="utf-8").read()
with open(path, "w", encoding="utf-8") as f:
    f.write(original.replace("first-failing in 2 evals", "first-failing in 3 evals"))
j, txt = run(path)
assert "pass1" not in j["loads"][0] and "no_eval_gap_after_pass1" not in j["loads"][0]
assert "LoadIsLoadEnd passed" not in txt and "native pass/caller unobserved" in txt
print("ALL OK (including ambiguous cover, typed markers and G9 aggregate regressions)")
