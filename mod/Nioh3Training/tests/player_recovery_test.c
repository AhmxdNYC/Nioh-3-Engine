#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static int g_cfg_cantdie=1, hazard, native_ok=1, hp_ok=1, warp_ok=1, recovers, warps;
static uint64_t player=1,hp=10,mx=100,g_ph_hazard_actor;
static uint8_t gs[3];
static struct { int active,hasStart; float pp[4]; } g_tr;
static float native_p[4]={1,2,3,4}, actual[4];
static uint64_t PlayerChar(void){return player;}
static const uint8_t* GameState(void){return gs;}
static int PlayerNativeHazard(uint64_t a){return a && hazard;}
static int PlayerHp(uint64_t* c,uint64_t* m){*c=hp;*m=mx;return hp_ok;}
static int PlayerNativeRecoveryPos(float* p){memcpy(p,native_p,sizeof native_p);return native_ok;}
static void PlayerRecoverFull(void){++recovers;hazard=0;}
static int PlayerRecoveryWarp(const float* p){++warps;memcpy(actual,p,sizeof actual);return warp_ok;}
#define Log(...) ((void)0)
#include "../src/player_recovery_tick.h"
int main(void){
 PlayerHazardRecoverTick(); assert(!warps);
 hazard=1; PlayerHazardRecoverTick(); assert(warps==1 && recovers==1 && !g_ph_hazard_actor && actual[1]==2);
 hazard=1;warp_ok=0;PlayerHazardRecoverTick();assert(g_ph_hazard_actor==1 && !hazard);
 warp_ok=1;PlayerHazardRecoverTick();assert(warps==3 && !g_ph_hazard_actor);
 hazard=1;g_cfg_cantdie=0;PlayerHazardRecoverTick();assert(warps==3);
 g_cfg_cantdie=1;gs[2]=1;PlayerHazardRecoverTick();assert(warps==3);
 gs[2]=0;hp=0;PlayerHazardRecoverTick();assert(warps==3);
 hp=10;hp_ok=0;PlayerHazardRecoverTick();assert(warps==3);
 hp_ok=1;native_ok=0;PlayerHazardRecoverTick();assert(warps==3);
 g_tr.active=g_tr.hasStart=1;g_tr.pp[1]=22;PlayerHazardRecoverTick();assert(warps==4 && actual[1]==22);
 hazard=1;g_tr.pp[0]=NAN;PlayerHazardRecoverTick();assert(warps==4);
 hazard=0;g_ph_hazard_actor=99;PlayerHazardRecoverTick();assert(warps==4 && !g_ph_hazard_actor);
 player=0;g_ph_hazard_actor=1;PlayerHazardRecoverTick();assert(warps==4 && !g_ph_hazard_actor);
 puts("player recovery: native point, fallback, failed warp retry, OFF/loading/dead/stale guards PASS");
}
