#ifdef NDEBUG
#undef NDEBUG
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint32_t DWORD;
#define N3T_SLOTS 2
static uint8_t actor[0xB000], other[0xB000];
static uint64_t stats[16],g_slot_act[2],g_kill_act,g_fin_live,player;
static int owned=1,dead,readable=1;
static DWORD now=10;
static uint64_t PlayerChar(void){return player;}
static int InSp(uint64_t a){return owned && a==(uint64_t)actor;}
static int Rd64(uint64_t p,uint64_t* out){if(!p || !readable)return 0;memcpy(out,(void*)p,8);return 1;}
static int ActorDeadNow(uint64_t a,int* d,uint64_t* hp,uint64_t* mx){(void)a;*d=dead;*hp=dead?0:100;*mx=100;return dead;}
static DWORD GetTickCount(void){return now;}
#define Log(...) ((void)0)
#include "../src/hazard_guard.h"
static int native_calls,rewards,deaths;static uint64_t args[9];
static int native_early;
static uint64_t NativeUpdate(uint64_t a,uint64_t context){
 assert(context==77);++native_calls;
 if(native_early) return 123; // 4032C4 true: returns before the latch is recalculated
 // Native AF910: reward/death happen independently of a blocked HP subtraction.
 if(*(uint8_t*)(a+0x89B) && !*(uint8_t*)(a+0x89A)){++deaths;++rewards;}
 *(uint8_t*)(a+0x89A)=*(uint8_t*)(a+0x89B);return 123;
}
static uint64_t NativeKill(uint64_t c,uint64_t a,uint64_t b,uint64_t d,uint64_t e,uint64_t f,uint64_t g,uint64_t h,uint64_t i){
 uint64_t v[9]={c,a,b,d,e,f,g,h,i};memcpy(args,v,sizeof v);++deaths;++rewards;return 456;
}
static uint8_t gs[3];
static struct { int active; } g_tr;
static struct { uint32_t id; } g_bm;
static uint64_t g_sp_actor[2];static int g_nsp=1, g_slot_sp_ok[2],placed,place_ok=1,quit,retry,despawn;
static float g_slot_sp[2][4];
static const uint8_t* GameState(void){return gs;}
static uint64_t ActorByHandle(uint32_t id){(void)id;return g_sp_actor[0];}
#define FL_QUIT 0
static void FightFinish(int why){assert(why==FL_QUIT);++quit;}
static void TrainRetry(const char* why){(void)why;++retry;}
static void Despawn(int i){assert(i==0);++despawn;}
static int BossPlace(uint64_t a,const float* p,float yaw,const char* why){(void)a;(void)p;(void)yaw;(void)why;++placed;return place_ok;}
#include "../src/hazard_recovery_tick.h"
int main(void){
 uint64_t a=(uint64_t)actor,c=a; *(uint64_t*)(actor+0x3A0)=(uint64_t)stats;
 g_slot_act[0]=a;g_hg_update=NativeUpdate;g_hg_kill=NativeKill;
 // Early exit on a just-spawned actor: no hazard, prior latch kept (#300 Ally spawn loop).
 native_early=1;assert(HgUpdate(a,77)==123 && !g_hg[0].pending && !actor[0x89A]);
 actor[0x89A]=1;HgUpdate(a,77);assert(!g_hg[0].pending && actor[0x89A]==1);actor[0x89A]=0;native_early=0;native_calls=0;
 actor[0x89B]=1;assert(HgUpdate(a,77)==123);assert(native_calls==1 && !deaths && !rewards && g_hg[0].pending);
 DWORD first=g_hg[0].since;now=100;HgUpdate(a,77);assert(g_hg[0].since==first); // repeated hazard cannot extend timeout
 assert(HgMovementKill((uint64_t)&c,1,2,3,4,5,6,7,8)==0 && !deaths);
 ++g_hg_generation[0];HgMark(0,a);assert(g_hg[0].since==100 && g_hg[0].generation==1);
 g_kill_act=a;assert(HgMovementKill((uint64_t)&c,1,2,3,4,5,6,7,8)==456 && deaths==1);
 for(int i=1;i<9;i++) assert(args[i]==(uint64_t)i);
 g_fin_live=a;g_kill_act=0;assert(HgSlot(a)==-1);g_fin_live=0;dead=1;HgMovementKill((uint64_t)&c,1,2,3,4,5,6,7,8);assert(deaths==2);dead=0;
 player=a;HgMovementKill((uint64_t)&c,1,2,3,4,5,6,7,8);assert(deaths==3);player=0;
 owned=0;actor[0x89A]=0;HgUpdate(a,77);assert(deaths==4);owned=1;
 c=(uint64_t)other;HgMovementKill((uint64_t)&c,1,2,3,4,5,6,7,8);assert(deaths==5);
 readable=0;assert(HgSlot(a)==-1);
 readable=1;c=a;g_sp_actor[0]=a;g_slot_sp_ok[0]=1;HgMark(0,a);HgTick();assert(placed==1 && !g_hg[0].pending);
 place_ok=0;HgMark(0,a);HgTick();assert(placed==2 && g_hg[0].pending);
 now+=2001;g_tr.active=1;HgTick();assert(quit==1 && retry==1 && !g_hg[0].pending);
 g_tr.active=0;HgMark(0,a);now+=2001;HgTick();assert(despawn==1);
 HgMark(0,a);++g_hg_generation[0];HgTick();assert(!g_hg[0].pending && placed==2);
 HgMark(0,a);gs[2]=1;HgTick();assert(!g_hg[0].pending);gs[2]=0;
 // A successful teleport into the same hazard is bounded too.
 place_ok=1;memset(g_hg,0,sizeof g_hg);int before=despawn;
 for(int i=0;i<5;i++){HgMark(0,a);HgTick();}
 assert(despawn==before+1);
 HgMark(0,a);dead=1;HgTick();assert(!g_hg[0].pending && retry==1);dead=0;
 puts("hazard wrappers and recovery PASS: native reward bypass prevented, native return/9 arguments preserved, player/foreign/dead/cleanup pass through, generation and bounded pending");
}
