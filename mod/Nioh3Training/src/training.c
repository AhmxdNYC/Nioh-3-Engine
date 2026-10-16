// Nioh3Training.asi - experimental training-mode RE probe (ally, bridge 4318).
// Command file <game>\Nioh3Training.cmd (one command per line, consumed on the game thread), log <game>\Nioh3Training.log.
// Commands:
//   pos                         log player position
//   spawn charId [dist] [owner]  load the character's resources (Load::Character native) then CreateDynamic next to the player
//   dyn a0 a4 a5 a6 [fwd]       native CharacterCreateDynamic 1C0C5E0(a0, &{player+fwd}, yaw, a4, a5, a6)
//   act id                      ActivatePlacement 1180740(id, 0, 0, 0)
//   lchr id                     Load::Character native A42A7C(loader, id) using the loader captured by the trace hook
//   mission id slot start       MissionRequestById 2239600(id, slot, start, {101 1000000 10100 0..})
//   call rva a b c d            raw native call with four integer args (RE only)
//   dump                        log-only: actors, registry (c0+0x48), player words; also auto on the no-owner path, then boss words (bridge 4650)
//   actors                      list the live actor table (mgr [4745348]+B98 intrusive hash set, node = actor+E0)
//   despawn [all|last|N]        delete actors spawned by this ASI (found by diffing the actor table around CreateDynamic; native delete 2BF7E8)
//   kill [last|N] [drop]        native Character::EnemyForceKill 786C84 on a spawned actor (drop=1: ...WithItemDrop)
//   gauge <id> / ungauge <id>   UI::ShowBossGauge / UI::HideBossGauge natives (1D115D4 / 1C208BC)
//   yaw                         log camera yaw (deg), player pos, forward convention
//   fwdmode <0..3>              forward convention: 0 (sin,cos) 1 (cos,sin) 2 (-sin,cos) 3 (cos,-sin) on (x,z)
//   autogauge <0|1>             show boss gauge after each spawn (default 1)
//   phase <ms>                  mission-switch phase sampling interval in ms (default 500); the log window after 'mission' is 60 s
//   stage                       log the stage streamer: world byte, main cell id, init state, loaded cells (stage mgr [45B9E58]+500)
//   warp x y z yaw              native Player::SetPos tail (position, physics, actor-manager, stream centre, path init) - no mission load
//   arena charId x y z yaw bx by bz [bossYaw]  live arena swap: Load::Character + warp the player to x y z, hold him there until the
//                               stage streamer reports ready, then CreateDynamic the boss at bx by bz; logs ms from command to control
//   mworld id [id..]            log mission record world byte (+15C) for mission ids (record = 377B38([[45B9E30]], id))
//   oworld world start [white]  World::WarpOtherWorld native request (22399B8: mission 0 = stay in this mission, new world byte + start
//                               point); white=1 uses the WhiteOut variant's transition flags. Starts the 60 s phase log.
//   fnow missionId [start]      Fight now through the C API (Nioh3Training_FightNow): in-place swap when the row is in Nioh3Training_arenas.tsv and in this world, else the native request
//   preload                     Nioh3Training_Preload: request the models of the nearest bosses of this world
//   bbm id [notify] / bbmend    native Mission::BossBattleMode / BossBattleModeEnd replica (HUD/BGM mode, bridge 4576); bossmode 0|1 [notify]; flag id [val] = SetMissionFlag
//   dump                        also (bridge 4717) wrapper words of live registry rows and +0..0xFF of every non-player actor: run it in a native boss fight
//   ownerwait ms / owneract 0|1 owner fallbacks for the arena swap (ini OwnerWaitMs / OwnerActivate)
//   swap 0|1                    arena swap Enable at run time
//   team / setteam id t         (bridge 4878) log the team id [[actor+B0]+10] of every actor / write one; teamfix 0|1 [enemyTeam]; forceowner N (1 = reproduce owner = player)
//   perc [id] / percnear [m] / percfix N / percdiag 0|1 / inject [id] / bpoke off size val / mon [id]  (bridge 5201) perception list diag + player inject fix
//   train id [dist] / retry / trainstop  (bridge 4942) no-load training session: summon here (any world), player NoDeath, reset round without a load; hp / nodeath 0|1 / recover
// Config Nioh3Training.ini [ArenaSwap]; table Nioh3Training_arenas.tsv (tools/gen_arena_table.py). Exports: Nioh3Training_FightNow / _Preload / _Version.
// Trace hooks (log only): CreateDynamic native 1C0C5E0, Load::Character native A42A7C.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

static uintptr_t B;
static char g_dir[MAX_PATH];
static CRITICAL_SECTION g_logcs;

static void Log(const char* fmt, ...) {
    char buf[1024];
    SYSTEMTIME t; GetLocalTime(&t);
    int n = snprintf(buf, sizeof buf, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a; va_start(a, fmt); n += vsnprintf(buf + n, sizeof buf - n, fmt, a); va_end(a);
    if (n > (int)sizeof buf - 2) n = sizeof buf - 2;
    buf[n++] = '\n';
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\Nioh3Training.log", g_dir);
    EnterCriticalSection(&g_logcs);
    HANDLE h = CreateFileA(p, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_ALWAYS, 0, 0);
    if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, buf, n, &w, 0); CloseHandle(h); }
    LeaveCriticalSection(&g_logcs);
}

// ---- inline hooks: 12-byte movabs rax / jmp rax at entry (rax is scratch at entry), trampoline = copied bytes + jmp back
static void* Hook(uintptr_t rva, int ncopy, void* detour) {
    uint8_t* site = (uint8_t*)(B + rva);
    uint8_t* tr = (uint8_t*)VirtualAlloc(0, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(tr, site, ncopy);
    uint8_t* j = tr + ncopy;
    j[0] = 0xFF; j[1] = 0x25; *(uint32_t*)(j + 2) = 0; *(uint64_t*)(j + 6) = (uint64_t)(site + ncopy);  // jmp [rip+0]
    DWORD old; VirtualProtect(site, ncopy, PAGE_EXECUTE_READWRITE, &old);
    uint8_t p[32]; memset(p, 0x90, sizeof p);
    p[0] = 0x48; p[1] = 0xB8; *(uint64_t*)(p + 2) = (uint64_t)detour; p[10] = 0xFF; p[11] = 0xE0;
    memcpy(site, p, ncopy);
    VirtualProtect(site, ncopy, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, ncopy);
    return tr;
}

static int Rd64(uintptr_t a, uint64_t* out) { __try { *out = *(volatile uint64_t*)a; return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }

static uint64_t PlayerChar(void) {
    uint64_t pt = 0, pl = 0;
    if (!Rd64(B + 74732360, &pt) || !pt) return 0;   // g:PlayerTab
    if (!Rd64(pt + 0xCD * 0x18, &pl)) return 0;
    return pl;
}
static int PlayerPos(float o[4]) {
    uint64_t ch = PlayerChar(); if (!ch) return 0;
    __try { memcpy(o, (void*)(ch + 0x110), 16); return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static float CamYawDeg(void) {
    void* cam = ((void* (*)(int))(B + 0x20A914))(0);
    if (!cam) return 0.0f;
    return ((float (*)(void*))(B + 0x356018))(cam) * 57.2957764f;
}
static int g_fwdmode = 0, g_autogauge = 1;
static void FwdVec(float yawDeg, float* dx, float* dz) {
    float y = yawDeg * 0.01745329252f, s = sinf(y), c = cosf(y);
    switch (g_fwdmode) { case 1: *dx = c; *dz = s; break; case 2: *dx = -s; *dz = c; break; case 3: *dx = c; *dz = -s; break; default: *dx = s; *dz = c; }
}
// Actor table: every live actor is linked (node = actor+0xE0, next = *node) into 16 hash buckets {head,count} at [[mgr]+0xB98+8].
static int Actors(uint64_t* out, int max) {
    int n = 0; uint64_t mgr = 0, bk = 0;
    __try {
        if (!Rd64(B + 0x4745348, &mgr) || !mgr || !Rd64(mgr + 0xB98 + 8, &bk) || !bk) return 0;
        for (int i = 0; i < 16; ++i) {
            uint64_t node = *(uint64_t*)(bk + i * 16);
            for (int guard = 0; node && guard < 4096; ++guard, node = *(uint64_t*)node)
                if (n < max) out[n++] = node - 0xE0;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
    return n;
}
#define MAXSP 32
static void PoolAdd(uint32_t chr);
static uint64_t g_sp_actor[MAXSP]; static int g_nsp;
static uint64_t g_snap[1024]; static int g_nsnap; static DWORD g_watch_t0; static int g_watch; static uint32_t g_watch_id;
static int InSnap(uint64_t a) { for (int i = 0; i < g_nsnap; ++i) if (g_snap[i] == a) return 1; return 0; }
static int InSp(uint64_t a) { for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i] == a) return 1; return 0; }
static int g_dump_pending, g_dump_full; static uint32_t g_dump_owner;
// bridge 4650: log-only diagnostics for "no live registered actor to own the boss" (passive boss)
static void DumpWords(const char* tag, uint64_t a, uint32_t owner, int bytes) {
    __try {
        char row[200];
        for (int o = 0; o < bytes; o += 32) {
            int k = 0; for (int j = 0; j < 8; ++j) k += snprintf(row + k, sizeof row - k, "%08X ", *(uint32_t*)(a + o + j * 4));
            Log("  %s +%04X: %s", tag, o, row);
        }
        for (int o = 0; o < 0x2000; o += 4) { uint32_t v = *(uint32_t*)(a + o); if (owner > 1 && v == owner) Log("  %s owner id %u found at +%X", tag, owner, o); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("  %s: unreadable %p", tag, (void*)a); }
}
static void DumpOwnerState(const char* why) {
    __try {
        uint64_t cm = 0, c0 = 0, me = PlayerChar(); Rd64(B + 0x45B9E40, &cm); if (cm) Rd64(cm, &c0);
        Log("DUMP [%s] charMgr=%p c0=%p player=%p", why, (void*)cm, (void*)c0, (void*)me);
        uint64_t cur[1024]; int n = Actors(cur, 1024);
        Log("DUMP actors: %d in table", n);
        for (int i = 0; i < n; ++i) { float xyz[4] = {0}; memcpy(xyz, (void*)(cur[i] + 0x110), 16);
            Log("  actor %p%s handle %08X %08X pos %.0f %.0f %.0f +ab84=%08X", (void*)cur[i], cur[i] == me ? " PLAYER" : "", *(uint32_t*)cur[i], *(uint32_t*)(cur[i] + 4), xyz[0], xyz[1], xyz[2], *(uint32_t*)(cur[i] + 0xAB84)); }
        if (c0) {
            uint64_t* reg = *(uint64_t**)(c0 + 0x48); uint64_t arr = reg[0], cnt = reg[1];
            Log("DUMP registry c0+0x48=%p count=%llu", (void*)reg, cnt);
            int shown = 0;
            for (uint64_t i = 0; i < cnt; ++i) {
                uint32_t id = *(uint32_t*)(arr + i * 16); uint64_t w = *(uint64_t*)(arr + i * 16 + 8), live = 0, rec = 0; uint32_t pl = 0;
                if (w) { Rd64(w, &live); Rd64(w + 0x18, &rec); pl = *(uint32_t*)(w + 0x20); }
                if (!live && shown >= 100) continue;  // live rows always, dead rows only the first 100
                if (!live) ++shown;
                if (live && live != me && g_dump_full) { char tg[40]; snprintf(tg, sizeof tg, "reg%u wrapper", id); DumpWords(tg, w, 0, 0x60); }
                char rb[200] = {0}; int k = 0; if (rec) for (int j = 0; j < 12; ++j) k += snprintf(rb + k, sizeof rb - k, "%u ", ((uint32_t*)rec)[j]);
                Log("  reg id=%u w=%p live=%p%s pl=%u rec=%p [%s]", id, (void*)w, (void*)live, live == me ? " PLAYER" : "", pl, (void*)rec, rb);
            }
        }
        if (me) DumpWords("player", me, 0, 0x100);
        if (g_dump_full) for (int i = 0; i < n; ++i) if (cur[i] != me) { char tg[40]; snprintf(tg, sizeof tg, "actor%d(%08X)", i, *(uint32_t*)(cur[i] + 4)); DumpWords(tg, cur[i], 0, 0x100); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("DUMP [%s]: exception", why); }
}
// bridge 4878: boss monitor, 20 s after a spawned boss enters the table: boss->player distance and team every 500 ms (engagement evidence: an active boss closes in)
static struct { uint64_t a; DWORD t0, last; float minD; } g_mon;
static int g_cfg_awarefix = 1;  // bridge 5089: 0 off, 1 SetAwareFlg + diag, 2 also AI-on 66982C, 3 also lock-on 68210C
static int ActorTeam(uint64_t actor);
// bridge 5201: perception entry ref = {qword key = *(u64*)actor, dword serial = actor+0x18} (27B870), empty key = 0xF; 20A6F0(perc,i) resolves it to the actor
// through the actor table. The old diag compared the key with the actor POINTER, so 'player-entry' was always -1 (false negative).
static uint64_t PercOf(uint64_t actor, uint64_t* comp, uint64_t* ai) {
    uint64_t c = 0, a = 0, p = 0;
    if (!actor || !Rd64(actor + 0x3B0, &c) || !c || !Rd64(c + 0x50, &a) || !a || !Rd64(a + 0x10, &p)) return 0;
    if (comp) *comp = c; if (ai) *ai = a; return p;
}
static uint64_t PercEntry(uint64_t perc, int i) { return ((uint64_t (*)(uint64_t, int))(B + 0x20A6F0))(perc, i); }
static int PercFind(uint64_t perc, uint64_t who) { for (int i = 0; i < 8; ++i) if (who && PercEntry(perc, i) == who) return i; return -1; }
static int PercDiag(uint64_t actor, const char* tag) {  // log-only; returns the player's entry index or -1
    uint64_t comp = 0, ai = 0, perc = PercOf(actor, &comp, &ai), me = PlayerChar();
    if (!perc) { Log("PERC %s %p: no comp/ai/perc (comp %p ai %p)", tag, (void*)actor, (void*)comp, (void*)ai); return -1; }
    int hit = -1, used = 0; char row[600]; int k = 0; row[0] = 0;
    __try {
        for (int i = 0; i < 8; ++i) {
            uint64_t key = *(uint64_t*)(perc + 0x28 + i * 0x58), e = PercEntry(perc, i);
            if (key != 0xF) used++;
            if (e && e == me) hit = i;
            if (key != 0xF) k += snprintf(row + k, sizeof row - k, "[%d %s%p id %08X team %d aw %u] ", i, e == me ? "PLAYER " : "", (void*)e, e ? *(uint32_t*)(e + 4) : 0, e ? ActorTeam(e) : -999, *(uint8_t*)(perc + 0x10 + i * 0x58));
        }
        uint64_t mt = ((uint64_t (*)(uint64_t))(B + 0xA8520))(actor);
        Log("PERC %s %p id %08X team %d: ai+1574=%u ai+1AA8=%u perc+2D0=%d +2DC=%d +558=%d +378=%d actor+8A4=%d player+8A4=%d actor+ACCA=%u player-entry %d used %d main-target %s%p | %s",
            tag, (void*)actor, *(uint32_t*)(actor + 4), ActorTeam(actor), *(uint8_t*)(ai + 0x1574), *(uint32_t*)(ai + 0x1AA8), *(int32_t*)(perc + 0x2D0), *(int32_t*)(perc + 0x2DC),
            *(int32_t*)(perc + 0x558), *(int32_t*)(perc + 0x378), *(int32_t*)(actor + 0x8A4), me ? *(int32_t*)(me + 0x8A4) : -999, *(uint8_t*)(actor + 0xACCA), hit, used, mt && mt == me ? "PLAYER " : "", (void*)mt, row);
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PERC %s %p: exception %08lX", tag, (void*)actor, GetExceptionCode()); }
    return hit;
}
// bridge 5201 fix candidate (ini [Summon] PerceptionFix, command 'percfix N'): put the player into the boss's perception list the way the native perception update does
// when it adds a new target (289CB0 tail 28A0BD..28A17A: clear the slot, ref = 27B870(player), perc+0x2DC = 0, 2BE2E8(perc, idx) = make it the current target), then SetAwareFlg.
// 1 = inject only when the player is missing, 2 = also 66982C (AlgoActiveFlag 1 + AI reset) before the inject.
static int g_cfg_percfix = 0, g_cfg_percdiag = 1;
static int PercInject(uint64_t actor, const char* why) {
    uint64_t comp = 0, ai = 0, perc = PercOf(actor, &comp, &ai), me = PlayerChar(); if (!perc || !me) return -1;
    int idx = PercFind(perc, me);
    if (idx >= 0) { Log("PERCFIX %s: player already in entry %d", why, idx); }
    else {
        for (int i = 0; i < 8 && idx < 0; ++i) if (*(uint64_t*)(perc + 0x28 + i * 0x58) == 0xF) idx = i;
        if (idx < 0) idx = 7;
        uint8_t* e = (uint8_t*)(perc + idx * 0x58);
        *(uint64_t*)(e + 0x14) = 0; *(uint64_t*)(e + 0x1C) = 0; *(uint64_t*)(e + 0x40) = 0; e[0x10] = 0; *(int32_t*)(e + 0x48) = -1;
        *(uint16_t*)(e + 0x38) = 0; *(uint64_t*)(e + 0x4C) = 0; *(uint64_t*)(e + 0x54) = 0; *(uint32_t*)(e + 0x3C) = 0; *(uint32_t*)(e + 0x5C) = 0; e[0x60] = 0;
        *(uint64_t*)(e + 0x28) = *(uint64_t*)me; *(uint32_t*)(e + 0x30) = *(uint32_t*)(me + 0x18);
        *(int32_t*)(perc + 0x2DC) = 0;
        Log("PERCFIX %s: player written into entry %d (resolves to %p)", why, idx, (void*)PercEntry(perc, idx));
    }
    if (g_cfg_percfix >= 2) ((void (*)(uint64_t))(B + 0x66982C))(actor);
    ((void (*)(uint64_t, int))(B + 0x2BE2E8))(perc, idx);
    ((void (*)(uint64_t, uint8_t, int))(B + 0x1FF1FA0))(comp, 1, 0);
    return idx;
}
static void AwareDiag(const char* tag) { PercDiag(g_mon.a, tag); }
static void AwareApply(void) {
    uint64_t comp = 0, me = PlayerChar(); if (!g_cfg_awarefix || !Rd64(g_mon.a + 0x3B0, &comp) || !comp) return;
    __try {
        AwareDiag("before");
        if (g_cfg_awarefix >= 2) ((void (*)(uint64_t))(B + 0x66982C))(g_mon.a);
        if (g_cfg_awarefix >= 3 && me) ((void (*)(uint64_t, uint64_t))(B + 0x68210C))(comp, me);
        ((void (*)(uint64_t, uint8_t, int))(B + 0x1FF1FA0))(comp, 1, 0);
        AwareDiag("after");
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("AWARE: native call faulted"); }
}
static void MonTick(void) {
    if (!g_mon.a) return;
    DWORD now = GetTickCount(); if (now - g_mon.last < 500) return; g_mon.last = now;
    uint64_t cur[1024]; int n = Actors(cur, 1024), found = 0; for (int i = 0; i < n; ++i) if (cur[i] == g_mon.a) found = 1;
    float b[4] = {0}, p[4] = {0};
    if (!found || !PlayerPos(p)) { Log("BOSSMON +%lu ms boss actor gone", now - g_mon.t0); g_mon.a = 0; return; }
    memcpy(b, (void*)(g_mon.a + 0x110), 16);
    float dx = b[0] - p[0], dz = b[2] - p[2], d = sqrtf(dx * dx + dz * dz) / 100.0f; if (d < g_mon.minD) g_mon.minD = d;
    if (now - g_mon.t0 < 6000) AwareApply();
    if (g_cfg_percfix && now - g_mon.t0 >= 600 && now - g_mon.t0 < 8000) {  // bridge 5201: re-inject while the player is missing (the native update may evict it)
        uint64_t perc = PercOf(g_mon.a, 0, 0);
        if (perc && PercFind(perc, PlayerChar()) < 0) { PercDiag(g_mon.a, "before-fix"); __try { PercInject(g_mon.a, "mon"); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PERCFIX: exception %08lX", GetExceptionCode()); } PercDiag(g_mon.a, "after-fix"); }
    }
    if (g_cfg_percdiag && ((now - g_mon.t0) / 500) % 4 == 0) PercDiag(g_mon.a, "mon");
    uint64_t c = 0; Rd64(g_mon.a + 0xB0, &c);
    Log("BOSSMON +%lu ms boss %.0f %.0f %.0f player %.0f %.0f %.0f dist %.1f m (min %.1f) team %d", now - g_mon.t0, b[0], b[1], b[2], p[0], p[1], p[2], d, g_mon.minD, c ? *(int32_t*)(c + 0x10) : -999);
    if (now - g_mon.t0 > 20000) { Log("BOSSMON end: closest %.1f m", g_mon.minD); g_mon.a = 0; }
}
static int g_cfg_sidefix = 1;
static int ActorTeam(uint64_t actor);
// bridge 4942: Character::SetAwareFlg(boss) (VM 2394200) = 1FF1FA0(actor+0x3B0, 1, 0) -> FC4EA0([[ai+0x50]+0x10], 1, 0). The duel script calls it once the player is within 8 m; a
// CreateDynamic boss has no script, so passive bosses (Takeda Shingen) never notice the player. Applied to every spawned boss at +AwareDelayMs and again 1.5 s later.
static int g_cfg_aware = 1, g_cfg_aware_ms = 400;
static struct { uint64_t a; DWORD t; int n; } g_aw;
static int SetAware(uint64_t actor, const char* why) {
    uint64_t ai = 0; if (!actor || !Rd64(actor + 0x3B0, &ai) || !ai) { Log("AWARE %s: no AI object on %p", why, (void*)actor); return 0; }
    __try { ((void (*)(uint64_t, uint8_t, uint32_t))(B + 0x1FF1FA0))(ai, 1, 0); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("AWARE %s: exception %08lX", why, GetExceptionCode()); return 0; }
    Log("AWARE %s: SetAwareFlg applied to actor %p (ai %p)", why, (void*)actor, (void*)ai); return 1;
}
static void AwareTick(void) {
    if (!g_aw.a || (int)(GetTickCount() - g_aw.t) < 0) return;
    SetAware(g_aw.a, g_aw.n ? "retry" : "spawn");
    if (g_aw.n++ >= 1) g_aw.a = 0; else g_aw.t = GetTickCount() + 1500;
}
static void WatchTick(void) {  // after CreateDynamic: collect actors that were not in the pre-spawn snapshot
    if (!g_watch) return;
    uint64_t cur[1024]; int n = Actors(cur, 1024);
    for (int i = 0; i < n; ++i) if (!InSnap(cur[i]) && !InSp(cur[i]) && *(uint32_t*)(cur[i] + 4) == g_watch_id && g_nsp < MAXSP) {
        float xyz[4] = {0}; memcpy(xyz, (void*)(cur[i] + 0x110), 16);
        g_sp_actor[g_nsp++] = cur[i];
        Log("SPAWN actor #%d appeared in table: %p handle %08X %08X pos %.0f %.0f %.0f +ab84=%08X (+%lu ms)", g_nsp - 1, (void*)cur[i], *(uint32_t*)cur[i], *(uint32_t*)(cur[i] + 4), xyz[0], xyz[1], xyz[2], *(uint32_t*)(cur[i] + 0xAB84), GetTickCount() - g_watch_t0);
        { uint64_t c = 0, me = PlayerChar(), pc = 0; Rd64(cur[i] + 0xB0, &c); if (me) Rd64(me + 0xB0, &pc);
          Log("SPAWN actor #%d team %d (player team %d)", g_nsp - 1, c ? *(int32_t*)(c + 0x10) : -999, pc ? *(int32_t*)(pc + 0x10) : -999); }
        if (g_cfg_sidefix) {  // bridge 5030 FIX 1 (pc RE): runtime side change + AI retarget; fallback SetSideEnemy 669510
            int t0 = ActorTeam(cur[i]), pt = ActorTeam(PlayerChar());
            if (1) {  // pc 5033: team already 1 yet the boss stayed passive, so always run the retarget call, not only when the team is wrong
                __try { ((void (*)(uint64_t, int))(B + 0x1C272F0))(cur[i], 1); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SIDEFIX: 1C272F0 faulted"); }
                int t1 = ActorTeam(cur[i]);
                Log("SIDEFIX: 1C272F0(actor,1) team %d -> %d", t0, t1);
                if (t1 == t0 && (t0 == pt || t0 == 0)) { __try { ((void (*)(uint64_t))(B + 0x669510))(cur[i]); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SIDEFIX: 669510 faulted"); }
                    Log("SIDEFIX: fallback 669510 team %d -> %d", t1, ActorTeam(cur[i])); }
            } else Log("SIDEFIX: skipped, team %d already differs from player's %d", t0, pt);
        }
        g_mon.a = cur[i]; g_mon.t0 = g_mon.last = GetTickCount(); g_mon.minD = 1e9f;
        if (g_cfg_aware) { g_aw.a = cur[i]; g_aw.n = 0; g_aw.t = GetTickCount() + g_cfg_aware_ms; }
        if (g_dump_pending) { DumpWords("boss", cur[i], g_dump_owner, 0x100); g_dump_pending = 0; }
        g_watch = 0;
    }
    if (GetTickCount() - g_watch_t0 > 3000) { Log("SPAWN: no new actor with handle id %08X appeared within 3 s (previous actor of the same owner/slot still alive? despawn it first)", g_watch_id); g_watch = 0; }
}
static void FakeGaugeHide(const char* why);
static int Despawn(int idx) {
    FakeGaugeHide("despawn");
    if (idx < 0 || idx >= g_nsp) { Log("DESPAWN #%d: no such spawned actor (have %d)", idx, g_nsp); return 0; }
    uint64_t a = g_sp_actor[idx], cur[1024]; int n = Actors(cur, 1024), found = 0;
    for (int i = 0; i < n; ++i) if (cur[i] == a) found = 1;
    if (!found) { Log("DESPAWN #%d %p: not in actor table any more (already gone)", idx, (void*)a); g_sp_actor[idx] = 0; return 0; }
    ((void (*)(uint64_t))(B + 0x2BF7E8))(a);
    Log("DESPAWN #%d %p: native delete 2BF7E8 called", idx, (void*)a);
    g_sp_actor[idx] = 0;
    return 1;
}
// Character::FadeOut(id, frames) (VM 238DC40): live = wrapper[0]; c = [live+0x20]; if c: 2BC5DC(c+0x240, 0.0f target, frames) -- a lerp {start,target,cur,t,dur}
// toward alpha 0. Mission scripts call it with f30 and follow with Character::Delete. Does not free the actor; Despawn later does. (ported from work-src529, 13:50 fade build)
static int FadeOutActor(int idx, float frames) {
    if (idx < 0 || idx >= g_nsp || !g_sp_actor[idx]) return 0;
    uint64_t a = g_sp_actor[idx], cur[1024], c = 0; int n = Actors(cur, 1024), found = 0;
    for (int i = 0; i < n; ++i) if (cur[i] == a) found = 1;
    if (!found) { Log("FADEOUT #%d %p: not in actor table any more", idx, (void*)a); return 0; }
    if (!Rd64(a + 0x20, &c) || !c) { Log("FADEOUT #%d %p: no fade component at +0x20", idx, (void*)a); return 0; }
    __try { ((void (*)(uint64_t, float, float))(B + 0x2BC5DC))(c + 0x240, 0.0f, frames); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("FADEOUT #%d: exception %08lX", idx, GetExceptionCode()); return 0; }
    Log("FADEOUT #%d %p: native fade 2BC5DC(%p+0x240, 0, %.0f) started", idx, (void*)a, (void*)c, frames);
    return 1;
}
// #6185 summon death toggles. Native Character::EnemyForceKill (VM 238D504) / EnemyForceKillWithItemDrop (VM 238DAA0) both call the death component
// 786C84(comp = actor+0x3B0, dmg 0, type 0, r9 -1, drop, force 1, 0, 0, notify 1) guarded by actor+0xAB84 == -1; the only difference is drop (arg 5):
// 786C84 hands it to the death/drop handler 1FFFEDC(comp, type, drop) and sets death-message bit 0x8000 when it is 0 (no item drop).
// With force = 1 the HP-floor guard (hp+0x188 bit 1, cleared through EC4C5C(hp+0x178,1,-1)) is bypassed, so it kills a boss that sits at HP 1 waiting for its death blow.
// 786C84 returns at once when hp+0x48 == 1 (already dead), so a second call is harmless. Ally bridge 458: Yamagata 'kill last 1' -> loot pillars + Amrita.
static int g_cfg_loot = 0, g_cfg_deathblow = 1, g_cfg_cutscene = 0, g_cfg_fin_timeout = 6000;
// #6643 death-blow revive. Logs 18:57 / 19:02: the native kill set the dead flag, then the boss came back at HP 1 (sensor 0->1->0), and a raw HP 0
// write was also undone. Cause (static RE, 786C84 after the damage apply 787268): once hp+0x48 == 1 (dead), unless actor+0x894 != 0,
//   786FFC  mov edx,27h / mov rcx,rdi / call 4893B8   ; CharacterConditionEvaluate(cond 0x27) = byte [chr+0x20D] bit 4 ("needs death blow")
//   787009  test al,al / jz 787058                   ; 0 -> 1FFFEDC(comp, type, drop): the normal death + drop handler
//   78700D  ... call 66C188(hp+0x38, 1)              ; 1 -> HP set to 1, dead flag cleared, +0xEA0 / 0x400000000000 = death-blow-pending state
// So the "HP floor" is a revive-to-1 inside the damage function. Fix: retarget that one call (E8 rel32) to a near stub. For the boss-mode / summoned
// boss with DeathBlowRequired=0, or during our own native force kill, a true cond 0x27 is reported as false, so the game takes its own death path
// in the same call (no timing change, no memory writes to the actor). BL (= 786C84 arg 5, drop) feeds 1FFFEDC and the "no drop" bit 0x8000
// (test bl,bl at 787219); the stub clears BL when the C detour returns 2 (natural kill with BossLoot=0 -> no drop, as before).
typedef uint8_t (*CondFn)(uint64_t, uint32_t);
static CondFn o_cond;
static volatile uint64_t g_db_act, g_kill_act;  // boss-mode boss actor (BossTick), actor being force-killed by ForceKillActor
static volatile LONG g_db_logs;
static volatile uint64_t g_killHideAct;  // #6711: actor killed inside the damage function (h_cond skip); GaugeTick hides its bar on the next pump tick
static void GaugeHideOnKill(uint64_t a, const char* why);
static void BossKilledNow(uint64_t a, const char* why);
static void KillFadeNow(uint64_t a);
static int g_db_done;  // BossTick: the HP-floor / finisher native kill of this boss-mode session was issued
static uint8_t h_cond(uint64_t comp, uint32_t cond) {
    uint8_t r = o_cond(comp, cond);
    if (!r || cond != 0x27) return r;
    uint64_t a = 0; if (!Rd64(comp, &a) || !a) return r;
    int forcing = a == g_kill_act;
    if (!forcing && (g_cfg_deathblow || (a != g_db_act && !InSp(a)))) return r;
    g_killHideAct = a;  // #6711: not from inside the damage function, the HUD is touched on the pump tick
    if (InterlockedIncrement(&g_db_logs) <= 20) Log("DEATHBLOW skip: actor %p lethal hit, death-blow revive (cond 0x27) skipped -> native death (%s, drop %s)", (void*)a, forcing ? "force kill" : "DeathBlowRequired=0", forcing ? "per kill call" : g_cfg_loot ? "game" : "off");
    return (forcing || g_cfg_loot) ? 0 : 2;
}
static int HookDeathBlowCond(void) {
    uint8_t* site = (uint8_t*)(B + 0x787004);
    static const uint8_t pre[8] = {0xBA, 0x27, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xCF}, post[4] = {0x84, 0xC0, 0x74, 0x4B};
    if (memcmp(site - 8, pre, 8) || site[0] != 0xE8 || memcmp(site + 5, post, 4) || memcmp((void*)(B + 0x787219), "\x84\xDB\x75\x08", 4)) { Log("DEATHBLOW hook 787004: bytes mismatch, DeathBlowRequired=0 falls back to the native kill only"); return 0; }
    uintptr_t prev = (uintptr_t)(site + 5) + *(int32_t*)(site + 1);
    uint8_t* stub = 0;
    for (uintptr_t a = ((uintptr_t)site - 0x70000000) & ~0xFFFFull; a < (uintptr_t)site + 0x70000000 && !stub; a += 0x10000)
        stub = (uint8_t*)VirtualAlloc((void*)a, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { Log("DEATHBLOW hook: no near stub"); return 0; }
    // sub rsp,28h; mov rax,h_cond; call rax; add rsp,28h; cmp al,2; jne ret; xor bl,bl; xor eax,eax; ret
    static const uint8_t code[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x28, 0x3C, 0x02, 0x75, 0x04, 0x30, 0xDB, 0x31, 0xC0, 0xC3};
    memcpy(stub, code, sizeof code); *(uint64_t*)(stub + 6) = (uint64_t)h_cond;
    FlushInstructionCache(GetCurrentProcess(), stub, sizeof code);
    o_cond = (CondFn)prev;
    int32_t rel = (int32_t)((intptr_t)stub - (intptr_t)(site + 5));
    DWORD old; VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    InterlockedExchange((volatile LONG*)(site + 1), rel);
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    Log("DEATHBLOW hook 787004 ok: cond 0x27 revive call -> stub %p (previous target rva %llX)", stub, (unsigned long long)(prev - B));
    return 1;
}
static int ForceKillActor(uint64_t live, int drop, const char* why) {
    if (!live) return 0;
    uint32_t flag = 0; uint64_t comp = 0, h = 0; int dead = 0;
    __try { flag = *(uint32_t*)(live + 0xAB84); comp = *(uint64_t*)(live + 0x3B0); if (Rd64(live + 0x3A0, &h) && h) dead = *(int*)(h + 0x48) == 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    if (dead) { Log("KILL %s %p: already dead (hp+48 == 1), nothing to do", why, (void*)live); return 0; }
    if (flag != 0xFFFFFFFFu || !comp) { Log("KILL %s %p: guard refused (+ab84=%08X comp=%p; the script natives need +ab84 == -1)", why, (void*)live, flag, (void*)comp); return 0; }
    g_kill_act = live;  // h_cond: a force kill really kills (no death-blow revive to HP 1)
    __try { ((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(B + 0x786C84))(comp, 0, 0, 0xFFFFFFFFull, drop ? 1 : 0, 1, 0, 0, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_kill_act = 0; Log("KILL %s: exception %08lX", why, GetExceptionCode()); return 0; }
    g_kill_act = 0;
    Log("KILL %s %p: native EnemyForceKill%s 786C84 called", why, (void*)live, drop ? "WithItemDrop" : "");
    BossKilledNow(live, why);  // #6711 / #6774: boss mode end + bar off the moment the kill is applied (pump thread, after 786C84 returned)
    return 1;
}

// ---- trace hooks
typedef uint32_t (*DynFn)(uint32_t, const float*, float, uint32_t, uint32_t, uint32_t);
static DynFn o_dyn;
static uint32_t h_dyn(uint32_t a0, const float* p, float yaw, uint32_t a4, uint32_t a5, uint32_t a6) {
    float pp[4] = {0}; if (p) memcpy(pp, p, 16);
    uint32_t r = o_dyn(a0, p, yaw, a4, a5, a6);
    Log("TRACE CreateDynamic a0=%u (0x%X) pos=%.1f %.1f %.1f yaw=%.2f a4=%u a5=%u a6=%u -> %u (0x%X) ret=%p",
        a0, a0, pp[0], pp[1], pp[2], yaw, a4, a5, a6, r, r, (void*)((uintptr_t)__builtin_return_address(0) - B));
    return r;
}
typedef void (*LoadChrFn)(uint64_t, uint32_t);
static LoadChrFn o_lchr;
static volatile uint64_t g_loader;
static void h_lchr(uint64_t loader, uint32_t id) {
    g_loader = loader;
    Log("TRACE Load::Character id=%u (0x%X) loader=%p ret=%p", id, id, (void*)loader, (void*)((uintptr_t)__builtin_return_address(0) - B));
    o_lchr(loader, id);
}

// registry message post 60BE48(reg, &pair{obj, ctrl}): every script->placement message (SetDoorControl*, SetStatusPlacement, ...) goes through it. Log-only trace (bridge 4718): vtable RVA, placement id (+8), dwords +0x10/+0x14.
typedef void (*PostFn)(uint64_t, uint64_t*);
static PostFn o_post; static volatile LONG g_post_trace = 1;
static void h_post(uint64_t reg, uint64_t* pair) {
    static volatile LONG n; if (g_post_trace && InterlockedIncrement(&n) < 20000) { uint64_t o = pair ? pair[0] : 0; uint64_t vt = 0, v10 = 0; uint32_t id = 0;
        __try { vt = *(uint64_t*)o; id = *(uint32_t*)(o + 8); v10 = *(uint64_t*)(o + 0x10); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        Log("TRACE Post vtbl=+%llX id=%u (0x%X) w10=%08X w14=%04X ret=%p", (unsigned long long)(vt - B), id, id, (uint32_t)v10, (uint32_t)(v10 >> 32) & 0xFFFF, (void*)((uintptr_t)__builtin_return_address(0) - B)); }
    o_post(reg, pair);
}
// ---- command channel: worker thread reads the file, game thread executes
static char g_cmd[4096];
static volatile LONG g_have;

typedef void (*LogFn)(const char*, ...);
typedef void (*PlugFn)(uintptr_t, const char*, LogFn);
static PlugFn g_plug;
// ---- spawn <charId> [dist] : Load::Character request (3F9E58 id,2,0), wait until resource entry +2C8 is ready, then CreateDynamic
static int g_cfg_ownwait;
static void SummonFinish(uint32_t h, uint32_t bgm);
static struct { uint32_t id, owner, bgm; float dist; DWORD t0, tOwn; int active, boss; } g_sp;
static uint64_t ResEntry(uint32_t id) {
    uint64_t key = ((uint64_t (*)(uint64_t, uint32_t))(B + 0x134314))(0, id);
    uint64_t rm = 0; if (!Rd64(B + 0x45B9E60, &rm) || !rm) return 0;
    return ((uint64_t (*)(uint64_t, uint64_t))(B + 0x133440))(rm, key);
}
static uint32_t g_force_owner;  // debug (bridge 4878): ForceOwner / 'forceowner N' makes PickOwner return N (1 = reproduce the no-live-owner case on any save)
static uint32_t PickOwner(void) {  // a registered placement with a live actor (not the player): enemy records first, then any live registered actor; 1 = the player (team fix below keeps the boss hostile)
    if (g_force_owner) return g_force_owner;
    uint64_t cm = 0, c0 = 0; Rd64(B + 0x45B9E40, &cm); if (!cm) return 1; Rd64(cm, &c0); if (!c0) return 1;
    uint64_t* reg = *(uint64_t**)(c0 + 0x48); uint64_t arr = reg[0], cnt = reg[1], me = PlayerChar(), any = 0;
    for (uint64_t i = 0; i < cnt; ++i) {
        uint32_t id = *(uint32_t*)(arr + i * 16); uint64_t w = *(uint64_t*)(arr + i * 16 + 8), live = 0, rec = 0;
        if (id <= 1 || id >= 0x10000000 || !w) continue;
        Rd64(w, &live); Rd64(w + 0x18, &rec);
        if (!live || live == me) continue;
        if (rec) return id;
        if (!any) any = id;  // bridge 4506: a live non-enemy registered actor (id 9528 in Rajomon) also gives the boss hostile AI; a dead/unspawned placement is refused (CreateDynamic -> 0)
    }
    return any ? (uint32_t)any : 1;
}
// ---- bridge 4878: team of a CreateDynamic boss. CreateDynamic 1C0C5E0 builds the spawn request (683FE4 init, 683F74 enqueue) and copies the OWNER actor's
// team id [[owner+0xB0]+0x10] into request+0x60 (the team every AI target check compares: same team = never a target, e.g. A23192.., 20CA86, 3273C2).
// Owner = player (no live registered actor in the area, handle 0x50000001) puts the boss on the PLAYER's team: it idles and never attacks.
// Fix: while our CreateDynamic runs with owner = player, the player's team field holds the enemy team (read back from a live enemy, else ini EnemyTeam / default);
// the request is filled synchronously inside the call, then the player's team is restored.
static int g_cfg_teamfix = 1, g_cfg_enemyteam = -1;
#define DEFAULT_ENEMY_TEAM 1
static int32_t* TeamPtr(uint64_t actor) { uint64_t c = 0; if (!actor || !Rd64(actor + 0xB0, &c) || !c) return 0; return (int32_t*)(c + 0x10); }
static int ActorTeam(uint64_t actor) { int32_t* p = TeamPtr(actor); int v = -999; if (p) { __try { v = *p; } __except (EXCEPTION_EXECUTE_HANDLER) { v = -999; } } return v; }
static int EnemyTeam(const char** src) {
    if (g_cfg_enemyteam >= 0) { *src = "ini EnemyTeam"; return g_cfg_enemyteam; }
    uint64_t me = PlayerChar(), cur[1024]; int n = Actors(cur, 1024), pt = ActorTeam(me), cnt[16] = {0}, best = -1;
    for (int i = 0; i < n; ++i) { if (cur[i] == me) continue; int t = ActorTeam(cur[i]); if (t >= 0 && t < 16 && t != pt) cnt[t]++; }
    for (int t = 0; t < 16; ++t) if (cnt[t] && (best < 0 || cnt[t] > cnt[best])) best = t;
    if (best >= 0) { *src = "live actors"; return best; }
    *src = "default"; return DEFAULT_ENEMY_TEAM;
}
static uint32_t CreateBoss(uint32_t chr, const float* p, float yaw, uint32_t owner, const char* tag) {
    uint64_t me = PlayerChar(); int32_t* tp = (owner == 1 && g_cfg_teamfix) ? TeamPtr(me) : 0; int old = 0, et = -1; const char* src = "";
    if (tp) { old = *tp; et = EnemyTeam(&src); if (et == old) { Log("%s TEAM: enemy team %d (%s) equals the player's, no override", tag, et, src); tp = 0; } }
    if (tp) *tp = et;
    uint32_t r = ((DynFn)(B + 0x1C0C5E0))(chr, p, yaw, 0, owner, 0);
    if (tp) { *tp = old; Log("%s TEAM: owner = player, boss created on enemy team %d (%s) instead of the player's team %d (player team restored)", tag, et, src, old); }
    return r;
}
// ---- boss HP bar for a CreateDynamic boss (bridge 4768). The gauge natives look the boss up by PLACEMENT id: AddBossGaugeRow 1CE7C6C -> FindWrapper 135B78 (sorted registry [c0+0x48], row = {id, wrapper*}) -> wrapper[0] = live actor.
// A dynamic boss has no registry row (its handle 0x1000xxxx is not an id there), so the gauge finds nothing or the owner placement's own actor. We hook FindWrapper: a private id FAKE_GAUGE_ID returns a copy of the owner's wrapper whose live-actor field points at the spawned boss.
#define FAKE_GAUGE_ID 0x7FFF0001u
typedef uint64_t (*FwFn)(uint64_t, uint32_t);
static FwFn o_fw; static uint64_t g_fwbuf[64]; static volatile uint64_t g_fakew;
// bridge 5437: every show uses a fresh fake id (FAKE_GAUGE_ID + n) so no HUD state left by an earlier bar can match it; hide uses the id that was shown
static volatile uint32_t g_fakeid = FAKE_GAUGE_ID, g_fakeid_shown = FAKE_GAUGE_ID;
static uint64_t h_fw(uint64_t cm, uint32_t id) { if ((id == g_fakeid || id == g_fakeid_shown) && (id & 0xFFFF0000u) == 0x7FFF0000u && g_fakew) return g_fakew; return o_fw(cm, id); }
static uint64_t ActorByHandle(uint32_t h);
static struct { int pending, shown; uint32_t handle, owner; DWORD t0, tHide; } g_gg;
// bridge 4801: the native HideBossGauge (queued event) leaves the empty frame on screen. Do what BossGaugeSetup does for unused rows by hand: clear the row vector (hud+0x3C0, 1CEE8C8),
// hide the 3 row elements (type 0xF5) and the 42 detail elements (type 0xF6) with 5D1D8C(elem,0), then hide the HUD (1CEC2E0).
static int g_hudHidden, g_noForceHide; static int g_cfg_freshid = 1, g_cfg_gaugereset = 1; static uint32_t g_gaugeN;
static void GaugeForceHide(const char* why) {
    uint64_t root = 0, hud = 0; Rd64(B + 0x45C84A8, &root); if (!root || !Rd64(root + 0xA8, &hud) || !hud) { Log("GAUGE force hide (%s): no HUD", why); return; }
    __try {
        ((void (*)(uint64_t))(B + 0x1CEE8C8))(hud + 0x3C0);
        typedef uint64_t (*ElemFn)(uint64_t, void*, uint32_t, uint64_t);
        for (int pass = 0; pass < 2; pass++) {
            uint32_t type = pass ? 0xF6 : 0xF5, cnt = pass ? 0x2A : 3;
            for (uint32_t i = 0; i < cnt; i++) {
                uint8_t tmp[256] = {0};
                uint64_t e = ((ElemFn)(B + 0x300F44))(hud + 0x140, tmp, type, i);
                ((void (*)(void*, uint8_t))(B + 0x5D1D8C))(e ? (void*)e : (void*)tmp, 0);
            }
        }
        ((void (*)(uint64_t, uint8_t))(B + 0x1CEC2E0))(hud, 0); g_hudHidden = 1;
        Log("GAUGE force hide done (%s), hud %p", why, (void*)hud);
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("GAUGE force hide exception %08lX (%s)", GetExceptionCode(), why); }
}
static DWORD g_forceHideAt;
static void FakeGaugeDrop(void) { g_fwbuf[0] = 0; g_fakew = 0; g_gg.tHide = 0; }
static void FakeGaugeHide(const char* why) {  // the native hide is a queued message that looks the id up again later: keep the fake wrapper valid for 3 s (GaugeTick drops it)
    g_gg.pending = 0; if (!g_gg.shown) return;
    ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x1C208BC))(g_fakeid_shown, 0, 0);
    g_gg.shown = 0; g_gg.tHide = GetTickCount() | 1; Log("GAUGE HideBossGauge(fake id) (%s)", why);
    GaugeForceHide(why); g_forceHideAt = GetTickCount() + 700; if (!g_forceHideAt) g_forceHideAt = 1;  // again after the queued native hide ran
}
static void FakeGaugeRequest(uint32_t handle, uint32_t owner) {  // the actor enters the table a few ms after CreateDynamic: GaugeTick finishes the job
    if (!o_fw) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x1D115D4))(handle, 0, 0); Log("GAUGE FindWrapper hook missing: legacy ShowBossGauge(0x%X)", handle); return; }
    FakeGaugeHide("new request"); FakeGaugeDrop(); g_gg.pending = 1; g_gg.handle = handle; g_gg.owner = owner; g_gg.t0 = GetTickCount();
}
// #6711: hide the boss bar the instant its boss is killed (h_cond skip or ForceKillActor), not at boss-mode end (4.5 s later). Only the actor the shown
// bar points at (g_fwbuf[0]) and only if it is really dead (hp+0x48 == 1 or HP 0). FakeGaugeHide sets shown = 0, so the later "boss mode end" hide is a no-op;
// the next FakeGaugeRequest shows the HUD again through the g_hudHidden path. No game-state change, only the HUD.
static int ActorDeadNow(uint64_t a, int* dead, uint64_t* cur, uint64_t* mx) {  // dead = hp+0x48 == 1 (death applied) or HP 0
    uint64_t h = 0; *dead = 0; *cur = 1; *mx = 0;
    __try { if (Rd64(a + 0x3A0, &h) && h) { *dead = *(int*)(h + 0x48) == 1; Rd64(h + 0x58, cur); Rd64(h + 0x50, mx); } } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return *dead || (*mx && *cur == 0);
}
static void GaugeHideOnKill(uint64_t a, const char* why) {
    if (!a) return;
    uint64_t cur = 1, mx = 0; int dead = 0;
    if (!ActorDeadNow(a, &dead, &cur, &mx)) { Log("GAUGE kill-hide (%s): actor %p not dead (hp+48 %d, HP %llu / %llu), bar kept", why, (void*)a, dead, cur, mx); return; }
    if (g_gg.shown && g_fakew && g_fwbuf[0] == a) { Log("GAUGE kill-hide (%s): boss %p dead, hiding the bar now", why, (void*)a); FakeGaugeHide("boss killed"); }
    KillFadeNow(a);  // only our summoned boss with DeathBlowRequired=0 (checked inside)
}
static void GaugeTick(void) {
    { uint64_t ka = g_killHideAct; if (ka) { g_killHideAct = 0; BossKilledNow(ka, "lethal hit, death-blow revive skipped"); } }
    if (g_forceHideAt && (int)(GetTickCount() - g_forceHideAt) >= 0) { g_forceHideAt = 0; GaugeForceHide("delayed"); }
    if (g_gg.tHide && GetTickCount() - g_gg.tHide > 3000) FakeGaugeDrop();
    if (!g_gg.pending) return;
    uint64_t a = ActorByHandle(g_gg.handle);
    if (!a) { if (GetTickCount() - g_gg.t0 > 4000) { Log("GAUGE actor 0x%X never appeared, no bar", g_gg.handle); g_gg.pending = 0; } return; }
    uint64_t cm = 0, tmpl = 0; Rd64(B + 0x45B9E40, &cm);
    if (g_gg.owner > 1 && cm) tmpl = o_fw(cm, g_gg.owner);
    memset(g_fwbuf, 0, sizeof g_fwbuf);
    if (tmpl) { __try { memcpy(g_fwbuf, (void*)tmpl, sizeof g_fwbuf); } __except (EXCEPTION_EXECUTE_HANDLER) { memset(g_fwbuf, 0, sizeof g_fwbuf); } }
    g_fwbuf[0] = a; *(uint32_t*)((uint8_t*)g_fwbuf + 0xD4) = 0;  // name text id from the actor's charParam
    g_fakew = (uint64_t)g_fwbuf;
    if (g_hudHidden) {  // bridge 4942: GaugeForceHide hid the gauge HUD (1CEC2E0(hud,0)): show it again before the new bar, else rounds 2+ have no boss bar
        uint64_t root = 0, hud = 0; Rd64(B + 0x45C84A8, &root);
        if (root && Rd64(root + 0xA8, &hud) && hud) { __try { ((void (*)(uint64_t, uint8_t))(B + 0x1CEC2E0))(hud, 1); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
        g_hudHidden = 0;
    }
    g_forceHideAt = 0;  // bridge 4942: a retry hides the old bar and shows the new one 300 ms later: the delayed force-hide of the old bar must not hit the new one
    if (g_cfg_freshid) g_fakeid = FAKE_GAUGE_ID + (++g_gaugeN & 0xFFF);
    g_fakeid_shown = g_fakeid;
    {   // bridge 5479: BossGaugeSetup 1D05824 starts with hud vf50 (1CFD6D4 -> 563754) = (hud+0x2C == 0 && hud+0x1F == 0). Show runs 70781C(hud) which sets hud+0x2C = 1 (opening);
        // only the native hide/close state machine puts it back to 0, and GaugeForceHide bypasses that, so every bar after the first was refused (rows stayed 0). Reset the idle state first.
        uint64_t root = 0, hud = 0; Rd64(B + 0x45C84A8, &root); if (root) Rd64(root + 0xA8, &hud);
        __try { if (hud) {
            uint32_t s2c = *(uint32_t*)(hud + 0x2C); uint8_t s1f = *(uint8_t*)(hud + 0x1F);
            Log("GAUGE pre-show #%u: hud+2C=%u hud+1F=%u%s", g_gaugeN, s2c, s1f, (s2c || s1f) && g_cfg_gaugereset ? " -> reset to 0" : "");
            if (g_cfg_gaugereset) { *(uint32_t*)(hud + 0x2C) = 0; *(uint8_t*)(hud + 0x1F) = 0; }
        } } __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
    ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x1D115D4))(g_fakeid, 0, 0);
    g_gg.pending = 0; g_gg.shown = 1;
    {   // bridge 5437 diag: ShowBossGauge (1D05824) is synchronous; it needs hud vfunc +0x50 true, a FindWrapper hit and 4892A8(actor) false. rows = bars added.
        uint64_t root = 0, hud = 0, r0 = 0, r1 = 0; int v50 = -1, blk = -1; Rd64(B + 0x45C84A8, &root); if (root) Rd64(root + 0xA8, &hud);
        __try { if (hud) { v50 = ((uint8_t (*)(uint64_t))(*(uint64_t*)(*(uint64_t*)hud + 0x50)))(hud); r0 = *(uint64_t*)(hud + 0x3C0); r1 = *(uint64_t*)(hud + 0x3C8); } blk = ((uint8_t (*)(uint64_t))(B + 0x4892A8))(a); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        Log("GAUGE diag #%u: hud %p vf50=%d rows=%lld actorBlocked(4892A8)=%d", g_gaugeN, (void*)hud, v50, r0 ? (long long)((r1 - r0) / 0x60) : -1LL, blk);
    }
    Log("GAUGE ShowBossGauge(fake id 0x%X) for actor %p (handle 0x%X, wrapper template owner %u -> %p)", g_fakeid, (void*)a, g_gg.handle, g_gg.owner, (void*)tmpl);
}
static int g_cfg_autoretry = 0;  // [Summon] AutoRetryOnDeath (merged with jumpfix InstantRespawn #7118: one toggle; see TrainTick / TrainLost)
static struct { int active, round, nodeath, beaten, hasStart, bodyGone, lost; uint32_t chr, bgm; char tok[32]; DWORD tCmd, tWin, tBeat, tRespawn, tReport, tFade; float pp[4], bp[3], face; } g_tr;  // pp/bp/face: round-1 player and boss spawn pose (bridge 5310)
// bridge 5467: spawn placement. No native raycast/collision query is known (see TECH_DEBT), so walkability is proven two other ways:
// (1) the player's own recent trail (0.5 s samples, 20 s): a point the player stood on is on the floor and connected to the player; (2) a post-spawn progress check
// (VerifyTick): a boss that has not closed in after 5 s is deleted and respawned at the next candidate. Candidate order: trail points 4-9 m away, then a fan around the camera
// front (Distance, 5 m, 8 m; angles 0/+-45/+-90/+-135/180), last the player's own spot offset 1.5 m. Every candidate is logged.
#define TRAILN 40
#define MAXCAND 40
static struct { float p[3]; DWORD t; } g_trail[TRAILN]; static int g_trN, g_trHead; static DWORD g_trLast; static float g_trPrev[3];
static struct { float p[3]; char kind[12]; } g_cand[MAXCAND]; static int g_ncand, g_candIdx;
static struct { int active, attempt; uint32_t h; DWORD t0, respawnAt; float d0; } g_vf;
static int g_cfg_spawnverify = 1; static DWORD g_candFor;
static void TrailTick(void) {
    DWORD now = GetTickCount(); if (now - g_trLast < 500) return; g_trLast = now;
    float pos[4]; if (!PlayerPos(pos)) return;
    float jx = pos[0] - g_trPrev[0], jz = pos[2] - g_trPrev[2];
    if (g_trN && jx * jx + jz * jz > 2500.0f * 2500.0f) g_trN = 0;  // warp / world change: the old trail is not connected
    g_trPrev[0] = pos[0]; g_trPrev[1] = pos[1]; g_trPrev[2] = pos[2];
    g_trail[g_trHead].p[0] = pos[0]; g_trail[g_trHead].p[1] = pos[1]; g_trail[g_trHead].p[2] = pos[2]; g_trail[g_trHead].t = now;
    g_trHead = (g_trHead + 1) % TRAILN; if (g_trN < TRAILN) g_trN++;
}
static void AddCand(const char* kind, float x, float y, float z) { if (g_ncand >= MAXCAND) return; g_cand[g_ncand].p[0] = x; g_cand[g_ncand].p[1] = y; g_cand[g_ncand].p[2] = z; snprintf(g_cand[g_ncand].kind, sizeof g_cand[0].kind, "%s", kind); g_ncand++; }
static void BuildCandidates(const float* pos, float camYaw, float dist) {
    g_ncand = 0; g_candIdx = 0;
    DWORD now = GetTickCount();
    for (int pass = 0; pass < 3; ++pass) {  // trail points nearest to 6 m first (at most 3, at least 1.5 m apart)
        int best = -1; float bd = 1e9f;
        for (int i = 0; i < g_trN; ++i) {
            float dx = g_trail[i].p[0] - pos[0], dz = g_trail[i].p[2] - pos[2], dy = g_trail[i].p[1] - pos[1], h = sqrtf(dx * dx + dz * dz);
            if (h < 400.0f || h > 900.0f || fabsf(dy) > 250.0f || now - g_trail[i].t > 20000) continue;
            int dup = 0; for (int k = 0; k < g_ncand; ++k) { float ex = g_cand[k].p[0] - g_trail[i].p[0], ez = g_cand[k].p[2] - g_trail[i].p[2]; if (ex * ex + ez * ez < 150.0f * 150.0f) dup = 1; }
            if (dup) continue;
            float sc = fabsf(h - 600.0f); if (sc < bd) { bd = sc; best = i; }
        }
        if (best >= 0) AddCand("trail", g_trail[best].p[0], g_trail[best].p[1], g_trail[best].p[2]);
    }
    static const float ang[8] = {0, 45, -45, 90, -90, 135, -135, 180};
    float rings[3] = {dist, 500.0f, 800.0f};
    for (int r = 0; r < 3; ++r) for (int a = 0; a < 8; ++a) {
        float dx, dz; FwdVec(camYaw + ang[a], &dx, &dz); AddCand("fan", pos[0] + dx * rings[r], pos[1], pos[2] + dz * rings[r]);
    }
    float dx, dz; FwdVec(camYaw + 180.0f, &dx, &dz); AddCand("self", pos[0] + dx * 150.0f, pos[1], pos[2] + dz * 150.0f);
    Log("SPAWNPOS %d candidates (trail %d samples, player %.0f %.0f %.0f camyaw %.1f)", g_ncand, g_trN, pos[0], pos[1], pos[2], camYaw);
    for (int i = 0; i < g_ncand && i < 12; ++i) Log("SPAWNPOS cand %d %s %.0f %.0f %.0f", i, g_cand[i].kind, g_cand[i].p[0], g_cand[i].p[1], g_cand[i].p[2]);
}
static void TrainClear(const char* why);
static void VerifyTick(void) {
    DWORD now = GetTickCount();
    if (g_vf.respawnAt && (int)(now - g_vf.respawnAt) >= 0) { g_vf.respawnAt = 0; g_sp.t0 = now; g_sp.active = 1; return; }
    if (!g_vf.active || !g_cfg_spawnverify) return;
    DWORD dt = now - g_vf.t0; if (dt < 5000) return;
    uint64_t a = ActorByHandle(g_vf.h); float pos[4];
    if (!a || !PlayerPos(pos)) { g_vf.active = 0; return; }
    float bx = *(float*)(a + 0x110), bz = *(float*)(a + 0x118), dx = bx - pos[0], dz = bz - pos[2], d = sqrtf(dx * dx + dz * dz);
    g_vf.active = 0;
    if (d > 700.0f && d > g_vf.d0 * 0.8f && g_candIdx + 1 < g_ncand && g_vf.attempt < 3) {
        Log("SPAWNPOS verify: boss 0x%X still %.0f from the player after %lu ms (start %.0f): stuck, respawn at the next candidate", g_vf.h, d, dt, g_vf.d0);
        g_candIdx++; g_vf.attempt++; TrainClear("spawn stuck");
        g_tr.hasStart = 0; g_vf.respawnAt = now + 500; return;
    }
    Log("SPAWNPOS verify: boss 0x%X %.0f from the player after %lu ms (start %.0f), attempt %d: %s", g_vf.h, d, dt, g_vf.d0, g_vf.attempt, d > 700.0f ? "not closing but out of candidates" : "ok");
}
static void SpawnTick(void) {
    if (!g_sp.active) return;
    uint64_t e = ResEntry(g_sp.id);
    DWORD dt = GetTickCount() - g_sp.t0;
    if (!e || !*(uint8_t*)(e + 0x2c8)) {
        if (dt > 30000) { Log("SPAWN %u: resources not ready after %lu ms, giving up", g_sp.id, dt); g_sp.active = 0; }
        return;
    }
    float pos[4]; uint64_t ch = PlayerChar();
    if (!PlayerPos(pos) || !ch) { Log("SPAWN %u: no player", g_sp.id); g_sp.active = 0; return; }
    // spawn dist units in front of the player (camera yaw), facing the player
    float cy = CamYawDeg(), dx, dz; FwdVec(cy, &dx, &dz);
    __attribute__((aligned(16))) float p[4] = {pos[0] + dx * g_sp.dist, pos[1], pos[2] + dz * g_sp.dist, 1.0f};
    if (g_sp.boss && g_cfg_spawnverify) {
        if (!g_vf.attempt && g_candFor != g_sp.t0) { g_candFor = g_sp.t0; BuildCandidates(pos, cy, g_sp.dist); }
        if (g_candIdx < g_ncand) { p[0] = g_cand[g_candIdx].p[0]; p[1] = g_cand[g_candIdx].p[1]; p[2] = g_cand[g_candIdx].p[2]; }
        float ddx = p[0] - pos[0], ddz = p[2] - pos[2];
        Log("SPAWNPOS chosen cand %d/%d (%s) %.0f %.0f %.0f, %.0f from the player; rejected so far: %d", g_candIdx, g_ncand, g_candIdx < g_ncand ? g_cand[g_candIdx].kind : "none", p[0], p[1], p[2], sqrtf(ddx * ddx + ddz * ddz), g_candIdx);
        float fx = pos[0] - p[0], fz = pos[2] - p[2];  // face the player
        cy = atan2f(-fx, -fz) * 57.2957795f;
    }
    uint32_t owner = g_sp.owner ? g_sp.owner : PickOwner();
    if (owner <= 1 && g_sp.boss && g_cfg_ownwait > 0 && !g_sp.tOwn) g_sp.tOwn = GetTickCount();
    if (owner <= 1 && g_sp.boss && g_sp.tOwn && GetTickCount() - g_sp.tOwn < (DWORD)g_cfg_ownwait) return;  // summon: keep retrying the owner pick
    if (owner <= 1) { Log("SPAWN WARNING no live registered actor to own the boss: owner = player"); DumpOwnerState("spawn-no-owner"); g_dump_pending = 1; g_dump_owner = owner; }
    float face = cy + 180.0f;
    if (g_tr.active && g_sp.boss) {  // bridge 5310: after "player beaten" the live player pose is a ragdoll/knock-up (Y 19756, 16260): every later round reuses the round-1 pose, no live height
        if (!g_tr.hasStart) { memcpy(g_tr.pp, pos, 4 * sizeof(float)); g_tr.bp[0] = p[0]; g_tr.bp[1] = p[1]; g_tr.bp[2] = p[2]; g_tr.face = face; g_tr.hasStart = 1; }
        else { p[0] = g_tr.bp[0]; p[1] = g_tr.bp[1]; p[2] = g_tr.bp[2]; face = g_tr.face; Log("SPAWN round %d: round-1 pose %.0f %.0f %.0f (player live %.0f %.0f %.0f)", g_tr.round, p[0], p[1], p[2], pos[0], pos[1], pos[2]); }
    }
    g_nsnap = Actors(g_snap, 1024);
    uint32_t r = CreateBoss(g_sp.id, p, face, owner, "SPAWN");
    g_watch = r != 0; g_watch_id = r; g_watch_t0 = GetTickCount();
    Log("SPAWN %u: resources ready after %lu ms; camyaw %.1f fwd %.2f %.2f; CreateDynamic(owner %u) player %.0f %.0f %.0f -> at %.0f %.0f %.0f yaw %.1f -> 0x%X%s", g_sp.id, dt, cy, dx, dz, owner, pos[0], pos[1], pos[2], p[0], p[1], p[2], face, r, r ? "" : " (refused)");
    if (r && g_autogauge) { FakeGaugeRequest(r, owner); Log("SPAWN %u: boss gauge requested for 0x%X", g_sp.id, r); }
    if (r && g_sp.boss) { SummonFinish(r, g_sp.bgm); float ex = p[0] - pos[0], ez = p[2] - pos[2]; g_vf.active = !g_tr.chr; g_vf.h = r; g_vf.t0 = GetTickCount();  /* bridge 6566: an arena session (g_tr.chr) keeps the boss at its round-1 pose; the 5 s 'stuck' verify despawned it and respawned it at a candidate spot */ g_vf.d0 = sqrtf(ex * ex + ez * ez); }
    g_sp.active = 0;
}

// ---- stage streamer (global stage mgr [45B9E58]; sub = +0x500: byte +0 world, u32 +4 main cell, u8 +0x389 init state 0..5, set +0x40 (count +0x60))
static uint64_t StageMgr(void) { uint64_t s = 0; Rd64(B + 0x45B9E58, &s); return s; }
static int StageState(void) { uint64_t s = StageMgr(); return s ? *(uint8_t*)(s + 0x500 + 0x389) : -1; }
static void StageLog(const char* tag) {
    uint64_t s = StageMgr(); if (!s) { Log("%s stage mgr null", tag); return; }
    uint64_t sub = s + 0x500; uint64_t n = *(uint64_t*)(sub + 0x60);
    char buf[700]; int k = 0;
    for (uint64_t i = 0; i < n && i < 48 && k < 600; ++i) {
        uint64_t e = ((uint64_t (*)(uint64_t, uint64_t))(B + 0xD3B9C))(sub + 0x40, i);
        if (e) k += snprintf(buf + k, sizeof buf - k, "%u%s ", *(uint32_t*)e >> 4, *(uint8_t*)(e + 0xE8) ? "*" : "");
    }
    buf[k] = 0;
    float* c = (float*)(s + 0x10B0);
    Log("%s world=0x%02X cell=%d state=%u loaded(%llu)=[%s] centre=%.0f %.0f %.0f req world=0x%02X start=%u", tag, *(uint8_t*)sub, *(int32_t*)(sub + 4),
        *(uint8_t*)(sub + 0x389), n, buf, c[0], c[1], c[2], *(uint8_t*)(B + 0x45BC428), *(uint32_t*)(B + 0x45BC42C));
}
// native Player::SetPos (VM 23AB4DC) tail for the non-riding case
// bridge 6514: the game's own loading screen widget (script UI::LoadStart / UI::LoadEnd: [45C84A8]+748h) = native fade to black + loading emblem.
static int LoadScreen(int on) {
    __try { ((void (*)(void))(B + (on ? 0x6372A0 : 0x3B1B6C)))(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("LOADSCREEN %d: exception %08lX", on, GetExceptionCode()); return 0; }
    Log("LOADSCREEN %s called", on ? "start" : "end"); return 1;
}
static int Warp(float x, float y, float z, float yaw, int full) {
    uint64_t pl = ((uint64_t (*)(int))(B + 0x20A914))(0);
    if (!pl) return 0;
    if (*(int32_t*)(pl + 0xAB84) != -1 && ((uint8_t (*)(void))(B + 0x20CF24))()) return -1;  // SetPos takes its attached/riding branch then
    __attribute__((aligned(16))) float p[4] = {x, y, z, 1.0f};
    ((void (*)(uint64_t, const float*, float))(B + 0x1C27010))(pl, p, yaw);
    uint64_t comp = *(uint64_t*)(pl + 0x3B0), ph = *(uint64_t*)(comp + 0x28); if (!ph) ph = *(uint64_t*)(comp + 0x18);
    ((void (*)(uint64_t))(*(uint64_t*)(*(uint64_t*)ph + 0xD0)))(ph);
    if (!full) return 1;
    uint64_t cm = *(uint64_t*)(B + 0x45B9E40), c0 = *(uint64_t*)cm, am = *(uint64_t*)(c0 + 0x48);
    memcpy((void*)(am + 0xD0), (void*)(pl + 0x110), 16);
    ((void (*)(void))(B + 0xA429B4))();
    ((void (*)(uint64_t))(B + 0x1C2A3CC))(pl + 0x110);
    uint64_t s = StageMgr();
    memcpy((void*)(s + 0x10B0), (void*)(pl + 0x110), 16); *(uint8_t*)(s + 0x10C0) = 1;
    ((void (*)(uint64_t, uint64_t))(B + 0x3A083C))(s + 0x500, s + 0x10B0);  // bridge 6047: rdx = centre pointer, as the native SetPos tail (23AB6C1) and 39E58D pass it
    ((uint8_t (*)(uint64_t, uint8_t))(B + 0x3B35B4))(s + 0x500, 1);
    return 1;
}

// ---- boss mode (bridge 4576): native replica of Mission::BossBattleMode (VM 23A2DCC) / ...End (23A2F70) without a VM context, and Mission::SetMissionFlag (70275C)
static int g_cfg_bossmode = 1, g_cfg_notify = 1, g_cfg_bgm = 1;
// bridge 4717: owner fallbacks when no live registered actor exists yet. (a) OwnerWaitMs: keep retrying PickOwner this long before spawning with owner = player; (b) OwnerActivate: while waiting, ActivatePlacement (1180740) one registered enemy-record placement per 250 ms (max 12) to get a live owner.
static int g_cfg_ownwait = 0, g_cfg_ownact = 0;
// ---- boss BGM (bridge 4640). Sound state object S = 5A16F8(); S+0xB4 = boss BGM id (-1 none). Natives behind the script wrappers: PlayBossBGM 29057EC(-, hash), StopBossBGM 59FFDC, RequestBossBattleBgmEndState 2906098.
// The field music is not touched, so StopBossBGM hands it back. Per-boss hash = first Sound::PlayBossBGM constant of the boss script (Nioh3Training_bgm.tsv); bosses without one keep the automatic boss music.
static uint32_t g_ar_bgm; static DWORD g_bgm_stop_at; static int g_bgm_on;
static void BgmStart(uint32_t h) {
    ((void (*)(uint32_t, uint32_t))(B + 0x29057EC))(0, h); g_bgm_on = 1; g_bgm_stop_at = 0;
    uint64_t S = ((uint64_t (*)(void))(B + 0x5A16F8))(); Log("BGM PlayBossBGM(0x%08X) called, state %p +B4=0x%08X", h, (void*)S, S ? *(uint32_t*)(S + 0xB4) : 0);
}
static void BgmEnd(void) {  // boss died: native end state now, plain stop after the victory sting
    if (!g_bgm_on) return;
    ((void (*)(void))(B + 0x2906098))(); g_bgm_stop_at = GetTickCount() + 6000; Log("BGM RequestBossBattleBgmEndState called, StopBossBGM in 6 s");
}
static void BgmStop(const char* why) {
    if (!g_bgm_on) return;
    ((void (*)(void))(B + 0x59FFDC))(); g_bgm_on = 0; g_bgm_stop_at = 0; Log("BGM StopBossBGM (%s): field music is back", why);
}
static void BgmTick(void) { if (g_bgm_on && g_bgm_stop_at && (int)(GetTickCount() - g_bgm_stop_at) >= 0) BgmStop("after boss end"); }
static uint64_t CharMgr(void) { uint64_t cm = 0; Rd64(B + 0x45B9E40, &cm); return cm; }
static uint64_t FindWrapper(uint32_t id) { uint64_t cm = CharMgr(); return cm ? ((uint64_t (*)(uint64_t, uint32_t))(B + 0x135B78))(cm, id) : 0; }
static struct { int active; uint32_t id; int v0, diff; DWORD t0, last, since; } g_bm;
static uint64_t ActorByHandle(uint32_t h) { uint64_t cur[1024]; int n = Actors(cur, 1024); for (int i = 0; i < n; ++i) if (*(uint32_t*)(cur[i] + 4) == h) return cur[i]; return 0; }
// live-object death predicate 4892A8 (the tail of 7E11F0 / Duel::IsAliveBoss); a dynamic boss (CreateDynamic handle) has no registered wrapper, so use the actor table
static int BossAliveSensor(uint32_t id) { uint64_t a = ActorByHandle(id); if (!a) { uint64_t w = FindWrapper(id); return w ? (int)(((uint8_t (*)(uint64_t))(B + 0x7E11F0))(w) & 1) : -1; } return (int)(((uint8_t (*)(uint64_t))(B + 0x4892A8))(a) & 1); }
static int BossModeBegin(uint32_t id, int notify) {
    uint64_t cm = CharMgr(), c0 = 0; if (!cm || !Rd64(cm, &c0) || !c0) { Log("BBM: no character manager"); return 0; }
    *(uint8_t*)(c0 + 0x26AA) = 1;
    uint64_t w = FindWrapper(id);
    if (!w) { Log("BBM %u: no registered wrapper (dynamic boss): HUD/mode steps only, no listener message", id); notify = 0; }
    if (!((uint8_t (*)(uint64_t))(B + 0x1341F8))(cm)) {
        ((void (*)(uint64_t))(B + 0x1C19E14))(cm);
        int keep = w && *(uint32_t*)(w + 0x28) == 0x4ACDF && ((uint8_t (*)(uint64_t, uint32_t))(B + 0x374184))(cm, 0xC12);
        if (!keep) { *(uint8_t*)(c0 + 0x3036) = 0; *(uint8_t*)(c0 + 0x3037) = 0; }
        ((void (*)(uint32_t))(B + 0x1C20600))(0);
        if (((uint8_t (*)(void))(B + 0x85948))() && *(uint8_t*)(c0 + 0x305A) == 0) ((void (*)(uint8_t))(B + 0x1CA885C))(1);
    } else Log("BBM %u: 1341F8 true (script lock set), HUD/mode steps skipped as the native does", id);
    if (notify && w) {  // listener message {wrapper+0x20}, as at 23A2EBD..F3B
        uint32_t v = *(uint32_t*)(w + 0x20); uint64_t out[2] = {0, 0};
        ((void (*)(uint64_t*, uint32_t*))(B + 0xE33410))(out, &v);
        uint64_t tmp[2] = {out[0], out[1]}; out[0] = out[1] = 0;
        ((void (*)(uint64_t, uint64_t*))(B + 0x66BBF4))(w, tmp);
        uint64_t obj = tmp[1];
        if (obj && InterlockedDecrement((volatile LONG*)(obj + 8)) == 0) {
            void** vt = *(void***)obj; ((void (*)(uint64_t))vt[0])(obj);
            if (InterlockedDecrement((volatile LONG*)(obj + 0xC)) == 0) ((void (*)(uint64_t))vt[1])(obj);
        }
    }
    g_bm.active = 1; g_bm.id = id; g_bm.t0 = g_bm.last = GetTickCount(); g_bm.v0 = BossAliveSensor(id); g_bm.diff = 0; g_bm.since = 0;
    Log("BBM %u: BossBattleMode applied (wrapper %p, +26AA=1, alive sensor %d, notify %d)", id, (void*)w, g_bm.v0, notify);
    return 1;
}
static void SummonFinish(uint32_t h, uint32_t bgm) { if (g_cfg_bossmode) BossModeBegin(h, g_cfg_notify); if (g_cfg_bgm && bgm) BgmStart(bgm); Log("SUMMON done: handle 0x%X", h); }
static void BossModeEnd(const char* why) {
    uint64_t cm = CharMgr(); if (!cm) return;
    if (((uint8_t (*)(uint64_t))(B + 0x1C1AE20))(cm)) ((void (*)(uint64_t))(B + 0x1C0CFB4))(cm);
    ((void (*)(uint64_t))(B + 0x1C0CEEC))(cm); ((void (*)(uint64_t))(B + 0x1C0D068))(cm);
    ((void (*)(uint64_t))(B + 0x1C0E83C))(cm); ((void (*)(uint64_t))(B + 0x9DECF8))(cm);
    Log("BBM END (%s): BossBattleModeEnd steps done", why);
}
// #6774/#6777: native kill (old Boss Rematch-only build, captures/pc/oldest_kill_2050.log 20:53:30.555-.557): the boss script's Character::ForceDropItem +
// ForceDropAmurita and BossBattleModeEnd all run in the same ms as the death; the game plays its own death animation. Ours ended boss mode / BGM only after
// "HP 0 stable 4.5 s". Now the confirmed kill (h_cond skip or ForceKillActor, drop already inside 786C84 per BossLoot) ends boss mode + BGM end state at once,
// hides the bar and starts the body fade (#6746), in that order. The 4.5 s sensor / HP-0 paths in BossTick stay as fallbacks for kills we did not see.
static DWORD g_dead_at; static uint32_t g_dead_id;
static void BossKilledNow(uint64_t a, const char* why) {
    if (!a) return;
    uint64_t cur = 1, mx = 0; int dead = 0, ended = 0;
    if (g_bm.active && ActorByHandle(g_bm.id) == a) {
        if (ActorDeadNow(a, &dead, &cur, &mx)) {
            DWORD now = GetTickCount(); g_dead_at = now; g_dead_id = g_bm.id;
            Log("BBM +%lu ms boss %u killed (%s, hp+48 %d, HP %llu / %llu): boss mode ends now, as the native kill", now - g_bm.t0, g_bm.id, why, dead, cur, mx);
            BgmEnd(); BossModeEnd("boss killed"); g_bm.active = 0; ended = 1;
        } else Log("BBM boss %u: kill (%s) not applied yet (hp+48 %d, HP %llu / %llu), boss mode kept", g_bm.id, why, dead, cur, mx);
    }
    GaugeHideOnKill(a, why);
    if (ended) FakeGaugeHide("boss mode end");  // no-op when the kill-hide above already took the bar down
}
static int ActorHp(uint64_t a, uint64_t* cur, uint64_t* max) {  // Character::GetHpRate parts: [[actor+0x3A0]+0x58] / [+0x50]
    uint64_t h = 0; *cur = *max = 0; if (!a) return 0;
    __try { if (!Rd64(a + 0x3A0, &h) || !h) return 0; return Rd64(h + 0x58, cur) && Rd64(h + 0x50, max); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static void BossTick(void) {
    if (!g_bm.active) { g_db_act = 0; return; }
    DWORD now = GetTickCount();
    // #6774: DeathBlowRequired=0 - the HP-floor kill is checked every pump frame (not only on the 250 ms tick), so the boss dies on the frame its HP hits
    // the floor like a native kill. g_db_act is only a hint (validated by ActorByHandle on the full tick below).
    int floorNow = 0; if (!g_cfg_deathblow && !g_db_done && g_db_act && g_bm.v0 >= 0) { uint64_t c = 9, m = 0; floorNow = ActorHp(g_db_act, &c, &m) && m && c <= 1; }
    if (now - g_bm.last < 250 && !floorNow) return; g_bm.last = now;
    {   // full-fight heartbeat (user play-test 12:27): boss + player HP every 2 s, one line per 25 % boss-HP step, player 0 HP
        static DWORD hbAt; static int lastPct = 101, plDead; uint64_t bc = 0, bmx = 0, pc = 0, pm = 0;
        if (now - hbAt >= 2000) { hbAt = now;
            int okb = ActorHp(ActorByHandle(g_bm.id), &bc, &bmx), okp = ActorHp(PlayerChar(), &pc, &pm);
            Log("HB +%lu ms boss HP %llu/%llu%s player HP %llu/%llu%s", now - g_bm.t0, bc, bmx, okb ? "" : " (n/a)", pc, pm, okp ? "" : " (n/a)");
            if (okb && bmx) { int pct = (int)(bc * 100 / bmx); if (pct < lastPct - 24 || pct <= 1 || pct > lastPct + 50) { Log("HB boss HP milestone %d %%", pct); lastPct = pct; } }
            if (okp && pm) { if (pc == 0 && !plDead) { plDead = 1; Log("HB player HP 0 (dead)"); } else if (pc > 0) plDead = 0; }
        }
    }
    static uint64_t lastHc, lastHm; { uint64_t c = 0, m = 0; if (ActorHp(ActorByHandle(g_bm.id), &c, &m) && m) { lastHc = c; lastHm = m; } }
    int v = BossAliveSensor(g_bm.id);
    if (g_bm.v0 < 0) { if (v >= 0) { g_bm.v0 = v; Log("BBM +%lu ms actor in table, death sensor baseline %d", now - g_bm.t0, v); } else if (now - g_bm.t0 > 8000) { Log("BBM actor never appeared, giving up"); g_bm.active = 0; } return; }
    {   // #6185 [Summon] DeathBlowRequired / CutsceneOnKill / BossLoot, summoned (ASI-spawned) bosses only. Without its mission script a summoned boss sits at HP 1
        // after the death blow (log 12:30: HP 1 / 51712, sensor 0->1 for ~2.3 s = the finisher, then 1->0 = back up) and needs a second finisher.
        //  DeathBlowRequired = 0: kill natively as soon as the boss HP reaches the floor (<= 1), no finisher needed.
        //  CutsceneOnKill = 1 (or BossLoot = 1): keep the death blow, but when the first finisher animation ends (sensor 1 -> 0 at HP <= 1, or FinisherKillTimeoutMs
        //  after it started) kill natively, so one finisher plays the final animation and the boss dies.
        //  BossLoot = 1: that native kill is EnemyForceKillWithItemDrop (drop table + Amrita); 0 = EnemyForceKill (no drop).
        static DWORD sess, finAt, killSess; static int prevV;
        if (sess != g_bm.t0) { sess = g_bm.t0; finAt = 0; prevV = v; g_db_done = 0; }
        uint64_t act = ActorByHandle(g_bm.id); int ours = 0;
        g_db_act = act;  // h_cond: lethal hits on this actor skip the death-blow revive when DeathBlowRequired=0
        for (int i = 0; i < g_nsp; ++i) if (act && g_sp_actor[i] == act) ours = 1;
        uint64_t fc = 0, fm = 0;
        { static DWORD dgAt; uint64_t dc = 9, dm = 0; if (act && ActorHp(act, &dc, &dm) && dm && dc <= 1 && now - dgAt > 5000) { dgAt = now; Log("DEATHBLOW diag: hp %llu ours %d done %d nsp %d act %p", dc, ours, g_db_done, g_nsp, (void*)act); } }
        if (!g_db_done && act && ActorHp(act, &fc, &fm) && fm && fc <= 1) {
            const char* why = 0;
            if (!g_cfg_deathblow) why = "HP floor, DeathBlowRequired=0";
            else if (g_cfg_cutscene || g_cfg_loot) {
                if (v == 1 && prevV != 1 && !finAt) { finAt = now; Log("DEATHBLOW +%lu ms boss %u at HP %llu / %llu: finisher started (sensor %d -> 1)", now - g_bm.t0, g_bm.id, fc, fm, prevV); }
                if (finAt && v == 0 && prevV == 1) why = "first finisher ended";
                else if (finAt && now - finAt >= (DWORD)g_cfg_fin_timeout) why = "finisher timeout";
            }
            if (why) { g_db_done = 1; Log("DEATHBLOW +%lu ms boss %u HP %llu / %llu: %s -> native kill (BossLoot %d)", now - g_bm.t0, g_bm.id, fc, fm, why, g_cfg_loot); ForceKillActor(act, g_cfg_loot, why); if (!g_bm.active) { g_db_act = 0; return; } }
        }
        // #6643: the raw HP 0 write (4df272c..fa17802) is gone (the game revived the boss to HP 1 anyway, and it is a suspect in a PC hard freeze).
        // The death-blow revive is now skipped inside the damage function (h_cond); this only reports if the boss is still at HP 1 1.5 s after the kill.
        static DWORD killAt; static int reported;
        if (sess != killSess) { killSess = sess; killAt = 0; reported = 0; }
        if (g_db_done && !killAt) killAt = now;
        if (g_db_done && !reported && killAt && now - killAt >= 1500 && act && ActorHp(act, &fc, &fm) && fm && fc == 1) {
            reported = 1; Log("KILL boss still at HP 1 %d ms after the native kill (death-blow hook %s)", (int)(now - killAt), o_cond ? "on" : "OFF");
        }
        prevV = v;
    }
    if (v != g_bm.v0) { if (!g_bm.since) { g_bm.since = now; Log("BBM +%lu ms alive sensor %d -> %d", now - g_bm.t0, g_bm.v0, v); } }
    else g_bm.since = 0;
    {   // user play-test 12:49: boss HP hit 0 but the sensor had already flickered at HP 1 (new baseline), so no transition ever fired and boss mode / bar stayed up: HP 0 stable for 4.5 s = dead
        static DWORD hp0Since; uint64_t zc = 1, zm = 0;
        if (ActorHp(ActorByHandle(g_bm.id), &zc, &zm) && zm && zc == 0) { if (!hp0Since) hp0Since = now; }
        else hp0Since = 0;
        if (hp0Since && now - hp0Since >= 4500) {
            hp0Since = 0; g_dead_at = now; g_dead_id = g_bm.id;
            Log("BBM +%lu ms boss %u dead (HP 0 stable 4.5 s, sensor %d)", now - g_bm.t0, g_bm.id, v);
            BgmEnd(); BossModeEnd("boss dead (HP 0)"); FakeGaugeHide("boss mode end"); g_bm.active = 0; return;
        }
    }
    if (g_bm.since && now - g_bm.since >= 1500) {
        if (v < 0 && lastHm && lastHc * 100 <= lastHm * 3) {  // user play-test: the boss left the table at <=3 % HP = it was killed (finisher done), not a player death / reset: victory path
            Log("BBM +%lu ms boss %u actor gone at %llu / %llu HP: treated as killed", now - g_bm.t0, g_bm.id, lastHc, lastHm);
            g_dead_at = now; g_dead_id = g_bm.id; BgmEnd(); BossModeEnd("boss dead (gone at low HP)"); FakeGaugeHide("boss mode end"); g_bm.active = 0; return;
        }
        if (v < 0) {  // bridge 4739: the actor left the table (player died / respawn reset the arena), the boss was not killed: no victory end state, just leave boss mode
            Log("BBM +%lu ms boss %u actor gone, not dead (player died or area reset): leaving boss mode without the victory sting", now - g_bm.t0, g_bm.id);
            BgmStop("boss gone");
            BossModeEnd("boss gone");
            FakeGaugeHide("boss mode end");
            g_bm.active = 0;
            return;
        }
        uint64_t hc = 0, hm = 0;  // bridge 5350: a sensor drift right after a fast respawn (round 2) must not end boss mode while the boss still has HP
        if (ActorHp(ActorByHandle(g_bm.id), &hc, &hm) && hm && hc > 0) { Log("BBM +%lu ms boss %u sensor %d -> %d but HP %llu / %llu: not dead, new baseline", now - g_bm.t0, g_bm.id, g_bm.v0, v, hc, hm); g_bm.v0 = v; g_bm.since = 0; return; }
        if (now - g_bm.since < 4500) return;  // HP 0 but the actor is still in the table: the finisher / break hand-off may still be running (no duel script) - keep boss mode up for 4.5 s of stable death sensor
        g_dead_at = now; g_dead_id = g_bm.id;  // bridge 5360: the only proof of a kill for the training round
        Log("BBM +%lu ms boss %u dead (sensor %d, was %d)", now - g_bm.t0, g_bm.id, v, g_bm.v0);
        BgmEnd();
        BossModeEnd("boss dead");
        FakeGaugeHide("boss mode end");
        g_bm.active = 0;
    }
}
static void SetFlag(uint32_t id, uint8_t val) {
    uint64_t mm = 0; Rd64(B + 0x47514D0, &mm);
    ((void (*)(uint64_t, uint32_t, uint8_t, uint8_t))(B + 0x70275C))(mm, id, val, 0);
    Log("FLAG SetMissionFlag(%u, %u) called (mgr %p)", id, val, (void*)mm);
}
// ---- gadget door/fog control (bridge 4718). Gadget::SetDoorControl{Normal=0,AbsoluteClose=3,AbsoluteOpen=4,...} (VM wrappers 60C7E4 / 60A45C / A52C0C, same template): build a message {vtbl 38F5468, placement id +8, mode +0x10}
// with 60C924(out, &id, &mode) -> shared pair, post it to the placement registry [c0+0x48] with 60BE48(reg, &pair), then release the pair. The registry routes it to the gadget with that placement id (no live actor wrapper needed on our side).
static int DoorCtl(uint32_t id, uint32_t mode) {
    uint64_t cm = CharMgr(), c0 = 0; if (!cm || !Rd64(cm, &c0) || !c0) { Log("DOOR: no character manager"); return 0; }
    uint64_t reg = 0; Rd64(c0 + 0x48, &reg); if (!reg) { Log("DOOR: no registry"); return 0; }
    uint64_t out[2] = {0, 0}; uint32_t m = mode;
    uint64_t* r = ((uint64_t* (*)(uint64_t*, uint32_t*, uint32_t*))(B + 0x60C924))(out, &id, &m);
    uint64_t tmp[2] = {r[0], r[1]}; r[0] = r[1] = 0;
    ((void (*)(uint64_t, uint64_t*))(B + 0x60BE48))(reg, tmp);
    uint64_t obj = tmp[1];
    if (obj && InterlockedDecrement((volatile LONG*)(obj + 8)) == 0) {
        void** vt = *(void***)obj; ((void (*)(uint64_t))vt[0])(obj);
        if (InterlockedDecrement((volatile LONG*)(obj + 0xC)) == 0) ((void (*)(uint64_t))vt[1])(obj);
    }
    Log("DOOR SetDoorControl(%u, mode %u) posted to registry %p", id, mode, (void*)reg);
    return 1;
}
static struct { int active, spawned, released; uint32_t chr; float p[4], yaw, b[4], byaw; DWORD t0, tReady, tSpawn, lastLog, tOwn, lastAct; int lastState, lastCell, nact, actIdx; } g_ar;
static int OwnerActivateOne(void) {  // next registered placement with an enemy record and no live actor -> ActivatePlacement; 1 = one was tried
    uint64_t cm = 0, c0 = 0; Rd64(B + 0x45B9E40, &cm); if (!cm) return 0; Rd64(cm, &c0); if (!c0) return 0;
    uint64_t* reg = *(uint64_t**)(c0 + 0x48); uint64_t arr = reg[0], cnt = reg[1];
    for (; (uint64_t)g_ar.actIdx < cnt; ++g_ar.actIdx) {
        uint64_t i = g_ar.actIdx; uint32_t id = *(uint32_t*)(arr + i * 16); uint64_t w = *(uint64_t*)(arr + i * 16 + 8), live = 0, rec = 0;
        if (id <= 1 || id >= 0x10000000 || !w) continue;
        Rd64(w, &live); Rd64(w + 0x18, &rec);
        if (live || !rec) continue;
        ++g_ar.actIdx; ++g_ar.nact;
        uint8_t r = ((uint8_t (*)(uint32_t, uint32_t, uint8_t, uint32_t))(B + 0x1180740))(id, 0, 0, 0);
        Log("ARENA owner-activate #%d: ActivatePlacement(%u) -> %u", g_ar.nact, id, r); return 1;
    }
    return 0;
}
static void ArenaTick(void) {
    if (!g_ar.active) return;
    DWORD now = GetTickCount(), dt = now - g_ar.t0;
    uint64_t s = StageMgr(); int st = StageState(), cell = s ? *(int32_t*)(s + 0x504) : -2;
    if (st != g_ar.lastState || cell != g_ar.lastCell) { Log("ARENA +%lu ms stage state %d -> %d, cell %d -> %d", dt, g_ar.lastState, st, g_ar.lastCell, cell); g_ar.lastState = st; g_ar.lastCell = cell; }
    float pos[4] = {0}; PlayerPos(pos);
    if (!g_ar.released) {
        Warp(g_ar.p[0], g_ar.p[1], g_ar.p[2], g_ar.yaw, 0);  // hold until the cells around the arena are in
        uint64_t e = ResEntry(g_ar.chr); int res = e && *(uint8_t*)(e + 0x2c8);
        if (st == 5 && res && dt >= 300) {
            if (!g_ar.tReady) { g_ar.tReady = now; Log("ARENA +%lu ms ready: stage state 5 and boss resources loaded", dt); StageLog("ARENA stage"); }
            if (!g_ar.spawned) {
                __attribute__((aligned(16))) float b[4] = {g_ar.b[0], g_ar.b[1], g_ar.b[2], 1.0f};
                uint32_t owner = PickOwner();
                if (owner <= 1 && (g_cfg_ownwait > 0 || g_cfg_ownact)) {
                    DWORD wait = g_cfg_ownwait > 0 ? (DWORD)g_cfg_ownwait : 4000;
                    if (!g_ar.tOwn) { g_ar.tOwn = now; Log("ARENA +%lu ms no live owner yet: waiting up to %lu ms (OwnerActivate %d)", dt, wait, g_cfg_ownact); }
                    if (g_cfg_ownact && g_ar.nact < 12 && now - g_ar.lastAct >= 250) { g_ar.lastAct = now; OwnerActivateOne(); }
                    if (now - g_ar.tOwn < wait) goto owner_wait;
                    Log("ARENA owner wait over after %lu ms", now - g_ar.tOwn);
                } else if (owner > 1 && g_ar.tOwn) Log("ARENA owner %u found %lu ms after the wait began", owner, now - g_ar.tOwn);
                if (owner <= 1) { Log("ARENA WARNING no live registered actor to own the boss: owner = player (%s)", g_cfg_teamfix ? "TeamFix 1: boss created on the enemy team" : "TeamFix 0: the boss will be passive"); DumpOwnerState("arena-no-owner"); g_dump_pending = 1; g_dump_owner = owner; }
                g_nsnap = Actors(g_snap, 1024);
                uint32_t r = CreateBoss(g_ar.chr, b, g_ar.byaw, owner, "ARENA");
                g_watch = r != 0; g_watch_id = r; g_watch_t0 = now; g_ar.spawned = 1; g_ar.tSpawn = now;
                Log("ARENA +%lu ms CreateDynamic(%u owner %u) at %.0f %.0f %.0f yaw %.1f -> 0x%X", dt, g_ar.chr, owner, b[0], b[1], b[2], g_ar.byaw, r);
                if (r && g_autogauge) FakeGaugeRequest(r, owner);
                if (r && g_cfg_bossmode) BossModeBegin(r, g_cfg_notify);
                if (r && g_cfg_bgm && g_ar_bgm) BgmStart(g_ar_bgm);
            }
            owner_wait:
            if (g_ar.spawned && now - g_ar.tSpawn >= 200) { g_ar.released = 1; Log("ARENA +%lu ms CONTROL: player released at %.0f %.0f %.0f (command -> control %.2f s)", dt, pos[0], pos[1], pos[2], dt / 1000.0); }
        } else if (dt > 30000) { Log("ARENA +%lu ms timeout: state %d res %d - releasing", dt, st, res); g_ar.released = 1; }
    }
    if (now - g_ar.lastLog >= 500) { g_ar.lastLog = now; Log("ARENA +%lu ms player %.0f %.0f %.0f state %d", dt, pos[0], pos[1], pos[2], st); }
    if (g_ar.released && dt > 15000 + (g_ar.tReady ? g_ar.tReady - g_ar.t0 : 0)) { Log("ARENA monitor end"); g_ar.active = 0; }
}

static int g_cfg_enable = 1, g_cfg_preload = 1, g_cfg_preload_max = 3, g_cfg_hook = 0, g_cfg_ambient = 0, g_cfg_bossdist = 900, g_cfg_anchor = 1;
static int StartArena(uint32_t chr, const float* p, const float* b, float byaw) {
    BgmStop("new swap");
    for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i]) Despawn(i);  // one swapped-in boss at a time
    memset(&g_ar, 0, sizeof g_ar);
    g_ar.chr = chr; memcpy(g_ar.p, p, 3 * sizeof(float)); g_ar.yaw = p[3]; memcpy(g_ar.b, b, 3 * sizeof(float)); g_ar.byaw = byaw;
    g_ar.t0 = GetTickCount(); g_ar.lastState = -9; g_ar.lastCell = -9;
    StageLog("ARENA before");
    uint64_t e = ResEntry(chr);
    if (!e || !*(uint8_t*)(e + 0x2c8)) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))(chr, 2, 0); PoolAdd(chr); }
    int r = Warp(g_ar.p[0], g_ar.p[1], g_ar.p[2], g_ar.yaw, 1);
    Log("ARENA start chr %u player -> %.0f %.0f %.0f yaw %.1f boss -> %.0f %.0f %.0f: warp %d (+%lu ms), resources %s", chr, g_ar.p[0], g_ar.p[1], g_ar.p[2], g_ar.yaw, g_ar.b[0], g_ar.b[1], g_ar.b[2], r, GetTickCount() - g_ar.t0,
        e && *(uint8_t*)(e + 0x2c8) ? "preloaded" : "loading");
    g_ar.active = r == 1;
    return r == 1;
}

// ---- arena table (Nioh3Training_arenas.tsv, tools/gen_arena_table.py): mission, start, world, char, placement, player x y z yaw, boss x y z, kind, id, name
typedef struct { uint32_t mission, start, chr, placement, bgm; uint8_t world; float p[4], b[3]; char kind[8], id[16], name[64]; uint32_t cells[32]; int ncells; } ArenaRow;  // cells: optional 16th tsv column (comma list) = CrossWorldCellSkip whitelist
static ArenaRow g_rows[160]; static int g_nrows;
static void LoadTable(void) {
    char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\Nioh3Training_arenas.tsv", g_dir);
    FILE* f = fopen(path, "r"); if (!f) { Log("arena table %s not found: Fight now keeps the native load", path); return; }
    char line[512];
    while (fgets(line, sizeof line, f) && g_nrows < 160) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char* col[16]; int n = 0; char* c = line;
        while (n < 16) { col[n++] = c; char* t = strchr(c, '\t'); if (!t) break; *t = 0; c = t + 1; }
        if (n < 15) continue;
        char* nl = strpbrk(col[n - 1], "\r\n"); if (nl) *nl = 0;
        ArenaRow* r = &g_rows[g_nrows];
        r->mission = (uint32_t)strtoul(col[0], 0, 0); r->start = (uint32_t)strtoul(col[1], 0, 0); r->world = (uint8_t)strtoul(col[2], 0, 0);
        r->chr = (uint32_t)strtoul(col[3], 0, 0); r->placement = (uint32_t)strtoul(col[4], 0, 0);
        for (int i = 0; i < 4; ++i) r->p[i] = (float)atof(col[5 + i]);
        for (int i = 0; i < 3; ++i) r->b[i] = (float)atof(col[9 + i]);
        snprintf(r->kind, sizeof r->kind, "%s", col[12]); snprintf(r->id, sizeof r->id, "%s", col[13]); snprintf(r->name, sizeof r->name, "%s", col[14]);
        r->ncells = 0;  // optional col 16: comma list of arena cell ids (CrossWorldCellSkip whitelist)
        if (n > 15) for (const char* q = col[15]; *q && r->ncells < 32; ) { char* e; unsigned long v = strtoul(q, &e, 0); if (e == q) { ++q; continue; } r->cells[r->ncells++] = (uint32_t)v; q = e; }
        if (r->mission && r->chr) ++g_nrows;
    }
    fclose(f);
    snprintf(path, sizeof path, "%s\\Nioh3Training_bgm.tsv", g_dir);  // mission<TAB>0xHASH
    int nb = 0; if ((f = fopen(path, "r"))) {
        while (fgets(line, sizeof line, f)) {
            if (line[0] == '#' || line[0] == '\n') continue;
            char* t = strchr(line, '\t'); if (!t) continue; *t = 0;
            uint32_t m = (uint32_t)strtoul(line, 0, 0), h = (uint32_t)strtoul(t + 1, 0, 0);
            for (int i = 0; i < g_nrows; ++i) if (g_rows[i].mission == m && h) { g_rows[i].bgm = h; ++nb; }
        }
        fclose(f);
    }
    Log("arena table: %d bosses, %d with a boss BGM id", g_nrows, nb);
}
static ArenaRow* FindRow(uint32_t mission) { for (int i = 0; i < g_nrows; ++i) if (g_rows[i].mission == mission) return &g_rows[i]; return 0; }
static int CurWorld(void) { uint64_t s = StageMgr(); return s ? *(uint8_t*)(s + 0x500) : -1; }
static const uint8_t* GameState(void) { uint64_t gm = 0; Rd64(B + 0x4759910, &gm); return gm ? (const uint8_t*)(gm + 0x588) : 0; }  // [2] loading, [3]/[4] paused

// ---- bridge 4942: no-load training session. 'train <R-id|charId> [dist]' summons the boss next to the player in whatever map is loaded (any world: only the
// boss's character resources are loaded, no mission/stage load), keeps the player alive (native Character::SetNoDeath replica on the player's wrapper), and resets the round
// without a load: player beaten (HP at the floor) or 'retry' -> despawn the boss, Player::RecoverFull replica, summon the same boss again (resources stay loaded = instant).
// Boss killed -> victory end state; RespawnOnWinMs > 0 respawns it after that many ms. 'trainstop' ends the session (NoDeath off, boss removed).
static int g_cfg_tr_dist = 300, g_cfg_tr_win = 0, g_cfg_tr_nodeath = 1, g_cfg_fn_summon = 1, g_cfg_fn_arena = 1, g_cfg_body_fade = 1, g_cfg_body_delay = 25000;
// #6774: native kill = the boss death animation plays out in full. The kill-time fade (DeathBlowRequired=0) was f30 (0.5 s) and hid the death animation:
// KillFadeFrames (default 120 = 2 s at 60 fps) still starts at the kill but lets the collapse be seen while the body fades. Delete waits for the fade end.
static int g_cfg_kill_fade_frames = 120;
static DWORD FadeDeleteMs(void) { DWORD f = (DWORD)(g_cfg_kill_fade_frames > 30 ? g_cfg_kill_fade_frames : 30) * 1000 / 60 + 1000; return f > 6000 ? f : 6000; }
static void KillFadeNow(uint64_t a) {
    // #6746: DeathBlowRequired=0 -> start the body fade at the kill too (not BodyFadeDelayMs after the 4.5 s "HP 0 stable" win detection). Same fade + 6 s delete as below.
    if (!g_cfg_deathblow && g_cfg_body_fade && g_tr.active && !g_tr.tFade && !g_tr.bodyGone && g_nsp > 0 && g_sp_actor[g_nsp - 1] == a) {
        Log("TRAIN round %d: fading out the dead boss body at once (DeathBlowRequired=0)", g_tr.round);
        if (FadeOutActor(g_nsp - 1, (float)(g_cfg_kill_fade_frames > 0 ? g_cfg_kill_fade_frames : 30))) g_tr.tFade = GetTickCount();
    }
}
static uint64_t PlayerWrapper(void) {  // registry row whose live actor is the player
    uint64_t cm = CharMgr(), c0 = 0, me = PlayerChar(); if (!cm || !Rd64(cm, &c0) || !c0 || !me) return 0;
    uint64_t* reg = *(uint64_t**)(c0 + 0x48); uint64_t arr = reg[0], cnt = reg[1];
    for (uint64_t i = 0; i < cnt; ++i) { uint64_t w = *(uint64_t*)(arr + i * 16 + 8), live = 0; if (w && Rd64(w, &live) && live == me) return w; }
    return 0;
}
static int PlayerSetNoDeath(int on) {  // Character::SetNoDeath (VM 239711C): msg = 2386460(&out, &flag), 66BBF4(wrapper, &msg), release
    uint64_t w = PlayerWrapper(); if (!w) { Log("TRAIN NoDeath %d: no player wrapper in the registry", on); return 0; }
    uint8_t flag = on ? 1 : 0; uint64_t out[2] = {0, 0};
    ((uint64_t* (*)(uint64_t*, uint8_t*))(B + 0x2386460))(out, &flag);
    uint64_t tmp[2] = {out[0], out[1]}; out[0] = out[1] = 0;
    ((void (*)(uint64_t, uint64_t*))(B + 0x66BBF4))(w, tmp);
    uint64_t obj = tmp[1];
    if (obj && InterlockedDecrement((volatile LONG*)(obj + 8)) == 0) {
        void** vt = *(void***)obj; ((void (*)(uint64_t))vt[0])(obj);
        if (InterlockedDecrement((volatile LONG*)(obj + 0xC)) == 0) ((void (*)(uint64_t))vt[1])(obj);
    }
    g_tr.nodeath = on; Log("TRAIN player NoDeath %d (wrapper %p)", on, (void*)w); return 1;
}
static int PlayerHp(uint64_t* cur, uint64_t* max) {  // Character::GetHpRate (323BB4): [[actor+0x3A0]+0x58] / [+0x50]
    uint64_t me = PlayerChar(), h = 0; if (!me || !Rd64(me + 0x3A0, &h) || !h) return 0;
    return Rd64(h + 0x58, cur) && Rd64(h + 0x50, max);
}
static void PlayerRecoverFull(void) {  // Player::RecoverFull (VM 23AAB04)
    uint64_t p = ((uint64_t (*)(uint32_t))(B + 0x20A914))(0);
    if (p) ((void (*)(uint64_t, uint64_t, uint64_t))(B + 0x54AFBC))(CharMgr(), 0, 0);
    p = ((uint64_t (*)(uint32_t))(B + 0x20A914))(0);
    if (p) ((void (*)(uint64_t))(B + 0x204E038))(p);
    uint64_t c = 0, m = 0; PlayerHp(&c, &m); Log("TRAIN player RecoverFull: HP %llu / %llu", c, m);
}
// bridge 5158: release of the resource refs the ASI took with 3F9E58(chr,2,0). 1C1DF80(chr,0) = 3F9900(chr,2,0), exact inverse, one call per ref we took.
// Pool of PoolMax characters: when a new boss needs a ref and the pool is full, the oldest chr that is not the current boss is released one tick later (actor already deleted by Despawn).
#define POOLMAX 8
static struct { uint32_t chr; int refs; DWORD t; } g_pool[POOLMAX]; static int g_cfg_pool = 3;
static void PoolAdd(uint32_t chr) {
    for (int i = 0; i < POOLMAX; ++i) if (g_pool[i].chr == chr) { g_pool[i].refs++; g_pool[i].t = GetTickCount(); return; }
    for (int i = 0; i < POOLMAX; ++i) if (!g_pool[i].chr) { g_pool[i].chr = chr; g_pool[i].refs = 1; g_pool[i].t = GetTickCount(); return; }
}
static void PoolTick(uint32_t cur) {  // game thread; keep at most g_cfg_pool loaded chars, never release the current boss
    if (g_cfg_pool <= 0) return;
    int n = 0, old = -1; for (int i = 0; i < POOLMAX; ++i) if (g_pool[i].chr) { n++; if (g_pool[i].chr != cur && (old < 0 || g_pool[i].t < g_pool[old].t)) old = i; }
    if (n <= g_cfg_pool || old < 0 || GetTickCount() - g_pool[old].t < 1500) return;
    uint32_t c = g_pool[old].chr; int r = g_pool[old].refs; g_pool[old].chr = 0; g_pool[old].refs = 0;
    for (int k = 0; k < r; ++k) ((void (*)(uint32_t, uint32_t))(B + 0x1C1DF80))(c, 0);
    Log("POOL: released char %u (%d ref(s)) via 1C1DF80, pool was %d (max %d)", c, r, n, g_cfg_pool);
}
static int SummonStart(const char* tok, float d) {
    if (g_sp.active) { Log("SUMMON %s: previous spawn %u still loading", tok, g_sp.id); return 0; }
    uint32_t chr = 0, bgm = 0;
    for (int i = 0; i < g_nrows; ++i) if (!_stricmp(g_rows[i].id, tok)) { chr = g_rows[i].chr; bgm = g_rows[i].bgm; break; }
    if (!chr) chr = (uint32_t)strtoul(tok, 0, 0);
    if (!chr) { Log("SUMMON: unknown boss '%s'", tok); return 0; }
    if (g_bm.active) BossModeEnd("new summon");
    BgmStop("new summon"); g_vf.attempt = 0; g_vf.active = 0; g_vf.respawnAt = 0;
    g_sp.id = chr; g_sp.dist = d; g_sp.owner = 0; g_sp.boss = 1; g_sp.bgm = bgm; g_sp.tOwn = 0; g_sp.t0 = GetTickCount();
    uint64_t e = ResEntry(chr);
    if (!e || !*(uint8_t*)(e + 0x2c8)) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))(chr, 2, 0); PoolAdd(chr); }
    Log("SUMMON %s -> char %u dist %.0f world 0x%02X: %s", tok, chr, d, CurWorld(), e && *(uint8_t*)(e + 0x2c8) ? "resources already loaded" : "Load::Character requested");
    g_sp.active = 1; SpawnTick();
    return 1;
}
static void TrainClear(const char* why) {  // remove our boss and its HUD/BGM state
    if (g_bm.active) { BossModeEnd(why); g_bm.active = 0; }
    BgmStop(why); FakeGaugeHide(why);  // the force-hide inside is required: without it the HUD gauge row keeps reading the despawned actor (crash 0xC0000005 at 4893CC, bridge 4942 test)
    for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i]) Despawn(i);
    g_nsp = 0; g_watch = 0; g_mon.a = 0;
}
static int TrainStart(const char* tok, float d) {
    if (GameState() && GameState()[2]) { Log("TRAIN %s: a load is running, refused", tok); return 0; }
    TrainClear("train start");
    memset(&g_tr, 0, sizeof g_tr); snprintf(g_tr.tok, sizeof g_tr.tok, "%s", tok); g_tr.tCmd = GetTickCount(); g_tr.round = 1;
    if (g_cfg_tr_nodeath) PlayerSetNoDeath(1);
    g_tr.tRespawn = GetTickCount() + 300;  // the native delete of the previous boss is applied on a later frame: spawn shortly after
    g_tr.active = 1; Log("TRAIN start %s dist %.0f (round 1)", tok, d); g_cfg_tr_dist = (int)d;
    return 1;
}
static int g_cfg_tr_respawn = 2000;
// user #7118 / round-2: [Summon] AutoRetryOnDeath (g_cfg_autoretry, default 0; legacy key InstantRespawn read as fallback). 0 = player beaten -> TrainLost: the boss is
// removed (boss mode / BGM / bar ended), player healed and put back on the round-1 spot, NO respawn: the Return gate appears. 1 = instant round reset (TrainRetry).
static void TrainRetry(const char* why) {
    if (!g_tr.active) { Log("TRAIN retry (%s): no session", why); return; }
    TrainClear(why); PlayerRecoverFull(); if (g_cfg_tr_nodeath && !g_tr.nodeath) PlayerSetNoDeath(1);
    if (g_tr.hasStart) { int w = Warp(g_tr.pp[0], g_tr.pp[1], g_tr.pp[2], g_tr.pp[3], 1); Log("TRAIN retry: player back to the round-1 position %.0f %.0f %.0f -> %d", g_tr.pp[0], g_tr.pp[1], g_tr.pp[2], w); }
    g_dead_at = 0; g_dead_id = 0;
    g_tr.round++; g_tr.beaten = 0; g_tr.lost = 0; g_tr.tWin = 0; g_tr.bodyGone = 0; g_tr.tFade = 0; g_tr.tBeat = 0; g_tr.tCmd = GetTickCount(); g_tr.tRespawn = GetTickCount() + g_cfg_tr_respawn;  // bridge 6566: 300 ms after the delete the new boss kept ai+1AA8 = -1 (passive, only jumps)
    Log("TRAIN retry (%s): round %d", why, g_tr.round);
}
static void TrainLost(void) {  // AutoRetryOnDeath 0: player beaten -> boss gone, no respawn, wait for the Return gate choice
    TrainClear("player beaten"); PlayerRecoverFull(); if (g_cfg_tr_nodeath && !g_tr.nodeath) PlayerSetNoDeath(1);
    g_sp.active = 0; g_vf.active = 0; g_vf.respawnAt = 0;
    if (g_tr.hasStart) { int w = Warp(g_tr.pp[0], g_tr.pp[1], g_tr.pp[2], g_tr.pp[3], 1); Log("TRAIN lost: player back to the round-1 position %.0f %.0f %.0f -> %d", g_tr.pp[0], g_tr.pp[1], g_tr.pp[2], w); }
    g_dead_at = 0; g_dead_id = 0;
    g_tr.beaten = 0; g_tr.tBeat = 0; g_tr.tRespawn = 0; g_tr.tReport = 0; g_tr.tWin = 0; g_tr.bodyGone = 0; g_tr.tFade = 0; g_tr.lost = 1;
    Log("TRAIN round %d lost (AutoRetryOnDeath 0): boss removed, no respawn; Return gate: Return to where I was / Fight again", g_tr.round);
}
static void TrainStop(const char* why) {
    TrainClear(why); if (g_tr.nodeath) PlayerSetNoDeath(0);
    g_tr.active = 0; Log("TRAIN stop (%s)", why);
}
// bridge 5338: Fight now in the boss's own arena (in-place ARENA swap, no load) becomes a retry-able session: round 1 pose = arena player start / boss spawn, so 'player beaten' and 'retry'
// warp back into the arena and respawn the boss there (no open-world summon). NoDeath + round reset as for 'train'.
static void TrainAttachArena(const ArenaRow* r, const float* pp, const float* bp, float face) {
    if (g_tr.active && g_tr.nodeath) PlayerSetNoDeath(0);
    memset(&g_tr, 0, sizeof g_tr); snprintf(g_tr.tok, sizeof g_tr.tok, "%s", r->id); g_tr.tCmd = GetTickCount(); g_tr.round = 1; g_tr.chr = r->chr;
    memcpy(g_tr.pp, pp, 4 * sizeof(float)); memcpy(g_tr.bp, bp, 3 * sizeof(float)); g_tr.face = face; g_tr.hasStart = 1;
    if (g_cfg_tr_nodeath) PlayerSetNoDeath(1);
    g_tr.active = 1; Log("TRAIN arena session %s: round 1 pose player %.0f %.0f %.0f boss %.0f %.0f %.0f (retry stays in the arena)", r->id, pp[0], pp[1], pp[2], bp[0], bp[1], bp[2]);
}
static void TrainTick(void) {
    if (!g_tr.active) return;
    DWORD now = GetTickCount();
    if (g_tr.tRespawn && (int)(now - g_tr.tRespawn) >= 0) { g_tr.tRespawn = 0; g_tr.tReport = 1; SummonStart(g_tr.tok, (float)g_cfg_tr_dist); }
    if (g_tr.tReport && g_nsp > 0 && g_sp_actor[g_nsp - 1]) { Log("TRAIN round %d: boss in the world %lu ms after the command", g_tr.round, now - g_tr.tCmd); g_tr.tReport = 0; }
    uint64_t c = 0, m = 0;
    if (!g_tr.beaten && !g_tr.lost && PlayerHp(&c, &m) && m && (c <= 1 || c * 100 <= m)) {
        g_tr.beaten = 1; g_tr.tBeat = now; Log("TRAIN round %d: player beaten (HP %llu / %llu), %s in 2 s", g_tr.round, c, m, g_cfg_autoretry ? "resetting the round (AutoRetryOnDeath 1)" : "removing the boss for the Return gate (AutoRetryOnDeath 0)");
    }
    // AutoRetryOnDeath 1 = the 'dejavu' instant round reset (player + boss) 2 s after the player is beaten; 0 (default) = TrainLost: boss removed, player healed,
    // NO respawn, the arena Return gate (spawned only while idle, #7138) offers Return to where I was / Fight again (Fight again = the same TrainRetry)
    if (g_tr.beaten && now - g_tr.tBeat >= 2000) { if (g_cfg_autoretry) TrainRetry("player beaten"); else TrainLost(); }
    if (!g_tr.tWin && !g_tr.tRespawn && !g_sp.active && g_nsp > 0 && !g_bm.active && g_tr.tReport == 0 && now - g_tr.tCmd > 3000 && g_dead_at && g_dead_at >= g_tr.tCmd && g_dead_id) {
        g_tr.tWin = now; { uint64_t hc = 0, hm = 0; ActorHp(g_sp_actor[g_nsp - 1], &hc, &hm); Log("TRAIN round %d: boss defeated%s (death seen for handle 0x%X at +%lu ms, boss HP %llu / %llu, nsp %d)", g_tr.round, g_cfg_tr_win > 0 ? ", respawning" : " ('retry' for another round)", g_dead_id, now - g_tr.tCmd, hc, hm, g_nsp); }
    }
    if (g_tr.tWin && g_cfg_tr_win > 0 && now - g_tr.tWin >= (DWORD)g_cfg_tr_win) TrainRetry("respawn on win");
    // user play-test 13:11 (corpse stays) + #6185: the body stays on the floor BodyFadeDelayMs after the kill (finisher / death animation untouched), then fades out like
    // mission scripts (Character::FadeOut f30); native delete 6 s later frees the slot (immediately if the fade could not start). BodyFade = 0: the body stays.
    if (g_tr.tWin && g_cfg_tr_win <= 0 && !g_tr.bodyGone && !g_tr.tFade && g_cfg_body_fade && now - g_tr.tWin >= (DWORD)g_cfg_body_delay && g_nsp > 0 && g_sp_actor[g_nsp - 1]) {
        Log("TRAIN round %d: fading out the dead boss body %d ms after the kill", g_tr.round, g_cfg_body_delay);
        if (FadeOutActor(g_nsp - 1, 30.0f)) g_tr.tFade = now; else { g_tr.bodyGone = 1; Despawn(g_nsp - 1); }
    }
    if (g_tr.tFade && !g_tr.bodyGone && now - g_tr.tFade >= FadeDeleteMs()) { g_tr.bodyGone = 1; if (g_nsp > 0 && g_sp_actor[g_nsp - 1]) { Log("TRAIN round %d: deleting the faded boss body", g_tr.round); Despawn(g_nsp - 1); } }
}
// C API (bridge 4942) for Boss Rematch: Nioh3Training_Summon(missionId or charId) starts a no-load session (1 = started), _Retry() resets the round, _StopTraining() ends it.
__declspec(dllexport) int Nioh3Training_Summon(uint32_t id) {
    ArenaRow* r = FindRow(id); char tok[32];
    if (r) snprintf(tok, sizeof tok, "%s", r->id); else snprintf(tok, sizeof tok, "%u", id);
    return TrainStart(tok, (float)g_cfg_tr_dist);
}
__declspec(dllexport) void Nioh3Training_Retry(void) { TrainRetry("API"); }
__declspec(dllexport) void Nioh3Training_StopTraining(void) { TrainStop("API"); }

// ---- Fight now without a loading screen: MissionRequestById 2239600 called by an add-on (BossRematch) for a catalogued boss in the world we stand in
typedef uint64_t (*ReqFn)(uint32_t, uint8_t, uint32_t, void*);
__declspec(dllexport) int Nioh3Training_FightNow(uint32_t mission);
static ReqFn o_req;
static uint32_t g_imgSize;
static int g_cfg_xearly, g_xw_busy;
static const char* SwapGate(const ArenaRow* r) {
    const uint8_t* st = GameState();
    if (!st) return "game state unreadable";
    if (st[2] && !(g_cfg_xearly && g_xw_busy)) return "a load is running";
    if (!PlayerChar()) return "no player";
    if (CurWorld() != r->world) return "boss is in another world";
    if (StageState() != 5) return "stage streamer not ready";
    return 0;
}
static uint64_t h_req(uint32_t id, uint8_t slot, uint32_t start, void* prm) {  // opt-in HookFightNow = 1: swaps for any add-on caller (Boss Rematch 1.4.13 then reports 'native request uncertain; restart game')
    uintptr_t ret = (uintptr_t)__builtin_return_address(0);
    if (g_cfg_enable && ret - B >= g_imgSize && Nioh3Training_FightNow(id)) return 0;  // game-internal callers (story, retry) are never touched
    return o_req(id, slot, start, prm);
}
// preload: when the game pauses (the Boss Rematch menu opens) request the model resources of the nearest catalogued bosses of this world
static void PreloadNearby(const char* tag) {
    float pos[4]; if (!PlayerPos(pos)) return;
    int w = CurWorld(), done[8] = {0}, nsel = 0, idx[8];
    for (int k = 0; k < g_cfg_preload_max && k < 8; ++k) {
        int best = -1; float bd = 1e30f;
        for (int i = 0; i < g_nrows; ++i) {
            if (g_rows[i].world != w) continue;
            int dup = 0; for (int j = 0; j < nsel; ++j) if (idx[j] == i) dup = 1;
            if (dup) continue;
            float dx = g_rows[i].b[0] - pos[0], dz = g_rows[i].b[2] - pos[2], d = dx * dx + dz * dz;
            if (d < bd) { bd = d; best = i; }
        }
        if (best < 0) break;
        idx[nsel++] = best;
    }
    (void)done;
    for (int j = 0; j < nsel; ++j) {
        ArenaRow* r = &g_rows[idx[j]];
        uint64_t e = ResEntry(r->chr);
        if (e && *(uint8_t*)(e + 0x2c8)) { Log("%s: %s char %u already loaded", tag, r->id, r->chr); continue; }
        ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))(r->chr, 2, 0); PoolAdd(r->chr);
        Log("%s: requested resources of %s %s (char %u)", tag, r->id, r->name, r->chr);
    }
}
static void PreloadTick(void) {
    static int wasPaused; static DWORD readySince; static int ambientWorld = -2;
    const uint8_t* st = GameState(); if (!st) return;
    int paused = (st[3] | st[4]) && !st[2];
    static uint8_t last[6]; if (memcmp(last, st, 6)) { Log("GAMESTATE bytes %u %u %u %u %u %u", st[0], st[1], st[2], st[3], st[4], st[5]); memcpy(last, st, 6); }
    if (paused && !wasPaused && g_cfg_preload && g_cfg_enable && PlayerChar()) PreloadNearby("PRELOAD (menu opened)");
    wasPaused = paused;
    if (g_cfg_ambient && g_cfg_preload && g_cfg_enable) {  // PreloadAmbient = 1: also once per world, 4 s after the player landed
        int w = PlayerChar() && !st[2] && StageState() == 5 ? CurWorld() : -1;
        if (w < 0) readySince = 0; else if (!readySince) readySince = GetTickCount();
        if (w >= 0 && w != ambientWorld && GetTickCount() - readySince > 4000) { ambientWorld = w; PreloadNearby("PRELOAD (landed)"); }
    }
}

// ---- C API for Nioh3BossRematch (bridge 4506): call both on the game thread, resolved with GetProcAddress(GetModuleHandle("Nioh3Training.asi"), ...)
//   int  Nioh3Training_FightNow(uint32_t missionId)  1 = the boss was swapped in place (warp + live spawn, no mission load): do NOT call MissionRequestById;
//                                                    0 = not handled (other world, row not in the table, Enable=0, a load is running): do the native request.
//   void Nioh3Training_Preload(void)                 call when the Boss Rematch menu opens: requests the model resources of the nearest bosses of this world.
//   int  Nioh3Training_Version(void)
#define N3T_API __declspec(dllexport)
// bridge 4718: NativeEnter = 1: the fog gate, arena intro cutscene, boss BGM, boss gauge/HUD, clear flags and rewards are all driven by the boss's mission script, which only exists in the mission's own placement DB. So Fight now does the native mission load
// (returns 0 so the caller requests it) and, once the player has landed, warps him into the arena box (the table's player start) where the script's PlacementBox trigger starts the native fight (verified live: Yamagata, 23:12).
static int g_cfg_native = 1; static char g_native_list[256] = "0x1CBC";
static int NativeListed(uint32_t m) {  // [ArenaSwap] NativeMissions = comma list of mission ids (or *): bosses whose script trigger is verified to start from the table arena point
    if (strchr(g_native_list, '*')) return 1;
    for (const char* c = g_native_list; *c; ) { char* e; unsigned long v = strtoul(c, &e, 0); if (e != c && v == m) return 1; c = e != c ? e : c + 1; while (*c == ',' || *c == ' ') ++c; }
    return 0;
}
static struct { int active, sawLoad, warped, fellBack; ArenaRow* row; DWORD t0, tReady, tWarp; } g_en;
N3T_API int Nioh3Training_FightNow(uint32_t mission);
static void EnterTick(void) {
    if (!g_en.active) return;
    DWORD now = GetTickCount();
    const uint8_t* st = GameState(); if (!st) return;
    if (now - g_en.t0 > 300000) { Log("ENTER %s: timeout, giving up", g_en.row->id); g_en.active = 0; return; }
    ArenaRow* r = g_en.row;
    if (g_en.warped) {
        if (ActorByHandle(r->placement)) { Log("ENTER %s: native fight started (boss placement %u live, +%lu ms after the warp): intro, gate, BGM and rewards are the mission script's", r->id, r->placement, now - g_en.tWarp); g_en.active = 0; return; }
        if (now - g_en.tWarp > 2000 && st[2]) { Log("ENTER %s: the mission script started a load/cutscene %lu ms after the warp: native fight (intro first)", r->id, now - g_en.tWarp); g_en.active = 0; return; }  // Yamagata: ~10 s after the warp
        if (now - g_en.tWarp > 30000) {  // the script trigger did not fire from the table's arena point: fall back to the in-place swap (no fog/intro for this boss)
            Log("ENTER %s: no native boss/intro 30 s after the warp, falling back to the in-place swap", r->id); g_en.active = 0;
            int keep = g_cfg_native; g_cfg_native = 0; Nioh3Training_FightNow(r->mission); g_cfg_native = keep;
        }
        return;
    }
    if (st[2]) { if (!g_en.sawLoad) Log("ENTER %s: load started (+%lu ms)", r->id, now - g_en.t0); g_en.sawLoad = 1; g_en.tReady = 0; return; }
    if (!g_en.sawLoad) return;
    if (!PlayerChar() || StageState() != 5 || CurWorld() != r->world) { g_en.tReady = 0; return; }
    if (!g_en.tReady) { g_en.tReady = now; Log("ENTER %s: landed in world 0x%02X, settling", r->id, r->world); }
    if (now - g_en.tReady < 3000) return;
    int w = Warp(r->p[0], r->p[1], r->p[2], r->p[3], 1); g_en.warped = 1; g_en.tWarp = now;
    Log("ENTER %s (%s): warped into the arena %.0f %.0f %.0f yaw %.1f -> %d (+%lu ms after the request); waiting for the mission script to spawn the boss", r->id, r->name, r->p[0], r->p[1], r->p[2], r->p[3], w, now - g_en.t0);
}
// bridge 6047: stage cell streaming after a world switch. Per-cell update 31DFB8 (called for every cell of the set stage+0x500+0x40 by 31D480,
// from the per-frame stage update 39FBF4 -> 31D28C) only STARTS a cell load (type 0 cells, record +0xD5 == 0) while
//   (A) 1FBFA30(stage) + loading areas of list stage+0x19C0 < 4   (31E0B2: 83 F8 04 = cmp eax,4; 1FBFA30 = type-0 ids in stage+0x1A90 + release queue stage+0x1AA8)
//   (B) map size [stage+0x1B48] < 4                              (31E0BB: 48 83 BB 48 1B 00 00 04)
// so at most 4 cells load at once, and after a world switch the old world's releases count against the same 4 slots.
// [ArenaSwap] CrossWorldCellLoads = N (5..64) raises both imm8 to N from the world request until the new world's cells are all loaded (or 60 s), then restores 4.
static int g_cfg_xcells = 0;
// bridge 6233 (ally): [ArenaSwap] CrossWorldBlockCells = 1 clears the stage byte +0x11F1 (gate of the cell admission 3A0408) from the world request until
// stage state 5 (the main cell is then the only cell in the set, so state 4 passes without the surrounding world), restores it at state 5, or at the end.
// Default 0 = off. The boss arena may need neighbour cells: that is what the test has to show.
static int g_cfg_xblock = 0;
static struct { int on, sawReset, lastSt, patched, lastSeq, lastG, target, aoSkips, blk; DWORD t0, t5, tAll, lastLog, tSeq, tAo; uint8_t w0, last58A, blkOld; uint64_t lastN, lastR, lastF; char tag[24]; } g_xs;
// bridge 6047b (state numbers corrected in 6047c: 1 223BDB0, 2 223B3A8, 3 223B3F8, 4 223B8F0 world switch, 5 MissionLoadStep): mission load sequencer (handlers via 5A29F0, state mirrored in u32 B+0x45BA21C): 5 MissionLoadStep 223AF50 waits 1FC6674(stage+0x500) AND LoadIsLoadEnd
// 3B3424 BEFORE it consumes the request (the world switch); 8 223C4A8 waits LoadIsLoadEnd again; 0xB 223BE08 UiLoadingScreenEnd; 0xF 223C308 fixed 30/100 dt60 timer, then
// StopFlagSet(2,0) = GAMESTATE[2] cleared. LoadIsLoadEnd gate G5 = ObjectListAnyNotReady([45B9E60]) = any character resource entry with +0x2C8 == 0, i.e. the
// boss resources requested by CrossWorldPreload hold state 1 (the world switch) until the boss model is loaded (Ally: switch at +6.5 s vs +0.1 s without).
static uint32_t SeqState(void) { uint32_t v = 0xFFFFFFFF; __try { v = *(volatile uint32_t*)(B + 0x45BA21C); } __except (EXCEPTION_EXECUTE_HANDLER) { } return v; }
static int LoadGates(void) {  // bit set = that LoadIsLoadEnd gate currently blocks (read-only predicates, game thread): 1 G2 area list, 2 G3 request queue, 4 G5 resource list, 8 G7 blocker list, 16 G8 area objects
    uint64_t s = StageMgr(), rm = 0; int g = 0; if (!s) return -1;
    __try {
        if (((uint8_t (*)(uint64_t))(B + 0x8B3868))(s + 0x540)) g |= 1;
        if (((uint8_t (*)(uint64_t))(B + 0x3B3974))(s + 0x450)) g |= 2;
        if (Rd64(B + 0x45B9E60, &rm) && rm && ((uint8_t (*)(uint64_t))(B + 0x13335C))(rm)) g |= 4;
        uint32_t a = 0, b = 0; if (!((uint8_t (*)(uint32_t*, uint32_t*))(B + 0x2BE284))(&a, &b)) g |= 8;
        if (!((uint8_t (*)(uint64_t, uint8_t, int32_t))(B + 0x3B3A28))(s, 0, -1)) g |= 16;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -2; }
    return g;
}
// bridge 6047c: gate G8 = StageAreasObjectsReady 3B3A28(stage, force, area): for every loaded cell A = D3B9C(stage+0x540, i) with o = [A+0x30]:
// ready only if o+0x1F0 != 0, o+0x1EC == 0 and A+0x1C0 == 0. o+0x1EC counts the cell's placeables whose load is in flight (placeable +0x10D = 3:
// incremented in PlaceableCollisionUpdate D8E00 at D97E8/D9EC7 when the load is queued on the loader [475A420] (+0x1D8 list), decremented in 2007E4
// when it finishes). Placeable resources load through the TrySync step pool (3 threads, read-bound on the Ally), no per-frame budget in D8E00.
static int AreaObjPending(int* blocking, int* pending) {
    uint64_t s = StageMgr(), n = 0; *blocking = *pending = 0; if (!s || !Rd64(s + 0x560, &n)) return 0;
    __try {
        for (uint64_t i = 0; i < n && i < 256; ++i) {
            uint64_t a = ((uint64_t (*)(uint64_t, uint64_t))(B + 0xD3B9C))(s + 0x540, i), o; if (!a) continue;
            o = *(uint64_t*)(a + 0x30); if (!o) continue;
            int32_t c = *(int32_t*)(o + 0x1EC);
            if (!*(uint8_t*)(o + 0x1F0) || c || *(int32_t*)(a + 0x1C0)) { ++*blocking; if (c > 0) *pending += c; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
// [ArenaSwap] CrossWorldAreaObjSkipMs = N (default 0 = off): during a cross-world switch (fnow / oworld), once the TARGET world is current, stage
// init state is 5 and the mission sequencer is in state 5 (MissionLoadStep) or 8 (both wait LoadIsLoadEnd), a 3B3A28(.., area -1) result of 0 is
// reported as 1 after N ms. The placeable loads are not cancelled; they keep streaming in like during normal play. State 4 (old world, before the
// unload) and calls for a single area are never touched.
static int g_cfg_xaoskip = 0;
typedef uint8_t (*AorFn)(uint64_t, uint8_t, int32_t);
static AorFn o_aor;
static uint8_t h_aor(uint64_t s, uint8_t force, int32_t area) {
    uint8_t r = o_aor(s, force, area);
    if (r || area != -1 || g_cfg_xaoskip <= 0 || !g_xs.on || g_xs.target < 0) return r;
    __try {
        uint32_t q = *(volatile uint32_t*)(B + 0x45BA21C); uint64_t sm = StageMgr();
        if ((q != 5 && q != 8) || !sm || s != sm || *(uint8_t*)(sm + 0x500) != (uint8_t)g_xs.target || *(uint8_t*)(sm + 0x500 + 0x389) != 5) return r;
        DWORD now = GetTickCount();
        if (!g_xs.tAo) { g_xs.tAo = now; return r; }
        if (now - g_xs.tAo < (DWORD)g_cfg_xaoskip) return r;
        if (g_xs.aoSkips++ < 3) { int b, p; AreaObjPending(&b, &p); Log("XSTREAM %s +%lu ms: area-objects gate passed by CrossWorldAreaObjSkipMs %d (seq state %u, %d cells blocking, %d placeable loads in flight)", g_xs.tag, now - g_xs.t0, g_cfg_xaoskip, q, b, p); }
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return r; }
}
static int CellCapSet(int n) {  // returns 1 when both imm8 now hold n
    static const uint8_t a0[] = {0x83, 0xF8}, a1[] = {0x0F, 0x8D, 0x3D, 0x01, 0x00, 0x00}, b0[] = {0x48, 0x83, 0xBB, 0x48, 0x1B, 0x00, 0x00}, b1[] = {0x0F, 0x83, 0x2F, 0x01, 0x00, 0x00};
    uint8_t* pa = (uint8_t*)(B + 0x31E0B2); uint8_t* pb = (uint8_t*)(B + 0x31E0BB);
    if (n < 1 || n > 0x7F) return 0;
    if (memcmp(pa, a0, 2) || memcmp(pa + 3, a1, 6) || memcmp(pb, b0, 7) || memcmp(pb + 8, b1, 6)) { Log("XSTREAM cell cap: bytes at 31E0B2/31E0BB differ (game version?), not patched"); return 0; }
    if (pa[2] == n && pb[7] == n) return 1;
    DWORD old; if (!VirtualProtect(pa, 0x18, PAGE_EXECUTE_READWRITE, &old)) return 0;
    pa[2] = (uint8_t)n; pb[7] = (uint8_t)n;  // single-byte stores: the game thread sees the old or the new immediate, never a torn instruction
    VirtualProtect(pa, 0x18, old, &old); FlushInstructionCache(GetCurrentProcess(), pa, 0x18);
    Log("XSTREAM cell load cap -> %d (31E0B4 / 31E0C2)", n);
    return 1;
}
static int CellStats(uint64_t* n, uint64_t* ready, uint64_t* loading, uint64_t* relq, uint64_t* m) {
    uint64_t s = StageMgr(), a = 0, b = 0; *n = *ready = *loading = *relq = *m = 0;
    if (!s || !Rd64(s + 0x500 + 0x60, n)) return 0;
    if (Rd64(s + 0x19C0, &a) && Rd64(s + 0x19C8, &b) && b >= a && b - a < 0x10000) *loading = (b - a) / 4;
    if (Rd64(s + 0x1AA8, &a) && Rd64(s + 0x1AB0, &b) && b >= a && b - a < 0x10000) *relq = (b - a) / 4;
    Rd64(s + 0x1B48, m);
    for (uint64_t i = 0; i < *n && i < 256; ++i) {
        uint64_t e = 0;
        __try { e = ((uint64_t (*)(uint64_t, uint64_t))(B + 0xD3B9C))(s + 0x500 + 0x40, i); if (e && *(uint8_t*)(e + 0xE8)) ++*ready; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }
    return 1;
}
static void XsStart(const char* tag, int target) {  // called right before a WarpOtherWorld request
    memset(&g_xs, 0, sizeof g_xs); snprintf(g_xs.tag, sizeof g_xs.tag, "%s", tag); g_xs.target = target;
    uint64_t s = StageMgr(); g_xs.w0 = s ? *(uint8_t*)(s + 0x500) : 0xFF; g_xs.t0 = GetTickCount(); g_xs.tSeq = g_xs.t0; g_xs.lastSt = StageState(); g_xs.on = 1; g_xs.lastSeq = -9; g_xs.last58A = 0xEE; g_xs.lastG = -9;
    g_xs.patched = g_cfg_xcells > 4 && CellCapSet(g_cfg_xcells);
    if (g_cfg_xblock && s) { __try { g_xs.blkOld = *(volatile uint8_t*)(s + 0x11F1); *(volatile uint8_t*)(s + 0x11F1) = 0; g_xs.blk = 1; } __except (EXCEPTION_EXECUTE_HANDLER) { g_xs.blk = 0; } }
    Log("XSTREAM %s: CrossWorldBlockCells %d (stage +0x11F1 %s)", tag, g_cfg_xblock, g_xs.blk ? "cleared until state 5" : "not changed");
    Log("XSTREAM %s: start, world 0x%02X state %d, cell load cap %d (CrossWorldCellLoads %d)", tag, g_xs.w0, g_xs.lastSt, g_xs.patched ? g_cfg_xcells : 4, g_cfg_xcells);
}
static void XsBlkRestore(const char* why) {
    uint64_t s = StageMgr(); if (!g_xs.blk || !s) { g_xs.blk = 0; return; }
    __try { *(volatile uint8_t*)(s + 0x11F1) = g_xs.blkOld; } __except (EXCEPTION_EXECUTE_HANDLER) { }
    g_xs.blk = 0; Log("XSTREAM %s: stage +0x11F1 restored to %u (%s)", g_xs.tag, g_xs.blkOld, why);
}
static void XsEnd(const char* why) {
    if (g_xs.patched) CellCapSet(4);
    XsBlkRestore(why);
    Log("XSTREAM %s: %s; state 5 at %s%lu ms, all cells loaded at %s%lu ms (cap %d)", g_xs.tag, why, g_xs.t5 ? "+" : "never/", g_xs.t5 ? g_xs.t5 - g_xs.t0 : 0, g_xs.tAll ? "+" : "never/", g_xs.tAll ? g_xs.tAll - g_xs.t0 : 0, g_xs.patched ? g_cfg_xcells : 4);
    g_xs.on = 0; g_xs.patched = 0;
}
static int g_cfg_xpre_after = 0;
static void XsTick(void) {
    if (!g_xs.on) return;
    DWORD now = GetTickCount(), dt = now - g_xs.t0; uint64_t s = StageMgr(); if (!s) return;
    int st = StageState(); uint8_t w = *(uint8_t*)(s + 0x500);
    uint64_t n, r, f, q, m; int ok = CellStats(&n, &r, &f, &q, &m);
    if (st != 5 || w != g_xs.w0) g_xs.sawReset = 1;
    { const uint8_t* gs = GameState(); uint8_t f = gs ? gs[2] : 0xFF; uint32_t q = SeqState(); int g = LoadGates();
      if (f != g_xs.last58A || (int)q != g_xs.lastSeq || g != g_xs.lastG || dt == 0) {
          Log("XSTREAM %s +%lu ms: GAMESTATE[2] %u, mission seq state %d (was %d, %lu ms), LoadIsLoadEnd gates 0x%X (1 areas 2 stage req 4 res list 8 blockers 16 area objs) world 0x%02X stage %d",
              g_xs.tag, dt, f, (int)q, g_xs.lastSeq, now - g_xs.tSeq, g, w, st);
          if ((int)q != g_xs.lastSeq) g_xs.tSeq = now;
          if (g & 16) { int b, p; if (AreaObjPending(&b, &p)) Log("XSTREAM %s +%lu ms: area objects: %d cells blocking, %d placeable loads in flight", g_xs.tag, dt, b, p); }
          g_xs.last58A = f; g_xs.lastSeq = (int)q; g_xs.lastG = g; } }
    int chg = st != g_xs.lastSt || (ok && (n != g_xs.lastN || f != g_xs.lastF || (r != g_xs.lastR && now - g_xs.lastLog >= 250)));
    if (chg || now - g_xs.lastLog >= 1000) {
        Log("XSTREAM %s +%lu ms: world 0x%02X state %d cells %llu loaded %llu | loading areas %llu release queue %llu map 1B48 %llu", g_xs.tag, dt, w, st, n, r, f, q, m);
        g_xs.lastLog = now; g_xs.lastN = n; g_xs.lastR = r; g_xs.lastF = f;
    }
    if (st == 5 && g_xs.sawReset && !g_xs.t5) { g_xs.t5 = now; Log("XSTREAM %s: stage state 5 at +%lu ms, %llu/%llu cells loaded", g_xs.tag, dt, r, n); if (g_xs.blk && g_cfg_xblock < 2) XsBlkRestore("state 5"); }
    g_xs.lastSt = st;
    if (g_xs.t5 && now - g_xs.t5 >= 500 && ok && n && r == n && g_xs.last58A == 0) { g_xs.tAll = now; XsEnd("new world streamed"); return; }
    if (dt > 60000) XsEnd("60 s timeout");
}
// bridge 5276 (pc2 5116/5275): boss in ANOTHER world, no mission load. Only a world switch brings the other world's arena: World::WarpOtherWorld request (22399B8, mission 0, target world, start point)
// switches in ~1 s and the stage is ready (state 5) ~5-11 s later; then the normal in-place arena swap (warp + live spawn) runs. ~10 s to a fightable boss vs ~29 s native. Any failure -> native MissionRequestById.
static int g_cfg_xworld = 1, g_cfg_xpre = 1, g_xw_busy, g_cfg_xearly;
// bridge 6375: CrossWorldSettleMs = wait after 'stage ready' before the swap (was fixed 700). CrossWorldPrewarm = 1: Nioh3Training_Preload()/PreloadFor(mission) (menu opened) starts the world request for the
// likely target (ini CrossWorldPrewarmMission, else the last cross-world boss) before the player picks; Fight now for a boss of that world then just claims the running request (or is a plain same-world swap once landed).
static int g_cfg_xabbm = 1, g_cfg_xhold = 30000, g_cfg_xsettle = 700, g_cfg_xprewarm, g_xw_last_mission; static uint32_t g_cfg_xprewarm_mission;
static struct { int active, claimed, preDeferred, cancel; ArenaRow* row; DWORD t0, tReady, tClaim, tCancel; uint8_t oWorld, oJourney; uint32_t oStart, oMode; float oPos[4]; } g_xw;  // bridge 6550: o* = where the prewarm started (cancel returns there)
static struct { int active; uint8_t world, journey; uint32_t start, mode; float pos[4]; DWORD t0; } g_xr;  // bridge 6550: return trip after a cancelled prewarm
// user #6779: 'Return to where I was' (Boss Rematch menu). g_home = where the player stood before the first Fight now of this arena session (world, spot,
// running mission / journey / mode); chained Fight nows keep it, a Fight now with no arena session running (or after a mission change) takes a new one.
static struct { int valid; uint8_t world, journey; uint32_t mission, mode, start; float pos[4]; } g_home;
static struct { volatile LONG want; int active, phase; DWORD t0, tPh, tStable; } g_rh;  // RhTick: 1 cancel a running swap, 2 move, 3 cross-world trip, 4 settle + fade in
static int MsRead(uint32_t* mission, uint8_t* journey, uint32_t* mode) {  // mission state: +10 journey, +24 mode (XwStart), +28 running mission id (Boss Rematch Snap.ms28)
    uint64_t mm = 0, ms = 0; if (!Rd64(B + 0x47514D0, &mm) || !mm || !Rd64(mm + 8, &ms) || !ms) return 0;
    __try { *journey = *(uint8_t*)(ms + 0x10); *mode = *(uint32_t*)(ms + 0x24); *mission = *(uint32_t*)(ms + 0x28); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
static void HomeCapture(int world, const float* pos, const char* why) {
    uint32_t mis = 0, mode = 0; uint8_t jr = 0; if (world < 0 || !MsRead(&mis, &jr, &mode)) return;
    if (g_home.valid && g_tr.active && g_tr.hasStart && mis == g_home.mission) return;  // still in the session that started there
    g_home.valid = 1; g_home.world = (uint8_t)world; g_home.journey = jr; g_home.mode = mode; g_home.mission = mis; g_home.start = *(uint32_t*)(B + 0x45BC42C); memcpy(g_home.pos, pos, sizeof g_home.pos);
    Log("RETURN home stored (%s): world 0x%02X %.0f %.0f %.0f yaw %.1f, mission 0x%X journey %u mode %u", why, world, pos[0], pos[1], pos[2], pos[3], mis, jr, mode);
}
static void XwPreload(ArenaRow* r, const char* when) {
    uint64_t e = ResEntry(r->chr);
    if (!e || !*(uint8_t*)(e + 0x2c8)) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))(r->chr, 2, 0); PoolAdd(r->chr); Log("XWORLD %s: boss resources (char %u) requested %s", r->id, r->chr, when); }
}
// task 5116b: [ArenaSwap] CrossWorldCellSkip. 0 = off (no hook installed, exact old behaviour); 1 = log-only probe of every stage-cell request during a cross-world
// Fight now; 2 = also skip requests whose cell id is not in the arena whitelist. Hook = 3A0534(sub=[45B9E58]+0x500, cellId, deferByte, void* p16, flagByte) -> al:
// the only entry to the cell streamers 1FB460C/1FB4738 (sub+0x40; 1 caller each = 3A0534) and to the pending list sub+0x390 (state byte sub+0x389 != 5).
// Static facts (rdump 3A0534/223B8F0, pc2): the WarpOtherWorld scheduler 223B8F0 asks for ONE centre cell (3A1700) via 1FC4208/1FCEE1C/1FCECE4 (1FCEE1C ->
// 1FCE6F4(sub,...), not dumped); whether the ring cells go through 3A0534 is NOT known statically -> the probe logs the caller RVA of every request.
// Skipped requests return 1 (= the 'already queued' path of 3A0534). The window (g_xw_skip) is open only between XwStart and the swap / abort / 40 s timeout.
typedef uint8_t (*CellReqFn)(uint64_t, uint32_t, uint8_t, void*, uint8_t);
static CellReqFn o_cellreq; static int g_cfg_cellskip;
static volatile LONG g_xw_skip, g_cs_allow, g_cs_skip, g_cs_other;
static uint32_t g_cs_wl[32]; static int g_cs_nwl; static uint8_t g_cs_world;
static volatile LONG64 g_cs_ret[8]; static volatile LONG g_cs_retn[8];
static const uint32_t kCellsB1[] = {67253, 221265, 285287, 301780, 320919, 329181, 364049, 500328, 525291, 584691, 625536, 780040, 820705, 833442, 889691, 974198, 983784};  // 0xB1 Yoritomo (pc2 5116, all runs)
static uint8_t h_cellreq(uint64_t sub, uint32_t id, uint8_t defer, void* p, uint8_t flag) {
    uintptr_t ret = (uintptr_t)__builtin_return_address(0) - B;
    if (g_xw_skip) {
        uint8_t w = 0xFF; __try { w = *(volatile uint8_t*)sub; } __except (EXCEPTION_EXECUTE_HANDLER) { }
        int inwl = 0; for (int i = 0; i < g_cs_nwl; ++i) if (g_cs_wl[i] == id) { inwl = 1; break; }
        int k; for (k = 0; k < 8; ++k) { LONG64 v = g_cs_ret[k]; if (!v) v = InterlockedCompareExchange64(&g_cs_ret[k], (LONG64)ret, 0); if (!v || v == (LONG64)ret) break; }
        if (k < 8) InterlockedIncrement(&g_cs_retn[k]);
        int skip = g_cfg_cellskip >= 2 && g_cs_nwl && w == g_cs_world && !inwl;
        LONG n = skip ? InterlockedIncrement(&g_cs_skip) : w == g_cs_world ? InterlockedIncrement(&g_cs_allow) : InterlockedIncrement(&g_cs_other);
        if (n <= 64) Log("CELLREQ %s cell %u world 0x%02X (target 0x%02X) defer %u flag %u state %u wl %d caller %llX tid %lu", skip ? "SKIP" : "allow", id, w, g_cs_world, defer, flag,
                         (unsigned)*(volatile uint8_t*)(sub + 0x389), inwl, (unsigned long long)ret, GetCurrentThreadId());
        if (skip) return 1;
    }
    return o_cellreq(sub, id, defer, p, flag);
}
static void CellSkipOpen(const ArenaRow* r) {
    if (!o_cellreq) return;
    g_cs_nwl = 0;
    for (int i = 0; i < r->ncells && g_cs_nwl < 32; ++i) g_cs_wl[g_cs_nwl++] = r->cells[i];
    if (!g_cs_nwl && r->world == 0xB1) for (unsigned i = 0; i < sizeof kCellsB1 / sizeof *kCellsB1; ++i) g_cs_wl[g_cs_nwl++] = kCellsB1[i];
    g_cs_world = r->world; g_cs_allow = g_cs_skip = g_cs_other = 0;
    for (int k = 0; k < 8; ++k) { g_cs_ret[k] = 0; g_cs_retn[k] = 0; }
    Log("CELLSKIP %s: window open, mode %d (%s), world 0x%02X whitelist %d cells%s", r->id, g_cfg_cellskip, g_cfg_cellskip >= 2 ? "skip" : "log only", r->world, g_cs_nwl, g_cs_nwl ? "" : " (unknown world: nothing skipped)");
    InterlockedExchange(&g_xw_skip, 1);
}
static void CellSkipClose(const char* why) {
    if (!InterlockedExchange(&g_xw_skip, 0)) return;
    char buf[400]; int k = 0;
    for (int i = 0; i < 8 && g_cs_ret[i]; ++i) k += snprintf(buf + k, sizeof buf - k, "%llX x%ld ", (unsigned long long)g_cs_ret[i], g_cs_retn[i]);
    buf[k] = 0;
    Log("CELLSKIP window closed (%s) +%lu ms: allowed %ld, skipped %ld, other-world %ld; callers [%s]", why, GetTickCount() - g_xw.t0, g_cs_allow, g_cs_skip, g_cs_other, buf);
    StageLog("CELLSKIP close");
}
static void XwNative(const char* why) {
    ArenaRow* r = g_xw.row; g_xw.active = 0; if (g_xs.on) XsEnd("native fallback"); if (!r) return;
    __attribute__((aligned(16))) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0};
    Log("XWORLD %s: %s, native mission request 0x%X start %u", r->id, why, r->mission, r->start);
    ((ReqFn)(o_req ? (void*)o_req : (void*)(B + 0x2239600)))(r->mission, 1, r->start, p);
}
static int XwStart(ArenaRow* r, int claimed) {
    const uint8_t* st = GameState();
    if (!st || st[2] || !PlayerChar() || StageState() != 5) return 0;
    uint64_t mm = 0, ms = 0; if (!Rd64(B + 0x47514D0, &mm) || !mm || !Rd64(mm + 8, &ms) || !ms) return 0;
    __attribute__((aligned(16))) uint8_t prm[32] = {0};
    ((void (*)(void*))(B + 0x1BF7D68))(prm);
    prm[7] = 0; prm[0xA] = 0; prm[0xE] = 1;  // WarpOtherWorld, mission 0 = stay in the running mission
    ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3B28FC))(2, 0, 0);
    uint8_t journey = *(uint8_t*)(ms + 0x10); uint32_t mode = *(uint32_t*)(ms + 0x24);
    uint8_t oW = (uint8_t)CurWorld(); uint32_t oS = *(uint32_t*)(B + 0x45BC42C); float oP[4] = {0}; PlayerPos(oP);
    if (!g_rh.active) HomeCapture(oW, oP, claimed ? "cross-world Fight now" : "prewarm");  // #6779
    memset(&g_xw, 0, sizeof g_xw); g_xw.oWorld = oW; g_xw.oJourney = journey; g_xw.oStart = oS; g_xw.oMode = mode; memcpy(g_xw.oPos, oP, sizeof oP); g_xw.row = r; g_xw.t0 = GetTickCount(); g_xw.active = 1; g_xw.claimed = claimed; g_xw.tClaim = claimed ? g_xw.t0 : 0; g_xw_last_mission = (int)r->mission;
    CellSkipOpen(r);  // before the request: it may already ask for cells synchronously
    Log("XWORLD %s (%s)%s: world 0x%02X -> 0x%02X, WarpOtherWorld start %u journey %u mode %u requested", r->id, r->name, claimed ? "" : " PREWARM (menu opened, not claimed yet)", CurWorld(), r->world, r->start, journey, mode);
    XsStart(r->id, r->world);
    ((void (*)(uint8_t, uint8_t, uint32_t, uint32_t, void*))(B + 0x22399B8))(r->world, journey, r->start, mode, prm);
    if (g_cfg_xpre && g_cfg_xpre_after) { g_xw.preDeferred = 1; Log("XWORLD %s: boss resource request deferred until the world has switched (CrossWorldPreloadAfterSwitch)", r->id); }
    else if (g_cfg_xpre) XwPreload(r, "with the world request");  // pc2 5294 (bridge 6047b: this holds MissionLoadStep's LoadIsLoadEnd, i.e. the switch itself, until the model is loaded)
    return 1;
}
N3T_API int Nioh3Training_FightNow(uint32_t mission);
static int FightNowImpl(uint32_t mission);
// bridge 6550: a prewarm (menu opened) issues a REAL WarpOtherWorld: the mission sequencer switches the world even if the menu is closed without Fight now. Cancel = wait until
// the request landed, then WarpOtherWorld back to the world/start it began in and put the player back on the spot (XrTick). A claimed request is never cancelled.
static void XwAbort(const char* why) { g_xw.active = 0; g_xw.cancel = 0; CellSkipClose(why); if (g_xs.on) XsEnd(why); XsBlkRestore(why); }
static void XwCancel(const char* why) {
    if (!g_xw.active || g_xw.claimed || g_xw.cancel) return;
    g_xw.cancel = 1; g_xw.tCancel = GetTickCount(); Log("XWORLD prewarm cancel requested (%s): will return to world 0x%02X start %u when the request landed", why, g_xw.oWorld, g_xw.oStart);
}
static void XrTick(void) {
    if (!g_xr.active) return;
    DWORD now = GetTickCount(); const uint8_t* st = GameState(); if (!st) return;
    if (now - g_xr.t0 > 60000) { Log("XWORLD return: position fix not applied within 60 s (the player stays at the return start point)"); g_xr.active = 0; return; }
    if (CurWorld() != g_xr.world || !PlayerChar() || StageState() != 5 || st[2]) return;
    static DWORD tSeen; if (!tSeen) tSeen = now; if (now - tSeen < 700) return;
    { uint64_t n, r, f, q, m; if (CellStats(&n, &r, &f, &q, &m) && n && r < n && now - tSeen < 25000) return; }  // bridge 6541: put the player back only once the old world's cells (collision) are loaded, not into a half-loaded world
    tSeen = 0; g_xr.active = 0;
    int w = Warp(g_xr.pos[0], g_xr.pos[1], g_xr.pos[2], g_xr.pos[3], 1);
    Log("XWORLD return: back in world 0x%02X +%lu ms, player put back at %.0f %.0f %.0f -> %d", g_xr.world, now - g_xr.t0, g_xr.pos[0], g_xr.pos[1], g_xr.pos[2], w);
}
// bridge 6541: the start point the origin world is re-entered at. The old read of the 'current start' global was 0 (an undefined spot: the player fell to death while the old world loaded), so use the table row of that world whose arena is nearest to where the player stood
static uint32_t XwReturnStart(uint8_t world, const float* pos, uint32_t fallback) {
    uint32_t best = fallback; float bd = 1e30f;
    for (int i = 0; i < g_nrows; ++i) { ArenaRow* q = &g_rows[i]; if (q->world != world || !q->start) continue; float dx = q->p[0] - pos[0], dz = q->p[2] - pos[2], d = dx * dx + dz * dz; if (d < bd) { bd = d; best = q->start; } }
    return best;
}
static void XwCancelTick(DWORD now) {
    ArenaRow* r = g_xw.row; const uint8_t* st = GameState(); if (!st) return;
    if (now - g_xw.tCancel > 60000) { Log("XWORLD cancel: prewarm never landed in 60 s, dropped"); XwAbort("cancel timeout"); return; }
    if (CurWorld() != r->world || !PlayerChar() || StageState() != 5 || (st[2] && now - g_xw.tCancel < 20000)) { return; }  // still switching: the return can only be requested once the request is through (bridge 6541: after 20 s try anyway, a stalled load flag must not strand the player)
    { uint64_t cm = CharMgr(), c0 = 0; if (g_cfg_xabbm && cm && Rd64(cm, &c0) && c0 && *(uint8_t*)(c0 + 0x26AA)) BossModeEnd("xworld abort: native boss mode in the wrong world (bridge 6541)"); }
    if (CurWorld() == g_xw.oWorld) { XwAbort("cancel: never left the world"); return; }
    uint64_t mm = 0, ms = 0; if (!Rd64(B + 0x47514D0, &mm) || !mm || !Rd64(mm + 8, &ms) || !ms) return;
    __attribute__((aligned(16))) uint8_t prm[32] = {0};
    ((void (*)(void*))(B + 0x1BF7D68))(prm); prm[7] = 0; prm[0xA] = 0; prm[0xE] = 1;
    ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3B28FC))(2, 0, 0);
    memset(&g_xr, 0, sizeof g_xr); g_xr.world = g_xw.oWorld; g_xr.start = XwReturnStart(g_xw.oWorld, g_xw.oPos, g_xw.oStart); g_xr.t0 = now; memcpy(g_xr.pos, g_xw.oPos, sizeof g_xr.pos); g_xr.active = 1;
    Log("XWORLD cancel: prewarm landed in 0x%02X, returning to world 0x%02X start %u (WarpOtherWorld)", CurWorld(), g_xw.oWorld, g_xr.start);
    XwAbort("prewarm cancelled");
    ((void (*)(uint8_t, uint8_t, uint32_t, uint32_t, void*))(B + 0x22399B8))(g_xr.world, g_xw.oJourney, g_xr.start, g_xw.oMode, prm);
}
static void XwTick(void) {
    XrTick();
    if (!g_xw.active) return;
    if (g_xw.cancel) { XwCancelTick(GetTickCount()); return; }
    DWORD now = GetTickCount(); ArenaRow* r = g_xw.row; const uint8_t* st = GameState(); if (!st) return;
    static DWORD lastLog; if (now - lastLog >= 2000) { lastLog = now; StageLog("XWORLD wait"); Log("XWORLD wait: GAMESTATE[2]=%u PlayerChar=%llX world 0x%02X (target 0x%02X) stage %d", st[2], (unsigned long long)PlayerChar(), CurWorld(), r->world, StageState()); }  // pc 5328: stage state 4->5 vs cell loading split
    if (g_xw.preDeferred && CurWorld() == r->world) { g_xw.preDeferred = 0; XwPreload(r, "after the world switch"); Log("XWORLD %s: world switched to 0x%02X at +%lu ms (mission seq state %d, GAMESTATE[2] %u)", r->id, r->world, now - g_xw.t0, (int)SeqState(), st[2]); }
    if (!g_xw.claimed && g_xw.tReady && now - g_xw.tReady > (DWORD)g_cfg_xhold) { XwCancel("nobody claimed it in time"); return; }  // bridge 6550
    if (!g_xw.claimed && now - g_xw.t0 > 120000) { XwAbort("prewarm 120 s"); return; }
    if (g_xw.claimed && now - (g_xw.tClaim > g_xw.t0 + 20000 ? g_xw.tClaim : g_xw.t0) > 60000) {  // bridge 6541: no native fallback, but never leave the player stranded in the half-loaded target world / in a native boss mode
        Log("XWORLD %s: stage not ready 60 s after the world request (world 0x%02X target 0x%02X, PlayerChar %llX, stage %d, GAMESTATE[2] %u, EarlySwap %d), swap aborted, returning to world 0x%02X (bridge 6541)", r->id, CurWorld(), r->world, (unsigned long long)PlayerChar(), StageState(), st[2], g_cfg_xearly, g_xw.oWorld);
        g_xw.claimed = 0; XwCancel("claimed swap timed out"); return;
    }
    if (CurWorld() == r->world && PlayerChar() && StageState() == 5 && st[2] && !g_cfg_xearly) XsBlkRestore("stage 5, waiting for the load flag: cells must load (bridge 6541, CrossWorldBlockCells 2 deadlock)");
    if (CurWorld() != r->world || !PlayerChar() || StageState() != 5 || (st[2] && !g_cfg_xearly)) { g_xw.tReady = 0; return; }
    if (!g_xw.tReady) { g_xw.tReady = now; Log("XWORLD %s: landed in world 0x%02X, stage ready +%lu ms after the request%s", r->id, r->world, now - g_xw.t0, g_xw.claimed ? "" : " (prewarm, not claimed)"); }
    if (!g_xw.claimed) return;  // prewarm landed: keep the state (GAMESTATE[2] may stay 1 on Ally) until Fight now claims it; dropped by the timeout below
    if (now - g_xw.tReady < (DWORD)g_cfg_xsettle) return;
    g_xw.active = 0; CellSkipClose("arena swap");
    int sf = g_cfg_fn_summon, sn = g_cfg_native; g_cfg_fn_summon = 0; g_cfg_native = 0; g_xw_busy = 1;
    int ok = Nioh3Training_FightNow(r->mission);
    g_xw_busy = 0; g_cfg_fn_summon = sf; g_cfg_native = sn;
    Log("XWORLD %s: arena swap in the new world -> %d (+%lu ms after the request, +%lu ms after Fight now claimed it)", r->id, ok, now - g_xw.t0, now - g_xw.tClaim);
    XsBlkRestore("arena swap fired");
    if (!ok) { g_xw.active = 1; XwNative("arena swap refused"); }
}
static int FightNowImpl(uint32_t mission) {
    if (g_cfg_enable && g_cfg_xworld && !g_xw_busy && g_xw.active && !g_xw.claimed) {  // a prewarm request is running: claim it when the boss is in that world
        ArenaRow* xr = FindRow(mission);
        if (xr && xr->world == g_xw.row->world) { g_xw.row = xr; g_xw.claimed = 1; g_xw.cancel = 0; g_xw.tClaim = GetTickCount(); Log("XWORLD %s: Fight now claimed the prewarm request (running %lu ms already)", xr->id, g_xw.tClaim - g_xw.t0); return 1; }
        if (xr) { Log("XWORLD %s: boss is in another world than the prewarm, prewarm cancelled", xr->id); XwCancel("other world picked"); return 0; }
    }
    if (g_cfg_enable && g_cfg_xworld && !g_xw_busy && !g_xw.active) {
        ArenaRow* xr = FindRow(mission);
        if (xr && CurWorld() != xr->world && CurWorld() >= 0) {
            if (XwStart(xr, 1)) return 1;
            Log("XWORLD %s: request refused (load running / stage not ready), native load", xr->id);
            return 0;
        }
    }
    ArenaRow* r = g_cfg_enable ? FindRow(mission) : 0;
    int arenaFirst = g_cfg_fn_summon && g_cfg_fn_arena && r && !SwapGate(r);  // bridge 5338: the boss's own arena when we stand in its world
    if (g_cfg_fn_summon && !arenaFirst) { Log("FIGHTNOW mission 0x%X: no-load summon here (FightNowSummon=1, bridge 4942)", mission); return Nioh3Training_Summon(mission); }
    if (!r) return 0;
    if (!arenaFirst && g_cfg_native && NativeListed(mission)) {
        memset(&g_en, 0, sizeof g_en); g_en.row = r; g_en.t0 = GetTickCount(); g_en.active = 1;
        Log("FIGHTNOW %s (%s) mission 0x%X: native load + auto-enter (NativeEnter=1, listed)", r->id, r->name, mission);
        return 0;
    }
    const char* why = SwapGate(r);
    if (why) { Log("FIGHTNOW %s mission 0x%X: native load (%s)", r->id, mission, why); return 0; }
    Log("FIGHTNOW %s (%s) mission 0x%X: arena swap in place (no mission load), world 0x%02X", r->id, r->name, mission, r->world);
    float b[3] = {r->b[0], r->b[1], r->b[2]};
    float dx = b[0] - r->p[0], dz = b[2] - r->p[2], d = sqrtf(dx * dx + dz * dz);
    if (g_cfg_anchor && g_cfg_bossdist > 0 && d > g_cfg_bossdist && !strcmp(r->kind, "start")) {
        // bridge 4852: a 'start' row's boss is far from the mission start point, usually on another level. Pulling the boss in along the line puts it on the wrong floor or off a ledge (Okita fell 4 m below the player, Nuribotoke stood still: passive).
        // Keep the boss on its own catalogue floor and put the player BossDistance away from it, towards the start point, facing it (verified Ally: Okita and Nuribotoke attack within 10 s).
        float k = g_cfg_bossdist / d, pp[4]; pp[0] = b[0] - dx * k; pp[1] = b[1] + 50.0f; pp[2] = b[2] - dz * k;
        pp[3] = atan2f(dx, dz) * 57.2957795f;
        Log("FIGHTNOW %s: boss anchored at its own position %.0f %.0f %.0f, player placed %d units away at %.0f %.0f %.0f yaw %.1f", r->id, b[0], b[1], b[2], g_cfg_bossdist, pp[0], pp[1], pp[2], pp[3]);
        g_ar_bgm = r->bgm;
        int ok = StartArena(r->chr, pp, b, pp[3] + 180.0f);
        if (ok && g_cfg_fn_arena) TrainAttachArena(r, pp, b, pp[3] + 180.0f);
        return ok;
    }
    if (g_cfg_bossdist > 0 && d > g_cfg_bossdist) {  // an idle boss only notices the player within ~10 m: pull it in along the same line
        float k = g_cfg_bossdist / d;
        b[0] = r->p[0] + dx * k; b[1] = r->p[1] + (b[1] - r->p[1]) * k; b[2] = r->p[2] + dz * k;
    }
    g_ar_bgm = r->bgm;
    int ok = StartArena(r->chr, r->p, b, r->p[3] + 180.0f);
    if (ok && g_cfg_fn_arena) TrainAttachArena(r, r->p, b, r->p[3] + 180.0f);
    return ok;
}
// ---- bridge 6519: arena switch transition. Own topmost click-through layered window (like Boss Rematch's menu, no graphics hook): fades to black around the swap and holds
// until the new arena has settled (boss spawned, stage state 5, no placeable loads in flight, quiet for FadeStableMs) instead of showing the 20-35 s streaming pop-in.
// Same-world Fight now: black first (FadeOutMs), then the in-place swap, then hold, then FadeInMs. Cross-world: black from the call, hold through the world switch + swap + settle.
static int g_cfg_fade = 1, g_cfg_fade_out = 250, g_cfg_fade_in = 400, g_cfg_fade_min = 800, g_cfg_fade_stable = 500, g_cfg_fade_max = 60000;
static volatile LONG g_fd_want, g_fd_alpha;   // want: overlay target (1 = black); alpha 0..255 as drawn
static struct { int pend, phase, inside; uint32_t mission; DWORD tPend, tSwap, tStable, tWant; } g_fd;  // phase 2 = waiting for the world/swap then the settle check
static HWND g_fd_wnd, g_fd_game;
static int g_fd_ok;  // overlay available: Nioh3EngineFix.asi export (D3D, drawn in the game's own frame) or the fallback GDI window
static int (*p_osdFade)(int, int), (*p_osdAlpha)(void);
static long FdAlpha(void) { if (p_osdAlpha) { int a = p_osdAlpha(); return a < 0 ? 0 : a; } return g_fd_alpha; }
static BOOL CALLBACK FdFind(HWND h, LPARAM lp) { (void)lp;
    DWORD pid; GetWindowThreadProcessId(h, &pid); if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || h == g_fd_wnd) return TRUE;
    RECT r; GetClientRect(h, &r); if (r.right < 400 || r.bottom < 300) return TRUE;
    g_fd_game = h; return FALSE;
}
static LRESULT CALLBACK FdProc(HWND h, UINT m, WPARAM w, LPARAM l) { if (m == WM_NCHITTEST) return HTTRANSPARENT; return DefWindowProcW(h, m, w, l); }
static DWORD WINAPI FadeThread(LPVOID x) {
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = FdProc; wc.hInstance = GetModuleHandleW(0); wc.lpszClassName = L"Nioh3TrainingFade"; wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);
    g_fd_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, 0, 0, wc.hInstance, 0);
    if (!g_fd_wnd) { Log("FADE: overlay window failed (%lu), transitions off", GetLastError()); g_cfg_fade = 0; return 0; }
    g_fd_ok = 1;
    SetLayeredWindowAttributes(g_fd_wnd, 0, 0, LWA_ALPHA);
    float a = 0; DWORD last = GetTickCount(); int shown = 0, lastA = -1;
    for (;;) {
        Sleep(10); DWORD now = GetTickCount(), dt = now - last; last = now;
        float step = 255.0f * dt / (float)((g_fd_want ? g_cfg_fade_out : g_cfg_fade_in) > 0 ? (g_fd_want ? g_cfg_fade_out : g_cfg_fade_in) : 1);
        if (g_fd_want) a = a + step > 255 ? 255 : a + step; else a = a - step < 0 ? 0 : a - step;
        MSG msg; while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        int ia = (int)(a + 0.5f); g_fd_alpha = ia;
        if (ia > 0) {
            if (!g_fd_game || !IsWindow(g_fd_game)) { g_fd_game = 0; EnumWindows(FdFind, 0); }
            if (g_fd_game && !IsIconic(g_fd_game)) {
                RECT r; GetClientRect(g_fd_game, &r); POINT o = {0, 0}; ClientToScreen(g_fd_game, &o);
                if (ia != lastA) SetLayeredWindowAttributes(g_fd_wnd, 0, (BYTE)ia, LWA_ALPHA);
                SetWindowPos(g_fd_wnd, HWND_TOPMOST, o.x, o.y, r.right, r.bottom, SWP_NOACTIVATE | SWP_SHOWWINDOW); shown = 1; lastA = ia;
            }
        } else if (shown) { ShowWindow(g_fd_wnd, SW_HIDE); shown = 0; lastA = -1; }
    }
}
static void FadeWant(int on, const char* why) {
    if (!g_cfg_fade || !g_fd_ok) return;
    if (g_fd_want != on) { g_fd_want = on; g_fd.tWant = GetTickCount(); Log("FADE %s (%s), alpha %ld", on ? "to black" : "back in", why, FdAlpha()); if (p_osdFade) p_osdFade(on, on ? g_cfg_fade_out : g_cfg_fade_in); }
}
static int FadeSettled(void) {
    if (g_sp.active || g_vf.respawnAt || (g_ar.active && !g_ar.released) || StageState() != 5 || !PlayerChar()) return 0;
    int b = 0, p = 0; if (AreaObjPending(&b, &p) && (b || p)) return 0;
    return 1;
}
static int FadeInPlaceOk(uint32_t mission, ArenaRow* r) {  // same logic as FightNowImpl: will it swap here without a native load?
    if (!r) return 0;
    if (g_cfg_fn_summon) return 1;
    return !(g_cfg_native && NativeListed(mission)) && !SwapGate(r);
}
static void FadeTick(void) {
    if (!g_cfg_fade || !g_fd_ok) return;
    DWORD now = GetTickCount();
    if (g_fd.pend) {  // same-world swap waits for black (or 400 ms over the ramp if the overlay cannot draw)
        if (FdAlpha() < 255 && now - g_fd.tPend < (DWORD)g_cfg_fade_out + 400) return;
        uint32_t m = g_fd.mission; g_fd.pend = 0; g_fd.inside = 1;
        Log("FADE black (alpha %ld, %lu ms): swapping mission 0x%X", FdAlpha(), now - g_fd.tPend, m);
        int ok = FightNowImpl(m); g_fd.inside = 0;
        if (!ok) { __attribute__((aligned(16))) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0}; Log("FADE: swap refused, native request"); FadeWant(0, "swap refused"); ((ReqFn)(o_req ? (void*)o_req : (void*)(B + 0x2239600)))(m, 1, 0, p); return; }
        g_fd.phase = 2; g_fd.tSwap = now; g_fd.tStable = 0; return;
    }
    if (g_fd.phase != 2) return;
    if (now - g_fd.tSwap > (DWORD)g_cfg_fade_max) { Log("FADE: not settled after %lu ms, releasing", now - g_fd.tSwap); g_fd.phase = 0; FadeWant(0, "max hold"); return; }
    if (g_xw.active) { g_fd.tStable = 0; if (!g_xw.claimed) { g_fd.phase = 0; FadeWant(0, "prewarm only"); } return; }  // cross-world still switching / waiting for the stage
    if (!FadeSettled()) { g_fd.tStable = 0; return; }
    if (!g_fd.tStable) g_fd.tStable = now;
    if (now - g_fd.tStable < (DWORD)g_cfg_fade_stable || now - g_fd.tSwap < (DWORD)g_cfg_fade_min) return;
    Log("FADE: arena settled %lu ms after the swap (stable %lu ms)", now - g_fd.tSwap, now - g_fd.tStable);
    g_fd.phase = 0; FadeWant(0, "settled");
}
N3T_API int Nioh3Training_FightNow(uint32_t mission) {
    if (!g_xw_busy && !g_fd.inside && !g_xw.active && !g_xr.active && !g_rh.active) { float p[4] = {0}; if (PlayerPos(p)) HomeCapture(CurWorld(), p, "Fight now"); }  // #6779
    if (!g_cfg_fade || !g_fd_ok || g_xw_busy || g_fd.inside || !g_cfg_enable) return FightNowImpl(mission);
    ArenaRow* r = FindRow(mission);
    if (g_fd.pend) return 1;  // a switch is already waiting for black
    if (r && g_cfg_xworld && CurWorld() >= 0 && (CurWorld() != r->world || g_xw.active)) {  // cross-world (or claiming a prewarm)
        int ok = FightNowImpl(mission);
        if (ok && g_xw.active) { FadeWant(1, "cross-world switch"); g_fd.phase = 2; g_fd.tSwap = GetTickCount(); g_fd.tStable = 0; }
        return ok;
    }
    if (!FadeInPlaceOk(mission, r)) return FightNowImpl(mission);
    g_fd.pend = 1; g_fd.mission = mission; g_fd.tPend = GetTickCount(); g_fd.phase = 0; FadeWant(1, "same-world switch");
    return 1;
}
static int XwPrewarm(uint32_t mission, const char* tag) {
    if (!g_cfg_enable || !g_cfg_xworld || !g_cfg_xprewarm || g_xw_busy || g_xw.active) return 0;
    if (!mission) mission = g_cfg_xprewarm_mission ? g_cfg_xprewarm_mission : (uint32_t)g_xw_last_mission;
    ArenaRow* xr = mission ? FindRow(mission) : 0;
    if (!xr || CurWorld() < 0 || CurWorld() == xr->world) { Log("XWORLD prewarm (%s): nothing to do (mission 0x%X row %p world 0x%02X)", tag, mission, (void*)xr, CurWorld()); return 0; }
    if (g_cfg_xprewarm < 2) { XwPreload(xr, "(menu opened, resources only: CrossWorldPrewarm 1 never switches the world, bridge 6550)"); return 0; }
    int ok = XwStart(xr, 0); Log("XWORLD prewarm (%s) %s -> %d", tag, xr->id, ok); return ok;
}
// bridge 6473: 1 while a prewarm world request is running or landed and not claimed yet (GAMESTATE[2] is 1 then, Boss Rematch must not refuse 'loading')
static DWORD WINAPI RmPipeThread(LPVOID p) {
    char* cmd = (char*)p; HANDLE h = CreateFileA("\\\\.\\pipe\\nioh3bossrematch", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) { Log("CMD rm '%s': pipe not available (%lu)", cmd, GetLastError()); free(cmd); return 0; }
    char b[700], r[1024]; DWORD n = 0; snprintf(b, sizeof b, "%s\n", cmd); WriteFile(h, b, (DWORD)strlen(b), &n, 0);
    if (ReadFile(h, r, sizeof r - 1, &n, 0)) { r[n] = 0; while (n && (r[n - 1] == '\n' || r[n - 1] == '\r')) r[--n] = 0; Log("CMD rm '%s' -> %s", cmd, r); } else Log("CMD rm '%s': no reply", cmd);
    CloseHandle(h); free(cmd); return 0;
}
N3T_API int Nioh3Training_XwPending(void) { return g_xw.active && !g_xw.claimed; }
N3T_API void Nioh3Training_PreloadCancel(void) { XwCancel("menu closed"); }
N3T_API void Nioh3Training_PreloadFor(uint32_t mission) { XwPrewarm(mission, "PreloadFor"); }
N3T_API void Nioh3Training_Preload(void) { XwPrewarm(0, "menu opened"); if (g_cfg_enable && g_cfg_preload) PreloadNearby("PRELOAD (menu opened)"); }
N3T_API int Nioh3Training_Version(void) { return 4942; }
// user #6618: Boss Rematch menu toggles = the 'deathcfg <loot> <blow> <cut>' command as an in-process API (no command file round trip).
// Get fills any non-null pointer with the live value. Set: -1 keeps a value; persist 1 also writes [Summon] BossLoot/DeathBlowRequired/CutsceneOnKill
// to Nioh3Training.ini on a short worker thread (the caller is the Rematch UI worker; no file I/O on its thread).
static DWORD WINAPI DeathCfgSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH], v[8]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    snprintf(v, sizeof v, "%d", g_cfg_loot); WritePrivateProfileStringA("Summon", "BossLoot", v, ini);
    snprintf(v, sizeof v, "%d", g_cfg_deathblow); WritePrivateProfileStringA("Summon", "DeathBlowRequired", v, ini);
    snprintf(v, sizeof v, "%d", g_cfg_cutscene); WritePrivateProfileStringA("Summon", "CutsceneOnKill", v, ini);
    WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
static DWORD WINAPI AutoRetrySaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH], v[8]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    snprintf(v, sizeof v, "%d", g_cfg_autoretry); WritePrivateProfileStringA("Summon", "AutoRetryOnDeath", v, ini); WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
N3T_API int Nioh3Training_GetAutoRetry(void) { return g_cfg_autoretry; }
N3T_API void Nioh3Training_SetAutoRetry(int on, int persist) {
    g_cfg_autoretry = on != 0; Log("API AutoRetryOnDeath %d%s", g_cfg_autoretry, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, AutoRetrySaveThread, 0, 0, 0); if (t) CloseHandle(t); }
}
N3T_API void Nioh3Training_GetDeathCfg(int* loot, int* blow, int* cut) { if (loot) *loot = g_cfg_loot; if (blow) *blow = g_cfg_deathblow; if (cut) *cut = g_cfg_cutscene; }
N3T_API void Nioh3Training_SetDeathCfg(int loot, int blow, int cut, int persist) {
    if (loot >= 0) g_cfg_loot = loot != 0; if (blow >= 0) g_cfg_deathblow = blow != 0; if (cut >= 0) g_cfg_cutscene = cut != 0;
    Log("API deathcfg (Boss Rematch menu, #6618): BossLoot %d DeathBlowRequired %d CutsceneOnKill %d%s", g_cfg_loot, g_cfg_deathblow, g_cfg_cutscene, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, DeathCfgSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
}
// user #7118: legacy aliases of Get/SetAutoRetry (jumpfix 'Instant respawn on death'); same flag, persisted as [Summon] AutoRetryOnDeath.
N3T_API int Nioh3Training_GetInstantRespawn(void) { return Nioh3Training_GetAutoRetry(); }
N3T_API void Nioh3Training_SetInstantRespawn(int on, int persist) { Nioh3Training_SetAutoRetry(on, persist); }
// user #6779: 'Return to where I was' (Boss Rematch menu): ends the arena session (boss, boss mode, BGM, bar, NoDeath) and puts the player back at g_home, after a kill,
// with no boss or mid-swap. A running cross-world swap / prewarm is cancelled through the bridge 6541/6550 abort path (XwCancel -> XwCancelTick WarpOtherWorld back
// -> XrTick puts the player on the spot), aimed at g_home; another world without a swap running uses the same WarpOtherWorld trip; the same world is a plain warp.
// Fade to black around it like Fight now. Game thread (pump); the export only queues.
static void RhDone(const char* why) { Log("RETURN done (%s) +%lu ms", why, GetTickCount() - g_rh.t0); g_rh.active = 0; g_rh.phase = 0; FadeWant(0, "return to where I was"); }
static void RhTick(void) {
    DWORD now = GetTickCount();
    if (InterlockedExchange(&g_rh.want, 0) && !g_rh.active) {
        g_rh.active = 1; g_rh.phase = 1; g_rh.t0 = g_rh.tPh = now; g_rh.tStable = 0;
        Log("RETURN requested (Boss Rematch menu, #6779): home world 0x%02X %.0f %.0f %.0f mission 0x%X; now world 0x%02X, swap %d (claimed %d), switch pending %d, session %d (round %d, beaten boss %d)",
            g_home.world, g_home.pos[0], g_home.pos[1], g_home.pos[2], g_home.mission, CurWorld(), g_xw.active, g_xw.claimed, g_fd.pend, g_tr.active, g_tr.round, g_tr.tWin != 0);
        g_fd.pend = 0; g_fd.phase = 0;  // a same-world switch still waiting for black is dropped
        if (g_xw.active) {
            g_xw.oWorld = g_home.world; g_xw.oJourney = g_home.journey; g_xw.oMode = g_home.mode; g_xw.oStart = g_home.start; memcpy(g_xw.oPos, g_home.pos, sizeof g_xw.oPos);
            g_xw.claimed = 0; g_xw.cancel = 0; XwCancel("Return to where I was");
        }
        if (g_home.valid) FadeWant(1, "return to where I was");
    }
    if (!g_rh.active) return;
    if (now - g_rh.t0 > 90000) { Log("RETURN: not finished after 90 s (phase %d), giving up", g_rh.phase); RhDone("timeout"); return; }
    if (g_rh.phase == 1) {
        if (g_xw.active) return;  // XwCancelTick: waits for the request to land, then sends the player back (g_xr)
        TrainStop("return to where I was"); g_sp.active = 0; g_vf.active = 0; g_vf.respawnAt = 0; g_ar.active = 0; g_en.active = 0;
        if (g_xr.active) { g_rh.phase = 3; g_home.valid = 0; return; }
        g_rh.phase = 2; g_rh.tPh = now;
    }
    if (g_rh.phase == 2) {
        const uint8_t* st = GameState();
        if (!g_home.valid) { Log("RETURN: no stored spot (no Fight now since the game started / last return): session ended, player not moved"); RhDone("nothing stored"); return; }
        if (!st || st[2] || !PlayerChar() || StageState() != 5) return;
        if (FdAlpha() < 255 && now - g_rh.tPh < (DWORD)g_cfg_fade_out + 400) return;  // black first (or the ramp + 400 ms when the overlay cannot draw)
        { uint64_t cm = CharMgr(), c0 = 0; if (cm && Rd64(cm, &c0) && c0 && *(uint8_t*)(c0 + 0x26AA)) BossModeEnd("return to where I was: boss mode still on"); }
        uint32_t mis = 0, mode = 0; uint8_t jr = 0;
        if (g_home.mission && MsRead(&mis, &jr, &mode) && mis != g_home.mission) { Log("RETURN: the mission changed 0x%X -> 0x%X since Fight now: not moved (the game's own mission exit applies)", g_home.mission, mis); g_home.valid = 0; RhDone("mission changed"); return; }
        g_home.valid = 0;
        if (CurWorld() == g_home.world) {
            int w = Warp(g_home.pos[0], g_home.pos[1], g_home.pos[2], g_home.pos[3], 1);
            Log("RETURN: same world 0x%02X, player put back at %.0f %.0f %.0f -> %d", g_home.world, g_home.pos[0], g_home.pos[1], g_home.pos[2], w);
            g_rh.phase = 4; g_rh.tPh = now; return;
        }
        uint64_t mm = 0, ms = 0; if (!Rd64(B + 0x47514D0, &mm) || !mm || !Rd64(mm + 8, &ms) || !ms) { Log("RETURN: mission state unreadable, world 0x%02X not left", CurWorld()); RhDone("mission state unreadable"); return; }
        __attribute__((aligned(16))) uint8_t prm[32] = {0};
        ((void (*)(void*))(B + 0x1BF7D68))(prm); prm[7] = 0; prm[0xA] = 0; prm[0xE] = 1;  // as XwCancelTick
        ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3B28FC))(2, 0, 0);
        memset(&g_xr, 0, sizeof g_xr); g_xr.world = g_home.world; g_xr.start = XwReturnStart(g_home.world, g_home.pos, g_home.start); g_xr.t0 = now; memcpy(g_xr.pos, g_home.pos, sizeof g_xr.pos); g_xr.active = 1;
        Log("RETURN: world 0x%02X -> home world 0x%02X start %u (WarpOtherWorld), put back on the spot once its cells are loaded", CurWorld(), g_home.world, g_xr.start);
        ((void (*)(uint8_t, uint8_t, uint32_t, uint32_t, void*))(B + 0x22399B8))(g_xr.world, g_home.journey, g_xr.start, g_home.mode, prm);
        g_rh.phase = 3; return;
    }
    if (g_rh.phase == 3) { if (g_xr.active) return; g_rh.phase = 4; g_rh.tPh = now; }
    if (g_rh.phase == 4) {  // hold black until the spot has settled (as the Fight now fade), at most 15 s
        if (now - g_rh.tPh > 15000) { RhDone("settle 15 s cap"); return; }
        if (!FadeSettled()) { g_rh.tStable = 0; return; }
        if (!g_rh.tStable) g_rh.tStable = now;
        if (now - g_rh.tStable >= (DWORD)g_cfg_fade_stable) RhDone("back where the player was");
    }
}
// 1 = queued, 2 = no stored spot: only the running session / swap is ended, 0 = nothing to do (no spot, no session, no swap), -1 = a return is already running
N3T_API int Nioh3Training_ReturnHome(void) {
    if (g_rh.active || g_rh.want) return -1;
    if (!g_home.valid && !g_tr.active && !g_xw.active && !g_fd.pend) return 0;
    int had = g_home.valid; InterlockedExchange(&g_rh.want, 1); return had ? 1 : 2;
}
// Boss Rematch menu state: 1 a spot is stored, 2 the session's boss is defeated, 4 session running with no boss in the world, 8 a swap is running, 16 a return is running
N3T_API int Nioh3Training_ReturnInfo(void) {
    int f = g_home.valid ? 1 : 0;
    if (g_tr.active && g_tr.tWin) f |= 2;
    if (g_tr.active && !g_sp.active && !g_tr.tRespawn && (g_nsp <= 0 || !g_sp_actor[g_nsp - 1] || g_tr.bodyGone)) f |= 4;
    if (g_xw.active || g_fd.pend || (g_fd.phase == 2 && !g_rh.active)) f |= 8;
    if (g_rh.active || g_rh.want) f |= 16;
    return f;
}
// user #6789/#6798: 'Return to where I was' as a PHYSICAL object in the arena = the game's own mission clear gadget (the one the mission script spawns at the boss
// after the kill: Function::CreateClearGadgetBossPosition(10|11|13, x, y, z) -> Gadget::CreatePlacementGadgetPosition -> 1C1F07C -> GadgetCreateFromRecord 7D39E8;
// clear gadget = placement record of type 0x353 (851), record 0x6C B: +8 type). Record lookup 7D4480(id) (set, default set for unknown ids) + 7D41E0(set, id).
// Spawned (once per arena session, gadgets have no delete native) when a session is running, a spot is stored and no boss is alive and no respawn is pending (#7138: not right after the arena load or a retry any more).
// Interacting: the native clear gadget ends the mission through MissionRequestReturn 22396D8; while our gate is up that call is diverted into ReturnHome.
// Fallback 'touch': standing ReturnGateTouchMs on the gate (after having left it once) also returns (ReturnGateTouchMs 0 = off).
static int g_cfg_gate = 1, g_cfg_gate_fake = 1, g_cfg_gate_dist = 220, g_cfg_gate_touch = 1500, g_cfg_gate_radius = 110; static char g_gate_ids[64] = "10,11,13,12";
static struct { int up, tried, left; uint32_t id, mission; float pos[4]; DWORD tTry, tIn; int session; } g_gate;
static uint8_t g_gate_fakerec[0x100];
typedef void (*ReqRetFn)(const uint32_t*, uint8_t);
static ReqRetFn o_reqret;
static int GateRecType(uint64_t rec) { __try { return rec ? *(int32_t*)(rec + 8) : -1; } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; } }
static uint64_t GateRecord(uint32_t id) {
    uint64_t set = ((uint64_t (*)(uint32_t))(B + 0x7D4480))(id); if (!set) return 0;
    return ((uint64_t (*)(uint64_t, uint32_t))(B + 0x7D41E0))(set, id);
}
// scan the default gadget placement set for any 0x353 record: set G: [[G]+0x488]+0x418 = header (count +4, records +8, 0x6C B each), id map G[3] {+4 empty key, +8..+10 (id, idx) pairs}
static uint64_t GateScan(uint32_t* outId) {
    __try {
        uint64_t g = ((uint64_t (*)(uint32_t))(B + 0x7D4480))(0x7FFFFFF1u); if (!g) return 0;
        uint64_t d0 = *(uint64_t*)g; if (!d0) return 0; uint64_t res = *(uint64_t*)(d0 + 0x488); if (!res || !*(uint32_t*)(res + 0x460)) return 0;
        uint64_t hdr = *(uint64_t*)(res + 0x418); if (!hdr) return 0; uint32_t cnt = *(uint32_t*)(hdr + 4); if (cnt > 100000) return 0;
        uint64_t map = *(uint64_t*)(g + 0x18); if (!map) return 0; uint64_t mb = *(uint64_t*)(map + 8), me = *(uint64_t*)(map + 0x10); int32_t empty = *(int32_t*)(map + 4);
        for (uint32_t i = 0; i < cnt; ++i) {
            uint64_t rec = hdr + 8 + (uint64_t)i * 0x6C; if (*(int32_t*)(rec + 8) != 0x353) continue;
            for (uint64_t e = mb; e < me; e += 8) if (*(int32_t*)e != empty && *(uint32_t*)(e + 4) == i) { *outId = *(uint32_t*)e; Log("GATE: default gadget set has clear gadget record #%u id %u", i, *outId); return rec; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("GATE: scan faulted"); }
    return 0;
}
static int GateSpawn(const char* why) {
    float pp[4] = {0}; if (!PlayerPos(pp)) return 0;
    float base[3] = {pp[0], pp[1], pp[2]}, dx = 1.0f, dz = 0.0f;
    if (g_tr.hasStart) { base[0] = g_tr.pp[0]; base[1] = g_tr.pp[1]; base[2] = g_tr.pp[2]; float vx = g_tr.pp[0] - g_tr.bp[0], vz = g_tr.pp[2] - g_tr.bp[2], l = sqrtf(vx * vx + vz * vz); if (l > 1.0f) { dx = vx / l; dz = vz / l; } }
    __attribute__((aligned(16))) float pos[4] = {base[0] + dx * g_cfg_gate_dist, base[1], base[2] + dz * g_cfg_gate_dist, 1.0f};
    __attribute__((aligned(16))) float mat[16]; memcpy(mat, ((const float* (*)(void))(B + 0x20F030))(), sizeof mat);
    uint32_t mis = 0, mode = 0; uint8_t jr = 0; MsRead(&mis, &jr, &mode);
    uint64_t rec = 0; uint32_t id = 0; char ids[64]; snprintf(ids, sizeof ids, "%s", g_gate_ids);
    for (char* s = strtok(ids, ", "); s && !rec; s = strtok(0, ", ")) { uint32_t c = (uint32_t)strtoul(s, 0, 0); uint64_t r = 0; __try { r = GateRecord(c); } __except (EXCEPTION_EXECUTE_HANDLER) { r = 0; } int t = GateRecType(r); Log("GATE: candidate id %u -> record %p type 0x%X", c, (void*)r, t); if (t == 0x353) { rec = r; id = c; } }
    if (!rec) rec = GateScan(&id);
    if (!rec && g_cfg_gate_fake) {  // no clear gadget record loaded here: synthesize one (type 0x353, all else zero; risky, ReturnGateFakeRecord = 0 turns it off)
        memset(g_gate_fakerec, 0, sizeof g_gate_fakerec); *(int32_t*)(g_gate_fakerec + 8) = 0x353; id = 0x7FFF0353u; rec = (uint64_t)g_gate_fakerec;
        Log("GATE: no clear gadget record in the loaded placement sets, using a synthesized 0x353 record (id 0x%X)", id);
    }
    if (!rec) { Log("GATE (%s): no clear gadget record, nothing spawned", why); return 0; }
    __try { ((void (*)(uint64_t, uint32_t, uint32_t, float*, float*, uint8_t))(B + 0x7D39E8))(rec, id, mis, pos, mat, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) { Log("GATE (%s): GadgetCreateFromRecord 7D39E8 faulted %08lX", why, GetExceptionCode()); return 0; }
    g_gate.up = 1; g_gate.id = id; g_gate.mission = mis; memcpy(g_gate.pos, pos, sizeof g_gate.pos); g_gate.left = 0; g_gate.tIn = 0;
    Log("GATE (%s): clear gadget id %u spawned at %.0f %.0f %.0f (mission 0x%X, record %p): interact (or stand on it %d ms) = Return to where I was", why, id, pos[0], pos[1], pos[2], mis, (void*)rec, g_cfg_gate_touch);
    return 1;
}
static int GateBossAlive(void) { int f = Nioh3Training_ReturnInfo(); return !(f & 2) && !(f & 4); }
// gate used: Boss Rematch opens its 2-option page (Return to where I was / Fight again); without it (old Rematch, no menu) return directly. 2 = choice shown.
static volatile LONG g_gate_again;
static int GateUse(void) {
    HMODULE rm = GetModuleHandleA("Nioh3BossRematch.asi");
    int (*choice)(void) = rm ? (int (*)(void))GetProcAddress(rm, "Nioh3BossRematch_GateChoice") : 0;
    if (choice && choice()) return 2;
    Log("GATE: Boss Rematch gate choice not available, returning directly"); return Nioh3Training_ReturnHome();
}
// 'Fight again' from the gate page: queued, the pump runs TrainRetry (same boss, round reset, player back to the round-1 spot). 1 = queued, 0 = no session.
N3T_API int Nioh3Training_GateFightAgain(void) { if (!g_tr.active || g_rh.active) return 0; InterlockedExchange(&g_gate_again, 1); return 1; }
static void GateTick(void) {
    if (InterlockedExchange(&g_gate_again, 0) && g_tr.active && !g_rh.active) { Log("GATE: Fight again (Boss Rematch gate page)"); TrainRetry("gate: fight again"); }
    if (!g_cfg_gate) return;
    if (!g_tr.active) { if (g_gate.session) { g_gate.session = 0; g_gate.up = 0; g_gate.tried = 0; } return; }
    if (!g_gate.session) { g_gate.session = 1; g_gate.up = 0; g_gate.tried = 0; g_gate.tTry = 0; }
    if (g_rh.active || g_xw.active || g_fd.pend) return;
    const uint8_t* st = GameState(); if (!st || st[2] || !PlayerChar() || StageState() != 5) return;
    DWORD now = GetTickCount();
    // #7118/#7138: only while the session is idle (no boss, no respawn pending). The removed 'or now - tCmd < 1500' clause also matched the 2 s window after every
    // TrainRetry (tCmd reset): the clear gadget was created 2 s before the retry boss (log 22:34:19.5 gadget, 22:34:21.5 boss) and that boss stayed passive
    // (ai+1AA8 = -1, walks up and hops back to its spawn = the 'jumping' boss; the confirmed-good 20:39 retry without a gadget went 0 at once).
    // Also never before the round-1 arena boss exists (Fight now arena swap still loading its resources: no boss yet, but one is about to spawn).
    int idle = !GateBossAlive() && !g_tr.tRespawn && !g_sp.active && !g_vf.respawnAt && !g_tr.beaten && !(g_tr.tWin && g_cfg_tr_win > 0) && !(g_ar.active && !g_ar.spawned);
    if (!g_gate.up && g_home.valid && g_gate.tried < 3 && (!g_gate.tTry || now - g_gate.tTry > 3000) && idle) {
        g_gate.tTry = now; g_gate.tried++; GateSpawn(g_gate.tried == 1 ? "arena session" : "retry");
    }
    if (g_gate.up && g_cfg_gate_touch > 0) {
        float p[4]; if (!PlayerPos(p)) return;
        float dx = p[0] - g_gate.pos[0], dz = p[2] - g_gate.pos[2], dy = p[1] - g_gate.pos[1]; int in = dx * dx + dz * dz < (float)g_cfg_gate_radius * g_cfg_gate_radius && fabsf(dy) < 250.0f;
        if (!in) { g_gate.left = 1; g_gate.tIn = 0; return; }
        if (!g_gate.left || GateBossAlive()) return;
        if (!g_gate.tIn) g_gate.tIn = now;
        if (now - g_gate.tIn >= (DWORD)g_cfg_gate_touch) { g_gate.tIn = 0; g_gate.left = 0; int r = GateUse(); Log("GATE: player stood on the return gate %d ms -> %d", g_cfg_gate_touch, r); }
    }
}
static void h_reqret(const uint32_t* p, uint8_t f) {
    void* ra = __builtin_return_address(0);
    if (g_cfg_gate && g_gate.up && g_tr.active && !g_rh.active) {
        if (GateBossAlive()) { Log("GATE: MissionRequestReturn from %p while the boss is alive: ignored (gate is inactive during a fight)", (void*)((uintptr_t)ra - B)); return; }
        int r = GateUse(); Log("GATE: MissionRequestReturn (caller +%llX) diverted to the gate choice -> %d", (unsigned long long)((uintptr_t)ra - B), r); return;
    }
    o_reqret(p, f);
}
// user #6781: temporary gold boss Ki bar. Boss gauge Ki row: BossGaugeRowsUpdate 1D21EF8 calls KiBarStyle 5D4434(charParam) at 1D220C1 -> KiRowWidget 235F9A4 ->
// KiWidgetAnimate 235F40C plays bar style anim 0x20 (style 1) or 0x21 (style 0). The colour itself lives in the layout's VertexColor FCurves (no direct colour field
// found statically). This call-site hook only stamps 'boss Ki row drawn now' (native style kept); the tint itself is GoldQuad below (UiQuadBuild vertex colours).
static int g_cfg_goldki = 1; static volatile DWORD g_ki_seen;
static uint8_t KiStyleHook(uint64_t charParam) {
    uint8_t s = ((uint8_t (*)(uint64_t))(B + 0x5D4434))(charParam);
    static int logged; if (!logged) { logged = 1; int v = -1; __try { v = charParam ? *(int32_t*)(charParam + 0x80) : -1; } __except (EXCEPTION_EXECUTE_HANDLER) {} Log("GOLDKI: first boss Ki row: charParam+0x80 = %d, native style %d, GoldKiBar %d", v, s, g_cfg_goldki); }
    g_ki_seen = GetTickCount();  // the boss Ki row is being drawn: enables the vertex tint (GoldQuad)
    return s;
}
static void HookGoldKi(void) {
    uint8_t* site = (uint8_t*)(B + 0x1D220C1);
    if (site[0] != 0xE8 || (uintptr_t)(site + 5) + *(int32_t*)(site + 1) != B + 0x5D4434) { Log("GOLDKI: call site 1D220C1 is not 'call 5D4434', not hooked"); return; }
    uint8_t* stub = 0;
    for (uintptr_t a = ((uintptr_t)site - 0x70000000) & ~0xFFFFull; a < (uintptr_t)site + 0x70000000 && !stub; a += 0x10000) stub = (uint8_t*)VirtualAlloc((void*)a, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { Log("GOLDKI: no near stub"); return; }
    stub[0] = 0x48; stub[1] = 0xB8; *(uint64_t*)(stub + 2) = (uint64_t)KiStyleHook; stub[10] = 0xFF; stub[11] = 0xE0;
    int32_t rel = (int32_t)((intptr_t)stub - (intptr_t)(site + 5)); DWORD old; VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    InterlockedExchange((volatile LONG*)(site + 1), rel); VirtualProtect(site, 5, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 5);
    Log("GOLDKI: boss Ki bar style hook at 1D220C1 on (GoldKiBar %d)", g_cfg_goldki);
}
// ---- user #6781 gold Ki bar, real tint (round 3). The colour lives in the HUD layout's VertexColor FCurves (no static field), so it is overridden in the
// UI vertex buffer: hook UiQuadBuild 1CD690 at 1CD7CD (after every element wrote its vertices, before the alpha pass / unmap). GoldWalk replays the alpha
// pass's own walk (groups of [obj+100h] vtbl+18h/+8, nodes from [obj+28h]; node +10h element, +24h vertex count; element type vtbl+8 and widget 1CEA08
// vtbl+90h decide quad (0xB0 B = 4 x 0x2C vertices, RGBA at +24h) or skip count*0x2C) so it runs whether or not the alpha pass is on.
// Ki quads are found from the start of the fight by a probe: when a boss gauge appears the pump sets the boss's Ki to 40 % for up to 1.5 s (then refills it);
// the elements whose quad width drops below 75 % while that happens are the Ki bar -> repainted GoldKiRGB every frame. GoldKiColors (exact RGBA) also tints;
// GoldKiDump = 1 logs the walked quads once.
static volatile float g_ki_ratio = -1.0f, g_hp_ratio = -1.0f;
static int g_cfg_goldki_dump = 0, g_goldki_dumped, g_cfg_goldki_learn = 1; static uint32_t g_goldki_cols[8]; static int g_goldki_ncols;
static uint8_t g_gold_r = 255, g_gold_g = 200, g_gold_b = 50;
static struct { uint64_t e; float baseW, minW; int seen; } g_gk[256]; static int g_ngk;
static uint64_t g_gold_el[8]; static volatile LONG g_ngold;
static struct { volatile LONG phase; uint64_t actor; DWORD t0; int walks; } g_kp;  // phase 0 idle, 1 baseline, 2 Ki lowered, 3 done
static void GoldKiRatios(void) {  // pump: ratios + the probe state machine for the summoned boss (the gauge actor)
    uint64_t a = (g_cfg_goldki && g_nsp > 0) ? (uint64_t)g_sp_actor[g_nsp - 1] : 0;
    if (!a) { g_ki_ratio = -1.0f; if (g_kp.phase == 2 && g_kp.actor) g_kp.phase = 0; return; }
    uint64_t st = 0; __try { st = *(uint64_t*)(a + 0x3A0); } __except (EXCEPTION_EXECUTE_HANDLER) { st = 0; }
    if (!st) return;
    float cur = *(float*)(st + 0xAC), mx = *(float*)(st + 0xB0); uint64_t hm = *(uint64_t*)(st + 0x50), hc = *(uint64_t*)(st + 0x58);
    g_ki_ratio = mx > 0 ? (cur < 0 ? 0 : cur / mx) : -1.0f; g_hp_ratio = hm ? (float)((double)hc / (double)hm) : -1.0f;
    DWORD now = GetTickCount();
    if (!g_cfg_goldki_learn || g_ngold) { g_kp.phase = 3; return; }
    if (g_kp.actor != a && g_kp.phase != 2) { g_kp.actor = a; g_kp.phase = 0; }
    if (g_kp.phase == 0 && now - g_ki_seen < 100 && mx > 0) { g_kp.phase = 4; g_kp.t0 = now; }  // wait out the gauge's intro fill animation
    else if (g_kp.phase == 4 && now - g_kp.t0 > 2500) { g_ngk = 0; g_kp.walks = 0; g_kp.t0 = now; g_kp.phase = 1; Log("GOLDKI probe: boss gauge up, measuring the bar quads"); }
    else if (g_kp.phase == 1 && now - g_kp.t0 > 400 && g_kp.walks >= 5) { g_kp.phase = 2; g_kp.t0 = now; Log("GOLDKI probe: %d quads measured, boss Ki -> 40 %% for up to 1.5 s", g_ngk); }
    else if (g_kp.phase == 1 && now - g_kp.t0 > 5000) { g_kp.phase = 3; Log("GOLDKI probe: no quad walk seen in 5 s (UI hook not running?): probe off"); }
    if (g_kp.phase == 2) {
        if (mx > 0) *(float*)(st + 0xAC) = mx * 0.4f;
        if (g_ngold || now - g_kp.t0 > 1500) {
            ((void (*)(uint64_t))(B + 0x5FB5C8))(st + 0x98);  // KiRefill
            g_kp.phase = 3; Log("GOLDKI probe: done, %ld Ki bar element(s) found, boss Ki refilled", g_ngold);
        }
    }
}
static void GoldPaint(uint8_t* q) {
    for (int v = 0; v < 4; ++v) { uint8_t* c = q + v * 0x2C + 0x24; int l = c[0] > c[1] ? c[0] : c[1]; if (c[2] > l) l = c[2]; if (l < 110) l = 110; c[0] = (uint8_t)(g_gold_r * l / 255); c[1] = (uint8_t)(g_gold_g * l / 255); c[2] = (uint8_t)(g_gold_b * l / 255); }
}
static void GoldQuad(uint8_t* q, uint64_t el) {
    for (LONG i = 0; i < g_ngold; ++i) if (g_gold_el[i] == el) { GoldPaint(q); return; }
    int ph = g_kp.phase, dump = g_cfg_goldki_dump && g_goldki_dumped < 120;
    if (ph != 1 && ph != 2 && !dump && !g_goldki_ncols) return;
    uint32_t rgba = *(uint32_t*)(q + 0x24);
    for (int i = 0; i < g_goldki_ncols; ++i) if ((g_goldki_cols[i] & 0x00FFFFFF) == (rgba & 0x00FFFFFF)) { GoldPaint(q); return; }
    if (ph != 1 && ph != 2 && !dump) return;
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    for (int v = 0; v < 4; ++v) { float x = *(float*)(q + v * 0x2C), y = *(float*)(q + v * 0x2C + 4); if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    if (dump) { g_goldki_dumped++; Log("GOLDKI dump: elem %p rgba %08X box %.1f %.1f - %.1f %.1f (Ki %.2f HP %.2f probe %d)", (void*)el, rgba, x0, y0, x1, y1, g_ki_ratio, g_hp_ratio, ph); }
    float w = x1 - x0; if (!(w > 0.5f) || w > 1e5f) return;
    int k = -1; for (int i = 0; i < g_ngk; ++i) if (g_gk[i].e == el) { k = i; break; }
    if (ph == 1) { if (k < 0 && g_ngk < 256) { k = g_ngk++; g_gk[k].e = el; g_gk[k].baseW = w; g_gk[k].minW = w; g_gk[k].seen = 0; } if (k >= 0) { if (w > g_gk[k].baseW) g_gk[k].baseW = w; g_gk[k].minW = w < g_gk[k].minW ? w : g_gk[k].minW; g_gk[k].seen++; } return; }
    if (ph == 2 && k >= 0 && g_gk[k].baseW >= 8.0f && g_gk[k].minW > g_gk[k].baseW * 0.95f && w < g_gk[k].baseW * 0.75f && g_ngold < 8) {
        for (LONG i = 0; i < g_ngold; ++i) if (g_gold_el[i] == el) return;
        g_gold_el[g_ngold] = el; InterlockedIncrement(&g_ngold);
        Log("GOLDKI probe: element %p shrank %.1f -> %.1f with the boss Ki at 40 %% (rgba %08X, box y %.1f-%.1f): Ki bar, tinted gold", (void*)el, g_gk[k].baseW, w, rgba, y0, y1);
    }
}
static void __cdecl GoldWalk(uint8_t* obj) {
    if (!g_cfg_goldki || GetTickCount() - g_ki_seen > 250) return;
    if (!g_ngold && !g_goldki_ncols && g_kp.phase != 1 && g_kp.phase != 2 && !(g_cfg_goldki_dump && g_goldki_dumped < 120)) return;
    __try {
        uint8_t* q = *(uint8_t**)(obj + 0x358); if (!q) return;
        uint64_t node = *(uint64_t*)(obj + 0x28), sent = (uint64_t)obj + 0x60, grpObj = (uint64_t)obj + 0x100;
        uint64_t* gv = *(uint64_t**)grpObj;
        uint32_t ng = ((uint32_t (*)(uint64_t))gv[3])(grpObj);
        int guard = 0;
        for (uint32_t g = 0; g < ng && g < 4096; ++g) {
            uint64_t grp = ((uint64_t (*)(uint64_t, uint32_t))gv[1])(grpObj, g); if (!grp) return;
            uint32_t cnt = *(uint32_t*)(grp + 4);
            while (cnt) {
                if (!node || node == sent || ++guard > 200000) return;
                uint64_t el = *(uint64_t*)(node + 0x10); if (!el) return;
                int type = ((int (*)(uint64_t))(*(uint64_t**)el)[1])(el);
                uint64_t wdg = ((uint64_t (*)(uint64_t))(B + 0x1CEA08))(el);
                if ((type & ~4) || (wdg && ((int (*)(uint64_t))(*(uint64_t**)wdg)[0x90 / 8])(wdg) == 1)) { q += (uint64_t)*(uint32_t*)(node + 0x24) * 0x2C; node = *(uint64_t*)(node + 8); continue; }
                GoldQuad(q, el); q += 0xB0; node = *(uint64_t*)(node + 8); --cnt;
            }
        }
        if (g_kp.phase == 1) g_kp.walks++;
    } __except (EXCEPTION_EXECUTE_HANDLER) { static int once; if (!once++) Log("GOLDKI: quad walk faulted %08lX", GetExceptionCode()); }
}
static void HookGoldQuad(void) {
    uint8_t* site = (uint8_t*)(B + 0x1CD7CD);  // mov rax, [rip+X] (g_UiSettings 4BD0AC0); cmp byte [rax+368h], r12b
    if (site[0] != 0x48 || site[1] != 0x8B || site[2] != 0x05 || (uintptr_t)(site + 7) + *(int32_t*)(site + 3) != B + 0x4BD0AC0) { Log("GOLDKI: UiQuadBuild 1CD7CD bytes differ, vertex tint off"); return; }
    uint8_t* s = 0;
    for (uintptr_t a = ((uintptr_t)site - 0x70000000) & ~0xFFFFull; a < (uintptr_t)site + 0x70000000 && !s; a += 0x10000) s = (uint8_t*)VirtualAlloc((void*)a, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s) { Log("GOLDKI: no near stub for 1CD7CD"); return; }
    uint8_t* p = s;
    static const uint8_t pre[] = {0x50, 0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53,  // push rax rcx rdx r8 r9 r10 r11 (rsp 16-aligned at the site)
                                  0x48, 0x83, 0xEC, 0x28,                                          // sub rsp, 28h
                                  0x48, 0x89, 0xF9};                                               // mov rcx, rdi (UiQuadBuild object)
    memcpy(p, pre, sizeof pre); p += sizeof pre;
    *p++ = 0x48; *p++ = 0xB8; *(uint64_t*)p = (uint64_t)GoldWalk; p += 8; *p++ = 0xFF; *p++ = 0xD0;  // mov rax, GoldWalk; call rax
    static const uint8_t post[] = {0x48, 0x83, 0xC4, 0x28, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59, 0x58};
    memcpy(p, post, sizeof post); p += sizeof post;
    *p++ = 0x48; *p++ = 0xB8; *(uint64_t*)p = (uint64_t)(B + 0x4BD0AC0); p += 8;  // mov rax, &g_UiSettings
    *p++ = 0x48; *p++ = 0x8B; *p++ = 0x00;                                          // mov rax, [rax]
    *p++ = 0xFF; *p++ = 0x25; *(uint32_t*)p = 0; p += 4; *(uint64_t*)p = (uint64_t)(site + 7); p += 8;  // jmp 1CD7D4
    DWORD old; VirtualProtect(site, 16, PAGE_EXECUTE_READWRITE, &old);
    LONG64 o8 = *(volatile LONG64*)site, n8 = o8; uint8_t* nb = (uint8_t*)&n8; nb[0] = 0xE9; *(int32_t*)(nb + 1) = (int32_t)((intptr_t)s - (intptr_t)(site + 5));
    InterlockedCompareExchange64((volatile LONG64*)site, n8, o8);
    VirtualProtect(site, 16, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 16);
    uint64_t us = 0; uint8_t f368 = 0xFF; if (Rd64(B + 0x4BD0AC0, &us) && us) f368 = *(uint8_t*)(us + 0x368);
    Log("GOLDKI: vertex tint hook at UiQuadBuild 1CD7CD on (gold %u %u %u, probe %d, colours %d, dump %d; alpha pass flag %u)", g_gold_r, g_gold_g, g_gold_b, g_cfg_goldki_learn, g_goldki_ncols, g_cfg_goldki_dump, f368);
}
static volatile LONG g_ticks;
static DWORD g_phase_ms = 500, g_lastTickT;
static struct { DWORD t0, last, lastTick, lastBeat, window; LONG ticks0, lastTicks; int hadPlayer, active; uint32_t id, lastReg; float lastPos[4]; } g_ms;
static uint32_t RegCount(void) {
    uint64_t cm = 0, c0 = 0; if (!Rd64(B + 0x45B9E40, &cm) || !cm || !Rd64(cm, &c0) || !c0) return 0;
    uint64_t* reg = *(uint64_t**)(c0 + 0x48); return reg ? (uint32_t)reg[1] : 0;
}
static void PhaseTick(void) {
    DWORD now = GetTickCount();
    if (g_lastTickT && now - g_lastTickT > 1000 && g_ms.active) {
        Log("PHASE pump gap %lu ms: game thread stalled from +%lu ms to +%lu ms", now - g_lastTickT, g_lastTickT - g_ms.t0, now - g_ms.t0);
        g_ms.lastBeat = 0;
    }
    g_lastTickT = now;
    if (!g_ms.active) return;
    if (now - g_ms.last < g_phase_ms) return;
    DWORD dt = now - g_ms.last; g_ms.last = now;
    long dticks = (long)(g_ticks - g_ms.lastTicks); g_ms.lastTicks = g_ticks;
    float pos[4] = {0}; uint64_t ch = PlayerChar(); int ok = ch && PlayerPos(pos); uint32_t rc = RegCount();
    int has = ch != 0, changed = 0;
    if (has != g_ms.hadPlayer) { Log("PHASE player pointer %s at +%lu ms", has ? "APPEARED" : "GONE", now - g_ms.t0); g_ms.hadPlayer = has; changed = 1; }
    if (rc != g_ms.lastReg) { Log("PHASE registry count %u -> %u at +%lu ms", g_ms.lastReg, rc, now - g_ms.t0); g_ms.lastReg = rc; changed = 1; }
    if (ok && (fabsf(pos[0] - g_ms.lastPos[0]) > 50 || fabsf(pos[2] - g_ms.lastPos[2]) > 50)) { Log("PHASE player moved %.0f %.0f %.0f -> %.0f %.0f %.0f at +%lu ms (world switched)", g_ms.lastPos[0], g_ms.lastPos[1], g_ms.lastPos[2], pos[0], pos[1], pos[2], now - g_ms.t0); changed = 1; }
    if (ok) memcpy(g_ms.lastPos, pos, 12);
    if (changed || now - g_ms.lastBeat >= 2000) {
        g_ms.lastBeat = now;
        Log("PHASE mission 0x%X +%lu ms: ticks=%ld (+%ld in %lu ms) player=%p pos=%.0f %.0f %.0f reg=%u", g_ms.id, now - g_ms.t0, (long)(g_ticks - g_ms.ticks0), dticks, dt, (void*)ch, ok ? pos[0] : 0, ok ? pos[1] : 0, ok ? pos[2] : 0, rc);
    }
    if (now - g_ms.t0 > g_ms.window) { Log("PHASE window of %lu ms over", g_ms.window); g_ms.active = 0; }
}
static void Exec(char* line) {
    char op[32] = {0}; long long a[6] = {0};
    int n = sscanf(line, "%31s %lli %lli %lli %lli %lli %lli", op, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);
    if (n < 1) return;
    float pos[4] = {0};
    __try {
        if (!strcmp(op, "gate")) {  // spawn the return gate now (test)
            Log("CMD gate -> %d", GateSpawn("command"));
        } else if (!strcmp(op, "pos")) {
            uint64_t ch = PlayerChar(); int ok = PlayerPos(pos);
            Log("CMD pos ch=%p ok=%d %.1f %.1f %.1f %.1f", (void*)ch, ok, pos[0], pos[1], pos[2], pos[3]);
        } else if (!strcmp(op, "reg")) {
            uint64_t cm = 0, c0 = 0; Rd64(B + 0x45B9E40, &cm); Rd64(cm, &c0);
            uint64_t* reg = *(uint64_t**)(c0 + 0x48); uint64_t arr = reg[0], cnt = reg[1];
            uint64_t me = PlayerChar();
            Log("CMD reg charMgr=%p reg=%p count=%llu player=%p", (void*)cm, (void*)reg, cnt, (void*)me);
            for (uint64_t i = 0; i < cnt && i < 400; ++i) {
                uint32_t id = *(uint32_t*)(arr + i * 16); uint64_t w = *(uint64_t*)(arr + i * 16 + 8);
                uint64_t live = 0, rec = 0; uint32_t pl = 0; float xyz[4] = {0};
                if (w) { Rd64(w, &live); Rd64(w + 0x18, &rec); pl = *(uint32_t*)(w + 0x20); }
                if (live) memcpy(xyz, (void*)(live + 0x110), 16);
                char rb[200] = {0}; int k = 0;
                if (rec) for (int j = 0; j < 12; ++j) k += snprintf(rb + k, sizeof rb - k, "%u ", ((uint32_t*)rec)[j]);
                Log("  id=%u w=%p live=%p%s pl=%u pos=%.0f %.0f %.0f rec=%p [%s]", id, (void*)w, (void*)live, live == me ? " PLAYER" : "", pl, xyz[0], xyz[1], xyz[2], (void*)rec, rb);
            }
        } else if (!strcmp(op, "dump")) {
            g_dump_full = 1; DumpOwnerState("cmd dump"); g_dump_full = 0;  // native-boss diff (bridge 4717): wrapper words of live registry rows and +0..0xFF of every non-player actor
        } else if (!strcmp(op, "loaded")) {
            uint64_t key = ((uint64_t (*)(uint64_t, uint32_t))(B + 0x134314))(0, (uint32_t)a[0]);
            uint64_t rm = 0; Rd64(B + 0x45B9E60, &rm);
            uint64_t e = ((uint64_t (*)(uint64_t, uint64_t))(B + 0x133440))(rm, key);
            Log("CMD loaded %llu key=%llX entry=%p flag2c8=%u", a[0], key, (void*)e, e ? *(uint8_t*)(e + 0x2c8) : 255);
        } else if (!strcmp(op, "lreq")) {
            ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))((uint32_t)a[0], (uint32_t)a[1], (uint32_t)a[2]);
            Log("CMD lreq 3F9E58(%llu,%llu,%llu) done", a[0], a[1], a[2]);
        } else if (!strcmp(op, "dyn")) {
            if (!PlayerPos(pos)) { Log("CMD dyn: no player"); return; }
            float fwd = n >= 6 ? (float)a[4] : 300.0f;
            __attribute__((aligned(16))) float p[4] = {pos[0] + fwd, pos[1], pos[2], 1.0f};
            uint32_t r = ((DynFn)(B + 0x1C0C5E0))((uint32_t)a[0], p, 0.0f, (uint32_t)a[1], (uint32_t)a[2], (uint32_t)a[3]);
            Log("CMD dyn a0=%llu a4=%llu a5=%llu a6=%llu at %.1f %.1f %.1f -> %u (0x%X)", a[0], a[1], a[2], a[3], p[0], p[1], p[2], r, r);
        } else if (!strcmp(op, "spawn")) {
            if (g_sp.active) { Log("CMD spawn: previous spawn %u still loading", g_sp.id); return; }
            g_sp.boss = 0; g_sp.tOwn = 0; g_sp.id = (uint32_t)a[0]; g_sp.dist = n >= 3 ? (float)a[1] : 400.0f; g_sp.owner = n >= 4 ? (uint32_t)a[2] : 0; g_sp.t0 = GetTickCount();
            uint64_t e = ResEntry(g_sp.id);
            if (!e || !*(uint8_t*)(e + 0x2c8)) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))(g_sp.id, 2, 0); PoolAdd(g_sp.id); }
            Log("CMD spawn %u dist %.0f: %s", g_sp.id, g_sp.dist, e && *(uint8_t*)(e + 0x2c8) ? "resources already loaded" : "Load::Character requested");
            g_sp.active = 1; SpawnTick();
        } else if (!strcmp(op, "summon")) {  // summon <R-id|charId> [dist]: boss next to the player in the current world, no arena swap / load; HUD gauge, boss mode, boss BGM
            char tok[32] = {0}; float d = 400.0f; sscanf(line, "%*s %31s %f", tok, &d); SummonStart(tok, d);
        } else if (!strcmp(op, "train")) {  // bridge 4942: train <R-id|charId> [dist]: no-load session (player NoDeath, auto reset when beaten, 'retry', 'trainstop')
            char tok[32] = {0}; float d = (float)g_cfg_tr_dist; sscanf(line, "%*s %31s %f", tok, &d); TrainStart(tok, d);
        } else if (!strcmp(op, "aware")) { uint64_t act = n >= 2 ? ActorByHandle((uint32_t)a[0]) : (g_nsp ? g_sp_actor[g_nsp - 1] : 0); SetAware(act, "command");
        } else if (!strcmp(op, "perc")) {  // bridge 5201: perc [handle id]: perception list of the spawned boss or of any actor by id (+4)
            uint64_t act = n >= 2 ? ActorByHandle((uint32_t)a[0]) : (g_nsp ? g_sp_actor[g_nsp - 1] : 0); PercDiag(act, "cmd");
        } else if (!strcmp(op, "percnear")) {  // percnear [m]: perception list of every non-player actor with an AI within m metres (default 30)
            uint64_t cur[1024]; int k = Actors(cur, 1024); uint64_t me = PlayerChar(); float pp[4] = {0}, r = n >= 2 ? (float)a[0] * 100.0f : 3000.0f; PlayerPos(pp);
            for (int i = 0; i < k; ++i) { if (cur[i] == me) continue; float* q = (float*)(cur[i] + 0x110); float dx = q[0] - pp[0], dz = q[2] - pp[2];
                if (dx * dx + dz * dz <= r * r) { char tg[48]; snprintf(tg, sizeof tg, "near %.1fm%s", sqrtf(dx * dx + dz * dz) / 100.0f, InSp(cur[i]) ? " SPAWNED" : ""); PercDiag(cur[i], tg); } }
        } else if (!strcmp(op, "percfix")) { g_cfg_percfix = (int)a[0]; Log("CMD percfix %d", g_cfg_percfix);
        } else if (!strcmp(op, "percdiag")) { g_cfg_percdiag = (int)a[0]; Log("CMD percdiag %d", g_cfg_percdiag);
        } else if (!strcmp(op, "inject")) { uint64_t act = n >= 2 ? ActorByHandle((uint32_t)a[0]) : (g_nsp ? g_sp_actor[g_nsp - 1] : 0); int sv = g_cfg_percfix; if (!g_cfg_percfix) g_cfg_percfix = 1;
            PercDiag(act, "inject-before"); PercInject(act, "command"); PercDiag(act, "inject-after"); g_cfg_percfix = sv;
        } else if (!strcmp(op, "bpoke")) {  // bpoke off size val: write 1/2/4 bytes at spawned-boss+off (RE)
            uint64_t act = g_nsp ? g_sp_actor[g_nsp - 1] : 0; if (!act) { Log("CMD bpoke: no boss"); return; }
            uint8_t* q = (uint8_t*)(act + a[0]); int64_t old = a[1] == 1 ? *q : a[1] == 2 ? *(uint16_t*)q : *(int32_t*)q;
            if (a[1] == 1) *q = (uint8_t)a[2]; else if (a[1] == 2) *(uint16_t*)q = (uint16_t)a[2]; else *(int32_t*)q = (int32_t)a[2];
            Log("CMD bpoke boss %p +0x%llX (%lld bytes): %lld -> %lld", (void*)act, a[0], a[1], old, a[2]);
        } else if (!strcmp(op, "mon")) { uint64_t act = n >= 2 ? ActorByHandle((uint32_t)a[0]) : (g_nsp ? g_sp_actor[g_nsp - 1] : 0);  // mon [id]: restart the 20 s BOSSMON on an actor
            if (act) { g_mon.a = act; g_mon.t0 = g_mon.last = GetTickCount(); g_mon.minD = 1e9f; Log("CMD mon %p", (void*)act); }
        } else if (!strcmp(op, "awarecfg")) { g_cfg_aware = (int)a[0]; if (n >= 3) g_cfg_aware_ms = (int)a[1]; Log("CMD awarecfg %d delay %d", g_cfg_aware, g_cfg_aware_ms);
        } else if (!strcmp(op, "retry")) { TrainRetry("command");
        } else if (!strcmp(op, "trainstop")) { TrainStop("command");
        } else if (!strcmp(op, "hp")) { uint64_t c = 0, m = 0; int ok = PlayerHp(&c, &m); Log("CMD hp: %s %llu / %llu, NoDeath %d, wrapper %p", ok ? "player" : "no player", c, m, g_tr.nodeath, (void*)PlayerWrapper());
        } else if (!strcmp(op, "nodeath")) { PlayerSetNoDeath((int)a[0]);
        } else if (!strcmp(op, "recover")) { PlayerRecoverFull();
        } else if (!strcmp(op, "actors")) {
            uint64_t cur[1024]; int n = Actors(cur, 1024); uint64_t me = PlayerChar();
            Log("CMD actors: %d in table, player=%p", n, (void*)me);
            for (int i = 0; i < n; ++i) { float xyz[4]; memcpy(xyz, (void*)(cur[i] + 0x110), 16);
                Log("  actor %p%s%s handle %08X %08X pos %.0f %.0f %.0f", (void*)cur[i], cur[i] == me ? " PLAYER" : "", InSp(cur[i]) ? " SPAWNED" : "", *(uint32_t*)cur[i], *(uint32_t*)(cur[i] + 4), xyz[0], xyz[1], xyz[2]); }
        } else if (!strcmp(op, "despawn")) {
            char w[16] = "all"; sscanf(line, "%*s %15s", w);
            if (!strcmp(w, "all")) { for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i]) Despawn(i); }
            else if (!strcmp(w, "last")) { for (int i = g_nsp - 1; i >= 0; --i) if (g_sp_actor[i]) { Despawn(i); break; } }
            else Despawn(atoi(w));
        } else if (!strcmp(op, "kill")) {
            char w[16] = "last"; int drop = 0; sscanf(line, "%*s %15s %d", w, &drop);
            int idx = -1;
            if (!strcmp(w, "last")) { for (int i = g_nsp - 1; i >= 0; --i) if (g_sp_actor[i]) { idx = i; break; } } else idx = atoi(w);
            if (idx < 0 || idx >= g_nsp || !g_sp_actor[idx]) { Log("CMD kill: no spawned actor %s", w); return; }
            uint64_t live = g_sp_actor[idx]; uint32_t flag = *(uint32_t*)(live + 0xAB84); uint64_t comp = *(uint64_t*)(live + 0x3B0);
            Log("CMD kill #%d %p: +ab84=%08X comp=%p drop=%d", idx, (void*)live, flag, (void*)comp, drop);
            if (flag != 0xFFFFFFFFu || !comp) { Log("CMD kill: guard refused (needs +ab84==-1 and comp)"); return; }
            g_kill_act = live; ((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(B + 0x786C84))(comp, 0, 0, 0xFFFFFFFFull, drop ? 1 : 0, 1, 0, 0, 1); g_kill_act = 0;
            Log("CMD kill #%d: 786C84 called", idx);
        } else if (!strcmp(op, "deathcfg")) {  // #6185: deathcfg BossLoot DeathBlowRequired CutsceneOnKill (run-time INI override)
            if (n >= 4) { g_cfg_loot = (int)a[0]; g_cfg_deathblow = (int)a[1]; g_cfg_cutscene = (int)a[2]; }
            Log("CMD deathcfg: BossLoot %d DeathBlowRequired %d CutsceneOnKill %d", g_cfg_loot, g_cfg_deathblow, g_cfg_cutscene);
        } else if (!strcmp(op, "bbm")) { BossModeBegin((uint32_t)a[0], n >= 3 ? (int)a[1] : g_cfg_notify);
        } else if (!strcmp(op, "bgm")) { if (n >= 2 && a[0]) BgmStart((uint32_t)a[0]); else { g_bgm_on = 1; BgmStop("cmd"); }
        } else if (!strcmp(op, "bbmend")) { BossModeEnd("cmd"); g_bm.active = 0;
        } else if (!strcmp(op, "bossmode")) { g_cfg_bossmode = (int)a[0]; if (n >= 3) g_cfg_notify = (int)a[1]; Log("CMD bossmode %d notify %d", g_cfg_bossmode, g_cfg_notify);
        } else if (!strcmp(op, "posttrace")) { g_post_trace = (LONG)a[0]; Log("CMD posttrace %ld", (long)g_post_trace);
        } else if (!strcmp(op, "door")) { DoorCtl((uint32_t)a[0], n >= 3 ? (uint32_t)a[1] : 3);
        } else if (!strcmp(op, "flag")) { SetFlag((uint32_t)a[0], n >= 3 ? (uint8_t)a[1] : 1);
        } else if (!strcmp(op, "rdump")) {  // rdump rva len: raw code/data bytes to <game>\\code_<rva>.bin (offline disassembly, the exe on disk is encrypted)
            char fp[MAX_PATH]; snprintf(fp, sizeof fp, "%s\\code_%llX.bin", g_dir, a[0]);
            HANDLE h = CreateFileA(fp, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0); DWORD w = 0;
            if (h != INVALID_HANDLE_VALUE) { __try { WriteFile(h, (void*)(B + a[0]), (DWORD)a[1], &w, 0); } __except (EXCEPTION_EXECUTE_HANDLER) { } CloseHandle(h); }
            Log("CMD rdump rva %llX len %llu -> %lu bytes written", a[0], a[1], w);
        } else if (!strcmp(op, "fgauge")) { FakeGaugeRequest((uint32_t)a[0], n >= 3 ? (uint32_t)a[1] : 0);
        } else if (!strcmp(op, "unfgauge")) { FakeGaugeHide("cmd");
        } else if (!strcmp(op, "hidegauge")) { GaugeForceHide("cmd");
        } else if (!strcmp(op, "gauge")) {
            ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x1D115D4))((uint32_t)a[0], 0, 0); Log("CMD gauge 0x%llX shown", a[0]);
        } else if (!strcmp(op, "ungauge")) {
            ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x1C208BC))((uint32_t)a[0], 0, 0); Log("CMD ungauge 0x%llX", a[0]);
        } else if (!strcmp(op, "yaw")) {
            float y = CamYawDeg(), dx, dz; FwdVec(y, &dx, &dz); PlayerPos(pos);
            Log("CMD yaw camyaw=%.2f deg mode=%d fwd=(%.3f,%.3f) player %.1f %.1f %.1f", y, g_fwdmode, dx, dz, pos[0], pos[1], pos[2]);
        } else if (!strcmp(op, "fwdmode")) { g_fwdmode = (int)a[0]; Log("CMD fwdmode %d", g_fwdmode);
        } else if (!strcmp(op, "autogauge")) { g_autogauge = (int)a[0]; Log("CMD autogauge %d", g_autogauge);
        } else if (!strcmp(op, "phase")) { g_phase_ms = (DWORD)a[0]; Log("CMD phase interval %lu ms", g_phase_ms);
        } else if (!strcmp(op, "stage")) { StageLog("CMD stage");
        } else if (!strcmp(op, "warp")) {
            float v[4] = {0}; sscanf(line, "%*s %f %f %f %f", &v[0], &v[1], &v[2], &v[3]);
            DWORD t = GetTickCount(); int r = Warp(v[0], v[1], v[2], v[3], 1);
            Log("CMD warp %.0f %.0f %.0f yaw %.1f -> %d (%lu ms)", v[0], v[1], v[2], v[3], r, GetTickCount() - t); StageLog("CMD warp stage");
        } else if (!strcmp(op, "arena")) {
            float v[9] = {0}; unsigned chr = 0;
            int k = sscanf(line, "%*s %u %f %f %f %f %f %f %f %f", &chr, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
            if (k < 8) { Log("CMD arena: need charId x y z yaw bx by bz [bossYaw]"); return; }
            StartArena(chr, v, v + 4, k < 9 ? v[3] + 180.0f : v[7]);
        } else if (!strcmp(op, "nativelist")) { char v[200] = {0}; sscanf(line, "%*s %199s", v); snprintf(g_native_list, sizeof g_native_list, "%s", v); Log("CMD nativelist %s", g_native_list);
        } else if (!strcmp(op, "nativeenter")) { g_cfg_native = (int)a[0]; Log("CMD nativeenter %d", g_cfg_native);
        } else if (!strcmp(op, "enter")) {  // enter <mission>: warp into the table arena of a mission that is already loaded (the script starts the fight)
            ArenaRow* r = FindRow((uint32_t)a[0]); if (!r) { Log("CMD enter: mission 0x%llX not in the table", a[0]); return; }
            int w = Warp(r->p[0], r->p[1], r->p[2], r->p[3], 1); Log("CMD enter %s -> warp %d", r->id, w);
        } else if (!strcmp(op, "killid")) {  // killid <handle id or placement id> [drop]: native EnemyForceKill on the live actor whose id (+4) matches (a native-script boss)
            uint64_t act = ActorByHandle((uint32_t)a[0]); if (!act) { Log("CMD killid: no actor with id %llu (0x%llX)", a[0], a[0]); return; }
            uint32_t flag = *(uint32_t*)(act + 0xAB84); uint64_t comp = *(uint64_t*)(act + 0x3B0); int drop = n >= 3 ? (int)a[1] : 1;
            Log("CMD killid %p: +ab84=%08X comp=%p drop=%d", (void*)act, flag, (void*)comp, drop);
            if (flag != 0xFFFFFFFFu || !comp) { Log("CMD killid: guard refused (needs +ab84==-1 and comp)"); return; }
            g_kill_act = act; ((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(B + 0x786C84))(comp, 0, 0, 0xFFFFFFFFull, drop ? 1 : 0, 1, 0, 0, 1); g_kill_act = 0;
            Log("CMD killid: EnemyForceKill called");
        } else if (!strcmp(op, "fnow")) {  // Fight now through the hooked MissionRequestById (as an add-on caller): swaps in place when the row is eligible, else the native load
            Log("CMD fnow mission 0x%llX", a[0]);
            int r = Nioh3Training_FightNow((uint32_t)a[0]);
            if (!r && GameState() && GameState()[2]) Log("CMD fnow: a load is running, nothing requested");
            else if (!r) { __attribute__((aligned(16))) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0}; Log("CMD fnow: not handled, native request"); ((ReqFn)(o_req ? (void*)o_req : (void*)(B + 0x2239600)))((uint32_t)a[0], 1, (uint32_t)a[1], p); }
        } else if (!strcmp(op, "rm")) {  // bridge 6473 test aid: send the rest of the line to the Boss Rematch pipe (from inside the game process: no second wine process) and log the reply
            const char* t = line + 2; while (*t == ' ') ++t; char* dup = _strdup(t); if (dup) { char* e = dup + strlen(dup); while (e > dup && (e[-1] == '\n' || e[-1] == '\r')) *--e = 0; CloseHandle(CreateThread(0, 0, RmPipeThread, dup, 0, 0)); }
        } else if (!strcmp(op, "fade")) { FadeWant((int)a[0], "CMD fade");
        } else if (!strcmp(op, "lstart")) { LoadScreen(1);
        } else if (!strcmp(op, "lend")) { LoadScreen(0);
        } else if (!strcmp(op, "xcancel")) { XwCancel("CMD xcancel");
        } else if (!strcmp(op, "xtimeout")) { g_xw.t0 -= 50000; if (g_xw.tClaim) g_xw.tClaim -= 50000; Log("CMD xtimeout: pretending the running world request is 50 s old (test aid, bridge 6541)");
        } else if (!strcmp(op, "xpre")) { Log("CMD xpre mission 0x%llX", a[0]); XwPrewarm((uint32_t)a[0], "CMD xpre");
        } else if (!strcmp(op, "preload")) { PreloadNearby("CMD preload");
        } else if (!strcmp(op, "swap")) { g_cfg_enable = (int)a[0]; Log("CMD swap %d", g_cfg_enable);
        } else if (!strcmp(op, "cellcap")) {  // cellcap N: set the stage cell load cap now (4 = native); cellcap alone: print the cell stats
            if (n >= 2) { g_cfg_xcells = (int)a[0]; Log("CMD cellcap %d -> %d", g_cfg_xcells, CellCapSet(g_cfg_xcells < 4 ? 4 : g_cfg_xcells)); }
            uint64_t cn, cr, cf, cq, cm; CellStats(&cn, &cr, &cf, &cq, &cm);
            Log("CMD cellcap: state %d cells %llu loaded %llu loading areas %llu release queue %llu map 1B48 %llu, imm %u/%u", StageState(), cn, cr, cf, cq, cm, *(uint8_t*)(B + 0x31E0B4), *(uint8_t*)(B + 0x31E0C2));
        } else if (!strcmp(op, "mworld")) {
            uint64_t db = *(uint64_t*)(B + 0x45B9E30); db = db ? *(uint64_t*)db : 0;
            for (int i = 0; i < n - 1; ++i) {
                uint64_t r = db ? ((uint64_t (*)(uint64_t, uint32_t))(B + 0x377B38))(db, (uint32_t)a[i]) : 0;
                Log("CMD mworld mission 0x%llX rec %p world 0x%02X", a[i], (void*)r, r ? *(uint8_t*)(r + 0x15C) : 0xFF);
            }
        } else if (!strcmp(op, "oworld")) {
            uint64_t mm = *(uint64_t*)(B + 0x47514D0), st = *(uint64_t*)(mm + 8);
            __attribute__((aligned(16))) uint8_t prm[32] = {0};
            ((void (*)(void*))(B + 0x1BF7D68))(prm);
            prm[7] = 0; prm[0xA] = 0; prm[0xE] = 1;               // WarpOtherWorld
            if (n >= 4 && a[2]) { prm[4] = 1; prm[0xF] = 1; }     // WarpOtherWorldWhiteOut
            ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3B28FC))(2, 0, 0);  // same pre-call as the VM wrapper (GameMain+588 stop bytes)
            uint8_t journey = *(uint8_t*)(st + 0x10); uint32_t mode = *(uint32_t*)(st + 0x24);
            XsStart("oworld", (int)(uint8_t)a[0]);
            ((void (*)(uint8_t, uint8_t, uint32_t, uint32_t, void*))(B + 0x22399B8))((uint8_t)a[0], journey, (uint32_t)a[1], mode, prm);
            Log("CMD oworld world 0x%llX start %llu white %d journey %u mode %u requested", a[0], a[1], n >= 4 && a[2] ? 1 : 0, journey, mode);
            g_ms.t0 = GetTickCount(); g_ms.last = g_ms.t0; g_ms.ticks0 = g_ticks; g_ms.hadPlayer = PlayerChar() != 0; g_ms.active = 1; g_ms.lastTick = g_ms.t0; g_ms.lastBeat = 0; g_ms.window = 60000; PlayerPos(g_ms.lastPos); g_ms.lastTicks = g_ticks; g_ms.lastReg = RegCount();
            g_ms.id = 0;
        } else if (!strcmp(op, "team")) {  // bridge 4878: team id [[actor+0xB0]+0x10] of every live actor
            uint64_t cur[1024]; int k = Actors(cur, 1024); uint64_t me = PlayerChar();
            Log("CMD team: %d actors, TeamFix %d, EnemyTeam %d, ForceOwner %u", k, g_cfg_teamfix, g_cfg_enemyteam, g_force_owner);
            for (int i = 0; i < k; ++i) { float xyz[4]; memcpy(xyz, (void*)(cur[i] + 0x110), 16);
                Log("  actor %p%s%s id %08X team %d pos %.0f %.0f %.0f", (void*)cur[i], cur[i] == me ? " PLAYER" : "", InSp(cur[i]) ? " SPAWNED" : "", *(uint32_t*)(cur[i] + 4), ActorTeam(cur[i]), xyz[0], xyz[1], xyz[2]); }
        } else if (!strcmp(op, "setteam")) {  // setteam <handle id> <team>: write the team of a live actor (RE)
            uint64_t act = ActorByHandle((uint32_t)a[0]); int32_t* tp = TeamPtr(act);
            if (!tp) { Log("CMD setteam: no actor/team for id 0x%llX", a[0]); return; }
            int old = *tp; *tp = (int32_t)a[1]; Log("CMD setteam %p id 0x%llX: team %d -> %d", (void*)act, a[0], old, *tp);
        } else if (!strcmp(op, "teamfix")) { g_cfg_teamfix = (int)a[0]; if (n >= 3) g_cfg_enemyteam = (int)a[1]; Log("CMD teamfix %d enemyteam %d", g_cfg_teamfix, g_cfg_enemyteam);
        } else if (!strcmp(op, "forceowner")) { g_force_owner = (uint32_t)a[0]; Log("CMD forceowner %u", g_force_owner);
        } else if (!strcmp(op, "ownerwait")) { g_cfg_ownwait = (int)a[0]; Log("CMD ownerwait %d ms", g_cfg_ownwait);
        } else if (!strcmp(op, "owneract")) { g_cfg_ownact = (int)a[0]; Log("CMD owneract %d", g_cfg_ownact);
        } else if (!strcmp(op, "act")) {
            uint8_t r = ((uint8_t (*)(uint32_t, uint32_t, uint8_t, uint32_t))(B + 0x1180740))((uint32_t)a[0], 0, 0, 0);
            Log("CMD act %llu -> %u", a[0], r);
        } else if (!strcmp(op, "lchr")) {
            if (!g_loader) { Log("CMD lchr: loader unknown (no Load::Character seen yet)"); return; }
            ((LoadChrFn)(B + 0xA42A7C))(g_loader, (uint32_t)a[0]);
            Log("CMD lchr %llu done (loader %p)", a[0], (void*)g_loader);
        } else if (!strcmp(op, "mission")) {
            __attribute__((aligned(16))) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0};
            ((ReqFn)(o_req ? (void*)o_req : (void*)(B + 0x2239600)))((uint32_t)a[0], (uint8_t)a[1], (uint32_t)a[2], p);  // always the native load (bypasses the arena-swap hook)
            Log("CMD mission 0x%llX slot %llu start %llu requested", a[0], a[1], a[2]);
            g_ms.t0 = GetTickCount(); g_ms.last = g_ms.t0; g_ms.ticks0 = g_ticks; g_ms.hadPlayer = PlayerChar() != 0; g_ms.active = 1; g_ms.lastTick = g_ms.t0; g_ms.lastBeat = 0; g_ms.window = 60000; PlayerPos(g_ms.lastPos); g_ms.lastTicks = g_ticks; g_ms.lastReg = RegCount();
            g_ms.id = (uint32_t)a[0];
        } else if (!strcmp(op, "dumpmem")) {  // dump rva len -> <dir>\\dump_<rva>.bin (RE: disassemble with objdump -D -b binary -mi386:x86-64 --adjust-vma)
            char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\dump_%llX.bin", g_dir, a[0]);
            FILE* f = fopen(p, "wb"); if (f) { fwrite((void*)(B + a[0]), 1, (size_t)a[1], f); fclose(f); }
            Log("CMD dump rva 0x%llX len %lld -> %s (%s)", a[0], a[1], p, f ? "ok" : "open failed");
        } else if (!strcmp(op, "call")) {
            uint64_t r = ((uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t))(B + a[0]))(a[1], a[2], a[3], a[4]);
            Log("CMD call 0x%llX(%llu,%llu,%llu,%llu) -> 0x%llX", a[0], a[1], a[2], a[3], a[4], r);
        } else if (!strcmp(op, "plug")) {
            char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\trplug\\p%llu.dll", g_dir, a[0]);
            HMODULE m = LoadLibraryA(p);
            g_plug = m ? (PlugFn)GetProcAddress(m, "Cmd") : 0;
            Log("CMD plug %s -> %p cmd=%p", p, m, g_plug);
        } else if (g_plug) { g_plug(B, line, Log); }
        else Log("CMD unknown: %s", line);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("CMD EXCEPTION 0x%08lX in: %s", GetExceptionCode(), line);
    }
}

typedef uint64_t (*PumpFn)(uint64_t, uint64_t, uint64_t, uint64_t);
static PumpFn o_pump;
static uint64_t h_pump(uint64_t c, uint64_t d, uint64_t r8, uint64_t r9) {
    if (InterlockedIncrement(&g_ticks) == 1) Log("pump: first game-thread tick (thread %lu)", GetCurrentThreadId());
    if (g_have) {
        char buf[4096]; memcpy(buf, g_cmd, sizeof buf); InterlockedExchange(&g_have, 0);
        for (char* s = strtok(buf, "\r\n"); s; s = strtok(0, "\r\n")) if (*s && *s != '#') { Log("exec: %s", s); Exec(s); }
    }
    __try { TrailTick(); VerifyTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { PhaseTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { PoolTick(g_sp.id); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("POOL: release faulted"); }
    __try { WatchTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_watch = 0; }
    __try { MonTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_mon.a = 0; }
    __try { EnterTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("ENTER tick exception %08lX", GetExceptionCode()); }
    __try { XsTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("XSTREAM tick exception %08lX", GetExceptionCode()); if (g_xs.patched) CellCapSet(4); g_xs.on = 0; g_xs.patched = 0; }
    __try { RhTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("RETURN tick exception %08lX", GetExceptionCode()); g_rh.active = 0; }
    __try { GoldKiRatios(); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { GateTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("GATE tick exception %08lX", GetExceptionCode()); g_gate.tried = 3; }
    __try { XwTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("XWORLD tick exception %08lX", GetExceptionCode()); g_xw.active = 0; g_xw_busy = 0; InterlockedExchange(&g_xw_skip, 0); }
    __try { FadeTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("FADE tick exception %08lX", GetExceptionCode()); g_fd.pend = 0; g_fd.phase = 0; g_fd.inside = 0; g_fd_want = 0; }
    __try { PreloadTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PRELOAD tick exception %08lX", GetExceptionCode()); }
    __try { BgmTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("BGM tick exception %08lX", GetExceptionCode()); g_bgm_on = 0; }
    __try { GaugeTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("GAUGE tick exception %08lX", GetExceptionCode()); g_gg.pending = 0; }
    __try { BossTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("BBM tick exception %08lX", GetExceptionCode()); g_bm.active = 0; }
    __try { ArenaTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("ARENA tick exception %08lX", GetExceptionCode()); g_ar.active = 0; }
    __try { AwareTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_aw.a = 0; }
    __try { TrainTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("TRAIN tick exception %08lX", GetExceptionCode()); g_tr.active = 0; }
    __try { SpawnTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SPAWN tick exception %08lX", GetExceptionCode()); g_sp.active = 0; }
    return o_pump(c, d, r8, r9);
}

// Chain the per-frame call at 56529C (E8 rel32; BossRematch may already redirect it): retarget rel32 to a near stub.
static int HookPump(void) {
    uint8_t* site = (uint8_t*)(B + 0x56529C);
    if (site[0] != 0xE8) { Log("pump site byte %02X != E8", site[0]); return 0; }
    uintptr_t prev = (uintptr_t)(site + 5) + *(int32_t*)(site + 1);
    uint8_t* stub = 0;
    for (uintptr_t a = ((uintptr_t)site - 0x70000000) & ~0xFFFFull; a < (uintptr_t)site + 0x70000000 && !stub; a += 0x10000)
        stub = (uint8_t*)VirtualAlloc((void*)a, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { Log("pump: no near stub"); return 0; }
    stub[0] = 0x48; stub[1] = 0xB8; *(uint64_t*)(stub + 2) = (uint64_t)h_pump; stub[10] = 0xFF; stub[11] = 0xE0;
    o_pump = (PumpFn)prev;
    int32_t rel = (int32_t)((intptr_t)stub - (intptr_t)(site + 5));
    DWORD old; VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    InterlockedExchange((volatile LONG*)(site + 1), rel);  // unaligned but x86 locked xchg is atomic
    VirtualProtect(site, 5, old, &old);
    Log("pump: hooked 56529C, previous target %p (rva %llX), stub %p", (void*)prev, (unsigned long long)(prev - B), stub);
    return 1;
}

static DWORD WINAPI Worker(LPVOID x) {
    Sleep(45000);  // let the loader + BossRematch finish patching, then chain
    // sanity: verify prologues before hooking
    const uint8_t dynp[] = {0x48, 0x8B, 0xC4, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57};
    const uint8_t lchp[] = {0x89, 0x54, 0x24, 0x10, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0xC2, 0x45, 0x33, 0xC0};
    { static const uint8_t fwp[12] = {0x48, 0x8B, 0x09, 0x45, 0x33, 0xC0, 0x44, 0x8B, 0xDA, 0x48, 0x85, 0xC9};
      if (!memcmp((void*)(B + 0x135B78), fwp, 12)) { o_fw = (FwFn)Hook(0x135B78, 12, h_fw); Log("hook FindWrapper ok (boss gauge for dynamic bosses)"); } else Log("hook FindWrapper: prologue mismatch, boss gauge falls back to the legacy call"); }
    if (!memcmp((void*)(B + 0x1C0C5E0), dynp, 13)) { o_dyn = (DynFn)Hook(0x1C0C5E0, 13, h_dyn); Log("hook CreateDynamic ok"); }
    else Log("hook CreateDynamic: prologue mismatch");
    if (!memcmp((void*)(B + 0xA42A7C), lchp, 14)) { o_lchr = (LoadChrFn)Hook(0xA42A7C, 14, h_lchr); Log("hook Load::Character ok"); }
    else Log("hook Load::Character: prologue mismatch");
    { const uint8_t postp[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20};
      if (!memcmp((void*)(B + 0x60BE48), postp, 15)) { o_post = (PostFn)Hook(0x60BE48, 15, h_post); Log("hook registry Post ok"); } else Log("hook registry Post: prologue mismatch"); }
    {   // arena swap: config, table, MissionRequestById hook
        char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
        g_cfg_enable = GetPrivateProfileIntA("ArenaSwap", "Enable", 1, ini);
        g_cfg_aware = GetPrivateProfileIntA("Summon", "AwareFlag", 1, ini); g_cfg_aware_ms = GetPrivateProfileIntA("Summon", "AwareDelayMs", 400, ini);
        g_cfg_spawnverify = GetPrivateProfileIntA("Summon", "SpawnVerify", 1, ini); g_cfg_fn_summon = GetPrivateProfileIntA("Summon", "FightNowSummon", 1, ini); g_cfg_fn_arena = GetPrivateProfileIntA("Summon", "FightNowArena", 1, ini); g_cfg_tr_dist = GetPrivateProfileIntA("Summon", "Distance", 300, ini);
        g_cfg_tr_nodeath = GetPrivateProfileIntA("Summon", "PlayerNoDeath", 1, ini); g_cfg_tr_win = GetPrivateProfileIntA("Summon", "RespawnOnWinMs", 0, ini); g_cfg_tr_respawn = GetPrivateProfileIntA("Summon", "RetryRespawnMs", 2000, ini); g_cfg_body_fade = GetPrivateProfileIntA("Summon", "BodyFade", 1, ini); g_cfg_body_delay = GetPrivateProfileIntA("Summon", "BodyFadeDelayMs", 25000, ini); g_cfg_kill_fade_frames = GetPrivateProfileIntA("Summon", "KillFadeFrames", 120, ini);
        g_cfg_loot = GetPrivateProfileIntA("Summon", "BossLoot", 0, ini); g_cfg_deathblow = GetPrivateProfileIntA("Summon", "DeathBlowRequired", 1, ini); g_cfg_cutscene = GetPrivateProfileIntA("Summon", "CutsceneOnKill", 0, ini); g_cfg_fin_timeout = GetPrivateProfileIntA("Summon", "FinisherKillTimeoutMs", 6000, ini);
        Log("summon death (#6185): BossLoot %d, DeathBlowRequired %d, CutsceneOnKill %d, FinisherKillTimeoutMs %d, BodyFade %d, BodyFadeDelayMs %d, KillFadeFrames %d", g_cfg_loot, g_cfg_deathblow, g_cfg_cutscene, g_cfg_fin_timeout, g_cfg_body_fade, g_cfg_body_delay, g_cfg_kill_fade_frames);
        Log("summon (bridge 4942): FightNowSummon %d, Distance %d, PlayerNoDeath %d, RespawnOnWinMs %d", g_cfg_fn_summon, g_cfg_tr_dist, g_cfg_tr_nodeath, g_cfg_tr_win);
        g_cfg_preload = GetPrivateProfileIntA("ArenaSwap", "Preload", 0, ini);
        g_cfg_preload_max = GetPrivateProfileIntA("ArenaSwap", "PreloadMax", 3, ini);
        g_cfg_hook = GetPrivateProfileIntA("ArenaSwap", "HookFightNow", 0, ini);
        g_cfg_bossdist = GetPrivateProfileIntA("ArenaSwap", "BossDistance", 900, ini); g_cfg_anchor = GetPrivateProfileIntA("ArenaSwap", "BossAnchor", 1, ini);
        g_cfg_ambient = GetPrivateProfileIntA("ArenaSwap", "PreloadAmbient", 0, ini);
        g_cfg_xworld = GetPrivateProfileIntA("ArenaSwap", "CrossWorld", 1, ini); g_cfg_xabbm = GetPrivateProfileIntA("ArenaSwap", "CrossWorldAbortEndBossMode", 1, ini); g_cfg_xearly = GetPrivateProfileIntA("ArenaSwap", "CrossWorldEarlySwap", 1, ini); Log("CrossWorldEarlySwap %d", g_cfg_xearly); g_cfg_xsettle = GetPrivateProfileIntA("ArenaSwap", "CrossWorldSettleMs", 700, ini); g_cfg_xprewarm = GetPrivateProfileIntA("ArenaSwap", "CrossWorldPrewarm", 0, ini); g_cfg_xprewarm_mission = (uint32_t)GetPrivateProfileIntA("ArenaSwap", "CrossWorldPrewarmMission", 0, ini); g_cfg_xhold = GetPrivateProfileIntA("ArenaSwap", "CrossWorldPrewarmHoldMs", 30000, ini); Log("bridge 6375: CrossWorldSettleMs %d, CrossWorldPrewarm %d, PrewarmMission 0x%X", g_cfg_xsettle, g_cfg_xprewarm, g_cfg_xprewarm_mission); g_cfg_xpre = GetPrivateProfileIntA("ArenaSwap", "CrossWorldPreload", 1, ini); Log("cross-world Fight now (bridge 5276): CrossWorld %d, CrossWorldPreload %d", g_cfg_xworld, g_cfg_xpre);
        g_cfg_xaoskip = GetPrivateProfileIntA("ArenaSwap", "CrossWorldAreaObjSkipMs", 0, ini); Log("cross-world (bridge 6047c): CrossWorldAreaObjSkipMs %d", g_cfg_xaoskip);
        g_cfg_xpre_after = GetPrivateProfileIntA("ArenaSwap", "CrossWorldPreloadAfterSwitch", 0, ini); Log("cross-world (bridge 6047b): CrossWorldPreloadAfterSwitch %d", g_cfg_xpre_after);
        g_cfg_xcells = GetPrivateProfileIntA("ArenaSwap", "CrossWorldCellLoads", 0, ini); g_cfg_xblock = GetPrivateProfileIntA("ArenaSwap", "CrossWorldBlockCells", 0, ini); Log("cross-world streaming (bridge 6047): CrossWorldCellLoads %d (%s)", g_cfg_xcells, g_cfg_xcells > 4 ? "cell load cap raised during world switches" : "native cap 4, timeline log only");
        g_cfg_cellskip = GetPrivateProfileIntA("ArenaSwap", "CrossWorldCellSkip", 0, ini);
        if (g_cfg_cellskip) {  // 3A0534 prologue: mov [rsp+8],rbx; mov [rsp+10h],rsi; mov [rsp+18h],rdi (15 bytes, no rip-relative)
            static const uint8_t csp[15] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x48, 0x89, 0x7C, 0x24, 0x18};
            if (!memcmp((void*)(B + 0x3A0534), csp, 15)) { o_cellreq = (CellReqFn)Hook(0x3A0534, 15, h_cellreq); Log("hook stage cell request 3A0534 ok: CrossWorldCellSkip %d (%s)", g_cfg_cellskip, g_cfg_cellskip >= 2 ? "skip non-arena cells during cross-world Fight now" : "log only"); }
            else Log("hook stage cell request 3A0534: prologue mismatch, CrossWorldCellSkip disabled");
        }
        g_cfg_bossmode = GetPrivateProfileIntA("ArenaSwap", "BossMode", 1,ini); g_cfg_bgm = GetPrivateProfileIntA("ArenaSwap", "BossBgm", 1, ini); g_cfg_native = GetPrivateProfileIntA("ArenaSwap", "NativeEnter", 1, ini); GetPrivateProfileStringA("ArenaSwap", "NativeMissions", "0x1CBC", g_native_list, sizeof g_native_list, ini);
        g_cfg_notify = GetPrivateProfileIntA("ArenaSwap", "BossModeNotify", 1, ini);
        g_cfg_ownwait = GetPrivateProfileIntA("ArenaSwap", "OwnerWaitMs", 0, ini); g_cfg_ownact = GetPrivateProfileIntA("ArenaSwap", "OwnerActivate", 0, ini);
        g_cfg_freshid = GetPrivateProfileIntA("ArenaSwap", "GaugeFreshId", 1, ini); g_cfg_gaugereset = GetPrivateProfileIntA("ArenaSwap", "GaugeStateReset", 1, ini); g_cfg_sidefix = GetPrivateProfileIntA("ArenaSwap", "SideFix", 1, ini); g_cfg_pool = GetPrivateProfileIntA("Summon", "PoolMax", 3, ini); g_cfg_awarefix = GetPrivateProfileIntA("ArenaSwap", "AwareFix", 0, ini); g_cfg_percfix = GetPrivateProfileIntA("Summon", "PerceptionFix", 0, ini); g_cfg_percdiag = GetPrivateProfileIntA("Summon", "PerceptionDiag", 1, ini); Log("perception (bridge 5201): PerceptionFix %d, PerceptionDiag %d", g_cfg_percfix, g_cfg_percdiag); g_cfg_teamfix = GetPrivateProfileIntA("ArenaSwap", "TeamFix", 1, ini); g_cfg_enemyteam = GetPrivateProfileIntA("ArenaSwap", "EnemyTeam", -1, ini); g_force_owner = (uint32_t)GetPrivateProfileIntA("ArenaSwap", "ForceOwner", 0, ini);
        Log("team fix (bridge 4878): TeamFix %d, EnemyTeam %d (-1 = from live enemies, else %d), ForceOwner %u", g_cfg_teamfix, g_cfg_enemyteam, DEFAULT_ENEMY_TEAM, g_force_owner);
        g_cfg_fade = GetPrivateProfileIntA("Fade", "Enable", 1, ini); g_cfg_fade_out = GetPrivateProfileIntA("Fade", "OutMs", 250, ini); g_cfg_fade_in = GetPrivateProfileIntA("Fade", "InMs", 400, ini);
        g_cfg_fade_min = GetPrivateProfileIntA("Fade", "MinHoldMs", 800, ini); g_cfg_fade_stable = GetPrivateProfileIntA("Fade", "StableMs", 500, ini); g_cfg_fade_max = GetPrivateProfileIntA("Fade", "MaxHoldMs", 60000, ini);
        Log("fade (bridge 6519): Enable %d, OutMs %d, InMs %d, MinHoldMs %d, StableMs %d, MaxHoldMs %d", g_cfg_fade, g_cfg_fade_out, g_cfg_fade_in, g_cfg_fade_min, g_cfg_fade_stable, g_cfg_fade_max);
        if (g_cfg_fade) {
            HMODULE ef = GetModuleHandleA("Nioh3EngineFix.asi"); if (!ef) ef = GetModuleHandleA("Nioh3EngineFix.dll");
            p_osdFade = ef ? (int (*)(int, int))GetProcAddress(ef, "Nioh3EngineFix_Fade") : 0; p_osdAlpha = ef ? (int (*)(void))GetProcAddress(ef, "Nioh3EngineFix_FadeAlpha") : 0;
            if (p_osdFade && p_osdAlpha) { g_fd_ok = 1; Log("FADE: using the Nioh3EngineFix overlay export (D3D)"); }
            else { p_osdFade = 0; p_osdAlpha = 0; Log("FADE: Nioh3EngineFix fade export not found, using the GDI overlay window"); CreateThread(0, 0, FadeThread, 0, 0, 0); }
        }
        g_imgSize = *(uint32_t*)(B + *(uint32_t*)(B + 0x3C) + 0x18 + 0x38);
        LoadTable();
        const uint8_t reqp[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70, 0x10, 0x48, 0x89, 0x78, 0x18, 0x4C, 0x89, 0x60, 0x20};
        if (!g_cfg_hook) Log("arena swap: Enable %d, Preload %d (max %d, ambient %d); API Nioh3Training_FightNow / _Preload for Boss Rematch; MissionRequestById hook off (HookFightNow = 0)", g_cfg_enable, g_cfg_preload, g_cfg_preload_max, g_cfg_ambient);
        else if (!memcmp((void*)(B + 0x2239600), reqp, sizeof reqp)) { o_req = (ReqFn)Hook(0x2239600, 19, h_req); Log("hook MissionRequestById ok; arena swap on Fight now %s, preload %s (max %d), image 0x%X", g_cfg_enable ? "ON" : "off", g_cfg_preload ? "on" : "off", g_cfg_preload_max, g_imgSize); }
        else Log("hook MissionRequestById: prologue mismatch (hooked by another add-on?): Fight now keeps the native load");
    }
    if (g_cfg_xaoskip > 0) {  // bridge 6047c
        static const uint8_t aorp[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18};
        if (!memcmp((void*)(B + 0x3B3A28), aorp, sizeof aorp)) { o_aor = (AorFn)Hook(0x3B3A28, 15, h_aor); Log("hook StageAreasObjectsReady 3B3A28 ok (CrossWorldAreaObjSkipMs %d)", g_cfg_xaoskip); }
        else { g_cfg_xaoskip = 0; Log("hook StageAreasObjectsReady 3B3A28: prologue mismatch, CrossWorldAreaObjSkipMs off"); }
    }
    {   // user #6789 return gate + #6781 gold Ki bar
        char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
        g_cfg_gate = GetPrivateProfileIntA("Return", "ReturnGate", 1, ini); g_cfg_gate_fake = GetPrivateProfileIntA("Return", "ReturnGateFakeRecord", 1, ini);
        g_cfg_gate_dist = GetPrivateProfileIntA("Return", "ReturnGateDistance", 220, ini); g_cfg_gate_touch = GetPrivateProfileIntA("Return", "ReturnGateTouchMs", 1500, ini);
        g_cfg_gate_radius = GetPrivateProfileIntA("Return", "ReturnGateRadius", 110, ini); GetPrivateProfileStringA("Return", "ReturnGateIds", "10,11,13,12", g_gate_ids, sizeof g_gate_ids, ini);
        g_cfg_goldki = GetPrivateProfileIntA("Summon", "GoldKiBar", 1, ini);
        g_cfg_autoretry = GetPrivateProfileIntA("Summon", "AutoRetryOnDeath", GetPrivateProfileIntA("Summon", "InstantRespawn", 0, ini), ini); Log("AutoRetryOnDeath %d (0 = boss removed on player defeat, Return gate offers Fight again)", g_cfg_autoretry);
        Log("return gate (#6789): ReturnGate %d, FakeRecord %d, Distance %d, TouchMs %d, Radius %d, Ids %s; GoldKiBar %d", g_cfg_gate, g_cfg_gate_fake, g_cfg_gate_dist, g_cfg_gate_touch, g_cfg_gate_radius, g_gate_ids, g_cfg_goldki);
        static const uint8_t rrp[15] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x68, 0x10, 0x48, 0x89, 0x70, 0x18};
        if (g_cfg_gate) { if (!memcmp((void*)(B + 0x22396D8), rrp, 15)) { o_reqret = (ReqRetFn)Hook(0x22396D8, 15, h_reqret); Log("hook MissionRequestReturn 22396D8 ok (return gate)"); } else Log("hook MissionRequestReturn: prologue mismatch, gate works by touch only"); }
        g_cfg_goldki_dump = GetPrivateProfileIntA("Summon", "GoldKiDump", 0, ini); g_cfg_goldki_learn = GetPrivateProfileIntA("Summon", "GoldKiLearn", 1, ini);
        { char b[160]; GetPrivateProfileStringA("Summon", "GoldKiColors", "", b, sizeof b, ini); for (char* s = strtok(b, ", "); s && g_goldki_ncols < 8; s = strtok(0, ", ")) g_goldki_cols[g_goldki_ncols++] = (uint32_t)strtoul(s, 0, 16);
          GetPrivateProfileStringA("Summon", "GoldKiRGB", "255,200,50", b, sizeof b, ini); int r = 255, g = 200, bl = 50; if (sscanf(b, "%d,%d,%d", &r, &g, &bl) == 3) { g_gold_r = (uint8_t)r; g_gold_g = (uint8_t)g; g_gold_b = (uint8_t)bl; } }
        if (g_cfg_goldki) { HookGoldKi(); HookGoldQuad(); }
    }
    HookDeathBlowCond();
    HookPump();
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\Nioh3Training.cmd", g_dir);
    for (;;) {
        Sleep(100);
        if (g_have) continue;
        HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        if (h == INVALID_HANDLE_VALUE) continue;
        DWORD r = 0; char buf[4095]; ReadFile(h, buf, sizeof buf - 1, &r, 0); CloseHandle(h);
        DeleteFileA(p);
        buf[r] = 0;
        if (r) { memcpy(g_cmd, buf, r + 1); InterlockedExchange(&g_have, 1); }
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID x) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        InitializeCriticalSection(&g_logcs);
        B = (uintptr_t)GetModuleHandleA(0);
        GetModuleFileNameA(0, g_dir, sizeof g_dir);
        char* s = strrchr(g_dir, '\\'); if (s) *s = 0;
        Log("Nioh3Training probe loaded, base %p", (void*)B);
        CreateThread(0, 0, Worker, 0, 0, 0);
    }
    return TRUE;
}
