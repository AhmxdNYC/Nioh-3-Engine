"""Execute production delayed native retry with an owned-return state fixture."""
from pathlib import Path
import subprocess,sys,tempfile
s=(Path(__file__).resolve().parents[1]/'src/rematch.cpp').read_text()
def fn(sig):
 a=s.index(sig);e=s.index('{',a)+1;d=1
 while d:
  d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
code=r'''
#include <atomic>
#include <cstdint>
#include <cassert>
#include <cstdio>
using ULONGLONG=uint64_t;
struct Snap{};
constexpr int aNone=0,aActive=1,aReturning=2,aQuarantined=3,kIdle=0,kReqFight=6;
int g_nativeAgainRow; ULONGLONG g_nativeAgainAt;
uint64_t g_nativeAgainSequence,g_nativeAgainEpoch,g_attemptSequence;
std::atomic<uint64_t> g_requestEpoch;
std::atomic<int> g_attemptPhase,g_state,g_req;
int g_pend,posts,lastRow; bool sessionLost,blocked;
void Log(const char*,...){}
const char* SessionGate(const Snap&){return sessionLost?"lost":nullptr;}
const char* Gates(const Snap&,bool){return blocked?"blocked":nullptr;}
void Post(uint32_t kind,int row,const char*){assert(kind==kReqFight);posts++;lastRow=row;g_requestEpoch++;g_req=kind;}
@BODY@
void reset(){g_nativeAgainRow=7;g_nativeAgainAt=100;g_nativeAgainSequence=g_attemptSequence=12;
 g_nativeAgainEpoch=g_requestEpoch=3;g_attemptPhase=aReturning;g_state=2;g_req=g_pend=posts=0;sessionLost=blocked=false;}
int main(){Snap s;
 reset();NativeGateRetryTick(s,200);assert(!posts&&g_nativeAgainRow==7);
 g_attemptPhase=aNone;NativeGateRetryTick(s,201);assert(!posts); // restoration has not gone idle
 g_state=kIdle;blocked=true;NativeGateRetryTick(s,202);assert(!posts); // fight/load gate still blocks
 blocked=false;NativeGateRetryTick(s,203);assert(posts==1&&lastRow==7&&g_nativeAgainRow==-1);
 NativeGateRetryTick(s,204);assert(posts==1);
 for(int k=0;k<5;k++){reset();if(k==0)g_requestEpoch++;if(k==1)g_attemptSequence++;if(k==2)g_attemptPhase=aQuarantined;
 if(k==3)sessionLost=true;NativeGateRetryTick(s,k==4?120101:200);assert(!posts&&g_nativeAgainRow==-1);}
 reset();g_attemptPhase=aNone;g_state=kIdle;g_req=1;NativeGateRetryTick(s,200);assert(!posts);
 g_req=0;g_pend=1;NativeGateRetryTick(s,201);assert(!posts);
 puts("PASS native gate retry: complete return only, normal gates, once, new action/attempt/session/quarantine/timeout cancellation, busy requests");
}
'''.replace('@BODY@',fn('void NativeGateRetryTick('))
# Actions must use guarded return, and game thread revalidates identity before arming retry.
assert 'Post(kReqExit, -1, "native Proceed gate")' in s
assert 'Post(kReqNativeAgain, row, "native Proceed gate")' in s
assert 'g_attempt.row != row' in s and 'g_rows[(size_t)row].missionId != NativeGateMission()' in s
assert 'if (!rc && kind == kReqNativeAgain)' in s
with tempfile.TemporaryDirectory() as d:
 p=Path(d);src=p/'test.cpp';exe=p/'test.exe';src.write_text(code)
 subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
