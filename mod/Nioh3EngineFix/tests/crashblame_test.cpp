// Offline check of crashblame (the "who crashed" section; human #94391/#94396). Run by run_crashblame_test.py, which
// builds blamedll.cpp into copies next to this exe: Nioh3Fake.asi (ours), Nioh3Gone.asi (ours, unloaded before a crash),
// winmm.dll (same name as a Windows DLL = proxy), other.dll (game-folder DLL). Real exceptions, no game.
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

static int Filter(EXCEPTION_POINTERS* ep) {
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
    HMODULE gone = load(L"Nioh3Gone.asi");
    CHECK(g_fake && g_proxy && g_other && gone);
    if (!g_fake || !g_proxy || !g_other || !gone) return 1;
    g_goneBase = (uintptr_t)gone;
    FreeLibrary(gone);

    crashblame::Owner o;
    crashblame::Classify(GetProcAddress(g_fake, "Crash"), o);
    CHECK(o.kind == crashblame::kOurs && !strcmp(o.name, "Nioh3Fake.asi"));
    crashblame::Classify(GetProcAddress(g_proxy, "Crash"), o);
    CHECK(o.kind == crashblame::kProxy);
    crashblame::Classify(GetProcAddress(g_other, "Crash"), o);
    CHECK(o.kind == crashblame::kGameFolder);
    CHECK(!strcmp(o.company, "Blame Test Co"));
    crashblame::Classify((void*)&GameCrash, o);
    CHECK(o.kind == crashblame::kGame);
    crashblame::Classify((void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"), o);
    CHECK(o.kind == crashblame::kWindows);
    crashblame::Classify((void*)(g_goneBase + 0x1000), o);
    CHECK(o.kind == crashblame::kUnloaded && !strcmp(o.name, "Nioh3Gone.asi"));

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
    CHECK(strstr(g_text, "OUR MOD: ran code of Nioh3Gone.asi after it was unloaded; called from crashblame_test.exe"));
    CHECK(strstr(g_text, "\r\n  Nioh3Gone.asi  0x"));
    bool unl = false;
    for (int i = 0; i < g_ns; ++i) unl |= strstr(g_short[i], "WHO unloaded: Nioh3Gone.asi") != nullptr;
    CHECK(unl);
    CHECK(strstr(g_text, "winmm.dll") && strstr(g_text, "proxy DLL"));
    for (int i = 0; i < g_ns; ++i) CHECK(strlen(g_short[i]) <= 118);
    printf("%s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
