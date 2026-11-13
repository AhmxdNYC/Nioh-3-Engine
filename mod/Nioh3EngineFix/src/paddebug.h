// paddebug.h - Gamepad debug row (pc #97485, universal-optimizer open.md #61623). Read-only: shows the pad exactly as the mod's
// XInputGetState IAT hook (menu.cpp PadFeed) received it, BEFORE any menu masking, plus whether the mods are hiding the pad from the game
// right now (EngineFix: menu open / release latch / MaskGameButtons lease / Start-tap swallow; BossRematch: FormBlockPad).
// Off by default: [Menu] PadDebug=1 shows it as one line on the Diagnostics page. Nothing here changes input; Record() only copies.
// Header-only, no Windows dependency (tests/paddebug_test.cpp builds it alone).
#pragma once
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace paddebug {

inline volatile int g_on = 0;  // [Menu] PadDebug (default 0)

enum Block : uint32_t { kMenuOpen = 1, kLatch = 2, kMaskLease = 4, kTapSwallow = 8, kFormBlock = 16 };

struct Snap {
    uint32_t packet = 0, buttons = 0, slot = 0, own = 0, blocked = 0;
    int lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
    uint64_t t = 0;  // ms of the last feed (0 = never)
};

// Written by the game input thread, read by the Present thread. A torn read can only mix two consecutive pad states for one frame of
// a debug line, so plain relaxed atomics are enough.
struct Store {
    std::atomic<uint32_t> packet{0}, buttons{0}, slot{0}, own{0}, blocked{0};
    std::atomic<int> lx{0}, ly{0}, rx{0}, ry{0}, lt{0}, rt{0};
    std::atomic<uint64_t> t{0};
};
inline Store g_store;

inline void Record(uint32_t slot, uint32_t packet, uint32_t buttons, int lx, int ly, int rx, int ry, int lt, int rt, bool own, uint64_t t) {
    if (!g_on) return;
    constexpr auto r = std::memory_order_relaxed;
    g_store.slot.store(slot, r); g_store.packet.store(packet, r); g_store.buttons.store(buttons & 0xFFFF, r);
    g_store.lx.store(lx, r); g_store.ly.store(ly, r); g_store.rx.store(rx, r); g_store.ry.store(ry, r);
    g_store.lt.store(lt, r); g_store.rt.store(rt, r); g_store.own.store(own ? 1u : 0u, r); g_store.t.store(t, r);
}
inline void RecordBlock(uint32_t bits) { if (g_on) g_store.blocked.store(bits, std::memory_order_relaxed); }

inline Snap Read() {
    constexpr auto r = std::memory_order_relaxed;
    Snap s;
    s.packet = g_store.packet.load(r); s.buttons = g_store.buttons.load(r); s.slot = g_store.slot.load(r); s.own = g_store.own.load(r);
    s.blocked = g_store.blocked.load(r);
    s.lx = g_store.lx.load(r); s.ly = g_store.ly.load(r); s.rx = g_store.rx.load(r); s.ry = g_store.ry.load(r);
    s.lt = g_store.lt.load(r); s.rt = g_store.rt.load(r); s.t = g_store.t.load(r);
    return s;
}

// Two lines (<= 58 columns each, the panel width). formBlock: BossRematch's FormBlockPad state (-1 = BossRematch not loaded).
inline void Format(const Snap& s, uint64_t now, int formBlock, char* l1, size_t n1, char* l2, size_t n2) {
    if (!s.t || now - s.t > 1500) {
        snprintf(l1, n1, "Pad: no XInput read for %s", s.t ? "1.5 s" : "this session");
        snprintf(l2, n2, "Mod blocking pad: %s", formBlock > 0 ? "yes (FormBlockPad)" : "no");
        return;
    }
    snprintf(l1, n1, "P%u%s #%u B%04X LT%d RT%d L%d,%d R%d,%d", s.slot, s.own ? "*" : "", s.packet & 0xFFFFu, s.buttons, s.lt,
             s.rt, s.lx, s.ly, s.rx, s.ry);
    const uint32_t b = (s.own ? 0u : s.blocked) | (formBlock > 0 ? (uint32_t)kFormBlock : 0u);
    if (!b) { snprintf(l2, n2, "Mod blocking pad: no"); return; }
    char why[96] = {};
    size_t k = 0;
    auto add = [&](const char* w) { k += (size_t)snprintf(why + k, sizeof(why) - k, "%s%s", k ? ", " : "", w); if (k >= sizeof(why)) k = sizeof(why) - 1; };
    if (b & kMenuOpen) add("menu open");
    if (b & kLatch) add("release latch");
    if (b & kMaskLease) add("button mask");
    if (b & kTapSwallow) add("Start tap");
    if (b & kFormBlock) add("FormBlockPad");
    snprintf(l2, n2, "Mod blocking pad: yes (%s)", why);
}

}  // namespace paddebug
