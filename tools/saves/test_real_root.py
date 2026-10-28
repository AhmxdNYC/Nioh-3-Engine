import unittest
from unittest.mock import patch
import saveslot as s
import test_saveslot as base
from real_root import RealRoot
import after_exit

class RealTests(base.Tests):
    def setUp(self):
        super().setUp()
        self.live=self.root.parent/'copied-game-root'
        base.fixture(self.live,b'C')
        (self.live/'extra-settings.bin').write_bytes(b'preserve all root files')
        self.before=s.inventory(self.live)
        self.adapter=RealRoot(self.root,self.live,lambda:True)
    def test_full_backup_switch(self):
        result=self.adapter.switch('A')
        from pathlib import Path
        self.assertEqual(s.inventory(self.live),self.a)
        self.assertEqual(s.inventory(Path(result['backup'])),self.before)
        self.assertEqual(s.inventory(Path(result['previous'])),self.before)
    def test_real_backup_failure(self):
        with patch.object(s,'copy_verified',side_effect=OSError('no space')):
            with self.assertRaises(OSError):self.adapter.switch('A')
        self.assertEqual(s.inventory(self.live),self.before)
    def test_real_recovery(self):
        for phase in ('detached','installed'):
            with self.assertRaises(RuntimeError):self.adapter.switch('A',fail_at=phase)
            self.adapter.recover()
            self.assertEqual(s.inventory(self.live),self.before)
    def test_real_no_mutation_before_backup(self):
        with self.assertRaises(RuntimeError):self.adapter.switch('A',fail_at='backed-up')
        self.assertEqual(s.inventory(self.live),self.before)
    def test_real_game_appears(self):
        calls=0
        def clear():
            nonlocal calls
            calls+=1;return calls<3
        adapter=RealRoot(self.root,self.live,clear)
        with self.assertRaises(ValueError):adapter.switch('A')
        self.assertEqual(s.inventory(self.live),self.before)
    def test_real_account_mismatch(self):
        (self.live/'Savedata'/base.ACCOUNT).rename(self.live/'Savedata/76561198000000001')
        before=s.inventory(self.live)
        with self.assertRaises(ValueError):self.adapter.switch('A')
        self.assertEqual(s.inventory(self.live),before)
    def test_real_overlap_refused(self):
        with self.assertRaises(ValueError):RealRoot(self.root,self.root/'current',lambda:True)
    def test_real_snapshot(self):
        self.adapter.snapshot('copied-root')
        self.assertEqual(self.m.verify('copied-root')['tree'],self.before)
        with self.assertRaises(ValueError):self.adapter.snapshot('copied-root')
        self.assertEqual(s.inventory(self.live),self.before)
    def test_helper_real_root_after_exit_only(self):
        class Game:
            code=None
            def __init__(self,*args):pass
            def exited(self):return self.code
            def close(self):pass
        def advance(_):
            self.assertEqual(s.inventory(self.live),self.before)
            self.assertEqual(list((self.root/'transactions').iterdir()),[])
            Game.code=0
        after_exit.run(self.root,123,456,'A','real-test','switch',real_root=True,save_root=self.live,
                       game_factory=Game,others=lambda pid:True,clear=lambda:True,sleep=advance)
        self.assertEqual(s.inventory(self.live),self.a)
    def test_helper_real_crash_does_not_touch_root(self):
        class Game:
            def __init__(self,*args):pass
            def exited(self):return 5
            def close(self):pass
        with self.assertRaises(ValueError):
            after_exit.run(self.root,123,456,'A','real-crash','switch',real_root=True,save_root=self.live,
                           game_factory=Game,others=lambda pid:True,clear=lambda:True)
        self.assertEqual(s.inventory(self.live),self.before)

if __name__=='__main__':unittest.main(verbosity=2)
