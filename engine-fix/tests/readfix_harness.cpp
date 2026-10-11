// readfix_harness.cpp - offline proof of readfix.cpp (RC read cache, AQ async reads, TSY/SH boot writes) and of
// readprobe.cpp's 7-argument save thunk. src\readprobe.cpp and src\readfix.cpp are #included. Not a game test.
//   build+run: py -I tests\run_readfix_harness.py  ->  "READFIX HARNESS: ALL OK"
// Checks (notes\loading-ally-fixed.md 8.3 "Proofs before shipping"):
//   - 8 threads x random reads (1 B .. 128 KiB, unaligned, block-crossing, at / past EOF) through readfix::Read at every
//     archive site, on synchronous and overlapped handles opened through the CreateFileW hook, a lazily classified
//     handle, and a file outside package\ (never served). Each result is compared with the real ReadFile on a twin handle
//     of the same kind: bytes, success / error code, byte count, Internal / InternalHigh, hEvent signalled, and the file
//     pointer of synchronous handles. Configurations: RC, AQ, RC+AQ, RC with a 16 MB cache (eviction), all off.
//   - close + reopen of another file with the same handle value never serves the old file's blocks
//   - a bad destination buffer returns the real error (ERROR_NOACCESS), not a crash
//   - the 7-argument save thunk forwards all arguments; TSY weight maths = SizeWorkerPools' float formula; SH refused off Wine
#include "../src/readprobe.cpp"
#include "../src/readfix.cpp"

#include <cstdarg>
#include <cstdio>
#include <initializer_list>
#include <vector>

namespace prefetch {  // 1.7.26 PF observes reads through readprobe.cpp; tested by run_prefetch_harness.py
bool Wants(const wchar_t*) { return false; }
bool Start(const StartInfo&) { return false; }
void Note(int, void*, const void*, unsigned long) {}
}  // namespace prefetch

namespace loadstats {
void NoteRead(int, uint32_t, bool, uint64_t) {}
uint64_t LastFailEvalQpc() { return 0; }
}  // namespace loadstats

static std::atomic<int> g_fail{0};
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

static uint8_t Pat(uint32_t file, uint64_t o) {
    uint64_t x = (o >> 3) * 0x9E3779B97F4A7C15ull + file * 0x632BE59BD9B4E019ull;
    x ^= x >> 29;
    return (uint8_t)(x >> (8 * (o & 7)));
}
static void MakeFile(const wchar_t* p, uint32_t id, uint64_t size) {
    HANDLE w = CreateFileW(p, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    std::vector<uint8_t> b(1 << 20);
    for (uint64_t o = 0; o < size; o += b.size()) {
        const DWORD n = (DWORD)(size - o < b.size() ? size - o : b.size());
        for (DWORD i = 0; i < n; ++i) b[i] = Pat(id, o + i);
        DWORD wr = 0;
        WriteFile(w, b.data(), n, &wr, nullptr);
    }
    CloseHandle(w);
}

using CreateFn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static CreateFn g_hookedOpen = nullptr;  // CreateFileW through readprobe's 7-arg thunk -> readfix's hook

struct Outcome {
    bool ok;
    DWORD got, err;
};
// normalizes an overlapped-capable call: TRUE, or FALSE+PENDING then wait on the event
static Outcome Finish(HANDLE h, BOOL r, DWORD le, OVERLAPPED& o, DWORD gotSync, bool useGor) {
    if (r) return {true, useGor ? (DWORD)o.InternalHigh : gotSync, 0};
    if (le != ERROR_IO_PENDING) return {false, 0, le};
    DWORD g = 0;
    if (useGor) {
        if (GetOverlappedResult(h, &o, &g, TRUE)) return {true, g, 0};
        return {false, 0, GetLastError()};
    }
    return {false, 0, le};
}

static const uint64_t kSizeA = 96ull << 20, kSizeB = 8ull << 20;
static wchar_t g_pathA[MAX_PATH], g_pathB[MAX_PATH], g_pathOut[MAX_PATH];
static std::atomic<uint64_t> g_ops{0}, g_cmp{0};

struct ThrArg {
    int id, iters;
    HANDLE lazyOvl;  // shared, opened without the hook
};

static DWORD WINAPI Worker(LPVOID pv) {
    ThrArg& a = *(ThrArg*)pv;
    uint64_t rng = 0x1234567ull * (a.id + 1);
    auto rnd = [&]() {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return rng;
    };
    // own handles through the hook (fromOpen) + real twins of the same kind
    HANDLE hs = g_hookedOpen(g_pathA, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0x80, nullptr);
    HANDLE ho = g_hookedOpen(g_pathA, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0x80 | FILE_FLAG_OVERLAPPED, nullptr);
    HANDLE hx = g_hookedOpen(g_pathOut, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0x80, nullptr);
    HANDLE ts = CreateFileW(g_pathA, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    HANDLE to = CreateFileW(g_pathA, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    HANDLE tx = CreateFileW(g_pathOut, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    std::vector<uint8_t> b1(140 * 1024), b2(140 * 1024);
    HANDLE evT = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    int bad = 0;
    for (int it = 0; it < a.iters && bad < 5; ++it) {
        // size: mostly game-like small reads, some up to 128 KiB (> RCMaxReadKB: pass-through)
        const uint64_t r0 = rnd();
        DWORD n = (r0 & 3) == 0 ? (DWORD)(rnd() % (128 * 1024)) + 1 : (r0 & 3) == 1 ? (DWORD)(rnd() % 4096) + 1 : (DWORD)(rnd() % 65536) + 1;
        const int kind = (int)(rnd() % 10);  // 0-3 sync own, 4-6 overlapped own, 7-8 lazy overlapped shared, 9 outside package
        const uint64_t fsize = kind == 9 ? kSizeB : kSizeA;
        uint64_t off;
        const uint64_t ro = rnd() % 100;
        if (ro < 4) off = fsize - (rnd() % 70000);                 // near / crossing EOF
        else if (ro < 6) off = fsize + (rnd() % 5000);             // past EOF
        else if (ro < 30) off = ((rnd() % (fsize >> 16)) << 16) - (rnd() % 64);  // around block edges
        else off = rnd() % fsize;
        if (off > (1ull << 40)) off = 0;
        int site;
        HANDLE h, tw;
        bool ovlH;
        if (kind <= 3) { site = (int)(rnd() % 3); site = site == 2 ? 6 : site; h = hs; tw = ts; ovlH = false; }
        else if (kind <= 6) { site = 2 + (int)(rnd() % 3); h = ho; tw = to; ovlH = true; }
        else if (kind <= 8) { site = 2 + (int)(rnd() % 3); h = a.lazyOvl; tw = to; ovlH = true; }
        else { site = (int)(rnd() % 5); h = hx; tw = tx; ovlH = false; }
        if (kind == 9) n = n % 8192 + 1;
        memset(b1.data(), 0xA5, n);
        memset(b2.data(), 0x5A, n);
        // the game's call
        HANDLE ev = ovlH ? CreateEventA(nullptr, TRUE, FALSE, nullptr) : nullptr;
        OVERLAPPED o{};
        o.Offset = (DWORD)off;
        o.OffsetHigh = (DWORD)(off >> 32);
        o.hEvent = ev;
        DWORD got = 0xDEAD;
        SetLastError(0x4242);
        const BOOL r = (BOOL)readfix::Read(site, h, b1.data(), n, ovlH ? nullptr : &got, &o);
        const DWORD le = GetLastError();
        Outcome g1;
        if (ovlH) {
            if (r || le == ERROR_IO_PENDING) {
                WaitForMultipleObjects(1, &ev, TRUE, INFINITE);  // FileReqWaitAll
                const bool ok = o.Internal == 0;
                g1 = {ok, ok ? (DWORD)o.InternalHigh : 0, ok ? 0 : (DWORD)o.Internal};
                if (!ok) {  // map like GetOverlappedResult would
                    DWORD gg = 0;
                    g1.err = GetOverlappedResult(h, &o, &gg, FALSE) ? 0 : GetLastError();
                }
                if (r && WaitForSingleObject(ev, 0) != WAIT_OBJECT_0) { ++bad; Check(false, "TRUE return without hEvent signalled"); }
            } else {
                g1 = {false, 0, le};
            }
        } else {
            g1 = r ? Outcome{true, got, 0} : Outcome{false, 0, le};
            if (r && le != 0x4242) { ++bad; Check(false, "successful read changed the last error"); }
            if (r && (o.Internal != 0 || o.InternalHigh != got)) { ++bad; Check(false, "sync read: Internal/InternalHigh"); }
        }
        // the reference: real ReadFile on the twin of the same kind
        OVERLAPPED o2{};
        o2.Offset = (DWORD)off;
        o2.OffsetHigh = (DWORD)(off >> 32);
        o2.hEvent = ovlH ? evT : nullptr;
        if (ovlH) ResetEvent(evT);
        DWORD got2 = 0;
        const BOOL r2 = ReadFile(tw, b2.data(), n, ovlH ? nullptr : &got2, &o2);
        const Outcome g2 = Finish(tw, r2, GetLastError(), o2, got2, ovlH);
        bool same = g1.ok == g2.ok && (g1.ok ? g1.got == g2.got : g1.err == g2.err);
        if (same && g1.ok) same = memcmp(b1.data(), b2.data(), g1.got) == 0;
        if (same && g1.ok && !ovlH && kind <= 3) {  // own synchronous handle: file pointer = offset + n
            LARGE_INTEGER z{}, c1{}, c2{};
            SetFilePointerEx(h, z, &c1, FILE_CURRENT);
            SetFilePointerEx(tw, z, &c2, FILE_CURRENT);
            same = c1.QuadPart == c2.QuadPart;
            if (!same) printf("    file pointer %lld vs %lld\n", c1.QuadPart, c2.QuadPart);
        }
        if (!same) {
            ++bad;
            printf("    MISMATCH t%d kind %d site %d off %llu n %lu: ok %d/%d got %lu/%lu err %lu/%lu\n", a.id, kind, site,
                   (unsigned long long)off, n, g1.ok, g2.ok, g1.got, g2.got, g1.err, g2.err);
        }
        if (ev) CloseHandle(ev);
        g_ops.fetch_add(1);
        g_cmp.fetch_add(same);
    }
    CloseHandle(evT);
    for (HANDLE x : {hs, ho, hx}) readfix::HkCloseHandle(x);
    for (HANDLE x : {ts, to, tx}) CloseHandle(x);
    if (bad) g_fail.fetch_add(1);
    return 0;
}

static void RunPhase(const char* name, int rc, int aq, int sr, bool priv, int iters) {
    readfix::g_rcMode = rc;
    readfix::g_aqMode = aq;
    readfix::g_srMode = sr;
    readfix::g_priv = priv;
    readfix::g_srActive.store(sr != 0);
    readfix::g_rcActive.store(rc != 0);
    readfix::g_aqActive.store(aq != 0);
    for (auto& c : readfix::g_c) c.store(0);
    g_ops = 0;
    g_cmp = 0;
    HANDLE lazy = CreateFileW(g_pathA, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    readfix::ResetHandle(lazy, false, false);  // the value may be in the table from an earlier phase: start unknown
    const int failBefore = g_fail.load();
    ThrArg args[8];
    HANDLE th[8];
    for (int i = 0; i < 8; ++i) {
        args[i] = {i, iters, lazy};
        th[i] = CreateThread(nullptr, 0, &Worker, &args[i], 0, nullptr);
    }
    WaitForMultipleObjects(8, th, TRUE, INFINITE);
    for (HANDLE t : th) CloseHandle(t);
    readfix::ResetHandle(lazy, false, false);
    CloseHandle(lazy);
    using namespace readfix;
    printf("    %s: %llu reads, served %llu (hits %llu, via block read %llu), blocks %llu, pass big %llu eof %llu inel %llu busy %llu, "
           "AQ %llu (max depth %u, direct %llu), SR %llu (passed %llu), private handles %llu\n",
           name, (unsigned long long)g_ops.load(), (unsigned long long)(g_c[cHit] + g_c[cMiss]), (unsigned long long)g_c[cHit].load(),
           (unsigned long long)g_c[cMiss].load(), (unsigned long long)g_c[cBlk].load(), (unsigned long long)g_c[cPassBig].load(),
           (unsigned long long)g_c[cPassEof].load(), (unsigned long long)g_c[cPassInel].load(), (unsigned long long)g_c[cPassBusy].load(),
           (unsigned long long)g_c[cAqQ].load(), g_aqMaxDepth.exchange(0), (unsigned long long)g_c[cAqDirect].load(),
           (unsigned long long)g_c[cSr].load(), (unsigned long long)g_c[cSrPass].load(), (unsigned long long)g_c[cPriv].load());
    char m[160];
    snprintf(m, sizeof(m), "%s: %llu/%llu reads identical to ReadFile (bytes, result, count, error, event, file pointer)", name,
             (unsigned long long)g_cmp.load(), (unsigned long long)g_ops.load());
    Check(g_fail.load() == failBefore && g_cmp.load() == g_ops.load(), m);
    if (rc) Check(g_c[cHit].load() > 0 && g_c[cMiss].load() > 0, "  RC served hits and block-read misses");
    if (rc) Check(g_c[cPassBig].load() > 0 && g_c[cPassEof].load() > 0 && g_c[cPassInel].load() > 0, "  RC passed big / EOF / not-eligible reads through");
    if (aq) Check(g_c[cAqQ].load() > 0, "  AQ queued reads");
    if (sr) Check(g_c[cSr].load() > 0 && g_c[cPriv].load() > 0, "  SR served reads through private synchronous handles");
    if (priv && !sr) Check(g_c[cPriv].load() > 0, "  private synchronous handles opened (block loads / AQ reads)");
    if (!rc && !aq && !sr) Check(g_c[cHit].load() == 0 && g_c[cAqQ].load() == 0 && g_c[cSr].load() == 0, "  all off: nothing served or queued");
}

static uintptr_t s_t7[7];
static HANDLE WINAPI Fake7(LPCWSTR a, DWORD b, DWORD c, LPSECURITY_ATTRIBUTES d, DWORD e, DWORD f, HANDLE g) {
    s_t7[0] = (uintptr_t)a; s_t7[1] = b; s_t7[2] = c; s_t7[3] = (uintptr_t)d; s_t7[4] = e; s_t7[5] = f; s_t7[6] = (uintptr_t)g;
    return (HANDLE)(uintptr_t)0x777;
}

int main() {
    // ---- fake image: IAT slots at the real RVAs; readprobe's thunk page near it ----
    const uint32_t kSize = 0x5300000;
    uint8_t* img = (uint8_t*)VirtualAlloc(nullptr, kSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!img) { puts("FAIL reserve"); return 1; }
    VirtualAlloc(img + 0x38E2000, 0x1000, MEM_COMMIT, PAGE_READWRITE);
    readprobe::g_base = readfix::g_base = (uintptr_t)img;
    *(void**)(img + readfix::kIatReadFile) = (void*)&ReadFile;
    *(void**)(img + readfix::kIatCreateFileW) = (void*)&CreateFileW;
    *(void**)(img + readfix::kIatCloseHandle) = (void*)&CloseHandle;
    readprobe::g_thunks = readprobe::AllocNear((uintptr_t)img, (uintptr_t)img + kSize);
    Check(readprobe::g_thunks != nullptr, "thunk page within 2 GB of the fake image");

    // ---- 1. the 7-argument save thunk ----
    {
        CreateFn t = (CreateFn)(void*)readprobe::NewSaveThunk7((const void*)&Fake7);
        SECURITY_ATTRIBUTES sa{};
        HANDLE r = t(L"x", 0x11111111, 0x22, &sa, 0x33, 0x44444444, (HANDLE)(uintptr_t)0x55);
        Check(r == (HANDLE)(uintptr_t)0x777 && s_t7[0] == (uintptr_t)L"x" && s_t7[1] == 0x11111111 && s_t7[2] == 0x22 &&
                  s_t7[3] == (uintptr_t)&sa && s_t7[4] == 0x33 && s_t7[5] == 0x44444444 && s_t7[6] == 0x55,
              "7-arg save thunk forwards all 7 arguments and the result");
        g_hookedOpen = (CreateFn)(void*)readprobe::NewSaveThunk7((const void*)&readfix::HkCreateFileW);
    }

    // ---- test tree: <tmp>\n3rf_<pid>\package\{a.bin 96 MB, b.bin 8 MB} + out.bin outside package ----
    wchar_t root[MAX_PATH], pkg[MAX_PATH];
    GetTempPathW(MAX_PATH, root);
    swprintf_s(pkg, L"%sn3rf_%lu", root, GetCurrentProcessId());
    CreateDirectoryW(pkg, nullptr);
    swprintf_s(g_pathOut, L"%s\\out.bin", pkg);
    wcscat_s(pkg, L"\\package");
    CreateDirectoryW(pkg, nullptr);
    swprintf_s(g_pathA, L"%s\\a.bin", pkg);
    swprintf_s(g_pathB, L"%s\\b.bin", pkg);
    MakeFile(g_pathA, 1, kSizeA);
    MakeFile(g_pathB, 2, kSizeB);
    MakeFile(g_pathOut, 3, kSizeB);
    wchar_t fin[1024] = {0};
    {
        HANDLE d = CreateFileW(pkg, 0, 7, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        GetFinalPathNameByHandleW(d, fin, 1024, 0);
        CloseHandle(d);
    }
    readfix::g_log = &Log;
    // 16 MB cache of 64 KiB blocks: the 96 MB random-offset workload evicts constantly
    Check(readfix::Init(2, 16, 64, 64, 2, 4, 2, false, fin), "Init: RC 16 MB / 64 KiB blocks / reads <= 64 KiB, AQ 4 threads, SR");

    // ---- 2. random-read equivalence, 8 threads, per configuration ----
    RunPhase("off", 0, 0, 0, false, 400);
    RunPhase("RC", 2, 0, 0, false, 4000);
    RunPhase("AQ", 0, 2, 0, false, 3000);
    RunPhase("RC+AQ", 2, 2, 0, false, 4000);
    RunPhase("SR", 0, 0, 2, true, 4000);
    RunPhase("RC+SR", 2, 0, 2, true, 4000);
    RunPhase("RC+AQ private", 2, 2, 0, true, 4000);

    // ---- 3. handle reuse: close a.bin, open b.bin (often the same handle value), same offsets -> b.bin's bytes ----
    {
        readfix::g_rcMode = 2;
        readfix::g_srMode = 0;
        readfix::g_rcActive.store(true);
        readfix::g_aqActive.store(false);
        int reused = 0, good = 0;
        for (int k = 0; k < 20; ++k) {
            HANDLE h1 = g_hookedOpen(g_pathA, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0x80, nullptr);
            uint8_t b[4096];
            OVERLAPPED o{};
            o.Offset = 65536 * (k + 1) + 100;
            DWORD got = 0;
            readfix::Read(0, h1, b, 4000, &got, &o);
            readfix::Read(0, h1, b, 4000, &got, &o);  // cached now
            readfix::HkCloseHandle(h1);
            HANDLE h2 = g_hookedOpen(g_pathB, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0x80, nullptr);
            reused += h2 == h1;
            o = OVERLAPPED{};
            o.Offset = 65536 * (k + 1) + 100;
            const BOOL r = (BOOL)readfix::Read(0, h2, b, 4000, &got, &o);
            bool ok = r && got == 4000;
            for (int i = 0; ok && i < 4000; ++i) ok = b[i] == Pat(2, o.Offset + i);
            good += ok;
            readfix::HkCloseHandle(h2);
        }
        printf("    handle value reused in %d/20 rounds\n", reused);
        Check(good == 20, "close + reopen: never the closed file's cached blocks");
    }

    // ---- 4. bad destination buffer on a cached block -> the real error, no crash ----
    {
        HANDLE h = g_hookedOpen(g_pathA, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0x80, nullptr);
        uint8_t b[512];
        OVERLAPPED o{};
        o.Offset = 4096;
        DWORD got = 0;
        readfix::Read(0, h, b, 512, &got, &o);
        void* noacc = VirtualAlloc(nullptr, 4096, MEM_RESERVE, PAGE_NOACCESS);
        o = OVERLAPPED{};
        o.Offset = 4096;
        const BOOL r = (BOOL)readfix::Read(0, h, noacc, 512, &got, &o);
        const DWORD le = GetLastError();
        Check(!r && le == ERROR_NOACCESS, "bad buffer: FALSE + ERROR_NOACCESS like ReadFile");
        readfix::HkCloseHandle(h);
    }

    // ---- 5. TSY weight maths + SH refusal ----
    {
        VirtualAlloc(img + 0x4B72000, 0x1000, MEM_COMMIT, PAGE_READWRITE);
        VirtualAlloc(img + 0x314000, 0x1000, MEM_COMMIT, PAGE_READWRITE);
        struct Case { int n, logical, cap, wantW; } cs[] = {{6, 16, 0, 7}, {6, 16, 10, 8}, {8, 16, 0, 9}, {4, 8, 0, 7}, {5, 12, 0, 6}};
        bool ok = true;
        for (const Case& c : cs) {
            *(uint32_t*)(img + readfix::kPoolCount) = 3;
            *(int32_t*)(img + readfix::kPoolWeight) = 4;
            readfix::BootCfg bc;
            bc.tsy = c.n;
            readfix::BootTry((uintptr_t)img, bc, c.cap, (unsigned long)c.logical, &Log);
            const int w = *(int32_t*)(img + readfix::kPoolWeight);
            // SizeWorkerPools: base = clamp(logical, 6, cap or 32); pool 9: min(base, 12); count = cvttss2si(b / 13.0f * w)
            int base = c.logical > (c.cap ? c.cap : 32) ? (c.cap ? c.cap : 32) : c.logical < 6 ? 6 : c.logical;
            const int bi = base < 12 ? base : 12;
            const int cnt = (int)((float)bi / 13.0f * (float)w);
            const int cntPrev = (int)((float)bi / 13.0f * (float)(w - 1));
            ok &= w == c.wantW && cnt >= c.n && cntPrev < c.n;
            printf("    TSY n=%d logical=%d cap=%d -> weight %d -> %d threads\n", c.n, c.logical, c.cap, w, cnt);
        }
        Check(ok, "TSY: smallest weight giving the wanted thread count (SizeWorkerPools float formula)");
        memcpy(img + readfix::kShSite, readfix::kShOrig, 5);
        readfix::BootCfg bc;
        bc.sh = true;
        readfix::BootTry((uintptr_t)img, bc, 0, 16, &Log);
        const bool wine = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") != nullptr;
        Check(wine ? img[readfix::kShSite + 2] == 0 : memcmp(img + readfix::kShSite, readfix::kShOrig, 5) == 0,
              wine ? "SH: written under Wine" : "SH: refused on Windows, bytes untouched");
    }

    DeleteFileW(g_pathA);
    DeleteFileW(g_pathB);
    DeleteFileW(g_pathOut);
    const int f = g_fail.load();
    puts(f ? "READFIX HARNESS: FAILED" : "READFIX HARNESS: ALL OK");
    fflush(stdout);
    ExitProcess(f ? 1 : 0);  // AQ workers never exit
}
