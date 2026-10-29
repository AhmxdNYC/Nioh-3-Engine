"""Offline tests of tools/iframe_probe_summary.py (log parse + window logic). py -I tools/test_iframe_probe_summary.py"""
import importlib.util
import os
import unittest

_p = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'iframe_probe_summary.py')
_s = importlib.util.spec_from_file_location('iframe_probe_summary', _p)
S = importlib.util.module_from_spec(_s)
_s.loader.exec_module(S)


def pr(t, act, f, mask):
    return '[22:00:00.000] IFRAMEPR t %.1f act %d f %d m %05X x | a8=0\n' % (t, act, f, mask)


class Tests(unittest.TestCase):
    def test_pr_runs_and_window(self):
        c0 = 1 << S.BITS.index('cannot0')
        lines = []
        for run in range(3):  # 3 dodges of 36 frames, cannot0 on frames 4..17
            for f in range(36):
                lines.append(pr(1000 * run + f * 16.7, 10015, f, c0 if 4 <= f <= 17 else 0))
        lines.append('[x] IFRAMEPR t 5000.0 act 10015 f 0 m 00000 -\n')  # a 4th run that never lights
        runs = S.parse_pr(lines)
        self.assertEqual(len(runs), 4)
        self.assertEqual(S.window(runs[0]['frames'], c0), (4, 17))
        self.assertIsNone(S.window(runs[3]['frames'], c0))
        res = S.summarize(lines)
        self.assertEqual(res[0]['act'], 10015)
        self.assertEqual(res[0]['stripe'], (3, 4, 4, 17, 17))
        self.assertEqual(res[0]['bits']['cannot0'][:3], (3, 4, 17))
        self.assertIn('NO-DAMAGE WINDOW (stripe bits): 3/4 runs, frames 4-17', S.render(res))

    def test_non_stripe_bit_is_reported_but_not_a_window(self):
        s48 = 1 << S.BITS.index('s48')
        lines = [pr(f, 10024, f, s48) for f in range(10)]
        res = S.summarize(lines)
        self.assertIsNone(res[0]['stripe'])
        self.assertIn('s48', res[0]['bits'])
        self.assertIn('likely not a window', S.render(res))

    def test_wn_candidates_against_control(self):
        lines = []
        for k in range(5):  # dodge: c+EA0 windows 4-14 in all 5 runs, s+48 windows in 2 (below the share), a+100 also windows in idle
            ent = ' c EA0=0>1@4-14'.replace(' c ', ' c') + (' s48=0>1@3-9' if k < 2 else '') + ' a100=0>5@2-6'
            lines.append('IFRAMEWN t %d.0 act 10015 frames 36 noisy 10/2/3:%s\n' % (k, ent))
        for k in range(4):  # idle 2000 (looping in the table): a+100 windows there too
            lines.append('IFRAMEWN t %d.0 act 2000 frames 200 noisy 10/2/3: a100=0>5@20-40\n' % (100 + k))
        ev = {2000: {'loops': '1', 'events': '', 'end_frame': '', 'next': '2000'},
              10015: {'loops': '0', 'events': '10072@0+ 41@24+', 'end_frame': '36', 'next': '0'}}
        wn = S.parse_wn(lines)
        self.assertEqual(wn[0]['fields']['c+EA0=0>1'], ('w', 4, 14))
        c = S.candidates(wn, 10015, {2000})
        self.assertEqual([x['field'] for x in c], ['c+EA0'])
        self.assertEqual((c[0]['f1'], c[0]['f2'], c[0]['n'], c[0]['kind']), (4, 14, 5, 'w'))
        res = S.summarize(lines, ev)
        self.assertEqual([r['act'] for r in res], [10015])
        out = S.render(res)
        self.assertIn('events 10072@0+ 41@24+ | end frame 36', out)
        self.assertIn('scan c+EA0', out)

    def test_open_entry(self):
        wn = S.parse_wn(['IFRAMEWN t 1.0 act 7 frames 20 noisy 0/0/0: s2DC8=0>80000000@5-\n'])
        self.assertEqual(wn[0]['fields']['s+2DC8=0>80000000'], ('o', 5, None))


if __name__ == '__main__':
    unittest.main()
