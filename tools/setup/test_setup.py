"""Offline tests for the Linux loader switch: py -I tools/setup/test_setup.py."""
from pathlib import Path
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parents[1] / "release"), str(HERE.parent / "updater")]
import setup  # noqa: E402

KEY = "[Software\\\\Wine\\\\DllOverrides]"  # user.reg writes key names with doubled backslashes


class LoaderSwitch(unittest.TestCase):
    def run_on(self, body):
        d = Path(tempfile.mkdtemp())
        (d / "user.reg").write_text(body, encoding="utf-8")
        self.assertTrue(setup.enable_loader(d))
        self.assertTrue(setup.enable_loader(d))  # idempotent
        return (d / "user.reg").read_text(encoding="utf-8")

    def test_existing_section(self):
        t = self.run_on(f'WINE REGISTRY Version 2\n\n{KEY} 1700000000\n#time=1d\n"d3d11"="native"\n\n[Other] 1\n"x"="y"\n')
        self.assertEqual(t.count('"dinput8"="native,builtin"'), 1)
        self.assertEqual(t.count("DllOverrides"), 1)
        self.assertIn(f'{KEY} 1700000000\n#time=1d\n"dinput8"="native,builtin"\n"d3d11"', t)

    def test_missing_section(self):
        t = self.run_on('WINE REGISTRY Version 2\n\n[Other] 1\n"x"="y"\n')
        self.assertIn(f'{KEY}\n"dinput8"="native,builtin"', t)

    def test_builtin_replaced(self):
        t = self.run_on(f'WINE REGISTRY Version 2\n\n{KEY} 1\n"dinput8"="builtin"\n')
        self.assertEqual(t.count('"dinput8"'), 1)
        self.assertIn('"dinput8"="native,builtin"', t)

    def test_no_prefix(self):
        self.assertFalse(setup.enable_loader(Path(tempfile.mkdtemp()) / "missing"))


if __name__ == "__main__":
    unittest.main()
