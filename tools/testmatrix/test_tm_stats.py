"""Offline schema/selection/quantile and launch-pair negative controls."""
import copy
import csv
import json
import tempfile
import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import tm


class Stats(unittest.TestCase):
    def capture(self, values, **extra):
        fields = ["Application", "ProcessID", "SwapChainAddress", "FrameTime", *extra]
        rows = [dict(Application="Nioh3.exe", ProcessID="10", SwapChainAddress="0x1", FrameTime=v,
                     **{k: vals[i] for k, vals in extra.items()}) for i, v in enumerate(values)]
        return fields, rows

    def stats(self, fixture):
        fields, rows = fixture
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/"capture.csv"
            with p.open("w", newline="", encoding="utf-8-sig") as f:
                writer = csv.DictWriter(f, fieldnames=fields)
                writer.writeheader(); writer.writerows(rows)
            return tm.stats(p)

    def test_boundary_removed_from_all_metrics_without_mod_log(self):
        result = self.stats(self.capture([3000, 10, 10], GPUBusy=[999, 0, 4],
            GPUTime=[999, 8, 10], CPUBusy=[999, 2, 4], DisplayedTime=[999, 5, 5],
            AllInputToPhotonLatency=[999, 7, 9]))
        self.assertTrue(result["ok"])
        self.assertEqual((result["frames"], result["fps"], result["gpu_ms"], result["gpu_time_ms"],
                          result["cpu_ms"], result["display_fps"], result["lat_aip"]), (2, 100, 2, 9, 3, 200, 8))
        self.assertFalse(result["fg_off_verified"])

    def test_nearest_rank_and_tail_definition(self):
        result = self.stats(self.capture([3000, *range(1, 101)]))
        self.assertEqual(result["p99"], 99)
        self.assertEqual(result["low1"], round(1000/99, 2))
        self.assertEqual(tm.pct([1, 2], .99), 2)

    def test_later_hitch_is_retained(self):
        result = self.stats(self.capture([3000, 10, 1000, 10]))
        self.assertEqual(result["p99"], 1000)

    def test_invalid_primary_values_fail_closed(self):
        for value in [0, -1, "NA", "nan", "inf", "-inf", "bad"]:
            with self.subTest(value=value):
                self.assertFalse(self.stats(self.capture([3000, 10, value]))["ok"])

    def test_mixed_target_streams_refused_foreign_process_excluded(self):
        fixture = self.capture([3000, 10, 10])
        fixture[1][1]["SwapChainAddress"] = "0x2"
        self.assertFalse(self.stats(fixture)["ok"])
        fixture[1][1]["SwapChainAddress"] = "0x1"
        fixture[1].append(dict(Application="other.exe", ProcessID="11", SwapChainAddress="0x2", FrameTime=1))
        result = self.stats(fixture)
        self.assertEqual(result["fps"], 100)
        self.assertEqual(result["foreign_rows_excluded"], 1)
        fixture[1][1]["ProcessID"] = "12"
        self.assertFalse(self.stats(fixture)["ok"])

    def test_missing_identity_and_truncated_row_refused(self):
        self.assertFalse(self.stats((["FrameTime"], [{"FrameTime": 10}]))["ok"])
        fixture = self.capture([3000, 10, 10]); fixture[1][-1]["FrameTime"] = None
        self.assertFalse(self.stats(fixture)["ok"])

    def test_generated_and_dropped_display_metrics_stay_separate(self):
        result = self.stats(self.capture([3000, 10, 0, 10],
            FrameType=["Application", "Application", "Generated", "Application"],
            DisplayedTime=[999, 5, 5, "NA"], GPUBusy=[999, 2, 999, 2]))
        self.assertEqual((result["fps"], result["display_fps"], result["gpu_ms"]), (100, 200, 2))
        self.assertEqual(result["display_frames"], 2)
        self.assertTrue(result["frame_type_tracking"])

    def test_gpu_total_is_never_busy_fallback(self):
        fixture = self.capture([3000, 10, 10], GPUTime=[999, 8, 10])
        result = self.stats(fixture)
        self.assertIsNone(result["gpu_ms"]); self.assertEqual(result["gpu_time_ms"], 9)

    def test_v1_aliases(self):
        fixture = self.capture([3000, 10, 10], MsGPUBusy=[999, 2, 4],
            MsBetweenDisplayChange=[999, 20, 20], MsClickToPhotonLatency=[999, 6, 8])
        fixture = (["MsBetweenPresents" if k == "FrameTime" else k for k in fixture[0]],
                   [dict(MsBetweenPresents=r.pop("FrameTime"), **r) for r in fixture[1]])
        result = self.stats(fixture)
        self.assertEqual((result["fps"], result["display_fps"], result["lat_click"]), (100, 50, 7))

    def test_hitch_is_strictly_over_twice_median(self):
        over = self.stats(self.capture([1, 10, 10, 10, 30]))
        self.assertEqual(over["hitch_count"], 1)
        self.assertEqual(over["hitch_worst_ms"], 30)
        tied = self.stats(self.capture([1, 10, 10, 10, 20]))
        self.assertEqual(tied["hitch_count"], 0)
        self.assertIsNone(tied["hitch_worst_ms"])

    def test_lhm_temperature_and_power_headers(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "lhm.csv"
            p.write_text(
                "ts,/gpu/core [Temperature],/cpu/package [Power],/gpu/core [Power],/cpu/package [Temperature]\n"
                "2026-10-24T00:00:00,70,40,180,60\n"
                "2026-10-24T00:00:01,80,50,200,70\n",
                encoding="utf-8")
            got = tm.sensors(p)
        self.assertEqual(got["gpu_temp"], 75)
        self.assertEqual(got["cpu_temp"], 65)
        self.assertEqual(got["gpu_w"], 190)
        self.assertEqual(got["cpu_w"], 45)

    def test_hitch_columns_sum_and_table(self):
        with tempfile.TemporaryDirectory() as d:
            def dump(name, counts, worsts):
                windows = [dict(ok=True, kind="stand", fps=60, hitch_count=c, hitch_worst_ms=w)
                           for c, w in zip(counts, worsts)]
                Path(d, name).write_text(json.dumps(
                    {"cfg": "STOCK", "preset": "USER", "sweep": None, "valid": True, "windows": windows}),
                    encoding="utf-8")
            dump("final_pc_STOCK_USER_r1.json", [1, 2], [40, 55])
            dump("final_pc_STOCK_USER_r2.json", [3], [70])
            cell = tm.cells(d, "pc")[("STOCK", "USER", "")]
            self.assertEqual(cell["hitch_count"], 6)
            self.assertEqual(cell["hitch_worst_ms"], 70)
            text = tm.table_md(d, "pc")
        self.assertIn("| hitch n | hitch worst ms |", text)
        self.assertIn("|" + "---|" * 18, text)


def launches():
    records = []
    for index, (run, cfg, fps) in enumerate([(1,"STOCK",100),(1,"OPTIMIZED",112),
                                            (2,"OPTIMIZED",114),(2,"STOCK",106)]):
        stamp = f"2026-10-11T12:{index*2:02d}:00+00:00"
        windows = [dict(kind=k, ok=True, fps=fps, low1=80, p99=12.5, gpu_ms=5, cpu_ms=2,
                        fps_metric="FrameTime", frame_types=[], t0=stamp,
                        requested_s=20, duration_s=20) for k in ["stand","stand","move"]]
        records.append(dict(cfg=cfg, preset="USER", run=run, valid=True,
            graphics_xml="<Config><FRAME_GENERATION>OFF</FRAME_GENERATION></Config>",
            input=dict(asi_sha256="a"*64, ini_sha256="b"*64, stand_windows=2, window_s=20),
            full_protocol=False, window_protocol_complete=False, windows=windows))
    return records


class Pairs(unittest.TestCase):
    def test_end_to_end_no_logs_recomputes_csv_and_refuses_short_file(self):
        with tempfile.TemporaryDirectory() as d:
            records = launches()
            for j in records:
                for n, w in enumerate(j["windows"]):
                    path = Path(d)/f"{j['cfg']}_{j['run']}_{n}.csv"
                    w["csv"] = str(path)
                    fps = w["fps"]
                    with path.open("w", newline="") as f:
                        writer = csv.writer(f)
                        writer.writerow(["Application","ProcessID","SwapChainAddress","FrameTime"])
                        writer.writerow(["Nioh3.exe",10,"0x1",3000])
                        writer.writerows(["Nioh3.exe",10,"0x1",1000/fps] for _ in range(int(fps*20)))
                (Path(d)/f"final_pc_{j['cfg']}_USER_r{j['run']}.json").write_text(json.dumps(j),encoding="utf-8")
            result = tm.paired(d, "pc")
            self.assertEqual(result["presets"]["USER"]["balanced_blocks"], 1)
            self.assertEqual(result["presets"]["USER"]["blocks"][0]["mean_delta"]["fps"], 10)
            path = Path(records[0]["windows"][0]["csv"])
            path.write_text("Application,ProcessID,SwapChainAddress,FrameTime\nNioh3.exe,10,0x1,3000\nNioh3.exe,10,0x1,10\n",encoding="utf-8")
            with self.assertRaises(ValueError): tm.paired(d,"pc")

    def test_abba_is_one_block_not_two_replications(self):
        result = tm.paired_runs(launches()); cell = result["presets"]["USER"]
        self.assertEqual([p["delta"]["fps"] for p in cell["pairs"]], [12, 8])
        self.assertEqual(cell["blocks"][0]["mean_delta"]["fps"], 10)
        self.assertEqual(cell["balanced_blocks"], 1)
        self.assertEqual(cell["blocks"][0]["time_center_delta_s"], 0)
        self.assertIsNone(result["ci95"])
        self.assertFalse(cell["full_protocol"])

    def test_missing_duplicate_mixed_and_invalid_launches_refused(self):
        cases = []
        cases.append(launches()[:-1])
        cases.append(launches()+[copy.deepcopy(launches()[0])])
        records = launches(); records[0]["windows"].pop(); cases.append(records)
        records = launches(); records[0]["windows"][0]["duration_s"] = 1; cases.append(records)
        records = launches(); records[0]["graphics_xml"] = "<Config/>"; cases.append(records)
        records = launches(); records[0]["graphics_xml"] = "<Config><FRAME_GENERATION>OFF</FRAME_GENERATION><FRAME_GENERATION>OFF</FRAME_GENERATION></Config>"; cases.append(records)
        for path, value in [("fg", "FSR"), ("fg", ""), ("pin", "c"*64),
                            ("valid", False), ("window", False), ("metric", "DisplayedTime"), ("type", ["Generated"])]:
            records = launches()
            if path == "fg": records[0]["graphics_xml"] = f"<Config><FRAME_GENERATION>{value}</FRAME_GENERATION></Config>"
            elif path == "pin": records[0]["input"]["asi_sha256"] = value
            elif path == "valid": records[0]["valid"] = value
            elif path == "window": records[0]["windows"][0]["ok"] = value
            elif path == "metric": records[0]["windows"][0]["fps_metric"] = value
            else: records[0]["windows"][0]["frame_types"] = value
            cases.append(records)
        records = launches(); records[2]["windows"][0]["t0"] = "2026-10-11T12:10:00+00:00"
        records[2]["windows"][1]["t0"] = "2026-10-11T12:10:00+00:00"; cases.append(records)
        for i, records in enumerate(cases):
            with self.subTest(case=i), self.assertRaises(ValueError): tm.paired_runs(records)

    def test_unbalanced_tail_is_reported_without_ci(self):
        records = launches()
        for j in copy.deepcopy(records[:2]):
            j["run"] = 3
            records.append(j)
        self.assertEqual(tm.paired_runs(records)["presets"]["USER"]["unbalanced_tail_pairs"], 1)


if __name__ == "__main__":
    unittest.main()
