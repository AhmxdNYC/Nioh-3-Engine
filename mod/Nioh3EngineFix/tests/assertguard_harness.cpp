// assertguard_harness.cpp - AssertGuard offline test (src/assertguard.cpp, GitHub #153). Not a game test.
// Copies the v2.02 assert logger page (18CBDAC) and its message buffer page (47090B0) from the decrypted dump into a fake
// image, installs the guard, then executes the real int3 at 18CBDF7 (the byte after it is replaced by `ret` once the guard
// is installed, so execution returns to the harness instead of the game's unlock call). assertguard_harness.exe
// <Nioh3.dump.exe> prints ALL OK, exit 0.
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/assertguard.h"

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

CRITICAL_SECTION g_cs;
char g_logBuf[16384];
size_t g_logLen = 0;
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
void LogClear() {
    EnterCriticalSection(&g_cs);
    g_logLen = 0;
    g_logBuf[0] = 0;
    LeaveCriticalSection(&g_cs);
}
bool WaitLog(const char* s) {
    for (int i = 0; i < 40; ++i) {
        if (LogHas(s)) return true;
        Sleep(50);
    }
    return false;
}

constexpr uint32_t kSpan = 0x4710000;
constexpr uint32_t kPages[] = {0x18CB000, 0x4709000};
constexpr uint32_t kBufRva = 0x47090B0;
uint8_t* g_img = nullptr;

bool LoadPage(FILE* f, uint32_t page) {
    uint8_t* dst = static_cast<uint8_t*>(VirtualAlloc(g_img + page, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!dst) return false;
    if (fseek(f, static_cast<long>(page), SEEK_SET) != 0) return false;
    return fread(dst, 1, 0x1000, f) == 0x1000;
}

std::wstring WriteIni(const wchar_t* name, const char* body) {
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    std::wstring p = std::wstring(dir) + name;
    FILE* fp = _wfopen(p.c_str(), L"wb");
    if (!fp) return L"";
    fputs(body, fp);
    fclose(fp);
    return p;
}

// Runs code at p; returns true if it came back normally, false if an exception reached this frame.
bool RunAt(const void* p, DWORD* code) {
    *code = 0;
    __try {
        reinterpret_cast<void (*)()>(const_cast<void*>(p))();
        return true;
    } __except (*code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

}  // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    InitializeCriticalSection(&g_cs);
    const char* dump = argc > 1 ? argv[1] : "C:\\Users\\Ahmad Hamza\\nioh3-engine\\re\\Nioh3.dump.exe";
    FILE* f = fopen(dump, "rb");
    if (!f) {
        printf("FAIL cannot open dump %s\n", dump);
        return 2;
    }
    g_img = static_cast<uint8_t*>(VirtualAlloc(nullptr, kSpan, MEM_RESERVE, PAGE_NOACCESS));
    CHECK(g_img != nullptr, "reserve");
    for (uint32_t pg : kPages) CHECK(LoadPage(f, pg), "page %X", pg);
    fclose(f);
    if (g_fail) return 1;

    uint8_t* fn = g_img + assertguard::kLogger;
    uint8_t* site = fn + assertguard::kInt3;
    CHECK(site[0] == 0xCC, "stock int3 at 18CBDF7 (%02X)", site[0]);
    CHECK(fn[assertguard::kBufLea] == 0x48 && fn[assertguard::kBufLea + 1] == 0x8D && fn[assertguard::kBufLea + 2] == 0x0D,
          "lea rcx, [rip+disp] at +3F");
    {
        int32_t disp = 0;
        memcpy(&disp, fn + assertguard::kBufLea + 3, 4);
        CHECK(static_cast<uint32_t>(fn + assertguard::kBufLea + 7 + disp - g_img) == kBufRva, "buffer at 47090B0");
    }

    // 1. ini 0: off, nothing installed
    const std::wstring off = WriteIni(L"ag_off.ini", "[General]\r\nAssertGuard = 0\r\n");
    assertguard::StartInfo si{};
    si.log = &Log;
    si.iniPath = off.c_str();
    si.base = reinterpret_cast<uintptr_t>(g_img);
    si.knownBuild = true;
    assertguard::Start(si);
    CHECK(!assertguard::Installed(), "ini 0 installed");
    CHECK(LogHas("ASSERTGUARD: off"), "missing off log");

    // 2. one logger byte changed: refused
    LogClear();
    const std::wstring on = WriteIni(L"ag_on.ini", "[General]\r\nAssertGuard = 1\r\n");
    si.iniPath = on.c_str();
    fn[0x23] ^= 0xFF;  // E8 of the lock call
    assertguard::Start(si);
    CHECK(!assertguard::Installed(), "installed over changed bytes");
    CHECK(LogHas("bytes differ"), "missing refuse log");
    fn[0x23] ^= 0xFF;
    site[0] = 0x90;  // int3 gone (another tool / build): refused too
    assertguard::Start(si);
    CHECK(!assertguard::Installed(), "installed without the int3");
    site[0] = 0xCC;

    // 3. default: installed, code bytes untouched
    LogClear();
    uint8_t before[0x62];
    memcpy(before, fn, sizeof before);
    assertguard::Start(si);
    CHECK(assertguard::Installed(), "default not installed");
    CHECK(WaitLog("ASSERTGUARD: installed at +18CBDF7"), "missing install log");
    CHECK(memcmp(before, fn, sizeof before) == 0, "guard changed game code bytes");
    CHECK(assertguard::Site() == reinterpret_cast<uintptr_t>(site), "site");

    // 4. the game's int3 fires: the guard resumes at the next instruction (here a ret) and logs the game's message
    const char kMsg[] = "CResourceDatabase::DeqCreate CreateResource failed. [nResFileID:14957]";
    memcpy(g_img + kBufRva, kMsg, sizeof kMsg);
    const uint8_t saved = site[1];
    site[1] = 0xC3;
    FlushInstructionCache(GetCurrentProcess(), site, 2);
    DWORD code = 0;
    CHECK(RunAt(site, &code), "game int3 not continued (exception %08lX)", code);
    CHECK(assertguard::Hits() == 1, "hits %ld want 1", assertguard::Hits());
    CHECK(WaitLog("ASSERTGUARD: game assert from"), "missing hit log");
    CHECK(LogHas(kMsg), "hit log lacks the game's message");
    CHECK(LogHas("(hit 1;"), "hit count in log");

    // 5. a message with a format specifier / newline stays one safe line
    LogClear();
    const char kFmt[] = "bad %s %n value\nline2";
    memcpy(g_img + kBufRva, kFmt, sizeof kFmt);
    CHECK(RunAt(site, &code), "second int3 not continued (exception %08lX)", code);
    CHECK(WaitLog("bad  s  n value line2"), "format/newline not neutralised");
    CHECK(assertguard::Hits() == 2, "hits %ld want 2", assertguard::Hits());
    site[1] = saved;

    // 6. any other breakpoint is passed on unchanged (crash report keeps it)
    uint8_t* other = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    other[0] = 0xCC;
    other[1] = 0xC3;
    FlushInstructionCache(GetCurrentProcess(), other, 2);
    CHECK(!RunAt(other, &code) && code == EXCEPTION_BREAKPOINT, "foreign int3 swallowed (exception %08lX)", code);
    CHECK(assertguard::Hits() == 2, "foreign int3 counted");
    // the same int3 byte one instruction later is not the logger's: passed on
    const uint8_t saved2 = site[2];
    site[1] = 0xCC;
    site[2] = 0xC3;
    FlushInstructionCache(GetCurrentProcess(), site, 3);
    CHECK(!RunAt(site + 1, &code) && code == EXCEPTION_BREAKPOINT, "int3 at +1 swallowed (exception %08lX)", code);
    site[1] = saved;
    site[2] = saved2;

    // 7. a second Start does not add a second handler
    assertguard::Start(si);
    CHECK(assertguard::Installed(), "still installed");

    if (g_fail) {
        printf("%d FAILED\n", g_fail);
        return 1;
    }
    printf("ALL OK\n");
    return 0;
}
