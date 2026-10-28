"""Chain-aware hit counts (human #41382, pc #41677): rule tests on synthetic tables plus the known archive facts.
The ChainRules cases test the AI-choice upper bound (chain_hits.chain, column ai_max); the menu uses plays_next (test_hit_labels.py).

    py -I tools/test_chain_hits.py
"""
import csv
import sys
import unittest
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import boss_action_record as bar  # noqa: E402
import chain_hits as c  # noqa: E402

NONE = (0xFFFF,) * 6


def act(motion, nxt=c.IDLE, attack=True, hits=1, tr=(), catch=False):
    return dict(row=0, motion=motion, nxt=nxt, attack=attack, hits=hits, groups=hits, tr=tr, catch=catch)


def row(target, req=0xFFFF):
    return (target,) + NONE[1:] + (req,)


def best(acts, rows, start):
    use = Counter(k for a in acts.values() for k in a["tr"])
    links, _ = c.build_links(acts, rows, use)
    return c.chain(acts, links, start, 6, False)


class ChainRules(unittest.TestCase):
    def test_finish_link_and_private_combo_link_add_hits(self):
        acts = {0x10: act(1000, nxt=0x11), 0x11: act(1001, tr=(0,)), 0x12: act(1002, hits=2)}
        self.assertEqual(best(acts, [row(0x12)], 0x10), (4, [0x10, 0x11, 0x12]))

    def test_locomotion_is_never_a_piece(self):
        for motion in (11, 31):
            acts = {0x10: act(1000, nxt=0x11, tr=(0,)), 0x11: act(motion, attack=False), 0x12: act(motion, attack=False)}
            self.assertEqual(best(acts, [row(0x12)], 0x10), (1, [0x10]))

    def test_shared_row_target_is_an_opener_not_a_piece(self):
        acts = {0x10: act(1000, tr=(0,)), 0x13: act(1003, tr=(0,)), 0x12: act(1002)}
        self.assertEqual(best(acts, [row(0x12)], 0x10), (1, [0x10]))

    def test_combo_link_needs_an_attack_block(self):
        acts = {0x10: act(1000, tr=(0,)), 0x12: act(1043, attack=False)}
        self.assertEqual(best(acts, [row(0x12)], 0x10), (1, [0x10]))
        acts[0x10]["nxt"] = 0x12  # the forced finish link still counts a hit-only piece
        self.assertEqual(best(acts, [row(0x12)], 0x10), (2, [0x10, 0x12]))

    def test_zero_power_catch_ends_the_chain(self):
        acts = {0x10: act(1000, tr=(0,), catch=True), 0x12: act(1020)}
        self.assertEqual(best(acts, [row(0x12)], 0x10), (1, [0x10]))

    def test_cycle_safe(self):
        acts = {0x10: act(1000, nxt=0x11), 0x11: act(1001, nxt=0x10)}
        self.assertEqual(best(acts, [], 0x10), (2, [0x10, 0x11]))


class ArchiveFacts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not bar.EXTRACT.is_dir():
            raise unittest.SkipTest("archive_01 not extracted")
        with bar.OUT.open(encoding="utf-8", newline="") as f:
            cls.rec = {(r["boss"], int(r["action_id"], 16)): r for r in csv.DictReader(f, delimiter="\t")}

    def test_known_counts(self):
        # pc #41677 rule: hit windows, zero-damage rows dropped, only continuations that play (no AI-request links)
        want = {("CE_YORITOMO_00", 0x4E4E): (1, "200"), ("CE_YORITOMO_00", 0x4E55): (1, "250"),
                ("CE_YORITOMO_00", 0x4E59): (2, "175/180"), ("CE_YORITOMO_00", 0x4E5C): (3, "140/150/205"),
                ("CE_YORITOMO_00", 0x4E6E): (2, "250/275"), ("CE_SHINGEN_00", 0x4E4E): (3, "85/85/240"),
                ("CE_OTOROSHI_00", 0x4E4E): (0, "")}
        for key, (hits, dmg) in want.items():
            r = self.rec[key]
            self.assertEqual((int(r["hit_count"]), r["damage_per_hit"]), (hits, dmg), key)

    def test_damage_groups_match_hits(self):
        for key, r in self.rec.items():
            n = int(r["hit_count"])
            self.assertEqual(len(r["damage_per_hit"].split("/")) if n else 0, n, key)
            self.assertLess(len(r["damage_per_hit"]), 96, key)  # ArRecord.damage

    def test_no_locomotion_in_any_path(self):
        with bar.GAINS.open(encoding="utf-8", newline="") as f:
            gains = list(csv.DictReader(f, delimiter="\t"))
        self.assertTrue(gains)
        motion = {(r["boss"], int(r["action_id"], 16)): int(r["motion"]) for r in self.rec.values()}
        for g in gains:
            for piece in g["path"].split("-")[1:]:
                self.assertNotIn(motion.get((g["boss"], int(piece, 16))), c.LOCOMOTION, g)


if __name__ == "__main__":
    unittest.main()
