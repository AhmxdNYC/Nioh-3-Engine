"""Execute production menu route, clear admission, arming and clear veto offline."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/rematch.cpp').read_text()
def fn(sig):
 a=s.index(sig);b=s.index('{',a);n=1;i=b+1
 while n:n+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
pre=r'''
#include <atomic>
#include <vector>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <cassert>
using ULONGLONG=unsigned long long;
struct Row {uint16_t missionId;const char* name;int route=0;};
namespace rcore {constexpr int kE=4; inline bool WavesRow(const Row&){return false;}}  // ActBossRoute now sends scroll rows to StartScrollRecord; this fixture has none
static bool StartScrollRecord(int,int,bool){assert(false);return false;}
std::vector<Row> g_rows={{123,"test boss"}};
std::atomic<int> g_status[1];
struct {bool ignoreProgress=false;} g_cfg;
struct Snap {bool msOk=true;uint32_t ms28=0;};
Snap snap;
Snap ReadSnap(){return snap;}
static bool readable=true,cleared=false,g_clearGuardOk=true,available=true,training=true,busy=false;
static bool g_hostedUiAction=false;
static int g_confirm=-9,posts,requests,trains,practice=-1;
static char g_msg[160];
constexpr int kMaxRows=512;
constexpr int kItFight=0x10000,kItShrine=0x20000,kItBossTrainHere=0x38000,kItTeleport=0x40000;
constexpr int kReqFight=1,kReqShrine=2,cGreen=1,cWhite=2;
static uint16_t g_bypassId;
static std::atomic<unsigned> g_unbeaten,g_unbeatenSeen,g_unbeatenVetoes;
static uint8_t g_unbeatenClear0;static ULONGLONG g_unbeatenAt;
static uint32_t g_runningMission;
constexpr int G_CLEAR=1,G_MS=2;
static ULONGLONG Ms(){return 1;}
static uint8_t ClearByte(uint16_t){return cleared?1:0;}
static bool Rd(uint64_t p,uint64_t&v){if(p!=G_CLEAR||!readable)return false;v=3;return true;}
static bool Rd(uint64_t,uint32_t&){return false;}
static bool CallCleared(uint64_t,uint16_t,uint8_t&c){c=cleared?1:0;return readable;}
namespace fight {void ConfirmNativeWin(uint32_t){}}
template<class... A> void Log(A...){}
template<class... A> void ToastF(A...){}
static const char* Gates(const Snap&,bool){return busy?"busy/online/loading":nullptr;}
static const char* CanArena(int){return available?nullptr:"route unavailable";}
static const char* CanShrine(int r){return CanArena(r);}
static int Train(uint32_t m){assert(m==123);++trains;return 1;}
static void* TrainingExport(const char*){return training?(void*)&Train:nullptr;}
static bool TrainingSwapAvailable(){return true;}
static void BeginHostedFight(int,void*,char*,size_t){assert(false);}
static void SetPracticeNext(int p){practice=p;}
static void Post(int,int,const char*);
'''
code='\n'.join(fn(x) for x in ['bool ClearedNow(', 'void UnbeatenArm(', 'bool VetoClear(', 'bool TrainHereOk(', 'const char* CanMatchArena(', 'bool ActBossRoute('])
main=r'''
// Request stub executes the actual game-thread admission + arming functions;
// source guards below ensure ExecDirect invokes them in this order before native request.
static void Post(int kind,int row,const char*){
 ++posts;const char*why=nullptr;
 if(!ClearedNow(g_rows[row].missionId,&why,kind==kReqFight))return;
 if(kind==kReqFight && g_bypassId==g_rows[row].missionId)UnbeatenArm(g_rows[row].missionId,"row");
 ++requests;
}
static void reset(int status,bool clear=false){
 g_status[0]=status;cleared=clear;readable=g_clearGuardOk=available=training=true;
 busy=false;g_confirm=-9;posts=requests=trains=0;g_unbeaten=0;g_unbeatenVetoes=0;g_bypassId=0;snap={};practice=-1;
}
static void click(int item){assert(!ActBossRoute(item));ActBossRoute(item);}
int main(){
 for(int status:{0,1,2}){
  reset(status,status==2);click(kItFight);assert(requests==1&&posts==1&&!trains&&practice==0);
  assert(g_unbeaten==(status==2?0u:123u));g_runningMission=123;
  assert(VetoClear(0)==(status!=2));assert(g_unbeatenVetoes==(status==2?0u:1u));
  g_runningMission=456;assert(!VetoClear(0));
  reset(status);click(kItBossTrainHere);assert(trains==1&&!posts&&!requests);
  reset(status);available=false;click(kItFight);assert(trains==1&&!posts);
  reset(status);busy=true;click(kItFight);assert(!posts&&!trains);
  reset(status);readable=false;click(kItFight);assert(!requests&&!trains);
 }
 reset(2,false);click(kItFight);assert(requests==1&&g_unbeaten==123); // stale cleared cache cannot skip protection
 reset(0);g_clearGuardOk=false;click(kItFight);assert(trains==1&&!requests);
 reset(2,false);g_clearGuardOk=false;click(kItFight);assert(!requests&&!trains); // live guard rechecked
 reset(0);snap.ms28=123;click(kItFight);assert(!requests&&!trains); // active unbeaten story refused
 reset(0);click(kItShrine);assert(!requests&&!trains);
 reset(0);training=false;available=false;click(kItFight);assert(!requests&&!trains);
 reset(0);assert(!ActBossRoute(kItFight));available=false;assert(!ActBossRoute(kItFight));assert(!trains);assert(ActBossRoute(kItFight)&&trains==1); // changed action needs new confirmation
 const char* why=nullptr;reset(0);assert(!ClearedNow(123,&why)); // ordinary replay is not globally unlocked
 puts("PASS menu statuses 0/1/2: native Fight now, explicit Train here, runtime fallback, solo/busy gate, unreadable DB, missing guard, stale status, active story, shrine, clear veto, changed-action confirmation");
}
'''
ui=fn('bool UiAct(');assert 'if (item >= kItFight) return ActBossRoute(item);' in ui
items=fn('int Items(');assert 'add(kItFight + g_uiBoss);\n            add(kItBossTrainHere + g_uiBoss);' in items
ex=fn('void ExecDirect(')
assert 'ClearedNow(r.missionId, &why, fight && warp)' in ex
assert ex.index('ClearedNow(')<ex.index('UnbeatenArm(')<ex.index('CallRequestById(')
assert 'AttemptStartGate()' in ex and 'TrainingEntryAdmission entryAdmission' in ex
assert 'if (fight && g_bypassId == r.missionId) UnbeatenArm' in ex
assert 'if (s.msOk && s.ms28 == id)' in fn('bool ClearedNow(')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(pre+code+main)
 subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-UNDEBUG',str(p/'test.cpp'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
