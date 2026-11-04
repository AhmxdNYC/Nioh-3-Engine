"""Offline arena policy, committed data, and C/Python scanner parity tests."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
import auto_arena as aa


class ArenaSafety(unittest.TestCase):
    def test_capabilities_not_names(self):
        for symbol in (b"Item::ReserveAllItemEdo", b"Item::ReserveAllItemAll", b"Item::ClearEquipment"):
            self.assertTrue(aa.inventory_script(b"prefix\0" + symbol + b"\0suffix"))
        self.assertFalse(aa.inventory_script(b"Tutorial::GetCount\0Item::AddEquipment\0"))
        self.assertFalse(aa.inventory_script(None))

    def test_future_mission_and_shared_world(self):
        enemy = dict(pid=51, chr=61, fl=4, pos=(100, 200, 300), yawf=0, name="NEW_BOSS")
        base = dict(mid=123, world=33, kind=0, flags=0, name_id=0, shipped=True,
                    enemies=[enemy], start=[], box=[], trig=[], gate=[], inventory_script=True)
        shared = dict(base, mid=124, inventory_script=False)
        safe = dict(base, mid=125, world=34, inventory_script=False)
        rows = aa.build_rows([base, shared, safe], {}, lambda _: None)
        self.assertEqual([r["kind"] for r in rows], ["summon", "summon", "field"])
        self.assertEqual([r["world"] for r in rows], [0, 0, 34])
        self.assertEqual([r["chr"] for r in rows], [61, 61, 61])

    def test_old_summon_row_does_not_select_zero_placement(self):
        enemy = dict(pid=51, chr=61, fl=4, pos=(100, 200, 300), yawf=0, name="NEW_BOSS")
        invalid = dict(enemy, pid=0, chr=0)
        m = dict(mid=123, world=33, kind=0, flags=0, name_id=0, shipped=True,
                 enemies=[invalid, enemy], start=[], box=[], trig=[], gate=[], inventory_script=True)
        old = {("R007B", True): ["0x007B", "0", "0", "61", "0"]}
        rows = aa.build_rows([m], old, lambda _: None)
        self.assertEqual([(r["chr"], r["kind"]) for r in rows], [(61, "summon")])

    def test_game_data_and_c_parity(self):
        game = aa.game_dir(None)
        if not game:
            self.skipTest("game archives unavailable; synthetic tests still run")
        lfm = aa.Lfm(game)
        missions = aa.mission_data(lfm, aa.find_tables(lfm, lambda _: None), lambda _: None)
        fresh = aa.build_rows(missions, {}, lambda _: None)
        cached = aa.build_rows(missions, aa.old_table(aa.TSV), lambda _: None)
        changed = [r for r in cached if r["kind"] == "summon"]
        self.assertEqual(len(changed), 10)
        by_id = {r["id"]: r for r in cached}
        table = aa.old_table(aa.TSV)
        for r in changed:
            c = table[(r["id"], True)]
            self.assertEqual(int(c[3]), r["chr"])
            self.assertEqual([int(c[i], 0) for i in (1, 2, 4)], [0, 0, 0])
        self.assertEqual(by_id["RC1D9"]["kind"], "summon")
        self.assertEqual(by_id["RA4B4"]["kind"], "arena")
        self.assertEqual(by_id["RD617"]["kind"], "arena")
        self.assertEqual(sum(r["chr"] == 894462 for r in cached), 1)
        with tempfile.TemporaryDirectory() as d:
            exe = Path(d) / "scan.exe"
            subprocess.run([sys.executable, "-m", "ziglang", "cc", "-O2", "-include", "windows.h",
                            str(Path(__file__).with_name("arena_scan_test.c")), "-o", str(exe)], check=True)
            run = subprocess.run([str(exe), game], check=True, capture_output=True, text=True, timeout=120)
        def columns(r):
            return [f'0x{r["mission"]:04X}', str(r["start"]), f'0x{r["world"]:02X}', str(r["chr"]),
                    str(r["pid"]), *(f"{v:.0f}" for v in r["p"][:3]), f'{r["p"][3]:.1f}',
                    *(f"{v:.0f}" for v in r["b"]), r["kind"], r["id"]]
        self.assertEqual([line.split("\t") for line in run.stdout.splitlines()], [columns(r) for r in fresh])
        print(f"PASS: {len(fresh)} C/Python rows agree; 10 inventory rows summon-only; Ibaraki one row")


if __name__ == "__main__":
    unittest.main()
