// skipfx.cpp - SkipFx: diagnostic draw / dispatch skip list for the cost-vs-looks audit. See skipfx.h and notes\cost-vs-looks.md.
// [Debug] SkipFx = 0 by default (nothing installed). Never ship it on.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "psoswap.h"
#include "skipfx.h"

namespace skipfx {
namespace {

// ID3D12GraphicsCommandList vtable slots (same numbering as gpuopt / gpuprof)
constexpr int kSlotGetDevice = 7, kSlotClose = 9, kSlotReset = 10, kSlotClearState = 11, kSlotDraw = 12, kSlotDrawIdx = 13,
              kSlotDispatch = 14, kSlotSetPso = 25, kSlotOMSetRT = 46, kSlotClearRTV = 48, kSlotBeginEvent = 57,
              kSlotEndEvent = 58, kSlotExecIndirect = 59, kSlotDispatchMesh = 79;
constexpr int kHookSlots[] = {kSlotClose, kSlotReset, kSlotClearState, kSlotDraw, kSlotDrawIdx, kSlotDispatch, kSlotSetPso,
                              kSlotOMSetRT, kSlotBeginEvent, kSlotEndEvent, kSlotExecIndirect, kSlotDispatchMesh};
constexpr int kMaxSetsListed = 32;
constexpr uint32_t kCache = 1u << 16;   // shader-set ids with a cached match bit
constexpr int kLists = 1024;
constexpr int kMaxDepth = 64;

void (*g_log)(const char*, ...) = nullptr;
#define SLOG(fmt, ...) do { if (g_log) g_log("SkipFx: " fmt, __VA_ARGS__); } while (0)

struct Cfg {   // immutable once published (counters aside); never freed (a few hundred applies per session at most)
    uint32_t gen = 0;
    char tag[64] = "-";
    std::vector<std::string> pso;    // lower-case hex fragments, optionally "xs=" stage-qualified
    std::vector<std::string> mk;     // lower-case marker names, trailing '*' = prefix
    int clearMode = 0;
    float clear[4] = {0, 0, 0, 0};
    bool any = false;
    unsigned long long tApplyUnixMs = 0, tApplyTick = 0;
    volatile LONG64 draws = 0, dispatches = 0, ei = 0, mesh = 0, clears = 0;
    volatile LONG nSets = 0;
    uint32_t sets[kMaxSetsListed] = {};
};
Cfg* volatile g_cur = nullptr;
volatile LONG g_gen = 0;
volatile LONG g_on = 0;       // hooks active (cleared on an exception)
int g_enabled = 0;
std::wstring g_iniPath, g_ctlPath, g_ackPath;
void* const* g_orig0 = nullptr;
void* const* g_orig1 = nullptr;
volatile LONG g_rtvInc = 0;
volatile uint32_t g_setCache[kCache];   // (gen << 1) | match; 0 = not computed
volatile LONG64 g_begins = 0, g_depthOverflow = 0, g_listFull = 0;

void* Orig(int set, int slot) {
    void* const* t = (set && g_orig1) ? g_orig1 : g_orig0;
    return t ? t[slot] : nullptr;
}

// ---- per-list state (open addressing on the list pointer; entries are never removed - the game keeps its lists) ----------
struct LS {
    void* volatile list;
    uintptr_t pso;
    uint32_t setId, psoGen;
    uint8_t psoSkip, bindingUsed, cleared, nRtv;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv[8];
};
LS g_ls[kLists];

LS* FindLS(void* list) {
    uint32_t h = (uint32_t)((((uintptr_t)list >> 4) * 0x9E3779B97F4A7C15ull) >> 40) & (kLists - 1);
    for (int i = 0; i < kLists; ++i, h = (h + 1) & (kLists - 1)) {
        LS& s = g_ls[h];
        void* cur = s.list;
        if (cur == list) return &s;
        if (!cur) {
            void* prev = InterlockedCompareExchangePointer((void* volatile*)&s.list, list, nullptr);
            if (!prev || prev == list) return &s;
        }
    }
    InterlockedIncrement64(&g_listFull);
    return nullptr;
}

// ---- marker stack of the recording thread (the engine opens / closes its markers on one dispatcher thread, as GP assumes) ---
thread_local int t_depth = 0;
thread_local int t_skipDepth = 0;
thread_local uint8_t t_bits[kMaxDepth];

// ---- matching ----------------------------------------------------------------------------------------------------------------
void Lower(char* s) {
    for (; *s; ++s)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

bool MatchSetUncached(uint32_t id, const Cfg& c) {
    char d[400];
    if (!id || !psoswap::DescribeSet(id, d, sizeof d)) return false;
    Lower(d);
    for (const std::string& t : c.pso)
        if (strstr(d, t.c_str())) return true;
    return false;
}

bool MatchSet(uint32_t id, Cfg& c) {
    if (!id) return false;
    if (id < kCache) {
        const uint32_t v = g_setCache[id];
        if ((v >> 1) == c.gen) return (v & 1) != 0;
    }
    const bool m = MatchSetUncached(id, c);
    if (id < kCache) g_setCache[id] = (c.gen << 1) | (m ? 1u : 0u);
    if (m) {
        const LONG k = InterlockedIncrement(&c.nSets) - 1;
        if (k < kMaxSetsListed) c.sets[k] = id;
    }
    return m;
}

bool MatchMarker(const Cfg& c, UINT meta, const void* data, UINT size) {
    if (c.mk.empty() || !data || !size) return false;
    char b[64];
    int n = 0;
    const uint8_t* p = (const uint8_t*)data;
    if (meta != 1 && size >= 2 && p[1] == 0) {
        const wchar_t* w = (const wchar_t*)data;
        for (UINT i = 0; i < size / 2 && n < 63 && w[i]; ++i) b[n++] = (w[i] >= 32 && w[i] < 127) ? (char)w[i] : '?';
    } else {
        const char* s = (const char*)data;
        for (UINT i = 0; i < size && n < 63 && s[i]; ++i) b[n++] = (s[i] >= 32 && s[i] < 127) ? s[i] : '?';
    }
    b[n] = 0;
    Lower(b);
    for (const std::string& m : c.mk) {
        const size_t L = m.size();
        if (L && m[L - 1] == '*') {
            if (!strncmp(b, m.c_str(), L - 1)) return true;
        } else if (m == b) {
            return true;
        }
    }
    return false;
}

// ---- hook body ---------------------------------------------------------------------------------------------------------------
using ClearRtvFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE, const FLOAT*, UINT, const D3D12_RECT*);
using GetDeviceFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, REFIID, void**);

UINT RtvInc(void* list, int set) {
    LONG v = g_rtvInc;
    if (v) return (UINT)v;
    ID3D12Device* d = nullptr;
    auto gd = reinterpret_cast<GetDeviceFn>(Orig(set, kSlotGetDevice));
    if (gd && SUCCEEDED(gd((ID3D12GraphicsCommandList*)list, __uuidof(ID3D12Device), (void**)&d)) && d) {
        v = (LONG)d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        d->Release();
        InterlockedExchange(&g_rtvInc, v);
    }
    return (UINT)v;
}

bool Impl(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    Cfg* c = g_cur;
    switch (slot) {
        case kSlotBeginEvent: {
            InterlockedIncrement64(&g_begins);
            const bool m = c && c->any && MatchMarker(*c, (UINT)a1, (const void*)a2, (UINT)a3);
            if (t_depth < kMaxDepth) {
                t_bits[t_depth] = m ? 1 : 0;
                if (m) ++t_skipDepth;
            } else {
                InterlockedIncrement64(&g_depthOverflow);
            }
            ++t_depth;
            return false;
        }
        case kSlotEndEvent:
            if (t_depth > 0) {
                --t_depth;
                if (t_depth < kMaxDepth && t_bits[t_depth] && t_skipDepth > 0) --t_skipDepth;
            }
            return false;
        default: break;
    }
    LS* s = FindLS(list);
    if (!s) return false;
    switch (slot) {
        case kSlotReset:   // Reset(allocator, initial PSO)
        case kSlotClearState:
            s->pso = slot == kSlotReset ? a2 : a1;
            s->setId = psoswap::PsoSetId((const void*)s->pso);
            s->psoGen = 0;
            s->nRtv = 0;
            s->bindingUsed = s->cleared = 0;
            return false;
        case kSlotClose:
            return false;
        case kSlotSetPso:
            if (a1 != s->pso) {
                s->pso = a1;
                s->setId = psoswap::PsoSetId((const void*)a1);
                s->psoGen = 0;
            }
            return false;
        case kSlotOMSetRT: {   // OMSetRenderTargets(n, pRTVs, singleRange, pDSV)
            const UINT n = (UINT)a1 > 8 ? 8 : (UINT)a1;
            const auto* h = (const D3D12_CPU_DESCRIPTOR_HANDLE*)a2;
            s->nRtv = 0;
            if (h && n) {
                if (a3 && n > 1) {   // one handle, consecutive descriptors
                    const UINT inc = RtvInc(list, set);
                    if (inc)
                        for (UINT i = 0; i < n; ++i) s->rtv[i].ptr = h[0].ptr + (SIZE_T)i * inc;
                    s->nRtv = (uint8_t)(inc ? n : 0);
                } else {
                    for (UINT i = 0; i < n; ++i) s->rtv[i] = a3 ? h[0] : h[i];
                    s->nRtv = (uint8_t)n;
                }
            }
            s->bindingUsed = s->cleared = 0;
            return false;
        }
        default: break;
    }
    // draw / dispatch
    const bool gfxDraw = slot == kSlotDraw || slot == kSlotDrawIdx;
    bool skip = false;
    if (c && c->any) {
        skip = t_skipDepth > 0;
        if (!skip && !c->pso.empty() && s->setId) {
            if (s->psoGen != c->gen) {
                s->psoSkip = MatchSet(s->setId, *c) ? 1 : 0;
                s->psoGen = c->gen;
            }
            skip = s->psoSkip != 0;
        }
    }
    if (!skip) {
        if (slot != kSlotDispatch) s->bindingUsed = 1;
        return false;
    }
    switch (slot) {
        case kSlotDraw:
        case kSlotDrawIdx: InterlockedIncrement64(&c->draws); break;
        case kSlotDispatch: InterlockedIncrement64(&c->dispatches); break;
        case kSlotExecIndirect: InterlockedIncrement64(&c->ei); break;
        default: InterlockedIncrement64(&c->mesh); break;
    }
    if (gfxDraw && c->clearMode == 1 && !s->bindingUsed && !s->cleared && s->nRtv) {
        auto clr = reinterpret_cast<ClearRtvFn>(Orig(set, kSlotClearRTV));
        if (clr) {
            for (int i = 0; i < s->nRtv; ++i) clr((ID3D12GraphicsCommandList*)list, s->rtv[i], c->clear, 0, nullptr);
            InterlockedIncrement64(&c->clears);
        }
        s->cleared = 1;
    }
    return true;
}

// ---- configuration ---------------------------------------------------------------------------------------------------------------
std::string Narrow(const wchar_t* w) {
    std::string s;
    for (; *w; ++w) s.push_back((*w >= 32 && *w < 127) ? (char)*w : '?');
    return s;
}
std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}
std::vector<std::string> Split(const std::string& s, const char* seps) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s) {
        if (strchr(seps, ch)) {
            cur = Trim(cur);
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    cur = Trim(cur);
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::string ReadStr(const wchar_t* sec, const wchar_t* key, const std::wstring& file) {
    std::vector<wchar_t> b(16384);
    GetPrivateProfileStringW(sec, key, L"", b.data(), (DWORD)b.size(), file.c_str());
    return Narrow(b.data());
}

Cfg* Build(const std::string& tag, const std::string& psoList, const std::string& mkList, int clearMode, const std::string& clear) {
    Cfg* c = new (std::nothrow) Cfg();
    if (!c) return nullptr;
    strncpy_s(c->tag, tag.empty() ? "-" : tag.c_str(), _TRUNCATE);
    for (std::string t : Split(psoList, ", ;|")) {
        char b[96];
        strncpy_s(b, t.c_str(), _TRUNCATE);
        Lower(b);
        std::string v = b;
        if (v.rfind("0x", 0) == 0) v = v.substr(2);
        const size_t eq = v.find('=');
        const size_t hexLen = eq == std::string::npos ? v.size() : v.size() - eq - 1;
        if (hexLen < 6) {
            SLOG("PSO token '%s' ignored (need >= 6 hex chars)", t.c_str());
            continue;
        }
        c->pso.push_back(v);
    }
    for (std::string t : Split(mkList, ",;|")) {
        char b[96];
        strncpy_s(b, t.c_str(), _TRUNCATE);
        Lower(b);
        c->mk.push_back(b);
    }
    c->clearMode = clearMode == 1 ? 1 : 0;
    const std::vector<std::string> cv = Split(clear, ", ;");
    for (size_t i = 0; i < 4 && i < cv.size(); ++i) c->clear[i] = (float)atof(cv[i].c_str());
    c->any = !c->pso.empty() || !c->mk.empty();
    return c;
}

unsigned long long UnixMs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    const unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return t / 10000ull - 11644473600000ull;
}

void LogTotals(Cfg* o) {
    if (!o) return;
    const double s = (double)(GetTickCount64() - o->tApplyTick) / 1000.0;
    std::string sets;
    const LONG n = o->nSets < kMaxSetsListed ? o->nSets : kMaxSetsListed;
    for (LONG i = 0; i < n; ++i) {
        char d[400];
        if (psoswap::DescribeSet(o->sets[i], d, sizeof d)) {
            if (!sets.empty()) sets += " | ";
            sets += d;
        }
    }
    SLOG("done '%s' (gen %u, %.1f s): skipped %lld draws, %lld dispatches, %lld ExecuteIndirect, %lld DispatchMesh, %lld RT clears; "
         "matched sets %ld%s%s",
         o->tag, o->gen, s, (long long)o->draws, (long long)o->dispatches, (long long)o->ei, (long long)o->mesh, (long long)o->clears,
         (long)o->nSets, sets.empty() ? "" : ": ", sets.c_str());
}

void Apply(Cfg* c) {
    if (!c) return;
    c->gen = (uint32_t)InterlockedIncrement(&g_gen);
    c->tApplyUnixMs = UnixMs();
    c->tApplyTick = GetTickCount64();
    Cfg* old = (Cfg*)InterlockedExchangePointer((void* volatile*)&g_cur, c);
    LogTotals(old);
    std::string p, m;
    for (const auto& t : c->pso) p += (p.empty() ? "" : ",") + t;
    for (const auto& t : c->mk) m += (m.empty() ? "" : ",") + t;
    SLOG("apply '%s' gen %u: PSO [%s] marker [%s] ClearMode %d (%.2f %.2f %.2f %.2f)%s", c->tag, c->gen, p.c_str(), m.c_str(), c->clearMode,
         c->clear[0], c->clear[1], c->clear[2], c->clear[3], c->any ? "" : " - nothing skipped (baseline)");
}

void WriteAck() {
    Cfg* c = g_cur;
    if (!c || g_ackPath.empty()) return;
    char b[512];
    const int n = _snprintf_s(b, sizeof b, _TRUNCATE,
                              "gen=%u tag=%s t_apply_unix_ms=%llu age_ms=%llu draws=%lld dispatches=%lld ei=%lld mesh=%lld clears=%lld sets=%ld "
                              "begins=%lld on=%ld tracking=%d\n",
                              c->gen, c->tag, c->tApplyUnixMs, GetTickCount64() - c->tApplyTick, (long long)c->draws, (long long)c->dispatches,
                              (long long)c->ei, (long long)c->mesh, (long long)c->clears, (long)c->nSets, (long long)g_begins, (long)g_on,
                              psoswap::Tracking() ? 1 : 0);
    if (n <= 0) return;
    const std::wstring tmp = g_ackPath + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(h, b, (DWORD)n, &w, nullptr);
    CloseHandle(h);
    MoveFileExW(tmp.c_str(), g_ackPath.c_str(), MOVEFILE_REPLACE_EXISTING);
}

Cfg* LoadControl() {
    const std::wstring& f = g_ctlPath;
    return Build(ReadStr(L"SkipFx", L"Tag", f), ReadStr(L"SkipFx", L"PSO", f), ReadStr(L"SkipFx", L"Marker", f),
                 (int)GetPrivateProfileIntW(L"SkipFx", L"ClearMode", 0, f.c_str()), ReadStr(L"SkipFx", L"Clear", f));
}

DWORD WINAPI CtlThread(void*) {
    FILETIME lastT = {};
    ULONGLONG lastS = ~0ull, lastAck = 0;
    bool warnedMk = false;
    uint32_t warnGen = 0;
    LONG64 beginsAtApply = 0;
    for (;;) {
        Sleep(100);
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(g_ctlPath.c_str(), GetFileExInfoStandard, &fa)) {
            const ULONGLONG sz = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
            if (CompareFileTime(&fa.ftLastWriteTime, &lastT) != 0 || sz != lastS) {
                lastT = fa.ftLastWriteTime;
                lastS = sz;
                Sleep(20);   // a writer that does not rename: let it finish
                Apply(LoadControl());
                lastAck = 0;
            }
        }
        Cfg* c = g_cur;
        if (c && c->gen != warnGen) {
            warnGen = c->gen;
            warnedMk = false;
            beginsAtApply = g_begins;
        }
        if (c && !c->mk.empty() && !warnedMk && GetTickCount64() - c->tApplyTick > 10000 && g_begins == beginsAtApply) {
            warnedMk = true;
            SLOG("'%s' names markers but no engine marker reached the hook in 10 s - set [Profiler] GPUProf_Markers = 1", c->tag);
        }
        const ULONGLONG now = GetTickCount64();
        if (now - lastAck >= 500) {
            lastAck = now;
            WriteAck();
        }
    }
}

std::wstring DirOf(const std::wstring& p) {
    const size_t k = p.find_last_of(L"\\/");
    return k == std::wstring::npos ? L"." : p.substr(0, k);
}

}  // namespace

void Start(const StartInfo& info) {
    g_log = info.log;
    g_iniPath = info.iniPath ? info.iniPath : L"";
    g_enabled = (int)GetPrivateProfileIntW(L"Debug", L"SkipFx", 0, g_iniPath.c_str()) == 1 ? 1 : 0;
    if (!g_enabled) return;
    wchar_t f[MAX_PATH] = {};
    GetPrivateProfileStringW(L"Debug", L"SkipFxFile", L"Nioh3EngineFix_skip.ini", f, MAX_PATH, g_iniPath.c_str());
    std::wstring ctl = f;
    if (ctl.empty()) ctl = L"Nioh3EngineFix_skip.ini";
    if (ctl.find(L':') == std::wstring::npos && ctl[0] != L'\\') ctl = DirOf(g_iniPath) + L"\\" + ctl;
    g_ctlPath = ctl;
    g_ackPath = ctl + L".ack";
    DeleteFileW(g_ackPath.c_str());
    Apply(Build("ini", ReadStr(L"Debug", L"SkipPSO", g_iniPath), ReadStr(L"Debug", L"SkipMarker", g_iniPath),
                (int)GetPrivateProfileIntW(L"Debug", L"SkipClearMode", 0, g_iniPath.c_str()), ReadStr(L"Debug", L"SkipClear", g_iniPath)));
    SLOG("DIAGNOSTIC ON - draws / dispatches of the listed PSOs / markers are NOT rendered. Control file %s (polled every 100 ms), "
         "ack %s.ack. Never play with SkipFx on.",
         Narrow(g_ctlPath.c_str()).c_str(), Narrow(g_ctlPath.c_str()).c_str());
    if (HANDLE h = CreateThread(nullptr, 0, CtlThread, nullptr, 0, nullptr)) CloseHandle(h);
}

bool WantsHooks() { return g_enabled != 0; }

void EnableHooks(volatile uint8_t* mask, int slots, void* const* orig0, void* const* orig1) {
    if (!g_enabled || !mask || slots <= kSlotDispatchMesh || !orig0) return;
    g_orig0 = orig0;
    g_orig1 = orig1;
    InterlockedExchange(&g_on, 1);
    for (int v : kHookSlots) mask[v] = 1;
    SLOG("command-list hooks on (SetPipelineState, Reset, ClearState, OMSetRenderTargets, Begin/EndEvent, Draw*, Dispatch, "
         "ExecuteIndirect, DispatchMesh); PSO map %s",
         psoswap::Tracking() ? "from psoswap" : "NOT available - PSO tokens cannot match (only markers work)");
}

bool OnListCall(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    if (!g_on) return false;
    __try {
        return Impl(list, set, slot, a1, a2, a3);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_on, 0);
        SLOG("exception 0x%08lX in the command-list hook (slot %d) - SkipFx switched off (nothing is skipped any more)", GetExceptionCode(), slot);
        return false;
    }
}

const char* StatusLine(char* buf, size_t n) {
    if (!buf || !n) return "";
    buf[0] = 0;
    Cfg* c = g_cur;
    if (!g_enabled || !c) return buf;
    _snprintf_s(buf, n, _TRUNCATE, "SkipFx '%s': %lld draws, %lld dispatches, %lld EI skipped, %ld sets%s", c->tag, (long long)c->draws,
                (long long)c->dispatches, (long long)c->ei, (long)c->nSets, g_on ? "" : " (OFF after an exception)");
    return buf;
}

}  // namespace skipfx
