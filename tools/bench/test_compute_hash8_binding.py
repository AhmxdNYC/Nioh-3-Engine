"""Tiny sparse fixtures for exact native binding; never modifies the dump."""
import pathlib,subprocess,sys,tempfile
audit=pathlib.Path(__file__).with_name('audit_compute_hash8.py')
source=pathlib.Path(sys.argv[1])
regions=((0x1199a4,164),(0x58be90,365),(0x58c000,216),(0x2efe1cc,220))
with source.open('rb') as f:
    originals=[]
    for at,size in regions:
        f.seek(at);originals.append((at,f.read(size)))
with tempfile.TemporaryDirectory(prefix='nioh3-hash8-bind-') as directory:
    fixture=pathlib.Path(directory)/'sparse.bin'
    def run(mutation=None):
        with fixture.open('wb') as f:
            for at,body in originals:f.seek(at);f.write(body)
            if mutation is not None:
                f.seek(mutation);f.write(b'\x90')
        return subprocess.run([sys.executable,'-I','-B',str(audit),str(fixture)],capture_output=True,timeout=10).returncode
    assert run()==0,'bound exact slices rejected'
    for at in (0x1199a7,0x1199fc,0x58bf35,0x58c01a,0x2efe209,0x58bf2f):
        assert run(at)!=0,('mutated binding accepted',hex(at))
print('PASS exact native slices and six mode/CRC/pointer/length binding negatives')
