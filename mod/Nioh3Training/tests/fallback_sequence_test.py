"""Compile production fallback transitions with game services mocked; no game."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/training.c').read_text()
def fn(signature):
 a=s.index(signature); b=s.index('{',a); depth=1; i=b+1
 while depth:
  depth += (s[i]=='{')-(s[i]=='}'); i+=1
 return s[a:i]+'\n'
start=s.index('static struct { const FallbackRoster* row;')
state=s[start:s.index('\n',start)]
preamble=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
typedef uint32_t DWORD;
#define Log(...) ((void)0)
#define FL_WIN 1
#define FL_LOSS 2
#define FL_QUIT 3
#define FL_NAME 100
#define AcquireSRWLockExclusive(x) ((void)0)
#define ReleaseSRWLockExclusive(x) ((void)0)
typedef int FlClock;
static int g_flLock;
static struct { int open; struct {char name[100];int mode;} rec[1]; } g_fl;
static int finishes,result;
static void FlEnsure(void){}
static void FlNow(FlClock*c){}
static int FlEnd(void*l,int r,DWORD t,FlClock*c){finishes++;result=r;g_fl.open=-1;return 1;}
static const char* FlModeName(int m){return "";}
static const char* FlResultName(int m){return "";}
static DWORD tick=100;
static DWORD GetTickCount(void){return tick;}
static struct {int armed,active,ended,releasing;DWORD pendingAt;} g_np;
static void NativePhaseEvent(int phase) {}
static struct {uint32_t chr,placement;int active;} g_ar;
static uint32_t g_arena_placement,g_dead_id;
static DWORD g_dead_at;
static struct {char tok[32];uint32_t chr;DWORD tCmd,tRespawn,tReport,tWin,tFade,tFadeAt;int bodyGone;} g_tr;
static struct {int active;} g_sp;
static int clears,spawns,duplicates,dedupeCalls;
static uint32_t spawned;
static float g_cfg_tr_dist=100;
static void TrainClear(const char*w){clears++;}
static int SummonStart(const char*t,float d){spawns++;spawned=(uint32_t)strtoul(t,0,0);return 1;}
static int ArenaNativeDuplicate(void){dedupeCalls++;return duplicates;}
'''
main=r'''
static void reset(const char*id){memset(&g_fb,0,sizeof g_fb);g_fb.row=FallbackRosterFind(id);g_tr.chr=g_fb.row?g_fb.row->chr[0]:7;strcpy(g_tr.tok,"RTEST");g_fl.open=0;finishes=clears=spawns=duplicates=dedupeCalls=0;g_dead_id=0;}
int main(void){
 reset("R0034");assert(FallbackMulti()&&g_fb.row->count==2);
 assert(!FightFinish(FL_WIN)&&finishes==0); // native callback cannot end before first kill
 FallbackTracked(100,999);assert(!g_fb.handle);
 FallbackTracked(100,g_tr.chr);assert(g_fb.handle==100);
 g_dead_id=99;assert(FallbackNext(1000)&&g_fb.index==0&&!clears); // unrelated kill ignored
 g_dead_id=100;assert(FallbackNext(1000)&&g_fb.index==1&&clears==1&&!finishes);
 assert(g_tr.chr==g_fb.row->chr[1]&&g_tr.tRespawn==2000&&!g_dead_id&&!g_fb.handle);
 assert(!FightFinish(FL_WIN)&&!finishes); // callback between bosses or before last kill
 duplicates=1;FallbackSpawn();assert(!spawns&&g_tr.tRespawn==tick+100&&dedupeCalls==2);
 duplicates=0;FallbackSpawn();assert(spawns==1&&spawned==g_tr.chr);
 FallbackTracked(200,g_tr.chr);g_dead_id=200;assert(!FallbackNext(4000)&&g_fb.complete);
 assert(FightFinish(FL_WIN)&&finishes==1&&result==FL_WIN);assert(!FightFinish(FL_WIN)&&finishes==1);
 reset("R0034");assert(FightFinish(FL_LOSS)&&g_fb.ended&&result==FL_LOSS);FallbackTracked(100,g_tr.chr);g_dead_id=100;assert(FallbackNext(4000));FallbackSpawn();assert(!spawns&&!g_fb.complete);
 reset("R0034");assert(FightFinish(FL_QUIT)&&g_fb.ended&&result==FL_QUIT);assert(!FightFinish(FL_WIN));
 reset("not-a-multi-row");assert(!FallbackMulti()&&!FallbackNext(1000));assert(FightFinish(FL_WIN)&&finishes==1); // native/single behavior unchanged
 assert(FallbackRosterFind("R0C12")->count==0&&FallbackRosterFind("R945F")->count==0);
 // Every generated complete roster can progress only once per matching tracked kill.
 for(unsigned k=0;k<sizeof(g_fallback_rosters)/sizeof(g_fallback_rosters[0]);++k){
  const FallbackRoster*r=&g_fallback_rosters[k];if(r->count<2)continue;reset(r->id);
  for(unsigned i=0;i<r->count;++i){assert(g_fb.index==i);FallbackTracked(100+i,g_tr.chr);g_dead_id=100+i;
   int more=FallbackNext(1000+i*4000);assert(more==(i+1<r->count));assert(finishes==0);
   if(more){assert(!FightFinish(FL_WIN));FallbackSpawn();assert(spawned==r->chr[i+1]);}}
  assert(FightFinish(FL_WIN)&&finishes==1);
 }
 puts("PASS fallback sequence: all resolved rosters, exact kills, final-only win, loss/quit, dedupe delay, native/single pass-through");
}
'''
header=(root/'src/fallback_rosters_gen.h').read_text()
# Guard actual integration paths as well as extracted helper behavior.
assert 'if (FallbackNext(now)) return;' in fn('static void TrainTick(void)')
assert 'g_fb.index = 0;' in fn('static void TrainRetry(')
assert 'memset(&g_fb, 0, sizeof g_fb);' in fn('static void TrainStop(')
assert 'sequenceRow.chr = roster->chr[0]' in fn('static int FightNowImpl(uint32_t mission) {')
assert 'if (roster && roster->count < 2)' in fn('static int FightNowImpl(uint32_t mission) {')
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);src=p/'sequence.c';exe=p/'sequence.exe'
 src.write_text(preamble+header+state+'\n'+''.join(fn(x) for x in ['static int FallbackMulti(', 'static void FallbackTracked(uint32_t handle, uint32_t chr) {','static int RosterDefeat(', 'static int RosterNextIndex(', 'static int FallbackDedupe(void) {','static int FightFinish(', 'static int FallbackNext(', 'static void FallbackSpawn('])+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
