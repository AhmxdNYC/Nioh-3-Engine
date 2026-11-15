#pragma once
// UO open #10: the button glyph set (Xbox / PlayStation / Nintendo) is picked from the connected pad itself, not from an ini
// switch. menu.cpp reads the vendor id of every raw-input HID joystick / gamepad (RID_DEVICE_INFO_HID::dwVendorId, USB and
// Bluetooth alike; XInput pads are listed as HID too). Sony = PlayStation, Nintendo = Nintendo, anything else (Microsoft, ASUS / Lenovo / Valve handheld pads, Steam's
// virtual pad, unknown) = Xbox. [Menu] ButtonStyle = Auto (default) uses this; Xbox / PlayStation / Nintendo there is an override.
// Pure logic (no Windows calls): menu.cpp feeds it the vendor ids it enumerates; tests feed fake ones.
#include <cstdint>

namespace pad_family {

enum : int { kXbox = 0, kPlayStation = 1, kNintendo = 2, kAuto = -1 };
constexpr uint16_t kVidSony = 0x054C, kVidNintendo = 0x057E;

inline int FamilyOfVid(uint16_t vid) { return vid == kVidSony ? kPlayStation : vid == kVidNintendo ? kNintendo : kXbox; }

// several pads: PlayStation wins, then Nintendo, else Xbox. A virtual Xbox pad (DS4Windows, Steam) next to a real DualSense is that
// DualSense, so the real pad's family beats the Xbox default.
inline int Pick(const uint16_t* vids, int n) {
    int best = kXbox;
    for (int i = 0; i < n; ++i) {
        const int f = FamilyOfVid(vids[i]);
        if (f == kPlayStation) return kPlayStation;
        if (f == kNintendo) best = kNintendo;
    }
    return best;
}
// [Menu] ButtonStyle text -> kAuto / family (anything unknown or empty = Auto)
inline int ParseStyle(const char* s) {
    auto eq = [](const char* a, const char* b) {
        for (; *a && *b; ++a, ++b) if ((*a | 0x20) != (*b | 0x20)) return false;
        return !*a && !*b;
    };
    if (!s) return kAuto;
    if (eq(s, "Xbox")) return kXbox;
    if (eq(s, "PlayStation")) return kPlayStation;
    if (eq(s, "Nintendo")) return kNintendo;
    return kAuto;
}
// effective glyph set: an explicit ini style wins, Auto uses the detected pads
inline int Effective(int iniStyle, const uint16_t* vids, int n) { return iniStyle >= 0 ? iniStyle : Pick(vids, n); }

}  // namespace pad_family
