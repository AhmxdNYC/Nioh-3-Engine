// prefetch_harness.cpp - offline check of prefetch.cpp (PF trace prefetch) through readprobe.cpp's real read hooks
// (HkReadEx = site 631FD5, HkRead<2> = 31305A). src\readprobe.cpp and src\prefetch.cpp are #included. Not a game test.
// A fake package\ folder (3 x 24 MiB files with a known byte pattern + one file outside it) is read the way the game does:
//   - ResourceLoad style: a new overlapped handle per read (CreateFileW GENERIC_READ, share READ) -> ReadFileEx with a
//     completion routine -> SleepEx(0, TRUE) until it ran -> CloseHandle (handle values get reused);
//   - archive style: one overlapped handle per file, ReadFile + event + GetOverlappedResult.
// Checks: every game read returns the right bytes / result / count while PF records or prefetches (and no game open fails
// because of PF's own handles); Note() keeps the last error and ignores null OVERLAPPED / n = 0 / files outside package\;
// window 1 records and saves pf0 (round trip equal to the recording); window 2 (same loads) picks pf0, prefetches ahead
// of the game within the lead limit, and the game finds most of its reads already prefetched; window 3 (other loads)
// matches nothing and saves into the free slot pf1; PF = 2 records without prefetching and overwrites the matching
// slot; a corrupt trace is ignored; the replay stops when the game leaves the trace; run merging / repeats.
//   build+run: py -I tests\run_prefetch_harness.py  ->  "PREFETCH HARNESS: ALL OK"
#include "../src/readprobe.cpp"
#include "../src/prefetch.cpp"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace loadstats {
void NoteRead(int, uint32_t, bool, uint64_t) {}
uint64_t LastFailEvalQpc() { return 0; }
}  // namespace loadstats
namespace readfix {
int SrMode(const wchar_t*, int* i, bool* w) { if (i) *i = 0; if (w) *w = false; return 0; }
bool Wants(const wchar_t*) { return false; }
bool Start(const StartInfo&) { return false; }
int __stdcall Read(int, void* h, void* b, unsigned long n, unsigned long* g, void* o) { return ReadFile(h, b, n, g, (LPOVERLAPPED)o); }
}  // namespace readfix

static int g_fail = 0;
static void Check(bool c, const char* what) {
    printf("%s %s\n", c ? "OK  " : "FAIL", what);
    if (!c) ++g_fail;
}
static void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    printf("    log: ");
    vprintf(fmt, a);
    printf("\n");
    va_end(a);
}

constexpr uint64_t kFile = 24ull << 20;
static wchar_t g_root[MAX_PATH], g_pkgDir[MAX_PATH];
static inline uint8_t Pat(int file, uint64_t o) { return (uint8_t)((o * 131u + (uint64_t)file * 7u + (o >> 12)) & 0xFF); }
static void FilePath(int file, wchar_t* out) {
    if (file < 3) swprintf_s(out, MAX_PATH, L"%lsarc%d.fdata", g_pkgDir, file);
    else swprintf_s(out, MAX_PATH, L"%lsoutside.bin", g_root);
}

struct Rd {
    int file;
    uint64_t off;
    uint32_t len;
    bool ex;  // ReadFileEx (ResourceLoad) or ReadFile (archive layer)
};
static std::vector<Rd> Seq(uint32_t seed, int n, bool withOutside) {
    std::vector<Rd> v;
    uint32_t x = seed;
    auto rnd = [&]() { x = x * 1664525u + 1013904223u; return x >> 8; };
    static const uint32_t lens[] = {512, 1024, 4096, 65536, 65536, 300000, 1u << 20};
    for (int i = 0; i < n; ++i) {
        Rd r;
        r.file = (int)(rnd() % 3);
        r.len = lens[rnd() % 7];
        r.off = ((uint64_t)rnd() * 4096u) % (kFile - r.len);
        r.ex = (rnd() & 1) != 0;
        if (withOutside && i % 37 == 5) r.file = 3;
        v.push_back(r);
    }
    return v;
}

struct ExOv {
    OVERLAPPED ov;
    volatile LONG done;
    DWORD err, got;
};
static VOID CALLBACK Done(DWORD err, DWORD got, LPOVERLAPPED ov) {
    ExOv* e = (ExOv*)ov;  // OVERLAPPED is the first member
    e->err = err;
    e->got = got;
    e->done = 1;
}

static int g_badBytes = 0, g_badCalls = 0, g_badOpens = 0;
static HANDLE g_arc[4];
static std::vector<uint8_t> g_buf(1u << 20);

static void GameRead(const Rd& r) {
    wchar_t p[MAX_PATH];
    FilePath(r.file, p);
    uint8_t* b = g_buf.data();
    memset(b, 0xEE, r.len);
    if (r.ex) {  // 631F20 + 631FD5 + 716280 + 631C84
        HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            ++g_badOpens;
            return;
        }
        ExOv e{};
        e.ov.Offset = (DWORD)r.off;
        e.ov.OffsetHigh = (DWORD)(r.off >> 32);
        SetLastError(0);
        const BOOL ok = readprobe::HkReadEx(h, b, r.len, &e.ov, &Done);
        if (!ok || GetLastError() != 0) ++g_badCalls;
        for (int i = 0; i < 10000 && !e.done; ++i) SleepEx(ok ? 1 : 0, TRUE);
        if (!e.done || e.err || e.got != r.len) ++g_badCalls;
        CloseHandle(h);
    } else {  // 31305A: overlapped archive handle + per-read event + wait
        HANDLE h = g_arc[r.file];
        OVERLAPPED o{};
        o.Offset = (DWORD)r.off;
        o.OffsetHigh = (DWORD)(r.off >> 32);
        o.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        DWORD got = 0;
        BOOL ok = readprobe::HkRead<2>(h, b, r.len, nullptr, &o);
        if (!ok && GetLastError() == ERROR_IO_PENDING) ok = GetOverlappedResult(h, &o, &got, TRUE);
        else if (ok) got = (DWORD)o.InternalHigh;
        if (!ok || got != r.len) ++g_badCalls;
        CloseHandle(o.hEvent);
    }
    for (uint32_t i = 0; i < r.len; ++i)
        if (b[i] != Pat(r.file, r.off + i)) {
            ++g_badBytes;
            break;
        }
}

// one load window; the gate thread's Decide() is driven from here after `decideAfter` reads; `pace` ms between reads
static void Window(const std::vector<Rd>& seq, int decideAfter, int paceMs) {
    prefetch::OpenWindow();
    for (size_t i = 0; i < seq.size(); ++i) {
        GameRead(seq[i]);
        if ((int)i + 1 == decideAfter) {
            prefetch::Decide(false);
            Sleep(30);  // let the workers get ahead
        }
        if (paceMs) Sleep(paceMs);
    }
    prefetch::CloseWindow();
}

static bool Exists(int slot) {
    wchar_t p[MAX_PATH + 40];
    prefetch::SlotPath(slot, p, MAX_PATH + 40);
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}
static uint64_t C(prefetch::C c) { return prefetch::g_c[c].load(); }

int main() {
    // fake image: only the two IAT slots readprobe's hooks call through
    uint8_t* iat = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    readprobe::g_base = (uintptr_t)iat - 0x38E2000;
    *(void**)(iat + (readprobe::kIatReadFile - 0x38E2000)) = (void*)&ReadFile;
    *(void**)(iat + (readprobe::kIatReadFileEx - 0x38E2000)) = (void*)&ReadFileEx;
    readprobe::g_pf = true;

    GetTempPathW(MAX_PATH, g_root);
    wcscat_s(g_root, L"n3pf_harness\\");
    swprintf_s(g_pkgDir, L"%lspackage\\", g_root);
    CreateDirectoryW(g_root, nullptr);
    CreateDirectoryW(g_pkgDir, nullptr);
    for (int s = 0; s < 8; ++s) {
        wchar_t p[MAX_PATH + 40];
        swprintf_s(p, L"%lsNioh3EngineFix_pf%d.bin", g_root, s);
        DeleteFileW(p);
    }
    {
        std::vector<uint8_t> blk(1u << 20);
        for (int f = 0; f < 4; ++f) {
            wchar_t p[MAX_PATH];
            FilePath(f, p);
            HANDLE h = CreateFileW(p, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            for (uint64_t o = 0; o < kFile; o += blk.size()) {
                for (size_t i = 0; i < blk.size(); ++i) blk[i] = Pat(f, o + i);
                DWORD w = 0;
                WriteFile(h, blk.data(), (DWORD)blk.size(), &w, nullptr);
            }
            CloseHandle(h);
        }
    }
    wchar_t fin[1024] = {0};
    HANDLE d = CreateFileW(g_pkgDir, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    GetFinalPathNameByHandleW(d, fin, 1024, 0);
    CloseHandle(d);
    prefetch::g_log = &Log;
    Check(prefetch::Init(1, 4, 8, 4, 50, fin, g_root), "Init (mode 1, 4 threads, lead 8 MB, 4 slots)");
    for (int f = 0; f < 4; ++f) {
        wchar_t p[MAX_PATH];
        FilePath(f, p);
        g_arc[f] = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    // Note(): last error kept, null OVERLAPPED / n = 0 ignored, nothing recorded outside a window
    {
        OVERLAPPED o{};
        SetLastError(0x1234);
        prefetch::Note(13, g_arc[0], &o, 4096);
        Check(GetLastError() == 0x1234 && prefetch::g_nRec.load() == 0, "Note outside a window: no record, last error kept");
        prefetch::OpenWindow();
        SetLastError(0x4321);
        prefetch::Note(13, g_arc[0], &o, 4096);
        Check(GetLastError() == 0x4321 && prefetch::g_nRec.load() == 1, "Note in a window: recorded, last error kept");
        prefetch::Note(13, g_arc[0], nullptr, 4096);
        prefetch::Note(13, g_arc[0], &o, 0);
        prefetch::Note(13, g_arc[3], &o, 4096);  // outside package\ .
        Check(prefetch::g_nRec.load() == 1 && C(prefetch::cOther) == 1, "Note ignores null OVERLAPPED, n = 0 and files outside package");
        prefetch::CloseWindow();
        Check(!Exists(0), "a window with < 256 reads is not saved");
    }

    const std::vector<Rd> A = Seq(7, 600, true), B = Seq(99, 600, false);
    size_t pkgA = 0;
    uint64_t bytesA = 0;
    for (const Rd& r : A)
        if (r.file < 3) ++pkgA, bytesA += r.len;

    // window 1: records A, saves pf0
    Window(A, 30, 0);
    Check(g_badBytes == 0 && g_badCalls == 0 && g_badOpens == 0, "W1 game reads exact (bytes, results, counts, opens)");
    Check(prefetch::g_nRec.load() == pkgA && C(prefetch::cPkgBytes) == bytesA, "W1 recorded every package read, none outside");
    Check(prefetch::g_R == nullptr && Exists(0), "W1 no trace -> no prefetch; saved pf0");
    {
        prefetch::Trace t;
        bool same = prefetch::LoadTrace(0, t) && t.e.size() == pkgA && t.rel.size() == 3;
        for (size_t i = 0; same && i < t.e.size(); ++i)
            same = t.e[i].hash == prefetch::g_rec[i].hash && t.e[i].off == prefetch::g_rec[i].off && t.e[i].len == prefetch::g_rec[i].len;
        Check(same, "pf0 round trip == the recording (3 files)");
    }

    // window 2: same loads -> pf0 picked, prefetch ahead, game hits prefetched ranges; pace the game so the workers lead
    Window(A, 30, 2);
    const uint64_t m2 = C(prefetch::cMatch), done2 = C(prefetch::cHitDone);
    Check(g_badBytes == 0 && g_badCalls == 0 && g_badOpens == 0, "W2 game reads exact while prefetching (no game open failed)");
    // reads 25..30 come after the 24 that pick the trace and before the (simulated) gate tick: neither counted nor missed
    Check(prefetch::g_pick == 0 && m2 + 6 >= pkgA && C(prefetch::cMiss) == 0, "W2 picked pf0, every counted package read is in it");
    Check(C(prefetch::cBytes) > 0 && C(prefetch::cRuns) > 0, "W2 prefetched bytes");
    Check(done2 * 100 >= m2 * 60, "W2 >= 60 % of the game's reads were prefetched before the read");
    Check(C(prefetch::cLeadMax) <= (8ull << 20) + prefetch::kRunMax, "W2 lead <= PFLeadMB + one run");
    printf("    W2: matched %llu, prefetched-before %llu (%.0f %%), busy %llu, todo %llu, %.1f MB in %llu runs (%llu skipped), lead waits %llu\n",
           (unsigned long long)m2, (unsigned long long)done2, prefetch::Pct(done2, m2), (unsigned long long)C(prefetch::cHitBusy),
           (unsigned long long)C(prefetch::cHitTodo), (double)C(prefetch::cBytes) / 1048576.0, (unsigned long long)C(prefetch::cRuns),
           (unsigned long long)C(prefetch::cRunSkip), (unsigned long long)C(prefetch::cLeadWait));

    // window 3: other loads -> nothing matches, saved into the free slot pf1
    Window(B, 30, 0);
    Check(g_badBytes == 0 && g_badCalls == 0, "W3 game reads exact");
    Check(prefetch::g_pick < 0 && C(prefetch::cBytes) == 0 && Exists(1), "W3 no match -> no prefetch; saved pf1");

    // PF = 2: record only; the matching slot pf0 is overwritten, no prefetch
    prefetch::g_mode = 2;
    Window(A, 30, 0);
    Check(prefetch::g_pick == 0 && C(prefetch::cBytes) == 0 && !Exists(2), "PF=2: pf0 matched, no prefetch, pf0 overwritten (no pf2)");
    prefetch::g_mode = 1;

    // corrupt pf1 -> ignored (only pf0 loads), still exact
    {
        wchar_t p[MAX_PATH + 40];
        prefetch::SlotPath(1, p, MAX_PATH + 40);
        HANDLE h = CreateFileW(p, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        LARGE_INTEGER z;
        z.QuadPart = 100;
        SetFilePointerEx(h, z, nullptr, FILE_BEGIN);
        SetEndOfFile(h);
        CloseHandle(h);
        prefetch::OpenWindow();
        Check(prefetch::g_traces.size() == 1 && prefetch::g_traces[0]->slot == 0, "a truncated trace (pf1) is ignored");
        prefetch::CloseWindow();
    }

    // replay stop: the first 30 reads are A's, the rest B's -> match rate falls under 25 % -> stop
    {
        std::vector<Rd> mix(A.begin(), A.begin() + 30);
        std::vector<Rd> more = Seq(1234, 4200, false);
        for (Rd& r : more) r.ex = false, r.len = 512;  // cheap reads
        mix.insert(mix.end(), more.begin(), more.end());
        prefetch::g_lead = 1ull << 20;  // keep the workers slow so the stop matters
        Window(mix, 30, 0);
        Check(g_badBytes == 0 && g_badCalls == 0, "stop test: game reads exact");
        Check(strstr(prefetch::g_stopWhy, "match rate") != nullptr, "replay stopped when the game left the trace");
        prefetch::g_lead = 8ull << 20;
    }

    // runs: adjacent reads merge, repeats map to the first run, far reads split
    {
        prefetch::Trace t;
        t.rel[11] = L"x";
        const prefetch::Ent e[] = {{11, 0, 4096, 0}, {11, 4096, 4096, 0}, {11, 100000, 512, 0}, {11, 0, 4096, 0}, {11, 50ull << 20, 4096, 0}};
        t.e.assign(e, e + 5);
        prefetch::Replay R;
        R.t = &t;
        prefetch::Build(R);
        Check(R.runs.size() == 2 && R.runs[0].off == 0 && R.runs[0].end == 100512 && R.runs[0].nEnt == 3 && R.e2r[3] == 0 &&
                  R.e2r[4] == 1 && R.prefix[2] == 100512 + 4096,
              "Build: merge within 256 KiB, repeat -> same run, far -> new run, prefix bytes");
    }

    for (HANDLE h : g_arc) CloseHandle(h);
    for (int s = 0; s < 8; ++s) {
        wchar_t p[MAX_PATH + 40];
        prefetch::SlotPath(s, p, MAX_PATH + 40);
        DeleteFileW(p);
    }
    for (int f = 0; f < 4; ++f) {
        wchar_t p[MAX_PATH];
        FilePath(f, p);
        DeleteFileW(p);
    }
    RemoveDirectoryW(g_pkgDir);
    RemoveDirectoryW(g_root);
    puts(g_fail ? "PREFETCH HARNESS: FAILED" : "PREFETCH HARNESS: ALL OK");
    return g_fail ? 1 : 0;
}
