// aichainguard_harness.cpp - AiChainGuard offline test (src/aichainguard.cpp, GitHub #95 / #30). Not a game test.
// Runs the game's own vector<ptr>::assign bytes (dump 310698..31075A, the two calls redirected to local memmove / reserve)
// inside a fake image, with the source list ending at a PAGE_NOACCESS page like the #95 crash (190 rows, 114 readable).
//   aichainguard_harness.exe <Nioh3.dump.exe>   -> prints ALL OK, exit 0
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../src/aichainguard.h"

namespace {

int g_fail = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            ++g_fail;                         \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);              \
            printf("\n");                     \
        }                                     \
    } while (0)

char g_logBuf[16384];
size_t g_logLen = 0;
CRITICAL_SECTION g_cs;
void Log(const char* fmt, ...) {
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    EnterCriticalSection(&g_cs);
    printf("  log: %s\n", line);
    const size_t n = strlen(line);
    if (g_logLen + n + 2 < sizeof g_logBuf) {
        memcpy(g_logBuf + g_logLen, line, n);
        g_logLen += n;
        g_logBuf[g_logLen++] = '\n';
        g_logBuf[g_logLen] = 0;
    }
    LeaveCriticalSection(&g_cs);
}
bool LogHas(const char* s) {
    EnterCriticalSection(&g_cs);
    const bool r = strstr(g_logBuf, s) != nullptr;
    LeaveCriticalSection(&g_cs);
    return r;
}

struct Vec {
    uint64_t* begin;
    uint64_t* end;
    uint64_t* cap;
};

extern "C" void* StubMemmove(void* d, const void* s, size_t n) { return memmove(d, s, n); }
extern "C" void StubReserve(Vec* v, size_t n) {  // game 82946C: grow capacity to n (keeps contents)
    const size_t size = (size_t)(v->end - v->begin);
    auto* p = (uint64_t*)malloc(n * 8);
    if (size) memcpy(p, v->begin, size * 8);
    v->begin = p;
    v->end = p + size;
    v->cap = p + n;
}

using AssignFn = void (*)(Vec*, const uint64_t* first, const uint64_t* last);
constexpr uint32_t kFn = aichainguard::kFnRva, kEnd = 0x31075A, kCallMemmove = 0x3106DA, kCallMemmove2 = 0x310733, kCallReserve = 0x310718;
uint8_t* g_img = nullptr;  // fake image: g_img + RVA

void Thunk(uint8_t* at, void* target) {  // mov rax, imm64 ; jmp rax
    at[0] = 0x48;
    at[1] = 0xB8;
    memcpy(at + 2, &target, 8);
    at[10] = 0xFF;
    at[11] = 0xE0;
}
void Redirect(uint32_t callRva, uint32_t thunkRva) {
    uint8_t* c = g_img + callRva;
    if (c[0] != 0xE8) {
        printf("FAIL call byte at %X\n", callRva);
        ++g_fail;
    }
    const int32_t rel = (int32_t)((int64_t)thunkRva - (int64_t)(callRva + 5));
    memcpy(c + 1, &rel, 4);
}

// a source list of `rows` entries whose first `readable` sit right before a PAGE_NOACCESS page
uint64_t* g_srcPages = nullptr;
uint64_t* MakeSource(size_t rows, size_t readable) {
    if (!g_srcPages) {
        g_srcPages = (uint64_t*)VirtualAlloc(nullptr, 3 * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        DWORD o;
        VirtualProtect((uint8_t*)g_srcPages + 2 * 4096, 4096, PAGE_NOACCESS, &o);
    }
    uint64_t* first = (uint64_t*)((uint8_t*)g_srcPages + 2 * 4096) - readable;
    for (size_t i = 0; i < readable; ++i) first[i] = 0x1111000000000000ull + i;
    (void)rows;
    return first;
}

// SEH wrappers (no C++ objects inside)
DWORD CallSeh(AssignFn fn, Vec* v, const uint64_t* a, const uint64_t* b) {
    __try {
        fn(v, a, b);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
    return 0;
}
DWORD ReadSeh(const volatile uint64_t* p, uint64_t* out) {
    __try {
        *out = *p;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
    return 0;
}

std::wstring WriteIni(const wchar_t* name, const char* body) {
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    std::wstring p = std::wstring(dir) + name;
    FILE* f = _wfopen(p.c_str(), L"wb");
    fputs(body, f);
    fclose(f);
    return p;
}

}  // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    InitializeCriticalSection(&g_cs);
    const char* dump = argc > 1 ? argv[1] : "C:\\nioh3-engine\\re\\Nioh3.dump.exe";
    FILE* f = fopen(dump, "rb");
    if (!f) {
        printf("FAIL cannot open dump %s\n", dump);
        return 2;
    }
    uint8_t code[kEnd - kFn];
    _fseeki64(f, kFn, SEEK_SET);
    const size_t got = fread(code, 1, sizeof code, f);
    fclose(f);
    CHECK(got == sizeof code, "dump read %zu", got);

    g_img = (uint8_t*)VirtualAlloc(nullptr, kEnd + 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    memset(g_img + kFn, 0xCC, kEnd - kFn + 0x10);
    memcpy(g_img + kFn, code, sizeof code);
    Thunk(g_img + 0x1000, (void*)&StubMemmove);
    Thunk(g_img + 0x1010, (void*)&StubReserve);
    Redirect(kCallMemmove, 0x1000);
    Redirect(kCallReserve, 0x1010);
    Redirect(kCallMemmove2, 0x1000);
    FlushInstructionCache(GetCurrentProcess(), g_img, kEnd + 0x1000);
    const auto fn = (AssignFn)(g_img + kFn);
    const uintptr_t base = (uintptr_t)g_img;

    // 1. stock (guard not installed): the #95 shape faults at +31074A. Runs in a child process (the fake code has no unwind
    //    data, so the fault is left unhandled there): the child must die with 0xC0000005.
    if (argc > 2 && !strcmp(argv[2], "--stock")) {
        uint64_t* first = MakeSource(190, 114);
        Vec v{};
        fn(&v, first, first + 190);
        printf("stock child returned (no fault)\n");
        return 0;
    }
    {
        char self[MAX_PATH];
        GetModuleFileNameA(nullptr, self, MAX_PATH);
        std::string cmd = std::string("\"") + self + "\" \"" + dump + "\" --stock";
        STARTUPINFOA si{};
        si.cb = sizeof si;
        PROCESS_INFORMATION pi{};
        SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
        DWORD exitCode = 0;
        if (CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 60000);
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        CHECK(exitCode == EXCEPTION_ACCESS_VIOLATION, "stock #95 shape: child exit %lX, want C0000005", exitCode);
        printf("case 1 stock faults: child exit %lX\n", exitCode);
    }
    // 2. ini AiChainGuard = 0: not installed
    {
        const std::wstring ini = WriteIni(L"acg_off.ini", "[General]\r\nAiChainGuard = 0\r\n");
        aichainguard::StartInfo si;
        si.log = &Log;
        si.iniPath = ini.c_str();
        si.base = base;
        si.knownBuild = true;
        aichainguard::Start(si);
        CHECK(!aichainguard::Installed(), "AiChainGuard=0 installed");
        CHECK(LogHas("AICHAINGUARD: off"), "no off line");
    }
    // 3. bytes differ: not installed (one loop byte changed, then restored)
    {
        const std::wstring ini = WriteIni(L"acg_def.ini", "[General]\r\n");
        const uint8_t keep = g_img[kFn + aichainguard::kLoopOff + 3];
        g_img[kFn + aichainguard::kLoopOff + 3] ^= 0xFF;
        aichainguard::StartInfo si;
        si.log = &Log;
        si.iniPath = ini.c_str();
        si.base = base;
        si.knownBuild = true;
        aichainguard::Start(si);
        CHECK(!aichainguard::Installed(), "installed over changed bytes");
        CHECK(LogHas("AICHAINGUARD: not installed (boss AI chain copy bytes differ"), "no bytes-differ line");
        g_img[kFn + aichainguard::kLoopOff + 3] = keep;
        // 4. default ini: installed
        aichainguard::Start(si);
        CHECK(aichainguard::Installed(), "default ini not installed");
        CHECK(LogHas("AICHAINGUARD: installed at +31074A (v2.02)"), "no installed line");
    }
    // 5. the #95 shape: copy ends at the last readable row, no exception, rows intact
    {
        uint64_t* first = MakeSource(190, 114);
        Vec v{};
        const DWORD ex = CallSeh(fn, &v, first, first + 190);
        CHECK(ex == 0, "#95 shape with guard: exception %lX", ex);
        const size_t n = (size_t)(v.end - v.begin);
        CHECK(n == 114, "#95 shape: %zu rows copied, want 114", n);
        CHECK(v.cap - v.begin == 190, "#95 shape: capacity %zd, want 190", (ptrdiff_t)(v.cap - v.begin));
        bool same = true;
        for (size_t i = 0; i < n && i < 114; ++i) same = same && v.begin[i] == first[i];
        CHECK(same, "#95 shape: rows differ");
        CHECK(aichainguard::Hits() == 1, "hits %ld, want 1", aichainguard::Hits());
        printf("case 5 guarded #95 shape: %zu rows, hits %ld\n", n, aichainguard::Hits());
        Sleep(400);
        CHECK(LogHas("copy ended after 114 rows, 76 rows skipped (hit 1"), "no hit line");
    }
    // 6. intact list: the game's code copies every row, no hit
    {
        static uint64_t src[190];
        for (int i = 0; i < 190; ++i) src[i] = 0x2222000000000000ull + i;
        Vec v{};
        const DWORD ex = CallSeh(fn, &v, src, src + 190);
        CHECK(ex == 0 && v.end - v.begin == 190 && v.begin[189] == src[189], "intact list: ex %lX rows %zd", ex,
              (ptrdiff_t)(v.end - v.begin));
        // second assign into the same vector (shorter: the memmove-only path)
        const DWORD ex2 = CallSeh(fn, &v, src + 10, src + 20);
        CHECK(ex2 == 0 && v.end - v.begin == 10 && v.begin[0] == src[10], "reassign: ex %lX rows %zd", ex2, (ptrdiff_t)(v.end - v.begin));
        CHECK(aichainguard::Hits() == 1, "intact list counted a hit (%ld)", aichainguard::Hits());
    }
    // 7. a fault anywhere else is not swallowed
    {
        uint64_t out = 0;
        const DWORD ex = ReadSeh((const volatile uint64_t*)((uint8_t*)g_srcPages + 2 * 4096), &out);
        CHECK(ex == EXCEPTION_ACCESS_VIOLATION, "other fault: %lX, want AV", ex);
        CHECK(aichainguard::Hits() == 1, "other fault counted (%ld)", aichainguard::Hits());
    }
    // 8. rows 0 readable (the very first read faults): empty result, no crash
    {
        uint64_t* first = MakeSource(5, 0);
        Vec v{};
        const DWORD ex = CallSeh(fn, &v, first, first + 5);
        CHECK(ex == 0 && v.end == v.begin, "first-row fault: ex %lX rows %zd", ex, (ptrdiff_t)(v.end - v.begin));
        CHECK(aichainguard::Hits() == 2, "hits %ld, want 2", aichainguard::Hits());
    }
    // 9. repeated hits stay stable (25 in a row; log is rate-limited, no crash)
    for (int i = 0; i < 25; ++i) {
        uint64_t* first = MakeSource(190, 114);
        Vec v{};
        const DWORD ex = CallSeh(fn, &v, first, first + 190);
        CHECK(ex == 0 && v.end - v.begin == 114, "repeat %d: ex %lX", i, ex);
    }
    CHECK(aichainguard::Hits() == 27, "hits %ld, want 27", aichainguard::Hits());

    if (g_fail) {
        printf("FAILED %d\n", g_fail);
        return 1;
    }
    printf("ALL OK\n");
    return 0;
}
