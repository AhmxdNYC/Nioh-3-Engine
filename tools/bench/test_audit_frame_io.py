import tempfile
import unittest
from pathlib import Path
from audit_frame_io import profile


class ProfileTests(unittest.TestCase):
    def parse(self, rows):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'profile.tsv'
            path.write_text('tid\tthread\tview\tcount\tpct\tbusy\tfunction\n'+rows)
            return profile(path)

    def test_views_remain_separate(self):
        result = self.parse('1\tworker\tattr\t2\t2.0\t\tNioh3+117D6C\n'
                            '1\tworker\tincl\t2\t2.0\t\tNioh3+117D6C 160\n')
        self.assertEqual(len(result['observations']), 2)
        self.assertIsNone(result['fps_savings'])
        self.assertIsNone(result['paired_flush_frequency'])

    def test_absence_not_zero(self):
        self.assertEqual(self.parse('1\tworker\tattr\t2\t2\t\tNioh3+12345\n')['observations'], [])

    def test_duplicate_rejected(self):
        with self.assertRaises(ValueError):
            self.parse('1\tw\tattr\t2\t2\t\tNioh3+117D6C\n'*2)

    def test_nonfinite_rejected(self):
        with self.assertRaises(ValueError):
            self.parse('1\tw\tattr\t2\tnan\t\tNioh3+117D6C\n')

    def test_negative_rejected(self):
        with self.assertRaises(ValueError):
            self.parse('1\tw\tattr\t-2\t2\t\tNioh3+117D6C\n')


if __name__ == '__main__':
    unittest.main()
