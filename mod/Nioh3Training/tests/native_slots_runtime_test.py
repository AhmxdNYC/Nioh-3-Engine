"""Run production native observation/ownership logic against fake actor memory."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/training.c').read_text()
def body(sig):
 a=s.index(sig+' {'); b=s.index('{',a); d=1;i=b+1
 while d:
  d+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
preamble=r'''
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define N3T_SLOTS 3
#define Log(...) ((void)0)
static uint64_t g_slot_act[3],g_sp_actor[3]; static uint32_t g_slot_chr[3],g_slot_native[3],g_slot_mission[3];
static int g_nsp,g_slot_ih[3],g_slot_sp_ok[3]; static LONG g_slot_fight[3],g_slot_rehold[3],g_slot_aggro[3],g_bv_req[3],g_bm_gen;
static DWORD g_bv_kiWait[3]; static struct {int live;} g_bv_live[3]; static struct{int phase,nOff; uint64_t a;}g_sph[3]; static struct{int active;}g_tr;
static uint8_t gs[3]; static uint32_t mission=1,record[2]={44,1234}; static uint64_t wrapper[4],actor=123;static int visible=1,dead,removed,despawns;
static uint64_t hp=50,maxhp=100;
static int ai_on=1,ai_restores;
static int BhAiOn(uint64_t a){return ai_on;}
#define NATIVE_SLOT_AI_ON(a) (ai_on=1,++ai_restores)
static const uint8_t* GameState(void){return gs;} static uint64_t PlayerChar(void){return 9;}
static int MsRead(uint32_t*m,uint8_t*j,uint32_t*o){*m=mission;*j=0;*o=0;return 1;}
static int ActorInTable(uint64_t a){return a==actor&&!removed;}
static uint64_t FindWrapper(uint32_t h){return h==44?(uint64_t)wrapper:0;}
static int Rd64(uint64_t a,uint64_t*out){*out=*(uint64_t*)a;return 1;}
static uint64_t NativeEntryActor(uint32_t h,uint32_t c){return h==44&&c==1234?actor:0;}
static int ActorDeadNow(uint64_t a,int*d,uint64_t*h,uint64_t*m){*d=dead;*h=hp;*m=maxhp;return dead||!hp;}
static int InSp(uint64_t a){for(int i=0;i<g_nsp;i++)if(g_sp_actor[i]==a)return 1;return 0;}
static int SlotOf(uint64_t a){for(int i=0;i<3;i++)if(g_slot_act[i]==a)return i;return -1;}
static int NativeSlotAlive(int s);static int SlotAlive(int s);
static int SlotFree(void){for(int i=0;i<3;i++)if(!SlotAlive(i))return i;return -1;}
static int KbCollect(uint64_t*a,int n,uint32_t*h){if(!visible)return 0;*a=actor;*h=44;return 1;}
static void Despawn(int i){despawns++;g_sp_actor[i]=0;}
'''
code=preamble+'\n#include "'+(root/'src/boss_vitals.h').as_posix()+'"\nstatic BvCfg g_bv[3];static void BvInit(void){}\n'
# Avoid selecting the early prototype.
a=s.index('static int SlotAlive(int s) {');code+=body(s[a:s.index('{',a)].strip())+'\n'
code+='#include "'+(root/'src/native_slots_game.h').as_posix()+'"\n'
code+=body('static void SlotDespawn(int s)')
code+=r"""
#define N3T_API
#define MAXSP 3
static LONG g_rq_op,g_rq_slot;static uint32_t g_rq_mission,g_rq_chr;static struct{int active;}g_sp;
static int g_sp_slot_next,g_practice_next,summons;
static const char* SummonTok(uint32_t m){return "test";}
static int SummonStart(const char*t,float d){summons++;return 1;}
static int Nioh3Training_Summon(uint32_t m){return 1;}
static void SumNoDeathRelease(const char*s){}
static void DespawnAll(const char*s){for(int i=0;i<g_nsp;i++)if(g_sp_actor[i])Despawn(i);g_nsp=0;}
"""
for sig in ('N3T_API int Nioh3Training_SummonSlot(int slot, uint32_t mission)',
            'N3T_API int Nioh3Training_SummonSlotChr(int slot, uint32_t chr)',
            'N3T_API int Nioh3Training_DespawnSlot(int slot)', 'static void RqTick(void)'):
 code+=body(sig)+'\n'

code+=r'''
int main(void){wrapper[3]=(uint64_t)record;
 g_bv[0].infHp=1;g_bv_req[0]=3;g_slot_rehold[0]=1;
 NativeSlotsTick();assert(SlotAlive(0)&&g_slot_chr[0]==1234&&NativeSlotsActive());
 assert(g_nsp==0&&g_slot_fight[0]==1&&!g_slot_aggro[0]&&!g_slot_rehold[0]);
 assert(!g_bv[0].infHp&&!g_bv[0].infKi&&!g_bv_req[0]&&hp==50&&maxhp==100);
 SlotDespawn(0);assert(SlotAlive(0)&&!despawns);
 assert(!Nioh3Training_SummonSlot(0,5)&&!Nioh3Training_SummonSlotChr(0,55)&&!Nioh3Training_DespawnSlot(0));
 g_rq_op=1;g_rq_slot=0;RqTick();assert(!summons&&SlotAlive(0)); // queued-before-adoption race
 g_rq_op=2;RqTick();assert(SlotAlive(0)&&!despawns); // Remove all preserves borrowed native
 assert(Nioh3Training_SummonSlot(-1,5)&&g_rq_slot==1);g_rq_op=0;
 g_bv[0].infHp=1;NativeSlotsTick();assert(g_bv[0].infHp); // explicit choice survives observation
 g_sph[0].phase=1;g_sph[0].nOff=1;g_sph[0].a=999;ai_on=0;
 NativeSlotReleaseHold(0);assert(!ai_restores); // foreign hold never released
 g_sph[0].a=actor;g_sph[0].nOff=0;NativeSlotReleaseHold(0);assert(!ai_restores);
 g_sph[0].nOff=1;ai_on=1;NativeSlotReleaseHold(0);assert(!ai_restores);
 ai_on=0;
 visible=0;NativeSlotsTick();assert(ai_restores==1&&ai_on&&!g_slot_act[0]);
 visible=1;NativeSlotsTick();
 g_sph[0].phase=1;g_sph[0].nOff=1;g_sph[0].a=actor;ai_on=0;
 gs[2]=1;assert(!SlotAlive(0));NativeSlotsTick();assert(!g_slot_act[0]);gs[2]=0;assert(ai_restores==1);
 NativeSlotsTick();g_sph[0].phase=1;g_sph[0].nOff=1;g_sph[0].a=actor;mission=2;assert(!SlotAlive(0));visible=0;NativeSlotsTick();assert(!g_slot_act[0]);
 visible=1;NativeSlotsTick();g_sph[0].phase=1;g_sph[0].nOff=1;g_sph[0].a=actor;dead=1;assert(!SlotAlive(0));NativeSlotsTick();assert(!g_slot_act[0]);dead=0;
 NativeSlotsTick();g_sph[0].phase=1;g_sph[0].nOff=1;g_sph[0].a=actor;removed=1;assert(!SlotAlive(0));NativeSlotsTick();assert(!g_slot_act[0]);removed=0;
 NativeSlotsTick();actor=456;assert(!SlotAlive(0));NativeSlotsTick();assert(g_slot_act[0]==456);
 hp=0;assert(SlotAlive(0));NativeSlotsTick();assert(SlotAlive(0));hp=50;
 visible=0;NativeSlotsTick();assert(!g_slot_act[0]); // dormant/next-wave actors not admitted
 g_nsp=1;g_sp_actor[0]=789;g_slot_act[0]=789;g_slot_chr[0]=55;assert(SlotAlive(0));
 SlotDespawn(0);assert(despawns==1&&!g_slot_act[0]);
 assert(ai_restores==1);
 puts("PASS native observation: identity/load/death/HUD expiry, no implicit stats, native despawn refusal, owned despawn");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(code);exe=Path(td)/'test.exe'
 subprocess.run([sys.executable,'-m','ziglang','cc','-target','x86_64-windows-gnu','-fms-extensions','-Xclang','-fasync-exceptions',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
