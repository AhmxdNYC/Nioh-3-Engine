"""Offline regression: exact-archive observations must not bleed into other data."""
import os
import tempfile
import unittest
from pathlib import Path
import gen_boss_move_facts as g

class HitCountTests(unittest.TestCase):
    def test_observations_and_controls(self):
        path = g.data_file(g.BOSSES[16102])
        self.assertIsNotNone(path)
        facts = {x['action']: x for x in g.ymf.facts(path)}
        for action in ('0x4E4E', '0x4E55'):
            self.assertEqual(facts[action]['hits'], 2)
            self.assertEqual(facts[action]['static_hits'], 1)
            self.assertEqual(facts[action]['hit_source'], 'player observation #40826')
        self.assertEqual(facts['0x4E52']['hits'], 2)
        self.assertEqual(facts['0x4E4F']['hits'], '')
        self.assertEqual(facts['0x4E56']['hits'], '')
        # Extra byte changes identity without changing parsed action tables.
        with tempfile.TemporaryDirectory() as tmp:
            other = Path(tmp) / 'different-archive'
            other.write_bytes(Path(path).read_bytes() + b'\0')
            changed = {x['action']: x for x in g.ymf.facts(other)}
            self.assertEqual(changed['0x4E4E']['hits'], 1)
            self.assertEqual(changed['0x4E55']['hits'], 1)
    def test_generated_scope(self):
        text = g.generate()
        self.assertIsNotNone(text)
        self.assertIn('{16102u, 0x4E4Eu, 2, 0, 1}', text)
        self.assertIn('{16102u, 0x4E55u, 2, 0, 1}', text)
        self.assertEqual(Path(g.OUT).read_text(), text)

if __name__ == '__main__':
    unittest.main()
