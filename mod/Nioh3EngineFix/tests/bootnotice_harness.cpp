// bootnotice_harness.cpp - offline test of bootnotice.cpp (SkipBootNotice): builds a fake v2.02 image with the dump's bytes
// at the boot-sequencer sites, runs Start() for each ini mode, then calls the installed thunks with stub targets.
//   py -I tests/run_bootnotice_harness.py
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/bootnotice.h"

static int g_fail = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);              \
            printf("\n");                     \
            ++g_fail;                         \
        }                                     \
    } while (0)

static std::vector<std::string> g_lines;
static void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    g_lines.push_back(b);
    printf("  log: %s\n", b);
}
static bool Logged(const char* s) {
    for (auto& l : g_lines)
        if (l.find(s) != std::string::npos) return true;
    return false;
}

constexpr uint32_t kSize = 0x52FB000, kSite1 = 0x1C35393, kSite2 = 0x1C353A2, kSeqAt = 0x1C35372, kFree = 0x310C18,
                   kShow = 0xE6DA30, kCheck = 0x1FE3D38;
static const uint8_t kSeq[] = {0xB9, 0xF1, 0xAB, 0xC4, 0x03, 0xE8, 0x88, 0xF7, 0x96, 0xFE, 0x48, 0x8B, 0xD0, 0x48, 0x8D, 0x4D, 0xB0,
                               0xE8, 0xEC, 0xAF, 0x6D, 0xFE, 0xBE, 0x01, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xD0, 0x44, 0x8A, 0xC6,
                               0xE8, 0x98, 0x86, 0x23, 0xFF, 0x44, 0x38, 0x6F, 0x55, 0x0F, 0x85, 0xAD, 0x01, 0x00, 0x00,
                               0xE8, 0x91, 0xE9, 0x3A, 0x00, 0x84, 0xC0, 0x0F, 0x84, 0xA0, 0x01, 0x00, 0x00};
static const uint8_t kFreeB[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x83, 0x79, 0x18, 0x08, 0x48, 0x8B, 0xD9, 0x72};

// stubs written into the fake image (they record into globals via absolute addresses)
static volatile uint64_t g_freeArg = 0, g_showArg = 0;
static volatile long g_showCalls = 0, g_checkCalls = 0;
static volatile uint8_t g_checkAnswer = 1;

static void Commit(uint8_t* base, uint32_t rva, size_t n) { VirtualAlloc(base + rva, n, MEM_COMMIT, PAGE_EXECUTE_READWRITE); }

// mov [abs], rcx/rdx via rax: 48 B8 imm64 ; 48 89 08 (rcx) / 48 89 10 (rdx) ; ret
static void StubStore(uint8_t* p, volatile void* slot, bool rdx, volatile long* cnt) {
    size_t i = 0;
    p[i++] = 0x48, p[i++] = 0xB8;
    uint64_t a = (uint64_t)slot;
    memcpy(p + i, &a, 8), i += 8;
    p[i++] = 0x48, p[i++] = 0x89, p[i++] = rdx ? 0x10 : 0x08;
    if (cnt) {
        p[i++] = 0x48, p[i++] = 0xB8;
        a = (uint64_t)cnt;
        memcpy(p + i, &a, 8), i += 8;
        p[i++] = 0xF0, p[i++] = 0xFF, p[i++] = 0x00;  // lock inc dword [rax]
    }
    p[i++] = 0xC3;
}
// check stub: inc count, al = [answer]
static void StubCheck(uint8_t* p) {
    size_t i = 0;
    uint64_t a = (uint64_t)&g_checkCalls;
    p[i++] = 0x48, p[i++] = 0xB8, memcpy(p + i, &a, 8), i += 8;
    p[i++] = 0xF0, p[i++] = 0xFF, p[i++] = 0x00;
    a = (uint64_t)&g_checkAnswer;
    p[i++] = 0x48, p[i++] = 0xB8, memcpy(p + i, &a, 8), i += 8;
    p[i++] = 0x8A, p[i++] = 0x00;  // mov al, [rax]
    p[i++] = 0xC3;
}

static uint8_t* MakeImage() {
    uint8_t* b = (uint8_t*)VirtualAlloc(nullptr, kSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!b) return nullptr;
    Commit(b, 0, 0x1000);
    auto* dos = (IMAGE_DOS_HEADER*)b;
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = (IMAGE_NT_HEADERS64*)(b + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->OptionalHeader.SizeOfImage = kSize;
    Commit(b, kSeqAt & ~0xFFFu, 0x2000);
    memcpy(b + kSeqAt, kSeq, sizeof(kSeq));
    Commit(b, kFree & ~0xFFFu, 0x1000);
    memcpy(b + kFree, kFreeB, sizeof(kFreeB));
    Commit(b, kShow & ~0xFFFu, 0x1000);
    Commit(b, kCheck & ~0xFFFu, 0x1000);
    Commit(b, 0x45C8480 & ~0xFFFu, 0x1000);
    return b;
}

static void WriteIni(const wchar_t* path, const char* body) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"wb") || !f) return;
    fputs(body, f);
    fclose(f);
}

typedef void (*ShowFn)(void* unused, void* str);
typedef bool (*CheckFn)();

static uint8_t* Thunk(uint8_t* site) {
    int32_t r;
    memcpy(&r, site + 1, 4);
    return site + 5 + r;
}

int main() {
    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"bootnotice_harness.ini");

    struct Case {
        const char* name;
        const char* ini;
        int expect;  // 0 not installed, 1 skip, 2 log only
    } cases[] = {
        {"default, normal player", "[Debug]\r\n", 0},
        {"default + Tour", "[Debug]\r\nTour = 1\r\n", 1},
        {"default + VirtualPad", "[Debug]\r\nVirtualPad = 1\r\n", 1},
        {"explicit 0 beats Tour", "[Debug]\r\nTour = 1\r\nSkipBootNotice = 0\r\n", 0},
        {"explicit 1", "[Debug]\r\nSkipBootNotice = 1\r\n", 1},
        {"explicit 2 (log only)", "[Debug]\r\nSkipBootNotice = 2\r\n", 2},
    };
    for (auto& c : cases) {
        printf("case: %s\n", c.name);
        g_lines.clear();
        uint8_t* b = MakeImage();
        CHECK(b, "no image");
        WriteIni(ini, c.ini);
        bootnotice::StartInfo si;
        si.log = &Log;
        si.iniPath = ini;
        si.base = (uintptr_t)b;
        si.knownBuild = true;
        bootnotice::Start(si);
        uint8_t* s1 = b + kSite1;
        uint8_t* s2 = b + kSite2;
        const bool patched = memcmp(b + kSeqAt, kSeq, sizeof(kSeq)) != 0;
        if (c.expect == 0) {
            CHECK(!patched, "%s: patched but should not be", c.name);
            continue;
        }
        CHECK(patched && s1[0] == 0xE8 && s2[0] == 0xE8, "%s: not patched", c.name);
        CHECK(Logged("BootNotice: installed"), "%s: no install line", c.name);
        // only the two 5-byte sites changed
        for (size_t i = 0; i < sizeof(kSeq); ++i) {
            const uint32_t rva = kSeqAt + (uint32_t)i;
            const bool inSite = (rva >= kSite1 && rva < kSite1 + 5) || (rva >= kSite2 && rva < kSite2 + 5);
            if (!inSite) CHECK(b[rva] == kSeq[i], "%s: byte %X changed", c.name, rva);
        }
        // stubs at the targets (after Start: it verified the real bytes at kFree)
        StubStore(b + kFree, &g_freeArg, false, nullptr);
        StubStore(b + kShow, &g_showArg, true, &g_showCalls);
        StubCheck(b + kCheck);
        FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
        g_freeArg = g_showArg = 0;
        g_showCalls = g_checkCalls = 0;
        uint64_t str[4] = {0x1234, 0, 0, 7};
        ((ShowFn)Thunk(s1))(nullptr, str);
        if (c.expect == 1) {
            CHECK(g_freeArg == (uint64_t)str && g_showCalls == 0, "%s: notice not skipped (free %llx show %ld)", c.name,
                  (unsigned long long)g_freeArg, g_showCalls);
            CHECK(bootnotice::NoticesSkipped() >= 1, "%s: skip count", c.name);
        } else {
            CHECK(g_showArg == (uint64_t)str && g_showCalls == 1 && g_freeArg == 0, "%s: log-only must call the original", c.name);
        }
        g_checkAnswer = 1;
        const bool r1 = ((CheckFn)Thunk(s2))();
        g_checkAnswer = 0;
        const bool r0 = ((CheckFn)Thunk(s2))();
        CHECK(g_checkCalls == 2, "%s: check called %ld times", c.name, g_checkCalls);
        CHECK(!r0, "%s: false must stay false", c.name);
        if (c.expect == 1) {
            CHECK(!r1, "%s: warning not suppressed", c.name);
            CHECK(bootnotice::WarningsSkipped() >= 1, "%s: warning count", c.name);
        } else {
            CHECK(r1, "%s: log-only must pass true through", c.name);
        }
        Sleep(200);  // watcher: logs the skip counters
        if (c.expect == 1) CHECK(Logged("skipped the 'This game has an autosave feature'"), "%s: watcher line", c.name);
    }
    // wrong bytes -> refused
    {
        printf("case: bytes differ\n");
        g_lines.clear();
        uint8_t* b = MakeImage();
        b[kSeqAt] = 0x90;
        WriteIni(ini, "[Debug]\r\nSkipBootNotice = 1\r\n");
        bootnotice::StartInfo si;
        si.log = &Log;
        si.iniPath = ini;
        si.base = (uintptr_t)b;
        si.knownBuild = true;
        bootnotice::Start(si);
        CHECK(Logged("not installed (boot sequencer code bytes differ"), "refusal line");
        CHECK(b[kSite1] == 0xE8 && b[kSite1 + 1] == 0x98, "site touched");
    }
    DeleteFileW(ini);
    printf(g_fail ? "BOOTNOTICE HARNESS: %d FAILURE(S)\n" : "BOOTNOTICE HARNESS: ALL OK\n", g_fail);
    return g_fail ? 1 : 0;
}
