"""Replay Ally #83907 using production queue wiring and actor snapshot functions."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent.parent
src = (root / 'src/bosslog.cpp').read_text(encoding='utf-8')
snap = src[src.index('struct PressSnap {'):src.index('void FormHideEndBegin(')]
start = src.index('    switch (pressq::Tick(g_pressQ')
wiring = src[start:src.index('    if (g_fhMode.load()', start)]
test = r'''
#include "press_queue.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
int checks=0, resets=0;
void check(bool b){++checks;if(!b){printf("FAIL check %d\n",checks);exit(1);}}
pressq::State g_pressQ;
std::atomic<uint64_t> g_ghostSeal{0}; std::atomic<bool> g_fbActive{false},g_pickLearned{false};
std::atomic<double> g_pickT{-100}; struct {uint64_t owner=0;} g_fh4;
double tm=344.486; double Now(){return tm;}
ULONGLONG FakeTicks(){return (ULONGLONG)(tm*1000);}
#define GetTickCount64 FakeTicks
void Log(const char*,...){}
uint64_t global=0,mgr[400]={},buckets[32]={},oldNode[1]={},existingNode[1]={};
uint64_t ActorTableGlobal(){return (uint64_t)&global;}
uint64_t FkPlayer(){return 0;}
bool Copy(void* out,uint64_t a,size_t n){memcpy(out,(void*)a,n);return true;}
template<class T> bool Rd(uint64_t a,T& v){return Copy(&v,a,sizeof(v));}
void SealExitReplayPressReset(){++resets;check(pressq::Buttons(g_pressQ,FakeTicks())==0);}
''' + snap + '\nvoid tick(){\n' + wiring + r'''
}
int main(){
 global=(uint64_t)mgr; mgr[0xBA0/8]=(uint64_t)buckets;
 const uint64_t old=(uint64_t)oldNode-0xE0,existing=(uint64_t)existingNode-0xE0;
 buckets[0]=(uint64_t)oldNode;buckets[1]=2;oldNode[0]=(uint64_t)existingNode;
 check(PressSnapTake(tm)); check(AliveAtPress(old)&&AliveAtPress(existing));
 pressq::Queue(g_pressQ,2,FakeTicks());g_ghostSeal=old;g_pickLearned=true; // old seal's stale completion
 tm=344.600;tick();check(resets==1&&pressq::Buttons(g_pressQ,FakeTicks())==0);
 // Old seal unlinks naturally. Replay arrives only 167 ms after physical press snapshot.
 buckets[0]=(uint64_t)existingNode;buckets[1]=1;g_ghostSeal=0;
 tm=344.653;tick();check(pressq::Buttons(g_pressQ,FakeTicks())==2);
 check(!AliveAtPress(old)); // pooled address can now represent the NEW spawn
 check(AliveAtPress(existing)); // genuinely preexisting actors remain forbidden
 check(g_pressSnap.t==tm&&resets==2&&!g_pickLearned); // production callback resets completion before pulse
 // New spawn reuses the old address after replay, before the next tick.
 buckets[0]=(uint64_t)oldNode;buckets[1]=2;tm=344.975;g_pickLearned=true;tick();
 check(!AliveAtPress(old)&&AliveAtPress(existing));check(g_pressQ.dir==0&&resets==2);
 // Retry belongs to the same logical press: do not recapture a delayed new spawn.
 pressq::Queue(g_pressQ,1,FakeTicks());g_pickLearned=false;
 buckets[0]=(uint64_t)existingNode;buckets[1]=1;tm=346;tick();check(resets==3);
 buckets[0]=(uint64_t)oldNode;buckets[1]=2;tm=346.5;tick();
 check(g_pressQ.tries==2&&resets==3&&!AliveAtPress(old)&&AliveAtPress(existing));
 printf("queued snapshot: %d checks PASS (production snapshot + queue wiring; offline)\n",checks);
}
'''
out = root / 'build/queued-snapshot-test'
out.mkdir(parents=True, exist_ok=True)
unit = out / 'test.cpp'
unit.write_text(test, encoding='utf-8')
setup = os.environ.get('MSVC_SETUP', 'C:/Users/User/pmsvc/msvc/setup_x64.bat')
exe = out / 'test.exe'
cmd = f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /I"{root / "src"}" /Fo"{out / "test.obj"}" /Fe"{exe}" "{unit}"'
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout, end=''); print(r.stderr, end=''); r.check_returncode()
subprocess.run([str(exe)], check=True, timeout=30)
