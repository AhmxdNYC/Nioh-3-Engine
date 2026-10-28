"""Synthetic offline tests: no installed saves or running process access."""
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import saveslot as s
import live_title as live
import title_gate
from test_saveslot import fixture, ACCOUNT

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'re/scripts_re'))


class FakeGate:
    def __init__(self): self.calls = 0; self.fail = None
    def settle(self): self.check()
    def check(self):
        self.calls += 1
        if self.calls == self.fail: raise ValueError('Gate lost')


class ImportTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.base = Path(self.tmp.name)
        self.ws = self.base/'library'; s.init(self.ws); fixture(self.ws/'current')
        self.manager = s.Slots(self.ws); self.manager.snapshot('A')
        self.root = self.base/'copied-game-root'; fixture(self.root, b'B')
        backup = self.root/'Savedata_backup'/ACCOUNT/'SAVEDATA00'; backup.mkdir(parents=True)
        (backup/'BACKUP.BIN').write_bytes(b'old backup')
        self.before = s.inventory(self.root); self.gate = FakeGate()
    def tearDown(self): self.tmp.cleanup()
    def run_import(self, **kw):
        return live.run(self.ws, self.root, 'A', 'test', self.gate,
                        validator=lambda b,a: {'sha256':hashlib.sha256(b).hexdigest()}, **kw)
    def unchanged(self): self.assertEqual(s.inventory(self.root), self.before)
    def test_atomic_import_preserves_existing_and_system(self):
        r = self.run_import()
        self.assertEqual(r['state'], 'imported'); self.assertFalse(r['loaded'])
        self.assertEqual(Path(r['target']).name, 'SAVEDATA01')
        for path, info in self.before['files'].items(): self.assertEqual(s.inventory(self.root)['files'][path], info)
        self.assertEqual(s.inventory(Path(r['backup'])), self.before)
        for name in ('SAVEDATA.BIN','BACKUP.BIN'): self.assertEqual((Path(r['target'])/name).read_bytes(), b'A'*4096)
        self.assertIn('Load Game', r['instruction'])
    def test_gate_loss_at_every_prepublication_boundary(self):
        for fail in range(1, 6):
            with self.subTest(fail=fail):
                self.gate = FakeGate(); self.gate.fail = fail
                with self.assertRaises(ValueError):
                    live.run(self.ws,self.root,'A','failure'+str(fail),self.gate,validator=lambda b,a: {})
                self.unchanged()
    def test_gate_loss_after_publish_retains_complete_slot(self):
        self.gate.fail = 6
        r = self.run_import(); self.assertEqual(r['state'], 'imported')
        self.assertIn('changed', r['gate_after']); self.assertTrue(Path(r['target']).is_dir())
    def test_target_race_never_overwrites(self):
        def race(stage, target):
            target.mkdir(); (target/'raced').write_bytes(b'other owner'); live.publish(stage,target)
        with self.assertRaises(OSError): self.run_import(publisher=race)
        self.assertEqual((self.root/'Savedata'/ACCOUNT/'SAVEDATA01/raced').read_bytes(),b'other owner')
        self.assertFalse((self.root/'Savedata'/ACCOUNT/'SAVEDATA01/SAVEDATA.BIN').exists())
    def test_empty_target_race_never_replaced(self):
        def race(stage, target): target.mkdir(); live.publish(stage,target)
        with self.assertRaises(OSError): self.run_import(publisher=race)
        self.assertEqual(list((self.root/'Savedata'/ACCOUNT/'SAVEDATA01').iterdir()), [])
    def test_no_space_backup(self):
        with patch.object(s,'copy_verified',side_effect=OSError('disk full')):
            with self.assertRaises(OSError): self.run_import()
        self.unchanged()
    def test_cancel(self):
        (self.ws/'cancel-test').touch()
        with self.assertRaises(ValueError): self.run_import()
        self.unchanged()
    def test_nonce_replay(self):
        self.run_import(); before = s.inventory(self.root)
        with self.assertRaises(FileExistsError): self.run_import()
        self.assertEqual(s.inventory(self.root),before)
    def test_account_mismatch(self):
        (self.root/'Savedata'/ACCOUNT).rename(self.root/'Savedata/76561198000000001')
        self.before=s.inventory(self.root)
        with self.assertRaises(ValueError): self.run_import()
        self.unchanged()
    def test_all_slots_full(self):
        for i in range(1,50): (self.root/'Savedata'/ACCOUNT/f'SAVEDATA{i:02d}').mkdir()
        self.before=s.inventory(self.root)
        with self.assertRaises(ValueError): self.run_import()
        self.unchanged()
    def test_multi_profile_needs_explicit_source(self):
        src=self.ws/'current/Savedata'/ACCOUNT/'SAVEDATA01'; src.mkdir();(src/'SAVEDATA.BIN').write_bytes(b'C')
        self.manager.snapshot('Multi')
        with self.assertRaises(ValueError): live.run(self.ws,self.root,'Multi','multi',self.gate,validator=lambda b,a:{})
        self.unchanged()
        r=live.run(self.ws,self.root,'Multi','explicit',self.gate,source_slot='SAVEDATA01',validator=lambda b,a:{})
        self.assertEqual((Path(r['target'])/'SAVEDATA.BIN').read_bytes(),b'C')
    def test_invalid_source_path(self):
        with self.assertRaises(ValueError): self.run_import(source_slot='../SYSTEMSAVEDATA00')
        self.unchanged()
    def test_validator_failure(self):
        with self.assertRaises(ValueError): live.run(self.ws,self.root,'A','invalid',self.gate)
        self.unchanged()
    def test_pending_transaction(self):
        s.write(self.ws/'pending.json',{})
        with self.assertRaises(ValueError): self.run_import()
        self.unchanged()
    def test_backup_and_stage_preserved_on_publish_failure(self):
        def fail(*args): raise OSError('sharing violation')
        with self.assertRaises(OSError): self.run_import(publisher=fail)
        self.unchanged(); r=s.read(self.ws/'request-test/receipt.json')
        self.assertEqual(r['state'],'refused'); self.assertEqual(s.inventory(Path(r['backup'])),self.before)
        self.assertTrue(Path(r['stage']).exists())


class GateTests(unittest.TestCase):
    def setUp(self):
        self.base=0x10000000; self.mem={}
        def put(addr,value,fmt='Q'): self.mem[addr]=struct.pack('<'+fmt,value)
        self.put=put; b=self.base
        for a,v in [(b+0x4709B50,0x1000),(0x1000,0x2000),(0x1008,0x2008),(0x2000,0x3000),
                    (0x3008,b+0x44C91D0),(0x1018,0),(0x1020,0),
                    (0x3030,100),(0x3038,200),(0x3040,300),(0x3058,1),
                    (b+0x4759910,0x4000),(b+0x45BBF80,0x5000),(0x50C0,0),
                    (b+0x45C8480,0x6000),(0x6000,0x7000),(b+0x45C6EC0,0)]: put(a,v)
        put(0x3010,3,'I');put(0x458A,0,'B');put(0x6010,1,'I');put(0x7038,0,'B')
    def read(self,a,n):
        data=self.mem[a]
        if len(data)!=n: raise OSError('short read')
        return data
    def test_idle(self): self.assertTrue(title_gate.sample(self.read,self.base))
    def test_all_predicate_failures(self):
        for a,v,fmt in [(0x3010,2,'I'),(0x3010,8,'I'),(0x6010,0,'I'),(0x6010,99,'I'),
                        (0x7038,1,'B'),(self.base+0x45C6EC0,123,'Q'),(0x458A,1,'B'),
                        (0x50C0,123,'Q'),(0x1020,123,'Q'),(0x3008,123,'Q'),(0x6000,0,'Q')]:
            with self.subTest(address=a,value=v):
                old=self.mem[a];self.put(a,v,fmt)
                with self.assertRaises(ValueError):title_gate.sample(self.read,self.base)
                self.mem[a]=old
    def test_any_unreadable_field_refuses(self):
        for a in list(self.mem):
            old=self.mem.pop(a)
            with self.assertRaises(KeyError):title_gate.sample(self.read,self.base)
            self.mem[a]=old
    def test_press_any(self):
        for a in (0x3030,0x3038,0x3040):self.put(a,0x3030)
        self.put(0x3058,0)
        with self.assertRaises(ValueError):title_gate.sample(self.read,self.base)


class CryptoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import savecrypt
        p=bytearray(0x900058+0x158);p[:6]=b'RNNUSR'
        struct.pack_into('<Q',p,0x10,int(ACCOUNT));struct.pack_into('<I',p,0x1C,0x900058)
        struct.pack_into('<I',p,0x158,0x25111400)
        struct.pack_into('<I',p,0x900194,savecrypt.checksum(p))
        cls.blob=savecrypt.encrypt(p)
    def test_valid_roundtrip(self):self.assertEqual(live.validate(self.blob,ACCOUNT)['sha256'],hashlib.sha256(self.blob).hexdigest())
    def test_header_rejections(self):
        import savecrypt
        for offset,value in [(0,b'RNNSYS'),(0x1C,struct.pack('<I',12)),(0x10,struct.pack('<Q',1))]:
            b=bytearray(self.blob);header=bytearray(savecrypt.xor(b[:0x158],savecrypt.hdr_stream()))
            header[offset:offset+len(value)]=value;b[:0x158]=savecrypt.xor(header,savecrypt.hdr_stream())
            with self.assertRaises(ValueError):live.validate(b,ACCOUNT)
    def test_checksum_corruption(self):
        b=bytearray(self.blob);b[0x300]^=1
        with self.assertRaisesRegex(ValueError,'checksum'):live.validate(b,ACCOUNT)


if __name__=='__main__':unittest.main(verbosity=2)
