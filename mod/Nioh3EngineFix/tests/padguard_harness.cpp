// padguard_harness.cpp - offline test of padguard.cpp (PadListGuard, GitHub #79 / #55): a fake v2.02 image with the dump's bytes at
// every pad list-walk site (and the first instruction at each exit), Start() for each ini / byte case, then every site is entered
// with a node of 0 / head / another node, unpatched and patched, and the stop address (ud2 at the walk's continue / exit),
// all GPRs and the arithmetic flags are compared. Also checks the chained unwind of the caves and the PADGUARD log line.
//   py -I tests/run_padguard_harness.py
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/padguard.h"
#include "../src/padguard_cave.h"

using padguard_cave::kNumSites;
using padguard_cave::kWalks;

static int g_fail = 0;
#define CHECK(c, ...)                                  \
    do {                                               \
        if (!(c)) {                                    \
            printf("FAIL %s:%d ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                       \
            printf("\n");                              \
            ++g_fail;                                  \
        }                                              \
    } while (0)

static std::vector<std::string> g_lines;
static CRITICAL_SECTION g_logLock;
static void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    EnterCriticalSection(&g_logLock);
    g_lines.push_back(b);
    LeaveCriticalSection(&g_logLock);
    printf("  log: %s\n", b);
}
static bool Logged(const char* s) {
    EnterCriticalSection(&g_logLock);
    bool f = false;
    for (auto& l : g_lines)
        if (l.find(s) != std::string::npos) f = true;
    LeaveCriticalSection(&g_logLock);
    return f;
}

constexpr uint32_t kSize = 0x52FB000;
constexpr uint32_t kFakeUnwind = 0x1000;  // fake host UNWIND_INFO: push rbx ; sub rsp,20h
struct Host {
    uint32_t begin, end;
};
static const Host kHosts[] = {{0x42FAC8, 0x4305A4}, {0x4309F8, 0x430B56}, {0x430B58, 0x431409}, {0x438604, 0x438706}};  // dump .pdata
static RUNTIME_FUNCTION g_hostRf[4];

static void Commit(uint8_t* base, uint32_t rva, size_t n) {
    const uint32_t lo = rva & ~0xFFFu;
    VirtualAlloc(base + lo, (rva + n + 0xFFF - lo) & ~(size_t)0xFFF, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}
static uintptr_t ExitRva(int k) { return padguard_cave::ExitOf(kWalks[k], kWalks[k].rva); }
static uintptr_t ContRva(int k) { return kWalks[k].rva + kWalks[k].len; }

static uint8_t* MakeImage(bool unwind) {
    uint8_t* b = (uint8_t*)VirtualAlloc(nullptr, kSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!b) return nullptr;
    Commit(b, 0, 0x2000);
    auto* dos = (IMAGE_DOS_HEADER*)b;
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = (IMAGE_NT_HEADERS64*)(b + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->OptionalHeader.SizeOfImage = kSize;
    for (int k = 0; k < kNumSites; ++k) {
        Commit(b, kWalks[k].rva, 16);
        memcpy(b + kWalks[k].rva, kWalks[k].orig, kWalks[k].len);
        Commit(b, (uint32_t)ExitRva(k), 16);
        memcpy(b + ExitRva(k), kWalks[k].exitBytes, kWalks[k].exitLen);
    }
    if (unwind) {  // the hosts' RUNTIME_FUNCTIONs (fake prolog: push rbx ; sub rsp,20h), like the game's .pdata
        const uint8_t ui[] = {1, 6, 2, 0, 6, 0x32, 1, 0x30};  // v1, prolog 6, 2 codes: @6 ALLOC_SMALL 20h, @1 PUSH_NONVOL rbx
        memcpy(b + kFakeUnwind, ui, sizeof ui);
        for (int i = 0; i < 4; ++i) g_hostRf[i] = {kHosts[i].begin, kHosts[i].end, kFakeUnwind};
        CHECK(RtlAddFunctionTable(g_hostRf, 4, (DWORD64)b), "host function table");
    }
    return b;
}

static void WriteIni(const wchar_t* path, const char* body) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"wb") || !f) return;
    fputs(body, f);
    fclose(f);
}

// ---- driver: set every GPR, jump to the site; ud2 at the walk's continue / exit stops it (vectored handler -> landing)
static uint8_t* g_code = nullptr;
static volatile uint64_t g_savedRsp = 0;
static uint8_t* g_landing = nullptr;
static CONTEXT g_stop;
static DWORD g_stopCode = 0;

static LONG CALLBACK Veh(EXCEPTION_POINTERS* e) {
    if (!g_landing) return EXCEPTION_CONTINUE_SEARCH;
    g_stop = *e->ContextRecord;
    g_stopCode = e->ExceptionRecord->ExceptionCode;
    e->ContextRecord->Rsp = g_savedRsp;
    e->ContextRecord->Rip = (DWORD64)g_landing;
    return EXCEPTION_CONTINUE_EXECUTION;
}

struct Regs {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r12, r13, r14, r15;
};
// the list head doubles as the object holding it: [H+18h] = [H+58h] = H, so `cmp node, r14` and `cmp node, [r14+18h]` both see H
static uint64_t g_head[0x20], g_other[4];

static void Imm(std::vector<uint8_t>& c, uint8_t rex, uint8_t op, uint64_t v) {
    c.push_back(rex), c.push_back(op);
    for (int i = 0; i < 8; ++i) c.push_back((uint8_t)(v >> (8 * i)));
}

// runs site k with node = v; returns the stop context
static CONTEXT Run(uint8_t* img, int k, uint64_t v, Regs* in) {
    const uint64_t H = (uint64_t)g_head;
    Regs r{H, H, 0x1111, 0x2222, H, H, 0x3333, 0x8888, 0x9999, 0xAAAA, H, 0xDDDD, H, 0xFFFF};
    if (kWalks[k].node == padguard_cave::kRbx) r.rbx = v;
    else r.rdi = v;
    *in = r;
    std::vector<uint8_t> c = {0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x08};
    Imm(c, 0x48, 0xB8, (uint64_t)&g_savedRsp);
    c.insert(c.end(), {0x48, 0x89, 0x20});  // mov [rax], rsp
    Imm(c, 0x48, 0xB8, r.rax), Imm(c, 0x48, 0xBB, r.rbx), Imm(c, 0x48, 0xB9, r.rcx), Imm(c, 0x48, 0xBA, r.rdx);
    Imm(c, 0x48, 0xBE, r.rsi), Imm(c, 0x48, 0xBF, r.rdi), Imm(c, 0x48, 0xBD, r.rbp), Imm(c, 0x49, 0xB8, r.r8);
    Imm(c, 0x49, 0xB9, r.r9), Imm(c, 0x49, 0xBA, r.r10), Imm(c, 0x49, 0xBC, r.r12), Imm(c, 0x49, 0xBD, r.r13);
    Imm(c, 0x49, 0xBE, r.r14), Imm(c, 0x49, 0xBF, r.r15);
    Imm(c, 0x49, 0xBB, (uint64_t)(img + kWalks[k].rva));
    c.insert(c.end(), {0x41, 0xFF, 0xE3});  // jmp r11
    const size_t landing = c.size();
    c.insert(c.end(), {0x48, 0x83, 0xC4, 0x08, 0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5F, 0x5E, 0x5D, 0x5B, 0xC3});
    memcpy(g_code, c.data(), c.size());
    g_landing = g_code + landing;
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    memset(&g_stop, 0, sizeof g_stop);
    g_stopCode = 0;
    ((void (*)())g_code)();
    g_landing = nullptr;
    return g_stop;
}

static void Stubs(uint8_t* img) {  // ud2 at every continue / exit
    for (int k = 0; k < kNumSites; ++k) {
        memcpy(img + ContRva(k), "\x0F\x0B", 2);
        memcpy(img + ExitRva(k), "\x0F\x0B", 2);
    }
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
}
static void Exits(uint8_t* img) {  // the real first instruction back at every exit (Start verifies it)
    for (int k = 0; k < kNumSites; ++k) memset(img + ContRva(k), 0, 2);
    for (int k = 0; k < kNumSites; ++k) memcpy(img + ExitRva(k), kWalks[k].exitBytes, kWalks[k].exitLen);
}

constexpr DWORD kFlags = 0x8D5;  // CF PF AF ZF SF OF
static bool SameRegs(const CONTEXT& a, const CONTEXT& b) {
    return a.Rax == b.Rax && a.Rbx == b.Rbx && a.Rcx == b.Rcx && a.Rdx == b.Rdx && a.Rsi == b.Rsi && a.Rdi == b.Rdi && a.Rbp == b.Rbp &&
           a.R8 == b.R8 && a.R9 == b.R9 && a.R10 == b.R10 && a.R12 == b.R12 && a.R13 == b.R13 && a.R14 == b.R14 && a.R15 == b.R15;
}
static bool RegsAre(const CONTEXT& a, const Regs& r) {
    return a.Rax == r.rax && a.Rbx == r.rbx && a.Rcx == r.rcx && a.Rdx == r.rdx && a.Rsi == r.rsi && a.Rdi == r.rdi && a.Rbp == r.rbp &&
           a.R8 == r.r8 && a.R9 == r.r9 && a.R10 == r.r10 && a.R12 == r.r12 && a.R13 == r.r13 && a.R14 == r.r14 && a.R15 == r.r15;
}

static uint8_t* CaveOf(uint8_t* img, int k) {
    const uint8_t* s = img + kWalks[k].rva;
    int32_t d;
    memcpy(&d, s + 1, 4);
    return (uint8_t*)s + 5 + d;
}

int main() {
    InitializeCriticalSection(&g_logLock);
    g_code = (uint8_t*)VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    AddVectoredExceptionHandler(1, Veh);
    g_head[0x18 / 8] = (uint64_t)g_head;
    g_head[0x58 / 8] = (uint64_t)g_head;
    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"padguard_harness.ini");

    // table sanity: shapes, continue / exit stubs never overlap a site
    for (int k = 0; k < kNumSites; ++k) {
        CHECK(padguard_cave::Shaped(kWalks[k]), "site %X shape", kWalks[k].rva);
        for (int j = 0; j < kNumSites; ++j)
            for (uintptr_t a : {ContRva(k), ExitRva(k)})
                CHECK(a + 2 <= kWalks[j].rva || a >= kWalks[j].rva + kWalks[j].len || a == kWalks[j].rva,
                      "stub %zX overlaps site %X", (size_t)a, kWalks[j].rva);
    }

    // ---- default ini: install, semantics, unwind, log
    {
        printf("case: default ini (PadListGuard missing = 1)\n");
        g_lines.clear();
        uint8_t* b = MakeImage(true);
        CHECK(b, "no image");
        WriteIni(ini, "[General]\r\n");
        // unpatched reference runs
        Stubs(b);
        CONTEXT ref[kNumSites][3];
        for (int k = 0; k < kNumSites; ++k) {
            const uint64_t vals[3] = {0, (uint64_t)g_head, (uint64_t)g_other};
            for (int c = 0; c < 3; ++c) {
                Regs in;
                ref[k][c] = Run(b, k, vals[c], &in);
                CHECK(g_stopCode == STATUS_ILLEGAL_INSTRUCTION, "site %X unpatched case %d stop %08lX", kWalks[k].rva, c, g_stopCode);
                CHECK(RegsAre(ref[k][c], in), "site %X unpatched case %d regs", kWalks[k].rva, c);
            }
            CHECK(ref[k][0].Rip == (DWORD64)(b + ContRva(k)), "site %X: stock code walks on with a null node", kWalks[k].rva);
            CHECK(ref[k][1].Rip == (DWORD64)(b + ExitRva(k)), "site %X: stock head -> exit", kWalks[k].rva);
            CHECK(ref[k][2].Rip == (DWORD64)(b + ContRva(k)), "site %X: stock other -> continue", kWalks[k].rva);
        }
        Exits(b);
        padguard::StartInfo si;
        si.log = &Log;
        si.iniPath = ini;
        si.base = (uintptr_t)b;
        si.knownBuild = true;
        padguard::Start(si);
        CHECK(Logged("PADGUARD: installed at 13 of 13 pad list walks"), "install line");
        CHECK(Logged("unwind registered"), "unwind registered");
        for (int k = 0; k < kNumSites; ++k) {
            const uint8_t* s = b + kWalks[k].rva;
            CHECK(s[0] == 0xE9, "site %X not patched", kWalks[k].rva);
            for (int i = 5; i < kWalks[k].len; ++i) CHECK(s[i] == 0x90, "site %X pad byte %d", kWalks[k].rva, i);
        }
        Stubs(b);
        for (int k = 0; k < kNumSites; ++k) {
            const long before = padguard::Hits();
            Regs in;
            const CONTEXT n0 = Run(b, k, 0, &in);
            CHECK(g_stopCode == STATUS_ILLEGAL_INSTRUCTION && n0.Rip == (DWORD64)(b + ExitRva(k)), "site %X: null node must end the walk",
                  kWalks[k].rva);
            CHECK(RegsAre(n0, in), "site %X: null case changed a register", kWalks[k].rva);
            CHECK((n0.EFlags & kFlags) == (ref[k][1].EFlags & kFlags), "site %X: null exit flags %lX != stock end-of-list flags %lX",
                  kWalks[k].rva, n0.EFlags & kFlags, ref[k][1].EFlags & kFlags);
            CHECK(padguard::Hits() == before + 1, "site %X: hit not counted", kWalks[k].rva);
            const CONTEXT n1 = Run(b, k, (uint64_t)g_head, &in);
            CHECK(n1.Rip == ref[k][1].Rip && SameRegs(n1, ref[k][1]) && (n1.EFlags & kFlags) == (ref[k][1].EFlags & kFlags),
                  "site %X: head case differs from stock", kWalks[k].rva);
            const CONTEXT n2 = Run(b, k, (uint64_t)g_other, &in);
            CHECK(n2.Rip == ref[k][2].Rip && SameRegs(n2, ref[k][2]) && (n2.EFlags & kFlags) == (ref[k][2].EFlags & kFlags),
                  "site %X: non-null walk differs from stock", kWalks[k].rva);
            CHECK(padguard::Hits() == before + 1, "site %X: non-null cases counted", kWalks[k].rva);
        }
        // chained unwind: a stack walk stopped anywhere in a cave unwinds like the host (push rbx ; sub rsp,20h)
        for (int k = 0; k < kNumSites; ++k) {
            uint8_t* cave = CaveOf(b, k);
            const size_t c = kWalks[k].cmpLen;
            const size_t pcs[] = {0, 5, 5 + c + 2, 14 + c, 21 + c, 31 + c};  // test, cmp, jmp cont, jmp exit, lock inc, jmp exit
            for (size_t off : pcs) {
                DWORD64 ib = 0;
                PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry((DWORD64)(cave + off), &ib, nullptr);
                CHECK(rf != nullptr, "site %X: no function entry for cave+%zu", kWalks[k].rva, off);
                if (!rf) continue;
                alignas(16) uint64_t stack[8] = {0, 0, 0, 0, 0xB0B0B0B0, 0x7777777700, 0, 0};
                CONTEXT ctx{};
                ctx.ContextFlags = CONTEXT_FULL;
                ctx.Rsp = (DWORD64)stack;
                ctx.Rip = (DWORD64)(cave + off);
                ctx.Rbx = 0;
                void* hd = nullptr;
                DWORD64 est = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, ib, ctx.Rip, rf, &ctx, &hd, &est, nullptr);
                CHECK(ctx.Rip == 0x7777777700 && ctx.Rsp == (DWORD64)(stack + 6) && ctx.Rbx == 0xB0B0B0B0,
                      "site %X: unwind from cave+%zu -> rip %llX rsp +%lld rbx %llX", kWalks[k].rva, off, (unsigned long long)ctx.Rip,
                      (long long)(ctx.Rsp - (DWORD64)stack), (unsigned long long)ctx.Rbx);
            }
        }
        Sleep(400);  // watcher
        CHECK(Logged("PADGUARD: null list link at +42FB3E (hit 1)"), "hit line for 42FB3E");
        CHECK(Logged("PADGUARD: null list link at +430A38 (hit 1)"), "hit line for 430A38");
        // rate limit: 30 more hits at one site within a second -> at most 20 lines for it
        for (int i = 0; i < 30; ++i) {
            Regs in;
            Run(b, 0, 0, &in);
            Sleep(i < 25 ? 120 : 0);
        }
        Sleep(300);
        int lines = 0;
        for (auto& l : g_lines)
            if (l.find("+42FB3E (hit") != std::string::npos) ++lines;
        CHECK(lines >= 15 && lines <= 20, "rate limit: %d lines for 42FB3E", lines);
        RtlDeleteFunctionTable(g_hostRf);
    }
    // ---- PadListGuard = 0: nothing written
    {
        printf("case: PadListGuard = 0\n");
        g_lines.clear();
        uint8_t* b = MakeImage(false);
        WriteIni(ini, "[General]\r\nPadListGuard = 0\r\n");
        padguard::StartInfo si;
        si.log = &Log;
        si.iniPath = ini;
        si.base = (uintptr_t)b;
        si.knownBuild = true;
        padguard::Start(si);
        for (int k = 0; k < kNumSites; ++k) CHECK(!memcmp(b + kWalks[k].rva, kWalks[k].orig, kWalks[k].len), "site %X written while off", kWalks[k].rva);
        CHECK(Logged("PADGUARD: off"), "off line");
    }
    // ---- one site's bytes differ (another patch): that walk alone is skipped; an exit that differs skips its site too
    {
        printf("case: bytes differ at 430A38, exit differs at 438659\n");
        g_lines.clear();
        uint8_t* b = MakeImage(false);
        b[0x430A38 + 3] = 0x50;  // cmp rbx,[rsi+50h]
        b[ExitRva(8) + 1] ^= 0x01;
        WriteIni(ini, "[General]\r\nPadListGuard = 1\r\n");
        padguard::StartInfo si;
        si.log = &Log;
        si.iniPath = ini;
        si.base = (uintptr_t)b;
        si.knownBuild = true;
        padguard::Start(si);
        CHECK(b[0x430A38] == 0x48 && b[0x430A38 + 3] == 0x50, "differing site written");
        CHECK(!memcmp(b + 0x438659, kWalks[8].orig, kWalks[8].len), "site with a differing exit written");
        CHECK(b[0x42FB3E] == 0xE9, "other sites must still install");
        CHECK(Logged("PADGUARD: installed at 11 of 13") && Logged("walks NOT guarded (bytes differ - another patch or build): 430A38 438659"),
              "partial install lines");
        CHECK(Logged("unwind NOT registered"), "no host table -> unwind not registered (logged)");
    }
    // ---- every site differs: not installed, nothing allocated
    {
        printf("case: no site matches\n");
        g_lines.clear();
        uint8_t* b = MakeImage(false);
        for (int k = 0; k < kNumSites; ++k) b[kWalks[k].rva] = 0x90;
        WriteIni(ini, "[General]\r\n");
        padguard::StartInfo si;
        si.log = &Log;
        si.iniPath = ini;
        si.base = (uintptr_t)b;
        si.knownBuild = true;
        padguard::Start(si);
        CHECK(Logged("PADGUARD: not installed (pad manager code bytes differ at every site"), "refusal line");
    }
    DeleteFileW(ini);
    if (g_fail) {
        printf("FAILED (%d)\n", g_fail);
        return 1;
    }
    printf("ALL OK: 13 sites x {null, head, other}: null ends the walk at the stock exit with stock end-of-list flags and no register "
           "change, head / other identical to stock; counted + logged (rate-limited); chained unwind; off / partial / refused cases\n");
    return 0;
}
