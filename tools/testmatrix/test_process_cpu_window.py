import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch

from process_cpu_window import collect, interval, parse_proc_stat


class CpuWindowTests(unittest.TestCase):
    def test_linux_name_and_tick_units(self):
        fields = ["0"] * 22
        fields[0], fields[11], fields[12], fields[19] = "S", "250", "50", "9876"
        self.assertEqual(parse_proc_stat("123 (Nioh3 (game)) " + " ".join(fields), 100),
                         {"identity": "9876", "cpu_seconds": 3})

    def test_cpu_is_multicore_time_not_frame_busy(self):
        a = dict(identity="1", monotonic_ns=1_000_000_000, cpu_seconds=10)
        b = dict(identity="1", monotonic_ns=3_000_000_000, cpu_seconds=16)
        self.assertEqual(interval(a, b)["cpu_cores"], 3)
        for change in ({"identity": "2"}, {"cpu_seconds": 9},
                       {"cpu_seconds": float("nan")}, {"monotonic_ns": 0}):
            with self.assertRaises((RuntimeError, ValueError)):
                interval(a, dict(b, **change))

    def test_wrong_identity_records_failure_without_sleeping(self):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / "sample.jsonl"
            with patch("process_cpu_window.snapshot", return_value={"identity": "new"}), \
                    patch("process_cpu_window.time.sleep") as sleep:
                with self.assertRaises(RuntimeError):
                    collect(1, 30, out, "WG-OFF", "old")
                sleep.assert_not_called()
            self.assertIn('"complete": false', out.read_text())
            with self.assertRaises(FileExistsError):
                collect(1, 30, out, "WG-OFF")


if __name__ == "__main__":
    unittest.main()
