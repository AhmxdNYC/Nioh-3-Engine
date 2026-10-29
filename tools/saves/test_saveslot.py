"""Disposable synthetic-save tests; never discovers or opens real save paths."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import saveslot as s

ACCOUNT='76561198000000000'
def fixture(root, value=b'A', account=ACCOUNT):
    base=root/'Savedata'/account
    for rel in ['SAVEDATA00/SAVEDATA.BIN','SAVEDATA00/BACKUP.BIN','SYSTEMSAVEDATA00/SAVEDATA.BIN']:
        p=base/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(value*4096)
    (root/'config.xml').write_bytes(b'<config>'+value+b'</config>')
    (root/'empty').mkdir(exist_ok=True)

class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='nioh3-copy-test-')
        self.root=Path(self.tmp.name)/'sandbox';s.init(self.root);fixture(self.root/'current');self.m=s.Slots(self.root)
        self.a=s.inventory(self.root/'current');self.m.snapshot('A')
        fixture(self.root/'current',b'B');self.b=s.inventory(self.root/'current')
    def tearDown(self): self.tmp.cleanup()
    def test_switch_restore_exact(self):
        r=self.m.switch('A');self.assertEqual(s.inventory(self.root/'current'),self.a)
        self.m.restore(r['undo']);self.assertEqual(s.inventory(self.root/'current'),self.b)
        self.assertEqual(self.m.verify('A')['tree'],self.a)
    def test_removed_and_extra_files(self):
        (self.root/'current'/'extra.bin').write_bytes(b'extra')
        before=s.inventory(self.root/'current');r=self.m.switch('A')
        self.assertFalse((self.root/'current'/'extra.bin').exists())
        self.m.restore(r['undo']);self.assertEqual(s.inventory(self.root/'current'),before)
    def test_mandatory_backup_before_mutation(self):
        with self.assertRaises(RuntimeError):self.m.switch('A',fail_at='backed-up')
        self.assertEqual(s.inventory(self.root/'current'),self.b)
        tx=next((self.root/'transactions').iterdir())
        self.assertEqual(s.inventory(tx/'backup'),self.b)
        self.assertTrue(s.read(tx/'backup.json')['verified'])
        self.assertFalse((tx/'previous').exists())
    def test_backup_copy_failure_preserves_current(self):
        original=s.copy_verified
        def fail_backup(src,dst):
            if dst.name=='backup':raise OSError('backup disk full')
            return original(src,dst)
        with patch.object(s,'copy_verified',side_effect=fail_backup):
            with self.assertRaises(OSError):self.m.switch('A')
        self.assertEqual(s.inventory(self.root/'current'),self.b)
        self.assertFalse((self.root/'pending.json').exists())
    def test_switch_retains_independent_backup(self):
        result=self.m.switch('A');tx=self.root/'transactions'/result['undo']
        self.assertEqual(s.inventory(tx/'backup'),self.b)
        self.assertEqual(s.inventory(tx/'previous'),self.b)
        self.m.restore(result['undo'])
        self.assertEqual(s.inventory(tx/'backup'),self.b)
    def test_slot_immutable(self):
        with self.assertRaises(ValueError):self.m.snapshot('A')
        self.assertEqual(self.m.verify('A')['tree'],self.a)
    def test_tampered_slot_refused(self):
        (self.root/'slots/A/data/config.xml').write_bytes(b'tampered')
        with self.assertRaises(ValueError):self.m.switch('A')
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_account_mismatch(self):
        (self.root/'current/Savedata'/ACCOUNT).rename(self.root/'current/Savedata/76561198000000001')
        before=s.inventory(self.root/'current')
        with self.assertRaises(ValueError):self.m.switch('A')
        self.assertEqual(s.inventory(self.root/'current'),before)
    def test_missing_pair(self):
        (self.root/'current/Savedata'/ACCOUNT/'SAVEDATA00/BACKUP.BIN').unlink()
        with self.assertRaises(ValueError):self.m.snapshot('incomplete')
    def test_interruptions_recover(self):
        for phase in ['prepared','detached','installed']:
            with self.subTest(phase=phase):
                with self.assertRaises(RuntimeError):self.m.switch('A',fail_at=phase)
                with self.assertRaises(ValueError):self.m.snapshot('blocked')
                self.m.recover();self.assertEqual(s.inventory(self.root/'current'),self.b)
                self.assertEqual(self.m.recover(),{'recovery':'none'})
    def test_lock_refuses_competing_writer(self):
        with self.m.lock():
            with self.assertRaises(FileExistsError):self.m.switch('A')
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_bad_names(self):
        for name in ['../escape','A/B','C:\\x','','.', 'a'*49,' trailing','trailing ','CON','com1','NUL']:
            with self.assertRaises(ValueError):self.m.snapshot(name)
    def test_named_save_with_spaces(self):
        self.m.snapshot('Yoritomo Practice')
        self.assertEqual(self.m.verify('Yoritomo Practice')['tree'],self.b)
        self.m.switch('A');self.m.switch('Yoritomo Practice')
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_init_existing_refused(self):
        with self.assertRaises(FileExistsError):s.init(self.root)
    def test_installed_paths_refused(self):
        for name in ['KoeiTecmo/NIOH3','Steam/userdata/123/3681010']:
            with self.assertRaises(ValueError):s.init(Path(self.tmp.name)/name)
    def test_hardlinks_refused(self):
        import os
        os.link(self.root/'current/config.xml',self.root/'current/link.xml')
        with self.assertRaises(ValueError):self.m.snapshot('linked')
    def test_failed_copy_keeps_current(self):
        with patch.object(s.shutil,'copyfileobj',side_effect=OSError('disk full')):
            with self.assertRaises(OSError):self.m.switch('A')
        self.assertEqual(s.inventory(self.root/'current'),self.b)
        self.assertFalse((self.root/'pending.json').exists())
    def test_backup_tamper_blocks_recovery(self):
        with self.assertRaises(RuntimeError):self.m.switch('A',fail_at='installed')
        tx=s.read(self.root/'pending.json')['id']
        (self.root/'transactions'/tx/'previous/config.xml').write_bytes(b'bad')
        with self.assertRaises(ValueError):self.m.recover()
        self.assertTrue((self.root/'pending.json').exists())
        self.assertEqual(s.inventory(self.root/'current'),self.a)
    def test_source_change_during_copy(self):
        original=s.shutil.copyfileobj
        def change(a,b):
            original(a,b)
            (self.root/'slots/A/data/config.xml').write_bytes(b'changed')
        with patch.object(s.shutil,'copyfileobj',side_effect=change):
            with self.assertRaises(ValueError):self.m.switch('A')
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_large_binary_roundtrip(self):
        p=self.root/'current/Savedata'/ACCOUNT/'SAVEDATA00/SAVEDATA.BIN'
        p.write_bytes(bytes(range(256))*36865)
        before=s.inventory(self.root/'current');r=self.m.switch('A');self.m.restore(r['undo'])
        self.assertEqual(s.inventory(self.root/'current'),before)

if __name__=='__main__':unittest.main(verbosity=2)
