// GH #133/#134: presenter_role::Classify - the DLSS-G pacer (no game frame, walk ends at the thread start) must be
// Foreign; the game's present thread (any Nioh3.exe frame) Game; an empty or truncated walk Unknown (old behaviour).
#include "../src/presenter_role.h"
#include <cstdio>

static int g_fail = 0;
static void Check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    using namespace presenter_role;
    const Range exe{0x140000000ull, 0x145300000ull};               // Nioh3.exe
    const Range start[2] = {{0x7ff93a000000ull, 0x7ff93a200000ull},  // ntdll
                            {0x7ff939000000ull, 0x7ff9390c0000ull}}; // kernel32
    const uintptr_t overlay = 0x7ff89fc40000ull + 0x80d0f, ours = 0x180000000ull + 0x926ef, reshade = 0x7ff8a0000000ull + 0x16cd73;
    const uintptr_t fg = 0x7ff880000000ull + 0x487ec, ucrt = 0x7ff938000000ull + 0x37b0;
    const uintptr_t k32 = 0x7ff939000000ull + 0x2e957, nt = 0x7ff93a000000ull + 0x427c;
    const uintptr_t game = 0x140000000ull + 0x3d211a;

    // #134 stack: overlay <- ours <- dxgi proxy <- 1B0_E658703 (DLSS-G) <- ucrtbase <- kernel32 <- ntdll
    const uintptr_t pacer[] = {ours, overlay, reshade, fg, fg, ucrt, k32, nt};
    Check(Classify(pacer, 8, exe, start, 2) == Foreign, "DLSS-G pacer stack (#134) is Foreign");
    // the game thread presenting through the same proxy
    const uintptr_t gameT[] = {ours, reshade, game, game, k32, nt};
    Check(Classify(gameT, 6, exe, start, 2) == Game, "game present thread is Game");
    // FG off under a wrapper whose frames come before the game: still Game (exe frame anywhere)
    const uintptr_t deepGame[] = {ours, reshade, overlay, fg, game};
    Check(Classify(deepGame, 5, exe, start, 2) == Game, "exe frame at the end of a truncated walk is Game");
    // walk stopped early (no unwind data) before reaching the thread start: Unknown, never Foreign
    const uintptr_t trunc[] = {ours, reshade, fg};
    Check(Classify(trunc, 3, exe, start, 2) == Unknown, "truncated walk without exe frame is Unknown");
    Check(Classify(pacer, 0, exe, start, 2) == Unknown, "empty walk is Unknown");
    Check(Classify(nullptr, 3, exe, start, 2) == Unknown, "null frames is Unknown");
    Check(Classify(pacer, 8, Range{}, start, 2) == Unknown, "unknown exe range is Unknown");
    Check(Classify(pacer, 8, exe, nullptr, 0) == Unknown, "no start modules is Unknown");
    const Range noStart[2] = {};
    Check(Classify(pacer, 8, exe, noStart, 2) == Unknown, "empty start ranges is Unknown");
    std::printf("%s: %d failure(s)\n", g_fail ? "FAILED" : "ALL PASS", g_fail);
    return g_fail ? 1 : 0;
}
