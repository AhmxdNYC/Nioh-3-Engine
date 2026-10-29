"""Actual-source restoration failure/retry tests; no native game access."""
from pathlib import Path
import argparse
import subprocess
import tempfile

p=argparse.ArgumentParser()
p.add_argument('source',type=Path,nargs='?',default=Path(__file__).resolve().parents[1]/'src/rematch.cpp')
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
bool readable=true; int failWrite=0,writes=0; uint64_t lookup=0; bool lie=false;
uint64_t CallWpFind(uint32_t) { return lookup; }
bool Copy(void* out,uint64_t address,size_t n) {
    if(!readable) return false;
    memcpy(out,reinterpret_cast<void*>(address),n); return true;
}
template<class T> bool Rd(uint64_t address,T& out) { return Copy(&out,address,sizeof(T)); }
bool WrBytes(uint64_t address,const void* value,size_t n) {
    if(++writes==failWrite) return false;
    if(!lie) memcpy(reinterpret_cast<void*>(address),value,n); return true;
}
void Log(const char*,...) {}
@SPAWN@
@WRITE@
@RESTORE@
void SpawnLast(const char*) {}
bool AttemptEffectsAllowed() { return true; }
struct Row { const char* id="test"; float warp[4]{},arena[4]{10,11,12,13}; };
namespace rcore {
bool SpawnRecordMatches(const Row& r,const float* cur) { return !memcmp(r.warp,cur,16); }
uint32_t SpawnFlags(uint32_t) { return 33; }
}
@APPLY@
void seed() {
    g_sp=Spawn{}; g_sp.st=2; g_sp.rec=reinterpret_cast<uint64_t>(record);
    lookup=g_sp.rec; g_sp.point=123; g_sp.landCheck=true;
    for(int i=0;i<4;++i) { g_sp.orig[i]=float(i); g_sp.mine[i]=float(i+10); }
    g_sp.origFlags=17; g_sp.mineFlags=33;
    memcpy(record,&g_sp.point,4); memcpy(record+0x40,g_sp.mine,16); memcpy(record+8,&g_sp.mineFlags,4);
    readable=true; failWrite=0; writes=0; lie=false;
}
int main() {
    int checks=0;
    auto check=[&](bool ok) { ++checks; if(!ok) printf("FAIL %d\n",checks); return ok; };
    seed(); if(!check(SpawnRestore("control") && g_sp.st==0 && g_sp.rec==0 && writes==2)) return 1;
    for(int fail=1;fail<=2;++fail) {
        seed(); failWrite=fail;
        if(!check(!SpawnRestore("failure") && g_sp.st==3 && g_sp.rec==lookup)) return 1;
        failWrite=0; writes=0;
        if(!check(SpawnRestore("retry") && g_sp.st==0 && writes==(fail==1?2:1))) return 1;
    }
    seed(); readable=false;
    if(!check(!SpawnRestore("unreadable") && g_sp.st==3 && writes==0)) return 1;
    readable=true; if(!check(SpawnRestore("read recovered"))) return 1;
    seed(); lookup=0;
    if(!check(!SpawnRestore("missing") && g_sp.st==3 && writes==0)) return 1;
    seed(); lookup+=128;
    if(!check(!SpawnRestore("replacement") && g_sp.st==4 && writes==0)) return 1;
    lookup=g_sp.rec; if(!check(!SpawnRestore("address reused") && writes==0)) return 1;
    seed(); record[0]=0;
    if(!check(!SpawnRestore("wrong id") && g_sp.st==4 && writes==0)) return 1;
    seed(); record[0x40]=99;
    if(!check(!SpawnRestore("foreign transform") && g_sp.st==4 && writes==0)) return 1;
    seed(); record[8]=99;
    if(!check(!SpawnRestore("foreign flags") && g_sp.st==4 && writes==0)) return 1;
    seed(); lie=true;
    if(!check(!SpawnRestore("lying write") && g_sp.st==3 && g_sp.rec==lookup)) return 1;
    lie=false; if(!check(SpawnRestore("verified retry"))) return 1;
    seed(); memcpy(record+0x40,g_sp.orig,16); memcpy(record+8,&g_sp.origFlags,4);
    if(!check(SpawnRestore("already original") && writes==0)) return 1;
    Row row;
    for(int i=0;i<4;++i) row.warp[i]=float(i);
    seed(); g_sp.st=1; memcpy(record+0x40,g_sp.orig,16); memcpy(record+8,&g_sp.origFlags,4);
    SpawnApply(row,"apply"); if(!check(g_sp.st==2 && writes==2)) return 1;
    writes=0; SpawnApply(row,"same"); if(!check(g_sp.st==2 && writes==0)) return 1;
    record[0x40]=99; SpawnApply(row,"foreign"); if(!check(g_sp.st==4 && writes==0)) return 1;
    seed(); g_sp.st=3; SpawnApply(row,"pending"); if(!check(g_sp.st==3 && writes==0)) return 1;
    seed(); lookup=0; SpawnApply(row,"missing"); if(!check(g_sp.st==3 && g_sp.rec!=0)) return 1;
    seed(); readable=false; SpawnApply(row,"read fail"); if(!check(g_sp.st==3 && g_sp.rec!=0)) return 1;
    seed(); g_sp.st=1; memcpy(record+0x40,g_sp.orig,16); memcpy(record+8,&g_sp.origFlags,4); failWrite=2;
    SpawnApply(row,"partial apply");
    if(!check(g_sp.st==0 && !memcmp(record+0x40,g_sp.orig,16) && !memcmp(record+8,&g_sp.origFlags,4))) return 1;
    printf("%d actual-source restoration checks PASS\n",checks);
}
'''.replace('@SPAWN@',spawn).replace('@WRITE@',function('bool SpawnWrite(')).replace('@RESTORE@',function('bool SpawnRestore(const char* why) {')).replace('@APPLY@',function('void SpawnApply('))
setup=Path(r'C:/nioh3-engine/tools/msvc/setup_x64.bat')
with tempfile.TemporaryDirectory(prefix='nioh3-spawn-repro-') as tmp:
    directory=Path(tmp); source=directory/'repro.cpp'; exe=directory/'repro.exe'
    source.write_text(code)
    command=f'call "{setup}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /Fo"{directory / "repro.obj"}" /Fe"{exe}" "{source}"'
    result=subprocess.run('cmd /d /s /c "'+command+'"',capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    print(result.stdout.strip()); assert result.returncode==0,result.stderr

for name in ('ExecOpen','ExecDirect','ExecEmaki'):
    body=function('void '+name+'(')
    assert body[body.index('{')+1:].lstrip().startswith('if (!SpawnRestore('), name
print('3 native admission entry guards PASS')
