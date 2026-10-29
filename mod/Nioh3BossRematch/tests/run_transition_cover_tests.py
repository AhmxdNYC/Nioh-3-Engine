"""Actual transition policy/readers and presentation-ticket tests, offline only."""
from pathlib import Path
import subprocess,tempfile,re
ROOT=Path(__file__).resolve().parents[3]
SETUP=Path(r'C:/nioh3-engine/tools/msvc/setup_x64.bat')
with tempfile.TemporaryDirectory(prefix='nioh3-cover-') as temp:
    out=Path(temp)
    for src in (ROOT/'mod/Nioh3BossRematch/tests/transition_cover_dojo_test.cpp',ROOT/'mod/Nioh3BossRematch/tests/transition_cover_branch_test.cpp',ROOT/'mod/Nioh3BossRematch/tests/transition_cover_test.cpp',ROOT/'mod/Nioh3BossRematch/tests/transition_native_read_test.cpp',ROOT/'mod/Nioh3EngineFix/tests/cover_present_test.cpp',ROOT/'mod/Nioh3EngineFix/tests/autocont_thunk_test.cpp'):
        exe=out/(src.stem+'.exe')
        cmd=f'call "{SETUP}" >nul && cl /nologo /W4 /WX /EHsc /std:c++17 "{src}" /Fo"{out / (src.stem+".obj")}" /Fe"{exe}"'
        r=subprocess.run('cmd /d /s /c "'+cmd+'"',capture_output=True,text=True,timeout=60)
        assert r.returncode==0,r.stdout+r.stderr
        r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
        print(r.stdout.strip());assert r.returncode==0,r.stdout+r.stderr

# Validate the additional passive-reader contracts against the same private v2.02 dump.
dump=ROOT/'re/Nioh3.dump.exe'
if not dump.exists(): dump=Path(r'C:/nioh3-engine/re/Nioh3.dump.exe')
blob=dump.read_bytes()
source=(ROOT/'mod/Nioh3BossRematch/src/rematch.cpp').read_text()
section=source[source.index('bool CoverAnchors()'):source.index('struct CoverLock')]
anchors=re.findall(r'\{0x([0-9A-F]+),"((?:\\x[0-9A-F]{2})+)",(\d+)\}',section)
assert len(anchors)>=20
for rva,encoded,size in anchors:
    expected=bytes.fromhex(encoded.replace('\\x',''))
    assert len(expected)==int(size)
    address=int(rva,16)
    assert blob[address:address+len(expected)]==expected, 'native anchor '+rva
print('PASS',len(anchors),'exact native transition anchors')
