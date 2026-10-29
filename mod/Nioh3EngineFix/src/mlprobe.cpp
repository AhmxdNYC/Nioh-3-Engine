// mlprobe.cpp - MLP CPU half: entry probes on the meshlet recorders (see mlprobe.h, notes\meshlet-frontend.md). Measurement only.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "fnprobe_core.h"
#include "mlprobe.h"

namespace mlprobe {
namespace {

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0, g_imageEnd = 0;
volatile LONG g_active = 0;
volatile LONG64 g_frame = 0;

struct Site {
    uint32_t rva;
    Fn fn;
    const char* name;
};
// all three start with `mov [rsp+10h],rbx` (48 89 5C 24 10): the thunk relocates those 5 bytes (patches.json group MLP, check sites)
const uint8_t kProl[5] = {0x48, 0x89, 0x5C, 0x24, 0x10};
const Site kSites[kNumFn] = {
    {0x531948, kLod, "LOD"},
    {0x5301FC, kPre, "PRE"},
    {0x53FD3C, kMic, "MIC"},
};
const char* const kFnName[kNumFn] = {"LOD", "PRE", "MIC"};

// v2.02 MeshletSys / recorder-argument offsets (decompiles 531948 / 5301FC; notes\gpu-rewrite-round3.md 3.1)
constexpr uintptr_t kOffPayloadBuf = 0x16A170;   // tToMshltCullPayloads buffer object; [obj+18h] = ID3D12Resource (tok 0x8A operand)
constexpr uintptr_t kOffPreMode = 0x169D48;      // PRE: mode (4 -> cnt at param_5+1Ch, else +20h)
constexpr uintptr_t kOffPreGateA = 0x14, kOffPreGateB = 0x169D2D;   // PRE: both set -> the #23..#31 / #26 chain is recorded

bool Rd(uintptr_t p, void* out, size_t n) {
    if (p < 0x10000) return false;
    __try {
        memcpy(out, (const void*)p, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
uint64_t Rd64(uintptr_t p) {
    uint64_t v = 0;
    return Rd(p, &v, 8) ? v : 0;
}
uint32_t Rva(uint64_t a) { return (a >= g_base && a < g_imageEnd) ? (uint32_t)(a - g_base) : 1u; }

struct Call {
    Fn fn;
    uint32_t caller;
    DWORD tid;
    uint64_t sys, res;
    int levels, roots, cull;   // LOD
    uint32_t cam;              // LOD: FNV of the 32-byte camera block
    uint8_t flag11;            // LOD param_11 (forces the shadow LOD permutation flag, 91BF54)
    uint32_t p9;               // LOD param_9 (float bits -> ctx+1434h)
    uint64_t p10;              // LOD param_10 (extra UAV output of the level kernel; 8DA0A8 = VSM clipmap list)
    uint64_t ctx;              // LOD param_3 (culling context)
    uint32_t ctx1430;          // LOD ctx+1430h (8DA0A8 writes clipmap << 4 | 1 before the call)
    int cnt, mode;             // PRE
    uint8_t gate;              // PRE: the MPC tail chain is recorded
    int p6;                    // PRE param_6
    uint8_t p7;                // PRE param_7
    uint32_t commit;           // PRE: RVA of param_3 vtbl+C8h (the commit that builds the MPC cbuffer)
};

struct Agg {
    uint64_t calls = 0, frames = 0;
    LONG64 lastFrame = -1;
    uint64_t sys[4] = {};
    int nsys = 0;
    bool sysMore = false;
    DWORD tid[4] = {};
    int ntid = 0;
    int levMin = INT_MAX, levMax = 0;
    double levSum = 0, rootSum = 0, cullSum = 0;
    uint64_t flag11 = 0;
    uint64_t cnt0 = 0, cntSmall = 0, cntBig = 0, gate = 0, gateSmall = 0, gateZero = 0;
    double cntSum = 0;
    int cntMax = 0;
    uint64_t mode4 = 0;
    uint32_t commit = 0;
};

SRWLOCK g_lock = SRWLOCK_INIT;
std::map<uint64_t, Agg> g_agg;             // (fn << 32) | caller
std::vector<Call> g_cur;                   // calls of the CPU frame in progress
LONG64 g_curFrame = -1;
std::vector<Call> g_last[kNumFn];          // per recorder: the calls of the last complete CPU frame, in call order
struct SeqStat { uint64_t n = 0; std::string text; };
std::unordered_map<uint64_t, SeqStat> g_seq;
uint64_t g_seqFrames = 0;
// LOD duplicate test (window counters)
uint64_t g_dupIdent = 0, g_dupSameSys = 0, g_dupOtherSys = 0, g_lodFrames2 = 0;
uint32_t g_dupPairA = 0, g_dupPairB = 0;   // callers of the last identical pair
volatile LONG64 g_hookCalls = 0, g_hookFaults = 0;

uint32_t Fnv(const void* p, size_t n) {
    uint32_t h = 0x811C9DC5u;
    for (size_t i = 0; i < n; ++i) h = (h ^ ((const uint8_t*)p)[i]) * 0x01000193u;
    return h;
}

void AddU64(uint64_t* a, int& n, bool& more, uint64_t v) {
    for (int i = 0; i < n; ++i)
        if (a[i] == v) return;
    if (n < 4) a[n++] = v;
    else more = true;
}

// g_lock held: the frame boundary moves the calls of the finished CPU frame to g_last and counts its sequence
void CloseFrameLocked() {
    if (g_curFrame < 0) return;
    std::vector<Call> per[kNumFn];
    std::string text;
    uint64_t h = 1469598103934665603ull;
    int lods = 0;
    for (const Call& c : g_cur) {
        per[c.fn].push_back(c);
        char b[24];
        _snprintf_s(b, sizeof b, _TRUNCATE, "%s%s@%X", text.empty() ? "" : " ", kFnName[c.fn], c.caller);
        if (text.size() < 400) text += b;
        h = (h ^ ((uint64_t)c.fn << 32 | c.caller)) * 1099511628211ull;
        lods += c.fn == kLod;
    }
    for (int f = 0; f < kNumFn; ++f) g_last[f].swap(per[f]);
    if (!g_cur.empty()) {
        SeqStat& s = g_seq[h];
        if (!s.n) s.text = text;
        s.n++;
        g_seqFrames++;
        if (lods >= 2) g_lodFrames2++;
    }
    g_cur.clear();
}

void Record(Call& c) {
    const LONG64 f = g_frame;
    AcquireSRWLockExclusive(&g_lock);
    if (f != g_curFrame) {
        CloseFrameLocked();
        g_curFrame = f;
    }
    if (c.fn == kLod)   // duplicate-resolve test against the earlier LOD calls of this frame
        for (const Call& o : g_cur) {
            if (o.fn != kLod) continue;
            if (o.sys == c.sys && o.cam == c.cam && o.levels == c.levels && o.roots == c.roots && o.cull == c.cull &&
                o.flag11 == c.flag11 && o.p9 == c.p9 && o.p10 == c.p10 && o.ctx == c.ctx && o.ctx1430 == c.ctx1430) {
                g_dupIdent++;
                g_dupPairA = o.caller;
                g_dupPairB = c.caller;
            } else if (o.sys == c.sys) {
                g_dupSameSys++;
            } else {
                g_dupOtherSys++;
            }
        }
    if (g_cur.size() < 256) g_cur.push_back(c);
    Agg& a = g_agg[((uint64_t)c.fn << 32) | c.caller];
    a.calls++;
    if (a.lastFrame != f) {
        a.lastFrame = f;
        a.frames++;
    }
    AddU64(a.sys, a.nsys, a.sysMore, c.sys);
    bool tm = false;
    uint64_t t64[4] = {a.tid[0], a.tid[1], a.tid[2], a.tid[3]};
    AddU64(t64, a.ntid, tm, c.tid);
    for (int i = 0; i < 4; ++i) a.tid[i] = (DWORD)t64[i];
    if (c.fn == kLod) {
        a.levMin = std::min(a.levMin, c.levels);
        a.levMax = std::max(a.levMax, c.levels);
        a.levSum += c.levels;
        a.rootSum += c.roots;
        a.cullSum += c.cull;
        a.flag11 += c.flag11 != 0;
    } else if (c.fn == kPre) {
        if (c.cnt <= 0) a.cnt0++;
        else if (c.cnt <= 1024) a.cntSmall++;
        else a.cntBig++;
        if (c.gate) {
            a.gate++;
            if (c.cnt >= 1 && c.cnt <= 1024) a.gateSmall++;
            if (c.cnt <= 0) a.gateZero++;
        }
        a.cntSum += c.cnt;
        a.cntMax = std::max(a.cntMax, c.cnt);
        a.mode4 += c.mode == 4;
        a.commit = c.commit;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

// ---- hooks (thunk slow path: original argument registers, entry rsp; sp[0] = return address, sp[5..] = stack args 5..) ----
void Common(Fn fn, uint64_t rcx, uint64_t /*rdx*/, uint64_t r8, uint64_t r9, const uint64_t* sp) {
    InterlockedIncrement64(&g_hookCalls);
    Call c = {};
    c.fn = fn;
    c.caller = Rva(sp[0]);
    c.tid = GetCurrentThreadId();
    c.sys = rcx;
    if (fn != kMic) {
        const uint64_t obj = Rd64(rcx + kOffPayloadBuf);
        c.res = obj ? Rd64(obj + 0x18) : 0;
    }
    uint64_t a5 = 0;
    Rd((uintptr_t)(sp + 5), &a5, 8);
    if (fn == kLod) {
        int v[4] = {};
        if (Rd(r9 + 0x24, v, 16)) {   // +24h levels, +28h, +2Ch roots, +30h cull codes
            c.levels = v[0];
            c.roots = v[2];
            c.cull = v[3];
        }
        uint8_t cam[32];
        c.cam = Rd(a5, cam, 32) ? Fnv(cam, 32) : 0;
        uint64_t a11 = 0;
        Rd((uintptr_t)(sp + 11), &a11, 8);
        c.flag11 = (uint8_t)a11;
        uint64_t a9 = 0, a10 = 0;
        Rd((uintptr_t)(sp + 9), &a9, 8);
        Rd((uintptr_t)(sp + 10), &a10, 8);
        c.p9 = (uint32_t)a9;
        c.p10 = a10;
        c.ctx = r8;
        Rd(r8 + 0x1430, &c.ctx1430, 4);
    } else if (fn == kPre) {
        int mode = 0;
        Rd(rcx + kOffPreMode, &mode, 4);
        c.mode = mode;
        int cnt = -1;
        Rd(a5 + (mode == 4 ? 0x1C : 0x20), &cnt, 4);
        c.cnt = cnt;
        uint8_t g1 = 0, g2 = 0;
        Rd(rcx + kOffPreGateA, &g1, 1);
        Rd(rcx + kOffPreGateB, &g2, 1);
        c.gate = (g1 && g2) ? 1 : 0;
        uint64_t a6 = 0, a7 = 0;
        Rd((uintptr_t)(sp + 6), &a6, 8);
        Rd((uintptr_t)(sp + 7), &a7, 8);
        c.p6 = (int)a6;
        c.p7 = (uint8_t)a7;
        const uint64_t vt = Rd64(r8);
        c.commit = vt ? Rva(Rd64(vt + 0xC8)) : 0;
    }
    Record(c);
}
void HkLod(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, const uint64_t* sp, uint64_t) {
    __try { Common(kLod, a0, a1, a2, a3, sp); } __except (EXCEPTION_EXECUTE_HANDLER) { InterlockedIncrement64(&g_hookFaults); }
}
void HkPre(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, const uint64_t* sp, uint64_t) {
    __try { Common(kPre, a0, a1, a2, a3, sp); } __except (EXCEPTION_EXECUTE_HANDLER) { InterlockedIncrement64(&g_hookFaults); }
}
void HkMic(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, const uint64_t* sp, uint64_t) {
    __try { Common(kMic, a0, a1, a2, a3, sp); } __except (EXCEPTION_EXECUTE_HANDLER) { InterlockedIncrement64(&g_hookFaults); }
}
using HookFn = void (*)(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*, uint64_t);
const HookFn kHooks[kNumFn] = {&HkLod, &HkPre, &HkMic};
volatile uint64_t g_count[kNumFn] = {};

// ---- patching (scheme of fnprobe.cpp) ----
constexpr size_t kThunkPage = 4096;
uint8_t* AllocIn(uintptr_t from, uintptr_t to) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + kThunkPage <= re && c + kThunkPage <= to)
                if (void* p = VirtualAlloc((void*)c, kThunkPage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo);
}

// suspends every other thread, retries while one sits inside a 5-byte prologue that is about to change, then writes all entries
int WriteAll(uint8_t* const* entry, const uint8_t (*bytes)[5], int np) {
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    for (int attempt = 0; attempt < 50; ++attempt) {
        HANDLE th[1024];
        int nth = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return -1;
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        for (BOOL ok = Thread32First(snap, &te); ok && nth < 1024; ok = Thread32Next(snap, &te)) {
            if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
            HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
            if (!h) continue;
            if (SuspendThread(h) == (DWORD)-1) {
                CloseHandle(h);
                continue;
            }
            th[nth++] = h;
        }
        CloseHandle(snap);
        bool busy = false;
        for (int i = 0; i < nth && !busy; ++i) {
            CONTEXT c;
            c.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(th[i], &c)) continue;
            for (int k = 0; k < np; ++k)
                if (c.Rip > (DWORD64)entry[k] && c.Rip < (DWORD64)(entry[k] + 5)) busy = true;
        }
        int written = 0;
        if (!busy)
            for (int k = 0; k < np; ++k) {
                DWORD old = 0;
                if (!VirtualProtect(entry[k], 5, PAGE_EXECUTE_READWRITE, &old)) continue;
                memcpy(entry[k], bytes[k], 5);
                VirtualProtect(entry[k], 5, old, &old);
                FlushInstructionCache(GetCurrentProcess(), entry[k], 5);
                ++written;
            }
        for (int i = 0; i < nth; ++i) {
            ResumeThread(th[i]);
            CloseHandle(th[i]);
        }
        if (!busy) return written;
        Sleep(2);
    }
    return -2;
}

// ---- SHA-256 (FIPS 180-4) ----
struct Sha {
    uint32_t h[8];
    uint8_t buf[64];
    uint64_t len = 0;
    size_t nb = 0;
    static uint32_t R(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    Sha() {
        static const uint32_t h0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        memcpy(h, h0, sizeof h);
    }
    void Block(const uint8_t* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
            0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
            0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
            0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
            0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = R(w[i - 15], 7) ^ R(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = R(w[i - 2], 17) ^ R(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t t1 = hh + (R(e, 6) ^ R(e, 11) ^ R(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            const uint32_t t2 = (R(a, 2) ^ R(a, 13) ^ R(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    void Add(const uint8_t* p, size_t n) {
        len += n;
        while (n) {
            const size_t t = std::min(n, 64 - nb);
            memcpy(buf + nb, p, t);
            nb += t; p += t; n -= t;
            if (nb == 64) { Block(buf); nb = 0; }
        }
    }
    void Done(uint8_t out[32]) {
        const uint64_t bits = len * 8;
        const uint8_t one = 0x80, zero = 0;
        Add(&one, 1);
        while (nb != 56) Add(&zero, 1);
        uint8_t l[8];
        for (int i = 0; i < 8; ++i) l[i] = (uint8_t)(bits >> (56 - 8 * i));
        Add(l, 8);
        for (int i = 0; i < 8; ++i) { out[4 * i] = (uint8_t)(h[i] >> 24); out[4 * i + 1] = (uint8_t)(h[i] >> 16); out[4 * i + 2] = (uint8_t)(h[i] >> 8); out[4 * i + 3] = (uint8_t)h[i]; }
    }
};

const char* const kMpcSha = "3947f3d70264920e8cf0514139b715ce6552c6389b0e364ca9fb443cca4f0d73";   // gpu-culling-rewrite.md 6

}  // namespace

void Sha256(const void* data, size_t n, uint8_t out[32]) {
    Sha s;
    s.Add((const uint8_t*)data, n);
    s.Done(out);
}

bool Sha256File(const wchar_t* path, char out[65]) {
    out[0] = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    Sha s;
    std::vector<uint8_t> b(1 << 16);
    DWORD got = 0;
    bool ok = true;
    for (;;) {
        if (!ReadFile(f, b.data(), (DWORD)b.size(), &got, nullptr)) { ok = false; break; }
        if (!got) break;
        s.Add(b.data(), got);
    }
    CloseHandle(f);
    if (!ok) return false;
    uint8_t d[32];
    s.Done(d);
    for (int i = 0; i < 32; ++i) sprintf_s(out + 2 * i, 3, "%02x", d[i]);
    return true;
}

bool MpcFileOk(char* why, size_t n) {
    wchar_t exe[MAX_PATH] = {};
    const DWORD l = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (!l || l >= MAX_PATH) { _snprintf_s(why, n, _TRUNCATE, "exe path unknown"); return false; }
    std::wstring p(exe);
    p = p.substr(0, p.find_last_of(L"\\/") + 1) + L"shader_dx12\\0x29ee0902.file";
    char h[65];
    if (!Sha256File(p.c_str(), h)) { _snprintf_s(why, n, _TRUNCATE, "shader_dx12\\0x29ee0902.file not readable"); return false; }
    if (strcmp(h, kMpcSha) != 0) {
        _snprintf_s(why, n, _TRUNCATE, "shader_dx12\\0x29ee0902.file is not the proven mpc build (sha256 %.8s..., need %.8s...)", h, kMpcSha);
        return false;
    }
    _snprintf_s(why, n, _TRUNCATE, "shader_dx12\\0x29ee0902.file = mpc build (sha256 %.8s...)", h);
    return true;
}

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    if (!g_log || !g_base || !info.iniPath) return false;
    if (GetPrivateProfileIntW(L"Profiler", L"MeshletProbe", 0, info.iniPath) == 0) return false;
    if (!info.knownBuild) {
        g_log("MLPC: refused (unknown build; the recorder RVAs and MeshletSys offsets are v2.02)");
        return false;
    }
    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    g_imageEnd = g_base + nt->OptionalHeader.SizeOfImage;
    uint8_t* thunks = AllocNear(g_base, g_imageEnd);
    if (!thunks) {
        g_log("MLPC: no executable page within 2 GB of the image - off");
        return false;
    }
    uint8_t* entry[kNumFn];
    uint8_t bytes[kNumFn][5];
    int np = 0;
    size_t used = 0;
    char bad[160] = "";
    size_t bl = 0;
    for (const Site& s : kSites) {
        uint8_t* p = (uint8_t*)(g_base + s.rva);
        if (memcmp(p, kProl, 5) != 0) {
            bl += (size_t)_snprintf_s(bad + bl, sizeof bad - bl, _TRUNCATE, " %X(%s)", s.rva, p[0] == 0xE9 ? "already hooked" : "bytes");
            continue;
        }
        uint8_t* t = thunks + used;
        const size_t sz = fnprobe::BuildThunk(t, p, 5, &g_count[s.fn], 0x7FFFFFFFu, 0u, (const void*)kHooks[s.fn]);
        used += (sz + 15) & ~size_t(15);
        const intptr_t d = (intptr_t)t - (intptr_t)(p + 5);
        if (d < INT32_MIN || d > INT32_MAX) {
            bl += (size_t)_snprintf_s(bad + bl, sizeof bad - bl, _TRUNCATE, " %X(reach)", s.rva);
            continue;
        }
        entry[np] = p;
        bytes[np][0] = 0xE9;
        const int32_t rel = (int32_t)d;
        memcpy(&bytes[np][1], &rel, 4);
        ++np;
    }
    FlushInstructionCache(GetCurrentProcess(), thunks, used);
    const int w = np ? WriteAll(entry, bytes, np) : 0;
    if (w < 0) {
        g_log("MLPC: could not patch (%s) - off", w == -1 ? "thread snapshot failed" : "a thread stayed inside a prologue for 50 tries");
        return false;
    }
    g_log("MLPC: meshlet recorder probes on %d/%d (531948 MeshletResolveLOD, 5301FC MeshletPreculling, 53FD3C MeshletInstanceCulling; "
          "measurement only, every call)%s%s",
          w, (int)kNumFn, bl ? " - skipped:" : "", bad);
    if (w > 0) InterlockedExchange(&g_active, 1);
    return w > 0;
}

bool Active() { return g_active != 0; }

void OnFrame() { InterlockedIncrement64(&g_frame); }

uint32_t CallerOf(Fn fn, int k, const void* res, bool* exact) {
    if (exact) *exact = false;
    if (!g_active || fn >= kNumFn || k < 0) return 0;
    uint32_t r = 0;
    AcquireSRWLockShared(&g_lock);
    const std::vector<Call>& v = g_last[fn];
    if ((size_t)k < v.size()) {
        r = v[(size_t)k].caller;
        if (exact) *exact = res && v[(size_t)k].res == (uint64_t)(uintptr_t)res;
    }
    ReleaseSRWLockShared(&g_lock);
    return r;
}

void LogLine() {
    if (!g_active) return;
    std::map<uint64_t, Agg> agg;
    std::unordered_map<uint64_t, SeqStat> seq;
    uint64_t seqFrames, dupI, dupS, dupO, lod2;
    uint32_t pa, pb;
    AcquireSRWLockExclusive(&g_lock);
    agg.swap(g_agg);
    seq.swap(g_seq);
    seqFrames = g_seqFrames; dupI = g_dupIdent; dupS = g_dupSameSys; dupO = g_dupOtherSys; lod2 = g_lodFrames2;
    pa = g_dupPairA; pb = g_dupPairB;
    g_seqFrames = g_dupIdent = g_dupSameSys = g_dupOtherSys = g_lodFrames2 = 0;
    ReleaseSRWLockExclusive(&g_lock);
    const double fr = seqFrames ? (double)seqFrames : 1.0;
    g_log("MLPC: %llu CPU frames with meshlet recorder calls; hook calls %lld, faults %lld | LOD duplicate test: %llu frames with >= 2 resolves; "
          "pairs: identical inputs on the same MeshletSys %llu (last %X + %X), same MeshletSys + other camera/counts %llu, other MeshletSys %llu",
          (unsigned long long)seqFrames, (long long)g_hookCalls, (long long)g_hookFaults, (unsigned long long)lod2, (unsigned long long)dupI, pa, pb,
          (unsigned long long)dupS, (unsigned long long)dupO);
    for (const auto& kv : agg) {
        const Fn fn = (Fn)(kv.first >> 32);
        const uint32_t caller = (uint32_t)kv.first;
        const Agg& a = kv.second;
        char sys[96] = "";
        size_t o = 0;
        for (int i = 0; i < a.nsys; ++i) o += (size_t)_snprintf_s(sys + o, sizeof sys - o, _TRUNCATE, "%s%llX", i ? "," : "", (unsigned long long)a.sys[i]);
        const double n = a.calls ? (double)a.calls : 1.0;
        if (fn == kLod)
            g_log("MLPC:   LOD@%X %.2f/frame (%llu calls, %d thread%s) sys %s%s | levels %d..%d avg %.1f, roots %.0f, cull codes %.0f, param_11 %.0f%%",
                  caller, (double)a.calls / fr, (unsigned long long)a.calls, a.ntid, a.ntid == 1 ? "" : "s", sys, a.sysMore ? "+" : "",
                  a.levMin == INT_MAX ? 0 : a.levMin, a.levMax, a.levSum / n, a.rootSum / n, a.cullSum / n, 100.0 * (double)a.flag11 / n);
        else if (fn == kPre)
            g_log("MLPC:   PRE@%X %.2f/frame (%llu calls) sys %s%s | cnt avg %.0f max %d: 0 %llu, 1..1024 %llu, >1024 %llu | MPC tail chain recorded %llu "
                  "(MPCS skip applies to %llu = %.0f%%, cnt 0 %llu) | mode 4 %llu | commit vtbl+C8h -> %X",
                  caller, (double)a.calls / fr, (unsigned long long)a.calls, sys, a.sysMore ? "+" : "", a.cntSum / n, a.cntMax,
                  (unsigned long long)a.cnt0, (unsigned long long)a.cntSmall, (unsigned long long)a.cntBig, (unsigned long long)a.gate,
                  (unsigned long long)a.gateSmall, a.gate ? 100.0 * (double)a.gateSmall / (double)a.gate : 0.0, (unsigned long long)a.gateZero,
                  (unsigned long long)a.mode4, a.commit);
        else
            g_log("MLPC:   MIC@%X %.2f/frame (%llu calls) sys %s%s", caller, (double)a.calls / fr, (unsigned long long)a.calls, sys, a.sysMore ? "+" : "");
    }
    std::vector<std::pair<uint64_t, const SeqStat*>> sv;
    for (const auto& kv : seq) sv.push_back({kv.second.n, &kv.second});
    std::sort(sv.begin(), sv.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
    for (size_t i = 0; i < sv.size() && i < 3; ++i)
        g_log("MLPC:   sequence %zu: %.0f%% of frames: %s", i + 1, 100.0 * (double)sv[i].first / fr, sv[i].second->text.c_str());
}

}  // namespace mlprobe
