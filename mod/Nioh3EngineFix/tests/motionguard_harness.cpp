// motionguard_harness.cpp - MotionGuard offline test (src/motionguard.cpp, GitHub #143 / #144). Not a game test.
// Runs the game's own root-motion track lookup (48A150) and motion key walk (A0BA8 + its cold chunk 1930AFC) from the
// decrypted dump inside a fake image (the dump pages copied to the same RVAs, incl. the float constants at 3BD4xxx).
//   motionguard_harness.exe <Nioh3.dump.exe>   -> prints ALL OK, exit 0
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../src/motionguard.h"

namespace {

int g_fail = 0;
#define CHECK(c, ...)                                  \
    do {                                               \
        if (!(c)) {                                    \
            ++g_fail;                                  \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                       \
            printf("\n");                              \
        }                                              \
    } while (0)

char g_logBuf[32768];
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

// game signatures (v2.02)
using FnA = uint8_t (*)(void* track, void* out, uint32_t id, float t);                          // 48A150
using FnB = void* (*)(void* list, float* out, uint64_t* outKey, float* outT, uint32_t* idx, float t);  // A0BA8
constexpr uint32_t kImage = 0x3BD6000;
constexpr uint32_t kPages[] = {0xA0000, 0x48A000, 0x1930000, 0x3BD4000};
uint8_t* g_img = nullptr;
FnA g_fa = nullptr;
FnB g_fb = nullptr;
uint8_t* g_reserved = nullptr;  // reserved, never committed: reads fault like #144's released motion memory

// #144 shape: the track pointer points into memory that is not there
void* FreedTrack() { return g_reserved; }

// motion key list (rcx of A0BA8): +10 count, +38 entry*[], +58 / +60 / +68 / +70 optional float arrays
struct KeyList {
    uint8_t raw[0x80];
};
struct Entry {
    uint8_t raw[0x40];
};
struct KeyData {
    float f[4];  // [+8] = key length
};
void SetPtr(void* obj, uint32_t off, const void* p) { memcpy((uint8_t*)obj + off, &p, 8); }
void SetU32(void* obj, uint32_t off, uint32_t v) { memcpy((uint8_t*)obj + off, &v, 4); }

KeyData g_data = {{0.f, 0.f, 1.0f, 0.f}};
Entry g_e[2];
void* g_keys[2];
KeyList MakeList(uint32_t count, bool nullData) {
    KeyList l{};
    memset(g_e, 0, sizeof g_e);
    for (int i = 0; i < 2; ++i) {
        SetPtr(&g_e[i], 0x28, nullData ? nullptr : &g_data);
        g_keys[i] = &g_e[i];
    }
    SetU32(&l, 0x10, count);
    SetPtr(&l, 0x38, g_keys);
    return l;
}

struct BResult {
    DWORD ex;
    void* ret;
    float out;
};
BResult CallB(KeyList* l, float t) {
    BResult r{0, (void*)1, -1.f};
    uint64_t key = 0;
    float kt = 0;
    uint32_t idx = 77;
    __try {
        r.ret = g_fb(l, &r.out, &key, &kt, &idx, t);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r.ex = GetExceptionCode();
    }
    return r;
}
struct AResult {
    DWORD ex;
    uint8_t ret;
};
AResult CallA(void* track) {
    AResult r{0, 0xEE};
    uint8_t out[0x20] = {};
    __try {
        r.ret = g_fa(track, out, 5, 0.5f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r.ex = GetExceptionCode();
    }
    return r;
}
DWORD ReadSeh(const volatile uint8_t* p) {
    __try {
        volatile uint8_t v = *p;
        (void)v;
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

DWORD RunChild(const char* dump, const char* mode) {
    char self[MAX_PATH];
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    std::string cmd = std::string("\"") + self + "\" \"" + dump + "\" " + mode;
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
    return exitCode;
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
    g_img = (uint8_t*)VirtualAlloc(nullptr, kImage, MEM_RESERVE, PAGE_NOACCESS);
    for (uint32_t pg : kPages) {
        uint8_t* p = (uint8_t*)VirtualAlloc(g_img + pg, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        _fseeki64(f, pg, SEEK_SET);
        const size_t got = fread(p, 1, 0x1000, f);
        CHECK(got == 0x1000, "dump page %X read %zu", pg, got);
    }
    fclose(f);
    FlushInstructionCache(GetCurrentProcess(), g_img, kImage);
    g_fa = (FnA)(g_img + motionguard::kFnA);
    g_fb = (FnB)(g_img + motionguard::kFnB);
    g_reserved = (uint8_t*)VirtualAlloc(nullptr, 0x10000, MEM_RESERVE, PAGE_NOACCESS);
    const uintptr_t base = (uintptr_t)g_img;

    // child modes: stock code (guard not installed) must die with an access violation (fake code has no unwind data)
    if (argc > 2 && !strcmp(argv[2], "--stockA")) {
        uint8_t out[0x20] = {};
        g_fa(FreedTrack(), out, 5, 0.5f);
        printf("stock child A returned\n");
        return 0;
    }
    if (argc > 2 && !strcmp(argv[2], "--stockB")) {
        KeyList l = MakeList(1, true);
        float o = 0, kt = 0;
        uint64_t key = 0;
        uint32_t idx = 0;
        g_fb(&l, &o, &key, &kt, &idx, 0.5f);
        printf("stock child B returned\n");
        return 0;
    }
    // 1. stock: #144 shape (track in freed memory) and #143 shape (key entry data null) fault like the reports
    {
        const DWORD a = RunChild(dump, "--stockA"), b = RunChild(dump, "--stockB");
        CHECK(a == EXCEPTION_ACCESS_VIOLATION, "stock #144 shape: child exit %lX, want C0000005", a);
        CHECK(b == EXCEPTION_ACCESS_VIOLATION, "stock #143 shape: child exit %lX, want C0000005", b);
        printf("case 1 stock faults: A %lX, B %lX\n", a, b);
    }
    // 2. intact data before any guard: the game's code runs (no fault)
    {
        uint8_t track[0x40] = {};  // count 0: no tracks
        const AResult a = CallA(track);
        CHECK(a.ex == 0 && a.ret == 0, "intact A (count 0): ex %lX ret %u", a.ex, a.ret);
        KeyList l = MakeList(1, false);
        const BResult b = CallB(&l, 0.5f);
        CHECK(b.ex == 0 && b.ret == &g_e[0], "intact B (1 key): ex %lX ret %p want %p", b.ex, b.ret, (void*)&g_e[0]);
        printf("case 2 intact before guard: A ret %u, B ret %p out %.3f\n", a.ret, b.ret, b.out);
    }
    // 3. ini MotionGuard = 0: not installed
    {
        const std::wstring ini = WriteIni(L"mg_off.ini", "[General]\r\nMotionGuard = 0\r\n");
        motionguard::StartInfo si;
        si.log = &Log;
        si.iniPath = ini.c_str();
        si.base = base;
        si.knownBuild = true;
        motionguard::Start(si);
        CHECK(!motionguard::Installed(), "MotionGuard=0 installed");
        CHECK(LogHas("MOTIONGUARD: off"), "no off line");
    }
    // 4. bytes differ in both functions: not installed; then restored + default ini: installed on both
    {
        const std::wstring ini = WriteIni(L"mg_def.ini", "[General]\r\n");
        uint8_t* pa = g_img + motionguard::kFnA + 0x30;
        uint8_t* pb = g_img + motionguard::kFnB + 0x20;
        const uint8_t ka = *pa, kb = *pb;
        *pa ^= 0xFF;
        *pb ^= 0xFF;
        motionguard::StartInfo si;
        si.log = &Log;
        si.iniPath = ini.c_str();
        si.base = base;
        si.knownBuild = true;
        motionguard::Start(si);
        CHECK(!motionguard::Installed(), "installed over changed bytes");
        CHECK(LogHas("MOTIONGUARD: not installed (motion code bytes differ"), "no bytes-differ line");
        *pa = ka;
        *pb = kb;
        // a changed byte inside a masked disp32 field (another build's constant address) must not block
        uint8_t* pm = g_img + motionguard::kFnB + 0x91;
        const uint8_t km = *pm;
        *pm ^= 0x01;
        motionguard::Start(si);
        *pm = km;
        CHECK(motionguard::Installed(), "default ini not installed");
        CHECK(LogHas("root-motion track +48A164 on, motion key list +A0C2A on"), "no installed line");
    }
    // 5. #144 shape with guard: returns 0 ('no root-motion track'), no exception
    {
        const AResult a = CallA(FreedTrack());
        CHECK(a.ex == 0 && a.ret == 0, "#144 shape guarded: ex %lX ret %u", a.ex, a.ret);
        CHECK(motionguard::HitsA() == 1, "hitsA %ld, want 1", motionguard::HitsA());
        Sleep(250);
        printf("case 5 guarded #144 shape: ret %u hitsA %ld\n", a.ret, motionguard::HitsA());
    }
    // 6. #143 shape with guard: single key with null data -> 0 (the empty-list result), no exception
    {
        KeyList l = MakeList(1, true);
        const BResult b = CallB(&l, 0.5f);
        CHECK(b.ex == 0 && b.ret == nullptr, "#143 shape guarded: ex %lX ret %p", b.ex, b.ret);
        CHECK(motionguard::HitsB() == 1, "hitsB %ld, want 1", motionguard::HitsB());
        Sleep(250);
        printf("case 6 guarded #143 shape: ret %p hitsB %ld\n", b.ret, motionguard::HitsB());
    }
    // 7. multi-key list with null data: faults in the cold chunk (1930B33), guard -> 0
    {
        KeyList l = MakeList(2, true);
        const BResult b = CallB(&l, 0.5f);
        CHECK(b.ex == 0 && b.ret == nullptr, "2-key null data guarded: ex %lX ret %p", b.ex, b.ret);
        CHECK(motionguard::HitsB() == 2, "hitsB %ld, want 2", motionguard::HitsB());
        printf("case 7 guarded 2-key chunk fault: ret %p hitsB %ld\n", b.ret, motionguard::HitsB());
    }
    Sleep(400);
    CHECK(LogHas("MOTIONGUARD: root-motion track at +48A164"), "no root-track hit line");
    CHECK(LogHas("MOTIONGUARD: motion key list at +A0C2A read 8"), "no key-list hit line");
    CHECK(LogHas("MOTIONGUARD: motion key list at +1930B33 read 8"), "no chunk hit line");
    // 8. intact data with the guard: same results as case 2 and as stock, no hits
    {
        const long h = motionguard::Hits();
        uint8_t track[0x40] = {};
        const AResult a = CallA(track);
        CHECK(a.ex == 0 && a.ret == 0, "intact A: ex %lX ret %u", a.ex, a.ret);
        KeyList l1 = MakeList(1, false);
        const BResult b1 = CallB(&l1, 0.5f);
        CHECK(b1.ex == 0 && b1.ret == &g_e[0], "intact B 1 key: ex %lX ret %p", b1.ex, b1.ret);
        KeyList l2 = MakeList(2, false);
        const BResult b2 = CallB(&l2, 0.5f);
        CHECK(b2.ex == 0 && b2.ret != nullptr, "intact B 2 keys: ex %lX ret %p", b2.ex, b2.ret);
        CHECK(motionguard::Hits() == h, "intact data counted a hit (%ld -> %ld)", h, motionguard::Hits());
        printf("case 8 intact with guard: A %u, B1 %p, B2 %p, hits %ld\n", a.ret, b1.ret, b2.ret, motionguard::Hits());
    }
    // 9. a fault anywhere else is not swallowed
    {
        const DWORD ex = ReadSeh(g_reserved + 8);
        CHECK(ex == EXCEPTION_ACCESS_VIOLATION, "other fault: %lX, want AV", ex);
        CHECK(motionguard::Hits() == 3, "other fault counted (%ld)", motionguard::Hits());
    }
    // 10. repeated hits stay stable (25 each; log is rate-limited, no crash)
    for (int i = 0; i < 25; ++i) {
        const AResult a = CallA(FreedTrack());
        KeyList l = MakeList(1 + (i & 1), true);
        const BResult b = CallB(&l, 0.25f * (float)i);
        CHECK(a.ex == 0 && a.ret == 0 && b.ex == 0 && b.ret == nullptr, "repeat %d: A %lX/%u B %lX/%p", i, a.ex, a.ret, b.ex, b.ret);
    }
    CHECK(motionguard::Hits() == 53, "hits %ld, want 53", motionguard::Hits());
    CHECK(motionguard::HitsA() == 26 && motionguard::HitsB() == 27, "hitsA %ld hitsB %ld", motionguard::HitsA(), motionguard::HitsB());

    if (g_fail) {
        printf("FAILED %d\n", g_fail);
        return 1;
    }
    printf("ALL OK\n");
    return 0;
}
