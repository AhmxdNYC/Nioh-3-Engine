import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import updater as u


class UpdaterTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.url = "https://updates.example/manifest.json"
        self.payload = b"new binary"
        self.manifest = {"schema": 1, "version": "1.2.3", "news": "New release",
                         "files": [{"name": "Nioh3EngineFix.asi", "sha256": u.digest(self.payload),
                                    "url": "https://updates.example/engine.asi"}]}
        # Local fake manifest + payload; transport injected only in tests, no HTTP bypass in CLI.
        self.fixture = self.root / "manifest.json"
        self.fixture.write_text(json.dumps(self.manifest))
        self.net = patch.object(u, "fetch", side_effect=self.fetch).start()
        self.addCleanup(patch.stopall)
        patch.object(u, "game_running", return_value=False).start()

    def fetch(self, url, limit=u.MAX_FILE):
        return self.fixture.read_bytes() if url == self.url else self.payload

    def install(self):
        return u.update(self.root, self.url)

    def test_newer_binary_blocks_downgrade_without_payload_download(self):
        binary = self.root / "Nioh3EngineFix.asi"
        original = b"MZ\0Nioh3EngineFix 1.10.0 (built Oct 27 2026)\0"
        binary.write_bytes(original)
        messages = []
        result = u.update(self.root, self.url, messages.append)
        self.assertTrue(result["already_newer"])
        self.assertEqual(result["changed"], 0)
        self.assertEqual(binary.read_bytes(), original)
        self.assertEqual(self.net.call_count, 1)
        self.assertIn("Already newer", messages[-1])

    def test_version_uses_binary_not_stale_log_and_compares_numerically(self):
        (self.root / "Nioh3EngineFix.log").write_text("Nioh3EngineFix 99.0.0 (built yesterday)")
        self.assertIsNone(u.installed_version(self.root))
        self.assertGreater(u.version_tuple("1.10.0"), u.version_tuple("1.9.99"))
        self.assertLess(u.version_tuple("1.7.85"), u.version_tuple("2.0.0"))

    def test_action_records_ship_and_rollback(self):
        name="Nioh3Training_action_records.tsv"
        self.manifest["files"]=[{"name":name,"sha256":u.digest(self.payload),"url":"https://updates.example/records.tsv"}]
        self.fixture.write_text(json.dumps(self.manifest))
        result=self.install()
        self.assertEqual((self.root/name).read_bytes(),self.payload)
        u.rollback(self.root,result["backup"])
        self.assertFalse((self.root/name).exists())

    def test_install_backup_preserve_ini_and_rollback(self):
        binary = self.root / "Nioh3EngineFix.asi"
        ini = self.root / "Nioh3EngineFix.ini"
        binary.write_bytes(b"old")
        ini.write_bytes(b"player settings")
        self.manifest["files"].append({"name": ini.name, "sha256": u.digest(self.payload),
                                       "url": "https://updates.example/default.ini"})
        self.fixture.write_text(json.dumps(self.manifest))
        result = self.install()
        self.assertEqual(result["changed"], 1)
        self.assertEqual(binary.read_bytes(), self.payload)
        self.assertEqual(ini.read_bytes(), b"player settings")
        self.assertEqual(self.install()["changed"], 0)
        u.rollback(self.root, result["backup"])
        self.assertEqual(binary.read_bytes(), b"old")
        self.assertEqual(ini.read_bytes(), b"player settings")

    def test_manifest_cli_and_config_select_hidden_endpoint(self):
        (self.root / "Nioh3.exe").write_bytes(b"fake executable")
        cfg = self.root / "Nioh3Updater.ini"
        cfg.write_text("[Update]\nManifestUrl=" + self.url)
        with patch.object(u, "pause"):
            self.assertEqual(u.main(["--game-dir", str(self.root), "--config", str(cfg), "--no-pause"]), 0)
        self.assertEqual((self.root / "Nioh3EngineFix.asi").read_bytes(), self.payload)
        self.assertEqual(u.manifest_url(self.url, self.root / "missing.ini"), self.url)
        with patch.object(u, "update", return_value={"version":"1.2.3","changed":0,"backup":None}) as apply:
            self.assertEqual(u.main(["--game-dir", str(self.root), "--manifest", self.url, "--no-pause"]), 0)
            self.assertEqual(apply.call_args.args[1], self.url)

    def test_packaged_updater_prefers_adjacent_game(self):
        (self.root / "Nioh3.exe").write_bytes(b"fake executable")
        with patch.object(u.sys, "frozen", True, create=True), patch.object(u.sys, "executable", str(self.root / "Nioh3Updater.exe")), patch.object(u, "detect_game") as steam:
            self.assertEqual(u.choose_game(), self.root.resolve())
            steam.assert_not_called()

    def test_invalid_config_or_manifest_fails_before_writes(self):
        cfg = self.root / "Nioh3Updater.ini"
        for value in ("http://example.com/x", "https://user:pass@example.com/x", "https://example.com/x#frag", ""):
            cfg.write_text("[Update]\nManifestUrl=" + value)
            self.assertEqual(u.main(["--config", str(cfg), "--no-pause"]), 1)
        self.assertEqual(u.main(["--config", str(self.root / "missing.ini"), "--no-pause"]), 1)
        self.assertEqual(u.main(["--manifest", "http://example.com/x", "--no-pause"]), 1)
        self.net.assert_not_called()
        self.assertFalse((self.root / "nioh3-update-backups").exists())

    def test_new_files_removed_on_rollback(self):
        result = self.install()
        u.rollback(self.root, result["backup"])
        self.assertFalse((self.root / "Nioh3EngineFix.asi").exists())

    def test_hash_failure_changes_nothing(self):
        self.payload = b"corrupted"
        with self.assertRaisesRegex(ValueError, "SHA256"):
            self.install()
        self.assertFalse((self.root / "Nioh3EngineFix.asi").exists())

    def test_running_game_refuses_before_network(self):
        with patch.object(u, "game_running", return_value=True):
            with self.assertRaises(RuntimeError):
                self.install()
        self.net.assert_not_called()

    def test_process_inspection_failure_refuses(self):
        with patch.object(u, "game_running", side_effect=OSError("process check failed")):
            with self.assertRaises(OSError):
                self.install()
        self.net.assert_not_called()

    def test_manifest_rejects_paths_duplicates_hash_and_http(self):
        for key, value in [("name", "../evil.asi"), ("name", "dinput8.dll"),
                           ("sha256", "1234"), ("url", "http://example/a")]:
            with self.subTest(key=key, value=value):
                m = copy.deepcopy(self.manifest)
                m["files"][0][key] = value
                with self.assertRaises(ValueError):
                    u.validate(m)
        m = copy.deepcopy(self.manifest)
        m["files"] *= 2
        with self.assertRaises(ValueError):
            u.validate(m)

    def test_https_redirect_downgrade_refused(self):
        with self.assertRaises(ValueError):
            u.SecureRedirect().redirect_request(None, None, 302, "", {}, "http://example/a")

    def test_second_swap_failure_restores_first(self):
        for name in ("Nioh3EngineFix.asi", "Nioh3Training.asi"):
            (self.root / name).write_bytes(b"old")
        self.manifest["files"].append({"name": "Nioh3Training.asi",
                                       "sha256": u.digest(self.payload), "url": "https://example/training"})
        self.fixture.write_text(json.dumps(self.manifest))
        replace = u.os.replace
        def fail_second(src, dst):
            if ".nioh3-update-" in str(src) and Path(dst).name == "Nioh3Training.asi":
                raise OSError("simulated locked file")
            return replace(src, dst)
        with patch.object(u.os, "replace", side_effect=fail_second):
            with self.assertRaises(OSError):
                self.install()
        for name in ("Nioh3EngineFix.asi", "Nioh3Training.asi"):
            self.assertEqual((self.root / name).read_bytes(), b"old")

    def test_tampered_backup_refused(self):
        (self.root / "Nioh3EngineFix.asi").write_bytes(b"old")
        result = self.install()
        (self.root / "nioh3-update-backups" / result["backup"] / "Nioh3EngineFix.asi").write_bytes(b"bad")
        with self.assertRaisesRegex(ValueError, "integrity"):
            u.rollback(self.root, result["backup"])
        self.assertEqual((self.root / "Nioh3EngineFix.asi").read_bytes(), self.payload)

    def test_interrupted_update_requires_rollback(self):
        result = self.install()
        j = self.root / "nioh3-update-backups" / result["backup"] / "journal.json"
        journal = json.loads(j.read_text())
        journal["state"] = "prepared"
        u.write_json(j, journal)
        with self.assertRaisesRegex(RuntimeError, "Interrupted"):
            self.install()
        u.rollback(self.root, result["backup"])

    def test_concurrent_updater_refused(self):
        with u.locked(self.root):
            with self.assertRaises(FileExistsError):
                self.install()

    def test_rollback_path_escape_refused(self):
        with self.assertRaises(ValueError):
            u.rollback(self.root, "../elsewhere")

    def test_website_shortcut_update_and_rollback(self):
        name = "Nioh3 Mod Website.url"
        self.payload = (Path(__file__).resolve().parents[2] / "release" / name).read_bytes()
        self.assertIn(b"URL=https://nioh3-mod-feedback.vercel.app/", self.payload)
        self.manifest["files"] = [{"name": name, "sha256": u.digest(self.payload),
                                   "url": "https://updates.example/Nioh3%20Mod%20Website.url"}]
        self.fixture.write_text(json.dumps(self.manifest))
        old = b"[InternetShortcut]\nURL=https://example.com/old\n"
        (self.root / name).write_bytes(old)
        result = self.install()
        self.assertEqual((self.root / name).read_bytes(), self.payload)
        u.rollback(self.root, result["backup"])
        self.assertEqual((self.root / name).read_bytes(), old)


if __name__ == "__main__":
    unittest.main()
