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
#include "gamerva.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
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
    uint8_t* site = (uint8_t*)(B + Grva(rva));
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

static __attribute__((noinline)) int Rd64(uintptr_t a, uint64_t* out) { __try { *out = *(volatile uint64_t*)a; return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }

static uint64_t PlayerChar(void) {
    uint64_t pt = 0, pl = 0;
    if (!Rd64(B + Grva(0x4745348), &pt) || !pt) return 0;   // g:PlayerTab
    if (!Rd64(pt + 0xCD * 0x18, &pl)) return 0;
    return pl;
}
static __attribute__((noinline)) int PlayerPos(float o[4]) {
    uint64_t ch = PlayerChar(); if (!ch) return 0;
    __try { memcpy(o, (void*)(ch + 0x110), 16); return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static float CamYawDeg(void) {
    void* cam = ((void* (*)(int))(B + Grva(0x20A914)))(0);
    if (!cam) return 0.0f;
    return ((float (*)(void*))(B + Grva(0x356018)))(cam) * 57.2957764f;
}
static int g_fwdmode = 0, g_autogauge = 1;
static void FwdVec(float yawDeg, float* dx, float* dz) {
    float y = yawDeg * 0.01745329252f, s = sinf(y), c = cosf(y);
    switch (g_fwdmode) { case 1: *dx = c; *dz = s; break; case 2: *dx = -s; *dz = c; break; case 3: *dx = c; *dz = -s; break; default: *dx = s; *dz = c; }
}
// Actor table: every live actor is linked (node = actor+0xE0, next = *node) into 16 hash buckets {head,count} at [[mgr]+0xB98+8].
static __attribute__((noinline)) int Actors(uint64_t* out, int max) {
    int n = 0; uint64_t mgr = 0, bk = 0;
    __try {
        if (!Rd64(B + Grva(0x4745348), &mgr) || !mgr || !Rd64(mgr + 0xB98 + 8, &bk) || !bk) return 0;
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
// bridge 12666: per-actor identity for multi-boss Training. g_sp_chr[i] = character id of g_sp_actor[i]; slots A/B = the menu's two summon slots
static uint32_t g_sp_chr[MAXSP], g_watch_chr; static int g_watch_slot = -1, g_sp_slot_next = -1, g_watch_session; static int SlotAlive(int s);
// user #14306 / #14309: unlimited Training bosses A, B, C ... (N3T_SLOTS letters; tracking still capped by MAXSP live actors). Each boss has its own
// stance (g_slot_fight: 0 = idle, the default; 1 = Fight normally / CPU), aggro (g_slot_aggro) and HP / Ki page (boss_vitals_game.h).
#include "boss_vitals.h"
#define N3T_SLOTS BV_SLOTS
static uint64_t g_slot_act[N3T_SLOTS]; static uint32_t g_slot_chr[N3T_SLOTS];
static volatile LONG g_slot_fight[N3T_SLOTS], g_slot_rehold[N3T_SLOTS], g_slot_aggro[N3T_SLOTS];
static int g_slot_ih[N3T_SLOTS];  // 1 = this slot is the Training session boss whose stance is still the Idle hold (idle_hold.h), not the slot hold
static int SlotOf(uint64_t a) { for (int s = 0; a && s < N3T_SLOTS; ++s) if (g_slot_act[s] == a) return s; return -1; }
static int SlotFree(void) { for (int s = 0; s < N3T_SLOTS; ++s) if (!SlotAlive(s)) return s; return -1; }
static void SlotNew(int s);  // per-boss defaults for a boss that just took slot s
static uint64_t g_snap[1024]; static int g_nsnap; static DWORD g_watch_t0; static int g_watch; static uint32_t g_watch_id;
static int InSnap(uint64_t a) { for (int i = 0; i < g_nsnap; ++i) if (g_snap[i] == a) return 1; return 0; }
static int InSp(uint64_t a) { for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i] == a) return 1; return 0; }
static int g_dump_pending, g_dump_full; static uint32_t g_dump_owner;
// bridge 4650: log-only diagnostics for "no live registered actor to own the boss" (passive boss)
static __attribute__((noinline)) void DumpWords(const char* tag, uint64_t a, uint32_t owner, int bytes) {
    __try {
        char row[200];
        for (int o = 0; o < bytes; o += 32) {
            int k = 0; for (int j = 0; j < 8; ++j) k += snprintf(row + k, sizeof row - k, "%08X ", *(uint32_t*)(a + o + j * 4));
            Log("  %s +%04X: %s", tag, o, row);
        }
        for (int o = 0; o < 0x2000; o += 4) { uint32_t v = *(uint32_t*)(a + o); if (owner > 1 && v == owner) Log("  %s owner id %u found at +%X", tag, owner, o); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("  %s: unreadable %p", tag, (void*)a); }
}
static __attribute__((noinline)) void DumpOwnerState(const char* why) {
    __try {
        uint64_t cm = 0, c0 = 0, me = PlayerChar(); Rd64(B + Grva(0x45B9E40), &cm); if (cm) Rd64(cm, &c0);
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
static uint64_t PercEntry(uint64_t perc, int i) { return ((uint64_t (*)(uint64_t, int))(B + Grva(0x20A6F0)))(perc, i); }
static int PercFind(uint64_t perc, uint64_t who) { for (int i = 0; i < 8; ++i) if (who && PercEntry(perc, i) == who) return i; return -1; }
static __attribute__((noinline)) int PercDiag(uint64_t actor, const char* tag) {  // log-only; returns the player's entry index or -1
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
        uint64_t mt = ((uint64_t (*)(uint64_t))(B + Grva(0xA8520)))(actor);
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
    if (g_cfg_percfix >= 2) ((void (*)(uint64_t))(B + Grva(0x66982C)))(actor);
    ((void (*)(uint64_t, int))(B + Grva(0x2BE2E8)))(perc, idx);
    ((void (*)(uint64_t, uint8_t, int))(B + Grva(0x1FF1FA0)))(comp, 1, 0);
    return idx;
}
static void AwareDiag(const char* tag) { PercDiag(g_mon.a, tag); }
static __attribute__((noinline)) void AwareApply(void) {
    uint64_t comp = 0, me = PlayerChar(); if (!g_cfg_awarefix || !Rd64(g_mon.a + 0x3B0, &comp) || !comp) return;
    __try {
        AwareDiag("before");
        if (g_cfg_awarefix >= 2) ((void (*)(uint64_t))(B + Grva(0x66982C)))(g_mon.a);
        if (g_cfg_awarefix >= 3 && me) ((void (*)(uint64_t, uint64_t))(B + Grva(0x68210C)))(comp, me);
        ((void (*)(uint64_t, uint8_t, int))(B + Grva(0x1FF1FA0)))(comp, 1, 0);
        AwareDiag("after");
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("AWARE: native call faulted"); }
}
static __attribute__((noinline)) void MonTick(void) {
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
static __attribute__((noinline)) int SetAware(uint64_t actor, const char* why) {
    uint64_t ai = 0; if (!actor || !Rd64(actor + 0x3B0, &ai) || !ai) { Log("AWARE %s: no AI object on %p", why, (void*)actor); return 0; }
    __try { ((void (*)(uint64_t, uint8_t, uint32_t))(B + Grva(0x1FF1FA0)))(ai, 1, 0); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("AWARE %s: exception %08lX", why, GetExceptionCode()); return 0; }
    Log("AWARE %s: SetAwareFlg applied to actor %p (ai %p)", why, (void*)actor, (void*)ai); return 1;
}
static void AwareTick(void) {
    if (!g_aw.a || (int)(GetTickCount() - g_aw.t) < 0) return;
    SetAware(g_aw.a, g_aw.n ? "retry" : "spawn");
    if (g_aw.n++ >= 1) g_aw.a = 0; else g_aw.t = GetTickCount() + 1500;
}
// per-actor identity (bridge 12666): bosses summoned from the same owner share the +4 handle, so gauge / boss mode bind to the actor pointer WatchTick finds
static uint64_t g_newActor; static uint32_t g_newH, g_newSeq;
// user #13164: Summon Boss menu 'Passive time' slider ([Summon] SummonPassiveMs, 0 = aggressive at once). A boss summoned into slot A/B keeps its AI
// off (668D74, the #9133 BOSSHOLD call) for that long after it appears, re-held when the game switches it back on, then gets it back (66982C). SphTick.
#define SUM_PASSIVE_MAX 60000
static int g_cfg_sum_passive = 3000;
static struct { uint64_t a; DWORD t0, tRe; int ms, phase, nOff, sess, chase; } g_sph[N3T_SLOTS];  // chase: #14462 aggro walk-up, AI on while set
// user #14304: in a Training session every boss starts in the Training idle stance. g_sph[s].ms = SPH_TRAINING = slot boss held (AI off) until
// the user picks: stance 'Fight normally' (IhIdleStance 0), a repeat move for that boss (MvRepeatId), or the session ends. Used for bosses
// summoned into slot A / B during a session and for the session boss whose Idle hold a new summon ends (IhKeepForSummon: no AI-on hand-back).
#include "slot_hold.h"  // SPH_TRAINING, SphReleaseWhy / SphAiAction / SphChaseWanted (user #14462)
static int SlotIdleDefault(void); static int IhIdleStance(void); static uint32_t MvRepeatId(uint32_t chr); static void IhKeepForSummon(void); static int TrActive(void); static int MvRepFresh(uint32_t chr);
static float g_slot_sp[N3T_SLOTS][4]; static int g_slot_sp_ok[N3T_SLOTS];  // user #13787: where the boss in each slot appeared ('Reset boss position', aggro.h)
static __attribute__((noinline)) void WatchTick(void) {  // after CreateDynamic: collect actors that were not in the pre-spawn snapshot
    if (!g_watch) return;
    uint64_t cur[1024]; int n = Actors(cur, 1024);
    for (int i = 0; i < n; ++i) if (!InSnap(cur[i]) && !InSp(cur[i]) && *(uint32_t*)(cur[i] + 4) == g_watch_id && g_nsp < MAXSP) {
        float xyz[4] = {0}; memcpy(xyz, (void*)(cur[i] + 0x110), 16);
        g_sp_chr[g_nsp] = g_watch_chr;
        // ally #89: the Training session boss (arena boss / round respawn) is Boss A when slot A is free, so Boss A moves / Summon into A see it.
        // No SummonPassiveMs hold for it: BOSSHOLD / Idle stance own its AI.
        // user #14309: the first free letter when A is taken (summons made before the session)
        if (g_watch_slot < 0 && g_watch_session && g_watch_chr && SlotFree() >= 0) {
            const int fs = SlotFree();
            g_slot_act[fs] = cur[i]; g_slot_chr[fs] = g_watch_chr; memset(&g_sph[fs], 0, sizeof g_sph[0]);
            memcpy(g_slot_sp[fs], xyz, sizeof xyz); g_slot_sp_ok[fs] = 1; SlotNew(fs); g_slot_ih[fs] = 1;
            Log("SLOT %c = Training session boss chr %u actor %p", 'A' + fs, g_watch_chr, (void*)cur[i]);
        }
        g_watch_session = 0;
        if (g_watch_slot >= 0 && g_watch_slot < N3T_SLOTS) { g_slot_act[g_watch_slot] = cur[i]; g_slot_chr[g_watch_slot] = g_watch_chr;
            memcpy(g_slot_sp[g_watch_slot], xyz, sizeof xyz); g_slot_sp_ok[g_watch_slot] = 1; SlotNew(g_watch_slot);
            // user #14304 / #14336 item 7: every summoned boss starts idle (AI held) until its own stance is set to Fight normally or a move repeats;
            // the old SummonPassiveMs 'wait before attacking' timer is no longer used for menu summons.
            if (SlotIdleDefault()) {  // #38077: only a Practice session starts summons idle; Boss Matches summons fight (CPU) from the start
                memset(&g_sph[g_watch_slot], 0, sizeof g_sph[0]); g_sph[g_watch_slot].a = cur[i]; g_sph[g_watch_slot].t0 = GetTickCount();
                g_sph[g_watch_slot].ms = SPH_TRAINING; g_sph[g_watch_slot].phase = 1; g_sph[g_watch_slot].sess = TrActive();
                Log("SUMPASSIVE slot %c actor %p: idle by default, AI held until this boss is set to Fight normally or repeats a move (#14304 / #14309)", 'A' + g_watch_slot, (void*)cur[i]);
            } else { InterlockedExchange(&g_slot_fight[g_watch_slot], 1); Log("SUMPASSIVE slot %c actor %p: CPU-controlled by default (Boss Matches, #38077)", 'A' + g_watch_slot, (void*)cur[i]); }
            g_watch_slot = -1; }
        MvRepFresh(g_watch_chr);  // user #14462: a repeat armed for this character starts over for the new boss (idle first, one gap)
        g_sp_actor[g_nsp++] = cur[i]; g_newActor = cur[i]; g_newH = g_watch_id; g_newSeq++;
        Log("SPAWN actor #%d appeared in table: %p handle %08X %08X pos %.0f %.0f %.0f +ab84=%08X (+%lu ms)", g_nsp - 1, (void*)cur[i], *(uint32_t*)cur[i], *(uint32_t*)(cur[i] + 4), xyz[0], xyz[1], xyz[2], *(uint32_t*)(cur[i] + 0xAB84), GetTickCount() - g_watch_t0);
        { uint64_t c = 0, me = PlayerChar(), pc = 0; Rd64(cur[i] + 0xB0, &c); if (me) Rd64(me + 0xB0, &pc);
          Log("SPAWN actor #%d team %d (player team %d)", g_nsp - 1, c ? *(int32_t*)(c + 0x10) : -999, pc ? *(int32_t*)(pc + 0x10) : -999); }
        if (g_cfg_sidefix) {  // bridge 5030 FIX 1 (pc RE): runtime side change + AI retarget; fallback SetSideEnemy 669510
            int t0 = ActorTeam(cur[i]), pt = ActorTeam(PlayerChar());
            if (1) {  // pc 5033: team already 1 yet the boss stayed passive, so always run the retarget call, not only when the team is wrong
                __try { ((void (*)(uint64_t, int))(B + Grva(0x1C272F0)))(cur[i], 1); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SIDEFIX: 1C272F0 faulted"); }
                int t1 = ActorTeam(cur[i]);
                Log("SIDEFIX: 1C272F0(actor,1) team %d -> %d", t0, t1);
                if (t1 == t0 && (t0 == pt || t0 == 0)) { __try { ((void (*)(uint64_t))(B + Grva(0x669510)))(cur[i]); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SIDEFIX: 669510 faulted"); }
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
static void SlotForget(uint64_t a) { for (int k = 0; k < N3T_SLOTS; ++k) if (a && g_slot_act[k] == a) { g_slot_act[k] = 0; g_slot_chr[k] = 0; g_slot_ih[k] = 0; g_sph[k].phase = 0; } }
static int Despawn(int idx) {
    FakeGaugeHide("despawn");
    if (idx < 0 || idx >= g_nsp) { Log("DESPAWN #%d: no such spawned actor (have %d)", idx, g_nsp); return 0; }
    uint64_t a = g_sp_actor[idx], cur[1024]; int n = Actors(cur, 1024), found = 0;
    for (int i = 0; i < n; ++i) if (cur[i] == a) found = 1;
    if (!found) { Log("DESPAWN #%d %p: not in actor table any more (already gone)", idx, (void*)a); SlotForget(g_sp_actor[idx]); g_sp_actor[idx] = 0; return 0; }
    ((void (*)(uint64_t))(B + Grva(0x2BF7E8)))(a);
    Log("DESPAWN #%d %p: native delete 2BF7E8 called", idx, (void*)a);
    SlotForget(g_sp_actor[idx]); g_sp_actor[idx] = 0;
    return 1;
}
// Character::FadeOut(id, frames) (VM 238DC40): live = wrapper[0]; c = [live+0x20]; if c: 2BC5DC(c+0x240, 0.0f target, frames) -- a lerp {start,target,cur,t,dur}
// toward alpha 0. Mission scripts call it with f30 and follow with Character::Delete. Does not free the actor; Despawn later does. (ported from work-src529, 13:50 fade build)
static __attribute__((noinline)) int FadeOutActor(int idx, float frames) {
    if (idx < 0 || idx >= g_nsp || !g_sp_actor[idx]) return 0;
    uint64_t a = g_sp_actor[idx], cur[1024], c = 0; int n = Actors(cur, 1024), found = 0;
    for (int i = 0; i < n; ++i) if (cur[i] == a) found = 1;
    if (!found) { Log("FADEOUT #%d %p: not in actor table any more", idx, (void*)a); return 0; }
    if (!Rd64(a + 0x20, &c) || !c) { Log("FADEOUT #%d %p: no fade component at +0x20", idx, (void*)a); return 0; }
    __try { ((void (*)(uint64_t, float, float))(B + Grva(0x2BC5DC)))(c + 0x240, 0.0f, frames); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("FADEOUT #%d: exception %08lX", idx, GetExceptionCode()); return 0; }
    Log("FADEOUT #%d %p: native fade 2BC5DC(%p+0x240, 0, %.0f) started", idx, (void*)a, (void*)c, frames);
    return 1;
}
// #6185 summon death toggles. Native Character::EnemyForceKill (VM 238D504) / EnemyForceKillWithItemDrop (VM 238DAA0) both call the death component
// 786C84(comp = actor+0x3B0, dmg 0, type 0, r9 -1, drop, force 1, 0, 0, notify 1) guarded by actor+0xAB84 == -1; the only difference is drop (arg 5):
// 786C84 hands it to the death/drop handler 1FFFEDC(comp, type, drop) and sets death-message bit 0x8000 when it is 0 (no item drop).
// With force = 1 the HP-floor guard (hp+0x188 bit 1, cleared through EC4C5C(hp+0x178,1,-1)) is bypassed, so it kills a boss that sits at HP 1 waiting for its death blow.
// 786C84 returns at once when hp+0x48 == 1 (already dead), so a second call is harmless. Ally bridge 458: Yamagata 'kill last 1' -> loot pillars + Amrita.
static int g_cfg_loot = 0, g_cfg_deathblow = 1, g_cfg_cutscene = 0, g_cfg_fin_timeout = 6000;
// #7465/#7467 [Summon] KiBreakNoFinisher (default 0): no grapple finisher on a watched boss, gold Ki bar while it is Ki-broken, HP-0 window (see NfTick).
static int g_cfg_nofin = 0, g_cfg_nofin_ms = 1000;
// #38077: a Boss Matches session (practice 0) starts with the Boss Match defaults: boss loot OFF and no-finisher / HP-0 window OFF (live only, not saved).
// A Practice session keeps whatever was set; the user's own values come back when the Boss Match ends (BossMatchRestore in TrainStop). The boss itself is CPU-controlled in a Boss Match (IhDefaultSession), summons included (SlotIdleDefault).
static void BossMatchDefaults(void); static void BossMatchRestore(const char* why); static int g_bmd_on, g_bmd_loot, g_bmd_nofin;  // stash of the user's values while a Boss Match runs
static void MvSet(const uint32_t* ids, int n); static int MvForce(uint32_t id); static volatile LONG g_mv_hooked; static volatile int g_mv_nblock; static volatile LONG g_mv_vetoes, g_mv_rerolls;  // #9928 move toggles (defined before KiBreakTick)
static void BhStart(uint32_t handle); static void IhStop(const char* why, int failed); static void IhDefault(int fresh); static void IhDefaultSession(void); static int IhWantIdle(void); static int IhAdoptBossHold(uint32_t handle, uint64_t actor);
static int HpDmgHookInstall(const char* who); static void NfInstallHooks(void); static void NfBreakNow(uint64_t st, const char* why, uint64_t a); static uint64_t NfFirstBoss(void); static int NfPrimeHold(uint64_t a, DWORD ms);
static void NfTraceStart(DWORD ms); static void NfSnap(int diff);
static int g_nf_clear101 = 1; static LONG g_nf_n101;  // #8827 [Summon] KiBreakHp0ClearDeathBlow (run time 'nofin101 0|1')
static int g_nf_kirow_unbreak = 1;  // #8827 [Summon] KiBreakGoldUnbreak (run time 'nofinunbreak 0|1')
static int g_nf_nonlethal = 1, g_nf_prime = 1, g_nf_hprow_ok;  // g_nf_prime: [Summon] KiBreakHp0Prime / 'nofinprime 0|1' (Ki 0.5 prime at the HP floor)
// #9319 [Summon] KiBreakHp0NonLethal (run time 'nofinnl 0|1'), see NfHpDamage
// #9542 [Summon] KiBreakKneelBlockAll (default 1, run time 'nofinkneelall 0|1'): the native kneel (ctrl+0x994 = 0x27 + 668D74 AI-off + actor+0x91B) is also
// blocked on a watched boss whose Ki is <= 0 / broken at HP > 0 (normal Ki breaks), not only in the HP-0 window. [Summon] KiBreakHp0AiHold (default 1,
// 'nofinaihold 0|1'): the boss AI is held off (668D74, re-applied every 100 ms) for the whole HP-0 window = no Ki-broken moves / dashes there.
static int g_nf_kneelall = 1, g_nf_hp0aihold = 1;
// #9579 [Summon] KiBreakBareFix (default 1, run time 'nofinbare 0|1') / KiBreakBareMs (default 200): a 'bare' Ki break (state 0x23 from ctrl+0x1060 bit 0x3A only,
// none of the stagger action's runtime bits 62/45/33; user log 00:56:24: Ki crossed 0 at -11.2 during a boss attack, no hit reaction, break bit set 670 ms
// later) plays no stagger: the boss stands crouched for the 3.8 s break timer with hit reactions off = the 'death stance' (vanilla: grapple bait, which
// KiBreakNoFinisher blocks). After KiBreakBareMs of bare break at HP > 0 the break is cancelled (bit 0x3A + ctrl+0xA8 bit 41 cleared, timer ctrl+0x1014 0)
// and the Ki is set to 0.5 (raw), so the boss acts again and the next hit that lands breaks it natively with the real stagger.
static int g_nf_barefix = 1, g_nf_bare_ms = 200;
// #9895 [Summon] KiBreakActFix (default 1, run time 'nofinact 0|1'): a NORMAL Ki break (stagger bits set, so not bare) whose boss action swaps mid-break
// (live catch 02:53:59 Yoritomo: break action 0x4E4C at +0/+157/+407 ms, then 0x4E3E at +1000 and 0x4E40 at +3.5 s while 0x23 stayed 1 = crouched
// 'death stance'). The break action is learned in the first KiBreakActLearnMs of the break; if the action then differs for KiBreakActFixMs while the
// boss is still Ki-broken at HP > 0 (inside KiBreakActFixWindowMs from the break start), the learned break action is requested again, at most
// KiBreakActFixMax times per break. KiBreakActFixMode 2 (default) = SetActionID's forced transition 66C4A8(state, id, 0, 0.0) (1C0A41C), 1 = the
// RequestActionTrans pending field state+0x2E4 (9A732C; its consumer is not traced).
// #9937 (03:12 F9 shots): re-requesting the START action was wrong - bad breaks also START on a bad id (0xFA6 / 0x4E3E). Now: while Ki-broken at HP > 0
// (HP-0 window excluded) and the boss action is in KiBreakActBadIds for KiBreakActFixMs, KiBreakActGoodId is requested instead (same mode 2/1 path),
// at most KiBreakActFixMax per break, >= KiBreakActFixGapMs apart, inside KiBreakActFixWindowMs. Defaults seen on Yoritomo (R7B35): bad 0x4E3E/0x4E40
// (hunched, wings folded) /0xFA6 (bare crouch), good 0x4E38 (upright, wings spread; 0x4E37 also good). Other bosses: set the ids in the ini.
static int g_nf_actfix = 1, g_nf_actfix_mode = 2, g_nf_actfix_ms = 100, g_nf_actfix_max = 3, g_nf_actfix_win = 4000, g_nf_actfix_learn = 400, g_nf_actfix_gap = 500;
static uint32_t g_nf_actbad[16], g_nf_actgood; static int g_nf_nactbad;  // ini = optional seed only (default empty); #9946: ids are LEARNED per boss
// #9946/#10069 learned ids per boss (key = training chr id, else gauge row id): good = the action held >= 300 ms in a REAL Ki-break stagger (0x23 + stagger
// bits 62/45/33); bad = the action seen in a bare break (bit 0x3A without stagger bits), log only. The swap fires on bare breaks only (see NfTick).
#include "auto_stagger.h"  // user #23432/#23436: auto fallback stagger (hit-reaction action) when no real stagger was ever learned
// [Summon] KiBreakAutoStagger (default 1, run time 'nofinauto 0|1'): a bare / no-action Ki break with no learned GOOD id gets the boss's learned
// hit-reaction action (auto_stagger.h; saved as 'auto' in Nioh3Training_learned.ini). KiBreakAutoHoldMs (default 2500): while that forced action
// plays the BareFix cancel waits (the break is cancelled after it, as before), so the reaction is seen instead of being cut after 200 ms.
static int g_nf_autostag = 1, g_nf_autohold = 2500;
static struct { uint32_t id, good, bad[8]; int nbad; uint32_t autoId; AstStat st[AST_N]; } g_nf_lrn[8];
static void NfLrnSave(int l);
static int NfLrn(uint32_t id) { int f = -1; for (int i = 0; i < 8; ++i) { if (g_nf_lrn[i].id == id && id) return i; if (f < 0 && !g_nf_lrn[i].id) f = i; } if (f < 0) f = 0; memset(&g_nf_lrn[f], 0, sizeof g_nf_lrn[f]); g_nf_lrn[f].id = id;
    {   // #10069: learned ids persist per boss (g_dir/Nioh3Training_learned.ini [boss <chr id>]) so a new session starts with them
        char fp[MAX_PATH], sec[32], v[96]; snprintf(fp, sizeof fp, "%s/Nioh3Training_learned.ini", g_dir); snprintf(sec, sizeof sec, "boss %X", id);
        GetPrivateProfileStringA(sec, "good", "0", v, sizeof v, fp); g_nf_lrn[f].good = (uint32_t)strtoul(v, 0, 0);
        GetPrivateProfileStringA(sec, "auto", "0", v, sizeof v, fp); g_nf_lrn[f].autoId = (uint32_t)strtoul(v, 0, 0); if (AstSkipId(g_nf_lrn[f].autoId)) g_nf_lrn[f].autoId = 0;
        GetPrivateProfileStringA(sec, "bad", "", v, sizeof v, fp); for (const char* q = v; *q && g_nf_lrn[f].nbad < 8; ) { char* e; unsigned long x = strtoul(q, &e, 0); if (e == q) { ++q; continue; } if (x) g_nf_lrn[f].bad[g_nf_lrn[f].nbad++] = (uint32_t)x; q = e; } }
    return f; }
static void NfLrnSave(int l) { char fp[MAX_PATH], sec[32], v[96] = ""; snprintf(fp, sizeof fp, "%s/Nioh3Training_learned.ini", g_dir); snprintf(sec, sizeof sec, "boss %X", g_nf_lrn[l].id);
    snprintf(v, sizeof v, "0x%X", g_nf_lrn[l].good); WritePrivateProfileStringA(sec, "good", v, fp);
    snprintf(v, sizeof v, "0x%X", g_nf_lrn[l].autoId); WritePrivateProfileStringA(sec, "auto", v, fp);
    v[0] = 0; for (int i = 0; i < g_nf_lrn[l].nbad; ++i) { size_t n = strlen(v); snprintf(v + n, sizeof v - n, "%s0x%X", i ? "," : "", g_nf_lrn[l].bad[i]); } WritePrivateProfileStringA(sec, "bad", v, fp); }
static int NfActBadFor(int l, uint32_t id) { for (int i = 0; i < g_nf_nactbad; ++i) if (g_nf_actbad[i] == id) return 1; for (int i = 0; i < g_nf_lrn[l].nbad; ++i) if (g_nf_lrn[l].bad[i] == id) return 1; return 0; }
static int NfAstBad(void* ctx, uint32_t id) { return NfActBadFor((int)(intptr_t)ctx, id); }  // AstPick filter: the boss's learned bare-crouch ids
static void NfLrnBad(int l, uint32_t id) { if (!id || NfActBadFor(l, id) || g_nf_lrn[l].nbad >= 8) return; g_nf_lrn[l].bad[g_nf_lrn[l].nbad++] = id; NfLrnSave(l); }
static int g_nf_kneel = 0;  // HP-0 window: also the native kneel-down action once ([Summon] KiBreakHp0Kneel, run time 'nofinkneel 0|1'). Default 0 since user play
                            // 2026-10-16 15:41 (boss AI on): ForceKneelDown is the scripted death-blow kneel; 1 s after it the boss was in the finisher/down action
                            // (KIFINISHER 35=1 'HP floor (death blow)', 1078 ms) and the user did the finisher although the grapple target was blocked 67 frames.
static volatile int g_nf_gold;  // GoldWalk: a watched boss is Ki-broken or in its HP-0 window right now -> paint the Ki fill quad gold
// user #9133 'gold Ki bar does not return to normal when the Ki refills': the gold bar (full while broken) snapped to the real (low, refilling) Ki in the
// native colour, and an exhausted field lingering after state 0x23 ended kept it gold + full while the Ki refilled. Now gold ends with 0x23 (the native broken
// layout's own test, A6A34 = StateTest 0x23) / Ki > 0, and the bar blends back over KiBreakGoldFadeMs: tint gold -> native colours, fill full -> real Ki.
static int g_cfg_nf_fade = 350; static volatile float g_nf_goldw;  // [Summon] KiBreakGoldFadeMs (0 = instant); goldw = current blend weight (0 = native)
// user #9133 'at HP 0 the boss plays a death pose and ignores hits': every hit at HP 1 is lethal, so 786C84 takes its cond 0x27 revive branch each hit:
// 78700D SetHP 1, then death-blow pending (ctrl+0xEA0 = 1, or ctrl+0xA8 bit 46 = finisher-down 2C) and at 7870D6 ctrl+0xA8 bit 51 + action request 0x308
// (3F21A4(chr+0x2DC0, 0x308) when the actor has that action = the HP-floor / dying motion) -> the boss re-enters the HP-floor pose on every hit and its
// normal hit reaction (3F21A4 at 329C86 in the hit apply) is overridden. [Summon] KiBreakHp0Stagger = 1 (default): for a watched boss h_cond does SetHP 1
// itself and the stub skips 78700D..78714B (no death-blow pending, no 0x308 motion), so the HP-0 window keeps the native hit reaction + the A4F098 break.
static int g_nf_stagger = 1; static volatile LONG g_nf_n_stag; static int g_db_stub3;  // stub3: the 787004 stub has the 'al == 3 -> 78714B' path (bytes verified)
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
static int ActorDeadNow(uint64_t a, int* dead, uint64_t* cur, uint64_t* mx);
static int g_db_done;  // BossTick: the HP-floor / finisher native kill of this boss-mode session was issued
// #7234 'after the kill cutscene the boss should die like in the real mission'. Log 22:52: CutsceneOnKill=1 - the first finisher's lethal hit went through the
// cond 0x27 revive (boss back to HP 1), the force kill at the finisher end did nothing (no 'DEATHBLOW skip' line: 786C84 left early or took the actor+0x894
// revive), the boss stood at HP 1 for 13 s until a 2nd finisher, then the body waited BodyFadeDelayMs (25 s). Now, with CutsceneOnKill / BossLoot, a lethal hit
// while the boss is already at HP 1 AND in the finisher/down state (death sensor 1, sampled every pump frame BEFORE the hit: g_fin_live) skips the revive,
// so the first finisher's own hit kills natively (death animation follows the finisher, drop per BossLoot) = the mission kill. Normal hits at HP 1 still revive.
static volatile uint64_t g_fin_live;  // BossTick: actor at HP <= 1 and in the finisher state right now (CutsceneOnKill / BossLoot with DeathBlowRequired=1)
static volatile uint64_t g_nf_act[4];  // KiBreakNoFinisher watched bosses (defined with the NfTick code)
static __attribute__((noinline)) uint8_t h_cond(uint64_t comp, uint32_t cond) {
    uint8_t r = o_cond(comp, cond);
    if (cond == 0x27 && g_cfg_nofin) {  // #7467 live 04:05: a hit at HP 1 in the HP-0 window killed the boss natively here (cond 0x27 false once its break
        uint64_t a = 0;                  // ended) -> a watched boss always takes the revive-to-HP-1 branch; NfTick clears the death-blow flag and decides
        if (Rd64(comp, &a) && a && a != g_kill_act) for (int i = 0; i < 4; ++i) if (g_nf_act[i] == a) {
            if (!g_nf_stagger || !g_db_stub3) return 1;  // native revive branch (HP 1 + death-blow pending + HP-floor motion)
            uint64_t st = 0; if (!Rd64(a + 0x3A0, &st) || !st) return 1;
            __try { ((void (*)(uint64_t, uint64_t))(B + Grva(0x66C188)))(st + 0x38, 1); } __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }  // = 787010..787020
            { LONG k = InterlockedIncrement(&g_nf_n_stag); if (k <= 10 || k % 50 == 0) Log("NOFIN boss %p lethal hit at HP floor #%ld: HP 1, death-blow pending + HP-floor motion 0x308 skipped (KiBreakHp0Stagger)", (void*)a, k); }
            return 3;  // stub: skip to 78714B (after the 0x308 request)
        }
    }
    if (!r || cond != 0x27) return r;
    uint64_t a = 0; if (!Rd64(comp, &a) || !a) return r;
    int forcing = a == g_kill_act, finisher = g_cfg_deathblow && (g_cfg_cutscene || g_cfg_loot) && a == g_fin_live && a == g_db_act;
    if (!forcing && g_cfg_nofin) return r;  // #7467: KiBreakNoFinisher owns the HP floor (2 s window, then NfTick's force kill)
    if (!forcing && !finisher &&(g_cfg_deathblow || (a != g_db_act && !InSp(a)))) return r;
    g_killHideAct = a;  // #6711: not from inside the damage function, the HUD is touched on the pump tick
    if (finisher) g_db_done = 1;  // BossTick: no extra force kill at the finisher end
    if (InterlockedIncrement(&g_db_logs) <= 20) Log("DEATHBLOW skip: actor %p lethal hit, death-blow revive (cond 0x27) skipped -> native death (%s, drop %s)", (void*)a, forcing ? "force kill" : finisher ? "first finisher, CutsceneOnKill" : "DeathBlowRequired=0", forcing ? "per kill call" : g_cfg_loot ? "game" : "off");
    return (forcing || g_cfg_loot) ? 0 : 2;
}
static int HookDeathBlowCond(void) {
    uint8_t* site = (uint8_t*)(B + Grva(0x787004));
    static const uint8_t pre[8] = {0xBA, 0x27, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xCF}, post[4] = {0x84, 0xC0, 0x74, 0x4B};
    if (memcmp(site - 8, pre, 8) || site[0] != 0xE8 || memcmp(site + 5, post, 4) || memcmp((void*)(B + Grva(0x787219)), "\x84\xDB\x75\x08", 4)) { Log("DEATHBLOW hook 787004: bytes mismatch, DeathBlowRequired=0 falls back to the native kill only"); return 0; }
    uintptr_t prev = (uintptr_t)(site + 5) + *(int32_t*)(site + 1);
    uint8_t* stub = 0;
    for (uintptr_t a = ((uintptr_t)site - 0x70000000) & ~0xFFFFull; a < (uintptr_t)site + 0x70000000 && !stub; a += 0x10000)
        stub = (uint8_t*)VirtualAlloc((void*)a, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { Log("DEATHBLOW hook: no near stub"); return 0; }
    // sub rsp,28h; mov rax,h_cond; call rax; add rsp,28h;
    // #9133: cmp al,3; jne L; add rsp,8 (drop the return to 787009); mov r12b,1; mov r14d,308h (register state of the native revive path at 78714B); mov rax,78714B; jmp rax
    // L: cmp al,2; jne ret; xor bl,bl; xor eax,eax; ret
    static const uint8_t code[] = {0x48, 0x83, 0xEC, 0x28, 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x28,
                                   0x3C, 0x03, 0x75, 0x19, 0x48, 0x83, 0xC4, 0x08, 0x41, 0xB4, 0x01, 0x41, 0xBE, 0x08, 0x03, 0x00, 0x00, 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0,
                                   0x3C, 0x02, 0x75, 0x04, 0x30, 0xDB, 0x31, 0xC0, 0xC3};
    memcpy(stub, code, sizeof code); *(uint64_t*)(stub + 6) = (uint64_t)h_cond; *(uint64_t*)(stub + 39) = B + Grva(0x78714B);
    // 78700D..78714B as expected (revive: mov rax,[rdi]; mov edx,1 ... / 7870E4 mov r14d,308h / 78714B call 2086D4): else al == 3 is never returned
    {   const uint8_t* t = (const uint8_t*)(B + Grva(0x78714B));
        g_db_stub3 = !memcmp((void*)(B + Grva(0x78700D)), "\x48\x8B\x07\xBA\x01\x00\x00\x00", 8) && !memcmp((void*)(B + Grva(0x7870E4)), "\x41\xBE\x08\x03\x00\x00", 6) &&
                     t[0] == 0xE8 && (uintptr_t)(t + 5) + *(int32_t*)(t + 1) == B + Grva(0x2086D4);
        Log("DEATHBLOW hook 787004: HP-floor stagger path (KiBreakHp0Stagger) %s", g_db_stub3 ? "available" : "OFF (bytes differ)"); }
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
// #7234: 786C84 leaves at once while [comp+8]+0xEB2 != 0 (786CEC) and revives to HP 1 without cond 0x27 while actor+0x894 != 0 (786F7E): the post-finisher
// force kill of the 22:52 log did nothing. unblock = 1 (BossTick retries only) clears those two bytes for the kill call and puts them back if the boss lived.
static __attribute__((noinline)) int ForceKillActorEx(uint64_t live, int drop, const char* why, int unblock) {
    if (!live) return 0;
    uint32_t flag = 0; uint64_t comp = 0, h = 0, c8 = 0; int dead = 0; uint8_t eb2 = 0xFF, f894 = 0xFF;
    __try { flag = *(uint32_t*)(live + 0xAB84); comp = *(uint64_t*)(live + 0x3B0); if (Rd64(live + 0x3A0, &h) && h) dead = *(int*)(h + 0x48) == 1;
            if (comp && Rd64(comp + 8, &c8) && c8) eb2 = *(uint8_t*)(c8 + 0xEB2); f894 = *(uint8_t*)(live + 0x894); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    if (dead) { Log("KILL %s %p: already dead (hp+48 == 1), nothing to do", why, (void*)live); return 0; }
    if (flag != 0xFFFFFFFFu || !comp) { Log("KILL %s %p: guard refused (+ab84=%08X comp=%p; the script natives need +ab84 == -1)", why, (void*)live, flag, (void*)comp); return 0; }
    int cleared = 0;
    if (unblock) { __try { if (c8 && eb2 && eb2 != 0xFF) { *(uint8_t*)(c8 + 0xEB2) = 0; cleared |= 1; } if (f894 && f894 != 0xFF) { *(uint8_t*)(live + 0x894) = 0; cleared |= 2; } } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    g_kill_act = live;  // h_cond: a force kill really kills (no death-blow revive to HP 1)
    __try { ((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(B + Grva(0x786C84)))(comp, 0, 0, 0xFFFFFFFFull, drop ? 1 : 0, 1, 0, 0, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_kill_act = 0; Log("KILL %s: exception %08lX", why, GetExceptionCode()); return 0; }
    g_kill_act = 0;
    { uint64_t c = 1, m = 0; int dd = 0; int ok = ActorDeadNow(live, &dd, &c, &m);
      if (!ok && cleared) { __try { if (cleared & 1) *(uint8_t*)(c8 + 0xEB2) = eb2; if (cleared & 2) *(uint8_t*)(live + 0x894) = f894; } __except (EXCEPTION_EXECUTE_HANDLER) { } }
      Log("KILL %s %p: native EnemyForceKill%s 786C84 called -> %s (hp+48 %d, HP %llu / %llu; gates before: chr+EB2 %u, actor+894 %u%s)", why, (void*)live, drop ? "WithItemDrop" : "", ok ? "dead" : "still alive",
          dd, c, m, eb2, f894, cleared ? (ok ? ", cleared for the kill" : ", cleared for the kill and restored") : ""); }
    BossKilledNow(live, why);  // #6711 / #6774: boss mode end + bar off the moment the kill is applied (pump thread, after 786C84 returned)
    return 1;
}
static int ForceKillActor(uint64_t live, int drop, const char* why) { return ForceKillActorEx(live, drop, why, 0); }

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
static __attribute__((noinline)) void h_post(uint64_t reg, uint64_t* pair) {
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
static struct { uint32_t id, owner, bgm; float dist; DWORD t0, tOwn; int active, boss, slot; } g_sp;
static uint64_t ResEntry(uint32_t id) {
    uint64_t key = ((uint64_t (*)(uint64_t, uint32_t))(B + Grva(0x134314)))(0, id);
    uint64_t rm = 0; if (!Rd64(B + Grva(0x45B9E60), &rm) || !rm) return 0;
    return ((uint64_t (*)(uint64_t, uint64_t))(B + Grva(0x133440)))(rm, key);
}
static uint32_t g_force_owner;  // debug (bridge 4878): ForceOwner / 'forceowner N' makes PickOwner return N (1 = reproduce the no-live-owner case on any save)
static uint32_t PickOwner(void) {  // a registered placement with a live actor (not the player): enemy records first, then any live registered actor; 1 = the player (team fix below keeps the boss hostile)
    if (g_force_owner) return g_force_owner;
    uint64_t cm = 0, c0 = 0; Rd64(B + Grva(0x45B9E40), &cm); if (!cm) return 1; Rd64(cm, &c0); if (!c0) return 1;
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
static __attribute__((noinline)) int ActorTeam(uint64_t actor) { int32_t* p = TeamPtr(actor); int v = -999; if (p) { __try { v = *p; } __except (EXCEPTION_EXECUTE_HANDLER) { v = -999; } } return v; }
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
    uint32_t r = ((DynFn)(B + Grva(0x1C0C5E0)))(chr, p, yaw, 0, owner, 0);
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
static struct { int pending, shown; uint32_t handle, owner, seq; int bindNew; uint64_t actor; DWORD t0, tHide, tVis; int waitLog; } g_gg;
// user #9685: the boss bar showed at spawn +16 ms while the Fight now screen was still black (fade overlay / load cover; log 01:29:29.67 vs screen
// visible ~01:29:30.7). [Summon] BossBarAfterVisibleMs (default 600, -1 = old: at once): the bar waits until the screen is visible (BhScreenVisible:
// GAMESTATE[2] 0, no mission-load sequencer state, our fade clear) + this many ms, max 60 s.
static int g_cfg_bar_vis = 600; static int BhScreenVisible(int* why);
// bridge 4801: the native HideBossGauge (queued event) leaves the empty frame on screen. Do what BossGaugeSetup does for unused rows by hand: clear the row vector (hud+0x3C0, 1CEE8C8),
// hide the 3 row elements (type 0xF5) and the 42 detail elements (type 0xF6) with 5D1D8C(elem,0), then hide the HUD (1CEC2E0).
static int g_hudHidden, g_noForceHide; static int g_cfg_freshid = 1, g_cfg_gaugereset = 1; static uint32_t g_gaugeN; static int g_elemsHidden; static DWORD g_elemShowAt;
static __attribute__((noinline)) void GaugeForceHide(const char* why) {
    uint64_t root = 0, hud = 0; Rd64(B + Grva(0x45C84A8), &root); if (!root || !Rd64(root + 0xA8, &hud) || !hud) { Log("GAUGE force hide (%s): no HUD", why); return; }
    __try {
        ((void (*)(uint64_t))(B + Grva(0x1CEE8C8)))(hud + 0x3C0);
        typedef uint64_t (*ElemFn)(uint64_t, void*, uint32_t, uint64_t);
        for (int pass = 0; pass < 2; pass++) {
            uint32_t type = pass ? 0xF6 : 0xF5, cnt = pass ? 0x2A : 3;
            for (uint32_t i = 0; i < cnt; i++) {
                uint8_t tmp[256] = {0};
                uint64_t e = ((ElemFn)(B + Grva(0x300F44)))(hud + 0x140, tmp, type, i);
                ((void (*)(void*, uint8_t))(B + Grva(0x5D1D8C)))(e ? (void*)e : (void*)tmp, 0);
            }
        }
        ((void (*)(uint64_t, uint8_t))(B + Grva(0x1CEC2E0)))(hud, 0); g_hudHidden = 1; g_elemsHidden = 1;
        Log("GAUGE force hide done (%s), hud %p", why, (void*)hud);
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("GAUGE force hide exception %08lX (%s)", GetExceptionCode(), why); }
}
// bridge 12666: after a force hide the rows/details stay hidden (a summon after despawn shows no bar): set their visibility back on. rows: type 0xF5 x3, details: type 0xF6 x42
static __attribute__((noinline)) void GaugeElemVis(int rows, int details, uint8_t on) {
    uint64_t root = 0, hud = 0; Rd64(B + Grva(0x45C84A8), &root); if (!root || !Rd64(root + 0xA8, &hud) || !hud) { Log("GAUGE elem vis: no HUD"); return; }
    __try {
        typedef uint64_t (*ElemFn)(uint64_t, void*, uint32_t, uint64_t);
        for (int pass = 0; pass < 2; pass++) {
            if (!(pass ? details : rows)) continue;
            uint32_t type = pass ? 0xF6 : 0xF5, cnt = pass ? 0x2A : 3;
            for (uint32_t i = 0; i < cnt; i++) { uint8_t tmp[256] = {0}; uint64_t e = ((ElemFn)(B + Grva(0x300F44)))(hud + 0x140, tmp, type, i); ((void (*)(void*, uint8_t))(B + Grva(0x5D1D8C)))(e ? (void*)e : (void*)tmp, on); }
        }
        ((void (*)(uint64_t, uint8_t))(B + Grva(0x1CEC2E0)))(hud, on); if (on) g_hudHidden = 0;
        Log("GAUGE elem vis rows %d details %d -> %d", rows, details, on);
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("GAUGE elem vis exception %08lX", GetExceptionCode()); }
}
static DWORD g_forceHideAt;
static void FakeGaugeDrop(void) { g_fwbuf[0] = 0; g_fakew = 0; g_gg.tHide = 0; }
static void FakeGaugeHide(const char* why) {  // the native hide is a queued message that looks the id up again later: keep the fake wrapper valid for 3 s (GaugeTick drops it)
    g_gg.pending = 0; g_elemShowAt = 0; if (!g_gg.shown) return;
    ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x1C208BC)))(g_fakeid_shown, 0, 0);
    g_gg.shown = 0; g_gg.tHide = GetTickCount() | 1; Log("GAUGE HideBossGauge(fake id) (%s)", why);
    GaugeForceHide(why); g_forceHideAt = GetTickCount() + 700; if (!g_forceHideAt) g_forceHideAt = 1;  // again after the queued native hide ran
}
static void FakeGaugeRequest(uint32_t handle, uint32_t owner) {  // the actor enters the table a few ms after CreateDynamic: GaugeTick finishes the job
    if (!o_fw) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x1D115D4)))(handle, 0, 0); Log("GAUGE FindWrapper hook missing: legacy ShowBossGauge(0x%X)", handle); return; }
    FakeGaugeHide("new request"); FakeGaugeDrop(); g_gg.pending = 1; g_gg.handle = handle; g_gg.owner = owner; g_gg.t0 = GetTickCount(); g_gg.tVis = 0; g_gg.waitLog = 0; g_gg.seq = g_newSeq; g_gg.actor = 0; g_gg.bindNew = g_watch && g_watch_id == handle;
}
// #6711: hide the boss bar the instant its boss is killed (h_cond skip or ForceKillActor), not at boss-mode end (4.5 s later). Only the actor the shown
// bar points at (g_fwbuf[0]) and only if it is really dead (hp+0x48 == 1 or HP 0). FakeGaugeHide sets shown = 0, so the later "boss mode end" hide is a no-op;
// the next FakeGaugeRequest shows the HUD again through the g_hudHidden path. No game-state change, only the HUD.
static __attribute__((noinline)) int ActorDeadNow(uint64_t a, int* dead, uint64_t* cur, uint64_t* mx) {  // dead = hp+0x48 == 1 (death applied) or HP 0
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
static __attribute__((noinline)) void GaugeTick(void) {
    { uint64_t ka = g_killHideAct; if (ka) { g_killHideAct = 0; BossKilledNow(ka, "lethal hit, death-blow revive skipped"); } }
    if (g_forceHideAt && (int)(GetTickCount() - g_forceHideAt) >= 0) { g_forceHideAt = 0; GaugeForceHide("delayed"); }
    if (g_gg.tHide && GetTickCount() - g_gg.tHide > 3000) FakeGaugeDrop();
    if (g_elemShowAt && (int)(GetTickCount() - g_elemShowAt) >= 0) { g_elemShowAt = 0; if (g_gg.shown) { GaugeElemVis(1, 0, 1); g_elemsHidden = 0; } }
    if (!g_gg.pending) return;
    if (g_gg.bindNew && !g_gg.actor && g_newSeq != g_gg.seq && g_newH == g_gg.handle) g_gg.actor = g_newActor;
    uint64_t a = g_gg.bindNew ? g_gg.actor : ActorByHandle(g_gg.handle);
    if (!a) { if (GetTickCount() - g_gg.t0 > 4000) { Log("GAUGE actor 0x%X never appeared, no bar", g_gg.handle); g_gg.pending = 0; } return; }
    if (g_cfg_bar_vis >= 0) {  // #9685
        DWORD nowg = GetTickCount(); int why = 0;
        if (!BhScreenVisible(&why) && nowg - g_gg.t0 < 60000) { g_gg.tVis = 0; if (!g_gg.waitLog) { g_gg.waitLog = 1; Log("GAUGE bar held until the boss is on a visible screen (%s, BossBarAfterVisibleMs %d, #9685)", why == 1 ? "GAMESTATE[2] load cover" : why == 2 ? "mission load sequencer" : "fade overlay", g_cfg_bar_vis); } return; }
        if (!g_gg.tVis) g_gg.tVis = nowg;
        if (nowg - g_gg.tVis < (DWORD)g_cfg_bar_vis && nowg - g_gg.t0 < 60000) return;
        Log("GAUGE bar shown +%lu ms after the boss request (screen visible %lu ms, #9685)", nowg - g_gg.t0, nowg - g_gg.tVis);
    }
    uint64_t cm = 0, tmpl = 0; Rd64(B + Grva(0x45B9E40), &cm);
    if (g_gg.owner > 1 && cm) tmpl = o_fw(cm, g_gg.owner);
    memset(g_fwbuf, 0, sizeof g_fwbuf);
    if (tmpl) { __try { memcpy(g_fwbuf, (void*)tmpl, sizeof g_fwbuf); } __except (EXCEPTION_EXECUTE_HANDLER) { memset(g_fwbuf, 0, sizeof g_fwbuf); } }
    g_fwbuf[0] = a; *(uint32_t*)((uint8_t*)g_fwbuf + 0xD4) = 0;  // name text id from the actor's charParam
    g_fakew = (uint64_t)g_fwbuf;
    if (g_hudHidden) {  // bridge 4942: GaugeForceHide hid the gauge HUD (1CEC2E0(hud,0)): show it again before the new bar, else rounds 2+ have no boss bar
        uint64_t root = 0, hud = 0; Rd64(B + Grva(0x45C84A8), &root);
        if (root && Rd64(root + 0xA8, &hud) && hud) { __try { ((void (*)(uint64_t, uint8_t))(B + Grva(0x1CEC2E0)))(hud, 1); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
        g_hudHidden = 0;
    }
    g_forceHideAt = 0;  // bridge 4942: a retry hides the old bar and shows the new one 300 ms later: the delayed force-hide of the old bar must not hit the new one
    if (g_cfg_freshid) g_fakeid = FAKE_GAUGE_ID + (++g_gaugeN & 0xFFF);
    g_fakeid_shown = g_fakeid;
    {   // bridge 5479: BossGaugeSetup 1D05824 starts with hud vf50 (1CFD6D4 -> 563754) = (hud+0x2C == 0 && hud+0x1F == 0). Show runs 70781C(hud) which sets hud+0x2C = 1 (opening);
        // only the native hide/close state machine puts it back to 0, and GaugeForceHide bypasses that, so every bar after the first was refused (rows stayed 0). Reset the idle state first.
        uint64_t root = 0, hud = 0; Rd64(B + Grva(0x45C84A8), &root); if (root) Rd64(root + 0xA8, &hud);
        __try { if (hud) {
            uint32_t s2c = *(uint32_t*)(hud + 0x2C); uint8_t s1f = *(uint8_t*)(hud + 0x1F);
            Log("GAUGE pre-show #%u: hud+2C=%u hud+1F=%u%s", g_gaugeN, s2c, s1f, (s2c || s1f) && g_cfg_gaugereset ? " -> reset to 0" : "");
            if (g_cfg_gaugereset) { *(uint32_t*)(hud + 0x2C) = 0; *(uint8_t*)(hud + 0x1F) = 0; }
        } } __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
    ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x1D115D4)))(g_fakeid, 0, 0);
    g_gg.pending = 0; g_gg.shown = 1;
    if (g_elemsHidden) { g_elemShowAt = GetTickCount() + 250; if (!g_elemShowAt) g_elemShowAt = 1; }  // bridge 12666: force hide left the row elements hidden: show them again after the native row exists
    {   // bridge 5437 diag: ShowBossGauge (1D05824) is synchronous; it needs hud vfunc +0x50 true, a FindWrapper hit and 4892A8(actor) false. rows = bars added.
        uint64_t root = 0, hud = 0, r0 = 0, r1 = 0; int v50 = -1, blk = -1; Rd64(B + Grva(0x45C84A8), &root); if (root) Rd64(root + 0xA8, &hud);
        __try { if (hud) { v50 = ((uint8_t (*)(uint64_t))(*(uint64_t*)(*(uint64_t*)hud + 0x50)))(hud); r0 = *(uint64_t*)(hud + 0x3C0); r1 = *(uint64_t*)(hud + 0x3C8); } blk = ((uint8_t (*)(uint64_t))(B + Grva(0x4892A8)))(a); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        Log("GAUGE diag #%u: hud %p vf50=%d rows=%lld actorBlocked(4892A8)=%d", g_gaugeN, (void*)hud, v50, r0 ? (long long)((r1 - r0) / 0x60) : -1LL, blk);
    }
    Log("GAUGE ShowBossGauge(fake id 0x%X) for actor %p (handle 0x%X, wrapper template owner %u -> %p)", g_fakeid, (void*)a, g_gg.handle, g_gg.owner, (void*)tmpl);
}
static int g_cfg_gate;  // [Return] ReturnGate (defined with the gate code)
static int g_cfg_autoretry = 1;  // [Summon] AutoRetryOnDeath (merged with jumpfix InstantRespawn #7118). user #13229/#13230: ALWAYS 1 in Training/Rematch (ini and API cannot turn it off); see TrainTick / TrainLost
static int g_practice_next;  // user #13223: set by Boss Rematch's Practice pages before the session starts; consumed into g_tr.practice
static struct { int active, round, nodeath, beaten, hasStart, bodyGone, lost, practice, arena; uint32_t chr, bgm; char tok[32]; DWORD tCmd, tWin, tBeat, tRespawn, tReport, tFade, tFadeAt; uint64_t fadeAct; float pp[4], bp[3], face; } g_tr;  // pp/bp/face: round-1 player and boss spawn pose (bridge 5310)
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
static void SumNoDeathRelease(const char* why); static int PlayerSetNoDeath(int on); static int g_cfg_tr_nodeath_summon = 0, g_tr_nodeath_now, g_sum_nodeath;  // user #13197: SummonNoDeath default 0; g_sum_nodeath = NoDeath was switched on by a summon
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
    g_watch = r != 0; g_watch_id = r; g_watch_t0 = GetTickCount(); g_watch_chr = g_sp.id; g_watch_slot = r ? g_sp.slot : -1; g_watch_session = r && g_tr.active && g_sp.boss && g_sp.slot < 0;
    Log("SPAWN %u: resources ready after %lu ms; camyaw %.1f fwd %.2f %.2f; CreateDynamic(owner %u) player %.0f %.0f %.0f -> at %.0f %.0f %.0f yaw %.1f -> 0x%X%s", g_sp.id, dt, cy, dx, dz, owner, pos[0], pos[1], pos[2], p[0], p[1], p[2], face, r, r ? "" : " (refused)");
    if (r && g_autogauge) { FakeGaugeRequest(r, owner); Log("SPAWN %u: boss gauge requested for 0x%X", g_sp.id, r); }
    // ally #89 / #13167: a Training round boss (retry / Training here) in Idle stance is held from its first frame and handed to Idle stance,
    // as the arena boss is, instead of attacking until its first native idle
    if (r && g_sp.boss && g_sp.slot < 0 && g_tr.active && IhWantIdle()) BhStart(r);
    if (r && g_sp.boss) { SummonFinish(r, g_sp.bgm); float ex = p[0] - pos[0], ez = p[2] - pos[2]; g_vf.active = !g_tr.arena && g_sp.slot < 0; g_vf.h = r; g_vf.t0 = GetTickCount();  /* bridge 6566: an arena session (g_tr.arena; was g_tr.chr, #14546) keeps the boss at its round-1 pose; the 5 s 'stuck' verify despawned it and respawned it at a candidate spot */ g_vf.d0 = sqrtf(ex * ex + ez * ez); }
    if (r && g_sp.boss && g_cfg_tr_nodeath_summon && !g_tr_nodeath_now && PlayerSetNoDeath(1)) g_sum_nodeath = 1;  // bridge 12666 ([Summon] SummonNoDeath, default 0 since #13197); SumNoDeathTick turns it off again
    g_sp.active = 0;
}

// ---- stage streamer (global stage mgr [45B9E58]; sub = +0x500: byte +0 world, u32 +4 main cell, u8 +0x389 init state 0..5, set +0x40 (count +0x60))
static uint64_t StageMgr(void) { uint64_t s = 0; Rd64(B + Grva(0x45B9E58), &s); return s; }
static int StageState(void) { uint64_t s = StageMgr(); return s ? *(uint8_t*)(s + 0x500 + 0x389) : -1; }
static void StageLog(const char* tag) {
    uint64_t s = StageMgr(); if (!s) { Log("%s stage mgr null", tag); return; }
    uint64_t sub = s + 0x500; uint64_t n = *(uint64_t*)(sub + 0x60);
    char buf[700]; int k = 0;
    for (uint64_t i = 0; i < n && i < 48 && k < 600; ++i) {
        uint64_t e = ((uint64_t (*)(uint64_t, uint64_t))(B + Grva(0xD3B9C)))(sub + 0x40, i);
        if (e) k += snprintf(buf + k, sizeof buf - k, "%u%s ", *(uint32_t*)e >> 4, *(uint8_t*)(e + 0xE8) ? "*" : "");
    }
    buf[k] = 0;
    float* c = (float*)(s + 0x10B0);
    Log("%s world=0x%02X cell=%d state=%u loaded(%llu)=[%s] centre=%.0f %.0f %.0f req world=0x%02X start=%u", tag, *(uint8_t*)sub, *(int32_t*)(sub + 4),
        *(uint8_t*)(sub + 0x389), n, buf, c[0], c[1], c[2], *(uint8_t*)(B + Grva(0x45BC428)), *(uint32_t*)(B + Grva(0x45BC42C)));
}
// native Player::SetPos (VM 23AB4DC) tail for the non-riding case
// bridge 6514: the game's own loading screen widget (script UI::LoadStart / UI::LoadEnd: [45C84A8]+748h) = native fade to black + loading emblem.
static __attribute__((noinline)) int LoadScreen(int on) {
    __try { ((void (*)(void))(B + (on ? Grva(0x6372A0) : Grva(0x3B1B6C))))(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("LOADSCREEN %d: exception %08lX", on, GetExceptionCode()); return 0; }
    Log("LOADSCREEN %s called", on ? "start" : "end"); return 1;
}
static int Warp(float x, float y, float z, float yaw, int full) {
    uint64_t pl = ((uint64_t (*)(int))(B + Grva(0x20A914)))(0);
    if (!pl) return 0;
    if (*(int32_t*)(pl + 0xAB84) != -1 && ((uint8_t (*)(void))(B + Grva(0x20CF24)))()) return -1;  // SetPos takes its attached/riding branch then
    __attribute__((aligned(16))) float p[4] = {x, y, z, 1.0f};
    ((void (*)(uint64_t, const float*, float))(B + Grva(0x1C27010)))(pl, p, yaw);
    uint64_t comp = *(uint64_t*)(pl + 0x3B0), ph = *(uint64_t*)(comp + 0x28); if (!ph) ph = *(uint64_t*)(comp + 0x18);
    ((void (*)(uint64_t))(*(uint64_t*)(*(uint64_t*)ph + 0xD0)))(ph);
    if (!full) return 1;
    uint64_t cm = *(uint64_t*)(B + Grva(0x45B9E40)), c0 = *(uint64_t*)cm, am = *(uint64_t*)(c0 + 0x48);
    memcpy((void*)(am + 0xD0), (void*)(pl + 0x110), 16);
    ((void (*)(void))(B + Grva(0xA429B4)))();
    ((void (*)(uint64_t))(B + Grva(0x1C2A3CC)))(pl + 0x110);
    uint64_t s = StageMgr();
    memcpy((void*)(s + 0x10B0), (void*)(pl + 0x110), 16); *(uint8_t*)(s + 0x10C0) = 1;
    ((void (*)(uint64_t, uint64_t))(B + Grva(0x3A083C)))(s + 0x500, s + 0x10B0);  // bridge 6047: rdx = centre pointer, as the native SetPos tail (23AB6C1) and 39E58D pass it
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
    ((void (*)(uint32_t, uint32_t))(B + Grva(0x29057EC)))(0, h); g_bgm_on = 1; g_bgm_stop_at = 0;
    uint64_t S = ((uint64_t (*)(void))(B + Grva(0x5A16F8)))(); Log("BGM PlayBossBGM(0x%08X) called, state %p +B4=0x%08X", h, (void*)S, S ? *(uint32_t*)(S + 0xB4) : 0);
}
static void BgmEnd(void) {  // boss died: native end state now, plain stop after the victory sting
    if (!g_bgm_on) return;
    ((void (*)(void))(B + Grva(0x2906098)))(); g_bgm_stop_at = GetTickCount() + 6000; Log("BGM RequestBossBattleBgmEndState called, StopBossBGM in 6 s");
}
static void BgmStop(const char* why) {
    if (!g_bgm_on) return;
    ((void (*)(void))(B + Grva(0x59FFDC)))(); g_bgm_on = 0; g_bgm_stop_at = 0; Log("BGM StopBossBGM (%s): field music is back", why);
}
static void BgmTick(void) { if (g_bgm_on && g_bgm_stop_at && (int)(GetTickCount() - g_bgm_stop_at) >= 0) BgmStop("after boss end"); }
static uint64_t CharMgr(void) { uint64_t cm = 0; Rd64(B + Grva(0x45B9E40), &cm); return cm; }
static uint64_t FindWrapper(uint32_t id) { uint64_t cm = CharMgr(); return cm ? ((uint64_t (*)(uint64_t, uint32_t))(B + Grva(0x135B78)))(cm, id) : 0; }
static struct { int active; uint32_t id; int v0, diff, await; uint32_t seq; uint64_t actor; DWORD t0, last, since; } g_bm;
static uint64_t ActorByHandle(uint32_t h) { uint64_t cur[1024]; int n = Actors(cur, 1024); for (int i = 0; i < n; ++i) if (*(uint32_t*)(cur[i] + 4) == h) return cur[i]; return 0; }
// live-object death predicate 4892A8 (the tail of 7E11F0 / Duel::IsAliveBoss); a dynamic boss (CreateDynamic handle) has no registered wrapper, so use the actor table
static int ActorInTable(uint64_t x) { uint64_t cur[1024]; int n = Actors(cur, 1024); for (int i = 0; i < n; ++i) if (cur[i] == x) return 1; return 0; }
static int BossAliveSensor(uint32_t id) { uint64_t a;
    if (g_bm.await) { if (!g_bm.actor && g_newSeq != g_bm.seq && g_newH == id) g_bm.actor = g_newActor; if (!g_bm.actor) return -1; a = ActorInTable(g_bm.actor) ? g_bm.actor : 0; if (!a) return -1; }
    else a = ActorByHandle(id); if (!a) { uint64_t w = FindWrapper(id); return w ? (int)(((uint8_t (*)(uint64_t))(B + Grva(0x7E11F0)))(w) & 1) : -1; } return (int)(((uint8_t (*)(uint64_t))(B + Grva(0x4892A8)))(a) & 1); }
static int BossModeBegin(uint32_t id, int notify) {
    uint64_t cm = CharMgr(), c0 = 0; if (!cm || !Rd64(cm, &c0) || !c0) { Log("BBM: no character manager"); return 0; }
    *(uint8_t*)(c0 + 0x26AA) = 1;
    uint64_t w = FindWrapper(id);
    if (!w) { Log("BBM %u: no registered wrapper (dynamic boss): HUD/mode steps only, no listener message", id); notify = 0; }
    if (!((uint8_t (*)(uint64_t))(B + Grva(0x1341F8)))(cm)) {
        ((void (*)(uint64_t))(B + Grva(0x1C19E14)))(cm);
        int keep = w && *(uint32_t*)(w + 0x28) == 0x4ACDF && ((uint8_t (*)(uint64_t, uint32_t))(B + Grva(0x374184)))(cm, 0xC12);
        if (!keep) { *(uint8_t*)(c0 + 0x3036) = 0; *(uint8_t*)(c0 + 0x3037) = 0; }
        ((void (*)(uint32_t))(B + Grva(0x1C20600)))(0);
        if (((uint8_t (*)(void))(B + Grva(0x85948)))() && *(uint8_t*)(c0 + 0x305A) == 0) ((void (*)(uint8_t))(B + Grva(0x1CA885C)))(1);
    } else Log("BBM %u: 1341F8 true (script lock set), HUD/mode steps skipped as the native does", id);
    if (notify && w) {  // listener message {wrapper+0x20}, as at 23A2EBD..F3B
        uint32_t v = *(uint32_t*)(w + 0x20); uint64_t out[2] = {0, 0};
        ((void (*)(uint64_t*, uint32_t*))(B + Grva(0xE33410)))(out, &v);
        uint64_t tmp[2] = {out[0], out[1]}; out[0] = out[1] = 0;
        ((void (*)(uint64_t, uint64_t*))(B + Grva(0x66BBF4)))(w, tmp);
        uint64_t obj = tmp[1];
        if (obj && InterlockedDecrement((volatile LONG*)(obj + 8)) == 0) {
            void** vt = *(void***)obj; ((void (*)(uint64_t))vt[0])(obj);
            if (InterlockedDecrement((volatile LONG*)(obj + 0xC)) == 0) ((void (*)(uint64_t))vt[1])(obj);
        }
    }
    g_bm.seq = g_newSeq; g_bm.actor = 0; g_bm.await = g_watch && g_watch_id == id;
    g_bm.active = 1; g_bm.id = id; g_bm.t0 = g_bm.last = GetTickCount(); g_bm.v0 = BossAliveSensor(id); g_bm.diff = 0; g_bm.since = 0;
    Log("BBM %u: BossBattleMode applied (wrapper %p, +26AA=1, alive sensor %d, notify %d)", id, (void*)w, g_bm.v0, notify);
    return 1;
}
static void SummonFinish(uint32_t h, uint32_t bgm) { if (g_cfg_bossmode) BossModeBegin(h, g_cfg_notify); if (g_cfg_bgm && bgm) BgmStart(bgm); Log("SUMMON done: handle 0x%X", h); }
static void BossModeEnd(const char* why) {
    IhStop(why, 0);
    uint64_t cm = CharMgr(); if (!cm) return;
    if (((uint8_t (*)(uint64_t))(B + Grva(0x1C1AE20)))(cm)) ((void (*)(uint64_t))(B + Grva(0x1C0CFB4)))(cm);
    ((void (*)(uint64_t))(B + Grva(0x1C0CEEC)))(cm); ((void (*)(uint64_t))(B + Grva(0x1C0D068)))(cm);
    ((void (*)(uint64_t))(B + Grva(0x1C0E83C)))(cm); ((void (*)(uint64_t))(B + Grva(0x9DECF8)))(cm);
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
static __attribute__((noinline)) int ActorHp(uint64_t a, uint64_t* cur, uint64_t* max) {  // Character::GetHpRate parts: [[actor+0x3A0]+0x58] / [+0x50]
    uint64_t h = 0; *cur = *max = 0; if (!a) return 0;
    __try { if (!Rd64(a + 0x3A0, &h) || !h) return 0; return Rd64(h + 0x58, cur) && Rd64(h + 0x50, max); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static __attribute__((noinline)) void BossTick(void) {
    if (!g_bm.active) { g_db_act = 0; g_fin_live = 0; return; }
    DWORD now = GetTickCount();
    {   // #7234: every pump frame (before any hit of this frame): is the boss at HP <= 1 and in the finisher/down state (death sensor 4892A8 = 1)? h_cond lets that hit kill.
        uint64_t fa = 0;
        if (g_cfg_deathblow && (g_cfg_cutscene || g_cfg_loot) && !g_db_done && g_db_act) {
            uint64_t c = 9, m = 0; int dd = 0;
            if (ActorHp(g_db_act, &c, &m) && m && c <= 1 && ActorDeadNow(g_db_act, &dd, &c, &m) == 0) { __try { if (((uint8_t (*)(uint64_t))(B + Grva(0x4892A8)))(g_db_act) & 1) fa = g_db_act; } __except (EXCEPTION_EXECUTE_HANDLER) { fa = 0; } }
        }
        static int finLogs; if (fa != g_fin_live) { if (fa && ++finLogs <= 20) Log("DEATHBLOW finisher state: boss %p at HP 1 in the finisher/down state -> its next lethal hit kills (CutsceneOnKill)", (void*)fa); g_fin_live = fa; }
    }
    // #6774: DeathBlowRequired=0 - the HP-floor kill is checked every pump frame (not only on the 250 ms tick), so the boss dies on the frame its HP hits
    // the floor like a native kill. g_db_act is only a hint (validated by ActorByHandle on the full tick below).
    int floorNow = 0; if (!g_cfg_deathblow && !g_cfg_nofin && !g_db_done && g_db_act && g_bm.v0 >= 0) { uint64_t c = 9, m = 0; floorNow = ActorHp(g_db_act, &c, &m) && m && c <= 1; }
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
        if (!g_db_done && !g_cfg_nofin && act && ActorHp(act, &fc, &fm) && fm && fc <= 1) {  // #7467: with KiBreakNoFinisher NfTick does the HP-floor kill
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
        static DWORD killAt; static int reported, retries;
        if (sess != killSess) { killSess = sess; killAt = 0; reported = 0; retries = 0; }
        if (g_db_done && !killAt) killAt = now;
        if (g_db_done && !reported && killAt && now - killAt >= 1500 && act && ActorHp(act, &fc, &fm) && fm && fc == 1) {
            reported = 1; Log("KILL boss still at HP 1 %d ms after the native kill (death-blow hook %s)", (int)(now - killAt), o_cond ? "on" : "OFF");
        }
        // #7234: the kill must take like a mission kill. Dead (hp+48 == 1 or HP 0) after our kill / the finisher-kill skip -> boss mode end, bar off, body fade NOW
        // (not after the 4.5 s 'HP 0 stable' fallback + BodyFadeDelayMs). Still at HP 1 -> force kill again every 500 ms (6 tries; from the 2nd the EB2/894 gates are cleared).
        if (g_db_done && act && g_bm.active) {
            uint64_t kc = 1, km = 0; int kd = 0;
            if (ActorDeadNow(act, &kd, &kc, &km)) { BossKilledNow(act, "kill applied"); if (!g_bm.active) { g_db_act = 0; g_fin_live = 0; return; } }
            else if (killAt && retries < 6 && now - killAt >= 500u * (DWORD)(retries + 1) && ActorHp(act, &kc, &km) && km && kc <= 1) {
                retries++; Log("KILL retry %d: boss %u still at HP %llu / %llu %lu ms after the kill", retries, g_bm.id, kc, km, now - killAt);
                ForceKillActorEx(act, g_cfg_loot, "kill retry", retries >= 2); if (!g_bm.active) { g_db_act = 0; g_fin_live = 0; return; }
            }
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
            BgmEnd(); BossModeEnd("boss dead (HP 0)"); FakeGaugeHide("boss mode end"); g_bm.active = 0;
            KillFadeNow(ActorByHandle(g_dead_id));  // #7234: fade the body now, as after a mission kill (not BodyFadeDelayMs later)
            return;
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
    uint64_t mm = 0; Rd64(B + Grva(0x47514D0), &mm);
    ((void (*)(uint64_t, uint32_t, uint8_t, uint8_t))(B + Grva(0x70275C)))(mm, id, val, 0);
    Log("FLAG SetMissionFlag(%u, %u) called (mgr %p)", id, val, (void*)mm);
}
// ---- gadget door/fog control (bridge 4718). Gadget::SetDoorControl{Normal=0,AbsoluteClose=3,AbsoluteOpen=4,...} (VM wrappers 60C7E4 / 60A45C / A52C0C, same template): build a message {vtbl 38F5468, placement id +8, mode +0x10}
// with 60C924(out, &id, &mode) -> shared pair, post it to the placement registry [c0+0x48] with 60BE48(reg, &pair), then release the pair. The registry routes it to the gadget with that placement id (no live actor wrapper needed on our side).
static int DoorCtl(uint32_t id, uint32_t mode) {
    uint64_t cm = CharMgr(), c0 = 0; if (!cm || !Rd64(cm, &c0) || !c0) { Log("DOOR: no character manager"); return 0; }
    uint64_t reg = 0; Rd64(c0 + 0x48, &reg); if (!reg) { Log("DOOR: no registry"); return 0; }
    uint64_t out[2] = {0, 0}; uint32_t m = mode;
    uint64_t* r = ((uint64_t* (*)(uint64_t*, uint32_t*, uint32_t*))(B + Grva(0x60C924)))(out, &id, &m);
    uint64_t tmp[2] = {r[0], r[1]}; r[0] = r[1] = 0;
    ((void (*)(uint64_t, uint64_t*))(B + Grva(0x60BE48)))(reg, tmp);
    uint64_t obj = tmp[1];
    if (obj && InterlockedDecrement((volatile LONG*)(obj + 8)) == 0) {
        void** vt = *(void***)obj; ((void (*)(uint64_t))vt[0])(obj);
        if (InterlockedDecrement((volatile LONG*)(obj + 0xC)) == 0) ((void (*)(uint64_t))vt[1])(obj);
    }
    Log("DOOR SetDoorControl(%u, mode %u) posted to registry %p", id, mode, (void*)reg);
    return 1;
}
static struct { int active, spawned, released, cross, xHeld, lsend; uint32_t chr; float p[4], yaw, b[4], byaw; DWORD t0, tReady, tSpawn, lastLog, tOwn, lastAct; int lastState, lastCell, nact, actIdx; } g_ar;
// ally #618/pc 05:48: cross-world swaps spawned the boss and released the player while the arena cells were still streaming and the game's own load cover
// (GAMESTATE[2]) was up: ~11 s live fight on a black scene (Shingen, Ally). [Fade] AreaWaitCrossMs (default 15000, 0 = off): for CROSS-world arenas only, hold the
// player at the arena and the boss unspawned until no cell is blocking and GAMESTATE[2] is 0, at most this long after 'ready'; the fade-in then waits for the
// same condition within the same window. Same-world swaps are unchanged ([Fade] AreaWaitMs).
static int g_cfg_fade_areawait_x = 15000, g_xw_busy, g_cfg_coverrel = -1;  // [ArenaSwap] CoverReleaseMs (pc2 #8619, default -1 = off): cross-world arena only, N ms after 'ready' clear GAMESTATE[2] (the game's load/control cover) by hand
static int AreaObjPending(int* blocking, int* pending); static const uint8_t* GameState(void); static void AreaObjDetail(const char* why);
// user #9133: with CoverReleaseMs 0 the player had control 0.5 s after the click but the screen stayed black ~30-40 s (pc 19:54/19:57: black ends exactly when
// the mission sequencer leaves its load states, seq 15 -> 16/17, 19:55:30.2 / 19:57:47.2), and the boss (spawned at +0.3 s) attacked in the black.
// [ArenaSwap] BossGraceAfterCoverMs (default 1500, -1 = off): every arena-swap boss spawns with its AI off (668D74) and gets it back (66982C) only
// BossGraceAfterCoverMs after the screen is really visible = GAMESTATE[2] 0, mission seq state not 1..15 (the native load cover) and our fade overlay clear.
// BossHoldMaxMs (default 60000) caps the hold.
static int g_cfg_bossgrace = 1500, g_cfg_bosshold_max = 60000;
// #9182 (notes/black-source-9182.md): the black after a cross-world Fight now is the game's own loading-screen widget, closed only when the mission
// sequencer reaches state 0xB (223BE08 -> UiLoadingScreenEnd 3B1B6C, void(void)). [ArenaSwap] LoadingScreenEndMs (default -1 = off): cross-world arena
// only, N ms after 'ready' call UiLoadingScreenEnd once per swap on the game thread (GAMESTATE[2] is left alone; CoverReleaseMs is independent).
static int g_cfg_lsend = -1;
static void FadeWant(int on, const char* why);
static void BhStart(uint32_t handle);
// pc 2026-10-16 (fnowvis3): on pc the arena 'ready' waited 9.3 s for sequencer state 5 (1912 open-world placeable loads); with
// CrossWorldAreaObjSkipMs > 0 the game lifts its load cover while placeables still stream, so the hold/fade only wait for GAMESTATE[2] = 0.
static int g_cfg_xaoskip;
// user #8236/#8248 (pc2: Shingen arena cells 193960/439542/633740 are only requested at stage state 4->5, the old world is waited on in sequencer state 4):
// [ArenaSwap] CrossWorldForceCells = 1: (a) sequencer state 4's StageAreasObjectsReady for the OLD world passes at once, (b) the stream centre is put on the
// arena point at the click and again when the stage holds the target world, (c) the arena cells are admitted then directly (3A0534 with defer = 1, the
// pending-list bypass) instead of waiting for state 5. 0 (default) = off.
static int g_cfg_xforce;
static int XArenaClear(int* b, int* p) { const uint8_t* gs = GameState(); *b = *p = 0; AreaObjPending(b, p); return (g_cfg_xaoskip > 0 || !*b) && !(gs && gs[2]); }
static int OwnerActivateOne(void) {  // next registered placement with an enemy record and no live actor -> ActivatePlacement; 1 = one was tried
    uint64_t cm = 0, c0 = 0; Rd64(B + Grva(0x45B9E40), &cm); if (!cm) return 0; Rd64(cm, &c0); if (!c0) return 0;
    uint64_t* reg = *(uint64_t**)(c0 + 0x48); uint64_t arr = reg[0], cnt = reg[1];
    for (; (uint64_t)g_ar.actIdx < cnt; ++g_ar.actIdx) {
        uint64_t i = g_ar.actIdx; uint32_t id = *(uint32_t*)(arr + i * 16); uint64_t w = *(uint64_t*)(arr + i * 16 + 8), live = 0, rec = 0;
        if (id <= 1 || id >= 0x10000000 || !w) continue;
        Rd64(w, &live); Rd64(w + 0x18, &rec);
        if (live || !rec) continue;
        ++g_ar.actIdx; ++g_ar.nact;
        uint8_t r = ((uint8_t (*)(uint32_t, uint32_t, uint8_t, uint32_t))(B + Grva(0x1180740)))(id, 0, 0, 0);
        Log("ARENA owner-activate #%d: ActivatePlacement(%u) -> %u", g_ar.nact, id, r); return 1;
    }
    return 0;
}
static void ArenaTick(void) {
    if (!g_ar.active) return;
    DWORD now = GetTickCount(), dt = now - g_ar.t0;
    uint64_t s = StageMgr(); int st = StageState(), cell = s ? *(int32_t*)(s + 0x504) : -2;
    if (st != g_ar.lastState || cell != g_ar.lastCell) { Log("ARENA +%lu ms stage state %d -> %d, cell %d -> %d", dt, g_ar.lastState, st, g_ar.lastCell, cell); g_ar.lastState = st; g_ar.lastCell = cell; }
    if (g_cfg_lsend >= 0 && g_ar.cross && g_ar.tReady && !g_ar.lsend && now - g_ar.tReady >= (DWORD)g_cfg_lsend) {
        g_ar.lsend = 1;  // once per swap (g_ar is zeroed per swap); only while this cross-world swap is active
        ((void (*)(void))(B + Grva(0x3B1B6C)))();
        // notes/loadscreen-state-study.md: the widget closes but the game's black fader [4758BA0] stays faded out until seq 0x12 (223ACF5
        // BlackFadeIn(0, 30.0f, 0.0f)). Issue the same fade-in now and drop our own overlay; seq 0x12 re-issues both later (no-ops).
        ((void (*)(uint32_t, float, float))(B + Grva(0x345508)))(0, 30.0f, 0.0f);
        FadeWant(0, "lsend");
        Log("LSEND +%lu ms: UiLoadingScreenEnd + BlackFadeIn(0, 30, 0) + overlay off, %lu ms after ready (LoadingScreenEndMs %d)", dt, now - g_ar.tReady, g_cfg_lsend);
    }
    float pos[4] = {0}; PlayerPos(pos);
    if (!g_ar.released) {
        Warp(g_ar.p[0], g_ar.p[1], g_ar.p[2], g_ar.yaw, 0);  // hold until the cells around the arena are in
        uint64_t e = ResEntry(g_ar.chr); int res = e && *(uint8_t*)(e + 0x2c8);
        if (st == 5 && res && dt >= 300) {
            if (!g_ar.tReady) { g_ar.tReady = now; Log("ARENA +%lu ms ready: stage state 5 and boss resources loaded", dt); StageLog("ARENA stage"); }
            if (!g_ar.spawned && g_ar.cross && g_cfg_fade_areawait_x > 0) {  // cross-world: boss + control wait for the arena cells and the game's load cover
                int cb, cp; const uint8_t* gs = GameState();
                if (g_cfg_coverrel >= 0 && gs && gs[2] && now - g_ar.tReady >= (DWORD)g_cfg_coverrel) { ((volatile uint8_t*)gs)[2] = 0; static int once; if (once++ < 3) Log("COVER +%lu ms: GAMESTATE[2] cleared by hand %lu ms after ready (CoverReleaseMs %d)", dt, now - g_ar.tReady, g_cfg_coverrel); }
                int clear = XArenaClear(&cb, &cp);
                if (!clear && now - g_ar.tReady < (DWORD)g_cfg_fade_areawait_x) {
                    { static DWORD tDet; if (!g_ar.xHeld || now - tDet >= 1000) { tDet = now; AreaObjDetail("ARENA hold"); } }
                    if (!g_ar.xHeld) { g_ar.xHeld = 1; Log("ARENA +%lu ms cross-world: holding boss spawn + control until the arena cells are in (%d cells blocking, %d placeable loads, GAMESTATE[2] %u; AreaWaitCrossMs %d)", dt, cb, cp, gs ? gs[2] : 0xFF, g_cfg_fade_areawait_x); }
                    goto owner_wait;
                }
                if (g_ar.xHeld == 1) { g_ar.xHeld = 2; Log("ARENA +%lu ms cross-world hold over after %lu ms: %s (%d cells blocking, %d placeable loads, GAMESTATE[2] %u)", dt, now - g_ar.tReady, clear ? "arena cells in" : "TIMEOUT", cb, cp, gs ? gs[2] : 0xFF); }
                else if (!g_ar.xHeld) { g_ar.xHeld = 2; Log("ARENA +%lu ms cross-world: arena already clear at ready, no hold", dt); }
            }
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
                g_watch_chr = g_ar.chr; g_watch_slot = -1; g_watch_session = r && g_tr.active;  // ally #89: arena boss = Boss A (WatchTick); g_watch_chr was stale here
                Log("ARENA +%lu ms CreateDynamic(%u owner %u) at %.0f %.0f %.0f yaw %.1f -> 0x%X", dt, g_ar.chr, owner, b[0], b[1], b[2], g_ar.byaw, r);
                if (r && g_autogauge) FakeGaugeRequest(r, owner);
                if (r && g_cfg_bossmode) BossModeBegin(r, g_cfg_notify);
                if (r && g_cfg_bgm && g_ar_bgm) BgmStart(g_ar_bgm);
                if (r) BhStart(r);  // #9133 boss AI held until the screen is visible + BossGraceAfterCoverMs
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
    g_ar.t0 = GetTickCount(); g_ar.lastState = -9; g_ar.lastCell = -9; g_ar.cross = g_xw_busy;
    StageLog("ARENA before");
    uint64_t e = ResEntry(chr);
    if (!e || !*(uint8_t*)(e + 0x2c8)) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x3F9E58)))(chr, 2, 0); PoolAdd(chr); }
    int r = Warp(g_ar.p[0], g_ar.p[1], g_ar.p[2], g_ar.yaw, 1);
    Log("ARENA start chr %u player -> %.0f %.0f %.0f yaw %.1f boss -> %.0f %.0f %.0f: warp %d (+%lu ms), resources %s", chr, g_ar.p[0], g_ar.p[1], g_ar.p[2], g_ar.yaw, g_ar.b[0], g_ar.b[1], g_ar.b[2], r, GetTickCount() - g_ar.t0,
        e && *(uint8_t*)(e + 0x2c8) ? "preloaded" : "loading");
    g_ar.active = r == 1;
    return r == 1;
}

// ---- arena table (Nioh3Training_arenas.tsv, tools/gen_arena_table.py): mission, start, world, char, placement, player x y z yaw, boss x y z, kind, id, name
typedef struct { uint32_t mission, start, chr, placement, bgm; uint8_t world; float p[4], b[3]; char kind[8], id[16], name[64]; uint32_t cells[32]; int ncells; } ArenaRow;  // cells: optional 16th tsv column (comma list) = CrossWorldCellSkip whitelist
// #23451: the table is generated from the game data by tools/auto_arena.py (every boss incl. DLC: one R/D row per mission, then S<chr> rows for every
// further boss character, kind 'field' = an open-field placement with no arena trigger). MAXROWS leaves room for future DLC.
#define MAXROWS 512
static ArenaRow g_rows[MAXROWS]; static int g_nrows;
// user #22984: kind 'summon' rows (tools/gen_summon_rows.py) = summon-only bosses with no arena data, kept apart from g_rows so the Fight now /
// world / preload scans never see them. mission != 0: a catalogue row (menu token R<mission>); mission 0: the per-boss roster (id S<chr>).
static struct { uint32_t mission, chr; char id[16], name[64]; } g_sum[MAXROWS]; static int g_nsum, g_nroster;
// #23436/#23447/#23451/#23479: rows derived in-process from the game's own data files (arena_scan.h, the C port of tools/auto_arena.py, no game
// addresses). [ArenaSwap] AutoArena = 1 (default): after the TSV (= cache + override) every scanned row whose mission (R/D rows) or boss character
// (S rows) the TSV does not have is added, so a game patch / DLC boss gets its arena with no table update; new characters also join the summon
// roster (named 'Boss <chr>' until the TSV names them). 0 = TSV only.
static int g_cfg_autoarena = 1;
#define AS_LOG(...) Log(__VA_ARGS__)
#include "arena_scan.h"
static void AsAddRow(const AsRow* s, void* ctx) {
    int* added = (int*)ctx;
    for (int i = 0; i < g_nrows; ++i) {
        if (s->id[0] != 'S' && g_rows[i].id[0] != 'S' && g_rows[i].mission == s->mission) return;  // the TSV has this mission
        if (g_rows[i].chr == s->chr && (s->id[0] == 'S' || !_stricmp(g_rows[i].id, s->id))) return;  // or this boss character
    }
    if (g_nrows >= MAXROWS) return;
    ArenaRow* r = &g_rows[g_nrows++]; memset(r, 0, sizeof *r);
    r->mission = s->mission; r->start = s->start; r->world = s->world; r->chr = s->chr; r->placement = s->placement;
    for (int i = 0; i < 4; ++i) r->p[i] = (float)s->p[i];
    for (int i = 0; i < 3; ++i) r->b[i] = (float)s->b[i];
    snprintf(r->kind, sizeof r->kind, "%s", s->kind); snprintf(r->id, sizeof r->id, "%s", s->id);
    const char* nm = 0; for (int i = 0; i < g_nsum && !nm; ++i) if (g_sum[i].chr == s->chr) nm = g_sum[i].name;
    for (int i = 0; i < g_nrows - 1 && !nm; ++i) if (g_rows[i].chr == s->chr) nm = g_rows[i].name;
    if (nm) snprintf(r->name, sizeof r->name, "%s", nm); else snprintf(r->name, sizeof r->name, "Boss %u", s->chr);
    int inRoster = 0; for (int i = 0; i < g_nsum; ++i) if (g_sum[i].chr == s->chr) inRoster = 1;
    if (r->id[0] == 'S' && !inRoster && g_nsum < MAXROWS) {
        g_sum[g_nsum].mission = 0; g_sum[g_nsum].chr = s->chr; snprintf(g_sum[g_nsum].id, sizeof g_sum[0].id, "%s", r->id); snprintf(g_sum[g_nsum].name, sizeof g_sum[0].name, "%s", r->name); ++g_nsum; ++g_nroster;
    }
    Log("ARENASCAN: added %s %s mission 0x%X world 0x%02X chr %u (%s), not in the TSV", r->kind, r->id, r->mission, r->world, r->chr, r->name);
    ++*added;
}
static void LoadTable(void) {
    char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\Nioh3Training_arenas.tsv", g_dir);
    FILE* f = fopen(path, "r"); char line[512];
    if (!f) Log("arena table %s not found", path);
    while (f && fgets(line, sizeof line, f) && g_nrows < MAXROWS) {
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
        if (!strcmp(r->kind, "summon")) {
            if (r->chr && g_nsum < MAXROWS) { g_sum[g_nsum].mission = r->mission; g_sum[g_nsum].chr = r->chr; snprintf(g_sum[g_nsum].id, sizeof g_sum[0].id, "%s", r->id); snprintf(g_sum[g_nsum].name, sizeof g_sum[0].name, "%s", r->name); g_nroster += !r->mission; ++g_nsum; }
            continue;
        }
        if (r->mission && r->chr) ++g_nrows;
    }
    if (f) fclose(f);
    { char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir); g_cfg_autoarena = GetPrivateProfileIntA("ArenaSwap", "AutoArena", 1, ini); }
    if (g_cfg_autoarena) {
        DWORD t0 = GetTickCount(); int added = 0, n = AsScan(g_dir, AsAddRow, &added);
        Log("ARENASCAN (#23451, [ArenaSwap] AutoArena 1): %d rows from the game data, %d not in the TSV added (%lu ms)", n, added, GetTickCount() - t0);
    }
    if (!g_nrows) { Log("arena table empty: Fight now keeps the native load"); return; }

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
    Log("arena table: %d bosses, %d with a boss BGM id; %d summon-only rows (%d roster)", g_nrows, nb, g_nsum, g_nroster);
}
// #23451: S<chr> rows (a further boss of a mission, or an open-field boss) share their mission id with the mission's own row (or are a region
// mission no add-on ever requests), so a mission lookup only sees R/D rows; an S row is reached by its character through Nioh3Training_FightNowChr,
// which pins it in g_pick for the Fight now that follows (FindRow then returns it for its mission until the next external Fight now).
static ArenaRow* g_pick;
static int FightNowEntry(uint32_t mission);
static ArenaRow* FindRow(uint32_t mission) {
    if (g_pick && g_pick->mission == mission) return g_pick;
    for (int i = 0; i < g_nrows; ++i) if (g_rows[i].mission == mission && g_rows[i].id[0] != 'S') return &g_rows[i];
    return 0;
}
static ArenaRow* FindRowChr(uint32_t chr) {  // the boss's own mission row first, else its S<chr> row
    ArenaRow* s = 0;
    for (int i = 0; chr && i < g_nrows; ++i) if (g_rows[i].chr == chr) { if (g_rows[i].id[0] != 'S') return &g_rows[i]; if (!s) s = &g_rows[i]; }
    return s;
}
// user #22984: summon token of a catalogue mission: the arena row's id, else a summon-only row's id (the menu built 'R%X', which missed
// zero-padded ids like R04DF and every D-route row). 0 = this mission cannot be summoned.
static const char* SummonTok(uint32_t mission) {
    ArenaRow* r = mission ? FindRow(mission) : 0; if (r) return r->id;
    for (int i = 0; mission && i < g_nsum; ++i) if (g_sum[i].mission == mission) return g_sum[i].id;
    return 0;
}
static int CurWorld(void) { uint64_t s = StageMgr(); return s ? *(uint8_t*)(s + 0x500) : -1; }
static const uint8_t* GameState(void) { uint64_t gm = 0; Rd64(B + Grva(0x4759910), &gm); return gm ? (const uint8_t*)(gm + 0x588) : 0; }  // [2] loading, [3]/[4] paused

// ---- bridge 4942: no-load training session. 'train <R-id|charId> [dist]' summons the boss next to the player in whatever map is loaded (any world: only the
// boss's character resources are loaded, no mission/stage load), keeps the player alive (native Character::SetNoDeath replica on the player's wrapper), and resets the round
// without a load: player beaten (HP at the floor) or 'retry' -> despawn the boss, Player::RecoverFull replica, summon the same boss again (resources stay loaded = instant).
// Boss killed -> victory end state; RespawnOnWinMs > 0 respawns it after that many ms. 'trainstop' ends the session (NoDeath off, boss removed).
// user #14147 (died in a Practice arena with [Summon] PlayerNoDeath 0 in the installed ini: HP 0 = the game's own death -> shrine load, the session
// retry came too late). [Summon] SessionNoDeath 1 (default) = every Training / Boss Match session (g_tr: Practice, Fight now arena, 'train')
// keeps player NoDeath on whatever PlayerNoDeath says, and TrainTick re-applies it when it is off or the player character changed (load);
// a defeat is then always the in-place round reset. 0 = PlayerNoDeath alone decides (old behaviour). Menu summons stay on SummonNoDeath.
static int g_cfg_session_nodeath = 1; static uint64_t g_nd_char; static DWORD g_nd_try;
// user #14150 'Player can't die' ([Summon] PlayerCantDie, default 1, menu toggle Nioh3Training_SetPlayerCantDie): 1 = in a session the player's HP at
// the floor is healed in place (fight goes on); 0 = HP at the floor counts as a death: instant in-arena respawn (the existing round reset: full HP,
// round-1 spot, boss respawned, stance / repeat / aggro kept). Either way native NoDeath stays on (SessionNoDeath), so the game's own death
// flow (game over -> shrine load) never runs inside Training / Boss Match.
static int g_cfg_cantdie = 1; static DWORD g_cd_t; static int g_cfg_beat_ms = 0, g_ph_wfail; static volatile LONG g_ph_logs;  // #14267: [Summon] BeatenRespawnMs (0..5000, was a fixed 2000)
static int g_cfg_tr_dist = 300, g_cfg_tr_win = 0, g_cfg_tr_nodeath = 1, g_cfg_fn_summon = 1, g_cfg_fn_arena = 1, g_cfg_body_fade = 1, g_cfg_body_delay = 25000;
// #6774: native kill = the boss death animation plays out in full. The kill-time fade (DeathBlowRequired=0) was f30 (0.5 s) and hid the death animation:
// KillFadeFrames (default 120 = 2 s at 60 fps) still starts at the kill but lets the collapse be seen while the body fades. Delete waits for the fade end.
static int g_cfg_kill_fade_frames = 120;
// user #8827 (play 16:09, KiBreakNoFinisher kill): 'the boss disappeared mid cutscene' - KillFadeNow started the 2 s fade at the kill itself, so the body was
// gone while its death animation was still playing. Every kill path (NOFIN HP-0 window, CutsceneOnKill, BossLoot, DeathBlowRequired=0, HP-0 fallback) now waits
// KillFadeDelayMs (default 5000, capped by BodyFadeDelayMs; 0 = old 'at once') after the kill before the fade starts; delete 6 s after the fade as before.
static int g_cfg_kill_fade_delay = 5000;
// #9543 (user play on 72cfe63c): after the HP-0 window kill the body faded 5 s later (window, then KillFadeDelayMs 5000 stacked on top) - too slow vs the
// mission / the earlier kifade build. A kill issued by the KiBreakNoFinisher HP-0 window path uses [Summon] KiBreakHp0FadeDelayMs (default 1000) instead.
static int g_cfg_nofin_fade = 1000; static volatile uint64_t g_nf_killact; static volatile DWORD g_nf_killt;
static DWORD FadeDeleteMs(void) { DWORD f = (DWORD)(g_cfg_kill_fade_frames > 30 ? g_cfg_kill_fade_frames : 30) * 1000 / 60 + 1000; return f > 6000 ? f : 6000; }
static void KillFadeNow(uint64_t a) {
    // #6746: DeathBlowRequired=0 -> start the body fade at the kill too (not BodyFadeDelayMs after the 4.5 s "HP 0 stable" win detection). Same fade + 6 s delete as below.
    // #7234: every confirmed kill (CutsceneOnKill / finisher / BossLoot / HP-0 fallback too), so the boss always dies + fades like in the mission; the
    // KillFadeFrames (2 s) fade lets the death animation be seen. BodyFadeDelayMs is only the fallback for kills we never saw.
    if (a && g_cfg_body_fade && g_tr.active && !g_tr.tFade && !g_tr.bodyGone && g_nsp > 0 && g_sp_actor[g_nsp - 1] == a) {
        int hp0 = a == g_nf_killact && GetTickCount() - g_nf_killt < 15000;
        int d = hp0 ? g_cfg_nofin_fade : g_cfg_kill_fade_delay; if (d > g_cfg_body_delay) d = g_cfg_body_delay; if (d < 0) d = 0;
        if (d > 0) { if (!g_tr.tFadeAt) { g_tr.tFadeAt = GetTickCount() + (DWORD)d; if (!g_tr.tFadeAt) g_tr.tFadeAt = 1; g_tr.fadeAct = a;
                Log("TRAIN round %d: kill confirmed, body fade in %d ms (death animation plays first; %s, DeathBlowRequired %d CutsceneOnKill %d KiBreakNoFinisher %d)", g_tr.round, d, hp0 ? "HP-0 window kill: KiBreakHp0FadeDelayMs" : "KillFadeDelayMs", g_cfg_deathblow, g_cfg_cutscene, g_cfg_nofin); }
            return; }
        Log("TRAIN round %d: fading out the dead boss body at once (kill confirmed, DeathBlowRequired %d CutsceneOnKill %d)", g_tr.round, g_cfg_deathblow, g_cfg_cutscene);
        if (FadeOutActor(g_nsp - 1, (float)(g_cfg_kill_fade_frames > 0 ? g_cfg_kill_fade_frames : 30))) g_tr.tFade = GetTickCount();
    }
}
static void KillFadeDue(void) {  // TrainTick: start the delayed kill fade (same body only; a retry / new round clears tFadeAt)
    if (!g_tr.tFadeAt || (int)(GetTickCount() - g_tr.tFadeAt) < 0) return;
    g_tr.tFadeAt = 0;
    if (!g_cfg_body_fade || g_tr.tFade || g_tr.bodyGone || g_nsp <= 0 || g_sp_actor[g_nsp - 1] != g_tr.fadeAct) { Log("TRAIN round %d: delayed body fade dropped (body gone / replaced)", g_tr.round); return; }
    Log("TRAIN round %d: fading out the dead boss body (fade delay after the kill elapsed; KillFadeDelayMs %d, KiBreakHp0FadeDelayMs %d)", g_tr.round, g_cfg_kill_fade_delay, g_cfg_nofin_fade);
    if (FadeOutActor(g_nsp - 1, (float)(g_cfg_kill_fade_frames > 0 ? g_cfg_kill_fade_frames : 30))) g_tr.tFade = GetTickCount(); else { g_tr.bodyGone = 1; Despawn(g_nsp - 1); }
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
    ((uint64_t* (*)(uint64_t*, uint8_t*))(B + Grva(0x2386460)))(out, &flag);
    uint64_t tmp[2] = {out[0], out[1]}; out[0] = out[1] = 0;
    ((void (*)(uint64_t, uint64_t*))(B + Grva(0x66BBF4)))(w, tmp);
    uint64_t obj = tmp[1];
    if (obj && InterlockedDecrement((volatile LONG*)(obj + 8)) == 0) {
        void** vt = *(void***)obj; ((void (*)(uint64_t))vt[0])(obj);
        if (InterlockedDecrement((volatile LONG*)(obj + 0xC)) == 0) ((void (*)(uint64_t))vt[1])(obj);
    }
    g_tr.nodeath = g_tr_nodeath_now = on; g_nd_char = on ? PlayerChar() : 0; Log("TRAIN player NoDeath %d (wrapper %p)", on, (void*)w); return 1;
}
static int TrNoDeath(void) { return g_cfg_tr_nodeath || g_cfg_session_nodeath; }  // user #14147
#include "player_hp.h"  // user #14267: Infinite health decision (PhDecide)
static int PlayerHp(uint64_t* cur, uint64_t* max) {  // Character::GetHpRate (323BB4): [[actor+0x3A0]+0x58] / [+0x50]
    uint64_t me = PlayerChar(), h = 0; if (!me || !Rd64(me + 0x3A0, &h) || !h) return 0;
    return Rd64(h + 0x58, cur) && Rd64(h + 0x50, max);
}
static void PlayerRecoverFull(void) {  // Player::RecoverFull (VM 23AAB04)
    uint64_t p = ((uint64_t (*)(uint32_t))(B + Grva(0x20A914)))(0);
    if (p) ((void (*)(uint64_t, uint64_t, uint64_t))(B + Grva(0x54AFBC)))(CharMgr(), 0, 0);
    p = ((uint64_t (*)(uint32_t))(B + Grva(0x20A914)))(0);
    if (p) ((void (*)(uint64_t))(B + Grva(0x204E038)))(p);
    uint64_t c = 0, m = 0; PlayerHp(&c, &m); Log("TRAIN player RecoverFull: HP %llu / %llu", c, m);
}
// user #14267 Infinite health ON: HP below max -> set back to max at once. Direct write of the HP field PlayerHp reads ([[actor+0x3A0]+0x58]);
// if the write does not stick 3 times (the game keeps HP elsewhere), Player::RecoverFull every 250 ms instead (proven: TrainRetry).
static void PlayerRefill(uint64_t c, uint64_t m) {
    uint64_t me = PlayerChar(), h = 0; const DWORD now = GetTickCount();
    if (c == 0) {  // field write does not undo a death the hit hook missed (Yamagata and the same class of hit)
        if (now - g_cd_t >= 250) { g_cd_t = now; if (InterlockedIncrement(&g_ph_logs) <= 50) Log("TRAIN infinite health: player HP 0 / %llu -> RecoverFull", m); PlayerRecoverFull(); }
        return;
    }
    if (g_ph_wfail < 3 && me && Rd64(me + 0x3A0, &h) && h) {
        uint64_t back = 0;
        __try { *(volatile uint64_t*)(h + 0x58) = m; back = *(volatile uint64_t*)(h + 0x58); } __except (EXCEPTION_EXECUTE_HANDLER) { back = 0; }
        if (back == m) { if (InterlockedIncrement(&g_ph_logs) <= 50) Log("TRAIN infinite health: player HP %llu -> %llu / %llu (refilled)", c, m, m); return; }
        ++g_ph_wfail; Log("TRAIN infinite health: HP write did not stick (%llu, try %d of 3)%s", back, g_ph_wfail, g_ph_wfail >= 3 ? ": Player::RecoverFull from now on" : "");
    }
    if (now - g_cd_t >= 250) { g_cd_t = now; if (InterlockedIncrement(&g_ph_logs) <= 50) Log("TRAIN infinite health: player HP %llu / %llu -> RecoverFull", c, m); PlayerRecoverFull(); }
}
// bridge 5158: release of the resource refs the ASI took with 3F9E58(chr,2,0). 1C1DF80(chr,0) = 3F9900(chr,2,0), exact inverse, one call per ref we took.
// Pool of PoolMax characters: when a new boss needs a ref and the pool is full, the oldest chr that is not the current boss is released one tick later (actor already deleted by Despawn).
#define POOLMAX 8
static struct { uint32_t chr; int refs; DWORD t; } g_pool[POOLMAX]; static int g_cfg_pool = 3;
static void PoolAdd(uint32_t chr) {
    for (int i = 0; i < POOLMAX; ++i) if (g_pool[i].chr == chr) { g_pool[i].refs++; g_pool[i].t = GetTickCount(); return; }
    for (int i = 0; i < POOLMAX; ++i) if (!g_pool[i].chr) { g_pool[i].chr = chr; g_pool[i].refs = 1; g_pool[i].t = GetTickCount(); return; }
}
static int PoolInUse(uint32_t chr) { for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i] && g_sp_chr[i] == chr) return 1; return 0; }
static void PoolTick(uint32_t cur) {  // game thread; keep at most g_cfg_pool loaded chars, never release the current boss
    if (g_cfg_pool <= 0) return;
    // user #22984 (live 12:10, 4th summon released Kagetoki's resources while he stood as Boss A and he vanished): a character a tracked
    // spawned actor still uses is never released either; the pool may then hold more than PoolMax until those bosses are removed.
    int n = 0, old = -1; for (int i = 0; i < POOLMAX; ++i) if (g_pool[i].chr) { n++; if (g_pool[i].chr != cur && !PoolInUse(g_pool[i].chr) && (old < 0 || g_pool[i].t < g_pool[old].t)) old = i; }
    if (n <= g_cfg_pool || old < 0 || GetTickCount() - g_pool[old].t < 1500) return;
    uint32_t c = g_pool[old].chr; int r = g_pool[old].refs; g_pool[old].chr = 0; g_pool[old].refs = 0;
    for (int k = 0; k < r; ++k) ((void (*)(uint32_t, uint32_t))(B + Grva(0x1C1DF80)))(c, 0);
    Log("POOL: released char %u (%d ref(s)) via 1C1DF80, pool was %d (max %d)", c, r, n, g_cfg_pool);
}
// boss token (arena row id like R7B35, or a character number) -> character id, 0 = unknown
static uint32_t TokChr(const char* tok, uint32_t* bgm) {
    uint32_t chr = 0, b = 0;
    for (int i = 0; i < g_nrows; ++i) if (!_stricmp(g_rows[i].id, tok)) { chr = g_rows[i].chr; b = g_rows[i].bgm; break; }
    for (int i = 0; !chr && i < g_nsum; ++i) if (!_stricmp(g_sum[i].id, tok)) chr = g_sum[i].chr;
    if (!chr) chr = (uint32_t)strtoul(tok, 0, 0);
    if (bgm) *bgm = b;
    return chr;
}
static int SummonStart(const char* tok, float d) {
    if (g_sp.active) { Log("SUMMON %s: previous spawn %u still loading", tok, g_sp.id); return 0; }
    uint32_t bgm = 0, chr = TokChr(tok, &bgm);
    if (!chr) { Log("SUMMON: unknown boss '%s'", tok); return 0; }
    if (g_bm.active) { IhKeepForSummon(); BossModeEnd("new summon"); }  // #14304: the held session boss stays idle (slot hold), not AI on
    BgmStop("new summon"); g_vf.attempt = 0; g_vf.active = 0; g_vf.respawnAt = 0;
    g_sp.slot = g_sp_slot_next; g_sp_slot_next = -1; g_sp.id = chr; g_sp.dist = d; g_sp.owner = 0; g_sp.boss = 1; g_sp.bgm = bgm; g_sp.tOwn = 0; g_sp.t0 = GetTickCount();
    uint64_t e = ResEntry(chr);
    if (!e || !*(uint8_t*)(e + 0x2c8)) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x3F9E58)))(chr, 2, 0); PoolAdd(chr); }
    Log("SUMMON %s -> char %u dist %.0f world 0x%02X: %s", tok, chr, d, CurWorld(), e && *(uint8_t*)(e + 0x2c8) ? "resources already loaded" : "Load::Character requested");
    g_sp.active = 1; SpawnTick();
    return 1;
}
static void TrainClear(const char* why) {  // remove our boss and its HUD/BGM state
    IhStop(why, 0);
    if (g_bm.active) { BossModeEnd(why); g_bm.active = 0; }
    BgmStop(why); FakeGaugeHide(why);  // the force-hide inside is required: without it the HUD gauge row keeps reading the despawned actor (crash 0xC0000005 at 4893CC, bridge 4942 test)
    for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i]) Despawn(i);
    g_nsp = 0; g_watch = 0; g_mon.a = 0;
}
// bridge 12614 (cx-pc study 10541): one despawn-all transaction for every entry point. Cancels a pending spawn so a late actor cannot appear after it, deletes every tracked actor,
// and resets the 32-slot counter only after the removal pass (individual Despawn never lowers g_nsp, so tracking otherwise stops after 32 cumulative spawns).
static int DespawnAll(const char* why) {
    int removed = 0, gone = 0;
    g_sp.active = 0; g_watch = 0; g_mon.a = 0;
    for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i]) { if (Despawn(i)) removed++; else gone++; }
    g_nsp = 0;
    Log("DESPAWN ALL (%s): %d deleted, %d already gone, slots reset", why, removed, gone);
    return removed;
}
static int TrainStart(const char* tok, float d) {
    if (GameState() && GameState()[2]) { Log("TRAIN %s: a load is running, refused", tok); return 0; }
    TrainClear("train start");
    memset(&g_tr, 0, sizeof g_tr); snprintf(g_tr.tok, sizeof g_tr.tok, "%s", tok); g_tr.tCmd = GetTickCount(); g_tr.round = 1;
    // user #14546 (live 23:39 'IDLEHOLD adopted the load hold: chr 0'): only the arena path set g_tr.chr, so in a 'Training here' session the
    // Idle hold, the drill, MvBossChr and aggro saw boss character 0: no drill, Stand-still picks unfiltered, aggro skipped the held boss.
    g_tr.chr = TokChr(tok, 0);
    g_tr.practice = g_practice_next; g_practice_next = 0;  // user #13223
    if (TrNoDeath()) PlayerSetNoDeath(1);
    g_tr.tRespawn = GetTickCount() + 300;  // the native delete of the previous boss is applied on a later frame: spawn shortly after
    g_tr.active = 1; IhDefaultSession(); BossMatchDefaults(); Log("TRAIN start %s dist %.0f (round 1)", tok, d); g_cfg_tr_dist = (int)d;
    return 1;
}
static int g_cfg_tr_respawn = 2000;
// user #7118 / round-2: [Summon] AutoRetryOnDeath (g_cfg_autoretry, default 0; legacy key InstantRespawn read as fallback). 0 = player beaten -> TrainLost: the boss is
// removed (boss mode / BGM / bar ended), player healed and put back on the round-1 spot, NO respawn: the Return gate appears. 1 = instant round reset (TrainRetry).
static void TrainRetry(const char* why) {
    if (!g_tr.active) { Log("TRAIN retry (%s): no session", why); return; }
    TrainClear(why); PlayerRecoverFull(); if (TrNoDeath() && !g_tr.nodeath) PlayerSetNoDeath(1);
    if (g_tr.hasStart) { int w = Warp(g_tr.pp[0], g_tr.pp[1], g_tr.pp[2], g_tr.pp[3], 1); Log("TRAIN retry: player back to the round-1 position %.0f %.0f %.0f -> %d", g_tr.pp[0], g_tr.pp[1], g_tr.pp[2], w); }
    g_dead_at = 0; g_dead_id = 0;
    g_tr.round++; g_tr.beaten = 0; g_tr.lost = 0; g_tr.tWin = 0; g_tr.bodyGone = 0; g_tr.tFade = 0; g_tr.tFadeAt = 0; g_tr.tBeat = 0; g_tr.tCmd = GetTickCount(); g_tr.tRespawn = GetTickCount() + g_cfg_tr_respawn;  // bridge 6566: 300 ms after the delete the new boss kept ai+1AA8 = -1 (passive, only jumps)
    IhDefault(0);
    Log("TRAIN retry (%s): round %d", why, g_tr.round);
}
static volatile DWORD g_gate_maskUntil; static int g_cfg_gate_mask_hold = 2000;  // #9909 [Return] ReturnGateMaskHoldMs; mask forced on after a lost round's put-back
static void TrainLost(void) {  // AutoRetryOnDeath 0: player beaten -> boss gone, no respawn, wait for the Return gate choice
    g_gate_maskUntil = GetTickCount() + 2500; if (!g_gate_maskUntil) g_gate_maskUntil = 1;  // #9909: the put-back lands next to the gate: no unmasked frames while the position settles
    TrainClear("player beaten"); PlayerRecoverFull(); if (TrNoDeath() && !g_tr.nodeath) PlayerSetNoDeath(1);
    g_sp.active = 0; g_vf.active = 0; g_vf.respawnAt = 0;
    if (g_tr.hasStart) { int w = Warp(g_tr.pp[0], g_tr.pp[1], g_tr.pp[2], g_tr.pp[3], 1); Log("TRAIN lost: player back to the round-1 position %.0f %.0f %.0f -> %d", g_tr.pp[0], g_tr.pp[1], g_tr.pp[2], w); }
    g_dead_at = 0; g_dead_id = 0;
    g_tr.beaten = 0; g_tr.tBeat = 0; g_tr.tRespawn = 0; g_tr.tReport = 0; g_tr.tWin = 0; g_tr.bodyGone = 0; g_tr.tFade = 0; g_tr.tFadeAt = 0; g_tr.lost = 1;
    Log("TRAIN round %d lost (AutoRetryOnDeath 0): boss removed, no respawn; Return gate: Return to where I was / Fight again", g_tr.round);
}
static void TrainStop(const char* why) {
    TrainClear(why); if (g_tr.nodeath || g_tr_nodeath_now) PlayerSetNoDeath(0);  // user #13197: also a summon's NoDeath (g_tr.nodeath is lost on the session memset)
    g_sum_nodeath = 0;
    g_tr.active = 0; BossMatchRestore(why); Log("TRAIN stop (%s)", why);
}
// bridge 5338: Fight now in the boss's own arena (in-place ARENA swap, no load) becomes a retry-able session: round 1 pose = arena player start / boss spawn, so 'player beaten' and 'retry'
// warp back into the arena and respawn the boss there (no open-world summon). NoDeath + round reset as for 'train'.
static void TrainAttachArena(const ArenaRow* r, const float* pp, const float* bp, float face) {
    IhStop("new arena session", 0);
    if (g_tr.active && g_tr.nodeath) PlayerSetNoDeath(0);
    memset(&g_tr, 0, sizeof g_tr); snprintf(g_tr.tok, sizeof g_tr.tok, "%s", r->id); g_tr.tCmd = GetTickCount(); g_tr.round = 1; g_tr.chr = r->chr; g_tr.arena = 1;
    memcpy(g_tr.pp, pp, 4 * sizeof(float)); memcpy(g_tr.bp, bp, 3 * sizeof(float)); g_tr.face = face; g_tr.hasStart = 1;
    g_tr.practice = g_practice_next; g_practice_next = 0;  // user #13223
    if (TrNoDeath()) PlayerSetNoDeath(1);
    g_tr.active = 1; IhDefaultSession(); BossMatchDefaults(); Log("TRAIN arena session %s: round 1 pose player %.0f %.0f %.0f boss %.0f %.0f %.0f (retry stays in the arena)", r->id, pp[0], pp[1], pp[2], bp[0], bp[1], bp[2]);
}
// user #22918 Infinite health. The saved toggle (g_cfg_cantdie) is the gate. Return ends the session
// (g_tr / g_bm / g_ar) but must not stop the heal or the hit cut. Game thread, every pump frame.
static void InfHpTick(void) {
    // Return (TrainStop) clears g_tr / g_ar and the saved toggle was left ON, so the heal must not require a live session.
    if (!g_cfg_cantdie) return;
    HpDmgHookInstall("Infinite health (#22918)");
    uint64_t c = 0, m = 0;
    if (PlayerHp(&c, &m) && PhMatchRefill(g_cfg_cantdie, 1, c, m)) PlayerRefill(c, m);
}
static void TrainTick(void) {
    if (!g_tr.active) return;
    DWORD now = GetTickCount();
    if (g_tr.tRespawn && (int)(now - g_tr.tRespawn) >= 0) { g_tr.tRespawn = 0; g_tr.tReport = 1; SummonStart(g_tr.tok, (float)g_cfg_tr_dist); }
    if (g_tr.tReport && g_nsp > 0 && g_sp_actor[g_nsp - 1]) { Log("TRAIN round %d: boss in the world %lu ms after the command", g_tr.round, now - g_tr.tCmd); g_tr.tReport = 0; }
    if (TrNoDeath() && (!g_tr_nodeath_now || g_nd_char != PlayerChar()) && PlayerChar() && now - g_nd_try >= 1000 && !(GameState() && GameState()[2])) {
        g_nd_try = now; Log("TRAIN NoDeath re-apply (user #14147): %s", !g_tr_nodeath_now ? "was off" : "player character changed"); PlayerSetNoDeath(1);
    }
    uint64_t c = 0, m = 0;
    const int ph = PlayerHp(&c, &m) ? PhDecide(g_cfg_cantdie, TrNoDeath(), g_tr.beaten, g_tr.lost, c, m) : PH_NONE;  // player_hp.h
    if (ph == PH_REFILL) PlayerRefill(c, m);  // user #14267 Infinite health ON: never lose HP
    else if (ph == PH_BEATEN) {
        g_tr.beaten = 1; g_tr.tBeat = now; Log("TRAIN round %d: player beaten (HP %llu / %llu), %s in %d ms", g_tr.round, c, m, g_tr.practice ? "resetting the round in place (Practice session)" : g_cfg_autoretry ? "resetting the round (AutoRetryOnDeath 1)" : "removing the boss for the Return gate (AutoRetryOnDeath 0)", g_cfg_beat_ms);
    }
    // AutoRetryOnDeath 1 = the 'dejavu' instant round reset (player + boss) 2 s after the player is beaten; 0 (default) = TrainLost: boss removed, player healed,
    // NO respawn, the arena Return gate (spawned only while idle, #7138) offers Return to where I was / Fight again (Fight again = the same TrainRetry)
    // #8827 toggle matrix: AutoRetryOnDeath 0 relies on the Return gate for 'Fight again'; with ReturnGate 0 there is no gate, so a defeat resets the round (as 1)
    // user #13223: a Practice (Training) session always retries in place: player revived at the round-1 pose, boss respawned in Idle stance; it never
    // ends on a defeat (only the player's own Return). AutoRetryOnDeath keeps applying to Boss Rematch Fight now sessions.
    // user #14150: the retry keeps the chosen stance (TrainRetry -> IhDefault(0): Stand still stays Stand still, Fight normally stays CPU); repeat
    // moves and aggro are global and untouched. Was IhDefault(1) here = always back to Stand still.
    if (g_tr.beaten && g_tr.practice && now - g_tr.tBeat >= (DWORD)g_cfg_beat_ms) TrainRetry("player beaten (Practice: retry in place)");
    else if (g_tr.beaten && now - g_tr.tBeat >= (DWORD)g_cfg_beat_ms) {  // #14267: BeatenRespawnMs (default 0 = instant)
        if (g_cfg_autoretry || !g_cfg_gate) TrainRetry(g_cfg_autoretry ? "player beaten" : "player beaten, ReturnGate 0"); else TrainLost(); }
    KillFadeDue();
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
    const char* t = SummonTok(id); char tok[32];
    if (t) snprintf(tok, sizeof tok, "%s", t); else snprintf(tok, sizeof tok, "%u", id);
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
        ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x3F9E58)))(r->chr, 2, 0); PoolAdd(r->chr);
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
// ally #89: boss name for a character id (arena table), for the menu's Boss A / B rows of bosses with no recorded move list. 1 = found.
N3T_API int Nioh3Training_BossName(uint32_t chr, char* out, int cap) {
    if (!out || cap <= 0) return 0;
    out[0] = 0;
    for (int i = 0; chr && i < g_nrows; ++i) if (g_rows[i].chr == chr) {
        const char* n = g_rows[i].name; const char* p = strstr(n, " (");
        snprintf(out, (size_t)cap, "%.*s", p ? (int)(p - n) : (int)strlen(n), n); return 1;
    }
    for (int i = 0; chr && i < g_nsum; ++i) if (g_sum[i].chr == chr) { snprintf(out, (size_t)cap, "%s", g_sum[i].name); return 1; }
    return 0;
}
// user #22984: Practice > Summon a boss here. CanSummon(mission) = 1 when the catalogue row resolves to one boss (multi-boss rows do not:
// their bosses are in the roster). Roster = the summon-only per-boss rows: RosterEntry fills the name and returns the character id (0 = none).
N3T_API int Nioh3Training_CanSummon(uint32_t mission) { return SummonTok(mission) != 0; }
N3T_API int Nioh3Training_RosterCount(void) { return g_nroster; }
N3T_API uint32_t Nioh3Training_RosterEntry(int i, char* name, int cap) {
    for (int k = 0; k < g_nsum; ++k) if (!g_sum[k].mission && i-- == 0) { if (name && cap > 0) snprintf(name, (size_t)cap, "%s", g_sum[k].name); return g_sum[k].chr; }
    if (name && cap > 0) name[0] = 0;
    return 0;
}
N3T_API int Nioh3Training_GetIdleHold(void);
N3T_API int Nioh3Training_SetMoveAllowed(uint32_t chr, uint32_t id, int allowed, int persist);
N3T_API int Nioh3Training_SetMoveRepeat(uint32_t chr, uint32_t id, int on);
N3T_API int Nioh3Training_SetRepeatGap(int ms, int persist);
N3T_API int Nioh3Training_SetIdleHold(int on);
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
            int keep = g_cfg_native; g_cfg_native = 0; g_pick = r; FightNowEntry(r->mission); g_cfg_native = keep;
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
static struct { int on, sawReset, lastSt, patched, lastSeq, lastG, target, aoSkips, blk, oldSkips; DWORD t0, t5, tAll, lastLog, tSeq, tAo, tAo4, maxMs; uint8_t w0, last58A, blkOld; uint64_t lastN, lastR, lastF; char tag[24]; int lgs; } g_xs;
// bridge 6047b (state numbers corrected in 6047c: 1 223BDB0, 2 223B3A8, 3 223B3F8, 4 223B8F0 world switch, 5 MissionLoadStep): mission load sequencer (handlers via 5A29F0, state mirrored in u32 B+0x45BA21C): 5 MissionLoadStep 223AF50 waits 1FC6674(stage+0x500) AND LoadIsLoadEnd
// 3B3424 BEFORE it consumes the request (the world switch); 8 223C4A8 waits LoadIsLoadEnd again; 0xB 223BE08 UiLoadingScreenEnd; 0xF 223C308 fixed 30/100 dt60 timer, then
// StopFlagSet(2,0) = GAMESTATE[2] cleared. LoadIsLoadEnd gate G5 = ObjectListAnyNotReady([45B9E60]) = any character resource entry with +0x2C8 == 0, i.e. the
// boss resources requested by CrossWorldPreload hold state 1 (the world switch) until the boss model is loaded (Ally: switch at +6.5 s vs +0.1 s without).
static __attribute__((noinline)) uint32_t SeqState(void) { uint32_t v = 0xFFFFFFFF; __try { v = *(volatile uint32_t*)(B + Grva(0x45BA21C)); } __except (EXCEPTION_EXECUTE_HANDLER) { } return v; }
static __attribute__((noinline)) int LoadGates(void) {  // bit set = that LoadIsLoadEnd gate currently blocks (read-only predicates, game thread): 1 G2 area list, 2 G3 request queue, 4 G5 resource list, 8 G7 blocker list, 16 G8 area objects
    uint64_t s = StageMgr(), rm = 0; int g = 0; if (!s) return -1;
    __try {
        if (((uint8_t (*)(uint64_t))(B + Grva(0x8B3868)))(s + 0x540)) g |= 1;
        if (((uint8_t (*)(uint64_t))(B + Grva(0x3B3974)))(s + 0x450)) g |= 2;
        if (Rd64(B + Grva(0x45B9E60), &rm) && rm && ((uint8_t (*)(uint64_t))(B + Grva(0x13335C)))(rm)) g |= 4;
        uint32_t a = 0, b = 0; if (!((uint8_t (*)(uint32_t*, uint32_t*))(B + Grva(0x2BE284)))(&a, &b)) g |= 8;
        if (!((uint8_t (*)(uint64_t, uint8_t, int32_t))(B + Grva(0x3B3A28)))(s, 0, -1)) g |= 16;
        // pc2 #6801 extra predicates of 3B3424 / 223AF50 (read-only): 32 G1 3B35B4 not ready, 64 G4 UI pair 3B3934, 128 G6 D3958(stage+0x540,[stage+0x504]) null, 256 G9 3B2970, 512 1FC6674 (state 5 own gate)
        if (*(volatile uint8_t*)(B + Grva(0x45C6F2C)) && !((uint8_t (*)(void))(B + Grva(0x3B3934)))()) g |= 64;
        if (*(volatile int32_t*)(s + 0x504) != -1 && !((uint64_t (*)(uint64_t, uint32_t))(B + Grva(0xD3958)))(s + 0x540, *(volatile uint32_t*)(s + 0x504))) g |= 128;
        if (((int (*)(void))(B + Grva(0x46518)))() && ((uint8_t (*)(void))(B + Grva(0x3B2970)))()) g |= 256;
        if (!((uint8_t (*)(uint64_t))(B + Grva(0x1FC6674)))(s + 0x500)) g |= 512;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -2; }
    return g;
}
// bridge 6047c: gate G8 = StageAreasObjectsReady 3B3A28(stage, force, area): for every loaded cell A = D3B9C(stage+0x540, i) with o = [A+0x30]:
// ready only if o+0x1F0 != 0, o+0x1EC == 0 and A+0x1C0 == 0. o+0x1EC counts the cell's placeables whose load is in flight (placeable +0x10D = 3:
// incremented in PlaceableCollisionUpdate D8E00 at D97E8/D9EC7 when the load is queued on the loader [475A420] (+0x1D8 list), decremented in 2007E4
// when it finishes). Placeable resources load through the TrySync step pool (3 threads, read-bound on the Ally), no per-frame budget in D8E00.
static __attribute__((noinline)) int AreaObjPending(int* blocking, int* pending) {
    uint64_t s = StageMgr(), n = 0; *blocking = *pending = 0; if (!s || !Rd64(s + 0x560, &n)) return 0;
    __try {
        for (uint64_t i = 0; i < n && i < 256; ++i) {
            uint64_t a = ((uint64_t (*)(uint64_t, uint64_t))(B + Grva(0xD3B9C)))(s + 0x540, i), o; if (!a) continue;
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
static int g_cfg_xaoold = 0;  // pc2 8256 (unused after merge: HEAD CrossWorldForceCells covers the old-world gate)
typedef uint8_t (*AorFn)(uint64_t, uint8_t, int32_t);
static AorFn o_aor;
static __attribute__((noinline)) uint8_t h_aor(uint64_t s, uint8_t force, int32_t area) {
    uint8_t r = o_aor(s, force, area);
    if (r || area != -1 || !g_xs.on || g_xs.target < 0) return r;
    __try {
        uint32_t q = *(volatile uint32_t*)(B + Grva(0x45BA21C)); uint64_t sm = StageMgr();
        if (g_cfg_xforce && q == 4 && sm && s == sm && *(uint8_t*)(sm + 0x500) == g_xs.w0 && g_xs.w0 != (uint8_t)g_xs.target) {  // old world before the unload: don't wait for its placeables
            if (!g_xs.oldSkips++) { int b, p; AreaObjPending(&b, &p); Log("XSTREAM %s +%lu ms: old-world area-objects wait skipped (CrossWorldForceCells, %d cells blocking, %d placeable loads)", g_xs.tag, GetTickCount() - g_xs.t0, b, p); }
            return 1;
        }
        if (g_cfg_xaoskip <= 0) return r;
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
    uint8_t* pa = (uint8_t*)(B + Grva(0x31E0B2)); uint8_t* pb = (uint8_t*)(B + Grva(0x31E0BB));
    if (n < 1 || n > 0x7F) return 0;
    if (memcmp(pa, a0, 2) || memcmp(pa + 3, a1, 6) || memcmp(pb, b0, 7) || memcmp(pb + 8, b1, 6)) { Log("XSTREAM cell cap: bytes at 31E0B2/31E0BB differ (game version?), not patched"); return 0; }
    if (pa[2] == n && pb[7] == n) return 1;
    DWORD old; if (!VirtualProtect(pa, 0x18, PAGE_EXECUTE_READWRITE, &old)) return 0;
    pa[2] = (uint8_t)n; pb[7] = (uint8_t)n;  // single-byte stores: the game thread sees the old or the new immediate, never a torn instruction
    VirtualProtect(pa, 0x18, old, &old); FlushInstructionCache(GetCurrentProcess(), pa, 0x18);
    Log("XSTREAM cell load cap -> %d (31E0B4 / 31E0C2)", n);
    return 1;
}
static __attribute__((noinline)) int CellStats(uint64_t* n, uint64_t* ready, uint64_t* loading, uint64_t* relq, uint64_t* m) {
    uint64_t s = StageMgr(), a = 0, b = 0; *n = *ready = *loading = *relq = *m = 0;
    if (!s || !Rd64(s + 0x500 + 0x60, n)) return 0;
    if (Rd64(s + 0x19C0, &a) && Rd64(s + 0x19C8, &b) && b >= a && b - a < 0x10000) *loading = (b - a) / 4;
    if (Rd64(s + 0x1AA8, &a) && Rd64(s + 0x1AB0, &b) && b >= a && b - a < 0x10000) *relq = (b - a) / 4;
    Rd64(s + 0x1B48, m);
    for (uint64_t i = 0; i < *n && i < 256; ++i) {
        uint64_t e = 0;
        __try { e = ((uint64_t (*)(uint64_t, uint64_t))(B + Grva(0xD3B9C)))(s + 0x500 + 0x40, i); if (e && *(uint8_t*)(e + 0xE8)) ++*ready; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }
    return 1;
}
// pc2 #6801: [ArenaSwap] CrossWorldLoaderGateSkip = bit mask (default 0; 1 = G9 only). NOT a gain on pc2: state 5 ends ~8 s earlier but state 6 (223C53C waits until every node of a list at 1C1C61C has +0x138 == 8, ~+24 s) then holds the same total; also skipping that wait (tried, jne at 223C55A) hangs the game at sequencer state 0xF. LoadIsLoadEnd gate G9 (3B3424: 46518() != 0 && 3B2970() != 0, jne at 3B3523 = 75 6E) blocks the mission sequencer states 5 and 8
// while the resource loader [475A420] has +0x248 / +0x249 set (placeable / area loads still queued): measured on pc2 it alone holds state 5 for ~12 s after the stage is ready (+11 -> +23.6 s) and
// state 8 for ~3 s, i.e. the control lock after a cross-world Fight now. NOPs the 2-byte jne from the world request until the switch has streamed (XsEnd, <= 60 s); loads keep running in the background.
static int g_cfg_xlgs = 0;
static const struct { uint32_t rva; uint8_t jcc, rel; int bit; const char* name; } k_lgs[] = {
    {0x3B3523, 0x75, 0x6E, 1, "G9 loader busy (3B2970)"},
};
static __attribute__((noinline)) int LoaderGatePatch(int on) {
    int ok = 0;
    for (int i = 0; i < (int)(sizeof k_lgs / sizeof k_lgs[0]); ++i) {
        if (!(g_cfg_xlgs & k_lgs[i].bit)) continue;
        uint8_t* pa = (uint8_t*)(B + Grva(k_lgs[i].rva)); DWORD old; int done = 0;
        __try {
            if (on && !(pa[0] == k_lgs[i].jcc && pa[1] == k_lgs[i].rel)) { Log("XSTREAM loader gate %s: bytes at %X are %02X %02X, left alone", k_lgs[i].name, k_lgs[i].rva, pa[0], pa[1]); continue; }
            if (!on && !(pa[0] == 0x90 && pa[1] == 0x90)) continue;
            if (VirtualProtect(pa, 2, PAGE_EXECUTE_READWRITE, &old)) { *(volatile uint16_t*)pa = on ? 0x9090 : (uint16_t)(k_lgs[i].jcc | (k_lgs[i].rel << 8)); VirtualProtect(pa, 2, old, &old); FlushInstructionCache(GetCurrentProcess(), pa, 2); done = 1; }
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
        if (done) { ok = 1; Log("XSTREAM loader gate %s (%X) %s", k_lgs[i].name, k_lgs[i].rva, on ? "skipped" : "restored"); }
    }
    return ok;
}
// pc2 placeable-skip: [ArenaSwap] CrossWorldPlaceableSkip = 1 (default 0 = no patch at all, byte-identical): while a cross-world switch runs, placeables
// of the OLD world, and of the NEW world outside the row's arena cells (tsv col 16 / kCellsB1), are not queued on the loader.
// Static facts (Nioh3.exe .text from the fl-bin memory dump, capstone): PlaceableCollisionUpdate D8E00(o = cell object list, ctx) walks the placeables
// of o (+0x180, 16-byte entries); placeable state +0x10D: 0 = not loaded, 3 = load in flight, 5 = loaded. D90B6 movzx eax,[r13+10D]; D90BE test al,al;
// D90C0 je D91FA (0F 84 rel32) is the ONLY entry into the state-0 load block D91FA..D9EF2 (no other jmp/jcc in .text targets that range).
// Both queue sites are inside it: D97E0/D97E8 (shared resource record via D8D90: alloc node, push on loader [475A420]+0x1D8, +0x1E0++, +0x24A = 1,
// record +0x10D = 3, [o]+0x1EC++) and D9EBF/D9EC7 (own resource: 5CE980 push on +0x1D8, +0x24A = 1, placeable +0x10D = 3, o+0x1EC++).
// o = [ctx+0x30]'s +0x30 = D8E00's rcx; e = [o] is the stage cell entry (567A90: [e] & 0xF == 1), cell id = [e] >> 4 (same ids as StageLog / CELLREQ).
// Skip = the native 'not eligible this frame' exit of the state-0 block (D9260 je D9EF2): nothing allocated or queued, +0x10D stays 0, o+0x1EC
// (gate G8 3B3A28) is never incremented, loader +0x24A not set, the placeable is re-evaluated next frame and loads normally once the window closes.
// Patch: the rel32 of D90C0 is retargeted (one locked 4-byte write) to a near stub that calls PsFilter(o, placeable) and jumps to D91FA or D9EF2.
// Window: from XsStart (world request) until the target world is current with stage 5, mission seq state not 4/5/8 and GAMESTATE[2] == 0, XsEnd,
// or CrossWorldPlaceableSkipMaxMs (default 20000). Loads queued BEFORE the window are not cancelled.
static const uint32_t kCellsB1[] = {67253, 221265, 285287, 301780, 320919, 329181, 364049, 500328, 525291, 584691, 625536, 780040, 820705, 833442, 889691, 974198, 983784};  // 0xB1 Yoritomo (pc2 5116, all runs)
static int g_cfg_pskip = 0, g_cfg_pskip_max = 20000;
// pc #8552 (user: "skip them all except what the arena needs"; branch pc/near-arena-only): [ArenaSwap] NearArenaOnly = 1 uses the same D90C0 hook as
// CrossWorldPlaceableSkip, but decides by DISTANCE instead of by cell: during a cross-world Fight now a NEW-world placeable (inside the arena cells too) is
// queued only if it lies within NearArenaRadius (game units = cm; default 15000 = 150 m) of the row's player spawn OR boss spawn point (distance from the
// placeable centre +0x80 minus half its scaled extent |+0x90 * +0x70| / 2, so big terrain pieces that reach the arena still count as near); old-world
// placeables are not queued at all. Why: StageAreasObjectsReady (G8 3B3A28, also the hold's XArenaClear) calls a cell blocking while its area object o has
// o+0x1EC != 0 (placeable loads in flight, ++ at D97E8/D9EC7 when queued, -- in 2007E4), o+0x1F0 == 0 or entry+0x1C0 != 0; on Shingen 3 arena cells carry
// ~3100 queued placeables (pc2 #8542) draining through manager A at 10 in flight. Far placeables held in state 0 are never counted in o+0x1EC, so the cells
// stop blocking as soon as the NEAR set is in and the cross-world hold (AreaWaitCrossMs) ends there instead of at the cap. The window stays open until the
// player has control and the fade-in is done, then the far ones stream normally; NearArenaMaxMs (default 40000) caps it. 0 (default) = off, nothing patched.
static int g_cfg_far = 0, g_cfg_far_radius = 15000, g_cfg_far_max = 40000;
static float g_ps_c[3], g_ps_c2[3]; static int g_ps_far;  // g_ps_far: this window uses the radius rule (player spawn g_ps_c, boss spawn g_ps_c2)
static volatile LONG g_ps_near, g_ps_farn, g_ps_nlog, g_ps_flog;
static const ArenaRow* g_ps_row;  // set by XwStart right before XsStart (whitelist source); NULL for the oworld command = old world only
static struct { volatile LONG on; uint8_t w0, target; int nwl; uint32_t wl[32]; DWORD t0; char tag[24]; } g_ps;
static volatile LONG g_ps_old, g_ps_new, g_ps_allow, g_ps_logged, g_ps_hooked;
// pc #8552 diagnostics: what each blocking cell waits on (G8 flags): area object o = [entry+0x30]: o+0x1F0 (setup done), o+0x1EC (placeable loads in flight),
// entry+0x1C0 (pending count on the entry); cell id = [[o]] >> 4.
static __attribute__((noinline)) void AreaObjDetail(const char* why) {
    uint64_t s = StageMgr(), n = 0; if (!s || !Rd64(s + 0x560, &n)) return;
    char buf[640]; int k = 0, nb = 0;
    __try {
        for (uint64_t i = 0; i < n && i < 256; ++i) {
            uint64_t a = ((uint64_t (*)(uint64_t, uint64_t))(B + Grva(0xD3B9C)))(s + 0x540, i), o, e = 0; if (!a) continue;
            o = *(uint64_t*)(a + 0x30); if (!o) continue;
            uint8_t f = *(uint8_t*)(o + 0x1F0); int32_t c = *(int32_t*)(o + 0x1EC), x = *(int32_t*)(a + 0x1C0);
            if (f && !c && !x) continue;
            ++nb; Rd64(o, &e); uint32_t id = 0; if (e) id = *(uint32_t*)e >> 4;
            if (k < (int)sizeof buf - 64) k += snprintf(buf + k, sizeof buf - k, " [%u 1F0=%u 1EC=%d 1C0=%d]", id, f, c, x);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    buf[k] = 0; const uint8_t* gs = GameState();
    Log("AREAOBJ %s: %d of %llu cells blocking%s; GAMESTATE[2] %u, seq state %u%s", why, nb, (unsigned long long)n, k ? buf : "", gs ? gs[2] : 0xFF, *(volatile uint32_t*)(B + Grva(0x45BA21C)),
        g_ps.on && g_ps_far ? "; NearArenaOnly window open" : "");
}
static float PsDist(uint64_t pl) {  // placeable -> nearer of the player / boss spawn point, minus half the scaled extent, >= 0
    const float* p = (const float*)(pl + 0x80); const float* sc = (const float*)(pl + 0x70); const float* ex = (const float*)(pl + 0x90);
    float dx = p[0] - g_ps_c[0], dy = p[1] - g_ps_c[1], dz = p[2] - g_ps_c[2], d1 = dx * dx + dy * dy + dz * dz;
    dx = p[0] - g_ps_c2[0]; dy = p[1] - g_ps_c2[1]; dz = p[2] - g_ps_c2[2]; float d2 = dx * dx + dy * dy + dz * dz;
    float ex0 = ex[0] * sc[0], ex1 = ex[1] * sc[1], ex2 = ex[2] * sc[2];
    float d = __builtin_sqrtf(d1 < d2 ? d1 : d2) - 0.5f * __builtin_sqrtf(ex0 * ex0 + ex1 * ex1 + ex2 * ex2);
    return d != d ? 0.0f : d < 0.0f ? 0.0f : d;  // NaN -> 0 (= keep: load it)
}
static __attribute__((noinline)) uint8_t PsFilter(uint64_t o, uint64_t pl) {  // called from the stub on the KIDSTS stage-update thread(s): 1 = do not queue this placeable now
    if (!g_ps.on) return 0;
    __try {
        uint64_t s = *(volatile uint64_t*)(B + Grva(0x45B9E58)); if (!s || !o) return 0;
        uint8_t w = *(volatile uint8_t*)(s + 0x500);
        uint64_t e = *(volatile uint64_t*)o; if (!e) return 0;
        uint32_t id = *(volatile uint32_t*)e >> 4;
        int old = w == g_ps.w0, skip = old;
        if (!old && w == g_ps.target && g_ps_far) {
            float d = PsDist(pl); skip = d > (float)g_cfg_far_radius;
            InterlockedIncrement(skip ? &g_ps_farn : &g_ps_near);
            if (InterlockedIncrement(skip ? &g_ps_flog : &g_ps_nlog) <= 6) { const float* p = (const float*)(pl + 0x80);
                Log("PSKIP %s +%lu ms: FAR %s cell %u placeable %llX at %.0f %.0f %.0f, %.0f from the player/boss spawn (radius %d)", g_ps.tag, GetTickCount() - g_ps.t0, skip ? "held" : "kept (near)", id, (unsigned long long)pl, p[0], p[1], p[2], d, g_cfg_far_radius); }
            if (!skip) return 0;
            InterlockedIncrement(&g_ps_new); return 1;
        }
        if (!old && w == g_ps.target && g_ps.nwl) { skip = 1; for (int i = 0; i < g_ps.nwl; ++i) if (g_ps.wl[i] == id) { skip = 0; break; } }
        if (!skip) { InterlockedIncrement(&g_ps_allow); return 0; }
        InterlockedIncrement(old ? &g_ps_old : &g_ps_new);
        if (InterlockedIncrement(&g_ps_logged) <= 8) Log("PSKIP %s +%lu ms: %s-world cell %u (world 0x%02X) placeable %llX not queued (tid %lu)", g_ps.tag, GetTickCount() - g_ps.t0, old ? "old" : "new", id, w, (unsigned long long)pl, GetCurrentThreadId());
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static __attribute__((noinline)) int PsHook(void) {  // installs the D90C0 retarget once (only when CrossWorldPlaceableSkip = 1)
    static const uint8_t pre[16] = {0x41, 0x0F, 0xB6, 0x85, 0x0D, 0x01, 0x00, 0x00, 0x84, 0xC0, 0x0F, 0x84, 0x34, 0x01, 0x00, 0x00};
    static const uint8_t ld3a[8] = {0x41, 0xC6, 0x87, 0x0D, 0x01, 0x00, 0x00, 0x03}, ld3b[8] = {0x41, 0xC6, 0x85, 0x0D, 0x01, 0x00, 0x00, 0x03}, tgt[7] = {0x49, 0x8B, 0x85, 0xB8, 0x00, 0x00, 0x00};
    uint8_t* site = (uint8_t*)(B + Grva(0xD90B6));
    __try {
        if (memcmp(site, pre, 16) || memcmp((void*)(B + Grva(0xD97E0)), ld3a, 8) || memcmp((void*)(B + Grva(0xD9EBF)), ld3b, 8) || memcmp((void*)(B + Grva(0xD91FA)), tgt, 7) || memcmp((void*)(B + Grva(0xD9EF2)), tgt, 7)) {
            Log("PSKIP: bytes at D90B6/D97E0/D9EBF/D91FA/D9EF2 differ (game version or another patch), CrossWorldPlaceableSkip disabled"); return 0; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PSKIP: site not readable, disabled"); return 0; }
    uint8_t* st = 0;
    for (uintptr_t a = ((uintptr_t)site - 0x70000000) & ~0xFFFFull; a < (uintptr_t)site + 0x70000000 && !st; a += 0x10000)
        st = (uint8_t*)VirtualAlloc((void*)a, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!st) { Log("PSKIP: no near stub, disabled"); return 0; }
    // entered with al == 0 (placeable state 0), rsp 16-aligned (D8E00 body: 7 pushes + sub 240h), this = [rsp+40h]; r10 (live constant) and the other volatiles are preserved
    static const uint8_t c0[] = {
        0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53,           // push rcx rdx r8 r9 r10 r11 (30h)
        0x48, 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00,                             // sub rsp,80h (20h shadow + 6 xmm), rsp stays 16-aligned
        0x0F, 0x11, 0x44, 0x24, 0x20, 0x0F, 0x11, 0x4C, 0x24, 0x30, 0x0F, 0x11, 0x54, 0x24, 0x40,
        0x0F, 0x11, 0x5C, 0x24, 0x50, 0x0F, 0x11, 0x64, 0x24, 0x60, 0x0F, 0x11, 0x6C, 0x24, 0x70,  // movups [rsp+20h..70h], xmm0..xmm5
        0x48, 0x8B, 0x8C, 0x24, 0xF0, 0x00, 0x00, 0x00,                       // mov rcx,[rsp+0F0h] (= D8E00's [rsp+40h] = o)
        0x4C, 0x89, 0xEA,                                                     // mov rdx,r13 (placeable)
        0x48, 0xB8 };                                                         // mov rax, imm64 (PsFilter)
    static const uint8_t c1[] = {
        0xFF, 0xD0,                                                           // call rax
        0x84, 0xC0,                                                           // test al,al (flags survive the restores below)
        0x0F, 0x10, 0x44, 0x24, 0x20, 0x0F, 0x10, 0x4C, 0x24, 0x30, 0x0F, 0x10, 0x54, 0x24, 0x40,
        0x0F, 0x10, 0x5C, 0x24, 0x50, 0x0F, 0x10, 0x64, 0x24, 0x60, 0x0F, 0x10, 0x6C, 0x24, 0x70,  // movups xmm0..xmm5
        0x48, 0x8D, 0xA4, 0x24, 0x80, 0x00, 0x00, 0x00,                       // lea rsp,[rsp+80h]
        0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59,           // pop r11 r10 r9 r8 rdx rcx
        0x75, 0x0E,                                                           // jnz skip
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };                                 // jmp [rip] -> D91FA (load block), then skip: jmp [rip] -> D9EF2
    int k = 0;
    memcpy(st + k, c0, sizeof c0); k += sizeof c0; *(uint64_t*)(st + k) = (uint64_t)PsFilter; k += 8;
    memcpy(st + k, c1, sizeof c1); k += sizeof c1; *(uint64_t*)(st + k) = B + Grva(0xD91FA); k += 8;
    st[k++] = 0xFF; st[k++] = 0x25; *(uint32_t*)(st + k) = 0; k += 4; *(uint64_t*)(st + k) = B + Grva(0xD9EF2); k += 8;
    FlushInstructionCache(GetCurrentProcess(), st, k);
    int32_t rel = (int32_t)((intptr_t)st - (intptr_t)(site + 16));  // je rel32 at D90C0: next ip = D90C6
    DWORD old; if (!VirtualProtect(site + 12, 4, PAGE_EXECUTE_READWRITE, &old)) { Log("PSKIP: VirtualProtect failed, disabled"); return 0; }
    InterlockedExchange((volatile LONG*)(site + 12), rel);  // only the rel32 of the je changes (D90C2, 4 bytes inside one cache line)
    VirtualProtect(site + 12, 4, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 16);
    g_ps_hooked = 1; Log("PSKIP: state-0 entry D90C0 retargeted to stub %p (%d bytes), CrossWorldPlaceableSkip on, max window %d ms", st, k, g_cfg_pskip_max);
    return 1;
}
static void PsClose(const char* why) {
    if (!InterlockedExchange(&g_ps.on, 0)) return;
    Log("PSKIP %s: window closed (%s) +%lu ms; skip decisions (per frame, a placeable counts again every frame it waits): old world %ld, new world %ld; allowed %ld%s",
        g_ps.tag, why, GetTickCount() - g_ps.t0, g_ps_old, g_ps_new, g_ps_allow, g_ps_far ? " (NearArenaOnly radius rule)" : "");
    if (g_ps_far) Log("PSKIP %s: NearArenaOnly new-world decisions: kept (near) %ld, held (far) %ld (per frame)", g_ps.tag, g_ps_near, g_ps_farn);
}
static void PsOpen(const char* tag, int target, uint8_t w0) {
    if ((!g_cfg_pskip && !g_cfg_far) || !g_ps_hooked) return;
    PsClose("reopened");
    const ArenaRow* r = g_ps_row; g_ps.nwl = 0; g_ps_far = 0;
    if (g_cfg_far && r && r->world == (uint8_t)target) { g_ps_c[0] = r->p[0]; g_ps_c[1] = r->p[1]; g_ps_c[2] = r->p[2]; memcpy(g_ps_c2, r->b, sizeof g_ps_c2); if (!g_ps_c2[0] && !g_ps_c2[1] && !g_ps_c2[2]) memcpy(g_ps_c2, g_ps_c, sizeof g_ps_c2); g_ps_far = 1; }
    g_ps_near = g_ps_farn = g_ps_nlog = g_ps_flog = 0;
    if (r && r->world == (uint8_t)target && !g_ps_far) {
        for (int i = 0; i < r->ncells && g_ps.nwl < 32; ++i) g_ps.wl[g_ps.nwl++] = r->cells[i];
        if (!g_ps.nwl && r->world == 0xB1) for (unsigned i = 0; i < sizeof kCellsB1 / sizeof *kCellsB1 && g_ps.nwl < 32; ++i) g_ps.wl[g_ps.nwl++] = kCellsB1[i];
    }
    if (target < 0 || (uint8_t)target == w0) return;
    g_ps.w0 = w0; g_ps.target = (uint8_t)target; g_ps.t0 = GetTickCount(); snprintf(g_ps.tag, sizeof g_ps.tag, "%s", tag);
    g_ps_old = g_ps_new = g_ps_allow = g_ps_logged = 0;
    InterlockedExchange(&g_ps.on, 1);
    if (g_ps_far) Log("PSKIP %s: window open, world 0x%02X -> 0x%02X, NearArenaOnly: new-world placeables farther than %d from the player spawn %.0f %.0f %.0f and the boss spawn %.0f %.0f %.0f held until control + fade-in (max %d ms)", tag, w0, (uint8_t)target, g_cfg_far_radius, g_ps_c[0], g_ps_c[1], g_ps_c[2], g_ps_c2[0], g_ps_c2[1], g_ps_c2[2], g_cfg_far_max);
    else Log("PSKIP %s: window open, world 0x%02X -> 0x%02X, new-world whitelist %d cells%s", tag, w0, (uint8_t)target, g_ps.nwl, g_ps.nwl ? "" : " (none: only old-world placeables are held back)");
}
static void PsTick(void) {
    if (!g_ps.on) return;
    DWORD now = GetTickCount();
    if (g_ps_far) { if (now - g_ps.t0 > (DWORD)g_cfg_far_max) PsClose("NearArenaMaxMs"); return; }  // closed by PsFarTick (control + fade-in)
    if (now - g_ps.t0 > (DWORD)g_cfg_pskip_max) { PsClose("CrossWorldPlaceableSkipMaxMs"); return; }
    uint64_t s = StageMgr(); const uint8_t* gs = GameState(); uint32_t q = SeqState(); if (!s || !gs) return;
    if (*(volatile uint8_t*)(s + 0x500) == g_ps.target && StageState() == 5 && !gs[2] && q != 4 && q != 5 && q != 8) PsClose("new world landed, load flag clear");
}
static __attribute__((noinline)) void XsStart(const char* tag, int target) {  // called right before a WarpOtherWorld request
    memset(&g_xs, 0, sizeof g_xs); snprintf(g_xs.tag, sizeof g_xs.tag, "%s", tag); g_xs.target = target;
    uint64_t s = StageMgr(); g_xs.w0 = s ? *(uint8_t*)(s + 0x500) : 0xFF; g_xs.t0 = GetTickCount(); g_xs.tSeq = g_xs.t0; g_xs.lastSt = StageState(); g_xs.on = 1; g_xs.lastSeq = -9; g_xs.last58A = 0xEE; g_xs.lastG = -9;
    g_xs.patched = g_cfg_xcells > 4 && CellCapSet(g_cfg_xcells);
    g_xs.lgs = g_cfg_xlgs && LoaderGatePatch(1);
    if (g_cfg_xblock && s) { __try { g_xs.blkOld = *(volatile uint8_t*)(s + 0x11F1); *(volatile uint8_t*)(s + 0x11F1) = 0; g_xs.blk = 1; } __except (EXCEPTION_EXECUTE_HANDLER) { g_xs.blk = 0; } }
    Log("XSTREAM %s: CrossWorldBlockCells %d (stage +0x11F1 %s)", tag, g_cfg_xblock, g_xs.blk ? "cleared until state 5" : "not changed");
    Log("XSTREAM %s: start, world 0x%02X state %d, cell load cap %d (CrossWorldCellLoads %d)", tag, g_xs.w0, g_xs.lastSt, g_xs.patched ? g_cfg_xcells : 4, g_cfg_xcells);
    PsOpen(tag, target, g_xs.w0);
}
static __attribute__((noinline)) void XsBlkRestore(const char* why) {
    uint64_t s = StageMgr(); if (!g_xs.blk || !s) { g_xs.blk = 0; return; }
    __try { *(volatile uint8_t*)(s + 0x11F1) = g_xs.blkOld; } __except (EXCEPTION_EXECUTE_HANDLER) { }
    g_xs.blk = 0; Log("XSTREAM %s: stage +0x11F1 restored to %u (%s)", g_xs.tag, g_xs.blkOld, why);
}
static void XsEnd(const char* why) {
    if (g_xs.patched) CellCapSet(4);
    if (g_xs.lgs) { LoaderGatePatch(0); g_xs.lgs = 0; }
    XsBlkRestore(why);
    PsClose(why);
    Log("XSTREAM %s: %s; state 5 at %s%lu ms, all cells loaded at %s%lu ms (cap %d)", g_xs.tag, why, g_xs.t5 ? "+" : "never/", g_xs.t5 ? g_xs.t5 - g_xs.t0 : 0, g_xs.tAll ? "+" : "never/", g_xs.tAll ? g_xs.tAll - g_xs.t0 : 0, g_xs.patched ? g_cfg_xcells : 4);
    g_xs.on = 0; g_xs.patched = 0;
}
static int g_cfg_xpre_after = 0;
static void XsTick(void) {
    PsTick();
    if (!g_xs.on) return;
    DWORD now = GetTickCount(), dt = now - g_xs.t0; uint64_t s = StageMgr(); if (!s) return;
    int st = StageState(); uint8_t w = *(uint8_t*)(s + 0x500);
    uint64_t n, r, f, q, m; int ok = CellStats(&n, &r, &f, &q, &m);
    if (st != 5 || w != g_xs.w0) g_xs.sawReset = 1;
    { const uint8_t* gs = GameState(); uint8_t f = gs ? gs[2] : 0xFF; uint32_t q = SeqState(); int g = LoadGates();
      if (f != g_xs.last58A || (int)q != g_xs.lastSeq || g != g_xs.lastG || dt == 0) {
          Log("XSTREAM %s +%lu ms: GAMESTATE[2] %u, mission seq state %d (was %d, %lu ms), LoadIsLoadEnd gates 0x%X (1 areas 2 stage req 4 res list 8 blockers 16 area objs 32 G1 64 G4 128 G6 256 G9 512 seq5) world 0x%02X stage %d",
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
    if (dt > (g_xs.maxMs ? g_xs.maxMs : 60000)) XsEnd("window timeout");
}
// bridge 5276 (pc2 5116/5275): boss in ANOTHER world, no mission load. Only a world switch brings the other world's arena: World::WarpOtherWorld request (22399B8, mission 0, target world, start point)
// switches in ~1 s and the stage is ready (state 5) ~5-11 s later; then the normal in-place arena swap (warp + live spawn) runs. ~10 s to a fightable boss vs ~29 s native. Any failure -> native MissionRequestById.
static int g_cfg_xworld = 1, g_cfg_xpre = 1, g_xw_busy, g_cfg_xearly;
// bridge 6375: CrossWorldSettleMs = wait after 'stage ready' before the swap (was fixed 700). CrossWorldPrewarm = 1: Nioh3Training_Preload()/PreloadFor(mission) (menu opened) starts the world request for the
// likely target (ini CrossWorldPrewarmMission, else the last cross-world boss) before the player picks; Fight now for a boss of that world then just claims the running request (or is a plain same-world swap once landed).
static int g_cfg_xabbm = 1, g_cfg_xhold = 30000, g_cfg_xsettle = 700, g_cfg_xprewarm, g_xw_last_mission; static uint32_t g_cfg_xprewarm_mission;
static struct { int active, claimed, preDeferred, cancel, forced; ArenaRow* row; DWORD t0, tReady, tClaim, tCancel; uint8_t oWorld, oJourney; uint32_t oStart, oMode; float oPos[4]; } g_xw;  // bridge 6550: o* = where the prewarm started (cancel returns there)
static struct { int active; uint8_t world, journey; uint32_t start, mode; float pos[4]; DWORD t0, tSeen, tLast, run, paused, tSeq9, tCell; int watch, pauseLogged; uint32_t ev[9]; uint64_t lastR; } g_xr;
static volatile LONG g_fz_hold; static int g_fz_vk_freeze; static int FzHoldSet(int on, const char* why);  // user #22589 Practice freeze (practice_freeze_game.h)
static volatile LONG g_dbg_frozen; static float DbgTimeScale(void); static void DbgFreeze(int on); static volatile LONG g_menu_frozen; static void MenuFreeze(int on, const char* why);  // F9 debug freeze (defined with the hotkeys below)  // bridge 6550: return trip after a cancelled prewarm
// user #6779: 'Return to where I was' (Boss Rematch menu). g_home = where the player stood before the first Fight now of this arena session (world, spot,
// running mission / journey / mode); chained Fight nows keep it, a Fight now with no arena session running (or after a mission change) takes a new one.
static struct { int valid; uint8_t world, journey; uint32_t mission, mode, start; float pos[4]; } g_home;
// home-mission-1790: after a WarpOtherWorld Return the mission state reads mission 0 (the trip starts a mission-less journey), so the next HomeCapture stored 'mission 0x0' and RhTick's "mission changed" guard (g_home.mission && ...) was skipped. g_homeKeep remembers the mission a Return went back to; HomeCapture reuses it while the state still reads 0 in that world.
static struct { int valid; uint8_t world, journey; uint32_t mission, mode; } g_homeKeep;
static struct { volatile LONG want; int active, phase, arena; DWORD t0, tPh, tStable; } g_rh;  // RhTick: 1 cancel a running swap, 2 move, 3 cross-world trip, 4 settle + fade in
static __attribute__((noinline)) int MsRead(uint32_t* mission, uint8_t* journey, uint32_t* mode) {  // mission state: +10 journey, +24 mode (XwStart), +28 running mission id (Boss Rematch Snap.ms28)
    uint64_t mm = 0, ms = 0; if (!Rd64(B + Grva(0x47514D0), &mm) || !mm || !Rd64(mm + 8, &ms) || !ms) return 0;
    __try { *journey = *(uint8_t*)(ms + 0x10); *mode = *(uint32_t*)(ms + 0x24); *mission = *(uint32_t*)(ms + 0x28); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
// user #9291/#9296: screen recording tied to Boss Rematch. [Record] Enable = 1: at every Fight now (Boss Rematch menu / Fight again) start the recorder
// script ([Record] Script, default <game dir>\Nioh3FightRecorder.ps1) once, hidden; it captures ~3 fps half-size jpgs into [Record] OutRoot\rec_<stamp>\
// and stops by itself at 'RETURN done', when the game exits, 90 s after a fight end with no new Fight now, or after MaxMinutes. 0 (default) = off.
static int g_cfg_rec = 0; static char g_rec_script[MAX_PATH], g_rec_out[MAX_PATH]; static int g_rec_max = 20; static HANDLE g_rec_proc;
static void RecStart(const char* why) {
    if (!g_cfg_rec) return;
    if (g_rec_proc) { if (WaitForSingleObject(g_rec_proc, 0) == WAIT_TIMEOUT) return; CloseHandle(g_rec_proc); g_rec_proc = 0; }
    if (GetFileAttributesA(g_rec_script) == INVALID_FILE_ATTRIBUTES) { static int nl; if (nl++ < 3) Log("RECORD: script %s not found, no recording", g_rec_script); return; }
    char cmd[3 * MAX_PATH + 160]; snprintf(cmd, sizeof cmd, "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\" -GamePid %lu -OutRoot \"%s\" -MaxMinutes %d", g_rec_script, GetCurrentProcessId(), g_rec_out, g_rec_max);
    STARTUPINFOA si; PROCESS_INFORMATION pi; memset(&si, 0, sizeof si); si.cb = sizeof si; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; memset(&pi, 0, sizeof pi);
    if (CreateProcessA(0, cmd, 0, 0, FALSE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, 0, 0, &si, &pi)) { g_rec_proc = pi.hProcess; CloseHandle(pi.hThread); Log("RECORD: recorder started (%s), pid %lu -> %s\\rec_<stamp>", why, pi.dwProcessId, g_rec_out); }
    else Log("RECORD: CreateProcess failed (%lu)", GetLastError());
}
static void HomeCapture(int world, const float* pos, const char* why) {
    RecStart(why);
    uint32_t mis = 0, mode = 0; uint8_t jr = 0; if (world < 0 || !MsRead(&mis, &jr, &mode)) return;
    if (mis) g_homeKeep.valid = 0;
    else if (g_homeKeep.valid && g_homeKeep.world == (uint8_t)world) { Log("RETURN home: mission state reads 0 after a world trip, keeping mission 0x%X journey %u mode %u", g_homeKeep.mission, g_homeKeep.journey, g_homeKeep.mode); mis = g_homeKeep.mission; jr = g_homeKeep.journey; mode = g_homeKeep.mode; }
    if (g_home.valid && g_tr.active && g_tr.hasStart && mis == g_home.mission) return;  // still in the session that started there
    g_home.valid = 1; g_home.world = (uint8_t)world; g_home.journey = jr; g_home.mode = mode; g_home.mission = mis; g_home.start = *(uint32_t*)(B + Grva(0x45BC42C)); memcpy(g_home.pos, pos, sizeof g_home.pos);
    Log("RETURN home stored (%s): world 0x%02X %.0f %.0f %.0f yaw %.1f, mission 0x%X journey %u mode %u", why, world, pos[0], pos[1], pos[2], pos[3], mis, jr, mode);
}
// CrossWorldForceCells: arena cells per row = tsv column 16 if given, else these (pc 10:33 OFF logs: cells that arrived only at stage 4->5 / after the swap)
static const struct { uint32_t mission, cells[8]; } kForceCells[] = {
    {0x15FF, {193960, 439542, 633740}},                                // Takeda Shingen 0x96 (pc2 #8222)
    {0x2EE9, {364049, 484693, 525291, 67253, 584691, 820705}},          // Otoroshi 0xB1
};
static __attribute__((noinline)) void XwCentre(ArenaRow* r, int stream) {  // stream centre -> the arena's player point (as Warp(full) does); stream = also run the native centre update 3A083C
    uint64_t s = StageMgr(); if (!s) return;
    __try {
        float* c = (float*)(s + 0x10B0); c[0] = r->p[0]; c[1] = r->p[1]; c[2] = r->p[2]; c[3] = 1.0f; *(uint8_t*)(s + 0x10C0) = 1;
        if (stream) ((void (*)(uint64_t, uint64_t))(B + Grva(0x3A083C)))(s + 0x500, s + 0x10B0);
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("XFORCE centre: exception %08lX", GetExceptionCode()); }
}
static int InStageSet(uint64_t sub, uint32_t id) {
    uint64_t n = *(uint64_t*)(sub + 0x60);
    for (uint64_t i = 0; i < n && i < 256; ++i) { uint64_t e = ((uint64_t (*)(uint64_t, uint64_t))(B + Grva(0xD3B9C)))(sub + 0x40, i); if (e && (*(uint32_t*)e >> 4) == id) return 1; }
    return 0;
}
static __attribute__((noinline)) void XwForceCells(ArenaRow* r, DWORD now) {  // the stage holds the target world now (any init state): centre + direct admission of the arena cells
    uint64_t s = StageMgr(); if (!s) return;
    uint64_t sub = s + 0x500; uint32_t ids[32]; int n = 0;
    for (int i = 0; i < r->ncells && n < 32; ++i) ids[n++] = r->cells[i];
    if (!n) for (unsigned k = 0; k < sizeof kForceCells / sizeof *kForceCells; ++k) if (kForceCells[k].mission == r->mission) for (int i = 0; i < 8 && kForceCells[k].cells[i]; ++i) ids[n++] = kForceCells[k].cells[i];
    XwCentre(r, 1);
    char buf[400]; int k = 0, ok = 0;
    __try {
        for (int i = 0; i < n; ++i) {
            if (InStageSet(sub, ids[i])) { k += snprintf(buf + k, sizeof buf - k, "%u:in ", ids[i]); continue; }
            uint64_t rec = ((uint64_t (*)(uint64_t, uint32_t))(B + Grva(0xDD570)))(*(uint64_t*)(*(uint64_t*)(B + Grva(0x45B9E30)) + 0xE0), ids[i]);
            if (!rec) { k += snprintf(buf + k, sizeof buf - k, "%u:norec ", ids[i]); continue; }
            uint8_t w = *(uint8_t*)(rec + 0xD4);
            uint8_t res = ((uint8_t (*)(uint64_t, uint32_t, uint8_t, void*, uint8_t))(B + Grva(0x3A0534)))(sub, ids[i], 1, NULL, w);
            ok += res; k += snprintf(buf + k, sizeof buf - k, "%u:w%02X->%u ", ids[i], w, res);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("XFORCE %s: exception %08lX", r->id, GetExceptionCode()); }
    buf[k] = 0;
    Log("XFORCE %s +%lu ms: world 0x%02X stage %d seq %d: centre -> %.0f %.0f %.0f, %d/%d arena cells admitted [%s]", r->id, now - g_xw.t0, CurWorld(), StageState(), (int)SeqState(), r->p[0], r->p[1], r->p[2], ok, n, buf);
    StageLog("XFORCE stage");
}
static void XwPreload(ArenaRow* r, const char* when) {
    uint64_t e = ResEntry(r->chr);
    if (!e || !*(uint8_t*)(e + 0x2c8)) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x3F9E58)))(r->chr, 2, 0); PoolAdd(r->chr); Log("XWORLD %s: boss resources (char %u) requested %s", r->id, r->chr, when); }
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
static __attribute__((noinline)) uint8_t h_cellreq(uint64_t sub, uint32_t id, uint8_t defer, void* p, uint8_t flag) {
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
    ((ReqFn)(o_req ? (void*)o_req : (void*)(B + Grva(0x2239600))))(r->mission, 1, r->start, p);
}
static int XwStart(ArenaRow* r, int claimed) {
    const uint8_t* st = GameState();
    if (!st || st[2] || !PlayerChar() || StageState() != 5) return 0;
    uint64_t mm = 0, ms = 0; if (!Rd64(B + Grva(0x47514D0), &mm) || !mm || !Rd64(mm + 8, &ms) || !ms) return 0;
    __attribute__((aligned(16))) uint8_t prm[32] = {0};
    ((void (*)(void*))(B + Grva(0x1BF7D68)))(prm);
    prm[7] = 0; prm[0xA] = 0; prm[0xE] = 1;  // WarpOtherWorld, mission 0 = stay in the running mission
    ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x3B28FC)))(2, 0, 0);
    uint8_t journey = *(uint8_t*)(ms + 0x10); uint32_t mode = *(uint32_t*)(ms + 0x24);
    uint8_t oW = (uint8_t)CurWorld(); uint32_t oS = *(uint32_t*)(B + Grva(0x45BC42C)); float oP[4] = {0}; PlayerPos(oP);
    if (!g_rh.active) HomeCapture(oW, oP, claimed ? "cross-world Fight now" : "prewarm");  // #6779
    memset(&g_xw, 0, sizeof g_xw); g_xw.oWorld = oW; g_xw.oJourney = journey; g_xw.oStart = oS; g_xw.oMode = mode; memcpy(g_xw.oPos, oP, sizeof oP); g_xw.row = r; g_xw.t0 = GetTickCount(); g_xw.active = 1; g_xw.claimed = claimed; g_xw.tClaim = claimed ? g_xw.t0 : 0; g_xw_last_mission = (int)r->mission;
    CellSkipOpen(r);  // before the request: it may already ask for cells synchronously
    Log("XWORLD %s (%s)%s: world 0x%02X -> 0x%02X, WarpOtherWorld start %u journey %u mode %u requested", r->id, r->name, claimed ? "" : " PREWARM (menu opened, not claimed yet)", CurWorld(), r->world, r->start, journey, mode);
    g_ps_row = r; XsStart(r->id, r->world); g_ps_row = 0;
    ((void (*)(uint8_t, uint8_t, uint32_t, uint32_t, void*))(B + Grva(0x22399B8)))(r->world, journey, r->start, mode, prm);
    if (g_cfg_xforce && claimed) { XwCentre(r, 0); Log("XFORCE %s: stream centre -> arena point %.0f %.0f %.0f at the click (old world 0x%02X not waited on)", r->id, r->p[0], r->p[1], r->p[2], oW); }
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
// #9920 (user log 03:02:22-03:03:22): the Return was picked while the F9 debug freeze was on (stop counter G+0x590+12, time scale 0 since 02:58:21).
// The WarpOtherWorld request only ran once the freeze was released (03:04:24, LoadStats #3 03:04:25-03:04:52), but this put-back gave up after 60 s of
// wall-clock time (03:03:22), so the player was left at the world's start point (next Fight now stored 139622 24414 207550, not 153478 21792 201895).
// Now: (1) the clock only runs while game time runs (time scale > 0); (2) after XrFixMs (60 s) of running time it does not give up but WATCHES for up to
// XrWatchMs (10 min, game running): as soon as the player is in the home world, the world is loaded and he is > 500 u from the stored spot, he is put
// back; within 500 u = already there, done; (3) a new Fight now / training session cancels the watch; (4) the frozen/paused time is logged.
static DWORD XrRunMs(DWORD now) {
    float ts = DbgTimeScale(); DWORD dt = g_xr.tLast ? now - g_xr.tLast : 0; g_xr.tLast = now; if (dt > 1000) dt = 1000;
    if (ts == 0.0f || g_dbg_frozen) { g_xr.paused += dt; if (!g_xr.pauseLogged) { g_xr.pauseLogged = 1; Log("XWORLD return: game time stopped (time scale %.3f, F9 freeze %ld): the return clock waits", ts, g_dbg_frozen); } }
    else { g_xr.run += dt; if (g_xr.pauseLogged) { g_xr.pauseLogged = 0; Log("XWORLD return: game time runs again (%lu ms stopped so far)", g_xr.paused); } }
    return g_xr.run;
}
// xworld-return-1790 (ally): the event-scene manager [45CE800] has 9 slots (+0x40, stride 0x308, scene id at +0x4C). Mission-sequencer state 9 (223C684) waits
// until 282B5C(0) finds no slot whose scene is flagged load-blocking. Scenes the arena left running (Yamagata: 0x21D1, 0xB20B, state 6) never end after the
// world trip, so a cross-world Return sat in state 9 with no player/HUD/control for 3-6 min (ally #59714). Proven live (#59906): zeroing those two ids let
// state 9 pass at once (9 -> 20, player + HUD + control back). EvSnap stores the ids before the WarpOtherWorld; XrTick clears only those ids, only while
// the sequencer has sat in state 9 for 1 s in the target world. New scenes of the home world (other ids) are never touched.
static __attribute__((noinline)) int EvSnap(uint32_t* ids) {
    uint64_t o = 0; int n = 0; memset(ids, 0, 9 * sizeof *ids);
    if (!Rd64(B + Grva(0x45CE800), &o) || !o) return 0;
    __try { for (int i = 0; i < 9; ++i) if ((ids[i] = *(volatile uint32_t*)(o + 0x40 + i * 0x308 + 0x4C))) ++n; } __except (EXCEPTION_EXECUTE_HANDLER) { memset(ids, 0, 9 * sizeof *ids); return 0; }
    return n;
}
static __attribute__((noinline)) int EvClearStale(uint32_t* ids, const char* tag) {
    uint64_t o = 0; int n = 0;
    if (!Rd64(B + Grva(0x45CE800), &o) || !o) return 0;
    __try {
        for (int i = 0; i < 9; ++i) {
            volatile uint32_t* p = (volatile uint32_t*)(o + 0x40 + i * 0x308 + 0x4C); uint32_t id = *p; if (!id) continue;
            for (int k = 0; k < 9; ++k) if (ids[k] == id) { Log("%s: event scene 0x%X (slot %d, state %u) left over from before the world trip holds the load (sequencer state 9): cleared", tag, id, i, *(volatile uint32_t*)(o + 0x40 + i * 0x308)); *p = 0; ids[k] = 0; ++n; break; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return n; }
    return n;
}
// Scenes that start after the request (the arena's scenes began during the fade in ally run #4) are added to the same list (union, max 9) every tick while still in another world.
static __attribute__((noinline)) uint32_t EvAccum(uint32_t* ids) {
    uint64_t o = 0; uint32_t added = 0; if (!Rd64(B + Grva(0x45CE800), &o) || !o) return 0;
    __try { for (int i = 0; i < 9; ++i) { uint32_t id = *(volatile uint32_t*)(o + 0x40 + i * 0x308 + 0x4C); if (!id) continue; int k, free_ = -1; for (k = 0; k < 9; ++k) { if (ids[k] == id) break; if (!ids[k] && free_ < 0) free_ = k; } if (k == 9 && free_ >= 0) { ids[free_] = id; added = id; } } } __except (EXCEPTION_EXECUTE_HANDLER) { }
    return added;
}
static void XrTick(void) {
    if (!g_xr.active) return;
    DWORD now = GetTickCount(); const uint8_t* st = GameState(); if (!st) return;
    DWORD run = XrRunMs(now);
    if (CurWorld() != g_xr.world) { uint32_t ea = EvAccum(g_xr.ev); if (ea) Log("XWORLD return: event scene 0x%X started after the Return request, tracked too", ea); }
    if (CurWorld() == g_xr.world && SeqState() == 9) { if (!g_xr.tSeq9) g_xr.tSeq9 = now; else if (now - g_xr.tSeq9 >= 1000) { EvClearStale(g_xr.ev, "XWORLD return"); g_xr.tSeq9 = now; } }  // xworld-return-1790
    else g_xr.tSeq9 = 0;
    if (g_xr.watch && (g_tr.active || (g_xw.active && g_xw.claimed))) { Log("XWORLD return watch: cancelled (a new Fight now / session started)"); g_xr.active = 0; return; }
    if (!g_xr.watch && run > 60000) {
        g_xr.watch = 1; g_xr.tSeen = 0;
        Log("XWORLD return: position fix not applied within 60 s of game time (+%lu ms wall, %lu ms stopped; world now 0x%02X, home 0x%02X, stage %d, load %d): watching up to 10 min, the player is put back once the home world is loaded",
            now - g_xr.t0, g_xr.paused, CurWorld(), g_xr.world, StageState(), st[2]);
    }
    if (g_xr.watch && run > 660000) { Log("XWORLD return watch: gave up after 10 min of game time (world 0x%02X, home 0x%02X)", CurWorld(), g_xr.world); g_xr.active = 0; return; }
    if (CurWorld() != g_xr.world || !PlayerChar() || StageState() != 5 || st[2]) { g_xr.tSeen = 0; return; }
    if (!g_xr.tSeen) { g_xr.tSeen = now; Log("XWORLD return: home world 0x%02X reached (+%lu ms wall, %lu ms game time, %lu ms stopped)%s", g_xr.world, now - g_xr.t0, run, g_xr.paused, g_xr.watch ? " [watch]" : ""); }
    if (now - g_xr.tSeen < 700) return;
    { uint64_t n, r, f, q, m; if (CellStats(&n, &r, &f, &q, &m) && n && r < n && now - g_xr.tSeen < 25000) { if (r != g_xr.lastR || !g_xr.tCell) { g_xr.lastR = r; g_xr.tCell = now; } if (now - g_xr.tCell < 4000) return; Log("XWORLD return: %llu/%llu cells loaded, no progress for 4 s: putting the player back now", (unsigned long long)r, (unsigned long long)n); } }  // xworld-return-1790: the far cells stalled (7/11 for minutes), the spot's cells are in  // bridge 6541: put the player back only once the old world's cells (collision) are loaded, not into a half-loaded world
    if (g_xr.watch) {
        float p[4]; if (PlayerPos(p)) { float dx = p[0] - g_xr.pos[0], dz = p[2] - g_xr.pos[2]; if (dx * dx + dz * dz < 500.0f * 500.0f) { Log("XWORLD return watch: player already at the spot (%.0f %.0f %.0f), done", p[0], p[1], p[2]); g_xr.active = 0; return; } }
    }
    g_xr.tSeen = 0; g_xr.active = 0;
    int w = Warp(g_xr.pos[0], g_xr.pos[1], g_xr.pos[2], g_xr.pos[3], 1);
    Log("XWORLD return: back in world 0x%02X +%lu ms (game time %lu ms, stopped %lu ms%s), player put back at %.0f %.0f %.0f -> %d", g_xr.world, now - g_xr.t0, run, g_xr.paused, g_xr.watch ? ", late fix from the watch" : "", g_xr.pos[0], g_xr.pos[1], g_xr.pos[2], w);
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
    uint64_t mm = 0, ms = 0; if (!Rd64(B + Grva(0x47514D0), &mm) || !mm || !Rd64(mm + 8, &ms) || !ms) return;
    __attribute__((aligned(16))) uint8_t prm[32] = {0};
    ((void (*)(void*))(B + Grva(0x1BF7D68)))(prm); prm[7] = 0; prm[0xA] = 0; prm[0xE] = 1;
    ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x3B28FC)))(2, 0, 0);
    memset(&g_xr, 0, sizeof g_xr); g_xr.world = g_xw.oWorld; g_xr.start = XwReturnStart(g_xw.oWorld, g_xw.oPos, g_xw.oStart); g_xr.t0 = now; memcpy(g_xr.pos, g_xw.oPos, sizeof g_xr.pos); g_xr.active = 1; { int ne = EvSnap(g_xr.ev); if (ne) Log("XWORLD cancel: %d event scene(s) running before the world trip (cleared if they hold the load)", ne); }
    Log("XWORLD cancel: prewarm landed in 0x%02X, returning to world 0x%02X start %u (WarpOtherWorld)", CurWorld(), g_xw.oWorld, g_xr.start);
    XwAbort("prewarm cancelled");
    g_ps_row = 0; XsStart("XCANCEL", g_xr.world); g_xs.maxMs = 180000;  // xworld-return-1790
    ((void (*)(uint8_t, uint8_t, uint32_t, uint32_t, void*))(B + Grva(0x22399B8)))(g_xr.world, g_xw.oJourney, g_xr.start, g_xw.oMode, prm);
}
static void XwTick(void) {
    XrTick();
    if (!g_xw.active) return;
    if (g_xw.cancel) { XwCancelTick(GetTickCount()); return; }
    DWORD now = GetTickCount(); ArenaRow* r = g_xw.row; const uint8_t* st = GameState(); if (!st) return;
    static DWORD lastLog; if (now - lastLog >= 2000) { lastLog = now; StageLog("XWORLD wait"); Log("XWORLD wait: GAMESTATE[2]=%u PlayerChar=%llX world 0x%02X (target 0x%02X) stage %d", st[2], (unsigned long long)PlayerChar(), CurWorld(), r->world, StageState()); }  // pc 5328: stage state 4->5 vs cell loading split
    if (g_cfg_xforce && g_xw.claimed && CurWorld() == r->world && g_xw.forced < 2) {
        int ss = StageState();
        if (!g_xw.forced) { g_xw.forced = 1; XwCentre(r, 0); Log("XFORCE %s +%lu ms: stage holds world 0x%02X (stage %d, seq %d), centre re-set", r->id, now - g_xw.t0, r->world, ss, (int)SeqState()); }
        if (ss >= 4) { g_xw.forced = 2; XwForceCells(r, now); }
    }
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
    g_pick = r; int ok = FightNowEntry(r->mission);  // #23451: this exact row (an S<chr> row shares its mission id)
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
// ally #8076/#8093: the settle check also waited for EVERY loaded cell's placeable loads (open-world streaming, 26-44 s on the Ally) while the boss was
// already live (7-30 s fighting in the dark). [Fade] AreaWaitMs (default 1500): once the boss is spawned, the player released and the stage ready, wait at
// most this long for the area objects, then fade in anyway (streaming pop-in at the edges is visible, the fight is not hidden). 0 = old behaviour (wait for all).
static int g_cfg_fade_areawait = 500;  // pc 05:29: 1500 left the boss live 2.2 s under black; 500 + the stable window counted from ready -> ~0.5 s
static volatile LONG g_fd_want, g_fd_alpha;   // want: overlay target (1 = black); alpha 0..255 as drawn
static struct { int pend, phase, inside, areaSkip, cross; uint32_t mission; DWORD tPend, tSwap, tStable, tWant, tBase; } g_fd;  // phase 2 = waiting for the world/swap then the settle check
static HWND g_fd_wnd, g_fd_game;
static DWORD g_fd_tPl;
static int g_fd_ok;  // overlay available: Nioh3EngineFix.asi export (D3D, drawn in the game's own frame) or the fallback GDI window
static int (*p_osdFade)(int, int), (*p_osdAlpha)(void);
// #8902/#9133 Return to where I was shows the Boss Rematch loading screen look (EngineFix export Nioh3EngineFix_LoadScreen) with the title 'Return'
// instead of a plain black screen. [Return] LoadScreen = 1 (default), 0 = black only. Missing export (older EngineFix) = black only.
static int (*p_osdMask)(int, int); static int g_cfg_gate_mask_r = 400, g_cfg_gate_mask_btn = 0xF000;  // #9324 [Return] ReturnGateMaskRadius / ReturnGateMaskButtons
static int (*p_osdLoad)(int, const char*, const char*); static int g_cfg_rh_ls = 1, g_rh_ls_on;
static void RhLoadScreen(int on) { if (!p_osdLoad || !g_cfg_rh_ls || on == g_rh_ls_on) return; g_rh_ls_on = on; int r = p_osdLoad(on, "Return", "Returning to where you were..."); Log("RETURN loading screen %s (EngineFix overlay %s)", on ? "on" : "off", r ? "drawing" : "menu off: black only"); }
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
    if (g_fd_want != on) { g_fd_want = on; if (on) g_fd_tPl = 0; g_fd.tWant = GetTickCount(); Log("FADE %s (%s), alpha %ld", on ? "to black" : "back in", why, FdAlpha()); if (p_osdFade) p_osdFade(on, on ? g_cfg_fade_out : g_cfg_fade_in); }
}
static int g_cfg_fade_pwait = -1;  // [Fade] PlaceableWaitMs (pc2 #6801): -1 = hold the cover until every placeable load finished (old); N >= 0 = give up waiting for them N ms after everything else is ready
static int FadeBaseReady(void) { return !(g_sp.active || g_vf.respawnAt || (g_ar.active && !g_ar.released) || StageState() != 5 || !PlayerChar()); }
static int FadeSettled(void) {
    if (!FadeBaseReady()) return 0;
    int b = 0, p = 0;
    if (AreaObjPending(&b, &p) && (b || p)) {
        const uint8_t* st = GameState();
        if (g_cfg_fade_pwait < 0 || !st || st[2]) { g_fd_tPl = 0; return 0; }
        DWORD now = GetTickCount(); if (!g_fd_tPl) g_fd_tPl = now;
        if (now - g_fd_tPl < (DWORD)g_cfg_fade_pwait) return 0;
    }
    return 1;
}
static int FadeInPlaceOk(uint32_t mission, ArenaRow* r) {  // same logic as FightNowImpl: will it swap here without a native load?
    if (!r) return 0;
    if (g_cfg_fn_summon) return 1;
    return !(g_cfg_native && NativeListed(mission)) && !SwapGate(r);
}
// NearArenaOnly window end: the swap has fired, the player is released and the fade-in is not pending any more (then the held placeables stream normally)
static void PsFarTick(void) {
    static DWORD tOk;
    if (!g_ps.on || !g_ps_far) { tOk = 0; return; }
    if (g_xw.active || (g_ar.active && !g_ar.released) || g_fd.phase == 2 || g_fd.pend) { tOk = 0; return; }
    DWORD now = GetTickCount(); if (!tOk) tOk = now;
    if (now - tOk >= 250) { tOk = 0; PsClose("player has control and the fade-in is done"); }
}
// #9133 boss hold (see g_cfg_bossgrace)
static struct { uint32_t h; uint64_t a; DWORD t0, tVis, tReapply; int phase, nOff, logged; } g_bh;  // phase 1 = waiting for the actor, 2 = AI held, 0 = idle
static void BhStart(uint32_t handle) {
    if (g_cfg_bossgrace < 0) return;
    memset(&g_bh, 0, sizeof g_bh); g_bh.h = handle; g_bh.t0 = GetTickCount(); g_bh.phase = 1;
    Log("BOSSHOLD boss 0x%X: AI held until the screen is visible + %d ms (BossGraceAfterCoverMs, max %d ms)", handle, g_cfg_bossgrace, g_cfg_bosshold_max);
}
static __attribute__((noinline)) int BhAiOn(uint64_t a) { uint64_t c = 0, ai = 0; uint32_t f = 0; if (!Rd64(a + 0x3B0, &c) || !c || !Rd64(c + 0x50, &ai) || !ai) return -1; __try { f = *(volatile uint32_t*)(ai + 0x1574); } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; } return f != 0; }
static int BhScreenVisible(int* why) {  // 1 = no load cover (GAMESTATE[2]), no native mission-load sequencer state, our fade overlay clear
    const uint8_t* gs = GameState(); uint32_t q = SeqState();
    if (!gs || gs[2]) { *why = 1; return 0; }
    if (q >= 1 && q <= 15) { *why = 2; return 0; }
    if (g_fd_ok && (g_fd_want || g_fd.pend || g_fd.phase == 2 || FdAlpha() > 24)) { *why = 3; return 0; }
    *why = 0; return 1;
}
static void BhTick(void) {
    if (!g_bh.phase) return;
    DWORD now = GetTickCount();
    if (g_cfg_bossgrace < 0) { if (g_bh.phase == 2 && g_bh.a) ((void (*)(uint64_t))(B + Grva(0x66982C)))(g_bh.a); Log("BOSSHOLD off (BossGraceAfterCoverMs < 0): boss AI back on"); g_bh.phase = 0; return; }
    if (g_bh.phase == 1) {
        uint64_t a = ActorByHandle(g_bh.h);
        if (!a) { if (now - g_bh.t0 > 5000) { Log("BOSSHOLD boss 0x%X: no actor after 5 s, nothing held", g_bh.h); g_bh.phase = 0; } return; }
        g_bh.a = a; g_bh.phase = 2; ((void (*)(uint64_t))(B + Grva(0x668D74)))(a); g_bh.nOff = 1; g_bh.tReapply = now;
        Log("BOSSHOLD boss 0x%X actor %p: AI off (+%lu ms after spawn)", g_bh.h, (void*)a, now - g_bh.t0);
        return;
    }
    uint64_t a = ActorByHandle(g_bh.h);
    if (!a || a != g_bh.a) { Log("BOSSHOLD boss 0x%X: actor gone / replaced, hold ends", g_bh.h); g_bh.phase = 0; return; }
    { int dead = 0; uint64_t hc = 0, hm = 0; if (ActorDeadNow(a, &dead, &hc, &hm) && dead) { g_bh.phase = 0; return; } }
    int why = 0, vis = BhScreenVisible(&why);
    if (!vis) g_bh.tVis = 0; else if (!g_bh.tVis) { g_bh.tVis = now; Log("BOSSHOLD boss 0x%X: screen visible +%lu ms after spawn (GAMESTATE[2] 0, seq %d, fade clear): AI on in %d ms", g_bh.h, now - g_bh.t0, (int)SeqState(), g_cfg_bossgrace); }
    int timeout = now - g_bh.t0 >= (DWORD)g_cfg_bosshold_max;
    if ((g_bh.tVis && now - g_bh.tVis >= (DWORD)g_cfg_bossgrace) || timeout) {
        g_bh.phase = 0;  // IhAdoptBossHold sees no foreign hold
        if (IhAdoptBossHold(g_bh.h, a)) { Log("BOSSHOLD boss 0x%X: +%lu ms grace over, AI stays off: Training Idle stance took the hold (#13167)", g_bh.h, now - g_bh.t0); return; }
        ((void (*)(uint64_t))(B + Grva(0x66982C)))(a);
        Log("BOSSHOLD boss 0x%X: AI on +%lu ms after spawn (%s; AI re-held %d times)", g_bh.h, now - g_bh.t0, timeout ? "BossHoldMaxMs TIMEOUT" : "grace over", g_bh.nOff - 1);
        return;
    }
    { static DWORD tl; if (now - tl >= 2000) { tl = now; Log("BOSSHOLD boss 0x%X: +%lu ms, waiting (%s), seq %d", g_bh.h, now - g_bh.t0, why == 1 ? "GAMESTATE[2] load cover" : why == 2 ? "mission load sequencer" : why == 3 ? "fade overlay" : "grace", (int)SeqState()); } }
    if (now - g_bh.tReapply >= 100 && BhAiOn(a) == 1) { g_bh.tReapply = now; ((void (*)(uint64_t))(B + Grva(0x668D74)))(a); if (++g_bh.nOff <= 10) Log("BOSSHOLD boss 0x%X: AI was switched back on by the game, off again (#%d)", g_bh.h, g_bh.nOff - 1); }
}
static void FadeTick(void) {
    if (!g_cfg_fade || !g_fd_ok) return;
    DWORD now = GetTickCount();
    if (g_fd.pend) {  // same-world swap waits for black (or 400 ms over the ramp if the overlay cannot draw)
        if (FdAlpha() < 255 && now - g_fd.tPend < (DWORD)g_cfg_fade_out + 400) return;
        uint32_t m = g_fd.mission; g_fd.pend = 0; g_fd.inside = 1;
        Log("FADE black (alpha %ld, %lu ms): swapping mission 0x%X", FdAlpha(), now - g_fd.tPend, m);
        int ok = FightNowImpl(m); g_fd.inside = 0;
        if (!ok) { __attribute__((aligned(16))) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0}; Log("FADE: swap refused, native request"); FadeWant(0, "swap refused"); ((ReqFn)(o_req ? (void*)o_req : (void*)(B + Grva(0x2239600))))(m, 1, 0, p); return; }
        g_fd.tBase = 0; g_fd.areaSkip = 0; g_fd.phase = 2; g_fd.tSwap = now; g_fd.tStable = 0; return;
    }
    if (g_fd.phase != 2) return;
    if (now - g_fd.tSwap > (DWORD)g_cfg_fade_max) { Log("FADE: not settled after %lu ms, releasing", now - g_fd.tSwap); g_fd.phase = 0; FadeWant(0, "max hold"); return; }
    if (g_xw.active) { g_fd.tStable = 0; if (!g_xw.claimed) { g_fd.phase = 0; FadeWant(0, "prewarm only"); } return; }  // cross-world still switching / waiting for the stage
    if (!FadeBaseReady()) { g_fd.tStable = 0; g_fd.tBase = 0; return; }  // boss spawned, player released, stage ready
    if (!g_fd.tBase) g_fd.tBase = now;
    int xw = g_fd.cross && g_cfg_fade_areawait_x > 0 && !g_ar.cross, xb = 0, xp = 0;  // pc 06:12: when the arena already held the spawn until clear, the boss spawn re-opens cells for ~2 s: use the AreaWaitMs rule then (boss was live 2.6 s under black)
    if (xw && !XArenaClear(&xb, &xp)) {  // cross-world: cells + game load cover, within AreaWaitCrossMs of the arena 'ready' (shared with the spawn hold)
        DWORD t = g_ar.tReady ? g_ar.tReady : g_fd.tBase;
        if (now - t < (DWORD)g_cfg_fade_areawait_x) { g_fd.tStable = 0; return; }
        if (!g_fd.areaSkip) { const uint8_t* gs = GameState(); Log("FADE: cross-world arena still not clear %lu ms after ready (%d cells blocking, %d placeable loads, GAMESTATE[2] %u): fading in anyway (AreaWaitCrossMs %d)", now - t, xb, xp, gs ? gs[2] : 0xFF, g_cfg_fade_areawait_x); g_fd.areaSkip = 1; g_fd.tStable = g_fd.tBase; }
    } else if (!xw && !FadeSettled()) {  // only the area objects (placeable streaming) still pending
        if (g_cfg_fade_areawait <= 0 || now - g_fd.tBase < (DWORD)g_cfg_fade_areawait) { g_fd.tStable = 0; return; }
        if (!g_fd.areaSkip) { int b = 0, p = 0; AreaObjPending(&b, &p); Log("FADE: boss live, area objects still streaming %lu ms after ready (%d cells blocking, %d placeable loads): not waiting for them (AreaWaitMs %d)", now - g_fd.tBase, b, p, g_cfg_fade_areawait); g_fd.areaSkip = 1; g_fd.tStable = g_fd.tBase; }  // the base state has been steady since tBase: count StableMs from there, not again after the wait
    }
    if (!g_fd.tStable) g_fd.tStable = now;
    if (now - g_fd.tStable < (DWORD)g_cfg_fade_stable || now - g_fd.tSwap < (DWORD)g_cfg_fade_min) return;
    Log("FADE: arena settled %lu ms after the swap (stable %lu ms, ready +%lu ms%s)", now - g_fd.tSwap, now - g_fd.tStable, g_fd.tBase - g_fd.tSwap, g_fd.areaSkip ? ", area streaming skipped" : "");
    g_fd.phase = 0; g_fd.areaSkip = 0; g_fd.tBase = 0; FadeWant(0, "settled");
}
N3T_API int Nioh3Training_FightNow(uint32_t mission) { g_pick = 0; return FightNowEntry(mission); }
// #23451: Fight now for a boss character (roster / Boss A-B menus): its mission row, else its S<chr> arena / field row. 1 = handled (in-place swap,
// cross-world switch or summon); 0 = no row / not handled: the caller must NOT issue a native mission request for it (a field row's mission id is
// an open-field region), summon it here instead (Nioh3Training_SummonSlotChr).
N3T_API int Nioh3Training_FightNowChr(uint32_t chr) {
    ArenaRow* r = FindRowChr(chr);
    if (!r) { Log("FIGHTNOW chr %u: no arena row", chr); return 0; }
    g_pick = r; Log("FIGHTNOW chr %u -> %s (%s) kind %s, mission 0x%X world 0x%02X", chr, r->id, r->name, r->kind, r->mission, r->world);
    return FightNowEntry(r->mission);
}
static int FightNowEntry(uint32_t mission) {
    if (g_xr.active && g_xr.watch) { g_xr.active = 0; Log("XWORLD return watch: cancelled by Fight now (the late put-back no longer applies)"); }  // #9920
    if (!g_xw_busy && !g_fd.inside && !g_xw.active && !g_xr.active && !g_rh.active) { float p[4] = {0}; if (PlayerPos(p)) HomeCapture(CurWorld(), p, "Fight now"); }  // #6779
    if (!g_cfg_fade || !g_fd_ok || g_xw_busy || g_fd.inside || !g_cfg_enable) return FightNowImpl(mission);
    ArenaRow* r = FindRow(mission);
    if (g_fd.pend) return 1;  // a switch is already waiting for black
    if (r && g_cfg_xworld && CurWorld() >= 0 && (CurWorld() != r->world || g_xw.active)) {  // cross-world (or claiming a prewarm)
        int ok = FightNowImpl(mission);
        if (ok && g_xw.active) { FadeWant(1, "cross-world switch"); g_fd.cross = 1; g_fd.tBase = 0; g_fd.areaSkip = 0; g_fd.phase = 2; g_fd.tSwap = GetTickCount(); g_fd.tStable = 0; }
        return ok;
    }
    if (!FadeInPlaceOk(mission, r)) return FightNowImpl(mission);
    g_fd.pend = 1; g_fd.cross = 0; g_fd.mission = mission; g_fd.tPend = GetTickCount(); g_fd.phase = 0; FadeWant(1, "same-world switch");
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
typedef struct { int loot, blow, cut; } DeathCfgSave;
static DWORD WINAPI DeathCfgSaveThread(LPVOID p) {
    DeathCfgSave saved=*(DeathCfgSave*)p; free(p);
    char ini[MAX_PATH], v[8]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    snprintf(v, sizeof v, "%d", saved.loot); WritePrivateProfileStringA("Summon", "BossLoot", v, ini);
    snprintf(v, sizeof v, "%d", saved.blow); WritePrivateProfileStringA("Summon", "DeathBlowRequired", v, ini);
    snprintf(v, sizeof v, "%d", saved.cut); WritePrivateProfileStringA("Summon", "CutsceneOnKill", v, ini);
    WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
static DWORD WINAPI AutoRetrySaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH], v[8]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    snprintf(v, sizeof v, "%d", g_cfg_autoretry); WritePrivateProfileStringA("Summon", "AutoRetryOnDeath", v, ini); WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
static DWORD WINAPI CantDieSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    WritePrivateProfileStringA("Summon", "PlayerCantDie", g_cfg_cantdie ? "1" : "0", ini); WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
// user #14150: 'Player can't die' menu toggle (see g_cfg_cantdie). Returns the new value; persist = save [Summon] PlayerCantDie.
N3T_API int Nioh3Training_GetPlayerCantDie(void) { return g_cfg_cantdie; }
N3T_API int Nioh3Training_SetPlayerCantDie(int on, int persist) {
    g_cfg_cantdie = on != 0;
    Log("API Player can't die %s%s", g_cfg_cantdie ? "ON (infinite health: HP refilled whenever it drops)" : "OFF (normal damage; HP at the floor = instant respawn in the arena, round reset)", persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, CantDieSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
    return g_cfg_cantdie;
}
N3T_API int Nioh3Training_GetAutoRetry(void) { return g_cfg_autoretry; }
N3T_API void Nioh3Training_SetAutoRetry(int on, int persist) {
    // user #13229/#13230: AutoRetryOnDeath is forced on (dying never removes the boss / sends the player to the Return gate); a request for 0 is ignored (1 is saved).
    g_cfg_autoretry = 1; Log("API AutoRetryOnDeath request %d ignored: always 1 (#13229)%s", on, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, AutoRetrySaveThread, 0, 0, 0); if (t) CloseHandle(t); }
}
// #7465 Boss Rematch menu row 'No finisher (Ki break)': KiBreakNoFinisher live + saved. The hooks are installed by NfTick on the game thread.
static DWORD WINAPI NoFinSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH], v[8]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    snprintf(v, sizeof v, "%d", g_cfg_nofin); WritePrivateProfileStringA("Summon", "KiBreakNoFinisher", v, ini); WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
N3T_API int Nioh3Training_GetNoFinisher(void) { return g_cfg_nofin; }
N3T_API void Nioh3Training_SetNoFinisher(int on, int persist) {
    g_cfg_nofin = on != 0; if (g_bmd_on) g_bmd_nofin = g_cfg_nofin; Log("API KiBreakNoFinisher %d (Boss Rematch menu)%s", g_cfg_nofin, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, NoFinSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
}
N3T_API void Nioh3Training_GetDeathCfg(int* loot, int* blow, int* cut) { if (loot) *loot = g_cfg_loot; if (blow) *blow = g_cfg_deathblow; if (cut) *cut = g_cfg_cutscene; }
N3T_API void Nioh3Training_SetDeathCfg(int loot, int blow, int cut, int persist) {
    if (loot >= 0) { g_cfg_loot = loot != 0; if (g_bmd_on) g_bmd_loot = g_cfg_loot; } if (blow >= 0) g_cfg_deathblow = blow != 0; if (cut >= 0) g_cfg_cutscene = cut != 0;
    Log("API deathcfg (Boss Rematch menu, #6618): BossLoot %d DeathBlowRequired %d CutsceneOnKill %d%s", g_cfg_loot, g_cfg_deathblow, g_cfg_cutscene, persist ? " (saved to ini)" : "");
    if (persist) {
        DeathCfgSave* saved=(DeathCfgSave*)malloc(sizeof *saved);
        if (saved) {
            saved->loot=g_bmd_on ? g_bmd_loot : g_cfg_loot;
            saved->blow=g_cfg_deathblow; saved->cut=g_cfg_cutscene;
            HANDLE t=CreateThread(0,0,DeathCfgSaveThread,saved,0,0);
            if(t)CloseHandle(t); else free(saved);
        }
    }
}
// user #9585 (+ NEXT BUILD list item 1): Boss Rematch menu sliders. which 0 = [Summon] FinisherKillTimeoutMs (CutsceneOnKill finisher window, read live
// by BossTick), 1 = [Summon] KiBreakHp0WindowMs (HP-0 no-hit window, read live by NfTick). Only the lengths change; the DeathBlowRequired /
// CutsceneOnKill / BossLoot toggles and their behaviour are untouched. Get returns -1 for an unknown id. Set clamps, persist 1 = saved to the ini.
static void TuneRange(int which, int* lo, int* hi) { if (which == 0) { *lo = 500; *hi = 30000; } else { *lo = 200; *hi = 5000; } }
static DWORD WINAPI TuneSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH], v[16]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    snprintf(v, sizeof v, "%d", g_cfg_fin_timeout); WritePrivateProfileStringA("Summon", "FinisherKillTimeoutMs", v, ini);
    snprintf(v, sizeof v, "%d", g_cfg_nofin_ms); WritePrivateProfileStringA("Summon", "KiBreakHp0WindowMs", v, ini);
    WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
N3T_API int Nioh3Training_GetTuning(int which) { return which == 0 ? g_cfg_fin_timeout : which == 1 ? g_cfg_nofin_ms : -1; }
N3T_API int Nioh3Training_SetTuning(int which, int value, int persist) {
    if (which != 0 && which != 1) return -1;
    int lo, hi; TuneRange(which, &lo, &hi); if (value < lo) value = lo; if (value > hi) value = hi;
    if (which == 0) g_cfg_fin_timeout = value; else g_cfg_nofin_ms = value;
    Log("API tuning (Boss Rematch menu slider, #9585): %s %d ms%s", which == 0 ? "FinisherKillTimeoutMs" : "KiBreakHp0WindowMs", value, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, TuneSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
    return value;
}
// user #7118: legacy aliases of Get/SetAutoRetry (jumpfix 'Instant respawn on death'); same flag, persisted as [Summon] AutoRetryOnDeath.
N3T_API int Nioh3Training_GetInstantRespawn(void) { return Nioh3Training_GetAutoRetry(); }
N3T_API void Nioh3Training_SetInstantRespawn(int on, int persist) { Nioh3Training_SetAutoRetry(on, persist); }
// user #6779: 'Return to where I was' (Boss Rematch menu): ends the arena session (boss, boss mode, BGM, bar, NoDeath) and puts the player back at g_home, after a kill,
// with no boss or mid-swap. A running cross-world swap / prewarm is cancelled through the bridge 6541/6550 abort path (XwCancel -> XwCancelTick WarpOtherWorld back
// -> XrTick puts the player on the spot), aimed at g_home; another world without a swap running uses the same WarpOtherWorld trip; the same world is a plain warp.
// Fade to black around it like Fight now. Game thread (pump); the export only queues.
static void RhDone(const char* why) { Log("RETURN done (%s) +%lu ms", why, GetTickCount() - g_rh.t0); g_rh.active = 0; g_rh.phase = 0; RhLoadScreen(0); FadeWant(0, "return to where I was"); }
static void RhTick(void) {
    DWORD now = GetTickCount();
    if (InterlockedExchange(&g_rh.want, 0) && !g_rh.active) {
        g_rh.active = 1; g_rh.phase = 1; g_rh.t0 = g_rh.tPh = now; g_rh.tStable = 0; g_rh.arena = g_tr.active && g_tr.arena;  // phase 1 ends the session, so remember an arena session here
        Log("RETURN requested (Boss Rematch menu, #6779): home world 0x%02X %.0f %.0f %.0f mission 0x%X; now world 0x%02X, swap %d (claimed %d), switch pending %d, session %d (round %d, beaten boss %d)",
            g_home.world, g_home.pos[0], g_home.pos[1], g_home.pos[2], g_home.mission, CurWorld(), g_xw.active, g_xw.claimed, g_fd.pend, g_tr.active, g_tr.round, g_tr.tWin != 0);
        g_fd.pend = 0; g_fd.phase = 0;  // a same-world switch still waiting for black is dropped
        // #9920: a Return picked while the F9 freeze holds game time never runs the world switch (it ran only after F9 was released, 2 min later): release it first
        if (g_dbg_frozen) { DbgFreeze(0); Log("RETURN: F9 debug freeze released first (game time was stopped: time scale now %.3f)", DbgTimeScale()); }
        if (g_menu_frozen) MenuFreeze(0, "Return to where I was");  // user #13274: the menu pause never holds a return
        if (g_fz_hold) FzHoldSet(0, "Return to where I was");  // user #22589: the Practice freeze never holds a return
        if (g_xr.active) { Log("RETURN: an older return put-back (watch %d) dropped for this return", g_xr.watch); g_xr.active = 0; }
        if (g_xw.active) {
            g_xw.oWorld = g_home.world; g_xw.oJourney = g_home.journey; g_xw.oMode = g_home.mode; g_xw.oStart = g_home.start; memcpy(g_xw.oPos, g_home.pos, sizeof g_xw.oPos);
            g_xw.claimed = 0; g_xw.cancel = 0; XwCancel("Return to where I was");
        }
        if (g_home.valid) { FadeWant(1, "return to where I was"); RhLoadScreen(1); }
    }
    if (!g_rh.active) return;
    if (now - g_rh.t0 > (g_rh.phase == 3 ? 200000u : 90000u)) { Log("RETURN: not finished after 90 s (phase %d), giving up", g_rh.phase); RhDone("timeout"); return; }
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
        // xworld-return-1790: inside a cross-world Fight now arena (no mission load) the mission reads 0: that is the arena, not a mission exit
        if (g_home.mission && MsRead(&mis, &jr, &mode) && mis != g_home.mission && !(mis == 0 && g_rh.arena && CurWorld() != g_home.world)) { Log("RETURN: the mission changed 0x%X -> 0x%X since Fight now: not moved (the game's own mission exit applies)", g_home.mission, mis); g_home.valid = 0; RhDone("mission changed"); return; }
        if (g_home.mission && CurWorld() != g_home.world) { g_homeKeep.valid = 1; g_homeKeep.world = g_home.world; g_homeKeep.journey = g_home.journey; g_homeKeep.mode = g_home.mode; g_homeKeep.mission = g_home.mission; }
        g_home.valid = 0;
        if (CurWorld() == g_home.world) {
            int w = Warp(g_home.pos[0], g_home.pos[1], g_home.pos[2], g_home.pos[3], 1);
            Log("RETURN: same world 0x%02X, player put back at %.0f %.0f %.0f -> %d", g_home.world, g_home.pos[0], g_home.pos[1], g_home.pos[2], w);
            g_rh.phase = 4; g_rh.tPh = now; return;
        }
        uint64_t mm = 0, ms = 0; if (!Rd64(B + Grva(0x47514D0), &mm) || !mm || !Rd64(mm + 8, &ms) || !ms) { Log("RETURN: mission state unreadable, world 0x%02X not left", CurWorld()); RhDone("mission state unreadable"); return; }
        __attribute__((aligned(16))) uint8_t prm[32] = {0};
        ((void (*)(void*))(B + Grva(0x1BF7D68)))(prm); prm[7] = 0; prm[0xA] = 0; prm[0xE] = 1;  // as XwCancelTick
        ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x3B28FC)))(2, 0, 0);
        memset(&g_xr, 0, sizeof g_xr); g_xr.world = g_home.world; g_xr.start = XwReturnStart(g_home.world, g_home.pos, g_home.start); g_xr.t0 = now; memcpy(g_xr.pos, g_home.pos, sizeof g_xr.pos); g_xr.active = 1; { int ne = EvSnap(g_xr.ev); if (ne) Log("RETURN: %d event scene(s) running before the world trip (cleared if they hold the load)", ne); }
        Log("RETURN: world 0x%02X -> home world 0x%02X start %u (WarpOtherWorld), put back on the spot once its cells are loaded", CurWorld(), g_home.world, g_xr.start);
        g_ps_row = 0; XsStart("RETURN", g_home.world); g_xs.maxMs = 180000;  // xworld-return-1790: same streaming relief as Fight now (area-objects gate, cell cap); without it the home world needs minutes (load flag stays set)
        ((void (*)(uint8_t, uint8_t, uint32_t, uint32_t, void*))(B + Grva(0x22399B8)))(g_xr.world, g_home.journey, g_xr.start, g_home.mode, prm);
        g_rh.phase = 3; return;
    }
    if (g_rh.phase == 3) {
        if (g_xr.active && now - g_rh.tPh < 190000u) return;  // xworld-return-1790: keep black until the home world is in and the player is put back (was lifted at 60 s into a still-loading world)
        if (g_xr.active) Log("RETURN: position fix still pending after 60 s of game time: loading screen lifted, the put-back watch keeps running");
        g_rh.phase = 4; g_rh.tPh = now;
    }
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
// user 20261018: a save made inside a Fight-now arena reloads the player IN the arena with no mission (mission 0, open world) and the native area script
// starts BossBattleMode with no boss: no exit, no fast travel, no return record. 'Leave arena': on the game thread end any arena session, end the native
// boss mode (BossBattleModeEnd steps, as 'bbmend') so the map fast travel / shrine works again, and run 'Return to where I was' if a spot is stored.
// 1 = queued, -1 = already queued.
static volatile LONG g_leave;
static void LeaveTick(void) {
    if (!InterlockedExchange(&g_leave, 0)) return;
    uint64_t cm = CharMgr(), c0 = 0; int bbm = cm && Rd64(cm, &c0) && c0 && *(uint8_t*)(c0 + 0x26AA);
    uint32_t mis = 0, mode = 0; uint8_t jr = 0; MsRead(&mis, &jr, &mode);
    Log("LEAVE ARENA requested: mission 0x%X world 0x%02X, native boss mode %d, session %d, stored spot %d", mis, CurWorld(), bbm, g_tr.active, g_home.valid);
    TrainStop("leave arena"); g_sp.active = 0; g_vf.active = 0; g_vf.respawnAt = 0; g_ar.active = 0; g_en.active = 0;
    BgmEnd(); BossModeEnd("leave arena"); g_bm.active = 0; FakeGaugeHide("leave arena");
    if (g_home.valid && !g_rh.active) InterlockedExchange(&g_rh.want, 1);
    c0 = 0; Log("LEAVE ARENA done: native boss mode now %d; %s", cm && Rd64(cm, &c0) && c0 ? *(uint8_t*)(c0 + 0x26AA) : -1, g_home.valid ? "returning to the stored spot" : "no stored spot: use the map / a shrine to travel");
}
N3T_API int Nioh3Training_LeaveArena(void) { return InterlockedExchange(&g_leave, 1) ? -1 : 1; }
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
// user #13255 [Return] ReturnGateAlways (default 1): the gate is spawned at session start (round-1 pose stored, the round-1 boss alive for 3 s, so the
// gadget is never created before the boss exists: #7138) instead of only once the session goes idle; same spot/ground logic. While a boss is alive the
// native interact (gadget prompt / end-mission box, both already swallowed) opens OUR gate page instead of nothing. The idle-only parts stay idle-only:
// no face-button mask, no raw pad A and no stand-on open during a fight, so fight inputs are untouched. 0 = old (spawn when idle, inactive in a fight).
static int g_cfg_gate_always = 1;
static struct { int up, tried, left; uint32_t id, mission; float pos[4]; DWORD tTry, tIn; int session; } g_gate;
static uint8_t g_gate_fakerec[0x100];
typedef void (*ReqRetFn)(const uint32_t*, uint8_t);
static ReqRetFn o_reqret;
static __attribute__((noinline)) int GateRecType(uint64_t rec) { __try { return rec ? *(int32_t*)(rec + 8) : -1; } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; } }
static uint64_t GateRecord(uint32_t id) {
    uint64_t set = ((uint64_t (*)(uint32_t))(B + Grva(0x7D4480)))(id); if (!set) return 0;
    return ((uint64_t (*)(uint64_t, uint32_t))(B + Grva(0x7D41E0)))(set, id);
}
// scan the default gadget placement set for any 0x353 record: set G: [[G]+0x488]+0x418 = header (count +4, records +8, 0x6C B each), id map G[3] {+4 empty key, +8..+10 (id, idx) pairs}
static __attribute__((noinline)) uint64_t GateScan(uint32_t* outId) {
    __try {
        uint64_t g = ((uint64_t (*)(uint32_t))(B + Grva(0x7D4480)))(0x7FFFFFF1u); if (!g) return 0;
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
// user #9330/#9543: the gate floated in the air. Its y was the stored round-1 player pose (g_tr.pp, log 00:28:59: 15111), taken right after the arena
// teleport while the player still drops ~1 m to the floor (standing y 15000-15016 in BOSSMON/SPAWNPOS of the same session). No native raycast is known
// (TECH_DEBT), so the floor height comes from where the player actually stood: the trail (0.5 s feet samples, last 20 s). Samples within 500 u of the
// gate x/z (|dy| < 300 from the old y): lowest of the 3 nearest (a jump/dodge sample is higher, never lower). Else the median of all recent samples
// within |dy| < 300 (flat arena floor around the fight). Else the player's current y if within 300. Else the old y.
// #13529 (user log 11:48:10, Yoritomo cross-world Fight now, ReturnGateAlways): the gate floated ~95 u again ('y 15111 -> 15111, trail near the gate',
// player standing at 15016). The cross-world arena hold kept the player frozen at the warp pose (y 15111) for ~9 s before control, and those trail
// samples have the SAME x/z as the later standing ones (the player had not moved yet), so the 'nearest 3' pick tied and kept the oldest (frozen) ones.
// Now: the LOWEST sample within 500 u of the gate x/z, and the player's current feet when near (a held warp pose, a jump or a dodge is higher, never
// lower). Else the median of the recent samples, else the player's y, else the old y. All within |dy| < 300 of the old y.
static float GateGroundY(float x, float z, float y0, const char** src) {
    DWORD now = GetTickCount(); float low = 1e9f, all[TRAILN]; int na = 0, nn = 0;
    for (int i = 0; i < g_trN; ++i) {
        if (now - g_trail[i].t > 30000 || fabsf(g_trail[i].p[1] - y0) > 300.0f) continue;
        all[na++] = g_trail[i].p[1];
        float ddx = g_trail[i].p[0] - x, ddz = g_trail[i].p[2] - z; if (ddx * ddx + ddz * ddz > 500.0f * 500.0f) continue;
        ++nn; if (g_trail[i].p[1] < low) low = g_trail[i].p[1];
    }
    float pp[4]; if (PlayerPos(pp) && fabsf(pp[1] - y0) < 300.0f) { float ddx = pp[0] - x, ddz = pp[2] - z; if (ddx * ddx + ddz * ddz <= 500.0f * 500.0f && pp[1] < low) { low = pp[1]; ++nn; } }
    if (nn) { *src = "lowest trail sample near the gate"; return low; }
    if (na > 0) { for (int i = 1; i < na; ++i) { float v = all[i]; int j = i - 1; while (j >= 0 && all[j] > v) { all[j + 1] = all[j]; --j; } all[j + 1] = v; } *src = "trail median"; return all[na / 2]; }
    if (PlayerPos(pp) && fabsf(pp[1] - y0) < 300.0f) { *src = "player now"; return pp[1]; }
    *src = "round-1 pose (no floor sample)"; return y0;
}
static int g_cfg_gate_ground = 1;  // [Return] ReturnGateGround (default 1): gate y = floor from the player trail (0 = old: round-1 pose y)
static __attribute__((noinline)) int GateSpawn(const char* why) {
    float pp[4] = {0}; if (!PlayerPos(pp)) return 0;
    float base[3] = {pp[0], pp[1], pp[2]}, dx = 1.0f, dz = 0.0f;
    if (g_tr.hasStart) { base[0] = g_tr.pp[0]; base[1] = g_tr.pp[1]; base[2] = g_tr.pp[2]; float vx = g_tr.pp[0] - g_tr.bp[0], vz = g_tr.pp[2] - g_tr.bp[2], l = sqrtf(vx * vx + vz * vz); if (l > 1.0f) { dx = vx / l; dz = vz / l; } }
    __attribute__((aligned(16))) float pos[4] = {base[0] + dx * g_cfg_gate_dist, base[1], base[2] + dz * g_cfg_gate_dist, 1.0f};
    if (g_cfg_gate_ground) { const char* src = "?"; float gy = GateGroundY(pos[0], pos[2], base[1], &src);
        Log("GATE ground (#9543): y %.0f -> %.0f (%s; %d trail samples, player now y %.0f)", base[1], gy, src, g_trN, pp[1]); pos[1] = gy; }
    __attribute__((aligned(16))) float mat[16]; memcpy(mat, ((const float* (*)(void))(B + Grva(0x20F030)))(), sizeof mat);
    uint32_t mis = 0, mode = 0; uint8_t jr = 0; MsRead(&mis, &jr, &mode);
    uint64_t rec = 0; uint32_t id = 0; char ids[64]; snprintf(ids, sizeof ids, "%s", g_gate_ids);
    for (char* s = strtok(ids, ", "); s && !rec; s = strtok(0, ", ")) { uint32_t c = (uint32_t)strtoul(s, 0, 0); uint64_t r = 0; __try { r = GateRecord(c); } __except (EXCEPTION_EXECUTE_HANDLER) { r = 0; } int t = GateRecType(r); Log("GATE: candidate id %u -> record %p type 0x%X", c, (void*)r, t); if (t == 0x353) { rec = r; id = c; } }
    if (!rec) rec = GateScan(&id);
    if (!rec && g_cfg_gate_fake) {  // no clear gadget record loaded here: synthesize one (type 0x353, all else zero; risky, ReturnGateFakeRecord = 0 turns it off)
        memset(g_gate_fakerec, 0, sizeof g_gate_fakerec); *(int32_t*)(g_gate_fakerec + 8) = 0x353; id = 0x7FFF0353u; rec = (uint64_t)g_gate_fakerec;
        Log("GATE: no clear gadget record in the loaded placement sets, using a synthesized 0x353 record (id 0x%X)", id);
    }
    if (!rec) { Log("GATE (%s): no clear gadget record, nothing spawned", why); return 0; }
    __try { ((void (*)(uint64_t, uint32_t, uint32_t, float*, float*, uint8_t))(B + Grva(0x7D39E8)))(rec, id, mis, pos, mat, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) { Log("GATE (%s): GadgetCreateFromRecord 7D39E8 faulted %08lX", why, GetExceptionCode()); return 0; }
    g_gate.up = 1; g_gate.id = id; g_gate.mission = mis; memcpy(g_gate.pos, pos, sizeof g_gate.pos); g_gate.left = 0; g_gate.tIn = 0;
    Log("GATE (%s): clear gadget id %u spawned at %.0f %.0f %.0f (mission 0x%X, record %p): interact (or stand on it %d ms) = Return to where I was", why, id, pos[0], pos[1], pos[2], mis, (void*)rec, g_cfg_gate_touch);
    return 1;
}
static int GateBossAlive(void) { int f = Nioh3Training_ReturnInfo(); return !(f & 2) && !(f & 4); }
N3T_API int Nioh3Training_GetIdleHold(void);  // idle_hold.h (included further down)
// gate used: Boss Rematch opens its 2-option page (Return to where I was / Fight again); without it (old Rematch, no menu) return directly. 2 = choice shown.
static volatile LONG g_gate_again; static volatile DWORD g_gate_tUse;  // #10022: last time our gate page was requested (dialog hook window)
static int GateUse(void) {
    g_gate_tUse = GetTickCount();
    HMODULE rm = GetModuleHandleA("Nioh3BossRematch.asi");
    int (*choice)(void) = rm ? (int (*)(void))GetProcAddress(rm, "Nioh3BossRematch_GateChoice") : 0;
    if (choice && choice()) return 2;
    Log("GATE: Boss Rematch gate choice not available, returning directly"); return Nioh3Training_ReturnHome();
}
// #9664/#9670 [Return] ReturnGateOpenOnA (default 1): raw pad A (XInputGetState read directly from xinput1_4, not through the game's hooked IAT
// slot, so the EngineFix mask does not hide it from us) near the gate opens our page. GateMenuOpen: EngineFix_MenuOpen (0 when the export is missing).
static int g_cfg_gate_opena = 1;
static int GatePadA(void) {
    typedef DWORD (WINAPI *XgsFn)(DWORD, void*); static XgsFn xgs; static int tried;
    if (!xgs && !tried) { tried = 1; HMODULE x = LoadLibraryA("xinput1_4.dll"); if (!x) x = LoadLibraryA("xinput1_3.dll"); xgs = x ? (XgsFn)GetProcAddress(x, "XInputGetState") : 0; }
    if (!xgs) return 0;
    for (DWORD i = 0; i < 4; ++i) { uint8_t st[16] = {0}; if (xgs(i, st) == 0 && (*(uint16_t*)(st + 4) & 0x1000)) return 1; }  // XINPUT_STATE: dwPacketNumber, Gamepad.wButtons; A = 0x1000
    return 0;
}
static int GateMenuOpen(void) {
    static int (*mo)(void); static int tried; if (!mo && !tried) { tried = 1; HMODULE ef = GetModuleHandleA("Nioh3EngineFix.asi"); mo = ef ? (int (*)(void))GetProcAddress(ef, "Nioh3EngineFix_MenuOpen") : 0; }
    return mo ? mo() : 0;
}
#include "gate_zone.h"
// user #13796 [Return] ReturnGateZone (1), ReturnGateOpenButtons (0x1000 = A), ReturnGateTapMs (350): see gate_zone.h
static int g_cfg_gate_zone = 1, g_cfg_gate_open_btn = 0x1000, g_cfg_gate_tap_ms = 350;
static int GatePadButtons(void) {  // raw XInput (xinput1_4 directly, not the game's IAT slot: the EngineFix mask does not hide it from us)
    typedef DWORD (WINAPI *XgsFn)(DWORD, void*); static XgsFn xgs; static int tried;
    if (!xgs && !tried) { tried = 1; HMODULE x = LoadLibraryA("xinput1_4.dll"); if (!x) x = LoadLibraryA("xinput1_3.dll"); xgs = x ? (XgsFn)GetProcAddress(x, "XInputGetState") : 0; }
    if (!xgs) return 0;
    int b = 0; for (DWORD i = 0; i < 4; ++i) { uint8_t st[16] = {0}; if (xgs(i, st) == 0) b |= *(uint16_t*)(st + 4); }
    return b;
}
static void* GateEf(const char* name) { HMODULE ef = GetModuleHandleA("Nioh3EngineFix.asi"); return ef ? (void*)GetProcAddress(ef, name) : 0; }
static void GateMenuClose(void) { static int (*mc)(const char*); static int tried; if (!mc && !tried) { tried = 1; mc = (int (*)(const char*))GateEf("Nioh3EngineFix_MenuClose"); } if (mc) mc("native box at the Return object"); }
// safety net: a native modal (the game's own Yes/No box) is open. EngineFix answers from the game's own UI state (Nioh3EngineFix_NativeModal);
// our dialog hooks below also stamp g_gate_nat_t when they see a box they let through.
static volatile DWORD g_gate_nat_t; static const char* g_gate_nat_why = "?";
static int GateNativeModal(void) {
    static int (*nm)(void); static DWORD tTry; DWORD now = GetTickCount();
    if (!nm && (!tTry || now - tTry > 5000)) { tTry = now; nm = (int (*)(void))GateEf("Nioh3EngineFix_NativeModal"); }
    if (nm && nm()) { g_gate_nat_why = "game UI modal flag"; return 1; }
    if (g_gate_nat_t && now - g_gate_nat_t < 500) { g_gate_nat_why = "native dialog hook"; return 1; }
    return 0;
}
static const char* GateNativeWhy(void) { return g_gate_nat_why; }
// 'Fight again' from the gate page: queued, the pump runs TrainRetry (same boss, round reset, player back to the round-1 spot). 1 = queued, 0 = no session.
// bridge 12666: Training menu (Boss Rematch UI) -> game thread. op 1 summon into slot A/B (replaces that slot's boss), 2 despawn all, 3 despawn one slot.
static volatile LONG g_rq_op, g_rq_slot; static volatile uint32_t g_rq_mission, g_rq_chr;
static int SlotAlive(int s) { if (s < 0 || s >= N3T_SLOTS || !g_slot_act[s]) return 0; for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i] == g_slot_act[s]) return 1; return 0; }
// user #14309: slot 0..N3T_SLOTS-1 (Boss A, B, C ...); -1 = the first free letter ('Summon another boss'). A taken slot is replaced.
N3T_API int Nioh3Training_SummonSlot(int slot, uint32_t mission) {
    if (slot == -1) slot = SlotFree();
    if (slot < 0 || slot >= N3T_SLOTS || !mission || g_rq_op || g_sp.active) return 0;
    if (!SlotAlive(slot) && g_nsp >= MAXSP) { Log("MENU summon slot %c refused: %d spawned actors tracked (remove all summoned bosses first)", 'A' + slot, g_nsp); return 0; }
    g_rq_slot = slot; g_rq_mission = mission; g_rq_chr = 0; InterlockedExchange(&g_rq_op, 1); return 1;
}
// user #22984: summon a roster boss (character id from Nioh3Training_RosterEntry) as the given letter, -1 = the first free one
N3T_API int Nioh3Training_SummonSlotChr(int slot, uint32_t chr) {
    if (slot == -1) slot = SlotFree();
    if (slot < 0 || slot >= N3T_SLOTS || !chr || g_rq_op || g_sp.active) return 0;
    if (!SlotAlive(slot) && g_nsp >= MAXSP) { Log("MENU summon slot %c refused: %d spawned actors tracked (remove all summoned bosses first)", 'A' + slot, g_nsp); return 0; }
    g_rq_slot = slot; g_rq_mission = 0; g_rq_chr = chr; InterlockedExchange(&g_rq_op, 1); return 1;
}
N3T_API int Nioh3Training_TrainHere(uint32_t mission) { if (!mission || g_rq_op || g_sp.active) return 0; g_rq_mission = mission; InterlockedExchange(&g_rq_op, 4); return 1; }  // user #13223
N3T_API void Nioh3Training_SetPracticeNext(int on) { g_practice_next = on != 0; }  // user #13223: 1 = the next session (arena Fight now) is a Practice session
N3T_API int Nioh3Training_DespawnAll(void) { if (g_rq_op) return 0; InterlockedExchange(&g_rq_op, 2); return 1; }
N3T_API int Nioh3Training_SlotCount(void) { return N3T_SLOTS; }  // user #14309: how many boss letters the menu may offer
N3T_API int Nioh3Training_DespawnSlot(int slot) { if (slot < 0 || slot >= N3T_SLOTS || g_rq_op) return 0; g_rq_slot = slot; InterlockedExchange(&g_rq_op, 3); return 1; }
N3T_API uint32_t Nioh3Training_SlotChr(int slot) { return SlotAlive(slot) ? g_slot_chr[slot] : 0; }  // 0 = empty
static void SlotDespawn(int s) { for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i] && g_sp_actor[i] == g_slot_act[s]) { Despawn(i); break; } g_slot_act[s] = 0; g_slot_chr[s] = 0; g_slot_ih[s] = 0; g_sph[s].phase = 0; }
static void RqTick(void) {
    LONG op = InterlockedExchange(&g_rq_op, 0); if (!op) return;
    int s = (int)g_rq_slot;
    if (op == 1) {
        if (GameState() && GameState()[2]) { Log("MENU summon slot %c: a load is running, refused", 'A' + s); return; }
        SlotDespawn(s);
        char tok[32]; const char* t = SummonTok(g_rq_mission);  // user #22984: the row's own id (R04DF, D084F ...) / a roster character
        if (g_rq_chr) snprintf(tok, sizeof tok, "%u", g_rq_chr); else if (t) snprintf(tok, sizeof tok, "%s", t); else snprintf(tok, sizeof tok, "R%04X", g_rq_mission);
        g_sp_slot_next = s; int ok = SummonStart(tok, 400.0f + 150.0f * (float)(s % 5));  // #14309: letters spread 4 / 5.5 / 7 / 8.5 / 10 m if (!ok) g_sp_slot_next = -1;
        Log("MENU summon slot %c %s -> %d", 'A' + s, tok, ok);
    } else if (op == 2) { DespawnAll("menu"); for (int k = 0; k < N3T_SLOTS; ++k) { g_slot_act[k] = 0; g_slot_chr[k] = 0; g_slot_ih[k] = 0; g_sph[k].phase = 0; } SumNoDeathRelease("despawn all"); }
    else if (op == 3) { SlotDespawn(s); Log("MENU despawn slot %c", 'A' + s); }
    else if (op == 4) {  // user #13223: Practice > Training here (no load, no teleport: boss summoned where the player stands, Idle stance, retry in place)
        if (GameState() && GameState()[2]) { Log("MENU training here: a load is running, refused"); return; }
        g_practice_next = 1; int ok = Nioh3Training_Summon(g_rq_mission); if (!ok) g_practice_next = 0;
        Log("MENU training here mission 0x%X -> %d", g_rq_mission, ok);
    }
}
// user #13197: player NoDeath switched on by a summon (SummonNoDeath 1) must not outlive the summoned bosses: off on despawn-all, when the last
// summoned boss is dead or gone, and on session stop (TrainStop). Kept while a Training session runs with PlayerNoDeath 1 (that session owns it).
// crash 2026-10-18 (notes/crash-20261018.md): a killed summon was never despawned; the shrine rest reload freed it and MvBossChr walked the stale
// g_sp_actor entry (AV at +E15E). Game thread (pump): on every load start and every ~500 ms drop each tracked actor that is no longer in the actor
// table, exactly as Despawn's 'already gone' branch does (SlotForget + null), so no hook ever dereferences a freed actor.
static void SpPruneTick(void) {
    static int wasLoad; static DWORD tLast;
    const int load = GameState() && GameState()[2];
    const DWORD now = GetTickCount();
    const int edge = load && !wasLoad; wasLoad = load;
    if (!edge && now - tLast < 500) return;
    tLast = now;
    if (g_nsp <= 0) return;
    int any = 0; for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i]) { any = 1; break; }
    if (!any) return;
    static uint64_t cur[1024]; const int n = Actors(cur, 1024);
    if (n <= 0 && !load) return;  // table unreadable outside a load: keep tracking (Despawn re-checks anyway)
    for (int i = 0; i < g_nsp; ++i) {
        const uint64_t a = g_sp_actor[i]; if (!a) continue;
        int found = 0; for (int k = 0; k < n; ++k) if (cur[k] == a) { found = 1; break; }
        if (found) continue;
        Log("PRUNE #%d %p: not in actor table any more (%s), forgotten", i, (void*)a, edge ? "load started" : load ? "loading" : "periodic");
        SlotForget(a); g_sp_actor[i] = 0;
        if (g_mon.a == a) g_mon.a = 0;
        if (g_aw.a == a) g_aw.a = 0;
        if (g_newActor == a) g_newActor = 0;
    }
}
static void SumNoDeathRelease(const char* why) {
    if (!g_sum_nodeath) return;
    g_sum_nodeath = 0;
    if (g_tr.active && TrNoDeath()) { Log("SUMNODEATH (%s): Training session keeps player NoDeath (PlayerNoDeath / SessionNoDeath 1)", why); return; }
    if (g_tr_nodeath_now) PlayerSetNoDeath(0);
    Log("SUMNODEATH (%s): player NoDeath off", why);
}
static void SumNoDeathTick(void) {  // game thread (pump)
    if (!g_sum_nodeath || g_sp.active || g_watch) return;
    if (GameState() && GameState()[2]) return;  // never call SetNoDeath during a load
    if (!PlayerChar()) return;
    for (int i = 0; i < g_nsp; ++i) {
        uint64_t a = g_sp_actor[i]; if (!a) continue;
        int dead = 0; uint64_t c = 1, m = 0;
        if (!ActorDeadNow(a, &dead, &c, &m) && !dead) return;  // a summoned boss is still alive
    }
    SumNoDeathRelease(g_nsp ? "every summoned boss is dead" : "no summoned boss left");
}
static int AnySlotAlive(void) { for (int s = 0; s < N3T_SLOTS; ++s) if (SlotAlive(s)) return 1; return 0; }
N3T_API int Nioh3Training_SessionActive(void) { return (g_tr.active ? 1 : 0) | (AnySlotAlive() ? 2 : 0); }  // user #13197: menu 'Practice tools' root row
// user #13262: mode of the running session for the Return gate page. 0 = no session, 1 = Boss Rematch Fight now, 2 = Training (Practice: Training here / arena)
N3T_API int Nioh3Training_SessionMode(void) { return !g_tr.active ? 0 : g_tr.practice ? 2 : 1; }
// user #13164: game thread (pump). Holds a freshly summoned slot boss passive for g_sph[s].ms, then gives its AI back (only if we switched it off).
static __attribute__((noinline)) void SphTick(void) {
    DWORD now = GetTickCount();
    for (int s = 0; s < N3T_SLOTS; ++s) {
        // user #14309: 'Boss X: Stand still' on a boss that was fighting (or whose repeat stopped): hold its AI again (not the session boss
        // still owned by the Idle hold, which SetSlotStance routes to Nioh3Training_SetIdleHold)
        if (InterlockedExchange(&g_slot_rehold[s], 0) && !g_sph[s].phase && !g_slot_ih[s] && SlotAlive(s) && !g_slot_fight[s]) {
            int dead = 0; uint64_t hc = 0, hm = 0;
            if (!(ActorDeadNow(g_slot_act[s], &dead, &hc, &hm) && dead)) {
                memset(&g_sph[s], 0, sizeof g_sph[0]); g_sph[s].a = g_slot_act[s]; g_sph[s].t0 = now; g_sph[s].ms = SPH_TRAINING; g_sph[s].phase = 1; g_sph[s].sess = g_tr.active;
                Log("SUMPASSIVE slot %c actor %p: Stand still again (menu / timer), AI held", 'A' + s, (void*)g_slot_act[s]);
            }
        }
        if (!g_sph[s].phase) continue;
        uint64_t a = g_sph[s].a;
        if (!a || g_slot_act[s] != a || !SlotAlive(s)) { Log("SUMPASSIVE slot %c: boss gone / replaced, hold ends", 'A' + s); g_sph[s].phase = 0; continue; }
        { int dead = 0; uint64_t hc = 0, hm = 0; if (ActorDeadNow(a, &dead, &hc, &hm) && dead) { g_sph[s].phase = 0; continue; } }
        // #14304 / #14309 per boss; #14462: a repeat no longer releases the hold (slot_hold.h), the repeat plays with the AI held
        const char* rel = SphReleaseWhy(g_sph[s].ms, now - g_sph[s].t0, g_sph[s].sess, g_tr.active, g_slot_fight[s] != 0);
        if (rel) {
            g_sph[s].chase = 0;
            if (g_sph[s].nOff && BhAiOn(a) == 0) { __try { ((void (*)(uint64_t))(B + Grva(0x66982C)))(a); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SUMPASSIVE: 66982C faulted"); } }
            Log("SUMPASSIVE slot %c actor %p: AI on +%lu ms (%s, passive %d ms, held %d times)", 'A' + s, (void*)a, now - g_sph[s].t0, rel, g_sph[s].ms, g_sph[s].nOff);
            g_sph[s].phase = 0; continue;
        }
        if (now - g_sph[s].tRe < 100) continue;
        const int act = SphAiAction(g_sph[s].ms == SPH_TRAINING && g_sph[s].chase, BhAiOn(a));  // #14462: AI on only for the aggro walk-up
        if (act == 1) {
            g_sph[s].tRe = now;
            __try { ((void (*)(uint64_t))(B + Grva(0x66982C)))(a); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SUMPASSIVE: 66982C faulted, hold ends"); g_sph[s].phase = 0; continue; }
            Log("SUMPASSIVE slot %c actor %p: AI on for the aggro walk-up (locomotion only, held again in range)", 'A' + s, (void*)a);
        } else if (act == 0) {
            g_sph[s].tRe = now;
            __try { ((void (*)(uint64_t))(B + Grva(0x668D74)))(a); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SUMPASSIVE: 668D74 faulted, hold ends"); g_sph[s].phase = 0; continue; }
            if (++g_sph[s].nOff <= 10) Log("SUMPASSIVE slot %c actor %p: AI off (#%d, +%lu ms)", 'A' + s, (void*)a, g_sph[s].nOff, now - g_sph[s].t0);
        }
    }
}
static DWORD WINAPI SphSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH], v[16]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    snprintf(v, sizeof v, "%d", g_cfg_sum_passive); WritePrivateProfileStringA("Summon", "SummonPassiveMs", v, ini);
    WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
// user #13164: Summon Boss menu slider. Clamped 0..60000 ms, applies to the next summon, persist 1 = saved as [Summon] SummonPassiveMs.
N3T_API int Nioh3Training_GetSummonPassive(void) { return g_cfg_sum_passive; }
N3T_API int Nioh3Training_SetSummonPassive(int ms, int persist) {
    if (ms < 0) ms = 0; if (ms > SUM_PASSIVE_MAX) ms = SUM_PASSIVE_MAX;
    g_cfg_sum_passive = ms;
    Log("API summon passive %d ms%s", ms, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, SphSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
    return ms;
}
N3T_API int Nioh3Training_GateFightAgain(void) { if (!g_tr.active || g_rh.active) return 0; InterlockedExchange(&g_gate_again, 1); return 1; }
static void GateTick(void) {
    if (InterlockedExchange(&g_gate_again, 0) && g_tr.active && !g_rh.active) { Log("GATE: Fight again (Boss Rematch gate page)"); TrainRetry("gate: fight again"); }
    if (!g_cfg_gate) return;
    static DWORD tBossSeen;  // #13255: when the boss was first seen alive this session (0 = not alive)
    if (!g_tr.active) { if (g_gate.session) { g_gate.session = 0; g_gate.up = 0; g_gate.tried = 0; } tBossSeen = 0; return; }
    if (!g_gate.session) { g_gate.session = 1; g_gate.up = 0; g_gate.tried = 0; g_gate.tTry = 0; tBossSeen = 0; }
    if (g_rh.active || g_xw.active || g_fd.pend) return;
    const uint8_t* st = GameState(); if (!st || st[2] || !PlayerChar() || StageState() != 5) return;
    DWORD now = GetTickCount();
    // #7118/#7138: only while the session is idle (no boss, no respawn pending). The removed 'or now - tCmd < 1500' clause also matched the 2 s window after every
    // TrainRetry (tCmd reset): the clear gadget was created 2 s before the retry boss (log 22:34:19.5 gadget, 22:34:21.5 boss) and that boss stayed passive
    // (ai+1AA8 = -1, walks up and hops back to its spawn = the 'jumping' boss; the confirmed-good 20:39 retry without a gadget went 0 at once).
    // Also never before the round-1 arena boss exists (Fight now arena swap still loading its resources: no boss yet, but one is about to spawn).
    int idle = !GateBossAlive() && !g_tr.tRespawn && !g_sp.active && !g_vf.respawnAt && !g_tr.beaten && !(g_tr.tWin && g_cfg_tr_win > 0) && !(g_ar.active && !g_ar.spawned);
    // #13255 ReturnGateAlways: also spawn at session start, 3 s after the round-1 boss is alive (pose stored, nothing pending), trail has floor samples
    const int alive = GateBossAlive(); if (!alive) tBossSeen = 0; else if (!tBossSeen) { tBossSeen = now; if (!tBossSeen) tBossSeen = 1; }
    int early = g_cfg_gate_always && g_tr.hasStart && alive && tBossSeen && now - tBossSeen >= 3000 && !g_tr.tRespawn && !g_sp.active && !g_vf.respawnAt && !g_tr.beaten && !(g_ar.active && !g_ar.spawned);
    if (!g_gate.up && g_home.valid && g_gate.tried < 3 && (!g_gate.tTry || now - g_gate.tTry > 3000) && (idle || early)) {
        g_gate.tTry = now; g_gate.tried++; GateSpawn(g_gate.tried == 1 ? (early && !idle ? "session start (ReturnGateAlways)" : "arena session") : "retry");
    }
    // #9324 (user screen 22:4x): the gate is the game's own mission clear gadget; its interact (A near it) opens the native 'This will end the mission.
    // Proceed?' prompt on top of our gate page, and its Yes ended the game process (22:14: no MissionRequestReturn, the game just closed). While the player
    // is within ReturnGateMaskRadius of the gate and no boss is alive, the face buttons are hidden from the GAME (EngineFix lease, renewed every tick;
    // our menus still read them), so the native prompt never opens; our gate page opens by standing on it (ReturnGateTouchMs).
    // #9664/#9670 (user log 01:23: 'EngineFix export MISSING'): the installed EngineFix had no MaskGameButtons, so A near the gate opened the native
    // 'end the mission' prompt (alone, or under our page after standing 1500 ms = both). Now: (1) the export is re-resolved here if it was missing at
    // start; (2) a pad A press within ReturnGateMaskRadius opens OUR gate page at once (ReturnGateOpenOnA, raw XInput read, not the game's), unless the
    // mod menu is already open (EngineFix_MenuOpen) or a page was opened < 1.5 s ago; (3) the mask lease covers the whole radius as before.
    if (g_gate.up && !p_osdMask) { static DWORD tRes; if (!tRes || now - tRes > 5000) { tRes = now; HMODULE ef = GetModuleHandleA("Nioh3EngineFix.asi");
        p_osdMask = ef ? (int (*)(int, int))GetProcAddress(ef, "Nioh3EngineFix_MaskGameButtons") : 0; static int nl; if (!p_osdMask && nl++ < 2) Log("GATE: EngineFix MaskGameButtons export MISSING: install the matching Nioh3EngineFix.asi or A near the gate can still open the native end-mission prompt"); } }
    // #9909 (user log 02:58:12-02:58:16, mark_025821_305_freeze.jpg: our gate page AND the native 'This will end the mission. Proceed?' box): right after the
    // lost-round teleport onto the round-1 spot (220 u from the gate) the mask flickered off for 1.3 s (02:58:13.318 'away' -> 02:58:14.593 'near') while
    // the player stood in the gadget's interact range; an A in that gap (or an A held through it: unmasking a held A is a fresh press for the game) opened
    // the native prompt, and standing on the gate 1.5 s later opened ours on top. Now the mask (a) stays on ReturnGateMaskHoldMs (2000) after the last
    // 'near' sample, (b) stays on while our gate page is open or the player position is unreadable, (c) is forced on for 2.5 s after a lost round put the
    // player back next to the gate (g_gate_maskUntil), and (d) EngineFix keeps a masked button hidden until it is released, so a held A never re-presses.
    // The 'away' line logs why (distance, height, boss, page).
    // user #13796: ONE gate zone for every mode (gate_zone.h; replaces the idle-only 400 u block and the boss-alive 110 u block below, which
    // left the native interact reachable between 110 u and its real reach and re-opened our page over the native box). [Return]
    // ReturnGateZone 1 (default); 0 = the old two blocks.
    if (g_gate.up && g_cfg_gate_zone) {
        static GzState gz; static int masked, nativeLogged;
        float q[4]; const int posOk = PlayerPos(q); float d = -1.0f, dy = 0.0f;
        if (posOk) { float mx = q[0] - g_gate.pos[0], mz = q[2] - g_gate.pos[2]; d = sqrtf(mx * mx + mz * mz); dy = fabsf(q[1] - g_gate.pos[1]); }
        const int boss = GateBossAlive(), page = GateMenuOpen(), nat = GateNativeModal();
        const int inZ = posOk ? (d < (float)g_cfg_gate_mask_r && dy < 400.0f) : masked;  // position unreadable: keep the last state
        const int hold = boss ? 300 : g_cfg_gate_mask_hold;  // in a fight the face buttons come back fast after leaving the zone
        const int force = g_gate_maskUntil && (LONG)(g_gate_maskUntil - now) > 0;
        const int r = GzStep(&gz, now, inZ || (force && !nat), page, nat, g_cfg_gate_opena ? (GatePadButtons() & g_cfg_gate_open_btn) != 0 : 0, g_cfg_gate_tap_ms, hold);
        const int m = (r & GZ_MASK) != 0;
        if (m && p_osdMask) p_osdMask(g_cfg_gate_mask_btn, 300);
        if (m != masked) { masked = m; if (!m && p_osdMask) p_osdMask(0, 0);
            Log(m ? "GATE zone: player at the Return object (%.0f u, dy %.0f, boss %d, page %d): face buttons hidden from the game, tap A opens our page"
                  : "GATE zone: player left the Return object (%.0f u, dy %.0f, boss %d, page %d): game buttons back", d, dy, boss, page); }
        if (r & GZ_NATIVE) { if (p_osdMask) p_osdMask(0, 0); masked = 0;
            if (nativeLogged++ < 50) Log("GATE zone: a native modal is open (%s): mask off, our page %s - the player answers the game's box", GateNativeWhy(), page ? "closed" : "not opened");
            if (page) GateMenuClose(); }
        if (r & GZ_HOLD_CANCEL) Log("GATE zone: button held > %d ms at the Return object: cancelled (nothing opens, the game never saw it)", g_cfg_gate_tap_ms);
        if (r & GZ_OPEN) { g_gate.tIn = 0; g_gate.left = 0; int u = GateUse(); Log("GATE zone: tap at the Return object (%.0f u, boss %d) -> our gate page (%d)", d, boss, u); }
    }
    if (g_gate.up && g_cfg_gate_mask_r > 0 && !g_cfg_gate_zone) {
        float q[4]; static int masked; static DWORD tNear;
        int boss = GateBossAlive(), posOk = PlayerPos(q), page = GateMenuOpen(); float d = -1.0f, dy = 0.0f;
        if (posOk) { float mx = q[0] - g_gate.pos[0], mz = q[2] - g_gate.pos[2]; d = sqrtf(mx * mx + mz * mz); dy = fabsf(q[1] - g_gate.pos[1]); }
        int inR = posOk && d < (float)g_cfg_gate_mask_r && dy < 400.0f;
        if (!boss && (inR || page || (!posOk && masked))) tNear = now;
        int nearG = !boss && (inR || page || (!posOk && masked) || (tNear && now - tNear < (DWORD)g_cfg_gate_mask_hold) || (g_gate_maskUntil && (LONG)(g_gate_maskUntil - now) > 0));
        if (nearG && p_osdMask) p_osdMask(g_cfg_gate_mask_btn, 300);
        if (nearG != masked) { masked = nearG;
            if (nearG) Log("GATE: player near the gate (%.0f u, dy %.0f%s%s): native interact %s", d, dy, page ? ", gate page open" : "", posOk ? "" : ", position unreadable", p_osdMask ? "hidden (no end-mission prompt), A opens the gate page" : "NOT hidden (EngineFix export missing), A opens the gate page");
            else Log("GATE: player away from the gate (%.0f u of %d, dy %.0f, boss alive %d, page %d, pos %d, %lu ms since near): native interact back", d, g_cfg_gate_mask_r, dy, boss, page, posOk, tNear ? now - tNear : 0);
            if (!nearG && p_osdMask) p_osdMask(0, 0); }
        static int aWas; static DWORD tOpen; int aNow = GatePadA();
        if (nearG && g_cfg_gate_opena && aNow && !aWas && (!tOpen || now - tOpen > 1500) && !GateMenuOpen()) { tOpen = now; g_gate.tIn = 0; g_gate.left = 0; int r = GateUse(); Log("GATE: pad A near the return gate -> our gate page (%d)", r); }
        aWas = aNow;
    }
    // #13529 (user log 11:48, Training arena: the gate opened no menu): with ReturnGateAlways the Practice boss is alive (Idle stance) for the whole
    // session, and every one of OUR open paths (raw pad A, stand-on, face-button mask) was idle-only (!boss), so only the native gadget interact was
    // left - and that gadget floated ~95 u above the floor (GateGroundY fix above), out of its interact reach. Now in a Practice session, or while
    // Stand still (Idle hold) holds the boss, a pad A press while standing at the gate (ReturnGateRadius, |dy| < 250) opens our gate page with the
    // boss alive. A Boss Match fight with a fighting boss keeps the native interact only (fight inputs untouched). One log line per approach.
    if (g_gate.up && g_cfg_gate_opena && GateBossAlive() && !g_cfg_gate_zone) {
        static int aWas2, inWas; static DWORD tOpen2;
        const int calm = g_tr.practice || Nioh3Training_GetIdleHold() == 2;
        float q[4]; int in = 0;
        if (calm && PlayerPos(q)) { float mx = q[0] - g_gate.pos[0], mz = q[2] - g_gate.pos[2]; in = mx * mx + mz * mz < (float)g_cfg_gate_radius * g_cfg_gate_radius && fabsf(q[1] - g_gate.pos[1]) < 250.0f; }
        // #13548: one tap of A opens our page; the face buttons are hidden from the GAME while standing at the gate (EngineFix lease, a button held
        // when it ends stays hidden until released = hold-cancel), so the native hold-to-interact never completes and no in-game mission menu opens.
        if (in && p_osdMask) p_osdMask(g_cfg_gate_mask_btn, 300);
        if (in != inWas) { inWas = in; if (!in && p_osdMask) p_osdMask(0, 0);
            if (in) Log("GATE: player at the gate with the boss alive (%s): tap A opens the gate page, native interact %s", g_tr.practice ? "Practice session" : "Stand still stance", p_osdMask ? "hidden (hold cancelled)" : "NOT hidden (EngineFix export missing)"); }
        const int aNow = GatePadA();
        if (in && aNow && !aWas2 && (!tOpen2 || now - tOpen2 > 1500) && !GateMenuOpen()) { tOpen2 = now; int r = GateUse(); Log("GATE: pad A at the gate (boss alive, %s) -> our gate page (%d)", g_tr.practice ? "Practice" : "Stand still", r); }
        aWas2 = aNow;
    }
    if (g_gate.up && g_cfg_gate_touch > 0) {
        float p[4]; if (!PlayerPos(p)) return;
        float dx = p[0] - g_gate.pos[0], dz = p[2] - g_gate.pos[2], dy = p[1] - g_gate.pos[1]; int in = dx * dx + dz * dz < (float)g_cfg_gate_radius * g_cfg_gate_radius && fabsf(dy) < 250.0f;
        if (!in) { g_gate.left = 1; g_gate.tIn = 0; return; }
        if (GateMenuOpen()) { g_gate.tIn = 0; return; }  // our menu already up: no second open; the stand timer restarts after it closes
        if (!g_gate.left || GateBossAlive() || GateNativeModal()) return;
        if (!g_gate.tIn) g_gate.tIn = now;
        if (now - g_gate.tIn >= (DWORD)g_cfg_gate_touch) { g_gate.tIn = 0; g_gate.left = 0; int r = GateUse(); Log("GATE: player stood on the return gate %d ms -> %d", g_cfg_gate_touch, r); }
    }
}
static void h_reqret(const uint32_t* p, uint8_t f) {
    void* ra = __builtin_return_address(0);
    if (g_cfg_gate && g_gate.up && g_tr.active && !g_rh.active) {
        if (GateBossAlive() && !g_cfg_gate_always) { Log("GATE: MissionRequestReturn from %p while the boss is alive: ignored (gate is inactive during a fight)", (void*)((uintptr_t)ra - B)); return; }        int r = GateUse(); Log("GATE: MissionRequestReturn (caller +%llX) diverted to the gate choice -> %d", (unsigned long long)((uintptr_t)ra - B), r); return;
    }
    o_reqret(p, f);
}
// #9909 (mark_025821_305_freeze.jpg: our gate page AND the native 'This will end the mission. Proceed?' Yes/No at once): the input mask only hides
// the XInput face buttons, so any other way the game sees an interact (keyboard, a pad read outside XInputGetState, an A already latched) still opened
// the native box. Game-side fix, independent of the input path: the box is built by EndMissionConfirmOpen 12A1F80(ctx, action) for action 0x2613
// (text 0x3E0C230 'This will end the mission. Proceed?' / 0x3C96C71 'You are about to abandon the mission...', Yes 0x3DB0E79 / No 0x3CD8F47, opened
// through the choice box 2806FD4; callers 1EC0E10 (passes 0x2613), 282FE6C / 2834370 (gadget action from the record)). While our Return gate is up
// in a training session the 0x2613 request is swallowed (the box never opens) and our gate page opens instead (unless it is already open or the
// boss is alive). [Return] ReturnGateBlockNativePrompt (default 1; 0 = old behaviour). Other actions (0x536 / 0x196D / 0x359E / 0x4A55 / 0x576B /
// 0x604E) are untouched; the pause menu's own end-mission entries (1DAE4E0 / 1DB58B0) are handled by #10020 below.
typedef uint64_t (*EndConfFn)(void*, int);
static EndConfFn o_endconf; static int g_cfg_gate_block = 1; static volatile LONG g_endconf_n;
static uint64_t h_endconf(void* ctx, int action) {
    int nearGate = 0;  // #9983: the box opened by itself near the gate with no 0x2613 call logged -> the gate record's own action id differs; any action while the player is at the gate counts
    if (action != 0x2613 && g_cfg_gate_block && g_cfg_gate && g_gate.up && g_tr.active && !g_rh.active) { float q[4]; if (PlayerPos(q)) { float mx = q[0] - g_gate.pos[0], mz = q[2] - g_gate.pos[2]; nearGate = sqrtf(mx * mx + mz * mz) < 600.0f; } }
    if ((action == 0x2613 || nearGate) && g_cfg_gate_block && g_cfg_gate && g_gate.up && g_tr.active && !g_rh.active) {
        void* ra = __builtin_return_address(0); LONG n = InterlockedIncrement(&g_endconf_n);
        int boss = GateBossAlive() && !g_cfg_gate_always, page = GateMenuOpen(), r = -1;  // #13255: with ReturnGateAlways a fight does not block our page
        if (!boss && !page) { g_gate.tIn = 0; g_gate.left = 0; r = GateUse(); }
        if (n <= 50) { float q[4]; float d = -1.0f; if (PlayerPos(q)) { float mx = q[0] - g_gate.pos[0], mz = q[2] - g_gate.pos[2]; d = sqrtf(mx * mx + mz * mz); }
            Log("GATE: native end-mission prompt (action 0x%X, caller +%llX, player %.0f u from the gate) blocked #%ld: %s", (unsigned)action, (unsigned long long)((uintptr_t)ra - B), d, n,
                boss ? "boss alive, nothing opened" : page ? "our gate page already open" : r >= 0 ? "our gate page opened instead" : "?"); }
        return 0;
    }
    if (g_cfg_gate && g_gate.up) { DWORD tn = GetTickCount(); g_gate_nat_t = tn ? tn : 1; }  // #13796 safety net
    if (g_cfg_gate && g_gate.up) { static LONG nO; if (InterlockedIncrement(&nO) <= 40) Log("GATE diag (#9978): EndMissionConfirmOpen action 0x%X NOT blocked (caller +%llX, tr.active %d, rh.active %d, block %d) - this opens a native box while our gate is up", action, (unsigned long long)((uintptr_t)__builtin_return_address(0) - B), g_tr.active, g_rh.active, g_cfg_gate_block); }
    return o_endconf(ctx, action);
}
// #10020 (mark_033810_846_freeze.jpg, log 03:37:28-03:38:10: the 'This will end the mission. Proceed?' box opened near the gate, h_endconf never logged, not even
// its diag): static RE, text 0x3E0C230 is loaded in only three functions: 12A1F80 (hooked above) and the pause menu's two 'decide' handlers PauseMenuDecide
// 1DAE4E0 (reached from the vtable slot 1DB8920 when the cursor row changed) and 1DB58B0 (vtable slot, row read through menu+0x3C0 vfunc +0x70). Both: state
// menu+0x3C8 == 1, menu+0x3E8 / +0x404 clear, entry id = ((int*)menu+0x3D0)[row]; id 3 = end mission -> Yes/No through the choice box 2806FD4 (short text
// 0x3E0C230 or 0x3C96C71 'abandon'). So the box came from the pause menu path, not from the gadget. While our gate is up in a training session and the
// player is within 600 u of it, entry 3 is swallowed (menu state untouched, no box) and our gate page opens instead. Other entries run natively.
typedef void (*PmDecideFn)(void*);
static PmDecideFn o_pmdec1, o_pmdec2; static volatile LONG g_pmdec_n;
static __attribute__((noinline)) int PmDecideSwallow(void* m, int viaVfunc, void* ra, const char* which) {
    if (!(g_cfg_gate_block && g_cfg_gate && g_gate.up && g_tr.active && !g_rh.active)) return 0;
    float q[4], d; if (!PlayerPos(q)) return 0;
    { float mx = q[0] - g_gate.pos[0], mz = q[2] - g_gate.pos[2]; d = sqrtf(mx * mx + mz * mz); } if (d >= 600.0f) return 0;
    int id = -1;
    __try { uint8_t* p = (uint8_t*)m;
        if (*(int*)(p + 0x3C8) != 1 || *(uint8_t*)(p + 0x3E8) || *(uint8_t*)(p + 0x404)) return 0;
        int row = *(int*)(p + 0x3FC);
        if (viaVfunc) { void* lst = *(void**)(p + 0x3C0); if (!lst) return 0; row = ((int (*)(void*))(*(void***)lst)[0x70 / 8])(lst); }
        int* ids = *(int**)(p + 0x3D0); if (!ids || row < 0) return 0; id = ids[row];
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    if (id != 3) return 0;
    LONG n = InterlockedIncrement(&g_pmdec_n); int boss = GateBossAlive() && !g_cfg_gate_always, page = GateMenuOpen(), r = -1;  // #13255
    if (!boss && !page) { g_gate.tIn = 0; g_gate.left = 0; r = GateUse(); }
    if (n <= 50) Log("GATE: native end-mission prompt from the pause menu (%s, entry 3, caller +%llX, player %.0f u from the gate) blocked #%ld: %s", which,
        (unsigned long long)((uintptr_t)ra - B), d, n, boss ? "boss alive, nothing opened" : page ? "our gate page already open" : r >= 0 ? "our gate page opened instead" : "?");
    return 1;
}
static void h_pmdec1(void* m) { if (PmDecideSwallow(m, 0, __builtin_return_address(0), "PauseMenuDecide 1DAE4E0")) return; o_pmdec1(m); }
static void h_pmdec2(void* m) { if (PmDecideSwallow(m, 1, __builtin_return_address(0), "PauseMenuDecide 1DB58B0")) return; o_pmdec2(m); }
// #10022 (user #10021/#10022: the box still opened after cdbb866/ae25528, no h_endconf line): lowest common chokepoint. Every Yes/No box in the game is
// opened by DialogOpen 348274(mgr = [g+0x6C8], std::wstring text BY VALUE, a3 (dtor 705218), a4 (dtor 7073FC), u128* hashes, int, u8); its only 3 callers
// are YesNoBox 347FE8 (30 callers, generic Yes 0x3DB0E79 / No 0x3CD8F47), ChoiceBox 2806FD4 (EndMissionConfirmOpen 12A1F80, PauseMenuDecide 1DAE4E0 /
// 1DB58B0, ...) and 2807094. Text 0x3E0C230 'This will end the mission. Proceed?' / 0x3C96C71 'You are about to abandon the mission' is only loaded in
// 12A1F80 / 1DAE4E0 / 1DB58B0, but a data-driven path would still pass through 348274, so the box is matched here by its TEXT (localized strings of
// those ids via TextGetById 5A4B04, prefix match; plus 'end the mission' / 'abandon the mission' in the English text). While our gate is up in a training
// session and (player < 600 u from the gate, or our gate page is open, or it was requested < 1500 ms ago) the box is dropped: the by-value args are
// destroyed exactly as 348274's own epilogue does (310C18 text, 705218 a3, 7073FC a4) and nothing opens. Every DialogOpen while the gate is up is logged
// (text, blocked or not, 5-frame stack as module offsets) so the real route shows in the log. [Return] ReturnGateBlockDialog (default 1).
typedef void (*DlgOpenFn)(void*, void*, void*, void*, void*, int, uint8_t);
static DlgOpenFn o_dlgopen; static int g_cfg_gate_dlg = 1; static volatile LONG g_dlg_n, g_dlg_blk;
static const wchar_t* WsData(const uint8_t* ws) { uint64_t cap = *(const uint64_t*)(ws + 0x18); return cap >= 8 ? *(const wchar_t* const*)ws : (const wchar_t*)ws; }
static int WsPrefix(const wchar_t* s, const wchar_t* p) { if (!s || !p || !*p) return 0; for (; *p; ++s, ++p) if (*s != *p) return 0; return 1; }
static int WsHasA(const wchar_t* s, const char* a) {  // ASCII case-insensitive substring
    if (!s) return 0; size_t n = strlen(a);
    for (int i = 0; s[i] && i < 512; ++i) { size_t k = 0; while (k < n && s[i + k] && (s[i + k] | 0x20) == (wchar_t)(a[k] | 0x20)) ++k; if (k == n) return 1; }
    return 0;
}
static __attribute__((noinline)) void h_dlgopen(void* mgr, void* text, void* a3, void* a4, void* a5, int a6, uint8_t a7) {
    if (g_cfg_gate_dlg && g_cfg_gate && g_gate.up && g_tr.active && !g_rh.active && text) {
        const wchar_t* s = 0; int endm = 0;
        __try { s = WsData((const uint8_t*)text);
            static const uint32_t ids[3] = {0x3E0C230, 0x3C96C71, 0x26AC231};
            for (int i = 0; i < 3 && !endm; ++i) { const wchar_t* t = ((const wchar_t* (*)(uint32_t))(B + Grva(0x5A4B04)))(ids[i]); endm = WsPrefix(s, t); }
            if (!endm) endm = WsHasA(s, "end the mission") || WsHasA(s, "abandon the mission");
        } __except (EXCEPTION_EXECUTE_HANDLER) { s = 0; endm = 0; }
        DWORD now = GetTickCount(); float q[4], d = -1.0f; if (PlayerPos(q)) { float mx = q[0] - g_gate.pos[0], mz = q[2] - g_gate.pos[2]; d = sqrtf(mx * mx + mz * mz); }
        int page = GateMenuOpen(), recent = g_gate_tUse && now - g_gate_tUse < 1500, nearG = d >= 0.0f && d < 600.0f;
        int block = endm && (nearG || page || recent);
        void* fr[6] = {0}; USHORT k = RtlCaptureStackBackTrace(1, 6, fr, 0); char st[160] = ""; size_t L = 0;
        for (USHORT i = 0; i < k && L < sizeof st - 20; ++i) { uintptr_t a = (uintptr_t)fr[i]; L += (size_t)snprintf(st + L, sizeof st - L, "%s%llX", i ? " < " : "", (unsigned long long)(a >= B && a - B < 0x6000000 ? a - B : a)); }
        char tx[72] = "?"; if (s) { __try { int i = 0; for (; s[i] && i < 70; ++i) tx[i] = s[i] < 0x80 && s[i] >= 0x20 ? (char)s[i] : '.'; tx[i] = 0; } __except (EXCEPTION_EXECUTE_HANDLER) { strcpy(tx, "?"); } }
        LONG n = InterlockedIncrement(&g_dlg_n);
        if (block) {
            int boss = GateBossAlive() && !g_cfg_gate_always, r = -1; LONG nb = InterlockedIncrement(&g_dlg_blk);  // #13255
            if (!boss && !page && !recent) { g_gate.tIn = 0; g_gate.left = 0; r = GateUse(); }
            if (nb <= 60) Log("GATE: native end-mission box at DialogOpen 348274 BLOCKED #%ld ('%s', player %.0f u from the gate, page %d, page requested %lu ms ago, stack %s): %s", nb, tx, d, page,
                g_gate_tUse ? now - g_gate_tUse : 0, st, boss ? "boss alive, nothing opened" : page || recent ? "our gate page already open/opening" : r >= 0 ? "our gate page opened instead" : "?");
            __try { ((void (*)(void*))(B + Grva(0x310C18)))(text); ((void (*)(void*))(B + Grva(0x705218)))(a3); ((void (*)(void*))(B + Grva(0x7073FC)))(a4); }
            __except (EXCEPTION_EXECUTE_HANDLER) { Log("GATE: DialogOpen drop: arg destructor faulted %08lX", GetExceptionCode()); }
            return;
        }
        g_gate_nat_t = now ? now : 1;  // #13796 safety net: a native box is opening while our gate is up
        if (n <= 60) Log("GATE diag (#10022): DialogOpen 348274 NOT blocked ('%s', end-mission text %d, player %.0f u from the gate, page %d, stack %s)", tx, endm, d, page, st);
    }
    o_dlgopen(mgr, text, a3, a4, a5, a6, a7);
}
// notes/native-endmission-box-source.md (static RE, pc/native-box-source): the hooks above never fired because the box at the Return object is opened by
// the clear gadget's own SCRIPT (archive_16 G_CLEAR_GADGET: SubID 10/11/12 -> UI::OpenCommonWindowTwoButton(text 0x3E0C230 / variant 0x3E8D85E)),
// gadget-script native 0x23F8CC0(ctx) -> CommonWindowOpen 2806F80 -> DialogShow 348B90 (not 348274). Native body: reader 1399B0(rd, ctx);
// result global 0x44BAE44 = -1; arg i via 139904(rd, &i) (arg0 = text id); returns *(u16*)(rd+0x10C). The script then waits on UI::IsOpenCommonWindow and
// reads UI::GetButtonNumLastCommonWindow (= 0x44BAE44): 0 = Yes -> mission end, anything else -> back to its interaction wait. So while our gate is up,
// for the end-mission text from the gate gadget (current gadget [0x45BA228], type check 3624D4, SubID +0x110 == g_gate.id, or player < 600 u from the
// gate) the box is not opened, the result is set to -1 ('no button'), our gate page opens instead, and the native returns 0 values like the original.
// [Return] ReturnGateHookScriptBox (default 1).
typedef uint64_t (*Cw2Fn)(void*);
static Cw2Fn o_cw2; static int g_cfg_gate_cw2 = 1; static volatile LONG g_cw2_n;
static __attribute__((noinline)) int Cw2Peek(void* ctx, uint32_t* text, uint32_t* sub, uint64_t* cur, uint16_t* nret) {
    __try {
        __attribute__((aligned(16))) uint8_t rd[0x140]; memset(rd, 0, sizeof rd);
        ((void* (*)(void*, void*))(B + Grva(0x1399B0)))(rd, ctx);
        uint32_t i0 = 0; *text = ((uint32_t (*)(void*, uint32_t*))(B + Grva(0x139904)))(rd, &i0);
        *nret = *(uint16_t*)(rd + 0x10C);
        *cur = *(uint64_t*)(B + Grva(0x45BA228));
        uint64_t g = ((uint64_t (*)(uint64_t))(B + Grva(0x3624D4)))(*cur);
        *sub = g ? *(uint32_t*)(g + 0x110) : 0xFFFFFFFFu;
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static __attribute__((noinline)) uint64_t h_cw2(void* ctx) {
    if (g_cfg_gate_cw2 && g_cfg_gate && g_gate.up && g_tr.active && !g_rh.active) {
        uint32_t text = 0, sub = 0xFFFFFFFFu; uint64_t cur = 0; uint16_t nret = 0;
        if (Cw2Peek(ctx, &text, &sub, &cur, &nret) && (text == 0x3E0C230 || text == 0x3E8D85E)) {
            float q[4], d = -1.0f; if (PlayerPos(q)) { float mx = q[0] - g_gate.pos[0], mz = q[2] - g_gate.pos[2]; d = sqrtf(mx * mx + mz * mz); }
            int ours = sub == g_gate.id || (d >= 0.0f && d < 600.0f);
            LONG n = InterlockedIncrement(&g_cw2_n);
            if (ours) {
                DWORD now = GetTickCount();
                int boss = GateBossAlive() && !g_cfg_gate_always, page = GateMenuOpen(), recent = g_gate_tUse && now - g_gate_tUse < 1500, r = -1;
                if (!boss && !page && !recent) { g_gate.tIn = 0; g_gate.left = 0; r = GateUse(); }
                *(volatile int32_t*)(B + Grva(0x44BAE44)) = -1;  // GetButtonNumLastCommonWindow: no button -> the script re-arms its interaction
                if (n <= 60) Log("GATE: script end-mission box (OpenCommonWindowTwoButton 23F8CC0, text 0x%X, gadget %p sub %u, gate id %u, player %.0f u) BLOCKED #%ld: %s", text,
                    (void*)cur, sub, g_gate.id, d, n, boss ? "boss alive, nothing opened" : page || recent ? "our gate page already open/opening" : r >= 0 ? "our gate page opened instead" : "?");
                return nret;
            }
            g_gate_nat_t = GetTickCount() | 1;  // safety net: a native box is opening while our gate is up
            if (n <= 60) Log("GATE diag: script end-mission box NOT blocked (text 0x%X, gadget %p sub %u, gate id %u, player %.0f u from the gate)", text, (void*)cur, sub, g_gate.id, d);
        }
    }
    return o_cw2(ctx);
}
// user #6781: temporary gold boss Ki bar. Boss gauge Ki row: BossGaugeRowsUpdate 1D21EF8 calls KiBarStyle 5D4434(charParam) at 1D220C1 -> KiRowWidget 235F9A4 ->
// KiWidgetAnimate 235F40C plays bar style anim 0x20 (style 1) or 0x21 (style 0). The colour itself lives in the layout's VertexColor FCurves (no direct colour field
// found statically). This call-site hook only stamps 'boss Ki row drawn now' (native style kept); the tint itself is GoldQuad below (UiQuadBuild vertex colours).
static int g_cfg_goldki = 1; static volatile DWORD g_ki_seen; static int g_gold_hooked;
static __attribute__((noinline)) uint8_t KiStyleHook(uint64_t charParam) {
    uint8_t s = ((uint8_t (*)(uint64_t))(B + Grva(0x5D4434)))(charParam);
    static int logged; if (!logged) { logged = 1; int v = -1; __try { v = charParam ? *(int32_t*)(charParam + 0x80) : -1; } __except (EXCEPTION_EXECUTE_HANDLER) {} Log("GOLDKI: first boss Ki row: charParam+0x80 = %d, native style %d, GoldKiBar %d", v, s, g_cfg_goldki); }
    g_ki_seen = GetTickCount();  // the boss Ki row is being drawn: enables the vertex tint (GoldQuad)
    return s;
}
static void HookGoldKi(void) {
    uint8_t* site = (uint8_t*)(B + Grva(0x1D220C1));
    if (site[0] != 0xE8 || (uintptr_t)(site + 5) + *(int32_t*)(site + 1) != B + Grva(0x5D4434)) { Log("GOLDKI: call site 1D220C1 is not 'call 5D4434', not hooked"); return; }
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
// #7234: NO gameplay change. The round-3 probe (b61d665) held the boss Ki at 40 % for 1.5 s to find the bar: the user saw the boss stamina drop, and the probe
// still picked the wrong quad (a transparent 0.9 px element, log 22:50:58) because its 256-slot table was full before the Ki fill was drawn. Now a PASSIVE
// learner (e22edb6 idea): the boss Ki only changes by itself (attacks, guard, hits, regen); every visible quad drawn while the boss Ki row is up goes into
// a 4096-slot hash table; a quad whose width / its max width follows the boss Ki ratio (|r - Ki| < 0.06, Ki < 0.95, Ki not close to the HP ratio) on
// 3 separate Ki changes is the Ki fill -> repainted GoldKiRGB every frame. The boss Ki value is only READ ([actor+0x3A0]+0xAC / +0xB0, notes/boss-ki-bar-re).
// GoldKiColors (exact RGBA) also tints; GoldKiDump = 1 logs 120 walked quads once; a 'GOLDKI diag' line every 5 s while learning.
static volatile float g_ki_ratio = -1.0f, g_hp_ratio = -1.0f;
static int g_cfg_goldki_dump = 0, g_goldki_dumped, g_cfg_goldki_learn = 1; static uint32_t g_goldki_cols[8]; static int g_goldki_ncols;
static uint8_t g_gold_r = 255, g_gold_g = 200, g_gold_b = 50;
#define GK_N 4096
static struct { uint64_t e; float maxW, lastKi; int score; } g_gk[GK_N]; static int g_ngk;
static uint64_t g_gold_el[8]; static volatile LONG g_ngold;
static volatile int g_gk_best; static volatile uint64_t g_gk_bestEl; static volatile LONG g_gk_walks;
static __attribute__((noinline)) void GoldKiRatios(void) {  // pump: READ-ONLY Ki / HP ratios of the summoned boss (the gauge actor)
    uint64_t a = (g_cfg_goldki && g_nsp > 0) ? (uint64_t)g_sp_actor[g_nsp - 1] : 0;
    if (!a) { g_ki_ratio = -1.0f; return; }
    __try {
        uint64_t st = *(uint64_t*)(a + 0x3A0); if (!st) { g_ki_ratio = -1.0f; return; }
        float cur = *(float*)(st + 0xAC), mx = *(float*)(st + 0xB0); uint64_t hm = *(uint64_t*)(st + 0x50), hc = *(uint64_t*)(st + 0x58);
        float r = mx > 0 ? (cur < 0 ? 0 : cur / mx) : -1.0f; if (r > 1.0f) r = 1.0f;
        g_ki_ratio = r; g_hp_ratio = hm ? (float)((double)hc / (double)hm) : -1.0f;
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_ki_ratio = -1.0f; }
    static DWORD tDiag; DWORD now = GetTickCount();
    if (g_cfg_goldki_learn && !g_ngold && now - g_ki_seen < 250 && now - tDiag >= 5000) {
        tDiag = now; Log("GOLDKI diag (learning, boss Ki read only): Ki %.2f HP %.2f, quads tracked %d, walks %ld, best score %d (%p), gold elements %ld", g_ki_ratio, g_hp_ratio, g_ngk, g_gk_walks, g_gk_best, (void*)g_gk_bestEl, g_ngold);
    }
}
static void GoldPaint(uint8_t* q) {
    for (int v = 0; v < 4; ++v) { uint8_t* c = q + v * 0x2C + 0x24; int l = c[0] > c[1] ? c[0] : c[1]; if (c[2] > l) l = c[2]; if (l < 110) l = 110; c[0] = (uint8_t)(g_gold_r * l / 255); c[1] = (uint8_t)(g_gold_g * l / 255); c[2] = (uint8_t)(g_gold_b * l / 255); }
}
// #7465 KiBreakNoFinisher gold: the boss Ki fill quad, identified by its geometry + atlas strip (live dump 23:30 log 'GOLDKI vtx', pc/jumpfix2 GoldKiTest=5
// screenshot captures/goldki/k5_crop.png: only the Ki bar gold, HP bar untouched). Boss-bar quads are local-space x -440.5..440.5, y -6..6 (881 x 12);
// the bar width does not follow Ki (the fill is shader/UV driven, why the width learner never matched). Ki fill = atlas v 0.688..0.746 (u 0..0.645);
// HP / frame / back quads use v 0.000, 0.062, 0.188, 0.500, 0.625 strips. Vertex = 11 floats: x y z u v ..., RGBA at +0x24.
// Live 01:49 (Yamagata, native Ki break): while Ki-broken the Ki fill is NOT the 0.688 strip: the broken (pink) fill and the in-break regen fill use other strips
// (dump: 0.375 and 0.438 appear only while broken). The Ki-fill strips are a list (strip start v, height ~0.058), run-time 'nofinstrips <v*1000> ...'.
// Live 02:00 strip colour test (nofintest 1, Yamagata): boss bar = two quad groups per frame. Ki row: frame (v 0.000, u 0.212..0.856), 0.500 (the pink
// Ki-broken flash layer), 0.438 (only while broken), 0.625, 0.375 (the Ki fill, blue in normal state). HP row: frame, 0.188, 0.062 (HP fill). While gold:
// Ki-row strips are tinted gold and the pink 0.500 flash layer is made transparent (pink x gold reads salmon, live 02:01), HP row untouched.
// User play 2026-10-16 15:41: no gold on a boss with Ki bar style 0 (log 'GOLDKI: first boss Ki row: charParam+0x80 = 1, native style 0'). The strips above
// were found on Yamagata = style 1 (charParam+0x80 = 0). Style-0 Ki row (pc/jumpfix2 dump 23:30 'GOLDKI vtx' k0..k3): frame 0.000, 0.500, 0.625, Ki fill
// 0.688 (gold-verified by GoldKiTest=5, captures/goldki/k5_crop.png, user #7394); style 1 never draws 0.688 (all nofindump lines), HP row neither. -> 0.688 added.
static float g_nf_strip[8] = {0.375f, 0.438f, 0.625f, 0.688f}; static int g_nf_nstrip = 4; static float g_nf_hide = 0.500f; static volatile LONG g_nf_dumpN; static int g_nf_test;
static int NfStrip(const uint8_t* q, float want) {  // boss-bar quad (12 units high) on the atlas strip starting at v = want
    const float* f = (const float*)q; float y0 = f[1], y1 = f[22 + 1], v0 = f[4], v2 = f[22 + 4];
    float h = y0 > y1 ? y0 - y1 : y1 - y0, dv = v2 - v0, w = f[11] - f[0]; if (w < 0) w = -w;
    // size test by SHAPE (881 x 12 = 73:1), not absolute pixels: with a non-default resolution / UI scale the walked vertices are not 12 units high (user play 15:41,
    // gold Ki bar never painted although the NOFIN gold state ran 4 s)
    int shape = (h > 10.0f && h < 14.0f) || (h > 1.0f && w > 100.0f && w / h > 55.0f && w / h < 95.0f);
    return shape && dv > 0.045f && dv < 0.070f && v0 > want - 0.010f && v0 < want + 0.010f;
}
static int NfIsKiFill(const uint8_t* q) { for (int i = 0; i < g_nf_nstrip; ++i) if (NfStrip(q, g_nf_strip[i])) return 1; return 0; }
// Live 02:09: the Ki-broken fill texture is pink, so pink x gold reads orange-red. g_nf_uv: sample the white HP-fill strip instead (v 0.062, atlas column
// u + 0.212, same 0.645 width) so the gold tint shows as gold. Run time 'nofinuv 0|1'.
static int g_nf_uv = 1;
static void NfGoldUv(uint8_t* q) {
    float u0 = ((float*)q)[3], v0 = ((float*)q)[4], base = (float)(int)(v0 / 0.0625f + 0.5f) * 0.0625f, du = u0 < 0.1f ? 0.212f : 0.0f;  // column 0 -> HP column
    for (int v = 0; v < 4; ++v) { float* f = (float*)(q + v * 0x2C); f[3] += du; f[4] = f[4] - base + 0.0625f; }
}
static void GoldQuad(uint8_t* q, uint64_t el) {
    {   // test aid 'nofindump [n]': log the next n boss-bar-sized quads (geometry, u range of the top edge, v strip, colour, matcher verdict)
        const float* f = (const float*)q; float w = f[11] - f[0], hh = f[1] - f[23]; if (hh < 0) hh = -hh;
        // #8827: only bar-shaped quads (width > 100, w/h 20..150): the 16:09 auto dump spent its 36 lines on icons / squares before the boss bar rows
        if (g_nf_dumpN > 0 && (w > 100.0f || w < -100.0f) && hh > 0.5f && (w < 0 ? -w : w) / hh > 20.0f && (w < 0 ? -w : w) / hh < 150.0f && InterlockedDecrement(&g_nf_dumpN) >= 0)
            Log("NOFIN goldquad: x %.1f..%.1f y %.1f..%.1f u %.3f..%.3f v %.3f..%.3f rgba %08X kifill %d gold %d", f[0], f[11], f[1], f[23], f[3], f[14], f[4], f[26], *(uint32_t*)(q + 0x24), NfIsKiFill(q), g_nf_gold);
    }
    if (g_nf_test) {  // test aid 'nofintest 1': every boss-bar quad (881 x 12) coloured by its atlas strip, to see which strip is the Ki fill
        const float* f = (const float*)q; float w = f[11] - f[0], h = f[1] - f[23]; if (w < 0) w = -w; if (h < 0) h = -h;
        if (w > 100.0f && h > 1.0f && w / h > 55.0f && w / h < 95.0f) { int k = (int)(f[4] / 0.0625f + 0.5f) & 15;  // strip index: v 0.000=0, 0.062=1, 0.188=3, 0.375=6, 0.438=7, 0.500=8, 0.625=10, 0.688=11
            static const uint8_t pal[16][3] = {{0,255,255},{255,0,255},{128,128,128},{255,255,0},{255,128,0},{0,0,0},{0,0,255},{0,255,0},{255,0,0},{128,0,255},{255,255,255},{255,200,50},{0,128,0},{128,0,0},{0,0,128},{64,64,64}};
            for (int v = 0; v < 4; ++v) { uint8_t* c = q + v * 0x2C + 0x24; c[0] = pal[k][0]; c[1] = pal[k][1]; c[2] = pal[k][2]; } }
        return;
    }
    if (g_nf_gold) {
        if (g_nf_hide > 0 && NfStrip(q, g_nf_hide)) { for (int v = 0; v < 4; ++v) q[v * 0x2C + 0x24 + 3] = 0; return; }  // pink broken flash layer: alpha 0
        if (NfIsKiFill(q)) { if (g_nf_uv) NfGoldUv(q); GoldPaint(q); return; }
    } else if (g_nf_goldw > 0.0f) {  // #9133 blend back: native strip / UVs, vertex colour native -> gold by the weight, pink flash layer faded in
        float w = g_nf_goldw; if (w > 1.0f) w = 1.0f;
        if (g_nf_hide > 0 && NfStrip(q, g_nf_hide)) { for (int v = 0; v < 4; ++v) { uint8_t* c = q + v * 0x2C + 0x24; c[3] = (uint8_t)(c[3] * (1.0f - w)); } return; }
        if (NfIsKiFill(q)) { uint8_t org[16]; for (int v = 0; v < 4; ++v) memcpy(org + v * 4, q + v * 0x2C + 0x24, 4);
            GoldPaint(q); for (int v = 0; v < 4; ++v) { uint8_t* c = q + v * 0x2C + 0x24; for (int k = 0; k < 3; ++k) c[k] = (uint8_t)(org[v * 4 + k] + (c[k] - org[v * 4 + k]) * w); }
            return; }
    }
    if (!g_cfg_goldki) return;
    for (LONG i = 0; i < g_ngold; ++i) if (g_gold_el[i] == el) { GoldPaint(q); return; }
    uint32_t rgba = *(uint32_t*)(q + 0x24);
    for (int i = 0; i < g_goldki_ncols; ++i) if ((g_goldki_cols[i] & 0x00FFFFFF) == (rgba & 0x00FFFFFF)) { GoldPaint(q); return; }
    int dump = g_cfg_goldki_dump && g_goldki_dumped < 120;
    float ki = g_ki_ratio, hp = g_hp_ratio;
    if (!dump && (!g_cfg_goldki_learn || g_ngold >= 2 || ki < 0)) return;
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f; int alpha = 0;
    for (int v = 0; v < 4; ++v) { float x = *(float*)(q + v * 0x2C), y = *(float*)(q + v * 0x2C + 4); if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; if (q[v * 0x2C + 0x27]) alpha = 1; }
    if (dump) { g_goldki_dumped++; Log("GOLDKI dump: elem %p rgba %08X box %.1f %.1f - %.1f %.1f (Ki %.2f HP %.2f)", (void*)el, rgba, x0, y0, x1, y1, ki, hp); }
    float w = x1 - x0;
    if (!g_cfg_goldki_learn || g_ngold >= 2 || ki < 0 || !alpha || !(w >= 0.0f) || w > 1e5f || y1 - y0 < 2.0f) return;  // invisible / degenerate quads (the b61d665 mis-pick) never qualify
    int k = -1; uint32_t hsh = (uint32_t)((el >> 4) * 0x9E3779B1u) & (GK_N - 1);
    for (int n = 0; n < 32; ++n, hsh = (hsh + 1) & (GK_N - 1)) {
        if (g_gk[hsh].e == el) { k = (int)hsh; break; }
        if (!g_gk[hsh].e) { if (g_ngk >= GK_N * 3 / 4) return; k = (int)hsh; g_ngk++; g_gk[k].e = el; g_gk[k].maxW = w; g_gk[k].lastKi = -1; g_gk[k].score = 0; break; }
    }
    if (k < 0) return;
    if (w > g_gk[k].maxW) g_gk[k].maxW = w;
    if (g_gk[k].maxW < 8.0f || fabsf(ki - g_gk[k].lastKi) < 0.04f) return;  // judge once per Ki change of >= 4 %
    g_gk[k].lastKi = ki;
    if (ki > 0.95f || (hp >= 0 && fabsf(ki - hp) < 0.1f)) return;  // full Ki = every full-width quad matches; Ki near HP = the HP bar would match
    float r = w / g_gk[k].maxW;
    if (fabsf(r - ki) < 0.06f) {
        int sc = ++g_gk[k].score; if (sc > g_gk_best) { g_gk_best = sc; g_gk_bestEl = el; }
        if (sc >= 3 && g_ngold < 8) {
            for (LONG i = 0; i < g_ngold; ++i) if (g_gold_el[i] == el) return;
            g_gold_el[g_ngold] = el; InterlockedIncrement(&g_ngold);
            Log("GOLDKI: element %p follows the boss Ki ratio (width %.1f of %.1f, Ki %.2f, HP %.2f, rgba %08X, box y %.1f-%.1f): Ki bar, tinted gold (boss Ki untouched)", (void*)el, w, g_gk[k].maxW, ki, hp, rgba, y0, y1);
        }
    } else if (g_gk[k].score > 0) g_gk[k].score--;
}
static __attribute__((noinline)) void __cdecl GoldWalk(uint8_t* obj, uint8_t* base) {
    if ((!g_cfg_goldki && !g_nf_gold && g_nf_goldw <= 0.0f && g_nf_dumpN <= 0 && !g_nf_test) || GetTickCount() - g_ki_seen > 250) return;
    if (!g_nf_gold && g_nf_goldw <= 0.0f && g_nf_dumpN <= 0 && !g_nf_test && !g_ngold && !g_goldki_ncols && !(g_cfg_goldki_learn && g_ki_ratio >= 0) && !(g_cfg_goldki_dump && g_goldki_dumped < 120)) return;
    __try {
        // pc/jumpfix2 d86350e: walk from the alpha pass's real vertex base (rbp at 1CD7CD; obj+358h is only the map out-param = misaligned walk) and skip
        // non-quad nodes only at the START of a group, then cnt consecutive quads, as the native alpha pass does.
        uint8_t* q = base; if (!q) return;
        uint64_t node = *(uint64_t*)(obj + 0x28), sent = (uint64_t)obj + 0x60, grpObj = (uint64_t)obj + 0x100;
        uint64_t* gv = *(uint64_t**)grpObj;
        uint32_t ng = ((uint32_t (*)(uint64_t))gv[3])(grpObj);
        int guard = 0;
        for (uint32_t g = 0; g < ng && g < 4096; ++g) {
            uint64_t grp = ((uint64_t (*)(uint64_t, uint32_t))gv[1])(grpObj, g); if (!grp) return;
            uint32_t cnt = *(uint32_t*)(grp + 4);
            if (!cnt) continue;
            for (;;) {
                if (!node || node == sent || ++guard > 200000) return;
                uint64_t el = *(uint64_t*)(node + 0x10); if (!el) return;
                int type = ((int (*)(uint64_t))(*(uint64_t**)el)[1])(el);
                uint64_t wdg = ((uint64_t (*)(uint64_t))(B + Grva(0x1CEA08)))(el);
                if (!(type & ~4) && (!wdg || ((int (*)(uint64_t))(*(uint64_t**)wdg)[0x90 / 8])(wdg) != 1)) break;
                q += (uint64_t)*(uint32_t*)(node + 0x24) * 0x2C; node = *(uint64_t*)(node + 8);
            }
            while (cnt) {
                if (!node || node == sent || ++guard > 200000) return;
                uint64_t el = *(uint64_t*)(node + 0x10); if (!el) return;
                GoldQuad(q, el); q += 0xB0; node = *(uint64_t*)(node + 8); --cnt;
            }
        }
        InterlockedIncrement(&g_gk_walks);
    } __except (EXCEPTION_EXECUTE_HANDLER) { static int once; if (!once++) Log("GOLDKI: quad walk faulted %08lX", GetExceptionCode()); }
}
static void HookGoldQuad(void) {
    uint8_t* site = (uint8_t*)(B + Grva(0x1CD7CD));  // mov rax, [rip+X] (g_UiSettings 4BD0AC0); cmp byte [rax+368h], r12b
    if (site[0] != 0x48 || site[1] != 0x8B || site[2] != 0x05 || (uintptr_t)(site + 7) + *(int32_t*)(site + 3) != B + Grva(0x4BD0AC0)) { Log("GOLDKI: UiQuadBuild 1CD7CD bytes differ, vertex tint off"); return; }
    uint8_t* s = 0;
    for (uintptr_t a = ((uintptr_t)site - 0x70000000) & ~0xFFFFull; a < (uintptr_t)site + 0x70000000 && !s; a += 0x10000) s = (uint8_t*)VirtualAlloc((void*)a, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!s) { Log("GOLDKI: no near stub for 1CD7CD"); return; }
    uint8_t* p = s;
    static const uint8_t pre[] = {0x50, 0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53,  // push rax rcx rdx r8 r9 r10 r11 (rsp 16-aligned at the site)
                                  0x48, 0x83, 0xEC, 0x28,                                          // sub rsp, 28h
                                  0x48, 0x89, 0xF9,                                                // mov rcx, rdi (UiQuadBuild object)
                                  0x48, 0x89, 0xEA};                                               // mov rdx, rbp (vertex buffer base, as the alpha pass uses; pc/jumpfix2)
    memcpy(p, pre, sizeof pre); p += sizeof pre;
    *p++ = 0x48; *p++ = 0xB8; *(uint64_t*)p = (uint64_t)GoldWalk; p += 8; *p++ = 0xFF; *p++ = 0xD0;  // mov rax, GoldWalk; call rax
    static const uint8_t post[] = {0x48, 0x83, 0xC4, 0x28, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59, 0x58};
    memcpy(p, post, sizeof post); p += sizeof post;
    *p++ = 0x48; *p++ = 0xB8; *(uint64_t*)p = (uint64_t)(B + Grva(0x4BD0AC0)); p += 8;  // mov rax, &g_UiSettings
    *p++ = 0x48; *p++ = 0x8B; *p++ = 0x00;                                          // mov rax, [rax]
    *p++ = 0xFF; *p++ = 0x25; *(uint32_t*)p = 0; p += 4; *(uint64_t*)p = (uint64_t)(site + 7); p += 8;  // jmp 1CD7D4
    DWORD old; VirtualProtect(site, 16, PAGE_EXECUTE_READWRITE, &old);
    LONG64 o8 = *(volatile LONG64*)site, n8 = o8; uint8_t* nb = (uint8_t*)&n8; nb[0] = 0xE9; *(int32_t*)(nb + 1) = (int32_t)((intptr_t)s - (intptr_t)(site + 5));
    InterlockedCompareExchange64((volatile LONG64*)site, n8, o8);
    VirtualProtect(site, 16, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 16);
    uint64_t us = 0; uint8_t f368 = 0xFF; if (Rd64(B + Grva(0x4BD0AC0), &us) && us) f368 = *(uint8_t*)(us + 0x368);
    Log("GOLDKI: vertex tint hook at UiQuadBuild 1CD7CD on (gold %u %u %u, passive learn %d (boss Ki read only), colours %d, dump %d; alpha pass flag %u)", g_gold_r, g_gold_g, g_gold_b, g_cfg_goldki_learn, g_goldki_ncols, g_cfg_goldki_dump, f368);
}
static volatile LONG g_ticks;
static DWORD g_phase_ms = 500, g_lastTickT;
static struct { DWORD t0, last, lastTick, lastBeat, window; LONG ticks0, lastTicks; int hadPlayer, active; uint32_t id, lastReg; float lastPos[4]; } g_ms;
static uint32_t RegCount(void) {
    uint64_t cm = 0, c0 = 0; if (!Rd64(B + Grva(0x45B9E40), &cm) || !cm || !Rd64(cm, &c0) || !c0) return 0;
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
static __attribute__((noinline)) void Exec(char* line) {
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
            uint64_t cm = 0, c0 = 0; Rd64(B + Grva(0x45B9E40), &cm); Rd64(cm, &c0);
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
            uint64_t key = ((uint64_t (*)(uint64_t, uint32_t))(B + Grva(0x134314)))(0, (uint32_t)a[0]);
            uint64_t rm = 0; Rd64(B + Grva(0x45B9E60), &rm);
            uint64_t e = ((uint64_t (*)(uint64_t, uint64_t))(B + Grva(0x133440)))(rm, key);
            Log("CMD loaded %llu key=%llX entry=%p flag2c8=%u", a[0], key, (void*)e, e ? *(uint8_t*)(e + 0x2c8) : 255);
        } else if (!strcmp(op, "lreq")) {
            ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x3F9E58)))((uint32_t)a[0], (uint32_t)a[1], (uint32_t)a[2]);
            Log("CMD lreq 3F9E58(%llu,%llu,%llu) done", a[0], a[1], a[2]);
        } else if (!strcmp(op, "dyn")) {
            if (!PlayerPos(pos)) { Log("CMD dyn: no player"); return; }
            float fwd = n >= 6 ? (float)a[4] : 300.0f;
            __attribute__((aligned(16))) float p[4] = {pos[0] + fwd, pos[1], pos[2], 1.0f};
            uint32_t r = ((DynFn)(B + Grva(0x1C0C5E0)))((uint32_t)a[0], p, 0.0f, (uint32_t)a[1], (uint32_t)a[2], (uint32_t)a[3]);
            Log("CMD dyn a0=%llu a4=%llu a5=%llu a6=%llu at %.1f %.1f %.1f -> %u (0x%X)", a[0], a[1], a[2], a[3], p[0], p[1], p[2], r, r);
        } else if (!strcmp(op, "spawn")) {
            if (g_sp.active) { Log("CMD spawn: previous spawn %u still loading", g_sp.id); return; }
            g_sp.boss = 0; g_sp.tOwn = 0; g_sp.id = (uint32_t)a[0]; g_sp.dist = n >= 3 ? (float)a[1] : 400.0f; g_sp.owner = n >= 4 ? (uint32_t)a[2] : 0; g_sp.t0 = GetTickCount();
            uint64_t e = ResEntry(g_sp.id);
            if (!e || !*(uint8_t*)(e + 0x2c8)) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x3F9E58)))(g_sp.id, 2, 0); PoolAdd(g_sp.id); }
            Log("CMD spawn %u dist %.0f: %s", g_sp.id, g_sp.dist, e && *(uint8_t*)(e + 0x2c8) ? "resources already loaded" : "Load::Character requested");
            g_sp.active = 1; SpawnTick();
        } else if (!strcmp(op, "summon")) {  // summon <R-id|charId> [dist]: boss next to the player in the current world, no arena swap / load; HUD gauge, boss mode, boss BGM
            char tok[32] = {0}; float d = 400.0f; sscanf(line, "%*s %31s %f", tok, &d); SummonStart(tok, d);
        } else if (!strcmp(op, "slot")) {  // slot <0|1> <R-id>: the Training menu's summon A/B path (bridge 12666)
            char tok[32] = {0}; int sl = 0; sscanf(line, "%*s %d %31s", &sl, tok); uint32_t ms = (uint32_t)strtoul(tok[0] == 'R' || tok[0] == 'r' ? tok + 1 : tok, 0, 16);
            Log("CMD slot %d %s -> %d", sl, tok, Nioh3Training_SummonSlot(sl, ms));
        } else if (!strcmp(op, "slotdel")) { int sl = (int)a[0]; Log("CMD slotdel %d -> %d", sl, Nioh3Training_DespawnSlot(sl));
        } else if (!strcmp(op, "slotinfo")) { Log("CMD slotinfo: A chr %u, B chr %u, spawned %d", Nioh3Training_SlotChr(0), Nioh3Training_SlotChr(1), g_nsp);
        } else if (!strcmp(op, "repeat")) {  // repeat <chr|A|B> <id hex> [0|1]: Training menu 'Repeat move' path (bridge 12953)
            char w[16] = {0}, x[16] = {0}; int on = 1; sscanf(line, "%*s %15s %15s %d", w, x, &on);
            uint32_t chr = (w[0] >= 'A' && w[0] < 'A' + N3T_SLOTS) ? Nioh3Training_SlotChr(w[0] - 'A') : (uint32_t)strtoul(w, 0, 10);
            Log("CMD repeat chr %u 0x%lX %d -> %d", chr, strtoul(x, 0, 16), on, Nioh3Training_SetMoveRepeat(chr, (uint32_t)strtoul(x, 0, 16), on));
        } else if (!strcmp(op, "repgap")) { Log("CMD repgap %d -> %d", (int)a[0], Nioh3Training_SetRepeatGap((int)a[0], 0));  // bridge 12958
        } else if (!strcmp(op, "moveallow")) { Log("CMD moveallow chr %llu id 0x%llX %llu -> %d", a[0], a[1], a[2], Nioh3Training_SetMoveAllowed((uint32_t)a[0], (uint32_t)a[1], (int)a[2], 0));
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
        } else if (!strcmp(op, "dumpimg")) {  // #9928 RE aid: dumpimg <file>: writes the live (decrypted) image [B, B+SizeOfImage) to <file> for offline disassembly
            char fn[MAX_PATH] = ""; sscanf(line, "%*s %259s", fn);
            if (!fn[0]) { Log("CMD dumpimg: no file"); return; }
            FILE* f = fopen(fn, "wb"); if (!f) { Log("CMD dumpimg: cannot open %s", fn); return; }
            uint32_t sz = g_imgSize ? g_imgSize : *(uint32_t*)(B + *(uint32_t*)(B + 0x3C) + 0x18 + 0x38); size_t wr = 0;
            for (uint32_t o = 0; o < sz; o += 0x10000) { uint32_t c = sz - o < 0x10000 ? sz - o : 0x10000; static uint8_t buf[0x10000];
                __try { memcpy(buf, (void*)(B + o), c); } __except (EXCEPTION_EXECUTE_HANDLER) { memset(buf, 0, c); }
                wr += fwrite(buf, 1, c, f); }
            fclose(f); Log("CMD dumpimg: %zu of %u bytes -> %s (base %p)", wr, sz, fn, (void*)B);
        } else if (!strcmp(op, "moveblock")) {  // #9928: moveblock [id ...] (0x4E4E style, up to 6; no ids = clear)
            uint32_t ids[6]; int k = 0; for (int i = 0; i < n - 1 && i < 6; ++i) ids[k++] = (uint32_t)a[i]; MvSet(ids, k);
        } else if (!strcmp(op, "moveforce")) { if (n >= 2) MvForce((uint32_t)a[0]);
        } else if (!strcmp(op, "idlehold")) {
            if (n >= 2) Log("CMD idlehold %llu -> %d", a[0], Nioh3Training_SetIdleHold(a[0] != 0));
            Log("CMD idlehold status %d (0 OFF, 1 pending, 2 held, -1 failed)", Nioh3Training_GetIdleHold());
        } else if (!strcmp(op, "idlestat")) {
            Log("CMD idlehold status %d (0 OFF, 1 pending, 2 held, -1 failed)", Nioh3Training_GetIdleHold());
        } else if (!strcmp(op, "movestat")) { Log("CMD movestat: hooked %d, %d blocked, %ld select vetoes, %ld rerolls", g_mv_hooked, g_mv_nblock, (long)g_mv_vetoes, (long)g_mv_rerolls);
        } else if (!strcmp(op, "nodeath")) { PlayerSetNoDeath((int)a[0]);
        } else if (!strcmp(op, "recover")) { PlayerRecoverFull();
        } else if (!strcmp(op, "actors")) {
            uint64_t cur[1024]; int n = Actors(cur, 1024); uint64_t me = PlayerChar();
            Log("CMD actors: %d in table, player=%p", n, (void*)me);
            for (int i = 0; i < n; ++i) { float xyz[4]; memcpy(xyz, (void*)(cur[i] + 0x110), 16);
                Log("  actor %p%s%s handle %08X %08X pos %.0f %.0f %.0f", (void*)cur[i], cur[i] == me ? " PLAYER" : "", InSp(cur[i]) ? " SPAWNED" : "", *(uint32_t*)cur[i], *(uint32_t*)(cur[i] + 4), xyz[0], xyz[1], xyz[2]); }
        } else if (!strcmp(op, "despawn")) {
            char w[16] = "all"; sscanf(line, "%*s %15s", w);
            if (!strcmp(w, "all")) { DespawnAll("command"); SumNoDeathRelease("despawn all command"); }
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
            g_kill_act = live; ((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(B + Grva(0x786C84)))(comp, 0, 0, 0xFFFFFFFFull, drop ? 1 : 0, 1, 0, 0, 1); g_kill_act = 0;
            Log("CMD kill #%d: 786C84 called", idx);
        } else if (!strcmp(op, "nofin")) {  // #7465: nofin <0|1> [windowMs]: KiBreakNoFinisher at run time (hooks installed on first use)
            g_cfg_nofin = (int)a[0]; if (n >= 3 && a[1] >= 200) g_cfg_nofin_ms = (int)a[1];
            if (g_cfg_nofin && !g_gold_hooked) { HookGoldKi(); HookGoldQuad(); g_gold_hooked = 1; } if (g_cfg_nofin) NfInstallHooks();
            Log("CMD nofin: KiBreakNoFinisher %d window %d ms", g_cfg_nofin, g_cfg_nofin_ms);
        } else if (!strcmp(op, "kibreaknow")) {  // test aid: Ki break the watched boss now (KiSetCur to the floor), as a natural Ki break
            // the Ki regenerates within frames, so the prime is HELD (NfTick, up to 10 s or until the break) until a hit's Ki damage crosses 0 (live 01:42: a single prime was gone before the hit)
            uint64_t a0 = NfFirstBoss(), st = 0;
            if (a0 && Rd64(a0 + 0x3A0, &st) && st && NfPrimeHold(a0, 10000)) NfBreakNow(st, "command, held 10 s", a0); else Log("CMD kibreaknow: no boss");
        } else if (!strcmp(op, "nofinstrips")) {  // test aid: nofinstrips <v*1000> ...: Ki-row atlas strips painted gold (default 375 438 625 688)
            if (n >= 2) { g_nf_nstrip = 0; for (int i = 0; i < n - 1 && i < 6; ++i) g_nf_strip[g_nf_nstrip++] = (float)a[i] / 1000.0f; }
            Log("CMD nofinstrips: %d strips %.3f %.3f %.3f %.3f", g_nf_nstrip, g_nf_strip[0], g_nf_strip[1], g_nf_strip[2], g_nf_strip[3]);
        } else if (!strcmp(op, "nofintest")) { g_nf_test = (int)a[0]; if (g_nf_test && !g_gold_hooked) { HookGoldKi(); HookGoldQuad(); g_gold_hooked = 1; } Log("CMD nofintest %d", g_nf_test);
        } else if (!strcmp(op, "nofinuv")) { g_nf_uv = (int)a[0]; Log("CMD nofinuv %d", g_nf_uv);
        } else if (!strcmp(op, "nofinhide")) { g_nf_hide = (float)a[0] / 1000.0f; Log("CMD nofinhide %.3f (0 = off)", g_nf_hide);
        } else if (!strcmp(op, "nofintrace")) { NfTraceStart(n >= 2 ? (DWORD)a[0] : 4000);
        } else if (!strcmp(op, "nofinstagger")) { g_nf_stagger = (int)a[0]; Log("CMD nofinstagger %d (stub path %s)", g_nf_stagger, g_db_stub3 ? "available" : "OFF");
        } else if (!strcmp(op, "nofinfade")) { g_cfg_nf_fade = (int)a[0] < 0 ? 0 : (int)a[0]; Log("CMD nofinfade %d ms", g_cfg_nf_fade);
        } else if (!strcmp(op, "bossgrace")) { g_cfg_bossgrace = (int)a[0]; Log("CMD bossgrace %d ms", g_cfg_bossgrace);
        } else if (!strcmp(op, "nofinprime")) { g_nf_prime = (int)a[0]; Log("CMD nofinprime %d", g_nf_prime);
        } else if (!strcmp(op, "nofinnl")) { g_nf_nonlethal = (int)a[0]; Log("CMD nofinnl %d", g_nf_nonlethal);
        } else if (!strcmp(op, "nofinkneelall")) { g_nf_kneelall = (int)a[0]; Log("CMD nofinkneelall %d", g_nf_kneelall);
        } else if (!strcmp(op, "nofinbare")) { g_nf_barefix = (int)a[0]; Log("CMD nofinbare %d (bare Ki break -> cancelled + Ki primed, #9579)", g_nf_barefix);
        } else if (!strcmp(op, "nofinauto")) { g_nf_autostag = (int)a[0]; Log("CMD nofinauto %d (AUTO fallback stagger = learned hit reaction when no real stagger is learned, #23432)", g_nf_autostag);
        } else if (!strcmp(op, "nofinact")) { g_nf_actfix = (int)a[0]; Log("CMD nofinact %d (bad Ki-break action -> good action requested, #9895/#9937)", g_nf_actfix);
        } else if (!strcmp(op, "nofinaihold")) { g_nf_hp0aihold = (int)a[0]; Log("CMD nofinaihold %d", g_nf_hp0aihold);
        } else if (!strcmp(op, "nofinkneel")) { g_nf_kneel = (int)a[0]; Log("CMD nofinkneel %d", g_nf_kneel);
        } else if (!strcmp(op, "nofin101")) { g_nf_clear101 = (int)a[0]; Log("CMD nofin101 %d", g_nf_clear101);
        } else if (!strcmp(op, "nofinunbreak")) { g_nf_kirow_unbreak = (int)a[0]; Log("CMD nofinunbreak %d", g_nf_kirow_unbreak);
        } else if (!strcmp(op, "nofinsnap")) { NfSnap(0);
        } else if (!strcmp(op, "nofindiff")) { NfSnap(1);
        } else if (!strcmp(op, "nofindump")) { g_nf_dumpN = n >= 2 ? (LONG)a[0] : 24; Log("CMD nofindump %ld", g_nf_dumpN);
        } else if (!strcmp(op, "bossai")) {  // test aid: bossai <0|1>: watched boss AI off (668D74, AlgoActiveFlag false) / on (66982C) so a scripted pad can land hits
            uint64_t a0 = NfFirstBoss(); if (!a0) Log("CMD bossai: no boss");
            else { if (a[0]) ((void (*)(uint64_t))(B + Grva(0x66982C)))(a0); else ((void (*)(uint64_t))(B + Grva(0x668D74)))(a0); Log("CMD bossai %p -> %lld", (void*)a0, a[0]); }
        } else if (!strcmp(op, "nearboss")) {  // test aid: nearboss [cm]: warp the player cm (default 150) from the watched boss on the player's side, facing it
            uint64_t a0 = NfFirstBoss(); float bp[4] = {0}, pp[4] = {0}; float dcm = n >= 2 && a[0] ? (float)a[0] : 150.0f;
            if (a0 && PlayerPos(pp)) { __try { memcpy(bp, (void*)(a0 + 0x110), 16); } __except (EXCEPTION_EXECUTE_HANDLER) { a0 = 0; } }
            if (a0) { float dx = pp[0] - bp[0], dz = pp[2] - bp[2], l = sqrtf(dx * dx + dz * dz); if (l < 1) { dx = 1; dz = 0; l = 1; } dx /= l; dz /= l;
                float yaw = atan2f(-dx, -dz); int r = Warp(bp[0] + dx * dcm, bp[1], bp[2] + dz * dcm, yaw, 1); Log("CMD nearboss %p: warp %.0f cm -> %d", (void*)a0, dcm, r); } else Log("CMD nearboss: no boss");
        } else if (!strcmp(op, "bosshp")) {  // test aid: bosshp <hp>: set the watched boss HP (native HP set 66C188 on hp+0x38)
            uint64_t a0 = NfFirstBoss(), st = 0; if (a0 && Rd64(a0 + 0x3A0, &st) && st) { ((void (*)(uint64_t, uint64_t))(B + Grva(0x66C188)))(st + 0x38, a[0]); uint64_t c = 0, m = 0; ActorHp(a0, &c, &m); Log("CMD bosshp %p: HP now %llu / %llu", (void*)a0, c, m); } else Log("CMD bosshp: no boss");
        } else if (!strcmp(op, "deathcfg")) {  // #6185: deathcfg BossLoot DeathBlowRequired CutsceneOnKill (run-time INI override)
            if (n >= 4) { g_cfg_loot = (int)a[0]; g_cfg_deathblow = (int)a[1]; g_cfg_cutscene = (int)a[2]; }
            Log("CMD deathcfg: BossLoot %d DeathBlowRequired %d CutsceneOnKill %d", g_cfg_loot, g_cfg_deathblow, g_cfg_cutscene);
        } else if (!strcmp(op, "bbm")) { BossModeBegin((uint32_t)a[0], n >= 3 ? (int)a[1] : g_cfg_notify);
        } else if (!strcmp(op, "bgm")) { if (n >= 2 && a[0]) BgmStart((uint32_t)a[0]); else { g_bgm_on = 1; BgmStop("cmd"); }
        } else if (!strcmp(op, "bbmend")) { BossModeEnd("cmd"); g_bm.active = 0;
        } else if (!strcmp(op, "leave")) { Nioh3Training_LeaveArena();
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
        } else if (!strcmp(op, "gshow")) { GaugeElemVis(n >= 2 ? (int)a[0] : 1, n >= 3 ? (int)a[1] : 0, 1);
        } else if (!strcmp(op, "gauge")) {
            ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x1D115D4)))((uint32_t)a[0], 0, 0); Log("CMD gauge 0x%llX shown", a[0]);
        } else if (!strcmp(op, "ungauge")) {
            ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x1C208BC)))((uint32_t)a[0], 0, 0); Log("CMD ungauge 0x%llX", a[0]);
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
            g_kill_act = act; ((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(B + Grva(0x786C84)))(comp, 0, 0, 0xFFFFFFFFull, drop ? 1 : 0, 1, 0, 0, 1); g_kill_act = 0;
            Log("CMD killid: EnemyForceKill called");
        } else if (!strcmp(op, "fnowchr")) {  // #23451 fnowchr <charId>: Fight now for a boss character (its R/D row, else its S<chr> arena / field row); never a native request
            Log("CMD fnowchr %llu -> %d", a[0], Nioh3Training_FightNowChr((uint32_t)a[0]));
        } else if (!strcmp(op, "fnow")) {  // Fight now through the hooked MissionRequestById (as an add-on caller): swaps in place when the row is eligible, else the native load
            Log("CMD fnow mission 0x%llX", a[0]);
            int r = Nioh3Training_FightNow((uint32_t)a[0]);
            if (!r && GameState() && GameState()[2]) Log("CMD fnow: a load is running, nothing requested");
            else if (!r) { __attribute__((aligned(16))) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0}; Log("CMD fnow: not handled, native request"); ((ReqFn)(o_req ? (void*)o_req : (void*)(B + Grva(0x2239600))))((uint32_t)a[0], 1, (uint32_t)a[1], p); }
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
            Log("CMD cellcap: state %d cells %llu loaded %llu loading areas %llu release queue %llu map 1B48 %llu, imm %u/%u", StageState(), cn, cr, cf, cq, cm, *(uint8_t*)(B + Grva(0x31E0B4)), *(uint8_t*)(B + Grva(0x31E0C2)));
        } else if (!strcmp(op, "mworld")) {
            uint64_t db = *(uint64_t*)(B + Grva(0x45B9E30)); db = db ? *(uint64_t*)db : 0;
            for (int i = 0; i < n - 1; ++i) {
                uint64_t r = db ? ((uint64_t (*)(uint64_t, uint32_t))(B + Grva(0x377B38)))(db, (uint32_t)a[i]) : 0;
                Log("CMD mworld mission 0x%llX rec %p world 0x%02X", a[i], (void*)r, r ? *(uint8_t*)(r + 0x15C) : 0xFF);
            }
        } else if (!strcmp(op, "oworld")) {
            uint64_t mm = *(uint64_t*)(B + Grva(0x47514D0)), st = *(uint64_t*)(mm + 8);
            __attribute__((aligned(16))) uint8_t prm[32] = {0};
            ((void (*)(void*))(B + Grva(0x1BF7D68)))(prm);
            prm[7] = 0; prm[0xA] = 0; prm[0xE] = 1;               // WarpOtherWorld
            if (n >= 4 && a[2]) { prm[4] = 1; prm[0xF] = 1; }     // WarpOtherWorldWhiteOut
            ((void (*)(uint32_t, uint32_t, uint32_t))(B + Grva(0x3B28FC)))(2, 0, 0);  // same pre-call as the VM wrapper (GameMain+588 stop bytes)
            uint8_t journey = *(uint8_t*)(st + 0x10); uint32_t mode = *(uint32_t*)(st + 0x24);
            XsStart("oworld", (int)(uint8_t)a[0]);
            ((void (*)(uint8_t, uint8_t, uint32_t, uint32_t, void*))(B + Grva(0x22399B8)))((uint8_t)a[0], journey, (uint32_t)a[1], mode, prm);
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
            uint8_t r = ((uint8_t (*)(uint32_t, uint32_t, uint8_t, uint32_t))(B + Grva(0x1180740)))((uint32_t)a[0], 0, 0, 0);
            Log("CMD act %llu -> %u", a[0], r);
        } else if (!strcmp(op, "lchr")) {
            if (!g_loader) { Log("CMD lchr: loader unknown (no Load::Character seen yet)"); return; }
            ((LoadChrFn)(B + Grva(0xA42A7C)))(g_loader, (uint32_t)a[0]);
            Log("CMD lchr %llu done (loader %p)", a[0], (void*)g_loader);
        } else if (!strcmp(op, "mission")) {
            __attribute__((aligned(16))) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0};
            ((ReqFn)(o_req ? (void*)o_req : (void*)(B + Grva(0x2239600))))((uint32_t)a[0], (uint8_t)a[1], (uint32_t)a[2], p);  // always the native load (bypasses the arena-swap hook)
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

// #7290 Ki-break finisher detection (PASSIVE, log only; notes/ki-break-finisher.md). Never writes boss Ki/HP/state.
// When a boss's Ki (stamina) runs out it is "Ki broken" (StateTest 0x23: ctrl+0xA8 bit 41 or ctrl+0x1060 bit 0x3A, set by the hit-apply 327DEC at 3EFE0
// when the attack flags have bit 63; Ki exhausted = K+0x5C/+0x74/+0x7C > 0 or K+0x83, K = [actor+0x3A0]+0x98). The player can then Grapple it (the game's
// "Final Blow/Grapple" damage class). While the grapple/final-blow animation holds the boss, the death sensor 4892A8 states are on:
// StateTest 0x35 (current action data dword +0x18 bit 8), 0xB0 (same bit on the linked action ctrl+0x118) or 0x2C (ctrl+0x1068 bit 8 = runtime bit 0x48).
// The death blow at the HP floor uses the same states (log 12:30: sensor 1 for ~2.3 s at HP 1); a Ki-break grapple is the same hit at HP > 1.
// Every pump frame, for each actor that has a boss HP/Ki bar (gauge rows hud+0x3C0, 0x60 B, +4 id -> FindWrapper, as BossGaugeRowsUpdate 1D21EF8 does) plus the
// boss-mode actor, log: Ki-break start/end, finisher (grapple) state on/off and every HP hit during a break or a finisher state, with the raw bits for in-game RE.
static int g_cfg_kibreak_log = 1; static volatile LONG g_kb_lines;
#define KB_MAX 4
static struct { uint64_t a; int broken, fin, valid; uint64_t hp, hm; DWORD tBreak, tFin, tLastWin, seen; uint64_t dmgBreak; int hitsBreak, finInBreak, nBreaks; } g_kb[KB_MAX];
static int KbState(uint64_t ai, uint32_t id) { return ((uint8_t (*)(uint64_t, uint32_t))(B + Grva(0x4893B8)))(ai, id) ? 1 : 0; }
static void KbLog(const char* fmt, ...) {
    if (InterlockedIncrement(&g_kb_lines) > 400) return;
    char m[600]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap); Log("%s", m);
}
static __attribute__((noinline)) int KbCollect(uint64_t* out, int max) {
    int n = 0; uint64_t root = 0, hud = 0, r0 = 0, r1 = 0;
    if (Rd64(B + Grva(0x45C84A8), &root) && root && Rd64(root + 0xA8, &hud) && hud && Rd64(hud + 0x3C0, &r0) && Rd64(hud + 0x3C8, &r1) && r0 && r1 > r0 && (r1 - r0) / 0x60 <= 8) {
        for (uint64_t r = r0; r < r1 && n < max; r += 0x60) {
            uint32_t slot = 0, id = 0; __try { slot = *(uint32_t*)r; id = *(uint32_t*)(r + 4); } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
            if (slot >= 3 || !id) continue;
            uint64_t w = FindWrapper(id), a = 0; if (!w || !Rd64(w, &a) || !a) continue;
            int dup = 0; for (int i = 0; i < n; ++i) if (out[i] == a) dup = 1; if (!dup) out[n++] = a;
        }
    }
    if (g_bm.active && n < max) { uint64_t a = ActorByHandle(g_bm.id); int dup = 0; for (int i = 0; i < n; ++i) if (out[i] == a) dup = 1; if (a && !dup) out[n++] = a; }
    return n;
}
static void KbxStart(int s, uint64_t a, uint64_t ai, uint64_t ctrl, int nBreak, float kc, uint64_t hc, uint64_t hm, int s23, int exh, uint64_t w0, uint64_t wA8);
static void KbxTick(int s, uint64_t a, uint64_t ai, uint64_t ctrl, DWORD now, int broken);
// #10087 who sets the Ki-break bit 0x3A on a watched boss: A4F098(ctrl) = the hit-apply's 'Ki-break hit' (66FC28, hit flag bit 43, default break time)
// and 236DF4C(ctrl, s) = the action-event setter (2004E69, break time from the current action's event = the stagger action). Log only: caller,
// action id, Ki, bits before the call.
static void (*o_kbA4F)(uint64_t); static void (*o_kb236)(uint64_t, float); static volatile LONG g_kbset_n;
static uint32_t ActIdOf(uint64_t ctrl);
static __attribute__((noinline)) void KbSetLog(const char* which, uint64_t ctrl, float secs, uintptr_t ra) {
    for (int i = 0; i < KB_MAX; ++i) { uint64_t a = g_kb[i].a, ai = 0, c = 0, st = 0; if (!a || !Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &c) || c != ctrl) continue;
        if (InterlockedIncrement(&g_kbset_n) > 200) return;
        float kc = 0; uint64_t w0 = 0, wA8 = 0; __try { if (Rd64(a + 0x3A0, &st) && st) kc = *(float*)(st + 0xAC); w0 = *(volatile uint64_t*)(ctrl + 0x1060); wA8 = *(volatile uint64_t*)(ctrl + 0xA8); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        void* fr[3] = {0}; USHORT k = RtlCaptureStackBackTrace(1, 3, fr, 0); uint32_t f[3] = {0}; for (int j = 0; j < k && j < 3; ++j) f[j] = fr[j] ? (uint32_t)((uintptr_t)fr[j] - B) : 0;
        Log("KIBRK SET %s boss %p: from %X (%X %X %X), %.2f s, action 0x%X, Ki %.1f, bits1060 before %016llX A8 %016llX%s", which, (void*)a, (uint32_t)(ra - B), f[0], f[1], f[2], secs, ActIdOf(ctrl), kc, w0, wA8,
            (w0 & (1ull << 0x3A)) ? " (bit 0x3A already set)" : ""); return; }
}
static void KbSetA4F(uint64_t ctrl) { KbSetLog("A4F098 hit-apply", ctrl, -1.0f, (uintptr_t)__builtin_return_address(0)); o_kbA4F(ctrl); }
static void KbSet236(uint64_t ctrl, float s) { KbSetLog("236DF4C action event", ctrl, s, (uintptr_t)__builtin_return_address(0)); o_kb236(ctrl, s); }
// pc2 #9508: boss current-action logger (read only). Every pump frame (>= 40 ms) for each boss (KbCollect): id = native 358528(state) = dword at
// current-action state+E8 (FFFF = no current action); logs each change and buckets per (arena chr, id): samples, entries, total ms.
// Written to Nioh3Training_actions.tsv in the game folder every 10 s (chr, action id, entries, samples, ms) so boss move lists can be built by id.
static int g_cfg_actlog = 1;
#define AL_MAX 768
static struct { uint32_t chr, id; uint32_t entries, samples; DWORD ms; } g_al[AL_MAX]; static int g_al_n;
static struct { uint64_t a; uint32_t id; DWORD t, seen; int valid; } g_alw[KB_MAX];
static DWORD g_al_lastDump, g_al_lastTick; static int g_al_dirty; static volatile LONG g_al_lines;
static int AlSlot(uint32_t chr, uint32_t id) { for (int i = 0; i < g_al_n; ++i) if (g_al[i].chr == chr && g_al[i].id == id) return i; if (g_al_n >= AL_MAX) return -1; g_al[g_al_n].chr = chr; g_al[g_al_n].id = id; return g_al_n++; }
static void AlDump(void) {
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s/Nioh3Training_actions.tsv", g_dir);
    FILE* f = fopen(p, "wb"); if (!f) return;
    fprintf(f, "chr\taction_id\tentries\tsamples\tms\r\n");
    for (int i = 0; i < g_al_n; ++i) fprintf(f, "%u\t0x%X\t%u\t%u\t%lu\r\n", g_al[i].chr, g_al[i].id, g_al[i].entries, g_al[i].samples, (unsigned long)g_al[i].ms);
    fclose(f);
}
#include "../../boss_moves.h"
#include "../../move_catalog.h"  // pc2 #23907: live move catalogue (runtime game data over the optional hand tables)
static int g_cfg_handmoves = 1;  // [Debug] HandMoves: 1 = seed the catalogue with the hand tables (fallback), 0 = nothing by hand
// user #13786: one MOVE line per finished boss action of a catalogued boss (not capped like ACTLOG): what the boss actually did, its kind
// (attack / wind-up / link / twitch / movement from boss_moves.h), how long it really ran and whether a Training REPEAT forced it.
static volatile LONG g_mvlog_lines;
static void MoveDoneLog(uint32_t chr, uint32_t id, uint32_t next, DWORD ran) {
    int i = N3MoveFind(chr, id);
    if (id == 0xFFFFFFFF || InterlockedIncrement(&g_mvlog_lines) > 200000) return;
    if (i < 0) {  // a catalogued boss did an id the list does not have yet (other Ki / HP phase): logged so tools/gen_boss_moves.py can add it
        if (N3MoveFind(chr, 2) >= 0) Log("MOVE chr %u 0x%X NEW (not in the move list) ran %lu ms -> 0x%X", chr, id, (unsigned long)ran, next);
        return;
    }
    const N3Move* m = &kN3Moves[i];
    Log("MOVE chr %u 0x%X %c ran %lu ms (typ %u) -> 0x%X%s%s%s", chr, id, m->kind ? m->kind : '?', (unsigned long)ran, m->ms, next,
        *m->label ? " \"" : "", m->label, *m->label ? "\"" : "");
}
static __attribute__((noinline)) void ActLogTick(void) {
    if (!g_cfg_actlog) return;
    DWORD now = GetTickCount(); if (now - g_al_lastTick < 40) return; g_al_lastTick = now;
    uint64_t act[KB_MAX]; int n = KbCollect(act, KB_MAX);
    for (int k = 0; k < n; ++k) {
        uint64_t a = act[k], ai = 0, ctrl = 0; uint32_t id = 0xFFFFFFFF;
        if (!Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) continue;
        __try { id = ((uint32_t (*)(uint64_t))(B + Grva(0x358528)))(ctrl); } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        int s = -1; for (int i = 0; i < KB_MAX; ++i) if (g_alw[i].a == a) s = i;
        if (s < 0) { for (int i = 0; i < KB_MAX; ++i) if (!g_alw[i].a || now - g_alw[i].seen > 10000) { s = i; break; } if (s < 0) continue; memset(&g_alw[s], 0, sizeof g_alw[s]); g_alw[s].a = a; }
        uint32_t chr = g_ar.chr;
        if (g_alw[s].valid) {
            int b = AlSlot(chr, g_alw[s].id); if (b >= 0) { g_al[b].samples++; g_al[b].ms += now - g_alw[s].seen; }
        }
        if (!g_alw[s].valid || g_alw[s].id != id) {
            int b = AlSlot(chr, id); if (b >= 0) g_al[b].entries++;
            if (g_alw[s].valid) MoveDoneLog(chr, g_alw[s].id, id, now - g_alw[s].t);
            if (InterlockedIncrement(&g_al_lines) < 3000) Log("ACTLOG boss %p chr %u: action 0x%X (prev 0x%X, ran %lu ms)", (void*)a, chr, id, g_alw[s].valid ? g_alw[s].id : 0xFFFFFFFF, g_alw[s].valid ? now - g_alw[s].t : 0UL);
            g_alw[s].id = id; g_alw[s].t = now; g_al_dirty = 1;
        }
        g_alw[s].valid = 1; g_alw[s].seen = now;
    }
    if (g_al_dirty && now - g_al_lastDump > 10000) { g_al_lastDump = now; g_al_dirty = 0; AlDump(); }
}
// Boss-specific Training menu preferences, selection veto and commit backstop.
#define MV_CHASE_MAX (N3T_SLOTS + 1)  // user #14309: one chase entry per Training boss
static void AgSeqFace(uint64_t st);  // aggro.h, user #14546: a Stand-still sequence boss faces the player before its head is forced
#define MV_PREFORCE(st) AgSeqFace(st)
#include "move_toggle.h"
static void IhNativeAi(uint64_t actor, int on) { ((void (*)(uint64_t))(B + (on ? Grva(0x66982C) : Grva(0x668D74))))(actor); }
static uint32_t IhReadAction(uint64_t ctrl) { return ((uint32_t (*)(uint64_t))(B + Grva(0x358528)))(ctrl); }
#include "ih_alive.h"
static __attribute__((noinline)) int IhEligible(uint64_t actor, uint64_t ctrl) {
    (void)ctrl;
    uint64_t ai = 0, stats = 0; int why = 0;
    if (!IhAlive(actor) || !Rd64(actor + 0x3B0, &ai) || !ai || !Rd64(actor + 0x3A0, &stats) || !stats) return 0;
    __try {
        if (!BhScreenVisible(&why) || KbState(ai, 0x23) || KbState(ai, 0x35) || KbState(ai, 0xB0) || KbState(ai, 0x2C)) return 0;
        if (!(*(float*)(stats + 0xAC) > 0)) return 0; // also excludes an unflagged first Ki break and NaN
        if (*(float*)(stats + 0xF4) > 0 || *(float*)(stats + 0x10C) > 0 || *(float*)(stats + 0x114) > 0 || *(uint8_t*)(stats + 0x11B)) return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
#include "idle_hold.h"
static void DrForceNative(uint64_t ctrl, uint32_t id) { ((void (*)(uint64_t, uint64_t, uint64_t, float))(B + Grva(0x66C4A8)))(ctrl, (uint64_t)id, 0, 0.0f); }  // SetActionID forced transition (MvRepTick)
static void AgDrillFace(uint64_t a);  // aggro.h, user #14147: Stand still + aggro: the boss turns to the player before each forced move
#define DR_PREFORCE(a) AgDrillFace(a)
#define DR_GAP(chr) MvGapFor(chr)  // user #14309: per-boss gap
#define DR_SEQ(chr) Nioh3Training_GetMoveRepeatSeq(chr)  // user #14546: a sequence repeat is forced by MvRepTick, not the drill
#include "drill.h"  // user #13529: move drill in the Stand still stance
#include "aggro.h"  // user #13787: Boss aggro on me / Reset boss position
static int IhIdleStance(void) { return InterlockedCompareExchange(&g_ih_idle_stance, 0, 0) != 0; }
static int TrActive(void) { return g_tr.active; }
static int SlotIdleDefault(void) { return g_tr.active && g_tr.practice; }
static volatile LONG g_st_due, g_st_idle, g_st_armed, g_st_bmgen;
static void BossMatchDefaults(void) {
    if (g_tr.practice) { BossMatchRestore("Practice session"); return; }
    InterlockedExchange(&g_st_armed, 0); // discard an old Training timer, never replay it after a match
    InterlockedIncrement(&g_bm_gen);  // #59523: blocks / repeats / timer set from now on apply to this match
    if (!g_bmd_on) { g_bmd_on = 1; g_bmd_loot = g_cfg_loot; g_bmd_nofin = g_cfg_nofin; }
    Log("BOSS MATCH defaults (#38077): boss CPU-controlled, BossLoot %d -> 0, KiBreakNoFinisher %d -> 0 (restored when the match ends)", g_cfg_loot, g_cfg_nofin);
    g_cfg_loot = 0; g_cfg_nofin = 0;
}
// #38077: back to the user's own BossLoot / KiBreakNoFinisher after a Boss Match (an explicit menu change updates only its own stashed preference).
static void BossMatchRestore(const char* why) {
    if (!g_bmd_on) return;
    g_bmd_on = 0; g_cfg_loot = g_bmd_loot; g_cfg_nofin = g_bmd_nofin;
    Log("BOSS MATCH defaults restored (%s): BossLoot %d, KiBreakNoFinisher %d", why, g_cfg_loot, g_cfg_nofin);
}
// user #14304 (log 20:07:16 'IDLEHOLD stop new summon ... owned 1' -> the session boss got its AI back and attacked with the new one): a summon
// during a Training session hands the Stand-still session boss to the slot hold (SPH_TRAINING) instead of IhStop giving its AI back.
static void IhKeepForSummon(void) {
    // user #14309: after a new summon the session boss is no longer owned by the Idle hold (BossModeEnd stops it): from now on its stance is its
    // own slot's (g_slot_fight / slot hold), like every summoned boss. Idle (or getting idle) -> slot hold; Fight normally -> stays CPU.
    const int idle = g_ih.phase == 2 || (g_ih.phase == 1 && IhIdleStance());
    for (int s = 0; s < N3T_SLOTS; ++s) {
        if (!g_slot_ih[s]) continue;
        g_slot_ih[s] = 0;
        InterlockedExchange(&g_slot_fight[s], !idle);
        if (!idle) Log("IDLEHOLD new summon: session boss (slot %c) keeps Fight normally (its own stance from now on)", 'A' + s);
    }
    if (!g_tr.active || g_ih.phase != 2 || !g_ih.actor || !IhAlive(g_ih.actor)) {
        if (g_ih.phase == 1 && idle) { const int s = SlotOf(g_ih.actor); if (s >= 0) { g_ih.owns_ai = 0; InterlockedExchange(&g_slot_rehold[s], 1); } }
        return;
    }
    const uint64_t a = g_ih.actor;
    const int s = SlotOf(a);
    if (s >= 0) {
        if (g_agd.st) AgDrillEnd("new summon");
        g_ih.owns_ai = 0;  // IhStop (BossModeEnd) must not switch the AI back on
        InterlockedExchange(&g_ih_drill_ai, 0);
        memset(&g_sph[s], 0, sizeof g_sph[0]); g_sph[s].a = a; g_sph[s].t0 = GetTickCount(); g_sph[s].ms = SPH_TRAINING; g_sph[s].phase = 1; g_sph[s].nOff = 1; g_sph[s].sess = 1;
        if (BhAiOn(a) == 1) IhNativeAi(a, 0);
        Log("IDLEHOLD new summon: session boss %p (slot %c) stays in the Training idle stance (slot hold until you pick a move / Fight normally)", (void*)a, 'A' + s);
        return;
    }
    Log("IDLEHOLD new summon: session boss %p is in no slot, its Idle hold ends as before", (void*)a);
}
// user #14309: per-boss stance (Boss X page). Returns -2 empty slot, -1 stand still failed, 0 Fight normally (CPU), 1 getting ready to stand
// still, 2 standing still (AI held), 3 standing still but repeating a move (the AI is on only to play the repeat). The session boss before any
// summon is still owned by the Idle hold (+ drill), so its status is GetIdleHold's.
N3T_API int Nioh3Training_GetSlotStance(int s) {
    if (!SlotAlive(s)) return -2;
    if (g_slot_ih[s] && g_tr.active) {
        const int h = Nioh3Training_GetIdleHold();
        return h == 2 ? (MvRepeatId(g_slot_chr[s]) ? 3 : 2) : h;
    }
    if (g_sph[s].phase) return 2;
    if (InterlockedCompareExchange(&g_slot_fight[s], 0, 0)) return 0;
    if (MvRepeatId(g_slot_chr[s])) return 3;
    return InterlockedCompareExchange(&g_slot_rehold[s], 0, 0) ? 1 : 0;
}
// idle 1 = Stand still, 0 = Fight normally (CPU). 1 = done / queued, 0 = empty slot or no Training session for the session boss.
N3T_API int Nioh3Training_SetSlotStance(int s, int idle) {
    if (!SlotAlive(s)) return 0;
    if (g_slot_ih[s] && g_tr.active) { Log("STANCE Boss %c (session boss) -> %s", 'A' + s, idle ? "Stand still" : "Fight normally"); return Nioh3Training_SetIdleHold(idle != 0); }
    InterlockedExchange(&g_slot_fight[s], idle ? 0 : 1);
    if (idle) InterlockedExchange(&g_slot_rehold[s], 1);
    Log("STANCE Boss %c -> %s", 'A' + s, idle ? "Stand still" : "Fight normally");
    return 1;
}
// user #14313: timed switch of ALL Training bosses between Fight normally (CPU) and Stand still. Armed from the menu, fired on the game thread.

N3T_API int Nioh3Training_StanceTimer(int ms, int toIdle) {
    if (ms <= 0) { InterlockedExchange(&g_st_armed, 0); Log("STANCE timer cancelled"); return 0; }
    if (ms > 60000) ms = 60000;
    InterlockedExchange(&g_st_idle, toIdle != 0); InterlockedExchange(&g_st_due, (LONG)(GetTickCount() + (DWORD)ms));
    InterlockedExchange(&g_st_bmgen, MvBossMatch() ? g_bm_gen : 0); InterlockedExchange(&g_st_armed, 1);  // #59523: armed during a Boss Match = fires in it
    Log("STANCE timer: every Training boss -> %s in %d ms", toIdle ? "Stand still" : "Fight normally", ms);
    return 1;
}
// remaining ms (0 = not armed); *toIdle = the stance it switches to
N3T_API int Nioh3Training_GetStanceTimer(int* toIdle) {
    if (toIdle) *toIdle = (int)g_st_idle;
    if (!InterlockedCompareExchange(&g_st_armed, 0, 0)) return 0;
    const int left = (int)((DWORD)g_st_due - GetTickCount());
    return left > 0 ? left : 1;
}
static void StanceTimerTick(void) {
    if (MvBossMatch() && g_st_bmgen != g_bm_gen) { InterlockedExchange(&g_st_armed, 0); return; }  // an old Training timer never fires in a match
    if (!InterlockedCompareExchange(&g_st_armed, 0, 0) || (int)((DWORD)g_st_due - GetTickCount()) > 0) return;
    InterlockedExchange(&g_st_armed, 0);
    const int idle = (int)g_st_idle; int n = 0;
    for (int s = 0; s < N3T_SLOTS; ++s) if (SlotAlive(s)) n += Nioh3Training_SetSlotStance(s, idle) > 0;
    Log("STANCE timer fired: %d boss(es) -> %s", n, idle ? "Stand still" : "Fight normally");
}
static int MvForce(uint32_t id) {
    uint64_t act[KB_MAX]; int n = KbCollect(act, KB_MAX); if (n < 1) { Log("MOVEFORCE: no boss"); return 0; }
    uint64_t ai = 0, st = 0; if (!Rd64(act[0] + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &st) || !st) { Log("MOVEFORCE: no state"); return 0; }
    uint32_t pend = *(uint32_t*)(st + 0x15C4); int32_t busy = *(int32_t*)(st + 0x788), cur = ((uint32_t (*)(uint64_t))(B + Grva(0x358528)))(st);
    if (pend != 0xFFFF || busy != -1) { Log("MOVEFORCE 0x%X refused: pending 0x%X, +788 %d, current 0x%X", id, pend, busy, cur); return 0; }
    *(uint32_t*)(st + 0x15C8) = 0; *(float*)(st + 0x15CC) = 0.0f; *(uint32_t*)(st + 0x15C4) = id;
    Log("MOVEFORCE 0x%X written (state %p, was current 0x%X)", id, (void*)st, cur); return 1;
}
static __attribute__((noinline)) void KiBreakTick(void) {
    if (!g_cfg_kibreak_log) return;
    {   static int once; if (!once) { once = 1;
            if (!memcmp((void*)(B + Grva(0xA4F098)), "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\x48\xB8", 11)) { o_kbA4F = (void (*)(uint64_t))Hook(0xA4F098, 19, (void*)KbSetA4F); Log("KIBRK SET hook A4F098: on"); } else Log("KIBRK SET hook A4F098: bytes differ, off");
            if (!memcmp((void*)(B + Grva(0x236DF4C)), "\x40\x53\x48\x83\xEC\x30\x0F\x29\x74\x24\x20\x48\x8B\xD9", 14)) { o_kb236 = (void (*)(uint64_t, float))Hook(0x236DF4C, 14, (void*)KbSet236); Log("KIBRK SET hook 236DF4C: on"); } else Log("KIBRK SET hook 236DF4C: bytes differ, off"); } }
    uint64_t act[KB_MAX]; int n = KbCollect(act, KB_MAX); DWORD now = GetTickCount();
    for (int k = 0; k < n; ++k) {
        uint64_t a = act[k], st = 0, ai = 0, ctrl = 0, hc = 0, hm = 0;
        if (!Rd64(a + 0x3A0, &st) || !st || !Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) continue;
        if (!ActorHp(a, &hc, &hm) || !hm) continue;
        int s = -1; for (int i = 0; i < KB_MAX; ++i) if (g_kb[i].a == a) s = i;
        if (s < 0) { for (int i = 0; i < KB_MAX; ++i) if (!g_kb[i].a || now - g_kb[i].seen > 10000) { s = i; break; } if (s < 0) continue; memset(&g_kb[s], 0, sizeof g_kb[s]); g_kb[s].a = a; }
        float kc = 0, km = 0, x5c = 0, x74 = 0, x7c = 0; uint8_t x83 = 0; uint64_t w0 = 0, w1 = 0, wA8 = 0; int s23 = 0, s35 = 0, sB0 = 0, s2C = 0, dead = 0;
        __try {
            kc = *(float*)(st + 0xAC); km = *(float*)(st + 0xB0); x5c = *(float*)(st + 0x98 + 0x5C); x74 = *(float*)(st + 0x98 + 0x74); x7c = *(float*)(st + 0x98 + 0x7C); x83 = *(uint8_t*)(st + 0x98 + 0x83);
            dead = *(int32_t*)(st + 0x48) == 1; w0 = *(uint64_t*)(ctrl + 0x1060); w1 = *(uint64_t*)(ctrl + 0x1068); wA8 = *(uint64_t*)(ctrl + 0xA8);
            s23 = KbState(ai, 0x23); s35 = KbState(ai, 0x35); sB0 = KbState(ai, 0xB0); s2C = KbState(ai, 0x2C);
        } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        int exh = x5c > 0 || x74 > 0 || x7c > 0 || x83, broken = s23 || exh, fin = s35 | (sB0 << 1) | (s2C << 2);
        if (dead) { g_kb[s].seen = now; g_kb[s].hp = hc; g_kb[s].broken = broken; g_kb[s].fin = fin; continue; }
        if (!g_kb[s].valid) { g_kb[s].valid = 1; g_kb[s].hp = hc; g_kb[s].hm = hm; g_kb[s].broken = broken; g_kb[s].fin = fin; g_kb[s].seen = now;
            KbLog("KIBREAK watch boss %p HP %llu/%llu Ki %.0f/%.0f (broken %d, fin %d)", (void*)a, hc, hm, kc, km, broken, fin); continue; }
        g_kb[s].seen = now;
        if (broken && !g_kb[s].broken) {
            g_kb[s].tBreak = now; g_kb[s].dmgBreak = 0; g_kb[s].hitsBreak = 0; g_kb[s].finInBreak = 0; g_kb[s].nBreaks++;
            KbLog("KIBREAK start #%d boss %p: Ki %.1f/%.1f HP %llu/%llu (state23 %d exhausted %d [5C %.2f 74 %.2f 7C %.2f 83 %u]) bits1060 %016llX 1068 %016llX A8 %016llX",
                  g_kb[s].nBreaks, (void*)a, kc, km, hc, hm, s23, exh, x5c, x74, x7c, x83, w0, w1, wA8);
            KbxStart(s, a, ai, ctrl, g_kb[s].nBreaks, kc, hc, hm, s23, exh, w0, wA8);
        }
        KbxTick(s, a, ai, ctrl, now, broken);
        if (fin && !g_kb[s].fin) {
            g_kb[s].tFin = now; int inBreak = broken || g_kb[s].broken || (g_kb[s].tLastWin && now - g_kb[s].tLastWin < 1500);
            if (inBreak) g_kb[s].finInBreak++;
            KbLog("KIFINISHER state on boss %p: 35=%d B0=%d 2C=%d, HP %llu/%llu, Ki %.1f/%.1f, %s bits1060 %016llX 1068 %016llX",
                  (void*)a, s35, sB0, s2C, hc, hm, kc, km, hc <= 1 ? "HP floor (death blow)" : inBreak ? "Ki broken -> GRAPPLE (Ki-break finisher)" : "not Ki broken (down/other finisher)", w0, w1);
        }
        if (hc < g_kb[s].hp) {
            uint64_t d = g_kb[s].hp - hc; int win = broken || fin || g_kb[s].broken || g_kb[s].fin || (g_kb[s].tLastWin && now - g_kb[s].tLastWin < 1000);
            if (win || d * 10 >= hm) {  // + any single hit >= 10 % max HP outside a break (finisher signature, live 01:56: 34586 in one hit)
                const char* kind = !win ? "BIG HIT outside a Ki break" : (g_kb[s].hp <= 1 || hc <= 1) ? "FINAL BLOW (HP floor)" : (fin || g_kb[s].fin) ? (broken || g_kb[s].broken || g_kb[s].finInBreak ? "KI-BREAK FINISHER (grapple)" : "finisher-state hit") : "hit on Ki-broken boss";
                if (broken || g_kb[s].broken) { g_kb[s].dmgBreak += d; g_kb[s].hitsBreak++; }
                KbLog("KIFINISHER hit boss %p: %s dmg %llu (%.1f %% max) HP %llu -> %llu/%llu, Ki %.1f/%.1f, 23=%d 35=%d B0=%d 2C=%d, +%lu ms since break, bits1060 %016llX 1068 %016llX",
                      (void*)a, kind, d, 100.0 * (double)d / (double)hm, g_kb[s].hp, hc, hm, kc, km, s23, s35, sB0, s2C, g_kb[s].tBreak ? now - g_kb[s].tBreak : 0, w0, w1);
            }
        }
        if (fin && !broken && g_kb[s].broken) g_kb[s].tLastWin = now;
        if (!fin && g_kb[s].fin) { g_kb[s].tLastWin = now; KbLog("KIFINISHER state off boss %p after %lu ms, HP %llu/%llu", (void*)a, now - g_kb[s].tFin, hc, hm); }
        if (!broken && g_kb[s].broken) {
            g_kb[s].tLastWin = now;
            KbLog("KIBREAK end #%d boss %p after %lu ms: %d hits, %llu dmg (%.1f %% max) during the break, grapple %s, Ki %.1f/%.1f",
                  g_kb[s].nBreaks, (void*)a, now - g_kb[s].tBreak, g_kb[s].hitsBreak, g_kb[s].dmgBreak, 100.0 * (double)g_kb[s].dmgBreak / (double)hm, g_kb[s].finInBreak ? "yes" : "no", kc, km);
        }
        g_kb[s].hp = hc; g_kb[s].hm = hm; g_kb[s].broken = broken; g_kb[s].fin = fin;
    }
}

// ---- #7465 / #7467 [Summon] KiBreakNoFinisher (default 0). notes/ki-break-nofinisher.md.
// (1) No grapple finisher on a watched boss (every boss with an HP/Ki bar + the boss-mode boss): the grapple is gated by StateTest 0x23 ("Ki broken"
//     window = ctrl+0x1060 bit 0x3A with its own timer ctrl+0x1014, set at the break by A4F098, ended by a hit whose attack flags have bit 63 at 32EEDC)
//     on the TARGET in exactly two places:
//       - CanGrapple A5F7F8(self, target): 0x23 on target, or target == self+0xB68 (grab target). Called from the action-condition switch F68D8 (case at
//         F9E3B, target = [actor+0xA90]) = the player's move-table condition that turns the attack input into the grapple. Hooked at its entry: false for
//         a watched boss.
//       - hit filter 117A6D0 (attack box vs victim): grab box (+0x4C bit0|bit7) connects only if victim 0x23 (vcall at 117A9EE) or victim is the grab
//         target; flag +0x48 bit 31 = box hits only 0x23 victims (117AC17); bit 28 = box skips 0x23 victims (117ABF7). For a watched boss: grab box rejected,
//         bit-31 box misses, bit-28 box hits (the normal hit of the attack lands, so the combo goes on).
//     The boss's own break (stagger, Ki state machine 844674, damage-vs-winded multipliers 78CE8C, Ki-damage immunity 2F8884, gauge flash) still reads
//     the real 0x23: no other timing change.
// (2) HP floor: a lethal hit on a boss that needs a death blow goes through 327DEC's cond 0x27 test (329C9F; true -> HP 1 + death-blow pending). For a
//     watched boss the test is reported true (so it never dies on the hit) and every lethal hit stamps the boss's hit time. NfTick: at HP <= 1 the boss
//     is Ki-broken (KiSetCur 2F88D0(K, -max), clamped to the native floor; re-broken if it recovers), its Ki row shows a gold bar draining over
//     KiBreakHp0WindowMs (display only: the Ki snapshot handed to KiRowWidget 235F9A4 at 1D220E3), and when no hit lands for that long: native force
//     kill (ForceKillActorEx, drop per BossLoot). Each hit refills the bar and restarts the window.
// (3) Gold: GoldQuad paints the Ki fill quad (NfIsKiFill) while any watched boss is Ki-broken (0x23 or Ki exhausted) or in the HP-0 window.
#define NF_N 4
#define NF_FLOOR (g_nf_nonlethal ? 2ull : 1ull)  // #9319: HP floor of the HP-0 window (2 with KiBreakHp0NonLethal)
static volatile uint64_t g_nf_act[NF_N], g_nf_ai[NF_N], g_nf_ctrl[NF_N];  // read by the asm stubs (0 = free)
static volatile DWORD g_nf_hitT[NF_N]; static volatile LONG g_nf_hits[NF_N];
static volatile LONG g_nf_cnt_sel, g_nf_cnt_grab, g_nf_cnt_b31, g_nf_cnt_b28, g_nf_cnt_cg; static volatile uint64_t g_nf_cg_tgt;  // + every CanGrapple call / its last target (diag)  // asm stub counters: grapple selection / grab box / bit-31 box / bit-28 box
static struct { volatile uint32_t id; volatile int on; volatile float ratio, w; } g_nf_drain[NF_N];  // w: 1 = gold override, 0..1 = blend back (#9133)
static struct { uint64_t a; uint32_t id; int win, broken, tries; DWORD tWin, tLast, tRebreak, tKill, seen; LONG hits0; int nRebreak, nHits, nSet, kneeled; DWORD tSet; DWORD tFade, tGoldEnd; int goldWas; float fadeFrom; int aiHeld, nAiRe; DWORD tAi, tDiag; DWORD tBare; int nBare; uint32_t brkAct; DWORD tBrk, tSwap, tFixLast; int nFix; } g_nf[NF_N];
static uint32_t g_nf_fbId[NF_N];  // user #23432: the AUTO fallback stagger id last forced on this slot (BareFix waits while it plays, KiBreakAutoHoldMs)
static int g_nf_hooked; static volatile LONG g_nf_lines;
static void NfLog(const char* fmt, ...) { if (InterlockedIncrement(&g_nf_lines) > 800) return; char m[600]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap); Log("%s", m); }
static uint8_t* NearPage(void* site) {
    uint8_t* s = 0;
    for (uintptr_t a = ((uintptr_t)site - 0x70000000) & ~0xFFFFull; a < (uintptr_t)site + 0x70000000 && !s; a += 0x10000) s = (uint8_t*)VirtualAlloc((void*)a, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    return s;
}
static uint8_t* E_mov_rax(uint8_t* p, uint64_t v) { *p++ = 0x48; *p++ = 0xB8; memcpy(p, &v, 8); return p + 8; }
static uint8_t* E_inc(uint8_t* p, volatile LONG* c) { p = E_mov_rax(p, (uint64_t)c); *p++ = 0xF0; *p++ = 0xFF; *p++ = 0x00; return p; }  // lock inc dword [rax]
// cmp <reg>, [rax + 8*i] for the 4 slots, je <label>; reg: 1 = rcx, 2 = rdx. Returns the je displacement bytes to patch.
static uint8_t* E_cmp4(uint8_t* p, int reg, uint8_t** je) {
    for (int i = 0; i < NF_N; ++i) { *p++ = 0x48; *p++ = 0x3B; *p++ = (uint8_t)(0x40 | (reg << 3)); *p++ = (uint8_t)(i * 8); *p++ = 0x74; je[i] = p; *p++ = 0; }
    return p;
}
static void E_bind(uint8_t** je, int n, uint8_t* target) { for (int i = 0; i < n; ++i) *je[i] = (uint8_t)(target - (je[i] + 1)); }
static int NfPatchCall(uint8_t* site, uintptr_t expect, uint8_t* stub, const char* what) {  // retarget an E8 rel32 call
    if (site[0] != 0xE8 || (uintptr_t)(site + 5) + *(int32_t*)(site + 1) != expect) { Log("NOFIN hook %s at %llX: bytes differ, not hooked", what, (unsigned long long)((uintptr_t)site - B)); return 0; }
    int32_t rel = (int32_t)((intptr_t)stub - (intptr_t)(site + 5)); DWORD old; VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    InterlockedExchange((volatile LONG*)(site + 1), rel); VirtualProtect(site, 5, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 5);
    return 1;
}
static __attribute__((noinline)) uint8_t NfLethalCond(uint64_t ai, uint32_t id) {  // 329C9F: 327DEC 'needs death blow' (cond 0x27) on a lethal hit
    uint8_t r = ((uint8_t (*)(uint64_t, uint32_t))(B + Grva(0x4893B8)))(ai, id);
    if (!g_cfg_nofin || !ai) return r;
    for (int i = 0; i < NF_N; ++i) if (g_nf_ai[i] == ai) {
        { static LONG nl; if (++nl <= 60) Log("NOFIN lethal hit on boss slot %d -> survive at HP 1 (hits counted by the hit probe 326E81)", i); }
        // the break: this check runs BEFORE the hit's KiDamage 844604 (327DEC); with the Ki primed to 0.5 the same hit's Ki damage crosses 0 = the native Ki break
        __try { uint64_t a = *(uint64_t*)ai, st = a ? *(uint64_t*)(a + 0x3A0) : 0;
                if (st && !((uint8_t (*)(uint64_t))(B + Grva(0x844FE4)))(st + 0x98) && *(float*)(st + 0xAC) > 0.5f) ((void (*)(uint64_t, float))(B + Grva(0x2F88D0)))(st + 0x98, 0.5f); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        return 2;  // survive at HP 1 via the native no-death path 329D1A (the asm stub redirects). NOT the death-blow branch: live 03:22 that set
                   // bit 46 -> finisher-down state 2C, the boss was not Ki-broken and later hits never reached this check (window ran out unhit)
    }
    return r;
}
// 2F7045: the player ctrl's per-frame target update (2F6EC8) fills the GRAPPLE target slot ctrl+0xB68 from FindGrappleTarget 867258 (27FA04 nearest-target
// search, range param 0x9A2D, filter 1174A60 = !StateTest 9 && StateTest 0x23 on the candidate). Live trace 03:13 (nofintrace): the LB grapple on a
// Ki-broken boss goes through this filter (vcall at 27FC6C, id 0x23), CanGrapple A5F7F8 is never called. No grapple target = LB grapple never starts.
static volatile LONG g_nf_cnt_tgt;
// user #8894 (Yoritomo play 16:29-16:30): the finisher still came on the staggers. The per-frame target update 2F6EC8 fills five target slots from five
// finders (static RE re/Nioh3.dump.exe): B50 <- 8671D4 (filter 1174A10 = StateTest 9, a DOWNED target), B68 <- 867258 (grapple, StateTest 0x23, hooked
// above), B80 <- 8670D4, B98 <- 867034, BE0 <- 866F24. The finisher on a downed / HP-floor boss (KIFINISHER 35=1 'HP floor' at 16:30:50 while the grapple
// slot was blocked 392 frames) is the B50 'down' target. While a watched boss's Ki bar is gold (stagger or HP-0 window) it is removed from that slot too.
static volatile uint64_t g_nf_goldact[4]; static volatile LONG g_nf_cnt_down;
// user #9135 (play 20:41, Yoritomo): the finisher (KIFINISHER 35=1 at HP 1) still came while the Ki bar was gold, with the grapple-target block at 362
// frames and down target / CanGrapple / grab box all 0, so it did not use any blocked slot (notes/grapple-finisher-paths-9133.md). Two more gates for a
// gold boss (g_nf_goldact: break, HP-0 window, HP floor until dead):
//  (a) grab boxes WITHOUT the Ki-broken flag (+0x4C bit 0 but not bit 7) jump past the 117A9EE hook (117A9AC 'test al,al; jns 117AA2A') and only
//      need the attacker's grab target; the stub at 117A9AC rejects (117C6ED) every grab box whose victim (rbx) is a gold boss.
//  (b) StateTest 0x27 ('needs death blow' = 2A3AC0(ctrl) || ctrl+0xA8 bit 45, the HP-floor death-blow offer) answers false for a gold boss, except to
//      the two native damage functions that use it as their revive test (E43BF2 / 21FAEFA; the other two sites, 329C9F and 787004, are our stubs).
//      Detour on the StateTest default implementation 2E9948 (same hook as the nofintrace aid). [Summon] KiBreakBlockDeathBlow = 1 (default).
static volatile uint64_t g_nf_goldctrl[4]; static volatile LONG g_nf_cnt_grab0, g_nf_cnt_st27; static int g_nf_st27 = 1;
static uint64_t NfDownTarget(uint64_t ctrl, uint8_t flag) {
    uint64_t t = ((uint64_t (*)(uint64_t, uint8_t))(B + Grva(0x8671D4)))(ctrl, flag);
    if (t && g_cfg_nofin) for (int i = 0; i < 4; ++i) if (g_nf_goldact[i] == t) { InterlockedIncrement(&g_nf_cnt_down); return 0; }
    return t;
}
static uint64_t NfGrappleTarget(uint64_t ctrl, uint8_t flag) {
    uint64_t t = ((uint64_t (*)(uint64_t, uint8_t))(B + Grva(0x867258)))(ctrl, flag);
    if (t && g_cfg_nofin) for (int i = 0; i < NF_N; ++i) if (g_nf_act[i] == t) { InterlockedIncrement(&g_nf_cnt_tgt); return 0; }
    return t;
}
// 326E81: hit processing 326C94 tests the VICTIM's StateTest 0x23 once per landed hit (live trace 03:13: n = number of hits, incl. the zero-damage hits at
// HP 1). Used as the HP-0 window's hit counter (the lethal-hit check 329C9F is no longer reached once the boss survives at HP 1, live 03:32).
static uint8_t NfHitProbe(uint64_t ai, uint32_t id) {
    uint8_t r = ((uint8_t (*)(uint64_t, uint32_t))(B + Grva(0x4893B8)))(ai, id);
    if (g_cfg_nofin && ai) for (int i = 0; i < NF_N; ++i) if (g_nf_ai[i] == ai) { g_nf_hitT[i] = GetTickCount(); InterlockedIncrement(&g_nf_hits[i]); break; }
    return r;
}
static __attribute__((noinline)) void NfKiRow(uint64_t w, uint64_t row28, uint64_t snap, uint8_t broken, uint8_t style) {  // 1D220E3: KiRowWidget 235F9A4 (display copy of the Ki)
    if (g_cfg_nofin && row28 && snap) {
        __try { uint32_t id = *(uint32_t*)(row28 - 0x24);
            for (int i = 0; i < NF_N; ++i) if (g_nf_drain[i].on && g_nf_drain[i].id == id) { float mx = *(float*)(snap + 0x18), r = g_nf_drain[i].ratio, w = g_nf_drain[i].w; if (r < 0) r = 0; if (r > 1) r = 1;
                if (w < 1.0f) {  // #9133 blend back after the gold: last gold value -> the native snapshot value (no snap to the refilling Ki), native layout
                    float cur = *(float*)(snap + 0x14); if (w < 0) w = 0; *(float*)(snap + 0x14) = mx * r * w + cur * (1.0f - w); break; }
                *(float*)(snap + 0x14) = mx * r; *(float*)(snap + 0x88) = 0;
                // user #8827 (play 16:09): gold only showed in the last 2 s of the HP-0 window = the only time the boss was NOT natively broken (23=0). While broken
                // the widget draws its broken layout (Ki 0 -> empty fill + pink broken layer, other atlas strips; dump 16:09:11 'kifill 0' on every quad), so no
                // fill quad existed to tint. While gold: draw the row as NOT broken, so the normal Ki fill quad (strip 0.688 / 0.375) is drawn at the snapshot
                // value (full while broken, draining in the HP-0 window) and GoldQuad tints it. Display only (the snapshot is the widget's own copy).
                if (broken && g_nf_kirow_unbreak) { broken = 0; static int nl; if (++nl <= 3) NfLog("NOFIN Ki row %u: native broken layout replaced by the normal fill (gold), style %u", id, style); }
                break; }
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
    ((void (*)(uint64_t, uint64_t, uint64_t, uint8_t, uint8_t))(B + Grva(0x235F9A4)))(w, row28, snap, broken, style);
}
// user #9296/#9297 (play 22:12): at HP 0 the boss must play the normal Ki-break stagger, not the HP-floor / dying pose. Every lethal hit (all hits at HP 1)
// requests motion 0x308 (HP floor / dying) in the hit apply (3F21A4(chr+0x2DC0, id) at 329C86, after 4B50D8(chr+0x2DC0, 0x308) says the boss has it) and
// again in 786C84's revive branch (7870E4). Hook on the motion request 3F21A4 for a watched boss: logs every id it is asked to play (first 300) and learns the
// id requested when its native Ki break starts (NfTick, state 0x23 rising outside the HP-0 window); while KiBreakNoFinisher owns the HP floor a 0x308 request
// is replaced by that Ki-break stagger id ([Summon] KiBreakHp0StaggerId, 0 = learned; until learned the 0x308 request is dropped = normal hit reaction).
typedef uint64_t (*NfMotFn)(uint64_t, uint32_t, uint64_t, uint64_t); static NfMotFn g_mot_orig;
static volatile uint64_t g_nf_chr[4]; static volatile DWORD g_mot_t[4]; static volatile uint32_t g_mot_id[4]; static volatile LONG g_mot_logs, g_mot_swaps;
static int g_cfg_stag_id = 0; static volatile uint32_t g_nf_brk_id;
// user #9648/#9655: good Ki break vs 'death pose' break look identical in the bits (4400200200001000 both, captures/pc/freeze9655). Log the boss's
// current ACTION id at every Ki-break start and 150 / 400 / 1000 ms later: 358528(state) = dword at [state+0xE8] (0xFFFF = no action), state =
// [[actor+0x3B0]+8] (= our ctrl; SetActionID 1C0A41C passes the same object to 66C4A8). Plus the last motion request (3F21A4 hook, NOFIN on) and its age.
static __attribute__((noinline)) uint32_t ActIdOf(uint64_t ctrl) { uint64_t p = 0; if (!ctrl || !Rd64(ctrl + 0xE8, &p)) return 0xFFFFFFFFu; if (!p) return 0xFFFF; __try { return *(volatile uint32_t*)p; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0xFFFFFFFFu; } }
static void MotOf(uint64_t a, uint32_t* id, DWORD* age) {
    *id = 0; *age = 0; DWORD now = GetTickCount();
    for (int i = 0; i < 4; ++i) if (a && g_nf[i].a == a) { *id = g_mot_id[i]; *age = g_mot_t[i] ? now - g_mot_t[i] : 0; return; }
}
static struct { uint64_t a; DWORD t0; int step, n; uint32_t act0, last; } g_kbx[KB_MAX];
static void KbxStart(int s, uint64_t a, uint64_t ai, uint64_t ctrl, int nBreak, float kc, uint64_t hc, uint64_t hm, int s23, int exh, uint64_t w0, uint64_t wA8) {
    (void)ai; uint32_t act = ActIdOf(ctrl), mid = 0; DWORD mage = 0; MotOf(a, &mid, &mage);
    g_kbx[s].a = a; g_kbx[s].t0 = GetTickCount(); g_kbx[s].step = 0; g_kbx[s].n = nBreak; g_kbx[s].act0 = act; g_kbx[s].last = act;
    Log("KIBRK ACT start #%d boss %p: action 0x%X (%u), last motion 0x%X %lu ms ago, Ki %.1f HP %llu/%llu (%.1f %%), 23=%d exh %d, bits1060 %016llX A8 %016llX",
        nBreak, (void*)a, act, act, mid, mage, kc, hc, hm, hm ? 100.0 * (double)hc / (double)hm : 0.0, s23, exh, w0, wA8);
}
static __attribute__((noinline)) void KbxTick(int s, uint64_t a, uint64_t ai, uint64_t ctrl, DWORD now, int broken) {
    static const DWORD at[3] = {150, 400, 1000};
    if (broken && g_kbx[s].a == a && g_kbx[s].t0) {  // #9895: every action change while the break lasts
        uint32_t cur = ActIdOf(ctrl);
        if (cur != g_kbx[s].last) { static int nc; if (++nc <= 300) { uint64_t w0 = 0; float kc = 0; uint64_t st = 0, hc = 0, hm = 0; int s23 = 0;
                __try { w0 = *(volatile uint64_t*)(ctrl + 0x1060); if (Rd64(a + 0x3A0, &st) && st) kc = *(float*)(st + 0xAC); s23 = KbState(ai, 0x23); } __except (EXCEPTION_EXECUTE_HANDLER) { }
                ActorHp(a, &hc, &hm);
                Log("KIBRK ACT change +%lu ms #%d boss %p: action 0x%X -> 0x%X (start 0x%X), Ki %.1f HP %llu/%llu 23=%d, bits1060 %016llX", now - g_kbx[s].t0, g_kbx[s].n, (void*)a, g_kbx[s].last, cur, g_kbx[s].act0, kc, hc, hm, s23, w0); }
            g_kbx[s].last = cur; }
    }
    if (g_kbx[s].a != a || !g_kbx[s].t0 || g_kbx[s].step >= 3 || now - g_kbx[s].t0 < at[g_kbx[s].step]) return;
    uint32_t act = ActIdOf(ctrl), mid = 0; DWORD mage = 0; MotOf(a, &mid, &mage); uint64_t w0 = 0, wA8 = 0; float kc = 0; uint64_t st = 0; int s23 = 0;
    __try { w0 = *(volatile uint64_t*)(ctrl + 0x1060); wA8 = *(volatile uint64_t*)(ctrl + 0xA8); if (Rd64(a + 0x3A0, &st) && st) kc = *(float*)(st + 0xAC); s23 = KbState(ai, 0x23); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    Log("KIBRK ACT +%lu ms #%d boss %p: action 0x%X (start 0x%X%s), last motion 0x%X %lu ms ago, Ki %.1f 23=%d, bits1060 %016llX A8 %016llX%s", now - g_kbx[s].t0, g_kbx[s].n, (void*)a, act, g_kbx[s].act0,
        act == g_kbx[s].act0 ? ", unchanged" : "", mid, mage, kc, s23, w0, wA8, (w0 & (1ull << 0x3A)) && !(w0 & ((1ull << 62) | (1ull << 45) | (1ull << 33))) ? "  <- BARE (no stagger bits)" : "");
    g_kbx[s].step++;
}
static uint64_t NfMotDetour(uint64_t c, uint32_t id, uint64_t r8, uint64_t r9) {
    uint32_t want = id & 0xFFFF;
    if (c && g_cfg_nofin) for (int i = 0; i < 4; ++i) if (g_nf_chr[i] && g_nf_chr[i] + 0x2DC0 == c) {
        void* fr[1] = {0}; RtlCaptureStackBackTrace(1, 1, fr, 0); uint32_t from = fr[0] ? (uint32_t)((uintptr_t)fr[0] - B) : 0;
        if (InterlockedIncrement(&g_mot_logs) <= 300) Log("NOFIN motion request boss slot %d: id 0x%X from %X", i, want, from);
        if (g_nf[i].win == 1 || g_nf[i].broken) { static volatile LONG nw; if (InterlockedIncrement(&nw) <= 150) Log("NOFIN motion while %s boss slot %d: id 0x%X from %X (AI %d)", g_nf[i].win == 1 ? "in the HP-0 window" : "Ki broken", i, want, from, g_nf[i].a ? BhAiOn(g_nf[i].a) : -1); }  // #9542 which moves
        if (want == 0x308 && g_nf_stagger && g_nf_act[i] && g_nf_act[i] != g_kill_act) {  // not during our own kill (g_nf_act cleared then)
            uint32_t sub = g_cfg_stag_id ? (uint32_t)g_cfg_stag_id : g_nf_brk_id; LONG k = InterlockedIncrement(&g_mot_swaps);
            if (k <= 20 || k % 50 == 0) Log("NOFIN HP-floor motion 0x308 on boss slot %d (from %X) -> %s 0x%X (#%ld)", i, from, sub ? "Ki-break stagger" : "dropped, no stagger id learned yet", sub, k);
            if (!sub) return 0;
            id = sub; want = sub;
        }
        g_mot_id[i] = want; g_mot_t[i] = GetTickCount(); break;
    }
    return g_mot_orig(c, id, r8, r9);
}
// user #9133/#9291/#9319 'at HP 0 the boss looks dead (crouched, no reaction to hits)'. Log 22:27:57 (b950cb2): no 0x308 request at all in the HP-0
// window, so the pose is not that motion. Every hit at HP 1 was LETHAL: HP damage 787268(hp = st+0x38, dmg, keepAlive, 0) at 329890 sets the dead flag
// (hp+0x48), the lethal block 329C1D.. (NfLethalCond -> 329D1A SetHP 1) skips the native hit/Ki-break transition, and A4F098 then set a bare runtime bit
// 0x3A (state 0x23 = 'broken', hit reactions off) without the break itself (bits1060 0400.. vs a real break 4400200200001000, ctrl+A8 bit 41 off).
// [Summon] KiBreakHp0NonLethal = 1 (default): for a watched boss the HP damage call at 329890 gets keepAlive = 1 (787268's own 'floor at HP 1' arg, the
// one non-lethal attacks use, hit+0x28D) when the hit would take HP below 1 -> HP 1, dead flag never set, the hit stays a NORMAL hit (native hit reaction,
// KiDamage, native Ki break = the real stagger). The Ki is primed to 0.5 on that hit (and held there in the window while not broken) so every hit at the
// floor re-breaks natively; A4F098 is not called in this mode. Window / gold / kill after KiBreakHp0WindowMs unchanged.
static volatile LONG g_nf_n_nl;
// #9319 live tries 4-8 (pc, Yoritomo R7B35): the first hit that takes the boss to the floor starts the native dying sequence (it stands ~2.25 s, then the
// crouched down action KIFINISHER 35=1 + runtime bit 101 = 'looks dead'), at HP 1 or 2, Ki primed or not, AI on or off; HP set to 2 with no hit never
// starts it (try 8). So no hit may ever be lethal: [Summon] KiBreakHp0Reserve = 1 (default): when a watched boss first drops to <= 20 % max HP its real
// HP gets a hidden reserve of 20 % max (g_nf_off) and its boss HP bar shows real - reserve (hook on the HP row call 1D220B5); the HP-0 window starts when
// the shown HP reaches 0, and in the window every hit is topped up so the real HP never drops below the reserve (normal, non-lethal hits: native hit
// reaction / Ki break). The kill at the end of the window is the native force kill as before.
static int g_nf_reserve = 1; static volatile int64_t g_nf_off[NF_N];
#include "practice_stats.h"  // user #22389: damage multipliers scale the hit HP damage here (PstDmgScale); game glue practice_stats_game.h
static PstCfg g_pst = {{0, 100, 100, 0, 100, 0, 100, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100}};  // PsDefaults; loaded from [Practice] at start
static volatile LONG g_pst_dmg_logs, g_pst_live;  // g_pst_live: PsTick (a Practice session or a Training boss is alive)
// user #22389 damage multipliers, by ATTACKER side. The hit apply 327DEC(hit = r13 for the whole function, set at 327E87) calls HP damage
// 787268(hp, edx = cvttss2si xmm15 = the final HP damage, r8b = hit+0x28D keepAlive, r9 = 0) at 329890 (proven on bosses: NOFIN 'HP damage boss slot').
// Static RE (re/Nioh3.dump.exe): hp = [[hit+0x1C8]+0x3A0]+0x38 (328399..328422, kept at rbp-0x10 -> r12 -> rcx) = the VICTIM; 327DEC also compares
// [hit+0x1C8] with GetPlayer [[4745348]+0x1338] (3283D8..3283FB). The ATTACKER is 325F40(hit): [hit+0x198] resolved to its owner character
// (type 0 = the character itself; a projectile / gadget (type 4) -> its owner +0x260 / +0x1E0; 588700 for the rest; pure reads). 327DEC calls it
// right after the HP damage (3298EF) and 35 more times. The stub at 329890 passes r13 (the hit) in r9 (the site's own r9 is a constant 0).
// Attacker == the player -> DamageDealtPct ('Damage I deal'), attacker == a Training boss -> DamageTakenPct ('Damage boss deals'), else x1.
// Ki ('spirit') damage has its own hook (KiDamage 844604 at 329E9D, PstKiDamage in practice_stats_game.h, user #22588); since #23924 it also applies
// Damage I deal (player -> others, x Ki damage I deal) and Damage boss deals (Training boss -> the player) to the hit's Ki damage.
// NOT reachable (#23924): damage over time (poison / burn / status ticks) and script / environment damage. The 7 other callers of the HP damage
// 787268 are not in the hit apply 327DEC, carry no hit object (so no attacker to tell 'mine' from 'the boss's'), and hooking them would need call-site
// RVAs and byte signatures that are not RE'd yet; hooking 787268 itself would also catch heals and our own call. They stay unscaled.
static int PstIsBossActor(uint64_t a);  // practice_stats_game.h
static volatile LONG g_pst_atk_ok;  // 1 = the 325F40 bytes match (HpDmgHookInstall); otherwise no attacker = no scaling
static __attribute__((noinline)) int PstDmgScale(uint64_t hp, int dmg, uint64_t hit) {
    const int dealt = g_pst.v[PS_DMG_DEALT], boss = g_pst.v[PS_DMG_TAKEN];
    if (!g_pst_live || (dealt == 100 && boss == 100) || dmg <= 0 || !hp || !hit || !g_pst_atk_ok) return dmg;
    uint64_t me = PlayerChar(), st = 0; if (me) Rd64(me + 0x3A0, &st);
    const uint64_t atk = ((uint64_t (*)(uint64_t))(B + Grva(0x325F40)))(hit);
    const int atkPlayer = me && atk == me, atkBoss = !atkPlayer && PstIsBossActor(atk), vicPlayer = st && st + 0x38 == hp;
    const int pct = PstDmgPct(atkPlayer, atkBoss, vicPlayer, dealt, boss), r = PstScaleDmg(dmg, pct);
    if (r != dmg && InterlockedIncrement(&g_pst_dmg_logs) <= 30) Log("PRACTICE damage x%d.%02d: %s hit %d -> %d (attacker %p)", pct / 100, pct % 100, atkPlayer ? "player deals" : "boss deals", dmg, r, (void*)atk);
    return r;
}
static void FmIfHpLog(uint64_t hp, int raw, int scaled, int applied, uint64_t hit);  // #35391 frame_meter_game.h (IFrameProbe)
static __attribute__((noinline)) uint64_t NfHpDamage(uint64_t hp, int dmg, uint8_t keep, uint64_t hit) {
    const uint8_t p4 = 0;  // 329890 does xor r9d, r9d before the call; the stub puts the hit in r9 instead
    const int rawDmg = dmg;
    __try { dmg = PstDmgScale(hp, dmg, hit); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    const int scaledDmg = dmg;  // user #22389 (first: the NOFIN floor below sees the scaled hit)
    if (g_cfg_cantdie && dmg > 0 && hp) {  // user #22918: cut player HP damage whenever the toggle is ON, including after Return clears the session
        uint64_t me = PlayerChar(), pst = 0;
        if (me && Rd64(me + 0x3A0, &pst) && pst && pst + 0x38 == hp) {
            static volatile LONG ncut; const int d0 = dmg; dmg = PhDamageCut(g_cfg_cantdie, 1, 1, dmg);
            if (InterlockedIncrement(&ncut) <= 20) Log("TRAIN infinite health: hit of %d on the player cut to %d (%s)", d0, dmg, g_tr.active ? "Practice" : g_bm.active ? "Boss Match" : g_ar.active ? "arena" : "toggle still on");
        }
    }
    if (g_cfg_nofin && g_nf_nonlethal && hp && !keep) for (int i = 0; i < NF_N; ++i) if (g_nf_chr[i] && g_nf_chr[i] + 0x38 == hp && g_nf_act[i] && g_nf_act[i] != g_kill_act) {
        __try { int64_t cur = *(int64_t*)(hp + 0x20), off = g_nf_off[i]; uint64_t st = g_nf_chr[i];
            { static LONG nd; if (InterlockedIncrement(&nd) <= 40) Log("NOFIN HP damage boss slot %d: HP %lld (reserve %lld) dmg %d", i, (long long)cur, (long long)off, dmg); }
            if (*(int32_t*)(hp + 0x10) != 1) {
                if (off > 0) {
                    if (g_nf[i].win == 1) { g_nf_hitT[i] = GetTickCount(); InterlockedIncrement(&g_nf_hits[i]); }  // every hit in the window restarts it (the 326E81 probe misses some)
                    if (cur - (int64_t)dmg < off && g_nf_prime && !((uint8_t (*)(uint64_t))(B + Grva(0x844FE4)))(st + 0x98) && !KbState(g_nf_ai[i], 0x23) && *(float*)(st + 0xAC) > 0.5f)
                        *(float*)(st + 0xAC) = 0.5f;  // raw Ki write (not KiSetCur 2F88D0: after it the hit's Ki damage went to -133 with no break, tries 4/5)
                    if (cur - (int64_t)dmg < off) { ((void (*)(uint64_t, uint64_t))(B + Grva(0x66C188)))(hp, (uint64_t)(off + dmg));
                        LONG k = InterlockedIncrement(&g_nf_n_nl); if (k <= 20 || k % 50 == 0) Log("NOFIN boss slot %d hit at the shown HP 0: real HP topped up to reserve %lld + dmg %d (non-lethal, normal hit) (#%ld)", i, (long long)off, dmg, k); }
                } else if (cur - (int64_t)dmg < 2) {  // no reserve (KiBreakHp0Reserve 0 or not set yet): floor at HP 2
                    int nd = cur > 2 ? (int)(cur - 2) : 0; dmg = nd;
                    if (g_nf_prime && !((uint8_t (*)(uint64_t))(B + Grva(0x844FE4)))(st + 0x98) && !KbState(g_nf_ai[i], 0x23)) { float kc = *(float*)(st + 0xAC); if (kc > 0.5f) ((void (*)(uint64_t, float))(B + Grva(0x2F88D0)))(st + 0x98, 0.5f); }
                    LONG k = InterlockedIncrement(&g_nf_n_nl); if (k <= 20 || k % 50 == 0) Log("NOFIN boss slot %d hit at HP floor (HP %lld): damage cut to %d (stays at HP 2) (#%ld)", i, (long long)cur, dmg, k);
                }
            } } __except (EXCEPTION_EXECUTE_HANDLER) { }
        break;
    }
    FmIfHpLog(hp, rawDmg, scaledDmg, dmg, hit);
    return ((uint64_t (*)(uint64_t, int, uint8_t, uint8_t))(B + Grva(0x787268)))(hp, dmg, keep, p4);
}
static __attribute__((noinline)) void NfHpRow(uint64_t w, uint64_t row, uint64_t snap, float ratio, float x5) {  // 1D220B5: boss HP row widget 235F92C(w, row, HP snapshot copy, ...)
    if (g_cfg_nofin && row && snap) __try { uint32_t id = *(uint32_t*)(row - 0x0C);
        for (int i = 0; i < NF_N; ++i) if (g_nf_off[i] > 0 && g_nf[i].id == id) { int64_t cur = *(int64_t*)(snap + 0x20), d = cur - g_nf_off[i]; if (d < 0) d = 0;
            ((void (*)(uint64_t, uint64_t))(B + Grva(0x66C188)))(snap, (uint64_t)d); break; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
    ((void (*)(uint64_t, uint64_t, uint64_t, float, float))(B + Grva(0x235F92C)))(w, row, snap, ratio, x5);
}
// #9319 live tries 4-6 (00:00:23, 00:03:29, 00:06:39): even at HP 2 with full Ki the boss entered a down action (KIFINISHER 35=1, bits1060 0x6000,
// runtime bit 101) 2.25 s after the floor hit, AI on or off = the native HP-floor kneel: every kneel writer (script ForceKneelDown 238E1BC, AI 7CF878
// at 1A153FF/1A15450, 2001E34, 2369F08) does ctrl+0x994 = 0x27 (kneel action), 668D74(actor) (AI off) and actor+0x91B = 1. With KiBreakHp0NonLethal a
// watched boss in its HP-0 window: 668D74 from the game is dropped (caller logged), and NfTick undoes 0x994 = 0x27 / 0x91B = 1.
static void (*o_aioff)(uint64_t); static volatile LONG g_nf_n_kneel; static volatile int g_nf_kneelblk[NF_N];
static __attribute__((noinline)) void NfAiOffDetour(uint64_t a) {
    uintptr_t ra = (uintptr_t)__builtin_return_address(0);
    if (g_cfg_nofin && g_nf_nonlethal && a && ra >= B && ra < B + g_imgSize) for (int i = 0; i < NF_N; ++i) if (g_nf[i].a == a && g_nf[i].win == 1 && g_nf_act[i]) {
        g_nf_kneelblk[i] = 1; LONG k = InterlockedIncrement(&g_nf_n_kneel);
        if (k <= 20) Log("NOFIN boss %p in the HP-0 window: native AI-off/kneel 668D74 from %llX dropped (#%ld)", (void*)a, (unsigned long long)(ra - B), k);
        return;
    }
    // #9542: the native kneel at HP > 0 during a normal Ki break (Ki <= 0 / broken) = the 'death pose' crouch the user saw; same signature (ctrl+0x994 = 0x27)
    if (g_cfg_nofin && a && ra >= B && ra < B + g_imgSize) for (int i = 0; i < NF_N; ++i) if (g_nf[i].a == a && g_nf_act[i] && g_nf[i].win == 0) {
        uint32_t r994 = 0; float kc = 1.0f; __try { if (g_nf_ctrl[i]) r994 = *(volatile uint32_t*)(g_nf_ctrl[i] + 0x994); if (g_nf_chr[i]) kc = *(float*)(g_nf_chr[i] + 0xAC); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        int drop = g_nf_kneelall && r994 == 0x27 && (kc <= 0.0f || g_nf[i].broken);
        static volatile LONG nl; if (InterlockedIncrement(&nl) <= 40) Log("NOFIN boss %p (HP > 0) native AI-off 668D74 from %llX: ctrl+994 0x%X Ki %.1f broken %d -> %s", (void*)a, (unsigned long long)(ra - B), r994, kc, g_nf[i].broken, drop ? "DROPPED (kneel in a normal Ki break, KiBreakKneelBlockAll)" : "passed");
        if (drop) { g_nf_kneelblk[i] = 1; InterlockedIncrement(&g_nf_n_kneel); return; }
        break;
    }
    o_aioff(a);
}
typedef uint8_t (*NfStFn)(uint64_t, uint32_t, uint64_t); static NfStFn g_tr_orig; static uint8_t NfStDetour(uint64_t c, uint32_t id, uint64_t r8);
// HP damage 787268 in the hit apply 329890 (r8b = hit+0x28D keepAlive) -> NfHpDamage. Installed once, by KiBreakNoFinisher or by the #22389 damage
// multipliers (whichever needs it first); NfHpDamage does both jobs. Game thread only. Returns 1 = hooked.
static volatile LONG g_hpdmg_hooked;  // 0 = not tried, 1 = on, 2 = failed (bytes differ)
static int HpDmgHookInstall(const char* who) {
    if (g_hpdmg_hooked) return g_hpdmg_hooked == 1;
    g_hpdmg_hooked = 2;
    uint8_t* site = (uint8_t*)(B + Grva(0x329890)); uint8_t* s = NearPage(site);
    // 325F40 (attacker of a hit, #22389): mov rcx,[rcx+198h] | jmp 325F4C (E9 00000000)
    g_pst_atk_ok = !memcmp((void*)(B + Grva(0x325F40)), "\x48\x8B\x89\x98\x01\x00\x00\xE9\x00\x00\x00\x00", 12);
    if (!g_pst_atk_ok) Log("hook HP damage 329890: attacker getter 325F40 bytes differ, damage multipliers off");
    // site - 22: mov r8b,[r13+28Dh] | xor r9d,r9d | cvttss2si edx,xmm15 | mov rcx,r12 (then movaps xmm11,xmm0 | call 787268)
    if (s && !memcmp(site - 22, "\x45\x8A\x85\x8D\x02\x00\x00\x45\x33\xC9\xF3\x41\x0F\x2C\xD7\x49\x8B\xCC", 18)) {
        uint8_t* p = s; *p++ = 0x4D; *p++ = 0x8B; *p++ = 0xCD;  // mov r9, r13 (the hit) = NfHpDamage's 4th argument
        p = E_mov_rax(p, (uint64_t)NfHpDamage); *p++ = 0xFF; *p++ = 0xE0; FlushInstructionCache(GetCurrentProcess(), s, 16);
        if (NfPatchCall(site, B + Grva(0x787268), s, "HP damage 329890")) { g_hpdmg_hooked = 1;
            Log("hook HP damage 329890: on for %s (KiBreakHp0NonLethal %d: hits at the HP floor stay normal hits; damage I deal %d %%, boss deals %d %%)", who, g_nf_nonlethal, g_pst.v[PS_DMG_DEALT], g_pst.v[PS_DMG_TAKEN]); } }
    else Log("hook HP damage 329890: bytes differ, off (%s; HP-floor hits stay lethal, damage multipliers off)", who);
    return g_hpdmg_hooked == 1;
}
static void NfInstallHooks(void) {
    if (g_nf_hooked) return; g_nf_hooked = 1;
    int ok = 0;
    {   // A5F7F8 CanGrapple(rcx self, rdx target) entry: 48 89 5C 24 08 (mov [rsp+8],rbx) | 57 push rdi
        uint8_t* site = (uint8_t*)(B + Grva(0xA5F7F8)); static const uint8_t pro[6] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57};
        uint8_t* s = memcmp(site, pro, 6) ? 0 : NearPage(site);
        if (!s) Log("NOFIN hook CanGrapple A5F7F8: prologue differs / no stub, grapple selection not blocked");
        else { uint8_t* p = s; uint8_t* je[NF_N];
            p = E_mov_rax(p, (uint64_t)&g_nf_cg_tgt); *p++ = 0x48; *p++ = 0x89; *p++ = 0x10; p = E_inc(p, &g_nf_cnt_cg);  // diag: mov [&g_nf_cg_tgt], rdx; lock inc calls
            p = E_mov_rax(p, (uint64_t)g_nf_act); p = E_cmp4(p, 2, je);
            memcpy(p, pro, 5); p += 5; *p++ = 0xFF; *p++ = 0x25; *(uint32_t*)p = 0; p += 4; *(uint64_t*)p = (uint64_t)(site + 5); p += 8;  // mov [rsp+8],rbx; jmp A5F7FD
            E_bind(je, NF_N, p); p = E_inc(p, &g_nf_cnt_sel); *p++ = 0x31; *p++ = 0xC0; *p++ = 0xC3;  // block: count; xor eax,eax; ret
            FlushInstructionCache(GetCurrentProcess(), s, p - s);
            DWORD old; VirtualProtect(site, 8, PAGE_EXECUTE_READWRITE, &old);
            LONG64 o8 = *(volatile LONG64*)site, n8 = o8; uint8_t* nb = (uint8_t*)&n8; nb[0] = 0xE9; *(int32_t*)(nb + 1) = (int32_t)((intptr_t)s - (intptr_t)(site + 5));
            InterlockedCompareExchange64((volatile LONG64*)site, n8, o8); VirtualProtect(site, 8, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 8); ok |= 1; }
    }
    {   // 117A9EE: mov rax,[rcx]; mov edx,23h; call [rax+0C0h] (rcx = victim ctrl) -> call stub; nop. Watched boss: drop the return address, jmp 117C6ED (reject)
        uint8_t* site = (uint8_t*)(B + Grva(0x117A9EE)); static const uint8_t org[6] = {0xFF, 0x90, 0xC0, 0x00, 0x00, 0x00};
        uint8_t* s = (memcmp(site, org, 6) || memcmp(site - 5, "\xBA\x23\x00\x00\x00", 5)) ? 0 : NearPage(site);
        if (!s) Log("NOFIN hook grab box 117A9EE: bytes differ / no stub, not hooked");
        else { uint8_t* p = s; uint8_t* je[NF_N]; p = E_mov_rax(p, (uint64_t)g_nf_ctrl); p = E_cmp4(p, 1, je);
            *p++ = 0x48; *p++ = 0x8B; *p++ = 0x01; *p++ = 0xFF; *p++ = 0xA0; *p++ = 0xC0; *p++ = 0; *p++ = 0; *p++ = 0;  // mov rax,[rcx]; jmp [rax+0C0h]
            E_bind(je, NF_N, p); p = E_inc(p, &g_nf_cnt_grab); *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x08;  // add rsp,8
            p = E_mov_rax(p, B + Grva(0x117C6ED)); *p++ = 0xFF; *p++ = 0xE0;
            FlushInstructionCache(GetCurrentProcess(), s, p - s);
            DWORD old; VirtualProtect(site, 8, PAGE_EXECUTE_READWRITE, &old);
            LONG64 o8 = *(volatile LONG64*)site, n8 = o8; uint8_t* nb = (uint8_t*)&n8; nb[0] = 0xE8; *(int32_t*)(nb + 1) = (int32_t)((intptr_t)s - (intptr_t)(site + 5)); nb[5] = 0x90;
            InterlockedCompareExchange64((volatile LONG64*)site, n8, o8); VirtualProtect(site, 8, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 8); ok |= 2; }
    }
    // bit-28 box 117ABF7 (skips Ki-broken victims) is NOT hooked any more: reporting 'not broken' there would make those boxes hit a broken boss = a
    // gameplay change outside the finisher. Only the bit-31 box (hits only Ki-broken victims = finisher-type boxes) is filtered.
    for (int k = 0; k < 1; ++k) {   // 117AC17 (bit 31: hits only Ki-broken): call 4893B8(rcx = victim ai, 0x23) -> 0 for a watched boss
        uint8_t* site = (uint8_t*)(B + (k ? Grva(0x117ABF7) : Grva(0x117AC17))); uint8_t* s = NearPage(site); if (!s) continue;
        uint8_t* p = s; uint8_t* je[NF_N]; uint8_t* jz;
        *p++ = 0x48; *p++ = 0x85; *p++ = 0xC9; *p++ = 0x74; jz = p; *p++ = 0;  // test rcx,rcx; jz orig
        p = E_mov_rax(p, (uint64_t)g_nf_ai); p = E_cmp4(p, 1, je);
        *jz = (uint8_t)(p - (jz + 1)); p = E_mov_rax(p, B + Grva(0x4893B8)); *p++ = 0xFF; *p++ = 0xE0;  // orig: jmp StateTest
        E_bind(je, NF_N, p); p = E_inc(p, k ? &g_nf_cnt_b28 : &g_nf_cnt_b31); *p++ = 0x31; *p++ = 0xC0; *p++ = 0xC3;
        FlushInstructionCache(GetCurrentProcess(), s, p - s);
        if (NfPatchCall(site, B + Grva(0x4893B8), s, k ? "bit-28 box 117ABF7" : "bit-31 box 117AC17")) ok |= k ? 8 : 4;
    }
    {   uint8_t* site = (uint8_t*)(B + Grva(0x329C9F)); uint8_t* s = NearPage(site);
        // stub: sub rsp,28h; call NfLethalCond; add rsp,28h; cmp al,2; jne ret; [rsp] = 329D1A (mov rdx,rsi(=1); mov rcx,r12; call 66C188 'survive at HP 1'
        // = the native no-death path; jmp 329DE0); xor eax,eax; ret
        if (s && !memcmp(site - 12, "\xBA\x27\x00\x00\x00", 5) && !memcmp((void*)(B + Grva(0x329D1A)), "\x48\x8B\xD6\x49\x8B\xCC\xE8", 7)) { uint8_t* p = s;
            *p++ = 0x48; *p++ = 0x83; *p++ = 0xEC; *p++ = 0x28; p = E_mov_rax(p, (uint64_t)NfLethalCond); *p++ = 0xFF; *p++ = 0xD0; *p++ = 0x48; *p++ = 0x83; *p++ = 0xC4; *p++ = 0x28;
            *p++ = 0x3C; *p++ = 0x02; *p++ = 0x75; uint8_t* jn = p++; p = E_mov_rax(p, B + Grva(0x329D1A)); *p++ = 0x48; *p++ = 0x89; *p++ = 0x04; *p++ = 0x24; *p++ = 0x31; *p++ = 0xC0;
            *jn = (uint8_t)(p - (jn + 1)); *p++ = 0xC3; FlushInstructionCache(GetCurrentProcess(), s, p - s);
            if (NfPatchCall(site, B + Grva(0x4893B8), s, "lethal-hit cond 0x27 329C9F")) ok |= 16; } else Log("NOFIN hook 329C9F: bytes differ, HP-0 window off");
    }
    {   uint8_t* site = (uint8_t*)(B + Grva(0x1D220E3)); uint8_t* s = NearPage(site);
        if (s) { uint8_t* p = E_mov_rax(s, (uint64_t)NfKiRow); *p++ = 0xFF; *p++ = 0xE0; FlushInstructionCache(GetCurrentProcess(), s, 16);
            if (NfPatchCall(site, B + Grva(0x235F9A4), s, "Ki row 1D220E3")) ok |= 32; }
    }
    {   uint8_t* site = (uint8_t*)(B + Grva(0x326E81)); uint8_t* s = NearPage(site);
        if (s && !memcmp(site - 7, "\x48\x8B\x88\xB0\x03\x00\x00", 7)) { uint8_t* p = E_mov_rax(s, (uint64_t)NfHitProbe); *p++ = 0xFF; *p++ = 0xE0; FlushInstructionCache(GetCurrentProcess(), s, 16);
            if (NfPatchCall(site, B + Grva(0x4893B8), s, "hit probe 326E81")) ok |= 128; }
    }
    HpDmgHookInstall("KiBreakNoFinisher");  // HP damage 329890, shared with the #22389 damage multipliers
    {   uint8_t* site = (uint8_t*)(B + Grva(0x1D220B5)); uint8_t* s2 = NearPage(site);  // HP row (rdx = row, r8 = HP snapshot copy)
        if (s2 && !memcmp(site - 9, "\x4C\x8D\x44\x24\x30\x48\x8D\x4D\x90", 9)) { uint8_t* p = E_mov_rax(s2, (uint64_t)NfHpRow); *p++ = 0xFF; *p++ = 0xE0; FlushInstructionCache(GetCurrentProcess(), s2, 16);
            if (NfPatchCall(site, B + Grva(0x235F92C), s2, "HP row 1D220B5") && (g_nf_hprow_ok = 1)) Log("NOFIN hook HP row 1D220B5: on (shown HP = real - reserve, KiBreakHp0Reserve %d)", g_nf_reserve); }
        else Log("NOFIN hook HP row 1D220B5: bytes differ, off");
    }
    {   uint8_t* site = (uint8_t*)(B + Grva(0x2F7045)); uint8_t* s = NearPage(site);
        if (s) { uint8_t* p = E_mov_rax(s, (uint64_t)NfGrappleTarget); *p++ = 0xFF; *p++ = 0xE0; FlushInstructionCache(GetCurrentProcess(), s, 16);
            if (NfPatchCall(site, B + Grva(0x867258), s, "grapple target 2F7045")) ok |= 64; }
    }
    {   uint8_t* site = (uint8_t*)(B + Grva(0x2F7015)); uint8_t* s = NearPage(site);
        if (s) { uint8_t* p = E_mov_rax(s, (uint64_t)NfDownTarget); *p++ = 0xFF; *p++ = 0xE0; FlushInstructionCache(GetCurrentProcess(), s, 16);
            if (NfPatchCall(site, B + Grva(0x8671D4), s, "down target 2F7015")) { Log("NOFIN hook down target 2F7015 (8671D4, StateTest 9 slot ctrl+B50): on, blocked while the Ki bar is gold (#8894)"); } else Log("NOFIN hook down target 2F7015: OFF (call site differs)"); }
    }
    {   // #9135 (a) 117A9AC: test al,al; jns 117AA2A; mov rax,[rsi+3B0h] (11 bytes) -> jmp stub. Victim actor = rbx (its ctrl is tested at 117A9EE).
        uint8_t* site = (uint8_t*)(B + Grva(0x117A9AC)); static const uint8_t org[11] = {0x84, 0xC0, 0x79, 0x7A, 0x48, 0x8B, 0x86, 0xB0, 0x03, 0x00, 0x00};
        uint8_t* s = memcmp(site, org, 11) ? 0 : NearPage(site);
        if (!s) Log("NOFIN hook plain grab box 117A9AC: bytes differ / no stub, not hooked (%02X %02X %02X %02X)", site[0], site[1], site[2], site[3]);
        else { uint8_t* p = s; uint8_t* je[4]; uint8_t* jz;
            *p++ = 0x48; *p++ = 0x85; *p++ = 0xDB; *p++ = 0x74; jz = p++;  // test rbx,rbx; jz orig
            *p++ = 0x50; p = E_mov_rax(p, (uint64_t)g_nf_goldact); p = E_cmp4(p, 3, je); *p++ = 0x58;  // push rax; cmp rbx,[rax+8i] x4; pop rax
            *jz = (uint8_t)(p - (jz + 1));
            uint8_t* orig = p; *p++ = 0x84; *p++ = 0xC0; *p++ = 0x79; uint8_t* jns = p++;  // test al,al; jns L1
            *p++ = 0x48; *p++ = 0x8B; *p++ = 0x86; *p++ = 0xB0; *p++ = 0x03; *p++ = 0; *p++ = 0;  // mov rax,[rsi+3B0h]
            *p++ = 0xFF; *p++ = 0x25; *(uint32_t*)p = 0; p += 4; *(uint64_t*)p = B + Grva(0x117A9B7); p += 8;
            *jns = (uint8_t)(p - (jns + 1)); *p++ = 0xFF; *p++ = 0x25; *(uint32_t*)p = 0; p += 4; *(uint64_t*)p = B + Grva(0x117AA2A); p += 8;
            E_bind(je, 4, p); *p++ = 0x58; p = E_inc(p, &g_nf_cnt_grab0);  // block: pop rax; count; jmp 117C6ED (reject)
            *p++ = 0xFF; *p++ = 0x25; *(uint32_t*)p = 0; p += 4; *(uint64_t*)p = B + Grva(0x117C6ED); p += 8;
            (void)orig; FlushInstructionCache(GetCurrentProcess(), s, p - s);
            DWORD old; VirtualProtect(site, 11, PAGE_EXECUTE_READWRITE, &old);
            uint8_t nb[11]; memset(nb, 0x90, sizeof nb); nb[0] = 0xE9; *(int32_t*)(nb + 1) = (int32_t)((intptr_t)s - (intptr_t)(site + 5));
            memcpy(site + 5, nb + 5, 6); InterlockedExchange((volatile LONG*)(site + 1), *(LONG*)(nb + 1)); site[0] = 0xE9;
            VirtualProtect(site, 11, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 11);
            Log("NOFIN hook plain grab box 117A9AC: on (grab boxes without the Ki-broken flag miss a gold boss, #9135)"); }
    }
    {   static const uint8_t mp[15] = {0x48,0x89,0x5C,0x24,0x08, 0x48,0x89,0x6C,0x24,0x10, 0x48,0x89,0x74,0x24,0x18};
        if (g_mot_orig) { } else if (memcmp((void*)(B + Grva(0x3F21A4)), mp, 15)) Log("NOFIN motion hook 3F21A4: prologue differs, OFF (HP floor keeps the native pose)");
        else { g_mot_orig = (NfMotFn)Hook(0x3F21A4, 15, (void*)NfMotDetour); Log("NOFIN motion hook 3F21A4: on (HP-floor motion 0x308 -> Ki-break stagger id %d (0 = learned), #9297)", g_cfg_stag_id); }
    }
    if (g_nf_nonlethal && !o_aioff) { static const uint8_t pro[13] = {0x40,0x53, 0x48,0x83,0xEC,0x20, 0x48,0x8B,0x99,0xB0,0x03,0x00,0x00};
        if (memcmp((void*)(B + Grva(0x668D74)), pro, 13)) Log("NOFIN kneel block 668D74: prologue differs, OFF");
        else { o_aioff = (void (*)(uint64_t))Hook(0x668D74, 13, (void*)NfAiOffDetour); Log("NOFIN kneel block 668D74: on (HP-0 window: no native HP-floor kneel / AI off, #9319)"); } }
    if (g_nf_st27) {  // #9135 (b): StateTest default impl 2E9948 detour (shared with the nofintrace aid)
        static const uint8_t pro[15] = {0x48,0x8B,0xC4, 0x48,0x89,0x58,0x08, 0x48,0x89,0x70,0x10, 0x48,0x89,0x78,0x20};
        if (g_tr_orig) Log("NOFIN death-blow test 2E9948: on (detour already in)");
        else if (memcmp((void*)(B + Grva(0x2E9948)), pro, 15)) Log("NOFIN death-blow test 2E9948: prologue differs, OFF");
        else { g_tr_orig = (NfStFn)Hook(0x2E9948, 15, (void*)NfStDetour); Log("NOFIN death-blow test 2E9948: on (StateTest 0x27 false for a gold boss, #9135)"); }
    }
    Log("NOFIN hooks (KiBreakNoFinisher %d, window %d ms): CanGrapple %s, grab box %s, bit-31 box %s, bit-28 box %s (off by design), lethal cond %s, Ki row drain %s, grapple target %s, hit probe %s", g_cfg_nofin, g_cfg_nofin_ms,
        ok & 1 ? "on" : "OFF", ok & 2 ? "on" : "OFF", ok & 4 ? "on" : "OFF", ok & 8 ? "on" : "OFF", ok & 16 ? "on" : "OFF", ok & 32 ? "on" : "OFF", ok & 64 ? "on" : "OFF", ok & 128 ? "on" : "OFF");
}
static uint64_t NfFirstBoss(void) { for (int i = 0; i < NF_N; ++i) if (g_nf[i].a) return g_nf[i].a; return g_nsp ? (uint64_t)g_sp_actor[g_nsp - 1] : 0; }
// test aid 'nofintrace [ms]': every StateTest (default impl 2E9948) on the watched boss's ctrl for ms, aggregated by (caller, caller's caller, id):
// count, true count, first / last ms. Used to find the gate the LB grapple really reads (CanGrapple A5F7F8 is never called, live 02:04).
typedef uint8_t (*NfStFn)(uint64_t, uint32_t, uint64_t);
static NfStFn g_tr_orig; static volatile uint64_t g_tr_ctrl; static volatile DWORD g_tr_t0, g_tr_until; static volatile LONG g_tr_on, g_tr_dumped = 1;
#define NF_TRN 512
static struct { volatile LONG64 key; volatile LONG n, nt; volatile DWORD t1, t2; uint32_t f2; } g_ntr[NF_TRN];
static uint8_t NfStDetour(uint64_t c, uint32_t id, uint64_t r8) {
    uint8_t r = g_tr_orig(c, id, r8);
    if (r && id == 0x27 && c && g_nf_st27 && g_cfg_nofin) {  // #9135 (b): no death-blow offer on a gold boss
        for (int i = 0; i < 4; ++i) if (g_nf_goldctrl[i] == c) {
            uint64_t a = g_nf_goldact[i]; if (!a || a == g_kill_act) break;
            void* fr[2] = {0}; USHORT k = RtlCaptureStackBackTrace(1, 2, fr, 0);
            uintptr_t f1 = k >= 1 ? (uintptr_t)fr[0] : 0, site = (f1 - B == Grva(0x4893EB) && k >= 2) ? (uintptr_t)fr[1] : f1;
            if (site < B || site >= B + g_imgSize) break;  // our own stubs (lethal-hit 329C9F, revive 787004) decide themselves
            uint32_t rv = (uint32_t)(site - B); if (rv == Grva(0xE43BF7) || rv == Grva(0x21FAEFF)) break;  // native damage revive tests: keep (no death instead of HP 1)
            LONG n = InterlockedIncrement(&g_nf_cnt_st27); if (n <= 8 || n % 500 == 0) Log("NOFIN death-blow test (StateTest 0x27) on gold boss %p from %X -> false (#%ld)", (void*)a, rv, n);
            return 0;
        }
    }
    if (g_tr_on && c == g_tr_ctrl) {
        void* fr[3] = {0}; USHORT k = RtlCaptureStackBackTrace(1, 2, fr, 0);
        uint32_t f1 = k >= 1 ? (uint32_t)((uintptr_t)fr[0] - B) : 0, f2 = k >= 2 ? (uint32_t)((uintptr_t)fr[1] - B) : 0;
        if (f1 != Grva(0x4893EB)) f2 = 0;  // direct vtable caller: f1 is the site
        LONG64 key = ((LONG64)(f1 == Grva(0x4893EB) ? f2 : f1) << 16) | (id & 0x7FFF) | ((LONG64)(f1 == Grva(0x4893EB)) << 15);
        DWORD now = GetTickCount() - g_tr_t0;
        for (uint32_t i = (uint32_t)(key * 2654435761u) % NF_TRN, j = 0; j < NF_TRN; ++j, i = (i + 1) % NF_TRN) {
            LONG64 cur = g_ntr[i].key;
            if (cur == 0) { if (InterlockedCompareExchange64(&g_ntr[i].key, key, 0) != 0 && g_ntr[i].key != key) continue; g_ntr[i].t1 = now; g_ntr[i].f2 = f2; cur = key; }
            if (cur == key) { InterlockedIncrement(&g_ntr[i].n); if (r) InterlockedIncrement(&g_ntr[i].nt); g_ntr[i].t2 = now; break; }
        }
    }
    return r;
}
static void NfTraceStart(DWORD ms) {
    uint64_t a = NfFirstBoss(), ai = 0, ctrl = 0;
    if (!a || !Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) { Log("CMD nofintrace: no boss"); return; }
    if (!g_tr_orig) { static const uint8_t pro[15] = {0x48,0x8B,0xC4, 0x48,0x89,0x58,0x08, 0x48,0x89,0x70,0x10, 0x48,0x89,0x78,0x20};
        if (memcmp((void*)(B + Grva(0x2E9948)), pro, 15)) { Log("CMD nofintrace: 2E9948 prologue differs"); return; }
        g_tr_orig = (NfStFn)Hook(0x2E9948, 15, (void*)NfStDetour); }
    memset((void*)g_ntr, 0, sizeof g_ntr); g_tr_ctrl = ctrl; g_tr_t0 = GetTickCount(); g_tr_until = g_tr_t0 + ms; g_tr_dumped = 0; g_tr_on = 1;
    Log("CMD nofintrace: boss %p ctrl %p, %lu ms", (void*)a, (void*)ctrl, ms);
}
// test aid 'nofinsnap' / 'nofindiff': dword diff of the watched boss actor (0xC000 B), its ai (0x2000) and ctrl (0x2000) between the snap and now
static uint8_t g_snp[3][0xC000]; static uint64_t g_snpBase[3]; static const uint32_t g_snpLen[3] = {0xC000, 0x2000, 0x2000};
static __attribute__((noinline)) void NfSnap(int diff) {
    uint64_t a = NfFirstBoss(), ai = 0, ctrl = 0; if (!a || !Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl)) { Log("CMD nofinsnap: no boss"); return; }
    uint64_t base[3] = {a, ai, ctrl}; static const char* nm[3] = {"actor", "ai", "ctrl"}; int nd = 0;
    for (int k = 0; k < 3; ++k) {
        if (!diff) { g_snpBase[k] = base[k]; __try { memcpy(g_snp[k], (void*)base[k], g_snpLen[k]); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("CMD nofinsnap: %s read fault", nm[k]); } continue; }
        if (g_snpBase[k] != base[k]) { Log("CMD nofindiff: %s moved", nm[k]); continue; }
        __try { for (uint32_t o = 0; o < g_snpLen[k]; o += 4) { uint32_t was = *(uint32_t*)(g_snp[k] + o), now = *(uint32_t*)(base[k] + o);
            if (was != now && nd < 300) { nd++; Log("NOFIN diff %s+%04X: %08X -> %08X (%g -> %g)", nm[k], o, was, now, *(float*)&was, *(float*)&now); } } } __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
    Log("CMD nofin%s: actor %p ai %p ctrl %p, %d diffs", diff ? "diff" : "snap", (void*)a, (void*)ai, (void*)ctrl, nd);
}
static void NfTraceTick(void) {
    if (g_tr_dumped || (int)(GetTickCount() - g_tr_until) < 0) return;
    g_tr_on = 0; g_tr_dumped = 1; int m = 0;
    for (int i = 0; i < NF_TRN; ++i) if (g_ntr[i].key) { LONG64 k = g_ntr[i].key; m++;
        Log("NOFIN trace: %s %06llX id %02llX n %ld true %ld t %lu..%lu ms", (k >> 15) & 1 ? "via4893B8 from" : "vcall at", (unsigned long long)(k >> 16), (unsigned long long)(k & 0x7FFF), g_ntr[i].n, g_ntr[i].nt, g_ntr[i].t1, g_ntr[i].t2); }
    Log("NOFIN trace end: %d keys", m);
}
static void NfClear(int s) { g_nf_off[s] = 0; g_nf_goldact[s] = 0; g_nf_goldctrl[s] = 0; g_nf_chr[s] = 0; g_nf_act[s] = 0; g_nf_ai[s] = 0; g_nf_ctrl[s] = 0; g_nf_drain[s].on = 0; g_nf_drain[s].id = 0; g_nf_fbId[s] = 0; memset(&g_nf[s], 0, sizeof g_nf[s]); }
// Live 00:32:11: KiSetCur(K, -max) leaves the boss at Ki -1000 but NOT broken (no exhaustion, it kept attacking): the break is the hit-reaction when a hit's
// Ki damage takes the Ki across 0 (327DEC). So 'break' = prime the Ki to 0.5: the next hit breaks the boss natively (NfLethalCond primes inside the lethal hit).
static __attribute__((noinline)) void NfBreakNow(uint64_t st, const char* why, uint64_t a) {
    __try { if (*(float*)(st + 0xAC) <= 0.5f) return; ((void (*)(uint64_t, float))(B + Grva(0x2F88D0)))(st + 0x98, 0.5f); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("NOFIN break: exception"); return; }
    float kc = 0; __try { kc = *(float*)(st + 0xAC); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    NfLog("NOFIN Ki primed boss %p (%s): Ki -> %.1f (next hit breaks it)", (void*)a, why, kc);
}
// test aid prime hold (kibreaknow), independent of the toggle so the toggle-off baseline gets the same native break
static uint64_t g_kbt_a; static DWORD g_kbt_until;
static int NfPrimeHold(uint64_t a, DWORD ms) { if (!a) return 0; g_kbt_a = a; g_kbt_until = GetTickCount() + ms; return 1; }
static __attribute__((noinline)) void NfTestPrime(void) {
    if (!g_kbt_a) return;
    uint64_t a = g_kbt_a, st = 0, ai = 0; float kc = 0; int brk = 0;
    if ((int)(g_kbt_until - GetTickCount()) <= 0 || !Rd64(a + 0x3A0, &st) || !st || !Rd64(a + 0x3B0, &ai) || !ai) { g_kbt_a = 0; return; }
    __try { kc = *(float*)(st + 0xAC); brk = KbState(ai, 0x23) || *(float*)(st + 0x98 + 0x5C) > 0 || *(float*)(st + 0x98 + 0x74) > 0 || *(float*)(st + 0x98 + 0x7C) > 0 || *(uint8_t*)(st + 0x98 + 0x83);
            if (brk) { g_kbt_a = 0; return; } if (kc > 0.5f) ((void (*)(uint64_t, float))(B + Grva(0x2F88D0)))(st + 0x98, 0.5f); } __except (EXCEPTION_EXECUTE_HANDLER) { g_kbt_a = 0; }
}
static __attribute__((noinline)) void NfTick(void) {
    NfTestPrime(); NfTraceTick();
    {   static LONG lastCg; static DWORD tCg; LONG c = g_nf_cnt_cg; DWORD now0 = GetTickCount();
        if (c != lastCg && now0 - tCg > 1000) { tCg = now0; uint64_t t = g_nf_cg_tgt, a = NfFirstBoss(), ai = 0, ctrl = 0; if (a && Rd64(a + 0x3B0, &ai) && ai) Rd64(ai + 8, &ctrl);
            NfLog("NOFIN diag CanGrapple calls %ld (+%ld), last target %p: %s (boss %p ai %p ctrl %p), blocked %ld, toggle %d", c, c - lastCg, (void*)t, t && t == a ? "THE BOSS" : t && (t == ai || t == ctrl) ? "boss ai/ctrl" : "other", (void*)a, (void*)ai, (void*)ctrl, g_nf_cnt_sel, g_cfg_nofin);
            lastCg = c; }
    }
    if (!g_cfg_nofin) { if (g_nf_gold || g_nf_act[0] || g_nf_act[1] || g_nf_act[2] || g_nf_act[3]) { for (int s = 0; s < NF_N; ++s) NfClear(s); g_nf_gold = 0; } g_nf_goldw = 0.0f; return; }
    if (!g_gold_hooked) { HookGoldKi(); HookGoldQuad(); g_gold_hooked = 1; }  // turned on at run time (Boss Rematch menu / 'nofin 1')
    if (!g_nf_hooked) NfInstallHooks();
    DWORD now = GetTickCount();
    uint64_t act[NF_N] = {0}; uint32_t ids[NF_N] = {0}; int n = 0;
    {   uint64_t root = 0, hud = 0, r0 = 0, r1 = 0;  // boss gauge rows (as KbCollect / BossGaugeRowsUpdate) + the boss-mode boss
        if (Rd64(B + Grva(0x45C84A8), &root) && root && Rd64(root + 0xA8, &hud) && hud && Rd64(hud + 0x3C0, &r0) && Rd64(hud + 0x3C8, &r1) && r0 && r1 > r0 && (r1 - r0) / 0x60 <= 8)
            for (uint64_t r = r0; r < r1 && n < NF_N; r += 0x60) {
                uint32_t slot = 0, id = 0; __try { slot = *(uint32_t*)r; id = *(uint32_t*)(r + 4); } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
                if (slot >= 3 || !id) continue;
                uint64_t w = FindWrapper(id), a = 0; if (!w || !Rd64(w, &a) || !a) continue;
                int dup = 0; for (int i = 0; i < n; ++i) if (act[i] == a) dup = 1; if (!dup) { ids[n] = id; act[n++] = a; }
            }
        if (g_bm.active && n < NF_N) { uint64_t a = ActorByHandle(g_bm.id); int dup = 0; for (int i = 0; i < n; ++i) if (act[i] == a) dup = 1; if (a && !dup) { ids[n] = 0; act[n++] = a; } }
    }
    for (int s = 0; s < NF_N; ++s) if (g_nf[s].a) { int here = 0; for (int k = 0; k < n; ++k) if (act[k] == g_nf[s].a) here = 1; if (!here && now - g_nf[s].seen > 3000) NfClear(s); }
    int gold = 0; float fadeW = 0.0f;
    for (int k = 0; k < n; ++k) {
        uint64_t a = act[k], st = 0, ai = 0, ctrl = 0, hc = 0, hm = 0;
        if (!Rd64(a + 0x3A0, &st) || !st || !Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl || !ActorHp(a, &hc, &hm) || !hm) continue;
        int s = -1; for (int i = 0; i < NF_N; ++i) if (g_nf[i].a == a) s = i;
        if (s < 0) { for (int i = 0; i < NF_N; ++i) if (!g_nf[i].a) { s = i; break; } if (s < 0) continue; NfClear(s); g_nf[s].a = a; g_nf[s].hits0 = g_nf_hits[s];
            NfLog("NOFIN watch boss %p (row id %u) HP %llu/%llu: grapple blocked, gold Ki while broken, HP-0 window %d ms", (void*)a, ids[k], hc, hm, g_cfg_nofin_ms); }
        g_nf[s].seen = now; if (ids[k]) g_nf[s].id = ids[k];
        if (g_nf_nonlethal && g_nf_reserve && g_nf_hprow_ok && !g_nf_off[s] && g_nf[s].win == 0 && hc > 0 && hc <= hm / 5 && g_nf[s].id) {  // #9319 hidden HP reserve
            int dead0 = 0; __try { dead0 = *(int32_t*)(st + 0x48) == 1; } __except (EXCEPTION_EXECUTE_HANDLER) { dead0 = 1; }
            if (!dead0) { int64_t off = (int64_t)(hm / 5); __try { ((void (*)(uint64_t, uint64_t))(B + Grva(0x66C188)))(st + 0x38, hc + (uint64_t)off); g_nf_off[s] = off; } __except (EXCEPTION_EXECUTE_HANDLER) { }
                NfLog("NOFIN boss %p HP %llu/%llu: hidden reserve %lld added (real HP %llu, HP bar shows real - reserve; no hit can be lethal now)", (void*)a, hc, hm, (long long)off, hc + (uint64_t)off); }
        }
        if (g_nf_off[s] > 0) { uint64_t real = hc; ActorHp(a, &real, &hm); hc = real > (uint64_t)g_nf_off[s] ? real - (uint64_t)g_nf_off[s] : 0; if (hc < 1) hc = 1; }
        { static uint64_t lastHp[NF_N]; static int nHpLog; if (hc != lastHp[s] && hc * 20 < hm && nHpLog < 80) { nHpLog++; NfLog("NOFIN boss %p HP %llu -> %llu (win %d)", (void*)a, lastHp[s], hc, g_nf[s].win); } lastHp[s] = hc; }
        int dead = 0, s23 = 0; float x5c = 0, x74 = 0, x7c = 0, kc = 0; uint8_t x83 = 0;
        __try { dead = *(int32_t*)(st + 0x48) == 1; x5c = *(float*)(st + 0x98 + 0x5C); x74 = *(float*)(st + 0x98 + 0x74); x7c = *(float*)(st + 0x98 + 0x7C); x83 = *(uint8_t*)(st + 0x98 + 0x83); kc = *(float*)(st + 0xAC);
                s23 = KbState(ai, 0x23); } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        // user #8894: Yoritomo sat at Ki 0 for 5 s+ (16:30:27-32) with neither state 0x23 nor the exhausted fields set: no gold, no block. Ki <= 0 = stagger too.
        // #9133: an exhausted field alone (0x23 over) no longer holds the gold once the Ki refills (> 0): that kept a full gold bar while the native bar refilled
        int exh = x5c > 0 || x74 > 0 || x7c > 0 || x83, ki0 = kc <= 0.0f && g_nf[s].win != 1, broken = s23 || (exh && kc <= 0.0f) || ki0;
        if (g_nf[s].win >= 2 && !dead && hc > NF_FLOOR) { g_nf[s].win = 0; g_nf[s].tries = 0; g_nf[s].aiHeld = 0; NfLog("NOFIN boss %p alive again (HP %llu, actor reused after despawn/summon): window re-armed", (void*)a, hc); }
        if (dead) { g_nf_off[s] = 0; if (g_nf[s].win == 1) NfLog("NOFIN boss %p died during the HP-0 window", (void*)a); g_nf_act[s] = 0; g_nf_ai[s] = 0; g_nf_ctrl[s] = 0; g_nf_drain[s].on = 0; g_nf_goldact[s] = 0; g_nf_goldctrl[s] = 0; g_nf[s].win = 3; continue; }
        g_nf_act[s] = a; g_nf_ai[s] = ai; g_nf_ctrl[s] = ctrl; g_nf_chr[s] = st;
        {   static int prev23[NF_N]; if (s23 && !prev23[s] && ((g_nf[s].win == 0 && hc > 1) || g_nf[s].win == 1)) {  // #9987: the HP-0 window's break too (death-like crouch FA6/4E3E seen while comboing)  // native Ki break started: learn its stagger motion id (#9297)
                uint32_t mid = g_mot_id[s]; DWORD mt = g_mot_t[s];
                if (mid && mid != 0x308 && now - mt < 400) { if (g_nf_brk_id != mid) NfLog("NOFIN boss %p Ki-break stagger motion learned: 0x%X (requested %lu ms before the break was seen)", (void*)a, mid, now - mt); g_nf_brk_id = mid; }
                else NfLog("NOFIN boss %p Ki break started, no motion request in the last 400 ms (last 0x%X %lu ms ago)", (void*)a, mid, mt ? now - mt : 0);
                g_nf[s].brkAct = ActIdOf(ctrl); g_nf[s].tBrk = now ? now : 1; g_nf[s].tSwap = 0; g_nf[s].tFixLast = 0; g_nf[s].nFix = 0; }  // #9895 break start action (log); #9937 fix state reset
            if (!s23) g_nf[s].tBrk = 0;
            prev23[s] = s23; }
        // #9946/#10014/#10069 learn the boss's good Ki-break action = the action it holds >= 300 ms inside its own REAL Ki-break stagger (state 0x23 with the
        // stagger action's runtime bits 62/45/33 next to the break bit 0x3A; Yoritomo: 0x4E4C, bits 4400200200003000). First one per boss wins (= the stagger
        // start action), never overwritten, saved per boss in Nioh3Training_learned.ini so the next session has it before its first break.
        // #10069 kibreak_fix3: no longer learned from the gold-Ki phase (Ki < 0, 0x23 off): those are hit-react/exhausted moves (0x4E37/0x4E38/0x4E42); forcing
        // one ended the break (03:36:08: 0x4E42 forced, 0x23 off 15 ms later). Bug fix: the old test required bit 0x3A CLEAR, which a real stagger never has.
        if (g_nf_actfix && g_nf[s].win == 0 && hc > NF_FLOOR) {
            static uint32_t lastA[NF_N]; static DWORD tA[NF_N]; uint32_t cur = ActIdOf(ctrl); uint64_t wg = 0;
            __try { wg = *(volatile uint64_t*)(ctrl + 0x1060); } __except (EXCEPTION_EXECUTE_HANDLER) { }
            int stag = s23 && (wg & ((1ull << 62) | (1ull << 45) | (1ull << 33))) != 0;
            if (!stag || cur != lastA[s]) { lastA[s] = cur; tA[s] = now; }
            else if (now - tA[s] >= 300u) { int l = NfLrn(g_tr.active && g_tr.chr ? g_tr.chr : g_nf[s].id ? g_nf[s].id : 0xFFFF);
                if (!g_nf_lrn[l].good && cur < 0xFFFF && !NfActBadFor(l, cur)) { g_nf_lrn[l].good = cur; NfLrnSave(l); NfLog("NOFIN boss %p: learned GOOD Ki-break action 0x%X (real stagger, bits %016llX, stable %lu ms, key %X)", (void*)a, cur, wg, now - tA[s], g_nf_lrn[l].id); } }
        }
        // user #23432/#23436 auto fallback stagger (auto_stagger.h): learn the boss's hit-reaction action from its own action runs outside any Ki break
        // (alive, not broken, Ki > 0, not in the HP-0 window). Runs that start <= 200 ms after a landed hit (hit probe 326E81) count as hit runs.
        if (g_nf_autostag && g_nf_actfix) {
            static AstTrack trk[NF_N]; static uint64_t trkA[NF_N];
            if (trkA[s] != a) { memset(&trk[s], 0, sizeof trk[s]); trkA[s] = a; }
            uint32_t cur = ActIdOf(ctrl), rid = 0, rms = 0; int rhit = 0;
            int learnable = g_nf[s].win == 0 && hc > NF_FLOOR && !broken && !s23 && kc > 0.0f;
            if (cur != 0xFFFFFFFFu && AstStep(&trk[s], cur, now, g_nf_hitT[s], learnable, &rid, &rhit, &rms)) {
                int l = NfLrn(g_tr.active && g_tr.chr ? g_tr.chr : g_nf[s].id ? g_nf[s].id : 0xFFFF); AstAdd(g_nf_lrn[l].st, AST_N, rid, rhit, rms);
                if (rhit) { uint32_t pick = AstPick(g_nf_lrn[l].st, AST_N, NfAstBad, (void*)(intptr_t)l);
                    if (pick && pick != g_nf_lrn[l].autoId) { uint32_t was = g_nf_lrn[l].autoId; g_nf_lrn[l].autoId = pick; NfLrnSave(l);
                        int e = -1; for (int i = 0; i < AST_N; ++i) if (g_nf_lrn[l].st[i].id == pick) e = i;
                        static int nau; if (++nau <= 40 && e >= 0) NfLog("NOFIN boss %p: learned AUTO stagger (hit reaction) 0x%X: %u of %u runs start <= %d ms after a hit, avg %u ms (was 0x%X, real-stagger good 0x%X, key %X)",
                            (void*)a, pick, g_nf_lrn[l].st[e].nHit, g_nf_lrn[l].st[e].n, AST_HIT_MS, g_nf_lrn[l].st[e].ms / (g_nf_lrn[l].st[e].nHit ? g_nf_lrn[l].st[e].nHit : 1), was, g_nf_lrn[l].good, g_nf_lrn[l].id); } } }
        }
        // #9895 Ki-break action swap fix (see g_nf_actfix)
        if (g_nf_actfix && g_nf[s].win == 0 && hc > NF_FLOOR && s23 && g_nf[s].tBrk) {  // #10014: NOT in the HP-0 window - the forced swap to a non-stagger move ended the break after 125 ms and the boss moved away mid-combo
            // #9937: bad action id (KiBreakActBadIds) -> the good one (KiBreakActGoodId); brkAct = the start action, log only
            int l = NfLrn(g_tr.active && g_tr.chr ? g_tr.chr : g_nf[s].id ? g_nf[s].id : 0xFFFF);
            uint32_t cur = ActIdOf(ctrl), b0 = g_nf[s].brkAct, good = g_nf_lrn[l].good ? g_nf_lrn[l].good : g_nf_actgood; DWORD el = now - g_nf[s].tBrk;
            int isAuto = 0; if (!good && g_nf_autostag && g_nf_lrn[l].autoId && !NfActBadFor(l, g_nf_lrn[l].autoId)) { good = g_nf_lrn[l].autoId; isAuto = 1; }  // user #23432: no real stagger learned -> hit reaction
            // #10069 kibreak_fix3: the swap fires ONLY on a BARE break (break bit 0x3A without the stagger bits 62/45/33 = Ki broken but no stagger action: the
            // crouch / 'death stance', starts on 0xFA6 or 0x4E3E). A real stagger is never touched - its own 0x4E4C -> 0x4E3E change is natural (03:06:24: 7 hits,
            // full 3.8 s break) and the old id-based swap cut it short (03:28:43: 0x4E3E -> 0x4E42 forced, break over 11 ms later). Target = the boss's own learned
            // stagger start action, so the bare break gets the genuine stagger. Bad ids are still learned (log / ini) but no longer gate anything.
            uint64_t wb = 0; __try { wb = *(volatile uint64_t*)(ctrl + 0x1060); } __except (EXCEPTION_EXECUTE_HANDLER) { }
            int bare = (wb & (1ull << 0x3A)) && !(wb & ((1ull << 62) | (1ull << 45) | (1ull << 33)));
            int noanim = bare || cur == 0xFFFF;  // user #23432: Ki-broken (0x23) with NO action at all = no animation either
            if (bare && cur != good && !NfActBadFor(l, cur)) { NfLrnBad(l, cur); NfLog("NOFIN boss %p: learned BAD Ki-break action 0x%X (bare crouch bits %016llX, key %X)", (void*)a, cur, wb, g_nf_lrn[l].id); }
            if (noanim && !good) { static int nng; if (++nng <= 10) NfLog("NOFIN boss %p: %s Ki break (action 0x%X, bits %016llX) but no real-stagger action and no AUTO hit reaction learned for key %X yet -> left as is", (void*)a, bare ? "BARE" : "NO-ACTION", cur, wb, g_nf_lrn[l].id); }
            if (!noanim || !good || cur == good || el > (DWORD)g_nf_actfix_win) g_nf[s].tSwap = 0;
            else if (!g_nf[s].tSwap) g_nf[s].tSwap = now ? now : 1;
            else if (now - g_nf[s].tSwap >= (DWORD)g_nf_actfix_ms && g_nf[s].nFix < g_nf_actfix_max && (!g_nf[s].tFixLast || now - g_nf[s].tFixLast >= (DWORD)g_nf_actfix_gap)) {
                uint64_t w0 = 0; int ok = 1;
                __try { w0 = *(volatile uint64_t*)(ctrl + 0x1060);
                    if (g_nf_actfix_mode == 1) *(volatile uint32_t*)(ctrl + 0x2E4) = good;
                    else ((void (*)(uint64_t, uint64_t, uint64_t, float))(B + Grva(0x66C4A8)))(ctrl, (uint64_t)good, 0, 0.0f);
                } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
                g_nf[s].nFix++; g_nf[s].tSwap = 0; g_nf[s].tFixLast = now ? now : 1; g_nf_fbId[s] = isAuto ? good : 0;
                static int nfx; if (++nfx <= 60) NfLog("NOFIN boss %p HP %llu: bad Ki-break action 0x%X at +%lu ms while broken (start 0x%X, 23=1, bits1060 %016llX%s) -> %s action 0x%X requested (%s)%s, now 0x%X (#%d, KiBreakActFix)",
                    (void*)a, hc, cur, el, b0, w0, (w0 & (1ull << 0x3A)) && !(w0 & ((1ull << 62) | (1ull << 45) | (1ull << 33))) ? " BARE" : "", isAuto ? "AUTO stagger (hit reaction)" : "good", good,
                    g_nf_actfix_mode == 1 ? "pending +2E4" : "forced 66C4A8", ok ? "" : " FAULTED", ActIdOf(ctrl), g_nf[s].nFix);
            }
        }
        {   // #10069: one result line 200 ms after each swap: did the break stay on with the real stagger bits?
            static DWORD tChk[NF_N]; if (g_nf[s].tFixLast && g_nf[s].tFixLast != tChk[s] && now - g_nf[s].tFixLast >= 200) { tChk[s] = g_nf[s].tFixLast;
                uint64_t wr = 0; __try { wr = *(volatile uint64_t*)(ctrl + 0x1060); } __except (EXCEPTION_EXECUTE_HANDLER) { }
                static int nr; if (++nr <= 60) NfLog("NOFIN boss %p KiBreakActFix result +%lu ms: action 0x%X, 23=%d, bits1060 %016llX (%s)", (void*)a, now - g_nf[s].tFixLast, ActIdOf(ctrl), s23, wr,
                    !s23 ? "break ENDED" : (wr & ((1ull << 62) | (1ull << 45) | (1ull << 33))) ? "real stagger" : "still bare"); } }
        // prime hold: the whole HP-0 window keeps the Ki at 0.5 while the boss is not broken, so the next hit's Ki damage breaks it natively
        if (g_nf_prime && !broken && kc > 0.5f && g_nf[s].win == 1) { __try { if (g_nf_off[s] > 0) *(float*)(st + 0xAC) = 0.5f; else ((void (*)(uint64_t, float))(B + Grva(0x2F88D0)))(st + 0x98, 0.5f); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
        if (broken != g_nf[s].broken) { g_nf[s].broken = broken; if (!broken && g_nf[s].win == 1) g_nf[s].nRebreak++;
            NfLog("NOFIN boss %p Ki %s (23=%d exhausted %d ki0 %d, Ki %.1f, HP %llu/%llu); blocked so far: down target %ld, grapple target %ld, CanGrapple %ld, grab box %ld, plain grab box %ld, death-blow test %ld, bit-31 box %ld, bit-28 box %ld", (void*)a, broken ? "BROKEN -> gold Ki bar" : "recovered", s23, exh, ki0, kc, hc, hm,
                  g_nf_cnt_down, g_nf_cnt_tgt, g_nf_cnt_sel, g_nf_cnt_grab, g_nf_cnt_grab0, g_nf_cnt_st27, g_nf_cnt_b31, g_nf_cnt_b28); }
        // #9542 (user play 00:27-00:35): normal Ki breaks at HP > 0 came in two kinds: Ki <= 0 then state 0x23 (KIBREAK start, 3.8 s stagger, Ki back to
        // 55 %), and Ki <= 0 for 7-8 s with 0x23 / exhausted / 0x35 never set, then Ki straight to 70-88 % (00:29:26, 00:32:52, 00:35:16; no hit motion
        // requests in it) = the suspected crouch. Kneel signature undone like in the HP-0 window (668D74 from the game dropped in NfAiOffDetour) + state dump.
        if (g_nf[s].win == 0 && broken) {
            if (g_nf_kneelall) __try { uint8_t* k91b = (uint8_t*)(a + 0x91B); volatile uint32_t* r994 = (volatile uint32_t*)(ctrl + 0x994);
                if (*r994 == 0x27 || g_nf_kneelblk[s]) { int was91b = *k91b; uint32_t was994 = *r994; *k91b = 0; if (*r994 == 0x27) *r994 = 0; g_nf_kneelblk[s] = 0;
                    static int nk; if (++nk <= 30) NfLog("NOFIN boss %p Ki broken at HP %llu: native kneel undone (actor+91B %d, ctrl+994 0x%X, KiBreakKneelBlockAll)", (void*)a, hc, was91b, was994); } } __except (EXCEPTION_EXECUTE_HANDLER) { }
            if (!g_nf[s].tDiag || now - g_nf[s].tDiag >= 1000) { g_nf[s].tDiag = now; static int nd; if (++nd <= 80) __try {
                NfLog("NOFIN break diag boss %p HP %llu: Ki %.1f 23=%d 35=%d 9=%d B0=%d 2C=%d 27=%d exh %d, ctrl+994 0x%X actor+91B %d AI %d, bits1060 %016llX 1068 %016llX, last motion 0x%X %lu ms ago", (void*)a, hc, kc, s23,
                      KbState(ai, 0x35), KbState(ai, 0x9), KbState(ai, 0xB0), KbState(ai, 0x2C), KbState(ai, 0x27), exh, *(uint32_t*)(ctrl + 0x994), *(uint8_t*)(a + 0x91B), BhAiOn(a),
                      *(uint64_t*)(ctrl + 0x1060), *(uint64_t*)(ctrl + 0x1068), g_mot_id[s], g_mot_t[s] ? now - g_mot_t[s] : 0); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
        } else if (g_nf[s].win == 0) g_nf[s].tDiag = 0;
        // #9579 bare Ki break (see g_nf_barefix): cancel it and prime the Ki so the next hit gives the real stagger
        if (g_nf_barefix && g_nf[s].win == 0 && s23 && hc > NF_FLOOR) {
            uint64_t w0 = 0, wA8 = 0; __try { w0 = *(volatile uint64_t*)(ctrl + 0x1060); wA8 = *(volatile uint64_t*)(ctrl + 0xA8); } __except (EXCEPTION_EXECUTE_HANDLER) { }
            const uint64_t stag = (1ull << 62) | (1ull << 45) | (1ull << 33);
            // user #23432: while the AUTO fallback stagger we forced is still playing (<= KiBreakAutoHoldMs after the swap) the cancel waits
            int fbHold = g_nf_fbId[s] && g_nf[s].tFixLast && now - g_nf[s].tFixLast < (DWORD)g_nf_autohold && ActIdOf(ctrl) == g_nf_fbId[s];
            if (!fbHold && (w0 & (1ull << 0x3A)) && !(w0 & stag)) {
                if (!g_nf[s].tBare) g_nf[s].tBare = now ? now : 1;
                else if (now - g_nf[s].tBare >= (DWORD)g_nf_bare_ms) {
                    float k0 = kc; __try {
                        ((void (*)(uint64_t, uint64_t, uint8_t))(B + Grva(0xFC1A20)))(ctrl + 0x1060, 0x3A, 0);
                        InterlockedAnd64((volatile LONG64*)(ctrl + 0xA8), ~(LONG64)(1ull << 41));
                        *(volatile float*)(ctrl + 0x1014) = 0.0f;
                        if (*(float*)(st + 0xAC) < 0.5f) *(volatile float*)(st + 0xAC) = 0.5f;
                    } __except (EXCEPTION_EXECUTE_HANDLER) { }
                    g_nf[s].nBare++; g_nf[s].tBare = 0;
                    int s23b = KbState(ai, 0x23);
                    static int nb; if (++nb <= 40) NfLog("NOFIN boss %p HP %llu: BARE Ki break (bits1060 %016llX A8 %016llX, no stagger bits 62/45/33, action 0x%X, last motion 0x%X) for %d ms -> cancelled + Ki %.1f -> 0.5 (23 now %d, #%d, KiBreakBareFix)", (void*)a, hc, w0, wA8, ActIdOf(ctrl), g_mot_id[s], g_nf_bare_ms, k0, s23b, g_nf[s].nBare);
                }
            } else g_nf[s].tBare = 0;
        } else g_nf[s].tBare = 0;
        if (hc <= NF_FLOOR && g_nf[s].win == 0) {
            g_nf[s].win = 1; g_nf[s].tWin = g_nf[s].tLast = now; g_nf[s].hits0 = g_nf_hits[s]; g_nf[s].nHits = 0; g_nf[s].nRebreak = 0;
            NfLog("NOFIN HP-0 window start boss %p (HP %llu/%llu): Ki broken + gold bar draining %d ms; dies if not hit", (void*)a, hc, hm, g_cfg_nofin_ms);
            if (!exh && g_nf_prime && !g_nf_off[s]) { NfBreakNow(st, "HP 0", a); g_nf[s].tRebreak = now; }
        }
        if (g_nf[s].win == 1 && g_nf_hp0aihold) {  // #9542 boss AI held for the HP-0 window (no Ki-broken moves / dashes; hit reactions stay native)
            if (!g_nf[s].aiHeld) { g_nf[s].aiHeld = 1; g_nf[s].tAi = now; int was = BhAiOn(a); ((void (*)(uint64_t))(B + Grva(0x668D74)))(a);
                NfLog("NOFIN boss %p HP-0 window: boss AI held off (was %d, now %d, KiBreakHp0AiHold)", (void*)a, was, BhAiOn(a)); }
            else if (now - g_nf[s].tAi >= 100) { g_nf[s].tAi = now; if (BhAiOn(a) == 1) { ((void (*)(uint64_t))(B + Grva(0x668D74)))(a); if (++g_nf[s].nAiRe <= 10) NfLog("NOFIN boss %p HP-0 window: AI switched back on by the game, off again (#%d)", (void*)a, g_nf[s].nAiRe); } }
        }
        if (g_nf[s].win == 1) {
            // live 03:49 (nofinsnap/nofindiff, lethal vs normal hit): the lethal hit leaves ctrl+0xEA0 = 1 = death-blow pending (786C84's own cond 0x27
            // revive at 787004); while it is set every normal hit whiffs (03:40: 0 hits landed in the window, only the grapple/death blow could). Clear it.
            __try { if (*(volatile uint32_t*)(ctrl + 0xEA0)) { *(volatile uint32_t*)(ctrl + 0xEA0) = 0; static int nc; if (++nc <= 20) NfLog("NOFIN boss %p: death-blow pending flag ctrl+EA0 cleared (normal hits land again)", (void*)a); } } __except (EXCEPTION_EXECUTE_HANDLER) { }
            // user #8827 'grapple buggy' (play 16:09, and 15:41): at HP 1 the finisher still played (KIFINISHER 35=1 'HP floor (death blow)' 1921 ms, KIBREAK end #2
            // 'grapple yes' = that finisher state seen in the break) although the grapple target hook blocked 746 frames. Both plays had runtime bit 101
            // (ctrl+0x1068 bit 37, 'bits1068 0000002000000000') set from the HP-0 window on; no dev run (boss AI off, no finisher) ever had it -> it is the
            // death-blow-available state the AI/script sets at the HP floor. Clear it every pump frame of the window ([Summon] KiBreakHp0ClearDeathBlow, default 1).
            if (g_nf_nonlethal) __try { uint8_t* k91b = (uint8_t*)(a + 0x91B); volatile uint32_t* r994 = (volatile uint32_t*)(ctrl + 0x994);
                if (*k91b || *r994 == 0x27 || g_nf_kneelblk[s]) { int was91b = *k91b; uint32_t was994 = *r994; *k91b = 0; if (*r994 == 0x27) *r994 = 0; g_nf_kneelblk[s] = 0;
                    static int nk; if (++nk <= 20) NfLog("NOFIN boss %p: HP-floor kneel undone (actor+91B %d, ctrl+994 0x%X)", (void*)a, was91b, was994); } } __except (EXCEPTION_EXECUTE_HANDLER) { }
            if (g_nf_clear101) __try { volatile uint64_t* rb = (volatile uint64_t*)(ctrl + 0x1068); if (*rb & (1ull << 37)) { *rb &= ~(1ull << 37); g_nf_n101++; if (g_nf_n101 <= 10 || g_nf_n101 % 100 == 0) NfLog("NOFIN boss %p: death-blow runtime bit 101 (ctrl+1068 bit 37) cleared (#%ld)", (void*)a, g_nf_n101); } } __except (EXCEPTION_EXECUTE_HANDLER) { }
            // live 03:52: at HP 1 the hits' Ki damage takes the Ki below 0 but the native break never starts (lethal hits skip it), so the HP-0 window sets the
            // break itself: A4F098(ctrl) = the native break-window start (runtime bit 0x3A = state 0x23 + per-character timer ctrl+0x1014), plus, once per window,
            // the native kneel-down action (script Character::ForceKneelDown 238E1BC: ctrl+0x994 = 0x27, 668D74(actor), actor+0x91B = 1) = the stagger.
            if (!g_nf_nonlethal && !s23 && now - g_nf[s].tSet >= 250) {  // #9319: not with KiBreakHp0NonLethal (the bare bit 0x3A = 'broken' without the stagger)
                 g_nf[s].tSet = now; __try { ((void (*)(uint64_t))(B + Grva(0xA4F098)))(ctrl);
                    if (g_nf_kneel && !g_nf[s].kneeled) { g_nf[s].kneeled = 1; *(volatile uint32_t*)(ctrl + 0x994) = 0x27; ((void (*)(uint64_t))(B + Grva(0x668D74)))(a); *(volatile uint8_t*)(a + 0x91B) = 1; }
                } __except (EXCEPTION_EXECUTE_HANDLER) { }
                g_nf[s].nSet++; if (g_nf[s].nSet <= 10) NfLog("NOFIN boss %p at HP 0: Ki break set (A4F098%s), break #%d", (void*)a, g_nf_kneel && g_nf[s].kneeled == 1 ? " + kneel-down" : "", g_nf[s].nSet); if (g_nf[s].kneeled == 1) g_nf[s].kneeled = 2; }
            LONG h = g_nf_hits[s]; if (h != g_nf[s].hits0) { g_nf[s].nHits += (int)(h - g_nf[s].hits0); g_nf[s].hits0 = h; g_nf[s].tLast = now;
                if (g_nf[s].nHits <= 30) NfLog("NOFIN hit at HP 0 boss %p: window restarted (+%lu ms since start, hit %d)", (void*)a, now - g_nf[s].tWin, g_nf[s].nHits); }
            { static DWORD tk; if (now - tk >= 250) { tk = now; NfLog("NOFIN window boss %p: +%lu ms, Ki %.1f, 23=%d exh %d, hits %d, since last hit %lu ms", (void*)a, now - g_nf[s].tWin, kc, s23, exh, g_nf[s].nHits, now - g_nf[s].tLast); } }
            if (hc > NF_FLOOR) { g_nf[s].win = 0; if (g_nf[s].aiHeld) { g_nf[s].aiHeld = 0; ((void (*)(uint64_t))(B + Grva(0x66982C)))(a); } NfLog("NOFIN boss %p HP back to %llu: window off (AI back on)", (void*)a, hc); }
            else if (now - g_nf[s].tLast >= (DWORD)g_cfg_nofin_ms) {
                g_nf[s].win = 2; g_nf[s].tKill = now; g_nf[s].tries = 0;
                NfLog("NOFIN boss %p: no hit for %d ms at HP 0 (window %lu ms, %d hits, break ends %d) -> native kill (BossLoot %d)", (void*)a, g_cfg_nofin_ms, now - g_nf[s].tWin, g_nf[s].nHits, g_nf[s].nRebreak, g_cfg_loot);
                g_nf_drain[s].on = 0; g_nf_act[s] = 0; g_nf_ai[s] = 0; g_nf_ctrl[s] = 0;  // the kill hit must not be revived / filtered
                if (a == g_db_act) g_db_done = 1;
                g_nf_killt = now; g_nf_killact = a;  // #9543: KillFadeNow uses KiBreakHp0FadeDelayMs for this body
                ForceKillActorEx(a, g_cfg_loot, "KiBreakNoFinisher HP-0 window", 1);
                continue;
            }
        }
        if (g_nf[s].win == 2) {  // kill issued, boss still alive: retry (the death-blow gates are cleared from the 1st retry)
            g_nf_goldact[s] = a; g_nf_goldctrl[s] = ctrl; g_nf_act[s] = 0; g_nf_ai[s] = 0; g_nf_ctrl[s] = 0;  // #9133: down-target slot stays blocked until it is dead
            if (hc <= NF_FLOOR && g_nf[s].tries < 6 && now - g_nf[s].tKill >= 500u * (DWORD)(g_nf[s].tries + 1)) { g_nf[s].tries++; NfLog("NOFIN kill retry %d boss %p HP %llu", g_nf[s].tries, (void*)a, hc); g_nf_killt = now; g_nf_killact = a; ForceKillActorEx(a, g_cfg_loot, "KiBreakNoFinisher retry", 1); }
            continue;
        }
        int isGold = broken || g_nf[s].win == 1;
        float want = g_nf[s].win == 1 ? 1.0f - (float)(now - g_nf[s].tLast) / (float)g_cfg_nofin_ms : 1.0f;
        g_nf_drain[s].id = g_nf[s].id;
        if (isGold) { g_nf_drain[s].ratio = want; g_nf_drain[s].w = 1.0f; g_nf_drain[s].on = 1; g_nf[s].tFade = 0; g_nf[s].goldWas = 1; gold = 1; }  // broken: full gold bar (display only)
        else {  // #9133: gold over -> blend back to the native bar over KiBreakGoldFadeMs (tint gold -> native, fill last gold value -> real Ki)
            if (g_nf[s].goldWas) { g_nf[s].goldWas = 0; g_nf[s].tGoldEnd = now ? now : 1; g_nf[s].tFade = g_cfg_nf_fade > 0 ? (now ? now : 1) : 0; g_nf[s].fadeFrom = g_nf_drain[s].ratio; NfLog("NOFIN boss %p gold over (Ki %.1f): blending back to the native bar over %d ms", (void*)a, kc, g_cfg_nf_fade); }
            float w = 0.0f; if (g_nf[s].tFade && now - g_nf[s].tFade < (DWORD)g_cfg_nf_fade) w = 1.0f - (float)(now - g_nf[s].tFade) / (float)g_cfg_nf_fade; else g_nf[s].tFade = 0;
            g_nf_drain[s].ratio = g_nf[s].fadeFrom; g_nf_drain[s].w = w; g_nf_drain[s].on = w > 0.0f;
            if (w > fadeW) fadeW = w;
        }
        // #9133 (4th bug): the finisher slipped through after the 3.9 s break window ended. The downed/HP-floor target slot (2F7015) stays blocked for the
        // whole break, the HP-0 window, any time the boss sits at the HP floor (HP <= 1) until it dies, and 1.5 s after the gold ends.
        g_nf_goldact[s] = (isGold || hc <= NF_FLOOR || (g_nf[s].tGoldEnd && now - g_nf[s].tGoldEnd < 1500)) ? a : 0; g_nf_goldctrl[s] = g_nf_goldact[s] ? ctrl : 0;
    }
    g_nf_goldw = gold ? 1.0f : fadeW;
    if (gold != g_nf_gold) { g_nf_gold = gold; NfLog("NOFIN gold Ki bar %s", gold ? "ON" : "off"); if (gold && g_nf_dumpN <= 0) g_nf_dumpN = 48; }  // auto-dump the wide quads of the first frames (evidence if the strips do not match)
}

// ---- user #9595/#9596/#9658 in-mod debug hotkeys ([Debug] Hotkeys 1 = on; FreezeKey default 0x78 = F9, BugKey default 0x77 = F8; only while the
// game window is in front). F9 toggles a freeze of the whole fight + player: engine stop request #3 (counter G+0x59C, G = [base+4759910]; the latch
// 89B6EC sets stop flag G+0x58B and FrameDtSetup 5AD6C5 then zeroes the game time scale G+0x624 = what the game's own pause uses; the logic and
// render keep running, time does not advance). F9 again releases our request. F8 = 'BUG HERE' marker. Both log the boss state (action id, last
// motion, Ki, HP, state 0x23, break bits, AI) and save a screenshot jpg (GDI+; bmp if GDI+ fails) to [Debug] ShotDir (default [Record] OutRoot\marks).
// Runs on its own thread (the game-thread pump may not tick while time is stopped). Our real-time timers (HP-0 window, bare-break cancel) keep running.
static int g_dbg_on = 1, g_dbg_freeze_vk = 0x78, g_dbg_bug_vk = 0x77; static char g_dbg_dir[MAX_PATH]; static volatile LONG g_dbg_frozen, g_dbg_n;
static __attribute__((noinline)) int DbgBossLine(char* o, int cap) {
    int len = 0, nb = 0; DWORD now = GetTickCount(); o[0] = 0;
    for (int i = 0; i < KB_MAX; ++i) {
        uint64_t a = g_kb[i].a; if (!a || now - g_kb[i].seen > 5000) continue;
        uint64_t st = 0, ai = 0, ctrl = 0, hc = 0, hm = 0; float kc = 0, km = 0; int dead = 0, s23 = -1, s35 = -1; uint64_t w0 = 0, wA8 = 0;
        if (!Rd64(a + 0x3A0, &st) || !st || !Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) continue;
        ActorHp(a, &hc, &hm);
        __try { kc = *(float*)(st + 0xAC); km = *(float*)(st + 0xB0); dead = *(int32_t*)(st + 0x48) == 1; w0 = *(volatile uint64_t*)(ctrl + 0x1060); wA8 = *(volatile uint64_t*)(ctrl + 0xA8); s23 = KbState(ai, 0x23); s35 = KbState(ai, 0x35); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        uint32_t act = ActIdOf(ctrl), mid = 0; DWORD mage = 0; MotOf(a, &mid, &mage);
        int ns = -1; for (int k = 0; k < NF_N; ++k) if (g_nf[k].a == a) ns = k;
        int n = snprintf(o + len, cap - len, "%sboss %p: action 0x%X, last motion 0x%X %lu ms ago, HP %llu/%llu%s, Ki %.1f/%.1f, 23=%d 35=%d dead %d, AI %d, bits1060 %016llX A8 %016llX%s, Ki breaks %d, NOFIN win %d broken %d",
            nb ? " | " : "", (void*)a, act, mid, mage, hc, hm, ns >= 0 && g_nf_off[ns] > 0 ? " (real, incl. hidden reserve)" : "", kc, km, s23, s35, dead, BhAiOn(a), w0, wA8,
            (w0 & (1ull << 0x3A)) && !(w0 & ((1ull << 62) | (1ull << 45) | (1ull << 33))) ? " BARE" : "", g_kb[i].nBreaks, ns >= 0 ? g_nf[ns].win : -1, ns >= 0 ? g_nf[ns].broken : -1);
        if (n > 0 && len + n < cap) len += n; nb++;
    }
    if (!nb) len += snprintf(o + len, cap - len, "no boss watched");
    float pp[4]; if (PlayerPos(pp) && len < cap) snprintf(o + len, cap - len, "; player %.0f %.0f %.0f", pp[0], pp[1], pp[2]);
    return nb;
}
typedef struct { UINT32 ver; void* cb; BOOL noThread, noCodecs; } DbgGpIn;
static int DbgShot(const char* tag, char* out, int cap) {
    out[0] = 0; HWND w = GetForegroundWindow(); RECT r; DWORD pid = 0; if (w) GetWindowThreadProcessId(w, &pid);
    if (!w || pid != GetCurrentProcessId() || !GetWindowRect(w, &r) || r.right - r.left < 64) { r.left = 0; r.top = 0; r.right = GetSystemMetrics(SM_CXSCREEN); r.bottom = GetSystemMetrics(SM_CYSCREEN); }
    else { POINT p0 = {0, 0}; RECT c; if (GetClientRect(w, &c) && ClientToScreen(w, &p0) && c.right > 64) { r.left = p0.x; r.top = p0.y; r.right = p0.x + c.right; r.bottom = p0.y + c.bottom; } }
    int W = r.right - r.left, H = r.bottom - r.top; if (W <= 0 || H <= 0) return 0;
    HDC sdc = GetDC(0), mdc = CreateCompatibleDC(sdc); HBITMAP bm = CreateCompatibleBitmap(sdc, W, H); HGDIOBJ old = SelectObject(mdc, bm);
    BitBlt(mdc, 0, 0, W, H, sdc, r.left, r.top, SRCCOPY | CAPTUREBLT); SelectObject(mdc, old);
    CreateDirectoryA(g_dbg_dir, 0);
    SYSTEMTIME t; GetLocalTime(&t); char base[MAX_PATH + 64]; snprintf(base, sizeof base, "%s\\mark_%02d%02d%02d_%03d_%s", g_dbg_dir, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, tag);
    int ok = 0;
    {   static HMODULE gp; static ULONG_PTR tok; static int (WINAPI *gStart)(ULONG_PTR*, const DbgGpIn*, void*); static int (WINAPI *gFromH)(HBITMAP, HPALETTE, void**);
        static int (WINAPI *gSave)(void*, const WCHAR*, const GUID*, const void*); static int (WINAPI *gDisp)(void*); static int inited;
        if (!inited) { inited = 1; gp = LoadLibraryA("gdiplus.dll");
            if (gp) { *(FARPROC*)&gStart = GetProcAddress(gp, "GdiplusStartup"); *(FARPROC*)&gFromH = GetProcAddress(gp, "GdipCreateBitmapFromHBITMAP"); *(FARPROC*)&gSave = GetProcAddress(gp, "GdipSaveImageToFile"); *(FARPROC*)&gDisp = GetProcAddress(gp, "GdipDisposeImage");
                DbgGpIn in = {1, 0, FALSE, FALSE}; if (!gStart || !gFromH || !gSave || !gDisp || gStart(&tok, &in, 0) != 0) gp = 0; } }
        if (gp) { static const GUID jpg = {0x557CF401, 0x1A04, 0x11D3, {0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E}}; void* img = 0; WCHAR wp[MAX_PATH + 80];
            snprintf(out, cap, "%s.jpg", base); MultiByteToWideChar(CP_ACP, 0, out, -1, wp, MAX_PATH + 80);
            if (gFromH(bm, 0, &img) == 0 && img) { ok = gSave(img, wp, &jpg, 0) == 0; gDisp(img); } }
    }
    if (!ok) {  // 24-bit bmp fallback
        snprintf(out, cap, "%s.bmp", base); BITMAPINFOHEADER bi; memset(&bi, 0, sizeof bi); bi.biSize = sizeof bi; bi.biWidth = W; bi.biHeight = H; bi.biPlanes = 1; bi.biBitCount = 24; bi.biCompression = BI_RGB;
        DWORD row = ((DWORD)W * 3 + 3) & ~3u, sz = row * (DWORD)H; uint8_t* px = (uint8_t*)VirtualAlloc(0, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (px && GetDIBits(mdc, bm, 0, (UINT)H, px, (BITMAPINFO*)&bi, DIB_RGB_COLORS)) {
            BITMAPFILEHEADER fh; memset(&fh, 0, sizeof fh); fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof bi; fh.bfSize = fh.bfOffBits + sz;
            HANDLE f = CreateFileA(out, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0); if (f != INVALID_HANDLE_VALUE) { DWORD wr; WriteFile(f, &fh, sizeof fh, &wr, 0); WriteFile(f, &bi, sizeof bi, &wr, 0); WriteFile(f, px, sz, &wr, 0); CloseHandle(f); ok = 1; } }
        if (px) VirtualFree(px, 0, MEM_RELEASE);
    }
    DeleteObject(bm); DeleteDC(mdc); ReleaseDC(0, sdc);
    if (!ok) snprintf(out, cap, "(screenshot failed)");
    return ok;
}
static volatile LONG* DbgStopCounter(void) { uint64_t g = 0; if (!Rd64(B + Grva(0x4759910), &g) || !g) return 0; return (volatile LONG*)(g + 0x590 + 4 * 3); }
static __attribute__((noinline)) float DbgTimeScale(void) { uint64_t g = 0; float v = -1.0f; if (Rd64(B + Grva(0x4759910), &g) && g) { __try { v = *(volatile float*)(g + 0x624); } __except (EXCEPTION_EXECUTE_HANDLER) { } } return v; }
static __attribute__((noinline)) void DbgFreeze(int on) {
    volatile LONG* c = DbgStopCounter(); if (!c) { Log("DBG FREEZE: no frame manager"); return; }
    __try {
        if (on) { InterlockedIncrement(c); InterlockedExchange(&g_dbg_frozen, 1); }
        else { LONG v; do { v = *c; if (v <= 0) break; } while (InterlockedCompareExchange(c, v - 1, v) != v); InterlockedExchange(&g_dbg_frozen, 0); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("DBG FREEZE: stop counter write faulted"); }
}
static DWORD WINAPI DbgThread(LPVOID p) {
    (void)p; int fWas = 1, bWas = 1;  // ignore keys already held at start
    Log("DBG hotkeys (#9658): freeze VK 0x%X, BUG HERE VK 0x%X, shots -> %s", g_dbg_freeze_vk, g_dbg_bug_vk, g_dbg_dir);
    for (;;) {
        Sleep(30);
        HWND w = GetForegroundWindow(); DWORD pid = 0; if (w) GetWindowThreadProcessId(w, &pid);
        int fg = pid == GetCurrentProcessId();
        int f = fg && g_dbg_freeze_vk && (GetAsyncKeyState(g_dbg_freeze_vk) & 0x8000), b = fg && g_dbg_bug_vk && (GetAsyncKeyState(g_dbg_bug_vk) & 0x8000);
        if (f && g_pst_live && !g_dbg_frozen && g_dbg_freeze_vk == g_fz_vk_freeze) f = 0;  // user #22589: in Practice this key drives the Practice freeze (FzKeyThread)
        if (f && !fWas) {
            int on = !g_dbg_frozen; LONG n = InterlockedIncrement(&g_dbg_n); char line[1400], shot[MAX_PATH + 96];
            if (on) { DbgFreeze(1); DbgBossLine(line, sizeof line); Sleep(120); DbgShot("freeze", shot, sizeof shot);
                Log("DBG FREEZE ON #%ld (F9; time scale now %.3f, 0 = frozen): %s; screenshot %s", n, DbgTimeScale(), line, shot); }
            else { DbgBossLine(line, sizeof line); DbgFreeze(0); Log("DBG FREEZE OFF #%ld (F9): %s", n, line); }
        }
        if (b && !bWas) {
            LONG n = InterlockedIncrement(&g_dbg_n); char line[1400], shot[MAX_PATH + 96];
            DbgBossLine(line, sizeof line); Log("BUG HERE #%ld (F8)%s: %s", n, g_dbg_frozen ? " (frozen)" : "", line);
            DbgShot("bug", shot, sizeof shot); Log("BUG HERE #%ld screenshot %s", n, shot);
        }
        fWas = f; bWas = b;
    }
    return 0;
}

// user #13274: our mod menus pause the game during Boss Matches and Practice. Same stop counter as the F9 freeze above (the game's own pause:
// time scale 0, logic and render keep running), held by its own flag so it never releases an F9 freeze or the native pause.
// Held only while (1) the EngineFix mod menu is open (Nioh3EngineFix_MenuOpen, polled every 30 ms on this thread), (2) a Training / Boss Match
// boss actor is alive in the scene, and (3) nothing time-driven is running: no load, arena swap / prewarm, return trip, fade, arena settle,
// summon or queued summon request. Released the moment any of these ends (closing the menu releases within 30 ms), and before a Return.
// [Training] MenuPause = 0 turns it off. See TECH_DEBT.md (live acceptance pending).
static int g_cfg_menupause = 1;
static __attribute__((noinline)) void MenuFreeze(int on, const char* why) {
    volatile LONG* c = DbgStopCounter(); if (!c) return;
    __try {
        if (on) { if (InterlockedCompareExchange(&g_menu_frozen, 1, 0) == 0) InterlockedIncrement(c); else return; }
        else { if (InterlockedCompareExchange(&g_menu_frozen, 0, 1) != 1) return; LONG v; do { v = *c; if (v <= 0) break; } while (InterlockedCompareExchange(c, v - 1, v) != v); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("MENU PAUSE: stop counter write faulted"); return; }
    Log("MENU PAUSE %s (%s; time scale now %.3f)", on ? "ON" : "OFF", why, DbgTimeScale());
}
static int MenuPauseBusy(void) {
    const uint8_t* gs = GameState();
    return (gs && gs[2]) || g_rh.active || g_xw.active || g_xr.active || g_fd.pend || g_fd.phase == 2 || (g_ar.active && !g_ar.released) || g_sp.active || g_rq_op;
}
// Pause requires a living actor, not merely an arena/session flag after a kill.
static int MenuPauseActorPresent(uint64_t a) {
    int dead = 0; uint64_t hp = 0, max = 0;
    return a && ActorInTable(a) && !ActorDeadNow(a, &dead, &hp, &max) && max && hp;
}
static int MenuPauseBossPresent(void) {
    for (int s = 0; s < N3T_SLOTS; ++s)
        if (SlotAlive(s) && MenuPauseActorPresent(g_slot_act[s])) return 1;
    uint64_t actors[8]; const int n = KbCollect(actors, 8);
    for (int i = 0; i < n; ++i) if (MenuPauseActorPresent(actors[i])) return 1;
    return 0;
}
static volatile LONG g_menu_pause_allowed;
static volatile DWORD g_menu_pause_checked;
N3T_API int Nioh3Training_MenuPauseAllowed(void) {
    const DWORD checked = g_menu_pause_checked;
    return checked && (DWORD)(GetTickCount() - checked) < 250 && g_menu_pause_allowed;
}
static DWORD WINAPI MenuPauseThread(LPVOID p) {
    (void)p; Log("MENU PAUSE (#13274): on while the mod menu is open in a Boss Match / Practice session");
    for (;;) {
        Sleep(30);
        const int open = GateMenuOpen(), busy = open ? MenuPauseBusy() : 0;
        const int live = open && !busy && MenuPauseBossPresent();
        InterlockedExchange(&g_menu_pause_allowed, live);
        g_menu_pause_checked = GetTickCount();
        const int want = g_cfg_menupause && open && live && !busy;
        if (want && !g_menu_frozen) MenuFreeze(1, "mod menu opened");
        else if (!want && g_menu_frozen) MenuFreeze(0, !open ? "mod menu closed" : !live ? "session ended" : "a load / swap / summon started");
    }
    return 0;
}

#include "boss_vitals_game.h"  // user #14320/#14323/#14324: per-boss HP / Ki controls (Boss A / B pages)
#include "practice_stats_game.h"  // user #22389: player infinite Ki, damage multipliers, boss Ki drain
#include "frame_probe_game.h"  // user #22356: FRAMEPROBE log-only boss frame data ([Debug] FrameProbe, default 0)
#include "frame_meter_game.h"  // Practice frame meter ([Practice] FrameMeter, default 0): drawn from the FRAMEPROBE signals
#include "autobind_game.h"  // pc2 #23565: automatic boss action-file binding ([Debug] AutoBind, default 1, log only)
#include "auto_moves_game.h"  // pc2 #23907: runtime boss move list from the loaded action file ([Debug] AutoMoves, default 1)
static void N3CatMergeAuto(uint32_t chr, const AmTable* t) {
    char nm[64] = {0}; Nioh3Training_BossName(chr, nm, (int)sizeof nm);
    const int add = N3CatMerge(chr, t->m, t->n, nm);
    Log("AUTOMOVES chr %u: move catalogue now from game data (%d moves, %d new rows; next links, hits and unblockable replaced)", chr, t->n, add);
}
// pc2 #23907: the runtime move list of a boss for Nioh3BossRematch (its Boss moves page). abi = sizeof(AmMove) of the caller (-1 if it differs);
// returns the number of moves copied (0 = not read yet / AutoMoves off).
// BossRematch 0814ce4e was built before anim_hash/damage were added, so its record is the 20-byte prefix (through kind).
N3T_API int Nioh3Training_AutoMoves(uint32_t chr, void* out, int cap, int abi) {
    const int full = (int)sizeof(AmMove);
    const int old = (int)offsetof(AmMove, anim_hash);
    static int told;
    if (!told) { told = 1; Log("AUTOMOVES export: caller %d bytes, current record %d, installed Boss Rematch prefix %d", abi, full, old); }
    if (abi != full && abi != old) return -1;
    const AmTable* t = AmFor(chr); if (!t || !out || cap <= 0) return 0;
    const int n = t->n < cap ? t->n : cap;
    if (abi == full) { memcpy(out, t->m, sizeof(AmMove) * (size_t)n); return n; }
    uint8_t* d = (uint8_t*)out;
    for (int i = 0; i < n; ++i) memcpy(d + (size_t)i * (size_t)old, &t->m[i], (size_t)old);
    return n;
}
#include "negki_meter_game.h"  // user #23400 negative Ki meter ([Practice] NegKiMeter, default 1)
#include "practice_freeze.h"
#include "practice_freeze_game.h"  // user #22589: Practice freeze + frame advance (F9 / F6, Practice hub rows)
// user #14309: a boss that just took slot s starts with the defaults: Stand still (idle), aggro = saved BossAggro, HP / Ki controls OFF.
static void SlotNew(int s) {
    if (s < 0 || s >= N3T_SLOTS) return;
    InterlockedExchange(&g_slot_fight[s], 0); InterlockedExchange(&g_slot_rehold[s], 0); InterlockedExchange(&g_slot_aggro[s], g_cfg_aggro);
    g_slot_ih[s] = 0; BvInit(); BvDefaults(&g_bv[s]); g_bv_live[s].live = 0;
}
typedef uint64_t (*PumpFn)(uint64_t, uint64_t, uint64_t, uint64_t);
static PumpFn o_pump;
static __attribute__((noinline)) uint64_t h_pump(uint64_t c, uint64_t d, uint64_t r8, uint64_t r9) {
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
    __try { XsTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("XSTREAM tick exception %08lX", GetExceptionCode()); if (g_xs.patched) CellCapSet(4); if (g_xs.lgs) LoaderGatePatch(0); g_xs.lgs = 0; g_xs.on = 0; g_xs.patched = 0; }
    __try { LeaveTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("LEAVE ARENA exception %08lX", GetExceptionCode()); }
    __try { RhTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("RETURN tick exception %08lX", GetExceptionCode()); g_rh.active = 0; RhLoadScreen(0); }
    __try { GoldKiRatios(); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { GateTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("GATE tick exception %08lX", GetExceptionCode()); g_gate.tried = 3; }
    __try { XwTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("XWORLD tick exception %08lX", GetExceptionCode()); g_xw.active = 0; g_xw_busy = 0; InterlockedExchange(&g_xw_skip, 0); }
    __try { PsFarTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PSKIP far tick exception %08lX", GetExceptionCode()); PsClose("exception"); }
    __try { FadeTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("FADE tick exception %08lX", GetExceptionCode()); g_fd.pend = 0; g_fd.phase = 0; g_fd.inside = 0; g_fd_want = 0; }
    __try { PreloadTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PRELOAD tick exception %08lX", GetExceptionCode()); }
    __try { BgmTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("BGM tick exception %08lX", GetExceptionCode()); g_bgm_on = 0; }
    __try { GaugeTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("GAUGE tick exception %08lX", GetExceptionCode()); g_gg.pending = 0; }
    __try { KiBreakTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { MvTick(); ActLogTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { }  // #9508 boss action-id logger (read only)  // #7290 passive Ki-break finisher log (read only), before BossTick
    __try { AbTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_cfg_ab = 0; }  // pc2 #23565
    __try { AmTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_cfg_am = 0; Log("AUTOMOVES tick exception %08lX, off", GetExceptionCode()); }  // pc2 #23907
    __try { FpTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { FpUnlock(); }  // user #22356 frame probe (log only, off unless [Debug] FrameProbe 1)
    __try { FzTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { FzFail(); }  // user #22589 Practice freeze + frame advance (after the meter clock)
    __try { NfTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("NOFIN tick exception %08lX", GetExceptionCode()); }  // #7465 KiBreakNoFinisher, before BossTick
    __try { BhTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("BOSSHOLD tick exception %08lX", GetExceptionCode()); g_bh.phase = 0; }  // #9133
    __try { BossTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("BBM tick exception %08lX", GetExceptionCode()); g_bm.active = 0; }
    __try { ArenaTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("ARENA tick exception %08lX", GetExceptionCode()); g_ar.active = 0; }
    __try { AwareTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_aw.a = 0; }
    __try { InfHpTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { }  // user #22918
    __try { TrainTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("TRAIN tick exception %08lX", GetExceptionCode()); g_tr.active = 0; }
    __try { BvTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("BOSSVITALS tick exception %08lX", GetExceptionCode()); }  // user #14320
    __try { PstTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PRACTICE stats tick exception %08lX", GetExceptionCode()); }  // user #22389
    __try { RqTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("MENU request tick exception %08lX", GetExceptionCode()); }
    __try { SpPruneTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PRUNE tick exception %08lX", GetExceptionCode()); }  // crash 2026-10-18
    __try { SumNoDeathTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SUMNODEATH tick exception %08lX", GetExceptionCode()); }  // user #13197
    __try { StanceTimerTick(); SphTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SUMPASSIVE tick exception %08lX", GetExceptionCode()); for (int k = 0; k < N3T_SLOTS; ++k) g_sph[k].phase = 0; }  // user #13164
    __try { SpawnTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("SPAWN tick exception %08lX", GetExceptionCode()); g_sp.active = 0; }
    __try { MvRepTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("REPEAT tick exception %08lX", GetExceptionCode()); }
    __try { IhTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("IDLEHOLD tick exception %08lX", GetExceptionCode()); IhStop("exception", 1); }
    __try { DrillTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("DRILL tick exception %08lX", GetExceptionCode()); g_dr_off = 1; DrStop("exception"); }
    __try { AggroTick(); ResetPosTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("AGGRO tick exception %08lX", GetExceptionCode()); g_ag_off = 1; for (int i = 0; i < MV_CHASE_MAX; ++i) g_mv_chase[i] = 0; }  // user #13787
    return o_pump(c, d, r8, r9);
}

// Chain the per-frame call at 56529C (E8 rel32; BossRematch may already redirect it): retarget rel32 to a near stub.
static int HookPump(void) {
    uint8_t* site = (uint8_t*)(B + Grva(0x56529C));
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
    GrVerify();  // .text is decrypted by now: log how many rvas.json entries are found (game RVAs ...)
    ArLoadGame();  // immutable facts loaded before installing any hook that can publish AutoMoves
    // sanity: verify prologues before hooking
    const uint8_t dynp[] = {0x48, 0x8B, 0xC4, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57};
    const uint8_t lchp[] = {0x89, 0x54, 0x24, 0x10, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0xC2, 0x45, 0x33, 0xC0};
    { static const uint8_t fwp[12] = {0x48, 0x8B, 0x09, 0x45, 0x33, 0xC0, 0x44, 0x8B, 0xDA, 0x48, 0x85, 0xC9};
      if (!memcmp((void*)(B + Grva(0x135B78)), fwp, 12)) { o_fw = (FwFn)Hook(0x135B78, 12, h_fw); Log("hook FindWrapper ok (boss gauge for dynamic bosses)"); } else Log("hook FindWrapper: prologue mismatch, boss gauge falls back to the legacy call"); }
    if (!memcmp((void*)(B + Grva(0x1C0C5E0)), dynp, 13)) { o_dyn = (DynFn)Hook(0x1C0C5E0, 13, h_dyn); Log("hook CreateDynamic ok"); }
    else Log("hook CreateDynamic: prologue mismatch");
    if (!memcmp((void*)(B + Grva(0xA42A7C)), lchp, 14)) { o_lchr = (LoadChrFn)Hook(0xA42A7C, 14, h_lchr); Log("hook Load::Character ok"); }
    else Log("hook Load::Character: prologue mismatch");
    { const uint8_t postp[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20};
      if (!memcmp((void*)(B + Grva(0x60BE48)), postp, 15)) { o_post = (PostFn)Hook(0x60BE48, 15, h_post); Log("hook registry Post ok"); } else Log("hook registry Post: prologue mismatch"); }
    {   // arena swap: config, table, MissionRequestById hook
        char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
        g_cfg_enable = GetPrivateProfileIntA("ArenaSwap", "Enable", 1, ini);
        PstLoadIni(ini);  // user #22389 [Practice]
        g_cfg_ab = GetPrivateProfileIntA("Debug", "AutoBind", 1, ini);
        g_cfg_am = GetPrivateProfileIntA("Debug", "AutoMoves", 1, ini);  // pc2 #23907
        g_cfg_fp = GetPrivateProfileIntA("Debug", "FrameProbe", 0, ini); if (g_cfg_fp || g_pst.v[PS_FRAME_METER] || g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW]) FpInstall(); else Log("FRAMEPROBE off ([Debug] FrameProbe 0, [Practice] FrameMeter 0, PlayerFrameMeter 0, KiWindow 0)");  // user #22356
        FmLoadIni(ini);  // Practice frame meter
        NkmLoadIni(ini);  // user #23400 negative Ki meter
        FzLoadIni(ini);  // user #22589 Practice freeze keys
        { HANDLE t = CreateThread(0, 0, FzKeyThread, 0, 0, 0); if (t) CloseHandle(t); }
        g_cfg_aware = GetPrivateProfileIntA("Summon", "AwareFlag", 1, ini); g_cfg_aware_ms = GetPrivateProfileIntA("Summon", "AwareDelayMs", 400, ini);
        { int sp = (int)GetPrivateProfileIntA("Summon", "SummonPassiveMs", 3000, ini); g_cfg_sum_passive = sp < 0 ? 0 : sp > SUM_PASSIVE_MAX ? SUM_PASSIVE_MAX : sp; }  // user #13164
        g_cfg_spawnverify = GetPrivateProfileIntA("Summon", "SpawnVerify", 1, ini); g_cfg_fn_summon = GetPrivateProfileIntA("Summon", "FightNowSummon", 1, ini); g_cfg_fn_arena = GetPrivateProfileIntA("Summon", "FightNowArena", 1, ini); g_cfg_tr_dist = GetPrivateProfileIntA("Summon", "Distance", 300, ini);
        g_cfg_tr_nodeath = GetPrivateProfileIntA("Summon", "PlayerNoDeath", 1, ini); g_cfg_session_nodeath = GetPrivateProfileIntA("Summon", "SessionNoDeath", 1, ini) != 0; g_cfg_cantdie = GetPrivateProfileIntA("Summon", "PlayerCantDie", 1, ini) != 0; g_cfg_beat_ms = GetPrivateProfileIntA("Summon", "BeatenRespawnMs", 0, ini); if (g_cfg_beat_ms < 0) g_cfg_beat_ms = 0; if (g_cfg_beat_ms > 5000) g_cfg_beat_ms = 5000; g_cfg_tr_win = GetPrivateProfileIntA("Summon", "RespawnOnWinMs", 0, ini); g_cfg_tr_respawn = GetPrivateProfileIntA("Summon", "RetryRespawnMs", 2000, ini); g_cfg_body_fade = GetPrivateProfileIntA("Summon", "BodyFade", 1, ini); g_cfg_body_delay = GetPrivateProfileIntA("Summon", "BodyFadeDelayMs", 25000, ini); g_cfg_kill_fade_frames = GetPrivateProfileIntA("Summon", "KillFadeFrames", 120, ini); g_cfg_kill_fade_delay = GetPrivateProfileIntA("Summon", "KillFadeDelayMs", 5000, ini);
        g_cfg_loot = GetPrivateProfileIntA("Summon", "BossLoot", 0, ini); g_cfg_deathblow = GetPrivateProfileIntA("Summon", "DeathBlowRequired", 1, ini); g_cfg_cutscene = GetPrivateProfileIntA("Summon", "CutsceneOnKill", 0, ini); g_cfg_fin_timeout = GetPrivateProfileIntA("Summon", "FinisherKillTimeoutMs", 6000, ini);
        Log("summon death (#6185): BossLoot %d, DeathBlowRequired %d, CutsceneOnKill %d, FinisherKillTimeoutMs %d, BodyFade %d, BodyFadeDelayMs %d, KillFadeFrames %d", g_cfg_loot, g_cfg_deathblow, g_cfg_cutscene, g_cfg_fin_timeout, g_cfg_body_fade, g_cfg_body_delay, g_cfg_kill_fade_frames);
        g_cfg_tr_nodeath_summon = GetPrivateProfileIntA("Summon", "SummonNoDeath", 0, ini);  // user #13197: default 0
        Log("summon (bridge 4942): FightNowSummon %d, Distance %d, PlayerNoDeath %d, SessionNoDeath %d (#14147), PlayerCantDie %d (#14150 infinite health), BeatenRespawnMs %d, RespawnOnWinMs %d", g_cfg_fn_summon, g_cfg_tr_dist, g_cfg_tr_nodeath, g_cfg_session_nodeath, g_cfg_cantdie, g_cfg_beat_ms, g_cfg_tr_win);
        g_cfg_preload = GetPrivateProfileIntA("ArenaSwap", "Preload", 0, ini);
        g_cfg_preload_max = GetPrivateProfileIntA("ArenaSwap", "PreloadMax", 3, ini);
        g_cfg_hook = GetPrivateProfileIntA("ArenaSwap", "HookFightNow", 0, ini);
        g_cfg_bossdist = GetPrivateProfileIntA("ArenaSwap", "BossDistance", 900, ini); g_cfg_anchor = GetPrivateProfileIntA("ArenaSwap", "BossAnchor", 1, ini);
        g_cfg_ambient = GetPrivateProfileIntA("ArenaSwap", "PreloadAmbient", 0, ini);
        g_cfg_xworld = GetPrivateProfileIntA("ArenaSwap", "CrossWorld", 1, ini); g_cfg_xabbm = GetPrivateProfileIntA("ArenaSwap", "CrossWorldAbortEndBossMode", 1, ini); g_cfg_xearly = GetPrivateProfileIntA("ArenaSwap", "CrossWorldEarlySwap", 1, ini); Log("CrossWorldEarlySwap %d", g_cfg_xearly); g_cfg_xsettle = GetPrivateProfileIntA("ArenaSwap", "CrossWorldSettleMs", 700, ini); g_cfg_xprewarm = GetPrivateProfileIntA("ArenaSwap", "CrossWorldPrewarm", 0, ini); g_cfg_xprewarm_mission = (uint32_t)GetPrivateProfileIntA("ArenaSwap", "CrossWorldPrewarmMission", 0, ini); g_cfg_xhold = GetPrivateProfileIntA("ArenaSwap", "CrossWorldPrewarmHoldMs", 30000, ini); Log("bridge 6375: CrossWorldSettleMs %d, CrossWorldPrewarm %d, PrewarmMission 0x%X", g_cfg_xsettle, g_cfg_xprewarm, g_cfg_xprewarm_mission); g_cfg_xpre = GetPrivateProfileIntA("ArenaSwap", "CrossWorldPreload", 1, ini); Log("cross-world Fight now (bridge 5276): CrossWorld %d, CrossWorldPreload %d", g_cfg_xworld, g_cfg_xpre);
        g_cfg_xaoskip = GetPrivateProfileIntA("ArenaSwap", "CrossWorldAreaObjSkipMs", 0, ini); Log("cross-world (bridge 6047c): CrossWorldAreaObjSkipMs %d", g_cfg_xaoskip);
        g_cfg_xforce = GetPrivateProfileIntA("ArenaSwap", "CrossWorldForceCells", 0, ini); Log("cross-world (#8236): CrossWorldForceCells %d", g_cfg_xforce);
        g_cfg_xaoold = GetPrivateProfileIntA("ArenaSwap", "CrossWorldOldAreaObjSkipMs", 0, ini);
        g_cfg_xpre_after = GetPrivateProfileIntA("ArenaSwap", "CrossWorldPreloadAfterSwitch", 0, ini); Log("cross-world (bridge 6047b): CrossWorldPreloadAfterSwitch %d", g_cfg_xpre_after);
        g_cfg_xcells = GetPrivateProfileIntA("ArenaSwap", "CrossWorldCellLoads", 0, ini); g_cfg_xblock = GetPrivateProfileIntA("ArenaSwap", "CrossWorldBlockCells", 0, ini); Log("cross-world streaming (bridge 6047): CrossWorldCellLoads %d (%s)", g_cfg_xcells, g_cfg_xcells > 4 ? "cell load cap raised during world switches" : "native cap 4, timeline log only");
        g_cfg_cellskip = GetPrivateProfileIntA("ArenaSwap", "CrossWorldCellSkip", 0, ini); g_cfg_xlgs = GetPrivateProfileIntA("ArenaSwap", "CrossWorldLoaderGateSkip", 0, ini); Log("cross-world (pc2 #6801): CrossWorldLoaderGateSkip %d", g_cfg_xlgs);
        g_cfg_pskip = GetPrivateProfileIntA("ArenaSwap", "CrossWorldPlaceableSkip", 0, ini); g_cfg_pskip_max = GetPrivateProfileIntA("ArenaSwap", "CrossWorldPlaceableSkipMaxMs", 20000, ini); Log("cross-world (pc2 placeable-skip): CrossWorldPlaceableSkip %d, MaxMs %d", g_cfg_pskip, g_cfg_pskip_max);
        g_cfg_far = GetPrivateProfileIntA("ArenaSwap", "NearArenaOnly", 0, ini); g_cfg_far_radius = GetPrivateProfileIntA("ArenaSwap", "NearArenaRadius", 15000, ini); g_cfg_far_max = GetPrivateProfileIntA("ArenaSwap", "NearArenaMaxMs", 40000, ini);
        if (g_cfg_far_radius < 0) g_cfg_far_radius = 0;
        Log("cross-world (pc #8552): NearArenaOnly %d, NearArenaRadius %d, NearArenaMaxMs %d", g_cfg_far, g_cfg_far_radius, g_cfg_far_max);
        if (g_cfg_pskip || g_cfg_far) PsHook();
        if (g_cfg_cellskip) {  // 3A0534 prologue: mov [rsp+8],rbx; mov [rsp+10h],rsi; mov [rsp+18h],rdi (15 bytes, no rip-relative)
            static const uint8_t csp[15] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x48, 0x89, 0x7C, 0x24, 0x18};
            if (!memcmp((void*)(B + Grva(0x3A0534)), csp, 15)) { o_cellreq = (CellReqFn)Hook(0x3A0534, 15, h_cellreq); Log("hook stage cell request 3A0534 ok: CrossWorldCellSkip %d (%s)", g_cfg_cellskip, g_cfg_cellskip >= 2 ? "skip non-arena cells during cross-world Fight now" : "log only"); }
            else Log("hook stage cell request 3A0534: prologue mismatch, CrossWorldCellSkip disabled");
        }
        GetPrivateProfileStringA("Practice", "SoulCoreMin", "", g_soul_min_str, sizeof g_soul_min_str, ini); g_cfg_bossmode = GetPrivateProfileIntA("ArenaSwap", "BossMode", 1,ini); g_cfg_bgm = GetPrivateProfileIntA("ArenaSwap", "BossBgm", 1, ini); g_cfg_native = GetPrivateProfileIntA("ArenaSwap", "NativeEnter", 1, ini); GetPrivateProfileStringA("ArenaSwap", "NativeMissions", "0x1CBC", g_native_list, sizeof g_native_list, ini);
        g_cfg_notify = GetPrivateProfileIntA("ArenaSwap", "BossModeNotify", 1, ini);
        g_cfg_ownwait = GetPrivateProfileIntA("ArenaSwap", "OwnerWaitMs", 0, ini); g_cfg_ownact = GetPrivateProfileIntA("ArenaSwap", "OwnerActivate", 0, ini);
        g_cfg_freshid = GetPrivateProfileIntA("ArenaSwap", "GaugeFreshId", 1, ini); g_cfg_gaugereset = GetPrivateProfileIntA("ArenaSwap", "GaugeStateReset", 1, ini); g_cfg_sidefix = GetPrivateProfileIntA("ArenaSwap", "SideFix", 1, ini); g_cfg_pool = GetPrivateProfileIntA("Summon", "PoolMax", 3, ini); g_cfg_awarefix = GetPrivateProfileIntA("ArenaSwap", "AwareFix", 0, ini); g_cfg_percfix = GetPrivateProfileIntA("Summon", "PerceptionFix", 0, ini); g_cfg_percdiag = GetPrivateProfileIntA("Summon", "PerceptionDiag", 1, ini); Log("perception (bridge 5201): PerceptionFix %d, PerceptionDiag %d", g_cfg_percfix, g_cfg_percdiag); g_cfg_teamfix = GetPrivateProfileIntA("ArenaSwap", "TeamFix", 1, ini); g_cfg_enemyteam = GetPrivateProfileIntA("ArenaSwap", "EnemyTeam", -1, ini); g_force_owner = (uint32_t)GetPrivateProfileIntA("ArenaSwap", "ForceOwner", 0, ini);
        Log("team fix (bridge 4878): TeamFix %d, EnemyTeam %d (-1 = from live enemies, else %d), ForceOwner %u", g_cfg_teamfix, g_cfg_enemyteam, DEFAULT_ENEMY_TEAM, g_force_owner);
        g_cfg_fade = GetPrivateProfileIntA("Fade", "Enable", 1, ini); g_cfg_fade_out = GetPrivateProfileIntA("Fade", "OutMs", 250, ini); g_cfg_fade_in = GetPrivateProfileIntA("Fade", "InMs", 400, ini);
        g_cfg_fade_min = GetPrivateProfileIntA("Fade", "MinHoldMs", 800, ini); g_cfg_fade_stable = GetPrivateProfileIntA("Fade", "StableMs", 500, ini); g_cfg_fade_max = GetPrivateProfileIntA("Fade", "MaxHoldMs", 60000, ini); g_cfg_fade_areawait = GetPrivateProfileIntA("Fade", "AreaWaitMs", 500, ini); g_cfg_fade_areawait_x = GetPrivateProfileIntA("Fade", "AreaWaitCrossMs", 15000, ini);
        g_cfg_fade_pwait = GetPrivateProfileIntA("Fade", "PlaceableWaitMs", -1, ini); Log("fade (pc2 #6801): PlaceableWaitMs %d", g_cfg_fade_pwait);
        g_cfg_coverrel = GetPrivateProfileIntA("ArenaSwap", "CoverReleaseMs", -1, ini); Log("cover release (pc2 #8619): CoverReleaseMs %d", g_cfg_coverrel); g_cfg_lsend = GetPrivateProfileIntA("ArenaSwap", "LoadingScreenEndMs", -1, ini); Log("loading screen end (#9182): LoadingScreenEndMs %d", g_cfg_lsend); Log("fade (bridge 6519): Enable %d, OutMs %d, InMs %d, MinHoldMs %d, StableMs %d, MaxHoldMs %d, AreaWaitMs %d, AreaWaitCrossMs %d", g_cfg_fade, g_cfg_fade_out, g_cfg_fade_in, g_cfg_fade_min, g_cfg_fade_stable, g_cfg_fade_max, g_cfg_fade_areawait, g_cfg_fade_areawait_x);
        if (g_cfg_fade) {
            HMODULE ef = GetModuleHandleA("Nioh3EngineFix.asi"); if (!ef) ef = GetModuleHandleA("Nioh3EngineFix.dll");
            p_osdFade = ef ? (int (*)(int, int))GetProcAddress(ef, "Nioh3EngineFix_Fade") : 0; p_osdAlpha = ef ? (int (*)(void))GetProcAddress(ef, "Nioh3EngineFix_FadeAlpha") : 0;
            p_osdLoad = ef ? (int (*)(int, const char*, const char*))GetProcAddress(ef, "Nioh3EngineFix_LoadScreen") : 0;
            p_osdMask = ef ? (int (*)(int, int))GetProcAddress(ef, "Nioh3EngineFix_MaskGameButtons") : 0;
            g_cfg_gate_mask_r = GetPrivateProfileIntA("Return", "ReturnGateMaskRadius", 400, ini); g_cfg_gate_mask_btn = GetPrivateProfileIntA("Return", "ReturnGateMaskButtons", 0xF000, ini);
            g_cfg_gate_mask_hold = GetPrivateProfileIntA("Return", "ReturnGateMaskHoldMs", 2000, ini); if (g_cfg_gate_mask_hold < 0) g_cfg_gate_mask_hold = 0; Log("RETURN gate (#9909): ReturnGateMaskHoldMs %d (mask kept after leaving the gate radius)", g_cfg_gate_mask_hold);
            Log("RETURN gate (#9324): native interact hidden within %d cm of the gate (buttons 0x%X), EngineFix export %s", g_cfg_gate_mask_r, g_cfg_gate_mask_btn, p_osdMask ? "found" : "MISSING: the native end-mission prompt can open");
            {   char gd[MAX_PATH]; snprintf(gd, sizeof gd, "%s", ini); char* sl = strrchr(gd, '\\'); if (sl) *sl = 0; char def[MAX_PATH + 32]; snprintf(def, sizeof def, "%s\\Nioh3FightRecorder.ps1", gd);
                g_cfg_rec = GetPrivateProfileIntA("Record", "Enable", 0, ini); GetPrivateProfileStringA("Record", "Script", def, g_rec_script, sizeof g_rec_script, ini); GetPrivateProfileStringA("Record", "OutRoot", gd, g_rec_out, sizeof g_rec_out, ini);
                g_rec_max = GetPrivateProfileIntA("Record", "MaxMinutes", 20, ini); Log("RECORD (#9291): Enable %d, Script %s, OutRoot %s, MaxMinutes %d", g_cfg_rec, g_rec_script, g_rec_out, g_rec_max); }
            g_cfg_rh_ls = GetPrivateProfileIntA("Return", "LoadScreen", 1, ini); Log("RETURN loading screen (#8902): [Return] LoadScreen %d, EngineFix export %s", g_cfg_rh_ls, p_osdLoad ? "found" : "missing (black only)");
            if (p_osdFade && p_osdAlpha) { g_fd_ok = 1; Log("FADE: using the Nioh3EngineFix overlay export (D3D)"); }
            else { p_osdFade = 0; p_osdAlpha = 0; Log("FADE: Nioh3EngineFix fade export not found, using the GDI overlay window"); CreateThread(0, 0, FadeThread, 0, 0, 0); }
        }
        g_imgSize = *(uint32_t*)(B + *(uint32_t*)(B + 0x3C) + 0x18 + 0x38);
        LoadTable();
        const uint8_t reqp[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70, 0x10, 0x48, 0x89, 0x78, 0x18, 0x4C, 0x89, 0x60, 0x20};
        if (!g_cfg_hook) Log("arena swap: Enable %d, Preload %d (max %d, ambient %d); API Nioh3Training_FightNow / _Preload for Boss Rematch; MissionRequestById hook off (HookFightNow = 0)", g_cfg_enable, g_cfg_preload, g_cfg_preload_max, g_cfg_ambient);
        else if (!memcmp((void*)(B + Grva(0x2239600)), reqp, sizeof reqp)) { o_req = (ReqFn)Hook(0x2239600, 19, h_req); Log("hook MissionRequestById ok; arena swap on Fight now %s, preload %s (max %d), image 0x%X", g_cfg_enable ? "ON" : "off", g_cfg_preload ? "on" : "off", g_cfg_preload_max, g_imgSize); }
        else Log("hook MissionRequestById: prologue mismatch (hooked by another add-on?): Fight now keeps the native load");
    }
    if (g_cfg_xaoskip > 0 || g_cfg_xforce) {  // bridge 6047c
        static const uint8_t aorp[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18};
        if (!memcmp((void*)(B + Grva(0x3B3A28)), aorp, sizeof aorp)) { o_aor = (AorFn)Hook(0x3B3A28, 15, h_aor); Log("hook StageAreasObjectsReady 3B3A28 ok (CrossWorldAreaObjSkipMs %d)", g_cfg_xaoskip); }
        else { g_cfg_xaoskip = 0; g_cfg_xforce = 0; Log("hook StageAreasObjectsReady 3B3A28: prologue mismatch, CrossWorldAreaObjSkipMs + CrossWorldForceCells off"); }
    }
    {   // user #6789 return gate + #6781 gold Ki bar
        char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
        g_cfg_gate = GetPrivateProfileIntA("Return", "ReturnGate", 1, ini); g_cfg_gate_fake = GetPrivateProfileIntA("Return", "ReturnGateFakeRecord", 1, ini);
        g_cfg_gate_dist = GetPrivateProfileIntA("Return", "ReturnGateDistance", 220, ini); g_cfg_gate_touch = GetPrivateProfileIntA("Return", "ReturnGateTouchMs", 1500, ini);
        g_cfg_gate_radius = GetPrivateProfileIntA("Return", "ReturnGateRadius", 110, ini); g_cfg_gate_ground = GetPrivateProfileIntA("Return", "ReturnGateGround", 1, ini); g_cfg_gate_always = GetPrivateProfileIntA("Return", "ReturnGateAlways", 1, ini);GetPrivateProfileStringA("Return", "ReturnGateIds", "10,11,13,12", g_gate_ids, sizeof g_gate_ids, ini);
        g_cfg_goldki = GetPrivateProfileIntA("Summon", "GoldKiBar", 1, ini);
        g_cfg_nofin = GetPrivateProfileIntA("Summon", "KiBreakNoFinisher", 0, ini); g_cfg_nofin_ms = GetPrivateProfileIntA("Summon", "KiBreakHp0WindowMs", 1000, ini); if (g_cfg_nofin_ms < 200) g_cfg_nofin_ms = 200;
        g_nf_kneelall = GetPrivateProfileIntA("Summon", "KiBreakKneelBlockAll", 1, ini); g_nf_hp0aihold = GetPrivateProfileIntA("Summon", "KiBreakHp0AiHold", 1, ini);
        g_cfg_nofin_fade = GetPrivateProfileIntA("Summon", "KiBreakHp0FadeDelayMs", 1000, ini); if (g_cfg_nofin_fade < 0) g_cfg_nofin_fade = 0;
        Log("KiBreakKneelBlockAll %d, KiBreakHp0AiHold %d (#9542); KiBreakHp0FadeDelayMs %d, ReturnGateGround %d (#9543), ReturnGateAlways %d (#13255)", g_nf_kneelall, g_nf_hp0aihold, g_cfg_nofin_fade, g_cfg_gate_ground, g_cfg_gate_always);
        g_nf_barefix = GetPrivateProfileIntA("Summon", "KiBreakBareFix", 1, ini); g_nf_bare_ms = GetPrivateProfileIntA("Summon", "KiBreakBareMs", 200, ini); if (g_nf_bare_ms < 0) g_nf_bare_ms = 0;
        Log("KiBreakBareFix %d, KiBreakBareMs %d (#9579: bare Ki break = crouch without stagger -> cancelled, Ki primed for a real break)", g_nf_barefix, g_nf_bare_ms);
        g_nf_actfix = GetPrivateProfileIntA("Summon", "KiBreakActFix", 1, ini); g_nf_actfix_mode = GetPrivateProfileIntA("Summon", "KiBreakActFixMode", 2, ini);
        g_nf_actfix_ms = GetPrivateProfileIntA("Summon", "KiBreakActFixMs", 100, ini); g_nf_actfix_max = GetPrivateProfileIntA("Summon", "KiBreakActFixMax", 3, ini);
        g_nf_actfix_win = GetPrivateProfileIntA("Summon", "KiBreakActFixWindowMs", 4000, ini); g_nf_actfix_learn = GetPrivateProfileIntA("Summon", "KiBreakActLearnMs", 400, ini);
        if (g_nf_actfix_ms < 0) g_nf_actfix_ms = 0; if (g_nf_actfix_learn < 0) g_nf_actfix_learn = 0;
        g_nf_actfix_gap = GetPrivateProfileIntA("Summon", "KiBreakActFixGapMs", 500, ini); if (g_nf_actfix_gap < 0) g_nf_actfix_gap = 0;
        {   char sb[256], sg[32]; GetPrivateProfileStringA("Summon", "KiBreakActBadIds", "", sb, sizeof sb, ini);
            GetPrivateProfileStringA("Summon", "KiBreakActGoodId", "0", sg, sizeof sg, ini); g_nf_actgood = (uint32_t)strtoul(sg, 0, 0);
            g_nf_nactbad = 0; for (const char* q = sb; *q && g_nf_nactbad < 16; ) { char* e; unsigned long v = strtoul(q, &e, 0); if (e == q) { ++q; continue; } if (v) g_nf_actbad[g_nf_nactbad++] = (uint32_t)v; q = e; }
            char lb[200] = ""; for (int i = 0; i < g_nf_nactbad; ++i) { size_t l = strlen(lb); snprintf(lb + l, sizeof lb - l, "%s0x%X", i ? "," : "", g_nf_actbad[i]); }
            Log("KiBreakActFix %d mode %d (2 forced 66C4A8, 1 pending +2E4): bad ids {%s} -> good 0x%X after %d ms, max %d per break, gap %d ms, window %d ms (#9937, defaults seen on Yoritomo R7B35)",
                g_nf_actfix, g_nf_actfix_mode, lb, g_nf_actgood, g_nf_actfix_ms, g_nf_actfix_max, g_nf_actfix_gap, g_nf_actfix_win); }
        g_nf_autostag = GetPrivateProfileIntA("Summon", "KiBreakAutoStagger", 1, ini); g_nf_autohold = GetPrivateProfileIntA("Summon", "KiBreakAutoHoldMs", 2500, ini); if (g_nf_autohold < 0) g_nf_autohold = 0;
        Log("KiBreakAutoStagger %d, KiBreakAutoHoldMs %d (#23432: bare / no-action Ki break with no learned real stagger -> the boss's learned hit-reaction action)", g_nf_autostag, g_nf_autohold);
        g_nf_kneel = GetPrivateProfileIntA("Summon", "KiBreakHp0Kneel", 0, ini); g_nf_clear101 = GetPrivateProfileIntA("Summon", "KiBreakHp0ClearDeathBlow", 1, ini); g_nf_kirow_unbreak = GetPrivateProfileIntA("Summon", "KiBreakGoldUnbreak", 1, ini);
        Log("KiBreakHp0ClearDeathBlow %d, KiBreakGoldUnbreak %d, KillFadeDelayMs %d (#8827)", g_nf_clear101, g_nf_kirow_unbreak, g_cfg_kill_fade_delay);
        g_cfg_stag_id = GetPrivateProfileIntA("Summon", "KiBreakHp0StaggerId", 0, ini); Log("KiBreakHp0StaggerId %d (0 = learned from the boss's own Ki break, #9297)", g_cfg_stag_id);
        g_nf_st27 = GetPrivateProfileIntA("Summon", "KiBreakBlockDeathBlow", 1, ini); Log("KiBreakBlockDeathBlow %d (#9135: no death-blow offer + no plain grab box on a gold boss)", g_nf_st27);
        g_nf_stagger = GetPrivateProfileIntA("Summon", "KiBreakHp0Stagger", 1, ini); g_nf_nonlethal = GetPrivateProfileIntA("Summon", "KiBreakHp0NonLethal", 1, ini); g_nf_prime = GetPrivateProfileIntA("Summon", "KiBreakHp0Prime", 1, ini); g_nf_reserve = GetPrivateProfileIntA("Summon", "KiBreakHp0Reserve", 1, ini); Log("KiBreakHp0Prime %d, KiBreakHp0Reserve %d", g_nf_prime, g_nf_reserve); Log("KiBreakHp0NonLethal %d (#9319: HP-floor hits stay normal hits = native hit reaction + Ki-break stagger)", g_nf_nonlethal); g_cfg_nf_fade = GetPrivateProfileIntA("Summon", "KiBreakGoldFadeMs", 350, ini); if (g_cfg_nf_fade < 0) g_cfg_nf_fade = 0;
        g_cfg_bossgrace = GetPrivateProfileIntA("ArenaSwap", "BossGraceAfterCoverMs", 1500, ini); g_cfg_bosshold_max = GetPrivateProfileIntA("ArenaSwap", "BossHoldMaxMs", 60000, ini);
        Log("KiBreakHp0Stagger %d, KiBreakGoldFadeMs %d, BossGraceAfterCoverMs %d, BossHoldMaxMs %d (#9133)", g_nf_stagger, g_cfg_nf_fade, g_cfg_bossgrace, g_cfg_bosshold_max);
        Log("KiBreakNoFinisher %d, KiBreakHp0WindowMs %d, KiBreakHp0Kneel %d (#7465: no grapple finisher on a boss, gold Ki bar while Ki-broken, HP-0 window; Ki strips %d)", g_cfg_nofin, g_cfg_nofin_ms, g_nf_kneel, g_nf_nstrip);
        { char b[512]; GetPrivateProfileStringA("Summon", "MoveBlock", "", b, sizeof b, ini); uint32_t ids[64]; int k = 0; for (char* t = strtok(b, ", "); t && k < 64; t = strtok(0, ", ")) ids[k++] = (uint32_t)strtoul(t, 0, 0); g_mv_fallback = (uint32_t)GetPrivateProfileIntA("Summon", "MoveBlockFallback", 2, ini); if (k) MvSet(ids, k); }
        g_cfg_handmoves = GetPrivateProfileIntA("Debug", "HandMoves", 1, ini); N3CatInit(g_cfg_handmoves);  // pc2 #23907, before the per-move prefs
        Log("MOVES catalogue: %d hand rows (HandMoves %d), runtime game data %s", N3_MOVE_COUNT, g_cfg_handmoves, g_cfg_am ? "on (AutoMoves 1)" : "OFF (AutoMoves 0)");
        MvLoadPrefs(ini);
        g_cfg_aggro = GetPrivateProfileIntA("Training", "BossAggro", 0, ini) != 0; g_cfg_aggro_range = GetPrivateProfileIntA("Training", "AggroRangeCm", 500, ini); if (g_cfg_aggro_range < 100) g_cfg_aggro_range = 100;
        g_cfg_aggro_face = GetPrivateProfileIntA("Training", "AggroFace", 1, ini) != 0; g_cfg_aggro_walk_ms = GetPrivateProfileIntA("Training", "AggroWalkTimeoutMs", 6000, ini);  // user #14147 / #14267
        Log("BossAggro %d, AggroRangeCm %d, AggroFace %d (user #13787 / #14147: Training bosses keep the player as target and close in before the repeat, Stand still too)", g_cfg_aggro, g_cfg_aggro_range, g_cfg_aggro_face);
        g_cfg_actlog = GetPrivateProfileIntA("Summon", "ActionLog", 1, ini); Log("ActionLog %d (#9508 boss current-action id log + Nioh3Training_actions.tsv)", g_cfg_actlog);
        g_cfg_kibreak_log = GetPrivateProfileIntA("Summon", "KiBreakLog", 1, ini); Log("KiBreakLog %d (#7290 passive Ki-break / grapple finisher log, read only)", g_cfg_kibreak_log);
        { int ini_ar = GetPrivateProfileIntA("Summon", "AutoRetryOnDeath", GetPrivateProfileIntA("Summon", "InstantRespawn", 1, ini), ini); g_cfg_autoretry = 1; Log("AutoRetryOnDeath 1 (forced, user #13229: player beaten = instant dejavu reset; ini value %d ignored)", ini_ar); }
        Log("return gate (#6789): ReturnGate %d, FakeRecord %d, Distance %d, TouchMs %d, Radius %d, Ids %s; GoldKiBar %d", g_cfg_gate, g_cfg_gate_fake, g_cfg_gate_dist, g_cfg_gate_touch, g_cfg_gate_radius, g_gate_ids, g_cfg_goldki);
        g_cfg_gate_opena = GetPrivateProfileIntA("Return", "ReturnGateOpenOnA", 1, ini); g_cfg_gate_zone = GetPrivateProfileIntA("Return", "ReturnGateZone", 1, ini);
        g_cfg_gate_open_btn = GetPrivateProfileIntA("Return", "ReturnGateOpenButtons", 0x1000, ini); g_cfg_gate_tap_ms = GetPrivateProfileIntA("Return", "ReturnGateTapMs", 350, ini);
        Log("RETURN gate zone (#13796): ReturnGateZone %d (one zone for every mode, radius ReturnGateMaskRadius %d u), open buttons 0x%X on a tap <= %d ms, hold = cancel; native modal safety net %s",
            g_cfg_gate_zone, g_cfg_gate_mask_r, g_cfg_gate_open_btn, g_cfg_gate_tap_ms, GateEf("Nioh3EngineFix_NativeModal") ? "on (EngineFix)" : "dialog hooks only (EngineFix export missing)"); g_cfg_bar_vis = GetPrivateProfileIntA("Summon", "BossBarAfterVisibleMs", 600, ini);
        Log("ReturnGateOpenOnA %d (#9664/#9670: pad A near the gate opens our gate page), BossBarAfterVisibleMs %d (#9685), FinisherKillTimeoutMs %d / KiBreakHp0WindowMs %d (Boss Rematch sliders, #9585)", g_cfg_gate_opena, g_cfg_bar_vis, g_cfg_fin_timeout, g_cfg_nofin_ms);
        g_dbg_on = GetPrivateProfileIntA("Debug", "Hotkeys", 1, ini); g_dbg_freeze_vk = GetPrivateProfileIntA("Debug", "FreezeKey", 0x78, ini); g_dbg_bug_vk = GetPrivateProfileIntA("Debug", "BugKey", 0x77, ini);
        {   char dd[MAX_PATH]; snprintf(dd, sizeof dd, "%s\\marks", g_rec_out[0] ? g_rec_out : g_dir); GetPrivateProfileStringA("Debug", "ShotDir", dd, g_dbg_dir, sizeof g_dbg_dir, ini); }
        if (g_dbg_on) { HANDLE t = CreateThread(0, 0, DbgThread, 0, 0, 0); if (t) CloseHandle(t); } else Log("DBG hotkeys off ([Debug] Hotkeys 0)");
        g_cfg_menupause = GetPrivateProfileIntA("Training", "MenuPause", 1, ini);  // user #13274
        { HANDLE t = CreateThread(0, 0, MenuPauseThread, 0, 0, 0); if (t) CloseHandle(t); }
        if (!g_cfg_menupause) Log("MENU PAUSE off ([Training] MenuPause 0); boss-presence sampling remains available to EngineFix");
        static const uint8_t rrp[15] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x68, 0x10, 0x48, 0x89, 0x70, 0x18};
        if (g_cfg_gate) { if (!memcmp((void*)(B + Grva(0x22396D8)), rrp, 15)) { o_reqret = (ReqRetFn)Hook(0x22396D8, 15, h_reqret); Log("hook MissionRequestReturn 22396D8 ok (return gate)"); } else Log("hook MissionRequestReturn: prologue mismatch, gate works by touch only"); }
        g_cfg_gate_block = GetPrivateProfileIntA("Return", "ReturnGateBlockNativePrompt", 1, ini);
        {   static const uint8_t ecp[15] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20};  // 12A1F80 v2.02
            if (g_cfg_gate && g_cfg_gate_block) { if (!memcmp((void*)(B + Grva(0x12A1F80)), ecp, 15)) { o_endconf = (EndConfFn)Hook(0x12A1F80, 15, h_endconf); Log("hook EndMissionConfirmOpen 12A1F80 ok (#9909: the native 'This will end the mission' box never opens while our Return gate is up)"); }
                else Log("hook EndMissionConfirmOpen 12A1F80: prologue mismatch, native end-mission prompt NOT blocked (input mask only)");
                // #10020: pause menu decide handlers, same 14-byte prologue (mov [rsp+10],rbx; push rbp; lea rbp,[rsp-120]), no rip-relative bytes
                static const uint8_t pmp[14] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x48, 0x8D, 0xAC, 0x24, 0xE0, 0xFE, 0xFF, 0xFF};
                if (!memcmp((void*)(B + Grva(0x1DAE4E0)), pmp, 14)) { o_pmdec1 = (PmDecideFn)Hook(0x1DAE4E0, 14, h_pmdec1); Log("hook PauseMenuDecide 1DAE4E0 ok (#10020: pause-menu 'end mission' near our gate opens our gate page)"); }
                else Log("hook PauseMenuDecide 1DAE4E0: prologue mismatch, not hooked");
                if (!memcmp((void*)(B + Grva(0x1DB58B0)), pmp, 14)) { o_pmdec2 = (PmDecideFn)Hook(0x1DB58B0, 14, h_pmdec2); Log("hook PauseMenuDecide 1DB58B0 ok (#10020)"); }
                else Log("hook PauseMenuDecide 1DB58B0: prologue mismatch, not hooked");
                // #10022: DialogOpen 348274, 16-byte prologue (push rbp/rbx/rsi/rdi/r12/r14/r15; lea rbp,[rsp-70h]), no rip-relative bytes
                g_cfg_gate_dlg = GetPrivateProfileIntA("Return", "ReturnGateBlockDialog", 1, ini);
                static const uint8_t dop[16] = {0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0x90};
                if (!g_cfg_gate_dlg) Log("ReturnGateBlockDialog 0: DialogOpen 348274 not hooked");
                else if (!memcmp((void*)(B + Grva(0x348274)), dop, 16)) { o_dlgopen = (DlgOpenFn)Hook(0x348274, 16, h_dlgopen); Log("hook DialogOpen 348274 ok (#10022: any Yes/No box with the end-mission text is dropped near our gate; every box while the gate is up is logged with its stack)"); }
                else Log("hook DialogOpen 348274: prologue mismatch, not hooked");
                // gadget-script native UI::OpenCommonWindowTwoButton 23F8CC0: same 15-byte prologue as 12A1F80 (mov rax,rsp; 3 saves), no rip-relative bytes
                g_cfg_gate_cw2 = GetPrivateProfileIntA("Return", "ReturnGateHookScriptBox", 1, ini);
                if (!g_cfg_gate_cw2) Log("ReturnGateHookScriptBox 0: OpenCommonWindowTwoButton 23F8CC0 not hooked");
                else if (!memcmp((void*)(B + Grva(0x23F8CC0)), ecp, 15)) { o_cw2 = (Cw2Fn)Hook(0x23F8CC0, 15, h_cw2); Log("hook OpenCommonWindowTwoButton 23F8CC0 ok (clear gadget script's 'This will end the mission' box -> our gate page)"); }
                else Log("hook OpenCommonWindowTwoButton 23F8CC0: prologue mismatch, not hooked"); }
            else Log("ReturnGateBlockNativePrompt %d: native end-mission prompt not blocked", g_cfg_gate_block); }
        g_cfg_goldki_dump = GetPrivateProfileIntA("Summon", "GoldKiDump", 0, ini); g_cfg_goldki_learn = GetPrivateProfileIntA("Summon", "GoldKiLearn", 1, ini);
        { char b[160]; GetPrivateProfileStringA("Summon", "GoldKiColors", "", b, sizeof b, ini); for (char* s = strtok(b, ", "); s && g_goldki_ncols < 8; s = strtok(0, ", ")) g_goldki_cols[g_goldki_ncols++] = (uint32_t)strtoul(s, 0, 16);
          GetPrivateProfileStringA("Summon", "GoldKiRGB", "255,200,50", b, sizeof b, ini); int r = 255, g = 200, bl = 50; if (sscanf(b, "%d,%d,%d", &r, &g, &bl) == 3) { g_gold_r = (uint8_t)r; g_gold_g = (uint8_t)g; g_gold_b = (uint8_t)bl; } }
        if (g_cfg_goldki || g_cfg_nofin) { HookGoldKi(); HookGoldQuad(); g_gold_hooked = 1; }
        if (g_cfg_nofin) NfInstallHooks();
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
        GrInit(B, Log);
        GetModuleFileNameA(0, g_dir, sizeof g_dir);
        char* s = strrchr(g_dir, '\\'); if (s) *s = 0;
        Log("Nioh3Training probe loaded, base %p", (void*)B);
        CreateThread(0, 0, Worker, 0, 0, 0);
    }
    return TRUE;
}
