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
//   actors                      list the live actor table (mgr [4745348]+B98 intrusive hash set, node = actor+E0)
//   despawn [all|last|N]        delete actors spawned by this ASI (found by diffing the actor table around CreateDynamic; native delete 2BF7E8)
//   kill [last|N] [drop]        native Character::EnemyForceKill 786C84 on a spawned actor (drop=1: ...WithItemDrop)
//   gauge <id> / ungauge <id>   UI::ShowBossGauge / UI::HideBossGauge natives (1D115D4 / 1C208BC)
//   yaw                         log camera yaw (deg), player pos, forward convention
//   fwdmode <0..3>              forward convention: 0 (sin,cos) 1 (cos,sin) 2 (-sin,cos) 3 (cos,-sin) on (x,z)
//   autogauge <0|1>             show boss gauge after each spawn (default 1)
//   phase <ms>                  mission-switch phase sampling interval in ms (default 500); the log window after 'mission' is 60 s
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
static void WatchTick(void) {  // after CreateDynamic: collect actors that were not in the pre-spawn snapshot
    if (!g_watch) return;
    uint64_t cur[1024]; int n = Actors(cur, 1024);
    for (int i = 0; i < n; ++i) if (!InSnap(cur[i]) && !InSp(cur[i]) && *(uint32_t*)(cur[i] + 4) == g_watch_id && g_nsp < MAXSP) {
        float xyz[4] = {0}; memcpy(xyz, (void*)(cur[i] + 0x110), 16);
        g_sp_actor[g_nsp++] = cur[i];
        Log("SPAWN actor #%d appeared in table: %p handle %08X %08X pos %.0f %.0f %.0f +ab84=%08X (+%lu ms)", g_nsp - 1, (void*)cur[i], *(uint32_t*)cur[i], *(uint32_t*)(cur[i] + 4), xyz[0], xyz[1], xyz[2], *(uint32_t*)(cur[i] + 0xAB84), GetTickCount() - g_watch_t0);
        g_watch = 0;
    }
    if (GetTickCount() - g_watch_t0 > 3000) { Log("SPAWN: no new actor with handle id %08X appeared within 3 s (previous actor of the same owner/slot still alive? despawn it first)", g_watch_id); g_watch = 0; }
}
static int Despawn(int idx) {
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

// ---- command channel: worker thread reads the file, game thread executes
static char g_cmd[4096];
static volatile LONG g_have;

typedef void (*LogFn)(const char*, ...);
typedef void (*PlugFn)(uintptr_t, const char*, LogFn);
static PlugFn g_plug;
// ---- spawn <charId> [dist] : Load::Character request (3F9E58 id,2,0), wait until resource entry +2C8 is ready, then CreateDynamic
static struct { uint32_t id, owner; float dist; DWORD t0; int active; } g_sp;
static uint64_t ResEntry(uint32_t id) {
    uint64_t key = ((uint64_t (*)(uint64_t, uint32_t))(B + 0x134314))(0, id);
    uint64_t rm = 0; if (!Rd64(B + 0x45B9E60, &rm) || !rm) return 0;
    return ((uint64_t (*)(uint64_t, uint64_t))(B + 0x133440))(rm, key);
}
static uint32_t PickOwner(void) {  // first registered enemy placement with a live actor (not the player); fall back to the player (1)
    uint64_t cm = 0, c0 = 0; Rd64(B + 0x45B9E40, &cm); if (!cm) return 1; Rd64(cm, &c0); if (!c0) return 1;
    uint64_t* reg = *(uint64_t**)(c0 + 0x48); uint64_t arr = reg[0], cnt = reg[1];
    for (uint64_t i = 0; i < cnt; ++i) {
        uint32_t id = *(uint32_t*)(arr + i * 16); uint64_t w = *(uint64_t*)(arr + i * 16 + 8), live = 0, rec = 0;
        if (id <= 1 || id >= 0x10000000 || !w) continue;
        Rd64(w, &live); Rd64(w + 0x18, &rec);
        if (live && rec) return id;
    }
    return 1;
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
    float face = cy + 180.0f;
    g_nsnap = Actors(g_snap, 1024);
    uint32_t r = ((DynFn)(B + 0x1C0C5E0))(g_sp.id, p, face, 0, owner, 0);
    g_watch = r != 0; g_watch_id = r; g_watch_t0 = GetTickCount();
    Log("SPAWN %u: resources ready after %lu ms; camyaw %.1f fwd %.2f %.2f; CreateDynamic(owner %u) player %.0f %.0f %.0f -> at %.0f %.0f %.0f yaw %.1f -> 0x%X%s", g_sp.id, dt, cy, dx, dz, owner, pos[0], pos[1], pos[2], p[0], p[1], p[2], face, r, r ? "" : " (refused)");
    if (r && g_autogauge) { ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x1D115D4))(r, 0, 0); Log("SPAWN %u: ShowBossGauge(0x%X) called", g_sp.id, r); }
    g_sp.active = 0;
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
            g_sp.id = (uint32_t)a[0]; g_sp.dist = n >= 3 ? (float)a[1] : 400.0f; g_sp.owner = n >= 4 ? (uint32_t)a[2] : 0; g_sp.t0 = GetTickCount();
            uint64_t e = ResEntry(g_sp.id);
            if (!e || !*(uint8_t*)(e + 0x2c8)) ((void (*)(uint32_t, uint32_t, uint32_t))(B + 0x3F9E58))(g_sp.id, 2, 0);
            Log("CMD spawn %u dist %.0f: %s", g_sp.id, g_sp.dist, e && *(uint8_t*)(e + 0x2c8) ? "resources already loaded" : "Load::Character requested");
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
        } else if (!strcmp(op, "act")) {
            uint8_t r = ((uint8_t (*)(uint32_t, uint32_t, uint8_t, uint32_t))(B + 0x1180740))((uint32_t)a[0], 0, 0, 0);
            Log("CMD act %llu -> %u", a[0], r);
        } else if (!strcmp(op, "lchr")) {
            if (!g_loader) { Log("CMD lchr: loader unknown (no Load::Character seen yet)"); return; }
            ((LoadChrFn)(B + 0xA42A7C))(g_loader, (uint32_t)a[0]);
            Log("CMD lchr %llu done (loader %p)", a[0], (void*)g_loader);
        } else if (!strcmp(op, "mission")) {
            __attribute__((aligned(16))) uint32_t p[8] = {0x101, 0x1000000, 0x10100, 0, 0, 0, 0, 0};
            ((void (*)(uint32_t, uint8_t, uint32_t, const uint32_t*))(B + 0x2239600))((uint32_t)a[0], (uint8_t)a[1], (uint32_t)a[2], p);
            Log("CMD mission 0x%llX slot %llu start %llu requested", a[0], a[1], a[2]);
            g_ms.t0 = GetTickCount(); g_ms.last = g_ms.t0; g_ms.ticks0 = g_ticks; g_ms.hadPlayer = PlayerChar() != 0; g_ms.active = 1; g_ms.lastTick = g_ms.t0; g_ms.lastBeat = 0; g_ms.window = 60000; PlayerPos(g_ms.lastPos); g_ms.lastTicks = g_ticks; g_ms.lastReg = RegCount();
            g_ms.id = (uint32_t)a[0];
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
    if (!memcmp((void*)(B + 0x1C0C5E0), dynp, 13)) { o_dyn = (DynFn)Hook(0x1C0C5E0, 13, h_dyn); Log("hook CreateDynamic ok"); }
    else Log("hook CreateDynamic: prologue mismatch");
    if (!memcmp((void*)(B + 0xA42A7C), lchp, 14)) { o_lchr = (LoadChrFn)Hook(0xA42A7C, 14, h_lchr); Log("hook Load::Character ok"); }
    else Log("hook Load::Character: prologue mismatch");
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
