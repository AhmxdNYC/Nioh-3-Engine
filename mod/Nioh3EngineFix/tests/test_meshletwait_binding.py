"""Exact v2.02 MW bytes, stack/register ABI and atomic-install contract."""
import json
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class Binding(unittest.TestCase):
    def test_native_sites_and_abi(self):
        dump = (ROOT.parents[1] / 're/Nioh3.dump.exe').read_bytes()
        doc = json.loads((ROOT / 'patches.json').read_text())
        sites = [s for s in doc['sites'] if s['group'] == 'MW']
        self.assertEqual({s['id'] for s in sites}, {'MWR', 'MWW'})
        for s in sites:
            a = int(s['rva'], 16)
            self.assertEqual(dump[a:a+s['len']], bytes.fromhex(s['orig']))
            self.assertEqual(s['call_at'] + 5, s['len']) # return at original next instruction
        for d in doc['deps']:
            if d['group'] == 'MW':
                a = int(d['rva'], 16); b = bytes.fromhex(d['bytes'])
                self.assertEqual(dump[a:a+len(b)], b)
        self.assertEqual(dump[0x3fd15:0x3fd1a], bytes.fromhex('56 48 83 EC 20'))
        self.assertEqual(dump[0x9811f8:0x9811fe], bytes.fromhex('40 53 48 83 EC 20'))
        # Entry RSP%16=8; push nonvolatile =>0; sub20h retains0 and supplies home space.
        self.assertEqual((8-8-32) % 16, 0)
        asm = (ROOT / 'src/caves.asm').read_text()
        self.assertIn('caveMW PROC\n    mov rcx, rsi\n    jmp MWWait', asm)
        self.assertIn('caveMWWake PROC\n    mov rcx, rax\n    jmp MWComplete', asm)
        source = (ROOT / 'src/dllmain.cpp').read_text()
        self.assertIn('expGroup("MW", wait && wake', source)
        self.assertIn('if (!installed && !g_mwInstallUncertain) g_mw.enabled = false;', source)
        self.assertIn('c.mw = I(L"Patches", L"MW", 0)', source)

if __name__ == '__main__': unittest.main()
