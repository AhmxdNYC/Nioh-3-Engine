// #70720: native hazard intent, not a map-height or move-ID heuristic.
static volatile uint64_t g_ph_hazard_actor;
static uint64_t g_ph_native_death_actor,g_ph_recovery_player;
static DWORD g_ph_recovery_last;
static int g_ph_recovery_attempts;
static int PhCanBlock(uint64_t a) {return a!=g_ph_native_death_actor;}
static void PlayerAllowNativeHazardDeath(uint64_t a) {
    g_ph_native_death_actor=a;
    if(g_tr_nodeath_now) PlayerSetNoDeath(0);
    __try { *(uint8_t*)(a+0x89A)=0; } __except(EXCEPTION_EXECUTE_HANDLER) {}
    Log("INFHP RECOVERY exhausted: native hazard death/respawn allowed for actor %p",(void*)a);
}
static int PlayerNativeHazard(uint64_t a) {
    if (!a) return 0;
    __try { return *(uint8_t*)(a + 0x89B) != 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static void PhNoteHazard(uint64_t a) {
    if (PlayerNativeHazard(a)) g_ph_hazard_actor = a;
}
static int PlayerCameraReset(void) {
    uintptr_t r = Grva(0x3B3B1C);
    if (!r) return 0;
    __try {
        // Camera::Reset (238AB44 -> 3B3B1C), no VM arguments.
        if (memcmp((void*)(B+r), "\x48\x83\xEC\x28\x48\x8B\x0D", 7)) return 0;
        ((void (*)(void))(B+r))(); return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static int PlayerRecoveryWarp(const float* p) {
    int ok = Warp(p[0], p[1], p[2], p[3], 1);
    if (ok > 0) Log("RECOVERY camera reset -> %d", PlayerCameraReset());
    return ok;
}
static int PlayerNativeRecoveryPos(float p[4]) {
    uintptr_t fn = Grva(0x2169D30), global = Grva(0x4751860);
    uint64_t manager = 0;
    if (!fn || !global || !Rd64(B+global, &manager) || !manager) return 0;
    __try {
        if (memcmp((void*)(B+fn), "\x40\x53\x48\x83\xEC\x20\x48\x8B\x19", 9)) return 0;
        const float* native = ((const float* (*)(uint64_t))(B+fn))(manager);
        if (!native) return 0;
        for (int i=0;i<3;i++) { p[i]=native[i]; if (!isfinite(p[i])) return 0; }
        p[3]=CamYawDeg(); return isfinite(p[3]);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
#include "player_recovery_tick.h"
