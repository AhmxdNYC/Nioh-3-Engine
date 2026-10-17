// Real merged menu dispatch with synthetic Training exports; no game or ASI load.
#include <windows.h>
#include <cstring>
static int tuneCalls, idleCalls, moveCalls;
static int TuneGet(int which) { return which ? 1000 : 6000; }
static int TuneSet(int, int, int) { ++tuneCalls; return 1; }
static int IdleGet() { return 0; }
static int IdleSet(int) { ++idleCalls; return 1; }
static unsigned BossGet() { return 16102; }
static int MoveGet(unsigned, unsigned) { return 1; }
static int MoveSet(unsigned, unsigned, int, int) { ++moveCalls; return 1; }
static HMODULE TestModule(const wchar_t*) { return (HMODULE)1; }
static FARPROC TestProc(HMODULE, const char* name) {
#define EXPORT(n, f) if (!strcmp(name, "Nioh3Training_" n)) return (FARPROC)f
    EXPORT("GetTuning", TuneGet); EXPORT("SetTuning", TuneSet);
    EXPORT("GetIdleHold", IdleGet); EXPORT("SetIdleHold", IdleSet);
    EXPORT("GetMoveBoss", BossGet); EXPORT("GetMoveAllowed", MoveGet); EXPORT("SetMoveAllowed", MoveSet);
#undef EXPORT
    return nullptr;
}
#define GetModuleHandleW TestModule
#define GetProcAddress TestProc
#define main legacy_ui_main
#include "ui_harness.cpp"
#undef main
#undef GetModuleHandleW
#undef GetProcAddress
int main() {
    using namespace addon::rematch;
    int items[kMaxRows + 4];
    UiOpen();
    int n = Items(items, kMaxRows + 4), moves = -1;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) CHECK(items[i] != items[j]);
        if (items[i] == kItMoves) moves = i;
        if (items[i] == kItFinT || items[i] == kItHp0W) { g_sel = i; UiChange(1); }
    }
    CHECK(tuneCalls == 2 && !idleCalls && !moveCalls && moves >= 0);
    g_sel = moves; CHECK(!UiAct() && g_uiPages.page == uMoves);
    n = Items(items, kMaxRows + 4);
    CHECK(n == 16); // Yoritomo: 14 recorded entries + idle control + Back
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) CHECK(items[i] != items[j]);
        if (items[i] == kItIdleHold) { g_sel = i; UiChange(1); CHECK(!UiAct()); }
        if (IsMoveItem(items[i]) && !N3MoveProtected(kN3Moves[items[i]-kItMoveBase].id)) { g_sel = i; UiChange(-1); }
    }
    CHECK(tuneCalls == 2 && idleCalls == 2 && moveCalls == 12);
    puts("PASS: merged timing sliders, Training page, idle and 12 editable Yoritomo moves dispatch independently");
    return 0;
}
