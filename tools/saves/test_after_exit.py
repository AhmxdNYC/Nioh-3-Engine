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
    def invoke(self,**kw):return h.run(self.root,123,456,'A','test','switch',game_factory=FakeGame,clear=lambda:True,**kw)
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
        with self.assertRaises(ValueError):h.run(self.root,123,456,'A','test','switch',game_factory=FakeGame,clear=lambda:False)
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_helper_timeout(self):
        FakeGame.code=None;t=iter([0,121])
        with self.assertRaises(ValueError):self.invoke(clock=lambda:next(t))
        self.assertEqual(s.inventory(self.root/'current'),self.b)
    def test_helper_no_write_while_alive(self):
        FakeGame.code=None
        def advance(_):
            self.assertEqual(s.inventory(self.root/'current'),self.b)
            FakeGame.code=0
        self.invoke(sleep=advance);self.assertEqual(s.inventory(self.root/'current'),self.a)
if __name__=='__main__':unittest.main(verbosity=2)
