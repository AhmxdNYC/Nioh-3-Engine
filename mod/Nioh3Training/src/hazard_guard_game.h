#include "hazard_guard.h"
static int g_hg_installed;
static void HgInstall(void) {
    if(g_hg_installed) return;
    g_hg_installed=-1;
    // These are engine integration sites, not actor/caller classification lists:
    // AF910's only call, both native movement world-kill branches targeting the moving actor, and the fall damage call.
    const uintptr_t rv[4]={0xB084B,0x4044F6,0x404540,0x678636};
    uint8_t* sites[4];
    for(int i=0;i<4;i++) {
        uintptr_t r=Grva(rv[i]),target=Grva(i ? 0x786C84 : 0xAF910);
        if(!r || !target) {Log("HAZARD guard unavailable: unresolved native site");return;}
        sites[i]=(uint8_t*)(B+r);
        if(sites[i][0]!=0xE8 || (uintptr_t)(sites[i]+5)+*(int32_t*)(sites[i]+1)!=B+target) {
            Log("HAZARD guard unavailable: native call %llX differs",(unsigned long long)rv[i]);return;
        }
    }
    uint8_t* stub=NearPage(sites[0]); if(!stub) {Log("HAZARD guard allocation failed");return;}
    uint8_t* p=E_mov_rax(stub,(uint64_t)HgUpdate);*p++=0xFF;*p++=0xE0;
    p=E_mov_rax(stub+16,(uint64_t)HgMovementKill);*p++=0xFF;*p++=0xE0;
    p=E_mov_rax(stub+32,(uint64_t)HgFallDamage);*p++=0xFF;*p++=0xE0;
    FlushInstructionCache(GetCurrentProcess(),stub,48);
    g_hg_update=(HgUpdateFn)(B+Grva(0xAF910));g_hg_kill=(HgKillFn)(B+Grva(0x786C84));
    MemoryBarrier();
    int mask=0;
    for(int i=0;i<4;i++) {
        DWORD old; if(!VirtualProtect(sites[i],5,PAGE_EXECUTE_READWRITE,&old)) continue;
        int32_t rel=(int32_t)((intptr_t)(stub+(i==3 ? 32 : i ? 16 : 0))-(intptr_t)(sites[i]+5));
        InterlockedExchange((volatile LONG*)(sites[i]+1),rel);
        VirtualProtect(sites[i],5,old,&old);FlushInstructionCache(GetCurrentProcess(),sites[i],5);mask|=1<<i;
    }
    g_hg_installed=mask==15 ? 1 : -1;
    Log("HAZARD guard native world-death sites mask %X / F (combat damage untouched)",mask);
}
static void HgNew(int s) {
    ++g_hg_generation[s];memset(&g_hg[s],0,sizeof g_hg[s]);HgInstall();
}
#include "hazard_recovery_tick.h"
