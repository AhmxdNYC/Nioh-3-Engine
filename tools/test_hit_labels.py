"""Menu hit labels vs hits the player really takes (pc #41677 accuracy run, real game, offline replay of the result).

    py -I tools/test_hit_labels.py          unit tests (also prints the predicted-vs-measured table)
    py -I tools/test_hit_labels.py --table  table only (markdown)

MEASURED = player HP hits per rep of each Yoritomo (chr 16102) move drilled from the Training moveset menu, including the
continuation the move played in the same rep (notes/hit-label-accuracy-yoritomo.md, recount of D:/nioh3-wt/acc/Nioh3Training-acc.log
with D:/nioh3-wt/acc/count3.py: most common total over reps that were not whiffs and not cut by the next forced rep).
4E4E / 4E55 / 4E6E come from the #41579 clips drill (.handoff/checkpoints.md 9:16 pm, 5 runs each).
These are observations, not inputs: nothing here feeds the generator. The prediction is data/boss-action-record.tsv (the file the
menu reads), which tools/boss_action_record.py builds for every boss by one rule.
"""
import csv
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import boss_action_record as bar  # noqa: E402
import chain_hits as c  # noqa: E402

BOSS = "CE_YORITOMO_00"
OLD_TSV_SHA = "5ddf6861a610"  # installed chain TSV the run was measured with (labels in column 'old')
# move: (old menu label from the installed TSV, measured hits incl. the continuation it played, what was seen)
MEASURED = {
    0x4E59: (1, 2, "2 every rep (738 + 757), plays 4E5E (+0)"),
    0x4E7C: (1, 0, "0 in 7 reps (its only row has power 0: catch)"),
    0x4E42: (1, 0, "0 in 20 reps: hit window in the data but never connects"),
    0x4E69: (2, 2, "2 (14 reps, 2 landed 1)"),
    0x4EA2: (2, 1, "1 (10 of 11 reps)"),
    0x4E81: (2, 0, "0 in 30 reps (wind-up only)"),
    0x4E8E: (3, 1, "1 (8 of 9)"),
    0x4E95: (3, 1, "1 (6 of 7)"),
    0x4EA8: (3, 3, "3 (4 of 6 non-whiff reps; others 1, 2)"),
    0x4E64: (4, 2, "2 (7 reps)"),
    0x4EA0: (4, 2, "1 own + 1 from 4EA2 (5 of 5 non-whiff reps)"),
    0x4EA1: (4, 2, "1 own + 1 from 4EA2 (2 of 2 non-whiff reps)"),
    0x4E5C: (5, 3, "1 own + 4E60 > 4E62 (+2) in 5 of 8 reps"),
    0x4E6C: (5, 2, "2 (8 reps)"),
    0x4EAC: (5, 2, "2 (3 reps)"),
    0x4E50: (0, 0, "0 (11), no hit window"),
    0x4E6A: (0, 0, "0 (9), no hit window"),
    0x4EAF: (0, 0, "0 (5), no hit window"),
    0x4EAE: (2, 2, "2 (4 reps)"),
    0x4E5B: (0, 0, "0 (6)"),
    0x4E65: (0, 0, "0 (6)"),
    0x4EB8: (0, 0, "0 (7), plays 4EB9 (+0)"),
    0x4E4E: (2, 1, "#41579 drill: 1 (2nd hit in a fight = AI-chosen 4E53)"),
    0x4E55: (2, 1, "#41579 drill: 1"),
    0x4E6E: (2, 2, "#41579 drill: 2"),
}
# Known gap, not patched: 0x4E42 (motion 6910) owns one started row (frame 87, power 20, element 10, type 5) so the data says 1 hit,
# but it never connected in 20 drilled reps. Nothing in the action / hitbox rows read so far separates it from a normal hit.
KNOWN_MISS = {0x4E42}


def predicted():
    with bar.OUT.open(encoding="utf-8", newline="") as f:
        return {int(r["action_id"], 16): r for r in csv.DictReader(f, delimiter="\t") if r["boss"] == BOSS}


def table():
    rec = predicted()
    lines = ["| move | old label | new label (damage per hit) | measured | match | seen |", "|---|---:|---|---:|---|---|"]
    for aid, (old, meas, seen) in MEASURED.items():
        r = rec[aid]
        new = int(r["hit_count"])
        ok = "yes" if new == meas else ("NO (known gap)" if aid in KNOWN_MISS else "NO")
        lines.append("| 0x%X | %d | %d (%s) | %d | %s | %s |" % (aid, old, new, r["damage_per_hit"] or "-", meas, ok, seen))
    old_ok = sum(o == m for o, m, _ in MEASURED.values())
    new_ok = sum(int(rec[a]["hit_count"]) == m for a, (_o, m, _s) in MEASURED.items())
    lines.append("")
    lines.append("labels matching the measurement: old %d / %d, new %d / %d" % (old_ok, len(MEASURED), new_ok, len(MEASURED)))
    return "\n".join(lines)


class HitLabels(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not bar.EXTRACT.is_dir():
            raise unittest.SkipTest("archive_01 not extracted")
        cls.rec = predicted()

    def test_labels_match_measured_hits(self):
        bad = {}
        for aid, (_old, meas, _seen) in MEASURED.items():
            got = int(self.rec[aid]["hit_count"])
            if aid in KNOWN_MISS:
                continue
            if got != meas:
                bad["0x%X" % aid] = (got, meas)
        self.assertEqual(bad, {}, "label != measured (got, measured)")

    def test_known_gap_is_still_a_gap(self):
        # if a later rule fixes 0x4E42 automatically, drop it from KNOWN_MISS
        for aid in KNOWN_MISS:
            self.assertNotEqual(int(self.rec[aid]["hit_count"]), MEASURED[aid][1])

    def test_tsv_is_regenerated(self):
        d = bar.choose_files({BOSS: bar.load_names()[BOSS]})[0][0][2].read_bytes()
        rows, _gains = bar.chained(d, bar.own_rows(d))
        for aid, _mot, _anim, nhit, dmg, zero in rows:
            r = self.rec[aid]
            self.assertEqual((int(r["hit_count"]), r["damage_per_hit"], int(r["zero_power"])), (nhit, dmg, zero), hex(aid))


class Rules(unittest.TestCase):
    """The three fixes on synthetic hitbox rows (no archive needed)."""

    @staticmethod
    def image(rows):
        import struct
        d = bytearray(192 * len(rows))
        for i, (st, ln, pw, el, grp) in enumerate(rows):
            b = 192 * i
            struct.pack_into("<HHHHH", d, b + 8, st, 0xFFFF, ln, 0, 0)
            struct.pack_into("<HH", d, b + 0x10, pw, el)
            d[b + 0xA5] = grp
        return bytes(d)

    def windows(self, rows):
        w, _ = c.hit_windows(self.image(rows), 0, range(len(rows)))
        return [w_["powers"] for w_ in w]

    def test_two_separate_windows_of_one_hit_id_are_two_hits(self):  # 0x4E59 '175|180' -> 175/180
        self.assertEqual(self.windows([(88, 8, 35, 0, 0), (104, 6, 36, 0, 0)]), [[35], [36]])

    def test_zero_damage_rows_are_not_hits(self):  # 0x4E7C, 0x4EA2's trailing /0
        self.assertEqual(self.windows([(30, 4, 0, 0, 0)]), [])
        self.assertEqual(self.windows([(30, 4, 0, 30, 4)]), [[0]])  # element-only damage still hits

    def test_one_swept_swing_is_one_hit(self):  # 0x4E8E: 6 capsules at frames 52..57 = 1 hit; 0x4E64: two sweeps = 2 hits
        self.assertEqual(len(self.windows([(52 + i, 5, 30, 10, 1) for i in range(6)])), 1)
        self.assertEqual(len(self.windows([(120, 4, 40, 0, 6), (122, 4, 40, 0, 6), (123, 4, 40, 0, 6),
                                           (167, 4, 49, 0, 6), (169, 4, 49, 0, 6)])), 2)

    def test_same_frame_other_hit_group_is_another_hit(self):  # Shingen 0x4E4E: groups 1 and 2 at frame 16 = 2 hits
        self.assertEqual(len(self.windows([(16, 10, 17, 0, 1), (16, 10, 17, 0, 2)])), 2)


def chain_acts(**kw):
    a = dict(row=0, motion=1000, nxt=c.IDLE, attack=True, hits=1, groups=1, tr=(), catch=False)
    a.update(kw)
    return a


def tr_row(target, req=0xFFFF):
    return (target,) + (0xFFFF,) * 5 + (req,)


class ChainPlays(unittest.TestCase):
    def path(self, acts, rows, start):
        from collections import Counter
        use = Counter(k for a in acts.values() for k in a["tr"])
        return c.play_path(acts, c.plays_next(acts, rows, use), start)

    def test_unconditional_combo_row_plays_and_preempts_finish_link(self):  # 4E60: rows 4E62, 4E61; E0 4E61 -> plays 4E62
        acts = {0x60: chain_acts(nxt=0x61, tr=(0, 1)), 0x61: chain_acts(), 0x62: chain_acts()}
        self.assertEqual(self.path(acts, [tr_row(0x62), tr_row(0x61)], 0x60), (2, [0x60, 0x62]))

    def test_ai_request_row_is_not_counted(self):  # 4E8E -> 4E92 (r13)
        acts = {0x8E: chain_acts(tr=(0,)), 0x92: chain_acts(hits=2)}
        self.assertEqual(self.path(acts, [tr_row(0x92, 13)], 0x8E), (1, [0x8E]))

    def test_finish_link_without_combo_row(self):  # 4E5C E0 = 4E60
        acts = {0x5C: chain_acts(nxt=0x60), 0x60: chain_acts()}
        self.assertEqual(self.path(acts, [], 0x5C), (2, [0x5C, 0x60]))

    def test_catch_stops_and_idle_row_ends(self):
        acts = {0x7C: chain_acts(hits=0, catch=True, tr=(0,)), 0xD1: chain_acts(attack=False),
                0x69: chain_acts(nxt=0x70, tr=(1,)), 0x70: chain_acts(), 0x3F1: chain_acts(attack=False, hits=0)}
        self.assertEqual(self.path(acts, [tr_row(0xD1), tr_row(0x3F1)], 0x7C), (0, [0x7C]))
        self.assertEqual(self.path(acts, [tr_row(0xD1), tr_row(0x3F1)], 0x69), (1, [0x69]))


if __name__ == "__main__":
    if "--table" in sys.argv:
        print(table())
        sys.exit(0)
    if bar.EXTRACT.is_dir():
        print(table())
    unittest.main()
