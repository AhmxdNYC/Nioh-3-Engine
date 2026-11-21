import json
import tempfile
import unittest
import zipfile
from pathlib import Path

import pack_public_assets as pack

NOTES = "# vX\n\n- New thing one.\n- New thing two\n\nOff by default.\n"
OLD_NEWS = "1.8.25: crash fixes (GitHub #133); lower CPU use while waiting on frames."


class PackPublicAssetsTests(unittest.TestCase):
    def test_zip_and_manifest_contain_the_five_name_lists(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src = root / "src"
            src.mkdir()
            bodies = {
                "Nioh3EngineFix.asi": b"engine",
                "Nioh3Training.asi": b"training",
                "Nioh3BossRematch.asi": b"boss SoulCoreForms",
                "Nioh3CamKiProbe.asi": b"camera",
                "Nioh3CamKiProbe.ini": b"[CamKiProbe]\n",
                "Nioh3Updater.exe": b"updater-bytes",
                "stable.json": b'{"schema":1,"version":"1.8.1","news":"Names","files":[{"name":"Nioh3EngineFix.asi","sha256":"aa","url":"https://nioh3-mod-feedback.vercel.app/releases/1.8.1/Nioh3EngineFix.asi"}]}\n',
            }
            for name in pack.TSVS:
                bodies[name] = ("row " + name).encode()
            for name, data in bodies.items():
                (src / name).write_bytes(data)
            out = root / "out"
            result = pack.pack("1.8.1", out, {
                "Nioh3EngineFix.asi": src / "Nioh3EngineFix.asi",
                "Nioh3Training.asi": src / "Nioh3Training.asi",
                "Nioh3BossRematch.asi": src / "Nioh3BossRematch.asi",
                "Nioh3CamKiProbe.asi": src / "Nioh3CamKiProbe.asi",
            }, src / "Nioh3Updater.exe", src / "stable.json", {name: src / name for name in pack.TSVS},
               {name: src / name for name in pack.INIS}, notes=NOTES)
            with zipfile.ZipFile(result["zip"]) as archive:
                self.assertEqual(archive.namelist(), [
                    *pack.ASIS, *pack.INIS, *pack.TSVS, "Nioh3Updater.exe"])
                self.assertIn(b"camera", archive.read("Nioh3CamKiProbe.asi"))
                self.assertEqual(archive.read("Nioh3BossRematch_bosses.tsv"), b"row Nioh3BossRematch_bosses.tsv")
            manifest = json.loads((out / "stable.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["version"], "1.8.1")
            listed = [entry["name"] for entry in manifest["files"]]
            self.assertEqual(listed[0], "Nioh3EngineFix.asi")
            self.assertIn("Nioh3CamKiProbe.asi", listed)
            self.assertIn("Nioh3CamKiProbe.ini", listed)
            self.assertEqual([n for n in listed if n in pack.TSVS], list(pack.TSVS))
            bosses = next(entry for entry in manifest["files"] if entry["name"] == "Nioh3BossRematch_bosses.tsv")
            self.assertEqual(bosses["sha256"], pack.sha256(b"row Nioh3BossRematch_bosses.tsv"))
            self.assertTrue(bosses["url"].endswith("/Nioh3BossRematch_bosses.tsv"))
            self.assertNotIn("Nioh3EngineFix.ini", listed)
            self.assertNotIn("Nioh3Training.ini", listed)
            self.assertNotIn("Nioh3BossRematch.ini", listed)
            self.assertEqual(result["assets"][-1], "Nioh3Updater.exe")
            self.assertIn("Nioh3Training_arenas.tsv", result["assets"])

    def test_refuses_boss_rematch_without_soul_cores(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src = root / "src"
            src.mkdir()
            bodies = {
                "Nioh3EngineFix.asi": b"engine",
                "Nioh3Training.asi": b"training",
                "Nioh3BossRematch.asi": b"stale-boss",
                "Nioh3CamKiProbe.asi": b"camera",
                "Nioh3CamKiProbe.ini": b"[CamKiProbe]\n",
                "Nioh3Updater.exe": b"updater-bytes",
                "stable.json": b'{"schema":1,"version":"1.8.1","news":"Names","files":[]}\n',
            }
            for name in pack.TSVS:
                bodies[name] = ("row " + name).encode()
            for name, data in bodies.items():
                (src / name).write_bytes(data)
            with self.assertRaises(ValueError) as caught:
                pack.pack("1.8.4", root / "out", {
                    "Nioh3EngineFix.asi": src / "Nioh3EngineFix.asi",
                    "Nioh3Training.asi": src / "Nioh3Training.asi",
                    "Nioh3BossRematch.asi": src / "Nioh3BossRematch.asi",
                    "Nioh3CamKiProbe.asi": src / "Nioh3CamKiProbe.asi",
                }, src / "Nioh3Updater.exe", src / "stable.json",
                   {name: src / name for name in pack.TSVS},
                   {name: src / name for name in pack.INIS}, notes=NOTES)
            self.assertIn("SoulCoreForms", str(caught.exception))

    def test_news_is_written_from_this_release_notes(self):
        base = json.dumps({"schema": 1, "version": "1.8.36", "news": OLD_NEWS, "files": []}).encode()
        out = json.loads(pack.apply_news(base, "1.8.37", NOTES, OLD_NEWS))
        self.assertEqual(out["news"], "1.8.37: New thing one; New thing two.")
        self.assertEqual(out["version"], "1.8.36")  # apply_news touches only news

    def test_gate_fails_old_news_carried_forward(self):
        # the 1.8.26..1.8.37 bug: base news copied unchanged
        with self.assertRaisesRegex(ValueError, "not 1.8.37"):
            pack.check_news(OLD_NEWS, "1.8.37", OLD_NEWS)
        # same text relabelled with the new version still fails
        with self.assertRaisesRegex(ValueError, "repeats"):
            pack.check_news("1.8.37: crash fixes (GitHub #133); lower CPU use while waiting on frames.",
                            "1.8.37", OLD_NEWS)
        with self.assertRaisesRegex(ValueError, "empty"):
            pack.check_news("", "1.8.37", OLD_NEWS)
        with self.assertRaisesRegex(ValueError, "no text"):
            pack.check_news("1.8.37: ", "1.8.37", OLD_NEWS)

    def test_gate_passes_correct_news(self):
        pack.check_news("1.8.37: reports include the Player log when Player log is on.", "1.8.37", OLD_NEWS)
        pack.check_news("1.8.37: first release news.", "1.8.37", None)

    def test_notes_without_bullets_are_refused(self):
        with self.assertRaisesRegex(ValueError, "bullet"):
            pack.news_from_notes("# v1.8.37\n\nJust prose.\n", "1.8.37")

    def test_pack_refuses_without_notes(self):
        with self.assertRaisesRegex(ValueError, "notes"):
            pack.apply_news(b"{}", "1.8.37", "", None)

    def test_rejects_a_bad_version(self):
        with self.assertRaises(ValueError):
            pack.pack("1.7", Path("."), {}, Path("."), Path("."), {}, {})

    def test_url_folder_follows_new_version_not_base(self):
        names = (*pack.ASIS, *pack.INIS, *pack.TSVS)
        base = {"version": "1.8.23", "files": [
            {"name": n, "sha256": "0", "url": "https://h.example/releases/1.8.23/" + n} for n in names]}
        out = json.loads(pack.manifest_with_name_lists(json.dumps(base).encode(), "1.8.24",
                                                       {n: n.encode() for n in names}))
        self.assertTrue(all(f["url"] == "https://h.example/releases/1.8.24/" + f["name"] for f in out["files"]))


if __name__ == "__main__":
    unittest.main()
