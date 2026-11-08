// prefetch.cpp - PF trace prefetch ([Patches] PF = 0 by default). Contract and safety argument in prefetch.h, design and
// Ally data in notes\loading-ally-fixed.md 8.8.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gamerva.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "prefetch.h"

namespace prefetch {
namespace {

// ---- v2.02 addresses (RVAs) ----
constexpr uint32_t kGameMainPtr = 0x4759910, kGm58A = 0x58A;

constexpr int kFirst = 24;                  // package reads that pick a trace
constexpr uint32_t kMagic = 0x4650334E;     // 'N3PF'
constexpr uint32_t kVer = 1;
constexpr uint32_t kRecCap = 262144;        // reads recorded per window (24 B each)
constexpr uint32_t kMinRec = 256;           // fewer -> not saved (a menu blip, not a load)
constexpr int kMaxFiles = 2048, kPathMax = 400;
constexpr uint64_t kGap = 256ull << 10;     // merge two reads of a file into one run when the gap is <= this
constexpr uint64_t kRunMax = 8ull << 20;    // largest run
constexpr DWORD kPiece = 4u << 20;          // largest single prefetch read
constexpr int kRunLook = 64;                // a run stays open for merging for this many newer runs
constexpr int kMaxSlots = 8;
constexpr uint32_t kCheckEvery = 2048;      // replay sanity check period (game reads)
constexpr int kStopPct = 25;                // replay stops below this match rate

struct Ent {
    uint64_t hash, off;
    uint32_t len, ms;
};
static_assert(sizeof(Ent) == 24, "trace entry");

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
int g_mode = 0, g_threads = 4, g_slots = 4, g_matchPct = 50;
uint64_t g_lead = 768ull << 20;
wchar_t g_pkg[1024] = {0};  // final path of <game>\package\ with the trailing backslash
size_t g_pkgLen = 0;
wchar_t g_dir[MAX_PATH] = {0};  // ini folder with the trailing backslash (trace files)
uint64_t g_qpcFreq = 1;

struct IoSb {
    union {
        LONG Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
};
using NtQifFn = LONG(WINAPI*)(HANDLE, IoSb*, PVOID, ULONG, ULONG);
NtQifFn g_ntQif = nullptr;

uint64_t Qpc() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)t.QuadPart;
}
double Sec(uint64_t a, uint64_t b) { return (double)(b - a) / (double)g_qpcFreq; }
double Pct(uint64_t a, uint64_t b) { return b ? 100.0 * (double)a / (double)b : 0.0; }
constexpr double kMB = 1048576.0;

bool SafeCopy(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uint64_t Fnv(const wchar_t* s) {  // case-insensitive (lower-case ASCII) FNV-1a 64 of a package-relative path
    uint64_t h = 0xCBF29CE484222325ull;
    for (; *s; ++s) {
        wchar_t c = *s;
        if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
        if (c == L'/') c = L'\\';
        h = (h ^ (uint64_t)c) * 0x100000001B3ull;
    }
    return h ? h : 1;
}
inline uint64_t Key(uint64_t hash, uint64_t off) { return hash ^ (off * 0x9E3779B97F4A7C15ull) ^ (off >> 29); }

// ---- counters (per window; zeroed at the open) ----
enum C {
    cObs,       // Note() calls with an OVERLAPPED inside a window
    cPkg,       // ... on a package file (recorded)
    cPkgEx,     // ... of them at ReadFileEx 631FD5
    cPkgBytes,
    cNoId,      // file id query failed
    cResolving, // another thread was resolving the file's path
    cOther,     // not a package file
    cMatch,     // replay: game read found in the trace
    cMiss,      // replay: game read not in the trace
    cHitDone,   // ... and its run had been prefetched already
    cHitBusy,   // ... its run was being read
    cHitTodo,   // ... its run was not issued yet
    cRuns, cRunSkip, cRunFail, cBytes, cBusyTicks, cLeadWait, cLeadMax,
    cN
};
std::atomic<uint64_t> g_c[cN];
inline void Add(C c, uint64_t v = 1) { g_c[c].fetch_add(v, std::memory_order_relaxed); }
void Max(C c, uint64_t v) {
    uint64_t cur = g_c[c].load(std::memory_order_relaxed);
    while (v > cur && !g_c[c].compare_exchange_weak(cur, v, std::memory_order_relaxed)) {}
}

// ---- file table: file id -> package-relative path (once per file per process) ----
struct FEnt {
    std::atomic<uint64_t> key;  // file id + 1 (0 = empty)
    std::atomic<uint32_t> st;   // 0 resolving, 1 package file, 2 other
    uint64_t hash;
};
constexpr int kFBits = 12, kFSlots = 1 << kFBits;
FEnt g_f[kFSlots];
wchar_t g_rel[kMaxFiles][kPathMax];
uint64_t g_relHash[kMaxFiles];
std::atomic<int> g_nRel{0};

void Resolve(FEnt& e, HANDLE h) {
    wchar_t path[1024];
    const DWORD n = GetFinalPathNameByHandleW(h, path, 1024, 0);
    uint32_t st = 2;
    if (n && n < 1024 && g_pkgLen && _wcsnicmp(path, g_pkg, g_pkgLen) == 0 && path[g_pkgLen]) {
        const wchar_t* rel = path + g_pkgLen;
        e.hash = Fnv(rel);
        st = 1;
        if (wcslen(rel) < kPathMax) {
            const int i = g_nRel.fetch_add(1, std::memory_order_relaxed);
            if (i < kMaxFiles) {
                wcsncpy_s(g_rel[i], rel, _TRUNCATE);
                g_relHash[i] = e.hash;
            }
        }
    }
    e.st.store(st, std::memory_order_release);
}

// The package file a handle refers to, or nullptr (not a package file / id unknown / being resolved).
const FEnt* FileOf(HANDLE h) {
    IoSb sb{};
    uint64_t id = 0;
    if (!g_ntQif || g_ntQif(h, &sb, &id, sizeof(id), 6 /*FileInternalInformation*/) < 0 || id == ~0ull) {
        Add(cNoId);
        return nullptr;
    }
    const uint64_t k = id + 1;
    uint32_t i = (uint32_t)((k * 0x9E3779B97F4A7C15ull) >> (64 - kFBits));
    for (int t = 0; t < 128; ++t, i = (i + 1) & (kFSlots - 1)) {
        FEnt& e = g_f[i];
        uint64_t cur = e.key.load(std::memory_order_acquire);
        if (!cur) {
            if (!e.key.compare_exchange_strong(cur, k, std::memory_order_acq_rel) && cur != k) continue;
            if (cur != k) Resolve(e, h);  // we inserted it
        } else if (cur != k) {
            continue;
        }
        const uint32_t st = e.st.load(std::memory_order_acquire);
        if (st == 1) return &e;
        Add(st == 0 ? cResolving : cOther);
        return nullptr;
    }
    Add(cOther);
    return nullptr;
}
const wchar_t* RelOf(uint64_t hash) {
    const int n = g_nRel.load(std::memory_order_acquire) < kMaxFiles ? g_nRel.load(std::memory_order_acquire) : kMaxFiles;
    for (int i = 0; i < n; ++i)
        if (g_relHash[i] == hash && g_rel[i][0]) return g_rel[i];
    return nullptr;
}

// ---- traces on disk ----
struct Trace {
    int slot = -1;
    FILETIME mt{};
    std::unordered_map<uint64_t, std::wstring> rel;  // path hash -> package-relative path
    std::vector<Ent> e;
    std::unordered_set<uint64_t> keys;  // Key(hash, off) of every entry (trace choice)
};
struct Hdr {
    uint32_t magic, ver, nPaths, nEnts, win, mode;
    uint64_t created;
};

void SlotPath(int slot, wchar_t* out, size_t cap) { swprintf_s(out, cap, L"%lsNioh3EngineFix_pf%d.bin", g_dir, slot); }

bool LoadTrace(int slot, Trace& t) {
    wchar_t p[MAX_PATH + 40];
    SlotPath(slot, p, MAX_PATH + 40);
    FILE* f = nullptr;
    if (_wfopen_s(&f, p, L"rb") || !f) return false;
    bool ok = false;
    Hdr h{};
    if (fread(&h, sizeof(h), 1, f) == 1 && h.magic == kMagic && h.ver == kVer && h.nPaths <= kMaxFiles && h.nEnts <= kRecCap &&
        h.nEnts >= kMinRec) {
        ok = true;
        for (uint32_t i = 0; ok && i < h.nPaths; ++i) {
            uint64_t hash = 0;
            uint16_t len = 0;
            wchar_t buf[kPathMax];
            if (fread(&hash, 8, 1, f) != 1 || fread(&len, 2, 1, f) != 1 || !len || len >= kPathMax || fread(buf, 2, len, f) != len) {
                ok = false;
                break;
            }
            buf[len] = 0;
            if (Fnv(buf) != hash) ok = false;  // corrupt or another hash scheme
            else t.rel[hash] = buf;
        }
        if (ok) {
            t.e.resize(h.nEnts);
            ok = fread(t.e.data(), sizeof(Ent), h.nEnts, f) == h.nEnts;
        }
        if (ok)
            for (const Ent& x : t.e)
                if (!t.rel.count(x.hash) || !x.len) {
                    ok = false;
                    break;
                }
    }
    fclose(f);
    if (!ok) {
        t = Trace();
        return false;
    }
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExW(p, GetFileExInfoStandard, &fa)) t.mt = fa.ftLastWriteTime;
    t.slot = slot;
    t.keys.reserve(t.e.size() * 2);
    for (const Ent& x : t.e) t.keys.insert(Key(x.hash, x.off));
    return true;
}

bool SaveTrace(int slot, const Ent* e, uint32_t n, int win) {
    std::unordered_map<uint64_t, const wchar_t*> used;
    for (uint32_t i = 0; i < n; ++i)
        if (!used.count(e[i].hash)) {
            const wchar_t* r = RelOf(e[i].hash);
            if (!r) return false;
            used[e[i].hash] = r;
        }
    wchar_t p[MAX_PATH + 40], tmp[MAX_PATH + 48];
    SlotPath(slot, p, MAX_PATH + 40);
    swprintf_s(tmp, MAX_PATH + 48, L"%ls.tmp", p);
    FILE* f = nullptr;
    if (_wfopen_s(&f, tmp, L"wb") || !f) return false;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    Hdr h{kMagic, kVer, (uint32_t)used.size(), n, (uint32_t)win, (uint32_t)g_mode, (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime};
    bool ok = fwrite(&h, sizeof(h), 1, f) == 1;
    for (const auto& u : used) {
        const uint16_t len = (uint16_t)wcslen(u.second);
        ok = ok && fwrite(&u.first, 8, 1, f) == 1 && fwrite(&len, 2, 1, f) == 1 && fwrite(u.second, 2, len, f) == len;
    }
    ok = ok && fwrite(e, sizeof(Ent), n, f) == n;
    ok = (fclose(f) == 0) && ok;
    if (!ok || !MoveFileExW(tmp, p, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp);
        return false;
    }
    return true;
}

// ---- replay ----
struct Run {
    uint64_t hash, off, end;
    uint32_t nEnt;
};
struct Replay {
    const Trace* t = nullptr;
    std::vector<Run> runs;
    std::vector<uint64_t> prefix;            // bytes of runs [0, i)
    std::unordered_map<uint64_t, int> k2e;   // Key -> first entry with that key
    std::vector<int> e2r;                    // entry -> run
    std::atomic<uint8_t>* runSt = nullptr;   // 0 todo, 1 reading, 2 done, 3 failed, 4 skipped (already read by the game)
    std::atomic<uint8_t>* entSeen = nullptr;
    std::atomic<uint32_t>* runSeen = nullptr;
    std::unordered_map<uint64_t, HANDLE> fh;  // path hash -> private handle (nullptr = failed)
    SRWLOCK fl = SRWLOCK_INIT;
    ~Replay() {
        delete[] runSt;
        delete[] entSeen;
        delete[] runSeen;
        for (auto& x : fh)
            if (x.second) CloseHandle(x.second);
    }
};

void Build(Replay& R) {
    const std::vector<Ent>& e = R.t->e;
    R.e2r.assign(e.size(), -1);
    R.k2e.reserve(e.size() * 2);
    std::unordered_map<uint64_t, int> last;  // path hash -> its newest run
    for (size_t i = 0; i < e.size(); ++i) {
        const Ent& x = e[i];
        const uint64_t k = Key(x.hash, x.off);
        auto it = R.k2e.find(k);
        if (it != R.k2e.end()) {  // a repeat: same run as the first time
            R.e2r[i] = R.e2r[(size_t)it->second];
            if (x.off + x.len > R.runs[(size_t)R.e2r[i]].end && x.off + x.len - R.runs[(size_t)R.e2r[i]].off <= kRunMax)
                R.runs[(size_t)R.e2r[i]].end = x.off + x.len;
            continue;
        }
        R.k2e.emplace(k, (int)i);
        auto lr = last.find(x.hash);
        int r = -1;
        if (lr != last.end() && (int)R.runs.size() - lr->second <= kRunLook) {
            Run& u = R.runs[(size_t)lr->second];
            const uint64_t ne = x.off + x.len > u.end ? x.off + x.len : u.end;
            if (x.off >= u.off && x.off <= u.end + kGap && ne - u.off <= kRunMax) {
                u.end = ne;
                r = lr->second;
            }
        }
        if (r < 0) {
            R.runs.push_back(Run{x.hash, x.off, x.off + x.len, 0});
            r = (int)R.runs.size() - 1;
            last[x.hash] = r;
        }
        R.runs[(size_t)r].nEnt++;
        R.e2r[i] = r;
    }
    R.prefix.assign(R.runs.size() + 1, 0);
    for (size_t r = 0; r < R.runs.size(); ++r) R.prefix[r + 1] = R.prefix[r] + (R.runs[r].end - R.runs[r].off);
    R.runSt = new std::atomic<uint8_t>[R.runs.size() + 1]();
    R.runSeen = new std::atomic<uint32_t>[R.runs.size() + 1]();
    R.entSeen = new std::atomic<uint8_t>[e.size() + 1]();
}

// ---- window state ----
std::atomic<bool> g_open{false};
std::atomic<int> g_inNote{0};
enum Phase { pChoose = 0, pReplay = 1, pNoReplay = 2 };
std::atomic<int> g_phase{pChoose};
uint64_t g_t0 = 0;
int g_win = 0;
Ent* g_rec = nullptr;
std::atomic<uint32_t> g_nRec{0};
std::atomic<uint32_t> g_nFirst{0};
uint64_t g_first[kFirst][2];
std::atomic<bool> g_firstReady[kFirst];
std::vector<Trace*> g_traces;
int g_pick = -1, g_pickHits = 0;    // chosen slot and its matches among the first kFirst reads
Replay* g_R = nullptr;             // published before g_phase = pReplay
std::atomic<int> g_next{0}, g_maxRun{-1};
std::atomic<bool> g_stop{false};
const char* g_stopWhy = "window end";
HANDLE g_workers[16];
int g_nWorkers = 0;
uint64_t g_replayT0 = 0;
std::atomic<uint32_t> g_obsSinceReplay{0};

void Progress(Replay& R, uint64_t hash, uint64_t off) {
    auto it = R.k2e.find(Key(hash, off));
    if (it == R.k2e.end() || R.t->e[(size_t)it->second].hash != hash || R.t->e[(size_t)it->second].off != off) {
        Add(cMiss);
        return;
    }
    Add(cMatch);
    const int r = R.e2r[(size_t)it->second];
    const uint8_t st = R.runSt[r].load(std::memory_order_acquire);
    Add(st == 2 ? cHitDone : st == 1 ? cHitBusy : cHitTodo);
    if (!R.entSeen[it->second].exchange(1, std::memory_order_acq_rel)) R.runSeen[r].fetch_add(1, std::memory_order_acq_rel);
    int cur = g_maxRun.load(std::memory_order_relaxed);
    while (r > cur && !g_maxRun.compare_exchange_weak(cur, r, std::memory_order_acq_rel)) {}
}

HANDLE FileHandle(Replay& R, uint64_t hash) {
    AcquireSRWLockShared(&R.fl);
    auto it = R.fh.find(hash);
    const bool have = it != R.fh.end();
    HANDLE h = have ? it->second : nullptr;
    ReleaseSRWLockShared(&R.fl);
    if (have) return h;
    AcquireSRWLockExclusive(&R.fl);
    it = R.fh.find(hash);
    if (it != R.fh.end()) {
        h = it->second;
    } else {
        h = nullptr;
        auto rp = R.t->rel.find(hash);
        if (rp != R.t->rel.end() && g_pkgLen + rp->second.size() < 1024) {
            wchar_t full[1024];
            swprintf_s(full, 1024, L"%ls%ls", g_pkg, rp->second.c_str());
            // read-only, synchronous; shares everything so no open of the game can fail because of this handle
            h = CreateFileW(full, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE) h = nullptr;
        }
        R.fh[hash] = h;
    }
    ReleaseSRWLockExclusive(&R.fl);
    return h;
}

DWORD WINAPI Worker(LPVOID) {
    Replay& R = *g_R;
    uint8_t* buf = (uint8_t*)VirtualAlloc(nullptr, kPiece, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buf) return 0;
    const int nRuns = (int)R.runs.size();
    while (!g_stop.load(std::memory_order_acquire)) {
        int r = g_next.load(std::memory_order_acquire);
        if (r >= nRuns) break;
        int p = g_maxRun.load(std::memory_order_acquire);
        if (p < 0) p = 0;
        const uint64_t ahead = R.prefix[(size_t)r] > R.prefix[(size_t)p] ? R.prefix[(size_t)r] - R.prefix[(size_t)p] : 0;
        if (ahead > g_lead) {
            Add(cLeadWait);
            Sleep(2);
            continue;
        }
        if (!g_next.compare_exchange_weak(r, r + 1, std::memory_order_acq_rel)) continue;
        Max(cLeadMax, ahead);
        const Run& u = R.runs[(size_t)r];
        if (R.runSeen[r].load(std::memory_order_acquire) >= u.nEnt) {  // the game already read all of it
            R.runSt[r].store(4, std::memory_order_release);
            Add(cRunSkip);
            continue;
        }
        R.runSt[r].store(1, std::memory_order_release);
        const uint64_t t0 = Qpc();
        HANDLE h = FileHandle(R, u.hash);
        bool ok = h != nullptr;
        for (uint64_t o = u.off; ok && o < u.end && !g_stop.load(std::memory_order_relaxed);) {
            const DWORD n = (DWORD)(u.end - o < kPiece ? u.end - o : kPiece);
            OVERLAPPED ov{};
            ov.Offset = (DWORD)o;
            ov.OffsetHigh = (DWORD)(o >> 32);
            DWORD got = 0;
            if (!ReadFile(h, buf, n, &got, &ov)) {
                ok = GetLastError() == ERROR_HANDLE_EOF;  // a trace range past a (patched) file's end: harmless
                break;
            }
            Add(cBytes, got);
            if (got < n) break;  // EOF
            o += got;
        }
        Add(cBusyTicks, Qpc() - t0);
        Add(cRuns);
        if (!ok) Add(cRunFail);
        R.runSt[r].store(ok ? 2 : 3, std::memory_order_release);
    }
    VirtualFree(buf, 0, MEM_RELEASE);
    return 0;
}

bool StartReplay(Trace* t) {
    Replay* R = new (std::nothrow) Replay();
    if (!R) return false;
    R->t = t;
    Build(*R);
    g_next.store(0);
    g_maxRun.store(-1);
    g_stop.store(false);
    g_R = R;
    for (int i = 0; i < kFirst; ++i)
        if (g_firstReady[i].load(std::memory_order_acquire)) Progress(*R, g_first[i][0], g_first[i][1]);
    g_replayT0 = Qpc();
    g_obsSinceReplay.store(0);
    g_phase.store(pReplay, std::memory_order_release);
    g_nWorkers = 0;
    for (int i = 0; i < g_threads && i < 16; ++i)
        if (HANDLE th = CreateThread(nullptr, 0, &Worker, nullptr, 0, nullptr)) g_workers[g_nWorkers++] = th;
    return true;
}

// Picks the trace (gate thread, once kFirst package reads are in, or at `force`).
void Decide(bool force) {
    if (g_phase.load(std::memory_order_acquire) != pChoose) return;
    const uint32_t nf = g_nFirst.load(std::memory_order_acquire) < kFirst ? g_nFirst.load(std::memory_order_acquire) : kFirst;
    if (nf < kFirst && !force) return;
    int got = 0;
    for (uint32_t i = 0; i < nf; ++i) got += g_firstReady[i].load(std::memory_order_acquire);
    int best = -1, bestHits = 0;
    for (size_t k = 0; k < g_traces.size(); ++k) {
        int hits = 0;
        for (uint32_t i = 0; i < nf; ++i)
            if (g_firstReady[i].load(std::memory_order_acquire) && g_traces[k]->keys.count(Key(g_first[i][0], g_first[i][1]))) ++hits;
        if (hits > bestHits) {
            bestHits = hits;
            best = (int)k;
        }
    }
    const bool match = best >= 0 && got > 0 && bestHits * 100 >= g_matchPct * got;
    g_pick = match ? g_traces[(size_t)best]->slot : -1;
    g_pickHits = bestHits;
    if (match && g_mode == 1 && StartReplay(g_traces[(size_t)best])) {
        g_log("Prefetch #%d: trace pf%d picked (%d of the first %d package reads in it, %zu reads, %zu files) - %d threads, lead %llu MB, "
              "%zu runs %.0f MB",
              g_win, g_pick, bestHits, got, g_traces[(size_t)best]->e.size(), g_traces[(size_t)best]->rel.size(), g_nWorkers,
              (unsigned long long)(g_lead >> 20), g_R->runs.size(), (double)g_R->prefix.back() / kMB);
    } else {
        g_phase.store(pNoReplay, std::memory_order_release);
        g_log("Prefetch #%d: %s (%zu traces, best %d of the first %d package reads)", g_win,
              g_mode == 2 ? (match ? "record only, a trace matches" : "record only") : g_traces.empty() ? "no trace yet - recording" : "no trace matches - recording",
              g_traces.size(), bestHits, got);
    }
}

void OpenWindow() {
    ++g_win;
    for (auto& c : g_c) c.store(0, std::memory_order_relaxed);
    g_nRec.store(0);
    g_nFirst.store(0);
    for (auto& f : g_firstReady) f.store(false);
    g_pick = -1;
    g_pickHits = 0;
    g_stopWhy = "window end";
    for (Trace* t : g_traces) delete t;
    g_traces.clear();
    for (int s = 0; s < g_slots; ++s) {
        Trace* t = new (std::nothrow) Trace();
        if (t && LoadTrace(s, *t)) g_traces.push_back(t);
        else delete t;
    }
    g_phase.store(pChoose, std::memory_order_release);
    g_t0 = Qpc();
    g_open.store(true, std::memory_order_seq_cst);
}

int SaveSlot(bool goodMatch) {
    if (goodMatch && g_pick >= 0) return g_pick;
    int oldest = 0;
    FILETIME best{0xFFFFFFFFu, 0xFFFFFFFFu};
    for (int s = 0; s < g_slots; ++s) {
        wchar_t p[MAX_PATH + 40];
        SlotPath(s, p, MAX_PATH + 40);
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (!GetFileAttributesExW(p, GetFileExInfoStandard, &fa)) return s;  // a free slot
        if (CompareFileTime(&fa.ftLastWriteTime, &best) < 0) {
            best = fa.ftLastWriteTime;
            oldest = s;
        }
    }
    return oldest;
}

void CloseWindow() {
    g_open.store(false, std::memory_order_seq_cst);
    while (g_inNote.load(std::memory_order_seq_cst)) YieldProcessor();
    const double secs = Sec(g_t0, Qpc());
    if (g_phase.load() == pChoose) Decide(true);
    g_stop.store(true, std::memory_order_release);
    // workers check g_stop between reads of <= 4 MiB; g_R must outlive them
    if (g_nWorkers) WaitForMultipleObjects((DWORD)g_nWorkers, g_workers, TRUE, INFINITE);
    for (int i = 0; i < g_nWorkers; ++i) CloseHandle(g_workers[i]);
    g_nWorkers = 0;
    auto v = [](C c) { return g_c[c].load(std::memory_order_relaxed); };
    const uint64_t match = v(cMatch), seen = match + v(cMiss);
    if (g_R) {
        g_log("Prefetch #%d: %.1f s | game package reads %llu (ReadFileEx %llu), %.0f MB | in trace pf%d %llu (%.0f %%): already "
              "prefetched %.0f %%, being read %.0f %%, not yet %.0f %% | prefetched %.0f MB in %llu runs (%llu skipped as read, %llu failed), "
              "workers busy %.1f s, lead waits %llu, max lead %.0f MB | stop: %s",
              g_win, secs, (unsigned long long)v(cPkg), (unsigned long long)v(cPkgEx), (double)v(cPkgBytes) / kMB, g_pick,
              (unsigned long long)match, Pct(match, seen), Pct(v(cHitDone), match), Pct(v(cHitBusy), match), Pct(v(cHitTodo), match),
              (double)v(cBytes) / kMB, (unsigned long long)v(cRuns), (unsigned long long)v(cRunSkip), (unsigned long long)v(cRunFail),
              (double)v(cBusyTicks) / (double)g_qpcFreq, (unsigned long long)v(cLeadWait), (double)v(cLeadMax) / kMB, g_stopWhy);
    } else {
        g_log("Prefetch #%d: %.1f s | game package reads %llu (ReadFileEx %llu), %.0f MB | no prefetch", g_win, secs,
              (unsigned long long)v(cPkg), (unsigned long long)v(cPkgEx), (double)v(cPkgBytes) / kMB);
    }
    if (v(cNoId) || v(cResolving)) g_log("Prefetch #%d: %llu reads without a file id, %llu while their path was resolved (not recorded)", g_win,
                                          (unsigned long long)v(cNoId), (unsigned long long)v(cResolving));
    const uint32_t n = g_nRec.load() < kRecCap ? g_nRec.load() : kRecCap;
    if (n >= kMinRec) {
        const bool good = g_R ? (seen && match * 2 >= seen) : g_pick >= 0;
        const int slot = SaveSlot(good);
        uint64_t bytes = 0;
        for (uint32_t i = 0; i < n; ++i) bytes += g_rec[i].len;
        const bool ok = SaveTrace(slot, g_rec, n, g_win);
        g_log("Prefetch #%d: recorded %u package reads (%.0f MB%s) -> Nioh3EngineFix_pf%d.bin %s", g_win, n, (double)bytes / kMB,
              g_nRec.load() > kRecCap ? ", list full" : "", slot, ok ? "saved" : "NOT saved (write failed)");
    }
    delete g_R;  // closes the private handles
    g_R = nullptr;
    for (Trace* t : g_traces) delete t;
    g_traces.clear();
    g_phase.store(pChoose, std::memory_order_release);
}

bool ReadGm58A(bool& on) {
    uintptr_t gm = 0;
    if (!SafeCopy(&gm, (const void*)(g_base + Grva(kGameMainPtr)), 8) || gm < 0x10000) return false;
    uint8_t b = 0;
    if (!SafeCopy(&b, (const void*)(gm + kGm58A), 1)) return false;
    on = b != 0;
    return true;
}

DWORD WINAPI Gate(LPVOID) {
    for (;;) {
        Sleep(10);
        bool on = false;
        if (!ReadGm58A(on)) continue;
        if (on && !g_open.load()) OpenWindow();
        else if (!on && g_open.load()) CloseWindow();
        else if (on && g_phase.load(std::memory_order_acquire) == pChoose) Decide(Sec(g_t0, Qpc()) > 30.0);
    }
}

std::atomic<bool> g_live{false};

// harness entry: Start without the ini, the image and the gate thread
bool Init(int mode, int threads, int leadMB, int slots, int matchPct, const wchar_t* pkgFinal, const wchar_t* dir) {
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcFreq = (uint64_t)fq.QuadPart;
    if (HMODULE nt = GetModuleHandleW(L"ntdll.dll")) g_ntQif = reinterpret_cast<NtQifFn>(GetProcAddress(nt, "NtQueryInformationFile"));
    if (!g_ntQif) return false;
    wcsncpy_s(g_pkg, pkgFinal, _TRUNCATE);
    g_pkgLen = wcslen(g_pkg);
    if (!g_pkgLen) return false;
    if (g_pkg[g_pkgLen - 1] != L'\\' && g_pkgLen + 1 < 1024) {
        g_pkg[g_pkgLen++] = L'\\';
        g_pkg[g_pkgLen] = 0;
    }
    wcsncpy_s(g_dir, dir, _TRUNCATE);
    g_mode = mode;
    g_threads = threads;
    g_lead = (uint64_t)leadMB << 20;
    g_slots = slots;
    g_matchPct = matchPct;
    if (!g_rec) g_rec = (Ent*)VirtualAlloc(nullptr, (size_t)kRecCap * sizeof(Ent), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!g_rec) return false;
    g_live.store(true, std::memory_order_release);
    return true;
}

int IniInt(const wchar_t* ini, const wchar_t* key, int def, int lo, int hi) {
    const int v = (int)GetPrivateProfileIntW(L"Patches", key, def, ini);
    return v < lo ? lo : v > hi ? hi : v;
}

}  // namespace

bool Wants(const wchar_t* iniPath) { return iniPath && (int)GetPrivateProfileIntW(L"Patches", L"PF", 0, iniPath) > 0; }

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    if (!g_log || !g_base || !info.iniPath) return false;
    const int mode = IniInt(info.iniPath, L"PF", 0, 0, 2);
    if (!mode) return false;
    if (!info.knownBuild) {
        g_log("Prefetch: refused (unknown build; read sites and G+58Ah are v2.02)");
        return false;
    }
    wchar_t dir[MAX_PATH];
    const DWORD dl = GetModuleFileNameW(nullptr, dir, MAX_PATH);
    if (!dl || dl >= MAX_PATH) return false;
    if (wchar_t* sl = wcsrchr(dir, L'\\')) sl[1] = 0;
    wcscat_s(dir, L"package");
    wchar_t fin[1024] = {0};
    HANDLE d = CreateFileW(dir, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (d != INVALID_HANDLE_VALUE) {
        const DWORD n = GetFinalPathNameByHandleW(d, fin, 1024, 0);
        if (!n || n >= 1024) fin[0] = 0;
        CloseHandle(d);
    }
    if (!fin[0]) {
        g_log("Prefetch: no final path for the package folder - off");
        return false;
    }
    wchar_t iniDir[MAX_PATH];
    wcsncpy_s(iniDir, info.iniPath, _TRUNCATE);
    if (wchar_t* sl = wcsrchr(iniDir, L'\\')) sl[1] = 0;
    else iniDir[0] = 0;
    const int th = IniInt(info.iniPath, L"PFThreads", 4, 1, 16);
    const int lead = IniInt(info.iniPath, L"PFLeadMB", 768, 64, 4096);
    const int slots = IniInt(info.iniPath, L"PFTraces", 4, 1, kMaxSlots);
    const int pct = IniInt(info.iniPath, L"PFMatchPct", 50, 10, 100);
    if (!Init(mode, th, lead, slots, pct, fin, iniDir)) {
        g_log("Prefetch: no NtQueryInformationFile or no %u KB for the read list - off", (unsigned)(kRecCap * sizeof(Ent) >> 10));
        return false;
    }
    if (HANDLE t = CreateThread(nullptr, 0, &Gate, nullptr, 0, nullptr)) CloseHandle(t);
    else {
        g_live.store(false);
        return false;
    }
    g_log("Prefetch: PF %d (%s), %d threads, lead %d MB, %d trace slots, match >= %d %% of the first %d package reads - package %ls", mode,
          mode == 1 ? "record + prefetch" : "record only", th, lead, slots, pct, kFirst, g_pkg);
    return true;
}

void Note(int site, void* hv, const void* ovv, unsigned long n) {
    if (!ovv || !n || !g_live.load(std::memory_order_relaxed)) return;
    g_inNote.fetch_add(1, std::memory_order_seq_cst);
    if (g_open.load(std::memory_order_seq_cst)) {
        const DWORD le = GetLastError();
        const OVERLAPPED* ov = (const OVERLAPPED*)ovv;
        Add(cObs);
        if (const FEnt* f = FileOf((HANDLE)hv)) {
            const uint64_t off = (uint64_t)ov->Offset | (uint64_t)ov->OffsetHigh << 32;
            const uint64_t hash = f->hash;
            Add(cPkg);
            if (site == 13) Add(cPkgEx);
            Add(cPkgBytes, n);
            const uint32_t i = g_nRec.fetch_add(1, std::memory_order_relaxed);
            if (i < kRecCap) g_rec[i] = Ent{hash, off, (uint32_t)n, (uint32_t)(Sec(g_t0, Qpc()) * 1000.0)};
            const int ph = g_phase.load(std::memory_order_acquire);
            if (ph == pChoose) {
                const uint32_t j = g_nFirst.fetch_add(1, std::memory_order_acq_rel);
                if (j < kFirst) {
                    g_first[j][0] = hash;
                    g_first[j][1] = off;
                    g_firstReady[j].store(true, std::memory_order_release);
                }
            } else if (ph == pReplay && g_R) {
                Progress(*g_R, hash, off);
                const uint32_t o = g_obsSinceReplay.fetch_add(1, std::memory_order_relaxed) + 1;
                if (o % kCheckEvery == 0 && !g_stop.load(std::memory_order_relaxed)) {
                    const uint64_t m = g_c[cMatch].load(std::memory_order_relaxed), s = m + g_c[cMiss].load(std::memory_order_relaxed);
                    if (s && m * 100 < (uint64_t)kStopPct * s) {
                        g_stopWhy = "match rate < 25 %";
                        g_stop.store(true, std::memory_order_release);
                    }
                }
            }
        }
        SetLastError(le);
    }
    g_inNote.fetch_sub(1, std::memory_order_seq_cst);
}

}  // namespace prefetch
