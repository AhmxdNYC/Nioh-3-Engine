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
static void WatchTick(void) {  // after CreateDynamic: collect actors that were not in the pre-spawn snapshot
    if (!g_watch) return;
    uint64_t cur[1024]; int n = Actors(cur, 1024);
    for (int i = 0; i < n; ++i) if (!InSnap(cur[i]) && !InSp(cur[i]) && *(uint32_t*)(cur[i] + 4) == g_watch_id && g_nsp < MAXSP) {
        float xyz[4] = {0}; memcpy(xyz, (void*)(cur[i] + 0x110), 16);
        g_sp_actor[g_nsp++] = cur[i];
        Log("SPAWN actor #%d appeared in table: %p handle %08X %08X pos %.0f %.0f %.0f +ab84=%08X (+%lu ms)", g_nsp - 1, (void*)cur[i], *(uint32_t*)cur[i], *(uint32_t*)(cur[i] + 4), xyz[0], xyz[1], xyz[2], *(uint32_t*)(cur[i] + 0xAB84), GetTickCount() - g_watch_t0);
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
static uint32_t PickOwner(void) {  // a registered placement with a live actor (not the player): enemy records first, then any live registered actor; 1 = the player (the boss is then friendly/passive)
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
// ---- boss HP bar for a CreateDynamic boss (bridge 4768). The gauge natives look the boss up by PLACEMENT id: AddBossGaugeRow 1CE7C6C -> FindWrapper 135B78 (sorted registry [c0+0x48], row = {id, wrapper*}) -> wrapper[0] = live actor.
// A dynamic boss has no registry row (its handle 0x1000xxxx is not an id there), so the gauge finds nothing or the owner placement's own actor. We hook FindWrapper: a private id FAKE_GAUGE_ID returns a copy of the owner's wrapper whose live-actor field points at the spawned boss.
#define FAKE_GAUGE_ID 0x7FFF0001u
typedef uint64_t (*FwFn)(uint64_t, uint32_t);
static FwFn o_fw; static uint64_t g_fwbuf[64]; static volatile uint64_t g_fakew;
static uint64_t h_fw(uint64_t cm, uint32_t id) { if (id == FAKE_GAUGE_ID && g_fakew) return g_fakew; return o_fw(cm, id); }
static uint64_t ActorByHandle(uint32_t h);
static struct { int pending, shown; uint32_t handle, owner; DWORD t0, tHide; } g_gg;
// bridge 4801: the native HideBossGauge (queued event) leaves the empty frame on screen. Do what BossGaugeSetup does for unused rows by hand: clear the row vector (hud+0x3C0, 1CEE8C8),
// hide the 3 row elements (type 0xF5) and the 42 detail elements (type 0xF6) with 5D1D8C(elem,0), then hide the HUD (1CEC2E0).
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
        ((void (*)(uint64_t, uint8_t))(B + 0x1CEC2E0))(hud, 0);
        Log("GAUGE force hide done (%s), hud %p", why, (void*)hud);
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("GAUGE force hide exception %08lX (%s)", GetExceptionCode(), why); }
}
static DWORD g_forceHideAt;
static void FakeGaugeDrop(void) { g_fwbuf[0] = 0; g_fakew = 0; g_gg.tHide = 0; }
static void FakeGaugeHide(const char* why) {  // the native hide is a queued message that looks the id up again later: keep the fake wrapper valid for 3 s (GaugeTick drops it)
    g_gg.pending = 0; if (!g_gg.shown) return;
    ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x1C208BC))(FAKE_GAUGE_ID, 0, 0);
    g_gg.shown = 0; g_gg.tHide = GetTickCount() | 1; Log("GAUGE HideBossGauge(fake id) (%s)", why);
    GaugeForceHide(why); g_forceHideAt = GetTickCount() + 700; if (!g_forceHideAt) g_forceHideAt = 1;  // again after the queued native hide ran
}
static void FakeGaugeRequest(uint32_t handle, uint32_t owner) {  // the actor enters the table a few ms after CreateDynamic: GaugeTick finishes the job
    if (!o_fw) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x1D115D4))(handle, 0, 0); Log("GAUGE FindWrapper hook missing: legacy ShowBossGauge(0x%X)", handle); return; }
    FakeGaugeHide("new request"); FakeGaugeDrop(); g_gg.pending = 1; g_gg.handle = handle; g_gg.owner = owner; g_gg.t0 = GetTickCount();
}
static void GaugeTick(void) {
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
    ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x1D115D4))(FAKE_GAUGE_ID, 0, 0);
    g_gg.pending = 0; g_gg.shown = 1;
    Log("GAUGE ShowBossGauge(fake id 0x%X) for actor %p (handle 0x%X, wrapper template owner %u -> %p)", FAKE_GAUGE_ID, (void*)a, g_gg.handle, g_gg.owner, (void*)tmpl);
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
    uint32_t owner = g_sp.owner ? g_sp.owner : PickOwner();
    if (owner <= 1 && g_sp.boss && g_cfg_ownwait > 0 && !g_sp.tOwn) g_sp.tOwn = GetTickCount();
    if (owner <= 1 && g_sp.boss && g_sp.tOwn && GetTickCount() - g_sp.tOwn < (DWORD)g_cfg_ownwait) return;  // summon: keep retrying the owner pick
    if (owner <= 1) { Log("SPAWN WARNING no live registered actor to own the boss: owner = player"); DumpOwnerState("spawn-no-owner"); g_dump_pending = 1; g_dump_owner = owner; }
    float face = cy + 180.0f;
    g_nsnap = Actors(g_snap, 1024);
    uint32_t r = ((DynFn)(B + 0x1C0C5E0))(g_sp.id, p, face, 0, owner, 0);
    g_watch = r != 0; g_watch_id = r; g_watch_t0 = GetTickCount();
    Log("SPAWN %u: resources ready after %lu ms; camyaw %.1f fwd %.2f %.2f; CreateDynamic(owner %u) player %.0f %.0f %.0f -> at %.0f %.0f %.0f yaw %.1f -> 0x%X%s", g_sp.id, dt, cy, dx, dz, owner, pos[0], pos[1], pos[2], p[0], p[1], p[2], face, r, r ? "" : " (refused)");
    if (r && g_autogauge) { FakeGaugeRequest(r, owner); Log("SPAWN %u: boss gauge requested for 0x%X", g_sp.id, r); }
    if (r && g_sp.boss) SummonFinish(r, g_sp.bgm);
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
    ((void (*)(uint64_t))(B + 0x3A083C))(s + 0x500);
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
static void BossTick(void) {
    if (!g_bm.active) return;
    DWORD now = GetTickCount(); if (now - g_bm.last < 250) return; g_bm.last = now;
    int v = BossAliveSensor(g_bm.id);
    if (g_bm.v0 < 0) { if (v >= 0) { g_bm.v0 = v; Log("BBM +%lu ms actor in table, death sensor baseline %d", now - g_bm.t0, v); } else if (now - g_bm.t0 > 8000) { Log("BBM actor never appeared, giving up"); g_bm.active = 0; } return; }
    if (v != g_bm.v0) { if (!g_bm.since) { g_bm.since = now; Log("BBM +%lu ms alive sensor %d -> %d", now - g_bm.t0, g_bm.v0, v); } }
    else g_bm.since = 0;
    if (g_bm.since && now - g_bm.since >= 1500) {
        if (v < 0) {  // bridge 4739: the actor left the table (player died / respawn reset the arena), the boss was not killed: no victory end state, just leave boss mode
            Log("BBM +%lu ms boss %u actor gone, not dead (player died or area reset): leaving boss mode without the victory sting", now - g_bm.t0, g_bm.id);
            BgmStop("boss gone");
            BossModeEnd("boss gone");
            FakeGaugeHide("boss mode end");
            g_bm.active = 0;
            return;
        }
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
                if (owner <= 1) { Log("ARENA WARNING no live registered actor to own the boss: owner = player, the boss will be passive"); DumpOwnerState("arena-no-owner"); g_dump_pending = 1; g_dump_owner = owner; }
                g_nsnap = Actors(g_snap, 1024);
                uint32_t r = ((DynFn)(B + 0x1C0C5E0))(g_ar.chr, b, g_ar.byaw, 0, owner, 0);
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

static int g_cfg_enable = 1, g_cfg_preload = 1, g_cfg_preload_max = 3, g_cfg_hook = 0, g_cfg_ambient = 0, g_cfg_bossdist = 900;
static int StartArena(uint32_t chr, const float* p, const float* b, float byaw) {
    BgmStop("new swap");
    for (int i = 0; i < g_nsp; ++i) if (g_sp_actor[i]) Despawn(i);  // one swapped-in boss at a time
    memset(&g_ar, 0, sizeof g_ar);
    g_ar.chr = chr; memcpy(g_ar.p, p, 3 * sizeof(float)); g_ar.yaw = p[3]; memcpy(g_ar.b, b, 3 * sizeof(float)); g_ar.byaw = byaw;
    g_ar.t0 = GetTickCount(); g_ar.lastState = -9; g_ar.lastCell = -9;
    StageLog("ARENA before");
    uint64_t e = ResEntry(chr);
    if (!e || !*(uint8_t*)(e + 0x2c8)) ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))(chr, 2, 0);
    int r = Warp(g_ar.p[0], g_ar.p[1], g_ar.p[2], g_ar.yaw, 1);
    Log("ARENA start chr %u player -> %.0f %.0f %.0f yaw %.1f boss -> %.0f %.0f %.0f: warp %d (+%lu ms), resources %s", chr, g_ar.p[0], g_ar.p[1], g_ar.p[2], g_ar.yaw, g_ar.b[0], g_ar.b[1], g_ar.b[2], r, GetTickCount() - g_ar.t0,
        e && *(uint8_t*)(e + 0x2c8) ? "preloaded" : "loading");
    g_ar.active = r == 1;
    return r == 1;
}

// ---- arena table (Nioh3Training_arenas.tsv, tools/gen_arena_table.py): mission, start, world, char, placement, player x y z yaw, boss x y z, kind, id, name
typedef struct { uint32_t mission, start, chr, placement, bgm; uint8_t world; float p[4], b[3]; char kind[8], id[16], name[64]; } ArenaRow;
static ArenaRow g_rows[160]; static int g_nrows;
static void LoadTable(void) {
    char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\Nioh3Training_arenas.tsv", g_dir);
    FILE* f = fopen(path, "r"); if (!f) { Log("arena table %s not found: Fight now keeps the native load", path); return; }
    char line[512];
    while (fgets(line, sizeof line, f) && g_nrows < 160) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char* col[16]; int n = 0; char* c = line;
        while (n < 15) { col[n++] = c; char* t = strchr(c, '\t'); if (!t) break; *t = 0; c = t + 1; }
        if (n < 15) continue;
        char* nl = strpbrk(col[14], "\r\n"); if (nl) *nl = 0;
        ArenaRow* r = &g_rows[g_nrows];
        r->mission = (uint32_t)strtoul(col[0], 0, 0); r->start = (uint32_t)strtoul(col[1], 0, 0); r->world = (uint8_t)strtoul(col[2], 0, 0);
        r->chr = (uint32_t)strtoul(col[3], 0, 0); r->placement = (uint32_t)strtoul(col[4], 0, 0);
        for (int i = 0; i < 4; ++i) r->p[i] = (float)atof(col[5 + i]);
        for (int i = 0; i < 3; ++i) r->b[i] = (float)atof(col[9 + i]);
        snprintf(r->kind, sizeof r->kind, "%s", col[12]); snprintf(r->id, sizeof r->id, "%s", col[13]); snprintf(r->name, sizeof r->name, "%s", col[14]);
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

// ---- Fight now without a loading screen: MissionRequestById 2239600 called by an add-on (BossRematch) for a catalogued boss in the world we stand in
typedef uint64_t (*ReqFn)(uint32_t, uint8_t, uint32_t, void*);
__declspec(dllexport) int Nioh3Training_FightNow(uint32_t mission);
static ReqFn o_req;
static uint32_t g_imgSize;
static const char* SwapGate(const ArenaRow* r) {
    const uint8_t* st = GameState();
    if (!st) return "game state unreadable";
    if (st[2]) return "a load is running";
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
        ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))(r->chr, 2, 0);
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
N3T_API int Nioh3Training_FightNow(uint32_t mission) {
    ArenaRow* r = g_cfg_enable ? FindRow(mission) : 0;
    if (!r) return 0;
    if (g_cfg_native && NativeListed(mission)) {
        memset(&g_en, 0, sizeof g_en); g_en.row = r; g_en.t0 = GetTickCount(); g_en.active = 1;
        Log("FIGHTNOW %s (%s) mission 0x%X: native load + auto-enter (NativeEnter=1, listed)", r->id, r->name, mission);
        return 0;
    }
    const char* why = SwapGate(r);
    if (why) { Log("FIGHTNOW %s mission 0x%X: native load (%s)", r->id, mission, why); return 0; }
    Log("FIGHTNOW %s (%s) mission 0x%X: arena swap in place (no mission load), world 0x%02X", r->id, r->name, mission, r->world);
    float b[3] = {r->b[0], r->b[1], r->b[2]};
    float dx = b[0] - r->p[0], dz = b[2] - r->p[2], d = sqrtf(dx * dx + dz * dz);
    if (g_cfg_bossdist > 0 && d > g_cfg_bossdist) {  // an idle boss only notices the player within ~10 m: pull it in along the same line
        float k = g_cfg_bossdist / d;
        b[0] = r->p[0] + dx * k; b[1] = r->p[1] + (b[1] - r->p[1]) * k; b[2] = r->p[2] + dz * k;
    }
    g_ar_bgm = r->bgm;
    return StartArena(r->chr, r->p, b, r->p[3] + 180.0f);
}
N3T_API void Nioh3Training_Preload(void) { if (g_cfg_enable && g_cfg_preload) PreloadNearby("PRELOAD (menu opened)"); }
N3T_API int Nioh3Training_Version(void) { return 4506; }
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
        if (!strcmp(op, "pos")) {
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
            if (!e || !*(uint8_t*)(e + 0x2c8)) ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))(g_sp.id, 2, 0);
            Log("CMD spawn %u dist %.0f: %s", g_sp.id, g_sp.dist, e && *(uint8_t*)(e + 0x2c8) ? "resources already loaded" : "Load::Character requested");
            g_sp.active = 1; SpawnTick();
        } else if (!strcmp(op, "summon")) {  // summon <R-id|charId> [dist]: boss next to the player in the current world, no arena swap / load; HUD gauge, boss mode, boss BGM
            if (g_sp.active) { Log("CMD summon: previous spawn %u still loading", g_sp.id); return; }
            char tok[32] = {0}; float d = 400.0f; sscanf(line, "%*s %31s %f", tok, &d);
            uint32_t chr = 0, bgm = 0;
            for (int i = 0; i < g_nrows; ++i) if (!_stricmp(g_rows[i].id, tok)) { chr = g_rows[i].chr; bgm = g_rows[i].bgm; break; }
            if (!chr) chr = (uint32_t)strtoul(tok, 0, 0);
            if (!chr) { Log("CMD summon: unknown boss '%s'", tok); return; }
            if (g_bm.active) BossModeEnd("new summon");
            BgmStop("new summon");
            g_sp.id = chr; g_sp.dist = d; g_sp.owner = 0; g_sp.boss = 1; g_sp.bgm = bgm; g_sp.tOwn = 0; g_sp.t0 = GetTickCount();
            uint64_t e = ResEntry(chr);
            if (!e || !*(uint8_t*)(e + 0x2c8)) ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))(chr, 2, 0);
            Log("CMD summon %s -> char %u dist %.0f: %s", tok, chr, d, e && *(uint8_t*)(e + 0x2c8) ? "resources already loaded" : "Load::Character requested");
            g_sp.active = 1; SpawnTick();
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
            ((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(B + 0x786C84))(comp, 0, 0, 0xFFFFFFFFull, drop ? 1 : 0, 1, 0, 0, 1);
            Log("CMD kill #%d: 786C84 called", idx);
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
            ((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(B + 0x786C84))(comp, 0, 0, 0xFFFFFFFFull, drop ? 1 : 0, 1, 0, 0, 1);
            Log("CMD killid: EnemyForceKill called");
        } else if (!strcmp(op, "fnow")) {  // Fight now through the hooked MissionRequestById (as an add-on caller): swaps in place when the row is eligible, else the native load
            Log("CMD fnow mission 0x%llX", a[0]);
            int r = Nioh3Training_FightNow((uint32_t)a[0]);
            if (!r && GameState() && GameState()[2]) Log("CMD fnow: a load is running, nothing requested");
            else if (!r) { __attribute__((aligned(16))) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0}; Log("CMD fnow: not handled, native request"); ((ReqFn)(o_req ? (void*)o_req : (void*)(B + 0x2239600)))((uint32_t)a[0], 1, (uint32_t)a[1], p); }
        } else if (!strcmp(op, "preload")) { PreloadNearby("CMD preload");
        } else if (!strcmp(op, "swap")) { g_cfg_enable = (int)a[0]; Log("CMD swap %d", g_cfg_enable);
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
            ((void (*)(uint8_t, uint8_t, uint32_t, uint32_t, void*))(B + 0x22399B8))((uint8_t)a[0], journey, (uint32_t)a[1], mode, prm);
            Log("CMD oworld world 0x%llX start %llu white %d journey %u mode %u requested", a[0], a[1], n >= 4 && a[2] ? 1 : 0, journey, mode);
            g_ms.t0 = GetTickCount(); g_ms.last = g_ms.t0; g_ms.ticks0 = g_ticks; g_ms.hadPlayer = PlayerChar() != 0; g_ms.active = 1; g_ms.lastTick = g_ms.t0; g_ms.lastBeat = 0; g_ms.window = 60000; PlayerPos(g_ms.lastPos); g_ms.lastTicks = g_ticks; g_ms.lastReg = RegCount();
            g_ms.id = 0;
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
    __try { PhaseTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { WatchTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_watch = 0; }
    __try { EnterTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("ENTER tick exception %08lX", GetExceptionCode()); }
    __try { PreloadTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PRELOAD tick exception %08lX", GetExceptionCode()); }
    __try { BgmTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("BGM tick exception %08lX", GetExceptionCode()); g_bgm_on = 0; }
    __try { GaugeTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("GAUGE tick exception %08lX", GetExceptionCode()); g_gg.pending = 0; }
    __try { BossTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("BBM tick exception %08lX", GetExceptionCode()); g_bm.active = 0; }
    __try { ArenaTick(); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("ARENA tick exception %08lX", GetExceptionCode()); g_ar.active = 0; }
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
        g_cfg_preload = GetPrivateProfileIntA("ArenaSwap", "Preload", 1, ini);
        g_cfg_preload_max = GetPrivateProfileIntA("ArenaSwap", "PreloadMax", 3, ini);
        g_cfg_hook = GetPrivateProfileIntA("ArenaSwap", "HookFightNow", 0, ini);
        g_cfg_bossdist = GetPrivateProfileIntA("ArenaSwap", "BossDistance", 900, ini);
        g_cfg_ambient = GetPrivateProfileIntA("ArenaSwap", "PreloadAmbient", 0, ini);
        g_cfg_bossmode = GetPrivateProfileIntA("ArenaSwap", "BossMode", 1, ini); g_cfg_bgm = GetPrivateProfileIntA("ArenaSwap", "BossBgm", 1, ini); g_cfg_native = GetPrivateProfileIntA("ArenaSwap", "NativeEnter", 1, ini); GetPrivateProfileStringA("ArenaSwap", "NativeMissions", "0x1CBC", g_native_list, sizeof g_native_list, ini);
        g_cfg_notify = GetPrivateProfileIntA("ArenaSwap", "BossModeNotify", 1, ini);
        g_cfg_ownwait = GetPrivateProfileIntA("ArenaSwap", "OwnerWaitMs", 0, ini); g_cfg_ownact = GetPrivateProfileIntA("ArenaSwap", "OwnerActivate", 0, ini);
        g_imgSize = *(uint32_t*)(B + *(uint32_t*)(B + 0x3C) + 0x18 + 0x38);
        LoadTable();
        const uint8_t reqp[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70, 0x10, 0x48, 0x89, 0x78, 0x18, 0x4C, 0x89, 0x60, 0x20};
        if (!g_cfg_hook) Log("arena swap: Enable %d, Preload %d (max %d, ambient %d); API Nioh3Training_FightNow / _Preload for Boss Rematch; MissionRequestById hook off (HookFightNow = 0)", g_cfg_enable, g_cfg_preload, g_cfg_preload_max, g_cfg_ambient);
        else if (!memcmp((void*)(B + 0x2239600), reqp, sizeof reqp)) { o_req = (ReqFn)Hook(0x2239600, 19, h_req); Log("hook MissionRequestById ok; arena swap on Fight now %s, preload %s (max %d), image 0x%X", g_cfg_enable ? "ON" : "off", g_cfg_preload ? "on" : "off", g_cfg_preload_max, g_imgSize); }
        else Log("hook MissionRequestById: prologue mismatch (hooked by another add-on?): Fight now keeps the native load");
    }
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
