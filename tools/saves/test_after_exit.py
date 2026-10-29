import unittest
from pathlib import Path
import after_exit as h
import test_saveslot as base
import saveslot as s
class FakeGame:
    code=0
    def __init__(self,pid,created):pass
    def exited(self):return self.code
    def close(self):pass
class HelperTests(base.Tests):
    def invoke(self,**kw):return h.run(self.root,123,456,'A','test','switch',game_factory=FakeGame,others=lambda pid:True,clear=lambda:True,**kw)
    def test_helper_clean_exit(self):
        FakeGame.code=0;self.invoke();self.assertEqual(s.inventory(self.root/'current'),self.a)
        with self.assertRaises(FileExistsError):self.invoke()
    def test_helper_crash(self):
        FakeGame.code=0xc0000005
        with self.assertRaises(ValueError):self.invoke()
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_helper_cancel(self):
        FakeGame.code=0;(self.root/'cancel-test').touch()
        with self.assertRaises(ValueError):self.invoke()
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_helper_second_process(self):
        FakeGame.code=0
        with self.assertRaises(ValueError):h.run(self.root,123,456,'A','test','switch',game_factory=FakeGame,others=lambda pid:True,clear=lambda:False)
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_helper_waits_past_old_timeout(self):
        FakeGame.code=None;t=iter([0,121])
        def advance(_):FakeGame.code=0
        self.invoke(clock=lambda:next(t),sleep=advance)
        self.assertEqual(s.inventory(self.root/'current'),self.a)
    def test_helper_different_pid_while_alive(self):
        FakeGame.code=None
        with self.assertRaises(ValueError):
            h.run(self.root,123,456,'A','different-pid','switch',game_factory=FakeGame,
                  clear=lambda:True,others=lambda pid:False)
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_helper_no_write_while_alive(self):
        FakeGame.code=None
        def advance(_):
            self.assertEqual(s.inventory(self.root/'current'),self.b)
            FakeGame.code=0
        self.invoke(sleep=advance);self.assertEqual(s.inventory(self.root/'current'),self.a)
    def test_one_activation_quit_switch_relaunch_order(self):
        FakeGame.code=None;events=[]
        def quit(game,pid):
            self.assertEqual(pid,123);self.assertEqual(s.inventory(self.root/'current'),self.b)
            events.append('quit-request');FakeGame.code=0
        def launch():
            self.assertEqual(s.inventory(self.root/'current'),self.a)
            self.assertTrue(any((self.root/'transactions').glob('*/backup.json')))
            events.append('launch')
        result=self.invoke(relaunch=True,quit_game=True,quitter=quit,launcher=launch)
        self.assertEqual(events,['quit-request','launch']);self.assertEqual(result['relaunch'],'requested')
    def test_quit_failure_preserves_save(self):
        FakeGame.code=None
        def fail(*args):raise OSError('window ambiguous')
        with self.assertRaises(OSError):self.invoke(relaunch=True,quit_game=True,quitter=fail,launcher=lambda:self.fail('must not launch'))
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_no_relaunch_after_abnormal_exit(self):
        FakeGame.code=5
        with self.assertRaises(ValueError):self.invoke(relaunch=True,launcher=lambda:self.fail('must not launch'))
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_launch_failure_reports_completed_switch(self):
        FakeGame.code=0
        def fail():raise OSError('Steam unavailable')
        result=self.invoke(relaunch=True,launcher=fail)
        self.assertEqual(result['relaunch'],'failed');self.assertEqual(s.inventory(self.root/'current'),self.a)
        receipt=s.read(self.root/'request-test/receipt.json')
        self.assertEqual(receipt['state'],'complete');self.assertIn('Steam unavailable',receipt['launch_error'])
    def test_cancel_prevents_automatic_quit(self):
        FakeGame.code=None;(self.root/'cancel-test').touch()
        with self.assertRaises(ValueError):self.invoke(relaunch=True,quit_game=True,quitter=lambda *a:self.fail('must not quit'))
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_failed_switch_never_relaunches(self):
        FakeGame.code=0
        from unittest.mock import patch
        with patch.object(s.Slots,'switch',side_effect=OSError('backup failure')):
            with self.assertRaises(OSError):self.invoke(relaunch=True,launcher=lambda:self.fail('must not launch'))
        self.assertEqual(s.inventory(self.root/'current'),self.b)
if __name__=='__main__':unittest.main(verbosity=2)
