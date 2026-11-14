// perfsnap.cpp - performance snapshot for player and crash reports. See perfsnap.h.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dxgi1_4.h>
#include <intrin.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "perfsnap.h"
#include "perfsnap_core.h"

namespace perfsnap {
namespace {

constexpr int kFrames = 4096;        // ~17 s at 240 fps, ~68 s at 60
constexpr int kSamples = 64;         // 16 s at 4 Hz
constexpr double kWindowMs = 10000;  // "last ~10 s"

StartInfo g_si;
wchar_t g_iniPath[MAX_PATH * 2];  // #97850: owned copy; the caller's string (dllmain's local std::wstring) dies after Start
int64_t g_freq = 1;

void KeepInfo(const StartInfo& info) {
    g_si = info;
    g_iniPath[0] = 0;
    if (info.iniPath) wcsncpy_s(g_iniPath, info.iniPath, _TRUNCATE);
    g_si.iniPath = info.iniPath ? g_iniPath : nullptr;
}

// ---- per-frame ring (written only by the game's presenting thread) ----
float g_ms[kFrames];
volatile LONG g_fHead = 0;           // frames written in total
volatile LONGLONG g_lastQpc = 0;
volatile DWORD g_presTid = 0;

struct Sample {
    int64_t qpc = 0;
    LONG frames = 0;
    DWORD tid = 0;
    uint64_t thr = 0, proc = 0;      // 100 ns CPU time: presenting thread, whole process
    long long w6 = 0, gf = 0, ds = 0; // QPC ticks
    long l1 = 0;
    bool bk = false;
    double bkMs[4] = {};
};
Sample g_s[kSamples];
volatile LONG g_sHead = 0;
int64_t g_nextSample = 0;

// ---- cached hardware + settings (background thread; the crash path reads these only) ----
char g_cpu[64] = "";
int g_cores = 0;
char g_gpu[128] = "";
char g_ids[32] = "";
char g_driver[40] = "";
volatile LONGLONG g_vramTotal = 0, g_vramUsed = 0, g_vramBudget = 0, g_ramTotal = 0, g_ramUsed = 0;  // MB
volatile LONG g_bbW = 0, g_bbH = 0;
char g_gameJson[2048] = "{}";
char g_gameText[1536] = "";
volatile LONG g_fpsCap = 0;
char g_upscaler[16] = "";
char g_frameGen[16] = "";
char g_modJson[4096] = "{}";
char g_modText[2048] = "";
IDXGIAdapter3* g_adapter = nullptr;
SRWLOCK g_cacheLock = SRWLOCK_INIT;  // settings strings (never taken on the crash path; it uses TryAcquire)

uint64_t Ft(const FILETIME& f) { return (uint64_t)f.dwHighDateTime << 32 | f.dwLowDateTime; }
long long Rd(volatile long long* p) { return p ? *p : 0; }
long Rd(volatile long* p) { return p ? *p : 0; }

void Copy(char* dst, size_t cap, const char* src) { snprintf(dst, cap, "%s", src ? src : ""); }

// ---- hardware --------------------------------------------------------------------------------------------------------
void ReadCpu() {
    int r[4] = {0};
    char brand[49] = {0};
    __cpuid(r, 0x80000000);
    if ((unsigned)r[0] >= 0x80000004u)
        for (int i = 0; i < 3; i++) { __cpuid(r, 0x80000002 + i); memcpy(brand + 16 * i, r, 16); }
    const char* b = brand;
    while (*b == ' ') ++b;
    Copy(g_cpu, sizeof g_cpu, *b ? b : "unknown");
    for (size_t n = strlen(g_cpu); n && g_cpu[n - 1] == ' ';) g_cpu[--n] = 0;
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    g_cores = (int)si.dwNumberOfProcessors;
}

void ReadRam() {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms)) {
        g_ramTotal = (LONGLONG)(ms.ullTotalPhys >> 20);
        g_ramUsed = (LONGLONG)((ms.ullTotalPhys - ms.ullAvailPhys) >> 20);
    }
}

// the system dxgi.dll (skips OptiScaler / Streamline proxies), hardware adapter with the most dedicated VRAM (= the game's GPU on
// every machine we ship to; same rule as hitchlog / crashreport)
void OpenAdapter() {
    wchar_t path[MAX_PATH];
    const UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n + 10 >= MAX_PATH) return;
    wcscat_s(path, L"\\dxgi.dll");
    HMODULE m = GetModuleHandleW(path);
    if (!m) m = LoadLibraryW(path);
    using CF = HRESULT(WINAPI*)(REFIID, void**);
    CF create = m ? (CF)GetProcAddress(m, "CreateDXGIFactory1") : nullptr;
    IDXGIFactory1* f = nullptr;
    if (!create || FAILED(create(__uuidof(IDXGIFactory1), (void**)&f)) || !f) return;
    IDXGIAdapter1* best = nullptr;
    SIZE_T bestMem = 0;
    DXGI_ADAPTER_DESC1 bestD{};
    IDXGIAdapter1* ad = nullptr;
    for (UINT i = 0; i < 8 && f->EnumAdapters1(i, &ad) == S_OK; ++i) {
        DXGI_ADAPTER_DESC1 d{};
        ad->GetDesc1(&d);
        if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && d.DedicatedVideoMemory >= bestMem) {
            if (best) best->Release();
            best = ad, bestMem = d.DedicatedVideoMemory, bestD = d;
        } else {
            ad->Release();
        }
    }
    f->Release();
    if (!best) return;
    char name[128];
    WideCharToMultiByte(CP_UTF8, 0, bestD.Description, -1, name, sizeof name, nullptr, nullptr);
    Copy(g_gpu, sizeof g_gpu, name);
    snprintf(g_ids, sizeof g_ids, "%04X:%04X", bestD.VendorId, bestD.DeviceId);
    LARGE_INTEGER drv{};
    if (SUCCEEDED(best->CheckInterfaceSupport(__uuidof(IDXGIDevice), &drv)))
        snprintf(g_driver, sizeof g_driver, "%u.%u.%u.%u", HIWORD(drv.HighPart), LOWORD(drv.HighPart), HIWORD(drv.LowPart),
                 LOWORD(drv.LowPart));
    g_vramTotal = (LONGLONG)(bestD.DedicatedVideoMemory >> 20);
    IDXGIAdapter3* a3 = nullptr;
    if (SUCCEEDED(best->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&a3))) g_adapter = a3;
    best->Release();
}

void ReadVram() {
    if (!g_adapter) return;
    DXGI_QUERY_VIDEO_MEMORY_INFO mi{};
    if (SUCCEEDED(g_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &mi))) {
        g_vramUsed = (LONGLONG)(mi.CurrentUsage >> 20);
        g_vramBudget = (LONGLONG)(mi.Budget >> 20);
    }
}

// ---- the game's settings table (Nioh 3 v2.02; notes\settings.md section 1) ---------------------------------------------
constexpr uint32_t kRvaDesc = 0x45C8570;   // 60 x 0x30: +0 name*, +8 values**, +0x24 value count
constexpr uint32_t kRvaLive = 0x45BAF30;   // int[60]: enum index per key
constexpr int kKeys = 60;
const char* const kWanted[] = {"DISP_MODE", "VSYNC", "SCR_REZ", "REN_RAT", "REN_FPS", "DYNAMIC_FPS", "PRESET", "SHADOW_QUAL",
                               "AMBIENT_OCCLUSION", "GI", "SSR", "MODEL_QUAL", "MODEL_DRAW", "VOLUMETRIC", "VISUAL_EFFECT",
                               "MOTION_QUALITY", "LANDSCAPE_QUALITY", "MESHLET_QUALITY", "GRASS_DENSITY",
                               "POSTEFFECT_RESOLUTION", "DYNAMIC_RESOLUTION", "RESOLUTION_SCALING", "FRAME_GENERATION",
                               "DLSS_FRAME_GEN_MULI", "CPU_SPEC_CLASS"};

bool InImage(uintptr_t p, uintptr_t lo, uintptr_t hi) { return p >= lo && p + 8 <= hi; }
bool SafeName(const char* s) {  // short printable ASCII inside the image
    for (int i = 0; i < 48; ++i) {
        if (!s[i]) return i > 0;
        if (s[i] < 0x20 || s[i] > 0x7E) return false;
    }
    return false;
}

void DecodeSettings(uintptr_t base, uintptr_t end, Buf& j, Buf& t, int& fpsCap, char* ups, char* fg) {
    j.raw("{");
    int w = 0;
    for (int i = 0; i < kKeys; ++i) {
        const uintptr_t e = base + kRvaDesc + (uintptr_t)i * 0x30;
        const uintptr_t namep = *(const uintptr_t*)e;
        if (!InImage(namep, base, end) || !SafeName((const char*)namep)) continue;
        const char* name = (const char*)namep;
        bool want = false;
        for (const char* k : kWanted) want = want || !strcmp(k, name);
        if (!want) continue;
        const int idx = *(const int*)(base + kRvaLive + 4u * (unsigned)i);
        const uintptr_t vals = *(const uintptr_t*)(e + 8);
        const int count = *(const int*)(e + 0x24);
        char v[48];
        snprintf(v, sizeof v, "%d", idx);
        if (InImage(vals, base, end) && count > 0 && count < 256 && idx >= 0 && idx < count) {
            const uintptr_t sp = ((const uintptr_t*)vals)[idx];
            if (InImage(sp, base, end) && SafeName((const char*)sp)) Copy(v, sizeof v, (const char*)sp);
        }
        if (w++) j.raw(",");
        j.str(name, 40);
        j.raw(":");
        j.str(v, 40);
        t.f("%s=%s ", name, v);
        if (!strcmp(name, "REN_FPS")) fpsCap = atoi(v);
        if (!strcmp(name, "RESOLUTION_SCALING")) Copy(ups, 16, v);
        if (!strcmp(name, "FRAME_GENERATION")) Copy(fg, 16, v);
    }
    j.raw("}");
}

int DecodeSettingsSeh(uintptr_t base, uintptr_t end, Buf& j, Buf& t, int& fpsCap, char* ups, char* fg) {
    __try {
        DecodeSettings(base, end, j, t, fpsCap, ups, fg);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// #98375 hash-free gate: another exe of the same game version (Steam DRM wrapper stripped, another store's packaging)
// keeps the table at the same place but fails the TimeDateStamp + SizeOfImage match. Check the table itself instead:
// at least 20 of the 25 wanted key names must sit at kRvaDesc with in-range live indexes. Read-only, SEH-guarded.
int CountTableKeys(uintptr_t base, uintptr_t end) {
    int hits = 0;
    for (int i = 0; i < kKeys; ++i) {
        const uintptr_t e = base + kRvaDesc + (uintptr_t)i * 0x30;
        if (!InImage(e + 0x28, base, end) || !InImage(base + kRvaLive + 4u * (unsigned)i, base, end)) return 0;
        const uintptr_t namep = *(const uintptr_t*)e;
        if (!InImage(namep, base, end) || !SafeName((const char*)namep)) continue;
        bool want = false;
        for (const char* k : kWanted) want = want || !strcmp(k, (const char*)namep);
        const int idx = *(const int*)(base + kRvaLive + 4u * (unsigned)i), count = *(const int*)(e + 0x24);
        if (want && count > 0 && count < 256 && idx >= 0 && idx < count) ++hits;
    }
    return hits;
}
bool TableLooksRight(uintptr_t base, uintptr_t end) {
    __try {
        return CountTableKeys(base, end) >= 20;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool g_layoutOk = false;  // unknown build: table verified at the v2.02 place (a miss is re-checked; the table may fill late)

void RefreshGame() {
    if (!g_si.base) return;
    const auto* dos = (const IMAGE_DOS_HEADER*)g_si.base;
    const auto* nt = (const IMAGE_NT_HEADERS*)(g_si.base + dos->e_lfanew);
    const uintptr_t end = g_si.base + nt->OptionalHeader.SizeOfImage;
    if (!g_si.knownBuild) {
        if (!g_layoutOk) g_layoutOk = TableLooksRight(g_si.base, end);
        if (!g_layoutOk) return;
    }
    char j[sizeof g_gameJson], t[sizeof g_gameText];  // stack: the worker and a report thread may both refresh
    Buf bj(j, sizeof j), bt(t, sizeof t);
    int cap = 0;
    char ups[16] = "", fg[16] = "";
    if (!DecodeSettingsSeh(g_si.base, end, bj, bt, cap, ups, fg) || bj.over) return;  // cut JSON is never published
    AcquireSRWLockExclusive(&g_cacheLock);
    Copy(g_gameJson, sizeof g_gameJson, j);
    Copy(g_gameText, sizeof g_gameText, t);
    Copy(g_upscaler, sizeof g_upscaler, ups);
    Copy(g_frameGen, sizeof g_frameGen, fg);
    g_fpsCap = cap;
    ReleaseSRWLockExclusive(&g_cacheLock);
}

// ---- the mod's performance settings ----------------------------------------------------------------------------------
void Trim(wchar_t*& s, wchar_t* e) {
    while (*s == L' ' || *s == L'\t') ++s;
    while (e > s && (e[-1] == L' ' || e[-1] == L'\t')) *--e = 0;
}

void RefreshMod() {
    if (!g_si.iniPath) return;
    char j[sizeof g_modJson], t[sizeof g_modText];  // stack (worker / report thread; never the crash path)
    Buf bj(j, sizeof j), bt(t, sizeof t);
    wchar_t sec[8192];
    const wchar_t* const kSections[] = {L"Perf", L"ExtraPerf", L"VRR", L"Latency", L"Tuning", L"GpuOpt", L"Memory", L"Overlay", L"General", L"Patches"};  // #97775: the real ini has no [Perf]/[ExtraPerf]
    bj.raw("{");
    int ws = 0;
    for (const wchar_t* name : kSections) {
        const bool patches = !wcscmp(name, L"Patches");
        const DWORD n = GetPrivateProfileSectionW(name, sec, (DWORD)(sizeof sec / sizeof sec[0]), g_si.iniPath);
        if (!n) continue;
        char sn[32];
        WideCharToMultiByte(CP_UTF8, 0, patches ? L"patches_off" : name, -1, sn, sizeof sn, nullptr, nullptr);
        if (ws++) bj.raw(",");
        bj.str(sn, 30);
        bj.raw(patches ? ":[" : ":{");
        bt.f("%s:", sn);
        int wk = 0;
        for (wchar_t* line = sec; *line; line += wcslen(line) + 1) {
            wchar_t buf[512];
            wcsncpy_s(buf, line, _TRUNCATE);
            wchar_t* k = buf;
            Trim(k, k + wcslen(k));
            if (!*k || *k == L';' || *k == L'#') continue;
            wchar_t* eq = wcschr(k, L'=');
            if (!eq) continue;
            *eq = 0;
            wchar_t* v = eq + 1;
            Trim(k, eq);
            Trim(v, v + wcslen(v));
            if (wchar_t* c = wcschr(v, L';')) { *c = 0; Trim(v, c); }
            char ka[64], va[96];
            WideCharToMultiByte(CP_UTF8, 0, k, -1, ka, sizeof ka, nullptr, nullptr);
            WideCharToMultiByte(CP_UTF8, 0, v, -1, va, sizeof va, nullptr, nullptr);
            if (wk >= 40) continue;  // site limit: 48 keys / 64 items per level
            if (patches) {  // only the patches the player turned off
                if (strcmp(va, "0")) continue;
                if (wk++) bj.raw(",");
                bj.str(ka, 60);
                bt.f(" %s", ka);
                continue;
            }
            if (wk++) bj.raw(",");
            bj.str(ka, 60);
            bj.raw(":");
            bj.str(va, 90);
            bt.f(" %s=%s", ka, va);
        }
        if (patches && !wk) bt.raw(" (none)");
        bj.raw(patches ? "]" : "}");
        bt.raw("\r\n    ");
    }
    bj.raw("}");
    if (bj.over) Copy(j, sizeof j, "{\"cut\":true}");  // cut JSON is never published
    AcquireSRWLockExclusive(&g_cacheLock);
    Copy(g_modJson, sizeof g_modJson, j);
    Copy(g_modText, sizeof g_modText, t);
    ReleaseSRWLockExclusive(&g_cacheLock);
}

DWORD WINAPI Worker(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    Sleep(20000);  // well after the game made its device; never from the Present hook
    OpenAdapter();
    ReadRam();
    ReadVram();
    RefreshGame();
    RefreshMod();
    LONG seen = g_fHead;
    for (int tick = 1;; ++tick) {
        Sleep(5000);
        const LONG now = g_fHead;
        if (now == seen) continue;  // no frames: nothing to refresh
        seen = now;
        ReadRam();
        ReadVram();
        RefreshGame();
        if (tick % 6 == 0) RefreshMod();  // the ini changes only from the menu: every 30 s is enough
    }
}

// ---- per-thread CPU over a rolling 30 s (human #97659, TASK #97673) -------------------------------------------------------
// Own low-priority thread, one tick per second: GetThreadTimes on every thread of the game process (handles kept open; the
// thread list and names are refreshed every 5 s from a Toolhelp snapshot). Nothing runs per frame. The ranked top 5 is
// published into static storage under g_thrLock; the crash path only TryAcquires it.
constexpr int kThr = 256;          // thread slots (the game runs ~100-150)
constexpr int kHist = 32;          // ticks of history per slot (> the 30 s window)
constexpr int kThrWindow = 30;     // seconds
constexpr int kTop = 5;
struct ThrSlot {
    DWORD tid = 0;
    HANDLE h = nullptr;
    int first = -1;                // tick of the first sample
    bool seen = false;             // still in the latest Toolhelp list
    uint64_t t[kHist] = {};        // kernel + user, 100 ns
    bool ok[kHist] = {};           // this tick's GetThreadTimes succeeded (false = hole, never used for a delta)
    bool presMarked = false;       // the name already carries "present"
    bool described = false;        // base came from GetThreadDescription (else the start module, which never changes)
    char base[48] = "";            // name without the "present" tag
    char name[48] = "";
};
ThrSlot g_thr[kThr];
int64_t g_thrQ[kHist];             // QPC per tick
int g_thrTick = 0;                 // ticks done
using GetDescFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
using NtQitFn = LONG(WINAPI*)(HANDLE, int, PVOID, ULONG, PULONG);
GetDescFn g_getDesc = nullptr;
NtQitFn g_ntQit = nullptr;
DWORD g_thrSelfTid = 0;
// published result (sampler thread writes; reports and the crash path read)
SRWLOCK g_thrLock = SRWLOCK_INIT;
ThreadUse g_thrTop[kTop + 1];  // top 5 + the presenting thread when it is not among them (#97775)
int g_thrTopN = 0, g_thrCount = 0;
double g_thrWinS = 0;

// #97817: called on every enumerate (5 s). The description is re-read each time (a thread named after it started, or renamed,
// is picked up); the start-module fallback is computed only while there is no description. The "present" tag follows the
// presenting TID: added when this thread becomes it, dropped when another thread does.
void ThreadName(ThrSlot& s) {
    char d8[48] = "";
    if (g_getDesc) {
        PWSTR d = nullptr;
        if (SUCCEEDED(g_getDesc(s.h, &d)) && d) {
            if (d[0]) WideCharToMultiByte(CP_UTF8, 0, d, -1, d8, sizeof d8 - 1, nullptr, nullptr);
            LocalFree(d);
        }
    }
    bool dirty = !s.name[0];
    if (d8[0]) {
        if (!s.described || strcmp(s.base, d8)) Copy(s.base, sizeof s.base, d8), s.described = true, dirty = true;
    } else if (s.described || !s.base[0]) {  // no description (or it was cleared): the module its start address is in
        s.described = false, s.base[0] = 0, dirty = true;
        PVOID start = nullptr;
        if (g_ntQit && g_ntQit(s.h, 9 /*ThreadQuerySetWin32StartAddress*/, &start, sizeof start, nullptr) == 0 && start) {
            HMODULE m = nullptr;
            wchar_t path[MAX_PATH];
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCWSTR)start, &m) && m && GetModuleFileNameW(m, path, MAX_PATH)) {
                const wchar_t* b = wcsrchr(path, L'\\');
                char mod[32] = "";
                WideCharToMultiByte(CP_UTF8, 0, b ? b + 1 : path, -1, mod, sizeof mod - 1, nullptr, nullptr);
                snprintf(s.base, sizeof s.base, "%s+0x%llx", mod, (unsigned long long)((uintptr_t)start - (uintptr_t)m));
            }
        }
    }
    const bool pres = g_presTid && s.tid == g_presTid && s.tid != g_thrSelfTid;
    if (pres != s.presMarked) dirty = true;
    if (!dirty) return;
    s.presMarked = pres;
    char n[48] = "";
    if (s.tid == g_thrSelfTid) Copy(n, sizeof n, "mod perf snapshot");
    else if (pres) snprintf(n, sizeof n, "%s%spresent", s.base, s.base[0] ? " / " : "");
    else Copy(n, sizeof n, s.base);
    if (!n[0]) snprintf(n, sizeof n, "thread %lu", s.tid);
    Copy(s.name, sizeof s.name, n);
}

void ThreadEnumerate() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    for (ThrSlot& s : g_thr) s.seen = false;
    const DWORD pid = GetCurrentProcessId();
    THREADENTRY32 te{};
    te.dwSize = sizeof te;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid) continue;
        ThrSlot* hit = nullptr;
        ThrSlot* freeS = nullptr;
        for (ThrSlot& s : g_thr) {
            if (s.h && s.tid == te.th32ThreadID) { hit = &s; break; }
            if (!s.h && !freeS) freeS = &s;
        }
        if (!hit && freeS) {
            HANDLE h = OpenThread(THREAD_QUERY_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
            if (!h) h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
            if (h) {
                hit = freeS;
                *hit = ThrSlot{};
                hit->tid = te.th32ThreadID, hit->h = h;
            }
        }
        if (hit) {
            hit->seen = true;
            ThreadName(*hit);  // cached per TID (#97734); refreshes only a changed description or "present" tag (#97817)
        }
    }
    CloseHandle(snap);
    for (ThrSlot& s : g_thr)
        if (s.h && !s.seen) { CloseHandle(s.h); s = ThrSlot{}; }  // gone
}

void ThreadTick(int64_t qpc) {
    const int tick = g_thrTick;
    if (tick % 5 == 0) ThreadEnumerate();
    g_thrQ[tick % kHist] = qpc;
    static ThreadUse use[kThr];  // only the sampler thread (or the test) runs this
    int n = 0, live = 0;
    double winS = 0;
    for (ThrSlot& s : g_thr) {
        if (!s.h) continue;
        FILETIME c, e, k, u;
        if (!GetThreadTimes(s.h, &c, &e, &k, &u)) {  // #97734: invalid sample, skipped (no stale ring value enters a delta)
            s.ok[tick % kHist] = false;
            continue;
        }
        ++live;
        s.t[tick % kHist] = Ft(k) + Ft(u);
        s.ok[tick % kHist] = true;
        if (s.first < 0) s.first = tick;
        int old = 0;
        uint64_t d = 0;
        if (!WindowDelta(s.t, s.ok, kHist, tick, s.first, kThrWindow, old, d)) continue;
        const double wall = (double)(qpc - g_thrQ[old % kHist]) / (double)g_freq;
        ThreadUse& x = use[n++];
        x.tid = s.tid;
        x.pct = CorePct(d, wall);
        Copy(x.name, sizeof x.name, s.name);
        if (wall > winS) winS = wall;
    }
    ThreadUse top[kTop + 1];
    int tn = RankThreads(use, n, top, kTop);
    if (g_presTid) {  // #97775: the presenting thread is always listed (tag "present"), as a 6th entry when it is outside the top 5
        bool in = false;
        for (int i = 0; i < tn; ++i) in = in || top[i].tid == g_presTid;
        if (!in)
            for (int i = 0; i < n; ++i)
                if (use[i].tid == g_presTid) { top[tn++] = use[i]; break; }
    }
    AcquireSRWLockExclusive(&g_thrLock);
    for (int i = 0; i < tn; ++i) g_thrTop[i] = top[i];
    g_thrTopN = tn, g_thrCount = live, g_thrWinS = winS;
    ReleaseSRWLockExclusive(&g_thrLock);
    g_thrTick = tick + 1;
}

void ThreadInit() {
    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    g_getDesc = kb ? reinterpret_cast<GetDescFn>(GetProcAddress(kb, "GetThreadDescription")) : nullptr;
    if (!g_getDesc && k32) g_getDesc = reinterpret_cast<GetDescFn>(GetProcAddress(k32, "GetThreadDescription"));
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    g_ntQit = nt ? reinterpret_cast<NtQitFn>(GetProcAddress(nt, "NtQueryInformationThread")) : nullptr;
}

DWORD WINAPI ThreadWorker(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    g_thrSelfTid = GetCurrentThreadId();
    using SetDescFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    if (auto sd = reinterpret_cast<SetDescFn>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription")))
        sd(GetCurrentThread(), L"Nioh3EF perfsnap threads");
    Sleep(20000);  // same start delay as the hardware worker
    for (;;) {
        LARGE_INTEGER q;
        QueryPerformanceCounter(&q);
        ThreadTick(q.QuadPart);
        Sleep(1000);
    }
}

struct ThreadView {
    ThreadUse top[kTop + 1];
    int n = 0, count = 0;
    double winS = 0;
};
bool ReadThreads(ThreadView& v, bool crashPath) {
    if (crashPath) {
        if (!TryAcquireSRWLockShared(&g_thrLock)) return false;
    } else {
        AcquireSRWLockShared(&g_thrLock);
    }
    for (int i = 0; i < g_thrTopN; ++i) v.top[i] = g_thrTop[i];
    v.n = g_thrTopN, v.count = g_thrCount, v.winS = g_thrWinS;
    ReleaseSRWLockShared(&g_thrLock);
    return v.n > 0;
}

// ---- snapshot --------------------------------------------------------------------------------------------------------
struct Snap {
    FrameStats f;
    WindowCost w;
    bool haveWindow = false;
    int bound = kUnknown;
    char why[200] = "";
    double ageS = -1;  // seconds since the newest frame
    int fpsCap = 0;
    char gpuPasses[256] = "";
};

void Build(Snap& s, bool crashPath) {
    // frames, chronological. Static copies (32 KB; the crash writer's stack is 256 KB) behind a busy flag: a report and a
    // crash at the same moment wait up to 200 ms, then the later one goes without the frame summary.
    static float ms[kFrames], scratch[kFrames];
    static volatile LONG busy = 0;
    bool got = false;
    for (int i = 0; i < 200 && !(got = InterlockedCompareExchange(&busy, 1, 0) == 0); ++i) Sleep(1);
    if (got) {
        const LONG head = g_fHead;
        const int n = head < kFrames ? (int)head : kFrames;
        for (int i = 0; i < n; ++i) ms[i] = g_ms[(head - n + i) % kFrames];
        s.f = Summarize(ms, n, kWindowMs, scratch);
        InterlockedExchange(&busy, 0);
    }
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    if (g_lastQpc) s.ageS = (double)(q.QuadPart - g_lastQpc) / (double)g_freq;
    // counter window: newest sample vs the oldest one within 10 s
    const LONG sh = g_sHead;
    if (sh >= 2) {
        const Sample b = g_s[(sh - 1) % kSamples];
        Sample a = g_s[(sh - 2) % kSamples];
        const int back = sh < kSamples ? (int)sh : kSamples - 1;
        for (int k = 2; k <= back; ++k) {
            const Sample& c = g_s[(sh - k) % kSamples];
            if ((double)(b.qpc - c.qpc) / (double)g_freq > kWindowMs / 1000.0 + 0.01) break;
            a = c;
        }
        const double spanS = (double)(b.qpc - a.qpc) / (double)g_freq;
        const double fr = (double)(b.frames - a.frames);
        if (spanS > 0 && fr > 0) {
            const double tk = 1000.0 / (double)g_freq / fr;  // QPC ticks -> ms per frame
            s.w.spanS = spanS;
            s.w.frames = fr;
            s.w.gameThreadMs = a.tid == b.tid ? (double)(b.thr - a.thr) / 1e4 / fr : -1;
            s.w.procCores = (double)(b.proc - a.proc) / 1e7 / spanS;
            s.w.gpuFenceMs = (double)(b.gf - a.gf) * tk;
            s.w.presentWaitMs = (double)(b.w6 - a.w6) * tk;
            s.w.dsRetryMs = (double)(b.ds - a.ds) * tk;
            s.w.limiterPerFrame = (double)(b.l1 - a.l1) / fr;
            s.w.bk = a.bk && b.bk;
            for (int i = 0; i < 4; ++i) s.w.bkMs[i] = s.w.bk ? (b.bkMs[i] - a.bkMs[i]) / fr : 0;
            s.haveWindow = true;
        }
    }
    s.fpsCap = g_fpsCap;
    ThreadView tv;
    const bool haveThr = ReadThreads(tv, crashPath);
    s.bound = Classify(s.f, s.w, s.fpsCap, s.why, sizeof s.why, haveThr ? tv.top[0].name : nullptr, haveThr ? tv.top[0].pct : -1);
    if (!crashPath && g_si.gpuPasses) g_si.gpuPasses(s.gpuPasses, sizeof s.gpuPasses);
}

struct Part { const char* name; double ms; };
int Parts(const Snap& s, Part* p) {
    int n = 0;
    if (!s.haveWindow) return 0;
    if (s.w.gameThreadMs >= 0) p[n++] = {"game thread CPU", s.w.gameThreadMs};
    p[n++] = {"GPU fence wait (GF)", s.w.gpuFenceMs};
    p[n++] = {"present wait (W6)", s.w.presentWaitMs};
    p[n++] = {"DS retry sleep", s.w.dsRetryMs};
    if (s.w.bk) {
        p[n++] = {"engine ECL ring full", s.w.bkMs[0]};
        p[n++] = {"engine submit idle spin", s.w.bkMs[1]};
        p[n++] = {"engine list recycle fence wait", s.w.bkMs[2]};
        p[n++] = {"engine cache upload (tok 0x95)", s.w.bkMs[3]};
    }
    int k = 0;  // drop parts under 0.01 ms (noise; keeps the list short)
    for (int i = 0; i < n; ++i)
        if (p[i].ms >= 0.01) p[k++] = p[i];
    n = k;
    for (int i = 1; i < n; ++i)  // descending by ms
        for (int j = i; j > 0 && p[j].ms > p[j - 1].ms; --j) { const Part t = p[j]; p[j] = p[j - 1]; p[j - 1] = t; }
    return n;
}

// settings strings: the crash path must not wait on the cache lock (the writer may be the crashed thread)
template <typename F>
void WithCache(bool crashPath, F fn) {
    if (crashPath) {
        if (TryAcquireSRWLockShared(&g_cacheLock)) { fn(true); ReleaseSRWLockShared(&g_cacheLock); }
        else fn(false);
        return;
    }
    AcquireSRWLockShared(&g_cacheLock);
    fn(true);
    ReleaseSRWLockShared(&g_cacheLock);
}

void FreshIfAllowed(bool crashPath) {
    if (crashPath) return;
    ReadRam();
    ReadVram();
    RefreshGame();
    RefreshMod();
}

}  // namespace

void Start(const StartInfo& info) {
    KeepInfo(info);
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart > 0 ? f.QuadPart : 1;
    ReadCpu();
    if (HANDLE h = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr)) CloseHandle(h);
    ThreadInit();
    if (HANDLE h = CreateThread(nullptr, 0, ThreadWorker, nullptr, 0, nullptr)) CloseHandle(h);
    if (g_si.log) g_si.log("PERFSNAP: armed (last 10 s fps / 1%% low / CPU-GPU bound in every report and crash record)");
}

void OnGamePresent(int64_t now) {
    const DWORD tid = GetCurrentThreadId();
    const int64_t prev = g_lastQpc;
    if (prev && tid == g_presTid) {
        const LONG h = g_fHead;
        g_ms[h % kFrames] = (float)((double)(now - prev) * 1000.0 / (double)g_freq);
        InterlockedExchange(&g_fHead, h + 1);
    }
    g_presTid = tid;
    g_lastQpc = now;
    if (now < g_nextSample) return;
    g_nextSample = now + g_freq / 4;
    const LONG sh = g_sHead;
    Sample& s = g_s[sh % kSamples];
    s.qpc = now;
    s.frames = g_fHead;
    s.tid = tid;
    FILETIME c, e, k, u;
    s.thr = GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u) ? Ft(k) + Ft(u) : 0;
    s.proc = GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u) ? Ft(k) + Ft(u) : 0;
    s.w6 = Rd(g_si.w6Ticks);
    s.gf = Rd(g_si.gfTicks);
    s.ds = Rd(g_si.dsTicks);
    s.l1 = Rd(g_si.l1);
    s.bk = g_si.backendWaitsMs && g_si.backendWaitsMs(s.bkMs);
    InterlockedExchange(&g_sHead, sh + 1);
}

void NoteBackBuffer(unsigned w, unsigned h) {
    if ((unsigned)g_bbW != w) InterlockedExchange(&g_bbW, (LONG)w);
    if ((unsigned)g_bbH != h) InterlockedExchange(&g_bbH, (LONG)h);
}

size_t Json(char* out, size_t cap, bool crashPath) {
    Buf j(out, cap);
    if (!cap) return 0;
    FreshIfAllowed(crashPath);
    Snap s;
    Build(s, crashPath);
    j.f("{\"window_s\":%.1f,\"frames\":%d,\"avg_fps\":%.1f,\"low1_fps\":%.1f,\"avg_ms\":%.2f,\"p99_ms\":%.2f,\"max_ms\":%.2f",
        s.f.spanMs / 1000.0, s.f.frames, s.f.avgFps, s.f.low1Fps, s.f.avgMs, s.f.p99Ms, s.f.maxMs);
    j.f(",\"last_frame_age_s\":%.1f,\"bound\":\"%s\",\"bound_why\":", s.ageS, BoundName(s.bound));
    j.str(s.why, 190);
    if (s.haveWindow)
        j.f(",\"game_thread_ms\":%.2f,\"process_cores\":%.2f,\"gpu_fence_wait_ms\":%.2f,\"present_wait_ms\":%.2f,\"limiter_per_frame\":%.2f",
            s.w.gameThreadMs, s.w.procCores, s.w.gpuFenceMs, s.w.presentWaitMs, s.w.limiterPerFrame);
    j.raw(",\"cpu\":");
    j.str(g_cpu, 63);
    j.f(",\"cpu_threads\":%d,\"gpu\":", g_cores);
    j.str(g_gpu, 127);
    j.raw(",\"gpu_ids\":");
    j.str(g_ids, 31);
    j.raw(",\"driver\":");
    j.str(g_driver, 39);
    j.f(",\"ram_mb\":{\"total\":%lld,\"used\":%lld},\"vram_mb\":{\"total\":%lld,\"used\":%lld,\"budget\":%lld}", g_ramTotal, g_ramUsed,
        g_vramTotal, g_vramUsed, g_vramBudget);
    j.f(",\"backbuffer\":\"%ldx%ld\",\"fps_cap\":%d", g_bbW, g_bbH, s.fpsCap);
    WithCache(crashPath, [&](bool ok) {
        j.raw(",\"upscaler\":");
        j.str(ok ? g_upscaler : "", 15);
        j.raw(",\"frame_gen\":");
        j.str(ok ? g_frameGen : "", 15);
        j.raw(",\"game_settings\":");
        j.raw(ok ? g_gameJson : "{}");
        if (ok && strcmp(g_modJson, "{}")) {  // #97775: an empty object says nothing: the key is left out
            j.raw(",\"mod_settings\":");
            j.raw(g_modJson);
        }
    });
    j.raw(",\"parts\":[");
    Part p[8];
    const int np = Parts(s, p);
    for (int i = 0; i < np; ++i) {
        if (i) j.raw(",");
        j.raw("{\"name\":");
        j.str(p[i].name, 60);
        j.f(",\"ms\":%.2f}", p[i].ms);
    }
    j.raw("]");
    ThreadView tv;
    if (ReadThreads(tv, crashPath)) {  // #97673: busiest game threads over the last 30 s, % of one core
        j.f(",\"threads_window_s\":%.1f,\"thread_count\":%d,\"busiest_thread\":{\"name\":", tv.winS, tv.count);
        j.str(tv.top[0].name, 47);
        j.f(",\"tid\":%u,\"core_pct\":%.1f},\"threads\":[", tv.top[0].tid, tv.top[0].pct);
        for (int i = 0; i < tv.n; ++i) {
            if (i) j.raw(",");
            j.raw("{\"name\":");
            j.str(tv.top[i].name, 47);
            j.f(",\"tid\":%u,\"core_pct\":%.1f}", tv.top[i].tid, tv.top[i].pct);
        }
        j.raw("]");
    }
    if (s.gpuPasses[0]) { j.raw(",\"gpu_passes\":"); j.str(s.gpuPasses, 250); }
    j.raw("}");
    if (j.over) { out[0] = 0; return 0; }  // cut JSON would make the site reject the whole record
    return j.n;
}

size_t Text(char* out, size_t cap, bool crashPath) {
    Buf t(out, cap);
    if (!cap) return 0;
    FreshIfAllowed(crashPath);
    Snap s;
    Build(s, crashPath);
    t.f("  last %.1f s: %d frames, avg %.1f fps, 1%% low %.1f fps, frame %.2f ms avg / %.2f p99 / %.2f max, newest frame %.1f s ago\r\n",
        s.f.spanMs / 1000.0, s.f.frames, s.f.avgFps, s.f.low1Fps, s.f.avgMs, s.f.p99Ms, s.f.maxMs, s.ageS);
    t.f("  bound: %s (%s)\r\n", BoundName(s.bound), s.why);
    if (s.haveWindow)
        t.f("  per frame: game thread CPU %.2f ms, GPU fence wait %.2f ms, present wait %.2f ms, limiter %.2f; process %.2f cores\r\n",
            s.w.gameThreadMs, s.w.gpuFenceMs, s.w.presentWaitMs, s.w.limiterPerFrame, s.w.procCores);
    t.f("  CPU %s (%d threads); GPU %s [%s] driver %s\r\n", g_cpu[0] ? g_cpu : "?", g_cores, g_gpu[0] ? g_gpu : "?", g_ids, g_driver);
    t.f("  RAM %lld / %lld MB used; VRAM %lld / %lld MB used (budget %lld)\r\n", g_ramUsed, g_ramTotal, g_vramUsed, g_vramTotal, g_vramBudget);
    t.f("  back buffer %ldx%ld, fps cap %d", g_bbW, g_bbH, s.fpsCap);
    WithCache(crashPath, [&](bool ok) {
        if (!ok) { t.raw("\r\n  (settings busy)\r\n"); return; }
        t.f(", upscaler %s, frame generation %s\r\n", g_upscaler[0] ? g_upscaler : "?", g_frameGen[0] ? g_frameGen : "?");
        t.f("  game: %s\r\n", g_gameText[0] ? g_gameText : "(not read: settings table not found on this exe, or not started)");
        t.f("  mod: %s\r\n", g_modText[0] ? g_modText : "(not read)");
    });
    Part p[8];
    const int np = Parts(s, p);
    if (np) {
        t.raw("  top frame costs (ms per frame):");
        for (int i = 0; i < np && i < 5; ++i) t.f(" %s %.2f;", p[i].name, p[i].ms);
        t.raw("\r\n");
    }
    if (s.gpuPasses[0]) t.f("  GPU passes: %s\r\n", s.gpuPasses);
    ThreadView tv;
    if (ReadThreads(tv, crashPath)) {  // #97673
        t.f("  threads (last %.0f s, %d threads, %% of one core): busiest %s %.0f%%; top:", tv.winS, tv.count, tv.top[0].name,
            tv.top[0].pct);
        for (int i = 0; i < tv.n; ++i) t.f(" %s %.0f%%;", tv.top[i].name, tv.top[i].pct);
        t.raw("\r\n");
    }
    return t.n;
}

void ResetForTest(const StartInfo& info) {
    KeepInfo(info);
    InterlockedExchange(&g_fHead, 0);
    InterlockedExchange(&g_sHead, 0);
    g_lastQpc = 0;
    g_presTid = 0;
    g_nextSample = 0;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart > 0 ? f.QuadPart : 1;
}

void ThreadTickForTest() {
    if (!g_thrTick) ThreadInit();
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    ThreadTick(q.QuadPart);
}

void SetHardwareForTest(const char* cpu, int cores, const char* gpu, const char* ids, const char* driver, uint64_t vramTotalMb,
                        uint64_t vramUsedMb, uint64_t vramBudgetMb, uint64_t ramTotalMb, uint64_t ramUsedMb) {
    Copy(g_cpu, sizeof g_cpu, cpu);
    g_cores = cores;
    Copy(g_gpu, sizeof g_gpu, gpu);
    Copy(g_ids, sizeof g_ids, ids);
    Copy(g_driver, sizeof g_driver, driver);
    g_vramTotal = (LONGLONG)vramTotalMb, g_vramUsed = (LONGLONG)vramUsedMb, g_vramBudget = (LONGLONG)vramBudgetMb;
    g_ramTotal = (LONGLONG)ramTotalMb, g_ramUsed = (LONGLONG)ramUsedMb;
}

void RefreshModForTest() { RefreshMod(); }

const char* ThreadNameForTest(unsigned long tid) {
    for (const ThrSlot& s : g_thr)
        if (s.h && s.tid == tid) return s.name;
    return "";
}

void SetSettingsForTest(const char* gameJson, const char* modJson, int fpsCap, const char* upscaler, const char* frameGen) {
    Copy(g_gameJson, sizeof g_gameJson, gameJson);
    Copy(g_modJson, sizeof g_modJson, modJson);
    g_fpsCap = fpsCap;
    Copy(g_upscaler, sizeof g_upscaler, upscaler);
    Copy(g_frameGen, sizeof g_frameGen, frameGen);
}

bool SettingsTableFoundForTest(uintptr_t base, uintptr_t end) { return TableLooksRight(base, end); }

}  // namespace perfsnap
