// tour.cpp - Tour mode ([Debug] Tour = 0 by default, 1.7.36). See tour.h and notes\tour-mode.md.
// Threads: the game thread runs Tick() (from the TT1 thunk, once per PerFrameUpdater pass) - every game call happens there;
// "Tour worker" logs the point lines, takes the memory / VRAM snapshot, flushes the GP / SP windows and prints a heartbeat.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <intrin.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "tour.h"
#include "tour_core.h"
#include "gpuprof.h"
#include "sp.h"
#include "memstats.h"
#include "menu.h"

namespace tour {
namespace {

// ---- v2.02 (RVAs; call sites in patches.json TT1 / SB1-SB3, verified by tests\test_patterns.py) ------------------------
constexpr uint32_t kTickSite = 0x56529C, kTickCallee = 0x3B29E4;    // PerFrameUpdater 5651D0 -> FastTravelSeqTickCall
constexpr uint32_t kSb1Site = 0x5B6F37, kSb1Callee = 0x5B7BB4;      // SaveWriteStep 5B6D68 -> SaveFileWriteAtomic
constexpr uint32_t kSb2Site = 0x1FDF2D0, kSb3Site = 0x1FDF62A, kSbBackupCallee = 0x1FD9A48;  // -> BackupBinWrite
// functions the tour calls (game thread only)
constexpr uint32_t kFillWarpPoint = 0x3B2528;       // bool (req*, u32 pointId)
constexpr uint32_t kFtPost = 0x22395AC;             // void (req*) -> FastTravelSeqRequest(g_FastTravelSeq, req)
constexpr uint32_t kMissionReqById = 0x2239600;     // void (u32 id, u8 slot, int sub, const u32 params[6])
constexpr uint32_t kMissionUnlocked = 0x909218;     // al (clearDb, int id)
constexpr uint32_t kMissionCleared = 0x702B64;      // al (clearDb, int id)
// globals (pointer variables in .data)
constexpr uint32_t kGameMainPtr = 0x4759910;        // +58Ah level-load window byte
constexpr uint32_t kFtSeqPtr = 0x4757470;           // g_FastTravelSeq: +0 state, +110h pending
constexpr uint32_t kWarpSeqBPtr = 0x47098A0;        // g_WarpSeqB: +0 state
constexpr uint32_t kMissionQueue = 0x45BC420;       // 3 x 0x58: +4 id, +8 byte (empty = both 0)
constexpr uint32_t kMissionMgrPtr = 0x47514D0;      // [P+8]: +10h slot, +11h region, +14h retry point, +1Ch mission, +24h mode
constexpr uint32_t kClearDbPtr = 0x4751848;         // [P+8]+0xCF8 replay marker
constexpr uint32_t kCharMgrPtrPtr = 0x45B9E40;      // **: +27A0h no-warp mode
constexpr uint32_t kDataDbPtrPtr = 0x45B9E30;       // **: [0] mission table (count +4, records +8, 0x198 B), [4] id->index map
constexpr uint32_t kPointRegPtr = 0x47098E0;        // warp-point registry: vector +18h..+20h of 16-B {obj, ?}
constexpr uint32_t kPlayerTabPtr = 0x4745348;       // player i = [P + (i + 0xCD) * 0x18]
constexpr uint32_t kNetMgrPtr = 0x45C84F8;          // +1DCh connected
constexpr uint32_t kPlayModePtr = 0x4709B50;        // +34h mode (1 online, 2 Tokoyo co-op)

// head bytes checked before anything is installed (rip-relative displacements included where they name the globals above)
struct HeadCheck {
    uint32_t rva;
    uint8_t n;
    uint8_t b[16];
};
const HeadCheck kHeads[] = {
    {kFillWarpPoint, 16, {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x20, 0x57}},
    {kFtPost, 14, {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0xD1, 0x48, 0x8B, 0x0D, 0xB6, 0xDE, 0x51, 0x02}},   // mov rcx,[4757470]
    {kMissionReqById, 16, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70, 0x10, 0x48, 0x89, 0x78, 0x18, 0x4C}},
    {kMissionUnlocked, 16, {0x4C, 0x8B, 0x49, 0x08, 0x49, 0xB8, 0x25, 0x23, 0x22, 0x84, 0xE4, 0x9C, 0xF2, 0xCB, 0x33, 0xC9}},
    {kMissionCleared, 16, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x89, 0x50, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48}},
    {kTickCallee, 14, {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0xD1, 0x48, 0x8B, 0x0D, 0x7E, 0x4A, 0x3A, 0x04}},  // mov rcx,[4757470]
    {kSb1Callee, 16, {0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC}},
    {kSbBackupCallee, 16, {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x68, 0x18, 0x48, 0x89, 0x70, 0x20, 0x48}},
};

using FillWarpPoint_t = uint64_t (*)(void* req, uint32_t id);
using FtPost_t = void (*)(void* req);
using MissionReqById_t = void (*)(uint32_t id, uint32_t slot, int sub, const uint32_t* params);
using DbGet_t = uint64_t (*)(void* db, int id);

// ---- config ------------------------------------------------------------------------------------------------------------
struct Cfg {
    int mode = 0;
    int dwellSec = 4;
    int pointsPerStage = 3;
    int passes = 1;
    int loadTimeoutSec = 120;
    int warpTimeoutSec = 45;
    int startDelaySec = 5;
    int settleSec = 3;
    int minPointDist = 3000;   // world units between chosen points
    std::wstring listPath, progressPath, dir;
} g_cfg;

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
double g_qpcMs = 0;
uint8_t* g_page = nullptr;
volatile uint8_t* g_blockOn = nullptr;   // thunk page byte: 1 = save writers skipped
volatile long* g_sb1Count = nullptr;     // user-save writes skipped
volatile long* g_sb2Count = nullptr;     // BACKUP.BIN writes skipped
volatile LONG g_guardDenied = 0;
volatile LONG g_guardLogged = 0;
bool g_savesBlocked = false;

#define TLOG(...) \
    do {          \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

inline int64_t Qpc() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return q.QuadPart;
}
inline double SinceMs(int64_t q0) { return (double)(Qpc() - q0) * g_qpcMs; }
std::string Narrow(const wchar_t* w) {
    if (!w) return "(null)";
    char b[600] = {};
    WideCharToMultiByte(CP_UTF8, 0, w, -1, b, (int)sizeof(b) - 1, nullptr, nullptr);
    return b;
}

// ---- patching (same scheme as autocont.cpp: one interlocked 8/16-byte CAS, or EB FE + tail + head) ---------------------
uint8_t* AllocIn(uintptr_t from, uintptr_t to, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + size <= re && c + size <= to)
                if (void* p = VirtualAlloc((void*)c, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach, size)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo, size);
}
bool PatchAtomic(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7), w16 = a & ~uintptr_t(15);
    const bool fit8 = a + len <= w8 + 8, fit16 = a + len <= w16 + 16;
    if (!fit8 && !fit16) return false;
    void* win = fit8 ? (void*)w8 : (void*)w16;
    const size_t wlen = fit8 ? 8 : 16;
    DWORD old = 0;
    if (!VirtualProtect(win, wlen, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = false;
    if (fit8) {
        volatile LONG64* q = (volatile LONG64*)w8;
        const LONG64 cur = *q;
        LONG64 nw = cur;
        memcpy((uint8_t*)&nw + (a - w8), bytes, len);
        ok = InterlockedCompareExchange64(q, nw, cur) == cur;
    } else {
        volatile LONG64* q = (volatile LONG64*)w16;
        alignas(16) LONG64 cur[2] = {q[0], q[1]};
        LONG64 nw[2] = {cur[0], cur[1]};
        memcpy((uint8_t*)nw + (a - w16), bytes, len);
        ok = _InterlockedCompareExchange128(q, nw[1], nw[0], cur) != 0;
    }
    VirtualProtect(win, wlen, old, &old);
    FlushInstructionCache(GetCurrentProcess(), win, wlen);
    return ok;
}
bool PatchSpin(uint8_t* p, const uint8_t* bytes, size_t len) {
    static const uint8_t spin[2] = {0xEB, 0xFE};
    uint8_t orig[2] = {p[0], p[1]};
    if (len < 3 || !PatchAtomic(p, spin, 2)) return false;
    DWORD old = 0;
    if (!VirtualProtect(p + 2, len - 2, PAGE_EXECUTE_READWRITE, &old)) {
        PatchAtomic(p, orig, 2);
        return false;
    }
    memcpy(p + 2, bytes + 2, len - 2);
    VirtualProtect(p + 2, len - 2, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return PatchAtomic(p, bytes, 2);
}
bool Patch(uint8_t* p, const uint8_t* bytes, size_t len) { return PatchAtomic(p, bytes, len) || PatchSpin(p, bytes, len); }

bool CallSiteOk(uint32_t site, uint32_t callee) {
    const uint8_t* s = (const uint8_t*)(g_base + site);
    int32_t rel = 0;
    memcpy(&rel, s + 1, 4);
    return s[0] == 0xE8 && (uintptr_t)(s + 5 + rel) == g_base + callee;
}
// Coexisting with another ASI (Nioh3BossRematch.asi loads BEFORE this one, alphabetically): it may already have
// rewritten the E8 rel32 at the tick site to its own thunk (which chains to the stock callee), and put an
// E9 rel32 + NOPs entry hook over the head of MissionRequestById. Both shapes are verified byte-for-byte below;
// anything else is "another patch" and only that feature is skipped.
bool InImage(uintptr_t a) {
    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    return a >= g_base && a < g_base + nt->OptionalHeader.SizeOfImage;
}
bool ExecutableAt(uintptr_t a) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((const void*)a, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return false;
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}
// E8 rel32 whose target is the stock callee, or (chained) another module's thunk in executable memory outside the image.
// *target = where the call goes now; *chained = it is not the stock callee.
bool CallSiteTarget(uint32_t site, uint32_t callee, uint64_t* target, bool* chained) {
    const uint8_t* s = (const uint8_t*)(g_base + site);
    if (s[0] != 0xE8) return false;
    int32_t rel = 0;
    memcpy(&rel, s + 1, 4);
    const uintptr_t t = (uintptr_t)(s + 5 + rel);
    *chained = t != g_base + callee;
    if (*chained && (InImage(t) || !ExecutableAt(t))) return false;
    *target = t;
    return true;
}
// function head: the stock bytes, or another module's E9 rel32 (+ NOPs) entry hook over the first 5..8 bytes with the stock tail intact
bool HeadOk(uint32_t rva, const uint8_t* want, size_t n, bool* hooked) {
    const uint8_t* p = (const uint8_t*)(g_base + rva);
    *hooked = false;
    if (memcmp(p, want, n) == 0) return true;
    // another module's entry hook: E9 rel32 or FF 25 00000000 abs64 (14 bytes), padded with NOPs up to the next instruction
    // boundary (909218: 4 + 10 bytes before the first boundary after 5, so a hook there is E9 + 9 NOPs or the 14-byte form)
    if (n < 9) return false;
    uintptr_t t = 0;
    size_t i = 0;
    if (p[0] == 0xE9) {
        int32_t rel = 0;
        memcpy(&rel, p + 1, 4);
        t = (uintptr_t)(p + 5 + rel);
        i = 5;
    } else if (p[0] == 0xFF && p[1] == 0x25 && !p[2] && !p[3] && !p[4] && !p[5] && n >= 14) {
        memcpy(&t, p + 6, 8);
        i = 14;
    } else {
        return false;
    }
    if (InImage(t) || !ExecutableAt(t)) return false;
    while (i < n && p[i] == 0x90) ++i;
    if (memcmp(p + i, want + i, n - i) != 0) return false;
    *hooked = true;
    return true;
}bool Retarget(uint32_t site, const uint8_t* thunk) {
    uint8_t* s = (uint8_t*)(g_base + site);
    const intptr_t d = (intptr_t)thunk - (intptr_t)(s + 5);
    if (d < INT32_MIN || d > INT32_MAX) return false;
    uint8_t nb[5] = {0xE8, 0, 0, 0, 0};
    const int32_t r = (int32_t)d;
    memcpy(nb + 1, &r, 4);
    return Patch(s, nb, 5);
}
void Put32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
void Put64(uint8_t* p, uint64_t v) { memcpy(p, &v, 8); }
int32_t RipDisp(const uint8_t* insnEnd, const volatile void* target) { return (int32_t)((intptr_t)target - (intptr_t)insnEnd); }

// ---- guarded reads -------------------------------------------------------------------------------------------------------
template <class T>
bool Rd(uintptr_t a, T& v) {
    __try {
        v = *(const volatile T*)a;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
uintptr_t Ptr(uint32_t rva) {
    uintptr_t v = 0;
    return Rd(g_base + rva, v) ? v : 0;
}
uintptr_t PtrPtr(uint32_t rva) {
    uintptr_t p = Ptr(rva), v = 0;
    return p && Rd(p, v) ? v : 0;
}

bool OnlineNow() {
    uintptr_t net = Ptr(kNetMgrPtr), pm = Ptr(kPlayModePtr);
    uint8_t c = 0;
    int32_t m = 0;
    if (net && (!Rd(net + 0x1DC, c) || c)) return true;
    if (pm && (!Rd(pm + 0x34, m) || m != 0)) return true;
    return false;
}
uintptr_t Player() {
    uintptr_t t = Ptr(kPlayerTabPtr), p = 0;
    return t && Rd(t + 0xCD * 0x18, p) ? p : 0;
}
bool PlayerPos(Pos& out) {
    const uintptr_t p = Player();
    return p && Rd(p + 0x110, out.x) && Rd(p + 0x114, out.y) && Rd(p + 0x118, out.z);
}
int LoadByte() {
    uintptr_t g = Ptr(kGameMainPtr);
    uint8_t f = 0;
    return g && Rd(g + 0x58A, f) ? f : -1;
}
// The sequencer states are small enums (FT 0..8, B 0..2). On the Ally (Proton) after a title "Load Game" the global pointers
// resolve to reused memory and read floats (FT 1148547920 = 0x4475E550, B 1148229280; bridge #745/#748), so the tour never
// armed. A value outside 0..63 is not a sequencer state: treat it as idle (0) and log it once. Player / 58A / mission queue /
// pause gates still apply, so a real load or warp keeps blocking the tour.
static int SaneSeq(int v, const char* which) {
    if (v >= -1 && v < 64) return v;
    static volatile LONG once[2] = {0, 0};
    const int i = which[0] == 'F' ? 0 : 1;
    if (InterlockedExchange(&once[i], 1) == 0)
        TLOG("Tour: %s sequencer state reads %d (0x%08X) - not a state (stale pointer after Load Game?), treated as idle", which,
             v, (unsigned)v);
    return 0;
}
int FtState() {
    uintptr_t s = Ptr(kFtSeqPtr);
    int32_t v = 0;
    if (!s) return -1;
    return Rd(s, v) ? SaneSeq(v, "FT") : -1;
}
int WarpBState() {
    uintptr_t s = Ptr(kWarpSeqBPtr);
    int32_t v = 0;
    if (!s) return 0;
    return Rd(s, v) ? SaneSeq(v, "B") : -1;
}
bool QueueEmpty() {
    for (int i = 0; i < 3; ++i) {
        uint32_t id = 1;
        uint8_t b = 1;
        if (!Rd(g_base + kMissionQueue + i * 0x58 + 4, id) || !Rd(g_base + kMissionQueue + i * 0x58 + 8, b)) return false;
        if (id || b) return false;
    }
    return true;
}
bool NoWarpMode() {
    uintptr_t cm = PtrPtr(kCharMgrPtrPtr);
    uint8_t f = 1;
    return !cm || !Rd(cm + 0x27A0, f) || f != 0;
}
uintptr_t MissionData() {
    uintptr_t m = Ptr(kMissionMgrPtr), d = 0;
    return m && Rd(m + 8, d) ? d : 0;
}
// mission key -> record (nullptr if absent): the same open-addressing map MissionIdToIndex 377B8C walks
uintptr_t MissionRecord(uint32_t key) {
    uintptr_t dm = PtrPtr(kDataDbPtrPtr), tab = 0, map = 0, b = 0, e = 0;
    int32_t empty = 0;
    uint32_t cnt = 0;
    if (!dm || !Rd(dm, tab) || !Rd(dm + 0x20, map) || !tab || !map) return 0;
    if (!Rd(map + 4, empty) || !Rd(map + 8, b) || !Rd(map + 0x10, e) || !Rd(tab + 4, cnt) || e < b || e - b > 0x100000) return 0;
    for (uintptr_t s = b; s < e; s += 8) {
        int32_t k = 0, idx = -1;
        if (!Rd(s, k) || !Rd(s + 4, idx)) return 0;
        if (k == (int32_t)key && k != empty && idx >= 0 && (uint32_t)idx < cnt) return tab + 8 + (uintptr_t)idx * 0x198;
    }
    return 0;
}

// ---- game calls (game thread; SEH: a fault disarms the tour) ------------------------------------------------------------
bool CallFill(void* req, uint32_t id, bool& ok) {
    __try {
        ok = (((FillWarpPoint_t)(g_base + kFillWarpPoint))(req, id) & 0xFF) != 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool CallFtPost(void* req) {
    __try {
        ((FtPost_t)(g_base + kFtPost))(req);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool CallMissionReq(uint32_t key, uint32_t slot) {
    alignas(16) static const uint32_t params[6] = {0x101, 0x1000000, 0x10100, 0, 0, 0};  // = Mission Replay menu F1FD80
    __try {
        ((MissionReqById_t)(g_base + kMissionReqById))(key, slot, 0, params);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool CallDbGet(uint32_t rva, uint32_t key, bool& v) {
    __try {
        const uintptr_t db = Ptr(kClearDbPtr);
        if (!db) return false;
        v = (((DbGet_t)(g_base + rva))((void*)db, (int)key) & 0xFF) != 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ---- warp points ----------------------------------------------------------------------------------------------------------
struct WarpPt {
    uint32_t id = 0, flags = 0, link = 0;
    Pos pos;
};
void EnumPointsRaw(std::vector<WarpPt>& out) {
    const uintptr_t reg = Ptr(kPointRegPtr);
    uintptr_t b = 0, e = 0;
    if (!reg || !Rd(reg + 0x18, b) || !Rd(reg + 0x20, e) || e < b || e - b > 16 * 4096) return;
    for (uintptr_t p = b; p < e && out.size() < 4096; p += 16) {
        uintptr_t obj = 0, data = 0, hdr = 0, map = 0, sb = 0, se = 0;
        int32_t live = 0, cnt = 0, empty = 0;
        if (!Rd(p, obj) || !obj) continue;
        if (!Rd(obj + 0x10, data) || !data || !Rd(data + 0x460, live) || !live || !Rd(data + 0x418, hdr) || !hdr) continue;
        if (!Rd(hdr + 4, cnt) || cnt <= 0 || cnt > 100000 || !Rd(obj + 0x10 + 3 * 8, map) || !map) continue;
        if (!Rd(map + 4, empty) || !Rd(map + 8, sb) || !Rd(map + 0x10, se) || se < sb || se - sb > 8 * 200000) continue;
        for (uintptr_t s = sb; s < se && out.size() < 4096; s += 8) {
            int32_t k = 0, idx = -1;
            if (!Rd(s, k) || !Rd(s + 4, idx) || k == empty || idx < 0 || idx >= cnt) continue;
            const uintptr_t rec = hdr + 8 + (uintptr_t)idx * 0xA4;
            WarpPt w;
            w.id = (uint32_t)k;
            if (!Rd(rec + 8, w.flags) || !Rd(rec + 0xC, w.link) || !Rd(rec + 0x40, w.pos.x) || !Rd(rec + 0x44, w.pos.y) ||
                !Rd(rec + 0x48, w.pos.z))
                continue;
            out.push_back(w);
        }
    }
}

// ---- IAT guard (verifies the save block; denies write-type calls into the save folder) -----------------------------------
using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using MoveFileExW_t = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, DWORD);
using MoveFileW_t = BOOL(WINAPI*)(LPCWSTR, LPCWSTR);
using DeleteFileW_t = BOOL(WINAPI*)(LPCWSTR);
using CopyFileW_t = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, BOOL);
using SetFileAttributesW_t = BOOL(WINAPI*)(LPCWSTR, DWORD);
CreateFileW_t o_CreateFileW = nullptr;
MoveFileExW_t o_MoveFileExW = nullptr;
MoveFileW_t o_MoveFileW = nullptr;
DeleteFileW_t o_DeleteFileW = nullptr;
CopyFileW_t o_CopyFileW = nullptr;
SetFileAttributesW_t o_SetFileAttributesW = nullptr;

void Denied(const char* api, LPCWSTR a, LPCWSTR b = nullptr) {
    InterlockedIncrement(&g_guardDenied);
    if (InterlockedIncrement(&g_guardLogged) <= 40)
        TLOG("Tour: SAVE GUARD denied %s %s%s%s (a save write the SB block did not catch - nothing reached the disk)", api,
             Narrow(a).c_str(), b ? " -> " : "", b ? Narrow(b).c_str() : "");
    SetLastError(ERROR_ACCESS_DENIED);
}
HANDLE WINAPI HkCreateFileW(LPCWSTR n, DWORD acc, DWORD sh, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD fl, HANDLE t) {
    const DWORD writeAcc = GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA |
                           FILE_WRITE_ATTRIBUTES | DELETE | WRITE_DAC | WRITE_OWNER;
    if (IsSavePath(n) && ((acc & writeAcc) || disp != OPEN_EXISTING || (fl & FILE_FLAG_DELETE_ON_CLOSE))) {
        Denied("CreateFileW", n);
        return INVALID_HANDLE_VALUE;
    }
    return o_CreateFileW(n, acc, sh, sa, disp, fl, t);
}
BOOL WINAPI HkMoveFileExW(LPCWSTR a, LPCWSTR b, DWORD f) {
    if (IsSavePath(a) || IsSavePath(b)) return Denied("MoveFileExW", a, b), FALSE;
    return o_MoveFileExW(a, b, f);
}
BOOL WINAPI HkMoveFileW(LPCWSTR a, LPCWSTR b) {
    if (IsSavePath(a) || IsSavePath(b)) return Denied("MoveFileW", a, b), FALSE;
    return o_MoveFileW(a, b);
}
BOOL WINAPI HkDeleteFileW(LPCWSTR a) {
    if (IsSavePath(a)) return Denied("DeleteFileW", a), FALSE;
    return o_DeleteFileW(a);
}
BOOL WINAPI HkCopyFileW(LPCWSTR a, LPCWSTR b, BOOL f) {
    if (IsSavePath(b)) return Denied("CopyFileW", a, b), FALSE;
    return o_CopyFileW(a, b, f);
}
BOOL WINAPI HkSetFileAttributesW(LPCWSTR a, DWORD at) {
    if (IsSavePath(a)) return Denied("SetFileAttributesW", a), FALSE;
    return o_SetFileAttributesW(a, at);
}

void** FindImportSlot(const char* dllPrefix, const char* name) {
    __try {
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(g_base);
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(g_base + dos->e_lfanew);
        const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dd.VirtualAddress) return nullptr;
        for (auto d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(g_base + dd.VirtualAddress); d->Name; ++d) {
            const char* dll = reinterpret_cast<const char*>(g_base + d->Name);
            if (_strnicmp(dll, dllPrefix, strlen(dllPrefix)) != 0 || !d->OriginalFirstThunk || !d->FirstThunk) continue;
            auto names = reinterpret_cast<IMAGE_THUNK_DATA64*>(g_base + d->OriginalFirstThunk);
            auto iat = reinterpret_cast<void**>(g_base + d->FirstThunk);
            for (int i = 0; names[i].u1.AddressOfData; ++i) {
                const ULONGLONG e = names[i].u1.AddressOfData;
                if (!IMAGE_SNAP_BY_ORDINAL64(e) && !strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(g_base + e)->Name, name))
                    return &iat[i];
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}
template <class F>
bool HookImport(const char* name, F hook, F& orig) {
    void** slot = FindImportSlot("KERNEL32", name);
    if (!slot) return false;
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    orig = (F)*slot;
    InterlockedExchangePointer(slot, (void*)hook);
    VirtualProtect(slot, sizeof(void*), old, &old);
    return orig != nullptr;
}

// ---- worker queue ----------------------------------------------------------------------------------------------------------
struct PointRec {
    int pass = 0, stage = 0, nStages = 0, pt = 0, nPts = 0;
    uint32_t key = 0, missionNow = 0, warpId = 0;
    int region = -1;
    std::string src, label;
    Pos pos;
    FrameStats fs;
    double dwellS = 0, loadS = 0, warpS = 0, cpuCores = 0, mainBusy = 0;
    bool interrupted = false;
    long long t0ms = 0, t1ms = 0;  // unix ms of the dwell window (PresentMon join)
};
struct Task {
    int kind = 0;  // 1 window begin, 2 point done, 3 plain line
    PointRec rec;
    std::string text;
};
CRITICAL_SECTION g_qLock;
std::deque<Task> g_q;
HANDLE g_qEvent = nullptr;
void Push(Task&& t) {
    EnterCriticalSection(&g_qLock);
    g_q.push_back(std::move(t));
    LeaveCriticalSection(&g_qLock);
    SetEvent(g_qEvent);
}
long long UnixMs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (long long)(u.QuadPart / 10000ULL) - 11644473600000LL;
}

// ---- progress file ---------------------------------------------------------------------------------------------------------
void ProgressLine(const char* fmt, ...) {
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(b, sizeof(b) - 2, fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    size_t len = strlen(b);
    b[len++] = '\n';
    HANDLE h = CreateFileW(g_cfg.progressPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(h, b, (DWORD)len, &w, nullptr);
    FlushFileBuffers(h);
    CloseHandle(h);
}
std::string ReadFileText(const std::wstring& p) {
    std::string s;
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return s;
    LARGE_INTEGER sz{};
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart < (1 << 24)) {
        s.resize((size_t)sz.QuadPart);
        DWORD r = 0;
        if (!ReadFile(h, &s[0], (DWORD)s.size(), &r, nullptr)) r = 0;
        s.resize(r);
    }
    CloseHandle(h);
    return s;
}

// ---- state machine (game thread) -------------------------------------------------------------------------------------------
enum class St { WaitArm, NextStage, Loading, Settle, NextPoint, Warping, WarpSettle, Dwell, Done, Off };
const char* StName(St s) {
    static const char* n[] = {"wait-arm", "next-stage", "loading", "settle", "next-point", "warping", "warp-settle", "dwell", "done", "off"};
    return n[(int)s];
}
struct Target {
    std::string src;     // "spawn", "warp", "list"
    uint32_t warpId = 0;
    Pos pos;
};
struct Run {
    std::vector<Stage> list;
    Progress prog;
    St st = St::WaitArm;
    int pass = 0, stage = -1, pt = 0;
    int64_t tState = 0, tArmOk = 0, tReq = 0, tFrame = 0;
    bool sawLoad = false, sawWarp = false, armed = false;
    uintptr_t playerAtReq = 0;
    double loadS = 0, warpS = 0;
    std::vector<Target> targets;
    std::vector<double> frames;
    FILETIME pk0{}, pu0{}, tk0{}, tu0{};
    int64_t tDwell0 = 0;
    long long dwellUnix0 = 0;
    bool interrupted = false;
    uint64_t ticks = 0;
} g_run;
volatile LONG g_tickBusy = 0;
volatile LONG64 g_ticks = 0;
volatile LONG g_stateId = 0, g_stageId = -1, g_ptId = 0;

void SetSt(St s) {
    g_run.st = s;
    g_run.tState = Qpc();
    InterlockedExchange(&g_stateId, (LONG)s);
}
void Disarm(const char* why) {
    TLOG("Tour: DISARMED - %s (no more loads or warps; saves stay blocked until the game is closed)", why);
    SetSt(St::Off);
}
bool StableField() {
    return Player() && LoadByte() == 0 && FtState() == 0 && WarpBState() == 0 && QueueEmpty() && menu::GamePauseReason() == 0;
}
uint64_t Ft(const FILETIME& f) { return ((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime; }

void BuildTargets() {
    Run& r = g_run;
    r.targets.clear();
    Target sp;
    sp.src = "spawn";
    PlayerPos(sp.pos);
    r.targets.push_back(sp);
    const int want = std::max(0, g_cfg.pointsPerStage - 1);
    std::vector<WarpPt> raw;
    EnumPointsRaw(raw);
    // real positions from the game's own FillWarpPoint (handles gadget-relative points); dedupe by id
    std::vector<Pos> cand;
    std::vector<uint32_t> ids;
    std::vector<WarpPt> kept;
    int filled = 0, bad = 0;
    for (const WarpPt& w : raw) {
        bool dup = false;
        for (uint32_t i : ids) dup |= i == w.id;
        if (dup || w.id == 0 || ids.size() >= 1024) continue;
        alignas(16) uint8_t req[0x80] = {};
        bool ok = false;
        if (!CallFill(req, w.id, ok)) {
            Disarm("exception in FillWarpPoint 3B2528");
            return;
        }
        Pos p;
        memcpy(&p.x, req + 0, 4);
        memcpy(&p.y, req + 4, 4);
        memcpy(&p.z, req + 8, 4);
        if (!ok || !PosOk(p)) {
            ++bad;
            continue;
        }
        ++filled;
        ids.push_back(w.id);
        cand.push_back(p);
        kept.push_back(w);
    }
    const std::vector<int> pick = FarthestPoints(sp.pos, cand, want, (float)g_cfg.minPointDist);
    for (int i : pick) {
        Target t;
        t.src = "warp";
        t.warpId = ids[i];
        t.pos = cand[i];
        r.targets.push_back(t);
    }
    if (r.stage >= 0 && r.stage < (int)r.list.size())
        for (const Pos& p : r.list[r.stage].extra) {
            Target t;
            t.src = "list";
            t.pos = p;
            r.targets.push_back(t);
        }
    TLOG("Tour: stage %d points: %zu loaded warp points, %d usable, %d unusable; chose %zu (spawn + %zu warp + %zu list)",
         r.stage + 1, raw.size(), filled, bad, r.targets.size(), pick.size(), r.list[r.stage].extra.size());
    for (size_t i = 0; i < pick.size(); ++i) {
        const WarpPt& w = kept[pick[i]];
        TLOG("Tour:   warp point id %u flags 0x%X link %u at %.0f %.0f %.0f", w.id, w.flags, w.link, cand[pick[i]].x,
             cand[pick[i]].y, cand[pick[i]].z);
    }
}

bool PostWarp(const Target& t) {
    alignas(16) uint8_t req[0x80] = {};
    if (t.warpId) {
        bool ok = false;
        if (!CallFill(req, t.warpId, ok)) return Disarm("exception in FillWarpPoint 3B2528"), false;
        if (!ok) return false;
        Put32(req + 0x24, 0);   // no point id: no retry-shrine registration / clear-db write (state 4, A61A0C); the
                                // state-8 AutosaveRequest still runs and its write is skipped by SB1
    } else {
        memcpy(req + 0, &t.pos.x, 4);
        memcpy(req + 4, &t.pos.y, 4);
        memcpy(req + 8, &t.pos.z, 4);
        const float one = 1.0f;
        memcpy(req + 12, &one, 4);
    }
    req[0x20] = 0;      // no long ground snap
    req[0x21] = 1;      // short ground snap (+-100), as FastTravelRequestTo
    req[0x28] = req[0x29] = req[0x2A] = 0;
    req[0x2B] = 1;      // as the native requests
    req[0x2C] = 1;      // never redirected to the current retry point (state 4)
    memset(req + 0x30, 0, 0x50);  // popping wait 0, empty callback
    if (!CallFtPost(req)) return Disarm("exception in FastTravelRequestPost 22395AC"), false;
    return true;
}

void BeginDwell() {
    Run& r = g_run;
    r.frames.clear();
    r.frames.reserve(4096);
    r.tFrame = 0;
    r.interrupted = false;
    FILETIME c, e;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &r.pk0, &r.pu0);
    GetThreadTimes(GetCurrentThread(), &c, &e, &r.tk0, &r.tu0);
    r.tDwell0 = Qpc();
    r.dwellUnix0 = UnixMs();
    ProgressLine("B %d %d %d", r.pass, r.stage, r.pt);
    sp::BeginWindow();
    Task t;
    t.kind = 1;
    Push(std::move(t));
    SetSt(St::Dwell);
}

void EndDwell() {
    Run& r = g_run;
    char tag[64];
    snprintf(tag, sizeof(tag), "tour %d/%d/%d", r.pass + 1, r.stage + 1, r.pt + 1);
    sp::EndWindow(tag);
    PointRec rec;
    rec.pass = r.pass + 1;
    rec.stage = r.stage + 1;
    rec.nStages = (int)r.list.size();
    rec.pt = r.pt + 1;
    rec.nPts = (int)r.targets.size();
    rec.key = r.list[r.stage].key;
    rec.label = r.list[r.stage].label;
    const Target& tg = r.targets[r.pt];
    rec.src = tg.src;
    rec.warpId = tg.warpId;
    if (!PlayerPos(rec.pos)) rec.pos = tg.pos;
    const uintptr_t md = MissionData();
    uint8_t reg = 0xFF;
    if (md) {
        Rd(md + 0x1C, rec.missionNow);  // [mgr+8]+1Ch = current mission (MissionRequestReturn); +14h is the retry point
        if (Rd(md + 0x11, reg)) rec.region = reg;
    }
    rec.fs = ComputeFrameStats(r.frames);
    rec.dwellS = SinceMs(r.tDwell0) / 1000.0;
    rec.loadS = r.pt == 0 ? r.loadS : 0;
    rec.warpS = r.pt == 0 ? 0 : r.warpS;
    FILETIME c, e, pk, pu, tk, tu;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &pk, &pu);
    GetThreadTimes(GetCurrentThread(), &c, &e, &tk, &tu);
    const double wall100ns = rec.dwellS * 1e7;
    if (wall100ns > 0) {
        rec.cpuCores = (double)((Ft(pk) - Ft(r.pk0)) + (Ft(pu) - Ft(r.pu0))) / wall100ns;
        rec.mainBusy = 100.0 * (double)((Ft(tk) - Ft(r.tk0)) + (Ft(tu) - Ft(r.tu0))) / wall100ns;
    }
    rec.interrupted = r.interrupted;
    rec.t0ms = r.dwellUnix0;
    rec.t1ms = UnixMs();
    Task t;
    t.kind = 2;
    t.rec = std::move(rec);
    Push(std::move(t));
    ProgressLine("D %d %d %d", r.pass, r.stage, r.pt);
}

// next stage index (pass, stage) not yet done; false at the end
bool AdvanceStage() {
    Run& r = g_run;
    for (;;) {
        ++r.stage;
        if (r.stage >= (int)r.list.size()) {
            r.stage = 0;
            if (++r.pass >= g_cfg.passes) return false;
        }
        if (!r.prog.doneStages.count({r.pass, r.stage})) return true;
    }
}

void StartStageLoad() {
    Run& r = g_run;
    const Stage& s = r.list[r.stage];
    const char* lbl = s.label.empty() ? "" : s.label.c_str();
    if (!MissionRecord(s.key)) {
        TLOG("Tour: stage %d/%zu mission %u %s - no such mission record, skipped", r.stage + 1, r.list.size(), s.key, lbl);
        ProgressLine("A %d %d %u missing", r.pass, r.stage, s.key);
        SetSt(St::NextStage);
        return;
    }
    bool unl = false, clr = false;
    if (!CallDbGet(kMissionUnlocked, s.key, unl) || !CallDbGet(kMissionCleared, s.key, clr)) {
        Disarm("exception in MissionUnlockedGet / MissionClearedGetSlot");
        return;
    }
    if (g_cfg.mode == 1 && !unl && !clr) {
        TLOG("Tour: stage %d/%zu mission %u %s - not unlocked on this save, skipped (Tour = 1)", r.stage + 1, r.list.size(),
             s.key, lbl);
        ProgressLine("A %d %d %u locked", r.pass, r.stage, s.key);
        SetSt(St::NextStage);
        return;
    }
    const uintptr_t md = MissionData();
    uint8_t slot = 0;
    if (!md || !Rd(md + 0x10, slot)) {
        Disarm("mission manager not readable");
        return;
    }
    ProgressLine("L %d %d %u", r.pass, r.stage, s.key);
    TLOG("Tour: stage %d/%zu pass %d - loading mission %u %s (unlocked %d, cleared %d, slot %u)", r.stage + 1, r.list.size(),
         r.pass + 1, s.key, lbl, unl, clr, slot);
    if (clr) {  // the Mission Replay menu marks a replay this way (F1FD80); only for cleared missions
        const uintptr_t db = Ptr(kClearDbPtr);
        uintptr_t d = 0;
        if (db && Rd(db + 8, d) && d) {
            __try {
                *(volatile uint8_t*)(d + 0xCF8) = 1;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }
    if (!CallMissionReq(s.key, slot)) {
        Disarm("exception in MissionRequestById 2239600");
        return;
    }
    r.tReq = Qpc();
    r.sawLoad = false;
    r.playerAtReq = Player();
    SetSt(St::Loading);
}

void Tick() {
    Run& r = g_run;
    InterlockedIncrement64(&g_ticks);
    const int64_t now = Qpc();
    if (r.st == St::Off || r.st == St::Done) return;
    if (OnlineNow()) {
        Disarm("an online / co-op session is active (net manager connected or online play mode) - the tour never runs online");
        return;
    }
    const double inState = (double)(now - r.tState) * g_qpcMs / 1000.0;
    switch (r.st) {
        case St::WaitArm:
            if (!StableField()) {
                r.tArmOk = 0;
                return;
            }
            if (!r.tArmOk) r.tArmOk = now;
            if ((double)(now - r.tArmOk) * g_qpcMs < g_cfg.startDelaySec * 1000.0) return;
            r.armed = true;
            TLOG("Tour: ARMED (mode %d, %zu stages x %d pass(es), %d points/stage, dwell %d s); saves blocked (SB1-SB3 + guard)",
                 g_cfg.mode, r.list.size(), g_cfg.passes, g_cfg.pointsPerStage, g_cfg.dwellSec);
            SetSt(St::NextStage);
            return;
        case St::NextStage:
            if (!AdvanceStage()) {
                TLOG("Tour: DONE - all stages visited. Save writes skipped: user %ld, backup %ld; guard denials %ld. "
                     "Close the game now (the in-memory state is not a play state).",
                     *g_sb1Count, *g_sb2Count, (long)g_guardDenied);
                ProgressLine("E");
                SetSt(St::Done);
                return;
            }
            r.pt = 0;
            StartStageLoad();
            return;
        case St::Loading: {
            const int lb = LoadByte();
            if (lb == 1) r.sawLoad = true;
            if (!r.sawLoad && inState > 3 && QueueEmpty() && Player() && Player() != r.playerAtReq) r.sawLoad = true;  // a load without 58A
            if (r.sawLoad && lb == 0 && QueueEmpty() && Player() && FtState() == 0 && WarpBState() == 0) {
                SetSt(St::Settle);
                return;
            }
            if (!r.sawLoad && inState > 20 && QueueEmpty()) {
                TLOG("Tour: stage %d mission %u - the request was dropped (no load within 20 s), skipped", r.stage + 1,
                     r.list[r.stage].key);
                ProgressLine("A %d %d %u refused", r.pass, r.stage, r.list[r.stage].key);
                SetSt(St::NextStage);
                return;
            }
            if (inState > g_cfg.loadTimeoutSec) {
                TLOG("Tour: STALL stage %d mission %u - not in the field %d s after the request (58A %d, queue %s, player %s, FT %d)",
                     r.stage + 1, r.list[r.stage].key, g_cfg.loadTimeoutSec, lb, QueueEmpty() ? "empty" : "busy",
                     Player() ? "yes" : "no", FtState());
                ProgressLine("A %d %d %u stall", r.pass, r.stage, r.list[r.stage].key);
                SetSt(St::NextStage);
            }
            return;
        }
        case St::Settle:
            if (!StableField()) {
                if (inState > g_cfg.loadTimeoutSec) {
                    TLOG("Tour: STALL stage %d - loaded but never stable (a menu, event or dialog?)", r.stage + 1);
                    ProgressLine("A %d %d %u stall", r.pass, r.stage, r.list[r.stage].key);
                    SetSt(St::NextStage);
                } else {
                    r.tState = now;  // restart the settle window
                }
                return;
            }
            if (inState < g_cfg.settleSec) return;
            r.loadS = (double)(r.tState - r.tReq) * g_qpcMs / 1000.0;
            {
                Pos p;
                PlayerPos(p);
                const uintptr_t md = MissionData();
                uint32_t mid = 0;
                uint8_t reg = 0;
                if (md) Rd(md + 0x1C, mid), Rd(md + 0x11, reg);
                TLOG("Tour: stage %d arrived: request -> field %.1f s (incl. %d s settle), mission now %u, region %u, at %.0f %.0f %.0f",
                     r.stage + 1, r.loadS + g_cfg.settleSec, g_cfg.settleSec, mid, reg, p.x, p.y, p.z);
            }
            ProgressLine("A %d %d %u ok", r.pass, r.stage, r.list[r.stage].key);
            BuildTargets();
            if (r.st == St::Off) return;
            r.pt = -1;
            SetSt(St::NextPoint);
            return;
        case St::NextPoint:
            for (++r.pt; r.pt < (int)r.targets.size(); ++r.pt)
                if (!r.prog.donePts.count({r.pass, r.stage, r.pt})) break;
            if (r.pt >= (int)r.targets.size()) {
                ProgressLine("S %d %d", r.pass, r.stage);
                SetSt(St::NextStage);
                return;
            }
            if (r.pt == 0) {  // the arrival spot: no warp
                BeginDwell();
                return;
            }
            if (!StableField()) return;
            if (NoWarpMode()) {
                TLOG("Tour: stage %d point %d skipped - the game is in no-warp mode ([charMgr]+27A0h)", r.stage + 1, r.pt + 1);
                ProgressLine("D %d %d %d", r.pass, r.stage, r.pt);
                return;  // next tick tries the next point
            }
            if (!PostWarp(r.targets[r.pt])) {
                if (r.st == St::Off) return;
                TLOG("Tour: stage %d point %d skipped - FillWarpPoint refused id %u", r.stage + 1, r.pt + 1, r.targets[r.pt].warpId);
                ProgressLine("D %d %d %d", r.pass, r.stage, r.pt);
                return;
            }
            r.tReq = now;
            r.sawWarp = false;
            SetSt(St::Warping);
            return;
        case St::Warping: {
            const int fs = FtState();
            if (fs > 0) r.sawWarp = true;
            if (r.sawWarp && fs == 0) {
                r.warpS = (double)(now - r.tReq) * g_qpcMs / 1000.0;
                SetSt(St::WarpSettle);
                return;
            }
            if (!r.sawWarp && inState > 5) {
                TLOG("Tour: stage %d point %d - the fast travel did not start (refused by the game: no-warp / player state), skipped",
                     r.stage + 1, r.pt + 1);
                ProgressLine("D %d %d %d", r.pass, r.stage, r.pt);
                SetSt(St::NextPoint);
                return;
            }
            if (inState > g_cfg.warpTimeoutSec) {
                TLOG("Tour: STALL stage %d point %d - fast travel still in state %d after %d s", r.stage + 1, r.pt + 1, fs,
                     g_cfg.warpTimeoutSec);
                ProgressLine("D %d %d %d", r.pass, r.stage, r.pt);
                SetSt(St::NextPoint);
            }
            return;
        }
        case St::WarpSettle:
            if (!StableField()) {
                r.tState = now;
                return;
            }
            if (inState < 1.0) return;
            BeginDwell();
            return;
        case St::Dwell: {
            if (r.tFrame) r.frames.push_back((double)(now - r.tFrame) * g_qpcMs);
            r.tFrame = now;
            if (!r.interrupted && (LoadByte() != 0 || !Player())) {
                r.interrupted = true;  // death / script load while dwelling
                TLOG("Tour: stage %d point %d interrupted (player gone or a load started: death or a script event)", r.stage + 1,
                     r.pt + 1);
            }
            if ((double)(now - r.tDwell0) * g_qpcMs < g_cfg.dwellSec * 1000.0 && !r.interrupted) return;
            EndDwell();
            SetSt(St::NextPoint);
            return;
        }
        default: return;
    }
}

extern "C" void TourTick() {
    if (InterlockedCompareExchange(&g_tickBusy, 1, 0) != 0) return;  // never re-entered
    __try {
        Tick();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_run.st = St::Off;
        TLOG("Tour: exception 0x%08lX in the tick - DISARMED", GetExceptionCode());
    }
    InterlockedExchange(&g_stageId, g_run.stage);
    InterlockedExchange(&g_ptId, g_run.pt);
    InterlockedExchange(&g_tickBusy, 0);
}

// ---- worker ----------------------------------------------------------------------------------------------------------------
void LogPoint(const PointRec& r) {
    PROCESS_MEMORY_COUNTERS_EX pm{};
    pm.cb = sizeof(pm);
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pm, sizeof(pm));
    DWORD handles = 0;
    GetProcessHandleCount(GetCurrentProcess(), &handles);
    int threads = 0;
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (sn != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te{};
        te.dwSize = sizeof(te);
        const DWORD me = GetCurrentProcessId();
        for (BOOL ok = Thread32First(sn, &te); ok; ok = Thread32Next(sn, &te)) threads += te.th32OwnerProcessID == me;
        CloseHandle(sn);
    }
    PERFORMANCE_INFORMATION pi{};
    pi.cb = sizeof(pi);
    GetPerformanceInfo(&pi, sizeof(pi));
    const double page = (double)pi.PageSize / (1024.0 * 1024.0);
    double lu = -1, lb = -1, nu = -1, nb = -1;
    memstats::Vram(&lu, &lb, &nu, &nb);
    TLOG("Tour P pass=%d stage=%d/%d key=%u pt=%d/%d src=%s warp=%u pos=%.0f,%.0f,%.0f mission=%u region=%d dwell=%.1f frames=%d "
         "fps=%.1f p50=%.2f p99=%.2f max=%.2f hitch=%d cpu=%.2f main=%.0f private=%.0f ws=%.0f handles=%lu threads=%d "
         "syscommit=%.0f/%.0f vram=%.0f/%.0f nonlocal=%.0f load=%.1f warpS=%.1f int=%d t0=%lld t1=%lld label=%s",
         r.pass, r.stage, r.nStages, r.key, r.pt, r.nPts, r.src.c_str(), r.warpId, r.pos.x, r.pos.y, r.pos.z, r.missionNow,
         r.region, r.dwellS, r.fs.frames, r.fs.avgFps, r.fs.p50, r.fs.p99, r.fs.maxMs, r.fs.over33, r.cpuCores, r.mainBusy,
         pm.PrivateUsage / 1048576.0, pm.WorkingSetSize / 1048576.0, handles, threads, pi.CommitTotal * page,
         pi.CommitLimit * page, lu, lb, nu, r.loadS, r.warpS, r.interrupted ? 1 : 0, r.t0ms, r.t1ms,
         r.label.empty() ? "-" : r.label.c_str());
    char tag[64];
    snprintf(tag, sizeof(tag), "tour %d/%d/%d", r.pass, r.stage, r.pt);
    gp::LogWindow(tag);
}

DWORD WINAPI Worker(void*) {
    int64_t lastHb = Qpc();
    LONG64 lastTicks = 0;
    for (;;) {
        WaitForSingleObject(g_qEvent, 1000);
        for (;;) {
            Task t;
            EnterCriticalSection(&g_qLock);
            const bool have = !g_q.empty();
            if (have) {
                t = std::move(g_q.front());
                g_q.pop_front();
            }
            LeaveCriticalSection(&g_qLock);
            if (!have) break;
            if (t.kind == 1) gp::LogWindow(nullptr);  // discard what GP gathered between dwells
            else if (t.kind == 2) LogPoint(t.rec);
            else if (t.kind == 3) TLOG("%s", t.text.c_str());
        }
        if (SinceMs(lastHb) >= 30000.0) {  // heartbeat: the wrapper's stall watchdog reads it
            lastHb = Qpc();
            const LONG64 tk = g_ticks;
            TLOG("Tour: hb state=%s stage=%ld pt=%ld ticks=%lld (+%lld) saves skipped user=%ld backup=%ld guard=%ld",
                 StName((St)g_stateId), (long)g_stageId + 1, (long)g_ptId + 1, (long long)tk, (long long)(tk - lastTicks),
                 *g_sb1Count, *g_sb2Count, (long)g_guardDenied);
            lastTicks = tk;
        }
    }
}

// ---- install ---------------------------------------------------------------------------------------------------------------
std::wstring DirOf(const wchar_t* p) {
    std::wstring s(p ? p : L"");
    const size_t k = s.find_last_of(L"\\/");
    return k == std::wstring::npos ? std::wstring(L".") : s.substr(0, k);
}
const char* kDefaultList =
    "# built-in TourList (notes\\tour-mode.md): 15 main missions, every area group\n"
    "7356   # Escape from Hitokoto Slope (75)\n"
    "52     # Crucible Manifested (150)\n"
    "7292   # Mirage Fortress (150)\n"
    "5631   # Tiger's Cause (150)\n"
    "50990  # Sacred Mountain Defiled (142)\n"
    "12009  # Black Feather Sanctuary (177)\n"
    "60021  # The Oni of Rajomon (177)\n"
    "31541  # Frozen City, Blazing Tree (177)\n"
    "13587  # The Spirit Tree Village (216)\n"
    "42164  # Light of Hope (57)\n"
    "19854  # The Sweet Sound of Strings (224)\n"
    "7658   # Scion of the Gods (224)\n"
    "45645  # A Sinister End (89)\n"
    "4953   # A Fortress in Ruins (53)\n"
    "46078  # Mountain Keep in the Storm's Eye (53)\n";

// Thunk page layout: +0 tick thunk, +100h SB1 thunk, +180h SB2/SB3 thunk, +400h data (block byte, two counters).
// Targets are absolute so tests\tour_harness.cpp can build the same bytes around its own fake callees.
struct Thunks {
    uint8_t *tick, *sb1, *sb2;
};
Thunks BuildThunks(uint8_t* page, uint64_t tickCallee, uint64_t sb1Callee, uint64_t backupCallee, void (*tickFn)()) {
    uint8_t* t = page;
    uint8_t* data = t + 0x400;
    g_blockOn = data + 0;
    g_sb1Count = (volatile long*)(data + 8);
    g_sb2Count = (volatile long*)(data + 16);
    *g_blockOn = 1;

    // SB1 thunk (5B6F37 -> SaveFileWriteAtomic(dir, name, buf, size, DWORD* err)): skip -> *err = 0, al = 1
    uint8_t* s1 = t + 0x100;
    {
        uint8_t* p = s1;
        p[0] = 0x80; p[1] = 0x3D; Put32(p + 2, RipDisp(p + 7, g_blockOn)); p[6] = 0x00;   // cmp byte [block], 0
        p[7] = 0x74; p[8] = 0x1D;                                                         // je orig (+38)
        p[9] = 0xF0; p[10] = 0xFF; p[11] = 0x05; Put32(p + 12, RipDisp(p + 16, g_sb1Count));  // lock inc dword [count]
        const uint8_t tail[] = {0x48, 0x8B, 0x44, 0x24, 0x28,        // mov rax, [rsp+28h]   (5th arg: DWORD* err)
                                0x48, 0x85, 0xC0,                    // test rax, rax
                                0x74, 0x06,                          // je +6
                                0xC7, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov dword [rax], 0
                                0xB8, 0x01, 0x00, 0x00, 0x00,        // mov eax, 1
                                0xC3,                                // ret
                                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00}; // orig: jmp [rip+0]
        memcpy(p + 16, tail, sizeof(tail));
        Put64(p + 16 + sizeof(tail), sb1Callee);
    }
    // SB2/SB3 thunk (-> BackupBinWrite(name, ?, buf, size)): skip -> al = 1
    uint8_t* s2 = t + 0x180;
    {
        uint8_t* p = s2;
        p[0] = 0x80; p[1] = 0x3D; Put32(p + 2, RipDisp(p + 7, g_blockOn)); p[6] = 0x00;
        p[7] = 0x74; p[8] = 0x0D;                                                          // je orig (+22)
        p[9] = 0xF0; p[10] = 0xFF; p[11] = 0x05; Put32(p + 12, RipDisp(p + 16, g_sb2Count));
        const uint8_t tail[] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3, 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
        memcpy(p + 16, tail, sizeof(tail));
        Put64(p + 16 + sizeof(tail), backupCallee);
    }
    // tick thunk (56529C -> FastTravelSeqTickCall 3B29E4): save rcx rdx r8-r11 rax xmm0-5, call TourTick, restore, jmp original
    {
        uint8_t* p = t;
        size_t o = 0;
        const uint8_t pre[] = {0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, 0x50,  // push rcx..rax (7)
                               0x48, 0x81, 0xEC, 0x80, 0x00, 0x00, 0x00};                         // sub rsp, 80h
        memcpy(p + o, pre, sizeof(pre));
        o += sizeof(pre);
        const uint8_t modrm[6] = {0x44, 0x4C, 0x54, 0x5C, 0x64, 0x6C};
        for (int i = 0; i < 6; ++i) {  // movdqu [rsp+20h+i*10h], xmm_i
            const uint8_t m[] = {0xF3, 0x0F, 0x7F, modrm[i], 0x24, (uint8_t)(0x20 + i * 0x10)};
            memcpy(p + o, m, 6);
            o += 6;
        }
        p[o++] = 0x48; p[o++] = 0xB8; Put64(p + o, (uint64_t)(uintptr_t)tickFn); o += 8;  // mov rax, TourTick
        p[o++] = 0xFF; p[o++] = 0xD0;                                                         // call rax
        for (int i = 0; i < 6; ++i) {  // movdqu xmm_i, [rsp+20h+i*10h]
            const uint8_t m[] = {0xF3, 0x0F, 0x6F, modrm[i], 0x24, (uint8_t)(0x20 + i * 0x10)};
            memcpy(p + o, m, 6);
            o += 6;
        }
        const uint8_t post[] = {0x48, 0x81, 0xC4, 0x80, 0x00, 0x00, 0x00,                          // add rsp, 80h
                                0x58, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59,  // pop rax..rcx
                                0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};                               // jmp [rip+0]
        memcpy(p + o, post, sizeof(post));
        o += sizeof(post);
        Put64(p + o, tickCallee);
    }
    FlushInstructionCache(GetCurrentProcess(), page, 4096);
    return Thunks{t, s1, s2};
}

}  // namespace

int Mode(const wchar_t* iniPath) {
    if (!iniPath) return 0;
    const int m = (int)GetPrivateProfileIntW(L"Debug", L"Tour", 0, iniPath);
    return m == 1 || m == 2 ? m : 0;
}
bool SavesBlocked() { return g_savesBlocked; }

bool TestBuildThunks(uint8_t* page, void* tickCallee, void* sb1Callee, void* backupCallee, void (*tickFn)(), TestThunkSet* out) {
    if (!page || !out) return false;
    const Thunks th = BuildThunks(page, (uint64_t)(uintptr_t)tickCallee, (uint64_t)(uintptr_t)sb1Callee,
                                  (uint64_t)(uintptr_t)backupCallee, tickFn);
    out->tick = th.tick;
    out->sb1 = th.sb1;
    out->sb2 = th.sb2;
    out->block = g_blockOn;
    out->sb1Count = g_sb1Count;
    out->sb2Count = g_sb2Count;
    return true;
}

void Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    if (!g_log || !info.iniPath || !info.base) return;
    g_cfg.mode = Mode(info.iniPath);
    if (!g_cfg.mode) return;
    if (!info.knownBuild) {
        TLOG("Tour: not installed (unknown build; addresses are v2.02) - saves are NOT blocked, the tour does not run");
        return;
    }
    auto I = [&](const wchar_t* k, int def, int lo, int hi) {
        const int v = (int)GetPrivateProfileIntW(L"Debug", k, def, info.iniPath);
        return v < lo ? lo : v > hi ? hi : v;
    };
    g_cfg.dwellSec = I(L"TourDwellSec", 4, 1, 1800);   // long dwells: the cost-vs-looks harness cycles SkipFx groups at each point
    g_cfg.pointsPerStage = I(L"TourPointsPerStage", 3, 1, 32);
    g_cfg.passes = I(L"TourPasses", 1, 1, 10);
    g_cfg.loadTimeoutSec = I(L"TourLoadTimeoutSec", 120, 30, 900);
    g_cfg.warpTimeoutSec = I(L"TourWarpTimeoutSec", 45, 10, 300);
    g_cfg.startDelaySec = I(L"TourStartDelaySec", 5, 1, 120);
    g_cfg.settleSec = I(L"TourSettleSec", 3, 0, 60);
    g_cfg.minPointDist = I(L"TourMinPointDist", 3000, 0, 1000000);
    g_cfg.dir = DirOf(info.iniPath);
    wchar_t lp[MAX_PATH] = {};
    GetPrivateProfileStringW(L"Debug", L"TourList", L"", lp, MAX_PATH, info.iniPath);
    std::wstring list = lp;
    if (!list.empty() && list.find(L':') == std::wstring::npos && list[0] != L'\\') list = g_cfg.dir + L"\\" + list;
    g_cfg.listPath = list;
    const std::wstring pdir = g_cfg.dir + L"\\Nioh3EngineFix_tour";
    CreateDirectoryW(pdir.c_str(), nullptr);
    g_cfg.progressPath = pdir + L"\\progress.txt";
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_qpcMs = 1000.0 / (double)f.QuadPart;

    // verify every byte we rely on before touching anything
    for (const HeadCheck& h : kHeads)
    {
        bool hooked = false;
        if (!HeadOk(h.rva, h.b, h.n, &hooked)) {
            TLOG("Tour: not installed (code at %X differs - another build or patch); saves are NOT blocked, the tour does not run",
                 h.rva);
            return;
        }
        if (hooked) TLOG("Tour: %X carries another module's entry hook (E9 rel32, e.g. Nioh3BossRematch) - verified, the tour calls through it", h.rva);
    }
    uint64_t tickTarget = 0;
    bool tickChained = false;
    if (!CallSiteTarget(kTickSite, kTickCallee, &tickTarget, &tickChained) || !CallSiteOk(kSb1Site, kSb1Callee) ||
        !CallSiteOk(kSb2Site, kSbBackupCallee) || !CallSiteOk(kSb3Site, kSbBackupCallee)) {
        TLOG("Tour: not installed (TT1 / SB1-SB3 call sites differ); saves are NOT blocked, the tour does not run");
        return;
    }
    if (tickChained) TLOG("Tour: tick site 56529C already retargeted by another module (thunk at %p) - chaining onto it", (void*)tickTarget);
    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    g_page = AllocNear(g_base, g_base + nt->OptionalHeader.SizeOfImage, 4096);
    if (!g_page) {
        TLOG("Tour: not installed (no executable page within 2 GB of the image)");
        return;
    }
    const Thunks th = BuildThunks(g_page, tickTarget, g_base + kSb1Callee, g_base + kSbBackupCallee, &TourTick);
    uint8_t* t = th.tick;
    uint8_t* s1 = th.sb1;
    uint8_t* s2 = th.sb2;

    // 1. save block first (if it fails nothing else is installed)
    if (!Retarget(kSb1Site, s1) || !Retarget(kSb2Site, s2) || !Retarget(kSb3Site, s2)) {
        *g_blockOn = 0;  // any site already retargeted now passes straight through
        TLOG("Tour: not installed (a save-writer site could not be written) - saves NOT blocked, the tour does not run");
        return;
    }
    g_savesBlocked = true;
    // 2. IAT guard (verification layer; a failed hook is logged, the SB block still holds)
    int hooked = 0;
    hooked += HookImport("CreateFileW", &HkCreateFileW, o_CreateFileW);
    hooked += HookImport("MoveFileExW", &HkMoveFileExW, o_MoveFileExW);
    hooked += HookImport("MoveFileW", &HkMoveFileW, o_MoveFileW);
    hooked += HookImport("DeleteFileW", &HkDeleteFileW, o_DeleteFileW);
    hooked += HookImport("CopyFileW", &HkCopyFileW, o_CopyFileW);
    hooked += HookImport("SetFileAttributesW", &HkSetFileAttributesW, o_SetFileAttributesW);

    // list + progress
    std::string text = g_cfg.listPath.empty() ? std::string() : ReadFileText(g_cfg.listPath);
    const bool builtIn = text.empty();
    if (builtIn) text = kDefaultList;
    int bad = 0;
    g_run.list = ParseTourList(text, &bad);
    if (g_run.list.empty()) {
        TLOG("Tour: the TourList is empty (%s) - saves blocked, the tour does not run", Narrow(g_cfg.listPath.c_str()).c_str());
        return;
    }
    const std::string prog = ReadFileText(g_cfg.progressPath);
    g_run.prog = ParseProgress(prog);
    for (const auto& c : g_run.prog.crashPts) {
        TLOG("Tour: RESUME - the previous run ended during pass %d stage %d point %d (crash or kill); that point is skipped",
             std::get<0>(c) + 1, std::get<1>(c) + 1, std::get<2>(c) + 1);
        ProgressLine("C %d %d %d", std::get<0>(c), std::get<1>(c), std::get<2>(c));
    }
    for (const auto& c : g_run.prog.crashLoads) {
        TLOG("Tour: RESUME - the previous run ended while loading pass %d stage %d; that stage is skipped", c.first + 1,
             c.second + 1);
        ProgressLine("A %d %d 0 crash", c.first, c.second);
    }
    for (const auto& c : g_run.prog.oldCrashPts)
        TLOG("Tour: earlier crash point: pass %d stage %d point %d", std::get<0>(c) + 1, std::get<1>(c) + 1, std::get<2>(c) + 1);
    if (g_run.prog.finished) {
        TLOG("Tour: progress.txt says the tour already finished - delete %s to run it again; saves blocked, nothing to do",
             Narrow(g_cfg.progressPath.c_str()).c_str());
        g_run.st = St::Done;
    }
    g_run.pass = 0;
    g_run.stage = -1;

    InitializeCriticalSection(&g_qLock);
    g_qEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    gp::SetExternalWindows(true);
    sp::SetExternalWindows(true);
    if (HANDLE h = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr)) CloseHandle(h);

    // 3. the tick last (it starts the state machine)
    if (!Retarget(kTickSite, t)) {
        TLOG("Tour: tick site 56529C could not be written - the tour does not run; saves stay BLOCKED for this session "
             "(restart without [Debug] Tour to save normally)");
        return;
    }
    TLOG("Tour: installed - mode %d (%s), %zu stages from %s%s, %d pass(es), %d points/stage, dwell %d s, load timeout %d s; "
         "SAVES BLOCKED for this whole session (SB1 5B6F37, SB2 1FDF2D0, SB3 1FDF62A; guard %d/6 imports); progress %s (%d lines). "
         "Waiting for the field (load your save; the tour arms %d s after the player stands in the field).",
         g_cfg.mode, g_cfg.mode == 1 ? "unlocked missions" : "all missions", g_run.list.size(),
         builtIn ? "the built-in list" : Narrow(g_cfg.listPath.c_str()).c_str(), bad ? " (bad lines ignored)" : "",
         g_cfg.passes, g_cfg.pointsPerStage, g_cfg.dwellSec, g_cfg.loadTimeoutSec, hooked,
         Narrow(g_cfg.progressPath.c_str()).c_str(), g_run.prog.lines, g_cfg.startDelaySec);
}

}  // namespace tour
