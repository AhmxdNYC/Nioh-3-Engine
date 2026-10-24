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
            self.assertIn("pending", text)
        finally:
            shutil.rmtree(root)
            shutil.rmtree(out)

    def test_ally_codes_and_sensor_aliases(self):
        root = tempfile.mkdtemp()
        out = tempfile.mkdtemp()
        try:
            write(os.path.join(root, "captures", "final-full", "ally", "results.csv"),
                  "label,arm,fg,avg_fps,low1_fps,hitches_2x,cpu_temp,gpu_temp,cpu_power,gpu_power,cpu_load,gpu_load\n"
                  "c1,a1,off,10,8,0,70,60,9,20,50,99\n"
                  "c2,a3,off,12,9,6,71,61,10,21,55,98\n")
            write(os.path.join(root, "captures", "final-full", "pc2", "results.csv"),
                  "run_id,arm,fg,avg_fps,hitch_n,cpu_c,gpu_c,cpu_w,gpu_w,cpu_pct,gpu_pct\n"
                  "r1,ARM2-PUREPERF,off,40,1,66,67,76,120,96,98\n")
            info = fr.write_report(root, out)
            self.assertEqual(info["rows"], 3)
            self.assertIn("avg_fps.svg", info["charts"])
            with open(os.path.join(out, "report.md"), encoding="utf-8") as handle:
                text = handle.read()
            self.assertIn("c1", text)
            self.assertIn("| ally | off | stock | 1 | 10.00 |", text)
            self.assertIn("| ally | off | full | 1 | 12.00 |", text)
            self.assertIn("| pc2 | off | pure-perf | 1 | 40.00 |", text)
            self.assertIn("pending for pc2 and the Ally", text)
            png = os.path.join(out, "avg_fps.png")
            if os.path.isfile(png):
                with open(png, "rb") as handle:
                    self.assertEqual(handle.read(8), b"\x89PNG\r\n\x1a\n")
        finally:
            shutil.rmtree(root)
            shutil.rmtree(out)


    def test_fgon_folder_is_pc2_on(self):
        root = tempfile.mkdtemp()
        try:
            write(os.path.join(root, "captures", "final-full", "pc2", "results.csv"),
                  "valid,arm,fg,avg_fps\n1,ARM1-STOCKLIKE,off,40\n")
            write(os.path.join(root, "captures", "final-full", "pc2-fgon", "results.csv"),
                  "valid,arm,avg_fps\n1,ARM1-STOCKLIKE,77\n")
            rows, skipped, _sources = fr.load_tree(root)
            self.assertEqual(skipped, 0)
            cells = {(c["device"], c["fg"], c["arm"]): c for c in fr.summarize(rows)}
            self.assertEqual(cells[("pc2", "off", "stock")]["avg_fps"], 40)
            self.assertEqual(cells[("pc2", "on", "stock")]["avg_fps"], 77)
            self.assertNotIn(("pc2-fgon", "off", "stock"), cells)
            groups = fr.chart_groups(list(fr.summarize(rows)))
            self.assertEqual(groups, [("pc2", "off"), ("pc2", "on")])
        finally:
            shutil.rmtree(root)


if __name__ == "__main__":
    unittest.main()
