// ptstart_harness.cpp - offline test of the PTS present-thread start guard (crash report #86). No game needed.
// Builds a copy of the game's Present thread proc (1A82298: the exact 39-byte entry, then the same wait / present
// loop) in executable memory, runs it with the start race forced (the wake event handle is still 0 when the thread
// starts, as when the renderer init resumes the thread before CreateEventW), and checks:
//   1. stock: the thread presents a null swapchain at once (the crash), caught by SEH in the harness;
//   2. patched: the thread waits, no Present until the event exists and fires, then exactly the queued Present runs,
//      the quit flag ends the loop, and WaitRounds() counts the held-off rounds;
//   3. patched, event already there (normal boot): no wait rounds, same behaviour;
//   4. Apply refuses code that is not in the v2.02 state; 5. BootTry stays pending on encrypted bytes.
// Build: cl /nologo /EHsc /std:c++17 /W4 /Febuild\ptstart_harness.exe /Fobuild\ tests\ptstart_harness.cpp src\ptstart.cpp
#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

#include "../src/ptstart.h"

static int g_fail = 0;
#define CHECK(c, m)                            \
    do {                                       \
        if (!(c)) {                            \
            std::printf("FAIL: %s\n", m);      \
            ++g_fail;                          \
        } else {                               \
            std::printf("ok:   %s\n", m);      \
        }                                      \
    } while (0)

static void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    std::printf("  log: ");
    std::vprintf(fmt, a);
    std::printf("\n");
    va_end(a);
}

// fake swapchain: vtable slot 8 (+40h) = Present(this, sync, flags)
static volatile LONG g_presents = 0;
static volatile LONG g_lastSync = -1, g_lastFlags = -1;
static HRESULT STDMETHODCALLTYPE FakePresent(void*, UINT sync, UINT flags) {
    g_lastSync = (LONG)sync;
    g_lastFlags = (LONG)flags;
    InterlockedIncrement(&g_presents);
    return S_OK;
}
static void* g_vtbl[16];
static void* g_sc[1] = {g_vtbl};

// Build the thread proc: the exact entry bytes of 1A82298, then the stock loop (absolute call through rax for the wait)
static uint8_t* BuildProc(uint8_t* mem) {
    std::vector<uint8_t> c;
    auto b = [&](std::initializer_list<uint8_t> v) { c.insert(c.end(), v); };
    auto q = [&](uint64_t v) {
        for (int i = 0; i < 8; ++i) c.push_back((uint8_t)(v >> (8 * i)));
    };
    // push rbx; sub rsp,20h; mov rbx,rcx; mov rcx,[rcx+7720h] (SITE); jmp WAIT
    b({0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0x48, 0x8B, 0x89, 0x20, 0x77, 0x00, 0x00, 0xEB, 0x00});
    const size_t jmpAt = c.size() - 1;
    // BODY: mov rax,[rip+slot]; test rax,rax; je +2; call rax; mov rcx,[rbx+7730h]
    const size_t body = c.size();
    b({0x48, 0x8B, 0x05, 0, 0, 0, 0});
    const size_t ripAt = c.size() - 4;
    b({0x48, 0x85, 0xC0, 0x74, 0x02, 0xFF, 0xD0, 0x48, 0x8B, 0x8B, 0x30, 0x77, 0x00, 0x00});
    // mov r8d,[rbx+773Ch]; mov edx,[rbx+7738h]; mov rax,[rcx] (= 1A822CC); call [rax+40h]; mov byte [rbx+7728h],0
    b({0x44, 0x8B, 0x83, 0x3C, 0x77, 0x00, 0x00, 0x8B, 0x93, 0x38, 0x77, 0x00, 0x00, 0x48, 0x8B, 0x01, 0xFF, 0x50, 0x40});
    b({0xC6, 0x83, 0x28, 0x77, 0x00, 0x00, 0x00});
    b({0x48, 0x8B, 0x8B, 0x20, 0x77, 0x00, 0x00});  // mov rcx,[rbx+7720h]
    // WAIT sits at +64h as 1A822FC does (the entry's `jmp` is EB 52, part of the checked context); nops up to it
    while (c.size() < 0x64) c.push_back(0x90);
    // WAIT: or edx,-1; mov rax,&WaitForSingleObject; call rax; cmp byte [rbx+7729h],0; je BODY
    const size_t wait = c.size();
    b({0x83, 0xCA, 0xFF, 0x48, 0xB8});
    q((uint64_t)(uintptr_t)&WaitForSingleObject);
    b({0xFF, 0xD0, 0x80, 0xBB, 0x29, 0x77, 0x00, 0x00, 0x00, 0x0F, 0x84, 0, 0, 0, 0});
    const size_t jeAt = c.size() - 4;
    // xor eax,eax; add rsp,20h; pop rbx; ret
    b({0x33, 0xC0, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3});
    const size_t slot = (c.size() + 7) & ~size_t(7);
    c.resize(slot + 8, 0);  // optional callback slot = 0 (none)
    c[jmpAt] = (uint8_t)(wait - (jmpAt + 1));
    const int32_t rip = (int32_t)(slot - (ripAt + 4));
    std::memcpy(&c[ripAt], &rip, 4);
    const int32_t je = (int32_t)body - (int32_t)(jeAt + 4);
    std::memcpy(&c[jeAt], &je, 4);
    std::memcpy(mem, c.data(), c.size());
    FlushInstructionCache(GetCurrentProcess(), mem, c.size());
    return mem;
}

struct Dev {
    uint8_t* p;
    HANDLE& ev() { return *reinterpret_cast<HANDLE*>(p + 0x7720); }
    volatile uint8_t& pending() { return *(volatile uint8_t*)(p + 0x7728); }
    volatile uint8_t& quit() { return *(volatile uint8_t*)(p + 0x7729); }
    void*& sc() { return *reinterpret_cast<void**>(p + 0x7730); }
    int& sync() { return *reinterpret_cast<int*>(p + 0x7738); }
    int& flags() { return *reinterpret_cast<int*>(p + 0x773C); }
};

struct Run {
    uint8_t* proc;
    uint8_t* dev;
    volatile LONG crashed;
};
// The generated proc has no unwind info, so SEH cannot unwind through it: a vectored handler catches the access
// violation inside the generated code, marks the run and ends that thread.
static uint8_t* g_codeLo = nullptr;
static uint8_t* g_codeHi = nullptr;
static Run* volatile g_cur = nullptr;
static void CrashExit() { ExitThread(0xC5); }
static LONG CALLBACK Veh(EXCEPTION_POINTERS* e) {
    const uint8_t* ip = (const uint8_t*)e->ContextRecord->Rip;
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || ip < g_codeLo || ip >= g_codeHi) return EXCEPTION_CONTINUE_SEARCH;
    if (g_cur) g_cur->crashed = 1;
    e->ContextRecord->Rsp = (e->ContextRecord->Rsp & ~0xFull) - 8;
    e->ContextRecord->Rip = (DWORD64)(uintptr_t)&CrashExit;
    return EXCEPTION_CONTINUE_EXECUTION;
}
static DWORD WINAPI Thunk(LPVOID a) {
    Run* r = (Run*)a;
    g_cur = r;
    reinterpret_cast<DWORD(__fastcall*)(void*)>(r->proc)(r->dev);
    return 0;
}

// queue one Present the way the game's main thread does (3D6530..3D6552), then wait for the thread to clear pending
static bool QueuePresent(Dev& d, int sync, int flags) {
    d.sc() = g_sc;
    d.sync() = sync;
    d.flags() = flags;
    d.pending() = 1;
    SetEvent(d.ev());
    for (int i = 0; i < 2000 && d.pending(); ++i) Sleep(1);
    return d.pending() == 0;
}

static void Stop(Dev& d, HANDLE th) {
    d.quit() = 1;
    if (d.ev()) SetEvent(d.ev());
    WaitForSingleObject(th, 3000);
}

int main() {
    g_vtbl[8] = (void*)&FakePresent;
    setvbuf(stdout, nullptr, _IONBF, 0);
    uint8_t* code = (uint8_t*)VirtualAlloc(nullptr, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    g_codeLo = code;
    g_codeHi = code + 0x10000;
    AddVectoredExceptionHandler(1, Veh);
    std::vector<uint8_t> devMem(0x8000, 0);
    Dev d{devMem.data()};

    // 1. stock + race: crash on the null swapchain (as Nioh3+1A822CC)
    {
        uint8_t* proc = BuildProc(code);
        Run r{proc, d.p, 0};
        HANDLE th = CreateThread(nullptr, 0, Thunk, &r, 0, nullptr);
        WaitForSingleObject(th, 3000);
        CloseHandle(th);
        CHECK(r.crashed == 1, "stock proc, event not created yet: access violation on the null swapchain (the #86 crash)");
        CHECK(g_presents == 0, "stock: no Present reached");
    }

    // 4. Apply refuses a changed entry
    {
        uint8_t* proc = BuildProc(code + 0x1000);
        proc[3] ^= 0xFF;
        CHECK(!ptstart::Apply(proc, &Log), "Apply refuses an entry that is not in the v2.02 state");
        proc[3] ^= 0xFF;
        CHECK(proc[9] == 0x48 && proc[10] == 0x8B && proc[11] == 0x89, "refused entry left untouched");
        CHECK(!ptstart::Applied(), "Applied() false after a refusal");
    }

    // 2. patched + race: waits, then presents exactly the queued frame
    {
        std::memset(d.p, 0, 0x8000);
        uint8_t* proc = BuildProc(code + 0x2000);
        CHECK(ptstart::Apply(proc, &Log), "Apply writes the guard on the v2.02 entry");
        CHECK(ptstart::Applied(), "Applied() true");
        CHECK(proc[9] == 0xE8 && proc[14] == 0x66 && proc[15] == 0x90, "site is call stub + 2-byte nop");
        CHECK(proc[16] == 0xEB, "the jmp after the site is untouched");
        Run r{proc, d.p, 0};
        HANDLE th = CreateThread(nullptr, 0, Thunk, &r, 0, nullptr);
        Sleep(150);  // the renderer init is slow to create the event
        CHECK(r.crashed == 0, "patched: no crash while the event handle is still 0");
        CHECK(g_presents == 0, "patched: no Present before the event exists");
        CHECK(WaitForSingleObject(th, 0) == WAIT_TIMEOUT, "patched: thread still alive, waiting");
        const long w = ptstart::WaitRounds();
        std::printf("  wait rounds after 150 ms: %ld\n", w);
        CHECK(w >= 1, "patched: WaitRounds counts the held-off rounds (Sleep(1) granularity varies)");
        d.ev() = CreateEventW(nullptr, FALSE, FALSE, nullptr);  // 2EE9B44 / 2EE9B4A
        Sleep(50);
        CHECK(g_presents == 0, "patched: event created but not signalled -> still no Present");
        CHECK(QueuePresent(d, 1, 0x200), "patched: queued Present ran and cleared pending");
        CHECK(g_presents == 1 && g_lastSync == 1 && g_lastFlags == 0x200, "patched: exactly one Present with the queued sync/flags");
        CHECK(QueuePresent(d, 0, 0) && g_presents == 2, "patched: second Present also runs");
        Stop(d, th);
        CHECK(WaitForSingleObject(th, 0) == WAIT_OBJECT_0 && r.crashed == 0, "patched: quit flag ends the thread cleanly");
        CloseHandle(th);
        CloseHandle(d.ev());
    }

    // 3. patched, normal order (event first): no wait rounds
    {
        std::memset(d.p, 0, 0x8000);
        g_presents = 0;
        uint8_t* proc = BuildProc(code + 0x3000);
        CHECK(ptstart::Apply(proc, &Log), "Apply on a second copy");
        const long w0 = ptstart::WaitRounds();
        d.ev() = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        Run r{proc, d.p, 0};
        HANDLE th = CreateThread(nullptr, 0, Thunk, &r, 0, nullptr);
        Sleep(30);
        CHECK(QueuePresent(d, 2, 0) && g_presents == 1 && g_lastSync == 2, "normal order: Present runs");
        CHECK(ptstart::WaitRounds() == w0, "normal order: no wait rounds (one compare, no Sleep)");
        Stop(d, th);
        CHECK(WaitForSingleObject(th, 0) == WAIT_OBJECT_0 && r.crashed == 0, "normal order: clean exit");
        CloseHandle(th);
        CloseHandle(d.ev());
    }

    // 5. BootTry: still-encrypted bytes keep it pending (no write)
    {
        std::vector<uint8_t> img(ptstart::kSiteRva + 64, 0xCC);
        CHECK(ptstart::BootTry((uintptr_t)img.data(), &Log), "BootTry stays pending while the site is not decrypted");
        CHECK(img[ptstart::kSiteRva] == 0xCC, "BootTry wrote nothing");
    }

    // 6. the real v2.02 bytes (re/Nioh3.dump.exe, file offset == RVA; path in NIOH3_DUMP or ..\..\re\Nioh3.dump.exe):
    //    the checked context must match the shipped game exactly
    {
        char path[MAX_PATH] = "..\\..\\re\\Nioh3.dump.exe";
        GetEnvironmentVariableA("NIOH3_DUMP", path, MAX_PATH);
        FILE* f = nullptr;
        if (fopen_s(&f, path, "rb") == 0 && f) {
            uint8_t* real = code + 0x4000;
            std::fseek(f, (long)ptstart::kCtxRva, SEEK_SET);
            const size_t got = std::fread(real, 1, 0x80, f);
            std::fclose(f);
            CHECK(got == 0x80, "read 1A82298 from the v2.02 dump");
            CHECK(ptstart::Apply(real, &Log), "Apply accepts the real v2.02 present thread entry");
            CHECK(real[ptstart::kCtxOffset] == 0xE8, "real entry: site rewritten");
        } else {
            std::printf("  (v2.02 dump not found at %s - real-bytes check skipped)\n", path);
        }
    }

    std::printf(g_fail ? "PTS harness: %d FAILED\n" : "PTS harness: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
