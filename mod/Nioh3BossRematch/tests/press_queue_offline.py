"""#83466 offline: a press made while the old seal is in its ghost exit is queued and replayed once it is gone. Never touches the game."""
from pathlib import Path
import os, subprocess
root = Path(__file__).resolve().parent.parent
src = (root / 'src/bosslog.cpp').read_text(encoding='utf-8')
# wiring: the real production code must queue at the press edge, tick the queue, and inject through the XInput hook
assert 'pressq::Queue(g_pressQ, held' in src and src.count('pressq::Queue(') == 2
assert 'pressq::Tick(g_pressQ' in src and 'pressq::Buttons(g_pressQ' in src
assert 'SealQueueDespawn' not in src[src.index('void PickPoll()'):src.index('void PickLearn(')], 'queued press must not despawn the old seal early'
test = r'''
#include "press_queue.h"
#include <cstdio>
#include <cstdlib>
int checks=0; void check(bool b){++checks; if(!b){printf("FAIL %d\n",checks);exit(1);}}
using namespace pressq;
// run the replay like the game: tick every 16 ms, hook polled every 8 ms; returns the tick (ms) the d-pad bit first showed, -1 never
struct Sim { State s; ULONGLONG t=1000; int firstBit=-1, pulses=0; Step last=Step::Idle; };
static void run(Sim& m,int ms,ULONGLONG ghostGoneAt,ULONGLONG blockEnd,ULONGLONG thrownAt){
  ULONGLONG end=m.t+ms;
  for(;m.t<end;m.t+=8){
    if(m.t%16==0){ Step st=Tick(m.s,m.t,m.t<ghostGoneAt,m.t<blockEnd,thrownAt&&m.t>=thrownAt); if(st==Step::Pulse)++m.pulses; if(st!=Step::Idle&&st!=Step::Wait)m.last=st; }
    if(Buttons(m.s,m.t)&&m.firstBit<0)m.firstBit=(int)(m.t-1000);
  }
}
int main(){
  { // PC2 #83967: a press 12-29 ms into ghost exit inherits the old seal's learned flag.
    State s; bool learned=true; int prepared=0;
    auto prepare=[&](bool first){check(first);++prepared;learned=false;};
    Queue(s,2,1012);
    check(Tick(s,1013,true,true,learned,prepare)==Step::Wait);
    check(s.dir==2 && s.tries==0 && prepared==0 && Buttons(s,1013)==0);
    check(Tick(s,1467,false,true,learned,prepare)==Step::Wait); // form block also gates replay
    check(Tick(s,1483,false,false,learned,prepare)==Step::Pulse);
    check(prepared==1 && !learned && s.tries==1 && Buttons(s,1483)==2);
    check(Tick(s,1499,false,false,learned,prepare)==Step::Wait);
    learned=true; // only the new replay's spawn completes it
    check(Tick(s,1805,false,false,learned,prepare)==Step::Done && s.dir==0);
  }
  { // Old learned flag cannot turn an expired, never-fired press into a success.
    State s; Queue(s,1,1000);
    check(Tick(s,4001,true,false,true)==Step::Expired && s.dir==0);
  }
  { // Replacement resets replay ownership even when the replaced press already fired.
    State s; Queue(s,1,1000);check(Tick(s,1000,false,false,false)==Step::Pulse);
    Queue(s,2,1100);
    check(Tick(s,1101,true,false,true)==Step::Wait && s.tries==0 && s.dir==2);
    check(Tick(s,1200,false,false,true)==Step::Pulse && s.tries==1);
  }
  { // queued press waits for the old seal (gone at +1200 ms), then fires within one tick, 120 ms pulse
    Sim m; Queue(m.s,2,m.t); run(m,1100,1000+1200,0,0);
    check(m.firstBit<0 && m.pulses==0);      // old seal alive: nothing injected (no early throw)
    run(m,150,1000+1200,0,0);
    check(m.firstBit>=1200 && m.firstBit<=1216 && m.pulses==1);
    check(Buttons(m.s,m.s.pulseAt.load()+10)==0x0002);                 // DOWN
    check(Buttons(m.s,m.s.pulseAt.load()+kPulseMs)==0);                // pulse ends
  }
  { // LEFT maps to its own bit; form block also holds the replay
    Sim m; Queue(m.s,1,m.t); run(m,2000,1000,1000+500,0);
    check(m.firstBit>=500 && m.firstBit<=516);
    check(Buttons(m.s,m.s.pulseAt.load()+5)==0x0004);
  }
  { // game accepts (new spawn seen) -> done, no more pulses
    Sim m; Queue(m.s,2,m.t); run(m,400,1000,0,1000+200);
    check(m.last==Step::Done && m.s.dir.load()==0 && m.pulses<=1);
    int p=m.pulses; run(m,2000,1000,0,1000+200); check(m.pulses==p && Buttons(m.s,m.t)==0);
  }
  { // game still drops it -> retried, gives up after kMaxTries
    Sim m; Queue(m.s,2,m.t); run(m,2800,1000,0,0);
    check(m.pulses==kMaxTries && m.last==Step::GaveUp && m.s.dir.load()==0);
  }
  { // old seal never leaves: dropped after the window, nothing fired
    Sim m; Queue(m.s,1,m.t); run(m,kWindowMs+200,1000+100000,0,0);
    check(m.pulses==0 && m.firstBit<0 && m.last==Step::Expired);
  }
  { // newest press replaces an older queued one
    Sim m; Queue(m.s,1,m.t); Queue(m.s,2,m.t+50); check(m.s.dir.load()==2 && m.s.tries.load()==0);
  }
  { // nothing queued: never injects
    Sim m; run(m,500,0,0,0); check(m.firstBit<0 && m.pulses==0 && Tick(m.s,m.t,false,false,false)==Step::Idle);
  }
  printf("press queue: %d checks PASS (offline; no game)\n",checks);
}
'''
out = root / 'build' / 'press-queue-test'
out.mkdir(parents=True, exist_ok=True)
unit = out / 'pq.cpp'; unit.write_text(test, encoding='utf-8')
setup = os.environ.get('MSVC_SETUP', 'C:/Users/User/pmsvc/msvc/setup_x64.bat')
exe = out / 'pq.exe'
cmd = f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /I"{root / "src"}" /Fo"{out / "pq.obj"}" /Fe"{exe}" "{unit}"'
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout, end=''); print(r.stderr, end=''); r.check_returncode()
r = subprocess.run([str(exe)], check=True, timeout=30, capture_output=True, text=True); print(r.stdout, end='')
