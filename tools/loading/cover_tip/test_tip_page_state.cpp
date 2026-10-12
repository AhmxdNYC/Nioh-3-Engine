// Offline test of tip_page_state.h against a fake address space. Build: cl /nologo /std:c++17 /W4 /EHsc test_tip_page_state.cpp
#include <cstdio>
#include <cstring>
#include <map>
#include "tip_page_state.h"

namespace {
std::map<uint64_t, uint8_t> g_mem;
bool Fake(void*, uint64_t a, void* out, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        auto it = g_mem.find(a + i);
        if (it == g_mem.end()) return false;
        static_cast<uint8_t*>(out)[i] = it->second;
    }
    return true;
}
void Put64(uint64_t a, uint64_t v) { for (int i = 0; i < 8; ++i) g_mem[a + i] = uint8_t(v >> (8 * i)); }
void Put8(uint64_t a, uint8_t v) { g_mem[a] = v; }
int g_fail = 0;
void Expect(bool c, const char* what) { if (!c) { std::printf("FAIL %s\n", what); ++g_fail; } }
}  // namespace

int main() {
    using namespace tip_page_state;
    const uint64_t base = 0x140000000, ui = 0x200000000, page = 0x300000000;
    State s = ReadTip(base, &Fake, nullptr);
    Expect(!s.known && !AwaitingConfirm(s), "unreadable global -> unknown");
    Put64(base + kUiManager, ui); Put64(ui + kSlot2B, 0);
    s = ReadTip(base, &Fake, nullptr);
    Expect(s.known && !s.open && !AwaitingConfirm(s), "no page -> known, closed");
    Put64(ui + kSlot2B, page);
    s = ReadTip(base, &Fake, nullptr);
    Expect(!s.known, "page bytes unreadable -> unknown");
    for (uint64_t o = kPrompt; o <= kLoadDone; ++o) Put8(page + o, 0);
    s = ReadTip(base, &Fake, nullptr);
    Expect(s.known && s.open && !s.loadDone && !AwaitingConfirm(s), "page open, load not done");
    Put8(page + kLoadDone, 1);
    s = ReadTip(base, &Fake, nullptr);
    Expect(s.loadDone && !s.prompt && !AwaitingConfirm(s), "load done, prompt not yet shown");
    Put8(page + kPrompt, 1);
    s = ReadTip(base, &Fake, nullptr);
    Expect(AwaitingConfirm(s), "load done + prompt -> awaiting confirm");
    Put8(page + 0x718, 1);  // neighbouring skip flags must not change the result
    Expect(AwaitingConfirm(ReadTip(base, &Fake, nullptr)), "neighbour bytes ignored");
    std::printf(g_fail ? "%d FAILED\n" : "ALL OK\n", g_fail);
    return g_fail != 0;
}
