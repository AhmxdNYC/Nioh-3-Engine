// invlog_harness.cpp - offline tests of the InvLog hook in src/bosslog.cpp ([Debug] InvLog). Not a game test.
// Built and run by tests\run_invlog_harness.py. Prints "ALL PASSED" on success.
// bosslog.cpp is included as source. A fake "image" (one RWX region) holds a stand-in InventoryAddItem at its real RVA 54D324 with the
// real 5-byte prologue (push rbp/rbx/rsi/rdi), so the real Install() relocates it exactly as in the game.
#include "../src/bosslog.cpp"

#include <cstdarg>
#include <string>
#include <vector>

using namespace bosslog;

static int g_fails = 0;
#define CHECK(c, ...)                      \
    do {                                   \
        if (!(c)) {                        \
            ++g_fails;                     \
            printf("FAIL %d: ", __LINE__); \
            printf(__VA_ARGS__);           \
            printf("\n");                  \
        }                                  \
    } while (0)

static std::vector<std::string> g_lines;
static void Log(const char* fmt, ...) {
    char b[1024];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof(b), fmt, a);
    va_end(a);
    g_lines.push_back(b);
}
static int Count(const char* sub) {
    int n = 0;
    for (auto& l : g_lines) n += l.find(sub) != std::string::npos;
    return n;
}

static uint8_t* B = nullptr;
constexpr size_t kImg = 0x1000000;
using AddFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int main() {
    B = (uint8_t*)VirtualAlloc(nullptr, kImg, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!B) return 2;
    g_log = &Log;
    g_base = (uintptr_t)B;
    g_imgEnd = g_base + kImg;
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    g_qpcSec = 1.0 / (double)fq.QuadPart;
    g_qpc0 = Qpc();
    // stand-in: real prologue, then rax = rcx + rdx + r8 + r9 + 5th arg, epilogue
    const uint8_t fn[] = {0x40, 0x55, 0x53, 0x56, 0x57,         // push rbp, rbx, rsi, rdi (the real 5 bytes)
                          0x48, 0x8D, 0x04, 0x11,               // lea rax,[rcx+rdx]
                          0x4C, 0x01, 0xC0, 0x4C, 0x01, 0xC8,   // add rax,r8 ; add rax,r9
                          0x48, 0x03, 0x44, 0x24, 0x48,         // add rax,[rsp+48h] (5th arg after 4 pushes)
                          0x5F, 0x5E, 0x5B, 0x5D, 0xC3};        // pop rdi, rsi, rbx, rbp ; ret
    const Hook& h = kInvHooks[0];
    CHECK(h.rva == 0x54D324 && h.n == 5 && !memcmp(h.orig, fn, 5), "kInvHooks[0] = InventoryAddItem 54D324, 5 prologue bytes");
    memcpy(B + h.rva, fn, sizeof(fn));
    auto add = (AddFn)(B + h.rva);
    CHECK(add(1, 2, 3, 4, 5) == 15, "stand-in itself");

    // a wrong prologue is refused, nothing written
    B[h.rva] = 0x48;
    g_thunks = AllocNear(g_base, g_imgEnd);
    CHECK(g_thunks != nullptr, "thunk page near the fake image");
    CHECK(Install(h) == 1 && B[h.rva] == 0x48, "bytes differ -> refused");
    B[h.rva] = 0x40;
    CHECK(Install(h) == 0, "installs");
    CHECK(B[h.rva] == 0xE9, "entry = jmp to the thunk");

    // one call: original result, args intact, one log line with the item fields
    uint8_t desc[0xE8] = {};
    *(uint16_t*)desc = 0x1234;
    *(uint16_t*)(desc + 2) = 0x1200;
    desc[0x30] = 3;
    const uint64_t d = (uint64_t)desc;
    g_lines.clear();
    CHECK(add(0x1000, 0x2000, d, 7, 1) == 0x3000 + d + 7 + 1, "result through the hook");
    CHECK(g_lines.size() == 1, "one line (%zu)", g_lines.size());
    if (!g_lines.empty()) {
        const std::string& l = g_lines[0];
        CHECK(l.find("InvLog #1: add item id=4660(0x1234) base=4608 b30=3 notify=1 r9=7 caller=+") == 0, "line fields: %s", l.c_str());
        CHECK(l.find("desc[48]=34 12 00 12") != std::string::npos, "desc hex: %s", l.c_str());
        CHECK(l.find("src=?") != std::string::npos, "unknown caller -> src=?: %s", l.c_str());
    }
    // rate limit: 30 lines per second, then one summary line in the next second
    g_lines.clear();
    const int64_t s0 = (int64_t)Now();
    int calls = 0;
    for (; calls < 50 && (int64_t)Now() == s0; ++calls) add(0, 0, d, 0, 0);
    const int logged = Count("InvLog #");
    CHECK(logged <= 30, "at most 30 lines in one second (%d)", logged);
    while ((int64_t)Now() == s0) Sleep(5);
    add(0, 0, d, 0, 0);
    CHECK(Count("calls suppressed by the 30/s limit") == (calls > logged ? 1 : 0), "suppressed calls reported once (calls %d, logged %d)", calls, logged);
    CHECK(calls == logged || logged >= 29, "the limit only drops after ~30 lines (calls %d, logged %d)", calls, logged);
    // helpers
    CHECK(!LooksLikeRet(0x10) && !LooksLikeRet(g_base + 0x10), "LooksLikeRet: outside / image head");
    B[0x2000] = 0xE8;
    CHECK(LooksLikeRet(g_base + 0x2005), "LooksLikeRet: after a call rel32");
    CHECK(!strcmp(InvSource(0x23A0A98), "script Item::AddItem") && !strcmp(InvSource(0x1), "?"), "source labels");
    printf(g_fails ? "%d FAILED\n" : "ALL PASSED\n", g_fails);
    return g_fails ? 1 : 0;
}
