"""Compile actual SelfTestTick and ClearedNow; mocked observations, no game."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/rematch.cpp').read_text()


def function(sig):
    start = source.index(sig)
    begin = source.index('{', start)
    end, depth = begin + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


struct = source[source.index('struct SelfTest {'):source.index('void SelfTestTick(')]
code = r'''
#include <atomic>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
using ULONGLONG=unsigned long long;
namespace rcore {enum {kR,kH,kD,kE,kX}; enum {kEArmed,kEIn};}
struct Row {const char* id; int route; uint16_t missionId; uint32_t storyMission;};
std::vector<Row> g_rows={{"E01",rcore::kE,0,0xcc96},{"E03",rcore::kE,0,0xcc96},
 {"E05",rcore::kE,0,0xcc96},{"E06",rcore::kE,0,0xcc96},{"R1234",rcore::kR,0x1234,0x1234},
 {"Xbad",rcore::kX,0,0}};
struct Snap {bool msOk=true,emakiOk=true,emaki=false,player=true,online=false,guest=false;
 uint32_t ms28=0; int st[6]={};} snap;
enum {kIdle,kEmaki,kInReplay};
enum {kFnIdle,kFnArmed,kFnWarped,kFnPass,kFnFail};
enum {kReqFight,kReqEmaki,kReqExit,cCyan};
std::atomic<int> g_state{kIdle},g_fn{kFnIdle};
std::atomic<bool> g_scrollNoWaves{false};
struct {int st=rcore::kEArmed;} g_et;
struct {bool seenFight=false;} g_wavePlace;
int g_eRow=-1,g_warps=0;
struct {const char* last="none";} g_sp;
bool effects=true,readable=true,cleared=true,g_clearGuardOk=true;
struct {bool ignoreProgress=false;} g_cfg;
uint16_t g_bypassId=0;
constexpr uint64_t G_CLEAR=1;
int reads=0,clearCalls=0,posts=0,lastKind=-1,lastRow=-1;
bool Rd(uint64_t,uint64_t& out){++reads;out=2;return readable;}
bool CallCleared(uint64_t,uint16_t id,uint8_t& c){++clearCalls;assert(id);c=cleared;return readable;}
Snap ReadSnap(){return snap;}
const char* Gates(const Snap&s,bool){return s.online||s.guest||s.st[2]?"blocked":nullptr;}
bool AttemptEffectsAllowed(){return effects;}
void Post(int kind,int row,const char*){++posts;lastKind=kind;lastRow=row;}
void PlayerPos(float*){}
template<class... T>void Log(T...){}
template<class... T>void ToastF(T...){}
'''
code += function('bool ClearedNow(') + struct + function('void SelfTestTick(')
code += r'''
void reset(std::vector<int> rows={0}){
 g_st={};g_st.rows=rows;snap={};g_state=kIdle;g_fn=kFnIdle;g_et.st=rcore::kEArmed;
 g_eRow=-1;g_wavePlace.seenFight=false;g_scrollNoWaves=false;effects=true;
 reads=clearCalls=posts=0;lastKind=lastRow=-1;cleared=readable=g_clearGuardOk=true;g_cfg.ignoreProgress=false;
}
void request(){SelfTestTick(snap,1000);SelfTestTick(snap,9000);}
void wave(int row=0){g_state=kEmaki;g_eRow=row;g_et.st=rcore::kEIn;
 snap.emaki=true;snap.ms28=0xcc96;g_wavePlace.seenFight=true;}
int main(){
 const char* why=nullptr;
 reset();g_cfg.ignoreProgress=true;g_bypassId=55;assert(!ClearedNow(0,&why,true));
 assert(!reads&&!clearCalls&&!g_bypassId&&strstr(why,"no replay mission id"));
 cleared=false;snap.ms28=0x1234;assert(!ClearedNow(0x1234,&why,true));assert(strstr(why,"right now"));
 snap.ms28=0;assert(ClearedNow(0x1234,&why,true));assert(g_bypassId==0x1234);
 // Explicit E01/E03/E05/E06 sweep: no replay clear call, native scroll request,
 // first wave rather than a boss bar, normal exit, then next row.
 reset({0,1,2,3});ULONGLONG t=1000;
 for(int row=0;row<4;++row){
  SelfTestTick(snap,t);t+=8000;SelfTestTick(snap,t);
  assert(lastKind==kReqEmaki&&lastRow==row&&g_st.phase==1&&!clearCalls);
  g_fn=kFnPass;SelfTestTick(snap,t+1);assert(g_st.pass==row); // stale boss PASS cannot pass scroll
  wave(row);g_wavePlace.seenFight=false;SelfTestTick(snap,t+2);assert(g_st.phase==1);
  g_wavePlace.seenFight=true;SelfTestTick(snap,t+3);assert(g_st.pass==row+1&&g_st.phase==2);
  SelfTestTick(snap,t+4003);assert(lastKind==kReqExit);
  g_state=kIdle;snap={};SelfTestTick(snap,t+4004);assert(g_st.phase==3);
  SelfTestTick(snap,t+4005);assert(g_st.next==size_t(row+1));t+=5000;
 }
 SelfTestTick(snap,t);assert(g_st.phase==4&&g_st.pass==4&&!g_st.skip&&!g_st.fail);
 // Fresh first-wave evidence must belong to the requested row/mission, and
 // survive native admission/read/online/no-waves guards.
 for(int bad=0;bad<10;++bad){
  reset();request();wave();
  switch(bad){case 0:g_eRow=1;break;case 1:snap.ms28=1;break;case 2:snap.msOk=false;break;
   case 3:snap.emakiOk=false;break;case 4:snap.st[2]=1;break;case 5:snap.online=true;break;
   case 6:g_scrollNoWaves=true;break;case 7:effects=false;break;case 8:g_et.st=rcore::kEArmed;break;
   case 9:snap.emaki=false;break;}
  SelfTestTick(snap,10000);assert(!g_st.pass&&g_st.phase==1);
 }
 reset();request();g_fn=kFnArmed;SelfTestTick(snap,25001);assert(g_st.fail==1); // rejected request, stale g_fn ignored
 reset();request();wave();g_wavePlace.seenFight=false;SelfTestTick(snap,309001);assert(g_st.fail==1&&g_st.phase==2);
 reset({4});request();assert(lastKind==kReqFight&&clearCalls==1);g_state=kInReplay;g_fn=kFnPass;
 SelfTestTick(snap,10000);assert(g_st.pass==1); // replay behavior retained
 reset({4});cleared=false;request();assert(g_st.skip==1&&!posts);
 reset({5});request();assert(g_st.skip==1&&!posts&&!clearCalls);
 reset();snap.online=true;request();assert(!posts);
 puts("PASS actual SelfTestTick/ClearedNow: 4-scroll sweep, first-wave ownership, 10 negative gates, refusal/timeout/exit, replay and zero-id guards");
}
'''
# Route reaches the existing guarded scroll implementation; latch is reset at
# admission and set only when native wave-manager state 3 is observed.
assert 'else if (kind == kReqEmaki) ExecEmaki(row, t);' in source
assert 'g_wavePlace = {};' in function('void ExecEmaki(')
assert 'if (ok && st == 3)' in function('void WavePlaceTick(')
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp)
    (p/'test.cpp').write_text(code)
    subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-UNDEBUG',
                    str(p/'test.cpp'),'-o',str(p/'test.exe')],check=True)
    subprocess.run([str(p/'test.exe')],check=True)
