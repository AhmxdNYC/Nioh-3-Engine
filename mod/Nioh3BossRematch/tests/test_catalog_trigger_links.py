#!/usr/bin/env python3
"""Offline: WaitWarpStart links through EndWarpRights Create; interact is not Death Veil's start."""
import os
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "re" / "scripts_re"))
import rematch_catalog as catalog
import gen_native_entry_gates as native_gates

DATA_LFM = pathlib.Path(os.environ.get("NIOH3_TEST_LFM", catalog.LFM))
RB24D_SCRIPT = DATA_LFM / "archive_09" / "37" / "663667288B5D953E8659AC1C10FED8AFA79DE0E767A85C9276BCABBBEEE18D"


class TriggerLinkTests(unittest.TestCase):
    @unittest.skip("main keeps the one-gate-per-mission fallback: the strict rule dropped 8 live-confirmed gates (#106048 merge)")
    def test_unique_unrelated_wait_gate_is_not_selected(self):
        fi = {0xB24D: {"gate": [{"id": 573194, "flags": "door", "sub": "570909 571811"}]}}
        self.assertIsNone(catalog.pick_gate(fi, 0xB24D, 573339))

    def test_end_warp_create_selects_door(self):
        fi = {0xB24D: {"gate": [
            {"id": 573194, "flags": "door", "sub": "570909 572719 573339 576527 577980"},
            {"id": 999, "flags": "box", "sub": "573339"},
        ]}}
        self.assertEqual(catalog.pick_gate(fi, 0xB24D, 573339), (573194, "door"))

    def test_rb24d_door_not_hiruko_interact(self):
        if not RB24D_SCRIPT.is_file():
            self.skipTest("local LFM extraction is not available")
        catalog.LFM = DATA_LFM
        starts = catalog._script_box_calls(RB24D_SCRIPT)[None]
        self.assertTrue(any(fn == "tenshu_location_switch" and gate == 573194 and kind == "door" and 573339 in linked
                            for fn, gate, kind, linked in starts))
        self.assertFalse(any(kind == "interaction" for _fn, _gate, kind, _linked in starts))
        fi = {0xB24D: {"gate": [
            {"id": gate, "flags": kind, "sub": " ".join(map(str, linked))}
            for _fn, gate, kind, linked in starts]}}
        self.assertEqual(catalog.pick_gate(fi, 0xB24D, 573339), (573194, "door"))

    def test_native_gate_generator_links_rb24d_end_warp(self):
        if not RB24D_SCRIPT.is_file():
            self.skipTest("local LFM extraction is not available")
        actual = native_gates.dis.load(RB24D_SCRIPT)
        paths = native_gates.gate_paths(actual, 573339)
        self.assertTrue(any(x["gate"] == 573194 for x in paths))
        self.assertFalse(any(x["gate"] == 573339 for x in paths))

    def test_create_exchange_head_is_not_a_gate(self):
        if not DATA_LFM.is_dir():
            self.skipTest("local LFM extraction is not available")
        catalog.LFM = DATA_LFM
        fi = catalog.load_fight_index()
        # REA75 368649 wait is a debug door; 364374 is CreateExchangeHead, not Create.
        self.assertIsNone(catalog.pick_gate(fi, 0xEA75, 364374))


if __name__ == "__main__":
    unittest.main(verbosity=2)
