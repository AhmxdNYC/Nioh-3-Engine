import tempfile, unittest
from pathlib import Path
from unittest.mock import patch
import bootstrap as b
import saveslot as s
ACCOUNT='76561190000000000'
def validate(blob, account):
    if not blob.startswith(b'valid-') or account!=ACCOUNT: raise ValueError('bad checksum')
    return {'savedAt':int(blob.split(b'-')[1])}
def publish(stage,target):
    if target.exists(): raise FileExistsError(target)
    stage.rename(target)
class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.base=Path(self.tmp.name);self.source=self.base/'source';self.lib=self.base/'library'
        self.profile('00',b'valid-10')
        p=self.source/'Savedata'/ACCOUNT/'SYSTEMSAVEDATA00';p.mkdir();(p/'SAVEDATA.BIN').write_bytes(b'system')
        (self.source/'settings.ini').write_text('original')
    def profile(self,n,blob):
        p=self.source/'Savedata'/ACCOUNT/('SAVEDATA'+n);p.mkdir(parents=True)
        (p/'SAVEDATA.BIN').write_bytes(blob);(p/'BACKUP.BIN').write_bytes(b'old')
    def runit(self,validator=validate,publisher=publish): return b.run(self.lib,self.source,validator=validator,publisher=publisher)
    def test_fresh_full_backup_source_unchanged(self):
        before=s.inventory(self.source);r=self.runit();self.assertEqual(r['slots'],['My current save'])
        self.assertEqual(s.inventory(self.source),before);self.assertEqual(s.inventory(self.lib/'backups/initial'),before)
        self.assertEqual(len(s.Slots(self.lib).verify('My current save')['tree']['files']),3)
    def test_multi_profile_unambiguous_newest(self):
        self.profile('03',b'valid-30');r=self.runit();self.assertEqual(r['slots'],['My save 00','My save 03'])
        for name in r['slots']:
            s.Slots(self.lib).verify(name)
            self.assertEqual([p.name for p in (self.lib/'slots'/name/'data/Savedata'/ACCOUNT).glob('SAVEDATA*')],['SAVEDATA00'])
        self.assertEqual((self.lib/'current/Savedata'/ACCOUNT/'SAVEDATA00/SAVEDATA.BIN').read_bytes(),b'valid-30')
    def test_existing_valid_unchanged(self):
        self.runit();before=s.inventory(self.lib);(self.source/'settings.ini').write_text('new')
        self.assertFalse(self.runit()['changed']);self.assertEqual(s.inventory(self.lib),before)
    def test_existing_malformed_unchanged(self):
        self.lib.mkdir();(self.lib/'keep').write_text('mine');before=s.inventory(self.lib)
        with self.assertRaises(Exception):self.runit()
        self.assertEqual(s.inventory(self.lib),before)
    def test_corrupt_source_no_publish(self):
        (self.source/'Savedata'/ACCOUNT/'SAVEDATA00/SAVEDATA.BIN').write_bytes(b'bad');before=s.inventory(self.source)
        with self.assertRaises(ValueError):self.runit()
        self.assertFalse(self.lib.exists());self.assertEqual(s.inventory(self.source),before)
    def test_source_race_no_publish(self):
        def mutate(blob,account):
            (self.source/'settings.ini').write_text('game write');return validate(blob,account)
        with self.assertRaisesRegex(ValueError,'changed'):self.runit(validator=mutate)
        self.assertFalse(self.lib.exists())
    def test_destination_race_no_overwrite(self):
        def race(stage,target):
            target.mkdir();(target/'keep').write_text('other');publish(stage,target)
        with self.assertRaises(FileExistsError):self.runit(publisher=race)
        self.assertEqual((self.lib/'keep').read_text(),'other');self.assertFalse((self.lib/'sandbox.json').exists())
    def test_nested_root_refused(self):
        with self.assertRaises(ValueError):b.run(self.source/'library',self.source,validator=validate,publisher=publish)
    def test_backup_mutation_no_publish(self):
        original=s.copy_verified
        def corrupt(src,dst):
            result=original(src,dst)
            if dst.name=='initial':(dst/'settings.ini').write_text('bad copy')
            return result
        with patch.object(s,'copy_verified',side_effect=corrupt):
            with self.assertRaisesRegex(ValueError,'backup changed'):self.runit()
        self.assertFalse(self.lib.exists())
if __name__=='__main__':unittest.main()
