"""Compile the actual verifier; stale replay identity must not authorize effects."""
from pathlib import Path
import subprocess
import tempfile

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

code = r'''
#include <atomic>
#include <cstdio>
#include "@CORE@"
using ULONGLONG = unsigned long long;
using rcore::Row;
struct Snap { unsigned ms28=1, ms14=2; int st[6]{}; };
bool blocked=false, allowed=true;
bool NativeGate(const Snap&) { return blocked; }
bool AttemptEffectsAllowed() { return allowed; }
enum { kFnArmed, kFnWarped, kFnPass, kFnFail };
std::atomic<int> g_fn{kFnWarped};
std::atomic<unsigned> g_gateWant{0};
std::vector<Row> g_rows(1);
int g_fightRow=0, g_fnFights0=0, g_warps=0;
unsigned g_fightMission=1, g_fightStart=2;
ULONGLONG g_fnLastT=0,g_fnFirst=1,g_fnDeadline=60000,g_fnCheckAt=50000,g_warpDue=0;
bool g_fightWarp=false,g_gateReq=false,g_warpOk=true,g_warpPending=false;
namespace fight { int g_fights=0; bool g_active=false; }
int calls=0,reads=0;
bool PlayerPos(float* p) { ++reads; p[0]=1000; p[1]=p[2]=p[3]=0; return true; }
int CallWarp(float,float,float,float) { ++calls; return 0; }
void Log(const char*,...) {}
void Result(const char*,...) {}
@VERIFY@
void reset() {
    calls=reads=g_warps=0; g_fn=kFnWarped; fight::g_fights=0;
    g_fnLastT=0; g_fnFirst=1; g_fnDeadline=60000; g_fnCheckAt=50000;
    g_fightWarp=g_warpPending=false; blocked=false; allowed=true;
    g_rows[0]=Row{}; g_rows[0].atFight=true; g_rows[0].bossOk=true;
}
int main() {
    Snap s;
    reset(); s.ms28=99; FnVerify(s,11002);
    if(calls || reads || g_fnLastT) { printf("FAIL: foreign mission warped=%d reads=%d\n",calls,reads); return 1; }
    reset(); s.ms28=1; s.ms14=99; FnVerify(s,11002);
    if(calls || g_warps) { puts("FAIL: changed retry warped"); return 1; }
    reset(); s.ms14=2; FnVerify(s,11002);
    if(calls!=1 || g_warps!=1) { puts("FAIL: valid duel warp lost"); return 1; }
    reset(); s.ms28=99; fight::g_fights=1; FnVerify(s,11002);
    if(g_fn!=kFnWarped) { puts("FAIL: foreign fight credited"); return 1; }
    reset(); s.ms28=1; s.ms14=99; fight::g_fights=1; FnVerify(s,11002);
    if(g_fn!=kFnPass) { puts("FAIL: same-mission fight after retry change lost"); return 1; }
    reset(); s.ms14=99; g_fightWarp=true; g_fnCheckAt=0;
    g_rows[0].warp[0]=1000; g_rows[0].arena[0]=5000;
    FnVerify(s,11002);
    if(g_warpPending) { puts("FAIL: changed retry scheduled warp"); return 1; }
    reset(); s.ms14=2; blocked=true; FnVerify(s,11002);
    if(calls || reads) return 1;
    reset(); allowed=false; FnVerify(s,11002);
    if(calls || reads) return 1;
    puts("8 actual-source verification identity checks PASS");
}
'''.replace('@CORE@', (ROOT/'src/rematch_core.h').as_posix()).replace('@VERIFY@',function('void FnVerify('))
setup=Path('C:/nioh3-engine/tools/msvc/setup_x64.bat')
with tempfile.TemporaryDirectory(prefix='nioh3-verify-identity-') as tmp:
    directory=Path(tmp); source=directory/'test.cpp'; exe=directory/'test.exe'
    source.write_text(code)
    command=f'call "{setup}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /Fo"{directory / "test.obj"}" /Fe"{exe}" "{source}"'
    result=subprocess.run('cmd /d /s /c "'+command+'"',capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    print(result.stdout.strip())
    assert result.returncode==0,result.stderr
