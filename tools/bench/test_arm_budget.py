"""Offline checks for the bench wall-time cut. No game and no PresentMon."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import arm_budget as ab


ROOT = Path(__file__).resolve().parents[2]


class Hitch(unittest.TestCase):
    def test_strictly_over_twice_median(self):
        hit = ab.hitch([10, 10, 10, 30])
        self.assertEqual(hit["count"], 1)
        self.assertEqual(hit["worst"], 30)
        tied = ab.hitch([10, 10, 10, 20])
        self.assertEqual(tied["count"], 0)
        self.assertIsNone(tied["worst"])

    def test_empty(self):
        self.assertEqual(ab.hitch([]), {"count": 0, "worst": None})


class WallTime(unittest.TestCase):
    def test_final_pc_arm_is_at_most_half(self):
        old = ab.final_pc_old_run_s()
        new = ab.final_pc_new_per_arm_s()
        self.assertEqual(old, 354)
        self.assertEqual(ab.final_pc_new_session_s(), 328)
        self.assertEqual(new, 164)
        self.assertLessEqual(new / old, 0.5)
        self.assertLessEqual(ab.final_pc_new_session_s() / (old * 3), 0.5)

    def test_audit_launches_drop_but_fixed_time_is_not_half(self):
        self.assertEqual(ab.audit_fixed_old_s(), 690)
        self.assertEqual(ab.audit_fixed_new_s(), 507)
        self.assertGreater(ab.audit_fixed_new_s() / ab.audit_fixed_old_s(), 0.5)
        self.assertEqual(ab.show()["audit_launches_new"], 3)

    def test_same_config_skips_launch(self):
        cells = [
            {"cfg": "STOCK", "preset": "USER"},
            {"cfg": "STOCK", "preset": "USER"},
            {"cfg": "OPTIMIZED", "preset": "USER", "sweep": "GI=0"},
            {"cfg": "OPTIMIZED", "preset": "USER", "sweep": "GI=0"},
        ]
        self.assertEqual(ab.plan_reuse(cells), [False, True, False, True])

    def test_chunk_stays_within_10_minutes(self):
        flags = [False, True, False, True, False, True]
        estimates = [248, 80, 248, 80, 248, 80]
        got = ab.take_chunk(estimates, flags, 600)
        self.assertLessEqual(sum(got), 600)
        self.assertEqual(got, [248, 80])
        over = ab.take_chunk([700, 10], [False, True], 600)
        self.assertEqual(over, [700, 10])

    def test_scripts_use_the_same_numbers(self):
        final = (ROOT / "tools" / "testmatrix" / "final_pc.ps1").read_text(encoding="utf-8")
        self.assertIn("[int]$Runs = 2", final)
        self.assertIn("launch = 160", final)
        self.assertIn("stats = 8", final)
        self.assertIn("close = 8", final)
        self.assertIn("WindowSec + 4", final)
        self.assertIn("[int]$ChunkSec = 600", final)
        audit = (ROOT / "tools" / "rematch" / "audit_screen_run.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("[int]$ChunkSec = 0", audit)
        self.assertIn("restart_required", audit)
        sched = (ROOT / "captures" / "final-bench" / "pc-plan" / "schedule.csv").read_text(encoding="utf-8")
        self.assertNotIn(",30,True,", sched)
        self.assertEqual(sched.count(",12,False,"), 3)
        runner = (ROOT / "captures" / "final-bench" / "run-pc-final.ps1").read_text(encoding="utf-8")
        self.assertIn("lhm_log.ps1", runner)
        self.assertIn("-WarmupSec 12", runner)
        self.assertIn("-ChunkSec 600", runner)
        self.assertNotIn("sensor_collector.ps1", runner)


if __name__ == "__main__":
    unittest.main()
