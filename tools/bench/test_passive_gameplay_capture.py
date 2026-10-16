import importlib.util
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
import zipfile

spec=importlib.util.spec_from_file_location('collector',Path(__file__).with_name('passive_gameplay_capture.py'))
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
HEADER=b'fps,frametime,gpu_load,elapsed\n'
class Tests(unittest.TestCase):
    def test_real_rows_only(self):
        r=c.summarize(HEADER,b'100,10,99,1000000000\n50,20,1,2000000000\n1,nan,50,3000000000\n1,4,50,1000000000\n')
        self.assertEqual(r['frametime_rows'],2);self.assertEqual(r['invalid_rows'],2)
        self.assertEqual(r['p99_sample_frametime_ms'],20);self.assertEqual(r['p95_sample_frametime_ms'],20);self.assertIsNone(r['gpu_busy_ms'])
        self.assertNotIn('p99_sample_frametime_ms',c.summarize(HEADER,b''))
    def test_offsets_partial_schema_and_rotation(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'m.csv';p.write_bytes(b'host\ninfo\n'+HEADER+b'1,100,1,0\npartial')
            t=c.Tail(p,'mangohud');self.assertTrue(t.partial_initial)
            with p.open('ab') as f:f.write(b' ending\n100,10,20,100\n50,20')
            self.assertEqual(t.poll(),b'100,10,20,100\n')
            self.assertEqual(t.pending,b'50,20');self.assertIn(HEADER,t.prefix)
            t.close()
            t=c.Tail(p,'engine');t.f.close();q=p.with_suffix('.old');p.rename(q);p.write_bytes(b'new\n');t.f=q.open('rb')
            with self.assertRaisesRegex(RuntimeError,'rotated'):t.poll()
            t.close()
    def test_truncate_regrow_and_limits(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'e.log';p.write_bytes(b'old\n');t=c.Tail(p,'engine');p.write_bytes(b'new\nlonger\n')
            with self.assertRaisesRegex(RuntimeError,'prefix_replaced'):t.poll()
            t.close();t=c.Tail(p,'engine');p.write_bytes(b'')
            with self.assertRaisesRegex(RuntimeError,'truncated'):t.poll()
            t.close();p.write_bytes(b'');t=c.Tail(p,'engine');t.total=c.MAX_BYTES
            p.write_bytes(b'x\n')
            with self.assertRaisesRegex(RuntimeError,'byte_limit'):t.poll()
            t.close()
    def run_fixture(self,replace=False):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'m.csv';p.write_bytes(b'host\ninfo\n'+HEADER+b'1,999,1,0\n')
            ini=Path(d)/'test.ini';ini.write_text('private-value-not-to-copy')
            args=SimpleNamespace(pid=123,start_ticks=45,exe='/wine',seconds=.3,engine_log=None,mangohud=str(p),output=str(Path(d)/'out'),hash_file=[str(ini)])
            now=[0.0]
            def ident(pid):return {'pid':pid,'start_ticks':46 if replace and now[0]>.1 else 45,'exe':'/wine','boot_id':'test'}
            def sleep(n):
                now[0]+=n
                with p.open('ab') as f:f.write(f'100,10,50,{int(now[0]*1e9)}\n'.encode())
            out,r=c.collect(args,ident,lambda:now[0],sleep)
            self.assertEqual(r['status'],'CAPTURE_FAILED' if replace else 'CAPTURE_COMPLETE')
            self.assertEqual(r['timing_samples_eligible'], not replace)
            self.assertEqual(r['operator_context']['phase'], 'unknown')
            self.assertEqual(ini.read_text(),'private-value-not-to-copy')
            raw=(out/'mangohud-appended.log').read_bytes();self.assertNotIn(b'999',raw)
            with zipfile.ZipFile(out.with_suffix('.zip')) as z:
                self.assertFalse(any(b'private-value-not-to-copy' in z.read(n) for n in z.namelist()))
            self.assertTrue(out.with_suffix('.zip.sha256').exists())
            if replace:self.assertIn('process_replaced',r['failure'])
            else:self.assertEqual(r['mangohud']['p99_sample_frametime_ms'],10)
    def test_complete(self):self.run_fixture()
    def test_pid_replacement(self):self.run_fixture(True)
    def test_identity_and_duration_reject(self):
        args=SimpleNamespace(seconds=61)
        with self.assertRaises(ValueError):c.collect(args)
        args=SimpleNamespace(seconds=1,pid=2,start_ticks=1,exe='wrong',hash_file=[])
        with self.assertRaisesRegex(ValueError,'identity'):c.collect(args,lambda _:dict(start_ticks=1,exe='actual'))
if __name__=='__main__':unittest.main()
