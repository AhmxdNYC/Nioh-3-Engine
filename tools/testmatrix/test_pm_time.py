"""Unit test: a synthetic PresentMon CSV shifted -14 h is realigned to the run window."""
import os, sys, tempfile, unittest
from datetime import datetime, timezone, timedelta
sys.path.insert(0, os.path.dirname(__file__))
import pm_time
import pc_gpu_report as R
import cost_vs_looks as C


def make_csv(path, start_utc, n, shift_h):
    with open(path, 'w') as f:
        f.write('Application,CPUStartDateTime,FrameTime,GPUBusy\n')
        for i in range(n):
            t = start_utc + timedelta(milliseconds=28 * i) + timedelta(hours=shift_h)
            f.write('Nioh3.exe,%s,28.0,27.0\n' % t.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3])


class T(unittest.TestCase):
    def test_detect(self):
        t0 = datetime(2026, 10, 9, 16, 34, 0, tzinfo=timezone.utc)
        base = t0.timestamp()
        ts = [base + 0.028 * i - 14 * 3600 for i in range(2000)]
        off = pm_time.detect_offset(ts, [(base + 5, base + 40)], 'synthetic')
        self.assertEqual(off, 14 * 3600)
        self.assertEqual(pm_time.detect_offset([base + 0.028 * i for i in range(2000)], [(base + 5, base + 40)], log=False), 0)

    def test_reports(self):
        t0 = datetime(2026, 10, 9, 16, 34, 0, tzinfo=timezone.utc)
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'x.csv')
            make_csv(p, t0, 3000, -14)
            w = [dict(t0_utc='2026-10-09T16:34:05Z', t1_utc='2026-10-09T16:34:40Z')]
            s = R.pm_stats(p, w)
            self.assertIsNotNone(s)
            self.assertGreater(s['frames'], 1000)
            pm = C.read_pm(p)
            e = C.utc_epoch('2026-10-09T16:34:05Z'); e1 = C.utc_epoch('2026-10-09T16:34:40Z')
            C.align_pm(pm, [(e, e1)])
            self.assertGreater(C.window(pm, e, e1)['frames'], 1000)


if __name__ == '__main__':
    unittest.main()
