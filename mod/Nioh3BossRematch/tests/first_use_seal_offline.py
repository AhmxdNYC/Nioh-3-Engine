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
helper = src[src.index('double KnownTransformWaitUntil('):src.index('bool UseFamilyAct(')]
start = src.index('                   const int useMs = g_formNeedUseMs.load();')
gate = src[start:src.index('                   static uint64_t useSkipLogged', start)]
cleanup=src[src.index('            // #82145/#82155:'):src.index('            ClearKeptExtras();',src.index('            // #82145/#82155:'))]
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
std::atomic<double> g_pickT{-100},g_rtAcceptT{-100};std::atomic<uint32_t> g_pickCore{0};bool transforms=true;
bool ScOn(){return transforms;}
bool UseConfirmedSince(double t){return (g_orbHooked&&g_orbT>=t-.05)||g_sealEdgeT>=t-.05;}
double KnownTransformWaitUntil(double);
uint32_t g_throwCutAct=0;struct {double t=0;} g_pressSnap;
bool alive=false;int checks=0,binds=0;
bool AliveAtPress(uint64_t){return alive;}
void Log(const char*,...){}
void FormHideEndBegin(uint64_t,uint64_t){++binds;}
void FormHideEndReason(uint64_t,uint64_t,double){}
uint64_t attempted=0;double attemptedT=-1;
double pendAt=1,pendTrig=2,pendCapT=3;bool pendEdge=true,pendSpawn=true;uint64_t pendPlayer=4,pendCap=5;
void cleanupFailed(const char* reason){
'''+cleanup+r'''
}

void check(bool b){++checks;assert(b);}
void reset(double t){g_fh4={};g_fh4.started=t;g_fkSealChr=0;g_fkSealT=-100;g_orbT=-100;g_sealEdgeT=t;
 g_throwDoneT=t+.2;g_throwCutT=-100;alive=false;binds=0;attempted=0;attemptedT=-1;g_orbSpawnMs=1800;g_formNeedUseMs=2200;
 g_pickT=-100;g_pickCore=0;transforms=true;}
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
 // pc2 #81993: confirmed equipped LEFT first spawn at +3.87 s survives old +2.59 s exit.
 reset(10);g_pickT=10;g_pickCore=0x5129B;g_orbT=10.38;
 check(!tick(12.59));check(!tick(13.87,789,13.87));check(g_fh4.seal==789&&binds==1);
 reset(10);g_pickT=10;g_pickCore=0x5129B;g_orbT=10.38;
 check(!tick(17.99));check(!strcmp(tick(18.01),"throw-done-no-seal"));
 // No FormSlot0 capture: spawn-hit route binds before timeout after its 0.3-second observation delay.
 reset(10);g_pickT=10;g_pickCore=0x5129B;g_orbT=10.38;g_fkSealChr=456;g_fkSealT=13.87;
 check(!tick(14.18));check(g_fh4.seal==456&&binds==1);
 // Confirmed only: known ID plus refused press does not extend the short release.
 reset(10);g_pickT=10;g_pickCore=0x5129B;g_sealEdgeT=-100;check(KnownTransformWaitUntil(10)<0);
 reset(10);g_pickT=10;g_pickCore=0x5129B;transforms=false;check(KnownTransformWaitUntil(10)<0);
 reset(10);g_pickT=8;g_pickCore=0x5129B;check(KnownTransformWaitUntil(10)<0);
 // Delayed-begin gate agrees with the same absolute deadline; unknown/refused/OFF remain strict.
 reset(10);g_pickT=10;g_pickCore=0x5129B;g_orbT=10.38;
 check(canStart(13.87,10));check(!canStart(18.01,10));
 g_pickCore=0;check(!canStart(13.87,10));g_pickCore=0x5129B;transforms=false;check(!canStart(13.87,10));
 transforms=true;g_orbT=-100;g_sealEdgeT=-100;check(!canStart(13.87,10));
 for(const char* why : {"throw-done-no-seal","press-no-seal","throw-cancelled"}) {
  pendAt=1;pendTrig=2;pendCapT=3;pendEdge=pendSpawn=true;pendPlayer=4;pendCap=5;
  cleanupFailed(why);check(!pendAt&&!pendTrig&&!pendEdge&&!pendSpawn&&!pendPlayer&&!pendCap&&pendCapT==-1);
 }
 pendAt=1;cleanupFailed("ghost-exit");check(pendAt==1);
 // #82145: 5.49-second launch capture and matching late admission; 17.2 seconds remains refused.
 reset(10);g_pickT=10;g_pickCore=0x5129B;g_orbT=10.1;
 check(!tick(15.48));check(canStart(15.49,10));check(!tick(15.49,789,15.49));check(g_fh4.seal==789&&binds==1);
 reset(10);g_pickT=10;g_pickCore=0x5129B;g_orbT=10.1;g_fkSealChr=456;g_fkSealT=15.49;
 check(!tick(15.8));check(g_fh4.seal==456&&binds==1);
 reset(10);g_pickT=10;g_pickCore=0x5129B;g_orbT=10.1;check(!canStart(27.2,10));
 check(tick(18.01,789,18.01)!=nullptr&&g_fh4.seal==0); // capture cannot outrun expired wait
 reset(10);g_pickT=10;g_pickCore=0x5129B;g_orbT=10.1;
 check(canStart(18.0,10));check(!tick(18.0,789,18.0));check(g_fh4.seal==789);
 // Later DOWN press has a fresh deadline; a game-refused press never gets this extension.
 reset(30);g_pickT=30;g_pickCore=0x60B86;g_orbT=30.1;
 check(!tick(35.49,999,35.49));check(g_fh4.seal==999);
 reset(40);g_pickT=40;g_pickCore=0x60B86;g_orbT=-100;g_sealEdgeT=-100;
 check(!canStart(45.49,40));check(tick(42.6)!=nullptr);
 printf("first-use seal binding/timeouts: %d checks PASS (actual source branches; offline)\n",checks);
}
'''
out=root/'build/first-use-test';out.mkdir(parents=True,exist_ok=True)
unit=out/'first-use.cpp'
# Insert helper and gate before tick, while retaining the real production expressions.
prefix, tick_start = pre.split('const char* tick(', 1)
unit.write_text(prefix+helper+'\nbool canStart(double capT,double leftT){\n'+gate+'\nreturn useOk;}\nconst char* tick('+tick_start+adopt+'\nif (!g_fh4.seal) {\n'+bind+'\n}\n'+post)
setup=Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat');exe=out/'first-use.exe'
cmd=f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /Fo"{out / "first-use.obj"}" /Fe"{exe}" "{unit}"'
r=subprocess.run('cmd /s /c "'+cmd+'"',capture_output=True,text=True);print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
subprocess.run([str(exe)],check=True,timeout=30)
