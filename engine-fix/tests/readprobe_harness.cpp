// readprobe_harness.cpp - offline check of readprobe.cpp (RP probe hooks + EP event pool); src\readprobe.cpp is #included.
// A fake 'image' (reserved like Nioh3.exe, a few committed pages) holds: the IAT slots (real kernel32 functions), a caller
// stub with `call [rip+IAT ReadFile]` and a caller stub with `call FsReadCacheLookup` at the real RVA 198357C (fake callee
// at 2F77A6C returning its 5th argument). Checks:
//   - the patched ReadFile site returns the real result/bytes/last error, and the caller sees rcx/rdx/r8/r9/r10/r11 unchanged
//     (bosslog-style save thunk; 1.7.17 crashed on an r11 clobber); the 5th stack argument (OVERLAPPED) is forwarded
//   - counters, LoadStats feed, locality classes
//   - the R3 asm thunk: same rax, r10/r11/rcx untouched, calls/hits counted (hit = eax > 0)
//   - EP: a signalled event returns to the pool and comes back non-signalled + manual-reset and works for the next
//     overlapped read; a non-signalled event and a non-event handle are really closed
//   build+run: py -I tests\run_readprobe_harness.py  ->  "READPROBE HARNESS: ALL OK"
#include "../src/readprobe.cpp"

#include <cstdio>
#include <initializer_list>

namespace prefetch {  // 1.7.26 PF observes reads through readprobe.cpp; tested by run_prefetch_harness.py
bool Wants(const wchar_t*) { return false; }
bool Start(const StartInfo&) { return false; }
void Note(int, void*, const void*, unsigned long) {}
}  // namespace prefetch

namespace loadstats {  // stubs for the two calls readprobe.cpp makes into loadstats.cpp
static int g_noteCalls = 0, g_noteCls = -1;
static uint32_t g_noteBytes = 0;
void NoteRead(int cls, uint32_t bytes, bool, uint64_t) {
    ++g_noteCalls;
    g_noteCls = cls;
    g_noteBytes = bytes;
}
uint64_t LastFailEvalQpc() { return 0; }
}  // namespace loadstats

namespace readfix {  // 1.7.25: readprobe.cpp routes to readfix.cpp when RC/AQ/SR are on; tested by run_readfix_harness.py
int SrMode(const wchar_t*, int* i, bool* w) { if (i) *i = 0; if (w) *w = false; return 0; }
bool Wants(const wchar_t*) { return false; }
bool Start(const StartInfo&) { return false; }
int __stdcall Read(int, void* h, void* b, unsigned long n, unsigned long* g, void* o) { return ReadFile(h, b, n, g, (LPOVERLAPPED)o); }
}  // namespace readfix

using namespace readprobe;

static int g_fail = 0;
static void Check(bool c, const char* what) {
    printf("%s %s\n", c ? "OK  " : "FAIL", what);
    if (!c) ++g_fail;
}

static uint8_t* Commit(uint8_t* base, uint32_t rva, uint32_t len, DWORD prot) {
    return (uint8_t*)VirtualAlloc(base + (rva & ~0xFFFu), ((rva & 0xFFF) + len + 0xFFF) & ~0xFFFu, MEM_COMMIT, prot);
}

int main() {
    const uint32_t kSize = 0x5300000;
    uint8_t* img = (uint8_t*)VirtualAlloc(nullptr, kSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!img) { puts("FAIL reserve"); return 1; }
    g_base = (uintptr_t)img;
    Commit(img, 0x38E2000, 0x1000, PAGE_READWRITE);
    auto slot = [&](uint32_t rva, void* fn) { *(void**)(img + rva) = fn; };
    slot(kIatReadFile, (void*)&ReadFile);
    slot(kIatReadFileEx, (void*)&ReadFileEx);
    slot(kIatCreateEventA, (void*)&CreateEventA);
    slot(kIatCloseHandle, (void*)&CloseHandle);
    slot(kIatWfmo, (void*)&WaitForMultipleObjects);
    // MZ/PE header for AllocNear users (not needed: we call AllocNear ourselves)
    g_thunks = AllocNear(g_base, g_base + kSize);
    Check(g_thunks != nullptr, "thunk page within 2 GB of the fake image");
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcFreq = (uint64_t)fq.QuadPart;
    g_usPerTick = 1e6 / (double)fq.QuadPart;
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    g_ntQif = reinterpret_cast<NtQifFn>(GetProcAddress(nt, "NtQueryInformationFile"));
    g_ntReset = reinterpret_cast<NtResetFn>(GetProcAddress(nt, "NtResetEvent"));
    g_tls = TlsAlloc();
    g_rp = true;
    g_lsFeed = true;

    // ---- test file: 1 MiB of a known pattern ----
    wchar_t tmp[MAX_PATH], path[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    GetTempFileNameW(tmp, L"rph", 0, path);
    {
        HANDLE w = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        static uint8_t data[1 << 20];
        for (int i = 0; i < (1 << 20); ++i) data[i] = (uint8_t)(i * 7 + (i >> 9));
        DWORD wr = 0;
        WriteFile(w, data, sizeof(data), &wr, nullptr);
        CloseHandle(w);
    }
    auto expect = [](uint64_t off, uint32_t i) { return (uint8_t)((off + i) * 7 + ((off + i) >> 9)); };
    HANDLE hs = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    HANDLE ho = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    Check(hs != INVALID_HANDLE_VALUE && ho != INVALID_HANDLE_VALUE, "test file opened (sync + overlapped)");

    // ---- 1. ReadFile call site -> save thunk ----
    const uint32_t kStub = 0x100000;
    uint8_t* c = Commit(img, kStub, 0x100, PAGE_EXECUTE_READWRITE) ? img + kStub : nullptr;
    size_t i = 0;
    auto put = [&](std::initializer_list<uint8_t> b) { for (uint8_t x : b) c[i++] = x; };
    auto put64 = [&](uint64_t v) { memcpy(c + i, &v, 8); i += 8; };
    put({0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B, 0x44, 0x24, 0x60, 0x48, 0x89, 0x44, 0x24, 0x20, 0x48, 0x8B, 0x5C, 0x24, 0x68});
    put({0x49, 0xBA}); put64(0x1010101010101010ull);  // r10
    put({0x49, 0xBB}); put64(0x1111111111111111ull);  // r11
    const uint32_t siteRva = kStub + (uint32_t)i;
    const int32_t disp = (int32_t)((intptr_t)(img + kIatReadFile) - (intptr_t)(c + i + 6));
    put({0xFF, 0x15}); memcpy(c + i, &disp, 4); i += 4;
    put({0x4C, 0x89, 0x13, 0x4C, 0x89, 0x5B, 0x08, 0x48, 0x89, 0x4B, 0x10, 0x48, 0x89, 0x53, 0x18, 0x4C, 0x89, 0x43, 0x20,
         0x4C, 0x89, 0x4B, 0x28, 0x48, 0x83, 0xC4, 0x30, 0x5B, 0xC3});
    using StubFn = BOOL (*)(HANDLE, void*, DWORD, DWORD*, OVERLAPPED*, uint64_t*);
    StubFn stub = (StubFn)(void*)c;
    uint8_t buf[256];
    DWORD got = 0;
    OVERLAPPED o{};
    o.Offset = 4096;
    uint64_t regs[6] = {};
    Check(stub(hs, buf, 100, &got, &o, regs) && got == 100 && buf[0] == expect(4096, 0), "unpatched stub reads (sanity)");
    Check(HookIatCall(siteRva, kIatReadFile, kReadHooks[0]) == 0, "HookIatCall patched the FF 15 site");
    Check(c[siteRva - kStub] == 0xE8 && c[siteRva - kStub + 5] == 0x90, "site is now E8 rel32 + NOP");
    memset(buf, 0, sizeof(buf));
    got = 0;
    o = OVERLAPPED{};
    o.Offset = 8192;
    BOOL r = stub(hs, buf, 200, &got, &o, regs);
    bool same = r && got == 200;
    for (uint32_t k = 0; k < 200; ++k) same &= buf[k] == expect(8192, k);
    Check(same, "patched site: same result, *got and bytes (OVERLAPPED offset forwarded as the 5th stack argument)");
    Check(regs[0] == 0x1010101010101010ull && regs[1] == 0x1111111111111111ull, "caller's r10/r11 survive the hook");
    Check(regs[2] == (uint64_t)hs && regs[3] == (uint64_t)buf && (uint32_t)regs[4] == 200 && regs[5] == (uint64_t)&got,
          "caller's rcx/rdx/r8/r9 survive the hook");
    Check(g_sv[0][fCalls].load() == 1 && g_sv[0][fBytes].load() == 200 && g_sv[0][fOvl].load() == 1, "site counters (calls, bytes, ovl-api)");
    Check(loadstats::g_noteCalls == 1 && loadstats::g_noteCls == 0 && loadstats::g_noteBytes == 200, "LoadStats NoteRead fed (site 0 = archive)");
    o = OVERLAPPED{};
    r = stub((HANDLE)(uintptr_t)0x1234560, buf, 10, &got, &o, regs);
    const DWORD le = GetLastError();
    Check(!r && le == ERROR_INVALID_HANDLE && g_sv[0][fErr].load() == 1, "error path: FALSE + the real last error, counted");
    // overlapped handle through the hook
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    o = OVERLAPPED{};
    o.Offset = 65536;
    o.hEvent = ev;
    r = stub(ho, buf, 64, &got, &o, regs);
    DWORD got2 = 0;
    const BOOL done = r || (GetLastError() == ERROR_IO_PENDING && GetOverlappedResult(ho, &o, &got2, TRUE));
    Check(done && buf[0] == expect(65536, 0) && buf[63] == expect(65536, 63), "overlapped handle through the hook completes with the right bytes");
    {
        const int s = HandleSlot(hs), so = HandleSlot(ho);
        Check(s >= 0 && so >= 0 && g_h[s].sync == 1 && g_h[so].sync == 0, "handle table: sync vs overlapped handle from FileModeInformation");
        Check(g_h[s].size == (1u << 20) && strstr(g_h[s].name, "rph") != nullptr, "handle table: size and path tail");
    }

    // ---- 2. locality classes (fresh handle entry) ----
    {
        HANDLE h2 = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        HEnt& e = g_h[HandleSlot(h2)];
        const int a = Classify(e, 0, 4096), b = Classify(e, 4096, 4096), d = Classify(e, 4096, 4096), f = Classify(e, 20000, 100),
                  g = Classify(e, 0x50000, 10), h = Classify(e, 0x10, 10);
        Check(a == kFar && b == kSeq && d == kReread && f == kNear && g == kFar && h == kInblk, "locality: far, seq, reread, near, far, inblk");
        CloseHandle(h2);
    }

    // ---- 3. R3 cache-call thunk at the real RVAs ----
    {
        uint8_t* fn = Commit(img, kCacheFn, 16, PAGE_EXECUTE_READWRITE) ? img + kCacheFn : nullptr;
        const uint8_t ret5[] = {0x48, 0x8B, 0x44, 0x24, 0x28, 0xC3};  // mov rax,[rsp+28h]; ret
        memcpy(fn, ret5, sizeof(ret5));
        uint8_t* s = Commit(img, kCacheCall - 40, 80, PAGE_EXECUTE_READWRITE) ? img + kCacheCall - 40 : nullptr;
        c = s;
        i = 0;
        put({0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B, 0x44, 0x24, 0x60, 0x48, 0x89, 0x44, 0x24, 0x20, 0x48, 0x8B, 0x5C, 0x24, 0x68});
        put({0x49, 0xBA}); put64(0x2020202020202020ull);
        put({0x49, 0xBB}); put64(0x2121212121212121ull);
        Check(i == 40, "cache stub E8 lands on 198357C");
        const int32_t rel = (int32_t)((intptr_t)fn - (intptr_t)(c + i + 5));
        put({0xE8}); memcpy(c + i, &rel, 4); i += 4;
        put({0x48, 0x89, 0x03, 0x4C, 0x89, 0x53, 0x08, 0x4C, 0x89, 0x5B, 0x10, 0x48, 0x89, 0x4B, 0x18, 0x48, 0x83, 0xC4, 0x30, 0x5B, 0xC3});
        using CFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t*);
        CFn cs = (CFn)(void*)s;
        Check(HookCacheCall() == 0, "HookCacheCall patched the E8 at 198357C");
        uint64_t rg[4] = {};
        const uint64_t r1 = cs(0xAA, 2, 3, 4, 77, rg);
        const uint64_t r2 = cs(0xAA, 2, 3, 4, 0, rg);
        const uint64_t r3 = cs(0xAA, 2, 3, 4, 0xFFFFFFFFull, rg);  // eax = -1 -> not a hit
        Check(r1 == 77 && r2 == 0 && r3 == 0xFFFFFFFFull, "cache thunk: same rax as the callee");
        Check(rg[1] == 0x2020202020202020ull && rg[2] == 0x2121212121212121ull && rg[3] == 0xAA, "cache thunk: r10/r11/rcx untouched");
        Check(g_cacheCalls && *g_cacheCalls == 3 && *g_cacheHits == 1, "cache thunk: 3 calls, 1 hit (eax > 0)");
    }

    // ---- 4. EP event pool ----
    g_ep = true;
    {
        HANDLE e1 = HkCreateEventA(nullptr, TRUE, FALSE, nullptr);
        Check(e1 && WaitForSingleObject(e1, 0) == WAIT_TIMEOUT && g_gv[gEpHit].load() == 0, "EP: empty pool -> real CreateEventA");
        o = OVERLAPPED{};
        o.Offset = 1000;
        o.hEvent = e1;
        if (!ReadFile(ho, buf, 32, nullptr, &o) && GetLastError() == ERROR_IO_PENDING) WaitForSingleObject(e1, INFINITE);
        Check(WaitForSingleObject(e1, 0) == WAIT_OBJECT_0, "EP: event signalled by the completed read");
        Check(HkCloseEvent(e1) && g_gv[gEpPut].load() == 1, "EP: signalled event goes back to the pool");
        HANDLE e2 = HkCreateEventA(nullptr, TRUE, FALSE, nullptr);
        Check(e2 == e1 && g_gv[gEpHit].load() == 1 && GetLastError() == ERROR_SUCCESS, "EP: pool hit returns it (last error 0)");
        Check(WaitForSingleObject(e2, 0) == WAIT_TIMEOUT, "EP: pooled event comes back non-signalled");
        SetEvent(e2);
        Check(WaitForSingleObject(e2, 0) == WAIT_OBJECT_0 && WaitForSingleObject(e2, 0) == WAIT_OBJECT_0, "EP: still manual-reset");
        ResetEvent(e2);
        o = OVERLAPPED{};
        o.Offset = 300000;
        o.hEvent = e2;
        DWORD g3 = 0;
        const BOOL ok3 = ReadFile(ho, buf, 48, nullptr, &o) || (GetLastError() == ERROR_IO_PENDING && GetOverlappedResult(ho, &o, &g3, TRUE));
        Check(ok3 && buf[0] == expect(300000, 0) && buf[47] == expect(300000, 47), "EP: reused event drives the next overlapped read");
        HANDLE e3 = CreateEventA(nullptr, TRUE, FALSE, nullptr);  // never signalled: must really close
        const uint64_t closed0 = g_gv[gEpClose].load();
        Check(HkCloseEvent(e3) && g_gv[gEpClose].load() == closed0 + 1, "EP: non-signalled event is really closed");
        Check(WaitForSingleObject(e3, 0) == WAIT_FAILED, "EP: ... and its handle is gone");
        HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        Check(HkCloseEvent(hf) && g_gv[gEpClose].load() == closed0 + 2, "EP: a non-event handle is really closed");
        HkCloseEvent(e2);
    }

    CloseHandle(hs);
    CloseHandle(ho);
    CloseHandle(ev);
    DeleteFileW(path);
    puts(g_fail ? "READPROBE HARNESS: FAILED" : "READPROBE HARNESS: ALL OK");
    return g_fail ? 1 : 0;
}
