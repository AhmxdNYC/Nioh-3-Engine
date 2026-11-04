"""Site reports 7 + 8, offline: the sandbox (tutorial / dojo) rows stay real arena trips (human #65467: fight them in their arena with
your own gear; the gear is kept by Nioh3BossRematch's KeepGear veto, not by avoiding the arena), and Ibaraki has exactly one row."""
from pathlib import Path
import unittest

TSV = Path(__file__).resolve().parents[1] / "Nioh3Training_arenas.tsv"
SANDBOX = ["RC1D9", "RF5E5", "R6E65", "R1A5E", "RD759", "RE18C", "R9BE4", "R26F3", "RA0A1", "R2826"]


class ArenaSafety(unittest.TestCase):
    def test_sandbox_rows_are_real_trips(self):
        text = TSV.read_text(encoding="utf-8")
        for rid in SANDBOX:
            line = next((l for l in text.splitlines() if ("\t" + rid + "\t") in ("\t" + l + "\t")), None)
            self.assertIsNotNone(line, rid)
            self.assertNotIn("\tsummon\t", "\t" + line + "\t", rid)
            cols = line.split("\t")
            self.assertNotEqual(cols[2].strip(), "0x00", rid)  # a world: the trip goes to the arena

    def test_one_ibaraki_row(self):
        self.assertEqual(sum(l.split("\t")[3:4] == ["894462"] for l in TSV.read_text(encoding="utf-8").splitlines()), 1)


if __name__ == "__main__":
    unittest.main()
