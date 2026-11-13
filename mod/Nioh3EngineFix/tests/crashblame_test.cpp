// Offline check of crashblame (the "who crashed" section; human #94391/#94396). Run by run_crashblame_test.py, which
// builds blamedll.cpp three ways and copies it next to this exe: ours (our CompanyName) = Nioh3Fake.asi; no version resource
// = Nioh3Training.asi (ours by shipped name), Nioh3CamKiProbe.asi (ours, unloaded before a crash), Nioh3NoVer.asi (not ours);
// foreign CompanyName = winmm.dll (Windows DLL name = proxy), other.dll, Nioh3Other.asi (another author's). Real exceptions.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "../src/crashblame.h"

static int fails = 0;
#define CHECK(x) do { if (!(x)) { ++fails; fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); } } while (0)

static char g_text[32768];
static char g_short[4][120];
static int g_ns = 0;

static bool g_save = false;
static EXCEPTION_RECORD g_er;
static CONTEXT g_ctx;

static int Filter(EXCEPTION_POINTERS* ep) {
    if (g_save) { g_er = *ep->ExceptionRecord; g_ctx = *ep->ContextRecord; }
    g_ns = crashblame::Analyze(ep->ExceptionRecord, ep->ContextRecord, g_text, sizeof g_text, g_short, 4);
    return EXCEPTION_EXECUTE_HANDLER;
}

using VoidFn = void (*)();
using CbFn = void (*)(VoidFn);

static __declspec(noinline) void GameCrash() { *(volatile int*)nullptr = 1; }
static void Run(const char* title, void (*body)()) {
    g_text[0] = 0;
    g_ns = 0;
    __try { body(); } __except (Filter(GetExceptionInformation())) {}
    printf("==== %s ====\n%s", title, g_text);
    for (int i = 0; i < g_ns; ++i) printf("  short: %s\n", g_short[i]);
}

static HMODULE g_fake, g_proxy, g_other;
static uintptr_t g_goneBase;
static void CaseGame() { GameCrash(); }
static void CaseOurs() { ((VoidFn)GetProcAddress(g_fake, "Crash"))(); }
static void CaseOursCallsGame() { ((CbFn)GetProcAddress(g_fake, "Call"))(&GameCrash); }
static void CaseOther() { ((VoidFn)GetProcAddress(g_other, "Crash"))(); }
static void CaseOtherViaWindows() { ((VoidFn)GetProcAddress(g_other, "CrashInWindows"))(); }
static void CaseUnloaded() { ((VoidFn)(g_goneBase + 0x1000))(); }

int wmain() {
    wchar_t dir[MAX_PATH];
    GetModuleFileNameW(nullptr, dir, MAX_PATH);
    *wcsrchr(dir, L'\\') = 0;
    auto load = [&](const wchar_t* n) { std::wstring p = std::wstring(dir) + L"\\" + n; return LoadLibraryW(p.c_str()); };
    g_fake = load(L"Nioh3Fake.asi");
    g_proxy = load(L"winmm.dll");
    g_other = load(L"other.dll");
    HMODULE gone = load(L"Nioh3CamKiProbe.asi");  // one of our shipped names, no version resource
    HMODULE plainOurs = load(L"Nioh3Training.asi"), noVer = load(L"Nioh3NoVer.asi"), foreignAsi = load(L"Nioh3Other.asi");
    CHECK(g_fake && g_proxy && g_other && gone && plainOurs && noVer && foreignAsi);
    if (!g_fake || !g_proxy || !g_other || !gone || !plainOurs || !noVer || !foreignAsi) return 1;
    g_goneBase = (uintptr_t)gone;
    FreeLibrary(gone);

    crashblame::Owner o;
    crashblame::Classify((void*)GetProcAddress(g_fake, "Crash"), o);
    CHECK(o.kind == crashblame::kOurs && !strcmp(o.name, "Nioh3Fake.asi") && !strcmp(o.company, crashblame::kOurCompany));
    // #94704: a Nioh3*.asi is ours only with our CompanyName, or with none under one of our shipped names
    crashblame::Classify((void*)GetProcAddress(plainOurs, "Crash"), o);
    CHECK(o.kind == crashblame::kOurs && !strcmp(o.name, "Nioh3Training.asi"));
    crashblame::Classify((void*)GetProcAddress(noVer, "Crash"), o);
    CHECK(o.kind == crashblame::kGameFolder);
    crashblame::Classify((void*)GetProcAddress(foreignAsi, "Crash"), o);
    CHECK(o.kind == crashblame::kGameFolder && !strcmp(o.company, "Blame Test Co"));
    crashblame::Classify((void*)GetProcAddress(foreignAsi, "Crash"), o, false);
    CHECK(o.kind == crashblame::kGameFolder);
    crashblame::Classify((void*)GetProcAddress(g_proxy, "Crash"), o);
    CHECK(o.kind == crashblame::kProxy);
    crashblame::Classify((void*)GetProcAddress(g_other, "Crash"), o);
    CHECK(o.kind == crashblame::kGameFolder);
    CHECK(!strcmp(o.company, "Blame Test Co"));
    crashblame::Classify((void*)&GameCrash, o);
    CHECK(o.kind == crashblame::kGame);
    crashblame::Classify((void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"), o);
    CHECK(o.kind == crashblame::kWindows);
    crashblame::Classify((void*)(g_goneBase + 0x1000), o);
    CHECK(o.kind == crashblame::kUnloaded && !strcmp(o.name, "Nioh3CamKiProbe.asi"));

    Run("game", CaseGame);
    CHECK(strstr(g_text, "verdict:   THE GAME"));
    Run("ours", CaseOurs);
    CHECK(strstr(g_text, "verdict:   OUR MOD: crashed inside Nioh3Fake.asi"));
    CHECK(g_ns >= 1 && strstr(g_short[0], "WHO: OUR MOD"));
    Run("ours calls game", CaseOursCallsGame);
    CHECK(strstr(g_text, "verdict:   OUR MOD (likely): game code"));
    CHECK(strstr(g_text, "our mods on the unwound stack: #01 Nioh3Fake.asi"));
    Run("other", CaseOther);
    CHECK(strstr(g_text, "verdict:   ANOTHER MOD/PROGRAM: crashed inside other.dll (Blame Test Co)"));
    Run("other via Windows", CaseOtherViaWindows);
    CHECK(strstr(g_text, "ANOTHER MOD/PROGRAM: inside ") && strstr(g_text, "called from other.dll"));
    Run("unloaded", CaseUnloaded);
    CHECK(strstr(g_text, "OUR MOD: ran code of Nioh3CamKiProbe.asi after it was unloaded; called from crashblame_test.exe"));
    CHECK(strstr(g_text, "\r\n  Nioh3CamKiProbe.asi  0x"));
    bool unl = false;
    for (int i = 0; i < g_ns; ++i) unl |= strstr(g_short[i], "WHO unloaded: Nioh3CamKiProbe.asi") != nullptr;
    CHECK(unl);
    CHECK(strstr(g_text, "winmm.dll") && strstr(g_text, "proxy DLL"));
    for (int i = 0; i < g_ns; ++i) CHECK(strlen(g_short[i]) <= 118);

    // #94704: Analyze must not touch the process heap (a crash can hold its lock). Another thread holds HeapLock while a
    // fresh thread analyses a saved "other.dll" crash; it must finish (it would wait forever on the heap lock otherwise).
    g_save = true;
    Run("other (saved)", CaseOther);
    g_save = false;
    // Both threads are fully started (thread start-up itself allocates) before the heap is locked.
    static HANDLE s_go, s_ready, s_locked, s_release, s_done;
    s_go = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    s_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    s_locked = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    s_release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    s_done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE th = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        SetEvent(s_ready);
        WaitForSingleObject(s_go, INFINITE);
        g_ns = crashblame::Analyze(&g_er, &g_ctx, g_text, sizeof g_text, g_short, 4);
        SetEvent(s_done);  // the thread's own exit frees CRT data (heap): wait on this, not on the thread
        return 0;
    }, nullptr, 0, nullptr);
    CHECK(th && WaitForSingleObject(s_ready, 5000) == WAIT_OBJECT_0);
    HANDLE holder = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        HeapLock(GetProcessHeap());
        SetEvent(s_locked);
        WaitForSingleObject(s_release, INFINITE);
        HeapUnlock(GetProcessHeap());
        return 0;
    }, nullptr, 0, nullptr);
    CHECK(holder && WaitForSingleObject(s_locked, 5000) == WAIT_OBJECT_0);
    const HANDLE release = s_release;
    g_text[0] = 0;
    SetEvent(s_go);
    const DWORD w = WaitForSingleObject(s_done, 10000);
    CONTEXT stuck{};
    if (w != WAIT_OBJECT_0) {  // where it waits: stack of the blocked thread (walked after the heap is released)
        SuspendThread(th);
        stuck.ContextFlags = CONTEXT_FULL;
        GetThreadContext(th, &stuck);
    }
    SetEvent(release);
    WaitForSingleObject(holder, INFINITE);
    if (w != WAIT_OBJECT_0) {  // raw frames, no lock-taking calls while the thread is suspended
        DWORD64 rips[40];
        int n = 0;
        CONTEXT c = stuck;
        for (; n < 40 && c.Rip; ++n) {
            rips[n] = c.Rip;
            DWORD64 ib = 0;
            RUNTIME_FUNCTION* rf = RtlLookupFunctionEntry(c.Rip, &ib, nullptr);
            if (!rf) { c.Rip = *(DWORD64*)c.Rsp; c.Rsp += 8; }
            else { void* hd; DWORD64 ef; RtlVirtualUnwind(UNW_FLAG_NHANDLER, ib, c.Rip, rf, &c, &hd, &ef, nullptr); }
        }
        ResumeThread(th);
        WaitForSingleObject(th, INFINITE);
        for (int i = 0; i < n; ++i) {
            HMODULE m = nullptr;
            char nm[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)rips[i], &m))
                GetModuleFileNameA(m, nm, MAX_PATH);
            printf("  stuck #%02d %s+0x%llx\n", i, strrchr(nm, '\\') ? strrchr(nm, '\\') + 1 : nm, (unsigned long long)(rips[i] - (DWORD64)m));
        }
    }
    CHECK(w == WAIT_OBJECT_0);  // finished while the heap was locked
    WaitForSingleObject(th, INFINITE);
    WaitForSingleObject(holder, INFINITE);
    printf("==== heap locked: %s ====\n", w == WAIT_OBJECT_0 ? "finished" : "BLOCKED");
    CHECK(strstr(g_text, "verdict:   ANOTHER MOD/PROGRAM: crashed inside other.dll (Blame Test Co)"));
    CHECK(strstr(g_text, "Nioh3Fake.asi") && strstr(g_text, "nioh3-engine mods"));
    printf("%s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
