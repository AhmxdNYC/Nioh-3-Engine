// Borrowed HUD/boss-mode actors: never put these in the owned spawn registry.
static uint32_t NativeSlotCharacter(uint64_t actor, uint32_t handle) {
    uint64_t w = FindWrapper(handle), rec = 0; uint32_t chr = 0;
    __try {
        if (!w || !Rd64(w + 0x18, &rec) || !rec || *(uint32_t*)rec != handle) return 0;
        chr = *(uint32_t*)(rec + 4);
        if (!chr || NativeEntryActor(handle, chr) != actor) return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return chr;
}
static int NativeSlotAlive(int s) {
    uint32_t mission = 0, mode = 0; uint8_t journey = 0;
    const uint8_t* gs = GameState(); uint64_t hp = 1, max = 0; int dead = 0;
    if (s < 0 || s >= N3T_SLOTS || !g_slot_native[s] || !gs || gs[2] ||
        !MsRead(&mission, &journey, &mode) || mission != g_slot_mission[s] ||
        !ActorInTable(g_slot_act[s]) || NativeSlotCharacter(g_slot_act[s], g_slot_native[s]) != g_slot_chr[s]) return 0;
    ActorDeadNow(g_slot_act[s], &dead, &hp, &max);
    return !dead && max; // preserve finisher/NOFIN HP-zero windows until death is applied
}
static int NativeSlotsActive(void) {
    for (int s = 0; s < N3T_SLOTS; ++s) if (g_slot_native[s] && NativeSlotAlive(s)) return 1;
    return 0;
}
// Undo only the explicit slot hold we acquired, while the same live native actor is validated.
#ifndef NATIVE_SLOT_AI_ON
#define NATIVE_SLOT_AI_ON(a) ((void (*)(uint64_t))(B + Grva(0x66982C)))(a)
#endif
static void NativeSlotReleaseHold(int s) {
    if (!g_slot_native[s] || !g_sph[s].phase || !g_sph[s].nOff ||
        g_sph[s].a != g_slot_act[s] || !NativeSlotAlive(s)) return;
    if (BhAiOn(g_slot_act[s]) == 0) {
        __try { NATIVE_SLOT_AI_ON(g_slot_act[s]); }
        __except (EXCEPTION_EXECUTE_HANDLER) { Log("NATIVE SLOT %c: hold release faulted", 'A' + s); }
    }
}
static void NativeSlotClear(int s) {
    NativeSlotReleaseHold(s);
    g_slot_native[s] = 0; g_slot_act[s] = 0; g_slot_chr[s] = 0; g_slot_ih[s] = 0;
    g_sph[s].phase = 0; g_slot_sp_ok[s] = 0;
    InterlockedExchange(&g_slot_rehold[s], 0); InterlockedExchange(&g_bv_req[s], 0);
    g_bv_kiWait[s] = 0; g_bv_live[s].live = 0;
}
static void NativeSlotsTick(void) {
    static int hadNative;
    uint64_t actors[8]; uint32_t handles[8] = {0}, mission = 0, mode = 0; uint8_t journey = 0;
    const uint8_t* gs = GameState();
    int ready = gs && !gs[2] && PlayerChar() && MsRead(&mission, &journey, &mode);
    int n = ready ? KbCollect(actors, 8, handles) : 0;
    for (int s = 0; s < N3T_SLOTS; ++s) if (g_slot_native[s]) {
        int found = 0;
        for (int i = 0; i < n; ++i) if (actors[i] == g_slot_act[s] && handles[i] == g_slot_native[s]) found = 1;
        if (!found || !NativeSlotAlive(s)) NativeSlotClear(s);
    }
    for (int i = 0; i < n; ++i) {
        if (!handles[i] || InSp(actors[i]) || SlotOf(actors[i]) >= 0 || !ActorInTable(actors[i])) continue;
        uint32_t chr = NativeSlotCharacter(actors[i], handles[i]);
        uint64_t hp = 1, max = 0; int dead = 0;
        if (!chr) continue;
        ActorDeadNow(actors[i], &dead, &hp, &max);
        if (dead || !max || !hp) continue;
        int s = SlotFree(); if (s < 0) break;
        NativeSlotClear(s); BvInit(); BvDefaults(&g_bv[s]);
        InterlockedExchange(&g_slot_fight[s], 1); InterlockedExchange(&g_slot_aggro[s], 0);
        g_slot_act[s] = actors[i]; g_slot_chr[s] = chr; g_slot_mission[s] = mission;
        g_slot_native[s] = handles[i];
        Log("NATIVE SLOT %c: handle %08X chr %u actor %p (borrowed, defaults off)", 'A' + s, handles[i], chr, (void*)actors[i]);
    }
    int live = NativeSlotsActive();
    if (live && !hadNative && !g_tr.active) InterlockedIncrement(&g_bm_gen);
    hadNative = live;
}
