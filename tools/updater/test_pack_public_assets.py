import tempfile
import unittest
import zipfile
from pathlib import Path

import pack_public_assets as pack


class PackPublicAssetsTests(unittest.TestCase):
    def test_zip_contains_updater_and_asset_copy_matches(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src = root / "src"
            src.mkdir()
            bodies = {
                "Nioh3EngineFix.asi": b"engine",
                "Nioh3Training.asi": b"training",
                "Nioh3BossRematch.asi": b"boss",
                "Nioh3Updater.exe": b"updater-bytes",
                "stable.json": b'{"schema":1,"version":"1.7.90"}\n',
            }
            for name, data in bodies.items():
                (src / name).write_bytes(data)
            out = root / "out"
            result = pack.pack("1.7.90", out, {
                "Nioh3EngineFix.asi": src / "Nioh3EngineFix.asi",
                "Nioh3Training.asi": src / "Nioh3Training.asi",
                "Nioh3BossRematch.asi": src / "Nioh3BossRematch.asi",
            }, src / "Nioh3Updater.exe", src / "stable.json")
            with zipfile.ZipFile(result["zip"]) as archive:
                self.assertEqual(archive.namelist(), [
                    "Nioh3EngineFix.asi", "Nioh3Training.asi",
                    "Nioh3BossRematch.asi", "Nioh3Updater.exe"])
                self.assertEqual(archive.read("Nioh3Updater.exe"), b"updater-bytes")
            self.assertEqual((out / "Nioh3Updater.exe").read_bytes(), b"updater-bytes")
            self.assertEqual((out / "stable.json").read_bytes(), bodies["stable.json"])
            self.assertEqual(result["assets"][-1], "Nioh3Updater.exe")
            self.assertTrue(result["assets"][0].endswith("-combined.zip"))
            sums = (out / "SHA256SUMS.txt").read_text(encoding="utf-8")
            self.assertIn(pack.sha256(b"updater-bytes") + "  Nioh3Updater.exe", sums)

    def test_rejects_a_bad_version(self):
        with self.assertRaises(ValueError):
            pack.pack("1.7", Path("."), {}, Path("."), Path("."))


if __name__ == "__main__":
    unittest.main()
