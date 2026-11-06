// Actual detour + actual install/trampoline; native HP subtraction modeled from
// the offline 787268 disassembly. No game process or game code is executed.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
static uintptr_t B; static uint32_t g_imgSize=0x800000;
static int g_cfg_cantdie=1, read_ok=1;
static uint64_t player=0x1000, stats;
static uint64_t PlayerChar(void) { return player; }
static int Rd64(uintptr_t address,uint64_t* out) {
    if(!read_ok || address!=player+0x3A0) return 0;
    *out=stats; return 1;
}
static uintptr_t Grva(uintptr_t rva) { return rva; }
static void Log(const char* fmt,...) { (void)fmt; }
static int allow_native_death;
static int PhCanBlock(uint64_t a) { (void)a; return !allow_native_death; }
static int hazard_notes;
static void PhNoteHazard(uint64_t a) { (void)a; ++hazard_notes; }
#include "../src/player_hp.h"
#include "../src/player_hp_entry.h"
static int last_damage, calls; static uint8_t last_keep,last_p4;
static uint64_t Native(uint64_t hp,int damage,uint8_t keep,uint8_t p4) {
    last_damage=damage; last_keep=keep; last_p4=p4; ++calls;
    uint32_t* dead=(uint32_t*)(hp+0x10); uint64_t* current=(uint64_t*)(hp+0x20);
    if(*dead==1) return 0;
    int64_t next=(int64_t)*current-damage;
    if(next>0) *current=(uint64_t)next;
    else if(keep && *current) *current=1;
    else { *dead=p4 ? 2 : 1; *current=0; }
    return 0x1122334455667701ull; // verify detour preserves full native result
}
static void Reset(uint8_t* state,uint64_t hp) {
    memset(state,0,0x100); *(uint64_t*)(state+0x58)=hp;
}
int main(void) {
    uint8_t state[0x100],enemy[0x100]; stats=(uint64_t)state;
    uint64_t hp=stats+0x38; Reset(state,2658); Reset(enemy,63402);
    uint8_t* site=(uint8_t*)VirtualAlloc(0,128,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE); assert(site);
    B=(uintptr_t)site-0x787268;
    memcpy(site,kPhDamagePrologue,17);
    // Undo the copied prologue, then tailcall Native with all args unchanged.
    const uint8_t epilogue[]={0x48,0x83,0xC4,0x20,0x5F,0x48,0x8B,0x5C,0x24,0x08,0x48,0xB8};
    memcpy(site+17,epilogue,sizeof epilogue);
    *(uint64_t*)(site+17+sizeof epilogue)=(uint64_t)Native;
    site[37]=0xFF; site[38]=0xE0;
    FlushInstructionCache(GetCurrentProcess(),site,128);
    assert(PhDamageEntryInstall() && g_ph_entry_state==1 && PhDamageEntryInstall());
    PhDamageFn entry=(PhDamageFn)site;
    // Two lethal damage scenarios represent grab/script and combo pathways,
    // bypassing the old NfHpDamage hit-apply wrapper entirely.
    for(int path=0;path<2;++path) for(int i=0;i<2;++i) {
        uint64_t result=entry(hp,path ? 9999 : 2658,0,(uint8_t)path);
        assert(result==0x1122334455667701ull && *(uint64_t*)(state+0x58)==2658 && *(uint32_t*)(state+0x48)==0);
        assert(last_damage==0 && last_keep==0 && last_p4==path);
    }
    assert(g_ph_entry_cuts==4 && hazard_notes==4);
    entry((uint64_t)enemy+0x38,70000,0,0); assert(*(uint64_t*)(enemy+0x58)==0 && *(uint32_t*)(enemy+0x48)==1 && last_p4==0);
    g_cfg_cantdie=0; entry(hp,5000,0,0); assert(*(uint64_t*)(state+0x58)==0 && *(uint32_t*)(state+0x48)==1);
    g_cfg_cantdie=1; assert(entry(hp,5000,0,0)==0 && *(uint32_t*)(state+0x48)==1); // no resurrection
    Reset(state,1000); entry(hp,-200,1,9); assert(*(uint64_t*)(state+0x58)==1200 && last_damage==-200 && last_keep==1 && last_p4==9);
    entry(hp,0,0,0); assert(*(uint64_t*)(state+0x58)==1200);
    read_ok=0; entry(hp,50,0,0); assert(*(uint64_t*)(state+0x58)==1150); read_ok=1;
    player=0; entry(hp,50,0,0); assert(*(uint64_t*)(state+0x58)==1100); player=0x2000;
    stats=(uint64_t)enemy; Reset(enemy,2000);
    entry(hp,50,0,0); assert(*(uint64_t*)(state+0x58)==1050); // old actor unprotected after load
    entry(stats+0x38,5000,0,0); assert(*(uint64_t*)(enemy+0x58)==2000);
    allow_native_death=1;entry(stats+0x38,5000,0,0);assert(*(uint64_t*)(enemy+0x58)==0 && last_damage==5000);
    VirtualFree((void*)g_ph_damage_original,0,MEM_RELEASE); g_ph_damage_original=0;
    memset(site,0xCC,17); g_ph_entry_state=0;
    assert(!PhDamageEntryInstall() && g_ph_entry_state==-1 && site[0]==0xCC);
    VirtualFree(site,0,MEM_RELEASE);
    puts("PASS: actual common-entry hook/trampoline; 4 bypass lethal events prevented before death flag; OFF, real boss kill, heals, flags/results, unreadable/replaced player, mismatch and no resurrection");
    return 0;
}
