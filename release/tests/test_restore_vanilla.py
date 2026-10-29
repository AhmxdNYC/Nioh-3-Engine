"""The game-folder shortcuts turn the mod off and back on. Saves and ini files stay. No game launch."""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OFF = ROOT / "Restore Vanilla Game.cmd"
ON = ROOT / "Put the mod back.cmd"
SH_OFF = ROOT / "restore-vanilla.sh"
SH_ON = ROOT / "put-mod-back.sh"
SH = shutil.which("sh")


def run(cmd, folder):
    env = os.environ.copy()
    env["NIOH3_RESTORE_SKIP_RUNNING_CHECK"] = "1"
    return subprocess.run(["cmd", "/d", "/c", str(cmd), str(folder), "--yes"],
                          env=env, capture_output=True, text=True)


def run_sh(script, folder):
    env = os.environ.copy()
    env["NIOH3_RESTORE_SKIP_RUNNING_CHECK"] = "1"
    return subprocess.run([SH, str(script).replace("\\", "/"), str(folder).replace("\\", "/"), "--yes"],
                          env=env, capture_output=True, text=True)


class RestoreVanillaTest(unittest.TestCase):
    @unittest.skipUnless(SH, "needs sh")
    def test_linux_scripts_round_trip(self):
        with tempfile.TemporaryDirectory() as raw:
            game = Path(raw)
            for name, data in (("Nioh3.exe", b"game"), ("dinput8.dll", b"loader"), ("Nioh3EngineFix.asi", b"ef"),
                               ("Nioh3Training.asi", b"tr"), ("Nioh3EngineFix.ini", b"ini-keep"), ("SAVEDATA.BIN", b"save-keep")):
                (game / name).write_bytes(data)
            off = run_sh(SH_OFF, game)
            self.assertEqual(off.returncode, 0, off.stdout + off.stderr)
            self.assertFalse((game / "dinput8.dll").exists())
            self.assertEqual((game / "dinput8.dll.mod-off").read_bytes(), b"loader")
            self.assertEqual((game / "Nioh3Training.asi.mod-off").read_bytes(), b"tr")
            self.assertEqual((game / "SAVEDATA.BIN").read_bytes(), b"save-keep")
            self.assertEqual(run_sh(SH_OFF, game).returncode, 0)
            back = run_sh(SH_ON, game)
            self.assertEqual(back.returncode, 0, back.stdout + back.stderr)
            self.assertEqual((game / "dinput8.dll").read_bytes(), b"loader")
            self.assertEqual((game / "Nioh3EngineFix.asi").read_bytes(), b"ef")
            self.assertFalse((game / "dinput8.dll.mod-off").exists())
            self.assertEqual((game / "Nioh3EngineFix.ini").read_bytes(), b"ini-keep")
            self.assertEqual(run_sh(SH_OFF, Path(raw) / "missing").returncode, 3)

    def test_round_trip_leaves_saves_and_settings(self):
        with tempfile.TemporaryDirectory() as raw:
            game = Path(raw)
            (game / "Nioh3.exe").write_bytes(b"game")
            (game / "dinput8.dll").write_bytes(b"loader")
            (game / "Nioh3EngineFix.asi").write_bytes(b"ef")
            (game / "Nioh3BossRematch.asi").write_bytes(b"br")
            (game / "Nioh3Training.asi").write_bytes(b"tr")
            (game / "Nioh3EngineFix.ini").write_bytes(b"ini-keep")
            (game / "SAVEDATA.BIN").write_bytes(b"save-keep")
            off = run(OFF, game)
            self.assertEqual(off.returncode, 0, off.stdout + off.stderr)
            self.assertFalse((game / "dinput8.dll").exists())
            self.assertEqual((game / "dinput8.dll.mod-off").read_bytes(), b"loader")
            self.assertEqual((game / "Nioh3EngineFix.asi.mod-off").read_bytes(), b"ef")
            self.assertEqual((game / "Nioh3BossRematch.asi.mod-off").read_bytes(), b"br")
            self.assertEqual((game / "Nioh3Training.asi.mod-off").read_bytes(), b"tr")
            self.assertEqual((game / "Nioh3EngineFix.ini").read_bytes(), b"ini-keep")
            self.assertEqual((game / "SAVEDATA.BIN").read_bytes(), b"save-keep")
            self.assertEqual((game / "Nioh3.exe").read_bytes(), b"game")
            again = run(OFF, game)
            self.assertEqual(again.returncode, 0, again.stdout + again.stderr)
            self.assertFalse((game / "dinput8.dll").exists())
            back = run(ON, game)
            self.assertEqual(back.returncode, 0, back.stdout + back.stderr)
            self.assertEqual((game / "dinput8.dll").read_bytes(), b"loader")
            self.assertEqual((game / "Nioh3Training.asi").read_bytes(), b"tr")
            self.assertFalse((game / "dinput8.dll.mod-off").exists())
            self.assertEqual((game / "SAVEDATA.BIN").read_bytes(), b"save-keep")
            self.assertEqual((game / "Nioh3EngineFix.ini").read_bytes(), b"ini-keep")

    def test_refuses_a_folder_without_the_game(self):
        with tempfile.TemporaryDirectory() as raw:
            result = run(OFF, Path(raw))
            self.assertEqual(result.returncode, 3, result.stdout + result.stderr)
            self.assertFalse(any(Path(raw).iterdir()))


if __name__ == "__main__":
    unittest.main()
