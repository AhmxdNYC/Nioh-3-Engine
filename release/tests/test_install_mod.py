import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
from types import SimpleNamespace
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("install_mod", Path(__file__).resolve().parents[1] / "install_mod.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class InstallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.package, self.game, self.saves, self.backups = [self.root / n for n in ("package", "game", "saves", "backups")]
        for p in (self.package, self.game, self.saves): p.mkdir()
        (self.game / "Nioh3.exe").write_bytes(b"fixture only")
        (self.saves / "account").mkdir()
        (self.saves / "account/SAVEDATA.BIN").write_bytes(b"CURRENT SAVE")
        for name, data in {"dinput8.dll": b"new loader", "Nioh3EngineFix.asi": b"new asi", "Nioh3EngineFix.ini": b"default settings"}.items():
            (self.package / name).write_bytes(data)
        (self.package / "SHA256SUMS.txt").write_text("\n".join(m.sha(self.package / n) + "  " + n for n in m.FILES if (self.package / n).exists()))
        (self.game / "dinput8.dll").write_bytes(b"old loader")

    def run_install(self, **kw):
        return m.install(self.package, self.game, self.saves, self.backups, running=kw.get("running", lambda: False))

    def receipt(self):
        return json.loads(next(self.backups.glob("*/receipt.json")).read_text())

    def test_success_and_update_keep_ini_new_backup(self):
        (self.game / "Nioh3EngineFix.ini").write_bytes(b"USER SETTINGS")
        first = self.run_install()
        self.assertEqual((first / "saves/account/SAVEDATA.BIN").read_bytes(), b"CURRENT SAVE")
        (self.saves / "account/SAVEDATA.BIN").write_bytes(b"NEWER SAVE")
        second = self.run_install()
        self.assertNotEqual(first, second)
        self.assertEqual((second / "saves/account/SAVEDATA.BIN").read_bytes(), b"NEWER SAVE")
        self.assertEqual((first / "saves/account/SAVEDATA.BIN").read_bytes(), b"CURRENT SAVE")
        self.assertEqual((self.game / "Nioh3EngineFix.ini").read_bytes(), b"USER SETTINGS")
        self.assertFalse((self.game / ".nioh3-mod-install.lock").exists())

    def test_combined_package_installs_action_records_and_keeps_training_ini(self):
        for name in ("Nioh3Training.asi","Nioh3Training_action_records.tsv","Nioh3Training.ini","Nioh3 Mod Website.url","SaveSlots-after-exit.pyz"):
            (self.package/name).write_bytes(b"combined payload")
        (self.package/"SHA256SUMS.txt").write_text("\n".join(m.sha(self.package/n)+"  "+n for n in m.FILES if (self.package/n).exists()))
        (self.game/"Nioh3Training.ini").write_bytes(b"USER TRAINING SETTINGS")
        self.run_install()
        self.assertEqual((self.game/"Nioh3Training_action_records.tsv").read_bytes(),b"combined payload")
        self.assertEqual((self.game/"Nioh3Training.ini").read_bytes(),b"USER TRAINING SETTINGS")
        self.assertEqual((self.saves/"account/SAVEDATA.BIN").read_bytes(),b"CURRENT SAVE")

    def test_running_game_refused_before_backup(self):
        with self.assertRaisesRegex(RuntimeError, "Close Nioh"):
            self.run_install(running=lambda: True)
        self.assertFalse(self.backups.exists())

    def test_bad_checksum_no_mutation(self):
        (self.package / "Nioh3EngineFix.asi").write_bytes(b"bad")
        with self.assertRaisesRegex(RuntimeError, "checksum"):
            self.run_install()
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"old loader")
        self.assertFalse(self.backups.exists())

    def test_missing_and_empty_save_root_refused(self):
        (self.saves / "account/SAVEDATA.BIN").unlink()
        with self.assertRaisesRegex(RuntimeError, "empty"):
            self.run_install()
        self.assertEqual(self.receipt()["status"], "failed")
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"old loader")

    def test_partial_save_copy_preserved_never_activated(self):
        original = m.exclusive_copy
        def fail(src, dst):
            original(src, dst)
            if "saves" in dst.parts: raise OSError("simulated full disk")
        with patch.object(m, "exclusive_copy", fail), self.assertRaises(OSError): self.run_install()
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"old loader")
        self.assertEqual(next(self.backups.glob("*/saves/account/SAVEDATA.BIN")).read_bytes(), b"CURRENT SAVE")
        self.assertEqual(self.receipt()["installed"], [])

    def test_save_change_mid_copy_refused(self):
        original = m.exclusive_copy
        def change(src, dst):
            original(src, dst)
            if src.parent.name == "account": src.write_bytes(b"CONCURRENT SAVE")
        with patch.object(m, "exclusive_copy", change), self.assertRaisesRegex(RuntimeError, "changed"):
            self.run_install()
        self.assertEqual((self.saves / "account/SAVEDATA.BIN").read_bytes(), b"CONCURRENT SAVE")
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"old loader")

    def test_game_starts_after_backup_no_activation(self):
        calls = iter([False, True])
        with self.assertRaisesRegex(RuntimeError, "Game started"):
            self.run_install(running=lambda: next(calls))
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"old loader")
        self.assertIn("saves", self.receipt())

    def test_partial_install_rolls_back_mods_not_saves(self):
        original = m.os.replace
        def fail(src, dst):
            if Path(dst).name == "Nioh3EngineFix.asi": raise OSError("activation failed")
            return original(src, dst)
        with patch.object(m.os, "replace", fail), self.assertRaises(OSError): self.run_install()
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"old loader")
        self.assertFalse((self.game / "Nioh3EngineFix.asi").exists())
        self.assertEqual((self.saves / "account/SAVEDATA.BIN").read_bytes(), b"CURRENT SAVE")
        self.assertEqual(self.receipt()["rollback"], [{"file": "dinput8.dll", "status": "restored"}])

    def test_external_change_is_not_overwritten_by_rollback(self):
        original = m.os.replace
        def fail(src, dst):
            if Path(dst).name == "Nioh3EngineFix.asi":
                (self.game / "dinput8.dll").write_bytes(b"SOMEONE ELSE")
                raise OSError("activation failed")
            return original(src, dst)
        with patch.object(m.os, "replace", fail), self.assertRaises(OSError): self.run_install()
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"SOMEONE ELSE")
        self.assertEqual(self.receipt()["status"], "failed_needs_review")
        self.assertTrue((self.game / ".nioh3-mod-install.lock").exists())

    def test_lock_busy_preserves_other_owner_lock(self):
        lock = self.game / ".nioh3-mod-install.lock"
        lock.write_text("other owner")
        with self.assertRaises(FileExistsError): self.run_install()
        self.assertEqual(lock.read_text(), "other owner")

    def test_backup_overlap_refused(self):
        self.backups = self.saves / "backup"
        with self.assertRaisesRegex(RuntimeError, "overlap"): self.run_install()
        self.assertFalse(self.backups.exists())

    def test_journal_failure_before_activation_no_mod_change(self):
        original = m.write_receipt
        def fail(path, data):
            if data["status"] == "backup_verified": raise OSError("journal failed")
            original(path, data)
        with patch.object(m, "write_receipt", fail), self.assertRaises(OSError): self.run_install()
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"old loader")

    def test_payload_copy_corruption_does_not_activate(self):
        original = m.exclusive_copy
        def corrupt(src, dst):
            original(src, dst)
            if dst.parent.resolve() == self.game.resolve(): dst.write_bytes(b"CORRUPT")
        with patch.object(m, "exclusive_copy", corrupt), self.assertRaisesRegex(RuntimeError, "payload changed"):
            self.run_install()
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"old loader")

    def test_target_changed_during_staging_retains_lock(self):
        original = m.exclusive_copy
        def change(src, dst):
            original(src, dst)
            if dst.parent.resolve() == self.game.resolve():
                (self.game / "dinput8.dll").write_bytes(b"EXTERNAL UPDATE")
        with patch.object(m, "exclusive_copy", change), self.assertRaisesRegex(RuntimeError, "target changed"):
            self.run_install()
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"EXTERNAL UPDATE")
        self.assertTrue((self.game / ".nioh3-mod-install.lock").exists())

    def test_game_starts_after_first_replace_no_later_install_or_live_rollback(self):
        def starts_after_first():
            return (self.game / "dinput8.dll").read_bytes() == b"new loader"
        with self.assertRaisesRegex(RuntimeError, "Game started"):
            self.run_install(running=starts_after_first)
        self.assertFalse((self.game / "Nioh3EngineFix.asi").exists())
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"new loader")
        self.assertEqual(self.receipt()["status"], "failed_needs_review")
        self.assertTrue((self.game / ".nioh3-mod-install.lock").exists())

    def test_failed_final_journal_keeps_lock_even_after_successful_rollback(self):
        original = m.write_receipt
        def fail_final(path, data):
            if data["status"] in ("installed", "failed"): raise OSError("journal unavailable")
            original(path, data)
        with patch.object(m, "write_receipt", fail_final), self.assertRaises(OSError):
            self.run_install()
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"old loader")
        self.assertTrue((self.game / ".nioh3-mod-install.lock").exists())

    def test_rollback_target_changed_during_copy_not_overwritten(self):
        original_copy, original_replace = m.exclusive_copy, m.os.replace
        def copy(src, dst):
            original_copy(src, dst)
            if src.parent.name == "mods-before":
                (self.game / "dinput8.dll").write_bytes(b"EXTERNAL DURING ROLLBACK")
        def fail(src, dst):
            if Path(dst).name == "Nioh3EngineFix.asi": raise OSError("activation failure")
            original_replace(src, dst)
        with patch.object(m, "exclusive_copy", copy), patch.object(m.os, "replace", fail), self.assertRaises(OSError):
            self.run_install()
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"EXTERNAL DURING ROLLBACK")
        self.assertTrue((self.game / ".nioh3-mod-install.lock").exists())

    def test_package_save_overlap_refused(self):
        self.saves = self.package
        with self.assertRaisesRegex(RuntimeError, "overlap"): self.run_install()

    def test_unreadable_nested_save_directory_never_counts_as_complete(self):
        original_scandir = m.os.scandir
        blocked = (self.saves / "account").resolve()
        def unreadable(path):
            if Path(path).resolve() == blocked:
                raise PermissionError("injected unreadable nested save directory")
            return original_scandir(path)
        with patch.object(m.os, "scandir", unreadable), self.assertRaises(PermissionError):
            self.run_install()
        self.assertEqual((self.game / "dinput8.dll").read_bytes(), b"old loader")
        self.assertFalse((self.game / "Nioh3EngineFix.asi").exists())
        receipt = self.receipt()
        self.assertEqual(receipt["status"], "failed")
        self.assertEqual(receipt["installed"], [])
        self.assertIn("unreadable nested save directory", receipt["error"])
        self.assertTrue(next(self.backups.iterdir()).is_dir())

    def test_packaging_contains_guarded_entry_points_and_checksums(self):
        package_spec = importlib.util.spec_from_file_location("package", Path(__file__).resolve().parents[1] / "package.py")
        pack = importlib.util.module_from_spec(package_spec)
        package_spec.loader.exec_module(pack)
        rel, mod = self.root / "release", self.root / "mod"
        (rel / "profiles").mkdir(parents=True)
        (mod / "bin").mkdir(parents=True)
        (mod / "bin/Nioh3EngineFix.asi").write_bytes(b"Nioh3EngineFix 1.2.3 ini keys set to -1 (auto)")
        (mod / "bin/README.md").write_text("fixture")
        (rel / "profiles/Nioh3EngineFix.all.ini").write_text("fixture")
        (rel / "profiles.json").write_text(json.dumps({"all": {"title": "fixture", "overrides": []}}))
        for name in ("install_mod.py", "Install-Mod.cmd", "install-mod.sh"):
            (rel / name).write_bytes((Path(__file__).resolve().parents[1] / name).read_bytes())
        (rel / "USER_GUIDE.md").write_text("fixture")
        with patch.object(pack, "REL", rel), patch.object(pack, "MOD", mod), \
             patch.object(pack, "LOADER", [self.package / "dinput8.dll"]), \
             patch.object(pack.subprocess, "run", return_value=SimpleNamespace(returncode=0, stdout="fixture gate", stderr="")):
            self.assertEqual(pack.main(["--out", str(self.root / "out")]), 0)
        with zipfile.ZipFile(next((self.root / "out").glob("*.zip"))) as archive:
            for name in ("install_mod.py", "Install-Mod.cmd", "install-mod.sh"):
                self.assertEqual(archive.read(name), (rel / name).read_bytes())
                self.assertIn(f"{m.sha(rel / name)}  {name}", archive.read("SHA256SUMS.txt").decode())


if __name__ == "__main__": unittest.main()
