"""Actual native-clear observer + save-veto + production fight ring, no game."""
from pathlib import Path
import subprocess, tempfile, os
ROOT=Path(__file__).resolve().parents[3]
def function(path, sig):
 s=(ROOT/path).read_text(); a=s.index(sig); b=s.index('{',a); depth=1; e=b+1
 while depth:
  depth+=(s[e]=='{')-(s[e]=='}'); e+=1
 return s[a:e]
hk=function('mod/Nioh3BossRematch/src/bosslog.cpp','void HkEnd(')
assert hk.index('g_clearWin.store(1)')<hk.index('MaybeEnd()'),'script boss-mode end must record a win before the fight is closed'
code=r'''#include <windows.h>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "fightlog.h"
FlLog logbook; double clockNow=1; uint32_t running=123; int nativeWrites=0;
namespace bosslog {
bool RosterPending() { return false; }
std::atomic<uint64_t> g_logToken{0}; std::atomic<uint32_t> g_logMission{0};
std::atomic<bool> g_active{true}; std::atomic<double> g_endAt{0}; std::atomic<int> g_clearWin{0};
}
void Confirm(uint64_t token) { FlConfirmWin(&logbook,token,1000,nullptr); }
HMODULE FakeModule(const wchar_t*) { return (HMODULE)1; }
FARPROC FakeExport(HMODULE,const char*) { return reinterpret_cast<FARPROC>(&Confirm); }
#define GetModuleHandleW FakeModule
#define GetProcAddress FakeExport
namespace fight { double Now() { return clockNow; }
@CONFIRM@
}
std::atomic<uint32_t> g_unbeaten{0}; std::atomic<int> g_unbeatenVetoes{0};
uint64_t G_MS=100; uint32_t g_runningMission=0;
template<class T> bool Rd(uint64_t at,T& value) { value=static_cast<T>(at==G_MS ? 200 : at==208 ? 300 : running); return true; }
void Log(const char*,...) {}
@VETO@
void start() {
 FlStart(&logbook,FL_BOSSMATCH,"runtime boss",0,nullptr);
 bosslog::g_logToken=logbook.rec[logbook.open].token; bosslog::g_logMission=123;
 bosslog::g_active=true; bosslog::g_endAt=0; clockNow=1; running=123; g_unbeaten=0;
}
int result() { return logbook.rec[(logbook.next+FL_CAP-1)%FL_CAP].result; }
int main() {
 FlInit(&logbook);
 start(); assert(!VetoClear(0)); assert(result()==FL_WIN); // debug loggers off, normal native clear
 start(); g_unbeaten=123; assert(VetoClear(0)); assert(result()==FL_WIN); assert(g_unbeatenVetoes==1); // still vetoed
 start(); FlEnd(&logbook,FL_QUIT,900,nullptr); bosslog::g_active=false; bosslog::g_endAt=1;
 assert(!VetoClear(0)); assert(result()==FL_WIN); assert(logbook.rec[(logbook.next+FL_CAP-1)%FL_CAP].t1==900); // late clear retains duration
 start(); FlEnd(&logbook,FL_LOSS,900,nullptr); assert(!VetoClear(0)); assert(result()==FL_LOSS);
 start(); running=456; assert(!VetoClear(0)); assert(result()==FL_OPEN); // unrelated mission
 start(); FlStart(&logbook,FL_PRACTICE,"new round",500,nullptr); assert(!VetoClear(0)); assert(result()==FL_OPEN); // stale token
 start(); bosslog::g_logToken=0; bosslog::g_logMission=0; FlEnd(&logbook,FL_QUIT,900,nullptr);
 assert(!VetoClear(0)); assert(result()==FL_QUIT); // explicit return invalidates native observer
 start(); bosslog::g_active=false; clockNow=22; FlEnd(&logbook,FL_QUIT,900,nullptr);
 assert(!VetoClear(0)); assert(result()==FL_QUIT); // grace expired
 start(); FlEnd(&logbook,FL_QUIT,900,nullptr); assert(result()==FL_QUIT); // HP poke / no native clear is not proof
 puts("9 native outcome scenarios PASS (debug-off, veto, late clear, loss, mission, token, return, timeout, no-clear)");
}
'''
code=code.replace('@CONFIRM@',function('mod/Nioh3BossRematch/src/bosslog.cpp','void ConfirmNativeWin(')).replace('@VETO@',function('mod/Nioh3BossRematch/src/rematch.cpp','bool VetoClear('))
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp); src=d/'test.cpp'; exe=d/'test.exe';src.write_text(code)
 setup=os.environ['N3_MSVC_SETUP']
 cmd=f'call "{setup}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /I"{ROOT / "mod/Nioh3Training/src"}" /Fo"{d / "test.obj"}" /Fe"{exe}" "{src}"'
 r=subprocess.run('cmd /d /s /c "'+cmd+'"',capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True);print(r.stdout);assert r.returncode==0,r.stderr
