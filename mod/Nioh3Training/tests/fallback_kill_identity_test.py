"""Compile actual lookup, confirmed kill and TrainTick; catalogue ids cannot target dynamic actors."""
from pathlib import Path
import runpy,subprocess,sys,tempfile
b=runpy.run_path(str(Path(__file__).with_name('fallback_sequence_test.py')));fn=b['fn'];s=b['s']
pre=b['preamble'].replace('int bodyGone;} g_tr;', 'int bodyGone,active,nodeath,beaten,lost,practice,round;DWORD tBeat;} g_tr;')
pre+=r'''
static struct {int active;uint32_t id;DWORD t0;} g_bm;
static uint64_t g_sp_actor[1];static int g_nsp;
static unsigned char actor[32];static int dead;
static int Actors(uint64_t*out,int max){out[0]=(uint64_t)actor;return 1;}
static int ActorDeadNow(uint64_t a,int*d,uint64_t*c,uint64_t*m){*d=dead;*c=dead?0:100;*m=100;return dead;}
static int ActorHp(uint64_t a,uint64_t*c,uint64_t*m){*c=dead?0:100;*m=100;return 1;}
static void BgmEnd(void){}
static void BossModeEnd(const char*s){}
static void GaugeHideOnKill(uint64_t a,const char*s){}
static void FakeGaugeHide(const char*s){}
static uint64_t g_ph_native_death_actor,g_nd_char;
static int g_tr_nodeath_now,g_cfg_cantdie,g_cfg_beat_ms,g_cfg_autoretry,g_cfg_gate=1,g_cfg_tr_win,g_cfg_body_fade,g_cfg_body_delay;
static DWORD g_nd_try;
static int TrNoDeath(void){return 0;}
static uint64_t PlayerChar(void){return 0;}
static const unsigned char* GameState(void){return 0;}
static void PlayerSetNoDeath(int x){}
static int PlayerHp(uint64_t*c,uint64_t*m){*c=*m=100;return 1;}
#define PH_NONE 0
#define PH_REFILL 1
#define PH_BEATEN 2
static int PhDecide(int a,int b,int c,int d,uint64_t hp,uint64_t max){return PH_NONE;}
static void PlayerRefill(uint64_t c,uint64_t m){}
static void TrainRetry(const char*s){}
static void TrainLost(void){}
static void KillFadeDue(void){}
static int wonEnds; static void TrainWonBodyGone(void){wonEnds++;}
static int FadeOutActor(int i,float f){return 1;}
static int Despawn(int i){return 1;}
static DWORD FadeDeleteMs(void){return 10000;}
'''
main=r'''
int main(void){
 // Actual pc2 log pairs: catalogue placement vs CreateDynamic returned handle.
 const uint32_t ids[][2]={{1071509,269505520},{1047415,269475499},{1065308,269496227},{1088859,269515497},{353508,268786927},{573339,269010402}};
 for(unsigned i=0;i<6;i++){
  memset(&g_fb,0,sizeof g_fb);memset(&g_tr,0,sizeof g_tr);memset(&g_np,0,sizeof g_np);
  g_dead_at=g_dead_id=0;dead=finishes=0;g_fl.open=0;tick=5000;
  *(uint32_t*)(actor+4)=ids[i][1];g_sp_actor[0]=(uint64_t)actor;g_nsp=1;
  g_bm.active=1;g_bm.id=ids[i][1];g_bm.t0=100;g_tr.active=1;g_tr.tCmd=100;g_tr.round=1;
  // Exactly the lookup used by killid: wrong catalogue handle returns before kill.
  assert(!ActorByHandle(ids[i][0]));assert(ActorByHandle(ids[i][1])==(uint64_t)actor);
  TrainTick();assert(!finishes&&!g_dead_id); // no hit actually delivered: no win is correct
  BossKilledNow((uint64_t)actor,"not dead yet");TrainTick();assert(!finishes);
  // Same confirmed death path uses the actual dynamically returned handle.
  dead=1;BossKilledNow((uint64_t)actor,"confirmed ordinary death");
  assert(g_dead_id==ids[i][1]&&!g_bm.active);TrainTick();assert(finishes==1&&result==FL_WIN&&g_tr.tWin==tick);
  TrainTick();assert(finishes==1);
 }
 puts("PASS six actual fallback identity pairs: catalogue kill misses; confirmed live dynamic kill -> exactly one TrainTick win");
}
'''
cmd=s[s.index('} else if (!strcmp(op, "killid"))'):s.index('} else if (!strcmp(op, "fnowchr"))')]
assert 'ActorByHandle((uint32_t)a[0])' in cmd and 'no actor with id' in cmd and 'return;' in cmd
assert 'g_dead_id = g_bm.id' in fn('static __attribute__((noinline)) void BossTick(void)')
assert 'g_watch_id = r' in fn('static void ArenaTick(void)')
assert 'BossModeBegin(r, g_cfg_notify)' in fn('static void ArenaTick(void)')
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);src=p/'identity.c';exe=p/'identity.exe'
 names=['static int FallbackMulti(', 'static int RosterDefeat(', 'static int RosterNextIndex(', 'static int FallbackDedupe(void) {', 'static int FightFinish(', 'static int FallbackNext(', 'static void FallbackSpawn(', 'static uint64_t ActorByHandle(uint32_t h) {', 'static void BossKilledNow(uint64_t a, const char* why) {', 'static void TrainTick(void)']
 src.write_text(pre+b['header']+b['state']+'\n'+''.join(fn(x) for x in names)+main)
 subprocess.run([sys.executable,'-m','ziglang','cc','-O2','-UNDEBUG',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
