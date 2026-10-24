"""Offline checks for the final-bench chart. No game."""
import os
import shutil
import tempfile
import unittest
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
import final_full_report as fr


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


class Means(unittest.TestCase):
    def test_means_skip_invalid_and_split_fg(self):
        root = tempfile.mkdtemp()
        try:
            header = "valid,arm,fg,avg_fps,low1_fps,hitches,cpu_temp,gpu_temp,cpu_power,gpu_power,cpu_util,gpu_util\n"
            write(os.path.join(root, "captures", "final-full", "pc", "results.csv"),
                  header + "1,ARM1-STOCKLIKE,off,10,8,1,60,50,40,100,20,80\n"
                  + "1,ARM1-STOCKLIKE,off,30,12,3,70,60,50,120,30,90\n"
                  + "0,ARM1-STOCKLIKE,off,999,999,999,999,999,999,999,999,999\n"
                  + "1,ARM2-PUREPERF,on,40,20,0,55,45,30,80,10,70\n")
            write(os.path.join(root, "captures", "final-full", "pc2", "results-fgon.csv"),
                  "arm,avg_fps,low1_fps\nARM3-FULLMOD,50,25\n")
            rows, skipped, _sources = fr.load_tree(root)
            self.assertEqual(skipped, 1)
            cells = { (c["device"], c["fg"], c["arm"]): c for c in fr.summarize(rows) }
            stock = cells[("pc", "off", "stock")]
            self.assertEqual(stock["n"], 2)
            self.assertEqual(stock["avg_fps"], 20)
            self.assertEqual(stock["low1_fps"], 10)
            self.assertEqual(stock["hitches"], 2)
            self.assertEqual(stock["cpu_temp"], 65)
            self.assertEqual(stock["gpu_power"], 110)
            self.assertEqual(cells[("pc", "on", "pure-perf")]["avg_fps"], 40)
            self.assertEqual(cells[("pc2", "on", "full")]["avg_fps"], 50)
            self.assertEqual(cells[("pc2", "on", "full")]["hitches"], None)
        finally:
            shutil.rmtree(root)

    def test_invalid_only_writes_no_chart(self):
        root = tempfile.mkdtemp()
        out = tempfile.mkdtemp()
        try:
            write(os.path.join(root, "captures", "final-full", "pc", "results.csv"),
                  "valid,arm,avg_fps,low1_fps\n0,ARM1-STOCKLIKE,77,51\n")
            info = fr.write_report(root, out)
            self.assertEqual(info["rows"], 0)
            self.assertEqual(info["charts"], [])
            with open(os.path.join(out, "report.md"), encoding="utf-8") as handle:
                text = handle.read()
            self.assertIn("No valid measurement", text)
            self.assertNotIn("77", text)
        finally:
            shutil.rmtree(root)
            shutil.rmtree(out)


if __name__ == "__main__":
    unittest.main()
