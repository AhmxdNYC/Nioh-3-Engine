"""Offline data-source tests for fallback rosters; no game or manual boss ID map."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('rosters', ROOT/'tools/gen_fallback_rosters.py')
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)


class RosterDataTest(unittest.TestCase):
    def test_checked_in_output(self):
        outputs, rows = g.generate(ROOT)
        for path, content in outputs.items():
            self.assertEqual((ROOT/path).read_text(encoding='utf-8'), content, path)
        catalogue = g.read_csv(ROOT/'mod/Nioh3BossRematch/Nioh3BossRematch_bosses.tsv', '\t')
        self.assertEqual({r['id'] for r in rows}, {r['id'] for r in catalogue if ';' in r['boss']})
        for row in rows:
            if row['errors']:
                self.assertIn('"%s", 0, {0}, {0}' % row['id'], outputs['mod/Nioh3Training/src/fallback_rosters_gen.h'])
                continue
            self.assertGreater(len(row['members']), 1)
            for member in row['members']:
                self.assertTrue(any(int(c['key64']) == member['chr'] and int(c['placement_id']) == member['placement'] for c in member['candidates']))

    def fixture(self):
        catalogue = [{'id':'RTEST','boss':'First;Second','replay_mission_id':'0x123','placement_id':'40'}]
        names = [{'english_name':'First','char_id':'11'}, {'english_name':'Second','char_id':'12'}]
        enemies = [dict(mission_id='0x123',char_id='11',key64='901',placement_id='10',flags9c='0x4'),
                   dict(mission_id='0x123',char_id='12',key64='902',placement_id='40',flags9c='0x4')]
        return catalogue, names, enemies

    def test_joins_spawn_key_not_name_id(self):
        row = g.resolve(*self.fixture())[0]
        self.assertEqual([m['chr'] for m in row['members']], [901, 902])
        self.assertEqual([m['placement'] for m in row['members']], [10, 40])

    def test_ignores_other_mission_and_nonboss_variant(self):
        cat,names,enemies = self.fixture()
        enemies += [dict(mission_id='0x124',char_id='11',key64='999',placement_id='20',flags9c='0x4'),
                    dict(mission_id='0x123',char_id='11',key64='998',placement_id='30',flags9c='0x20')]
        row = g.resolve(cat,names,enemies)[0]
        self.assertFalse(row['errors'])
        self.assertEqual(row['members'][0]['chr'], 901)

    def test_rejects_ambiguous_and_missing_names(self):
        cat,names,enemies = self.fixture()
        enemies.append(dict(mission_id='0x123',char_id='11',key64='998',placement_id='30',flags9c='0x4'))
        self.assertTrue(g.resolve(cat,names,enemies)[0]['errors'])
        self.assertTrue(g.resolve(cat,names[:1],enemies[:2])[0]['errors'])

    def test_equivalent_placements_prefer_catalogue_selection(self):
        cat,names,enemies = self.fixture()
        enemies.append(dict(mission_id='0x123',char_id='12',key64='902',placement_id='20',flags9c='0x4'))
        self.assertEqual(g.resolve(cat,names,enemies)[0]['members'][1]['placement'],40)
        cat[0]['placement_id']='1'
        self.assertEqual(g.resolve(cat,names,enemies)[0]['members'][1]['placement'],20)


if __name__ == '__main__':
    unittest.main()
