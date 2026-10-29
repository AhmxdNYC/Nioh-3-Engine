"""Fault each actual snapshot read; exercise native admission gates without a game."""
from pathlib import Path
import subprocess
import tempfile
import os, sys

ROOT = Path(__file__).resolve().parents[1]
text = (ROOT / 'src/rematch.cpp').read_text()
def function(signature):
    begin = text.index(signature)
    end = text.index('{', begin) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[begin:end]
attempt = text[text.index('enum AttemptPhase'):text.index('// Unknown session reads')]
snapshot = text[text.index('struct Snap {'):text.index('Snap ReadSnap()')]
exit_dispatch = text[text.index('            if (!g_exitOk) Refuse(g_exitWhy);'):text.index('        } else if (kind == kReqAbort)')]
tick_observation = function('void Tick()').split('    SpawnTick(s, t, loadEnded);')[0].replace('void Tick()', 'bool ObserveTick()') + '\n    return loadEnded;\n}'
code = r"""
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <map>
#include <vector>
#include "rematch_core.h"
using ULONGLONG=unsigned long long;
constexpr uint64_t G_GM=1,G_PT=2,G_MS=3,G_QUEUE=4,G_CH=5,G_PM=6,G_NET=7;
std::map<uint64_t,std::vector<unsigned char>> memory;
std::vector<uint64_t> reads;
uint64_t bad=0;
bool Copy(void* out,uint64_t address,size_t n) {
    reads.push_back(address);
    if(address==bad) return false;
    const auto found=memory.find(address);
    if(found==memory.end() || found->second.size()!=n) return false;
    memcpy(out,found->second.data(),n); return true;
}
template<class T> bool Rd(uint64_t address,T& out) { return Copy(&out,address,sizeof(T)); }
template<class T> void put(uint64_t address,T value) {
    auto& bytes=memory[address]; bytes.resize(sizeof(value)); memcpy(bytes.data(),&value,sizeof(value));
}
std::atomic<ULONGLONG> g_qChangedAt{0},g_lastTick{20000};
ULONGLONG Ms() { return 20000; }
bool g_live=true,g_exitOk=true;
const char* g_exitWhy="exit unavailable";
constexpr int kIdle=0;
std::atomic<int> g_state{kIdle};
namespace fight { bool g_active=false; void ForceEnd(const char*) { g_active=false; } }
void AutoMovesTick() {}
bool TrainingOwnsFight() { return false; }
std::atomic<bool> g_staleBbm{false};
void Log(const char*,...);
@SNAPSHOT@
@ATTEMPT@
bool XwPending() { return false; }  // Nioh3Training absent
void CoverTick(const Snap&, ULONGLONG) {}
@FUNCTIONS@
void ToIdle(const char*,bool=false) {}
bool g_wasLoading=false,g_posAtLoadOk=false,g_posLiveOk=true;
float g_posAtLoad[4]{},g_posLive[4]{};
ULONGLONG g_loadRiseAt=0;
bool PlayerPos(float*) { return true; }
@TICK_OBSERVATION@
int nativeReturns=0;
const char* refusal=nullptr;
void Refuse(const char* reason) { refusal=reason; }
int CallReturn() { ++nativeReturns; return 0; }
void Log(const char*,...) {}
void Result(const char*,...) {}
// This suite exercises the existing generic return dispatch; actual practice
// ownership paths are executed by test_native_attempt.py.
constexpr uint32_t kReqPracticeExit=10;
uint32_t kind=8;
const char* PracticeReturnGate(const Snap&) { return "not a practice fixture"; }
void ExitDispatch(const Snap& s) {
@EXIT@
}
void seed() {
    g_attempt=NativeAttempt{}; g_attemptPhase=aNone;
    memory.clear(); reads.clear(); bad=0;
    put<uint64_t>(G_GM,10000); memory[10000+0x588]=std::vector<unsigned char>(6,0);
    put<uint64_t>(G_PT,20000); put<uint64_t>(20000+0xCD*0x18,30000);
    put<uint64_t>(G_MS,40000); put<uint64_t>(40008,50000);
    put<uint32_t>(50000+0x14,67064); put<uint32_t>(50000+0x28,0x1CBC);
    memory[G_QUEUE]=std::vector<unsigned char>(3*0x58,0);
    put<uint64_t>(G_CH,60000); put<uint64_t>(60000,70000); put<uint8_t>(70000+0x2C18,0);
    put<uint64_t>(G_PM,80000); put<int32_t>(80000+0x34,0);
    put<uint64_t>(G_NET,90000); put<uint8_t>(90000+0x1DC,0);
    g_qChangedAt=0; g_lastTick=20000; g_state=kIdle; g_live=true; fight::g_active=false;
    nativeReturns=0; refusal=nullptr;
}
int main() {
    int checks=0;
    auto check=[&](bool ok) { ++checks; if(!ok) printf("FAIL %d\n",checks); return ok; };
    seed(); Snap s=ReadSnap();
    if(!check(!Gates(s,false) && !Gates(s,true))) return 1;
    auto addresses=reads;
    ExitDispatch(s); if(!check(nativeReturns==1 && !refusal)) return 1;
    for(auto address:addresses) {
        seed(); bad=address; s=ReadSnap();
        if(!check(Gates(s,false)!=nullptr)) return 1;
        // Only session/game/mission reads are required by the return route.
        const bool exitCritical=address==G_GM || address==10000+0x588 || address==G_MS || address==40008 ||
            address==50000+0x14 || address==50000+0x28 || address==G_PM || address==80000+0x34 ||
            address==G_NET || address==90000+0x1DC;
        ExitDispatch(s);
        if(!check(exitCritical ? nativeReturns==0 && refusal : nativeReturns==1 && !refusal)) return 1;
        if(exitCritical && !check(NativeGate(s)!=nullptr)) return 1;
        bad=0; s=ReadSnap();
        if(!check(nativeReturns ? AttemptStartGate()!=nullptr && !NativeGate(s) : !Gates(s,false))) return 1;
    }
    for(uint64_t address:{G_GM,G_PT,G_MS,uint64_t(40008),G_CH,uint64_t(60000),G_PM,G_NET}) {
        seed(); put<uint64_t>(address,0); s=ReadSnap();
        if(!check(Gates(s,false)!=nullptr)) return 1;
    }
    for(int32_t mode:{1,2,-1,3,INT32_MAX}) {
        seed(); put<int32_t>(80000+0x34,mode); s=ReadSnap(); ExitDispatch(s);
        if(!check(Gates(s,false) && nativeReturns==0 && NativeGate(s))) return 1;
    }
    seed(); put<uint8_t>(90000+0x1DC,1); s=ReadSnap(); ExitDispatch(s);
    if(!check(Gates(s,false) && nativeReturns==0)) return 1;
    seed(); memory[10000+0x588][2]=1; s=ReadSnap(); ExitDispatch(s);
    if(!check(Gates(s,false) && nativeReturns==0)) return 1;
    seed(); memory[10000+0x588][3]=1; s=ReadSnap();
    if(!check(Gates(s,false)!=nullptr)) return 1;
    seed(); put<uint8_t>(70000+0x2C18,1); s=ReadSnap();
    if(!check(Gates(s,false)!=nullptr)) return 1;
    seed(); g_qChangedAt=19000; s=ReadSnap(); if(!check(Gates(s,false)!=nullptr)) return 1;
    seed(); g_state=1; s=ReadSnap(); if(!check(Gates(s,false)!=nullptr)) return 1;
    seed(); fight::g_active=true; s=ReadSnap(); if(!check(Gates(s,false)!=nullptr)) return 1;
    seed(); g_lastTick=100; s=ReadSnap(); if(!check(Gates(s,true) && !Gates(s,false))) return 1;
    seed(); g_wasLoading=false;
    memory[10000+0x588][2]=1;
    if(!check(!ObserveTick() && g_wasLoading)) return 1;
    bad=10000+0x588;
    if(!check(!ObserveTick() && g_wasLoading && !g_posLiveOk)) return 1;
    bad=0;
    if(!check(!ObserveTick() && g_wasLoading)) return 1;
    memory[10000+0x588][2]=0;
    if(!check(ObserveTick() && !g_wasLoading)) return 1;
    if(!check(!ObserveTick())) return 1;
    printf("%d actual-source admission checks PASS (%zu snapshot reads faulted)\n",checks,addresses.size());
}
""".replace('@SNAPSHOT@',snapshot).replace('@FUNCTIONS@','\n'.join(function(sig) for sig in (
    'Snap ReadSnap()', 'const char* SessionGate(', 'const char* NativeGate(', 'const char* ExitGate(', 'void AttemptObserve(', 'bool AttemptEffectsAllowed(', 'const char* AttemptReturnGate(', 'void AttemptReturning(', 'const char* Gates('))).replace('@EXIT@',exit_dispatch).replace('@TICK_OBSERVATION@',tick_observation).replace('@ATTEMPT@',attempt)
setup=Path(r'C:/nioh3-engine/tools/msvc/setup_x64.bat')
with tempfile.TemporaryDirectory(prefix='nioh3-read-gates-') as tmp:
    directory=Path(tmp); source=directory/'test.cpp'; exe=directory/'test.exe'
    source.write_text(code)
    command=f'call "{setup}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /I"{ROOT / "src"}" /Fo"{directory / "test.obj"}" /Fe"{exe}" "{source}"'
    result=subprocess.run(([sys.executable, '-m', 'ziglang', 'c++', '-target', 'x86_64-windows-gnu', '-std=c++17', '-fms-extensions', '-I'+str(ROOT/'src'), str(source), '-o', str(exe)] if os.environ.get('N3_TEST_ZIG') else 'cmd /d /s /c "'+command+'"'),capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    print(result.stdout.strip()); assert result.returncode==0,result.stderr
for name in ('DoWarp','FnVerify','GateTick'):
    body=function('void '+name+'(')
    assert body[body.index('{')+1:].lstrip().startswith('if (NativeGate(s) || !AttemptEffectsAllowed()) return;'),name
spawn=function('void SpawnTick(')
assert spawn.index('if (NativeGate(s)) return;') < spawn.index('SpawnApply(')
assert spawn.index('SpawnRestore("online")') < spawn.index('if (NativeGate(s)) return;')
tick=function('void Tick()')
assert 'if (s.queueOk && s.qHash != g_qHash)' in tick
assert tick.index('Gates(s, false)') < tick.index('ExecDirect(')
print('7 source integration assertions PASS (not native-runtime coverage)')
