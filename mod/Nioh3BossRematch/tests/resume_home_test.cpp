// Offline decisions for the resume hold (#75648). No game, no save bytes.
#include "../src/rematch_core.h"
#include <cstdio>

static int g_n = 0, g_fail = 0;
static void eq(bool cond, const char* name) {
    ++g_n;
    if (!cond) {
        std::printf("FAIL %s\n", name);
        ++g_fail;
    }
}

int main() {
    using rcore::ResumeBlocksWrite;
    using rcore::ResumeHold;
    // Picture Scroll: the resume mission is the trip, so the disk write waits.
    eq(ResumeBlocksWrite(true, 0xCC96, 0x0F7F, 0xCC96, true, false), "scroll blocks");
    eq(ResumeHold(true, 0xCC96, 0x0F7F, 0xCC96, true, false, false, false) == 1, "scroll hold");
    // Return has left the trip and a write was held: ask for the game's autosave.
    eq(!ResumeBlocksWrite(true, 0xCC96, 0x0F7F, 0x0F7F, true, false), "home does not block");
    eq(ResumeHold(true, 0xCC96, 0x0F7F, 0x0F7F, true, false, false, true) == 2, "home after the trip");
    // Still loading, or the mission word cannot be read: keep holding.
    eq(ResumeHold(true, 0xCC96, 0x0F7F, 0x0F7F, true, true, false, true) == 1, "hold through the load");
    eq(ResumeBlocksWrite(true, 0xCC96, 0x0F7F, 0, false, false), "unreadable blocks");
    // The request has not landed yet. Do not force a save, and do not block the home file.
    eq(ResumeHold(true, 0xCC96, 0x0F7F, 0x0F7F, true, false, false, false) == 0, "not entered");
    eq(!ResumeBlocksWrite(false, 0xCC96, 0x0F7F, 0xCC96, true, false), "disarmed");
    // Fight now / lesson that loads its own mission. Same two fields, no mission list.
    eq(ResumeBlocksWrite(true, 0x0F7F, 0x1CBC, 0x0F7F, true, false), "fight mission blocks");
    eq(ResumeHold(true, 0x0F7F, 0x1CBC, 0x1CBC, true, false, false, true) == 2, "fight returned");
    // In-place arena: the mission id does not change, so only the session flag holds the write.
    eq(ResumeBlocksWrite(true, 0x1CBC, 0x1CBC, 0x1CBC, true, true), "arena session blocks");
    eq(!ResumeBlocksWrite(true, 0x1CBC, 0x1CBC, 0x1CBC, true, false), "arena session over");
    eq(ResumeHold(true, 0x1CBC, 0x1CBC, 0x1CBC, true, false, false, true) == 2, "arena home save");
    std::printf("%d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
