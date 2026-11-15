// padcard.h - Controller status card (UO open #8): answers "my controller does not work" in a glance on the mod menu's Controller
// overlay page, instead of a raw-input probe. Two lines:
//   1. which pad: name (VID/PID table, else the ids), its glyph family, and whether the game actually gets XInput from it
//      (a DualSense listed by Windows but no XInput packets = Steam Input / DS4Windows is off);
//   2. sticks: at rest + how far off centre, "moving", or "possible drift" when a stick sits still off centre past the XInput
//      deadzone for 1.5 s with no button held. A slot with no packet for 5 s counts as gone.
// Read-only. Feed() is a few relaxed stores per XInput poll (game input thread); Update()/Format() run on the Present thread only
// while the page is shown. Header-only, no Windows dependency (tests/padcard_test.cpp builds it alone).
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "pad_family.h"

namespace padcard {

struct PadId { uint16_t vid = 0, pid = 0; };

// product names for common pads; nullptr = unknown (the card shows the ids)
inline const char* Name(uint16_t vid, uint16_t pid) {
    struct K { uint16_t v, p; const char* n; };
    static const K k[] = {
        {0x054C, 0x0CE6, "DualSense"}, {0x054C, 0x0DF2, "DualSense Edge"}, {0x054C, 0x05C4, "DualShock 4"}, {0x054C, 0x09CC, "DualShock 4"},
        {0x054C, 0x0BA0, "DS4 USB adapter"}, {0x054C, 0x0268, "DualShock 3"},
        {0x057E, 0x2009, "Switch Pro Controller"}, {0x057E, 0x2006, "Joy-Con (L)"}, {0x057E, 0x2007, "Joy-Con (R)"},
        {0x045E, 0x028E, "Xbox 360 Controller"}, {0x045E, 0x02FF, "Xbox One Controller"}, {0x045E, 0x02EA, "Xbox One Controller"},
        {0x045E, 0x0B12, "Xbox Series Controller"}, {0x045E, 0x0B13, "Xbox Series Controller"}, {0x045E, 0x0B20, "Xbox Series Controller"},
        {0x045E, 0x0B00, "Xbox Elite Series 2"}, {0x045E, 0x0B05, "Xbox Elite Series 2"}, {0x045E, 0x0B22, "Xbox Elite Series 2"},
        {0x0B05, 0x1ABE, "ROG Ally controls"}, {0x0B05, 0x1B4C, "ROG Ally X controls"}, {0x17EF, 0x6182, "Legion Go controls"},
        {0x28DE, 0x11FF, "Steam virtual pad (Valve)"}, {0x28DE, 0x1205, "Steam Deck controls"},
    };
    for (const K& e : k)
        if (e.v == vid && e.p == pid) return e.n;
    return nullptr;
}
inline const char* FamilyName(int f) { return f == pad_family::kPlayStation ? "PlayStation" : f == pad_family::kNintendo ? "Nintendo" : "Xbox"; }

// ---- live XInput state per slot (fed raw, before any menu masking) ----
struct Slot {
    std::atomic<uint64_t> t{0};  // ms of the last packet (0 = never)
    std::atomic<uint32_t> buttons{0};
    std::atomic<int> lx{0}, ly{0}, rx{0}, ry{0};
};
inline Slot g_slots[4];
inline void Feed(unsigned slot, uint32_t buttons, int lx, int ly, int rx, int ry, uint64_t t) {
    if (slot >= 4) return;
    constexpr auto r = std::memory_order_relaxed;
    Slot& s = g_slots[slot];
    s.buttons.store(buttons & 0xFFFF, r); s.lx.store(lx, r); s.ly.store(ly, r); s.rx.store(rx, r); s.ry.store(ry, r); s.t.store(t, r);
}
struct SlotSnap { uint64_t t = 0; uint32_t buttons = 0; int lx = 0, ly = 0, rx = 0, ry = 0; };
inline SlotSnap Read(unsigned slot) {
    constexpr auto r = std::memory_order_relaxed;
    const Slot& s = g_slots[slot & 3];
    SlotSnap o;
    o.t = s.t.load(r); o.buttons = s.buttons.load(r); o.lx = s.lx.load(r); o.ly = s.ly.load(r); o.rx = s.rx.load(r); o.ry = s.ry.load(r);
    return o;
}

// ---- drift check ----
constexpr int kDeadL = 7849, kDeadR = 8689;  // XINPUT_GAMEPAD_LEFT/RIGHT_THUMB_DEADZONE
constexpr int kStill = 1200;                  // a stick counts as "held still" while it stays within this of where it settled
constexpr uint64_t kDriftMs = 1500;
inline int Pct(int x, int y) { return (int)std::lround(std::min(1.0, std::hypot((double)x, (double)y) / 32767.0) * 100.0); }
struct Drift {
    uint64_t since = 0;  // when the stick settled off centre (0 = not off centre / moving)
    int ax = 0, ay = 0;
    bool flagged = false;
};
// one stick: off centre past dz, no button held, not moving for kDriftMs -> flagged. Back inside dz clears it.
inline void Update(Drift& d, int x, int y, int dz, uint32_t buttons, uint64_t t) {
    const bool off = std::hypot((double)x, (double)y) > dz;
    if (!off) { d = Drift{}; return; }
    if (buttons || d.since == 0 || std::abs(x - d.ax) > kStill || std::abs(y - d.ay) > kStill) {
        d.since = buttons ? 0 : t; d.ax = x; d.ay = y; d.flagged = false;
        return;
    }
    if (t - d.since >= kDriftMs) d.flagged = true;
}

struct Card {
    Drift l[4], r[4];
    PadId hid[8];
    int nHid = 0;
};
// the active XInput slot: the newest packet within 5 s; -1 = none
inline int ActiveSlot(uint64_t now, SlotSnap* out = nullptr) {
    int best = -1;
    uint64_t bt = 0;
    for (int i = 0; i < 4; ++i) {
        const SlotSnap s = Read((unsigned)i);
        if (s.t && now >= s.t && now - s.t < 5000 && s.t > bt) { best = i; bt = s.t; if (out) *out = s; }
    }
    return best;
}
inline void Step(Card& c, uint64_t now) {
    for (int i = 0; i < 4; ++i) {
        const SlotSnap s = Read((unsigned)i);
        if (!s.t || now < s.t || now - s.t >= 5000) { c.l[i] = Drift{}; c.r[i] = Drift{}; continue; }
        Update(c.l[i], s.lx, s.ly, kDeadL, s.buttons, now);
        Update(c.r[i], s.rx, s.ry, kDeadR, s.buttons, now);
    }
}
// all card lines fit the 58-column menu panel (names <= 25 chars + "Pad: ..., no XInput (Steam Input?)")
// the pad to name: the first one whose family matches the glyph pick (so a DualSense beats Steam's virtual pad), else the first
inline int NamedPad(const Card& c) {
    if (c.nHid <= 0) return -1;
    uint16_t v[8];
    for (int i = 0; i < c.nHid; ++i) v[i] = c.hid[i].vid;
    const int fam = pad_family::Pick(v, c.nHid);
    for (int i = 0; i < c.nHid; ++i)
        if (pad_family::FamilyOfVid(c.hid[i].vid) == fam && !(c.hid[i].vid == 0x28DE && c.nHid > 1)) return i;
    return 0;
}
inline void Format(const Card& c, uint64_t now, int glyphStyle, char* l1, size_t n1, char* l2, size_t n2) {
    SlotSnap s;
    const int slot = ActiveSlot(now, &s);
    const int np = NamedPad(c);
    char name[32] = "";
    if (np >= 0) {
        const PadId& p = c.hid[np];
        if (const char* k = Name(p.vid, p.pid)) snprintf(name, sizeof name, "%s", k);
        else if (p.vid == 0x28DE) snprintf(name, sizeof name, "Steam virtual pad (Valve)");  // #106253: any other Valve pid
        else snprintf(name, sizeof name, "pad %04X:%04X", p.vid, p.pid);
    }
    if (np < 0 && slot < 0) snprintf(l1, n1, "Pad: none found - plug one in, or check Steam Input");
    else if (slot < 0) snprintf(l1, n1, "Pad: %s, no XInput (Steam Input?)", name);
    else if (np < 0) snprintf(l1, n1, "Pad: XInput %d (%s buttons)", slot + 1, FamilyName(glyphStyle));
    else snprintf(l1, n1, "Pad: %s (%s), XInput %d", name, FamilyName(glyphStyle), slot + 1);
    if (slot < 0) { snprintf(l2, n2, "Sticks: no input yet"); return; }
    const int pl = Pct(s.lx, s.ly), pr = Pct(s.rx, s.ry);
    const Drift &dl = c.l[slot], &dr = c.r[slot];
    if (dl.flagged || dr.flagged) {
        char which[24];
        if (dl.flagged && dr.flagged) snprintf(which, sizeof which, "L %d%% R %d%%", pl, pr);
        else if (dl.flagged) snprintf(which, sizeof which, "L %d%%", pl);
        else snprintf(which, sizeof which, "R %d%%", pr);
        snprintf(l2, n2, "Sticks: possible drift %s (deadzone %d%%)", which, Pct(dl.flagged ? kDeadL : kDeadR, 0));
    } else if (std::hypot((double)s.lx, (double)s.ly) > kDeadL || std::hypot((double)s.rx, (double)s.ry) > kDeadR) {
        snprintf(l2, n2, "Sticks: moving L %d%% R %d%%", pl, pr);
    } else {
        snprintf(l2, n2, "Sticks: at rest L %d%% R %d%%, no drift", pl, pr);
    }
}

}  // namespace padcard
