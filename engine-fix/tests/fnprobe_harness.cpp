// fnprobe_harness.cpp - offline tests for the FnProbe entry thunk and prologue decoder (src\fnprobe_core.h). Not a game test.
//   fnprobe_harness.exe                 thunk tests (register-exact, counts, sampling, args, 4-thread exact count) -> ALL PASSED
//   fnprobe_harness.exe decode <dump> <rva-file>   prints "rva len" per RVA (PrologueLen on the dump bytes), for capstone checks
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "../src/fnprobe_core.h"

extern "C" {
void RunProbe(const void* fn, const void* in);
void Clobber();
void TargetA();
void TargetB();
void TargetC();
void TargetD();
extern uint8_t p_in[160];
extern uint8_t p_out[176];
extern uint64_t p_entry_rsp;
}

static int g_fail = 0;
#define CHECK(c, ...)                   \
    do {                                \
        if (!(c)) {                     \
            printf("FAIL: " __VA_ARGS__); \
            printf("\n");               \
            ++g_fail;                   \
        }                               \
    } while (0)

struct HookRec {
    uint64_t a[4];
    uint64_t sp;
    uint64_t ret;
    uint64_t old;
};
static std::vector<HookRec> g_recs;
static bool g_record = true;

static void TestHook(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, const uint64_t* sp, uint64_t old) {
    if (g_record) g_recs.push_back({{a0, a1, a2, a3}, (uint64_t)sp, sp[0], old});
    Clobber();  // the thunk must hide every volatile the hook writes
}
static void NopHook(uint64_t, uint64_t, uint64_t, uint64_t, const uint64_t*, uint64_t) { Clobber(); }

static void FillIn(uint64_t seed, uint64_t flags) {
    uint64_t* q = (uint64_t*)p_in;
    for (int i = 0; i < 7; ++i) q[i] = 0x1111111111111111ull * (uint64_t)(i + 1) ^ seed;
    q[7] = flags;
    for (int i = 0; i < 12; ++i) q[8 + i] = 0xA5A5000000000000ull + (uint64_t)i * 0x0101010101ull + seed;
}

static int Thunk(uint8_t* mem, const void* target, int n, volatile uint64_t* ctr, uint32_t firstN, uint32_t mask, const void* hook) {
    const size_t sz = fnprobe::BuildThunk(mem, (const uint8_t*)target, n, ctr, firstN, mask, hook);
    FlushInstructionCache(GetCurrentProcess(), mem, sz);
    return (int)sz;
}

static int RunThunkTests() {
    uint8_t* mem = (uint8_t*)VirtualAlloc(nullptr, 65536, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    struct T {
        const char* name;
        const void* fn;
        int n;
        bool raxIsRsp;
        bool flagsKept;
    } ts[] = {{"A mov [rsp+8],rbx", (const void*)&TargetA, 5, false, true},
              {"B push rbp,rbx,rsi,rdi", (const void*)&TargetB, 5, false, true},
              {"C mov rax,rsp; mov [rax+8],rbx", (const void*)&TargetC, 7, true, true},
              {"D push rbx; sub rsp,40h", (const void*)&TargetD, 6, false, false}};
    for (auto& t : ts) {
        CHECK(fnprobe::PrologueLen((const uint8_t*)t.fn) == t.n, "%s: PrologueLen %d != %d", t.name, fnprobe::PrologueLen((const uint8_t*)t.fn), t.n);
        for (uint32_t mask : {0u, 3u}) {
            alignas(64) static volatile uint64_t ctr;
            ctr = 0;
            g_recs.clear();
            const uint32_t firstN = 3;
            const int sz = Thunk(mem, t.fn, t.n, &ctr, firstN, mask, (const void*)&TestHook);
            CHECK(sz > 0 && sz <= (int)fnprobe::kThunkMax, "%s: thunk size %d", t.name, sz);
            const int calls = 20;
            for (int c = 0; c < calls; ++c) {
                // alternate flag patterns (CF/ZF/SF/OF/PF/AF set or clear; IF and bit 1 always on)
                const uint64_t fl = (c & 1) ? 0x0202 | 0x08D5 : 0x0202;
                FillIn(0x1000ull * (uint64_t)c, fl);
                RunProbe(mem, p_in);
                const uint64_t* in = (const uint64_t*)p_in;
                const uint64_t* out = (const uint64_t*)p_out;
                const uint64_t rsp0 = p_entry_rsp;
                CHECK(out[0] == (t.raxIsRsp ? rsp0 : in[0]), "%s mask %u call %d: rax %llX", t.name, mask, c, (unsigned long long)out[0]);
                for (int r = 1; r < 7; ++r)
                    CHECK(out[r] == in[r], "%s mask %u call %d: reg %d %llX != %llX", t.name, mask, c, r, (unsigned long long)out[r], (unsigned long long)in[r]);
                if (t.flagsKept) CHECK((out[7] & 0x8D5) == (in[7] & 0x8D5), "%s mask %u call %d: flags %llX != %llX", t.name, mask, c, (unsigned long long)out[7], (unsigned long long)in[7]);
                CHECK(memcmp(p_out + 0x40, p_in + 0x40, 0x60) == 0, "%s mask %u call %d: xmm0-5 changed", t.name, mask, c);
                const uint64_t want = rsp0 - (t.fn == (const void*)&TargetB ? 32 : t.fn == (const void*)&TargetD ? 0x48 : 0);
                CHECK(out[20] == want, "%s mask %u call %d: body rsp %llX != %llX", t.name, mask, c, (unsigned long long)out[20], (unsigned long long)want);
            }
            CHECK(ctr == (uint64_t)calls, "%s mask %u: counter %llu != %d", t.name, mask, (unsigned long long)ctr, calls);
            // hook calls: old < firstN, then every old with (old & mask) == 0
            std::vector<uint64_t> wantOld;
            for (uint64_t o = 0; o < (uint64_t)calls; ++o)
                if (o < firstN || (o & mask) == 0) wantOld.push_back(o);
            CHECK(g_recs.size() == wantOld.size(), "%s mask %u: %zu hook calls, want %zu", t.name, mask, g_recs.size(), wantOld.size());
            for (size_t i = 0; i < g_recs.size() && i < wantOld.size(); ++i) {
                const HookRec& h = g_recs[i];
                CHECK(h.old == wantOld[i], "%s mask %u: hook %zu old %llu != %llu", t.name, mask, i, (unsigned long long)h.old, (unsigned long long)wantOld[i]);
                const uint64_t c = h.old;
                const uint64_t seed = 0x1000ull * c;
                for (int r = 0; r < 4; ++r) {
                    const int reg = r == 0 ? 1 : r == 1 ? 2 : r == 2 ? 3 : 4;  // rcx rdx r8 r9
                    const uint64_t v = 0x1111111111111111ull * (uint64_t)(reg + 1) ^ seed;
                    CHECK(h.a[r] == v, "%s mask %u: hook %zu arg %d %llX != %llX", t.name, mask, i, r, (unsigned long long)h.a[r], (unsigned long long)v);
                }
                CHECK(h.ret != 0, "%s: no return address", t.name);
            }
            if (!g_recs.empty()) {
                // sp handed to the hook is the entry rsp; [sp] is RunProbe's return address (same for every call)
                CHECK(g_recs[0].ret == g_recs.back().ret, "%s: return address unstable", t.name);
            }
        }
    }
    // entry rsp handed to the hook == the rsp the target saw at entry
    {
        alignas(64) static volatile uint64_t ctr;
        ctr = 0;
        g_recs.clear();
        Thunk(mem, (const void*)&TargetA, 5, &ctr, 1, 0xFFFFFFFF, (const void*)&TestHook);
        FillIn(0, 0x202);
        RunProbe(mem, p_in);
        CHECK(g_recs.size() == 1 && g_recs[0].sp == p_entry_rsp, "sp arg %llX != entry rsp %llX", g_recs.empty() ? 0ull : (unsigned long long)g_recs[0].sp,
              (unsigned long long)p_entry_rsp);
    }
    // 4 threads x 200k calls through one thunk: exact count (lock xadd), hook sampled 1 in 16
    {
        alignas(64) static volatile uint64_t ctr;
        ctr = 0;
        g_record = false;
        Thunk(mem, (const void*)&TargetA, 5, &ctr, 8, 15, (const void*)&NopHook);
        typedef void (*Fn)();
        Fn f = (Fn)(void*)mem;
        std::vector<std::thread> th;
        for (int k = 0; k < 4; ++k)
            th.emplace_back([f] {
                for (int i = 0; i < 200000; ++i) f();
            });
        LARGE_INTEGER q0, q1, fq;
        QueryPerformanceFrequency(&fq);
        QueryPerformanceCounter(&q0);
        for (auto& x : th) x.join();
        QueryPerformanceCounter(&q1);
        CHECK(ctr == 800000, "threads: counter %llu != 800000", (unsigned long long)ctr);
        // single-thread cost of the fast path
        ctr = 1000;
        QueryPerformanceCounter(&q0);
        for (int i = 0; i < 1000000; ++i) f();
        QueryPerformanceCounter(&q1);
        const double ns = (double)(q1.QuadPart - q0.QuadPart) * 1e9 / (double)fq.QuadPart / 1e6;
        printf("info: thunk + TargetA, 1 thread: %.1f ns per call (incl. 1 in 16 hook calls)\n", ns);
        g_record = true;
    }
    // decoder negatives
    {
        const uint8_t rip[] = {0x48, 0x8B, 0x05, 1, 2, 3, 4, 0x90};           // mov rax,[rip+x]
        const uint8_t jmp[] = {0xE9, 1, 2, 3, 4};                             // jmp rel32
        const uint8_t sub[] = {0x48, 0x83, 0xEC, 0x28, 0xE8, 0, 0, 0, 0};       // sub rsp,28h; call -> 4 B then call
        const uint8_t ok1[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57};           // mov [rsp+8],rbx
        const uint8_t ok2[] = {0x40, 0x53, 0x48, 0x81, 0xEC, 0x60, 0x01, 0x00, 0x00};  // push rbx; sub rsp,160h
        const uint8_t mov3[] = {0x4C, 0x8B, 0xDC, 0x49, 0x89, 0x5B, 0x08};      // mov r11,rsp; mov [r11+8],rbx
        // 9AFDF0: the word immediate must not swallow the following ret/padding.
        const uint8_t wordStore[] = {0x48, 0x83, 0x02, 0x04, 0x66, 0xC7, 0x81, 0x43, 0x05, 0, 0, 1, 0, 0xC3, 0xCC};
        const uint8_t wordAlu[] = {0x66, 0x81, 0xEC, 0x60, 0x01};
        const uint8_t wordMov[] = {0x66, 0xB8, 0x01, 0x02};
        const uint8_t rexAlu[] = {0x66, 0x48, 0x81, 0xEC, 0x60, 0x01, 0, 0};
        const uint8_t rexMov[] = {0x66, 0x48, 0xB8, 1, 2, 3, 4, 5, 6, 7, 8};
        CHECK(fnprobe::PrologueLen(wordStore) == 13, "word store includes ret/padding");
        CHECK(fnprobe::InsnLen(wordAlu) == 5, "word ALU immediate width");
        CHECK(fnprobe::InsnLen(wordMov) == 4, "word MOV immediate width");
        CHECK(fnprobe::InsnLen(rexAlu) == 8, "REX.W must override 66 for ALU");
        CHECK(fnprobe::InsnLen(rexMov) == 11, "REX.W must override 66 for MOV");
        CHECK(fnprobe::PrologueLen(rip) == -1, "decoder accepted rip-relative");
        CHECK(fnprobe::PrologueLen(jmp) == -1, "decoder accepted jmp");
        CHECK(fnprobe::PrologueLen(sub) == -1, "decoder accepted a call in the first 5 bytes");
        CHECK(fnprobe::PrologueLen(ok1) == 5, "ok1 %d", fnprobe::PrologueLen(ok1));
        CHECK(fnprobe::PrologueLen(ok2) == 9, "ok2 %d", fnprobe::PrologueLen(ok2));
        CHECK(fnprobe::PrologueLen(mov3) == 7, "mov3 %d", fnprobe::PrologueLen(mov3));
        CHECK(fnprobe::IsHashish(0x91A41AD2) && fnprobe::IsHashish(0x37A3E452) && fnprobe::IsHashish(0x5C24DB04), "known game hashes not hash-like");
        CHECK(!fnprobe::IsHashish(0x3F800000) && !fnprobe::IsHashish(100) && !fnprobe::IsHashish(0x00010000), "1.0f / small ints flagged");
    }
    return g_fail;
}

static int Decode(const char* dump, const char* list) {
    FILE* f = fopen(dump, "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> img((size_t)sz + 64);
    fread(img.data(), 1, (size_t)sz, f);
    fclose(f);
    FILE* l = fopen(list, "r");
    if (!l) return 2;
    unsigned long long rva;
    while (fscanf(l, "%llx", &rva) == 1)
        if (rva + 32 < (unsigned long long)sz) printf("%llX %d\n", rva, fnprobe::PrologueLen(img.data() + rva));
    fclose(l);
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 4 && !strcmp(argv[1], "decode")) return Decode(argv[2], argv[3]);
    const int f = RunThunkTests();
    if (f) {
        printf("%d FAILED\n", f);
        return 1;
    }
    printf("fnprobe thunk: ALL PASSED\n");
    return 0;
}
