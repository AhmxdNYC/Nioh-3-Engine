// #69771: guard the common HP damage entry, not only hit-apply's 329890 call.
// Native 787268 writes death at hp+10 and zero at hp+20 before tick refills.
// Uses the current player identity on EVERY call; no cached actor survives load.
typedef uint64_t (*PhDamageFn)(uint64_t, int, uint8_t, uint8_t);
static PhDamageFn g_ph_damage_original;
static volatile LONG g_ph_entry_state, g_ph_entry_cuts;
static const uint8_t kPhDamagePrologue[17]={
    0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,
    0x83,0x79,0x10,0x01,0x48,0x8B,0xD9
}; // five whole instructions; no RIP-relative data or branch
static __attribute__((noinline)) uint64_t PhDamageEntry(uint64_t hp,int dmg,uint8_t keep,uint8_t p4) {
    if (g_cfg_cantdie && hp && dmg>0) {
        uint64_t player=PlayerChar(), stats=0;
        if (player && Rd64(player+0x3A0,&stats) && stats && stats+0x38==hp) {
            PhNoteHazard(player); // recover on the pump, never teleport inside native damage
            const int before=dmg;
            dmg=PhDamageCut(g_cfg_cantdie,1,1,dmg);
            if (dmg!=before) {
                LONG n=InterlockedIncrement(&g_ph_entry_cuts);
                uintptr_t caller=(uintptr_t)__builtin_return_address(0);
                if(n<=20 || n%100==0) Log("INFHP ENTRY #%ld: player HP damage %d -> %d before native death check; caller %llX keep %u p4 %u",n,before,dmg,
                    (unsigned long long)(caller>=B && caller<B+g_imgSize ? caller-B : 0),keep,p4);
            }
        }
    }
    // Native result and both flags preserved. Heals, OFF and non-player damage
    // use the same original function. Already dead actors are never revived.
    return g_ph_damage_original(hp,dmg,keep,p4);
}
static int PhDamageEntryInstall(void) {
    if (g_ph_entry_state) return g_ph_entry_state==1;
    const uintptr_t rva=Grva(0x787268);
    uint8_t* site=(uint8_t*)(B+rva);
    if(!rva || memcmp(site,kPhDamagePrologue,sizeof kPhDamagePrologue)) {
        g_ph_entry_state=-1; Log("INFHP ENTRY 787268 unavailable: prologue mismatch; call-site protection only"); return 0;
    }
    uint8_t* trampoline=(uint8_t*)VirtualAlloc(0,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!trampoline) { g_ph_entry_state=-1; Log("INFHP ENTRY unavailable: trampoline allocation failed"); return 0; }
    memcpy(trampoline,site,sizeof kPhDamagePrologue);
    uint8_t* j=trampoline+sizeof kPhDamagePrologue;
    j[0]=0xFF; j[1]=0x25; *(uint32_t*)(j+2)=0; *(uint64_t*)(j+6)=(uint64_t)(site+sizeof kPhDamagePrologue);
    FlushInstructionCache(GetCurrentProcess(),trampoline,64);
    DWORD old;
    if(!VirtualProtect(site,sizeof kPhDamagePrologue,PAGE_EXECUTE_READWRITE,&old)) {
        VirtualFree(trampoline,0,MEM_RELEASE); g_ph_entry_state=-1; Log("INFHP ENTRY unavailable: site protection failed"); return 0;
    }
    // Publish original before redirecting the first possible native caller.
    g_ph_damage_original=(PhDamageFn)trampoline; MemoryBarrier();
    uint8_t patch[sizeof kPhDamagePrologue]; memset(patch,0x90,sizeof patch);
    patch[0]=0x48; patch[1]=0xB8; *(uint64_t*)(patch+2)=(uint64_t)PhDamageEntry; patch[10]=0xFF; patch[11]=0xE0;
    memcpy(site,patch,sizeof patch); VirtualProtect(site,sizeof patch,old,&old);
    FlushInstructionCache(GetCurrentProcess(),site,sizeof patch);
    g_ph_entry_state=1; Log("INFHP ENTRY 787268 installed: all HP-damage callers, player only, live toggle, heals/boss damage unchanged"); return 1;
}
