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
// Diagnosis for "wrapper record did not match" (2026-11-25: every Picture Scroll wave boss, handles 0xF3C..0xF3E): which check failed.
static __attribute__((noinline)) void NativeSlotWhy(uint64_t actor, uint32_t handle, char* o, int n) {
    uint64_t w = FindWrapper(handle), w0 = 0, rec = 0; uint32_t rid = 0, chr = 0; int recOk = 0;
    __try {
        if (w) { Rd64(w, &w0); Rd64(w + 0x18, &rec); }
        if (rec) { rid = *(uint32_t*)rec; chr = *(uint32_t*)(rec + 4); recOk = 1; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
    const uint64_t byH = ActorByHandle(handle);
    snprintf(o, n, "wrapper %p [0] %p rec %p%s id %08X chr %u, ActorByHandle %p, player %d, own spawn %d -> %s", (void*)w, (void*)w0, (void*)rec, recOk ? "" : " (unreadable)", rid, chr,
        (void*)byH, actor == PlayerChar(), InSp(actor) ? 1 : 0,
        !w ? "no wrapper" : !rec ? "no record at wrapper+0x18" : rid != handle ? "record id is not the handle" : !chr ? "record chr 0"
        : w0 != actor ? "wrapper actor differs" : byH != actor ? "ActorByHandle differs" : "entry check failed");
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
static void NativeSlotClear(int s) {
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
    {   // diagnosis (style result card / Boss A-Z in a Picture Scroll): the mission read failing while in game means no boss is admitted
        static DWORD msT; static int msN;
        if (!ready && gs && !gs[2] && PlayerChar() && GetTickCount() - msT > 30000 && msN < 10) { msT = GetTickCount(); ++msN;
            Log("NATIVE SLOT: mission state unreadable in game, no HUD boss admitted this tick (logged every 30 s, %d/10)", msN); }
    }
    // HUD evidence admits a native actor once; a summoned boss may replace its gauge.
    // Retain the borrowed identity until its actual mission/actor/death lifecycle ends.
    // GitHub #94: a dark-realm form keeps the same slot. The wrapper's character id
    // changes; clearing the slot here is what dropped Nuribotoke from Training.
    for (int s = 0; s < N3T_SLOTS; ++s) {
        if (!g_slot_native[s] || !ready || !ActorInTable(g_slot_act[s])) continue;
        uint32_t live = NativeSlotCharacter(g_slot_act[s], g_slot_native[s]);
        if (!live || live == g_slot_chr[s]) continue;
        Log("NATIVE SLOT %c: character %u -> %u (form kept, not dropped)", 'A' + s, g_slot_chr[s], live);
        g_slot_chr[s] = live;
    }
    for (int s = 0; s < N3T_SLOTS; ++s)
        if (g_slot_native[s] && !NativeSlotAlive(s)) {
            uint32_t m2 = 0, md2 = 0; uint8_t j2 = 0; int d2 = 0; uint64_t c2 = 1, x2 = 0;
            const char* why = !gs || gs[2] ? "loading" : !MsRead(&m2, &j2, &md2) ? "mission unreadable" : m2 != g_slot_mission[s] ? "mission changed"
                : !ActorInTable(g_slot_act[s]) ? "actor left the table" : NativeSlotCharacter(g_slot_act[s], g_slot_native[s]) != g_slot_chr[s] ? "character changed"
                : (ActorDeadNow(g_slot_act[s], &d2, &c2, &x2), d2) ? "dead" : "no max HP";
            Log("NATIVE SLOT %c: actor %p released (%s)", 'A' + s, (void*)g_slot_act[s], why);
            NativeSlotClear(s);
        }
    // diagnosis: a HUD boss bar whose actor is not admitted, once per actor and reason (the last 16 remembered)
    static uint64_t rejA[16]; static int rejW[16], rejK;
#define NS_REJECT(a, why, ...) do { int _k = 0; while (_k < 16 && !(rejA[_k] == (a) && rejW[_k] == __LINE__)) ++_k; \
        if (_k == 16) { rejA[rejK] = (a); rejW[rejK] = __LINE__; rejK = (rejK + 1) % 16; Log("NATIVE SLOT: HUD boss actor %p not admitted: " why, (void*)(a), __VA_ARGS__); } } while (0)
    for (int i = 0; i < n; ++i) {
        if (!handles[i] || InSp(actors[i]) || SlotOf(actors[i]) >= 0) continue;
        if (!ActorInTable(actors[i])) { NS_REJECT(actors[i], "not in the actor table (handle %08X)", handles[i]); continue; }
        uint32_t chr = NativeSlotCharacter(actors[i], handles[i]);
        uint64_t hp = 1, max = 0; int dead = 0;
        if (!chr) { char why[256]; NativeSlotWhy(actors[i], handles[i], why, sizeof why);
            NS_REJECT(actors[i], "no character id for handle %08X (wrapper record did not match: %s; a Picture Scroll boss is carded as a HUD boss instead)", handles[i], why); continue; }
        ActorDeadNow(actors[i], &dead, &hp, &max);
        if (dead || !max || !hp) { NS_REJECT(actors[i], "chr %u dead %d HP %llu / %llu", chr, dead, hp, max); continue; }
        int s = SlotFree(); if (s < 0) { NS_REJECT(actors[i], "chr %u: every boss slot is taken", chr); break; }
        NativeSlotClear(s); BvInit(); BvDefaults(&g_bv[s]);
        InterlockedExchange(&g_slot_fight[s], 1); InterlockedExchange(&g_slot_aggro[s], 0);
        g_slot_act[s] = actors[i]; g_slot_chr[s] = chr; g_slot_mission[s] = mission;
        g_slot_native[s] = handles[i];
        Log("NATIVE SLOT %c: handle %08X chr %u actor %p (borrowed, defaults off)", 'A' + s, handles[i], chr, (void*)actors[i]);
    }
#undef NS_REJECT
    int live = NativeSlotsActive();
    if (live && !hadNative && !g_tr.active) InterlockedIncrement(&g_bm_gen);
    hadNative = live;
}
