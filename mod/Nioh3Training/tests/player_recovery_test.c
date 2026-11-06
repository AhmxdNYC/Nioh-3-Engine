#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static int g_cfg_cantdie=1, hazard, native_ok=1, hp_ok=1, warp_ok=1, recovers, warps;
static uint64_t player=1,hp=10,mx=100,g_ph_hazard_actor;
static uint8_t gs[3];
typedef uint32_t DWORD;
static uint64_t g_ph_native_death_actor,g_ph_recovery_player;
static DWORD g_ph_recovery_last,now=10;
static int g_ph_recovery_attempts;
static DWORD GetTickCount(void){return now;}
static void PlayerAllowNativeHazardDeath(uint64_t a){g_ph_native_death_actor=a;}

static struct { int active,hasStart; float pp[4]; } g_tr;
static float native_p[4]={1,2,3,4}, actual[4];
static uint64_t PlayerChar(void){return player;}
static const uint8_t* GameState(void){return gs;}
static int PlayerNativeHazard(uint64_t a){return a && hazard;}
static int PlayerHp(uint64_t* c,uint64_t* m){*c=hp;*m=mx;return hp_ok;}
static int PlayerNativeRecoveryPos(float* p){memcpy(p,native_p,sizeof native_p);return native_ok;}
static void PlayerRecoverFull(void){++recovers;hazard=0;}
static int PlayerRecoveryWarp(const float* p){++warps;memcpy(actual,p,sizeof actual);return warp_ok;}
static int NativeFightRecoverPos(float p[4]){(void)p;return 0;}
#define Log(...) ((void)0)
#include "../src/player_recovery_tick.h"
static void Reset(void){
 g_cfg_cantdie=1;hazard=0;native_ok=hp_ok=warp_ok=1;recovers=warps=0;
 player=1;hp=10;mx=100;g_ph_hazard_actor=g_ph_native_death_actor=g_ph_recovery_player=0;
 g_ph_recovery_attempts=0;g_ph_recovery_last=0;g_ph_giveup=0;g_ph_anchor_dead=0;now=10;memset(gs,0,sizeof gs);memset(&g_tr,0,sizeof g_tr);
}
int main(void){
 Reset();PlayerHazardRecoverTick();assert(!warps);
 hazard=1;PlayerHazardRecoverTick();assert(warps==1 && recovers==1 && !g_ph_hazard_actor && actual[1]==2);
 Reset();hazard=1;warp_ok=0;PlayerHazardRecoverTick();assert(g_ph_hazard_actor==1 && !hazard);
 warp_ok=1;PlayerHazardRecoverTick();assert(warps==2 && !g_ph_hazard_actor);
 Reset();hazard=1;g_cfg_cantdie=0;PlayerHazardRecoverTick();assert(!warps);
 g_cfg_cantdie=1;gs[2]=1;PlayerHazardRecoverTick();assert(!warps);
 gs[2]=0;hp=0;PlayerHazardRecoverTick();assert(!warps);
 hp=10;hp_ok=0;PlayerHazardRecoverTick();assert(!warps);
 Reset();hazard=1;native_ok=0;PlayerHazardRecoverTick();assert(!warps && g_ph_native_death_actor==1);
 Reset();hazard=1;native_ok=0;g_tr.active=g_tr.hasStart=1;g_tr.pp[1]=22;
 PlayerHazardRecoverTick();assert(warps==1 && actual[1]==22);
 Reset();hazard=1;native_ok=0;g_tr.active=g_tr.hasStart=1;g_tr.pp[0]=NAN;
 PlayerHazardRecoverTick();assert(!warps && g_ph_native_death_actor==1);
 Reset();g_ph_hazard_actor=99;PlayerHazardRecoverTick();assert(!warps && !g_ph_hazard_actor);
 player=0;g_ph_hazard_actor=1;PlayerHazardRecoverTick();assert(!warps && !g_ph_hazard_actor);
 Reset();g_tr.active=g_tr.hasStart=1;g_tr.pp[1]=22;
 // Even successful warps cannot loop forever if their destination is hazardous.
 for(int i=0;i<5;i++){hazard=1;PlayerHazardRecoverTick();}
 assert(warps==4 && actual[1]==22 && g_ph_native_death_actor==1 && g_ph_giveup==1 && g_ph_anchor_dead==1);
 PlayerHazardRecoverTick();assert(warps==4);
 gs[2]=1;PlayerHazardRecoverTick();assert(!g_ph_native_death_actor && !g_ph_recovery_attempts);
 puts("player recovery PASS: native point, fallback, bounded attempts, native respawn escape, OFF/loading/dead/stale guards");
}
