#pragma once
#include <cstddef>
#include <cstdint>
// Native load tip page ("[DECIDE] Continue", UI slot 0x2B), passive read for the BossRematch transition cover.
// v2.02 RVAs; the caller validates the build like transition_native_read.h does. Static facts (Nioh3.dump.exe):
// - page = [[base+45C84A8]+158h]: TipPageSetLoadDone 1D010E4 loads it the same way (1D010EE / 1D010F8).
// - +71Bh load done: written = 1 only by 1D010E4 (and its unreferenced twin 1D0112C), from mission sequencer state
//   0Bh 223BE08 after LoadIsLoadEnd passed (notes/auto-continue-load.md).
// - +717h prompt shown: TipPageLoadDoneTick 1CECB24 clears it while +71Bh == 0 and polls the confirm check 1CECAE4
//   every frame once it is set.
// The ctor 1CD1DA8 stores two vtables (4015EC8 then 40157D8), so the vtable is NOT used as an identity check; the
// dedicated slot +158h is.
namespace tip_page_state {
using Read = bool (*)(void*, uint64_t, void*, size_t);
constexpr uint64_t kUiManager = 0x45C84A8, kSlot2B = 0x158, kPrompt = 0x717, kLoadDone = 0x71B;
struct State { bool known = false, open = false, prompt = false, loadDone = false; };
inline State ReadTip(uint64_t base, Read read, void* ctx) {
    State s;
    uint64_t ui = 0, page = 0;
    uint8_t b[kLoadDone - kPrompt + 1] = {};
    if (!read || !base || !read(ctx, base + kUiManager, &ui, sizeof(ui))) return s;
    if (ui && !read(ctx, ui + kSlot2B, &page, sizeof(page))) return s;
    if (!page) { s.known = true; return s; }  // no tip page open
    if (!read(ctx, page + kPrompt, b, sizeof(b))) return s;
    s.known = s.open = true;
    s.prompt = b[0] != 0;
    s.loadDone = b[kLoadDone - kPrompt] != 0;
    return s;
}
// The native load finished and the page is waiting for the player's confirm. Nothing that the mission script creates
// after the 58A clear (boss intro load, BossBattleMode, the boss actor) can appear before this page is answered.
inline bool AwaitingConfirm(const State& s) { return s.known && s.open && s.loadDone && s.prompt; }
}  // namespace tip_page_state
