"""Cross-language recovery checks; run after dxgidiag_harness emits a fixture."""
import importlib.util
from pathlib import Path
import unittest
import tempfile

spec = importlib.util.spec_from_file_location('dxgidiag_report', Path(__file__).parents[1] / 'tools/dxgidiag_report.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class Recovery(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = (Path(tempfile.gettempdir()) / 'dxgidiag-fixture.bin').read_bytes()

    def test_report_and_failure_survive_wrap(self):
        report, events = m.recover(self.data)
        self.assertEqual(len(report.splitlines()), 15)
        self.assertEqual(len(events), 256)
        self.assertIn('hr=887A0001', report)
        self.assertEqual(events, sorted(events, key=lambda e: e['seq']))

    def test_torn_record_ignored(self):
        data = bytearray(self.data)
        offset = m.HEADER.size + 4096 + 68 * m.RECORD.size
        data[offset + 20] ^= 0x80
        _, events = m.recover(bytes(data))
        self.assertEqual(len(events), 255)

    def test_truncated_or_unknown_rejected(self):
        for data in [self.data[:-1], b'BADMAGIC' + self.data[8:]]:
            with self.assertRaises(ValueError):
                m.recover(data)

if __name__ == '__main__':
    unittest.main()
