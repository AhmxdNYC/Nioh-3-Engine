"""Reproduce the current spawn ownership defect using extracted production C++.

Diagnostic only: successful reproduction is NOT a restoration-invariant PASS.
No game process, native addresses or save files are accessed.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

p=argparse.ArgumentParser()
p.add_argument('source',type=Path)
a=p.parse_args()
text=a.source.read_text()
def function(signature):
    start=text.index(signature); pos=text.index('{',start); depth=1; end=pos+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}'); end+=1
    return text[start:end]
spawn=text[text.index('struct Spawn {'):text.index('} g_sp;')+7]
code=r'''
#include <cstdint>
#include <cstring>
#include <cstdio>
using ULONGLONG=unsigned long long;
unsigned char record[128]{};
bool readable=true; int failWrite=0,writes=0;
uint64_t CallWpFind(uint32_t) { return reinterpret_cast<uint64_t>(record); }
bool Copy(void* out,uint64_t address,size_t n) {
    if(!readable) return false;
    memcpy(out,reinterpret_cast<void*>(address),n); return true;
}
template<class T> bool Rd(uint64_t address,T& out) { return Copy(&out,address,sizeof(T)); }
bool WrBytes(uint64_t address,const void* value,size_t n) {
    if(++writes==failWrite) return false;
    memcpy(reinterpret_cast<void*>(address),value,n); return true;
}
void Log(const char*,...) {}
@SPAWN@
@WRITE@
@RESTORE@
int main() {
    for(int scenario=0;scenario<4;++scenario) {
        g_sp=Spawn{}; g_sp.st=2; g_sp.rec=reinterpret_cast<uint64_t>(record);
        g_sp.point=123; g_sp.landCheck=true;
        for(int i=0;i<4;++i) { g_sp.orig[i]=float(i); g_sp.mine[i]=float(i+10); }
        g_sp.origFlags=17; g_sp.mineFlags=33;
        memcpy(record+0x40,g_sp.mine,16); memcpy(record+8,&g_sp.mineFlags,4);
        readable=scenario!=3; failWrite=scenario; writes=0;
        SpawnRestore("offline fault injection");
        const bool restored=!memcmp(record+0x40,g_sp.orig,16) && !memcmp(record+8,&g_sp.origFlags,4);
        const bool lost=g_sp.st==0 && g_sp.rec==0;
        if(scenario==0 ? !(restored && lost) : !(!restored && lost)) return 1;
        printf("%s: writes=%d restored=%d ownership_cleared=%d\n",
            scenario==0?"control":scenario==1?"first write fails":scenario==2?"second write fails":"read fails",
            writes,int(restored),int(lost));
    }
    puts("REPRODUCED: 3 unsafe ownership-loss paths; this is not a safety PASS.");
}
'''.replace('@SPAWN@',spawn).replace('@WRITE@',function('bool SpawnWrite(')).replace('@RESTORE@',function('void SpawnRestore('))
setup=Path(__file__).resolve().parents[1]/'msvc/setup_x64.bat'
with tempfile.TemporaryDirectory(prefix='nioh3-spawn-repro-') as tmp:
    directory=Path(tmp); source=directory/'repro.cpp'; exe=directory/'repro.exe'
    source.write_text(code)
    command=f'call "{setup}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /Fo"{directory / "repro.obj"}" /Fe"{exe}" "{source}"'
    result=subprocess.run('cmd /d /s /c "'+command+'"',capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    print(result.stdout.strip()); assert result.returncode==0,result.stderr
