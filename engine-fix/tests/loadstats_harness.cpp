// loadstats_harness.cpp - offline check of LoadStats' live patching (src\loadstats.cpp is #included to reach its internals).
// Builds two copies of the manager's "mov ecx,N; call [rip+IAT Sleep]" shape in an RWX buffer - one with the FF 15
// inside a 16-byte window (interlocked path), one straddling a 16-byte boundary (EB FE fallback) - patches them with
// InstallSite() and checks: the original Sleep target still runs with the same argument, the thunk passed r14 (the
// "manager") and the site id, and the counters moved. Also re-runs both copies from a second thread while patching.
//   build+run: tests\run_loadstats_harness.cmd  ->  "LOADSTATS HARNESS: ALL OK"
#include "../src/loadstats.cpp"

#include <cstdio>

using namespace loadstats;

static volatile DWORD g_lastMs = 0;
static volatile LONG g_fakeCalls = 0;
static void WINAPI FakeSleep(DWORD ms) {
    g_lastMs = ms;
    InterlockedIncrement(&g_fakeCalls);
}

// push r14; sub rsp,20h; mov r14,rcx; mov ecx,7; call [rip+rel]; add rsp,20h; pop r14; ret
// The call [rip+rel] is placed at code+callOff; returns the offset of the FF 15.
static size_t EmitCaller(uint8_t* code, size_t pad, uint8_t* slot) {
    size_t i = 0;
    for (size_t k = 0; k < pad; ++k) code[i++] = 0x90;
    const uint8_t pre[] = {0x41, 0x56, 0x48, 0x83, 0xEC, 0x20, 0x49, 0x89, 0xCE, 0xB9, 0x07, 0, 0, 0};
    memcpy(code + i, pre, sizeof(pre));
    i += sizeof(pre);
    const size_t callAt = i;
    code[i++] = 0xFF;
    code[i++] = 0x15;
    const int32_t rel = (int32_t)(slot - (code + i + 4));
    memcpy(code + i, &rel, 4);
    i += 4;
    const uint8_t post[] = {0x48, 0x83, 0xC4, 0x20, 0x41, 0x5E, 0xC3};
    memcpy(code + i, post, sizeof(post));
    return callAt;
}

static int g_fail = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            printf("FAIL: " __VA_ARGS__);     \
            printf("\n");                     \
            ++g_fail;                         \
        }                                     \
    } while (0)

static void Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

using CallerFn = void (*)(uintptr_t mgr);
static volatile LONG g_stop = 0;
static CallerFn g_fnA = nullptr, g_fnB = nullptr;
static DWORD WINAPI Hammer(void*) {
    while (!g_stop) {
        g_fnA(0x1111);
        g_fnB(0x2222);
    }
    return 0;
}

int main() {
    uint8_t* buf = (uint8_t*)VirtualAlloc(nullptr, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!buf) return 2;
    uint8_t* slot = buf + 0x100;
    *(void**)slot = (void*)&FakeSleep;
    g_log = &Log;
    g_base = (uintptr_t)slot - kIatSleep;  // so that g_base + kIatSleep == slot (never dereferenced otherwise)
    g_thunks = buf + 0x8000;
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcMs = 1000.0 / (double)fq.QuadPart;

    uint8_t* codeA = buf + 0x1000;
    uint8_t* codeB = buf + 0x2000;
    // A: NOP pad so the 6-byte FF 15 sits in one aligned 8-byte window; B: so it straddles a 16-byte boundary.
    size_t padA = 0, padB = 0;
    for (size_t p = 0; p < 16; ++p) {
        const uintptr_t a = (uintptr_t)codeA + p + 14;
        if (a + 6 <= ((a & ~uintptr_t(7)) + 8)) { padA = p; break; }
    }
    for (size_t p = 0; p < 16; ++p) {
        const uintptr_t a = (uintptr_t)codeB + p + 14;
        if (a + 6 > ((a & ~uintptr_t(15)) + 16) && a + 2 <= ((a & ~uintptr_t(15)) + 16)) { padB = p; break; }
    }
    const size_t callA = EmitCaller(codeA, padA, slot), callB = EmitCaller(codeB, padB, slot);
    FlushInstructionCache(GetCurrentProcess(), buf, 0x10000);
    g_fnA = (CallerFn)codeA;
    g_fnB = (CallerFn)codeB;

    g_fnA(0x1234);
    CHECK(g_lastMs == 7 && g_fakeCalls == 1, "unpatched call");

    HANDLE th = CreateThread(nullptr, 0, &Hammer, nullptr, 0, nullptr);  // run both while they are patched
    Sleep(20);
    const Site sa{(uint32_t)((uintptr_t)(codeA + callA) - g_base), kMgrSleep, kIatSleep, 0};
    const Site sb{(uint32_t)((uintptr_t)(codeB + callB) - g_base), kMgrSleep, kIatSleep, 2};
    const uintptr_t aa = (uintptr_t)(codeA + callA), ab = (uintptr_t)(codeB + callB);
    printf("site A at +%zX (%s), site B at +%zX (%s)\n", (size_t)(aa & 15), aa + 6 <= (aa & ~uintptr_t(15)) + 16 ? "one window" : "straddles",
           (size_t)(ab & 15), ab + 6 <= (ab & ~uintptr_t(15)) + 16 ? "one window" : "straddles");
    CHECK(InstallSite(sa) == 0, "install A");
    CHECK(InstallSite(sb) == 0, "install B");
    Sleep(50);
    g_stop = 1;
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th);

    CHECK(codeA[callA] == 0xE8 && codeA[callA + 5] == 0x90, "A bytes");
    CHECK(codeB[callB] == 0xE8 && codeB[callB + 5] == 0x90, "B bytes");
    const uint64_t s0 = g_c.mgrSleep[0].load(), s2 = g_c.mgrSleep[2].load();
    const LONG calls0 = g_fakeCalls;
    g_lastMs = 0;
    g_fnA(0xABCD);
    CHECK(g_lastMs == 7, "A forwards ms");
    CHECK(g_mgr[0].load() == 0xABCD, "A passes r14 (got %llX)", (unsigned long long)g_mgr[0].load());
    CHECK(g_c.mgrSleep[0].load() == s0 + 1, "A counter");
    g_fnB(0xBEEF);
    CHECK(g_mgr[1].load() == 0xBEEF, "B passes r14 (got %llX)", (unsigned long long)g_mgr[1].load());
    CHECK(g_c.mgrSleep[2].load() == s2 + 1, "B counter");
    CHECK(g_fakeCalls == calls0 + 2, "original Sleep ran once per call");
    CHECK(InstallSite(sa) == 1, "re-install refuses changed bytes");
    printf("%s\n", g_fail ? "LOADSTATS HARNESS: FAILED" : "LOADSTATS HARNESS: ALL OK");
    return g_fail ? 1 : 0;
}
