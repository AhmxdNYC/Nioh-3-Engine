"""Compile the actual pending-seal adoption/end branches against pc2 #81875 timing fixtures."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
src = (root/'src/bosslog.cpp').read_text(encoding='utf-8')
start = src.index('    if (g_fh4.owner && (g_fh4.fromSpawn || g_fh4.keepSeal))')
end = src.index('    {   // pc #56435', start)
adopt = src[start:end]
start = src.index('            if (cap && capT >= g_fh4.started - .05 && AliveAtPress(cap))')
end = src.index('        } else if (cap != g_fh4.seal', start)
bind = src[start:end]
pre = r'''
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cstring>
struct State {uint64_t owner=123,seal=0;double started=0,capT=0;bool fromSpawn=false,keepSeal=false;} g_fh4;
std::atomic<int> g_fhFollow{0},g_orbHooked{1},g_orbSpawnMs{1800},g_noSpawnEndMs{400},g_formNeedUseMs{2200},g_sealCancelEndMs{100};
std::atomic<uint64_t> g_fkSealChr{0};std::atomic<uint32_t> g_fkSealDesc{0};
std::atomic<double> g_orbT{-100};double g_fkSealT=-100,g_sealEdgeT=-100,g_throwDoneT=-100,g_throwCutT=-100;
uint32_t g_throwCutAct=0;struct {double t=0;} g_pressSnap;
bool alive=false;int checks=0,binds=0;
bool AliveAtPress(uint64_t){return alive;}
void Log(const char*,...){}
void FormHideEndBegin(uint64_t,uint64_t){++binds;}
void FormHideEndReason(uint64_t,uint64_t,double){}
uint64_t attempted=0;double attemptedT=-1;
void check(bool b){++checks;assert(b);}
void reset(double t){g_fh4={};g_fh4.started=t;g_fkSealChr=0;g_fkSealT=-100;g_orbT=-100;g_sealEdgeT=t;
 g_throwDoneT=t+.2;g_throwCutT=-100;alive=false;binds=0;attempted=0;attemptedT=-1;g_orbSpawnMs=1800;g_formNeedUseMs=2200;}
const char* tick(double now,uint64_t cap=0,double capT=-100){uint64_t player=123;const char* reason=nullptr;
'''
post = r'''
return reason;}
int main(){
 // DOWN first: press 206.669, hide 207.069, spawn 207.109, no FormSlot0 capture, follow OFF.
 reset(206.669);g_orbT=206.8;g_fkSealChr=456;g_fkSealT=207.109;
 check(!tick(207.111)&&!g_fh4.seal);check(!tick(207.410));
 check(g_fh4.seal==456&&g_fh4.fromSpawn&&g_fh4.keepSeal&&binds==1);
 // LEFT first: old timeout 222.639, real capture 222.810; keep ownership through the gap.
 reset(220.730);g_orbT=220.839;g_sealEdgeT=220.781;g_throwDoneT=221.189;
 check(!tick(222.651));check(!g_fh4.seal);check(!tick(222.810,789,222.810));check(g_fh4.seal==789&&binds==1);
 // Genuine missing spawn still ends after the bounded accepted-use window.
 reset(10);g_orbT=10.1;check(!tick(12.29));check(!strcmp(tick(12.31),"throw-done-no-seal"));
 // Refused/no-orb use and cancelled throw keep the previous fast release behavior.
 reset(10);check(!strcmp(tick(10.61),"throw-done-no-seal"));
 reset(10);g_throwDoneT=-100;g_throwCutT=10.1;check(!strcmp(tick(10.41),"throw-cancelled"));
 reset(10);g_throwDoneT=-100;check(!strcmp(tick(12.51),"press-no-seal"));
 // An already-alive or stale spawn cannot bind, even in pin mode.
 reset(10);g_fkSealChr=456;g_fkSealT=10.1;alive=true;check(!tick(10.45)&&!g_fh4.seal);
 reset(10);g_fkSealChr=456;g_fkSealT=9;check(!tick(10.45)&&!g_fh4.seal);
 // A real FormSlot0 capture wins over a different spawn note.
 reset(10);g_fkSealChr=456;g_fkSealT=10.1;check(!tick(10.45,789,10.2));check(g_fh4.seal==789&&!g_fh4.fromSpawn);
 // Longer configured orb wait must not be cut by the independent 2.5 s watchdog.
 reset(10);g_orbT=10.1;g_orbSpawnMs=4000;check(!tick(12.6));check(!strcmp(tick(14.11),"throw-done-no-seal"));
 printf("first-use seal binding/timeouts: %d checks PASS (actual source branches; offline)\n",checks);
}
'''
out=root/'build/first-use-test';out.mkdir(parents=True,exist_ok=True)
unit=out/'first-use.cpp';unit.write_text(pre+adopt+'\nif (!g_fh4.seal) {\n'+bind+'\n}\n'+post)
setup=Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat');exe=out/'first-use.exe'
cmd=f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /Fo"{out / "first-use.obj"}" /Fe"{exe}" "{unit}"'
r=subprocess.run('cmd /s /c "'+cmd+'"',capture_output=True,text=True);print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
subprocess.run([str(exe)],check=True,timeout=30)
